// Encode a sample of the game's sounds in FLAC, Ogg Vorbis and Opus, which the game lacks, so that the decoders of
// those formats are measured on this game's sounds (surveys/audio-decoders.md).
//
// usage: encode <game_mp3.bin> <out folder>
// The sample: every stereo file (the music) and every 40th mono file, in the corpus order, decoded by dr_mp3. Written:
// sample_flac.bin (libFLAC, level 5), sample_vorbis.bin (libvorbis, quality 4, variable rate), sample_opus.bin
// (libopusenc, 48 kbit/s mono, 96 kbit/s stereo, variable rate, resampled to 48 kHz by libopusenc), and
// sample_ref.tsv: name, channels, rate, frames and hash of each source decoding (a FLAC decoding must give it back).
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

#include "bench.h"

#include <FLAC/stream_encoder.h>
#include <opusenc.h>
#include <vorbis/vorbisenc.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
struct File
{
	std::string name;
	std::vector<uint8_t> data;
};

std::vector<File> Load( const char *path )
{
	std::vector<File> out;
	FILE *f = std::fopen( path, "rb" );
	char magic[4];
	uint32_t count = 0;
	if ( !f || std::fread( magic, 1, 4, f ) != 4 || std::fread( &count, 4, 1, f ) != 1 )
		std::exit( 1 );
	for ( uint32_t i = 0; i < count; ++i )
	{
		uint16_t nl = 0;
		uint32_t dl = 0;
		File e;
		if ( std::fread( &nl, 2, 1, f ) != 1 )
			std::exit( 1 );
		e.name.resize( nl );
		if ( std::fread( e.name.data(), 1, nl, f ) != nl || std::fread( &dl, 4, 1, f ) != 1 )
			std::exit( 1 );
		e.data.resize( dl );
		if ( std::fread( e.data.data(), 1, dl, f ) != dl )
			std::exit( 1 );
		out.push_back( std::move( e ) );
	}
	std::fclose( f );
	return out;
}

void Save( const std::string &path, const std::vector<File> &files )
{
	FILE *f = std::fopen( path.c_str(), "wb" );
	const uint32_t count = uint32_t( files.size() );
	std::fwrite( "ADC1", 1, 4, f );
	std::fwrite( &count, 4, 1, f );
	for ( const File &e : files )
	{
		const uint16_t nl = uint16_t( e.name.size() );
		const uint32_t dl = uint32_t( e.data.size() );
		std::fwrite( &nl, 2, 1, f );
		std::fwrite( e.name.data(), 1, nl, f );
		std::fwrite( &dl, 4, 1, f );
		std::fwrite( e.data.data(), 1, dl, f );
	}
	std::fclose( f );
}

std::string WithExt( const std::string &name, const char *ext ) { return name.substr( 0, name.rfind( '.' ) + 1 ) + ext; }

// FLAC, level 5 (libFLAC's default), into memory, with the seek and tell callbacks so that libFLAC writes the total
// length into STREAMINFO at the end, as the flac tool's files have it.
struct FlacOut
{
	std::vector<uint8_t> data;
	size_t pos = 0;
};

std::vector<uint8_t> EncodeFlac( const Pcm &p )
{
	FlacOut out;
	FLAC__StreamEncoder *e = FLAC__stream_encoder_new();
	FLAC__stream_encoder_set_channels( e, unsigned( p.channels ) );
	FLAC__stream_encoder_set_bits_per_sample( e, 16 );
	FLAC__stream_encoder_set_sample_rate( e, unsigned( p.rate ) );
	FLAC__stream_encoder_set_compression_level( e, 5 );
	auto write = []( const FLAC__StreamEncoder *, const FLAC__byte buffer[], size_t bytes, uint32_t, uint32_t, void *u ) {
		auto *o = static_cast<FlacOut *>( u );
		if ( o->data.size() < o->pos + bytes )
			o->data.resize( o->pos + bytes );
		std::memcpy( o->data.data() + o->pos, buffer, bytes );
		o->pos += bytes;
		return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
	};
	auto seek = []( const FLAC__StreamEncoder *, FLAC__uint64 offset, void *u ) {
		static_cast<FlacOut *>( u )->pos = size_t( offset );
		return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
	};
	auto tell = []( const FLAC__StreamEncoder *, FLAC__uint64 *offset, void *u ) {
		*offset = static_cast<FlacOut *>( u )->pos;
		return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
	};
	FLAC__stream_encoder_init_stream( e, write, seek, tell, nullptr, &out );
	const size_t frames = p.samples.size() / size_t( p.channels );
	std::vector<FLAC__int32> buf( 4096 * size_t( p.channels ) );
	for ( size_t i = 0; i < frames; i += 4096 )
	{
		const size_t n = frames - i < 4096 ? frames - i : 4096;
		for ( size_t k = 0; k < n * size_t( p.channels ); ++k )
			buf[k] = p.samples[i * size_t( p.channels ) + k];
		FLAC__stream_encoder_process_interleaved( e, buf.data(), unsigned( n ) );
	}
	FLAC__stream_encoder_finish( e );
	FLAC__stream_encoder_delete( e );
	return out.data;
}

// Ogg Vorbis, quality 4 (variable rate), into memory.
std::vector<uint8_t> EncodeVorbis( const Pcm &p, int serial )
{
	std::vector<uint8_t> out;
	vorbis_info vi;
	vorbis_info_init( &vi );
	vorbis_encode_init_vbr( &vi, p.channels, p.rate, 0.4f );
	vorbis_comment vc;
	vorbis_comment_init( &vc );
	vorbis_dsp_state vd;
	vorbis_block vb;
	vorbis_analysis_init( &vd, &vi );
	vorbis_block_init( &vd, &vb );
	ogg_stream_state os;
	ogg_stream_init( &os, serial );
	ogg_packet h0, h1, h2;
	vorbis_analysis_headerout( &vd, &vc, &h0, &h1, &h2 );
	ogg_stream_packetin( &os, &h0 );
	ogg_stream_packetin( &os, &h1 );
	ogg_stream_packetin( &os, &h2 );
	ogg_page og;
	auto put = [&]( const ogg_page &pg ) {
		out.insert( out.end(), pg.header, pg.header + pg.header_len );
		out.insert( out.end(), pg.body, pg.body + pg.body_len );
	};
	while ( ogg_stream_flush( &os, &og ) )
		put( og );
	const size_t frames = p.samples.size() / size_t( p.channels );
	for ( size_t i = 0;; i += 1024 )
	{
		const size_t n = i < frames ? ( frames - i < 1024 ? frames - i : 1024 ) : 0;
		if ( n )
		{
			float **in = vorbis_analysis_buffer( &vd, int( n ) );
			for ( size_t k = 0; k < n; ++k )
				for ( int c = 0; c < p.channels; ++c )
					in[c][k] = p.samples[( i + k ) * size_t( p.channels ) + size_t( c )] / 32768.0f;
		}
		vorbis_analysis_wrote( &vd, int( n ) );
		while ( vorbis_analysis_blockout( &vd, &vb ) == 1 )
		{
			vorbis_analysis( &vb, nullptr );
			vorbis_bitrate_addblock( &vb );
			ogg_packet op;
			while ( vorbis_bitrate_flushpacket( &vd, &op ) )
			{
				ogg_stream_packetin( &os, &op );
				while ( ogg_stream_pageout( &os, &og ) )
					put( og );
			}
		}
		if ( !n )
			break;
	}
	while ( ogg_stream_flush( &os, &og ) )
		put( og );
	ogg_stream_clear( &os );
	vorbis_block_clear( &vb );
	vorbis_dsp_clear( &vd );
	vorbis_comment_clear( &vc );
	vorbis_info_clear( &vi );
	return out;
}

// Opus in Ogg through libopusenc, into memory.
std::vector<uint8_t> EncodeOpus( const Pcm &p )
{
	std::vector<uint8_t> out;
	OpusEncCallbacks cb{ []( void *u, const unsigned char *ptr, opus_int32 len ) -> int {
							auto *v = static_cast<std::vector<uint8_t> *>( u );
							v->insert( v->end(), ptr, ptr + len );
							return 0;
						},
						 []( void * ) -> int { return 0; } };
	OggOpusComments *comments = ope_comments_create();
	int err = 0;
	OggOpusEnc *e = ope_encoder_create_callbacks( &cb, &out, comments, p.rate, p.channels, 0, &err );
	ope_encoder_ctl( e, OPUS_SET_BITRATE( p.channels == 1 ? 48000 : 96000 ) );
	ope_encoder_write( e, p.samples.data(), int( p.samples.size() / size_t( p.channels ) ) );
	ope_encoder_drain( e );
	ope_encoder_destroy( e );
	ope_comments_destroy( comments );
	return out;
}
} // namespace

int main( int argc, char **argv )
{
	if ( argc < 3 )
	{
		std::fprintf( stderr, "usage: encode <game_mp3.bin> <out folder>\n" );
		return 2;
	}
	const std::vector<File> mp3 = Load( argv[1] );
	const std::string dir = argv[2];
	std::vector<File> flac, vorbis, opus;
	FILE *ref = std::fopen( ( dir + "/sample_ref.tsv" ).c_str(), "wb" );
	size_t monoSeen = 0;
	for ( const File &f : mp3 )
	{
		drmp3_config cfg{};
		drmp3_uint64 frames = 0;
		drmp3_int16 *s = drmp3_open_memory_and_read_pcm_frames_s16( f.data.data(), f.data.size(), &cfg, &frames, nullptr );
		if ( !s )
			continue;
		const bool take = cfg.channels == 2 || monoSeen++ % 40 == 0;
		if ( take && frames > 0 )
		{
			Pcm p;
			p.channels = int( cfg.channels );
			p.rate = int( cfg.sampleRate );
			p.samples.assign( s, s + frames * cfg.channels );
			flac.push_back( { WithExt( f.name, "flac" ), EncodeFlac( p ) } );
			vorbis.push_back( { WithExt( f.name, "ogg" ), EncodeVorbis( p, int( vorbis.size() ) + 1 ) } );
			opus.push_back( { WithExt( f.name, "opus" ), EncodeOpus( p ) } );
			std::fprintf( ref, "%s\t%d\t%d\t%llu\t%016llx\n", f.name.c_str(), p.channels, p.rate, static_cast<unsigned long long>( frames ),
						  static_cast<unsigned long long>( HashPcm( p ) ) );
		}
		drmp3_free( s, nullptr );
	}
	std::fclose( ref );
	Save( dir + "/sample_flac.bin", flac );
	Save( dir + "/sample_vorbis.bin", vorbis );
	Save( dir + "/sample_opus.bin", opus );
	size_t bf = 0, bv = 0, bo = 0;
	for ( size_t i = 0; i < flac.size(); ++i )
	{
		bf += flac[i].data.size();
		bv += vorbis[i].data.size();
		bo += opus[i].data.size();
	}
	std::printf( "%zu files: FLAC %zu bytes, Ogg Vorbis %zu, Opus %zu\n", flac.size(), bf, bv, bo );
	return 0;
}

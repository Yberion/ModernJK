// Writes the sample files of the sound module's tests (test/sound/data): one synthetic signal (two tones, a sweep and
// a little noise) as WAV (16-bit mono at 22,050 Hz, 8-bit mono at 11,025 Hz, 16-bit and 24-bit stereo at 44,100 Hz),
// FLAC (the two stereo ones, libFLAC level 5) and Ogg Vorbis (mono and stereo, libvorbis quality 4, a fixed stream
// serial), and prints, for each WAV, its frames and the FNV-1a hash of the 16-bit samples a decoder must give (8-bit
// as (x - 128) << 8, 24-bit as its upper 16 bits): the constants of test/sound/decoder.cpp. Also MP3 by LAME when built
// with it (mono at 64 kbit/s, stereo at 128), and eleven minutes of silence in FLAC.
// usage: make_test_samples <out folder>
#include <FLAC/stream_encoder.h>
#include <vorbis/vorbisenc.h>
#if defined( WITH_LAME )
#include <lame.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
constexpr double kPi = 3.14159265358979323846;

// channel c of frame i, in [-1, 1]
double Signal( int i, int c, int rate, int frames )
{
	const double t = double( i ) / rate, length = double( frames ) / rate;
	static uint32_t noise = 12345;
	noise = noise * 1664525u + 1013904223u;
	const double n = ( double( noise >> 8 ) / double( 1u << 24 ) ) * 2.0 - 1.0;
	const double sweep = 2.0 * kPi * ( 200.0 * t + 2000.0 * t * t / ( 2.0 * length ) );
	const double tone = c == 0 ? std::sin( 2.0 * kPi * 440.0 * t ) : std::sin( 2.0 * kPi * 660.0 * t + 1.0 );
	return 0.35 * tone + 0.25 * std::sin( sweep * ( c == 0 ? 1.0 : 1.5 ) ) + 0.02 * n;
}

struct Sound
{
	int rate, channels, frames;
	std::vector<double> x; // interleaved
};

Sound Make( int rate, int channels, double seconds )
{
	Sound s{ rate, channels, int( rate * seconds ), {} };
	for ( int i = 0; i < s.frames; ++i )
		for ( int c = 0; c < channels; ++c )
			s.x.push_back( Signal( i, c, rate, s.frames ) );
	return s;
}

int32_t Quantize( double x, int bits ) { return int32_t( std::lround( x * double( ( 1 << ( bits - 1 ) ) - 1 ) ) ); }

uint64_t Fnv( uint64_t h, int16_t v )
{
	const uint16_t u = uint16_t( v );
	for ( int b = 0; b < 2; ++b )
	{
		h ^= ( u >> ( 8 * b ) ) & 0xff;
		h *= 1099511628211ull;
	}
	return h;
}

void Put( std::string &out, uint32_t v, int bytes )
{
	for ( int b = 0; b < bytes; ++b )
		out.push_back( char( ( v >> ( 8 * b ) ) & 0xff ) );
}

void WriteFile( const std::string &path, const std::string &data )
{
	FILE *f = std::fopen( path.c_str(), "wb" );
	if ( !f || std::fwrite( data.data(), 1, data.size(), f ) != data.size() )
		std::exit( 1 );
	std::fclose( f );
}

void Wav( const std::string &dir, const char *name, const Sound &s, int bits )
{
	const int bytes = bits / 8;
	std::string data;
	uint64_t hash = 1469598103934665603ull;
	for ( double x : s.x )
	{
		if ( bits == 8 )
		{
			const int32_t u = Quantize( x, 8 ) + 128;
			data.push_back( char( u ) );
			hash = Fnv( hash, int16_t( ( u - 128 ) * 256 ) );
		}
		else
		{
			const int32_t v = Quantize( x, bits );
			Put( data, uint32_t( v ), bytes );
			hash = Fnv( hash, int16_t( v >> ( bits - 16 ) ) );
		}
	}
	std::string out = "RIFF";
	Put( out, uint32_t( 36 + data.size() ), 4 );
	out += "WAVEfmt ";
	Put( out, 16, 4 );
	Put( out, 1, 2 ); // PCM
	Put( out, uint32_t( s.channels ), 2 );
	Put( out, uint32_t( s.rate ), 4 );
	Put( out, uint32_t( s.rate * s.channels * bytes ), 4 );
	Put( out, uint32_t( s.channels * bytes ), 2 );
	Put( out, uint32_t( bits ), 2 );
	out += "data";
	Put( out, uint32_t( data.size() ), 4 );
	out += data;
	WriteFile( dir + "/" + name, out );
	std::printf( "%-20s rate %d, channels %d, frames %d, hash 0x%016llx\n", name, s.rate, s.channels, s.frames, (unsigned long long)hash );
}

void Flac( const std::string &dir, const char *name, const Sound &s, int bits )
{
	std::vector<FLAC__int32> pcm;
	for ( double x : s.x )
		pcm.push_back( Quantize( x, bits ) );
	FLAC__StreamEncoder *e = FLAC__stream_encoder_new();
	FLAC__stream_encoder_set_channels( e, unsigned( s.channels ) );
	FLAC__stream_encoder_set_bits_per_sample( e, unsigned( bits ) );
	FLAC__stream_encoder_set_sample_rate( e, unsigned( s.rate ) );
	FLAC__stream_encoder_set_compression_level( e, 5 );
	FLAC__stream_encoder_set_total_samples_estimate( e, FLAC__uint64( s.frames ) );
	const std::string path = dir + "/" + name;
	if ( FLAC__stream_encoder_init_file( e, path.c_str(), nullptr, nullptr ) != FLAC__STREAM_ENCODER_INIT_STATUS_OK ||
		 !FLAC__stream_encoder_process_interleaved( e, pcm.data(), unsigned( s.frames ) ) || !FLAC__stream_encoder_finish( e ) )
		std::exit( 1 );
	FLAC__stream_encoder_delete( e );
	std::printf( "%-20s written\n", name );
}

void Vorbis( const std::string &dir, const char *name, const Sound &s )
{
	vorbis_info vi;
	vorbis_info_init( &vi );
	vorbis_encode_init_vbr( &vi, s.channels, s.rate, 0.4f );
	vorbis_comment vc;
	vorbis_comment_init( &vc );
	vorbis_dsp_state vd;
	vorbis_block vb;
	vorbis_analysis_init( &vd, &vi );
	vorbis_block_init( &vd, &vb );
	ogg_stream_state os;
	ogg_stream_init( &os, 1 );
	ogg_packet h0, h1, h2;
	vorbis_analysis_headerout( &vd, &vc, &h0, &h1, &h2 );
	ogg_stream_packetin( &os, &h0 );
	ogg_stream_packetin( &os, &h1 );
	ogg_stream_packetin( &os, &h2 );
	std::string out;
	ogg_page og;
	auto put = [&]( const ogg_page &pg ) {
		out.append( reinterpret_cast<const char *>( pg.header ), size_t( pg.header_len ) );
		out.append( reinterpret_cast<const char *>( pg.body ), size_t( pg.body_len ) );
	};
	while ( ogg_stream_flush( &os, &og ) )
		put( og );
	for ( int pass = 0; pass < 2; ++pass )
	{
		if ( pass == 0 )
		{
			float **in = vorbis_analysis_buffer( &vd, s.frames );
			for ( int i = 0; i < s.frames; ++i )
				for ( int c = 0; c < s.channels; ++c )
					in[c][i] = float( s.x[size_t( i ) * size_t( s.channels ) + size_t( c )] );
			vorbis_analysis_wrote( &vd, s.frames );
		}
		else
			vorbis_analysis_wrote( &vd, 0 ); // the end of the stream
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
	}
	while ( ogg_stream_flush( &os, &og ) )
		put( og );
	ogg_stream_clear( &os );
	vorbis_block_clear( &vb );
	vorbis_dsp_clear( &vd );
	vorbis_comment_clear( &vc );
	vorbis_info_clear( &vi );
	WriteFile( dir + "/" + name, out );
	std::printf( "%-20s written\n", name );
}
#if defined( WITH_LAME )
// LAME at a constant rate, without ID3 tag; its Info tag (the encoder's delay and padding) written in place of the
// empty first frame it leaves for it, so that a decoder that reads the tag gives back the source's frames
void Mp3( const std::string &dir, const char *name, const Sound &s, int kbps )
{
	std::vector<short> pcm;
	for ( double x : s.x )
		pcm.push_back( short( Quantize( x, 16 ) ) );
	lame_t g = lame_init();
	lame_set_num_channels( g, s.channels );
	lame_set_in_samplerate( g, s.rate );
	lame_set_out_samplerate( g, s.rate );
	lame_set_mode( g, s.channels == 1 ? MONO : JOINT_STEREO );
	lame_set_brate( g, kbps );
	lame_set_quality( g, 2 );
	lame_set_bWriteVbrTag( g, 1 );
	lame_set_write_id3tag_automatic( g, 0 );
	if ( lame_init_params( g ) < 0 )
		std::exit( 1 );
	std::vector<unsigned char> out( size_t( s.frames ) * 2 + 16384 );
	int n = s.channels == 1 ? lame_encode_buffer( g, pcm.data(), pcm.data(), s.frames, out.data(), int( out.size() ) )
							: lame_encode_buffer_interleaved( g, pcm.data(), s.frames, out.data(), int( out.size() ) );
	if ( n < 0 )
		std::exit( 1 );
	const int flushed = lame_encode_flush( g, out.data() + n, int( out.size() ) - n );
	if ( flushed < 0 )
		std::exit( 1 );
	out.resize( size_t( n + flushed ) );
	unsigned char tag[2880];
	const size_t tagSize = lame_get_lametag_frame( g, tag, sizeof( tag ) );
	if ( tagSize == 0 || tagSize > out.size() )
		std::exit( 1 );
	std::copy( tag, tag + tagSize, out.begin() );
	lame_close( g );
	WriteFile( dir + "/" + name, std::string( out.begin(), out.end() ) );
	std::printf( "%-20s written (LAME %s)\n", name, get_lame_version() );
}
#endif
} // namespace

int main( int argc, char **argv )
{
	if ( argc != 2 )
	{
		std::fprintf( stderr, "usage: make_test_samples <out folder>\n" );
		return 1;
	}
	const std::string dir = argv[1];
	const Sound mono22 = Make( 22050, 1, 0.25 ), mono11 = Make( 11025, 1, 0.25 ), stereo44 = Make( 44100, 2, 0.1 );
	Wav( dir, "mono22k_s16.wav", mono22, 16 );
	Wav( dir, "mono11k_u8.wav", mono11, 8 );
	Wav( dir, "stereo44k_s16.wav", stereo44, 16 );
	Wav( dir, "stereo44k_s24.wav", stereo44, 24 );
	Flac( dir, "stereo44k_s16.flac", stereo44, 16 );
	Flac( dir, "stereo44k_s24.flac", stereo44, 24 );
	Vorbis( dir, "mono22k.ogg", mono22 );
	Vorbis( dir, "stereo44k.ogg", stereo44 );
	// eleven minutes of silence at 8,000 Hz: a few kilobytes that decode beyond the ten minutes a sound decoded whole
	// may last
	const Sound silence{ 8000, 1, 8000 * 660, std::vector<double>( size_t( 8000 * 660 ), 0.0 ) };
	Flac( dir, "silence_11min.flac", silence, 16 );
#if defined( WITH_LAME )
	Mp3( dir, "mono22k.mp3", mono22, 64 );	   // MPEG-2 layer III
	Mp3( dir, "stereo44k.mp3", stereo44, 128 ); // MPEG-1 layer III, joint stereo
#endif
	return 0;
}

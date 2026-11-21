// The FLAC, Ogg Vorbis and Opus decoders of the survey (DEC_DRFLAC, DEC_LIBFLAC, DEC_STBVORBIS, DEC_VORBISFILE,
// DEC_OPUSFILE).
#include "bench.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
// A file in memory behind the read, seek and tell callbacks the reference libraries ask for.
struct Memory
{
	const uint8_t *data;
	size_t size, pos;
	size_t Read( void *dst, size_t n )
	{
		n = n < size - pos ? n : size - pos;
		std::memcpy( dst, data + pos, n );
		pos += n;
		return n;
	}
	int Seek( long long off, int whence )
	{
		const long long base = whence == SEEK_SET ? 0 : ( whence == SEEK_CUR ? (long long)pos : (long long)size );
		if ( base + off < 0 || base + off > (long long)size )
			return -1;
		pos = size_t( base + off );
		return 0;
	}
};
} // namespace

#if defined( DEC_DRFLAC )
#define DR_FLAC_IMPLEMENTATION
#include "dr_flac.h"

static bool Run( const uint8_t *d, size_t n, Pcm &o )
{
	unsigned int channels = 0, rate = 0;
	drflac_uint64 frames = 0;
	drflac_int16 *s = drflac_open_memory_and_read_pcm_frames_s16( d, n, &channels, &rate, &frames, nullptr );
	if ( !s )
		return false;
	o.channels = int( channels );
	o.rate = int( rate );
	o.samples.assign( s, s + frames * channels );
	drflac_free( s, nullptr );
	return true;
}
DECODER_EXPORT( "dr_flac", Run )

#elif defined( DEC_LIBFLAC )
#include <FLAC/stream_decoder.h>

namespace
{
struct FlacState
{
	Memory mem;
	Pcm *out;
	bool error;
};
FLAC__StreamDecoderReadStatus ReadCb( const FLAC__StreamDecoder *, FLAC__byte buffer[], size_t *bytes, void *u )
{
	auto *s = static_cast<FlacState *>( u );
	*bytes = s->mem.Read( buffer, *bytes );
	return *bytes ? FLAC__STREAM_DECODER_READ_STATUS_CONTINUE : FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
}
FLAC__StreamDecoderWriteStatus WriteCb( const FLAC__StreamDecoder *, const FLAC__Frame *f, const FLAC__int32 *const buffer[], void *u )
{
	auto *s = static_cast<FlacState *>( u );
	const unsigned ch = f->header.channels, shift = f->header.bits_per_sample > 16 ? f->header.bits_per_sample - 16 : 0;
	s->out->channels = int( ch );
	s->out->rate = int( f->header.sample_rate );
	const size_t base = s->out->samples.size();
	s->out->samples.resize( base + size_t( f->header.blocksize ) * ch );
	for ( unsigned i = 0; i < f->header.blocksize; ++i )
		for ( unsigned c = 0; c < ch; ++c )
			s->out->samples[base + size_t( i ) * ch + c] = int16_t( buffer[c][i] >> shift );
	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}
void ErrorCb( const FLAC__StreamDecoder *, FLAC__StreamDecoderErrorStatus, void *u ) { static_cast<FlacState *>( u )->error = true; }
} // namespace

static bool Run( const uint8_t *d, size_t n, Pcm &o )
{
	FLAC__StreamDecoder *dec = FLAC__stream_decoder_new();
	FlacState s{ { d, n, 0 }, &o, false };
	bool ok = dec && FLAC__stream_decoder_init_stream( dec, ReadCb, nullptr, nullptr, nullptr, nullptr, WriteCb, nullptr, ErrorCb, &s ) ==
						 FLAC__STREAM_DECODER_INIT_STATUS_OK;
	// A damaged frame calls the error callback and libFLAC goes on with the next one: what decodes is kept.
	ok = ok && FLAC__stream_decoder_process_until_end_of_stream( dec );
	if ( dec )
		FLAC__stream_decoder_delete( dec );
	return ok && o.channels > 0;
}
DECODER_EXPORT( "libFLAC", Run )

#elif defined( DEC_STBVORBIS )
#include "stb_vorbis.c"

static bool Run( const uint8_t *d, size_t n, Pcm &o )
{
	int channels = 0, rate = 0;
	short *s = nullptr;
	const int frames = stb_vorbis_decode_memory( d, int( n ), &channels, &rate, &s );
	if ( frames < 0 || !s )
		return false;
	o.channels = channels;
	o.rate = rate;
	o.samples.assign( s, s + size_t( frames ) * size_t( channels ) );
	std::free( s );
	return true;
}
DECODER_EXPORT( "stb_vorbis", Run )

#elif defined( DEC_VORBISFILE )
#include <vorbis/vorbisfile.h>

namespace
{
size_t VRead( void *dst, size_t size, size_t count, void *u ) { return static_cast<Memory *>( u )->Read( dst, size * count ) / ( size ? size : 1 ); }
int VSeek( void *u, ogg_int64_t off, int whence ) { return static_cast<Memory *>( u )->Seek( off, whence ); }
long VTell( void *u ) { return long( static_cast<Memory *>( u )->pos ); }
} // namespace

static bool Run( const uint8_t *d, size_t n, Pcm &o )
{
	Memory mem{ d, n, 0 };
	OggVorbis_File vf;
	if ( ov_open_callbacks( &mem, &vf, nullptr, 0, { VRead, VSeek, nullptr, VTell } ) )
		return false;
	const vorbis_info *vi = ov_info( &vf, -1 );
	o.channels = vi->channels;
	o.rate = int( vi->rate );
	char buf[16384];
	int section = 0;
	bool ok = true;
	for ( ;; )
	{
		const long r = ov_read( &vf, buf, sizeof( buf ), 0, 2, 1, &section );
		if ( r == 0 )
			break;
		if ( r == OV_HOLE ) // damaged or missing data: it goes on after it
			continue;
		if ( r < 0 )
		{
			ok = !o.samples.empty();
			break;
		}
		const auto *s = reinterpret_cast<const int16_t *>( buf );
		o.samples.insert( o.samples.end(), s, s + r / 2 );
	}
	ov_clear( &vf );
	return ok;
}
DECODER_EXPORT( "libvorbis (vorbisfile)", Run )

#elif defined( DEC_OPUSFILE )
#include <opusfile.h>

static bool Run( const uint8_t *d, size_t n, Pcm &o )
{
	int err = 0;
	OggOpusFile *f = op_open_memory( d, n, &err );
	if ( !f )
		return false;
	o.channels = op_channel_count( f, -1 );
	o.rate = 48000; // Opus always decodes at 48 kHz
	opus_int16 buf[5760 * 2];
	bool ok = true;
	for ( ;; )
	{
		const int r = op_read( f, buf, int( sizeof( buf ) / sizeof( buf[0] ) ), nullptr );
		if ( r == 0 )
			break;
		if ( r == OP_HOLE ) // damaged or missing data: it goes on after it
			continue;
		if ( r < 0 )
		{
			ok = !o.samples.empty();
			break;
		}
		o.samples.insert( o.samples.end(), buf, buf + size_t( r ) * size_t( o.channels ) );
	}
	op_free( f );
	return ok;
}
DECODER_EXPORT( "opusfile (libopus)", Run )
#endif

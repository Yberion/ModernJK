// The MP3 decoders of the survey, one per executable (DEC_MP3CODE, DEC_MINIMP3, DEC_DRMP3, DEC_MPG123).
#include "bench.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined( DEC_MP3CODE )
// The engine's decoder (codemp/mp3code), called as the engine calls it (snd_mp3.cpp): the size pass, then the
// decoding; mono unless the file is stereo (the engine asks stereo for the music only).
extern "C" {
char *C_MP3_GetHeaderData( void *pvData, int iDataLen, int *piRate, int *piWidth, int *piChannels, int bStereoDesired );
char *C_MP3_GetUnpackedSize( void *pvData, int iSourceBytesRemaining, int *piUnpackedSize, int bStereoDesired );
char *C_MP3_UnpackRawPCM( void *pvData, int iSourceBytesRemaining, int *piUnpackedSize, void *pbUnpackBuffer, int bStereoDesired );
}

static int IsStereo( const uint8_t *d, size_t n )
{
	size_t i = 0;
	if ( n >= 10 && !std::memcmp( d, "ID3", 3 ) )
		i = 10 + ( size_t( d[6] & 0x7F ) << 21 | size_t( d[7] & 0x7F ) << 14 | size_t( d[8] & 0x7F ) << 7 | size_t( d[9] & 0x7F ) );
	for ( ; i + 4 <= n; ++i )
		if ( d[i] == 0xFF && ( d[i + 1] & 0xE0 ) == 0xE0 )
			return ( d[i + 3] >> 6 ) != 3;
	return 0;
}

static bool Run( const uint8_t *d, size_t n, Pcm &o )
{
	void *p = const_cast<uint8_t *>( d );
	const int stereo = IsStereo( d, n );
	int rate = 0, width = 0, channels = 0, size = 0, got = 0;
	if ( C_MP3_GetHeaderData( p, int( n ), &rate, &width, &channels, stereo ) || C_MP3_GetUnpackedSize( p, int( n ), &size, stereo ) )
		return false;
	o.samples.resize( size_t( size ) / 2 + 4608 );
	if ( C_MP3_UnpackRawPCM( p, int( n ), &got, o.samples.data(), stereo ) )
		return false;
	o.samples.resize( size_t( got ) / 2 );
	o.channels = channels;
	o.rate = rate;
	return got > 0;
}
DECODER_EXPORT( "mp3code (the engine's)", Run )

#elif defined( DEC_MINIMP3 )
// Its frame decoder, not minimp3_ex.h's file loader: the loader trusts the frame count of the Xing header, which is 0
// in many of the game's files, and then returns no sample. The tags are skipped here as the loader would.
#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

static bool Run( const uint8_t *d, size_t n, Pcm &o )
{
	if ( n >= 10 && !std::memcmp( d, "ID3", 3 ) )
	{
		const size_t skip = 10 + ( size_t( d[6] & 0x7F ) << 21 | size_t( d[7] & 0x7F ) << 14 | size_t( d[8] & 0x7F ) << 7 | size_t( d[9] & 0x7F ) );
		d += skip < n ? skip : n;
		n -= skip < n ? skip : n;
	}
	if ( n >= 128 && !std::memcmp( d + n - 128, "TAG", 3 ) )
		n -= 128;
	mp3dec_t dec;
	mp3dec_init( &dec );
	mp3dec_frame_info_t info{};
	mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
	while ( n > 0 )
	{
		const int samples = mp3dec_decode_frame( &dec, d, int( n ), pcm, &info );
		if ( !info.frame_bytes )
			break;
		d += info.frame_bytes;
		n -= size_t( info.frame_bytes ) < n ? size_t( info.frame_bytes ) : n;
		if ( samples > 0 )
		{
			o.channels = info.channels;
			o.rate = info.hz;
			o.samples.insert( o.samples.end(), pcm, pcm + samples * info.channels );
		}
	}
	return o.channels > 0;
}
DECODER_EXPORT( "minimp3", Run )

#elif defined( DEC_DRMP3 )
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

static bool Run( const uint8_t *d, size_t n, Pcm &o )
{
	drmp3_config cfg{};
	drmp3_uint64 frames = 0;
	drmp3_int16 *s = drmp3_open_memory_and_read_pcm_frames_s16( d, n, &cfg, &frames, nullptr );
	if ( !s )
		return false;
	o.channels = int( cfg.channels );
	o.rate = int( cfg.sampleRate );
	o.samples.assign( s, s + frames * cfg.channels );
	drmp3_free( s, nullptr );
	return true;
}
DECODER_EXPORT( "dr_mp3", Run )

#elif defined( DEC_MPG123 )
#include <mpg123.h>

static bool Run( const uint8_t *d, size_t n, Pcm &o )
{
	static const bool initialised = mpg123_init() == MPG123_OK;
	if ( !initialised )
		return false;
	mpg123_handle *h = mpg123_new( nullptr, nullptr );
	if ( !h )
		return false;
	// 16-bit output at the file's own rate and channels, decoded from memory.
	if ( !std::getenv( "AUDIODEC_DEBUG" ) ) // its own messages on stderr when debugging
		mpg123_param2( h, MPG123_ADD_FLAGS, MPG123_QUIET, 0.0 );
	mpg123_format_none( h );
	const long *rates = nullptr;
	size_t count = 0;
	mpg123_rates( &rates, &count );
	for ( size_t i = 0; i < count; ++i )
		mpg123_format( h, rates[i], MPG123_MONO | MPG123_STEREO, MPG123_ENC_SIGNED_16 );
	// The file read from memory through its reader (the feeder mode keeps the last frames until more data comes).
	struct Mem
	{
		const uint8_t *d;
		int64_t size, pos;
	} mem{ d, int64_t( n ), 0 };
	auto readCb = []( void *u, void *dst, size_t bytes, size_t *got ) -> int {
		auto *m = static_cast<Mem *>( u );
		const size_t left = m->pos < m->size ? size_t( m->size - m->pos ) : 0;
		const size_t k = left < bytes ? left : bytes;
		std::memcpy( dst, m->d + m->pos, k );
		m->pos += int64_t( k );
		*got = k;
		return 0;
	};
	auto seekCb = []( void *u, int64_t off, int whence ) -> int64_t {
		auto *m = static_cast<Mem *>( u );
		const int64_t p = ( whence == SEEK_SET ? 0 : whence == SEEK_CUR ? m->pos : m->size ) + off;
		if ( p < 0 )
			return -1;
		return m->pos = p; // past the end allowed, as lseek does: reads there return nothing
	};
	bool ok = mpg123_reader64( h, readCb, seekCb, nullptr ) == MPG123_OK && mpg123_open_handle64( h, &mem ) == MPG123_OK;
	if ( !ok && std::getenv( "AUDIODEC_DEBUG" ) )
		std::fprintf( stderr, "mpg123 open: %s\n", mpg123_strerror( h ) );
	unsigned char buf[16384];
	while ( ok )
	{
		size_t done = 0;
		const int r = mpg123_read( h, buf, sizeof( buf ), &done );
		if ( r != MPG123_OK && std::getenv( "AUDIODEC_DEBUG" ) )
			std::fprintf( stderr, "mpg123 read: %d (%s), %zu bytes\n", r, mpg123_plain_strerror( r ), done );
		const auto *s = reinterpret_cast<const int16_t *>( buf );
		o.samples.insert( o.samples.end(), s, s + done / 2 );
		if ( r == MPG123_NEW_FORMAT )
		{
			long rate = 0;
			int channels = 0, enc = 0;
			mpg123_getformat( h, &rate, &channels, &enc );
			o.rate = int( rate );
			o.channels = channels;
		}
		else if ( r == MPG123_NEED_MORE || r == MPG123_DONE )
			break;
		else if ( r != MPG123_OK )
		{
			// A file cut inside its last frame ends in a reader error (59 of the game's files): what was decoded
			// before it is kept, as a player would play it.
			if ( std::getenv( "AUDIODEC_DEBUG" ) )
				std::fprintf( stderr, "mpg123: %s (%d)\n", mpg123_strerror( h ), mpg123_errcode( h ) );
			ok = !o.samples.empty();
			break;
		}
	}
	mpg123_delete( h );
	return ok && o.channels > 0;
}
DECODER_EXPORT( "libmpg123", Run )
#endif

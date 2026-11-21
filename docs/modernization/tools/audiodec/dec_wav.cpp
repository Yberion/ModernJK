// The WAV readers of the survey (DEC_ENGINEWAV, DEC_DRWAV).
#include "bench.h"

#include <cstring>

#if defined( DEC_ENGINEWAV )
// What the engine does (GetWavinfo, src/codemp/client/snd_mem.cpp): the RIFF chunks, PCM only; 8-bit samples become
// 16-bit as its ResampleSfx makes them ((x - 128) << 8).
static uint32_t U32( const uint8_t *p ) { return uint32_t( p[0] ) | uint32_t( p[1] ) << 8 | uint32_t( p[2] ) << 16 | uint32_t( p[3] ) << 24; }
static uint16_t U16( const uint8_t *p ) { return uint16_t( p[0] | p[1] << 8 ); }

static bool Run( const uint8_t *d, size_t n, Pcm &o )
{
	if ( n < 12 || std::memcmp( d, "RIFF", 4 ) || std::memcmp( d + 8, "WAVE", 4 ) )
		return false;
	int format = 0, width = 0;
	for ( size_t i = 12; i + 8 <= n; )
	{
		const uint32_t len = U32( d + i + 4 );
		const uint8_t *c = d + i + 8;
		if ( !std::memcmp( d + i, "fmt ", 4 ) && len >= 16 && i + 8 + 16 <= n )
		{
			format = U16( c );
			o.channels = U16( c + 2 );
			o.rate = int( U32( c + 4 ) );
			width = U16( c + 14 ) / 8;
		}
		else if ( !std::memcmp( d + i, "data", 4 ) )
		{
			if ( format != 1 || ( width != 1 && width != 2 ) || o.channels == 0 )
				return false;
			const size_t bytes = len <= n - i - 8 ? len : n - i - 8;
			if ( width == 2 )
			{
				o.samples.resize( bytes / 2 );
				std::memcpy( o.samples.data(), c, o.samples.size() * 2 );
			}
			else
			{
				o.samples.resize( bytes );
				for ( size_t k = 0; k < bytes; ++k )
					o.samples[k] = int16_t( ( int( c[k] ) - 128 ) << 8 );
			}
			return true;
		}
		i += 8 + len + ( len & 1 );
	}
	return false;
}
DECODER_EXPORT( "the engine's WAV reader", Run )

#elif defined( DEC_DRWAV )
#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

static bool Run( const uint8_t *d, size_t n, Pcm &o )
{
	unsigned int channels = 0, rate = 0;
	drwav_uint64 frames = 0;
	drwav_int16 *s = drwav_open_memory_and_read_pcm_frames_s16( d, n, &channels, &rate, &frames, nullptr );
	if ( !s )
		return false;
	o.channels = int( channels );
	o.rate = int( rate );
	o.samples.assign( s, s + frames * channels );
	drwav_free( s, nullptr );
	return true;
}
DECODER_EXPORT( "dr_wav", Run )
#endif

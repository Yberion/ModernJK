// The decoders for several formats behind one interface (DEC_MINIAUDIO, DEC_NYQUIST). miniaudio's Ogg Vorbis comes
// from stb_vorbis (MA_WITH_STBVORBIS), its Opus from its libopus decoder (MA_WITH_LIBOPUS); libnyquist needs the
// file's extension (NYQ_EXT).
#include "bench.h"

#if defined( DEC_MINIAUDIO )
#define MA_NO_DEVICE_IO
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_THREADING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#ifdef MA_WITH_STBVORBIS
#define STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"
#endif
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#ifdef MA_WITH_STBVORBIS
#undef STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"
#endif
#ifdef MA_WITH_LIBOPUS
#include "extras/decoders/libopus/miniaudio_libopus.h"
#endif

static bool Run( const uint8_t *d, size_t n, Pcm &o )
{
	ma_decoder_config cfg = ma_decoder_config_init( ma_format_s16, 0, 0 );
#ifdef MA_WITH_LIBOPUS
	ma_decoding_backend_vtable *backends[] = { ma_decoding_backend_libopus };
	cfg.ppCustomBackendVTables = backends;
	cfg.customBackendCount = 1;
#endif
	ma_decoder dec;
	if ( ma_decoder_init_memory( d, n, &cfg, &dec ) != MA_SUCCESS )
		return false;
	o.channels = int( dec.outputChannels );
	o.rate = int( dec.outputSampleRate );
	int16_t buf[4096 * 8];
	const ma_uint64 chunk = sizeof( buf ) / sizeof( buf[0] ) / dec.outputChannels;
	bool ok = true;
	for ( ;; )
	{
		ma_uint64 got = 0;
		const ma_result r = ma_decoder_read_pcm_frames( &dec, buf, chunk, &got );
		o.samples.insert( o.samples.end(), buf, buf + got * dec.outputChannels );
		if ( r == MA_AT_END || got < chunk )
			break;
		if ( r != MA_SUCCESS )
		{
			ok = false;
			break;
		}
	}
	ma_decoder_uninit( &dec );
	return ok;
}
DECODER_EXPORT( "miniaudio (ma_decoder)", Run )

#elif defined( DEC_NYQUIST )
#include <libnyquist/Decoders.h>

static bool Run( const uint8_t *d, size_t n, Pcm &o )
{
	try
	{
		static nqr::NyquistIO io;
		nqr::AudioData data;
		const std::vector<uint8_t> buffer( d, d + n ); // its interface takes a vector: the copy is part of its cost
		io.Load( &data, NYQ_EXT, buffer );
		o.channels = data.channelCount;
		o.rate = data.sampleRate;
		o.samples.resize( data.samples.size() );
		for ( size_t i = 0; i < data.samples.size(); ++i )
			o.samples[i] = FloatToS16( data.samples[i] );
		return o.channels > 0;
	}
	catch ( ... )
	{
		return false;
	}
}
DECODER_EXPORT( "libnyquist", Run )
#endif

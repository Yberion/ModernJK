// The audio decoder benchmark of surveys/audio-decoders.md: one executable per decoder, all on this harness.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Decoded sound: interleaved 16-bit samples, what the engine's mixer and OpenAL Soft take.
struct Pcm
{
	int channels = 0;
	int rate = 0;
	std::vector<int16_t> samples;
};

// Defined once per executable by the decoder under test. Returns false if the file is refused or fails.
const char *DecoderName();
bool Decode( const uint8_t *data, size_t size, Pcm &out );

// Defined by the reference decoder when the executable has one (REFERENCE_DECODER), else unused.
const char *ReferenceName();
bool ReferenceDecode( const uint8_t *data, size_t size, Pcm &out );

// The executable's entry points of a decoder file: as the decoder under test, or as the reference (AS_REFERENCE).
#ifdef AS_REFERENCE
#define DECODER_EXPORT( label, fn ) \
	const char *ReferenceName() { return label; } \
	bool ReferenceDecode( const uint8_t *d, size_t n, Pcm &o ) { return fn( d, n, o ); }
#else
#define DECODER_EXPORT( label, fn ) \
	const char *DecoderName() { return label; } \
	bool Decode( const uint8_t *d, size_t n, Pcm &o ) { return fn( d, n, o ); }
#endif

inline uint64_t Fnv( const void *p, size_t n, uint64_t h = 1469598103934665603ull )
{
	const auto *b = static_cast<const uint8_t *>( p );
	for ( size_t i = 0; i < n; ++i )
		h = ( h ^ b[i] ) * 1099511628211ull;
	return h;
}

// The hash of a decoding: its channels, rate and samples.
inline uint64_t HashPcm( const Pcm &p )
{
	uint64_t h = Fnv( &p.channels, sizeof( p.channels ) );
	h = Fnv( &p.rate, sizeof( p.rate ), h );
	return Fnv( p.samples.data(), p.samples.size() * sizeof( int16_t ), h );
}

// Float samples to 16-bit: rounded to nearest, clamped (for the decoders that only give floats).
inline int16_t FloatToS16( float f )
{
	float v = f * 32768.0f;
	v = v < -32768.0f ? -32768.0f : ( v > 32767.0f ? 32767.0f : v );
	return static_cast<int16_t>( v < 0.0f ? v - 0.5f : v + 0.5f );
}

// The harness of the audio decoder benchmark (surveys/audio-decoders.md).
//
// usage: bench_<decoder> <corpus.bin> [--runs N] [--list out.tsv] [--compare] [--core K] [--limit N]
//   --runs N    decodings per file (default 5): the first is the cold time, the fastest the warm time
//   --list      one line per file: name, ok, frames, channels, rate, cold ns, warm ns, hash, then the comparison
//   --compare   also decode with the reference decoder and compare the samples (needs REFERENCE_DECODER)
//   --core K    run on logical processor K only (Windows)
//   --limit N   only the first N files

#include "bench.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

#ifdef _WIN32
#define NOMINMAX // its min and max macros break std::min and std::max with MSVC
#include <windows.h>
#endif

namespace
{
struct Entry
{
	std::string name;
	std::vector<uint8_t> data;
};

bool LoadCorpus( const char *path, std::vector<Entry> &out )
{
	FILE *f = std::fopen( path, "rb" );
	if ( !f )
		return false;
	char magic[4];
	uint32_t count = 0;
	bool ok = std::fread( magic, 1, 4, f ) == 4 && std::memcmp( magic, "ADC1", 4 ) == 0 && std::fread( &count, 4, 1, f ) == 1;
	for ( uint32_t i = 0; ok && i < count; ++i )
	{
		uint16_t nameLen = 0;
		uint32_t dataLen = 0;
		Entry e;
		ok = std::fread( &nameLen, 2, 1, f ) == 1;
		e.name.resize( nameLen );
		ok = ok && std::fread( e.name.data(), 1, nameLen, f ) == nameLen && std::fread( &dataLen, 4, 1, f ) == 1;
		e.data.resize( dataLen );
		ok = ok && std::fread( e.data.data(), 1, dataLen, f ) == dataLen;
		out.push_back( std::move( e ) );
	}
	std::fclose( f );
	return ok;
}

uint64_t NowNs()
{
	return static_cast<uint64_t>(
		std::chrono::duration_cast<std::chrono::nanoseconds>( std::chrono::steady_clock::now().time_since_epoch() ).count() );
}

// How two decodings of one file differ. The decoders may drop the encoder's delay and padding or not (the gapless
// information of the Xing/LAME header) and may add a frame at the start, so the second decoding is aligned on the
// first: the offsets these give (0, 529, 576, 1,105, 1,152, 1,681, 2,304 frames, either way) and the offset of the
// first sample louder than 64, each within 8 frames, the best over the loudest 4,096 frames of the first (a silent
// stretch matches at any offset). A
// decoder can also give the sound with its polarity inverted (every sample negated): both polarities are tried.
struct Diff
{
	bool comparable = false;
	bool inverted = false;	// b is a's sound negated
	long offset = 0;		// frames of b skipped (positive) or of a skipped (negative)
	long frameDelta = 0;	// frames of b minus frames of a
	int maxAbs = 0;
	uint64_t differing = 0;
	double signal = 0.0, noise = 0.0;
};

Diff Compare( const Pcm &a, const Pcm &b )
{
	Diff d;
	if ( a.channels != b.channels || a.rate != b.rate || a.channels == 0 )
		return d;
	const long ch = a.channels;
	const long fa = static_cast<long>( a.samples.size() ) / ch, fb = static_cast<long>( b.samples.size() ) / ch;
	d.frameDelta = fb - fa;
	auto firstLoud = []( const Pcm &p ) -> long {
		for ( size_t i = 0; i < p.samples.size(); ++i )
			if ( std::abs( p.samples[i] ) > 64 )
				return static_cast<long>( i ) / p.channels;
		return 0;
	};
	const long known[] = { 0, 529, 576, 1105, 1152, 1681, 2304 };
	std::vector<long> centres{ firstLoud( b ) - firstLoud( a ) };
	for ( long k : known )
	{
		centres.push_back( k );
		centres.push_back( -k );
	}
	long loud = 0;
	double loudEnergy = -1.0;
	for ( long s = 0; s < fa; s += 1024 )
	{
		double e = 0.0;
		for ( long i = s; i < std::min( fa, s + 4096 ); ++i )
			for ( long c = 0; c < ch; ++c )
				e += double( a.samples[i * ch + c] ) * a.samples[i * ch + c];
		if ( e > loudEnergy )
		{
			loudEnergy = e;
			loud = s;
		}
	}
	double best = -1.0;
	for ( long centre : centres )
		for ( long off = centre - 8; off <= centre + 8; ++off )
		{
			const long lo = std::max( 0L, -off ), hi = std::min( fa, fb - off );
			if ( hi <= lo )
				continue;
			const long start = std::clamp( loud, lo, std::max( lo, hi - 4096 ) ), end = std::min( hi, start + 4096 );
			for ( int sign = 1; sign >= -1; sign -= 2 )
			{
				double sad = 0.0;
				for ( long i = start; i < end; ++i )
					for ( long c = 0; c < ch; ++c )
						sad += std::abs( a.samples[i * ch + c] - sign * b.samples[( i + off ) * ch + c] );
				sad /= double( end - start );
				if ( best < 0.0 || sad < best )
				{
					best = sad;
					d.offset = off;
					d.inverted = sign < 0;
				}
			}
		}
	const int sign = d.inverted ? -1 : 1;
	for ( long i = std::max( 0L, -d.offset ); i < fa && i + d.offset < fb; ++i )
		for ( long c = 0; c < ch; ++c )
		{
			const int x = a.samples[i * ch + c], y = sign * b.samples[( i + d.offset ) * ch + c];
			const int e = std::abs( x - y );
			d.maxAbs = std::max( d.maxAbs, e );
			d.differing += e != 0;
			d.signal += static_cast<double>( x ) * x;
			d.noise += static_cast<double>( e ) * e;
		}
	d.comparable = true;
	return d;
}
} // namespace

int main( int argc, char **argv )
{
	if ( argc < 2 )
	{
		std::fprintf( stderr, "usage: %s <corpus.bin> [--runs N] [--list out.tsv] [--compare] [--core K] [--limit N]\n", argv[0] );
		return 2;
	}
	int runs = 5;
	long limit = -1;
	const char *listPath = nullptr;
	bool compare = false;
	for ( int i = 2; i < argc; ++i )
	{
		if ( !std::strcmp( argv[i], "--runs" ) && i + 1 < argc )
			runs = std::max( 1, std::atoi( argv[++i] ) );
		else if ( !std::strcmp( argv[i], "--list" ) && i + 1 < argc )
			listPath = argv[++i];
		else if ( !std::strcmp( argv[i], "--compare" ) )
			compare = true;
		else if ( !std::strcmp( argv[i], "--limit" ) && i + 1 < argc )
			limit = std::atol( argv[++i] );
		else if ( !std::strcmp( argv[i], "--mutate" ) && i + 1 < argc )
		{
			// --mutate N: decode N damaged copies of each file (cut short, bytes changed, a block overwritten), for
			// the robustness runs under AddressSanitizer; a fault stops the program with the sanitizer's report.
			std::vector<Entry> c;
			if ( !LoadCorpus( argv[1], c ) )
				return 1;
			const int copies = std::atoi( argv[i + 1] );
			uint64_t state = 0x9E3779B97F4A7C15ull;
			auto next = [&state]() {
				uint64_t z = ( state += 0x9E3779B97F4A7C15ull );
				z = ( z ^ ( z >> 30 ) ) * 0xBF58476D1CE4E5B9ull;
				z = ( z ^ ( z >> 27 ) ) * 0x94D049BB133111EBull;
				return z ^ ( z >> 31 );
			};
			long inputs = 0, accepted = 0;
			for ( const Entry &e : c )
				for ( int k = 0; k < copies && !e.data.empty(); ++k )
				{
					std::vector<uint8_t> d = e.data;
					switch ( k % 3 )
					{
					case 0: d.resize( size_t( next() % d.size() ) ); break;
					case 1:
						for ( int b = 1 + int( next() % 16 ); b > 0; --b )
							d[size_t( next() % d.size() )] ^= uint8_t( 1 + next() % 255 );
						break;
					default:
					{
						const size_t at = size_t( next() % d.size() );
						for ( size_t b = at; b < d.size() && b < at + 64; ++b )
							d[b] = uint8_t( next() );
					}
					}
					Pcm p;
					accepted += Decode( d.data(), d.size(), p );
					++inputs;
				}
			std::printf( "decoder %s: %ld damaged inputs, %ld decoded, no fault\n", DecoderName(), inputs, accepted );
			return 0;
		}
		else if ( !std::strcmp( argv[i], "--dump" ) && i + 2 < argc )
		{
			// --dump <file index> <out.raw>: the 16-bit samples of one file, then exit
			std::vector<Entry> c;
			if ( !LoadCorpus( argv[1], c ) )
				return 1;
			const size_t k = size_t( std::atol( argv[i + 1] ) );
			Pcm p;
			if ( k >= c.size() || !Decode( c[k].data.data(), c[k].data.size(), p ) )
				return 1;
			FILE *f = std::fopen( argv[i + 2], "wb" );
			std::fwrite( p.samples.data(), 2, p.samples.size(), f );
			std::fclose( f );
			std::printf( "%s: %d channels, %d Hz, %zu samples\n", c[k].name.c_str(), p.channels, p.rate, p.samples.size() );
			return 0;
		}
		else if ( !std::strcmp( argv[i], "--core" ) && i + 1 < argc )
		{
			const int core = std::atoi( argv[++i] );
#ifdef _WIN32
			SetThreadAffinityMask( GetCurrentThread(), DWORD_PTR( 1 ) << core );
			SetPriorityClass( GetCurrentProcess(), HIGH_PRIORITY_CLASS );
#else
			(void)core;
#endif
		}
	}
#ifndef REFERENCE_DECODER
	if ( compare )
	{
		std::fprintf( stderr, "no reference decoder in this executable\n" );
		return 2;
	}
#endif
	std::vector<Entry> corpus;
	if ( !LoadCorpus( argv[1], corpus ) )
	{
		std::fprintf( stderr, "cannot read %s\n", argv[1] );
		return 1;
	}
	if ( limit >= 0 && static_cast<size_t>( limit ) < corpus.size() )
		corpus.resize( static_cast<size_t>( limit ) );

	FILE *list = listPath ? std::fopen( listPath, "wb" ) : nullptr;
	uint64_t coldTotal = 0, warmTotal = 0, framesTotal = 0, allHash = 1469598103934665603ull;
	double seconds = 0.0;
	long failed = 0, refFailed = 0, identical = 0, differ = 0, notComparable = 0, lengthDiffers = 0, inverted = 0;
	int maxAbs = 0;
	double worstSnr = 1e9, signalAll = 0.0, noiseAll = 0.0;
	std::map<long, long> offsets;
	std::string worstName;
	for ( const Entry &e : corpus )
	{
		Pcm pcm;
		uint64_t t0 = NowNs();
		const bool ok = Decode( e.data.data(), e.data.size(), pcm );
		const uint64_t cold = NowNs() - t0;
		uint64_t warm = cold;
		for ( int r = 1; r < runs && ok; ++r )
		{
			Pcm again;
			t0 = NowNs();
			Decode( e.data.data(), e.data.size(), again );
			warm = std::min( warm, NowNs() - t0 );
		}
		const long frames = pcm.channels ? static_cast<long>( pcm.samples.size() ) / pcm.channels : 0;
		const uint64_t h = ok ? HashPcm( pcm ) : 0;
		allHash = Fnv( &h, sizeof( h ), allHash );
		if ( !ok )
			++failed;
		else
		{
			coldTotal += cold;
			warmTotal += warm;
			framesTotal += static_cast<uint64_t>( frames );
			seconds += pcm.rate ? static_cast<double>( frames ) / pcm.rate : 0.0;
		}
		if ( list )
			std::fprintf( list, "%s\t%d\t%ld\t%d\t%d\t%llu\t%llu\t%016llx", e.name.c_str(), ok ? 1 : 0, frames, pcm.channels, pcm.rate,
						  static_cast<unsigned long long>( cold ), static_cast<unsigned long long>( warm ),
						  static_cast<unsigned long long>( h ) );
#ifdef REFERENCE_DECODER
		if ( compare && ok )
		{
			Pcm ref;
			if ( !ReferenceDecode( e.data.data(), e.data.size(), ref ) )
				++refFailed;
			else
			{
				const Diff d = Compare( ref, pcm );
				if ( !d.comparable )
					++notComparable;
				else
				{
					++offsets[d.offset];
					inverted += d.inverted;
					lengthDiffers += d.frameDelta != 0;
					if ( d.differing == 0 )
						++identical;
					else
						++differ;
					maxAbs = std::max( maxAbs, d.maxAbs );
					signalAll += d.signal;
					noiseAll += d.noise;
					if ( d.noise > 0.0 && d.signal > 0.0 )
					{
						const double snr = 10.0 * std::log10( d.signal / d.noise );
						if ( snr < worstSnr )
						{
							worstSnr = snr;
							worstName = e.name;
						}
					}
				}
				if ( list )
					std::fprintf( list, "\t%d\t%d\t%ld\t%ld\t%ld\t%d\t%llu", d.comparable ? 1 : 0, d.inverted ? 1 : 0, ref.channels ? static_cast<long>( ref.samples.size() ) / ref.channels : 0,
								  d.offset, d.frameDelta, d.maxAbs, static_cast<unsigned long long>( d.differing ) );
			}
		}
#endif
		if ( list )
			std::fprintf( list, "\n" );
	}
	if ( list )
		std::fclose( list );

	std::printf( "decoder %s\n", DecoderName() );
	std::printf( "files %zu, failed %ld, frames %llu, sound %.1f s\n", corpus.size(), failed, static_cast<unsigned long long>( framesTotal ), seconds );
	std::printf( "cold %.1f ms, warm %.1f ms, warm %.0f times real time\n", coldTotal / 1e6, warmTotal / 1e6, warmTotal ? seconds / ( warmTotal / 1e9 ) : 0.0 );
	std::printf( "hash %016llx\n", static_cast<unsigned long long>( allHash ) );
#ifdef REFERENCE_DECODER
	if ( compare )
	{
		std::printf( "against %s: identical %ld, different %ld, not comparable %ld, reference failed %ld, length differs %ld, polarity inverted %ld, max abs diff %d, SNR all %.1f dB, worst %.1f dB (%s)\n",
					 ReferenceName(), identical, differ, notComparable, refFailed, lengthDiffers, inverted, maxAbs,
					 noiseAll > 0.0 ? 10.0 * std::log10( signalAll / noiseAll ) : 999.0, worstSnr, worstName.c_str() );
		std::printf( "offsets (frames: files):" );
		for ( const auto &[off, n] : offsets )
			std::printf( " %ld: %ld", off, n );
		std::printf( "\n" );
	}
#endif
	return 0;
}

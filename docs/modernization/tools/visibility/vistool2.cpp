// TMP: the visibility table from a shared lattice of camera points: near targets by rays between the lattice and the
// target cells' points, far targets by a fan of rays from each lattice point marking the blocks it crosses.
// usage: vistool2 <tris> <export2> <out prefix> <D> <block size> <fan rays>
// Writes <out prefix>0.bin: one row of n bits per cell (the server's TMPvis3load with 1 slice).
#define TINYBVH_IMPLEMENTATION
#include "tiny_bvh.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

struct Cell { float b0[3], b1[3]; int firstPt, numPts, firstReach, numReach; };
static std::vector<Cell> cells;
static std::vector<float> pts, lat;
static std::vector<int> ptCl, latCl, reach;
static std::vector<uint8_t> pvs;
static int clBytes;
static tinybvh::BVH4_CPU bvh;
static long long rays, fanRays;

static inline bool Pvs( int a, int b ) { return a >= 0 && b >= 0 && ( pvs[(size_t)a * clBytes + ( b >> 3 )] >> ( b & 7 ) & 1 ); }
static inline bool Clear( const float *a, const float *b ) {
	rays++;
	const tinybvh::bvhvec3 d( b[0] - a[0], b[1] - a[1], b[2] - a[2] );
	const tinybvh::bvhvec3 o( a[0] + d.x * 1e-4f, a[1] + d.y * 1e-4f, a[2] + d.z * 1e-4f );
	return !bvh.IsOccluded( tinybvh::Ray( o, d, 1.0f - 2e-4f ) );
}
static float BoxDist( const Cell &a, const Cell &b ) {
	float d2 = 0;
	for ( int k = 0; k < 3; k++ ) { const float e = std::max( 0.0f, std::max( a.b0[k] - b.b1[k], b.b0[k] - a.b1[k] ) ); d2 += e * e; }
	return std::sqrt( d2 );
}
template <typename T> static void Read( FILE *f, T *p, size_t n ) { if ( fread( p, sizeof( T ), n, f ) != n ) { printf( "short read\n" ); exit( 1 ); } }

int main( int argc, char **argv ) {
	if ( argc < 7 ) return 1;
	using Clock = std::chrono::steady_clock;
	FILE *f = fopen( argv[1], "rb" ); int nt = 0;
	Read( f, &nt, 1 );
	std::vector<float> raw( (size_t)nt * 9 ); Read( f, raw.data(), raw.size() ); fclose( f );
	f = fopen( argv[2], "rb" );
	int hdr[7]; Read( f, hdr, 7 );
	const int n = hdr[0], np = hdr[2], nl = hdr[3], nr = hdr[4], maxCl = hdr[5]; clBytes = hdr[6];
	cells.resize( n );
	for ( Cell &c : cells ) { Read( f, c.b0, 3 ); Read( f, c.b1, 3 ); int ix[4]; Read( f, ix, 4 ); c.firstPt = ix[0]; c.numPts = ix[1]; c.firstReach = ix[2]; c.numReach = ix[3]; }
	pts.resize( (size_t)np * 3 ); ptCl.resize( np ); lat.resize( (size_t)nl * 3 ); latCl.resize( nl ); reach.resize( nr );
	for ( int i = 0; i < np; i++ ) { Read( f, &pts[(size_t)i * 3], 3 ); Read( f, &ptCl[i], 1 ); }
	for ( int i = 0; i < nl; i++ ) { Read( f, &lat[(size_t)i * 3], 3 ); Read( f, &latCl[i], 1 ); }
	Read( f, reach.data(), reach.size() );
	pvs.resize( (size_t)maxCl * clBytes ); Read( f, pvs.data(), pvs.size() );
	fclose( f );
	const float D = (float)atof( argv[4] ), B = (float)atof( argv[5] );
	const int K = atoi( argv[6] );

	auto tb0 = Clock::now();
	std::vector<tinybvh::bvhvec4> v4( (size_t)nt * 3 );
	for ( size_t i = 0; i < (size_t)nt * 3; i++ ) v4[i] = tinybvh::bvhvec4( raw[i * 3], raw[i * 3 + 1], raw[i * 3 + 2], 0 );
	bvh.Build( v4.data(), nt );
	const double buildMs = std::chrono::duration<double, std::milli>( Clock::now() - tb0 ).count();

	const int rowWords = ( n + 63 ) / 64;
	std::vector<uint64_t> table( (size_t)n * rowWords, 0 );
	auto set = [&]( int a, int b ) { table[(size_t)a * rowWords + ( b >> 6 )] |= 1ull << ( b & 63 ); };

	// near, target by target: each lattice point judged once per target, lazily
	const auto t1 = Clock::now();
	std::unordered_map<long long, std::vector<int>> grid;
	auto key = []( int x, int y, int z ) { return ( (long long)( x + 4096 ) << 40 ) | ( (long long)( y + 4096 ) << 20 ) | (long long)( z + 4096 ); };
	std::vector<int> gx( n ), gy( n ), gz( n );
	for ( int c = 0; c < n; c++ ) {
		gx[c] = (int)std::floor( ( cells[c].b0[0] + cells[c].b1[0] ) * 0.5f / D );
		gy[c] = (int)std::floor( ( cells[c].b0[1] + cells[c].b1[1] ) * 0.5f / D );
		gz[c] = (int)std::floor( ( cells[c].b0[2] + cells[c].b1[2] ) * 0.5f / D );
		grid[key( gx[c], gy[c], gz[c] )].push_back( c );
	}
	std::vector<int> stamp( nl, -1 ); std::vector<uint8_t> val( nl, 0 );
	long long nearPairs = 0, latJudged = 0;
	for ( int t = 0; t < n; t++ ) {
		const Cell &ct = cells[t];
		for ( int dz = -1; dz <= 1; dz++ ) for ( int dy = -1; dy <= 1; dy++ ) for ( int dx = -1; dx <= 1; dx++ ) {
			auto it = grid.find( key( gx[t] + dx, gy[t] + dy, gz[t] + dz ) );
			if ( it == grid.end() ) continue;
			for ( int v : it->second ) {
				if ( v == t ) { set( v, t ); continue; }
				if ( BoxDist( cells[v], ct ) > D ) continue;
				nearPairs++;
				const Cell &cv = cells[v];
				bool vis = false;
				for ( int r = cv.firstReach; r < cv.firstReach + cv.numReach && !vis; r++ ) {
					const int p = reach[r];
					if ( stamp[p] != t ) {
						stamp[p] = t; latJudged++;
						bool any = false;
						for ( int q = ct.firstPt; q < ct.firstPt + ct.numPts && !any; q++ )
							if ( Pvs( latCl[p], ptCl[q] ) ) any = Clear( &lat[(size_t)p * 3], &pts[(size_t)q * 3] );
						val[p] = any;
					}
					vis = val[p];
				}
				if ( vis ) set( v, t );
			}
		}
	}
	const long long nearRays = rays;
	const auto t2 = Clock::now();

	// far: a fan of K rays from each lattice point, marking the blocks it crosses, widened where the rays spread apart
	float bmin[3] = { 1e30f, 1e30f, 1e30f }, bmax[3] = { -1e30f, -1e30f, -1e30f };
	for ( const Cell &c : cells ) for ( int k = 0; k < 3; k++ ) { bmin[k] = std::min( bmin[k], c.b0[k] ); bmax[k] = std::max( bmax[k], c.b1[k] ); }
	const int nbx = (int)std::ceil( ( bmax[0] - bmin[0] ) / B ), nby = (int)std::ceil( ( bmax[1] - bmin[1] ) / B ), nbz = (int)std::ceil( ( bmax[2] - bmin[2] ) / B );
	const int nb = nbx * nby * nbz, bw = ( nb + 63 ) / 64;
	auto blockOf = [&]( const float *p, int *bi ) {
		for ( int k = 0; k < 3; k++ ) bi[k] = (int)std::floor( ( p[k] - bmin[k] ) / B );
	};
	std::vector<float> dirs( (size_t)K * 3 );
	for ( int i = 0; i < K; i++ ) {
		const float z = 1.0f - ( 2.0f * i + 1.0f ) / K, r = std::sqrt( std::max( 0.0f, 1.0f - z * z ) ), phi = 2.39996323f * i;
		dirs[i * 3] = r * std::cos( phi ); dirs[i * 3 + 1] = r * std::sin( phi ); dirs[i * 3 + 2] = z;
	}
	const float theta = std::sqrt( 4.0f * 3.14159265f / K );
	const float maxLen = std::sqrt( ( bmax[0] - bmin[0] ) * ( bmax[0] - bmin[0] ) + ( bmax[1] - bmin[1] ) * ( bmax[1] - bmin[1] ) + ( bmax[2] - bmin[2] ) * ( bmax[2] - bmin[2] ) );
	std::vector<uint64_t> latFar( (size_t)nl * bw, 0 );
	for ( int p = 0; p < nl; p++ ) {
		uint64_t *bits = &latFar[(size_t)p * bw];
		const float *o = &lat[(size_t)p * 3];
		for ( int k = 0; k < K; k++ ) {
			const float *d = &dirs[(size_t)k * 3];
			tinybvh::Ray ray( tinybvh::bvhvec3( o[0], o[1], o[2] ), tinybvh::bvhvec3( d[0], d[1], d[2] ), maxLen );
			bvh.Intersect( ray ); fanRays++;
			const float len = std::min( ray.hit.t, maxLen );
			for ( float s = 0.0f; s <= len; s += B * 0.5f ) {
				const float x[3] = { o[0] + d[0] * s, o[1] + d[1] * s, o[2] + d[2] * s };
				const float rad = s * theta * 0.5f;
				int lo[3], hi[3];
				const float xl[3] = { x[0] - rad, x[1] - rad, x[2] - rad }, xh[3] = { x[0] + rad, x[1] + rad, x[2] + rad };
				blockOf( xl, lo ); blockOf( xh, hi );
				for ( int bz = std::max( 0, lo[2] ); bz <= std::min( nbz - 1, hi[2] ); bz++ )
				for ( int by = std::max( 0, lo[1] ); by <= std::min( nby - 1, hi[1] ); by++ )
				for ( int bx = std::max( 0, lo[0] ); bx <= std::min( nbx - 1, hi[0] ); bx++ ) {
					const int b = ( bz * nby + by ) * nbx + bx;
					bits[b >> 6] |= 1ull << ( b & 63 );
				}
			}
		}
	}
	// each cell's far row: its camera's lattice points together
	std::vector<uint64_t> cellFar( (size_t)n * bw, 0 );
	for ( int v = 0; v < n; v++ )
		for ( int r = cells[v].firstReach; r < cells[v].firstReach + cells[v].numReach; r++ )
			for ( int w = 0; w < bw; w++ ) cellFar[(size_t)v * bw + w] |= latFar[(size_t)reach[r] * bw + w];
	const auto t3 = Clock::now();
	std::vector<int> cellBlock( n );
	for ( int c = 0; c < n; c++ ) {
		const float ctr[3] = { ( cells[c].b0[0] + cells[c].b1[0] ) * 0.5f, ( cells[c].b0[1] + cells[c].b1[1] ) * 0.5f, ( cells[c].b0[2] + cells[c].b1[2] ) * 0.5f };
		int bi[3]; blockOf( ctr, bi );
		cellBlock[c] = ( std::min( nbz - 1, std::max( 0, bi[2] ) ) * nby + std::min( nby - 1, std::max( 0, bi[1] ) ) ) * nbx + std::min( nbx - 1, std::max( 0, bi[0] ) );
	}
	for ( int v = 0; v < n; v++ ) for ( int t = 0; t < n; t++ ) {
		const int b = cellBlock[t];
		if ( ( cellFar[(size_t)v * bw + ( b >> 6 )] >> ( b & 63 ) & 1 ) && BoxDist( cells[v], cells[t] ) > D ) set( v, t );
	}
	long long setBits = 0;
	for ( uint64_t w : table ) setBits += __builtin_popcountll( w );
	printf( "cells %d, lattice %d, reach %.1f per cell, triangles %d (BVH %.0f ms)\n", n, nl, (double)nr / n, nt, buildMs );
	printf( "near (D %.0f): pairs %lld, lattice judgements %lld, rays %lld, %.1f s; far: blocks %d (%.0f units), fan %d rays per lattice point, %lld rays, %.1f s; table set %.1f %%\n",
		D, nearPairs, latJudged, nearRays, std::chrono::duration<double>( t2 - t1 ).count(), nb, B, K, fanRays,
		std::chrono::duration<double>( t3 - t2 ).count(), 100.0 * setBits / ( (double)n * n ) );
	printf( "compact size: near pairs %.2f MB as bits, far rows %d x %d bits = %.2f MB\n", nearPairs / 8.0 / 1048576, n, nb, (double)n * nb / 8 / 1048576 );
	const std::string out = std::string( argv[3] ) + "0.bin";
	f = fopen( out.c_str(), "wb" );
	const int rowBytes = ( n + 7 ) / 8;
	for ( int c = 0; c < n; c++ ) fwrite( &table[(size_t)c * rowWords], 1, rowBytes, f );
	fclose( f );
	return 0;
}

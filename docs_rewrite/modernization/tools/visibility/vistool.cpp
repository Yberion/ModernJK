// TMP: the visibility table computed offline on the drawn triangles (tinybvh BVH4_CPU), from the server's export.
// usage: vistool <tris> <export> <out prefix> full
//        vistool <tris> <export> <out prefix> nearfar <D> <viewer group size> <target block size> <far viewer points> <far target points>
// Writes <out prefix>0.bin: one row of n bits per cell (the server's TMPvis3load with 1 slice).
#define TINYBVH_IMPLEMENTATION
#include "tiny_bvh.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

struct Cell { float b0[3], b1[3]; int firstPt, numPts, firstVp, numVp; };
static std::vector<Cell> cells;
static std::vector<float> pts, vps;
static std::vector<int> ptCl, vpCl;
static std::vector<uint8_t> pvs;
static int nCells, numFloor, maxCl, clBytes;
static tinybvh::BVH4_CPU bvh;
static long long rays;

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
// any ray between the two point sets clear (the first set's points against the second's)
static bool AnyClear( const int *va, int na, const float *pa, const int *ca, const int *vb, int nb, const float *pb, const int *cb ) {
	for ( int i = 0; i < na; i++ ) for ( int j = 0; j < nb; j++ )
		if ( Pvs( ca[va[i]], cb[vb[j]] ) && Clear( &pa[(size_t)va[i] * 3], &pb[(size_t)vb[j] * 3] ) ) return true;
	return false;
}
// greedy farthest-point choice of at most k points among candidates
static std::vector<int> Spread( const std::vector<int> &cand, const std::vector<float> &p, int k ) {
	std::vector<int> out;
	if ( cand.empty() ) return out;
	std::vector<float> dmin( cand.size(), 1e30f );
	size_t pick = 0;
	for ( int n = 0; n < k && n < (int)cand.size(); n++ ) {
		out.push_back( cand[pick] );
		const float *q = &p[(size_t)cand[pick] * 3];
		float best = -1; size_t bi = 0;
		for ( size_t i = 0; i < cand.size(); i++ ) {
			const float *r = &p[(size_t)cand[i] * 3];
			const float d = ( r[0] - q[0] ) * ( r[0] - q[0] ) + ( r[1] - q[1] ) * ( r[1] - q[1] ) + ( r[2] - q[2] ) * ( r[2] - q[2] );
			dmin[i] = std::min( dmin[i], d );
			if ( dmin[i] > best ) { best = dmin[i]; bi = i; }
		}
		if ( best <= 0 ) break;
		pick = bi;
	}
	return out;
}

int main( int argc, char **argv ) {
	using Clock = std::chrono::steady_clock;
	FILE *f = fopen( argv[1], "rb" ); int nt = 0;
	if ( !f || fread( &nt, 4, 1, f ) != 1 ) return 1;
	std::vector<float> raw( (size_t)nt * 9 );
	if ( fread( raw.data(), 4, raw.size(), f ) != raw.size() ) return 1;
	fclose( f );
	f = fopen( argv[2], "rb" );
	int hdr[6];
	if ( !f || fread( hdr, sizeof( hdr ), 1, f ) != 1 ) return 1;
	nCells = hdr[0]; numFloor = hdr[1]; const int np = hdr[2], nv = hdr[3]; maxCl = hdr[4]; clBytes = hdr[5];
	cells.resize( nCells );
	for ( Cell &c : cells ) { fread( c.b0, 4, 3, f ); fread( c.b1, 4, 3, f ); int ix[4]; fread( ix, 4, 4, f ); c.firstPt = ix[0]; c.numPts = ix[1]; c.firstVp = ix[2]; c.numVp = ix[3]; }
	pts.resize( (size_t)np * 3 ); ptCl.resize( np ); vps.resize( (size_t)nv * 3 ); vpCl.resize( nv );
	for ( int i = 0; i < np; i++ ) { fread( &pts[(size_t)i * 3], 4, 3, f ); fread( &ptCl[i], 4, 1, f ); }
	for ( int i = 0; i < nv; i++ ) { fread( &vps[(size_t)i * 3], 4, 3, f ); fread( &vpCl[i], 4, 1, f ); }
	pvs.resize( (size_t)maxCl * clBytes );
	if ( fread( pvs.data(), 1, pvs.size(), f ) != pvs.size() ) return 1;
	fclose( f );

	const auto t0 = Clock::now();
	std::vector<tinybvh::bvhvec4> v4( (size_t)nt * 3 );
	for ( size_t i = 0; i < (size_t)nt * 3; i++ ) v4[i] = tinybvh::bvhvec4( raw[i * 3], raw[i * 3 + 1], raw[i * 3 + 2], 0 );
	bvh.Build( v4.data(), nt );
	const double buildMs = std::chrono::duration<double, std::milli>( Clock::now() - t0 ).count();

	const int rowWords = ( nCells + 63 ) / 64;
	std::vector<uint64_t> table( (size_t)nCells * rowWords, 0 );
	auto set = [&]( int a, int b ) { table[(size_t)a * rowWords + ( b >> 6 )] |= 1ull << ( b & 63 ); };
	std::vector<int> idx;
	const bool full = !strcmp( argv[4], "full" );
	const float D = full ? 1e30f : (float)atof( argv[5] );
	const float Bv = full ? 0 : (float)atof( argv[6] ), Bt = full ? 0 : (float)atof( argv[7] );
	const int farVp = full ? 0 : atoi( argv[8] ), farTp = full ? 0 : atoi( argv[9] );
	const auto t1 = Clock::now();
	long long nearPairs = 0, farPairs = 0, nearRays = 0;
	// near: every viewer point of the cell against every point of the target cell
	// candidates by a grid of size D over the cells' centres
	std::unordered_map<long long, std::vector<int>> grid;
	auto key = []( int x, int y, int z ) { return ( (long long)( x + 4096 ) << 40 ) | ( (long long)( y + 4096 ) << 20 ) | (long long)( z + 4096 ); };
	const float G = full ? 1e30f : std::max( D, 64.0f );
	std::vector<int> gx( nCells ), gy( nCells ), gz( nCells );
	for ( int c = 0; c < nCells; c++ ) {
		gx[c] = full ? 0 : (int)std::floor( ( cells[c].b0[0] + cells[c].b1[0] ) * 0.5f / G );
		gy[c] = full ? 0 : (int)std::floor( ( cells[c].b0[1] + cells[c].b1[1] ) * 0.5f / G );
		gz[c] = full ? 0 : (int)std::floor( ( cells[c].b0[2] + cells[c].b1[2] ) * 0.5f / G );
		grid[key( gx[c], gy[c], gz[c] )].push_back( c );
	}
	std::vector<int> va, tb;
	for ( int v = 0; v < nCells; v++ ) {
		set( v, v );
		const Cell &cv = cells[v];
		va.resize( cv.numVp ); for ( int i = 0; i < cv.numVp; i++ ) va[i] = cv.firstVp + i;
		const int r = full ? 0 : 1;
		for ( int dz = -r; dz <= r; dz++ ) for ( int dy = -r; dy <= r; dy++ ) for ( int dx = -r; dx <= r; dx++ ) {
			auto it = grid.find( key( gx[v] + dx, gy[v] + dy, gz[v] + dz ) );
			if ( it == grid.end() ) continue;
			for ( int t : it->second ) {
				if ( t == v || BoxDist( cv, cells[t] ) > D ) continue;
				nearPairs++;
				const Cell &ct = cells[t];
				tb.resize( ct.numPts ); for ( int i = 0; i < ct.numPts; i++ ) tb[i] = ct.firstPt + i;
				if ( AnyClear( va.data(), (int)va.size(), vps.data(), vpCl.data(), tb.data(), (int)tb.size(), pts.data(), ptCl.data() ) ) set( v, t );
			}
		}
	}
	nearRays = rays;
	const auto t2 = Clock::now();
	long long groupsN = 0, blocksN = 0;
	if ( !full ) {
		// far: viewer groups against target blocks, with a few spread points on each side
		std::unordered_map<long long, int> gid, bid;
		std::vector<int> cellGroup( nCells ), cellBlock( nCells );
		std::vector<std::vector<int>> groupCells, blockCells;
		for ( int c = 0; c < nCells; c++ ) {
			const float cx = ( cells[c].b0[0] + cells[c].b1[0] ) * 0.5f, cy = ( cells[c].b0[1] + cells[c].b1[1] ) * 0.5f, cz = ( cells[c].b0[2] + cells[c].b1[2] ) * 0.5f;
			const long long kg = key( (int)std::floor( cx / Bv ), (int)std::floor( cy / Bv ), (int)std::floor( cz / Bv ) );
			const long long kb = key( (int)std::floor( cx / Bt ), (int)std::floor( cy / Bt ), (int)std::floor( cz / Bt ) );
			auto ig = gid.find( kg ); if ( ig == gid.end() ) { ig = gid.emplace( kg, (int)groupCells.size() ).first; groupCells.emplace_back(); }
			auto ib = bid.find( kb ); if ( ib == bid.end() ) { ib = bid.emplace( kb, (int)blockCells.size() ).first; blockCells.emplace_back(); }
			cellGroup[c] = ig->second; groupCells[ig->second].push_back( c );
			cellBlock[c] = ib->second; blockCells[ib->second].push_back( c );
		}
		groupsN = (long long)groupCells.size(); blocksN = (long long)blockCells.size();
		std::vector<std::vector<int>> gPts( groupsN ), bPts( blocksN );
		for ( long long g = 0; g < groupsN; g++ ) {
			std::vector<int> cand;
			for ( int c : groupCells[g] ) for ( int i = 0; i < cells[c].numVp; i++ ) cand.push_back( cells[c].firstVp + i );
			gPts[g] = Spread( cand, vps, farVp );
		}
		for ( long long b = 0; b < blocksN; b++ ) {
			std::vector<int> cand;
			for ( int c : blockCells[b] ) for ( int i = 0; i < cells[c].numPts; i++ ) cand.push_back( cells[c].firstPt + i );
			bPts[b] = Spread( cand, pts, farTp );
		}
		std::vector<uint8_t> far( (size_t)groupsN * blocksN, 0 );
		for ( long long g = 0; g < groupsN; g++ ) for ( long long b = 0; b < blocksN; b++ ) {
			farPairs++;
			far[(size_t)g * blocksN + b] = AnyClear( gPts[g].data(), (int)gPts[g].size(), vps.data(), vpCl.data(), bPts[b].data(), (int)bPts[b].size(), pts.data(), ptCl.data() );
		}
		for ( int v = 0; v < nCells; v++ ) for ( int t = 0; t < nCells; t++ )
			if ( BoxDist( cells[v], cells[t] ) > D && far[(size_t)cellGroup[v] * blocksN + cellBlock[t]] ) set( v, t );
	}
	const auto t3 = Clock::now();
	long long setBits = 0;
	for ( uint64_t w : table ) setBits += __builtin_popcountll( w );
	printf( "cells %d, triangles %d (BVH %.0f ms); near pairs %lld (%.1f %% of all), rays %lld, %.1f s; far: groups %lld x blocks %lld = %lld pairs, rays %lld, %.1f s; total %.1f s; set %.1f %%\n",
		nCells, nt, buildMs, nearPairs, 100.0 * nearPairs / ( (double)nCells * nCells ), nearRays,
		std::chrono::duration<double>( t2 - t1 ).count(), groupsN, blocksN, farPairs, rays - nearRays, std::chrono::duration<double>( t3 - t2 ).count(),
		std::chrono::duration<double>( t3 - t1 ).count(), 100.0 * setBits / ( (double)nCells * nCells ) );
	const std::string out = std::string( argv[3] ) + "0.bin";
	f = fopen( out.c_str(), "wb" );
	const int rowBytes = ( nCells + 7 ) / 8;
	for ( int c = 0; c < nCells; c++ ) fwrite( &table[(size_t)c * rowWords], 1, rowBytes, f );
	fclose( f );
	return 0;
}

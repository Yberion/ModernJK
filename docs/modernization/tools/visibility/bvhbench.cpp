// TMP: a BVH over a map's drawn opaque triangles, timed on rays recorded from the engine's tracer, verdicts compared.
// usage: bvhbench <tris file> <rays file> [threads]
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <vector>

struct Tri { float v0[3], e1[3], e2[3]; };
struct Node { float bmin[3]; int first; float bmax[3]; int count; }; // count 0: inner node, first = left child

static std::vector<Tri> tris;
static std::vector<int> order;
static std::vector<Node> nodes;
static std::vector<float> cen, tmin, tmax;

static void Bounds( int first, int count, float *mn, float *mx ) {
	for ( int a = 0; a < 3; a++ ) { mn[a] = 1e30f; mx[a] = -1e30f; }
	for ( int i = first; i < first + count; i++ ) {
		const int t = order[i];
		for ( int a = 0; a < 3; a++ ) { mn[a] = std::min( mn[a], tmin[t * 3 + a] ); mx[a] = std::max( mx[a], tmax[t * 3 + a] ); }
	}
}
static float Area( const float *mn, const float *mx ) {
	const float x = mx[0] - mn[0], y = mx[1] - mn[1], z = mx[2] - mn[2];
	return x < 0 ? 0 : 2 * ( x * y + y * z + z * x );
}
static void Build( int ni ) {
	Node &n = nodes[ni];
	if ( n.count <= 4 ) return;
	// binned SAH over the centroids
	float cmn[3] = { 1e30f, 1e30f, 1e30f }, cmx[3] = { -1e30f, -1e30f, -1e30f };
	for ( int i = n.first; i < n.first + n.count; i++ ) for ( int a = 0; a < 3; a++ ) { cmn[a] = std::min( cmn[a], cen[order[i] * 3 + a] ); cmx[a] = std::max( cmx[a], cen[order[i] * 3 + a] ); }
	const int B = 16;
	float bestCost = 1e30f; int bestAxis = -1, bestSplit = 0;
	for ( int a = 0; a < 3; a++ ) {
		if ( cmx[a] - cmn[a] < 1e-6f ) continue;
		float bmn[B][3], bmx[B][3]; int bc[B] = {};
		for ( int b = 0; b < B; b++ ) for ( int k = 0; k < 3; k++ ) { bmn[b][k] = 1e30f; bmx[b][k] = -1e30f; }
		const float scale = B / ( cmx[a] - cmn[a] );
		for ( int i = n.first; i < n.first + n.count; i++ ) {
			const int t = order[i];
			int b = std::min( B - 1, (int)( ( cen[t * 3 + a] - cmn[a] ) * scale ) );
			bc[b]++;
			for ( int k = 0; k < 3; k++ ) { bmn[b][k] = std::min( bmn[b][k], tmin[t * 3 + k] ); bmx[b][k] = std::max( bmx[b][k], tmax[t * 3 + k] ); }
		}
		float lA[B], rA[B]; int lC[B], rC[B];
		float mn[3] = { 1e30f, 1e30f, 1e30f }, mx[3] = { -1e30f, -1e30f, -1e30f }; int c = 0;
		for ( int b = 0; b < B - 1; b++ ) {
			c += bc[b]; for ( int k = 0; k < 3; k++ ) { mn[k] = std::min( mn[k], bmn[b][k] ); mx[k] = std::max( mx[k], bmx[b][k] ); }
			lA[b] = Area( mn, mx ); lC[b] = c;
		}
		for ( int k = 0; k < 3; k++ ) { mn[k] = 1e30f; mx[k] = -1e30f; } c = 0;
		for ( int b = B - 1; b > 0; b-- ) {
			c += bc[b]; for ( int k = 0; k < 3; k++ ) { mn[k] = std::min( mn[k], bmn[b][k] ); mx[k] = std::max( mx[k], bmx[b][k] ); }
			rA[b - 1] = Area( mn, mx ); rC[b - 1] = c;
		}
		for ( int b = 0; b < B - 1; b++ ) {
			const float cost = lA[b] * lC[b] + rA[b] * rC[b];
			if ( lC[b] && rC[b] && cost < bestCost ) { bestCost = cost; bestAxis = a; bestSplit = b; }
		}
	}
	if ( bestAxis < 0 || bestCost >= Area( n.bmin, n.bmax ) * n.count ) return;
	const float scale = B / ( cmx[bestAxis] - cmn[bestAxis] );
	int *mid = std::partition( &order[n.first], &order[n.first] + n.count, [&]( int t ) {
		return std::min( B - 1, (int)( ( cen[t * 3 + bestAxis] - cmn[bestAxis] ) * scale ) ) <= bestSplit; } );
	const int leftCount = (int)( mid - &order[n.first] );
	const int l = (int)nodes.size();
	nodes.push_back( Node() ); nodes.push_back( Node() );
	Node &L = nodes[l], &R = nodes[l + 1];
	L.first = nodes[ni].first; L.count = leftCount;
	R.first = nodes[ni].first + leftCount; R.count = nodes[ni].count - leftCount;
	Bounds( L.first, L.count, L.bmin, L.bmax );
	Bounds( R.first, R.count, R.bmin, R.bmax );
	nodes[ni].first = l; nodes[ni].count = 0;
	Build( l ); Build( l + 1 );
}

// does the segment a-b cross a triangle (strictly between its ends)?
static bool Occluded( const float *a, const float *b ) {
	float d[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
	float inv[3];
	for ( int k = 0; k < 3; k++ ) inv[k] = 1.0f / ( d[k] != 0.0f ? d[k] : 1e-30f );
	int stack[64], sp = 0;
	int ni = 0;
	for ( ;; ) {
		const Node &n = nodes[ni];
		if ( n.count ) {
			for ( int i = n.first; i < n.first + n.count; i++ ) {
				const Tri &t = tris[order[i]];
				const float p[3] = { d[1] * t.e2[2] - d[2] * t.e2[1], d[2] * t.e2[0] - d[0] * t.e2[2], d[0] * t.e2[1] - d[1] * t.e2[0] };
				const float det = t.e1[0] * p[0] + t.e1[1] * p[1] + t.e1[2] * p[2];
				if ( std::fabs( det ) < 1e-12f ) continue;
				const float id = 1.0f / det;
				const float s[3] = { a[0] - t.v0[0], a[1] - t.v0[1], a[2] - t.v0[2] };
				const float u = ( s[0] * p[0] + s[1] * p[1] + s[2] * p[2] ) * id;
				if ( u < 0.0f || u > 1.0f ) continue;
				const float q[3] = { s[1] * t.e1[2] - s[2] * t.e1[1], s[2] * t.e1[0] - s[0] * t.e1[2], s[0] * t.e1[1] - s[1] * t.e1[0] };
				const float v = ( d[0] * q[0] + d[1] * q[1] + d[2] * q[2] ) * id;
				if ( v < 0.0f || u + v > 1.0f ) continue;
				const float tt = ( t.e2[0] * q[0] + t.e2[1] * q[1] + t.e2[2] * q[2] ) * id;
				if ( tt > 1e-4f && tt < 1.0f - 1e-4f ) return true;
			}
		} else {
			int c[2] = { n.first, n.first + 1 }; float tn[2]; bool hit[2];
			for ( int k = 0; k < 2; k++ ) {
				const Node &ch = nodes[c[k]];
				float t0 = 0.0f, t1 = 1.0f;
				for ( int ax = 0; ax < 3; ax++ ) {
					float lo = ( ch.bmin[ax] - a[ax] ) * inv[ax], hi = ( ch.bmax[ax] - a[ax] ) * inv[ax];
					if ( lo > hi ) std::swap( lo, hi );
					t0 = std::max( t0, lo ); t1 = std::min( t1, hi );
				}
				hit[k] = t0 <= t1; tn[k] = t0;
			}
			if ( hit[0] && hit[1] ) {
				const int nearC = tn[0] <= tn[1] ? 0 : 1;
				stack[sp++] = c[1 - nearC]; ni = c[nearC]; continue;
			}
			if ( hit[0] ) { ni = c[0]; continue; }
			if ( hit[1] ) { ni = c[1]; continue; }
		}
		if ( !sp ) return false;
		ni = stack[--sp];
	}
}

int main( int argc, char **argv ) {
	FILE *f = fopen( argv[1], "rb" ); int nt = 0;
	if ( !f || fread( &nt, 4, 1, f ) != 1 ) return 1;
	std::vector<float> raw( (size_t)nt * 9 );
	if ( fread( raw.data(), 4, raw.size(), f ) != raw.size() ) return 1;
	fclose( f );
	const auto b0 = std::chrono::steady_clock::now();
	tris.resize( nt ); cen.resize( (size_t)nt * 3 ); tmin.resize( (size_t)nt * 3 ); tmax.resize( (size_t)nt * 3 ); order.resize( nt );
	for ( int i = 0; i < nt; i++ ) {
		const float *v = &raw[(size_t)i * 9];
		for ( int k = 0; k < 3; k++ ) {
			tris[i].v0[k] = v[k]; tris[i].e1[k] = v[3 + k] - v[k]; tris[i].e2[k] = v[6 + k] - v[k];
			tmin[i * 3 + k] = std::min( { v[k], v[3 + k], v[6 + k] } ); tmax[i * 3 + k] = std::max( { v[k], v[3 + k], v[6 + k] } );
			cen[i * 3 + k] = ( tmin[i * 3 + k] + tmax[i * 3 + k] ) * 0.5f;
		}
		order[i] = i;
	}
	nodes.reserve( (size_t)nt * 2 );
	nodes.push_back( Node() ); nodes[0].first = 0; nodes[0].count = nt;
	Bounds( 0, nt, nodes[0].bmin, nodes[0].bmax );
	Build( 0 );
	const double buildMs = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - b0 ).count();

	f = fopen( argv[2], "rb" );
	if ( !f ) return 1;
	std::vector<float> rp; std::vector<uint8_t> rr;
	float rec[6]; uint8_t r;
	while ( fread( rec, sizeof( rec ), 1, f ) == 1 && fread( &r, 1, 1, f ) == 1 ) { rp.insert( rp.end(), rec, rec + 6 ); rr.push_back( r ); }
	fclose( f );
	const size_t n = rr.size();
	std::vector<uint8_t> occ( n );
	double best = 1e30;
	for ( int rep = 0; rep < 3; rep++ ) {
		const auto t0 = std::chrono::steady_clock::now();
		for ( size_t i = 0; i < n; i++ ) occ[i] = Occluded( &rp[i * 6], &rp[i * 6 + 3] );
		best = std::min( best, std::chrono::duration<double, std::nano>( std::chrono::steady_clock::now() - t0 ).count() );
	}
	const int threads = argc > 3 ? atoi( argv[3] ) : (int)std::thread::hardware_concurrency();
	const auto m0 = std::chrono::steady_clock::now();
	{
		std::vector<std::thread> pool; std::atomic<size_t> next( 0 );
		for ( int k = 0; k < threads; k++ ) pool.emplace_back( [&]() {
			for ( ;; ) {
				const size_t s = next.fetch_add( 65536 );
				if ( s >= n ) return;
				for ( size_t i = s; i < std::min( n, s + 65536 ); i++ ) occ[i] = Occluded( &rp[i * 6], &rp[i * 6 + 3] );
			}
		} );
		for ( auto &t : pool ) t.join();
	}
	const double mt = std::chrono::duration<double, std::nano>( std::chrono::steady_clock::now() - m0 ).count();
	size_t bvhClearCmBlocked = 0, bvhBlockedCmClear = 0, clear = 0;
	int shownA = 0, shownB = 0;
	for ( size_t i = 0; i < n; i++ ) {
		const bool bc = !occ[i], cc = rr[i] != 0;
		clear += bc;
		if ( bc && !cc ) { bvhClearCmBlocked++; if ( shownA++ < 4 ) printf( "  clear for the BVH, blocked for the engine: %.0f %.0f %.0f -> %.0f %.0f %.0f\n", rp[i*6], rp[i*6+1], rp[i*6+2], rp[i*6+3], rp[i*6+4], rp[i*6+5] ); }
		if ( !bc && cc ) { bvhBlockedCmClear++; if ( shownB++ < 4 ) printf( "  blocked for the BVH, clear for the engine: %.0f %.0f %.0f -> %.0f %.0f %.0f\n", rp[i*6], rp[i*6+1], rp[i*6+2], rp[i*6+3], rp[i*6+4], rp[i*6+5] ); }
	}
	printf( "triangles %d, nodes %zu, build %.0f ms; rays %zu: %.0f ns per ray on one thread, %.1f ns per ray on %d threads; clear %zu; clear for the BVH and blocked for the engine %zu (%.3f %%), the reverse %zu (%.3f %%)\n",
		nt, nodes.size(), buildMs, n, best / n, mt / n, threads, clear, bvhClearCmBlocked, 100.0 * bvhClearCmBlocked / n, bvhBlockedCmClear, 100.0 * bvhBlockedCmClear / n );
	return 0;
}

// TMP: tinybvh and madmann91/bvh v2 on the recorded rays: build time, ns per occlusion ray on one thread, verdicts against the engine.
// usage: libbench <tris file> <rays file>
#define TINYBVH_IMPLEMENTATION
#include "tiny_bvh.h"
#include <bvh/v2/bvh.h>
#include <bvh/v2/vec.h>
#include <bvh/v2/ray.h>
#include <bvh/v2/node.h>
#include <bvh/v2/default_builder.h>
#include <bvh/v2/thread_pool.h>
#include <bvh/v2/stack.h>
#include <bvh/v2/tri.h>
#include <chrono>
#include <cstdio>
#include <vector>

using Clock = std::chrono::steady_clock;
static double Ms( Clock::time_point a ) { return std::chrono::duration<double, std::milli>( Clock::now() - a ).count(); }

static std::vector<float> rp; static std::vector<uint8_t> rr;

template <typename F> static void Run( const char *name, double buildMs, F occluded ) {
	const size_t n = rr.size();
	std::vector<uint8_t> occ( n );
	double best = 1e30;
	for ( int rep = 0; rep < 3; rep++ ) {
		const auto t0 = Clock::now();
		for ( size_t i = 0; i < n; i++ ) occ[i] = occluded( &rp[i * 6], &rp[i * 6 + 3] );
		best = std::min( best, std::chrono::duration<double, std::nano>( Clock::now() - t0 ).count() );
	}
	size_t a = 0, b = 0;
	for ( size_t i = 0; i < n; i++ ) { const bool clear = !occ[i], cm = rr[i] != 0; a += clear && !cm; b += !clear && cm; }
	printf( "%-28s build %7.1f ms, %6.1f ns per ray; clear here and blocked for the engine %zu, the reverse %zu\n", name, buildMs, best / n, a, b );
}

int main( int argc, char **argv ) {
	FILE *f = fopen( argv[1], "rb" ); int nt = 0;
	if ( !f || fread( &nt, 4, 1, f ) != 1 ) return 1;
	std::vector<float> raw( (size_t)nt * 9 );
	if ( fread( raw.data(), 4, raw.size(), f ) != raw.size() ) return 1;
	fclose( f );
	f = fopen( argv[2], "rb" );
	if ( !f ) return 1;
	float rec[6]; uint8_t r;
	while ( fread( rec, sizeof( rec ), 1, f ) == 1 && fread( &r, 1, 1, f ) == 1 ) { rp.insert( rp.end(), rec, rec + 6 ); rr.push_back( r ); }
	fclose( f );
	printf( "triangles %d, rays %zu\n", nt, rr.size() );

	// tinybvh: vertices as bvhvec4, three per triangle
	std::vector<tinybvh::bvhvec4> v4( (size_t)nt * 3 );
	for ( size_t i = 0; i < (size_t)nt * 3; i++ ) v4[i] = tinybvh::bvhvec4( raw[i * 3], raw[i * 3 + 1], raw[i * 3 + 2], 0 );
	auto tray = []( const float *a, const float *b ) {
		const tinybvh::bvhvec3 d( b[0] - a[0], b[1] - a[1], b[2] - a[2] );
		const tinybvh::bvhvec3 o( a[0] + d.x * 1e-4f, a[1] + d.y * 1e-4f, a[2] + d.z * 1e-4f );
		return tinybvh::Ray( o, d, 1.0f - 2e-4f );
	};
	{
		tinybvh::BVH bvh; auto t0 = Clock::now(); bvh.Build( v4.data(), nt ); const double ms = Ms( t0 );
		Run( "tinybvh BVH (Build)", ms, [&]( const float *a, const float *b ) { return bvh.IsOccluded( tray( a, b ) ); } );
	}
	{
		tinybvh::BVH bvh; auto t0 = Clock::now(); bvh.BuildHQ( v4.data(), nt ); const double ms = Ms( t0 );
		Run( "tinybvh BVH (BuildHQ)", ms, [&]( const float *a, const float *b ) { return bvh.IsOccluded( tray( a, b ) ); } );
	}
#ifdef BVH_USESSE
	{
		tinybvh::BVH4_CPU bvh; auto t0 = Clock::now(); bvh.Build( v4.data(), nt ); const double ms = Ms( t0 );
		Run( "tinybvh BVH4_CPU (Build)", ms, [&]( const float *a, const float *b ) { return bvh.IsOccluded( tray( a, b ) ); } );
	}
	{
		tinybvh::BVH4_CPU bvh; auto t0 = Clock::now(); bvh.BuildHQ( v4.data(), nt ); const double ms = Ms( t0 );
		Run( "tinybvh BVH4_CPU (BuildHQ)", ms, [&]( const float *a, const float *b ) { return bvh.IsOccluded( tray( a, b ) ); } );
	}
#endif
#ifdef BVH_USEAVX2
	{
		tinybvh::BVH8_CPU bvh; auto t0 = Clock::now(); bvh.Build( v4.data(), nt ); const double ms = Ms( t0 );
		Run( "tinybvh BVH8_CPU (Build)", ms, [&]( const float *a, const float *b ) { return bvh.IsOccluded( tray( a, b ) ); } );
	}
	{
		tinybvh::BVH8_CPU bvh; auto t0 = Clock::now(); bvh.BuildHQ( v4.data(), nt ); const double ms = Ms( t0 );
		Run( "tinybvh BVH8_CPU (BuildHQ)", ms, [&]( const float *a, const float *b ) { return bvh.IsOccluded( tray( a, b ) ); } );
	}
#endif

	// madmann91/bvh v2, high quality builder, any-hit traversal over precomputed triangles
	{
		using Vec3 = bvh::v2::Vec<float, 3>; using BBox = bvh::v2::BBox<float, 3>; using Tri = bvh::v2::Tri<float, 3>;
		using Node = bvh::v2::Node<float, 3>; using Bvh = bvh::v2::Bvh<Node>; using Ray = bvh::v2::Ray<float, 3>;
		std::vector<Tri> tris( nt );
		for ( int i = 0; i < nt; i++ ) {
			const float *t = &raw[(size_t)i * 9];
			tris[i] = Tri( Vec3( t[0], t[1], t[2] ), Vec3( t[3], t[4], t[5] ), Vec3( t[6], t[7], t[8] ) );
		}
		for ( int q = 0; q < 2; q++ ) {
			auto t0 = Clock::now();
			bvh::v2::ThreadPool pool( 1 );
			std::vector<BBox> boxes( nt ); std::vector<Vec3> centers( nt );
			for ( int i = 0; i < nt; i++ ) { boxes[i] = tris[i].get_bbox(); centers[i] = tris[i].get_center(); }
			typename bvh::v2::DefaultBuilder<Node>::Config config;
			config.quality = q ? bvh::v2::DefaultBuilder<Node>::Quality::High : bvh::v2::DefaultBuilder<Node>::Quality::Medium;
			Bvh tree = bvh::v2::DefaultBuilder<Node>::build( pool, boxes, centers, config );
			std::vector<bvh::v2::PrecomputedTri<float>> pt( nt );
			for ( int i = 0; i < nt; i++ ) pt[i] = tris[tree.prim_ids[i]];
			const double ms = Ms( t0 );
			Run( q ? "bvh v2 (High)" : "bvh v2 (Medium)", ms, [&]( const float *a, const float *b ) {
				Ray ray( Vec3( a[0], a[1], a[2] ), Vec3( b[0] - a[0], b[1] - a[1], b[2] - a[2] ), 1e-4f, 1.0f - 1e-4f );
				bvh::v2::SmallStack<Bvh::Index, 64> stack;
				bool hit = false;
				tree.intersect<true, false>( ray, tree.get_root().index, stack, [&]( size_t begin, size_t end ) {
					for ( size_t i = begin; i < end; ++i ) if ( pt[i].intersect( ray ) ) { hit = true; return true; }
					return false;
				} );
				return hit;
			} );
		}
	}
	return 0;
}

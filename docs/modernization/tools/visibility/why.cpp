// TMP: for the rays the drawn triangles block and the engine's tracer does not, which surfaces block them.
// usage: why <tris file> <rays file>   (reads <tris file>.meta: shader, surface type, surface index per triangle)
#define TINYBVH_IMPLEMENTATION
#include "tiny_bvh.h"
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>

int main( int, char **argv ) {
	FILE *f = fopen( argv[1], "rb" ); int nt = 0;
	if ( !f || fread( &nt, 4, 1, f ) != 1 ) return 1;
	std::vector<float> raw( (size_t)nt * 9 );
	if ( fread( raw.data(), 4, raw.size(), f ) != raw.size() ) return 1;
	fclose( f );
	std::vector<std::string> shader( nt ); std::vector<int> stype( nt ), surf( nt );
	{
		std::ifstream m( std::string( argv[1] ) + ".meta" ); std::string line; int i = 0;
		while ( std::getline( m, line ) && i < nt ) { std::istringstream ss( line ); ss >> shader[i] >> stype[i] >> surf[i]; i++; }
	}
	std::vector<tinybvh::bvhvec4> v4( (size_t)nt * 3 );
	for ( size_t i = 0; i < (size_t)nt * 3; i++ ) v4[i] = tinybvh::bvhvec4( raw[i * 3], raw[i * 3 + 1], raw[i * 3 + 2], 0 );
	tinybvh::BVH bvh; bvh.Build( v4.data(), nt );
	f = fopen( argv[2], "rb" );
	float rec[6]; uint8_t r;
	std::map<std::string, int> byShader; std::map<int, int> byType, bySurf; int n = 0, nearEnd = 0;
	while ( fread( rec, sizeof( rec ), 1, f ) == 1 && fread( &r, 1, 1, f ) == 1 ) {
		if ( !r ) continue; // the engine says blocked
		const tinybvh::bvhvec3 d( rec[3] - rec[0], rec[4] - rec[1], rec[5] - rec[2] );
		const tinybvh::bvhvec3 o( rec[0] + d.x * 1e-4f, rec[1] + d.y * 1e-4f, rec[2] + d.z * 1e-4f );
		tinybvh::Ray ray( o, d, 1.0f - 2e-4f );
		bvh.Intersect( ray );
		if ( ray.hit.t >= 1.0f - 2e-4f ) continue;
		n++;
		const uint32_t p = ray.hit.prim;
		byShader[shader[p] + ( stype[p] == 2 ? " (patch)" : stype[p] == 3 ? " (soup)" : " (planar)" )]++;
		byType[stype[p]]++; bySurf[surf[p]]++;
		const float len = sqrtf( d.x * d.x + d.y * d.y + d.z * d.z );
		if ( ray.hit.t * len < 8.0f || ( 1.0f - ray.hit.t ) * len < 8.0f ) nearEnd++;
	}
	fclose( f );
	printf( "rays blocked by drawn triangles, clear for the engine: %d (hit within 8 units of an end: %d); planar %d, patch %d, soup %d; surfaces %zu\n",
		n, nearEnd, byType[1], byType[2], byType[3], bySurf.size() );
	std::vector<std::pair<int, std::string>> v;
	for ( auto &kv : byShader ) v.push_back( { kv.second, kv.first } );
	std::sort( v.rbegin(), v.rend() );
	for ( size_t i = 0; i < v.size() && i < 15; i++ ) printf( "  %6d  %s\n", v[i].first, v[i].second.c_str() );
	return 0;
}

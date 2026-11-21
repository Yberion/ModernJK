// TMP: one missed pair looked at closely: which of the viewer's lattice points and which of the target cells' points see each other.
// usage: diag <tris> <tris meta> <export2> ex ey ez tx ty tz
#define TINYBVH_IMPLEMENTATION
#include "tiny_bvh.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

struct Cell { float b0[3], b1[3]; int firstPt, numPts, firstReach, numReach; };
static tinybvh::BVH bvh;
static std::vector<std::string> shader;
static std::string Hit( const float *a, const float *b ) {
	const tinybvh::bvhvec3 d( b[0] - a[0], b[1] - a[1], b[2] - a[2] );
	const tinybvh::bvhvec3 o( a[0] + d.x * 1e-4f, a[1] + d.y * 1e-4f, a[2] + d.z * 1e-4f );
	tinybvh::Ray r( o, d, 1.0f - 2e-4f );
	bvh.Intersect( r );
	if ( r.hit.t >= 1.0f - 2e-4f ) return "clear";
	char buf[160]; snprintf( buf, sizeof( buf ), "blocked at %.0f%% by %s", r.hit.t * 100, shader[r.hit.prim].c_str() );
	return buf;
}

int main( int, char **argv ) {
	FILE *f = fopen( argv[1], "rb" ); int nt = 0; fread( &nt, 4, 1, f );
	std::vector<float> raw( (size_t)nt * 9 ); fread( raw.data(), 4, raw.size(), f ); fclose( f );
	{ std::ifstream m( argv[2] ); std::string line; while ( std::getline( m, line ) ) { std::istringstream ss( line ); std::string s; ss >> s; shader.push_back( s ); } }
	std::vector<tinybvh::bvhvec4> v4( (size_t)nt * 3 );
	for ( size_t i = 0; i < (size_t)nt * 3; i++ ) v4[i] = tinybvh::bvhvec4( raw[i * 3], raw[i * 3 + 1], raw[i * 3 + 2], 0 );
	bvh.Build( v4.data(), nt );
	f = fopen( argv[3], "rb" );
	int hdr[7]; fread( hdr, 4, 7, f );
	const int n = hdr[0], np = hdr[2], nl = hdr[3], nr = hdr[4];
	std::vector<Cell> cells( n );
	for ( Cell &c : cells ) { fread( c.b0, 4, 3, f ); fread( c.b1, 4, 3, f ); int ix[4]; fread( ix, 4, 4, f ); c.firstPt = ix[0]; c.numPts = ix[1]; c.firstReach = ix[2]; c.numReach = ix[3]; }
	std::vector<float> pts( (size_t)np * 3 ), lat( (size_t)nl * 3 ); std::vector<int> cl( 1 ), reach( nr );
	for ( int i = 0; i < np; i++ ) { fread( &pts[(size_t)i * 3], 4, 3, f ); fread( cl.data(), 4, 1, f ); }
	for ( int i = 0; i < nl; i++ ) { fread( &lat[(size_t)i * 3], 4, 3, f ); fread( cl.data(), 4, 1, f ); }
	fread( reach.data(), 4, nr, f ); fclose( f );
	const float eye[3] = { (float)atof( argv[4] ), (float)atof( argv[5] ), (float)atof( argv[6] ) };
	const float tgt[3] = { (float)atof( argv[7] ), (float)atof( argv[8] ), (float)atof( argv[9] ) };
	auto inBox = []( const Cell &c, const float *lo, const float *hi ) {
		for ( int k = 0; k < 3; k++ ) if ( c.b1[k] < lo[k] || c.b0[k] > hi[k] ) return false; return true; };
	// the viewer's cell: the floor cell holding the eye, else any cell
	int from = -1;
	for ( int c = 0; c < n && from < 0; c++ ) if ( inBox( cells[c], eye, eye ) ) from = c;
	printf( "eye %.0f %.0f %.0f in cell %d (%.0f %.0f %.0f .. %.0f %.0f %.0f), %d lattice points reached\n", eye[0], eye[1], eye[2], from,
		cells[from].b0[0], cells[from].b0[1], cells[from].b0[2], cells[from].b1[0], cells[from].b1[1], cells[from].b1[2], cells[from].numReach );
	const float tb[8][3] = { { -15, -15, -24 }, { 15, 15, 40 } };
	const float lo[3] = { tgt[0] - 15 - 58, tgt[1] - 15 - 58, tgt[2] - 24 - 58 }, hi[3] = { tgt[0] + 15 + 58, tgt[1] + 15 + 58, tgt[2] + 40 + 58 };
	(void)tb;
	const float body[3] = { tgt[0], tgt[1], tgt[2] + 8 };
	printf( "eye to the target's body centre: %s\n", Hit( eye, body ).c_str() );
	for ( int c = 0; c < n; c++ ) {
		if ( !inBox( cells[c], lo, hi ) ) continue;
		printf( " target cell %d (%.0f %.0f %.0f .. %.0f %.0f %.0f):\n", c, cells[c].b0[0], cells[c].b0[1], cells[c].b0[2], cells[c].b1[0], cells[c].b1[1], cells[c].b1[2] );
		for ( int q = cells[c].firstPt; q < cells[c].firstPt + cells[c].numPts; q++ ) {
			const float *p = &pts[(size_t)q * 3];
			int seenByLattice = 0;
			std::string firstBlock;
			for ( int r = cells[from].firstReach; r < cells[from].firstReach + cells[from].numReach; r++ ) {
				const std::string h = Hit( &lat[(size_t)reach[r] * 3], p );
				if ( h == "clear" ) seenByLattice++; else if ( firstBlock.empty() ) firstBlock = h;
			}
			printf( "   point %.0f %.0f %.0f: from the eye %s; seen by %d of the viewer's lattice points (%s)\n", p[0], p[1], p[2], Hit( eye, p ).c_str(), seenByLattice, firstBlock.c_str() );
		}
	}
	return 0;
}

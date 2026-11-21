// TMP: the measure's pairs (extended dump) re-judged on the drawn triangles, and the misses sorted by cause.
// usage: truth2 <tris> <pairs dump>
#define TINYBVH_IMPLEMENTATION
#include "tiny_bvh.h"
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

static tinybvh::BVH4_CPU bvh;
static bool Clear( const float *a, const float *b ) {
	const tinybvh::bvhvec3 d( b[0] - a[0], b[1] - a[1], b[2] - a[2] );
	const tinybvh::bvhvec3 o( a[0] + d.x * 1e-4f, a[1] + d.y * 1e-4f, a[2] + d.z * 1e-4f );
	return !bvh.IsOccluded( tinybvh::Ray( o, d, 1.0f - 2e-4f ) );
}
static const char *Ground( int g ) { return g == 1022 ? "world" : g == 1023 ? "air" : "entity"; }

int main( int, char **argv ) {
	FILE *f = fopen( argv[1], "rb" ); int nt = 0;
	if ( !f || fread( &nt, 4, 1, f ) != 1 ) return 1;
	std::vector<float> raw( (size_t)nt * 9 );
	if ( fread( raw.data(), 4, raw.size(), f ) != raw.size() ) return 1;
	fclose( f );
	std::vector<tinybvh::bvhvec4> v4( (size_t)nt * 3 );
	for ( size_t i = 0; i < (size_t)nt * 3; i++ ) v4[i] = tinybvh::bvhvec4( raw[i * 3], raw[i * 3 + 1], raw[i * 3 + 2], 0 );
	bvh.Build( v4.data(), nt );
	f = fopen( argv[2], "rb" );
	if ( !f ) return 1;
	std::map<std::string, int> missedBy, pairsBy;
	int shown = 0, missedEye = 0, pairs = 0, pvsN = 0, sent = 0, truthN = 0, missedAll = 0, missedFar80 = 0;
	uint8_t counts[2], flags;
	float views[64 * 3], tp[16 * 3];
	int extra[8]; float fz[2];
	while ( fread( counts, 1, 2, f ) == 2 && fread( &flags, 1, 1, f ) == 1 ) {
		const int nv = counts[0], ntp = counts[1];
		if ( fread( views, 12, nv, f ) != (size_t)nv || fread( tp, 12, ntp, f ) != (size_t)ntp ) break;
		if ( fread( extra, sizeof( extra ), 1, f ) != 1 || fread( fz, sizeof( fz ), 1, f ) != 1 ) break;
		const bool areas = flags & 2, tab = flags & 4;
		bool eye = false;
		for ( int b = 0; b < ntp && !eye; b++ ) eye = Clear( &views[0], &tp[b * 3] );
		const std::string key = std::string( "viewer on " ) + Ground( extra[0] ) + ", target on " + Ground( extra[1] ) +
			", viewer cell " + ( extra[3] == 0 ? "none" : extra[3] == 1 ? "floor" : "air" );
		bool truth = eye, near80 = eye;
		for ( int a = 1; a < nv && !near80; a++ ) {
			const float dx = views[a * 3] - views[3], dy = views[a * 3 + 1] - views[4], dz = views[a * 3 + 2] - views[5];
			if ( std::sqrt( dx * dx + dy * dy + dz * dz ) > 80.0f ) continue;
			for ( int b = 0; b < ntp && !near80; b++ ) near80 = Clear( &views[a * 3], &tp[b * 3] );
		}
		truth = near80;
		for ( int a = 1; a < nv && !truth; a++ ) for ( int b = 0; b < ntp && !truth; b++ ) truth = Clear( &views[a * 3], &tp[b * 3] );
		pvsN += ( flags & 1 ) != 0; sent += areas && tab; truthN += truth; missedAll += truth && areas && !tab; missedFar80 += truth && !near80 && areas && !tab;
		pairs++; pairsBy[key]++;
		if ( !( eye && areas && !tab ) ) continue;
		missedEye++; missedBy[key]++;
		if ( shown++ < 12 ) {
			const float dx = views[0] - tp[8 * 3], dy = views[1] - tp[8 * 3 + 1], dz = views[2] - tp[8 * 3 + 2];
			printf( "  missed from the eye: eye %.0f %.0f %.0f (cell %d, z %.0f..%.0f), target %.0f %.0f %.0f, %.0f apart; viewer on %d, target on %d; target cells floor %d air %d\n",
				views[0], views[1], views[2], extra[2], fz[0], fz[1], tp[8 * 3], tp[8 * 3 + 1], tp[8 * 3 + 2], std::sqrt( dx * dx + dy * dy + dz * dz ),
				extra[0], extra[1], extra[4], extra[5] );
		}
	}
	fclose( f );
	printf( "pairs %d: PVS+areas %.1f %%, sent (areas and table) %.1f %%, truth %.1f %%; missed %d, from the eye %d, only from a camera point beyond 80 units %d\n",
		pairs, 100.0 * pvsN / pairs, 100.0 * sent / pairs, 100.0 * truthN / pairs, missedAll, missedEye, missedFar80 );
	printf( "by what each one stands on:\n" );
	for ( auto &kv : pairsBy ) printf( "  %-62s pairs %6d, missed from the eye %4d\n", kv.first.c_str(), kv.second, missedBy[kv.first] );
	return 0;
}

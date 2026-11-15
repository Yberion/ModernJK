// TMP: the measure's pairs re-judged on the drawn triangles (tinybvh), so that the table and the truth use one geometry.
// usage: truth <tris> <pairs dump>
#define TINYBVH_IMPLEMENTATION
#include "tiny_bvh.h"
#include <cmath>
#include <cstdio>
#include <vector>

static tinybvh::BVH4_CPU bvh;
static bool Clear( const float *a, const float *b ) {
	const tinybvh::bvhvec3 d( b[0] - a[0], b[1] - a[1], b[2] - a[2] );
	const tinybvh::bvhvec3 o( a[0] + d.x * 1e-4f, a[1] + d.y * 1e-4f, a[2] + d.z * 1e-4f );
	return !bvh.IsOccluded( tinybvh::Ray( o, d, 1.0f - 2e-4f ) );
}

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
	long long bPairs[5] = {}, bSent[5] = {}, bTruth[5] = {}, bMissed[5] = {}, bMissedEye[5] = {};
	long long pairs = 0, pvsN = 0, after = 0, afterAreas = 0, truthN = 0, truthEyeN = 0, missed = 0, missedAreas = 0, missedEye = 0, missedFar = 0, engineDiff = 0;
	uint8_t counts[2], flags;
	float views[64 * 3], tp[16 * 3];
	while ( fread( counts, 1, 2, f ) == 2 && fread( &flags, 1, 1, f ) == 1 ) {
		const int nv = counts[0], ntp = counts[1];
		if ( fread( views, 12, nv, f ) != (size_t)nv || fread( tp, 12, ntp, f ) != (size_t)ntp ) break;
		const bool pvs = flags & 1, areas = flags & 2, tab = flags & 4, truthEngine = flags & 8;
		bool eye = false;
		for ( int b = 0; b < ntp && !eye; b++ ) eye = Clear( &views[0], &tp[b * 3] );
		bool nearCam = eye;
		for ( int a = 1; a < nv && !nearCam; a++ ) {
			const float dx = views[a * 3] - views[3], dy = views[a * 3 + 1] - views[4], dz = views[a * 3 + 2] - views[5];
			if ( std::sqrt( dx * dx + dy * dy + dz * dz ) > 80.0f ) continue;
			for ( int b = 0; b < ntp && !nearCam; b++ ) nearCam = Clear( &views[a * 3], &tp[b * 3] );
		}
		bool truth = nearCam;
		for ( int a = 1; a < nv && !truth; a++ ) for ( int b = 0; b < ntp && !truth; b++ ) truth = Clear( &views[a * 3], &tp[b * 3] );
		pairs++; pvsN += pvs; after += pvs && tab; afterAreas += areas && tab; truthN += truth; truthEyeN += eye;
		missed += truth && pvs && !tab; missedAreas += truth && areas && !tab; missedEye += eye && pvs && !tab;
		missedFar += truth && !nearCam && pvs && !tab; engineDiff += truth != truthEngine;
		{
			const float dx = views[0] - tp[8 * 3], dy = views[1] - tp[8 * 3 + 1], dz = views[2] - tp[8 * 3 + 2];
			const float dist = std::sqrt( dx * dx + dy * dy + dz * dz );
			const int bucket = dist < 512 ? 0 : dist < 1024 ? 1 : dist < 2048 ? 2 : dist < 4096 ? 3 : 4;
			bPairs[bucket]++; bSent[bucket] += areas && tab; bTruth[bucket] += truth; bMissed[bucket] += truth && areas && !tab; bMissedEye[bucket] += eye && areas && !tab;
		}
	}
	fclose( f );
	const double P = pairs ? (double)pairs : 1.0;
	printf( "truth on the drawn triangles: %lld pairs: PVS+areas %.1f %%, PVS and table %.1f %%, areas and table %.1f %%, truth %.1f %% (from the eye %.1f %%); missed %lld (areas and table %lld), from the eye %lld, only from a camera point beyond 80 units %lld; truth differing from the engine's %lld\n",
		pairs, 100 * pvsN / P, 100 * after / P, 100 * afterAreas / P, 100 * truthN / P, 100 * truthEyeN / P, missed, missedAreas, missedEye, missedFar, engineDiff );
	static const char *names[5] = { "< 512", "512-1024", "1024-2048", "2048-4096", ">= 4096" };
	for ( int b = 0; b < 5; b++ ) printf( "  distance %-10s pairs %6lld, sent %6lld, truth %6lld, missed %4lld (from the eye %4lld)\n", names[b], bPairs[b], bSent[b], bTruth[b], bMissed[b], bMissedEye[b] );
	return 0;
}

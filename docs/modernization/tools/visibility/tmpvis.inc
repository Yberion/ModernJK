// TMP visibility table prototype (step 14 part 11): measurement only, removed after.
#include <vector>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include "qcommon/cm_public.h"

namespace tmpvis {

static float s_cell = 64.0f;
static vec3_t s_org;
static int s_nx, s_ny, s_nz;          // cells
static int s_lx, s_ly, s_lz;          // lattice points = cells + 1
static std::vector<int32_t> s_latOpen; // lattice point -> index of open point or -1
static std::vector<float> s_pts;       // open points, xyz
static std::vector<int> s_ptCluster;
static std::vector<int32_t> s_cellOpen; // cell -> open cell index or -1
static std::vector<int> s_openCells;    // open cell index -> linear cell
static int s_rowWords;
static std::vector<uint64_t> s_table;   // open cell rows
static long long s_rays;

static uint32_t s_rng = 12345u;
static uint32_t Rand() { s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5; return s_rng; }
static float RandF() { return ( Rand() >> 8 ) * ( 1.0f / 16777216.0f ); }

static bool OpenPoint( const vec3_t p ) {
	const int leaf = CM_PointLeafnum( p );
	if ( CM_LeafCluster( leaf ) < 0 ) return false;
	return !( CM_PointContents( p, 0 ) & ( CONTENTS_SOLID | CONTENTS_TERRAIN ) );
}

static bool Pvs( int ca, int cb ) {
	if ( ca < 0 || cb < 0 ) return false;
	const byte *v = CM_ClusterPVS( ca );
	return ( v[cb >> 3] & ( 1 << ( cb & 7 ) ) ) != 0;
}

static long long s_nodrawPass, s_transPass;
// a ray is blocked only by an opaque surface that is drawn (terrain is drawn by the terrain system)
static bool Clear( const vec3_t a, const vec3_t b ) {
	vec3_t start, dir;
	VectorCopy( a, start );
	VectorSubtract( b, a, dir );
	const float total = VectorNormalize( dir );
	float done = 0.0f;
	for ( int n = 0; n < 32; n++ ) {
		trace_t tr;
		CM_BoxTrace( &tr, start, b, NULL, NULL, 0, CONTENTS_OPAQUE | CONTENTS_TERRAIN, 0 );
		s_rays++;
		if ( tr.fraction >= 1.0f || tr.allsolid ) return true;
		const bool terrain = ( tr.contents & CONTENTS_TERRAIN ) != 0;
		const bool trans = ( tr.contents & CONTENTS_TRANSLUCENT ) != 0;
		const bool nodraw = ( tr.surfaceFlags & SURF_NODRAW ) != 0;
		if ( terrain || ( !trans && !nodraw ) ) return false;
		if ( trans ) s_transPass++; else s_nodrawPass++;
		vec3_t d; VectorSubtract( tr.endpos, a, d );
		done = VectorLength( d ) + 1.0f;
		if ( done >= total ) return true;
		VectorMA( a, done, dir, start );
	}
	return true;
}

static inline int CellLinear( int i, int j, int k ) { return ( k * s_ny + j ) * s_nx + i; }
static inline int LatLinear( int i, int j, int k ) { return ( k * s_ly + j ) * s_lx + i; }

static int CellOf( const vec3_t p ) {
	const int i = (int)floorf( ( p[0] - s_org[0] ) / s_cell );
	const int j = (int)floorf( ( p[1] - s_org[1] ) / s_cell );
	const int k = (int)floorf( ( p[2] - s_org[2] ) / s_cell );
	if ( i < 0 || j < 0 || k < 0 || i >= s_nx || j >= s_ny || k >= s_nz ) return -1;
	return s_cellOpen[CellLinear( i, j, k )];
}

static inline bool Bit( int a, int b ) { return ( s_table[(size_t)a * s_rowWords + ( b >> 6 )] >> ( b & 63 ) ) & 1u; }
static inline void SetBit( int a, int b ) { s_table[(size_t)a * s_rowWords + ( b >> 6 )] |= 1ull << ( b & 63 ); }

// the open cells having lattice point (i,j,k) as a corner
static int CornerCells( int pt, int *out ) {
	const int i = pt % s_lx, j = ( pt / s_lx ) % s_ly, k = pt / ( s_lx * s_ly );
	int n = 0;
	for ( int dk = -1; dk <= 0; dk++ ) for ( int dj = -1; dj <= 0; dj++ ) for ( int di = -1; di <= 0; di++ ) {
		const int ci = i + di, cj = j + dj, ck = k + dk;
		if ( ci < 0 || cj < 0 || ck < 0 || ci >= s_nx || cj >= s_ny || ck >= s_nz ) continue;
		const int c = s_cellOpen[CellLinear( ci, cj, ck )];
		if ( c >= 0 ) out[n++] = c;
	}
	return n;
}

static void Build_f( void ) {
	if ( !sv.state ) { Com_Printf( "no map\n" ); return; }
	s_cell = Cmd_Argc() > 1 ? atof( Cmd_Argv( 1 ) ) : 64.0f;
	const float ground = Cmd_Argc() > 2 ? atof( Cmd_Argv( 2 ) ) : 0.0f;
	const float pairFrac = Cmd_Argc() > 3 ? atof( Cmd_Argv( 3 ) ) : 1.0f; // < 1: estimate on a share of the pairs
	vec3_t mins, maxs;
	CM_ModelBounds( CM_InlineModel( 0 ), mins, maxs );
	for ( int a = 0; a < 3; a++ ) s_org[a] = mins[a];
	s_nx = (int)ceilf( ( maxs[0] - mins[0] ) / s_cell );
	s_ny = (int)ceilf( ( maxs[1] - mins[1] ) / s_cell );
	s_nz = (int)ceilf( ( maxs[2] - mins[2] ) / s_cell );
	s_lx = s_nx + 1; s_ly = s_ny + 1; s_lz = s_nz + 1;
	Com_Printf( "TMPVIS bounds %.0f %.0f %.0f .. %.0f %.0f %.0f, cell %.0f, cells %d x %d x %d = %d\n",
		mins[0], mins[1], mins[2], maxs[0], maxs[1], maxs[2], s_cell, s_nx, s_ny, s_nz, s_nx * s_ny * s_nz );

	const long long t0 = SV_Microseconds();
	// lattice points: open, or moved within half a cell to an open place
	static const float offs[][3] = {
		{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},
		{1,1,1},{1,1,-1},{1,-1,1},{1,-1,-1},{-1,1,1},{-1,1,-1},{-1,-1,1},{-1,-1,-1} };
	const int nl = s_lx * s_ly * s_lz;
	s_latOpen.assign( nl, -1 );
	s_pts.clear(); s_ptCluster.clear();
	int moved = 0, noGround = 0;
	for ( int k = 0; k < s_lz; k++ ) for ( int j = 0; j < s_ly; j++ ) for ( int i = 0; i < s_lx; i++ ) {
		vec3_t base = { s_org[0] + i * s_cell, s_org[1] + j * s_cell, s_org[2] + k * s_cell };
		vec3_t p; VectorCopy( base, p );
		bool ok = OpenPoint( p );
		if ( !ok ) {
			for ( int r = 1; r <= 2 && !ok; r++ ) {
				const float d = r == 1 ? s_cell * 0.25f : s_cell * 0.5f - 1.0f;
				for ( int o = 0; o < 14 && !ok; o++ ) {
					VectorMA( base, d, offs[o], p );
					ok = OpenPoint( p );
				}
			}
			if ( ok ) moved++;
		}
		if ( !ok ) continue;
		if ( ground > 0.0f ) {
			trace_t tr; vec3_t down = { p[0], p[1], p[2] - ground };
			CM_BoxTrace( &tr, p, down, NULL, NULL, 0, CONTENTS_SOLID | CONTENTS_TERRAIN | CONTENTS_PLAYERCLIP, 0 );
			if ( tr.fraction >= 1.0f ) { noGround++; continue; }
		}
		s_latOpen[LatLinear( i, j, k )] = (int32_t)s_ptCluster.size();
		s_pts.push_back( p[0] ); s_pts.push_back( p[1] ); s_pts.push_back( p[2] );
		s_ptCluster.push_back( CM_LeafCluster( CM_PointLeafnum( p ) ) );
	}
	const int np = (int)s_ptCluster.size();
	// open cells: one open corner at least
	const int nc = s_nx * s_ny * s_nz;
	s_cellOpen.assign( nc, -1 );
	s_openCells.clear();
	for ( int k = 0; k < s_nz; k++ ) for ( int j = 0; j < s_ny; j++ ) for ( int i = 0; i < s_nx; i++ ) {
		bool any = false;
		for ( int c = 0; c < 8 && !any; c++ )
			any = s_latOpen[LatLinear( i + ( c & 1 ), j + ( ( c >> 1 ) & 1 ), k + ( c >> 2 ) )] >= 0;
		if ( any ) { s_cellOpen[CellLinear( i, j, k )] = (int32_t)s_openCells.size(); s_openCells.push_back( CellLinear( i, j, k ) ); }
	}
	const int no = (int)s_openCells.size();
	s_rowWords = ( no + 63 ) / 64;
	s_table.assign( (size_t)no * s_rowWords, 0 );
	const long long t1 = SV_Microseconds();
	// the lattice index of each open point, for its corner cells
	std::vector<int> ptLat( np );
	for ( int l = 0; l < nl; l++ ) if ( s_latOpen[l] >= 0 ) ptLat[s_latOpen[l]] = l;
	std::vector<int> cc( (size_t)np * 8 ); std::vector<uint8_t> ccn( np );
	for ( int p = 0; p < np; p++ ) ccn[p] = (uint8_t)CornerCells( ptLat[p], &cc[(size_t)p * 8] );
	for ( int c = 0; c < no; c++ ) SetBit( c, c );

	s_rays = 0; s_nodrawPass = 0; s_transPass = 0;
	long long pvsPairs = 0, tested = 0, skipped = 0, visible = 0, sampled = 0;
	const uint32_t pairLimit = pairFrac >= 1.0f ? 0xFFFFFFFFu : (uint32_t)( pairFrac * 4294967295.0f );
	for ( int a = 0; a < np; a++ ) {
		const int ca = s_ptCluster[a];
		const byte *pvs = CM_ClusterPVS( ca );
		const float *pa = &s_pts[(size_t)a * 3];
		for ( int b = a + 1; b < np; b++ ) {
			const int cb = s_ptCluster[b];
			if ( !( pvs[cb >> 3] & ( 1 << ( cb & 7 ) ) ) ) continue;
			pvsPairs++;
			if ( pairLimit != 0xFFFFFFFFu && Rand() > pairLimit ) continue;
			sampled++;
			// skip when every cell pair it would set is set already
			bool all = true;
			for ( int x = 0; x < ccn[a] && all; x++ ) for ( int y = 0; y < ccn[b] && all; y++ )
				all = Bit( cc[(size_t)a * 8 + x], cc[(size_t)b * 8 + y] );
			if ( all ) { skipped++; continue; }
			tested++;
			if ( !Clear( pa, &s_pts[(size_t)b * 3] ) ) continue;
			visible++;
			for ( int x = 0; x < ccn[a]; x++ ) for ( int y = 0; y < ccn[b]; y++ ) {
				SetBit( cc[(size_t)a * 8 + x], cc[(size_t)b * 8 + y] );
				SetBit( cc[(size_t)b * 8 + y], cc[(size_t)a * 8 + x] );
			}
		}
	}
	const long long t2 = SV_Microseconds();
	long long setBits = 0, pvsCellPairs = 0;
	for ( size_t w = 0; w < s_table.size(); w++ ) setBits += __builtin_popcountll( s_table[w] );
	Com_Printf( "TMPVIS lattice %d, open %d (moved %d, no ground %d); open cells %d of %d\n", nl, np, moved, noGround, no, nc );
	Com_Printf( "TMPVIS point pairs in PVS %lld, sampled %lld, skipped %lld, traced %lld, visible %lld, rays %lld (nodraw passed %lld, trans passed %lld)\n",
		pvsPairs, sampled, skipped, tested, visible, s_rays, s_nodrawPass, s_transPass );
	Com_Printf( "TMPVIS time: points and cells %.2f s, pairs %.2f s (%.3f us per traced pair)\n",
		( t1 - t0 ) / 1e6, ( t2 - t1 ) / 1e6, tested ? (double)( t2 - t1 ) / tested : 0.0 );
	Com_Printf( "TMPVIS table %d x %d bits = %.2f MB, set %lld (%.2f %%)\n", no, no, (double)no * no / 8.0 / 1048576.0, setBits,
		100.0 * setBits / ( (double)no * no ) );
	(void)pvsCellPairs;
	if ( Cmd_Argc() > 4 ) {
		FILE *f = fopen( Cmd_Argv( 4 ), "wb" );
		if ( f ) {
			for ( int c = 0; c < no; c++ ) fwrite( &s_table[(size_t)c * s_rowWords], 1, ( no + 7 ) / 8, f );
			fclose( f );
			Com_Printf( "TMPVIS written %s\n", Cmd_Argv( 4 ) );
		}
	}
}

static bool RandomOpenPoint( vec3_t p, int *cell ) {
	const int no = (int)s_openCells.size();
	for ( int tries = 0; tries < 100; tries++ ) {
		const int c = s_openCells[Rand() % no];
		const int i = c % s_nx, j = ( c / s_nx ) % s_ny, k = c / ( s_nx * s_ny );
		p[0] = s_org[0] + ( i + RandF() ) * s_cell;
		p[1] = s_org[1] + ( j + RandF() ) * s_cell;
		p[2] = s_org[2] + ( k + RandF() ) * s_cell;
		if ( OpenPoint( p ) ) { *cell = s_cellOpen[c]; return true; }
	}
	return false;
}

static bool Dilated( int a, int bLinear ) {
	const int i = bLinear % s_nx, j = ( bLinear / s_nx ) % s_ny, k = bLinear / ( s_nx * s_ny );
	for ( int dk = -1; dk <= 1; dk++ ) for ( int dj = -1; dj <= 1; dj++ ) for ( int di = -1; di <= 1; di++ ) {
		const int ci = i + di, cj = j + dj, ck = k + dk;
		if ( ci < 0 || cj < 0 || ck < 0 || ci >= s_nx || cj >= s_ny || ck >= s_nz ) continue;
		const int c = s_cellOpen[CellLinear( ci, cj, ck )];
		if ( c >= 0 && Bit( a, c ) ) return true;
	}
	return false;
}

// random pairs of open points: the true ray against the table
static void Check_f( void ) {
	if ( s_openCells.empty() ) { Com_Printf( "build first\n" ); return; }
	const int n = Cmd_Argc() > 1 ? atoi( Cmd_Argv( 1 ) ) : 100000;
	long long pvs = 0, truth = 0, table = 0, fail = 0, failDil = 0, overSend = 0, hidden = 0;
	for ( int t = 0; t < n; t++ ) {
		vec3_t p, q; int cp, cq;
		if ( !RandomOpenPoint( p, &cp ) || !RandomOpenPoint( q, &cq ) ) continue;
		if ( !Pvs( CM_LeafCluster( CM_PointLeafnum( p ) ), CM_LeafCluster( CM_PointLeafnum( q ) ) ) ) continue;
		pvs++;
		const bool tv = Clear( p, q );
		const bool bv = Bit( cp, cq );
		truth += tv; table += bv;
		if ( tv && !bv ) { fail++; if ( !Dilated( cp, s_openCells[cq] ) ) failDil++; }
		if ( !tv ) { hidden++; if ( bv ) overSend++; }
	}
	Com_Printf( "TMPVIS check %d pairs: in PVS %lld, truly visible %lld, table visible %lld, missed %lld (with the target's neighbours %lld), hidden %lld of which table visible %lld (%.1f %%)\n",
		n, pvs, truth, table, fail, failDil, hidden, overSend, hidden ? 100.0 * overSend / hidden : 0.0 );
}

// pairs of players: today (PVS and areas) against the table with the camera's reach, and the truth by rays
static long long g_afterNoPvs1;
static long long g_pairs, g_pvs, g_after, g_afterNoPvs, g_truth, g_fail, g_failNoPvs, g_pvsMissed, g_noCell, g_samples, g_regionCells, g_viewers;
static long long g_evalUs, g_truthUs, g_after1, g_fail1, g_failNoPvs1, g_truthEye, g_eyeOutPvs;
static void Game_f( void ) {
	if ( s_openCells.empty() ) { Com_Printf( "build first\n" ); return; }
	if ( Cmd_Argc() > 1 && !Q_stricmp( Cmd_Argv( 1 ), "print" ) ) {
		Com_Printf( "TMPVIS game %lld samples, %lld pairs: PVS+areas %lld (%.1f %%), after %lld (%.1f %%), after without the eye's PVS %lld, truth %lld (%.1f %%); missed %lld, missed without PVS %lld, truth outside the eye's PVS %lld, no cell %lld; region %.1f cells, table %.1f us per viewer, truth %.1f us per pair\n",
			g_samples, g_pairs, g_pvs, 100.0 * g_pvs / ( g_pairs ? g_pairs : 1 ), g_after, 100.0 * g_after / ( g_pairs ? g_pairs : 1 ), g_afterNoPvs,
			g_truth, 100.0 * g_truth / ( g_pairs ? g_pairs : 1 ), g_fail, g_failNoPvs, g_pvsMissed, g_noCell,
			g_viewers ? (double)g_regionCells / g_viewers : 0.0, g_viewers ? (double)g_evalUs / g_viewers : 0.0, g_pairs ? (double)g_truthUs / g_pairs : 0.0 );
		Com_Printf( "TMPVIS game, one cell more on both sides: after %lld, after without PVS (areas) %lld, missed %lld, missed without PVS %lld; truth from the eye alone %lld, of which outside the eye's PVS %lld\n",
			g_after1, g_afterNoPvs1, g_fail1, g_failNoPvs1, g_truthEye, g_eyeOutPvs );
		return;
	}
	if ( Cmd_Argc() > 1 && !Q_stricmp( Cmd_Argv( 1 ), "reset" ) ) {
		g_pairs = g_pvs = g_after = g_afterNoPvs = g_truth = g_fail = g_failNoPvs = g_pvsMissed = g_noCell = g_samples = g_regionCells = g_viewers = g_evalUs = g_truthUs = 0;
		g_after1 = g_fail1 = g_failNoPvs1 = g_truthEye = g_eyeOutPvs = 0;
		return;
	}
	const float lead = Cmd_Argc() > 1 ? atof( Cmd_Argv( 1 ) ) / 1000.0f : 0.11f; // round trip plus a tick
	const float camRange = 120.0f, camHeight = 32.0f;
	g_samples++;
	std::vector<uint64_t> acc( s_rowWords ), acc1( s_rowWords );
	const int maxc = sv_maxclients->integer;
	static vec3_t dirs[26]; static bool dirsMade;
	if ( !dirsMade ) {
		int n = 0;
		for ( int z = -1; z <= 1; z++ ) for ( int y = -1; y <= 1; y++ ) for ( int x = -1; x <= 1; x++ ) {
			if ( !x && !y && !z ) continue;
			VectorSet( dirs[n], x, y, z ); VectorNormalize( dirs[n] ); n++;
		}
		dirsMade = true;
	}
	for ( int v = 0; v < maxc; v++ ) {
		if ( svs.clients[v].state != CS_ACTIVE ) continue;
		sharedEntity_t *ve = SV_GentityNum( v );
		if ( !ve->r.linked || ( ve->r.svFlags & SVF_NOCLIENT ) ) continue;
		playerState_t *ps = SV_GameClientNum( v );
		vec3_t eye, head;
		VectorCopy( ps->origin, eye ); eye[2] += ps->viewheight;
		VectorCopy( eye, head ); head[2] += camHeight;
		const int leaf = CM_PointLeafnum( eye );
		const int area = CM_LeafArea( leaf ), cluster = CM_LeafCluster( leaf );
		// the viewer's points for the truth: the eye, the camera's target, the camera around it as cgame traces it
		vec3_t views[28]; int nv = 0;
		VectorCopy( eye, views[nv++] ); VectorCopy( head, views[nv++] );
		for ( int d = 0; d < 26; d++ ) {
			trace_t tr; vec3_t end; VectorMA( head, camRange, dirs[d], end );
			CM_BoxTrace( &tr, head, end, NULL, NULL, 0, CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_TERRAIN, 0 );
			VectorMA( tr.endpos, -4.0f, dirs[d], views[nv] ); nv++;
		}
		// the region: the open cells within the camera's reach plus the movement until the snapshot is drawn
		const long long e0 = SV_Microseconds();
		const float speed = VectorLength( ps->velocity );
		const float reach = camRange + speed * lead + s_cell * 0.8661f;
		const int c0 = CellOf( head ), ce = CellOf( eye );
		bool noCell = ( ce < 0 && c0 < 0 );
		std::fill( acc.begin(), acc.end(), 0 );
		std::fill( acc1.begin(), acc1.end(), 0 );
		int region = 0;
		if ( !noCell ) {
			const int from = c0 >= 0 ? c0 : ce;
			int lo[3], hi[3];
			for ( int a = 0; a < 3; a++ ) {
				lo[a] = (int)floorf( ( head[a] - reach - s_cell - s_org[a] ) / s_cell );
				hi[a] = (int)floorf( ( head[a] + reach + s_cell - s_org[a] ) / s_cell );
			}
			for ( int k = lo[2] < 0 ? 0 : lo[2]; k <= hi[2] && k < s_nz; k++ )
			for ( int j = lo[1] < 0 ? 0 : lo[1]; j <= hi[1] && j < s_ny; j++ )
			for ( int i = lo[0] < 0 ? 0 : lo[0]; i <= hi[0] && i < s_nx; i++ ) {
				const int c = s_cellOpen[CellLinear( i, j, k )];
				if ( c < 0 ) continue;
				const vec3_t ctr = { s_org[0] + ( i + 0.5f ) * s_cell, s_org[1] + ( j + 0.5f ) * s_cell, s_org[2] + ( k + 0.5f ) * s_cell };
				vec3_t d; VectorSubtract( ctr, head, d );
				const float dist = VectorLength( d );
				if ( dist > reach + s_cell ) continue;
				if ( c != from && c != ce && !Bit( from, c ) ) continue;
				const uint64_t *row = &s_table[(size_t)c * s_rowWords];
				for ( int w = 0; w < s_rowWords; w++ ) acc1[w] |= row[w];
				if ( dist > reach ) continue;
				region++;
				for ( int w = 0; w < s_rowWords; w++ ) acc[w] |= row[w];
			}
		}
		g_evalUs += SV_Microseconds() - e0;
		g_regionCells += region; g_viewers++;

		for ( int t = 0; t < maxc; t++ ) {
			if ( t == v || svs.clients[t].state != CS_ACTIVE ) continue;
			sharedEntity_t *te = SV_GentityNum( t );
			if ( !te->r.linked || ( te->r.svFlags & SVF_NOCLIENT ) ) continue;
			svEntity_t *sve = SV_SvEntityForGentity( te );
			g_pairs++;
			const bool areas = CM_AreasConnected( area, sve->areanum ) || CM_AreasConnected( area, sve->areanum2 );
			bool pvs = false;
			const byte *bits = CM_ClusterPVS( cluster );
			for ( int i = 0; i < sve->numClusters && !pvs; i++ ) { const int l = sve->clusternums[i]; pvs = ( bits[l >> 3] & ( 1 << ( l & 7 ) ) ) != 0; }
			if ( !pvs && sve->lastCluster && sve->numClusters > 0 ) for ( int l = sve->clusternums[sve->numClusters - 1]; l <= sve->lastCluster && !pvs; l++ ) pvs = ( bits[l >> 3] & ( 1 << ( l & 7 ) ) ) != 0;
			pvs = pvs && areas;
			// the target: its box, the saber's reach, two ticks of its movement
			const long long e1 = SV_Microseconds();
			playerState_t *tps = SV_GameClientNum( t );
			const float grow = 50.0f + VectorLength( tps->velocity ) * 0.02f;
			bool tab = noCell;
			int lo[3], hi[3], anyCell = 0;
			for ( int a = 0; a < 3; a++ ) {
				lo[a] = (int)floorf( ( te->r.absmin[a] - grow - s_org[a] ) / s_cell );
				hi[a] = (int)floorf( ( te->r.absmax[a] + grow - s_org[a] ) / s_cell );
				if ( lo[a] < 0 ) lo[a] = 0;
			}
			for ( int k = lo[2]; k <= hi[2] && k < s_nz && !tab; k++ )
			for ( int j = lo[1]; j <= hi[1] && j < s_ny && !tab; j++ )
			for ( int i = lo[0]; i <= hi[0] && i < s_nx && !tab; i++ ) {
				const int c = s_cellOpen[CellLinear( i, j, k )];
				if ( c < 0 ) continue;
				anyCell++;
				tab = ( acc[c >> 6] >> ( c & 63 ) ) & 1u;
			}
			if ( !anyCell && !noCell ) tab = true;
			bool tab1 = noCell || !anyCell;
			for ( int k = (int)floorf( ( te->r.absmin[2] - grow - s_cell - s_org[2] ) / s_cell ); k <= (int)floorf( ( te->r.absmax[2] + grow + s_cell - s_org[2] ) / s_cell ) && !tab1; k++ )
			for ( int j = (int)floorf( ( te->r.absmin[1] - grow - s_cell - s_org[1] ) / s_cell ); j <= (int)floorf( ( te->r.absmax[1] + grow + s_cell - s_org[1] ) / s_cell ) && !tab1; j++ )
			for ( int i = (int)floorf( ( te->r.absmin[0] - grow - s_cell - s_org[0] ) / s_cell ); i <= (int)floorf( ( te->r.absmax[0] + grow + s_cell - s_org[0] ) / s_cell ) && !tab1; i++ ) {
				if ( i < 0 || j < 0 || k < 0 || i >= s_nx || j >= s_ny || k >= s_nz ) continue;
				const int c = s_cellOpen[CellLinear( i, j, k )];
				if ( c >= 0 ) tab1 = ( acc1[c >> 6] >> ( c & 63 ) ) & 1u;
			}
			if ( noCell || !anyCell ) g_noCell++;
			g_evalUs += SV_Microseconds() - e1;
			// the truth: any viewer point to any point of the target's box
			const long long e2 = SV_Microseconds();
			vec3_t tp[10]; int nt = 0;
			for ( int c = 0; c < 8; c++ ) {
				VectorSet( tp[nt], ( c & 1 ) ? te->r.absmax[0] - 1 : te->r.absmin[0] + 1, ( c & 2 ) ? te->r.absmax[1] - 1 : te->r.absmin[1] + 1,
					( c & 4 ) ? te->r.absmax[2] - 1 : te->r.absmin[2] + 1 ); nt++;
			}
			VectorAdd( te->r.absmin, te->r.absmax, tp[nt] ); VectorScale( tp[nt], 0.5f, tp[nt] ); nt++;
			VectorCopy( tps->origin, tp[nt] ); tp[nt][2] += tps->viewheight; nt++;
			bool truthEye = false;
			for ( int b = 0; b < nt && !truthEye; b++ ) truthEye = Clear( views[0], tp[b] );
			bool truth = truthEye;
			for ( int a = 1; a < nv && !truth; a++ ) for ( int b = 0; b < nt && !truth; b++ ) truth = Clear( views[a], tp[b] );
			g_truthEye += truthEye; if ( truthEye && !pvs && areas ) g_eyeOutPvs++;
			g_after1 += ( pvs && tab1 ); g_afterNoPvs1 += ( areas && tab1 );
			if ( truth && pvs && !tab1 ) g_fail1++;
			if ( truth && areas && !tab1 ) g_failNoPvs1++;
			g_truthUs += SV_Microseconds() - e2;
			g_pvs += pvs; g_truth += truth;
			g_after += ( pvs && tab ); g_afterNoPvs += ( areas && tab );
			if ( truth && !( pvs && tab ) && pvs ) g_fail++;
			if ( truth && !( areas && tab ) && areas ) g_failNoPvs++;
			if ( truth && !pvs && areas ) g_pvsMissed++;
		}
	}
}

} // namespace tmpvis

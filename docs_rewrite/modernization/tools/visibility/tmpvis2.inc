// TMP visibility table on the walkable layer (step 14 part 11): measurement only, removed after.
#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <algorithm>
#include <unordered_map>
#include "qcommon/cm_public.h"

namespace tmpvis2 {

static const float kColumn = 16.0f, kStep = 18.0f, kJump = 426.0f, kNormalJump = 66.0f;
static const vec3_t kMins = { -15, -15, -24 }, kMaxs = { 15, 15, 40 };
static const int kMask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_TERRAIN;

struct Spot { float x, y, z, top; int col; };
struct Cell { float b0[3], b1[3]; int firstPt, numPts; int cl0, cl1; };

static vec3_t s_min, s_max;
static float s_fp = 64.0f, s_air = 128.0f;
static int s_fx, s_fy, s_ax, s_ay, s_az, s_numFloor;
static std::vector<Spot> s_spots;
static std::vector<char> s_reached;
static std::vector<Cell> s_cells;
static std::vector<float> s_pts;
static std::vector<int> s_ptCl;
static std::vector<std::vector<int>> s_fpCells;
static std::vector<int> s_airCell;
static int s_rowWords;
static std::vector<uint64_t> s_table;
static long long s_rays;

static bool InWorld( const vec3_t p ) { return CM_LeafCluster( CM_PointLeafnum( p ) ) >= 0; }
static bool OpenPoint( const vec3_t p ) { return InWorld( p ) && !( CM_PointContents( p, 0 ) & ( CONTENTS_SOLID | CONTENTS_TERRAIN ) ); }
static void Box( trace_t *tr, const vec3_t a, const vec3_t b ) { CM_BoxTrace( tr, a, b, kMins, kMaxs, 0, kMask, 0 ); }
static inline bool PvsBit( int ca, int cb ) {
	if ( ca < 0 || cb < 0 ) return false;
	const byte *v = CM_ClusterPVS( ca );
	return ( v[cb >> 3] & ( 1 << ( cb & 7 ) ) ) != 0;
}
// blocked only by an opaque surface that is drawn (terrain is drawn by the terrain system)
static bool ClearImpl( const vec3_t a, const vec3_t b );
static FILE *s_dump; static uint32_t s_dumpRng = 777u, s_dumpEvery = 8; static long long s_dumped;
static bool Clear( const vec3_t a, const vec3_t b ) {
	const bool r = ClearImpl( a, b );
	if ( s_dump ) {
		s_dumpRng ^= s_dumpRng << 13; s_dumpRng ^= s_dumpRng >> 17; s_dumpRng ^= s_dumpRng << 5;
		if ( s_dumpRng % s_dumpEvery == 0 ) {
			float rec[6] = { a[0], a[1], a[2], b[0], b[1], b[2] }; const uint8_t res = r ? 1 : 0;
			fwrite( rec, sizeof( rec ), 1, s_dump ); fwrite( &res, 1, 1, s_dump ); s_dumped++;
		}
	}
	return r;
}
// TMPvisdump <file> <one ray in n>: record the rays of the next commands; TMPvisdump stop
static void Dump_f( void ) {
	if ( s_dump ) { fclose( s_dump ); s_dump = NULL; Com_Printf( "TMPVIS dumped %lld rays\n", s_dumped ); }
	if ( Cmd_Argc() > 1 && Q_stricmp( Cmd_Argv( 1 ), "stop" ) ) {
		s_dump = fopen( Cmd_Argv( 1 ), "wb" ); s_dumped = 0;
		s_dumpEvery = Cmd_Argc() > 2 ? (uint32_t)atoi( Cmd_Argv( 2 ) ) : 8u;
	}
}
// TMPvisreplay <file>: the engine's tracer timed on recorded rays, and its verdicts checked
static void Replay_f( void ) {
	FILE *f = fopen( Cmd_Argv( 1 ), "rb" );
	if ( !f ) return;
	std::vector<float> pts; std::vector<uint8_t> res;
	float rec[6]; uint8_t r;
	while ( fread( rec, sizeof( rec ), 1, f ) == 1 && fread( &r, 1, 1, f ) == 1 ) { pts.insert( pts.end(), rec, rec + 6 ); res.push_back( r ); }
	fclose( f );
	const size_t n = res.size();
	double best = 1e30; size_t diff = 0, clear = 0; long long traces = 0;
	for ( int rep = 0; rep < 3; rep++ ) {
		s_rays = 0; diff = 0; clear = 0;
		const long long t0 = SV_Microseconds();
		for ( size_t i = 0; i < n; i++ ) {
			const bool c = ClearImpl( &pts[i * 6], &pts[i * 6 + 3] );
			clear += c; diff += ( c != ( res[i] != 0 ) );
		}
		const double us = (double)( SV_Microseconds() - t0 );
		if ( us < best ) best = us;
		traces = s_rays;
	}
	Com_Printf( "TMPVIS replay %zu rays: engine tracer %.0f ns per ray (%lld traces, continuations included), clear %zu, differing from the record %zu\n",
		n, best * 1000.0 / n, traces, clear, diff );
}
static bool ClearImpl( const vec3_t a, const vec3_t b ) {
	vec3_t start, dir;
	VectorCopy( a, start );
	VectorSubtract( b, a, dir );
	const float total = VectorNormalize( dir );
	for ( int n = 0; n < 32; n++ ) {
		trace_t tr;
		CM_BoxTrace( &tr, start, b, NULL, NULL, 0, CONTENTS_OPAQUE | CONTENTS_TERRAIN, 0 );
		s_rays++;
		if ( tr.fraction >= 1.0f || tr.allsolid ) return true;
		const bool terrain = ( tr.contents & CONTENTS_TERRAIN ) != 0;
		const bool trans = ( tr.contents & CONTENTS_TRANSLUCENT ) != 0;
		const bool nodraw = ( tr.surfaceFlags & SURF_NODRAW ) != 0;
		if ( terrain || ( !trans && !nodraw ) ) return false;
		vec3_t d; VectorSubtract( tr.endpos, a, d );
		const float done = VectorLength( d ) + 1.0f;
		if ( done >= total ) return true;
		VectorMA( a, done, dir, start );
	}
	return true;
}
static inline bool Bit( int a, int b ) { return ( s_table[(size_t)a * s_rowWords + ( b >> 6 )] >> ( b & 63 ) ) & 1u; }
static inline void SetBit( int a, int b ) { s_table[(size_t)a * s_rowWords + ( b >> 6 )] |= 1ull << ( b & 63 ); }

static std::vector<int> s_colFirst; static int s_cx, s_cy, s_layerMode; static long long s_jumpReached, s_extraSeeds;
static void FindSpots( void ) {
	s_jumpReached = s_extraSeeds = 0;
	CM_ModelBounds( CM_InlineModel( 0 ), s_min, s_max );
	const int cx = (int)ceilf( ( s_max[0] - s_min[0] ) / kColumn ), cy = (int)ceilf( ( s_max[1] - s_min[1] ) / kColumn );
	s_spots.clear();
	std::vector<int> colFirst( (size_t)cx * cy + 1, 0 );
	for ( int j = 0; j < cy; j++ ) for ( int i = 0; i < cx; i++ ) {
		const int col = j * cx + i;
		colFirst[col] = (int)s_spots.size();
		const float x = s_min[0] + ( i + 0.5f ) * kColumn, y = s_min[1] + ( j + 0.5f ) * kColumn;
		float z = s_max[2] + 64.0f;
		for ( int n = 0; n < 64 && z > s_min[2]; n++ ) {
			trace_t tr; vec3_t a = { x, y, z }, b = { x, y, s_min[2] - 64.0f };
			Box( &tr, a, b );
			if ( tr.allsolid ) { z -= 32.0f; continue; }
			if ( tr.fraction >= 1.0f ) break;
			// walkable floors; with s_layerMode & 4 also the steep surfaces a player slides on (any face turned upwards)
			if ( tr.plane.normal[2] >= ( ( s_layerMode & 4 ) ? 0.05f : 0.7f ) && InWorld( tr.endpos ) ) {
				trace_t up; vec3_t top = { tr.endpos[0], tr.endpos[1], tr.endpos[2] + kJump };
				Box( &up, tr.endpos, top );
				Spot s = { tr.endpos[0], tr.endpos[1], tr.endpos[2], up.endpos[2], col };
				s_spots.push_back( s );
			}
			z = tr.endpos[2] - 65.0f;
		}
	}
	colFirst[(size_t)cx * cy] = (int)s_spots.size();
	const int ns = (int)s_spots.size();
	s_reached.assign( ns, 0 );
	std::vector<int> stack;
	const char *p = CM_EntityString();
	char cls[128] = "", org[128] = "";
	for ( ;; ) {
		const char *tok = COM_Parse( &p );
		if ( !p || !tok[0] ) break;
		if ( !strcmp( tok, "{" ) ) { cls[0] = org[0] = 0; continue; }
		if ( !strcmp( tok, "}" ) ) {
			// seeds: the spawn points; with s_layerMode & 2 also the teleporters' destinations and the targets a push aims at
			const bool spawn = !Q_strncmp( cls, "info_player", 11 ) || !Q_strncmp( cls, "team_CTF_", 9 );
			const bool dest = ( s_layerMode & 2 ) && ( !Q_stricmp( cls, "misc_teleporter_dest" ) || !Q_stricmp( cls, "target_position" ) || !Q_stricmp( cls, "info_notnull" ) );
			if ( dest && org[0] ) {
				// the first spot below the point, whatever the drop
				vec3_t o; sscanf( org, "%f %f %f", &o[0], &o[1], &o[2] );
				const int i = (int)( ( o[0] - s_min[0] ) / kColumn ), j = (int)( ( o[1] - s_min[1] ) / kColumn );
				if ( i >= 0 && j >= 0 && i < cx && j < cy ) {
					const int col = j * cx + i;
					int best = -1; float bd = 1e9f;
					for ( int s = colFirst[col]; s < colFirst[col + 1]; s++ ) {
						const float d = o[2] - s_spots[s].z;
						if ( d > -40.0f && d < bd ) { bd = d; best = s; }
					}
					if ( best >= 0 && !s_reached[best] ) { s_reached[best] = 1; stack.push_back( best ); s_extraSeeds++; }
				}
			}
			if ( spawn && org[0] ) {
				vec3_t o; sscanf( org, "%f %f %f", &o[0], &o[1], &o[2] );
				const int i = (int)( ( o[0] - s_min[0] ) / kColumn ), j = (int)( ( o[1] - s_min[1] ) / kColumn );
				if ( i >= 0 && j >= 0 && i < cx && j < cy ) {
					const int col = j * cx + i;
					int best = -1; float bd = 1e9f;
					for ( int s = colFirst[col]; s < colFirst[col + 1]; s++ ) {
						const float d = o[2] - s_spots[s].z;
						if ( d > -40.0f && d < bd ) { bd = d; best = s; }
					}
					if ( best >= 0 && !s_reached[best] ) { s_reached[best] = 1; stack.push_back( best ); }
				}
			}
			continue;
		}
		char key[128]; Q_strncpyz( key, tok, sizeof( key ) );
		tok = COM_Parse( &p );
		if ( !Q_stricmp( key, "classname" ) ) Q_strncpyz( cls, tok, sizeof( cls ) );
		else if ( !Q_stricmp( key, "origin" ) ) Q_strncpyz( org, tok, sizeof( org ) );
	}
	while ( !stack.empty() ) {
		const int a = stack.back(); stack.pop_back();
		const Spot &sa = s_spots[a];
		const int i = sa.col % cx, j = sa.col / cx;
		for ( int dj = -1; dj <= 1; dj++ ) for ( int di = -1; di <= 1; di++ ) {
			if ( !di && !dj ) continue;
			const int ni = i + di, nj = j + dj;
			if ( ni < 0 || nj < 0 || ni >= cx || nj >= cy ) continue;
			const int col = nj * cx + ni;
			for ( int b = colFirst[col]; b < colFirst[col + 1]; b++ ) {
				if ( s_reached[b] ) continue;
				const Spot &sb = s_spots[b];
				const float dz = sb.z - sa.z;
				if ( dz > sa.top - sa.z + 1.0f ) continue;
				const float h = ( dz > 0.0f ? sb.z : sa.z ) + kStep;
				if ( h > sa.top + kStep ) continue;
				trace_t tr; vec3_t p0 = { sa.x, sa.y, sa.z }, p1 = { sa.x, sa.y, h }, p2 = { sb.x, sb.y, h }, p3 = { sb.x, sb.y, sb.z };
				Box( &tr, p0, p1 ); if ( tr.fraction < 1.0f ) continue;
				Box( &tr, p1, p2 ); if ( tr.fraction < 1.0f ) continue;
				Box( &tr, p2, p3 ); if ( tr.fraction < 1.0f && tr.endpos[2] > sb.z + 1.0f ) continue;
				s_reached[b] = 1; stack.push_back( b );
			}
		}
		// jumps over a gap (s_layerMode & 1): in 16 directions, 32 to 384 units away, a spot not reached yet that the
		// box can reach over the gap: up to an apex under this spot's ceiling, across, then down onto it
		if ( s_layerMode & 1 ) {
			for ( int d = 0; d < 16; d++ ) {
				const float ang = d * ( 2.0f * 3.14159265f / 16.0f ), ux = Q_cosf( ang ), uy = Q_sinf( ang );
				for ( float dist = 32.0f; dist <= 384.0f; dist += 32.0f ) {
					const int ci = (int)( ( sa.x + ux * dist - s_min[0] ) / kColumn ), cj = (int)( ( sa.y + uy * dist - s_min[1] ) / kColumn );
					if ( ci < 0 || cj < 0 || ci >= cx || cj >= cy ) break;
					const int col = cj * cx + ci;
					for ( int b = colFirst[col]; b < colFirst[col + 1]; b++ ) {
						if ( s_reached[b] ) continue;
						const Spot &sb = s_spots[b];
						if ( sb.z > sa.top || sb.z < sa.z - 1024.0f ) continue;
						const float apex = std::min( sa.top, std::max( sa.z, sb.z ) + 64.0f );
						if ( apex < sb.z ) continue;
						trace_t tr; vec3_t p0 = { sa.x, sa.y, sa.z }, p1 = { sa.x, sa.y, apex }, p2 = { sb.x, sb.y, apex }, p3 = { sb.x, sb.y, sb.z };
						Box( &tr, p0, p1 ); if ( tr.fraction < 1.0f ) continue;
						Box( &tr, p1, p2 ); if ( tr.fraction < 1.0f ) continue;
						Box( &tr, p2, p3 ); if ( tr.fraction < 1.0f && tr.endpos[2] > sb.z + 1.0f ) continue;
						s_reached[b] = 1; stack.push_back( b ); s_jumpReached++;
					}
				}
			}
		}
	}
	s_colFirst = colFirst; s_cx = cx; s_cy = cy;
}

static int AddPoint( const vec3_t p ) {
	s_pts.push_back( p[0] ); s_pts.push_back( p[1] ); s_pts.push_back( p[2] );
	s_ptCl.push_back( CM_LeafCluster( CM_PointLeafnum( p ) ) );
	return (int)s_ptCl.size() - 1;
}

static void MakeCells( int perCell ) {
	s_fx = (int)ceilf( ( s_max[0] - s_min[0] ) / s_fp ); s_fy = (int)ceilf( ( s_max[1] - s_min[1] ) / s_fp );
	s_ax = (int)ceilf( ( s_max[0] - s_min[0] ) / s_air ); s_ay = (int)ceilf( ( s_max[1] - s_min[1] ) / s_air );
	s_az = (int)ceilf( ( s_max[2] - s_min[2] + 600.0f ) / s_air );
	s_cells.clear(); s_pts.clear(); s_ptCl.clear();
	s_fpCells.assign( (size_t)s_fx * s_fy, std::vector<int>() );
	s_airCell.assign( (size_t)s_ax * s_ay * s_az, -1 );
	// floor cells: the reached spots of one footprint, grouped by height (a gap over 48 starts another level)
	std::vector<std::vector<int>> byFp( (size_t)s_fx * s_fy );
	for ( int s = 0; s < (int)s_spots.size(); s++ ) {
		if ( !s_reached[s] ) continue;
		const int fi = (int)( ( s_spots[s].x - s_min[0] ) / s_fp ), fj = (int)( ( s_spots[s].y - s_min[1] ) / s_fp );
		byFp[(size_t)fj * s_fx + fi].push_back( s );
	}
	for ( int f = 0; f < s_fx * s_fy; f++ ) {
		std::vector<int> &v = byFp[f];
		if ( v.empty() ) continue;
		std::sort( v.begin(), v.end(), []( int a, int b ) { return s_spots[a].z < s_spots[b].z; } );
		const float cxm = s_min[0] + ( f % s_fx + 0.5f ) * s_fp, cym = s_min[1] + ( f / s_fx + 0.5f ) * s_fp;
		size_t start = 0;
		for ( size_t k = 1; k <= v.size(); k++ ) {
			if ( k < v.size() && s_spots[v[k]].z - s_spots[v[k - 1]].z <= 48.0f ) continue;
			Cell c;
			c.b0[0] = s_min[0] + ( f % s_fx ) * s_fp; c.b1[0] = c.b0[0] + s_fp;
			c.b0[1] = s_min[1] + ( f / s_fx ) * s_fp; c.b1[1] = c.b0[1] + s_fp;
			c.b0[2] = s_spots[v[start]].z + kMins[2];
			c.b1[2] = s_spots[v[k - 1]].z + kMaxs[2] + kNormalJump;
			c.firstPt = (int)s_ptCl.size();
			// samples: the spot nearest the footprint's centre, then the farthest from those taken, at the body's centre and the eye
			std::vector<int> pick;
			for ( int n = 0; n < perCell && n < (int)( k - start ); n++ ) {
				int best = -1; float bd = -1.0f;
				for ( size_t m = start; m < k; m++ ) {
					const Spot &sp = s_spots[v[m]];
					float d;
					if ( pick.empty() ) d = -( ( sp.x - cxm ) * ( sp.x - cxm ) + ( sp.y - cym ) * ( sp.y - cym ) );
					else {
						d = 1e9f;
						for ( int q : pick ) { const Spot &o = s_spots[q]; const float e = ( sp.x - o.x ) * ( sp.x - o.x ) + ( sp.y - o.y ) * ( sp.y - o.y ) + ( sp.z - o.z ) * ( sp.z - o.z ); if ( e < d ) d = e; }
					}
					if ( best < 0 || d > bd ) { bd = d; best = v[m]; }
				}
				pick.push_back( best );
			}
			for ( int q : pick ) {
				const Spot &sp = s_spots[q];
				vec3_t body = { sp.x, sp.y, sp.z + 8.0f }, eye = { sp.x, sp.y, sp.z + 36.0f };
				AddPoint( eye ); AddPoint( body );
			}
			c.numPts = (int)s_ptCl.size() - c.firstPt;
			s_fpCells[f].push_back( (int)s_cells.size() );
			s_cells.push_back( c );
			start = k;
		}
	}
	s_numFloor = (int)s_cells.size();
	// air cells: above each spot's floor cell up to its ceiling or the highest jump
	for ( int s = 0; s < (int)s_spots.size(); s++ ) {
		if ( !s_reached[s] ) continue;
		const Spot &sp = s_spots[s];
		const float lo = sp.z + kMaxs[2] + kNormalJump, hi = sp.top + kMaxs[2];
		if ( hi <= lo ) continue;
		const int i = (int)( ( sp.x - s_min[0] ) / s_air ), j = (int)( ( sp.y - s_min[1] ) / s_air );
		for ( int k = (int)( ( lo - s_min[2] ) / s_air ); k <= (int)( ( hi - s_min[2] ) / s_air ) && k < s_az; k++ ) {
			int &ac = s_airCell[( (size_t)k * s_ay + j ) * s_ax + i];
			if ( ac >= 0 ) continue;
			Cell c;
			c.b0[0] = s_min[0] + i * s_air; c.b1[0] = c.b0[0] + s_air;
			c.b0[1] = s_min[1] + j * s_air; c.b1[1] = c.b0[1] + s_air;
			c.b0[2] = s_min[2] + k * s_air; c.b1[2] = c.b0[2] + s_air;
			vec3_t p = { sp.x, sp.y, c.b0[2] + s_air * 0.5f };
			if ( p[2] < lo ) p[2] = lo;
			if ( p[2] > hi ) p[2] = hi;
			c.firstPt = (int)s_ptCl.size();
			AddPoint( p );
			c.numPts = 1;
			ac = (int)s_cells.size();
			s_cells.push_back( c );
		}
	}
}

// the cells a box touches: floor cells of its footprints whose heights overlap it, and air cells
template <typename F> static void CellsInBox( const vec3_t lo, const vec3_t hi, F f ) {
	int i0 = (int)floorf( ( lo[0] - s_min[0] ) / s_fp ), i1 = (int)floorf( ( hi[0] - s_min[0] ) / s_fp );
	int j0 = (int)floorf( ( lo[1] - s_min[1] ) / s_fp ), j1 = (int)floorf( ( hi[1] - s_min[1] ) / s_fp );
	for ( int j = j0 < 0 ? 0 : j0; j <= j1 && j < s_fy; j++ ) for ( int i = i0 < 0 ? 0 : i0; i <= i1 && i < s_fx; i++ )
		for ( int c : s_fpCells[(size_t)j * s_fx + i] ) if ( s_cells[c].b1[2] >= lo[2] && s_cells[c].b0[2] <= hi[2] ) f( c );
	i0 = (int)floorf( ( lo[0] - s_min[0] ) / s_air ); i1 = (int)floorf( ( hi[0] - s_min[0] ) / s_air );
	j0 = (int)floorf( ( lo[1] - s_min[1] ) / s_air ); j1 = (int)floorf( ( hi[1] - s_min[1] ) / s_air );
	const int k0 = (int)floorf( ( lo[2] - s_min[2] ) / s_air ), k1 = (int)floorf( ( hi[2] - s_min[2] ) / s_air );
	for ( int k = k0 < 0 ? 0 : k0; k <= k1 && k < s_az; k++ ) for ( int j = j0 < 0 ? 0 : j0; j <= j1 && j < s_ay; j++ )
		for ( int i = i0 < 0 ? 0 : i0; i <= i1 && i < s_ax; i++ ) {
			const int c = s_airCell[( (size_t)k * s_ay + j ) * s_ax + i];
			if ( c >= 0 ) f( c );
		}
}
static int CellAt( const vec3_t p ) {
	int found = -1;
	const vec3_t lo = { p[0], p[1], p[2] }, hi = { p[0], p[1], p[2] };
	CellsInBox( lo, hi, [&]( int c ) { if ( found < 0 || c < s_numFloor ) found = c; } );
	return found;
}

static void Build_f( void ) {
	if ( !sv.state ) { Com_Printf( "no map\n" ); return; }
	s_fp = Cmd_Argc() > 1 ? atof( Cmd_Argv( 1 ) ) : 64.0f;
	s_air = Cmd_Argc() > 2 ? atof( Cmd_Argv( 2 ) ) : 128.0f;
	const int perCell = Cmd_Argc() > 3 ? atoi( Cmd_Argv( 3 ) ) : 2;
	const long long t0 = SV_Microseconds();
	FindSpots();
	const long long t1 = SV_Microseconds();
	MakeCells( perCell );
	const int n = (int)s_cells.size();
	s_rowWords = ( n + 63 ) / 64;
	s_table.assign( (size_t)n * s_rowWords, 0 );
	// the clusters of each cell
	for ( Cell &c : s_cells ) {
		c.cl0 = c.cl1 = -1;
		for ( int q = c.firstPt; q < c.firstPt + c.numPts; q++ ) {
			const int cl = s_ptCl[q];
			if ( cl < 0 ) continue;
			if ( c.cl0 < 0 ) c.cl0 = cl; else if ( cl != c.cl0 && c.cl1 < 0 ) c.cl1 = cl;
		}
	}
	const long long t2 = SV_Microseconds();
	s_rays = 0;
	long long pvsPairs = 0, visible = 0;
	for ( int a = 0; a < n; a++ ) {
		SetBit( a, a );
		const Cell &ca = s_cells[a];
		for ( int b = a + 1; b < n; b++ ) {
			const Cell &cb = s_cells[b];
			if ( !( PvsBit( ca.cl0, cb.cl0 ) || PvsBit( ca.cl0, cb.cl1 ) || PvsBit( ca.cl1, cb.cl0 ) || PvsBit( ca.cl1, cb.cl1 ) ) ) continue;
			pvsPairs++;
			bool vis = false;
			for ( int p = ca.firstPt; p < ca.firstPt + ca.numPts && !vis; p++ )
				for ( int q = cb.firstPt; q < cb.firstPt + cb.numPts && !vis; q++ )
					if ( PvsBit( s_ptCl[p], s_ptCl[q] ) ) vis = Clear( &s_pts[(size_t)p * 3], &s_pts[(size_t)q * 3] );
			if ( vis ) { SetBit( a, b ); SetBit( b, a ); visible++; }
		}
	}
	const long long t3 = SV_Microseconds();
	Com_Printf( "TMPVIS2 footprint %.0f, air %.0f, %d spots per cell: spots %d (reached %d), floor cells %d, air cells %d, points %d\n",
		s_fp, s_air, perCell, (int)s_spots.size(), (int)std::count( s_reached.begin(), s_reached.end(), 1 ), s_numFloor, n - s_numFloor, (int)s_ptCl.size() );
	Com_Printf( "TMPVIS2 pairs %lld, in PVS %lld, visible %lld, rays %lld; time: spots %.2f s, cells %.2f s, table %.2f s\n",
		(long long)n * ( n - 1 ) / 2, pvsPairs, visible, s_rays, ( t1 - t0 ) / 1e6, ( t2 - t1 ) / 1e6, ( t3 - t2 ) / 1e6 );
	Com_Printf( "TMPVIS2 table %d x %d bits: %.2f MB, half %.2f MB\n", n, n, (double)n * n / 8 / 1048576, (double)n * n / 16 / 1048576 );
	if ( Cmd_Argc() > 4 ) {
		FILE *f = fopen( Cmd_Argv( 4 ), "wb" );
		if ( f ) { for ( int c = 0; c < n; c++ ) fwrite( &s_table[(size_t)c * s_rowWords], 1, ( n + 7 ) / 8, f ); fclose( f ); }
	}
}

// option b: each cell's row from its own camera positions (cgame's camera: the head raised by 32, then up to 120 units in
// any direction, both traced with a box of 4), so the row already holds what the camera can see
static bool s_camRows;
static std::vector<int> s_vpFirst, s_vpNum, s_vpCl;
static std::vector<float> s_vp;
static const int kCamMask = CONTENTS_SOLID | CONTENTS_TERRAIN | CONTENTS_PLAYERCLIP;

static void AddViewer( const vec3_t eye, bool camera, const vec3_t *dirs, int ndirs ) {
	static const vec3_t cmin = { -4, -4, -4 }, cmax = { 4, 4, 4 };
	auto add = [&]( const vec3_t p ) {
		s_vp.push_back( p[0] ); s_vp.push_back( p[1] ); s_vp.push_back( p[2] );
		s_vpCl.push_back( CM_LeafCluster( CM_PointLeafnum( p ) ) );
	};
	add( eye );
	trace_t tr; vec3_t up = { eye[0], eye[1], eye[2] + 32.0f };
	CM_BoxTrace( &tr, eye, up, cmin, cmax, 0, kCamMask, 0 );
	vec3_t head; VectorCopy( tr.endpos, head );
	add( head );
	if ( !camera ) return;
	for ( int d = 0; d < ndirs; d++ ) {
		vec3_t end; VectorMA( head, 120.0f, dirs[d], end );
		CM_BoxTrace( &tr, head, end, cmin, cmax, 0, kCamMask, 0 );
		add( tr.endpos );
	}
}

static void MakeViewers( int ndirs ) {
	std::vector<float> dirf( (size_t)ndirs * 3 );
	for ( int i = 0; i < ndirs; i++ ) { // a Fibonacci spiral, not the 26 directions of the measure's truth
		const float z = 1.0f - ( 2.0f * i + 1.0f ) / ndirs, r = sqrtf( 1.0f - z * z ), phi = 2.39996323f * i;
		VectorSet( &dirf[(size_t)i * 3], r * Q_cosf( phi ), r * Q_sinf( phi ), z );
	}
	const vec3_t *dirs = (const vec3_t *)dirf.data();
	s_vp.clear(); s_vpCl.clear(); s_vpFirst.clear(); s_vpNum.clear();
	for ( int c = 0; c < (int)s_cells.size(); c++ ) {
		s_vpFirst.push_back( (int)s_vpCl.size() );
		const Cell &cell = s_cells[c];
		if ( c < s_numFloor ) {
			// the eyes of the cell's spots; the camera from the first (the spot nearest the footprint's centre)
			for ( int q = cell.firstPt, k = 0; q < cell.firstPt + cell.numPts; q += 2, k++ ) AddViewer( &s_pts[(size_t)q * 3], k == 0, dirs, ndirs );
		} else {
			AddViewer( &s_pts[(size_t)cell.firstPt * 3], true, dirs, ndirs );
		}
		s_vpNum.push_back( (int)s_vpCl.size() - s_vpFirst.back() );
	}
}

static void PrepareCells( void ) {
	for ( Cell &c : s_cells ) {
		c.cl0 = c.cl1 = -1;
		for ( int q = c.firstPt; q < c.firstPt + c.numPts; q++ ) {
			const int cl = s_ptCl[q];
			if ( cl < 0 ) continue;
			if ( c.cl0 < 0 ) c.cl0 = cl; else if ( cl != c.cl0 && c.cl1 < 0 ) c.cl1 = cl;
		}
	}
}

// TMPvis3slice <footprint> <air> <spots> <directions> <slices> <slice> <file>: the rows of the cells v with v % slices == slice
static void Slice_f( void ) {
	if ( !sv.state || Cmd_Argc() < 8 ) { Com_Printf( "usage\n" ); return; }
	s_fp = atof( Cmd_Argv( 1 ) ); s_air = atof( Cmd_Argv( 2 ) );
	const int perCell = atoi( Cmd_Argv( 3 ) ), ndirs = atoi( Cmd_Argv( 4 ) ), K = atoi( Cmd_Argv( 5 ) ), k = atoi( Cmd_Argv( 6 ) );
	const long long t0 = SV_Microseconds();
	FindSpots(); MakeCells( perCell ); PrepareCells(); MakeViewers( ndirs );
	const int n = (int)s_cells.size();
	const int rowBytes = ( n + 7 ) / 8;
	std::vector<uint8_t> row( rowBytes );
	FILE *f = fopen( Cmd_Argv( 7 ), "wb" );
	if ( !f ) { Com_Printf( "cannot write\n" ); return; }
	s_rays = 0;
	long long visible = 0, rows = 0;
	std::vector<int> vcl;
	for ( int v = k; v < n; v += K ) {
		std::fill( row.begin(), row.end(), 0 );
		vcl.clear();
		for ( int p = s_vpFirst[v]; p < s_vpFirst[v] + s_vpNum[v]; p++ )
			if ( s_vpCl[p] >= 0 && std::find( vcl.begin(), vcl.end(), s_vpCl[p] ) == vcl.end() ) vcl.push_back( s_vpCl[p] );
		for ( int t = 0; t < n; t++ ) {
			const Cell &ct = s_cells[t];
			bool vis = t == v;
			if ( !vis ) {
				bool pvs = false;
				for ( int c : vcl ) if ( PvsBit( c, ct.cl0 ) || PvsBit( c, ct.cl1 ) ) { pvs = true; break; }
				if ( !pvs ) continue;
			}
			for ( int p = s_vpFirst[v]; p < s_vpFirst[v] + s_vpNum[v] && !vis; p++ )
				for ( int q = ct.firstPt; q < ct.firstPt + ct.numPts && !vis; q++ )
					if ( PvsBit( s_vpCl[p], s_ptCl[q] ) ) vis = Clear( &s_vp[(size_t)p * 3], &s_pts[(size_t)q * 3] );
			if ( vis ) { row[t >> 3] |= (uint8_t)( 1 << ( t & 7 ) ); visible++; }
		}
		fwrite( row.data(), 1, rowBytes, f );
		rows++;
	}
	fclose( f );
	Com_Printf( "TMPVIS3 slice %d/%d: cells %d (floor %d), viewer points %d, rows %lld, visible %lld, rays %lld, %.2f s\n",
		k, K, n, s_numFloor, (int)s_vpCl.size(), rows, visible, s_rays, ( SV_Microseconds() - t0 ) / 1e6 );
}

// TMPvisexport <footprint> <air> <spots> <directions> <file>: cells, points and PVS for the offline tool
static void Export_f( void ) {
	if ( !sv.state || Cmd_Argc() < 6 ) { Com_Printf( "usage\n" ); return; }
	s_fp = atof( Cmd_Argv( 1 ) ); s_air = atof( Cmd_Argv( 2 ) );
	const int perCell = atoi( Cmd_Argv( 3 ) ), ndirs = atoi( Cmd_Argv( 4 ) );
	const long long t0 = SV_Microseconds();
	FindSpots(); MakeCells( perCell ); PrepareCells(); MakeViewers( ndirs );
	const long long t1 = SV_Microseconds();
	FILE *f = fopen( Cmd_Argv( 5 ), "wb" );
	if ( !f ) return;
	int maxCl = 0;
	for ( int c : s_ptCl ) maxCl = std::max( maxCl, c + 1 );
	for ( int c : s_vpCl ) maxCl = std::max( maxCl, c + 1 );
	const int clBytes = ( maxCl + 7 ) / 8;
	const int hdr[6] = { (int)s_cells.size(), s_numFloor, (int)s_ptCl.size(), (int)s_vpCl.size(), maxCl, clBytes };
	fwrite( hdr, sizeof( hdr ), 1, f );
	for ( size_t c = 0; c < s_cells.size(); c++ ) {
		const Cell &cell = s_cells[c];
		fwrite( cell.b0, 4, 3, f ); fwrite( cell.b1, 4, 3, f );
		const int ix[4] = { cell.firstPt, cell.numPts, s_vpFirst[c], s_vpNum[c] };
		fwrite( ix, 4, 4, f );
	}
	for ( size_t q = 0; q < s_ptCl.size(); q++ ) { fwrite( &s_pts[q * 3], 4, 3, f ); fwrite( &s_ptCl[q], 4, 1, f ); }
	for ( size_t q = 0; q < s_vpCl.size(); q++ ) { fwrite( &s_vp[q * 3], 4, 3, f ); fwrite( &s_vpCl[q], 4, 1, f ); }
	for ( int c = 0; c < maxCl; c++ ) fwrite( CM_ClusterPVS( c ), 1, clBytes, f );
	fclose( f );
	Com_Printf( "TMPVIS export: cells %d (floor %d), points %d, viewer points %d, clusters %d; layer and cells %.2f s\n",
		(int)s_cells.size(), s_numFloor, (int)s_ptCl.size(), (int)s_vpCl.size(), maxCl, ( t1 - t0 ) / 1e6 );
}

// TMPvisexport2 <footprint> <air> <spots> <file>: the cells, a shared lattice of camera points (above each floor cell:
// the eye, +64 and +128 under the ceiling; each air cell's point) and, per cell, the lattice points its camera reaches
// (within 152 of the head, a box of 4 traced from the head as cgame does), for the offline tool
static int s_extraEyes;
static void Export2_f( void ) {
	if ( !sv.state || Cmd_Argc() < 5 ) { Com_Printf( "usage\n" ); return; }
	s_fp = atof( Cmd_Argv( 1 ) ); s_air = atof( Cmd_Argv( 2 ) );
	const int perCell = atoi( Cmd_Argv( 3 ) );
	s_extraEyes = Cmd_Argc() > 5 ? atoi( Cmd_Argv( 5 ) ) : 0;
	const long long t0 = SV_Microseconds();
	FindSpots(); MakeCells( perCell ); PrepareCells();
	static const vec3_t cmin = { -4, -4, -4 }, cmax = { 4, 4, 4 };
	std::vector<float> lat; std::vector<int> latCl;
	std::vector<float> heads( s_cells.size() * 3 );
	for ( size_t c = 0; c < s_cells.size(); c++ ) {
		const float *eye = &s_pts[(size_t)s_cells[c].firstPt * 3];
		trace_t tr; vec3_t up = { eye[0], eye[1], eye[2] + ( (int)c < s_numFloor ? 160.0f : 32.0f ) };
		CM_BoxTrace( &tr, eye, up, cmin, cmax, 0, kCamMask, 0 );
		const float top = tr.endpos[2];
		heads[c * 3] = eye[0]; heads[c * 3 + 1] = eye[1]; heads[c * 3 + 2] = std::min( eye[2] + 32.0f, top );
		const int levels = (int)c < s_numFloor ? 3 : 1;
		for ( int l = 0; l < levels; l++ ) {
			const vec3_t p = { eye[0], eye[1], eye[2] + 64.0f * l };
			if ( l > 0 && p[2] > top ) break;
			if ( l > 0 && !InWorld( p ) ) continue;
			lat.push_back( p[0] ); lat.push_back( p[1] ); lat.push_back( p[2] );
			latCl.push_back( CM_LeafCluster( CM_PointLeafnum( p ) ) );
		}
		// the eyes of the cell's other spots (TMPVIS_EXTRA_EYES), for a viewer away from the footprint's centre
		if ( (int)c < s_numFloor && s_extraEyes ) {
			for ( int q = s_cells[c].firstPt + 2; q < s_cells[c].firstPt + s_cells[c].numPts; q += 2 ) {
				const float *e2 = &s_pts[(size_t)q * 3];
				lat.push_back( e2[0] ); lat.push_back( e2[1] ); lat.push_back( e2[2] );
				latCl.push_back( CM_LeafCluster( CM_PointLeafnum( e2 ) ) );
			}
		}
	}
	const int nl = (int)latCl.size();
	// the lattice points by a grid of 64
	std::unordered_map<long long, std::vector<int>> grid;
	auto key = []( int x, int y, int z ) { return ( (long long)( x + 4096 ) << 40 ) | ( (long long)( y + 4096 ) << 20 ) | (long long)( z + 4096 ); };
	for ( int i = 0; i < nl; i++ ) grid[key( (int)floorf( lat[i * 3] / 64 ), (int)floorf( lat[i * 3 + 1] / 64 ), (int)floorf( lat[i * 3 + 2] / 64 ) )].push_back( i );
	std::vector<int> reachFirst( s_cells.size() ), reachNum( s_cells.size() ), reach;
	long long traces = 0;
	for ( size_t c = 0; c < s_cells.size(); c++ ) {
		const float *h = &heads[c * 3];
		reachFirst[c] = (int)reach.size();
		const int gx = (int)floorf( h[0] / 64 ), gy = (int)floorf( h[1] / 64 ), gz = (int)floorf( h[2] / 64 );
		for ( int dz = -3; dz <= 3; dz++ ) for ( int dy = -3; dy <= 3; dy++ ) for ( int dx = -3; dx <= 3; dx++ ) {
			auto it = grid.find( key( gx + dx, gy + dy, gz + dz ) );
			if ( it == grid.end() ) continue;
			for ( int i : it->second ) {
				const float *p = &lat[(size_t)i * 3];
				const float d2 = ( p[0] - h[0] ) * ( p[0] - h[0] ) + ( p[1] - h[1] ) * ( p[1] - h[1] ) + ( p[2] - h[2] ) * ( p[2] - h[2] );
				if ( d2 > 152.0f * 152.0f ) continue;
				trace_t tr; CM_BoxTrace( &tr, h, p, cmin, cmax, 0, kCamMask, 0 ); traces++;
				if ( tr.fraction < 1.0f && d2 > 1.0f ) continue;
				reach.push_back( i );
			}
		}
		reachNum[c] = (int)reach.size() - reachFirst[c];
	}
	const long long t1 = SV_Microseconds();
	FILE *f = fopen( Cmd_Argv( 4 ), "wb" );
	if ( !f ) return;
	int maxCl = 0;
	for ( int c : s_ptCl ) maxCl = std::max( maxCl, c + 1 );
	for ( int c : latCl ) maxCl = std::max( maxCl, c + 1 );
	const int clBytes = ( maxCl + 7 ) / 8;
	const int hdr[7] = { (int)s_cells.size(), s_numFloor, (int)s_ptCl.size(), nl, (int)reach.size(), maxCl, clBytes };
	fwrite( hdr, sizeof( hdr ), 1, f );
	for ( size_t c = 0; c < s_cells.size(); c++ ) {
		const Cell &cell = s_cells[c];
		fwrite( cell.b0, 4, 3, f ); fwrite( cell.b1, 4, 3, f );
		const int ix[4] = { cell.firstPt, cell.numPts, reachFirst[c], reachNum[c] };
		fwrite( ix, 4, 4, f );
	}
	for ( size_t q = 0; q < s_ptCl.size(); q++ ) { fwrite( &s_pts[q * 3], 4, 3, f ); fwrite( &s_ptCl[q], 4, 1, f ); }
	for ( int q = 0; q < nl; q++ ) { fwrite( &lat[(size_t)q * 3], 4, 3, f ); fwrite( &latCl[q], 4, 1, f ); }
	fwrite( reach.data(), 4, reach.size(), f );
	for ( int c = 0; c < maxCl; c++ ) fwrite( CM_ClusterPVS( c ), 1, clBytes, f );
	fclose( f );
	Com_Printf( "TMPVIS export2: cells %d (floor %d), lattice %d, reach %d (%.1f per cell), camera traces %lld; %.2f s\n",
		(int)s_cells.size(), s_numFloor, nl, (int)reach.size(), (double)reach.size() / s_cells.size(), traces, ( t1 - t0 ) / 1e6 );
}

// TMPvis3load <footprint> <air> <spots> <slices> <file prefix>: the slices' rows into the table
static void Load_f( void ) {
	if ( !sv.state || Cmd_Argc() < 6 ) { Com_Printf( "usage\n" ); return; }
	s_fp = atof( Cmd_Argv( 1 ) ); s_air = atof( Cmd_Argv( 2 ) );
	const int perCell = atoi( Cmd_Argv( 3 ) ), K = atoi( Cmd_Argv( 4 ) );
	FindSpots(); MakeCells( perCell ); PrepareCells();
	const int n = (int)s_cells.size();
	s_rowWords = ( n + 63 ) / 64;
	s_table.assign( (size_t)n * s_rowWords, 0 );
	const int rowBytes = ( n + 7 ) / 8;
	long long setBits = 0;
	for ( int k = 0; k < K; k++ ) {
		FILE *f = fopen( va( "%s%d.bin", Cmd_Argv( 5 ), k ), "rb" );
		if ( !f ) { Com_Printf( "missing slice %d\n", k ); return; }
		for ( int v = k; v < n; v += K ) {
			if ( fread( &s_table[(size_t)v * s_rowWords], 1, rowBytes, f ) != (size_t)rowBytes ) { Com_Printf( "short slice %d\n", k ); fclose( f ); return; }
		}
		fclose( f );
	}
	for ( size_t w = 0; w < s_table.size(); w++ ) setBits += __builtin_popcountll( s_table[w] );
	s_camRows = true;
	Com_Printf( "TMPVIS3 loaded %d x %d bits (%.2f MB), set %.1f %%\n", n, n, (double)n * n / 8 / 1048576, 100.0 * setBits / ( (double)n * n ) );
}

// pairs of players: today (PVS and areas) against the table with the camera's reach, and the truth by rays
static FILE *s_gdump;
static long long g_truthEye, g_failEye, g_failEyeAreas, g_failCamFar;
static long long g_pairs, g_pvs, g_after, g_afterAreas, g_truth, g_fail, g_failAreas, g_noCell, g_viewers, g_regionCells, g_evalUs, g_samples;
static void Game_f( void ) {
	if ( s_cells.empty() ) { Com_Printf( "build first\n" ); return; }
	if ( Cmd_Argc() > 1 && !Q_stricmp( Cmd_Argv( 1 ), "print" ) ) {
		const double P = g_pairs ? (double)g_pairs : 1.0;
		Com_Printf( "TMPVIS2 game %lld samples, %lld pairs: PVS+areas %lld (%.1f %%), PVS and table %lld (%.1f %%), areas and table %lld (%.1f %%), truth %lld (%.1f %%); missed %lld (areas and table %lld); no cell %lld; region %.1f cells, %.1f us per viewer and its targets\n",
			g_samples, g_pairs, g_pvs, 100 * g_pvs / P, g_after, 100 * g_after / P, g_afterAreas, 100 * g_afterAreas / P, g_truth, 100 * g_truth / P,
			g_fail, g_failAreas, g_noCell, g_viewers ? (double)g_regionCells / g_viewers : 0.0, g_viewers ? (double)g_evalUs / g_viewers : 0.0 );
		Com_Printf( "TMPVIS2 game: truth from the eye alone %lld, missed from the eye %lld (areas and table %lld); missed seen only from a camera point beyond 80 units %lld\n",
			g_truthEye, g_failEye, g_failEyeAreas, g_failCamFar );
		return;
	}
	const float lead = Cmd_Argc() > 1 ? atof( Cmd_Argv( 1 ) ) / 1000.0f : 0.11f;
	const float camRange = 120.0f, camHeight = 32.0f;
	g_samples++;
	std::vector<uint64_t> acc( s_rowWords );
	static vec3_t dirs[26]; static bool made;
	if ( !made ) {
		int k = 0;
		for ( int z = -1; z <= 1; z++ ) for ( int y = -1; y <= 1; y++ ) for ( int x = -1; x <= 1; x++ ) {
			if ( !x && !y && !z ) continue;
			VectorSet( dirs[k], x, y, z ); VectorNormalize( dirs[k] ); k++;
		}
		made = true;
	}
	const int maxc = sv_maxclients->integer;
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
		vec3_t views[28]; int nv = 0;
		VectorCopy( eye, views[nv++] ); VectorCopy( head, views[nv++] );
		for ( int d = 0; d < 26; d++ ) {
			trace_t tr; vec3_t end; VectorMA( head, camRange, dirs[d], end );
			CM_BoxTrace( &tr, head, end, NULL, NULL, 0, CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_TERRAIN, 0 );
			VectorMA( tr.endpos, -4.0f, dirs[d], views[nv] ); nv++;
		}
		// the region: the cells within the camera's reach plus the movement, seen from the viewer's cell
		const long long e0 = SV_Microseconds();
		// with the camera rows, only the cells the viewer may move into before the snapshot is drawn
		const float reach = ( s_camRows ? 0.0f : camRange + camHeight ) + VectorLength( ps->velocity ) * lead;
		int from = CellAt( eye );
		if ( from < 0 ) from = CellAt( ps->origin );
		const bool noCell = from < 0;
		std::fill( acc.begin(), acc.end(), 0 );
		int region = 0;
		if ( !noCell ) {
			const vec3_t lo = { eye[0] - reach, eye[1] - reach, eye[2] - reach }, hi = { eye[0] + reach, eye[1] + reach, eye[2] + reach };
			CellsInBox( lo, hi, [&]( int c ) {
				if ( !s_camRows && c != from && !Bit( from, c ) ) return;
				// within reach: the nearest point of the cell's box
				float d2 = 0.0f;
				for ( int a = 0; a < 3; a++ ) { const float e = eye[a] < s_cells[c].b0[a] ? s_cells[c].b0[a] - eye[a] : eye[a] > s_cells[c].b1[a] ? eye[a] - s_cells[c].b1[a] : 0.0f; d2 += e * e; }
				if ( d2 > reach * reach ) return;
				region++;
				const uint64_t *row = &s_table[(size_t)c * s_rowWords];
				for ( int w = 0; w < s_rowWords; w++ ) acc[w] |= row[w];
			} );
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
			pvs = pvs && areas;
			const long long e1 = SV_Microseconds();
			playerState_t *tps = SV_GameClientNum( t );
			const float grow = 50.0f + VectorLength( tps->velocity ) * 0.02f;
			bool tab = noCell, any = false;
			const vec3_t lo = { te->r.absmin[0] - grow, te->r.absmin[1] - grow, te->r.absmin[2] - grow }, hi = { te->r.absmax[0] + grow, te->r.absmax[1] + grow, te->r.absmax[2] + grow };
			CellsInBox( lo, hi, [&]( int c ) { any = true; if ( ( acc[c >> 6] >> ( c & 63 ) ) & 1u ) tab = true; } );
			// a target whose own position is in no cell is unknown to the table: sent, whatever cells its box touches
			const bool targetOut = CellAt( tps->origin ) < 0;
			if ( !any || targetOut ) tab = true;
			if ( noCell || !any || targetOut ) g_noCell++;
			g_evalUs += SV_Microseconds() - e1;
			vec3_t tp[10]; int nt = 0;
			for ( int c = 0; c < 8; c++ ) {
				VectorSet( tp[nt], ( c & 1 ) ? te->r.absmax[0] - 1 : te->r.absmin[0] + 1, ( c & 2 ) ? te->r.absmax[1] - 1 : te->r.absmin[1] + 1,
					( c & 4 ) ? te->r.absmax[2] - 1 : te->r.absmin[2] + 1 ); nt++;
			}
			VectorAdd( te->r.absmin, te->r.absmax, tp[nt] ); VectorScale( tp[nt], 0.5f, tp[nt] ); nt++;
			VectorCopy( tps->origin, tp[nt] ); tp[nt][2] += tps->viewheight; nt++;
			bool truthEye = false;
			for ( int b = 0; b < nt && !truthEye; b++ ) truthEye = Clear( views[0], tp[b] );
			bool truth = truthEye, nearCam = truthEye;
			for ( int a = 1; a < nv && !nearCam; a++ ) {
				vec3_t d; VectorSubtract( views[a], head, d );
				if ( VectorLength( d ) > 80.0f ) continue;
				for ( int b = 0; b < nt && !nearCam; b++ ) nearCam = Clear( views[a], tp[b] );
			}
			truth = nearCam;
			for ( int a = 1; a < nv && !truth; a++ ) for ( int b = 0; b < nt && !truth; b++ ) truth = Clear( views[a], tp[b] );
			g_truthEye += truthEye;
			if ( truthEye && pvs && !tab ) g_failEye++;
			if ( truthEye && areas && !tab ) g_failEyeAreas++;
			if ( truth && !nearCam && pvs && !tab ) g_failCamFar++;
			g_pvs += pvs; g_truth += truth;
			g_after += ( pvs && tab ); g_afterAreas += ( areas && tab );
			if ( truth && pvs && !tab ) g_fail++;
			if ( truth && areas && !tab ) g_failAreas++;
			if ( s_gdump ) {
				// the pair for the offline truth on the drawn triangles: the viewer's points, the target's, the verdicts
				const uint8_t counts[2] = { (uint8_t)nv, (uint8_t)nt };
				const uint8_t flags = (uint8_t)( ( pvs ? 1 : 0 ) | ( areas ? 2 : 0 ) | ( tab ? 4 : 0 ) | ( truth ? 8 : 0 ) );
				fwrite( counts, 1, 2, s_gdump ); fwrite( &flags, 1, 1, s_gdump );
				fwrite( views, sizeof( vec3_t ), nv, s_gdump ); fwrite( tp, sizeof( vec3_t ), nt, s_gdump );
				// the diagnosis: what each one stands on, the viewer's cell, the target's cells
				int tFloor = 0, tAir = 0;
				CellsInBox( lo, hi, [&]( int c ) { if ( c < s_numFloor ) tFloor++; else tAir++; } );
				const int extra[8] = { ps->groundEntityNum, tps->groundEntityNum, from, from < 0 ? 0 : from < s_numFloor ? 1 : 2,
					tFloor, tAir, (int)ps->origin[2], (int)tps->origin[2] };
				fwrite( extra, sizeof( extra ), 1, s_gdump );
				const float fz[2] = { from >= 0 ? s_cells[from].b0[2] : 0.0f, from >= 0 ? s_cells[from].b1[2] : 0.0f };
				fwrite( fz, sizeof( fz ), 1, s_gdump );
			}
		}
	}
}
// TMPvisgamedump <file> | stop: record every pair the measure looks at
static void GameDump_f( void ) {
	if ( s_gdump ) { fclose( s_gdump ); s_gdump = NULL; }
	if ( Cmd_Argc() > 1 && Q_stricmp( Cmd_Argv( 1 ), "stop" ) ) s_gdump = fopen( Cmd_Argv( 1 ), "wb" );
}

// TMPvisprep <layer mode>: the layer and the cells only (mode bits: 1 jumps over gaps, 2 teleporters and push targets as seeds)
static void Prep_f( void ) {
	s_layerMode = Cmd_Argc() > 1 ? atoi( Cmd_Argv( 1 ) ) : 0;
	s_fp = 64.0f; s_air = 128.0f;
	const long long t0 = SV_Microseconds();
	FindSpots(); MakeCells( 2 ); PrepareCells();
	Com_Printf( "TMPVIS prep mode %d: spots %d, reached %d (by jumps %lld, from extra seeds %lld), cells %d (floor %d); %.2f s\n",
		s_layerMode, (int)s_spots.size(), (int)std::count( s_reached.begin(), s_reached.end(), 1 ), s_jumpReached, s_extraSeeds,
		(int)s_cells.size(), s_numFloor, ( SV_Microseconds() - t0 ) / 1e6 );
}

// TMPviscover [print]: where the players are against the layer
static long long c_total, c_inCell, c_unreachedBelow, c_reachedBelow, c_noFloor, c_onEntity, c_inAir;
static void Cover_f( void ) {
	if ( Cmd_Argc() > 1 ) {
		Com_Printf( "TMPVIS cover %lld positions: in a cell %lld (%.1f %%); out: a spot not reached below %lld, a reached spot far below %lld, no spot below %lld; of those out, on an entity %lld, in the air %lld\n",
			c_total, c_inCell, 100.0 * c_inCell / ( c_total ? c_total : 1 ), c_unreachedBelow, c_reachedBelow, c_noFloor, c_onEntity, c_inAir );
		return;
	}
	for ( int v = 0; v < sv_maxclients->integer; v++ ) {
		if ( svs.clients[v].state != CS_ACTIVE ) continue;
		sharedEntity_t *ve = SV_GentityNum( v );
		if ( !ve->r.linked || ( ve->r.svFlags & SVF_NOCLIENT ) ) continue;
		playerState_t *ps = SV_GameClientNum( v );
		c_total++;
		if ( CellAt( ps->origin ) >= 0 ) { c_inCell++; continue; }
		if ( ps->groundEntityNum == ENTITYNUM_NONE ) c_inAir++;
		else if ( ps->groundEntityNum != ENTITYNUM_WORLD ) c_onEntity++;
		const int i = (int)( ( ps->origin[0] - s_min[0] ) / kColumn ), j = (int)( ( ps->origin[1] - s_min[1] ) / kColumn );
		int best = -1; float bd = 1e9f;
		if ( i >= 0 && j >= 0 && i < s_cx && j < s_cy ) {
			const int col = j * s_cx + i;
			for ( int s = s_colFirst[col]; s < s_colFirst[col + 1]; s++ ) {
				const float d = ps->origin[2] - s_spots[s].z;
				if ( d > -24.0f && d < bd ) { bd = d; best = s; }
			}
		}
		if ( best < 0 ) c_noFloor++;
		else if ( !s_reached[best] ) c_unreachedBelow++;
		else c_reachedBelow++;
		static int shown;
		if ( shown++ < 40 )
			Com_Printf( "TMPVIS out: client %d at %.0f %.0f %.0f, velocity z %.0f, ground %d, jetpack %d, health %d, %s %.0f below\n", v, ps->origin[0], ps->origin[1], ps->origin[2],
				ps->velocity[2], ps->groundEntityNum, ( ps->eFlags & ( 1 << 11 ) ) ? 1 : 0, ps->stats[0], best < 0 ? "no spot" : s_reached[best] ? "reached spot" : "spot not reached", best < 0 ? 0.0f : bd );
	}
}

} // namespace tmpvis2

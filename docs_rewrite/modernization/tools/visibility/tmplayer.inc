// TMP walkable layer count (step 14 part 11): measurement only, removed after.
#include <vector>
#include <cstdint>
#include <cstring>
#include "qcommon/cm_public.h"

namespace tmplayer {

static const float kColumn = 16.0f;
static const float kStep = 18.0f;
static const float kJump = 426.0f;         // force jump level 3, with the step and the crouch
static const float kNormalJump = 66.0f;
static const vec3_t kMins = { -15, -15, -24 }, kMaxs = { 15, 15, 40 };
static const int kMask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP | CONTENTS_TERRAIN;

struct Spot { float x, y, z, top; int col; };

static bool InWorld( const vec3_t p ) { return CM_LeafCluster( CM_PointLeafnum( p ) ) >= 0; }

static void Box( trace_t *tr, const vec3_t a, const vec3_t b ) { CM_BoxTrace( tr, a, b, kMins, kMaxs, 0, kMask, 0 ); }

static std::vector<uint8_t> s_mark[2]; static vec3_t s_mins; static int s_nx, s_ny, s_nz;
static void Layer_f( void ) {
	if ( !sv.state ) { Com_Printf( "no map\n" ); return; }
	const long long t0 = SV_Microseconds();
	vec3_t mins, maxs;
	CM_ModelBounds( CM_InlineModel( 0 ), mins, maxs );
	const int cx = (int)ceilf( ( maxs[0] - mins[0] ) / kColumn ), cy = (int)ceilf( ( maxs[1] - mins[1] ) / kColumn );
	std::vector<Spot> spots;
	std::vector<int> colFirst( (size_t)cx * cy + 1, 0 );
	long long traces = 0;
	// every resting place of the player's box in every column, from the top down
	for ( int j = 0; j < cy; j++ ) for ( int i = 0; i < cx; i++ ) {
		const int col = j * cx + i;
		colFirst[col] = (int)spots.size();
		const float x = mins[0] + ( i + 0.5f ) * kColumn, y = mins[1] + ( j + 0.5f ) * kColumn;
		float z = maxs[2] + 64.0f;
		for ( int n = 0; n < 64 && z > mins[2]; n++ ) {
			trace_t tr; vec3_t a = { x, y, z }, b = { x, y, mins[2] - 64.0f };
			Box( &tr, a, b ); traces++;
			if ( tr.allsolid ) { z -= 32.0f; continue; }
			if ( tr.fraction >= 1.0f ) break;
			if ( tr.plane.normal[2] >= 0.7f && InWorld( tr.endpos ) ) {
				// the ceiling over it, up to the highest jump
				trace_t up; vec3_t top = { tr.endpos[0], tr.endpos[1], tr.endpos[2] + kJump };
				Box( &up, tr.endpos, top ); traces++;
				Spot s = { tr.endpos[0], tr.endpos[1], tr.endpos[2], up.endpos[2], col };
				spots.push_back( s );
			}
			z = tr.endpos[2] - 65.0f;
		}
	}
	colFirst[(size_t)cx * cy] = (int)spots.size();
	const int ns = (int)spots.size();
	const long long t1 = SV_Microseconds();

	// the spawn points, as seeds
	std::vector<char> reached( ns, 0 );
	std::vector<int> stack;
	int seeds = 0;
	{
		const char *p = CM_EntityString();
		char cls[128] = "", org[128] = "";
		for ( ;; ) {
			const char *tok = COM_Parse( &p );
			if ( !p || !tok[0] ) break;
			if ( !strcmp( tok, "{" ) ) { cls[0] = org[0] = 0; continue; }
			if ( !strcmp( tok, "}" ) ) {
				if ( ( !Q_strncmp( cls, "info_player", 11 ) || !Q_strncmp( cls, "team_CTF_", 9 ) ) && org[0] ) {
					vec3_t o; sscanf( org, "%f %f %f", &o[0], &o[1], &o[2] );
					const int i = (int)( ( o[0] - mins[0] ) / kColumn ), j = (int)( ( o[1] - mins[1] ) / kColumn );
					if ( i >= 0 && j >= 0 && i < cx && j < cy ) {
						const int col = j * cx + i;
						int best = -1; float bd = 1e9f;
						for ( int s = colFirst[col]; s < colFirst[col + 1]; s++ ) {
							const float d = o[2] - spots[s].z;
							if ( d > -40.0f && d < bd ) { bd = d; best = s; }
						}
						if ( best >= 0 && !reached[best] ) { reached[best] = 1; stack.push_back( best ); seeds++; }
					}
				}
				continue;
			}
			char key[128]; Q_strncpyz( key, tok, sizeof( key ) );
			tok = COM_Parse( &p );
			if ( !Q_stricmp( key, "classname" ) ) Q_strncpyz( cls, tok, sizeof( cls ) );
			else if ( !Q_stricmp( key, "origin" ) ) Q_strncpyz( org, tok, sizeof( org ) );
		}
	}
	// walk, step, jump up and fall between neighbouring columns
	while ( !stack.empty() ) {
		const int a = stack.back(); stack.pop_back();
		const Spot &sa = spots[a];
		const int i = sa.col % cx, j = sa.col / cx;
		for ( int dj = -1; dj <= 1; dj++ ) for ( int di = -1; di <= 1; di++ ) {
			if ( !di && !dj ) continue;
			const int ni = i + di, nj = j + dj;
			if ( ni < 0 || nj < 0 || ni >= cx || nj >= cy ) continue;
			const int col = nj * cx + ni;
			for ( int b = colFirst[col]; b < colFirst[col + 1]; b++ ) {
				if ( reached[b] ) continue;
				const Spot &sb = spots[b];
				const float dz = sb.z - sa.z;
				if ( dz > sa.top - sa.z + 1.0f ) continue; // higher than the jump reaches here
				// go up to the higher of the two (plus a step), across, then down
				const float h = ( dz > 0.0f ? sb.z : sa.z ) + kStep;
				if ( h > sa.top + kStep ) continue;
				trace_t tr; vec3_t p0 = { sa.x, sa.y, sa.z }, p1 = { sa.x, sa.y, h }, p2 = { sb.x, sb.y, h }, p3 = { sb.x, sb.y, sb.z };
				Box( &tr, p0, p1 ); traces++;
				if ( tr.fraction < 1.0f ) continue;
				Box( &tr, p1, p2 ); traces++;
				if ( tr.fraction < 1.0f ) continue;
				Box( &tr, p2, p3 ); traces++;
				if ( tr.fraction < 1.0f && tr.endpos[2] > sb.z + 1.0f ) continue;
				reached[b] = 1; stack.push_back( b );
			}
		}
	}
	int nr = 0; for ( int s = 0; s < ns; s++ ) nr += reached[s];
	const long long t2 = SV_Microseconds();
	Com_Printf( "TMPLAYER columns %d x %d, resting places %d, reached from %d spawns %d, traces %lld, %.2f s + %.2f s\n",
		cx, cy, ns, seeds, nr, traces, ( t1 - t0 ) / 1e6, ( t2 - t1 ) / 1e6 );

	// cells: the body near the floor (feet to head plus a normal jump) and the air up to the ceiling or the highest jump
	static const float sizes[] = { 32, 64, 128, 256 };
	for ( int pass = 0; pass < 2; pass++ ) {
		for ( int si = 0; si < 4; si++ ) {
			const float s = sizes[si];
			const int nx = (int)ceilf( ( maxs[0] - mins[0] ) / s ), ny = (int)ceilf( ( maxs[1] - mins[1] ) / s ), nz = (int)ceilf( ( maxs[2] - mins[2] + 600.0f ) / s );
			std::vector<uint8_t> mark( (size_t)nx * ny * nz, 0 );
			for ( int k = 0; k < ns; k++ ) {
				if ( pass && !reached[k] ) continue;
				const Spot &sp = spots[k];
				const int i0 = (int)( ( sp.x + kMins[0] - mins[0] ) / s ), i1 = (int)( ( sp.x + kMaxs[0] - mins[0] ) / s );
				const int j0 = (int)( ( sp.y + kMins[1] - mins[1] ) / s ), j1 = (int)( ( sp.y + kMaxs[1] - mins[1] ) / s );
				const float fineTop = sp.z + kMaxs[2] + kNormalJump < sp.top + kMaxs[2] ? sp.z + kMaxs[2] + kNormalJump : sp.top + kMaxs[2];
				const int k0 = (int)( ( sp.z + kMins[2] - mins[2] ) / s ), kf = (int)( ( fineTop - mins[2] ) / s ), ka = (int)( ( sp.top + kMaxs[2] - mins[2] ) / s );
				for ( int kk = k0 < 0 ? 0 : k0; kk <= ka && kk < nz; kk++ )
				for ( int jj = j0 < 0 ? 0 : j0; jj <= j1 && jj < ny; jj++ )
				for ( int ii = i0 < 0 ? 0 : i0; ii <= i1 && ii < nx; ii++ ) {
					uint8_t &m = mark[( (size_t)kk * ny + jj ) * nx + ii];
					m |= kk <= kf ? 1 : 2;
				}
			}
			if ( s == 64 ) { s_mark[pass] = mark; VectorCopy( mins, s_mins ); s_nx = nx; s_ny = ny; s_nz = nz; }
			long long fine = 0, air = 0;
			for ( size_t c = 0; c < mark.size(); c++ ) { if ( mark[c] & 1 ) fine++; else if ( mark[c] & 2 ) air++; }
			Com_Printf( "TMPLAYER %s cell %3.0f: near the floor %lld, air above only %lld, all %lld (grid %d)\n",
				pass ? "reached" : "all    ", s, fine, air, fine + air, nx * ny * nz );
		}
	}
}

static long long c_total, c_reached, c_allOnly, c_none;
static void Check_f( void ) {
	if ( Cmd_Argc() > 1 ) {
		Com_Printf( "TMPLAYER bots %lld positions: in reached cells %lld, in the other resting cells only %lld, in none %lld\n", c_total, c_reached, c_allOnly, c_none );
		return;
	}
	for ( int v = 0; v < sv_maxclients->integer; v++ ) {
		if ( svs.clients[v].state != CS_ACTIVE ) continue;
		playerState_t *ps = SV_GameClientNum( v );
		const int i = (int)( ( ps->origin[0] - s_mins[0] ) / 64 ), j = (int)( ( ps->origin[1] - s_mins[1] ) / 64 ), k = (int)( ( ps->origin[2] - s_mins[2] ) / 64 );
		if ( i < 0 || j < 0 || k < 0 || i >= s_nx || j >= s_ny || k >= s_nz ) { c_none++; c_total++; continue; }
		const size_t c = ( (size_t)k * s_ny + j ) * s_nx + i;
		c_total++;
		if ( s_mark[1][c] ) c_reached++;
		else if ( s_mark[0][c] ) { c_allOnly++; if ( c_allOnly <= 5 ) Com_Printf( "TMPLAYER not reached: %.0f %.0f %.0f\n", ps->origin[0], ps->origin[1], ps->origin[2] ); }
		else { c_none++; if ( c_none <= 5 ) Com_Printf( "TMPLAYER in no cell: %.0f %.0f %.0f vel z %.0f\n", ps->origin[0], ps->origin[1], ps->origin[2], ps->velocity[2] ); }
	}
}

} // namespace tmplayer

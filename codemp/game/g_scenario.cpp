// Scripted bot scenarios for plan step 14: two bots, an attacker and a defender, placed face to face and reset at every
// repetition, play a fixed sequence of commands timed in milliseconds (one every 10 ms, the tick, as every bot sends
// them), so that a scenario compares the game per swing, per shot or per power use rather than through the chaos of a
// free-for-all. A first repetition, not counted, replaces the bots'
// state left by the game before the scenario (animations); every reset also clears what the free play could leave in
// the bots (powerups, pain timers and damage sums, walking direction), and the corpses, missiles, events and dropped
// items are removed when the counted repetitions start, so that every run plays the same game; the statistics of the
// counted repetitions are printed at the end (gamestats), and the bots then stand still until "scenario off".
// In a team game a third bot, a teammate of the attacker, stands beside it as the ally the team powers act on; the
// defender is the first bot that is not the attacker's teammate.
//
// scenario saber <reps> <fast|medium|strong|dual|staff> <idle|attack> [distance]
// scenario force <reps> <power> [level 1 to 3, 3] [distance] [the defender's level 0 to 3 in push or pull, 0]
//   power: lightning grip drain rage protect absorb heal levitation speed push pull telepathy teamheal teamforce see
//   saberthrow (teamheal and teamforce need the ally; against saberthrow the defender holds no saber)
// scenario weapon <reps> <weapon number> <0|1 alternate> [distance]
// scenario off

#include "g_local.h"

extern void WP_ForcePowerStop( gentity_t *self, forcePowers_t forcePower );
extern bool G_SetSaber( gentity_t *ent, int saberNum, const char *saberName, bool siegeOverride );
extern void Svcmd_GameStats_f( void );
extern void saberReactivate( gentity_t *saberent, gentity_t *saberOwner );
extern void SaberGotHit( gentity_t *self, gentity_t *other, trace_t *trace );
extern void SaberUpdateSelf( gentity_t *ent );
extern void WP_SaberRemoveG2Model( gentity_t *saberent );

constexpr int SCN_REP_MSEC = 2000;	// one repetition
constexpr int SCN_SETTLE_MSEC = 500;		// still, after the reset, before the script starts

enum class scenarioType_t {
	SCN_NONE,
	SCN_SABER,
	SCN_FORCE,
	SCN_WEAPON
};

static struct {
	scenarioType_t	type;
	int				reps;			// repetitions asked
	int				rep;			// the current repetition: 0 is the warm-up, then 1 to reps
	bool		done;			// all repetitions played: the bots stand still
	int				repStart;		// level.time of the current repetition's reset, 0 before the first
	int				client[3];		// attacker, defender, ally (a teammate of the attacker, in team games)
	int				numClients;		// 2, or 3 with the ally
	saber_styles_t	style;			// the attacker's saber style
	bool		counter;		// the defender attacks at the same time
	forcePowers_t	power;
	forcePowerLevels_t	level;		// the attacker's level in that power
	forcePowerLevels_t	defenderLevel;	// push and pull: the defender's level in the same power, which resists it
	weapon_t		weapon;
	bool		alt;
	float			dist;			// between the two origins
	vec3_t			origin;			// the attacker's place, facing yaw
	float			yaw;
	int				traceReps;		// repetitions traced at every 20 Hz instant (cvar scenario_trace), to diff two rates
} scn;

// what each bot of the scenario did in the counted repetitions, by role, sampled at the 20 Hz instants: what the counters
// of gamestats do not see (the health and force given, the movement, the states a power puts a player in)
static struct {
	int		healthGained[3], healthLost[3], forceGained[3], forceSpent[3];
	int		airborne[3];		// instants off the ground
	int		knockedDown[3];		// instants knocked down (HANDEXTEND_KNOCKDOWN)
	int		mindTricked[3];		// instants in the attacker's mind trick
	float	path[3];			// distance moved
	int		end[3][3];			// the place at the end of each repetition against the place at its start, rounded to the unit
	// the previous instant's values, from the reset on
	int		health[3], force[3];
	vec3_t	origin[3], start[3];
} scnStats;

// the eight attack directions, as forward and right moves
static const signed char scnDirs[8][2] = {
	{ 127, 0 }, { -127, 0 }, { 0, -127 }, { 0, 127 }, { 127, -127 }, { 127, 127 }, { -127, -127 }, { -127, 127 }
};

bool G_ScenarioControls( int clientNum ) {
	if ( scn.type == scenarioType_t::SCN_NONE ) {
		return false;
	}
	for ( int i = 0; i < scn.numClients; i++ ) {
		if ( clientNum == scn.client[i] ) {
			return true;
		}
	}
	return false;
}

// the ally's place: beside the attacker, on its right
static void G_ScenarioAllyPlace( const vec3_t origin, float yaw, vec3_t out ) {
	const vec3_t right = { Q_sinpif( yaw / 180.0f ), -Q_cospif( yaw / 180.0f ), 0 };

	VectorMA( origin, 64, right, out );
}

// a place where the attacker stands on a spawn point and the defender on the floor in front of it
static bool G_ScenarioFindPlace( void ) {
	const vec3_t mins = { -15, -15, DEFAULT_MINS_2 };
	const vec3_t maxs = { 15, 15, DEFAULT_MAXS_2 };
	gentity_t *spot = nullptr;

	while ( ( spot = G_Find( spot, FOFS( classname ), "info_player_deathmatch" ) ) != nullptr ) {
		vec3_t fwd, start, front, below;
		trace_t tr;

		VectorSet( fwd, Q_cospif( spot->s.angles[YAW] / 180.0f ), Q_sinpif( spot->s.angles[YAW] / 180.0f ), 0 );
		VectorCopy( spot->s.origin, start );
		start[2] += 9;
		VectorMA( start, scn.dist + 32, fwd, front );
		trap->Trace( &tr, start, mins, maxs, front, ENTITYNUM_NONE, MASK_SOLID, false, g2TraceFlags_t::G2TRFLAG_NONE, 0 );
		if ( tr.fraction < 1.0f || tr.startsolid || tr.allsolid ) {
			continue;
		}
		VectorMA( start, scn.dist, fwd, front );
		VectorCopy( front, below );
		below[2] -= 48;
		trap->Trace( &tr, front, mins, maxs, below, ENTITYNUM_NONE, MASK_SOLID, false, g2TraceFlags_t::G2TRFLAG_NONE, 0 );
		if ( tr.fraction >= 1.0f || tr.startsolid || tr.endpos[2] < start[2] - 24 ) {
			continue;	// no floor at the attacker's height in front
		}
		if ( scn.numClients == 3 ) { // the ally's place: open from the attacker's, with a floor at its height
			vec3_t ally;

			G_ScenarioAllyPlace( start, spot->s.angles[YAW], ally );
			trap->Trace( &tr, start, mins, maxs, ally, ENTITYNUM_NONE, MASK_SOLID, false, g2TraceFlags_t::G2TRFLAG_NONE, 0 );
			if ( tr.fraction < 1.0f || tr.startsolid || tr.allsolid ) {
				continue;
			}
			VectorCopy( ally, below );
			below[2] -= 48;
			trap->Trace( &tr, ally, mins, maxs, below, ENTITYNUM_NONE, MASK_SOLID, false, g2TraceFlags_t::G2TRFLAG_NONE, 0 );
			if ( tr.fraction >= 1.0f || tr.startsolid || tr.endpos[2] < start[2] - 24 ) {
				continue;
			}
		}
		{ // away from doors and lifts, which crush the bots standing in their way
			int e;
			bool nearMover = false;

			for ( e = MAX_CLIENTS; e < level.num_entities && !nearMover; e++ ) {
				const gentity_t *m = &g_entities[e];
				vec3_t center;

				if ( !m->inuse || m->s.eType != entityType_t::ET_MOVER ) {
					continue;
				}
				VectorAdd( m->r.absmin, m->r.absmax, center );
				VectorScale( center, 0.5f, center );
				nearMover = ( Distance( center, start ) < 512 ) ? true : false;
			}
			if ( nearMover ) {
				continue;
			}
		}
		VectorCopy( start, scn.origin );
		scn.yaw = spot->s.angles[YAW];
		return true;
	}
	return false;
}

// a saber thrown or knocked away in the free play or the previous repetition comes back to the hand, as when its owner
// catches it (DownedSaberThink, saberBackToOwner), in the state of a saber never thrown
static void G_ScenarioSaberToHand( gentity_t *ent ) {
	gclient_t *client = ent->client;
	gentity_t *saberent;

	if ( !client->ps.saberInFlight || !client->saberStoredIndex ) {
		return;
	}
	saberent = &g_entities[client->saberStoredIndex];
	client->ps.saberEntityNum = client->saberStoredIndex;
	saberReactivate( saberent, ent );
	saberent->touch = SaberGotHit;
	saberent->think = SaberUpdateSelf;
	saberent->genericValue5 = 0;
	saberent->nextthink = level.time;
	saberent->r.svFlags |= svFlags_t::SVF_NOCLIENT;
	saberent->s.eFlags = entityFlags_t::EF_NODRAW;
	saberent->s.saberInFlight = false;
	saberent->s.loopSound = 0;
	saberent->s.loopIsSoundset = false;
	WP_SaberRemoveG2Model( saberent );
	client->ps.saberInFlight = false;
	client->ps.saberEntityState = 0;
	client->ps.saberThrowDelay = 0;
	client->saberKnockedTime = 0;
}

static void G_ScenarioPlace( gentity_t *ent, const vec3_t origin, float yaw ) {
	gclient_t *client = ent->client;
	vec3_t angles = { 0, 0, 0 };
	int i, j;

	if ( ent->health <= 0 || client->ps.stats[statIndex_t::STAT_HEALTH] <= 0 ) {
		ClientRespawn( ent );
	}
	G_ScenarioSaberToHand( ent );
	// the blades' last positions, from before the bot is moved here: the first sweep of a repetition would start from them
	client->lastSaberStorageTime = 0;
	client->hasCurrentPosition = false;
	for ( i = 0; i < MAX_SABERS; i++ ) {
		for ( j = 0; j < MAX_BLADES; j++ ) {
			client->saber[i].blade[j].trail.lastTime = 0;
		}
	}
	for ( forcePowers_t fp = forcePowers_t::FP_FIRST; fp < forcePowers_t::NUM_FORCE_POWERS; fp++ ) {
		if ( client->ps.fd.forcePowersActive & Bit( fp ) ) {
			WP_ForcePowerStop( ent, fp );
		}
		client->ps.fd.forcePowerDuration[fp] = 0;
		client->ps.fd.forcePowerDebounce[fp] = 0;
	}
	ent->health = client->ps.stats[statIndex_t::STAT_HEALTH] = client->ps.stats[statIndex_t::STAT_MAX_HEALTH] = 100;
	client->ps.stats[statIndex_t::STAT_ARMOR] = 0;
	client->ps.fd.forcePower = 100;
	client->ps.fd.forcePowerRegenDebounceTime = level.time;
	client->ps.fd.forceGripBeingGripped = 0;
	client->ps.fd.forceRageRecoveryTime = 0;
	client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_NONE;
	client->ps.forceHandExtendTime = 0;
	client->ps.weaponTime = 0;
	client->ps.weaponstate = weaponstate_t::WEAPON_READY;
	client->ps.saberBlocked = saberBlockedType_t::BLOCKED_NONE;
	client->ps.saberMove = saberMoveName_t::LS_READY;
	client->ps.saberHolstered = 0;
	client->ps.legsTimer = client->ps.torsoTimer = 0;
	client->ps.electrifyTime = 0;
	client->ps.pm_time = 0;
	// PMF_BACKWARDS_RUN and movementDir change only with movement commands, so a still bot kept those of its last step
	// in the free play: a backward one turns the legs of a pushed player the other way (BG_G2PlayerAngles), which moved
	// the model points the missiles hit
	client->ps.pm_flags &= ~( pmoveFlags_t::PMF_TIME_KNOCKBACK | pmoveFlags_t::PMF_TIME_WATERJUMP | pmoveFlags_t::PMF_BACKWARDS_RUN );
	client->ps.movementDir = 0;
	client->ps.saberAttackWound = client->ps.saberIdleWound = 0;
	client->invulnerableTimer = 0;
	client->ps.eFlags &= ~entityFlags_t::EF_INVULNERABLE;
	// powerups picked up in the free play before the scenario, and the pain sound timers (P_DamageFeedback), which a
	// pain in the previous repetition or in the free play would carry into this one
	client->ps.powerups = {};
	client->ps.painTime = 0;
	ent->pain_debounce_time = 0;
	// the damage P_DamageFeedback keeps summing until it plays a pain (under 10 points, or within 500 ms of a pain)
	client->damage_blood = client->damage_armor = client->damage_knockback = 0;
	client->g2LastSurfaceHit = client->g2LastSurfaceTime = 0;	// shown by scenario_trace at every health loss

	trap->UnlinkEntity( (sharedEntity_t *)ent );
	VectorCopy( origin, client->ps.origin );
	VectorClear( client->ps.velocity );
	angles[YAW] = yaw;
	SetClientViewAngle( ent, angles );
	client->ps.eFlags ^= entityFlags_t::EF_TELEPORT_BIT;
	BG_PlayerStateToEntityState( &client->ps, &ent->s, true );
	VectorCopy( client->ps.origin, ent->r.currentOrigin );
	trap->LinkEntity( (sharedEntity_t *)ent );
}

static void G_ScenarioEquip( gentity_t *ent, bool attacker ) {
	gclient_t *client = ent->client;

	client->ps.fd.forcePowerLevel[forcePowers_t::FP_SABER_OFFENSE] = forcePowerLevels_t::FORCE_LEVEL_3;
	client->ps.fd.forcePowerLevel[forcePowers_t::FP_SABER_DEFENSE] = forcePowerLevels_t::FORCE_LEVEL_3;
	client->ps.fd.forcePowersKnown |= Bit( forcePowers_t::FP_SABER_OFFENSE ) | Bit( forcePowers_t::FP_SABER_DEFENSE );
	if ( scn.type == scenarioType_t::SCN_FORCE && attacker ) {
		client->ps.fd.forcePowerLevel[scn.power] = scn.level;
		client->ps.fd.forcePowersKnown |= Bit( scn.power );
		// the side the power needs; the neutral powers on the dark side
		client->ps.fd.forceSide = ( forcePowerDarkLight[scn.power] == FORCE_LIGHTSIDE ) ? FORCE_LIGHTSIDE : FORCE_DARKSIDE;
	}

	if ( scn.type == scenarioType_t::SCN_FORCE && !attacker
		&& ( scn.power == forcePowers_t::FP_PUSH || scn.power == forcePowers_t::FP_PULL ) ) {
		client->ps.fd.forcePowerLevel[scn.power] = scn.defenderLevel;
	}

	if ( scn.type == scenarioType_t::SCN_WEAPON || ( scn.type == scenarioType_t::SCN_FORCE && scn.power == forcePowers_t::FP_SABERTHROW && !attacker ) ) {
		// the defender cannot block with a saber
		const weapon_t weapon = ( attacker && scn.type == scenarioType_t::SCN_WEAPON ) ? scn.weapon : weapon_t::WP_MELEE;

		client->ps.stats[statIndex_t::STAT_WEAPONS] |= Bit( weapon );
		client->ps.ammo[weaponData[weapon].ammoIndex] = 999;
		client->ps.weapon = weapon;
		return;
	}

	client->ps.stats[statIndex_t::STAT_WEAPONS] |= Bit( weapon_t::WP_SABER );
	client->ps.weapon = weapon_t::WP_SABER;
	if ( attacker && scn.type == scenarioType_t::SCN_SABER ) {
		G_SetSaber( ent, 0, scn.style == saber_styles_t::SS_STAFF ? "dual_1" : "single_1", true );
		G_SetSaber( ent, 1, scn.style == saber_styles_t::SS_DUAL ? "single_1" : "none", true );
		client->ps.fd.saberAnimLevel = client->ps.fd.saberAnimLevelBase = client->ps.fd.saberDrawAnimLevel = scn.style;
		client->saberCycleQueue = scn.style;
	}
	else {
		G_SetSaber( ent, 0, "single_1", true );
		G_SetSaber( ent, 1, "none", true );
		client->ps.fd.saberAnimLevel = client->ps.fd.saberAnimLevelBase = client->ps.fd.saberDrawAnimLevel = saber_styles_t::SS_MEDIUM;
		client->saberCycleQueue = saber_styles_t::SS_MEDIUM;
	}
}

// the end of a repetition: where each bot stands against its place at the start
static void G_ScenarioEndRepetition( void ) {
	for ( int n = 0; n < scn.numClients; n++ ) {
		const gentity_t *ent = &g_entities[scn.client[n]];

		if ( !ent->inuse || !ent->client ) {
			continue;
		}
		for ( int k = 0; k < 3; k++ ) {
			scnStats.end[n][k] += (int)Q_roundf( ent->client->ps.origin[k] - scnStats.start[n][k] );
		}
	}
}

// the bots' values after the reset, from which the instants of the repetition are counted
static void G_ScenarioStartSampling( void ) {
	for ( int n = 0; n < scn.numClients; n++ ) {
		const gentity_t *ent = &g_entities[scn.client[n]];

		scnStats.health[n] = ent->health;
		scnStats.force[n] = ent->client->ps.fd.forcePower;
		VectorCopy( ent->client->ps.origin, scnStats.origin[n] );
		VectorCopy( ent->client->ps.origin, scnStats.start[n] );
	}
}

extern bool G_IsMindTricked( forcedata_t *fd, int client );

// at the end of each 20 Hz instant of a counted repetition
static void G_ScenarioSample( void ) {
	for ( int n = 0; n < scn.numClients; n++ ) {
		const gentity_t *ent = &g_entities[scn.client[n]];
		const playerState_t *ps = &ent->client->ps;
		const int health = ent->health - scnStats.health[n];
		const int force = ps->fd.forcePower - scnStats.force[n];

		if ( health > 0 ) {
			scnStats.healthGained[n] += health;
		}
		else {
			scnStats.healthLost[n] -= health;
		}
		if ( force > 0 ) {
			scnStats.forceGained[n] += force;
		}
		else {
			scnStats.forceSpent[n] -= force;
		}
		if ( ps->groundEntityNum == ENTITYNUM_NONE ) {
			scnStats.airborne[n]++;
		}
		if ( ps->forceHandExtend == forceHandAnims_t::HANDEXTEND_KNOCKDOWN ) {
			scnStats.knockedDown[n]++;
		}
		if ( n && G_IsMindTricked( &g_entities[scn.client[0]].client->ps.fd, scn.client[n] ) ) {
			scnStats.mindTricked[n]++;
		}
		scnStats.path[n] += Distance( ps->origin, scnStats.origin[n] );
		scnStats.health[n] = ent->health;
		scnStats.force[n] = ps->fd.forcePower;
		VectorCopy( ps->origin, scnStats.origin[n] );
	}
}

// with the gamestats of a scenario: what each bot did, by role
void G_ScenarioPrintStats( void ) {
	static const char *roles[] = { "attacker", "defender", "ally" };
	char line[MAX_STRING_CHARS] = "scenario statistics: health gained and lost, force gained and spent, instants off the ground, "
		"knocked down and mind tricked, distance moved, end place against the start, by role:";

	if ( scn.type == scenarioType_t::SCN_NONE ) {
		return;
	}
	for ( int n = 0; n < scn.numClients; n++ ) {
		Q_strcat( line, sizeof( line ), va( "%s %s %d %d %d %d %d %d %d %.0f %d %d %d", n ? "," : "", roles[n],
			scnStats.healthGained[n], scnStats.healthLost[n], scnStats.forceGained[n], scnStats.forceSpent[n],
			scnStats.airborne[n], scnStats.knockedDown[n], scnStats.mindTricked[n], scnStats.path[n],
			scnStats.end[n][0], scnStats.end[n][1], scnStats.end[n][2] ) );
	}
	trap->Print( "%s\n", line );
}

// called at the start of every server frame: resets the bots at the start of each repetition
void G_ScenarioFrame( void ) {
	vec3_t fwd, front;
	int seed, i;

	if ( scn.type == scenarioType_t::SCN_NONE || scn.done ) {
		return;
	}
	if ( scn.repStart && level.time < scn.repStart + SCN_REP_MSEC ) {
		return;
	}
	if ( scn.repStart && scn.rep >= 1 ) {
		G_ScenarioEndRepetition();
	}
	if ( scn.rep >= scn.reps ) {
		trap->Print( "scenario done: %d repetitions\n", scn.rep );
		Svcmd_GameStats_f();
		scn.done = true;
		return;
	}
	for ( i = 0; i < scn.numClients; i++ ) {
		if ( !g_entities[scn.client[i]].inuse ) {
			trap->Print( "scenario stopped: a bot left\n" );
			scn.type = scenarioType_t::SCN_NONE;
			return;
		}
	}

	// the same random numbers at the start of a repetition at any rate, from before the bots are placed: a bot killed in
	// the previous repetition respawns at a spawn point drawn at random (ClientRespawn), which the free play's left-overs
	// had moved
	seed = 7919 * ( scn.rep + 1 );
	Rand_Init( seed );

	VectorSet( fwd, Q_cospif( scn.yaw / 180.0f ), Q_sinpif( scn.yaw / 180.0f ), 0 );
	VectorMA( scn.origin, scn.dist, fwd, front );
	G_ScenarioPlace( &g_entities[scn.client[0]], scn.origin, scn.yaw );
	G_ScenarioPlace( &g_entities[scn.client[1]], front, AngleNormalize360( scn.yaw + 180 ) );
	G_ScenarioEquip( &g_entities[scn.client[0]], true );
	G_ScenarioEquip( &g_entities[scn.client[1]], false );
	if ( scn.numClients == 3 ) {
		vec3_t ally;

		G_ScenarioAllyPlace( scn.origin, scn.yaw, ally );
		G_ScenarioPlace( &g_entities[scn.client[2]], ally, scn.yaw );
		G_ScenarioEquip( &g_entities[scn.client[2]], false );
	}
	if ( scn.type == scenarioType_t::SCN_FORCE ) { // what the healing and replenishing powers act on
		gentity_t *attacker = &g_entities[scn.client[0]];
		gentity_t *ally = &g_entities[scn.client[scn.numClients - 1]];

		if ( scn.power == forcePowers_t::FP_HEAL ) {
			attacker->health = attacker->client->ps.stats[statIndex_t::STAT_HEALTH] = 40;
		}
		if ( scn.numClients == 3 && scn.power == forcePowers_t::FP_TEAM_HEAL ) {
			ally->health = ally->client->ps.stats[statIndex_t::STAT_HEALTH] = 40;
		}
		if ( scn.numClients == 3 && scn.power == forcePowers_t::FP_TEAM_FORCE ) {
			ally->client->ps.fd.forcePower = 20;
		}
	}
	// the animations restart from the weapon's ready pose: a still bot keeps its torso and legs animations (the choke of
	// a grip still held, the slope stance of the legs), so the warm-up's, shaped by the free play, carried into every
	// repetition; the ghoul2 animations restart with them (G_UpdateClientAnims)
	for ( i = 0; i < scn.numClients; i++ ) {
		gclient_t *client = g_entities[scn.client[i]].client;

		client->ps.torsoAnim = client->ps.legsAnim = WeaponReadyAnim[client->ps.weapon];
		client->torsoAnimExecute = client->legsAnimExecute = animNumber_t::ANIM_NONE;
	}

	scn.repStart = level.time;
	scn.rep++;
	if ( scn.rep == 1 ) {
		memset( &g_stats, 0, sizeof( g_stats ) );	// the counted repetitions start
		memset( &scnStats, 0, sizeof( scnStats ) );
		// without the corpses left by the free play or the warm-up, the body queue from its first slot (as BodySink
		// leaves a body at its end)
		for ( i = 0; i < BODY_QUEUE_SIZE; i++ ) {
			trap->UnlinkEntity( (sharedEntity_t *)level.bodyQue[i] );
			level.bodyQue[i]->physicsObject = false;
			level.bodyQue[i]->takedamage = false;
			level.bodyQue[i]->nextthink = 0;
		}
		level.bodyQueIndex = 0;
		// the events, missiles (in flight or laid, as mines) and dropped items left by the free play or the warm-up are
		// freed and every free slot can be taken at once: G_Spawn skips a slot freed less than a second ago, so the free
		// play's last frees gave the missiles other entity numbers, and the missiles run in the order of their numbers
		for ( i = MAX_CLIENTS; i < level.num_entities; i++ ) {
			gentity_t *e = &g_entities[i];

			if ( e->inuse && !e->neverFree &&
				( e->freeAfterEvent || e->s.eType == entityType_t::ET_MISSILE || Any( e->flags & gentityFlags_t::FL_DROPPED_ITEM ) ) ) {
				G_FreeEntity( e );
			}
			if ( !e->inuse ) {
				e->freetime = 0;
			}
		}
	}
	G_ScenarioStartSampling();
}

// called at the end of the 20 Hz instants: the scenario's statistics, and the two bots' state, printed for the first
// scenario_trace repetitions, so that runs at two rates can be diffed line by line
void G_ScenarioTrace( void ) {
	int n;

	if ( scn.type == scenarioType_t::SCN_NONE || scn.done || scn.rep < 1 ) {
		return;
	}
	G_ScenarioSample();
	if ( scn.rep > scn.traceReps ) {
		return;
	}
	for ( n = 0; n < scn.numClients; n++ ) {
		const gentity_t *ent = &g_entities[scn.client[n]];
		const playerState_t *ps = &ent->client->ps;
		const bladeInfo_t *blade = &ent->client->saber[0].blade[0];

		trap->Print( "SCN %d %4d %c cmd%+d hp%d fp%d mv%d blk%d wt%d ta%d tt%d la%d lt%d o%.1f,%.1f,%.1f m%.1f,%.1f,%.1f\n",
			scn.rep, level.time - scn.repStart, "ADT"[n], ps->commandTime - level.time, ent->health, ps->fd.forcePower, to_underlying( ps->saberMove ),
			to_underlying( ps->saberBlocked ), ps->weaponTime, to_underlying( ps->torsoAnim ), ps->torsoTimer, to_underlying( ps->legsAnim ), ps->legsTimer,
			ps->origin[0], ps->origin[1], ps->origin[2], blade->muzzlePoint[0], blade->muzzlePoint[1], blade->muzzlePoint[2] );
	}
}

// every health loss of a client during the traced repetitions: the tick, the inflictor's position and the point hit
void G_ScenarioDamageTrace( gentity_t *targ, gentity_t *inflictor, const vec3_t point, int take, meansOfDeath_t mod ) {
	if ( scn.type == scenarioType_t::SCN_NONE || scn.done || scn.rep < 1 || scn.rep > scn.traceReps ) {
		return;
	}
	trap->Print( "SCN %d %4d dmg tick%+d targ%d take%d mod%d surf%d@%+d inf%d p%.1f,%.1f,%.1f io%.1f,%.1f,%.1f\n", scn.rep,
		level.time - scn.repStart, level.tickTime - level.time, targ->s.number, take, to_underlying( mod ), targ->client->g2LastSurfaceHit,
		targ->client->g2LastSurfaceTime - level.time, inflictor ? inflictor->s.number : -1,
		point ? point[0] : 0.0f, point ? point[1] : 0.0f, point ? point[2] : 0.0f,
		inflictor ? inflictor->r.currentOrigin[0] : 0.0f, inflictor ? inflictor->r.currentOrigin[1] : 0.0f,
		inflictor ? inflictor->r.currentOrigin[2] : 0.0f );
}

// fills a bot's command for the time t; false when the bot is not in the scenario
bool G_ScenarioCommand( int clientNum, int t, usercmd_t *ucmd ) {
	gentity_t *ent = &g_entities[clientNum];
	const bool attacker = ( clientNum == scn.client[0] ) ? true : false;
	const int phase = t - scn.repStart - SCN_SETTLE_MSEC;
	vec3_t view = { 0, 0, 0 };
	int i;

	if ( !G_ScenarioControls( clientNum ) || !ent->client ) {
		return false;
	}

	memset( ucmd, 0, sizeof( *ucmd ) );
	ucmd->serverTime = t;
	UC_SetWeapon( *ucmd, ent->client->ps.weapon );
	view[YAW] = attacker ? scn.yaw : AngleNormalize360( scn.yaw + 180 );
	for ( i = 0; i < 3; i++ ) {
		ucmd->angles[i] = ANGLE2SHORT( view[i] ) - ent->client->ps.delta_angles[i];
	}
	if ( !scn.repStart || phase < 0 || scn.done ) {
		return true;	// still
	}

	switch ( scn.type ) {
	case scenarioType_t::SCN_SABER:
		if ( phase < 100 && ( attacker || scn.counter ) ) {
			const signed char *dir = scnDirs[( scn.rep + 7 ) % 8];	// repetition 1 in the first direction

			ucmd->buttons = buttonFlags_t::BUTTON_ATTACK;
			ucmd->forwardmove = dir[0];
			ucmd->rightmove = attacker ? dir[1] : -dir[1];
		}
		break;
	case scenarioType_t::SCN_FORCE:
		if ( !attacker ) {
			break;
		}
		switch ( scn.power ) {
		case forcePowers_t::FP_LIGHTNING:
			if ( phase < 1000 ) ucmd->buttons = buttonFlags_t::BUTTON_FORCE_LIGHTNING;
			break;
		case forcePowers_t::FP_GRIP:
			if ( phase < 1500 ) ucmd->buttons = buttonFlags_t::BUTTON_FORCEGRIP;
			break;
		case forcePowers_t::FP_DRAIN:
			if ( phase < 1000 ) ucmd->buttons = buttonFlags_t::BUTTON_FORCE_DRAIN;
			break;
		case forcePowers_t::FP_HEAL:
		case forcePowers_t::FP_PUSH:
		case forcePowers_t::FP_PULL:
		case forcePowers_t::FP_TELEPATHY:
		case forcePowers_t::FP_TEAM_HEAL:
		case forcePowers_t::FP_TEAM_FORCE:	// instant powers: used three times, the button released in between
			ucmd->forcesel = to_underlying( scn.power );
			ent->client->ps.fd.forcePowerSelected = scn.power;
			if ( phase < 1250 && phase % 600 < 50 ) ucmd->buttons = buttonFlags_t::BUTTON_FORCEPOWER;
			break;
		case forcePowers_t::FP_LEVITATION:	// a force jump to the right, the jump held
			if ( phase < 1000 ) {
				ucmd->upmove = 127;
				ucmd->rightmove = 127;
			}
			break;
		case forcePowers_t::FP_SPEED:	// switched on, then a run to the right
			ucmd->forcesel = to_underlying( scn.power );
			ent->client->ps.fd.forcePowerSelected = scn.power;
			if ( phase < 50 ) ucmd->buttons = buttonFlags_t::BUTTON_FORCEPOWER;
			if ( phase < 1000 ) ucmd->rightmove = 127;
			break;
		case forcePowers_t::FP_SABERTHROW:	// the saber thrown and held out
			if ( phase < 1000 ) ucmd->buttons = buttonFlags_t::BUTTON_ALT_ATTACK;
			break;
		default:	// powers used on oneself: switched on once, held until the next reset
			ucmd->forcesel = to_underlying( scn.power );
			ent->client->ps.fd.forcePowerSelected = scn.power;
			if ( phase < 50 ) ucmd->buttons = buttonFlags_t::BUTTON_FORCEPOWER;
			break;
		}
		break;
	case scenarioType_t::SCN_WEAPON:
		if ( attacker && phase < 1000 ) {
			ucmd->buttons = scn.alt ? buttonFlags_t::BUTTON_ALT_ATTACK : buttonFlags_t::BUTTON_ATTACK;
		}
		break;
	default:
		break;
	}
	return true;
}

static int G_ScenarioArgInt( int n, int def ) {
	char arg[MAX_TOKEN_CHARS];

	if ( trap->Argc() <= n ) {
		return def;
	}
	trap->Argv( n, arg, sizeof( arg ) );
	return atoi( arg );
}

void Svcmd_Scenario_f( void ) {
	char type[MAX_TOKEN_CHARS], arg[MAX_TOKEN_CHARS];
	int i, n = 0;

	trap->Argv( 1, type, sizeof( type ) );
	if ( !Q_stricmp( type, "off" ) || !type[0] ) {
		scn.type = scenarioType_t::SCN_NONE;
		return;
	}
	if ( !trap->Cvar_VariableIntegerValue( "sv_cheats" ) ) {
		trap->Print( "scenario: needs sv_cheats 1 (devmap)\n" );
		return;
	}

	memset( &scn, 0, sizeof( scn ) );
	scn.rep = -1;	// the first reset starts the warm-up, repetition 0
	{ // the attacker, the first bot that is not its teammate as the defender, and a teammate as the ally
		bool defender = false, ally = false;

		for ( i = 0; i < MAX_CLIENTS; i++ ) {
			gentity_t *e = &g_entities[i];

			if ( !e->inuse || !e->client || !Any( e->r.svFlags & svFlags_t::SVF_BOT )
				|| e->client->pers.connected != clientConnected_t::CON_CONNECTED ) {
				continue;
			}
			if ( !n ) {
				scn.client[0] = i;
				n = 1;
			}
			else if ( !defender && !OnSameTeam( &g_entities[scn.client[0]], e ) ) {
				scn.client[1] = i;
				defender = true;
			}
			else if ( !ally && OnSameTeam( &g_entities[scn.client[0]], e ) ) {
				scn.client[2] = i;
				ally = true;
			}
		}
		if ( !defender ) {
			trap->Print( "scenario: needs two bots\n" );
			return;
		}
		scn.numClients = ally ? 3 : 2;
	}
	scn.reps = G_ScenarioArgInt( 2, 100 );
	trap->Argv( 3, arg, sizeof( arg ) );
	if ( !Q_stricmp( type, "saber" ) ) {
		static const char *styles[] = { "fast", "medium", "strong", "dual", "staff" };
		static const saber_styles_t styleValues[] = { saber_styles_t::SS_FAST, saber_styles_t::SS_MEDIUM, saber_styles_t::SS_STRONG, saber_styles_t::SS_DUAL, saber_styles_t::SS_STAFF };
		char mode[MAX_TOKEN_CHARS];

		scn.style = saber_styles_t::SS_MEDIUM;
		for ( i = 0; i < (int)ARRAY_LEN( styles ); i++ ) {
			if ( !Q_stricmp( arg, styles[i] ) ) {
				scn.style = styleValues[i];
			}
		}
		trap->Argv( 4, mode, sizeof( mode ) );
		scn.counter = !Q_stricmp( mode, "attack" ) ? true : false;
		scn.dist = (float)G_ScenarioArgInt( 5, 56 );
		scn.type = scenarioType_t::SCN_SABER;
	}
	else if ( !Q_stricmp( type, "force" ) ) {
		static const char *powers[] = { "lightning", "grip", "drain", "rage", "protect", "absorb", "heal", "levitation", "speed",
			"push", "pull", "telepathy", "teamheal", "teamforce", "see", "saberthrow" };
		static const forcePowers_t powerValues[] = { forcePowers_t::FP_LIGHTNING, forcePowers_t::FP_GRIP, forcePowers_t::FP_DRAIN, forcePowers_t::FP_RAGE, forcePowers_t::FP_PROTECT, forcePowers_t::FP_ABSORB,
			forcePowers_t::FP_HEAL, forcePowers_t::FP_LEVITATION, forcePowers_t::FP_SPEED, forcePowers_t::FP_PUSH, forcePowers_t::FP_PULL, forcePowers_t::FP_TELEPATHY,
			forcePowers_t::FP_TEAM_HEAL, forcePowers_t::FP_TEAM_FORCE, forcePowers_t::FP_SEE, forcePowers_t::FP_SABERTHROW };

		scn.power = forcePowers_t::FP_LIGHTNING;
		for ( i = 0; i < (int)ARRAY_LEN( powers ); i++ ) {
			if ( !Q_stricmp( arg, powers[i] ) ) {
				scn.power = powerValues[i];
			}
		}
		if ( ( scn.power == forcePowers_t::FP_TEAM_HEAL || scn.power == forcePowers_t::FP_TEAM_FORCE ) && scn.numClients < 3 ) {
			trap->Print( "scenario: the team powers need a team game and a third bot, a teammate of the first\n" );
			return;
		}
		scn.level = static_cast<forcePowerLevels_t>( Q_max( 1, Q_min( 3, G_ScenarioArgInt( 4, 3 ) ) ) );
		scn.dist = (float)G_ScenarioArgInt( 5, 128 );
		scn.defenderLevel = static_cast<forcePowerLevels_t>( Q_max( 0, Q_min( 3, G_ScenarioArgInt( 6, 0 ) ) ) );
		scn.type = scenarioType_t::SCN_FORCE;
	}
	else if ( !Q_stricmp( type, "weapon" ) ) {
		scn.weapon = static_cast<weapon_t>( atoi( arg ) );
		if ( scn.weapon <= weapon_t::WP_NONE || scn.weapon >= weapon_t::WP_NUM_WEAPONS ) {
			trap->Print( "scenario: bad weapon number\n" );
			return;
		}
		scn.alt = G_ScenarioArgInt( 4, 0 ) ? true : false;
		scn.dist = (float)G_ScenarioArgInt( 5, 256 );
		scn.type = scenarioType_t::SCN_WEAPON;
	}
	else {
		trap->Print( "scenario: saber, force, weapon or off\n" );
		return;
	}
	if ( !G_ScenarioFindPlace() ) {
		trap->Print( "scenario: no open spawn point for a distance of %.0f\n", scn.dist );
		scn.type = scenarioType_t::SCN_NONE;
		return;
	}
	scn.traceReps = trap->Cvar_VariableIntegerValue( "scenario_trace" );
	trap->Print( "scenario %s: %d repetitions of %d ms, bots %d and %d, distance %.0f\n", type, scn.reps, SCN_REP_MSEC,
		scn.client[0], scn.client[1], scn.dist );
}

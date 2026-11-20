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
// scenario saber <reps> <fast|medium|strong|dual|staff> <mode> [distance]
//   mode: idle (an attack in one of the eight directions in turn), attack (the same, the defender attacking at the same
//   time), and the special moves of plan step 20, the defender idle unless said: throw (the saber thrown, the alternate
//   attack held 1 s), lock (both the same attack, held, in a private duel; during a lock the attacker presses attack
//   every 100 ms),
//   kata (attack and alternate attack together), back and backcrouch (facing away from the defender, backward and
//   attack, crouched for the second), jump (a jump, then forward and attack), lunge (crouched, then forward and attack),
//   cartwheel (a jump, then the side and attack, right and left in turn), backflip (backward, jump and attack), kick
//   (alternate attack and a direction: forward, back, right, left in turn), airkick (the same after a force jump),
//   rollstab (a run forward, crouched into a roll, attack held to its end), stabdown (the defender knocked down, forward
//   and attack)
// scenario force <reps> <power> [level 1 to 3, 3] [distance] [the defender's level 0 to 3 in push or pull, 0]
//   power: lightning grip drain rage protect absorb heal levitation speed push pull telepathy teamheal teamforce see
//   saberthrow (teamheal and teamforce need the ally; against saberthrow the defender holds no saber)
// scenario weapon <reps> <weapon number> <0 primary|1 alternate|2 both> [distance]
//   the button held for hold= ms (1000), released, which fires the charged weapons; the disruptor's alternate is the
//   sniper shot (zoomed in, the zoom locked, then the shot charged for hold= ms), the det pack's alternate sets off a
//   pack thrown first; the emplaced gun (17) is spawned in front of the attacker, which mounts it with the use key
// scenario turret <reps> <g2|hoth> [distance]
//   a turret spawned between the bots (misc_turretG2 or misc_turret) fires at the defender, the attacker untargeted
// scenario flag <reps> <capture|return|switch> [distance] (CTF and CTY, with pickup=)
//   capture: the attacker takes the enemy flag, then walks back onto its own; return: the defender kills the carrier
//   with a rocket, then runs onto the flag dropped, which goes back to its base
//   switch: the attacker takes the enemy flag, then changes team, which sends the flag back to its base
// scenario objective <reps> <number> [distance] (siege)
//   the attacker completes its team's objective of that number at the start of every repetition, which then is not
//   complete again at the next reset (an objective that neither counts for the round nor fires a target)
// scenario team <reps> switch [distance] (a team game)
//   the defender changes to the other team at the start of the script (SetTeam, the team command without its 5 s
//   wait), and every reset puts it back in its team without killing it
// scenario spectate <reps> <cycle|fly> [distance] (with the spectator; the players stand)
//   cycle: following the attacker, the spectator presses attack (the next player), the alternate attack (the
//   previous one), then jumps (it stops following); fly: a free spectator at the attacker's place flies forward 1 s
// scenario holdable <reps> <seeker|shield|medpac|medpac_big|binoculars|sentry|jetpack|healthdisp|ammodisp|eweb|cloak>
//   [distance] (the attacker carries the item and uses it at the start of the script with the item key, the item
//   selected; the jetpack and the dispensers, which cannot be selected, with the use key as the game uses them (TryUse):
//   the jetpack after a jump, then the jump held 1 s, a dispenser held 1 s on the teammate in front (target=ally; the
//   health dispenser's at 40 health, the ammunition's holding an empty blaster), or else on nothing, which throws the
//   ammunition; the shield's owner fires a blaster into it, the E-Web's fires it 1 s; before a medpac the attacker has
//   40 health; the defender holds no saber, so that the seeker's, the sentry's and the E-Web's shots hit)
// scenario item <reps> <item classname> [distance] (with pickup=: the item that far in front of the attacker, which
//   walks onto it; the attacker has 40 health, no armour, the saber only and no ammunition before)
// scenario vehicle <reps> <vehicle> <ride|destroy> [distance]
//   the vehicle (its name in ext_data/vehicles: swoop_mp, atst_vehicle, x-wing) spawned at every reset in front of the
//   attacker, 40 units of floor between their boxes, and dropped onto the floor (a spawn point is taken only where its
//   box fits); both bots hold the melee (a speeder fires only for a rider holding it or its saber off); the attacker
//   boards it with the use key (looking at it) at the start of the script; ride: a speeder fires 1 s at the defender,
//   drives 1 s at it, then its rider jumps off; a walker fires its laser 1 s and a rocket, walks 1 s, then its rider
//   gets off with the use key; a fighter takes off (the jump held 1.5 s), flies forward 0.6 s and fires 1 s, above the
//   defender; destroy: the
//   defender fires a rocket at the vehicle, whose health is 100 at most (the swoop's 80 kept)
// scenario off
// Options after the arguments: hold=<ms>, rep=<ms> (a repetition, 2000; the scripts read it to wait for the end);
// pickup=<units> (plan step 20, the game types: in a holocron game a holocron of the force scenario's power, in a Jedi
// Master game the Jedi Master's saber, in CTF and CTY the enemy flag, put that far in front of the attacker, which
// walks onto it before its script; in CTF and CTY the attacker's own flag as far behind it);
// target=ally (in a team game the attacker's teammate takes the defender's place and role, the defender the ally's);
// away (the defender turned away from the attacker);
// spectator, spectator=follow1, spectator=follow2 (plan step 20, teams and spectators: a bot spectator, the first
// spectator bot or else the first bot without a role, made a spectator, follows the attacker, or the first or second
// player by client number as the game's follow1 and follow2 do; it is reset to that at every repetition);
// item=<classname> (plan step 20, the items: with pickup=, that item in front of the attacker, as in an item scenario,
// before the script of any other scenario: a force scenario after a powerup picked up).
// Every reset clears the holdable items' state and what they spawned (the seeker, the sentry, the shield, the E-Web,
// the cloak and the jetpack switched off and full), and gives the attacker the holdable item of its scenario only.
// Every reset gets the bots off any vehicle, and removes the vehicle of a vehicle scenario, then spawns it again.
// The bots are the first ones that are not spectators; in a power duel the defender is the lone duelist. In siege a
// dead bot is respawned at once, not at the next wave.

#include "g_local.h"

extern void WP_ForcePowerStop( gentity_t *self, forcePowers_t forcePower );
extern qboolean G_SetSaber( gentity_t *ent, int saberNum, char *saberName, qboolean siegeOverride );
extern void Svcmd_GameStats_f( void );
extern void saberReactivate( gentity_t *saberent, gentity_t *saberOwner );
extern void SaberGotHit( gentity_t *self, gentity_t *other, trace_t *trace );
extern void SaberUpdateSelf( gentity_t *ent );
extern void WP_SaberRemoveG2Model( gentity_t *saberent );
extern qboolean G_CallSpawn( gentity_t *ent );
extern void G_Knockdown( gentity_t *victim );
extern void HolocronRespawn( gentity_t *self );
extern void ThrowSaberToAttacker( gentity_t *self, gentity_t *attacker );
extern gentity_t *Team_ResetFlag( int team );
extern void Team_ResetFlags( void );
extern void G_SiegeSetObjectiveComplete( int team, int objective, qboolean failIt );
extern void Jedi_Decloak( gentity_t *self );
extern void EWebDisattach( gentity_t *owner, gentity_t *eweb );
extern char *modNames[];
extern gentity_t *NPC_Spawn_Do( gentity_t *ent );
extern void BG_VehicleAdjustBBoxForOrientation( Vehicle_t *veh, vec3_t origin, vec3_t mins, vec3_t maxs, int clientNum,
	int tracemask, void ( *localTrace )( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs,
	const vec3_t end, int passEntityNum, int contentMask ) );

#define SCN_REP_MSEC		2000	// one repetition
#define SCN_SETTLE_MSEC		500		// still, after the reset, before the script starts
#define SCN_PICKUP_WALK_MSEC	300	// pickup=: the attacker walks forward onto the object
#define SCN_PICKUP_MSEC		400		// pickup=: the script starts that much later

typedef enum {
	SCN_NONE,
	SCN_SABER,
	SCN_FORCE,
	SCN_WEAPON,
	SCN_TURRET,
	SCN_FLAG,
	SCN_OBJECTIVE,
	SCN_TEAM,
	SCN_SPECTATE,
	SCN_HOLDABLE,
	SCN_ITEM,
	SCN_VEHICLE
} scenarioType_t;

// what the bots do in a vehicle scenario
typedef enum {
	VM_RIDE,
	VM_DESTROY
} vehicleMode_t;

// what the bots do in a flag scenario
typedef enum {
	FM_CAPTURE,
	FM_RETURN,
	FM_SWITCH
} flagMode_t;

// the spectator of a scenario: none, following the attacker, the game's follow1 or follow2, or free (spectate fly)
typedef enum {
	SPEC_NONE,
	SPEC_ATTACKER,
	SPEC_FOLLOW1,
	SPEC_FOLLOW2,
	SPEC_FREE
} spectatorMode_t;

// what the spectator does in a spectate scenario
typedef enum {
	SPM_CYCLE,
	SPM_FLY
} spectateMode_t;

#define SCN_SPECTATOR		3		// the spectator's slot in scn.client, after the attacker, the defender and the ally

// what the bots do in a saber scenario, in the order of the names the command takes
typedef enum {
	SM_IDLE,
	SM_ATTACK,
	SM_THROW,
	SM_LOCK,
	SM_KATA,
	SM_BACK,
	SM_BACKCROUCH,
	SM_JUMP,
	SM_LUNGE,
	SM_CARTWHEEL,
	SM_BACKFLIP,
	SM_KICK,
	SM_AIRKICK,
	SM_ROLLSTAB,
	SM_STABDOWN,
	SM_NUM_MODES
} saberMode_t;

static struct {
	scenarioType_t	type;
	int				reps;			// repetitions asked
	int				rep;			// the current repetition: 0 is the warm-up, then 1 to reps
	qboolean		done;			// all repetitions played: the bots stand still
	int				repStart;		// level.time of the current repetition's reset, 0 before the first
	int				client[4];		// attacker, defender, ally (a teammate of the attacker, in team games), spectator
	team_t			team[4];		// their teams, which every reset gives back
	int				numClients;		// 2, or 3 with the ally
	int				style;			// the attacker's saber style
	saberMode_t		saberMode;
	forcePowers_t	power;
	int				level;			// the attacker's level in that power
	int				defenderLevel;	// push and pull: the defender's level in the same power, which resists it
	int				weapon;
	int				fire;			// 0 primary, 1 alternate, 2 both
	int				hold;			// ms the fire button is held (option hold=)
	int				repMsec;		// a repetition (option rep=)
	qboolean		turretG2;		// misc_turretG2, else misc_turret
	gentity_t		*spawned[2];	// what the scenario spawned (the emplaced gun, a turret and its top), freed at its end
	gentity_t		*pickup;		// pickup=: the holocron (spawned), the Jedi Master's saber or the enemy flag (the map's)
	float			pickupDist;		// pickup=: its distance in front of the attacker, 0 without
	gentity_t		*ownFlag;		// pickup= in CTF and CTY: the attacker's flag, as far behind it
	vec3_t			flagSpot[2];	// the two flags' places, the enemy's then the attacker's
	flagMode_t		flagMode;
	gentity_t		*objective;		// the siege objective the attacker completes
	qboolean		actionDone;		// the repetition's one action done (the objective completed, the team changed)
	qboolean		away;			// the defender turned away from the attacker
	spectatorMode_t	spectatorMode;
	spectateMode_t	spectateMode;
	holdable_t		holdable;		// the holdable item of a holdable scenario
	char			itemClass[MAX_QPATH];	// the item of an item scenario or of item=, "" without
	char			vehicleType[MAX_QPATH];	// the vehicle of a vehicle scenario (ext_data/vehicles)
	vehicleMode_t	vehicleMode;
	gentity_t		*vehicle;		// the vehicle spawned at the last reset
	float			vehicleDist;	// its distance in front of the attacker
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
	int		saberLocked[3];		// instants in a saber lock
	float	path[3];			// distance moved
	int		end[3][3];			// the place at the end of each repetition against the place at its start, rounded to the unit
	// the scores at the start of the counted repetitions (plan step 20, the game types)
	int		scoreStart[3], capturesStart[3], teamScoreStart[2];
	// plan step 20, teams and spectators, by slot (the spectator's last): the team changes, the instants as a free
	// spectator and the distance moved then, the instants following the attacker, the defender or the ally, the
	// distance from the player followed
	int		teamChanges[4], freeSpectator[4], following[4][3];
	float	freePath[4], followGap[4];
	team_t	team[4];			// the previous instant's team and place, from the reset on
	vec3_t	teamOrigin[4];
	// plan step 20, the items, by role: armour and ammunition gained and lost, the instants cloaked, flying a jetpack
	// and zoomed in
	int		armorGained[3], armorLost[3], ammoGained[3], ammoSpent[3], cloaked[3], jetpack[3], zoomed[3];
	int		armor[3], ammo[3];	// the previous instant's values, from the reset on
	// plan step 20, the vehicles: by role the boardings and exits, the instants aboard and the distance moved then; the
	// vehicle's health and shield lost, and its destructions
	int		boardings[3], exits[3], aboard[3];
	float	aboardPath[3];
	int		vehicleNum[3];		// the previous instant's vehicle, from the reset on
	int		vehicleHealthLost, vehicleShieldLost, vehicleDestroyed;
	int		vehicleHealth, vehicleShield;	// the previous instant's values, from the reset on
	// the previous instant's values, from the reset on
	int		health[3], force[3];
	vec3_t	origin[3], start[3];
} scnStats;

// the eight attack directions, as forward and right moves
static const signed char scnDirs[8][2] = {
	{ 127, 0 }, { -127, 0 }, { 0, -127 }, { 0, 127 }, { 127, -127 }, { 127, 127 }, { -127, -127 }, { -127, 127 }
};

qboolean G_ScenarioControls( int clientNum ) {
	int i;

	if ( scn.type == SCN_NONE ) {
		return qfalse;
	}
	for ( i = 0; i < scn.numClients; i++ ) {
		if ( clientNum == scn.client[i] ) {
			return qtrue;
		}
	}
	return ( scn.spectatorMode != SPEC_NONE && clientNum == scn.client[SCN_SPECTATOR] ) ? qtrue : qfalse;
}

// the attacker's view: towards the defender, or away from it for the back attacks
static float G_ScenarioAttackerYaw( void ) {
	const qboolean back = ( scn.type == SCN_SABER && ( scn.saberMode == SM_BACK || scn.saberMode == SM_BACKCROUCH ) ) ? qtrue : qfalse;

	return back ? AngleNormalize360( scn.yaw + 180 ) : scn.yaw;
}

// the defender's view: towards the attacker, or away from it (option away)
static float G_ScenarioDefenderYaw( void ) {
	return scn.away ? scn.yaw : AngleNormalize360( scn.yaw + 180 );
}

// the ally's place: beside the attacker, on its right
static void G_ScenarioAllyPlace( const vec3_t origin, float yaw, vec3_t out ) {
	const vec3_t right = { Q_sinpif( yaw / 180.0f ), -Q_cospif( yaw / 180.0f ), 0 };

	VectorMA( origin, 64, right, out );
}

// a player's box goes from start to place, where it has a floor at start's height
static qboolean G_ScenarioOpenFloor( const vec3_t start, const vec3_t place ) {
	const vec3_t mins = { -15, -15, DEFAULT_MINS_2 };
	const vec3_t maxs = { 15, 15, DEFAULT_MAXS_2 };
	vec3_t below;
	trace_t tr;

	trap->Trace( &tr, start, mins, maxs, place, ENTITYNUM_NONE, MASK_SOLID, qfalse, 0, 0 );
	if ( tr.fraction < 1.0f || tr.startsolid || tr.allsolid ) {
		return qfalse;
	}
	VectorCopy( place, below );
	below[2] -= 48;
	trap->Trace( &tr, place, mins, maxs, below, ENTITYNUM_NONE, MASK_SOLID, qfalse, 0, 0 );
	return ( tr.fraction < 1.0f && !tr.startsolid && tr.endpos[2] >= start[2] - 24 ) ? qtrue : qfalse;
}

// the spawn points in turn: free for all's, then CTF's, then siege's, for the maps without the first kind (plan step
// 20, the game types)
static gentity_t *G_ScenarioNextSpot( gentity_t *spot, size_t *kind ) {
	static const char *const kinds[] = { "info_player_deathmatch", "team_CTF_redspawn", "team_CTF_bluespawn",
		"info_player_siegeteam1", "info_player_siegeteam2" };

	for ( ; *kind < ARRAY_LEN( kinds ); ( *kind )++, spot = NULL ) {
		if ( ( spot = G_Find( spot, FOFS( classname ), kinds[*kind] ) ) != NULL ) {
			return spot;
		}
	}
	return NULL;
}

static qboolean G_ScenarioVehicleFits( void );

// a place where the attacker stands on a spawn point and the defender on the floor in front of it
static qboolean G_ScenarioFindPlace( void ) {
	const vec3_t mins = { -15, -15, DEFAULT_MINS_2 };
	const vec3_t maxs = { 15, 15, DEFAULT_MAXS_2 };
	gentity_t *spot = NULL;
	size_t kind = 0;

	while ( ( spot = G_ScenarioNextSpot( spot, &kind ) ) != NULL ) {
		vec3_t fwd, start, front, below;
		trace_t tr;

		VectorSet( fwd, Q_cospif( spot->s.angles[YAW] / 180.0f ), Q_sinpif( spot->s.angles[YAW] / 180.0f ), 0 );
		VectorCopy( spot->s.origin, start );
		start[2] += 9;
		VectorMA( start, scn.dist + 32, fwd, front );
		trap->Trace( &tr, start, mins, maxs, front, ENTITYNUM_NONE, MASK_SOLID, qfalse, 0, 0 );
		if ( tr.fraction < 1.0f || tr.startsolid || tr.allsolid ) {
			continue;
		}
		VectorMA( start, scn.dist, fwd, front );
		VectorCopy( front, below );
		below[2] -= 48;
		trap->Trace( &tr, front, mins, maxs, below, ENTITYNUM_NONE, MASK_SOLID, qfalse, 0, 0 );
		if ( tr.fraction >= 1.0f || tr.startsolid || tr.endpos[2] < start[2] - 24 ) {
			continue;	// no floor at the attacker's height in front
		}
		if ( scn.numClients == 3 ) { // the ally's place: open from the attacker's, with a floor at its height
			vec3_t ally;

			G_ScenarioAllyPlace( start, spot->s.angles[YAW], ally );
			if ( !G_ScenarioOpenFloor( start, ally ) ) {
				continue;
			}
		}
		if ( scn.pickupDist && ( level.gametype == GT_CTF || level.gametype == GT_CTY ) ) {
			// the attacker's flag behind it, where it walks back to
			vec3_t behind;

			VectorMA( start, -scn.pickupDist, fwd, behind );
			if ( !G_ScenarioOpenFloor( start, behind ) ) {
				continue;
			}
		}
		{ // away from doors and lifts, which crush the bots standing in their way
			int e;
			qboolean nearMover = qfalse;

			for ( e = MAX_CLIENTS; e < level.num_entities && !nearMover; e++ ) {
				const gentity_t *m = &g_entities[e];
				vec3_t center;

				if ( !m->inuse || m->s.eType != ET_MOVER ) {
					continue;
				}
				VectorAdd( m->r.absmin, m->r.absmax, center );
				VectorScale( center, 0.5f, center );
				nearMover = ( Distance( center, start ) < 512 ) ? qtrue : qfalse;
			}
			if ( nearMover ) {
				continue;
			}
		}
		VectorCopy( start, scn.origin );
		scn.yaw = spot->s.angles[YAW];
		if ( scn.type == SCN_VEHICLE && !G_ScenarioVehicleFits() ) {
			continue;
		}
		return qtrue;
	}
	return qfalse;
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
	saberent->r.svFlags |= SVF_NOCLIENT;
	saberent->s.eFlags = EF_NODRAW;
	saberent->s.saberInFlight = qfalse;
	saberent->s.loopSound = 0;
	saberent->s.loopIsSoundset = qfalse;
	WP_SaberRemoveG2Model( saberent );
	client->ps.saberInFlight = qfalse;
	client->ps.saberEntityState = 0;
	client->ps.saberThrowDelay = 0;
	client->saberKnockedTime = 0;
}

// at every reset, what a holdable item left (a use in the previous repetition, or a bot's in the free play): the seeker,
// the sentry, the shield and the E-Web removed, the cloak and the jetpack switched off and full
static void G_ScenarioClearHoldables( gentity_t *ent ) {
	gclient_t *client = ent->client;
	int i;

	client->ps.eFlags &= ~EF_SEEKERDRONE;
	client->ps.droneExistTime = client->ps.droneFireTime = 0;
	for ( i = MAX_CLIENTS; i < level.num_entities; i++ ) {
		gentity_t *e = &g_entities[i];

		if ( e->inuse && e->classname
			&& ( ( e->s.owner == ent->s.number && !strcmp( e->classname, "sentryGun" ) )		// ItemUse_Sentry
				|| ( e->parent == ent && !e->item && !strcmp( e->classname, "item_shield" ) ) ) ) {	// PlaceShield
			G_FreeEntity( e );
		}
	}
	client->ps.fd.sentryDeployed = qfalse;
	if ( client->ewebIndex ) {
		gentity_t *eweb = &g_entities[client->ewebIndex];

		EWebDisattach( ent, eweb );
		G_FreeEntity( eweb );
	}
	if ( client->ps.powerups[PW_CLOAKED] ) {
		Jedi_Decloak( ent );
	}
	client->ps.cloakFuel = 100;
	client->cloakToggleTime = 0;
	Jetpack_Off( ent );
	client->ps.jetpackFuel = 100;
	client->jetPackToggleTime = 0;
}

// the vehicle of a vehicle scenario while it exists (an exploded one frees itself, DeathUpdate)
static gentity_t *G_ScenarioVehicle( void ) {
	gentity_t *v = scn.vehicle;

	return ( v && v->inuse && v->client && v->m_pVehicle && v->client->NPC_class == CLASS_VEHICLE ) ? v : NULL;
}

// at every reset, a bot on a vehicle (the scenario's, or one of the map's taken in the free play) gets off where it is,
// as with the use key but forced (Eject); when no side is free, as a dead rider, whom Eject leaves in place
static void G_ScenarioLeaveVehicle( gentity_t *ent ) {
	gentity_t *veh = &g_entities[ent->client->ps.m_iVehicleNum];

	if ( !ent->client->ps.m_iVehicleNum || !veh->inuse || !veh->m_pVehicle ) {
		return;
	}
	if ( !veh->m_pVehicle->m_pVehicleInfo->Eject( veh->m_pVehicle, (bgEntity_t *)ent, qtrue ) ) {
		const int health = ent->health;

		ent->health = 0;
		veh->m_pVehicle->m_pVehicleInfo->Eject( veh->m_pVehicle, (bgEntity_t *)ent, qtrue );
		ent->health = health;
	}
	ent->flags &= ~FL_VEH_BOARDING;	// a dismount started (UpdateRider)
}

static void G_ScenarioPlace( gentity_t *ent, const vec3_t origin, float yaw ) {
	gclient_t *client = ent->client;
	vec3_t angles = { 0, 0, 0 };
	int i, j;

	G_ScenarioLeaveVehicle( ent );
	if ( level.gametype == GT_SIEGE
		&& ( ent->health <= 0 || client->ps.stats[STAT_HEALTH] <= 0 || client->tempSpectate >= level.time ) ) {
		// in siege the dead wait for the next wave as spectators with 1 point of health (ClientRespawn, g_siegeRespawn):
		// respawned at once, as ClientRespawn does in the other game types
		if ( client->tempSpectate < level.time ) {
			MaintainBodyQueue( ent );
		}
		client->tempSpectate = 0;
		ClientSpawn( ent );
	}
	else if ( ent->health <= 0 || client->ps.stats[STAT_HEALTH] <= 0 ) {
		ClientRespawn( ent );
	}
	if ( level.gametype == GT_CTF || level.gametype == GT_CTY ) {
		// the times the flag bonuses look back to (10 s for an assist, 8 s for a carrier's defence), longer than a
		// repetition: set by the free play or the previous repetition
		client->pers.teamState.lasthurtcarrier = client->pers.teamState.lastreturnedflag = 0;
		client->pers.teamState.lastfraggedcarrier = client->pers.teamState.flagsince = 0;
	}
	G_ScenarioSaberToHand( ent );
	G_ScenarioClearHoldables( ent );
	// the blades' last positions, from before the bot is moved here: the first sweep of a repetition would start from them
	client->lastSaberStorageTime = 0;
	client->hasCurrentPosition = qfalse;
	for ( i = 0; i < MAX_SABERS; i++ ) {
		for ( j = 0; j < MAX_BLADES; j++ ) {
			client->saber[i].blade[j].trail.lastTime = 0;
		}
	}
	for ( i = 0; i < NUM_FORCE_POWERS; i++ ) {
		if ( client->ps.fd.forcePowersActive & ( 1 << i ) ) {
			WP_ForcePowerStop( ent, (forcePowers_t)i );
		}
		client->ps.fd.forcePowerDuration[i] = 0;
		client->ps.fd.forcePowerDebounce[i] = 0;
	}
	ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH] = 100;
	if ( level.gametype == GT_POWERDUEL && client->sess.duelTeam == DUELTEAM_LONE ) {
		// the lone duelist's health as it spawns before any win (ClientSpawn)
		ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH] = g_powerDuelStartHealth.integer;
	}
	client->ps.stats[STAT_ARMOR] = 0;
	client->ps.fd.forcePower = 100;
	client->ps.fd.forcePowerRegenDebounceTime = level.time;
	client->ps.fd.forceGripBeingGripped = 0;
	client->ps.fd.forceRageRecoveryTime = 0;
	client->ps.forceHandExtend = HANDEXTEND_NONE;
	client->ps.forceHandExtendTime = 0;
	client->ps.weaponTime = 0;
	client->ps.weaponstate = WEAPON_READY;
	client->ps.saberBlocked = BLOCKED_NONE;
	// a saber lock still running at the end of the previous repetition, and its private duel (a death ends it)
	client->ps.saberLockTime = 0;
	client->ps.saberLockFrame = 0;
	client->ps.saberLockHits = 0;
	client->ps.saberLockAdvance = qfalse;
	client->ps.duelInProgress = qfalse;
	client->ps.saberMove = LS_READY;
	client->ps.saberHolstered = 0;
	client->ps.legsTimer = client->ps.torsoTimer = 0;
	client->ps.electrifyTime = 0;
	client->ps.pm_time = 0;
	// PMF_BACKWARDS_RUN and movementDir change only with movement commands, so a still bot kept those of its last step
	// in the free play: a backward one turns the legs of a pushed player the other way (BG_G2PlayerAngles), which moved
	// the model points the missiles hit
	client->ps.pm_flags &= ~( PMF_TIME_KNOCKBACK | PMF_TIME_WATERJUMP | PMF_BACKWARDS_RUN );
	client->ps.movementDir = 0;
	// the disruptor's zoom, the rocket launcher's lock and a charge, which the next repetition would start from
	client->ps.zoomMode = 0;
	client->ps.zoomLocked = qfalse;
	client->ps.zoomFov = 0;
	BG_ClearRocketLock( &client->ps );
	client->ps.weaponChargeTime = 0;
	client->ps.weaponChargeSubtractTime = 0;
	ent->flags &= ~FL_NOTARGET;	// the turrets' attacker
	client->ps.saberAttackWound = client->ps.saberIdleWound = 0;
	client->invulnerableTimer = 0;
	client->ps.eFlags &= ~EF_INVULNERABLE;
	// powerups picked up in the free play before the scenario, and the pain sound timers (P_DamageFeedback), which a
	// pain in the previous repetition or in the free play would carry into this one
	memset( client->ps.powerups, 0, sizeof( client->ps.powerups ) );
	// the holocrons carried, which keep their powers (HolocronUpdate); G_ScenarioReturnPickup puts them back in place
	memset( client->ps.holocronsCarried, 0, sizeof( client->ps.holocronsCarried ) );
	client->ps.holocronBits = 0;
	client->ps.holocronCantTouch = 0;
	client->ps.holocronCantTouchTime = 0;
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
	client->ps.eFlags ^= EF_TELEPORT_BIT;
	BG_PlayerStateToEntityState( &client->ps, &ent->s, qtrue );
	VectorCopy( client->ps.origin, ent->r.currentOrigin );
	trap->LinkEntity( (sharedEntity_t *)ent );
}

// at every reset, a team changed in the previous repetition given back without the death of a change (SetTeam)
static void G_ScenarioRestoreTeam( int slot ) {
	gentity_t *ent = &g_entities[scn.client[slot]];

	if ( ent->client->sess.sessionTeam == scn.team[slot] ) {
		return;
	}
	ent->client->sess.sessionTeam = scn.team[slot];
	ent->client->ps.persistant[PERS_TEAM] = scn.team[slot];
	ClientUserinfoChanged( scn.client[slot] );	// the team in the player's configstring, its model's skin
}

// at every reset, the spectator following its player again, or free at the attacker's place (spectate fly)
static void G_ScenarioPlaceSpectator( void ) {
	gentity_t *ent = &g_entities[scn.client[SCN_SPECTATOR]];
	gclient_t *client = ent->client;

	client->buttons = client->oldbuttons = 0;	// the follow commands act on a press
	if ( scn.spectatorMode == SPEC_FREE ) {
		vec3_t angles;

		VectorSet( angles, 0, scn.yaw, 0 );
		if ( client->sess.spectatorState == SPECTATOR_FOLLOW ) {
			StopFollowing( ent );
		}
		VectorCopy( scn.origin, client->ps.origin );
		VectorClear( client->ps.velocity );
		SetClientViewAngle( ent, angles );
	}
	else { // the attacker by its number, or the game's follow1 and follow2 (SpectatorClientEndFrame)
		client->sess.spectatorState = SPECTATOR_FOLLOW;
		client->sess.spectatorClient = ( scn.spectatorMode == SPEC_FOLLOW1 ) ? -1
			: ( scn.spectatorMode == SPEC_FOLLOW2 ) ? -2 : scn.client[0];
	}
	// the bot's flag, which StopFollowing clears (the jump of spectate cycle in the previous repetition, or above)
	ent->r.svFlags |= SVF_BOT;
}

static void G_ScenarioEquip( gentity_t *ent, qboolean attacker ) {
	gclient_t *client = ent->client;

	// the holdable items carried: the attacker's of its holdable scenario only, selected but for the jetpack and the
	// dispensers, which cannot be (plan step 20, the items)
	client->ps.stats[STAT_HOLDABLE_ITEMS] = ( attacker && scn.type == SCN_HOLDABLE ) ? ( 1 << scn.holdable ) : 0;
	client->ps.stats[STAT_HOLDABLE_ITEM] = ( attacker && scn.type == SCN_HOLDABLE
		&& BG_IsItemSelectable( &client->ps, scn.holdable ) ) ? BG_GetItemIndexByTag( scn.holdable, IT_HOLDABLE ) : 0;

	client->ps.fd.forcePowerLevel[FP_SABER_OFFENSE] = FORCE_LEVEL_3;
	client->ps.fd.forcePowerLevel[FP_SABER_DEFENSE] = FORCE_LEVEL_3;
	client->ps.fd.forcePowersKnown |= ( 1 << FP_SABER_OFFENSE ) | ( 1 << FP_SABER_DEFENSE );
	if ( scn.type == SCN_FORCE && attacker ) {
		client->ps.fd.forcePowerLevel[scn.power] = scn.level;
		client->ps.fd.forcePowersKnown |= ( 1 << scn.power );
		// the side the power needs; the neutral powers on the dark side
		client->ps.fd.forceSide = ( forcePowerDarkLight[scn.power] == FORCE_LIGHTSIDE ) ? FORCE_LIGHTSIDE : FORCE_DARKSIDE;
	}
	if ( scn.type == SCN_FORCE && !attacker && ( scn.power == FP_PUSH || scn.power == FP_PULL ) ) {
		client->ps.fd.forcePowerLevel[scn.power] = scn.defenderLevel;
		if ( scn.defenderLevel > FORCE_LEVEL_0 ) {
			// a power not known does not resist (CanCounterThrow), and a siege class may not give it (WP_InitForcePowers)
			client->ps.fd.forcePowersKnown |= ( 1 << scn.power );
		}
	}
	if ( scn.type == SCN_SABER && attacker && scn.saberMode > SM_LOCK ) {
		// the jumps, rolls and the staff's back flip need the jump at level 2 at least
		client->ps.fd.forcePowerLevel[FP_LEVITATION] = FORCE_LEVEL_3;
		client->ps.fd.forcePowersKnown |= ( 1 << FP_LEVITATION );
	}
	if ( scn.type == SCN_SABER && attacker && scn.saberMode == SM_THROW ) {
		client->ps.fd.forcePowerLevel[FP_SABERTHROW] = FORCE_LEVEL_3;
		client->ps.fd.forcePowersKnown |= ( 1 << FP_SABERTHROW );
	}

	if ( scn.type == SCN_WEAPON || scn.type == SCN_TURRET || ( scn.type == SCN_FORCE && scn.power == FP_SABERTHROW && !attacker )
		|| ( scn.type == SCN_FLAG && scn.flagMode == FM_RETURN && !attacker ) || scn.type == SCN_HOLDABLE
		|| scn.type == SCN_VEHICLE ) {
		// the defender cannot block with a saber; in a flag's return it kills the carrier with a rocket, and the
		// vehicle in a vehicle's destruction; the shield's owner fires a blaster into it; the ammunition dispenser's
		// teammate holds an empty blaster (G_CanUseDispOn); a speeder fires only for a rider holding the melee or its
		// saber off (ClientEvents)
		const qboolean blaster = ( scn.type == SCN_HOLDABLE
			&& ( ( attacker && scn.holdable == HI_SHIELD ) || ( !attacker && scn.holdable == HI_AMMODISP ) ) ) ? qtrue : qfalse;
		int weapon = ( attacker && scn.type == SCN_WEAPON ) ? scn.weapon
			: ( scn.type == SCN_FLAG || ( scn.type == SCN_VEHICLE && scn.vehicleMode == VM_DESTROY && !attacker ) ) ? WP_ROCKET_LAUNCHER
			: blaster ? WP_BLASTER : WP_MELEE;

		if ( weapon == WP_EMPLACED_GUN ) { // the hands free to mount the gun, which takes the place of the weapon
			client->ps.ammo[weaponData[weapon].ammoIndex] = 999;
			weapon = WP_MELEE;
		}

		client->ps.stats[STAT_WEAPONS] |= ( 1 << weapon );
		client->ps.ammo[weaponData[weapon].ammoIndex] = ( blaster && !attacker ) ? 0 : 999;
		client->ps.weapon = weapon;
		return;
	}

	if ( attacker && scn.type == SCN_ITEM ) {
		// no weapon and no ammunition, so that a pickup gives the same at every repetition, and a weapon is never refused
		// for being carried already (BG_CanItemBeGrabbed)
		client->ps.stats[STAT_WEAPONS] = 0;
		memset( client->ps.ammo, 0, sizeof( client->ps.ammo ) );
		client->ps.weapon = WP_NONE;
		return;
	}
	client->ps.stats[STAT_WEAPONS] |= ( 1 << WP_SABER );
	client->ps.weapon = WP_SABER;
	if ( attacker && scn.type == SCN_SABER ) {
		G_SetSaber( ent, 0, scn.style == SS_STAFF ? "dual_1" : "single_1", qtrue );
		G_SetSaber( ent, 1, scn.style == SS_DUAL ? "single_1" : "none", qtrue );
		client->ps.fd.saberAnimLevel = client->ps.fd.saberAnimLevelBase = client->ps.fd.saberDrawAnimLevel = scn.style;
		client->saberCycleQueue = scn.style;
	}
	else {
		G_SetSaber( ent, 0, "single_1", qtrue );
		G_SetSaber( ent, 1, "none", qtrue );
		client->ps.fd.saberAnimLevel = client->ps.fd.saberAnimLevelBase = client->ps.fd.saberDrawAnimLevel = SS_MEDIUM;
		client->saberCycleQueue = SS_MEDIUM;
	}
	if ( level.gametype == GT_JEDIMASTER ) {
		// only the Jedi Master holds a saber, the Jedi Master's (JMSaberTouch): the others the melee and the bryar, as they
		// spawn (ClientSpawn); the saber set above is the one the attacker plays once it is the Jedi Master
		client->ps.stats[STAT_WEAPONS] = ( 1 << WP_MELEE ) | ( 1 << WP_BRYAR_PISTOL );
		client->ps.weapon = WP_BRYAR_PISTOL;
	}
}

// a player's ammunition of every kind, summed
static int G_ScenarioAmmo( const playerState_t *ps ) {
	int sum = 0, a;

	for ( a = 0; a < MAX_AMMO; a++ ) {
		sum += ps->ammo[a];
	}
	return sum;
}

// the end of a repetition: where each bot stands against its place at the start
static void G_ScenarioEndRepetition( void ) {
	int n, k;

	for ( n = 0; n < scn.numClients; n++ ) {
		const gentity_t *ent = &g_entities[scn.client[n]];

		if ( !ent->inuse || !ent->client ) {
			continue;
		}
		for ( k = 0; k < 3; k++ ) {
			scnStats.end[n][k] += (int)Q_roundf( ent->client->ps.origin[k] - scnStats.start[n][k] );
		}
	}
}

// the bots' values after the reset, from which the instants of the repetition are counted
static void G_ScenarioStartSampling( void ) {
	int n;

	for ( n = 0; n < scn.numClients; n++ ) {
		const gentity_t *ent = &g_entities[scn.client[n]];

		scnStats.health[n] = ent->health;
		scnStats.force[n] = ent->client->ps.fd.forcePower;
		VectorCopy( ent->client->ps.origin, scnStats.origin[n] );
		VectorCopy( ent->client->ps.origin, scnStats.start[n] );
		scnStats.team[n] = ent->client->sess.sessionTeam;
		VectorCopy( ent->client->ps.origin, scnStats.teamOrigin[n] );
		scnStats.armor[n] = ent->client->ps.stats[STAT_ARMOR];
		scnStats.ammo[n] = G_ScenarioAmmo( &ent->client->ps );
		scnStats.vehicleNum[n] = ent->client->ps.m_iVehicleNum;
	}
	if ( G_ScenarioVehicle() ) {
		const gentity_t *veh = G_ScenarioVehicle();

		scnStats.vehicleHealth = veh->health;
		scnStats.vehicleShield = veh->client->ps.stats[STAT_ARMOR];
	}
	if ( scn.spectatorMode != SPEC_NONE ) {
		const gclient_t *client = g_entities[scn.client[SCN_SPECTATOR]].client;

		scnStats.team[SCN_SPECTATOR] = client->sess.sessionTeam;
		VectorCopy( client->ps.origin, scnStats.teamOrigin[SCN_SPECTATOR] );
	}
}

// at the end of each 20 Hz instant of a counted repetition, a slot's team and, as a spectator, whom it follows
static void G_ScenarioSampleTeam( int n ) {
	const gclient_t *client = g_entities[scn.client[n]].client;
	const float moved = Distance( client->ps.origin, scnStats.teamOrigin[n] );
	int followed, r;

	VectorCopy( client->ps.origin, scnStats.teamOrigin[n] );
	if ( client->sess.sessionTeam != scnStats.team[n] ) {
		scnStats.teamChanges[n]++;
		scnStats.team[n] = client->sess.sessionTeam;
	}
	if ( client->sess.sessionTeam != TEAM_SPECTATOR ) {
		return;
	}
	if ( client->sess.spectatorState != SPECTATOR_FOLLOW ) {
		scnStats.freeSpectator[n]++;
		scnStats.freePath[n] += moved;
		return;
	}
	followed = client->sess.spectatorClient;	// -1 and -2 the game's follow1 and follow2 (SpectatorClientEndFrame)
	if ( followed == -1 ) {
		followed = level.follow1;
	}
	else if ( followed == -2 ) {
		followed = level.follow2;
	}
	for ( r = 0; r < scn.numClients; r++ ) {
		if ( followed == scn.client[r] ) {
			scnStats.following[n][r]++;
			scnStats.followGap[n] += Distance( client->ps.origin, level.clients[followed].ps.origin );
		}
	}
}

extern qboolean G_IsMindTricked( forcedata_t *fd, int client );

// at the end of each 20 Hz instant of a counted repetition
static void G_ScenarioSample( void ) {
	int n;

	for ( n = 0; n < scn.numClients; n++ ) {
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
		if ( ps->forceHandExtend == HANDEXTEND_KNOCKDOWN ) {
			scnStats.knockedDown[n]++;
		}
		if ( n && G_IsMindTricked( &g_entities[scn.client[0]].client->ps.fd, scn.client[n] ) ) {
			scnStats.mindTricked[n]++;
		}
		if ( ps->saberLockTime > level.time ) {
			scnStats.saberLocked[n]++;
		}
		scnStats.path[n] += Distance( ps->origin, scnStats.origin[n] );
		if ( ps->m_iVehicleNum ) { // the rider goes with its vehicle (G_AttachToVehicle)
			scnStats.aboardPath[n] += Distance( ps->origin, scnStats.origin[n] );
		}
		scnStats.health[n] = ent->health;
		scnStats.force[n] = ps->fd.forcePower;
		VectorCopy( ps->origin, scnStats.origin[n] );
		G_ScenarioSampleTeam( n );
		{ // plan step 20, the items
			const int armor = ps->stats[STAT_ARMOR] - scnStats.armor[n];
			const int ammo = G_ScenarioAmmo( ps ) - scnStats.ammo[n];

			if ( armor > 0 ) {
				scnStats.armorGained[n] += armor;
			}
			else {
				scnStats.armorLost[n] -= armor;
			}
			if ( ammo > 0 ) {
				scnStats.ammoGained[n] += ammo;
			}
			else {
				scnStats.ammoSpent[n] -= ammo;
			}
			scnStats.armor[n] = ps->stats[STAT_ARMOR];
			scnStats.ammo[n] += ammo;
			if ( ps->powerups[PW_CLOAKED] ) {
				scnStats.cloaked[n]++;
			}
			if ( ent->client->jetPackOn ) {
				scnStats.jetpack[n]++;
			}
			if ( ps->zoomMode ) {
				scnStats.zoomed[n]++;
			}
		}
		// plan step 20, the vehicles
		if ( ps->m_iVehicleNum && !scnStats.vehicleNum[n] ) {
			scnStats.boardings[n]++;
		}
		else if ( !ps->m_iVehicleNum && scnStats.vehicleNum[n] ) {
			scnStats.exits[n]++;
		}
		if ( ps->m_iVehicleNum ) {
			scnStats.aboard[n]++;
		}
		scnStats.vehicleNum[n] = ps->m_iVehicleNum;
	}
	if ( G_ScenarioVehicle() ) {
		const gentity_t *veh = G_ScenarioVehicle();
		const int health = veh->health, shield = veh->client->ps.stats[STAT_ARMOR];

		scnStats.vehicleHealthLost += Q_max( 0, scnStats.vehicleHealth - health );
		scnStats.vehicleShieldLost += Q_max( 0, scnStats.vehicleShield - shield );
		if ( health <= 0 && scnStats.vehicleHealth > 0 ) {
			scnStats.vehicleDestroyed++;
		}
		scnStats.vehicleHealth = health;
		scnStats.vehicleShield = shield;
	}
	if ( scn.spectatorMode != SPEC_NONE ) {
		G_ScenarioSampleTeam( SCN_SPECTATOR );
	}
}

// with the gamestats of a scenario: what each bot did, by role
void G_ScenarioPrintStats( void ) {
	static const char *roles[] = { "attacker", "defender", "ally", "spectator" };
	char line[MAX_STRING_CHARS] = "scenario statistics: health gained and lost, force gained and spent, instants off the ground, "
		"knocked down, mind tricked and in a saber lock, distance moved, end place against the start, by role:";
	int n, i;

	if ( scn.type == SCN_NONE ) {
		return;
	}
	for ( n = 0; n < scn.numClients; n++ ) {
		Q_strcat( line, sizeof( line ), va( "%s %s %d %d %d %d %d %d %d %d %.0f %d %d %d", n ? "," : "", roles[n],
			scnStats.healthGained[n], scnStats.healthLost[n], scnStats.forceGained[n], scnStats.forceSpent[n],
			scnStats.airborne[n], scnStats.knockedDown[n], scnStats.mindTricked[n], scnStats.saberLocked[n], scnStats.path[n],
			scnStats.end[n][0], scnStats.end[n][1], scnStats.end[n][2] ) );
	}
	trap->Print( "%s\n", line );
	// plan step 20, the game types: what the counted repetitions added to the scores
	Q_strncpyz( line, "score statistics: score and captures gained by role, red and blue team scores gained:", sizeof( line ) );
	for ( n = 0; n < scn.numClients; n++ ) {
		const playerState_t *ps = &g_entities[scn.client[n]].client->ps;

		Q_strcat( line, sizeof( line ), va( "%s %s %d %d", n ? "," : "", roles[n],
			ps->persistant[PERS_SCORE] - scnStats.scoreStart[n], ps->persistant[PERS_CAPTURES] - scnStats.capturesStart[n] ) );
	}
	Q_strcat( line, sizeof( line ), va( ", teams %d %d", level.teamScores[TEAM_RED] - scnStats.teamScoreStart[0],
		level.teamScores[TEAM_BLUE] - scnStats.teamScoreStart[1] ) );
	trap->Print( "%s\n", line );
	// plan step 20, teams and spectators
	Q_strncpyz( line, "team statistics: team changes, instants as a free spectator and distance moved, instants following "
		"the attacker, the defender and the ally, distance from the player followed, by role:", sizeof( line ) );
	for ( n = 0; n < SCN_SPECTATOR + 1; n++ ) {
		if ( n >= scn.numClients && ( n != SCN_SPECTATOR || scn.spectatorMode == SPEC_NONE ) ) {
			continue;
		}
		Q_strcat( line, sizeof( line ), va( "%s %s %d %d %.0f %d %d %d %.0f", n ? "," : "", roles[n], scnStats.teamChanges[n],
			scnStats.freeSpectator[n], scnStats.freePath[n], scnStats.following[n][0], scnStats.following[n][1],
			scnStats.following[n][2], scnStats.followGap[n] ) );
	}
	trap->Print( "%s\n", line );
	// plan step 20, the items
	Q_strncpyz( line, "item statistics: items picked up:", sizeof( line ) );
	for ( i = 1; i < bg_numItems; i++ ) {
		if ( g_stats.itemPickups[i] ) {
			Q_strcat( line, sizeof( line ), va( " %s %d", bg_itemlist[i].classname, g_stats.itemPickups[i] ) );
		}
	}
	Q_strcat( line, sizeof( line ), "; health taken from what is not a client, by cause:" );
	for ( i = 0; i < MOD_MAX; i++ ) {
		if ( g_stats.objectDamage[i] ) {
			Q_strcat( line, sizeof( line ), va( " %s %d", modNames[i], g_stats.objectDamage[i] ) );
		}
	}
	Q_strcat( line, sizeof( line ), "; armour gained and lost, ammunition gained and spent, instants cloaked, flying a jetpack "
		"and zoomed in, by role:" );
	for ( n = 0; n < scn.numClients; n++ ) {
		Q_strcat( line, sizeof( line ), va( "%s %s %d %d %d %d %d %d %d", n ? "," : "", roles[n], scnStats.armorGained[n],
			scnStats.armorLost[n], scnStats.ammoGained[n], scnStats.ammoSpent[n], scnStats.cloaked[n], scnStats.jetpack[n],
			scnStats.zoomed[n] ) );
	}
	trap->Print( "%s\n", line );
	// plan step 20, the vehicles
	Q_strncpyz( line, "vehicle statistics: health taken from the NPCs and vehicles, by cause:", sizeof( line ) );
	for ( i = 0; i < MOD_MAX; i++ ) {
		if ( g_stats.npcDamage[i] ) {
			Q_strcat( line, sizeof( line ), va( " %s %d", modNames[i], g_stats.npcDamage[i] ) );
		}
	}
	Q_strcat( line, sizeof( line ), va( "; the vehicle's health and shield lost, destructions: %d %d %d; boardings, exits, "
		"instants aboard, distance moved aboard, by role:", scnStats.vehicleHealthLost, scnStats.vehicleShieldLost,
		scnStats.vehicleDestroyed ) );
	for ( n = 0; n < scn.numClients; n++ ) {
		Q_strcat( line, sizeof( line ), va( "%s %s %d %d %d %.0f", n ? "," : "", roles[n], scnStats.boardings[n],
			scnStats.exits[n], scnStats.aboard[n], scnStats.aboardPath[n] ) );
	}
	trap->Print( "%s\n", line );
}

// an entity of a map's kind spawned by the scenario, with no keys but its place, angle, flags and count (the spawn
// functions would read the keys of the last entity the map loaded)
static gentity_t *G_ScenarioSpawn( const char *classname, const vec3_t origin, float yaw, int spawnflags, int count ) {
	gentity_t *ent = G_Spawn();

	level.numSpawnVars = 0;
	level.numSpawnVarChars = 0;
	ent->classname = (char *)classname;
	ent->spawnflags = spawnflags;
	ent->count = count;
	VectorCopy( origin, ent->s.origin );
	VectorCopy( origin, ent->s.pos.trBase );
	VectorCopy( origin, ent->r.currentOrigin );
	ent->s.angles[YAW] = yaw;
	if ( !G_CallSpawn( ent ) ) {
		G_FreeEntity( ent );
		return NULL;
	}
	return ent;
}

// the vehicle of a vehicle scenario in front of the attacker, facing as it does, spawned as a map's NPC_Vehicle without
// a targetname (SP_NPC_Vehicle, G_VehicleSpawn): no team, no AI
static gentity_t *G_ScenarioSpawnVehicle( void ) {
	const vec3_t fwd = { Q_cospif( scn.yaw / 180.0f ), Q_sinpif( scn.yaw / 180.0f ), 0 };
	gentity_t *spawner = G_Spawn();
	gentity_t *veh;

	level.numSpawnVars = 0;
	level.numSpawnVarChars = 0;
	spawner->classname = "NPC_Vehicle";
	spawner->NPC_type = scn.vehicleType;
	spawner->count = 1;
	VectorMA( scn.origin, scn.vehicleDist, fwd, spawner->s.origin );
	G_SetOrigin( spawner, spawner->s.origin );
	spawner->s.angles[YAW] = scn.yaw;
	veh = NPC_Spawn_Do( spawner );	// frees the spawner
	if ( !veh || !veh->NPC || !veh->m_pVehicle ) {
		return NULL;
	}
	veh->s.angles[YAW] = scn.yaw;
	veh->NPC->behaviorState = BS_CINEMATIC;
	// the box its first move gives it, from its .veh file (BG_VehicleAdjustBBoxForOrientation), at once: NPC_Begin waits
	// while a player touches the .npc file's box (NPC_SpotWouldTelefrag), larger for the AT-ST (80 units against 40)
	BG_VehicleAdjustBBoxForOrientation( veh->m_pVehicle, veh->r.currentOrigin, veh->r.mins, veh->r.maxs, veh->s.number,
		MASK_NPCSOLID, NULL );
	{ // dropped onto the floor from 128 units up with that box: a fighter's is wide, and the ground under it uneven (in
		// the ground, it cannot move)
		vec3_t top, bottom;
		trace_t tr;

		VectorCopy( veh->r.currentOrigin, top );
		VectorCopy( veh->r.currentOrigin, bottom );
		top[2] += 128;
		bottom[2] -= 128;
		trap->Trace( &tr, top, veh->r.mins, veh->r.maxs, bottom, veh->s.number, MASK_NPCSOLID, qfalse, 0, 0 );
		if ( !tr.startsolid && !tr.allsolid ) {
			G_SetOrigin( veh, tr.endpos );
			VectorCopy( tr.endpos, veh->s.origin );
			VectorCopy( tr.endpos, veh->client->ps.origin );
		}
	}
	trap->LinkEntity( (sharedEntity_t *)veh );
	return veh;
}

// an NPC removed with the goal entity every NPC spawns with (freed at an NPC's death, player_die)
static void G_ScenarioFreeNPC( gentity_t *npc ) {
	if ( npc->NPC && npc->NPC->tempGoal ) {
		G_FreeEntity( npc->NPC->tempGoal );
		npc->NPC->tempGoal = NULL;
	}
	G_FreeEntity( npc );
}

// the vehicle of a vehicle scenario removed, its riders off first, with the astromech a fighter spawns with (NPC_Begin),
// which would stand in the next vehicle's place and keep it from spawning (NPC_SpotWouldTelefrag)
static void G_ScenarioRemoveVehicle( void ) {
	gentity_t *veh = G_ScenarioVehicle();
	int i;

	scn.vehicle = NULL;
	if ( !veh ) {
		return;
	}
	for ( i = 0; i < scn.numClients; i++ ) {
		if ( g_entities[scn.client[i]].client && g_entities[scn.client[i]].client->ps.m_iVehicleNum == veh->s.number ) {
			G_ScenarioLeaveVehicle( &g_entities[scn.client[i]] );
		}
	}
	if ( veh->m_pVehicle->m_pDroidUnit ) {
		G_ScenarioFreeNPC( (gentity_t *)veh->m_pVehicle->m_pDroidUnit );
	}
	G_ScenarioFreeNPC( veh );
}

// at the attacker's place (G_ScenarioFindPlace), the vehicle's distance from its box, read from one spawned once: 40
// units of floor between the attacker's box and its own along the axis nearest the view (the boxes keep to the axes, so
// a slanted view needs more; a vehicle touching a player waits to spawn, NPC_SpotWouldTelefrag), the near side within
// the use key's 64 units from the eyes; then its box clear of the world 128 units up, whence it drops
// (G_ScenarioSpawnVehicle), and the defender beyond it
static qboolean G_ScenarioVehicleFits( void ) {
	const float c = Q_fabsf( Q_cospif( scn.yaw / 180.0f ) ), s = Q_fabsf( Q_sinpif( scn.yaw / 180.0f ) );
	const float axis = Q_max( c, s );
	const vec3_t fwd = { Q_cospif( scn.yaw / 180.0f ), Q_sinpif( scn.yaw / 180.0f ), 0 };
	vec3_t mins, maxs, top;
	float half;
	trace_t tr;

	scn.vehicleDist = 64;
	scn.vehicle = G_ScenarioSpawnVehicle();
	if ( !scn.vehicle ) {
		return qfalse;
	}
	VectorCopy( scn.vehicle->r.mins, mins );
	VectorCopy( scn.vehicle->r.maxs, maxs );
	G_ScenarioRemoveVehicle();
	half = ( c >= s ) ? maxs[0] : maxs[1];	// a fighter's box turns with it
	scn.vehicleDist = ( half + 15 + 25 ) / axis;
	if ( scn.vehicleDist + ( half + 15 ) / axis > scn.dist ) {
		return qfalse;
	}
	VectorMA( scn.origin, scn.vehicleDist, fwd, top );
	top[2] += 128;
	trap->Trace( &tr, top, mins, maxs, top, ENTITYNUM_NONE, MASK_SOLID, qfalse, 0, 0 );
	return ( !tr.startsolid && !tr.allsolid ) ? qtrue : qfalse;
}

// the emplaced gun's user taken off it as emplaced_gun_update does, and free to mount it again at once
static void G_ScenarioReleaseGun( void ) {
	gentity_t *gun = scn.spawned[0];
	gentity_t *user;

	if ( scn.type != SCN_WEAPON || scn.weapon != WP_EMPLACED_GUN || !gun || !gun->inuse
		|| !gun->activator || !gun->activator->client ) {
		return;
	}
	user = gun->activator;
	user->client->ps.stats[STAT_WEAPONS] &= ~( 1 << WP_EMPLACED_GUN );
	user->client->ps.weapon = gun->s.weapon;
	gun->s.weapon = WP_EMPLACED_GUN;
	user->r.ownerNum = ENTITYNUM_NONE;
	user->client->ps.emplacedTime = 0;
	user->client->ps.emplacedIndex = 0;
	user->client->ps.saberHolstered = 0;
	gun->activator = NULL;
	gun->s.activeForcePass = 0;
	gun->genericValue1 = 0;
}

// a flag's place: on the floor below the point
static void G_ScenarioFlagSpot( const gentity_t *flag, const vec3_t at, vec3_t spot ) {
	vec3_t below;
	trace_t tr;

	VectorCopy( at, below );
	below[2] -= 128;
	trap->Trace( &tr, at, flag->r.mins, flag->r.maxs, below, flag->s.number, MASK_SOLID, qfalse, 0, 0 );
	VectorCopy( tr.endpos, spot );
}

// the flag the carrier dropped in a flag's return: the defender's, the one the attacker took
static gentity_t *G_ScenarioDroppedFlag( void ) {
	gentity_t *flag = NULL;

	while ( ( flag = G_Find( flag, FOFS( classname ), scn.pickup->classname ) ) != NULL ) {
		if ( flag->flags & FL_DROPPED_ITEM ) {
			return flag;
		}
	}
	return NULL;
}

// at every reset: nobody the Jedi Master any more (its saber dropped as at its death without a killer, back to its spot,
// ThrowSaberToAttacker), and the holocron or the Jedi Master's saber of pickup= back in its place; the free play or the
// previous repetition left them taken (the holocrons carried are cleared by G_ScenarioPlace); in CTF and CTY the flags
// at their bases, the dropped ones freed (a carried one is cleared by G_ScenarioPlace), and the bases next to the
// scenario
static void G_ScenarioReturnPickup( void ) {
	gentity_t *p = scn.pickup;
	int i;

	if ( p && scn.itemClass[0] ) {
		// the item of item= or an item scenario back once taken (Touch_Item hides it, or leaves a weapon's or a powerup's
		// placeholder), as at the end of its respawn time; not before, which would cancel the end of its spawn
		// (FinishSpawningItem, its think)
		if ( p->s.eFlags & ( EF_NODRAW | EF_ITEMPLACEHOLDER ) ) {
			RespawnItem( p );
		}
		return;
	}
	if ( p && ( level.gametype == GT_CTF || level.gametype == GT_CTY ) ) {
		gentity_t *flags[2];

		flags[0] = p;
		flags[1] = scn.ownFlag;
		Team_ResetFlags();
		for ( i = 0; i < 2; i++ ) {
			G_SetOrigin( flags[i], scn.flagSpot[i] );
			VectorCopy( scn.flagSpot[i], flags[i]->s.origin );
			trap->LinkEntity( (sharedEntity_t *)flags[i] );
		}
		return;
	}
	if ( level.gametype == GT_JEDIMASTER ) {
		for ( i = 0; i < scn.numClients; i++ ) {
			gentity_t *ent = &g_entities[scn.client[i]];

			if ( ent->client->ps.isJediMaster ) {
				ThrowSaberToAttacker( ent, NULL );
				ent->client->ps.isJediMaster = qfalse;
				ent->client->ps.saberInFlight = qfalse;	// set by ThrowSaberToAttacker for the dead body
			}
		}
	}
	if ( !p || !p->inuse ) {
		return;
	}
	p->enemy = NULL;
	p->pos2[0] = 0;
	VectorCopy( p->s.origin2, p->s.pos.trBase );
	VectorCopy( p->s.origin2, p->s.origin );
	VectorCopy( p->s.origin2, p->r.currentOrigin );
	VectorClear( p->s.pos.trDelta );
	p->s.pos.trTime = level.time;
	if ( level.gametype == GT_HOLOCRON ) {
		HolocronRespawn( p );
	}
	trap->LinkEntity( (sharedEntity_t *)p );
}

// at the scenario's end: what it spawned removed
static void G_ScenarioFreeSpawned( void ) {
	int i;

	G_ScenarioReleaseGun();
	G_ScenarioRemoveVehicle();
	for ( i = 0; i < 2; i++ ) {
		if ( scn.spawned[i] && scn.spawned[i]->inuse ) {
			G_FreeEntity( scn.spawned[i] );
		}
		scn.spawned[i] = NULL;
	}
	scn.pickup = NULL;
}

// called at the start of every server frame: resets the bots at the start of each repetition
void G_ScenarioFrame( void ) {
	vec3_t fwd, front;
	int seed, i;

	if ( scn.type == SCN_NONE || scn.done ) {
		return;
	}
	if ( scn.repStart && level.time < scn.repStart + scn.repMsec ) {
		return;
	}
	if ( scn.repStart && scn.rep >= 1 ) {
		G_ScenarioEndRepetition();
	}
	if ( scn.rep >= scn.reps ) {
		trap->Print( "scenario done: %d repetitions\n", scn.rep );
		Svcmd_GameStats_f();
		G_ScenarioFreeSpawned();
		if ( scn.spectatorMode != SPEC_NONE ) { // still a bot for the next scenario (StopFollowing)
			g_entities[scn.client[SCN_SPECTATOR]].r.svFlags |= SVF_BOT;
		}
		scn.done = qtrue;
		return;
	}
	for ( i = 0; i < SCN_SPECTATOR + 1; i++ ) {
		if ( ( i < scn.numClients || ( i == SCN_SPECTATOR && scn.spectatorMode != SPEC_NONE ) )
			&& !g_entities[scn.client[i]].inuse ) {
			trap->Print( "scenario stopped: a bot left\n" );
			scn.type = SCN_NONE;
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
	G_ScenarioReleaseGun();
	G_ScenarioReturnPickup();
	if ( scn.type == SCN_OBJECTIVE ) { // not complete, completed again in the repetition
		G_SiegeSetObjectiveComplete( scn.objective->side, scn.objective->objective, qtrue );
	}
	scn.actionDone = qfalse;
	for ( i = 0; i < scn.numClients; i++ ) {
		G_ScenarioRestoreTeam( i );
	}
	G_ScenarioPlace( &g_entities[scn.client[0]], scn.origin, G_ScenarioAttackerYaw() );
	G_ScenarioPlace( &g_entities[scn.client[1]], front, G_ScenarioDefenderYaw() );
	G_ScenarioEquip( &g_entities[scn.client[0]], qtrue );
	G_ScenarioEquip( &g_entities[scn.client[1]], qfalse );
	if ( scn.type == SCN_SABER && scn.saberMode == SM_LOCK && level.gametype != GT_DUEL ) {
		// saber locks happen only in a duel (WP_SabersCheckLock): a private one, already started, but in the duel game
		// type, where every clash may lock
		for ( i = 0; i < 2; i++ ) {
			gclient_t *client = g_entities[scn.client[i]].client;

			client->ps.duelInProgress = qtrue;
			client->ps.duelIndex = scn.client[1 - i];
			client->ps.duelTime = 0;
		}
	}
	if ( scn.numClients == 3 ) {
		vec3_t ally;

		G_ScenarioAllyPlace( scn.origin, scn.yaw, ally );
		G_ScenarioPlace( &g_entities[scn.client[2]], ally, scn.yaw );
		G_ScenarioEquip( &g_entities[scn.client[2]], qfalse );
	}
	if ( scn.spectatorMode != SPEC_NONE ) {
		G_ScenarioPlaceSpectator();
	}
	if ( scn.type == SCN_TURRET ) { // the turret's target is the defender
		g_entities[scn.client[0]].flags |= FL_NOTARGET;
	}
	if ( scn.type == SCN_VEHICLE ) { // a new vehicle, the bots off the last one (G_ScenarioPlace)
		G_ScenarioRemoveVehicle();
		scn.vehicle = G_ScenarioSpawnVehicle();
		if ( scn.vehicle && scn.vehicleMode == VM_DESTROY && scn.vehicle->health > 100 ) {
			// what one rocket destroys (the AT-ST's 1,500); NPC_Begin keeps it, as a health set in a map
			scn.vehicle->health = scn.vehicle->client->ps.stats[STAT_HEALTH] = scn.vehicle->m_pVehicle->m_iArmor = 100;
		}
	}
	if ( scn.type == SCN_ITEM || ( scn.type == SCN_HOLDABLE && ( scn.holdable == HI_MEDPAC || scn.holdable == HI_MEDPAC_BIG ) ) ) {
		// what a health item or a medpac gives back
		gentity_t *attacker = &g_entities[scn.client[0]];

		attacker->health = attacker->client->ps.stats[STAT_HEALTH] = 40;
	}
	if ( scn.type == SCN_HOLDABLE && scn.holdable == HI_HEALTHDISP ) {
		// what the health dispenser gives the teammate in front (target=ally)
		gentity_t *teammate = &g_entities[scn.client[1]];

		teammate->health = teammate->client->ps.stats[STAT_HEALTH] = 40;
	}
	if ( scn.type == SCN_FORCE ) { // what the healing and replenishing powers act on
		gentity_t *attacker = &g_entities[scn.client[0]];
		gentity_t *ally = &g_entities[scn.client[scn.numClients - 1]];

		if ( scn.power == FP_HEAL ) {
			attacker->health = attacker->client->ps.stats[STAT_HEALTH] = 40;
		}
		if ( scn.numClients == 3 && scn.power == FP_TEAM_HEAL ) {
			ally->health = ally->client->ps.stats[STAT_HEALTH] = 40;
		}
		if ( scn.numClients == 3 && scn.power == FP_TEAM_FORCE ) {
			ally->client->ps.fd.forcePower = 20;
		}
	}
	// the animations restart from the weapon's ready pose: a still bot keeps its torso and legs animations (the choke of
	// a grip still held, the slope stance of the legs), so the warm-up's, shaped by the free play, carried into every
	// repetition; the ghoul2 animations restart with them (G_UpdateClientAnims)
	for ( i = 0; i < scn.numClients; i++ ) {
		gclient_t *client = g_entities[scn.client[i]].client;

		client->ps.torsoAnim = client->ps.legsAnim = WeaponReadyAnim[client->ps.weapon];
		client->torsoAnimExecute = client->legsAnimExecute = -1;
	}

	scn.repStart = level.time;
	scn.rep++;
	if ( scn.rep == 1 ) {
		memset( &g_stats, 0, sizeof( g_stats ) );	// the counted repetitions start
		memset( &scnStats, 0, sizeof( scnStats ) );
		for ( i = 0; i < scn.numClients; i++ ) {
			const playerState_t *ps = &g_entities[scn.client[i]].client->ps;

			scnStats.scoreStart[i] = ps->persistant[PERS_SCORE];
			scnStats.capturesStart[i] = ps->persistant[PERS_CAPTURES];
		}
		scnStats.teamScoreStart[0] = level.teamScores[TEAM_RED];
		scnStats.teamScoreStart[1] = level.teamScores[TEAM_BLUE];
		// without the corpses left by the free play or the warm-up, the body queue from its first slot (as BodySink
		// leaves a body at its end)
		for ( i = 0; i < BODY_QUEUE_SIZE; i++ ) {
			trap->UnlinkEntity( (sharedEntity_t *)level.bodyQue[i] );
			level.bodyQue[i]->physicsObject = qfalse;
			level.bodyQue[i]->takedamage = qfalse;
			level.bodyQue[i]->nextthink = 0;
		}
		level.bodyQueIndex = 0;
		// the events, missiles (in flight or laid, as mines) and dropped items left by the free play or the warm-up are
		// freed and every free slot can be taken at once: G_Spawn skips a slot freed less than a second ago, so the free
		// play's last frees gave the missiles other entity numbers, and the missiles run in the order of their numbers
		for ( i = MAX_CLIENTS; i < level.num_entities; i++ ) {
			gentity_t *e = &g_entities[i];

			if ( e->inuse && !e->neverFree &&
				( e->freeAfterEvent || e->s.eType == ET_MISSILE || ( e->flags & FL_DROPPED_ITEM ) ) ) {
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

	if ( scn.type == SCN_NONE || scn.done || scn.rep < 1 ) {
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
			scn.rep, level.time - scn.repStart, "ADT"[n], ps->commandTime - level.time, ent->health, ps->fd.forcePower, ps->saberMove,
			ps->saberBlocked, ps->weaponTime, ps->torsoAnim, ps->torsoTimer, ps->legsAnim, ps->legsTimer,
			ps->origin[0], ps->origin[1], ps->origin[2], blade->muzzlePoint[0], blade->muzzlePoint[1], blade->muzzlePoint[2] );
	}
}

// every health loss of a client during the traced repetitions: the tick, the inflictor's position and the point hit
void G_ScenarioDamageTrace( gentity_t *targ, gentity_t *inflictor, const vec3_t point, int take, int mod ) {
	if ( scn.type == SCN_NONE || scn.done || scn.rep < 1 || scn.rep > scn.traceReps ) {
		return;
	}
	trap->Print( "SCN %d %4d dmg tick%+d targ%d take%d mod%d surf%d@%+d inf%d p%.1f,%.1f,%.1f io%.1f,%.1f,%.1f\n", scn.rep,
		level.time - scn.repStart, level.tickTime - level.time, targ->s.number, take, mod, targ->client->g2LastSurfaceHit,
		targ->client->g2LastSurfaceTime - level.time, inflictor ? inflictor->s.number : -1,
		point ? point[0] : 0.0f, point ? point[1] : 0.0f, point ? point[2] : 0.0f,
		inflictor ? inflictor->r.currentOrigin[0] : 0.0f, inflictor ? inflictor->r.currentOrigin[1] : 0.0f,
		inflictor ? inflictor->r.currentOrigin[2] : 0.0f );
}

// the view from a bot's eyes towards a point
static void G_ScenarioLookAt( const gentity_t *ent, const vec3_t target, vec3_t view ) {
	vec3_t eyes, to;

	VectorCopy( ent->client->ps.origin, eyes );
	eyes[2] += ent->client->ps.viewheight;
	VectorSubtract( target, eyes, to );
	vectoangles( to, view );
}

// the view that aims a bot's weapon at a point from its muzzle, beside and below the eyes (CalcMuzzlePoint)
static void G_ScenarioAimMuzzle( gentity_t *ent, const vec3_t target, vec3_t view ) {
	vec3_t fwd, right, up, muzzle, to;

	G_ScenarioLookAt( ent, target, view );	// from the eyes
	AngleVectors( view, fwd, right, up );
	CalcMuzzlePoint( ent, fwd, right, up, muzzle );
	VectorSubtract( target, muzzle, to );
	vectoangles( to, view );	// from the muzzle
}

// a saber scenario's buttons and moves, from the start of the repetition's script (phase, in ms)
static void G_ScenarioSaberCommand( gentity_t *ent, qboolean attacker, int phase, usercmd_t *ucmd ) {
	static const signed char kickDirs[4][2] = { { 127, 0 }, { -127, 0 }, { 0, 127 }, { 0, -127 } };
	static const signed char lockDirs[3][2] = { { 127, 0 }, { 127, -127 }, { 127, 127 } };
	const int turn = scn.rep + 7;	// repetition 1 in the first direction
	const signed char side = ( scn.rep & 1 ) ? 127 : -127;	// right, then left
	const signed char *dir;

	if ( scn.saberMode == SM_LOCK && ent->client->ps.saberLockTime > level.time ) {
		// the attacker pushes the lock: the game reads the buttons once per 20 Hz frame, at the frame's first command and
		// before that command sets them (g_active.c), so it counts a press made by the command before, the bot's command
		// at the instant's time: pressed from there, 50 ms every 100 ms
		if ( attacker && phase % 100 < 50 ) {
			ucmd->buttons = BUTTON_ATTACK;
		}
		return;
	}
	if ( scn.saberMode == SM_STABDOWN && !attacker && phase < 50 && ent->client->ps.forceHandExtend != HANDEXTEND_KNOCKDOWN ) {
		G_Knockdown( ent );
	}
	if ( !attacker && scn.saberMode != SM_ATTACK && scn.saberMode != SM_LOCK ) {
		return;	// the defender idle
	}

	switch ( scn.saberMode ) {
	case SM_IDLE:
	case SM_ATTACK:
		if ( phase < 100 ) {
			dir = scnDirs[turn % 8];
			ucmd->buttons = BUTTON_ATTACK;
			ucmd->forwardmove = dir[0];
			ucmd->rightmove = attacker ? dir[1] : -dir[1];
		}
		break;
	case SM_LOCK:
		// both the same attack, top-down, then each diagonal in turn, the pairs a lock starts from (WP_SabersCheckLock),
		// held 600 ms: a clash locks only a player who is pressing something (G_ClientIdleInWorld)
		if ( phase < 600 ) {
			ucmd->buttons = BUTTON_ATTACK;
			ucmd->forwardmove = lockDirs[turn % 3][0];
			ucmd->rightmove = lockDirs[turn % 3][1];
		}
		break;
	case SM_THROW:
		if ( phase < 1000 ) ucmd->buttons = BUTTON_ALT_ATTACK;
		break;
	case SM_KATA:
		if ( phase < 100 ) ucmd->buttons = BUTTON_ATTACK | BUTTON_ALT_ATTACK;
		break;
	case SM_BACK:
	case SM_BACKCROUCH:	// ducked first for the crouched back attack
		if ( scn.saberMode == SM_BACKCROUCH && phase < 300 ) ucmd->upmove = -127;
		if ( phase >= 50 && phase < 150 ) {
			ucmd->buttons = BUTTON_ATTACK;
			ucmd->forwardmove = -127;
		}
		break;
	case SM_JUMP:	// the jump held, the attack once in the air (the flip and the strong style's need rising)
		if ( phase < 120 ) ucmd->upmove = 127;
		if ( phase >= 20 && phase < 120 ) {
			ucmd->buttons = BUTTON_ATTACK;
			ucmd->forwardmove = 127;
		}
		break;
	case SM_LUNGE:	// ducked first
		if ( phase < 300 ) ucmd->upmove = -127;
		if ( phase >= 50 && phase < 150 ) {
			ucmd->buttons = BUTTON_ATTACK;
			ucmd->forwardmove = 127;
		}
		break;
	case SM_CARTWHEEL:	// the jump held, then the side and the attack once rising (a jump begun to the side
						// is a side flip, during which no attack starts)
		if ( phase < 120 ) ucmd->upmove = 127;
		if ( phase >= 20 && phase < 120 ) {
			ucmd->buttons = BUTTON_ATTACK;
			ucmd->rightmove = side;
		}
		break;
	case SM_BACKFLIP:
		if ( phase < 100 ) {
			ucmd->buttons = BUTTON_ATTACK;
			ucmd->forwardmove = -127;
			ucmd->upmove = 127;
		}
		break;
	case SM_KICK:
		if ( phase < 100 ) {
			ucmd->buttons = BUTTON_ALT_ATTACK;
			ucmd->forwardmove = kickDirs[turn % 4][0];
			ucmd->rightmove = kickDirs[turn % 4][1];
		}
		break;
	case SM_AIRKICK:	// a force jump held, then the kick high enough above the floor (more than 64 units)
		if ( phase < 400 ) ucmd->upmove = 127;
		if ( phase >= 300 && phase < 400 ) {
			ucmd->buttons = BUTTON_ALT_ATTACK;
			ucmd->forwardmove = kickDirs[turn % 4][0];
			ucmd->rightmove = kickDirs[turn % 4][1];
		}
		break;
	case SM_ROLLSTAB:	// running at full speed, ducked into a roll, the attack held to the roll's end
		if ( phase < 800 ) ucmd->forwardmove = 127;
		if ( phase >= 600 && phase < 800 ) ucmd->upmove = -127;
		if ( phase >= 650 && phase < 1600 ) ucmd->buttons = BUTTON_ATTACK;
		break;
	case SM_STABDOWN:	// once the defender is down
		if ( phase >= 150 && phase < 250 ) {
			ucmd->buttons = BUTTON_ATTACK;
			ucmd->forwardmove = 127;
		}
		break;
	default:
		break;
	}
}

// fills a bot's command for the time t; false when the bot is not in the scenario
qboolean G_ScenarioCommand( int clientNum, int t, usercmd_t *ucmd ) {
	gentity_t *ent = &g_entities[clientNum];
	const qboolean attacker = ( clientNum == scn.client[0] ) ? qtrue : qfalse;
	const qboolean spectator = ( scn.spectatorMode != SPEC_NONE && clientNum == scn.client[SCN_SPECTATOR] ) ? qtrue : qfalse;
	int phase = t - scn.repStart - SCN_SETTLE_MSEC;
	vec3_t view = { 0, 0, 0 };
	int i;

	if ( !G_ScenarioControls( clientNum ) || !ent->client ) {
		return qfalse;
	}

	memset( ucmd, 0, sizeof( *ucmd ) );
	ucmd->serverTime = t;
	ucmd->weapon = ent->client->ps.weapon;
	// a free spectator looks towards the defender (a following one sees what its player sees)
	view[YAW] = attacker ? G_ScenarioAttackerYaw() : spectator ? scn.yaw : G_ScenarioDefenderYaw();
	if ( !attacker && !spectator && scn.type == SCN_FLAG && scn.flagMode == FM_RETURN ) {
		const gentity_t *flag = G_ScenarioDroppedFlag();
		vec3_t to;

		if ( flag ) { // the defender turns to the flag the carrier dropped
			VectorSubtract( flag->r.currentOrigin, ent->client->ps.origin, to );
			view[YAW] = vectoyaw( to );
		}
		else { // it aims the rocket launcher at the carrier's chest, which a rocket kills: the legs and the arms take
			// less (G_LocationBasedDamageModifier), and a carrier hurt is thrown back onto its own flag
			vec3_t target;

			VectorCopy( g_entities[scn.client[0]].client->ps.origin, target );
			target[2] += 16;
			G_ScenarioAimMuzzle( ent, target, view );
		}
	}
	if ( scn.type == SCN_VEHICLE && scn.repStart && phase >= 0 && !spectator ) {
		const gentity_t *veh = G_ScenarioVehicle();
		const gentity_t *defender = &g_entities[scn.client[1]];

		if ( attacker && veh && phase < 50 ) { // the use key's trace, from the eyes, onto the vehicle's centre
			G_ScenarioLookAt( ent, veh->r.currentOrigin, view );
		}
		else if ( attacker && veh && scn.vehicleMode == VM_RIDE && veh->m_pVehicle->m_pVehicleInfo->type != VH_FIGHTER ) {
			// the defender's chest, from the rider's place on the vehicle (a fighter flies level)
			vec3_t target;

			VectorCopy( defender->client->ps.origin, target );
			target[2] += 16;
			G_ScenarioLookAt( ent, target, view );
		}
		else if ( !attacker && veh && scn.vehicleMode == VM_DESTROY ) {
			// the rocket at the vehicle's body, three quarters up its box (the missiles hit the model: halfway up, between
			// the AT-ST's legs)
			vec3_t target;

			VectorCopy( veh->r.currentOrigin, target );
			target[2] += ( veh->r.mins[2] + 3 * veh->r.maxs[2] ) / 4;
			G_ScenarioAimMuzzle( ent, target, view );
		}
	}
	if ( attacker && scn.type == SCN_WEAPON && scn.weapon == WP_EMPLACED_GUN && scn.repStart && phase >= 0 ) {
		// the use key's trace leaves the eyes level with the top of the gun's box: looking down at it; then 2 degrees up,
		// since the two barrels fire 10 units left and right of the view, beside the model of a defender aimed at
		// (2 up, 15 of 41 bolts hit; 4 up, 10; level, 6 up and 2, 3 or 5 down, none)
		view[PITCH] = ( phase < 50 ) ? 20.0f : -2.0f;
	}
	for ( i = 0; i < 3; i++ ) {
		ucmd->angles[i] = ANGLE2SHORT( view[i] ) - ent->client->ps.delta_angles[i];
	}
	if ( !scn.repStart || phase < 0 || scn.done ) {
		return qtrue;	// still
	}
	if ( spectator ) { // the spectator acts only in a spectate scenario
		if ( scn.type == SCN_SPECTATE && scn.spectateMode == SPM_CYCLE ) {
			// the next player, the previous one, then a jump, which stops following (SpectatorThink)
			if ( phase < 50 ) {
				ucmd->buttons = BUTTON_ATTACK;
			}
			else if ( phase >= 500 && phase < 550 ) {
				ucmd->buttons = BUTTON_ALT_ATTACK;
			}
			else if ( phase >= 1000 && phase < 1050 ) {
				ucmd->upmove = 127;
			}
		}
		else if ( scn.type == SCN_SPECTATE && phase < 1000 ) { // flies forward
			ucmd->forwardmove = 127;
		}
		return qtrue;
	}
	if ( scn.pickupDist ) { // the attacker walks onto the holocron or the Jedi Master's saber, then the script starts
		if ( attacker && phase < SCN_PICKUP_WALK_MSEC ) {
			ucmd->forwardmove = 127;
		}
		phase -= SCN_PICKUP_MSEC;
		if ( phase < 0 ) {
			return qtrue;
		}
	}

	switch ( scn.type ) {
	case SCN_SABER:
		G_ScenarioSaberCommand( ent, attacker, phase, ucmd );
		break;
	case SCN_FORCE:
		if ( !attacker ) {
			break;
		}
		switch ( scn.power ) {
		case FP_LIGHTNING:
			if ( phase < 1000 ) ucmd->buttons = BUTTON_FORCE_LIGHTNING;
			break;
		case FP_GRIP:
			if ( phase < 1500 ) ucmd->buttons = BUTTON_FORCEGRIP;
			break;
		case FP_DRAIN:
			if ( phase < 1000 ) ucmd->buttons = BUTTON_FORCE_DRAIN;
			break;
		case FP_HEAL:
		case FP_PUSH:
		case FP_PULL:
		case FP_TELEPATHY:
		case FP_TEAM_HEAL:
		case FP_TEAM_FORCE:	// instant powers: used three times, the button released in between
			ucmd->forcesel = scn.power;
			ent->client->ps.fd.forcePowerSelected = scn.power;
			if ( phase < 1250 && phase % 600 < 50 ) ucmd->buttons = BUTTON_FORCEPOWER;
			break;
		case FP_LEVITATION:	// a force jump to the right, the jump held
			if ( phase < 1000 ) {
				ucmd->upmove = 127;
				ucmd->rightmove = 127;
			}
			break;
		case FP_SPEED:	// switched on, then a run to the right
			ucmd->forcesel = scn.power;
			ent->client->ps.fd.forcePowerSelected = scn.power;
			if ( phase < 50 ) ucmd->buttons = BUTTON_FORCEPOWER;
			if ( phase < 1000 ) ucmd->rightmove = 127;
			break;
		case FP_SABERTHROW:	// the saber thrown and held out
			if ( phase < 1000 ) ucmd->buttons = BUTTON_ALT_ATTACK;
			break;
		default:	// powers used on oneself: switched on once, held until the next reset
			ucmd->forcesel = scn.power;
			ent->client->ps.fd.forcePowerSelected = scn.power;
			if ( phase < 50 ) ucmd->buttons = BUTTON_FORCEPOWER;
			break;
		}
		break;
	case SCN_WEAPON: {
		const int buttons = ( scn.fire == 0 ) ? BUTTON_ATTACK : ( scn.fire == 1 ) ? BUTTON_ALT_ATTACK : BUTTON_ATTACK | BUTTON_ALT_ATTACK;

		if ( !attacker ) {
			break;
		}
		if ( scn.weapon == WP_DISRUPTOR && scn.fire == 1 ) { // zoomed in, the zoom locked on release, the shot charged
			if ( phase < 300 ) {
				ucmd->buttons = BUTTON_ALT_ATTACK;
			}
			else if ( phase >= 400 && phase < 400 + scn.hold ) {
				ucmd->buttons = BUTTON_ATTACK;
			}
		}
		else if ( scn.weapon == WP_DET_PACK && scn.fire == 1 ) { // a pack thrown, then set off
			if ( phase < 50 ) {
				ucmd->buttons = BUTTON_ATTACK;
			}
			else if ( phase >= 800 && phase < 850 ) {
				ucmd->buttons = BUTTON_ALT_ATTACK;
			}
		}
		else if ( scn.weapon == WP_EMPLACED_GUN ) { // the gun mounted, then fired
			if ( phase < 50 ) {
				ucmd->buttons = BUTTON_USE;
			}
			else if ( phase >= 200 && phase < 200 + scn.hold ) {
				ucmd->buttons = buttons;
			}
		}
		else if ( phase < scn.hold ) {
			ucmd->buttons = buttons;
			if ( scn.weapon == WP_MELEE && scn.fire == 1 && phase < 100 ) {
				// with g_debugMelee the alternate attack kicks in the direction moved: the eight directions in turn
				const signed char *dir = scnDirs[( scn.rep + 7 ) % 8];

				ucmd->forwardmove = dir[0];
				ucmd->rightmove = dir[1];
			}
			else if ( scn.weapon == WP_MELEE && scn.fire == 2 ) {
				// with g_debugMelee both buttons grab, and the grab punches moving forward, throws moving back
				ucmd->forwardmove = ( scn.rep & 1 ) ? 127 : -127;
			}
		}
		break;
	}
	case SCN_FLAG:
		if ( scn.flagMode == FM_CAPTURE ) { // the enemy flag taken, back onto its own
			if ( attacker && phase < 800 ) {
				ucmd->forwardmove = -127;
			}
		}
		else if ( scn.flagMode == FM_RETURN && !attacker ) {
			// the carrier shot, then a run onto the flag dropped once it has landed
			if ( phase < 50 ) {
				ucmd->buttons = BUTTON_ATTACK;
			}
			else if ( phase >= 700 && G_ScenarioDroppedFlag() ) {
				ucmd->forwardmove = 127;
			}
		}
		else if ( scn.flagMode == FM_SWITCH && attacker && !scn.actionDone ) {
			// the carrier to the other team, as its team command does: its death sends the flag back (player_die)
			scn.actionDone = qtrue;
			SetTeam( ent, ( ent->client->sess.sessionTeam == TEAM_RED ) ? "blue" : "red" );
		}
		break;
	case SCN_OBJECTIVE:
		if ( attacker && !scn.actionDone ) { // used by the attacker, as when a player completes it
			scn.actionDone = qtrue;
			scn.objective->use( scn.objective, ent, ent );
		}
		break;
	case SCN_TEAM:
		if ( clientNum == scn.client[1] && !scn.actionDone ) { // the defender to the other team, as its team command does
			scn.actionDone = qtrue;
			SetTeam( ent, ( ent->client->sess.sessionTeam == TEAM_RED ) ? "blue" : "red" );
		}
		break;
	case SCN_HOLDABLE:
		if ( !attacker ) {
			break;
		}
		if ( scn.holdable == HI_JETPACK ) {
			// a jump, the use key in the air, which switches the jetpack on (TryUse), then the jump held 1 s
			if ( phase < 50 || ( phase >= 200 && phase < 1200 ) ) {
				ucmd->upmove = 127;
			}
			else if ( phase >= 100 && phase < 150 ) {
				ucmd->buttons = BUTTON_USE;
			}
		}
		else if ( scn.holdable == HI_HEALTHDISP || scn.holdable == HI_AMMODISP ) {
			// the use key held 1 s, on the teammate in front, or else on nothing: the ammunition thrown (TryUse)
			if ( phase < 1000 ) {
				ucmd->buttons = BUTTON_USE;
			}
		}
		else if ( phase < 50 ) { // the item key, the item selected (PM_Weapon)
			ucmd->buttons = BUTTON_USE_HOLDABLE;
		}
		if ( ( scn.holdable == HI_SHIELD || scn.holdable == HI_EWEB ) && phase >= 500 && phase < 1500 ) {
			ucmd->buttons = BUTTON_ATTACK;	// the shield up after 500 ms (PlaceShield)
		}
		break;
	case SCN_VEHICLE: {
		const gentity_t *veh = G_ScenarioVehicle();
		const int kind = veh ? veh->m_pVehicle->m_pVehicleInfo->type : VH_NONE;

		if ( attacker && phase < 50 ) { // boards (TryUse); held longer, the next use 100 ms later would get it off
			ucmd->buttons = BUTTON_USE;
		}
		else if ( scn.vehicleMode == VM_DESTROY ) { // a rocket once the attacker is aboard
			if ( !attacker && phase >= 1000 && phase < 1050 ) {
				ucmd->buttons = BUTTON_ATTACK;
			}
		}
		else if ( attacker && kind == VH_SPEEDER ) { // fires, drives at the defender, jumps off
			if ( phase >= 1000 && phase < 2000 ) {
				ucmd->buttons = BUTTON_ATTACK;
			}
			else if ( phase >= 2000 && phase < 3000 ) {
				ucmd->forwardmove = 127;
			}
			else if ( phase >= 3000 && phase < 3050 ) {
				ucmd->upmove = 127;
			}
		}
		else if ( attacker && kind == VH_WALKER ) { // the laser, a rocket, walks, gets off (1.5 s aboard first)
			if ( phase >= 1600 && phase < 2600 ) {
				ucmd->buttons = BUTTON_ATTACK;
			}
			else if ( phase >= 2600 && phase < 2650 ) {
				ucmd->buttons = BUTTON_ALT_ATTACK;
			}
			else if ( phase >= 2700 && phase < 3700 ) {
				ucmd->forwardmove = 127;
			}
			else if ( phase >= 3800 && phase < 3850 ) {
				ucmd->buttons = BUTTON_USE;
			}
		}
		else if ( attacker && kind == VH_FIGHTER ) { // takes off, flies forward, fires (1.5 s aboard first)
			if ( phase >= 1600 && phase < 3100 ) {
				ucmd->upmove = 127;
			}
			else if ( phase >= 3100 && phase < 3700 ) {
				ucmd->forwardmove = 127;
			}
			if ( phase >= 3500 && phase < 4500 ) {
				ucmd->buttons = BUTTON_ATTACK;
			}
		}
		break;
	}
	default:
		break;
	}
	return qtrue;
}

static int G_ScenarioArgInt( int n, int def ) {
	char arg[MAX_TOKEN_CHARS];

	if ( trap->Argc() <= n ) {
		return def;
	}
	trap->Argv( n, arg, sizeof( arg ) );
	return ( strchr( arg, '=' ) || !( arg[0] == '-' || ( arg[0] >= '0' && arg[0] <= '9' ) ) ) ? def : atoi( arg );
}

// an option name=value among the arguments, or def
static int G_ScenarioOpt( const char *name, int def ) {
	char arg[MAX_TOKEN_CHARS];
	const int len = (int)strlen( name );
	int n;

	for ( n = 2; n < trap->Argc(); n++ ) {
		trap->Argv( n, arg, sizeof( arg ) );
		if ( !Q_strncmp( arg, name, len ) && arg[len] == '=' ) {
			return atoi( arg + len + 1 );
		}
	}
	return def;
}

// an option given as a word among the arguments
static qboolean G_ScenarioOptIs( const char *text ) {
	char arg[MAX_TOKEN_CHARS];
	int n;

	for ( n = 2; n < trap->Argc(); n++ ) {
		trap->Argv( n, arg, sizeof( arg ) );
		if ( !Q_stricmp( arg, text ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

// an option name=text among the arguments, its text copied, or "" without it
static void G_ScenarioOptString( const char *name, char *out, int size ) {
	char arg[MAX_TOKEN_CHARS];
	const size_t len = strlen( name );
	int n;

	out[0] = '\0';
	for ( n = 2; n < trap->Argc(); n++ ) {
		trap->Argv( n, arg, sizeof( arg ) );
		if ( !Q_strncmp( arg, name, len ) && arg[len] == '=' ) {
			Q_strncpyz( out, arg + len + 1, size );
		}
	}
}

void Svcmd_Scenario_f( void ) {
	char type[MAX_TOKEN_CHARS], arg[MAX_TOKEN_CHARS];
	int i, n = 0;
	int specBot = -1, spare = -1;	// the first bot spectator, the first bot without a role

	trap->Argv( 1, type, sizeof( type ) );
	if ( !Q_stricmp( type, "off" ) || !type[0] ) {
		G_ScenarioFreeSpawned();
		scn.type = SCN_NONE;
		return;
	}
	if ( !trap->Cvar_VariableIntegerValue( "sv_cheats" ) ) {
		trap->Print( "scenario: needs sv_cheats 1 (devmap)\n" );
		return;
	}

	G_ScenarioFreeSpawned();	// a scenario stopped before its end
	memset( &scn, 0, sizeof( scn ) );
	scn.rep = -1;	// the first reset starts the warm-up, repetition 0
	scn.repMsec = Q_max( 1000, G_ScenarioOpt( "rep", SCN_REP_MSEC ) );
	scn.hold = G_ScenarioOpt( "hold", 1000 );
	{ // the attacker, the first bot that is not its teammate as the defender, and a teammate as the ally
		qboolean defender = qfalse, ally = qfalse;

		for ( i = 0; i < MAX_CLIENTS; i++ ) {
			gentity_t *e = &g_entities[i];

			if ( !e->inuse || !e->client || !( e->r.svFlags & SVF_BOT ) || e->client->pers.connected != CON_CONNECTED ) {
				continue;
			}
			if ( e->client->sess.sessionTeam == TEAM_SPECTATOR ) {	// the queue of a duel, or a spectator
				if ( specBot < 0 ) {
					specBot = i;
				}
				continue;
			}
			if ( !n ) {
				scn.client[0] = i;
				n = 1;
			}
			else if ( !defender && !OnSameTeam( &g_entities[scn.client[0]], e ) ) {
				scn.client[1] = i;
				defender = qtrue;
			}
			else if ( !ally && OnSameTeam( &g_entities[scn.client[0]], e ) ) {
				scn.client[2] = i;
				ally = qtrue;
			}
			else if ( spare < 0 ) {
				spare = i;
			}
		}
		if ( !defender ) {
			trap->Print( "scenario: needs two bots\n" );
			return;
		}
		scn.numClients = ally ? 3 : 2;
		if ( G_ScenarioOptIs( "target=ally" ) ) { // the teammate in the defender's place and role
			const int enemy = scn.client[1];

			if ( !ally ) {
				trap->Print( "scenario: target=ally needs a team game and a third bot, a teammate of the first\n" );
				return;
			}
			scn.client[1] = scn.client[2];
			scn.client[2] = enemy;
		}
	}
	scn.pickupDist = (float)G_ScenarioOpt( "pickup", 0 );
	scn.away = G_ScenarioOptIs( "away" );
	scn.spectatorMode = G_ScenarioOptIs( "spectator=follow1" ) ? SPEC_FOLLOW1
		: G_ScenarioOptIs( "spectator=follow2" ) ? SPEC_FOLLOW2
		: G_ScenarioOptIs( "spectator" ) ? SPEC_ATTACKER : SPEC_NONE;
	scn.reps = G_ScenarioArgInt( 2, 100 );
	trap->Argv( 3, arg, sizeof( arg ) );
	if ( !Q_stricmp( type, "saber" ) ) {
		static const char *styles[] = { "fast", "medium", "strong", "dual", "staff" };
		static const int styleValues[] = { SS_FAST, SS_MEDIUM, SS_STRONG, SS_DUAL, SS_STAFF };
		static const char *modes[] = { "idle", "attack", "throw", "lock", "kata", "back", "backcrouch", "jump", "lunge",
			"cartwheel", "backflip", "kick", "airkick", "rollstab", "stabdown" };
		char mode[MAX_TOKEN_CHARS];

		scn.style = SS_MEDIUM;
		for ( i = 0; i < (int)ARRAY_LEN( styles ); i++ ) {
			if ( !Q_stricmp( arg, styles[i] ) ) {
				scn.style = styleValues[i];
			}
		}
		trap->Argv( 4, mode, sizeof( mode ) );
		scn.saberMode = SM_IDLE;
		for ( i = 0; i < SM_NUM_MODES; i++ ) {
			if ( !Q_stricmp( mode, modes[i] ) ) {
				scn.saberMode = (saberMode_t)i;
			}
		}
		scn.dist = (float)G_ScenarioArgInt( 5, 56 );
		scn.type = SCN_SABER;
	}
	else if ( !Q_stricmp( type, "force" ) ) {
		static const char *powers[] = { "lightning", "grip", "drain", "rage", "protect", "absorb", "heal", "levitation", "speed",
			"push", "pull", "telepathy", "teamheal", "teamforce", "see", "saberthrow" };
		static const forcePowers_t powerValues[] = { FP_LIGHTNING, FP_GRIP, FP_DRAIN, FP_RAGE, FP_PROTECT, FP_ABSORB,
			FP_HEAL, FP_LEVITATION, FP_SPEED, FP_PUSH, FP_PULL, FP_TELEPATHY, FP_TEAM_HEAL, FP_TEAM_FORCE, FP_SEE, FP_SABERTHROW };

		scn.power = FP_LIGHTNING;
		for ( i = 0; i < (int)ARRAY_LEN( powers ); i++ ) {
			if ( !Q_stricmp( arg, powers[i] ) ) {
				scn.power = powerValues[i];
			}
		}
		if ( ( scn.power == FP_TEAM_HEAL || scn.power == FP_TEAM_FORCE ) && scn.numClients < 3 ) {
			trap->Print( "scenario: the team powers need a team game and a third bot, a teammate of the first\n" );
			return;
		}
		scn.level = Q_max( 1, Q_min( 3, G_ScenarioArgInt( 4, 3 ) ) );
		scn.dist = (float)G_ScenarioArgInt( 5, 128 );
		scn.defenderLevel = Q_max( 0, Q_min( 3, G_ScenarioArgInt( 6, 0 ) ) );
		scn.type = SCN_FORCE;
	}
	else if ( !Q_stricmp( type, "weapon" ) ) {
		scn.weapon = atoi( arg );
		if ( scn.weapon <= WP_NONE || scn.weapon >= WP_NUM_WEAPONS ) {
			trap->Print( "scenario: bad weapon number\n" );
			return;
		}
		scn.fire = Q_max( 0, Q_min( 2, G_ScenarioArgInt( 4, 0 ) ) );
		scn.dist = (float)G_ScenarioArgInt( 5, 256 );
		scn.type = SCN_WEAPON;
	}
	else if ( !Q_stricmp( type, "turret" ) ) {
		scn.turretG2 = Q_stricmp( arg, "hoth" ) ? qtrue : qfalse;
		scn.dist = (float)G_ScenarioArgInt( 4, 256 );
		scn.type = SCN_TURRET;
	}
	else if ( !Q_stricmp( type, "flag" ) ) {
		scn.flagMode = !Q_stricmp( arg, "return" ) ? FM_RETURN : !Q_stricmp( arg, "switch" ) ? FM_SWITCH : FM_CAPTURE;
		scn.dist = (float)G_ScenarioArgInt( 4, 128 );
		scn.type = SCN_FLAG;
	}
	else if ( !Q_stricmp( type, "objective" ) ) {
		const int team = g_entities[scn.client[0]].client->sess.sessionTeam;
		const int number = atoi( arg );
		gentity_t *o = NULL;

		while ( ( o = G_Find( o, FOFS( classname ), "info_siege_objective" ) ) != NULL
			&& ( o->objective != number || o->side != team ) ) {
		}
		if ( !o ) {
			trap->Print( "scenario: no siege objective %d of the attacker's team\n", number );
			return;
		}
		scn.objective = o;
		scn.dist = (float)G_ScenarioArgInt( 4, 128 );
		scn.type = SCN_OBJECTIVE;
	}
	else if ( !Q_stricmp( type, "team" ) ) {
		if ( level.gametype != GT_TEAM && level.gametype != GT_CTF && level.gametype != GT_CTY ) {
			trap->Print( "scenario: team needs a team game, CTF or CTY\n" );
			return;
		}
		scn.dist = (float)G_ScenarioArgInt( 4, 128 );
		scn.type = SCN_TEAM;
	}
	else if ( !Q_stricmp( type, "spectate" ) ) {
		scn.spectateMode = Q_stricmp( arg, "fly" ) ? SPM_CYCLE : SPM_FLY;
		scn.spectatorMode = ( scn.spectateMode == SPM_FLY ) ? SPEC_FREE : SPEC_ATTACKER;
		scn.dist = (float)G_ScenarioArgInt( 4, 128 );
		scn.type = SCN_SPECTATE;
	}
	else if ( !Q_stricmp( type, "holdable" ) ) {
		static const struct { const char *name; holdable_t item; } holdables[] = {
			{ "seeker", HI_SEEKER }, { "shield", HI_SHIELD }, { "medpac", HI_MEDPAC },
			{ "medpac_big", HI_MEDPAC_BIG }, { "binoculars", HI_BINOCULARS },
			{ "sentry", HI_SENTRY_GUN }, { "jetpack", HI_JETPACK }, { "healthdisp", HI_HEALTHDISP },
			{ "ammodisp", HI_AMMODISP }, { "eweb", HI_EWEB }, { "cloak", HI_CLOAK } };

		scn.holdable = HI_NONE;
		for ( i = 0; i < (int)ARRAY_LEN( holdables ); i++ ) {
			if ( !Q_stricmp( arg, holdables[i].name ) ) {
				scn.holdable = holdables[i].item;
			}
		}
		if ( scn.holdable == HI_NONE ) {
			trap->Print( "scenario: holdable seeker, shield, medpac, medpac_big, binoculars, sentry, jetpack, healthdisp, ammodisp, eweb or cloak\n" );
			return;
		}
		scn.dist = (float)G_ScenarioArgInt( 4, 256 );
		scn.type = SCN_HOLDABLE;
	}
	else if ( !Q_stricmp( type, "item" ) ) {
		Q_strncpyz( scn.itemClass, arg, sizeof( scn.itemClass ) );
		scn.dist = (float)G_ScenarioArgInt( 4, 128 );
		scn.type = SCN_ITEM;
	}
	else if ( !Q_stricmp( type, "vehicle" ) ) {
		char mode[MAX_TOKEN_CHARS];

		Q_strncpyz( scn.vehicleType, arg, sizeof( scn.vehicleType ) );
		trap->Argv( 4, mode, sizeof( mode ) );
		scn.vehicleMode = Q_stricmp( mode, "destroy" ) ? VM_RIDE : VM_DESTROY;
		scn.dist = (float)G_ScenarioArgInt( 5, 512 );
		scn.type = SCN_VEHICLE;
	}
	else {
		trap->Print( "scenario: saber, force, weapon, turret, flag, objective, team, spectate, holdable, item, vehicle or off\n" );
		return;
	}
	if ( scn.type != SCN_ITEM ) {
		G_ScenarioOptString( "item", scn.itemClass, sizeof( scn.itemClass ) );
	}
	if ( scn.itemClass[0] && ( !scn.pickupDist || !BG_FindItem( scn.itemClass ) ) ) {
		trap->Print( "scenario: an item needs pickup= and an item's classname\n" );
		scn.type = SCN_NONE;
		return;
	}
	if ( scn.spectatorMode != SPEC_NONE ) { // a bot spectator, or else a bot without a role made one
		const int bot = ( specBot >= 0 ) ? specBot : spare;

		if ( bot < 0 ) {
			trap->Print( "scenario: the spectator needs a bot spectator or a bot without a role\n" );
			scn.type = SCN_NONE;
			return;
		}
		scn.client[SCN_SPECTATOR] = bot;
		if ( g_entities[bot].client->sess.sessionTeam != TEAM_SPECTATOR ) {
			SetTeam( &g_entities[bot], "spectator" );
		}
	}
	for ( i = 0; i < scn.numClients; i++ ) { // the teams every reset gives back
		scn.team[i] = g_entities[scn.client[i]].client->sess.sessionTeam;
	}
	if ( scn.type == SCN_VEHICLE && BG_VehicleGetIndex( scn.vehicleType ) == VEHICLE_NONE ) {
		trap->Print( "scenario: no vehicle %s in ext_data/vehicles\n", scn.vehicleType );
		scn.type = SCN_NONE;
		return;
	}
	if ( !G_ScenarioFindPlace() ) {
		trap->Print( "scenario: no open spawn point for a distance of %.0f%s\n", scn.dist,
			( scn.type == SCN_VEHICLE ) ? ", the vehicle between the bots" : "" );
		scn.type = SCN_NONE;
		return;
	}
	if ( ( scn.type == SCN_WEAPON && scn.weapon == WP_EMPLACED_GUN ) || scn.type == SCN_TURRET ) {
		// the gun 52 units in front of the attacker, clear of its box (the gun drops itself to the floor; it is used from 64
		// units at most), a turret 48 units in front, on the floor
		const vec3_t fwd = { Q_cospif( scn.yaw / 180.0f ), Q_sinpif( scn.yaw / 180.0f ), 0 };
		vec3_t at, below;
		trace_t tr;

		if ( scn.type == SCN_WEAPON ) {
			VectorMA( scn.origin, 52, fwd, at );
			scn.spawned[0] = G_ScenarioSpawn( "emplaced_gun", at, scn.yaw, 0, 0 );
		}
		else {
			VectorMA( scn.origin, 48, fwd, at );
			VectorCopy( at, below );
			below[2] -= 128;
			trap->Trace( &tr, at, vec3_origin, vec3_origin, below, ENTITYNUM_NONE, MASK_SOLID, qfalse, 0, 0 );
			VectorCopy( tr.endpos, at );
			if ( scn.turretG2 ) { // upside down (2), on the floor: its spawn lowers it by 22 units
				at[2] += 22;
				scn.spawned[0] = G_ScenarioSpawn( "misc_turretG2", at, scn.yaw, 2, 0 );
			}
			else {
				scn.spawned[0] = G_ScenarioSpawn( "misc_turret", at, scn.yaw, 0, 0 );
				if ( scn.spawned[0] && scn.spawned[0]->r.ownerNum != ENTITYNUM_NONE ) {
					scn.spawned[1] = &g_entities[scn.spawned[0]->r.ownerNum];	// its top, which aims and fires
				}
			}
		}
		if ( !scn.spawned[0] ) {
			trap->Print( "scenario: the entity could not be spawned\n" );
			scn.type = SCN_NONE;
			return;
		}
	}
	if ( scn.pickupDist ) { // the holocron, the Jedi Master's saber or the enemy flag in front of the attacker, on the way to the defender
		const vec3_t fwd = { Q_cospif( scn.yaw / 180.0f ), Q_sinpif( scn.yaw / 180.0f ), 0 };
		vec3_t at;

		VectorMA( scn.origin, scn.pickupDist, fwd, at );
		if ( scn.itemClass[0] ) { // an item of the map's kind, put back at every reset
			scn.spawned[0] = scn.pickup = G_ScenarioSpawn( scn.itemClass, at, scn.yaw, 0, 0 );
		}
		else if ( level.gametype == GT_HOLOCRON && scn.type == SCN_FORCE ) {
			scn.spawned[0] = scn.pickup = G_ScenarioSpawn( "misc_holocron", at, scn.yaw, 0, scn.power );
		}
		else if ( level.gametype == GT_JEDIMASTER && gJMSaberEnt && gJMSaberEnt->inuse ) {
			scn.pickup = gJMSaberEnt;
			VectorCopy( at, scn.pickup->s.origin2 );	// its spot from now on (ThrowSaberToAttacker), placed at every reset
		}
		else if ( level.gametype == GT_CTF || level.gametype == GT_CTY ) {
			// the flags' bases from now on, the attacker's own flag behind it; placed at every reset
			const int team = g_entities[scn.client[0]].client->sess.sessionTeam;

			scn.pickup = Team_ResetFlag( OtherTeam( team ) );
			scn.ownFlag = Team_ResetFlag( team );
			if ( scn.pickup && scn.ownFlag ) {
				G_ScenarioFlagSpot( scn.pickup, at, scn.flagSpot[0] );
				VectorMA( scn.origin, -scn.pickupDist, fwd, at );
				G_ScenarioFlagSpot( scn.ownFlag, at, scn.flagSpot[1] );
			}
			else {
				scn.pickup = NULL;
			}
		}
		if ( !scn.pickup ) {
			trap->Print( "scenario: pickup= needs an item, a force scenario in a holocron game, a Jedi Master game, or a CTF or CTY game\n" );
			scn.type = SCN_NONE;
			return;
		}
	}
	if ( scn.type == SCN_FLAG && !scn.ownFlag ) {
		trap->Print( "scenario: flag needs a CTF or CTY game and pickup=\n" );
		scn.type = SCN_NONE;
		return;
	}
	scn.traceReps = trap->Cvar_VariableIntegerValue( "scenario_trace" );
	trap->Print( "scenario %s: %d repetitions of %d ms, bots %d and %d, distance %.0f\n", type, scn.reps, scn.repMsec,
		scn.client[0], scn.client[1], scn.dist );
}

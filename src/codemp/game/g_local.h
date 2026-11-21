/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
Copyright (C) 2013 - 2015, OpenJK contributors
Copyright (C) 2026, ModernJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

#pragma once

// g_local.h -- local definitions for game module

#include <optional>

#include <qcommon/q_shared.h>
#include "bg_public.h"
#include "bg_vehicles.h"
#include "g_public.h"

typedef struct gentity_s gentity_t;
typedef struct gclient_s gclient_t;

// npc stuff
#include "b_public.h"

extern meansOfDeath_t gPainMOD;
extern int gPainHitLoc;
extern vec3_t gPainPoint;

//==================================================================

#define SECURITY_LOG "security.log"

inline constexpr int BODY_QUEUE_SIZE = 8;

#ifndef INFINITE
#define INFINITE 1'000'000
#endif

inline constexpr int FRAMETIME = 100; // msec

// Two clocks (plan step 14). The game module was tuned on the 50 ms frames of sv_fps 20; the server ticks at 100 Hz,
// and every 20 Hz instant (a multiple of 50 ms) is a tick. level.time is the logic clock: the last 20 Hz instant, which
// all game code reads (timers, thinks, damage, user commands), so it keeps the timing of sv_fps 20. level.tickTime is the
// tick's time, read only for the bots' command times and the sanity clamp of a command's time. The logic tuned on 50 ms
// frames (thinks, force powers, the saber, pain, missiles, movers, items, the bots' AI) runs on the frames where
// G_GRID20_STEP is true.
#define G_GRID20_STEP (level.logicFrame)
inline constexpr int CARNAGE_REWARD_TIME = 3000;
inline constexpr int REWARD_SPRITE_TIME = 2000;

inline constexpr int INTERMISSION_DELAY_TIME = 1000;
inline constexpr int SP_INTERMISSION_DELAY_TIME = 5000;

// primarily used by NPCs
inline constexpr int START_TIME_LINK_ENTS = FRAMETIME * 1;   // time-delay after map start at which all ents have been spawned, so can link them
inline constexpr int START_TIME_FIND_LINKS = FRAMETIME * 2;  // time-delay after map start at which you can find linked entities
inline constexpr int START_TIME_REMOVE_ENTS = FRAMETIME * 3; // time-delay after map start to remove temporary ents
inline constexpr int START_TIME_NAV_CALC = FRAMETIME * 4;    // time-delay after map start to connect waypoints and calc routes

// gentity->flags
enum class gentityFlags_t {
    FL_NONE = 0,
    FL_GODMODE = 0x00000010,
    FL_NOTARGET = 0x00000020,
    FL_TEAMSLAVE = 0x00000400, // not the first on the team
    FL_NO_KNOCKBACK = 0x00000800,
    FL_DROPPED_ITEM = 0x00001000,
    FL_NO_BOTS = 0x00002000,       // spawn point not for bot use
    FL_NO_HUMANS = 0x00004000,     // spawn point just for bots
    FL_FORCE_GESTURE = 0x00008000, // force gesture on client
    FL_INACTIVE = 0x00010000,      // inactive
    FL_NAVGOAL = 0x00020000,       // for npc nav stuff
    FL_DONT_SHOOT = 0x00040000,
    FL_SHIELDED = 0x00080000,
    FL_UNDYING = 0x00100000, // takes damage down to 1, but never dies

    // ex-eFlags -rww
    FL_BOUNCE = 0x08000000,          // for missiles
    FL_BOUNCE_HALF = 0x00200000,     // for missiles
    FL_BOUNCE_SHRAPNEL = 0x00400000, // special shrapnel flag

    // vehicle game-local stuff -rww
    FL_VEH_BOARDING = 0x00800000, // special shrapnel flag

    // breakable flags -rww
    FL_DMG_BY_SABER_ONLY = 0x01000000,      // only take dmg from saber
    FL_DMG_BY_HEAVY_WEAP_ONLY = 0x02000000, // only take dmg from explosives

    FL_BBRUSH = 0x04000000, // I am a breakable brush
};
template <> inline constexpr bool enableEnumFlags<gentityFlags_t> = true;

// damage flags
enum class damageFlags_t {
    DAMAGE_NORMAL = 0x00000000,             // No flags set.
    DAMAGE_RADIUS = 0x00000001,             // damage was indirect
    DAMAGE_NO_ARMOR = 0x00000002,           // armour does not protect from this damage
    DAMAGE_NO_KNOCKBACK = 0x00000004,       // do not affect velocity, just view angles
    DAMAGE_NO_PROTECTION = 0x00000008,      // armor, shields, invulnerability, and godmode have no effect
    DAMAGE_NO_TEAM_PROTECTION = 0x00000010, // armor, shields, invulnerability, and godmode have no effect
    // JK2 flags
    DAMAGE_EXTRA_KNOCKBACK = 0x00000040,      // add extra knockback to this damage
    DAMAGE_DEATH_KNOCKBACK = 0x00000080,      // only does knockback on death of target
    DAMAGE_IGNORE_TEAM = 0x00000100,          // damage is always done, regardless of teams
    DAMAGE_NO_DAMAGE = 0x00000200,            // do no actual damage but react as if damage was taken
    DAMAGE_HALF_ABSORB = 0x00000400,          // half shields, half health
    DAMAGE_HALF_ARMOR_REDUCTION = 0x00000800, // This damage doesn't whittle down armor as efficiently.
    DAMAGE_HEAVY_WEAP_CLASS = 0x00001000,     // Heavy damage
    DAMAGE_NO_HIT_LOC = 0x00002000,           // No hit location
    DAMAGE_NO_SELF_PROTECTION = 0x00004000,   // Dont apply half damage to self attacks
    DAMAGE_NO_DISMEMBER = 0x00008000,         // Dont do dismemberment
    DAMAGE_SABER_KNOCKBACK1 = 0x00010000,     // Check the attacker's first saber for a knockbackScale
    DAMAGE_SABER_KNOCKBACK2 = 0x00020000,     // Check the attacker's second saber for a knockbackScale
    DAMAGE_SABER_KNOCKBACK1_B2 = 0x00040000,  // Check the attacker's first saber for a knockbackScale2
    DAMAGE_SABER_KNOCKBACK2_B2 = 0x00080000,  // Check the attacker's second saber for a knockbackScale2
};
template <> inline constexpr bool enableEnumFlags<damageFlags_t> = true;

#ifndef FINAL_BUILD
#define DEBUG_SABER_BOX
#endif

// make sure this matches game/match.h for botlibs
#define EC "\x19"

inline constexpr int MAX_G_SHARED_BUFFER_SIZE = 8192;
// used for communication with the engine
typedef union sharedBuffer_u {
    char raw[MAX_G_SHARED_BUFFER_SIZE];
    T_G_ICARUS_PLAYSOUND playSound;
    T_G_ICARUS_SET set;
    T_G_ICARUS_LERP2POS lerp2Pos;
    T_G_ICARUS_LERP2ORIGIN lerp2Origin;
    T_G_ICARUS_LERP2ANGLES lerp2Angles;
    T_G_ICARUS_GETTAG getTag;
    T_G_ICARUS_LERP2START lerp2Start;
    T_G_ICARUS_LERP2END lerp2End;
    T_G_ICARUS_USE use;
    T_G_ICARUS_KILL kill;
    T_G_ICARUS_REMOVE remove;
    T_G_ICARUS_PLAY play;
    T_G_ICARUS_GETFLOAT getFloat;
    T_G_ICARUS_GETVECTOR getVector;
    T_G_ICARUS_GETSTRING getString;
    T_G_ICARUS_SOUNDINDEX soundIndex;
    T_G_ICARUS_GETSETIDFORSTRING getSetIDForString;
} sharedBuffer_t;
extern sharedBuffer_t gSharedBuffer;

// movers are things like doors, plats, buttons, etc
enum class moverState_t { MOVER_POS1, MOVER_POS2, MOVER_1TO2, MOVER_2TO1 };

#define SP_PODIUM_MODEL "models/mapobjects/podium/podium4.md3"

enum class hitLocation_t {
    HL_NONE = 0,
    HL_FOOT_RT,
    HL_FOOT_LT,
    HL_LEG_RT,
    HL_LEG_LT,
    HL_WAIST,
    HL_BACK_RT,
    HL_BACK_LT,
    HL_BACK,
    HL_CHEST_RT,
    HL_CHEST_LT,
    HL_CHEST,
    HL_ARM_RT,
    HL_ARM_LT,
    HL_HAND_RT,
    HL_HAND_LT,
    HL_HEAD,
    HL_GENERIC1,
    HL_GENERIC2,
    HL_GENERIC3,
    HL_GENERIC4,
    HL_GENERIC5,
    HL_GENERIC6,
    HL_MAX
};
template <> inline constexpr bool enableEnumArithmetic<hitLocation_t> = true;

// the part of a player a damage box stands for, whose damage goes to the player
enum class damageRedirect_t {
    DAMAGEREDIRECT_NONE, // takes its own damage
    DAMAGEREDIRECT_HEAD,
    DAMAGEREDIRECT_RLEG,
    DAMAGEREDIRECT_LLEG,
};

//============================================================================
extern void* precachedKyle;
extern void* g2SaberInstance;

extern bool gEscaping;
extern int gEscapeTime;

struct gentity_s {
    // rww - entstate must be first, to correspond with the bg shared entity structure
    entityState_t s;            // communicated by server to clients
    playerState_t* playerState; // ptr to playerstate if applicable (for bg ents)
    Vehicle_t* m_pVehicle;      // vehicle data
    void* ghoul2;               // g2 instance
    int localAnimIndex;         // index locally (game/cgame) to anim data for this skel
    vec3_t modelScale;          // needed for g2 collision

    // From here up must be the same as centity_t/bgEntity_t

    entityShared_t r; // shared by both the server system and game

    // rww - these are shared icarus things. They must be in this order as well in relation to the entityshared structure.
    EnumArray<int, taskID_t, taskID_t::NUM_TIDS> taskID;
    parms_t* parms;
    EnumArray<char*, bSet_t, bSet_t::NUM_BSETS> behaviorSet;
    char* script_targetname;
    int delayScriptTime;
    const char* fullName;

    // rww - targetname and classname are now shared as well. ICARUS needs access to them.
    char* targetname;
    const char* classname; // set in QuakeEd

    // rww - and yet more things to share. This is because the nav code is in the exe because it's all C++.
    int waypoint;          // Set once per frame, if you've moved, and if someone asks
    int lastWaypoint;      // To make sure you don't double-back
    int lastValidWaypoint; // ALWAYS valid -used for tracking someone you lost
    int noWaypointTime;    // Debouncer - so don't keep checking every waypoint in existance every frame that you can't find one
    int combatPoint;
    int failedWaypoints[MAX_FAILED_NODES];
    int failedWaypointCheckTime;

    int next_roff_time; // rww - npc's need to know when they're getting roff'd

    // DO NOT MODIFY ANYTHING ABOVE THIS, THE SERVER
    // EXPECTS THE FIELDS IN THAT ORDER!
    //================================

    struct gclient_s* client; // nullptr if not a client

    gNPC_t* NPC;             // Only allocated if the entity becomes an NPC
    int cantHitEnemyCounter; // HACK - Makes them look for another enemy on the same team if the one they're after can't be hit

    bool noLumbar; // see note in cg_local.h

    bool inuse;

    int lockCount; // used by NPCs

    int spawnflags; // set in QuakeEd

    team_t teamnodmg; // damage will be ignored if it comes from this team

    char* roffname;   // set in QuakeEd
    char* rofftarget; // set in QuakeEd

    char* healingclass;  // set in quakeed
    char* healingsound;  // set in quakeed
    int healingrate;     // set in quakeed
    int healingDebounce; // debounce for generic object healing shiz

    char* ownername;

    int objective;
    team_t side;

    int passThroughNum; // set to index to pass through (+1) for missiles

    int aimDebounceTime;
    int painDebounceTime;
    int attackDebounceTime;
    team_t alliedTeam; // only useable by this team, never target this team

    int roffid; // if roffname != nullptr then set on spawn

    bool neverFree; // if true, FreeEntity will only unlink
                    // bodyque uses this

    gentityFlags_t flags; // FL_* variables

    const char* model;
    const char* model2;
    int freetime; // level.time when the object was freed

    int eventTime; // events will be cleared EVENT_VALID_MSEC after set
    bool freeAfterEvent;
    bool unlinkAfterEvent;

    bool physicsObject;       // if true, it can be pushed by movers and fall off edges
                              // all game items are physicsObjects,
    float physicsBounce;      // 1.0 = continuous bounce, 0.0 = no bounce
    contentsFlags_t clipmask; // brushes with this content value will be collided against
                              // when moving.  items and corpses do not collide against
                              // players, for instance

    // Only used by NPC_spawners
    const char* NPC_type;
    char* NPC_targetname;
    char* NPC_target;

    // movers
    moverState_t moverState;
    int soundPos1;
    int sound1to2;
    int sound2to1;
    int soundPos2;
    int soundLoop;
    gentity_t* parent;
    gentity_t* nextTrain;
    gentity_t* prevTrain;
    vec3_t pos1, pos2;

    // for npc's
    vec3_t pos3;

    char* message;

    int timestamp; // body queue sinking, etc

    float angle; // set in editor, -1 = up, -2 = down
    char* target;
    char* target2;
    char* target3; // For multiple targets, not used for firing/triggering/using, though, only for path branches
    char* target4; // For multiple targets, not used for firing/triggering/using, though, only for path branches
    char* target5; // mainly added for siege items
    char* target6; // mainly added for siege items

    char* team;
    char* targetShaderName;
    char* targetShaderNewName;
    gentity_t* target_ent;

    char* closetarget;
    char* opentarget;
    char* paintarget;

    char* goaltarget;
    char* idealclass;

    float radius;

    int maxHealth; // used as a base for crosshair health display

    float speed;
    vec3_t movedir;
    float mass;
    int setTime;

    // Think Functions
    int nextthink;
    void (*think)(gentity_t* self);
    void (*reached)(gentity_t* self); // movers call this when hitting endpoint
    void (*blocked)(gentity_t* self, gentity_t* other);
    void (*touch)(gentity_t* self, gentity_t* other, trace_t* trace);
    void (*use)(gentity_t* self, gentity_t* other, gentity_t* activator);
    void (*pain)(gentity_t* self, gentity_t* attacker, int damage);
    void (*die)(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, meansOfDeath_t mod);

    int pain_debounce_time;
    int fly_sound_debounce_time; // wind tunnel
    int last_move_time;

    // Health and damage fields
    int health;
    bool takedamage;
    material_t material;

    int damage;
    damageFlags_t dflags;
    int splashDamage; // quad will increase this without increasing radius
    int splashRadius;
    meansOfDeath_t methodOfDeath;
    meansOfDeath_t splashMethodOfDeath;

    int locationDamage[to_underlying(hitLocation_t::HL_MAX)]; // Damage accumulated on different body locations

    int count;
    int bounceCount;
    bool alt_fire;

    gentity_t* chain;
    gentity_t* enemy;
    gentity_t* lastEnemy;
    gentity_t* activator;
    gentity_t* teamchain;  // next entity in team
    gentity_t* teammaster; // master of the team

    contentsFlags_t watertype;
    int waterlevel;

    int noise_index;

    // timing variables
    float wait;
    float random;
    int delay;

    // generic values used by various entities for different purposes.
    int genericValue1;
    int genericValue2;
    int genericValue3;
    int genericValue4;
    int genericValue5;
    int genericValue6;
    int genericValue7;
    int genericValue8;
    int genericValue9;
    int genericValue10;
    int genericValue11;
    int genericValue12;
    int genericValue13;
    int genericValue14;
    int genericValue15;

    char* soundSet;

    bool isSaberEntity;

    damageRedirect_t damageRedirect; // if entity takes damage, redirect to..
    int damageRedirectTo;            // this entity number

    vec3_t epVelocity;
    float epGravFactor;

    gitem_t* item; // for bonus items

    // OpenJK add
    int useDebounceTime; // for cultist_destroyer
};

enum class clientConnected_t { CON_DISCONNECTED, CON_CONNECTING, CON_CONNECTED };

enum class spectatorState_t { SPECTATOR_NOT, SPECTATOR_FREE, SPECTATOR_FOLLOW, SPECTATOR_SCOREBOARD };

enum class playerTeamStateState_t {
    TEAM_BEGIN, // Beginning a team game, spawn at base
    TEAM_ACTIVE // Now actively playing
};

typedef struct playerTeamState_s {
    playerTeamStateState_t state;

    int location;

    int captures;
    int basedefense;
    int carrierdefense;
    int flagrecovery;
    int fragcarrier;
    int assists;

    float lasthurtcarrier;
    float lastreturnedflag;
    float flagsince;
    float lastfraggedcarrier;
} playerTeamState_t;

// the auto following clients don't follow a specific client
// number, but instead follow the first two active players

// client data that stays across multiple levels or tournament restarts
// this is achieved by writing all the data to cvar strings at game shutdown
// time and reading them back at connection time.  Anything added here
// MUST be dealt with in G_InitSessionData() / G_ReadSessionData() / G_WriteSessionData()
typedef struct clientSession_s {
    team_t sessionTeam;
    int spectatorNum; // for determining next-in-line to play
    spectatorState_t spectatorState;
    int spectatorClient;       // for chasecam and follow mode
    int wins, losses;          // tournament stats
    forcePowers_t selectedFP;  // check against this, if doesn't match value in playerstate then update userinfo
    saber_styles_t saberLevel; // similar to above method, but for current saber attack level
    int setForce;              // set to true once player is given the chance to set force powers
    int updateUITime;          // only update userinfo for FP/SL if < level.time
    bool teamLeader;           // true when this client is a team leader
    char siegeClass[64];
    duelTeam_t duelTeam;
    team_t siegeDesiredTeam;

    char IP[NET_ADDRSTRMAXLEN];
} clientSession_t;

// playerstate mGameFlags
enum class voteFlags_t {
    PSG_NONE = 0,
    PSG_VOTED = 1 << 0,     // already cast a vote
    PSG_TEAMVOTED = 1 << 1, // already cast a team vote
};
template <> inline constexpr bool enableEnumFlags<voteFlags_t> = true;

//

// client data that stays across multiple respawns, but is cleared
// on each level change or team change at ClientBegin()
typedef struct clientPersistant_s {
    clientConnected_t connected;
    usercmd_t cmd;          // we would lose angles if not persistant
    bool localClient;       // true if "ip" info key is "localhost"
    bool initialSpawn;      // the first spawn should be at a cool location
    bool predictItemPickup; // based on cg_predictItems userinfo
    char netname[MAX_NETNAME];
    char netname_nocolor[MAX_NETNAME];
    int netnameTime;             // Last time the name was changed
    int maxHealth;               // for handicapping
    int enterTime;               // level.time the client entered the game
    playerTeamState_t teamState; // status in teamplay games
    bool teamInfo;               // send team overlay updates?

    int connectTime;

    char saber1[MAX_QPATH], saber2[MAX_QPATH];

    int vote, teamvote; // 0 = none, 1 = yes, 2 = no

    char guid[33];
} clientPersistant_t;

typedef struct renderInfo_s {
    // In whole degrees, How far to let the different model parts yaw and pitch
    int headYawRangeLeft;
    int headYawRangeRight;
    int headPitchRangeUp;
    int headPitchRangeDown;

    int torsoYawRangeLeft;
    int torsoYawRangeRight;
    int torsoPitchRangeUp;
    int torsoPitchRangeDown;

    int legsFrame;
    int torsoFrame;

    float legsFpsMod;
    float torsoFpsMod;

    // Fields to apply to entire model set, individual model's equivalents will modify this value
    vec3_t customRGB; // Red Green Blue, 0 = don't apply
    int customAlpha;  // Alpha to apply, 0 = none?

    // RF?
    int renderFlags;

    //
    vec3_t muzzlePoint;
    vec3_t muzzleDir;
    vec3_t muzzlePointOld;
    vec3_t muzzleDirOld;
    // vec3_t		muzzlePointNext;	// Muzzle point one server frame in the future!
    // vec3_t		muzzleDirNext;
    int mPCalcTime; // Last time muzzle point was calced

    //
    float lockYaw; //

    //
    vec3_t headPoint;   // Where your tag_head is
    vec3_t headAngles;  // where the tag_head in the torso is pointing
    vec3_t handRPoint;  // where your right hand is
    vec3_t handLPoint;  // where your left hand is
    vec3_t crotchPoint; // Where your crotch is
    vec3_t footRPoint;  // where your right hand is
    vec3_t footLPoint;  // where your left hand is
    vec3_t torsoPoint;  // Where your chest is
    vec3_t torsoAngles; // Where the chest is pointing
    vec3_t eyePoint;    // Where your eyes are
    vec3_t eyeAngles;   // Where your eyes face
    int lookTarget;     // Which ent to look at with lookAngles
    lookMode_t lookMode;
    int lookTargetClearTime;    // Time to clear the lookTarget
    int lastVoiceVolume;        // Last frame's voice volume
    vec3_t lastHeadAngles;      // Last headAngles, NOT actual facing of head model
    vec3_t headBobAngles;       // headAngle offsets
    vec3_t targetHeadBobAngles; // head bob angles will try to get to targetHeadBobAngles
    int lookingDebounceTime;    // When we can stop using head looking angle behavior
    float legsYaw;              // yaw angle your legs are actually rendering at
    vec3_t modelAngles;         // the angles the server's model was last posed with (WP_SaberPositionUpdate): the legs', or the vehicle's yaw

    // for tracking legitimate bolt indecies
    void* lastG2; // if it doesn't match ent->ghoul2, the bolts are considered invalid.
    int headBolt;
    int handRBolt;
    int handLBolt;
    int torsoBolt;
    int crotchBolt;
    int footRBolt;
    int footLBolt;
    int motionBolt;

    int boltValidityTime;
} renderInfo_t;

// this structure is cleared on each ClientSpawn(),
// except for 'client->pers' and 'client->sess'
struct gclient_s {
    // ps MUST be the first element, because the server expects it
    playerState_t ps; // communicated by server to clients

    // the rest of the structure is private to game
    clientPersistant_t pers;
    clientSession_t sess;

    saberInfo_t saber[MAX_SABERS];
    void* weaponGhoul2[MAX_SABERS];

    int tossableItemDebounce;

    int bodyGrabTime;
    int bodyGrabIndex;

    int pushEffectTime;

    int invulnerableTimer;

    saber_styles_t saberCycleQueue;

    animNumber_t legsAnimExecute;
    animNumber_t torsoAnimExecute;
    bool legsLastFlip;
    bool torsoLastFlip;

    bool readyToExit; // wishes to leave the intermission

    bool noclip;

    int lastCmdTime; // level.time of last usercmd_t, for EF_CONNECTION
                     // we can't just use pers.lastCommand.time, because
                     // of the g_sycronousclients case
    buttonFlags_t buttons;
    buttonFlags_t oldbuttons;
    buttonFlags_t latched_buttons;

    vec3_t oldOrigin;

    // sum up damage over an entire frame, so
    // shotgun blasts give a single big kick
    int damage_armor;      // damage absorbed by armor
    int damage_blood;      // damage taken out of health
    int damage_knockback;  // impact damage
    vec3_t damage_from;    // origin for vector calculation
    bool damage_fromWorld; // if true, don't use the damage_from vector

    int damageBoxHandle_Head; // entity number of head damage box
    int damageBoxHandle_RLeg; // entity number of right leg damage box
    int damageBoxHandle_LLeg; // entity number of left leg damage box

    int accurateCount; // for "impressive" reward sound

    int accuracy_shots; // total number of shots
    int accuracy_hits;  // total number of hits

    //
    int lastkilled_client;       // last client that this client killed
    int lasthurt_client;         // last client that damaged this client
    meansOfDeath_t lasthurt_mod; // type of damage the client did

    // timers
    int respawnTime;        // can respawn when time > this, force after g_forcerespwan
    int inactivityTime;     // kick players when time > this
    bool inactivityWarning; // true if the five seoond warning has been given
    int rewardTime;         // clear the EF_AWARD_IMPRESSIVE, etc when time > this

    int airOutTime;

    int lastKillTime; // for multiple kill rewards

    bool fireHeld;   // used for hook
    gentity_t* hook; // grapple hook if out

    int switchTeamTime; // time the player switched teams

    int switchDuelTeamTime; // time the player switched duel teams

    int switchClassTime; // class changed debounce timer

    // timeResidual is used to handle events that happen every second
    // like health / armor countdowns and regeneration
    int timeResidual;

    char* areabits;

    int g2LastSurfaceHit;  // index of surface hit during the most recent ghoul2 collision performed on this client.
    int g2LastSurfaceTime; // time when the surface index was set (to make sure it's up to date)

    int corrTime;

    vec3_t lastHeadAngles;
    int lookTime;

    int brokenLimbs;

    bool noCorpse; // don't leave a corpse on respawn this time.

    int jetPackTime;

    bool jetPackOn;
    int jetPackToggleTime;
    int jetPackDebRecharge;
    int jetPackDebReduce;

    int cloakToggleTime;
    int cloakDebRecharge;
    int cloakDebReduce;

    int saberStoredIndex; // stores saberEntityNum from playerstate for when it's set to 0 (indicating saber was knocked out of the air)

    int saberKnockedTime; // if saber gets knocked away, can't pull it back until this value is < level.time

    vec3_t olderSaberBase; // Set before lastSaberBase_Always, to whatever lastSaberBase_Always was previously
    bool olderIsValid;     // is it valid?

    vec3_t lastSaberDir_Always;  // every getboltmatrix, set to saber dir
    vec3_t lastSaberBase_Always; // every getboltmatrix, set to saber base
    int lastSaberStorageTime;    // server time that the above two values were updated (for making sure they aren't out of date)

    bool hasCurrentPosition; // are lastSaberTip and lastSaberBase valid?

    int dangerTime; // level.time when last attack occured

    int idleTime; // keep track of when to play an idle anim on the client.

    int idleHealth;        // stop idling if health decreases
    vec3_t idleViewAngles; // stop idling if viewangles change

    int forcePowerSoundDebounce; // if > level.time, don't do certain sound events again (drain sound, absorb sound, etc)

    char modelname[MAX_QPATH];

    bool fjDidJump;

    bool ikStatus;

    int throwingIndex;
    int beingThrown;
    int doingThrow;

    float hiddenDist; // How close ents have to be to pick you up as an enemy
    vec3_t hiddenDir; // Normalized direction in which NPCs can't see you (you are hidden)

    renderInfo_t renderInfo;

    // mostly NPC stuff:
    npcteam_t playerTeam;
    npcteam_t enemyTeam;
    char* squadname;
    gentity_t* team_leader;
    gentity_t* leader;
    gentity_t* follower;
    int numFollowers;
    gentity_t* formationGoal;
    int nextFormGoal;
    class_t NPC_class;

    vec3_t pushVec;
    int pushVecTime;

    int siegeClass;
    int holdingObjectiveItem;

    // time values for when being healed/supplied by supplier class
    int isMedHealed;
    int isMedSupplied;

    // seperate debounce time for refilling someone's ammo as a supplier
    int medSupplyDebounce;

    // used in conjunction with ps.hackingTime
    int isHacking;
    vec3_t hackingAngles;

    // debounce time for sending extended siege data to certain classes
    int siegeEDataSend;

    int ewebIndex;  // index of e-web gun if spawned
    int ewebTime;   // e-web use debounce
    int ewebHealth; // health of e-web (to keep track between deployments)

    int inSpaceIndex;       // ent index of space trigger if inside one
    int inSpaceSuffocation; // suffocation timer

    int tempSpectate; // time to force spectator mode

    // keep track of last person kicked and the time so we don't hit multiple times per kick
    int jediKickIndex;
    int jediKickTime;

    // special moves (designed for kyle boss npc, but useable by players in mp)
    int grappleIndex;
    int grappleState;

    int solidHack;

    int noLightningTime;

    voteFlags_t mGameFlags;

    // fallen duelist
    bool iAmALoser;

    int lastGenCmd;
    int lastGenCmdTime;

    struct force {
        int regenDebounce;
        int drainDebounce;
        int lightningDebounce;
    } force;
};

// Interest points

inline constexpr int MAX_INTEREST_POINTS = 64;

typedef struct {
    vec3_t origin;
    char* target;
} interestPoint_t;

// Combat points

inline constexpr int MAX_COMBAT_POINTS = 512;

enum class combatPointFlags_t; // b_local.h

typedef struct {
    vec3_t origin;
    combatPointFlags_t flags;
    //	char		*NPC_targetname;
    //	team_t		team;
    bool occupied;
    int waypoint;
    int dangerTime;
} combatPoint_t;

// Alert events

inline constexpr int MAX_ALERT_EVENTS = 32;

enum class alertEventType_e {
    AET_SIGHT,
    AET_SOUND,
};

enum class alertEventLevel_e {
    AEL_NONE = -1,    // no alert yet
    AEL_MINOR,        // Enemy responds to the sound, but only by looking
    AEL_SUSPICIOUS,   // Enemy looks at the sound, and will also investigate it
    AEL_DISCOVERED,   // Enemy knows the player is around, and will actively hunt
    AEL_DANGER,       // Enemy should try to find cover
    AEL_DANGER_GREAT, // Enemy should run like hell!
};

typedef struct alertEvent_s {
    vec3_t position;         // Where the event is located
    float radius;            // Consideration radius
    alertEventLevel_e level; // Priority level of the event
    alertEventType_e type;   // Event type (sound,sight)
    gentity_t* owner;        // Who made the sound
    float light;             // ambient light level at point
    float addLight;          // additional light- makes it more noticable, even in darkness
    int ID;        // unique... if get a ridiculous number, this will repeat, but should not be a problem as it's just comparing it to your lastAlertID
    int timestamp; // when it was created
} alertEvent_t;

//
// this structure is cleared as each map is entered
//
typedef struct waypointData_s {
    char targetname[MAX_QPATH];
    char target[MAX_QPATH];
    char target2[MAX_QPATH];
    char target3[MAX_QPATH];
    char target4[MAX_QPATH];
    int nodeID;
} waypointData_t;

typedef struct {
    char message[MAX_SPAWN_VARS_CHARS];
    int count;
    int cs_index;
    vec3_t origin;
} locationData_t;

typedef struct level_locals_s {
    struct gclient_s* clients; // [maxclients]

    struct gentity_s* gentities;
    int gentitySize;
    int num_entities; // current number, <= MAX_GENTITIES

    int warmupTime; // restart match at this time

    fileHandle_t logFile;

    // store latched cvars here that we want to get at often
    int maxclients;

    int framenum;
    int time;         // in msec: the logic clock, the last 20 Hz instant (plan step 14)
    int previousTime; // so movers can back up when blocked: the logic clock's previous instant
    int tickTime;     // the tick's time: the bots' command times, the clamp of a command's time
    bool logicFrame;  // this frame is a 20 Hz instant: the logic clock moved (G_GRID20_STEP)

    int startTime; // level.time the map was started

    EnumArray<int, team_t, team_t::TEAM_NUM_TEAMS> teamScores;
    int lastTeamLocationTime; // last time of client team location update

    bool newSession; // don't use any old session data, because
                     // we changed gametype

    bool restarted; // waiting for a map_restart to fire

    int numConnectedClients;
    int numNonSpectatorClients;     // includes connecting clients
    int numPlayingClients;          // connected, non-spectators
    int sortedClients[MAX_CLIENTS]; // sorted by score
    int follow1, follow2;           // clientNums for auto-follow spectators

    int snd_fry; // sound index for standing in lava

    int snd_hack;        // hacking loop sound
    int snd_medHealed;   // being healed by supply class
    int snd_medSupplied; // being supplied by supply class

    // voting state
    char voteString[MAX_STRING_CHARS];
    char voteStringClean[MAX_STRING_CHARS];
    char voteDisplayString[MAX_STRING_CHARS];
    int voteTime;         // level.time vote was called
    int voteExecuteTime;  // time the vote is executed
    int voteExecuteDelay; // set per-vote
    int voteYes;
    int voteNo;
    int numVotingClients; // set by CalculateRanks

    bool votingGametype;
    gametype_t votingGametypeTo;

    // team voting state
    char teamVoteString[2][MAX_STRING_CHARS];
    char teamVoteStringClean[2][MAX_STRING_CHARS];
    char teamVoteDisplayString[2][MAX_STRING_CHARS];
    int teamVoteTime[2];        // level.time vote was called
    int teamVoteExecuteTime[2]; // time the vote is executed
    int teamVoteYes[2];
    int teamVoteNo[2];
    int numteamVotingClients[2]; // set by CalculateRanks

    // spawn variables
    bool spawning; // the G_Spawn*() functions are valid
    int numSpawnVars;
    char* spawnVars[MAX_SPAWN_VARS][2]; // key / value pairs
    int numSpawnVarChars;
    char spawnVarChars[MAX_SPAWN_VARS_CHARS];

    // intermission state
    int intermissionQueued; // intermission was qualified, but
                            // wait INTERMISSION_DELAY_TIME before
                            // actually going there so the last
                            // frag can be watched.  Disable future
                            // kills during this delay
    int intermissiontime;   // time the intermission was started
    char* changemap;
    bool readyToExit; // at least one client wants to exit
    int exitTime;
    vec3_t intermission_origin; // also used for spectator spawns
    vec3_t intermission_angle;

    int bodyQueIndex; // dead bodies
    gentity_t* bodyQue[BODY_QUEUE_SIZE];
    int portalSequence;

    alertEvent_t alertEvents[MAX_ALERT_EVENTS];
    int numAlertEvents;
    int curAlertID;

    AIGroupInfo_t groups[MAX_FRAME_GROUPS];

    // Interest points- squadmates automatically look at these if standing around and close to them
    interestPoint_t interestPoints[MAX_INTEREST_POINTS];
    int numInterestPoints;

    // Combat points- NPCs in bState BS_COMBAT_POINT will find their closest empty combat_point
    combatPoint_t combatPoints[MAX_COMBAT_POINTS];
    int numCombatPoints;

    // rwwRMG - added:
    int mNumBSPInstances;
    int mBSPInstanceDepth;
    vec3_t mOriginAdjust;
    float mRotationAdjust;
    char* mTargetAdjust;

    char mTeamFilter[MAX_QPATH];

    struct {
        fileHandle_t log;
    } security;

    struct {
        int num;
        char* infos[MAX_BOTS];
    } bots;

    struct {
        int num;
        char* infos[MAX_ARENAS];
    } arenas;

    struct {
        int num;
        bool linked;
        locationData_t data[MAX_LOCATIONS];
    } locations;

    gametype_t gametype;
    char mapname[MAX_QPATH];
    char rawmapname[MAX_QPATH];
} level_locals_t;

//
// g_spawn.cpp
//
bool G_SpawnString(const char* key, const char* defaultString, char** out);
// spawn string returns a temporary reference, you must CopyString() if you want to keep it
bool G_SpawnFloat(const char* key, const char* defaultString, float* out);
bool G_SpawnInt(const char* key, const char* defaultString, int* out);
bool G_SpawnVector(const char* key, const char* defaultString, float* out);
bool G_SpawnBoolean(const char* key, const char* defaultString, bool* out);
void G_SpawnEntitiesFromString(bool inSubBSP);
char* G_NewString(const char* string);

//
// g_cmds.cpp
//
void Cmd_Score_f(gentity_t* ent);
void StopFollowing(gentity_t* ent);
void BroadcastTeamChange(gclient_t* client, std::optional<team_t> oldTeam);
void SetTeam(gentity_t* ent, const char* s);
void Cmd_FollowCycle_f(gentity_t* ent, int dir);
void Cmd_SaberAttackCycle_f(gentity_t* ent);
int G_ItemUsable(playerState_t* ps, holdable_t forcedUse);
void Cmd_ToggleSaber_f(gentity_t* ent);
void Cmd_EngageDuel_f(gentity_t* ent);

//
// g_items.cpp
//
void ItemUse_Binoculars(gentity_t* ent);
void ItemUse_Shield(gentity_t* ent);
void ItemUse_Sentry(gentity_t* ent);

void Jetpack_Off(gentity_t* ent);
void Jetpack_On(gentity_t* ent);
void ItemUse_Jetpack(gentity_t* ent);
void ItemUse_UseCloak(gentity_t* ent);
void ItemUse_UseDisp(gentity_t* ent, holdable_t type);
void ItemUse_UseEWeb(gentity_t* ent);
void G_PrecacheDispensers(void);

void ItemUse_Seeker(gentity_t* ent);
void ItemUse_MedPack(gentity_t* ent);
void ItemUse_MedPack_Big(gentity_t* ent);

void G_CheckTeamItems(void);
void G_RunItem(gentity_t* ent);
void RespawnItem(gentity_t* ent);

gentity_t* Drop_Item(gentity_t* ent, gitem_t* item, float angle);
gentity_t* LaunchItem(gitem_t* item, vec3_t origin, vec3_t velocity);
void G_SpawnItem(gentity_t* ent, gitem_t* item);
void FinishSpawningItem(gentity_t* ent);
void Add_Ammo(gentity_t* ent, ammo_t weapon, int count);
void Touch_Item(gentity_t* ent, gentity_t* other, trace_t* trace);

void ClearRegisteredItems(void);
void RegisterItem(gitem_t* item);
void SaveRegisteredItems(void);

//
// g_utils.cpp
//
team_t G_TeamFromKey(const char* key);
int G_ModelIndex(const char* name);
int G_SoundIndex(const char* name);
int G_SoundSetIndex(const char* name);
int G_EffectIndex(const char* name);
int G_BSPIndex(const char* name);
int G_IconIndex(const char* name);

bool G_PlayerHasCustomSkeleton(gentity_t* ent);

void G_TeamCommand(team_t team, char* cmd);
void G_ScaleNetHealth(gentity_t* self);
void G_KillBox(gentity_t* ent);
gentity_t* G_Find(gentity_t* from, int fieldofs, const char* match);
int G_RadiusList(vec3_t origin, float radius, gentity_t* ignore, bool takeDamage, gentity_t* ent_list[MAX_GENTITIES]);

void G_Throw(gentity_t* targ, vec3_t newDir, float push);

void G_FreeFakeClient(gclient_t** cl);
void G_CreateFakeClient(int entNum, gclient_t** cl);
void G_CleanAllFakeClients(void);

void G_SetAnim(gentity_t* ent, usercmd_t* ucmd, setAnimParts_t setAnimParts, animNumber_t anim, setAnimFlags_t setAnimFlags, int blendTime);
gentity_t* G_PickTarget(char* targetname);
void GlobalUse(gentity_t* self, gentity_t* other, gentity_t* activator);
void G_UseTargets2(gentity_t* ent, gentity_t* activator, const char* string);
void G_UseTargets(gentity_t* ent, gentity_t* activator);
void G_SetMovedir(vec3_t angles, vec3_t movedir);
void G_SetAngles(gentity_t* ent, vec3_t angles);

void G_InitGentity(gentity_t* e);
gentity_t* G_Spawn(void);
gentity_t* G_TempEntity(vec3_t origin, entity_event_t event);
gentity_t* G_PlayEffect(effectTypes_t fxID, vec3_t org, vec3_t ang);
gentity_t* G_PlayEffectID(const int fxID, vec3_t org, vec3_t ang);
gentity_t* G_ScreenShake(vec3_t org, gentity_t* target, float intensity, int duration, bool global);
void G_MuteSound(int entnum, soundChannel_t channel);
void G_Sound(gentity_t* ent, soundChannel_t channel, int soundIndex);
void G_Sound(gentity_t* ent, trackchan_t channel, int soundIndex);
void G_SoundAtLoc(vec3_t loc, soundChannel_t channel, int soundIndex);
void G_EntitySound(gentity_t* ent, soundChannel_t channel, int soundIndex);
void TryUse(gentity_t* ent);
void G_SendG2KillQueue(void);
void G_KillG2Queue(int entNum);
void G_FreeEntity(gentity_t* e);
bool G_EntitiesFree(void);

bool G_ActivateBehavior(gentity_t* self, bSet_t bset);

void G_TouchTriggers(gentity_t* ent);
void G_TouchSolids(gentity_t* ent);
void GetAnglesForDirection(const vec3_t p1, const vec3_t p2, vec3_t out);
bool G_AimSpot(const gentity_t* shooter, const gentity_t* target, vec3_t point);

//
// g_object.cpp
//

extern void G_RunObject(gentity_t* ent);

float* tv(float x, float y, float z);
char* vtos(const vec3_t v);

void G_AddPredictableEvent(gentity_t* ent, entity_event_t event, int eventParm);
void G_AddEvent(gentity_t* ent, entity_event_t event, int eventParm);
void G_SetOrigin(gentity_t* ent, vec3_t origin);
bool G_CheckInSolid(gentity_t* self, bool fix);
void AddRemap(const char* oldShader, const char* newShader, float timeOffset);
const char* BuildShaderStateConfig(void);
/*
Ghoul2 Insert Start
*/
int G_BoneIndex(const char* name);

/*
Ghoul2 Insert End
*/

//
// g_combat.cpp
//
bool CanDamage(gentity_t* targ, vec3_t origin);
void G_Damage(gentity_t* targ, gentity_t* inflictor, gentity_t* attacker, vec3_t dir, vec3_t point, int damage, damageFlags_t dflags, meansOfDeath_t mod);
bool G_RadiusDamage(vec3_t origin, gentity_t* attacker, float damage, float radius, gentity_t* ignore, gentity_t* missile, meansOfDeath_t mod);
void body_die(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, meansOfDeath_t meansOfDeath);
void TossClientWeapon(gentity_t* self, vec3_t direction, float speed);
void TossClientItems(gentity_t* self);
void TossClientCubes(gentity_t* self);
void ExplodeDeath(gentity_t* self);
void G_CheckForDismemberment(gentity_t* ent, gentity_t* enemy, vec3_t point, int damage, animNumber_t deathAnim, bool postDeath);
extern int gGAvoidDismember;

//
// g_exphysics.cpp
//
void G_RunExPhys(gentity_t* ent, float gravity, float mass, float bounce, bool autoKill, int* g2Bolts, int numG2Bolts);

//
// g_missile.cpp
//
void G_ReflectMissile(gentity_t* ent, gentity_t* missile, vec3_t forward);

void G_RunMissile(gentity_t* ent);

gentity_t* CreateMissile(vec3_t org, vec3_t dir, float vel, int life, gentity_t* owner, bool altFire);
void G_BounceProjectile(vec3_t start, vec3_t impact, vec3_t dir, vec3_t endout);
void G_ExplodeMissile(gentity_t* ent);

void WP_FireBlasterMissile(gentity_t* ent, vec3_t start, vec3_t dir, bool altFire);

//
// g_mover.cpp
//
extern int BMS_START;
extern int BMS_MID;
extern int BMS_END;

inline constexpr int SPF_BUTTON_FPUSHABLE = 2;
void G_PlayDoorLoopSound(gentity_t* ent);
void G_PlayDoorSound(gentity_t* ent, int type);
void G_RunMover(gentity_t* ent);
void Touch_DoorTrigger(gentity_t* ent, gentity_t* other, trace_t* trace);

//
// g_trigger.cpp
//
void trigger_teleporter_touch(gentity_t* self, gentity_t* other, trace_t* trace);

//
// g_misc.cpp
//
inline constexpr int MAX_REFNAME = 32;

inline constexpr int RTF_NAVGOAL = 0x00000001;

typedef struct reference_tag_s {
    char name[MAX_REFNAME];
    vec3_t origin;
    vec3_t angles;
    int flags;  // Just in case
    int radius; // For nav goals
    bool inuse;
} reference_tag_t;

void TAG_Init(void);
reference_tag_t* TAG_Find(const char* owner, const char* name);
reference_tag_t* TAG_Add(const char* name, const char* owner, vec3_t origin, vec3_t angles, int radius, int flags);
int TAG_GetOrigin(const char* owner, const char* name, vec3_t origin);
int TAG_GetOrigin2(const char* owner, const char* name, vec3_t origin);
int TAG_GetAngles(const char* owner, const char* name, vec3_t angles);
int TAG_GetRadius(const char* owner, const char* name);
int TAG_GetFlags(const char* owner, const char* name);

void TeleportPlayer(gentity_t* player, vec3_t origin, vec3_t angles);

//
// g_weapon.cpp
//
void WP_FireTurretMissile(gentity_t* ent, vec3_t start, vec3_t dir, bool altFire, int damage, int velocity, meansOfDeath_t mod, gentity_t* ignore);
void WP_FireGenericBlasterMissile(gentity_t* ent, vec3_t start, vec3_t dir, bool altFire, int damage, int velocity, meansOfDeath_t mod);
bool LogAccuracyHit(gentity_t* target, gentity_t* attacker);
void CalcMuzzlePoint(gentity_t* ent, const vec3_t inForward, const vec3_t inRight, const vec3_t inUp, vec3_t muzzlePoint);
bool CheckGauntletAttack(gentity_t* ent);

//
// g_client.cpp
//
int TeamCount(int ignoreClientNum, team_t team);
int TeamLeader(team_t team);
team_t PickTeam(int ignoreClientNum);
void SetClientViewAngle(gentity_t* ent, vec3_t angle);
gentity_t* SelectSpawnPoint(vec3_t avoidPoint, vec3_t origin, vec3_t angles, team_t team, bool isbot);
void MaintainBodyQueue(gentity_t* ent);
void ClientRespawn(gentity_t* ent);
void BeginIntermission(void);
void InitBodyQue(void);
void ClientSpawn(gentity_t* ent);
void player_die(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, meansOfDeath_t mod);
void AddScore(gentity_t* ent, vec3_t origin, int score);
void CalculateRanks(void);
bool SpotWouldTelefrag(gentity_t* spot);

extern gentity_t* gJMSaberEnt;

//
// g_svcmds.cpp
//
bool ConsoleCommand(void);
void G_ProcessIPBans(void);
bool G_FilterPacket(const char* from);

//
// g_weapon.cpp
//
void FireWeapon(gentity_t* ent, bool altFire);
void BlowDetpacks(gentity_t* ent);
void RemoveDetpacks(gentity_t* ent);

//
// p_hud.c
//
void MoveClientToIntermission(gentity_t* client);
void G_SetStats(gentity_t* ent);
void DeathmatchScoreboardMessage(gentity_t* client);

//
// g_cmds.cpp
//

//
// g_pweapon.c
//

//
// g_main.cpp
//
extern vmCvar_t g_ff_objectives;
extern bool gDoSlowMoDuel;
extern int gSlowMoDuelTime;

void G_PowerDuelCount(int* loners, int* doubles, bool countSpec);

void FindIntermissionPoint(void);
void SetLeader(team_t team, int client);
void CheckTeamLeader(team_t team);
void G_RunThink(gentity_t* ent);
void AddTournamentQueue(gclient_t* client);
void QDECL G_LogPrintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void QDECL G_SecurityLogPrintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void SendScoreboardMessageToAllClients(void);
const char* G_GetStringEdString(const char* refSection, const char* refName);

//
// g_client.cpp
//
const char* ClientConnect(int clientNum, bool firstTime, bool isBot);
bool ClientUserinfoChanged(int clientNum);
void ClientDisconnect(int clientNum);
void ClientBegin(int clientNum, bool allowTeamReset);
void G_BreakArm(gentity_t* ent, brokenLimb_t arm);
void G_UpdateClientAnims(gentity_t* self, float animSpeedScale);
void ClientCommand(int clientNum);
void G_ClearVote(gentity_t* ent);
void G_ClearTeamVote(gentity_t* ent, team_t team);

//
// g_active.cpp
//
void G_CheckClientTimeouts(gentity_t* ent);
void ClientThink(int clientNum, usercmd_t* ucmd);
void ClientEndFrame(gentity_t* ent);

//
// g_team.cpp
//
bool OnSameTeam(gentity_t* ent1, gentity_t* ent2);
void Team_CheckDroppedItem(gentity_t* dropped);

//
// g_mem.cpp
//
void* G_Alloc(int size);
void G_InitMemory(void);
void Svcmd_GameMem_f(void);

//
// g_session.cpp
//
void G_ReadSessionData(gclient_t* client);
void G_InitSessionData(gclient_t* client, char* userinfo, bool isBot);

void G_InitWorldSession(void);
void G_WriteSessionData(void);

//
// NPC_senses.cpp
//
extern void AddSightEvent(gentity_t* owner, vec3_t position, float radius, alertEventLevel_e alertLevel, float addLight); // addLight = 0.0f
extern void AddSoundEvent(gentity_t* owner, vec3_t position, float radius, alertEventLevel_e alertLevel, bool needLOS);   // needLOS = false
extern bool G_CheckForDanger(gentity_t* self, int alertEvent);
extern int G_CheckAlertEvents(gentity_t* self, bool checkSight, bool checkSound, float maxSeeDist, float maxHearDist, int ignoreAlert, bool mustHaveOwner,
                              alertEventLevel_e minAlertLevel); // ignoreAlert = -1, mustHaveOwner = false, minAlertLevel = alertEventLevel_e::AEL_MINOR
extern bool G_CheckForDanger(gentity_t* self, int alertEvent);
extern bool G_ClearLOS(gentity_t* self, const vec3_t start, const vec3_t end);
extern bool G_ClearLOS2(gentity_t* self, gentity_t* ent, const vec3_t end);
extern bool G_ClearLOS3(gentity_t* self, const vec3_t start, gentity_t* ent);
extern bool G_ClearLOS4(gentity_t* self, gentity_t* ent);
extern bool G_ClearLOS5(gentity_t* self, const vec3_t end);

//
// g_bot.cpp
//
void G_InitBots(void);
char* G_GetBotInfoByNumber(int num);
char* G_GetBotInfoByName(const char* name);
void G_CheckBotSpawn(void);
void G_RemoveQueuedBotBegin(int clientNum);
bool G_BotConnect(int clientNum, bool restart);
void Svcmd_AddBot_f(void);
void Svcmd_BotList_f(void);
void BotInterbreedEndMatch(void);
bool G_DoesMapSupportGametype(const char* mapname, gametype_t gametype);
const char* G_RefreshNextMap(gametype_t gametype, bool forced);
void G_LoadArenas(void);

// w_force.cpp / w_saber.cpp
gentity_t* G_PreDefSound(vec3_t org, pdSounds_t pdSound);
bool HasSetSaberOnly(void);
void WP_ForcePowerStop(gentity_t* self, forcePowers_t forcePower);
void WP_SaberPositionUpdate(gentity_t* self, usercmd_t* ucmd);
int WP_SaberCanBlock(gentity_t* self, vec3_t point, damageFlags_t dflags, meansOfDeath_t mod, bool projectile, int attackStr);
void WP_SaberInitBladeData(gentity_t* ent);
void WP_InitForcePowers(gentity_t* ent);
void WP_SpawnInitForcePowers(gentity_t* ent);
void WP_ForcePowersUpdate(gentity_t* self, usercmd_t* ucmd);
int ForcePowerUsableOn(gentity_t* attacker, gentity_t* other, forcePowers_t forcePower);
void ForceHeal(gentity_t* self);
void ForceSpeed(gentity_t* self, int forceDuration);
void ForceRage(gentity_t* self);
void ForceGrip(gentity_t* self);
void ForceProtect(gentity_t* self);
void ForceAbsorb(gentity_t* self);
void ForceTeamHeal(gentity_t* self);
void ForceTeamForceReplenish(gentity_t* self);
void ForceSeeing(gentity_t* self);
void ForceThrow(gentity_t* self, bool pull);
void ForceTelepathy(gentity_t* self);
bool Jedi_DodgeEvasion(gentity_t* self, gentity_t* shooter, trace_t* tr, hitLocation_t hitLoc);

// g_log.cpp
void QDECL G_LogWeaponPickup(int client, weapon_t weaponid);
void QDECL G_LogWeaponFire(int client, weapon_t weaponid);
void QDECL G_LogWeaponDamage(int client, meansOfDeath_t mod, int amount);
void QDECL G_LogWeaponKill(int client, meansOfDeath_t mod);
void QDECL G_LogWeaponDeath(int client, weapon_t weaponid);
void QDECL G_LogWeaponFrag(int attacker, int deadguy);
void QDECL G_LogWeaponPowerup(int client, powerup_t powerupid);
void QDECL G_LogWeaponItem(int client, holdable_t itemid);
void QDECL G_LogWeaponInit(void);
void QDECL G_LogWeaponOutput(void);
void QDECL G_LogExit(const char* string);
void QDECL G_ClearClientLog(int client);

// g_siege.c
void InitSiegeMode(void);
void G_SiegeClientExData(gentity_t* msgTarg);

// g_timer
// Timing information
void TIMER_Clear(void);
void TIMER_Clear2(gentity_t* ent);
void TIMER_Set(gentity_t* ent, const char* identifier, int duration);
int TIMER_Get(gentity_t* ent, const char* identifier);
bool TIMER_Done(gentity_t* ent, const char* identifier);
bool TIMER_Start(gentity_t* self, const char* identifier, int duration);
bool TIMER_Done2(gentity_t* ent, const char* identifier, bool remove);
bool TIMER_Exists(gentity_t* ent, const char* identifier);
void TIMER_Remove(gentity_t* ent, const char* identifier);

float NPC_GetHFOVPercentage(vec3_t spot, vec3_t from, vec3_t facing, float hFOV);
float NPC_GetVFOVPercentage(vec3_t spot, vec3_t from, vec3_t facing, float vFOV);

// NPC_stats.cpp
std::optional<weapon_t> G_WeaponFromName(const char* name);

extern void G_SetEnemy(gentity_t* self, gentity_t* enemy);
bool InFront(vec3_t spot, vec3_t from, vec3_t fromAngles, float threshHold);

// ai_main.cpp
inline constexpr int MAX_FILEPATH = 144;

int OrgVisible(vec3_t org1, vec3_t org2, int ignore);
void BotOrder(gentity_t* ent, int clientnum, int ordernum);
int InFieldOfVision(vec3_t viewangles, float fov, vec3_t angles);

// ai_util.cpp
void B_InitAlloc(void);
void B_CleanupAlloc(void);

// bot settings
typedef struct bot_settings_s {
    char personalityfile[MAX_FILEPATH];
    float skill;
    char team[MAX_FILEPATH];
} bot_settings_t;

int BotAISetup(int restart);
int BotAIShutdown(int restart);
int BotAILoadMap(int restart);
int BotAISetupClient(int client, struct bot_settings_s* settings, bool restart);
int BotAIShutdownClient(int client, bool restart);
int BotAIStartFrame(int time);

#include "g_team.h" // teamplay specific stuff

extern level_locals_t level;
extern gentity_t g_entities[MAX_GENTITIES];

// Combat statistics since the last "gamestats" server command (plan step 14): the game must play the same at any server
// frame rate, so the hits, blocks and damage per saber swing are compared at 20 Hz and at the tick's rate.
typedef struct gameStats_s {
    int frames;
    int startTime;     // level.time of the first frame counted
    int saberSwings;   // saber attack moves started by clients
    int saberWounds;   // saber wounds of more than idle damage on clients
    int saberTouches;  // idle saber touches (SABER_NONATTACK_DAMAGE) on clients
    int saberDamage;   // damage of both, before armour
    int saberBlocks;   // blade on a client's body parried without the sabers touching (WP_SaberCanBlock)
    int saberClashes;  // saber against saber: the blades touch (a block)
    int saberLocks;    // saber locks started
    int missileBlocks; // projectiles and disruptor shots blocked by a saber
    int forceDamage;   // health taken by the dark force powers
    int deaths;
    int saberHitEffects;                                                        // hit effects (blood or sparks, and their sound) of sabers on clients
    EnumArray<int, forcePowers_t, forcePowers_t::NUM_FORCE_POWERS> forceStarts; // times each force power was started (WP_ForcePowerStart)
    EnumArray<int, forcePowers_t, forcePowers_t::NUM_FORCE_POWERS>
        forceTimerEvents; // periodic events of the held powers: grip, protect and absorb drains, rage health losses, lightning and drain shots
    EnumArray<int, forcePowers_t, forcePowers_t::NUM_FORCE_POWERS> forceActiveMsec; // time each power was held, 50 ms per 20 Hz instant it was active
    EnumArray<int, entity_event_t, entity_event_t::EV_NUM_ENTITY_EVENTS> events; // events and temporary entities added by the game (G_AddEvent, G_TempEntity)
    EnumArray<int, meansOfDeath_t, meansOfDeath_t::MOD_MAX> modDamage;           // health taken from clients, by cause
    EnumArray<int, meansOfDeath_t, meansOfDeath_t::MOD_MAX> modKills;            // client deaths, by cause
    EnumArray<int[2], weapon_t, weapon_t::WP_NUM_WEAPONS> shots;                 // FireWeapon by weapon, primary and alternate
    EnumArray<int, saber_styles_t, saber_styles_t::SS_NUM_SABER_STYLES> styleAttacks; // saber attack moves started by clients, by style
    EnumArray<int, saber_styles_t, saber_styles_t::SS_NUM_SABER_STYLES> styleBounces; // saber moves of clients sent into a bounce or deflection, by style
    EnumArray<int, saberMoveName_t, saberMoveName_t::LS_MOVE_MAX>
        specialMoves;                                                     // special saber moves started by clients (back attacks to the hilt bash), by move
    int itemPickups[MAX_ITEMS];                                           // items picked up (Touch_Item), by index in bg_itemlist
    EnumArray<int, meansOfDeath_t, meansOfDeath_t::MOD_MAX> objectDamage; // health taken from what is not a client (a shield, a sentry, an E-Web), by cause
    EnumArray<int, meansOfDeath_t, meansOfDeath_t::MOD_MAX> npcDamage;    // health taken from the NPCs and vehicles, clients counted in modDamage too, by cause
} gameStats_t;
extern gameStats_t g_stats;
void G_StatsSaberMove(gentity_t* ent, saberMoveName_t oldSaberMove);

// g_scenario.cpp: scripted bot scenarios for the measurements of plan step 14
bool G_ScenarioControls(int clientNum);
bool G_ScenarioCommand(int clientNum, int t, usercmd_t* ucmd);
void G_ScenarioFrame(void);
void G_ScenarioTrace(void);
void G_ScenarioDamageTrace(gentity_t* targ, gentity_t* inflictor, const vec3_t point, int take, meansOfDeath_t mod);
void G_ScenarioHitLocation(const gentity_t* targ, hitLocation_t hitLoc);
void G_ScenarioPrintStats(void);
void Svcmd_Scenario_f(void);

#define FOFS(x) offsetof(gentity_t, x)

// userinfo validation bitflags
// default is all except extended ascii
// numUserinfoFields + USERINFO_VALIDATION_MAX should not exceed 31
enum class userinfoValidationBits_t {
    // validation & (1<<(numUserinfoFields+USERINFO_VALIDATION_BLAH))
    USERINFO_VALIDATION_SIZE = 0,
    USERINFO_VALIDATION_SLASH,
    USERINFO_VALIDATION_EXTASCII,
    USERINFO_VALIDATION_CONTROLCHARS,
    USERINFO_VALIDATION_MAX
};

void Svcmd_ToggleUserinfoValidation_f(void);
void Svcmd_ToggleAllowVote_f(void);

// g_cvar.cpp
#define XCVAR_PROTO
#include "g_xcvar.h"
#undef XCVAR_PROTO
void G_RegisterCvars(void);
void G_UpdateCvars(void);
// the dmflags cvar's flags, the one conversion of its number
[[nodiscard]] inline dmFlags_t G_DmFlags(void)
{
    return static_cast<dmFlags_t>(dmflags.integer);
}

extern gameImport_t* trap;

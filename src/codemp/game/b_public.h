/*
===========================================================================
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
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

#include "ai.h"

enum class npcAiFlags_t {
    NPCAI_NONE = 0,
    NPCAI_CHECK_WEAPON = 0x00000001,
    NPCAI_BURST_WEAPON = 0x00000002,
    NPCAI_MOVING = 0x00000004,
    NPCAI_TOUCHED_GOAL = 0x00000008,
    NPCAI_PUSHED = 0x00000010,
    NPCAI_NO_COLL_AVOID = 0x00000020,
    NPCAI_BLOCKED = 0x00000040,
    NPCAI_OFF_PATH = 0x00000100,
    NPCAI_IN_SQUADPOINT = 0x00000200,
    NPCAI_STRAIGHT_TO_DESTPOS = 0x00000400,
    NPCAI_NO_SLOWDOWN = 0x00001000,
    NPCAI_LOST = 0x00002000,              // Can't nav to his goal
    NPCAI_SHIELDS = 0x00004000,           // Has shields, borg can adapt
    NPCAI_GREET_ALLIES = 0x00008000,      // Say hi to nearby allies
    NPCAI_FORM_TELE_NAV = 0x00010000,     // Tells formation people to use nav info to get to
    NPCAI_ENROUTE_TO_HOMEWP = 0x00020000, // Lets us know to run our lostenemyscript when we get to homeWp
    NPCAI_MATCHPLAYERWEAPON = 0x00040000, // Match the player's weapon except when it changes during cinematics
    NPCAI_DIE_ON_IMPACT = 0x00100000,     // Next time you crashland, die!
    NPCAI_CUSTOM_GRAVITY = 0x00200000,    // Don't use g_gravity, I fly!
};
template <> inline constexpr bool enableEnumFlags<npcAiFlags_t> = true;

// Script flags
enum class scriptFlags_t {
    SCF_NONE = 0,
    SCF_CROUCHED = 0x00000001,         // Force ucmd.upmove to be -127
    SCF_WALKING = 0x00000002,          // Force BUTTON_WALKING to be pressed
    SCF_MORELIGHT = 0x00000004,        // NPC will have a minlight of 96
    SCF_LEAN_RIGHT = 0x00000008,       // Force rightmove+BUTTON_USE
    SCF_LEAN_LEFT = 0x00000010,        // Force leftmove+BUTTON_USE
    SCF_RUNNING = 0x00000020,          // Takes off walking button, overrides SCF_WALKING
    SCF_ALT_FIRE = 0x00000040,         // Force to use alt-fire when firing
    SCF_NO_RESPONSE = 0x00000080,      // NPC will not do generic responses to being used
    SCF_FFDEATH = 0x00000100,          // Just tells player_die to run the friendly fire deathscript
    SCF_NO_COMBAT_TALK = 0x00000200,   // NPC will not use their generic combat chatter stuff
    SCF_CHASE_ENEMIES = 0x00000400,    // NPC chase enemies - FIXME: right now this is synonymous with using combat points... should it be?
    SCF_LOOK_FOR_ENEMIES = 0x00000800, // NPC be on the lookout for enemies
    SCF_FACE_MOVE_DIR = 0x00001000,    // NPC face direction it's moving - FIXME: not really implemented right now
    SCF_IGNORE_ALERTS = 0x00002000,    // NPC ignore alert events
    SCF_DONT_FIRE = 0x00004000,        // NPC won't shoot
    SCF_DONT_FLEE = 0x00008000,        // NPC never flees
    SCF_FORCED_MARCH = 0x00010000,     // NPC that the player must aim at to make him walk
    SCF_NO_GROUPS = 0x00020000,        // NPC cannot alert groups or be part of a group
    SCF_FIRE_WEAPON = 0x00040000,      // NPC will fire his (her) weapon
    SCF_NO_MIND_TRICK = 0x00080000,    // Not succeptible to mind tricks
    SCF_USE_CP_NEAREST = 0x00100000,   // Will use combat point close to it, not next to player or try and flank player
    SCF_NO_FORCE = 0x00200000,         // Not succeptible to force powers
    SCF_NO_FALLTODEATH = 0x00400000,   // NPC will not scream and tumble and fall to hit death over large drops
    SCF_NO_ACROBATICS = 0x00800000,    // Jedi won't jump, roll or cartwheel
    SCF_USE_SUBTITLES = 0x01000000,    // Regardless of subtitle setting, this NPC will display subtitles when it speaks lines
    SCF_NO_ALERT_TALK = 0x02000000,    // Will not say alert sounds, but still can be woken up by alerts
};
template <> inline constexpr bool enableEnumFlags<scriptFlags_t> = true;

enum class visibility_t { VIS_UNKNOWN, VIS_NOT, VIS_PVS, VIS_360, VIS_FOV, VIS_SHOOT };
enum class spot_t { SPOT_ORIGIN, SPOT_CHEST, SPOT_HEAD, SPOT_HEAD_LEAN, SPOT_WEAPON, SPOT_LEGS, SPOT_GROUND };

enum class lookMode_t // # lookMode_e
{
    LM_ENT = 0,
    LM_INTEREST
};

enum class jumpState_t // # jumpState_e
{
    JS_WAITING = 0,
    JS_FACING,
    JS_CROUCHING,
    JS_JUMPING,
    JS_LANDING
};

typedef struct gNPCstats_e { // Stats, loaded in, and can be set by scripts
    // AI
    int aggression;      //			"
    int aim;             //			"
    float earshot;       //			"
    int evasion;         //			"
    int hfov;            // horizontal field of view
    int intelligence;    //			"
    int move;            //			"
    int reactions;       // 1-5, higher is better
    float shootDistance; // Maximum range- overrides range set for weapon if nonzero
    int vfov;            // vertical field of view
    float vigilance;     //			"
    float visrange;      //			"
    // Movement
    int runSpeed;
    int walkSpeed;
    float yawSpeed; // 1 - whatever, default is 50
    int health;
    int acceleration;
} gNPCstats_t;

// NOTE!!!  If you add any ptr fields into this structure could you please tell me so I can update the load/save code?
//	so far the only things I've got to cope with are a bunch of gentity_t*'s, but tell me if any more get added -slc
//

inline constexpr int MAX_ENEMY_POS_LAG = 2400;
inline constexpr int ENEMY_POS_LAG_INTERVAL = 100;
inline constexpr int ENEMY_POS_LAG_STEPS = MAX_ENEMY_POS_LAG / ENEMY_POS_LAG_INTERVAL;
typedef struct {
    // FIXME: Put in playerInfo or something
    int timeOfDeath; // FIXME do we really need both of these
    gentity_t* touchedByPlayer;

    visibility_t enemyLastVisibility;

    int aimTime;
    float desiredYaw;
    float desiredPitch;
    float lockedDesiredYaw;
    float lockedDesiredPitch;
    gentity_t* aimingBeam; // debugging aid

    vec3_t enemyLastSeenLocation;
    int enemyLastSeenTime;
    vec3_t enemyLastHeardLocation;
    int enemyLastHeardTime;
    int lastAlertID; // unique ID

    int eFlags;
    npcAiFlags_t aiFlags;

    int currentAmmo; // this sucks, need to find a better way
    int shotTime;
    int burstCount;
    int burstMin;
    int burstMean;
    int burstMax;
    int burstSpacing;
    int attackHold;
    int attackHoldTime;
    vec3_t shootAngles; // Angles to where bot is shooting - fixme: make he torso turn to reflect these

    // extra character info
    rank_t rank; // for pips

    // Behavior state info
    bState_t behaviorState;   // determines what actions he should be doing
    bState_t defaultBehavior; // State bot will default to if none other set
    bState_t tempBehavior;    // While valid, overrides other behavior

    bool ignorePain; // only play pain scripts when take pain

    int duckDebounceTime; // Keeps them ducked for a certain time
    int walkDebounceTime;
    int enemyCheckDebounceTime;
    int investigateDebounceTime;
    int investigateCount;
    vec3_t investigateGoal;
    int investigateSoundDebounceTime;
    int greetingDebounceTime; // when we can greet someone next
    gentity_t* eventOwner;

    // bState-specific fields
    gentity_t* coverTarg;
    jumpState_t jumpState;
    float followDist;

    // goal, navigation & pathfinding
    gentity_t* tempGoal; // used for locational goals (player's last seen/heard position)
    gentity_t* goalEntity;
    gentity_t* lastGoalEntity;
    gentity_t* eventualGoal;
    gentity_t* captureGoal; // Where we should try to capture
    gentity_t* defendEnt;   // Who we're trying to protect
    gentity_t* greetEnt;    // Who we're greeting
    int goalTime;           // FIXME: This is never actually used
    bool straightToGoal;    // move straight at navgoals
    float distToGoal;
    int navTime;
    int blockingEntNum;
    int blockedSpeechDebounceTime;
    int lastSideStepSide;
    int sideStepHoldTime;
    int homeWp;
    AIGroupInfo_t* group;

    vec3_t lastPathAngles; // So we know which way to face generally when we stop

    // stats
    gNPCstats_t stats;
    int aimErrorDebounceTime;
    float lastAimErrorYaw;
    float lastAimErrorPitch;
    vec3_t aimOfs;
    int currentAim;
    int currentAggression;

    // scriptflags
    scriptFlags_t scriptFlags; // in b_local.h

    // moveInfo
    int desiredSpeed;
    int currentSpeed;
    char last_forwardmove;
    char last_rightmove;
    vec3_t lastClearOrigin;
    int consecutiveBlockedMoves;
    int blockedDebounceTime;
    int shoveCount;
    vec3_t blockedDest;

    //
    int combatPoint;             // NPCs in bState BS_COMBAT_POINT will find their closest empty combat_point
    int lastFailedCombatPoint;   // NPCs in bState BS_COMBAT_POINT will find their closest empty combat_point
    speechType_t movementSpeech; // what to say when you first successfully move
    float movementSpeechChance;  // how likely you are to say it
    int angerTaunts;             // Galak's anger taunts said so far, 0 to 3

    // Testing physics at 20fps
    int nextBStateThink;
    usercmd_t last_ucmd;

    //
    // JWEIER ADDITIONS START

    bool combatMove;
    int goalRadius;

    // FIXME: These may be redundant

    /*
    int			weaponTime;		//Time until refire is valid
    int			jumpTime;
    */
    int pauseTime; // Time to stand still
    int standTime;

    int localState;          // Tracking information local to entity
    squadState_t squadState; // Tracking information for team level interaction

    // JWEIER ADDITIONS END
    //

    int confusionTime;  // Doesn't respond to alerts or pick up enemies (unless shot) until this time is up
    int charmedTime;    // charmed to enemy team
    int controlledTime; // controlled by player
    int surrenderTime;  // Hands up

    // Lagging enemy position - FIXME: seems awful wasteful...
    vec3_t enemyLaggedPos[ENEMY_POS_LAG_STEPS];

    gentity_t* watchTarget; // for BS_CINEMATIC, keeps facing this ent

    int ffireCount; // sigh... you'd think I'd be able to find a way to do this without having to use 3 int fields, but...
    int ffireDebounce;
    int ffireFadeDebounce;
} gNPC_t;

// gNPC_t::localState holds the set of the NPC's AI file (droidLocalState_t, sentryLocalState_t, ...), named by the
// caller: the one conversion of the field
template <typename E>
    requires std::is_enum_v<E>
[[nodiscard]] E NPC_LocalState(const gNPC_t& npc)
{
    return static_cast<E>(npc.localState);
}
template <typename E>
    requires std::is_enum_v<E>
void NPC_SetLocalState(gNPC_t& npc, E state)
{
    npc.localState = to_underlying(state);
}

void G_SquadPathsInit(void);
void NPC_InitGame(void);
void G_LoadBoltOns(void);
void Svcmd_NPC_f(void);
void NAV_DebugShowWaypoints(void);
void NAV_DebugShowBoxes(void);
void NAV_DebugShowSquadPaths(void);

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

// B_local.h
// re-added by MCG
#ifndef __B_LOCAL_H__
#define __B_LOCAL_H__

#include "g_local.h"
#include "say.h"

#include "ai.h"

#define AI_TIMERS 0 // turn on to see print-outs of AI/nav timing
//
// Navigation susbsystem
//

#include "qcommon/q_gamesets.h"

inline constexpr float MIN_ANGLE_ERROR = 0.01f;

inline constexpr int MIN_ROCKET_DIST_SQUARED = 16384; // 128*128
//
// NPC.cpp
//
// ai debug cvars
void SetNPCGlobals(gentity_t* ent);
void SaveNPCGlobals();
void RestoreNPCGlobals();
extern cvar_t* debugNPCAI;     // used to print out debug info about the NPC AI
extern cvar_t* debugNPCFreeze; // set to disable NPC ai and temporarily freeze them in place
extern cvar_t* debugNPCName;
extern cvar_t* d_JediAI;
extern cvar_t* d_saberCombat;
extern void NPC_Think(gentity_t* self);
extern void pitch_roll_for_slope(gentity_t* forwhom, vec3_t pass_slope = nullptr, vec3_t storeAngles = nullptr, bool keepPitch = false);

// NPC_reactions.cpp
extern void NPC_Pain(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, vec3_t point, int damage, meansOfDeath_t mod, hitLocation_t hitLoc);
extern void NPC_Touch(gentity_t* self, gentity_t* other, trace_t* trace);
extern void NPC_Use(gentity_t* self, gentity_t* other, gentity_t* activator);
extern float NPC_GetPainChance(gentity_t* self, int damage);

//
// NPC_misc.cpp
//
extern void Debug_Printf(cvar_t* cv, aiDebugLevel_t level, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
extern void Debug_NPCPrintf(gentity_t* printNPC, cvar_t* cv, aiDebugLevel_t debugLevel, char* fmt, ...);

// MCG - Begin============================================================
// NPC_ai variables - shared by NPC.cpp andf the following modules
extern gentity_t* NPC;
extern gNPC_t* NPCInfo;
extern gclient_t* client;
extern usercmd_t ucmd;
extern visibility_t enemyVisibility;

// AI_Default
extern bool NPC_CheckInvestigate(int alertEventNum);
extern bool NPC_StandTrackAndShoot(gentity_t* NPC);
extern void NPC_BSIdle(void);
extern void NPC_BSPointShoot(bool shoot);
extern void NPC_BSStandGuard(void);
extern void NPC_BSPatrol(void);
extern void NPC_BSHuntAndKill(void);
extern void NPC_BSStandAndShoot(void);
extern void NPC_BSRunAndShoot(void);
extern void NPC_BSWait(void);
extern void NPC_BSDefault(void);

// NPC_behavior
extern void NPC_BSAdvanceFight(void);
extern void NPC_BSInvestigate(void);
extern void NPC_BSSleep(void);
extern void NPC_BSFollowLeader(void);
extern void NPC_BSJump(void);
extern void NPC_BSRemove(void);
extern void NPC_BSSearch(void);
extern void NPC_BSSearchStart(int homeWp, bState_t bState);
extern void NPC_BSWander(void);
extern bool NPC_BSFlee(void);
extern void NPC_StartFlee(gentity_t* enemy, vec3_t dangerPoint, alertEventLevel_e dangerLevel, int fleeTimeMin, int fleeTimeMax);
extern void G_StartFlee(gentity_t* self, gentity_t* enemy, vec3_t dangerPoint, alertEventLevel_e dangerLevel, int fleeTimeMin, int fleeTimeMax);

// NPC_combat
extern weapon_t ChooseBestWeapon(void);
extern void NPC_ChangeWeapon(weapon_t newWeapon);
extern void ShootThink(void);
extern void WeaponThink(bool inCombat);
extern bool HaveWeapon(weapon_t weapon);
extern bool CanShoot(gentity_t* ent, gentity_t* shooter);
extern void NPC_CheckPossibleEnemy(gentity_t* other, visibility_t vis);
extern gentity_t* NPC_PickEnemy(gentity_t* closestTo, team_t enemyTeam, bool checkVis, bool findPlayersFirst, bool findClosest);
extern gentity_t* NPC_CheckEnemy(bool findNew, bool tooFarOk, bool setEnemy = true);
extern bool NPC_CheckAttack(float scale);
extern bool NPC_CheckDefend(float scale);
extern bool NPC_CheckCanAttack(float attack_scale, bool stationary);
extern int NPC_AttackDebounceForWeapon(void);
extern bool EntIsGlass(gentity_t* check);
extern bool ShotThroughGlass(trace_t* tr, gentity_t* target, vec3_t spot, contentsFlags_t mask);
extern void G_ClearEnemy(gentity_t* self);
extern void G_SetEnemy(gentity_t* self, gentity_t* enemy);
extern gentity_t* NPC_PickAlly(bool facingEachOther, float range, bool ignoreGroup, bool movingOnly);
extern void NPC_LostEnemyDecideChase(void);
extern float NPC_MaxDistSquaredForWeapon(void);
extern bool NPC_EvaluateShot(int hit, bool glassOK);
extern int NPC_ShotEntity(gentity_t* ent, vec3_t impactPos = nullptr);

// NPC_formation
extern bool NPC_SlideMoveToGoal(void);
extern float NPC_FindClosestTeammate(gentity_t* self);
extern void NPC_CalcClosestFormationSpot(gentity_t* self);
extern void G_MaintainFormations(gentity_t* self);
extern void NPC_BSFormation(void);
extern void NPC_CreateFormation(gentity_t* self);
extern void NPC_DropFormation(gentity_t* self);
extern void NPC_ReorderFormation(gentity_t* self);
extern void NPC_InsertIntoFormation(gentity_t* self);
extern void NPC_DeleteFromFormation(gentity_t* self);

// NPC spawnflags
inline constexpr int SFB_RIFLEMAN = 2;
inline constexpr int SFB_PHASER = 4;

inline constexpr int SFB_CINEMATIC = 32;
inline constexpr int SFB_NOTSOLID = 64;
inline constexpr int SFB_STARTINSOLID = 128;

// NPC_goal
extern void SetGoal(gentity_t* goal, float rating);
extern void NPC_SetGoal(gentity_t* goal, float rating);
extern void NPC_ClearGoal(void);
extern void NPC_ReachedGoal(void);
extern bool ReachedGoal(gentity_t* goal);
extern gentity_t* UpdateGoal(void);
extern bool NPC_MoveToGoal(bool tryStraight);

// NPC_move
bool NPC_Jumping(void);
bool NPC_JumpBackingUp(void);

bool NPC_TryJump(gentity_t* goal, float max_xy_dist = 0.0f, float max_z_diff = 0.0f);
bool NPC_TryJump(const vec3_t& pos, float max_xy_dist = 0.0f, float max_z_diff = 0.0f);

// NPC_reactions

// NPC_senses
inline constexpr int ALERT_CLEAR_TIME = 200;
#include "qcommon/q_npcflags.h"
extern bool CanSee(gentity_t* ent);
extern bool InFOV(gentity_t* ent, gentity_t* from, int hFOV, int vFOV);
extern bool InFOV(vec3_t origin, gentity_t* from, int hFOV, int vFOV);
extern bool InFOV(vec3_t spot, vec3_t from, vec3_t fromAngles, int hFOV, int vFOV);
extern visibility_t NPC_CheckVisibility(gentity_t* ent, visCheckFlags_t flags);
extern bool InVisrange(gentity_t* ent);

// NPC_spawn
extern void NPC_Spawn(gentity_t* self);

// NPC_stats
extern int NPC_ReactionTime(void);
extern bool NPC_ParseParms(const char* NPCName, gentity_t* NPC);
extern void NPC_LoadParms(void);

// NPC_utils
extern EnumArray<int, team_t, team_t::TEAM_NUM_TEAMS> teamNumbers;
extern EnumArray<int, team_t, team_t::TEAM_NUM_TEAMS> teamStrength;
extern EnumArray<int, team_t, team_t::TEAM_NUM_TEAMS> teamCounter;
extern void CalcEntitySpot(const gentity_t* ent, const spot_t spot, vec3_t point);
extern bool NPC_UpdateAngles(bool doPitch, bool doYaw);
extern void NPC_UpdateShootAngles(vec3_t angles, bool doPitch, bool doYaw);
extern bool NPC_UpdateFiringAngles(bool doPitch, bool doYaw);
extern void SetTeamNumbers(void);
extern bool G_ActivateBehavior(gentity_t* self, bSet_t bset);
extern void NPC_AimWiggle(vec3_t enemy_org);
extern void NPC_SetLookTarget(gentity_t* self, int entNum, int clearTime);

// other modules
extern void CalcMuzzlePoint(gentity_t* const ent, vec3_t forward, vec3_t right, vec3_t up, vec3_t muzzlePoint, float lead_in);

// g_combat
extern void ExplodeDeath(gentity_t* self);
extern void ExplodeDeath_Wait(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, meansOfDeath_t meansOfDeath, damageFlags_t dFlags,
                              hitLocation_t hitLoc);
extern void GoExplodeDeath(gentity_t* self, gentity_t* other, gentity_t* activator);
extern float IdealDistance(gentity_t* self);

// g_client
extern bool SpotWouldTelefrag(gentity_t* spot, team_t checkteam);

// g_squad
extern void NPC_SetSayState(gentity_t* self, gentity_t* to, int saying);

// g_utils
extern bool G_CheckInSolid(gentity_t* self, bool fix);
extern bool infront(gentity_t* from, gentity_t* to);

// MCG - End============================================================

// NPC.cpp
extern void NPC_SetAnim(gentity_t* ent, setAnimParts_t setAnimParts, animNumber_t anim, setAnimFlags_t setAnimFlags, int iBlend = SETANIM_BLEND_DEFAULT);
extern bool NPC_EnemyTooFar(gentity_t* enemy, float dist, bool toShoot);

// ==================================================================

inline bool NPC_ClearLOS(const vec3_t start, const vec3_t end)
{
    return G_ClearLOS(NPC, start, end);
}
inline bool NPC_ClearLOS(const vec3_t end)
{
    return G_ClearLOS(NPC, end);
}
inline bool NPC_ClearLOS(gentity_t* ent)
{
    return G_ClearLOS(NPC, ent);
}
inline bool NPC_ClearLOS(const vec3_t start, gentity_t* ent)
{
    return G_ClearLOS(NPC, start, ent);
}
inline bool NPC_ClearLOS(gentity_t* ent, const vec3_t end)
{
    return G_ClearLOS(NPC, ent, end);
}

extern bool NPC_ClearShot(gentity_t* ent);

// What NPC_FindCombatPoint looks for
enum class combatPointSearchFlags_t {
    CP_ANY = 0,                     // No flags
    CP_COVER = 0x00000001,          // The enemy cannot currently shoot this position
    CP_CLEAR = 0x00000002,          // This cover point has a clear shot to the enemy
    CP_FLEE = 0x00000004,           // This cover point is marked as a flee point
    CP_DUCK = 0x00000008,           // This cover point is marked as a duck point
    CP_NEAREST = 0x00000010,        // Find the nearest combat point
    CP_AVOID_ENEMY = 0x00000020,    // Avoid our enemy
    CP_INVESTIGATE = 0x00000040,    // A special point worth enemy investigation if searching
    CP_SQUAD = 0x00000080,          // Squad path
    CP_AVOID = 0x00000100,          // Avoid supplied position
    CP_APPROACH_ENEMY = 0x00000200, // Try to get closer to enemy
    CP_CLOSEST = 0x00000400,        // Take the closest combatPoint to the enemy that's available
    CP_FLANK = 0x00000800,          // Pick a combatPoint behind the enemy
    CP_HAS_ROUTE = 0x00001000,      // Pick a combatPoint that we have a route to
    CP_SNIPE = 0x00002000,          // Pick a combatPoint that is marked as a sniper spot
    CP_SAFE = 0x00004000,           // Pick a combatPoint that is not have dangerTime
    CP_HORZ_DIST_COLL = 0x00008000, // Collect combat points within *horizontal* dist
    CP_NO_PVS = 0x00010000,         // A combat point out of the PVS of enemy pos
    CP_RETREAT = 0x00020000,        // Try to get farther from enemy
    CP_SHORTEST_PATH = 0x00040000,  // Shortest path from me to combat point to enemy
    CP_TRYFAR = 0x00080000,
};
template <> inline constexpr bool enableEnumFlags<combatPointSearchFlags_t> = true;

extern int NPC_FindCombatPoint(const vec3_t position, const vec3_t avoidPosition, vec3_t enemyPosition, const combatPointSearchFlags_t flags,
                               const float avoidDist, const int ignorePoint = -1);
extern int NPC_FindCombatPointRetry(const vec3_t position, const vec3_t avoidPosition, vec3_t enemyPosition, combatPointSearchFlags_t* cpFlags, float avoidDist,
                                    const int ignorePoint);

extern bool NPC_ReserveCombatPoint(int combatPointID);
extern bool NPC_FreeCombatPoint(int combatPointID, bool failed = false);
extern bool NPC_SetCombatPoint(int combatPointID);

extern bool NPC_ValidEnemy(gentity_t* ent);
extern bool NPC_CheckEnemyExt(bool checkAlerts = false);
extern bool NPC_FindPlayer(void);
extern bool NPC_CheckCanAttackExt(void);

extern int NPC_CheckAlertEvents(bool checkSight, bool checkSound, int ignoreAlert = -1, bool mustHaveOwner = false,
                                alertEventLevel_e minAlertLevel = alertEventLevel_e::AEL_MINOR, bool onGroundOnly = false);
extern bool NPC_CheckForDanger(int alertEvent);
extern void G_AlertTeam(gentity_t* victim, gentity_t* attacker, float radius, float soundDist);

extern int NPC_FindSquadPoint(vec3_t position);

extern void ClearPlayerAlertEvents(void);

extern bool G_BoundsOverlap(const vec3_t mins1, const vec3_t maxs1, const vec3_t mins2, const vec3_t maxs2);

extern void NPC_SetMoveGoal(gentity_t* ent, vec3_t point, int radius, bool isNavGoal = false, int combatPoint = -1, gentity_t* targetEnt = nullptr);

extern void NPC_ApplyWeaponFireDelay(void);

// NPC_FaceXXX suite
extern bool NPC_FacePosition(vec3_t position, bool doPitch = true);
extern bool NPC_FaceEntity(gentity_t* ent, bool doPitch = true);
extern bool NPC_FaceEnemy(bool doPitch = true);

// Skill level cvar
extern cvar_t* g_spskill;

/*
-------------------------
struct navInfo_s
-------------------------
*/

typedef struct navInfo_s {
    gentity_t* blocker;
    vec3_t direction;
    vec3_t pathDirection;
    float distance;
    trace_t trace;
    int flags;
} navInfo_t;

#endif

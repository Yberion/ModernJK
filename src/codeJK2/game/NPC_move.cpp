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

//
// NPC_move.cpp
//

#include "g_headers.h"

#include "b_local.h"
#include "g_nav.h"
#include "anims.h"

extern cvar_t* d_altRoutes;

bool G_BoundsOverlap(const vec3_t mins1, const vec3_t maxs1, const vec3_t mins2, const vec3_t maxs2);
int NAV_Steer(gentity_t* self, vec3_t dir, float distance);
extern int GetTime(int lastTime);

navInfo_t frameNavInfo;
extern bool FlyingCreature(gentity_t* ent);
extern bool PM_InKnockDown(playerState_t* ps);

/*
-------------------------
NPC_ClearPathToGoal
-------------------------
*/

bool NPC_ClearPathToGoal(vec3_t dir, gentity_t* goal)
{
    trace_t trace;

    // FIXME: What does do about area portals?  THIS IS BROKEN
    // if ( gi.inPVS( NPC->currentOrigin, goal->currentOrigin ) == false )
    //	return false;

    // Look ahead and see if we're clear to move to our goal position
    if (NAV_CheckAhead(NPC, goal->currentOrigin, trace, (NPC->clipmask & ~contentsFlags_t::CONTENTS_BODY) | contentsFlags_t::CONTENTS_BOTCLIP)) {
        // VectorSubtract( goal->currentOrigin, NPC->currentOrigin, dir );
        return true;
    }

    if (!FlyingCreature(NPC)) {
        // See if we're too far above
        if (Q_fabsf(NPC->currentOrigin[2] - goal->currentOrigin[2]) > 48)
            return false;
    }

    // This is a work around
    float radius = (NPC->maxs[0] > NPC->maxs[1]) ? NPC->maxs[0] : NPC->maxs[1];
    float dist = Distance(NPC->currentOrigin, goal->currentOrigin);
    float tFrac = 1.0f - (radius / dist);

    if (trace.fraction >= tFrac)
        return true;

    // See if we're looking for a navgoal
    if (Any(goal->svFlags & svFlags_t::SVF_NAVGOAL)) {
        // Okay, didn't get all the way there, let's see if we got close enough:
        if (NAV_HitNavGoal(trace.endpos, NPC->mins, NPC->maxs, goal->currentOrigin, NPCInfo->goalRadius, FlyingCreature(NPC))) {
            // VectorSubtract(goal->currentOrigin, NPC->currentOrigin, dir);
            return true;
        }
    }

    return false;
}

/*
-------------------------
NPC_CheckCombatMove
-------------------------
*/

inline bool NPC_CheckCombatMove(void)
{
    // return NPCInfo->combatMove;
    if ((NPCInfo->goalEntity && NPC->enemy && NPCInfo->goalEntity == NPC->enemy) || (NPCInfo->combatMove)) {
        return true;
    }

    if (NPCInfo->goalEntity && NPCInfo->watchTarget) {
        if (NPCInfo->goalEntity != NPCInfo->watchTarget) {
            return true;
        }
    }

    return false;
}

/*
-------------------------
NPC_LadderMove
-------------------------
*/

static void NPC_LadderMove(vec3_t dir)
{
    // FIXME: this doesn't guarantee we're facing ladder
    // ALSO: Need to be able to get off at top
    // ALSO: Need to play an anim
    // ALSO: Need transitionary anims?

    if ((dir[2] > 0) || (dir[2] < 0 && NPC->client->ps.groundEntityNum == ENTITYNUM_NONE)) {
        // Set our movement direction
        ucmd.upmove = (dir[2] > 0) ? 127 : -127;

        // Don't move around on XY
        ucmd.forwardmove = ucmd.rightmove = 0;
    }
}

/*
-------------------------
NPC_GetMoveInformation
-------------------------
*/

inline bool NPC_GetMoveInformation(vec3_t dir, float* distance)
{
    // NOTENOTE: Use path stacks!

    // Make sure we have somewhere to go
    if (NPCInfo->goalEntity == nullptr)
        return false;

    // Get our move info
    VectorSubtract(NPCInfo->goalEntity->currentOrigin, NPC->currentOrigin, dir);
    *distance = VectorNormalize(dir);

    VectorCopy(NPCInfo->goalEntity->currentOrigin, NPCInfo->blockedDest);

    return true;
}

/*
-------------------------
NAV_GetLastMove
-------------------------
*/

void NAV_GetLastMove(navInfo_t& info)
{
    info = frameNavInfo;
}

/*
-------------------------
NPC_GetMoveDirection
-------------------------
*/

bool NPC_GetMoveDirection(vec3_t out, float* distance)
{
    vec3_t angles;

    // Clear the struct
    //  all zero, the trace's collision records included: only their default constructor makes navInfo_t non-trivial
    memset((void*)&frameNavInfo, 0, sizeof(frameNavInfo));

    // Get our movement, if any
    if (NPC_GetMoveInformation(frameNavInfo.direction, &frameNavInfo.distance) == false)
        return false;

    // Setup the return value
    *distance = frameNavInfo.distance;

    // For starters
    VectorCopy(frameNavInfo.direction, frameNavInfo.pathDirection);

    // If on a ladder, move appropriately
    if (Any(NPC->watertype & contentsFlags_t::CONTENTS_LADDER)) {
        NPC_LadderMove(frameNavInfo.direction);
        return true;
    }

    // Attempt a straight move to goal
    if (NPC_ClearPathToGoal(frameNavInfo.direction, NPCInfo->goalEntity) == false) {
        // See if we're just stuck
        if (NAV_MoveToGoal(NPC, frameNavInfo) == WAYPOINT_NONE) {
            // Can't reach goal, just face
            vectoangles(frameNavInfo.direction, angles);
            NPCInfo->desiredYaw = AngleNormalize360(angles[YAW]);
            VectorCopy(frameNavInfo.direction, out);
            *distance = frameNavInfo.distance;
            return false;
        }

        frameNavInfo.flags |= navInfoFlags_t::NIF_MACRO_NAV;
    }

    // Avoid any collisions on the way
    if (NAV_AvoidCollision(NPC, NPCInfo->goalEntity, frameNavInfo) == false) {
        // FIXME: Emit a warning, this is a worst case scenario
        // FIXME: if we have a clear path to our goal (exluding bodies), but then this
        //			check (against bodies only) fails, shouldn't we fall back
        //			to macro navigation?  Like so:
        if (!Any(frameNavInfo.flags &
                 navInfoFlags_t::NIF_MACRO_NAV)) { // we had a clear path to goal and didn't try macro nav, but can't avoid collision so try macro nav here
            // See if we're just stuck
            if (NAV_MoveToGoal(NPC, frameNavInfo) == WAYPOINT_NONE) {
                // Can't reach goal, just face
                vectoangles(frameNavInfo.direction, angles);
                NPCInfo->desiredYaw = AngleNormalize360(angles[YAW]);
                VectorCopy(frameNavInfo.direction, out);
                *distance = frameNavInfo.distance;
                return false;
            }

            frameNavInfo.flags |= navInfoFlags_t::NIF_MACRO_NAV;
        }
    }

    // Setup the return values
    VectorCopy(frameNavInfo.direction, out);
    *distance = frameNavInfo.distance;

    return true;
}

/*
-------------------------
NPC_GetMoveDirectionAltRoute
-------------------------
*/
extern int NAVNEW_MoveToGoal(gentity_t* self, navInfo_t& info);
extern bool NAVNEW_AvoidCollision(gentity_t* self, gentity_t* goal, navInfo_t& info, bool setBlockedInfo, int blockedMovesLimit);
bool NPC_GetMoveDirectionAltRoute(vec3_t out, float* distance, bool tryStraight)
{
    vec3_t angles;

    NPCInfo->aiFlags &= ~npcAiFlags_t::NPCAI_BLOCKED;

    // Clear the struct
    //  all zero, the trace's collision records included: only their default constructor makes navInfo_t non-trivial
    memset((void*)&frameNavInfo, 0, sizeof(frameNavInfo));

    // Get our movement, if any
    if (NPC_GetMoveInformation(frameNavInfo.direction, &frameNavInfo.distance) == false)
        return false;

    // Setup the return value
    *distance = frameNavInfo.distance;

    // For starters
    VectorCopy(frameNavInfo.direction, frameNavInfo.pathDirection);

    // If on a ladder, move appropriately
    if (Any(NPC->watertype & contentsFlags_t::CONTENTS_LADDER)) {
        NPC_LadderMove(frameNavInfo.direction);
        return true;
    }

    // Attempt a straight move to goal
    if (!tryStraight || NPC_ClearPathToGoal(frameNavInfo.direction, NPCInfo->goalEntity) == false) { // blocked
        // Can't get straight to goal, use macro nav
        if (NAVNEW_MoveToGoal(NPC, frameNavInfo) == WAYPOINT_NONE) {
            // Can't reach goal, just face
            vectoangles(frameNavInfo.direction, angles);
            NPCInfo->desiredYaw = AngleNormalize360(angles[YAW]);
            VectorCopy(frameNavInfo.direction, out);
            *distance = frameNavInfo.distance;
            return false;
        }
        // else we are on our way
        frameNavInfo.flags |= navInfoFlags_t::NIF_MACRO_NAV;
    }
    else { // we have no architectural problems, see if there are ents inthe way and try to go around them
        // not blocked
        if (d_altRoutes->integer) { // try macro nav
            navInfo_t tempInfo;
            memcpy(&tempInfo, &frameNavInfo, sizeof(tempInfo));
            if (NAVNEW_AvoidCollision(NPC, NPCInfo->goalEntity, tempInfo, true, 5) == false) { // revert to macro nav
                // Can't get straight to goal, dump tempInfo and use macro nav
                if (NAVNEW_MoveToGoal(NPC, frameNavInfo) == WAYPOINT_NONE) {
                    // Can't reach goal, just face
                    vectoangles(frameNavInfo.direction, angles);
                    NPCInfo->desiredYaw = AngleNormalize360(angles[YAW]);
                    VectorCopy(frameNavInfo.direction, out);
                    *distance = frameNavInfo.distance;
                    return false;
                }
                // else we are on our way
                frameNavInfo.flags |= navInfoFlags_t::NIF_MACRO_NAV;
            }
            else { // otherwise, either clear or can avoid
                memcpy(&frameNavInfo, &tempInfo, sizeof(frameNavInfo));
            }
        }
        else {                                                                                      // OR: just give up
            if (NAVNEW_AvoidCollision(NPC, NPCInfo->goalEntity, frameNavInfo, true, 30) == false) { // give up
                return false;
            }
        }
    }

    // Setup the return values
    VectorCopy(frameNavInfo.direction, out);
    *distance = frameNavInfo.distance;

    return true;
}

void G_UcmdMoveForDir(gentity_t* self, usercmd_t* cmd, vec3_t dir)
{
    vec3_t forward, right;

    AngleVectors(self->currentAngles, forward, right, nullptr);

    dir[2] = 0;
    VectorNormalize(dir);
    // NPCs cheat and store this directly because converting movement into a ucmd loses precision
    VectorCopy(dir, self->client->ps.moveDir);

    float fDot = DotProduct(forward, dir) * 127.0f;
    float rDot = DotProduct(right, dir) * 127.0f;
    // Must clamp this because DotProduct is not guaranteed to return a number within -1 to 1, and that would be bad when we're shoving this into a signed byte
    if (fDot > 127.0f) {
        fDot = 127.0f;
    }
    if (fDot < -127.0f) {
        fDot = -127.0f;
    }
    if (rDot > 127.0f) {
        rDot = 127.0f;
    }
    if (rDot < -127.0f) {
        rDot = -127.0f;
    }
    cmd->forwardmove = Q_floorf(fDot);
    cmd->rightmove = Q_floorf(rDot);

    /*
    vec3_t	wishvel;
    for ( int i = 0 ; i < 3 ; i++ )
    {
        wishvel[i] = forward[i]*cmd->forwardmove + right[i]*cmd->rightmove;
    }
    VectorNormalize( wishvel );
    if ( !VectorCompare( wishvel, dir ) )
    {
        Q_Println("PRECISION LOSS: {} != {}", vtos(wishvel), vtos(dir));
    }
    */
}

/*
-------------------------
NPC_MoveToGoal

  Now assumes goal is goalEntity, was no reason for it to be otherwise
-------------------------
*/
#if AI_TIMERS
extern int navTime;
#endif //	AI_TIMERS
bool NPC_MoveToGoal(bool tryStraight)
{
#if AI_TIMERS
    int startTime = GetTime(0);
#endif //	AI_TIMERS
    // If taking full body pain, don't move
    if (PM_InKnockDown(&NPC->client->ps) || ((NPC->s.legsAnim >= animNumber_t::BOTH_PAIN1) && (NPC->s.legsAnim <= animNumber_t::BOTH_PAIN19))) {
        return true;
    }

    if (Any(NPC->s.eFlags & entityFlags_t::EF_LOCKED_TO_WEAPON)) { // If in an emplaced gun, never try to navigate!
        return true;
    }

    float distance;
    vec3_t dir;

    // FIXME: if can't get to goal & goal is a target (enemy), try to find a waypoint that has line of sight to target, at least?
    // Get our movement direction
#if 1
    if (NPC_GetMoveDirectionAltRoute(dir, &distance, tryStraight) == false)
#else
    if (NPC_GetMoveDirection(dir, &distance) == false)
#endif
        return false;

    NPCInfo->distToGoal = distance;

    // Convert the move to angles
    vectoangles(dir, NPCInfo->lastPathAngles);

    // FIXME: still getting ping-ponging in certain cases... !!!  Nav/avoidance error?  WTF???!!!
    // If in combat move, then move directly towards our goal
    if (NPC_CheckCombatMove()) { // keep current facing
        G_UcmdMoveForDir(NPC, &ucmd, dir);
    }
    else { // face our goal
        // FIXME: strafe instead of turn if change in dir is small and temporary
        NPCInfo->desiredPitch = 0.0f;
        NPCInfo->desiredYaw = AngleNormalize360(NPCInfo->lastPathAngles[YAW]);

        // Pitch towards the goal and also update if flying or swimming
        if (NPCInfo->stats.moveType == movetype_t::MT_FLYSWIM) {
            NPCInfo->desiredPitch = AngleNormalize360(NPCInfo->lastPathAngles[PITCH]);

            if (dir[2]) {
                float scale = (dir[2] * distance);
                if (scale > 64) {
                    scale = 64;
                }
                else if (scale < -64) {
                    scale = -64;
                }
                NPC->client->ps.velocity[2] = scale;
                // NPC->client->ps.velocity[2] = (dir[2] > 0) ? 64 : -64;
            }
        }

        // Set any final info
        ucmd.forwardmove = 127;
    }

#if AI_TIMERS
    navTime += GetTime(startTime);
#endif //	AI_TIMERS
    return true;
}

/*
-------------------------
void NPC_SlideMoveToGoal( void )

  Now assumes goal is goalEntity, if want to use tempGoal, you set that before calling the func
-------------------------
*/
bool NPC_SlideMoveToGoal(void)
{
    float saveYaw = NPC->client->ps.viewangles[YAW];

    NPCInfo->combatMove = true;

    bool ret = NPC_MoveToGoal(true);

    NPCInfo->desiredYaw = saveYaw;

    return ret;
}

/*
-------------------------
NPC_ApplyRoff
-------------------------
*/

void NPC_ApplyRoff(void)
{
    PlayerStateToEntityState(&NPC->client->ps, &NPC->s);
    VectorCopy(NPC->currentOrigin, NPC->lastOrigin);

    // use the precise origin for linking
    gi.linkentity(NPC);
}

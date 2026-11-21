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

// NPC_utils.cpp

#include "b_local.h"
#include <icarus/Q3_Interface.h>
#include <ghoul2/G2.h>

EnumArray<int, npcteam_t, npcteam_t::NPCTEAM_NUM_TEAMS> teamNumbers;
EnumArray<int, npcteam_t, npcteam_t::NPCTEAM_NUM_TEAMS> teamStrength;
EnumArray<int, npcteam_t, npcteam_t::NPCTEAM_NUM_TEAMS> teamCounter;

constexpr float VALID_ATTACK_CONE = 2.0f; // Degrees
extern void G_DebugPrint(WL_e level, const char* format, ...);

/*
void CalcEntitySpot ( gentity_t *ent, spot_t spot, vec3_t point )

Added: Uses shootAngles if a NPC has them

*/
void CalcEntitySpot(const gentity_t* ent, const spot_t spot, vec3_t point)
{
    vec3_t forward, up, right;
    vec3_t start, end;
    trace_t tr;

    if (!ent) {
        return;
    }
    switch (spot) {
    case spot_t::SPOT_ORIGIN:
        if (VectorCompare(ent->r.currentOrigin, vec3_origin)) {  // brush
            VectorSubtract(ent->r.absmax, ent->r.absmin, point); // size
            VectorMA(ent->r.absmin, 0.5, point, point);
        }
        else {
            VectorCopy(ent->r.currentOrigin, point);
        }
        break;

    case spot_t::SPOT_CHEST:
    case spot_t::SPOT_HEAD:
        if (ent->client &&
            VectorLengthSquared(
                ent->client->renderInfo
                    .eyePoint) /*&& (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)*/) { // Actual tag_head eyespot!
            // FIXME: Stasis aliens may have a problem here...
            VectorCopy(ent->client->renderInfo.eyePoint, point);
            if (ent->client->NPC_class == class_t::CLASS_ATST) { // adjust up some
                point[2] += 28;                                  // magic number :)
            }
            if (ent->NPC) { // always aim from the center of my bbox, so we don't wiggle when we lean forward or backwards
                point[0] = ent->r.currentOrigin[0];
                point[1] = ent->r.currentOrigin[1];
            }
            /*
            else if (ent->s.eType == entityType_t::ET_PLAYER )
            {
                SubtractLeanOfs( ent, point );
            }
            */
        }
        else {
            VectorCopy(ent->r.currentOrigin, point);
            if (ent->client) {
                point[2] += ent->client->ps.viewheight;
            }
        }
        if (spot == spot_t::SPOT_CHEST && ent->client) {
            if (ent->client->NPC_class != class_t::CLASS_ATST) { // adjust up some
                point[2] -= ent->r.maxs[2] * 0.2f;
            }
        }
        break;

    case spot_t::SPOT_HEAD_LEAN:
        if (ent->client &&
            VectorLengthSquared(
                ent->client->renderInfo
                    .eyePoint) /*&& (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD*/) { // Actual tag_head eyespot!
            // FIXME: Stasis aliens may have a problem here...
            VectorCopy(ent->client->renderInfo.eyePoint, point);
            if (ent->client->NPC_class == class_t::CLASS_ATST) { // adjust up some
                point[2] += 28;                                  // magic number :)
            }
            if (ent->NPC) { // always aim from the center of my bbox, so we don't wiggle when we lean forward or backwards
                point[0] = ent->r.currentOrigin[0];
                point[1] = ent->r.currentOrigin[1];
            }
            /*
            else if ( ent->s.eType == entityType_t::ET_PLAYER )
            {
                SubtractLeanOfs( ent, point );
            }
            */
            // NOTE: automatically takes leaning into account!
        }
        else {
            VectorCopy(ent->r.currentOrigin, point);
            if (ent->client) {
                point[2] += ent->client->ps.viewheight;
            }
            // AddLeanOfs ( ent, point );
        }
        break;

        // FIXME: implement...
        // case spot_t::SPOT_CHEST:
        // Returns point 3/4 from tag_torso to tag_head?
        // break;

    case spot_t::SPOT_LEGS:
        VectorCopy(ent->r.currentOrigin, point);
        point[2] += (ent->r.mins[2] * 0.5);
        break;

    case spot_t::SPOT_WEAPON:
        if (ent->NPC && !VectorCompare(ent->NPC->shootAngles, vec3_origin) && !VectorCompare(ent->NPC->shootAngles, ent->client->ps.viewangles)) {
            AngleVectors(ent->NPC->shootAngles, forward, right, up);
        }
        else {
            AngleVectors(ent->client->ps.viewangles, forward, right, up);
        }
        CalcMuzzlePoint((gentity_t*)ent, forward, right, up, point);
        // NOTE: automatically takes leaning into account!
        break;

    case spot_t::SPOT_GROUND:
        // if entity is on the ground, just use it's absmin
        if (ent->s.groundEntityNum != ENTITYNUM_NONE) {
            VectorCopy(ent->r.currentOrigin, point);
            point[2] = ent->r.absmin[2];
            break;
        }

        // if it is reasonably close to the ground, give the point underneath of it
        VectorCopy(ent->r.currentOrigin, start);
        start[2] = ent->r.absmin[2];
        VectorCopy(start, end);
        end[2] -= 64;
        trap->Trace(&tr, start, ent->r.mins, ent->r.maxs, end, ent->s.number, MASK_PLAYERSOLID, false, g2TraceFlags_t::G2TRFLAG_NONE, 0);
        if (tr.fraction < 1.0) {
            VectorCopy(tr.endpos, point);
            break;
        }

        // otherwise just use the origin
        VectorCopy(ent->r.currentOrigin, point);
        break;

    default:
        VectorCopy(ent->r.currentOrigin, point);
        break;
    }
}

//===================================================================================

/*
bool NPC_UpdateAngles ( bool doPitch, bool doYaw )

Added: option to do just pitch or just yaw

Does not include "aim" in it's calculations

FIXME: stop compressing angles into shorts!!!!
*/
bool NPC_UpdateAngles(bool doPitch, bool doYaw)
{
#if 1

    float error;
    float decay;
    float targetPitch = 0;
    float targetYaw = 0;
    float yawSpeed;
    bool exact = true;

    // if angle changes are locked; just keep the current angles
    // aimTime isn't even set anymore... so this code was never reached, but I need a way to lock NPC's yaw, so instead of making a new SCF_ flag, just use the
    // existing render flag... - dmv
    if (!NPCS.NPC->enemy && ((level.time < NPCS.NPCInfo->aimTime) /*|| NPC->client->renderInfo.renderFlags & RF_LOCKEDANGLE*/)) {
        if (doPitch)
            targetPitch = NPCS.NPCInfo->lockedDesiredPitch;

        if (doYaw)
            targetYaw = NPCS.NPCInfo->lockedDesiredYaw;
    }
    else {
        // we're changing the lockedDesired Pitch/Yaw below so it's lost it's original meaning, get rid of the lock flag
        //	NPC->client->renderInfo.renderFlags &= ~RF_LOCKEDANGLE;

        if (doPitch) {
            targetPitch = NPCS.NPCInfo->desiredPitch;
            NPCS.NPCInfo->lockedDesiredPitch = NPCS.NPCInfo->desiredPitch;
        }

        if (doYaw) {
            targetYaw = NPCS.NPCInfo->desiredYaw;
            NPCS.NPCInfo->lockedDesiredYaw = NPCS.NPCInfo->desiredYaw;
        }
    }

    if (NPCS.NPC->s.weapon == weapon_t::WP_EMPLACED_GUN) {
        // FIXME: this seems to do nothing, actually...
        yawSpeed = 20;
    }
    else {
        yawSpeed = NPCS.NPCInfo->stats.yawSpeed;
    }

    if (NPCS.NPC->s.weapon == weapon_t::WP_SABER && NPCS.NPC->client->ps.fd.forcePowersActive & Bit(forcePowers_t::FP_SPEED)) {
        char buf[128];
        float tFVal = 0;

        trap->Cvar_VariableStringBuffer("timescale", buf, sizeof(buf));

        tFVal = Q_atof(buf);

        yawSpeed *= 1.0f / tFVal;
    }

    if (doYaw) {
        // decay yaw error
        error = AngleDelta(NPCS.NPC->client->ps.viewangles[YAW], targetYaw);
        if (Q_fabs(error) > MIN_ANGLE_ERROR) {
            if (error) {
                exact = false;

                decay = 60.0 + yawSpeed * 3;
                decay *= 50.0f / 1000.0f; // msec

                if (error < 0.0) {
                    error += decay;
                    if (error > 0.0) {
                        error = 0.0;
                    }
                }
                else {
                    error -= decay;
                    if (error < 0.0) {
                        error = 0.0;
                    }
                }
            }
        }

        NPCS.ucmd.angles[YAW] = ANGLE2SHORT(targetYaw + error) - NPCS.client->ps.delta_angles[YAW];
    }

    // FIXME: have a pitchSpeed?
    if (doPitch) {
        // decay pitch error
        error = AngleDelta(NPCS.NPC->client->ps.viewangles[PITCH], targetPitch);
        if (Q_fabs(error) > MIN_ANGLE_ERROR) {
            if (error) {
                exact = false;

                decay = 60.0 + yawSpeed * 3;
                decay *= 50.0f / 1000.0f; // msec

                if (error < 0.0) {
                    error += decay;
                    if (error > 0.0) {
                        error = 0.0;
                    }
                }
                else {
                    error -= decay;
                    if (error < 0.0) {
                        error = 0.0;
                    }
                }
            }
        }

        NPCS.ucmd.angles[PITCH] = ANGLE2SHORT(targetPitch + error) - NPCS.client->ps.delta_angles[PITCH];
    }

    NPCS.ucmd.angles[ROLL] = ANGLE2SHORT(NPCS.NPC->client->ps.viewangles[ROLL]) - NPCS.client->ps.delta_angles[ROLL];

    if (exact && trap->ICARUS_TaskIDPending((sharedEntity_t*)NPCS.NPC, taskID_t::TID_ANGLE_FACE)) {
        trap->ICARUS_TaskIDComplete((sharedEntity_t*)NPCS.NPC, taskID_t::TID_ANGLE_FACE);
    }
    return exact;

#else

    float error;
    float decay;
    float targetPitch = 0;
    float targetYaw = 0;
    float yawSpeed;
    // float		runningMod = NPCInfo->currentSpeed/100.0f;
    bool exact = true;
    bool doSound = false;

    // if angle changes are locked; just keep the current angles
    if (level.time < NPCInfo->aimTime) {
        if (doPitch)
            targetPitch = NPCInfo->lockedDesiredPitch;
        if (doYaw)
            targetYaw = NPCInfo->lockedDesiredYaw;
    }
    else {
        if (doPitch)
            targetPitch = NPCInfo->desiredPitch;
        if (doYaw)
            targetYaw = NPCInfo->desiredYaw;

        //		NPCInfo->aimTime = level.time + 250;
        if (doPitch)
            NPCInfo->lockedDesiredPitch = NPCInfo->desiredPitch;
        if (doYaw)
            NPCInfo->lockedDesiredYaw = NPCInfo->desiredYaw;
    }

    yawSpeed = NPCInfo->stats.yawSpeed;

    if (doYaw) {
        // decay yaw error
        error = AngleDelta(NPC->client->ps.viewangles[YAW], targetYaw);
        if (Q_fabs(error) > MIN_ANGLE_ERROR) {
            /*
            if(NPC->client->playerTeam == TEAM_BORG&&
                NPCInfo->behaviorState != BS_FACE&&NPCInfo->tempBehavior!= BS_FACE)
            {//HACK - borg turn more jittery
                if ( error )
                {
                    exact = false;

                    decay = 60.0 + yawSpeed * 3;
                    decay *= 50.0 / 1000.0;//msec
                    //Snap to
                    if(Q_fabsf(error) > 10)
                    {
                        if(Q_flrand(0.0f, 1.0f) > 0.6)
                        {
                            doSound = true;
                        }
                    }

                    if ( error < 0.0)//-10.0 )
                    {
                        error += decay;
                        if ( error > 0.0 )
                        {
                            error = 0.0;
                        }
                    }
                    else if ( error > 0.0)//10.0 )
                    {
                        error -= decay;
                        if ( error < 0.0 )
                        {
                            error = 0.0;
                        }
                    }
                }
            }
            else*/

            if (error) {
                exact = false;

                decay = 60.0 + yawSpeed * 3;
                decay *= 50.0 / 1000.0; // msec

                if (error < 0.0) {
                    error += decay;
                    if (error > 0.0) {
                        error = 0.0;
                    }
                }
                else {
                    error -= decay;
                    if (error < 0.0) {
                        error = 0.0;
                    }
                }
            }
        }
        ucmd.angles[YAW] = ANGLE2SHORT(targetYaw + error) - client->ps.delta_angles[YAW];
    }

    // FIXME: have a pitchSpeed?
    if (doPitch) {
        // decay pitch error
        error = AngleDelta(NPC->client->ps.viewangles[PITCH], targetPitch);
        if (Q_fabs(error) > MIN_ANGLE_ERROR) {
            /*
            if(NPC->client->playerTeam == TEAM_BORG&&
                NPCInfo->behaviorState != BS_FACE&&NPCInfo->tempBehavior!= BS_FACE)
            {//HACK - borg turn more jittery
                if ( error )
                {
                    exact = false;

                    decay = 60.0 + yawSpeed * 3;
                    decay *= 50.0 / 1000.0;//msec
                    //Snap to
                    if(Q_fabsf(error) > 10)
                    {
                        if(Q_flrand(0.0f, 1.0f) > 0.6)
                        {
                            doSound = true;
                        }
                    }

                    if ( error < 0.0)//-10.0 )
                    {
                        error += decay;
                        if ( error > 0.0 )
                        {
                            error = 0.0;
                        }
                    }
                    else if ( error > 0.0)//10.0 )
                    {
                        error -= decay;
                        if ( error < 0.0 )
                        {
                            error = 0.0;
                        }
                    }
                }
            }
            else*/

            if (error) {
                exact = false;

                decay = 60.0 + yawSpeed * 3;
                decay *= 50.0 / 1000.0; // msec

                if (error < 0.0) {
                    error += decay;
                    if (error > 0.0) {
                        error = 0.0;
                    }
                }
                else {
                    error -= decay;
                    if (error < 0.0) {
                        error = 0.0;
                    }
                }
            }
        }
        ucmd.angles[PITCH] = ANGLE2SHORT(targetPitch + error) - client->ps.delta_angles[PITCH];
    }

    ucmd.angles[ROLL] = ANGLE2SHORT(NPC->client->ps.viewangles[ROLL]) - client->ps.delta_angles[ROLL];

    /*
    if(doSound)
    {
        G_Sound(NPC, G_SoundIndex(va("sound/enemies/borg/borgservo%d.wav", Q_irand(1, 8))));
    }
    */

    return exact;

#endif
}

void NPC_AimWiggle(vec3_t enemy_org)
{
    // shoot for somewhere between the head and torso
    // NOTE: yes, I know this looks weird, but it works
    if (NPCS.NPCInfo->aimErrorDebounceTime < level.time) {
        NPCS.NPCInfo->aimOfs[0] = 0.3 * Q_flrand(NPCS.NPC->enemy->r.mins[0], NPCS.NPC->enemy->r.maxs[0]);
        NPCS.NPCInfo->aimOfs[1] = 0.3 * Q_flrand(NPCS.NPC->enemy->r.mins[1], NPCS.NPC->enemy->r.maxs[1]);
        if (NPCS.NPC->enemy->r.maxs[2] > 0) {
            NPCS.NPCInfo->aimOfs[2] = NPCS.NPC->enemy->r.maxs[2] * Q_flrand(0.0f, -1.0f);
        }
    }
    VectorAdd(enemy_org, NPCS.NPCInfo->aimOfs, enemy_org);
}

/*
bool NPC_UpdateFiringAngles ( bool doPitch, bool doYaw )

  Includes aim when determining angles - so they don't always hit...
  */
bool NPC_UpdateFiringAngles(bool doPitch, bool doYaw)
{
#if 0

	float		diff;
	float		error;
	float		targetPitch = 0;
	float		targetYaw = 0;
	bool	exact = true;

	if ( level.time < NPCInfo->aimTime )
	{
		if( doPitch )
			targetPitch = NPCInfo->lockedDesiredPitch;

		if( doYaw )
			targetYaw = NPCInfo->lockedDesiredYaw;
	}
	else
	{
		if( doPitch )
		{
			targetPitch = NPCInfo->desiredPitch;
			NPCInfo->lockedDesiredPitch = NPCInfo->desiredPitch;
		}

		if( doYaw )
		{
			targetYaw = NPCInfo->desiredYaw;
			NPCInfo->lockedDesiredYaw = NPCInfo->desiredYaw;
		}
	}

	if( doYaw )
	{
		// add yaw error based on NPCInfo->aim value
		error = ((float)(6 - NPCInfo->stats.aim)) * Q_flrand(-1, 1);

		if(Q_irand(0, 1))
			error *= -1;

		diff = AngleDelta ( NPC->client->ps.viewangles[YAW], targetYaw );

		if ( diff )
			exact = false;

		ucmd.angles[YAW] = ANGLE2SHORT( targetYaw + diff + error ) - client->ps.delta_angles[YAW];
	}

	if( doPitch )
	{
		// add pitch error based on NPCInfo->aim value
		error = ((float)(6 - NPCInfo->stats.aim)) * Q_flrand(-1, 1);

		diff = AngleDelta ( NPC->client->ps.viewangles[PITCH], targetPitch );

		if ( diff )
			exact = false;

		ucmd.angles[PITCH] = ANGLE2SHORT( targetPitch + diff + error ) - client->ps.delta_angles[PITCH];
	}

	ucmd.angles[ROLL] = ANGLE2SHORT ( NPC->client->ps.viewangles[ROLL] ) - client->ps.delta_angles[ROLL];

	return exact;

#else

    float error, diff;
    float decay;
    float targetPitch = 0;
    float targetYaw = 0;
    bool exact = true;

    // if angle changes are locked; just keep the current angles
    if (level.time < NPCS.NPCInfo->aimTime) {
        if (doPitch)
            targetPitch = NPCS.NPCInfo->lockedDesiredPitch;
        if (doYaw)
            targetYaw = NPCS.NPCInfo->lockedDesiredYaw;
    }
    else {
        if (doPitch)
            targetPitch = NPCS.NPCInfo->desiredPitch;
        if (doYaw)
            targetYaw = NPCS.NPCInfo->desiredYaw;

        //		NPCInfo->aimTime = level.time + 250;
        if (doPitch)
            NPCS.NPCInfo->lockedDesiredPitch = NPCS.NPCInfo->desiredPitch;
        if (doYaw)
            NPCS.NPCInfo->lockedDesiredYaw = NPCS.NPCInfo->desiredYaw;
    }

    if (NPCS.NPCInfo->aimErrorDebounceTime < level.time) {
        if (Q_irand(0, 1)) {
            NPCS.NPCInfo->lastAimErrorYaw = ((float)(6 - NPCS.NPCInfo->stats.aim)) * Q_flrand(-1, 1);
        }
        if (Q_irand(0, 1)) {
            NPCS.NPCInfo->lastAimErrorPitch = ((float)(6 - NPCS.NPCInfo->stats.aim)) * Q_flrand(-1, 1);
        }
        NPCS.NPCInfo->aimErrorDebounceTime = level.time + Q_irand(250, 2000);
    }

    if (doYaw) {
        // decay yaw diff
        diff = AngleDelta(NPCS.NPC->client->ps.viewangles[YAW], targetYaw);

        if (diff) {
            exact = false;

            decay = 60.0 + 80.0;
            decay *= 50.0f / 1000.0f; // msec
            if (diff < 0.0) {
                diff += decay;
                if (diff > 0.0) {
                    diff = 0.0;
                }
            }
            else {
                diff -= decay;
                if (diff < 0.0) {
                    diff = 0.0;
                }
            }
        }

        // add yaw error based on NPCInfo->aim value
        error = NPCS.NPCInfo->lastAimErrorYaw;

        /*
        if(Q_irand(0, 1))
        {
            error *= -1;
        }
        */

        NPCS.ucmd.angles[YAW] = ANGLE2SHORT(targetYaw + diff + error) - NPCS.client->ps.delta_angles[YAW];
    }

    if (doPitch) {
        // decay pitch diff
        diff = AngleDelta(NPCS.NPC->client->ps.viewangles[PITCH], targetPitch);
        if (diff) {
            exact = false;

            decay = 60.0 + 80.0;
            decay *= 50.0f / 1000.0f; // msec
            if (diff < 0.0) {
                diff += decay;
                if (diff > 0.0) {
                    diff = 0.0;
                }
            }
            else {
                diff -= decay;
                if (diff < 0.0) {
                    diff = 0.0;
                }
            }
        }

        error = NPCS.NPCInfo->lastAimErrorPitch;

        NPCS.ucmd.angles[PITCH] = ANGLE2SHORT(targetPitch + diff + error) - NPCS.client->ps.delta_angles[PITCH];
    }

    NPCS.ucmd.angles[ROLL] = ANGLE2SHORT(NPCS.NPC->client->ps.viewangles[ROLL]) - NPCS.client->ps.delta_angles[ROLL];

    return exact;

#endif
}
//===================================================================================

/*
static void NPC_UpdateShootAngles (vec3_t angles, bool doPitch, bool doYaw )

Does update angles on shootAngles
*/

void NPC_UpdateShootAngles(vec3_t angles, bool doPitch, bool doYaw)
{ // FIXME: shoot angles either not set right or not used!
    float error;
    float decay;
    float targetPitch = 0;
    float targetYaw = 0;

    if (doPitch)
        targetPitch = angles[PITCH];
    if (doYaw)
        targetYaw = angles[YAW];

    if (doYaw) {
        // decay yaw error
        error = AngleDelta(NPCS.NPCInfo->shootAngles[YAW], targetYaw);
        if (error) {
            decay = 60.0 + 80.0 * NPCS.NPCInfo->stats.aim;
            decay *= 100.0f / 1000.0f; // msec
            if (error < 0.0) {
                error += decay;
                if (error > 0.0) {
                    error = 0.0;
                }
            }
            else {
                error -= decay;
                if (error < 0.0) {
                    error = 0.0;
                }
            }
        }
        NPCS.NPCInfo->shootAngles[YAW] = targetYaw + error;
    }

    if (doPitch) {
        // decay pitch error
        error = AngleDelta(NPCS.NPCInfo->shootAngles[PITCH], targetPitch);
        if (error) {
            decay = 60.0 + 80.0 * NPCS.NPCInfo->stats.aim;
            decay *= 100.0f / 1000.0f; // msec
            if (error < 0.0) {
                error += decay;
                if (error > 0.0) {
                    error = 0.0;
                }
            }
            else {
                error -= decay;
                if (error < 0.0) {
                    error = 0.0;
                }
            }
        }
        NPCS.NPCInfo->shootAngles[PITCH] = targetPitch + error;
    }
}

/*
void SetTeamNumbers (void)

Sets the number of living clients on each team

FIXME: Does not account for non-respawned players!
FIXME: Don't include medics?
*/
void SetTeamNumbers(void)
{
    gentity_t* found;
    int i;

    teamNumbers = {};
    teamStrength = {};

    // OJKFIXME: clientnum 0
    for (i = 0; i < 1; i++) {
        found = &g_entities[i];

        if (found->client) {
            if (found->health > 0) // FIXME: or if a player!
            {
                teamNumbers[found->client->playerTeam]++;
                teamStrength[found->client->playerTeam] += found->health;
            }
        }
    }

    for (npcteam_t t = npcteam_t::NPCTEAM_FREE; t < npcteam_t::NPCTEAM_NUM_TEAMS; t++) { // Get the average health
        teamStrength[t] = Q_floor(((float)(teamStrength[t])) / ((float)(teamNumbers[t])));
    }
}

extern stringID_table_t BSTable[];
extern stringID_table_t BSETTable[];
bool G_ActivateBehavior(gentity_t* self, bSet_t bset)
{
    int bsID = -1; // the behavior state named, -1 for none
    char* bs_name = nullptr;

    if (!self) {
        return false;
    }

    bs_name = self->behaviorSet[bset];

    if (!(VALIDSTRING(bs_name))) {
        return false;
    }

    if (self->NPC) {
        bsID = GetIDForString(BSTable, bs_name);
    }

    if (bsID != -1) {
        const bState_t bSID = static_cast<bState_t>(bsID);
        self->NPC->tempBehavior = bState_t::BS_DEFAULT;
        self->NPC->behaviorState = bSID;
    }
    else {
        /*
        char			newname[MAX_FILENAME_LENGTH];
        sprintf((char *) &newname, "%s/%s", Q3_SCRIPT_DIR, bs_name );
        */

        // FIXME: between here and actually getting into the ICARUS_RunScript function, the stack gets blown!
        // if ( ( ICARUS_entFilter == -1 ) || ( ICARUS_entFilter == self->s.number ) )
        if (0) {
            G_DebugPrint(WL_e::WL_VERBOSE, "%s attempting to run bSet %s (%s)\n", self->targetname, GetStringForID(BSETTable, to_underlying(bset)), bs_name);
        }
        trap->ICARUS_RunScript((sharedEntity_t*)self, va("%s/%s", Q3_SCRIPT_DIR, bs_name));
    }
    return true;
}

/*
=============================================================================

    Extended Functions

=============================================================================
*/

// rww - special system for sync'ing bone angles between client and server.
void NPC_SetBoneAngles(gentity_t* ent, const char* bone, vec3_t angles)
{
    int* thebone = &ent->s.boneIndex1;
    int* firstFree = nullptr;
    int i = 0;
    int boneIndex = G_BoneIndex(bone);
    boneFlags_t flags;
    orientations_t up, right, forward;
    vec3_t* boneVector = &ent->s.boneAngles1;
    vec3_t* freeBoneVec = nullptr;

    while (thebone) {
        if (!*thebone && !firstFree) { // if the value is 0 then this index is clear, we can use it if we don't find the bone we want already existing.
            firstFree = thebone;
            freeBoneVec = boneVector;
        }
        else if (*thebone) {
            if (*thebone == boneIndex) { // this is it
                break;
            }
        }

        switch (i) {
        case 0:
            thebone = &ent->s.boneIndex2;
            boneVector = &ent->s.boneAngles2;
            break;
        case 1:
            thebone = &ent->s.boneIndex3;
            boneVector = &ent->s.boneAngles3;
            break;
        case 2:
            thebone = &ent->s.boneIndex4;
            boneVector = &ent->s.boneAngles4;
            break;
        default:
            thebone = nullptr;
            boneVector = nullptr;
            break;
        }

        i++;
    }

    if (!thebone) {       // didn't find it, create it
        if (!firstFree) { // no free bones.. can't do a thing then.
            Com_Printf("WARNING: NPC has no free bone indexes\n");
            return;
        }

        thebone = firstFree;

        *thebone = boneIndex;
        boneVector = freeBoneVec;
    }

    // If we got here then we have a vector and an index.

    // Copy the angles over the vector in the entitystate, so we can use the corresponding index
    // to set the bone angles on the client.
    VectorCopy(angles, *boneVector);

    // Now set the angles on our server instance if we have one.

    if (!ent->ghoul2) {
        return;
    }

    flags = boneFlags_t::BONE_ANGLES_POSTMULT;
    up = orientations_t::POSITIVE_X;
    right = orientations_t::NEGATIVE_Y;
    forward = orientations_t::NEGATIVE_Z;

    // first 3 bits is forward, second 3 bits is right, third 3 bits is up
    ent->s.boneOrient = ((to_underlying(forward)) | (to_underlying(right) << 3) | (to_underlying(up) << 6));

    trap->G2API_SetBoneAngles(ent->ghoul2, 0, bone, angles, flags, up, right, forward, nullptr, 100, level.time);
}

// rww - and another method of automatically managing surface status for the client and server at once
constexpr g2SurfaceFlags_t TURN_ON = g2SurfaceFlags_t::G2SURFACEFLAG_NONE;

void NPC_SetSurfaceOnOff(gentity_t* ent, const char* surfaceName, g2SurfaceFlags_t surfaceFlags)
{
    int i = 0;
    bool foundIt = false;

    while (i < BG_NUM_TOGGLEABLE_SURFACES && bgToggleableSurfaces[i]) {
        if (!Q_stricmp(surfaceName, bgToggleableSurfaces[i])) { // got it
            foundIt = true;
            break;
        }
        i++;
    }

    if (!foundIt) {
        Com_Printf("WARNING: Tried to toggle NPC surface that isn't in toggleable surface list (%s)\n", surfaceName);
        return;
    }

    if (surfaceFlags == TURN_ON) { // Make sure the entitystate values reflect this surface as on now.
        ent->s.surfacesOn |= (1 << i);
        ent->s.surfacesOff &= ~(1 << i);
    }
    else { // Otherwise make sure they're off.
        ent->s.surfacesOn &= ~(1 << i);
        ent->s.surfacesOff |= (1 << i);
    }

    if (!ent->ghoul2) {
        return;
    }

    trap->G2API_SetSurfaceOnOff(ent->ghoul2, surfaceName, surfaceFlags);
}

// rww - cheap check to see if an armed client is looking in our general direction
bool NPC_SomeoneLookingAtMe(gentity_t* ent)
{
    int i = 0;
    gentity_t* pEnt;

    while (i < MAX_CLIENTS) {
        pEnt = &g_entities[i];

        if (pEnt && pEnt->inuse && pEnt->client && pEnt->client->sess.sessionTeam != team_t::TEAM_SPECTATOR && pEnt->client->tempSpectate < level.time &&
            !Any(pEnt->client->ps.pm_flags & pmoveFlags_t::PMF_FOLLOW) && pEnt->s.weapon != weapon_t::WP_NONE) {
            if (trap->InPVS(ent->r.currentOrigin, pEnt->r.currentOrigin)) {
                if (InFOV(ent, pEnt, 30, 30)) { // I'm in a 30 fov or so cone from this player.. that's enough I guess.
                    return true;
                }
            }
        }

        i++;
    }

    return false;
}

bool NPC_ClearLOS(const vec3_t start, const vec3_t end)
{
    return G_ClearLOS(NPCS.NPC, start, end);
}
bool NPC_ClearLOS5(const vec3_t end)
{
    return G_ClearLOS5(NPCS.NPC, end);
}
bool NPC_ClearLOS4(gentity_t* ent)
{
    return G_ClearLOS4(NPCS.NPC, ent);
}
bool NPC_ClearLOS3(const vec3_t start, gentity_t* ent)
{
    return G_ClearLOS3(NPCS.NPC, start, ent);
}
bool NPC_ClearLOS2(gentity_t* ent, const vec3_t end)
{
    return G_ClearLOS2(NPCS.NPC, ent, end);
}

/*
-------------------------
NPC_ValidEnemy
-------------------------
*/

bool NPC_ValidEnemy(gentity_t* ent)
{
    npcteam_t entTeam = npcteam_t::NPCTEAM_FREE;

    // Must be a valid pointer
    if (ent == nullptr)
        return false;

    // Must not be me
    if (ent == NPCS.NPC)
        return false;

    // Must not be deleted
    if (ent->inuse == false)
        return false;

    // Must be alive
    if (ent->health <= 0)
        return false;

    // In case they're in notarget mode
    if (Any(ent->flags & gentityFlags_t::FL_NOTARGET))
        return false;

    // Must be an NPC
    if (ent->client == nullptr) {
        //	if ( Any( ent->svFlags & SVF_NONNPC_ENEMY ) )
        if (ent->s.eType != entityType_t::ET_NPC) { // still potentially valid
            if (to_underlying(ent->alliedTeam) == to_underlying(NPCS.NPC->client->playerTeam)) {
                return false;
            }
            else {
                return true;
            }
        }
        else {
            return false;
        }
    }
    else if (ent->client && ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR) { // don't go after spectators
        return false;
    }
    else if (ent->client && ent->client->tempSpectate >= level.time) { // don't go after spectators
        return false;
    }
    if (ent->NPC && ent->client) {
        entTeam = ent->client->playerTeam;
    }
    else if (ent->client) {
        if (level.gametype < gametype_t::GT_TEAM) {
            entTeam = npcteam_t::NPCTEAM_PLAYER;
        }
        else {
            if (ent->client->sess.sessionTeam == team_t::TEAM_BLUE) {
                entTeam = npcteam_t::NPCTEAM_PLAYER;
            }
            else if (ent->client->sess.sessionTeam == team_t::TEAM_RED) {
                entTeam = npcteam_t::NPCTEAM_ENEMY;
            }
            else {
                entTeam = npcteam_t::NPCTEAM_NEUTRAL;
            }
        }
    }
    // Can't be on the same team
    if (ent->client->playerTeam == NPCS.NPC->client->playerTeam)
        return false;

    // if haven't seen him in a while, give up
    // if ( NPCInfo->enemyLastSeenTime != 0 && level.time - NPCInfo->enemyLastSeenTime > 7000 )//FIXME: make a stat?
    //	return false;
    if (entTeam == NPCS.NPC->client->enemyTeam // simplest case: they're on my enemy team
        || (NPCS.NPC->client->enemyTeam == npcteam_t::NPCTEAM_FREE &&
            ent->client->NPC_class != NPCS.NPC->client->NPC_class)         // I get mad at anyone and this guy isn't the same class as me
        || (ent->client->NPC_class == class_t::CLASS_WAMPA && ent->enemy)  // a rampaging wampa
        || (ent->client->NPC_class == class_t::CLASS_RANCOR && ent->enemy) // a rampaging rancor
        || (entTeam == npcteam_t::NPCTEAM_FREE && ent->client->enemyTeam == npcteam_t::NPCTEAM_FREE && ent->enemy && ent->enemy->client &&
            (ent->enemy->client->playerTeam == NPCS.NPC->client->playerTeam ||
             (ent->enemy->client->playerTeam != npcteam_t::NPCTEAM_ENEMY &&
              NPCS.NPC->client->playerTeam ==
                  npcteam_t::NPCTEAM_PLAYER))) // enemy is a rampaging non-aligned creature who is attacking someone on our team or a non-enemy (this last
                                               // condition is used only if we're a good guy - in effect, we protect the innocent)
    ) {
        return true;
    }

    return false;
}

/*
-------------------------
NPC_TargetVisible
-------------------------
*/

bool NPC_TargetVisible(gentity_t* ent)
{

    // Make sure we're in a valid range
    if (DistanceSquared(ent->r.currentOrigin, NPCS.NPC->r.currentOrigin) > (NPCS.NPCInfo->stats.visrange * NPCS.NPCInfo->stats.visrange))
        return false;

    // Check our FOV
    if (InFOV(ent, NPCS.NPC, NPCS.NPCInfo->stats.hfov, NPCS.NPCInfo->stats.vfov) == false)
        return false;

    // Check for sight
    if (NPC_ClearLOS4(ent) == false)
        return false;

    return true;
}

/*
-------------------------
NPC_GetCheckDelta
-------------------------
*/

/*
#define	CHECK_TIME_BASE				250
#define CHECK_TIME_BASE_SQUARED		( CHECK_TIME_BASE * CHECK_TIME_BASE )

static int NPC_GetCheckDelta( void )
{
    if ( NPC_ValidEnemy( NPC->enemy ) == false )
    {
        int distance = DistanceSquared( NPC->r.currentOrigin, g_entities[0].currentOrigin );

        distance /= CHECK_TIME_BASE_SQUARED;

        return ( CHECK_TIME_BASE * distance );
    }

    return 0;
}
*/

/*
-------------------------
NPC_FindNearestEnemy
-------------------------
*/

constexpr int MAX_RADIUS_ENTS = 256; // NOTE: This can cause entities to be lost

int NPC_FindNearestEnemy(gentity_t* ent)
{
    int iradiusEnts[MAX_RADIUS_ENTS];
    gentity_t* radEnt;
    vec3_t mins, maxs;
    int nearestEntID = -1;
    float nearestDist = (float)WORLD_SIZE * (float)WORLD_SIZE;
    float distance;
    int numEnts;
    int i;

    // Setup the bbox to search in
    for (i = 0; i < 3; i++) {
        mins[i] = ent->r.currentOrigin[i] - NPCS.NPCInfo->stats.visrange;
        maxs[i] = ent->r.currentOrigin[i] + NPCS.NPCInfo->stats.visrange;
    }

    // Get a number of entities in a given space
    numEnts = trap->EntitiesInBox(mins, maxs, iradiusEnts, MAX_RADIUS_ENTS);

    for (i = 0; i < numEnts; i++) {
        radEnt = &g_entities[iradiusEnts[i]];
        // Don't consider self
        if (radEnt == ent)
            continue;

        // Must be valid
        if (NPC_ValidEnemy(radEnt) == false)
            continue;

        // Must be visible
        if (NPC_TargetVisible(radEnt) == false)
            continue;

        distance = DistanceSquared(ent->r.currentOrigin, radEnt->r.currentOrigin);

        // Found one closer to us
        if (distance < nearestDist) {
            nearestEntID = radEnt->s.number;
            nearestDist = distance;
        }
    }

    return nearestEntID;
}

/*
-------------------------
NPC_PickEnemyExt
-------------------------
*/

gentity_t* NPC_PickEnemyExt(bool checkAlerts)
{
    // If we've asked for the closest enemy
    int entID = NPC_FindNearestEnemy(NPCS.NPC);

    // If we have a valid enemy, use it
    if (entID >= 0)
        return &g_entities[entID];

    if (checkAlerts) {
        int alertEvent = NPC_CheckAlertEvents(true, true, -1, true, alertEventLevel_e::AEL_DISCOVERED);

        // There is an event to look at
        if (alertEvent >= 0) {
            alertEvent_t* event = &level.alertEvents[alertEvent];

            // Don't pay attention to our own alerts
            if (event->owner == NPCS.NPC)
                return nullptr;

            if (event->level >= alertEventLevel_e::AEL_DISCOVERED) {
                // If it's the player, attack him
                // OJKFIXME: clientnum 0
                if (event->owner == &g_entities[0])
                    return event->owner;

                // If it's on our team, then take its enemy as well
                if ((event->owner->client) && (event->owner->client->playerTeam == NPCS.NPC->client->playerTeam))
                    return event->owner->enemy;
            }
        }
    }

    return nullptr;
}

/*
-------------------------
NPC_FindPlayer
-------------------------
*/

bool NPC_FindPlayer(void)
{
    // OJKFIXME: clientnum 0
    return NPC_TargetVisible(&g_entities[0]);
}

/*
-------------------------
NPC_CheckPlayerDistance
-------------------------
*/

static bool NPC_CheckPlayerDistance(void)
{
    return false; // MOOT in MP
    /*
    float distance;

    //Make sure we have an enemy
    if ( NPC->enemy == nullptr )
        return false;

    //Only do this for non-players
    if ( NPC->enemy->s.number == 0 )
        return false;

    //must be set up to get mad at player
    if ( !NPC->client || NPC->client->enemyTeam != npcteam_t::NPCTEAM_PLAYER )
        return false;

    //Must be within our FOV
    if ( InFOV( &g_entities[0], NPC, NPCInfo->stats.hfov, NPCInfo->stats.vfov ) == false )
        return false;

    distance = DistanceSquared( NPC->r.currentOrigin, NPC->enemy->r.currentOrigin );

    if ( distance > DistanceSquared( NPC->r.currentOrigin, g_entities[0].r.currentOrigin ) )
    { //rwwFIXMEFIXME: care about all clients not just client 0
        G_SetEnemy( NPC, &g_entities[0] );
        return true;
    }

    return false;
    */
}

/*
-------------------------
NPC_FindEnemy
-------------------------
*/

bool NPC_FindEnemy(bool checkAlerts)
{
    gentity_t* newenemy;

    // We're ignoring all enemies for now
    // if( Any( NPC->svFlags & SVF_IGNORE_ENEMIES ) )
    if (0) // rwwFIXMEFIXME: support for flag
    {
        G_ClearEnemy(NPCS.NPC);
        return false;
    }

    // we can't pick up any enemies for now
    if (NPCS.NPCInfo->confusionTime > level.time) {
        return false;
    }

    // Don't want a new enemy
    // rwwFIXMEFIXME: support for locked enemy
    // if ( ( ValidEnemy( NPC->enemy ) ) && Any( NPC->svFlags & SVF_LOCKEDENEMY ) )
    //	return true;

    // See if the player is closer than our current enemy
    if (NPC_CheckPlayerDistance()) {
        return true;
    }

    // Otherwise, turn off the flag
    //	NPC->svFlags &= ~SVF_LOCKEDENEMY;
    // See if the player is closer than our current enemy
    if (NPCS.NPC->client->NPC_class != class_t::CLASS_RANCOR &&
        NPCS.NPC->client->NPC_class != class_t::CLASS_WAMPA
        //&& NPC->client->NPC_class != CLASS_SAND_CREATURE
        && NPC_CheckPlayerDistance()) { // rancors, wampas & sand creatures don't care if player is closer, they always go with closest
        return true;
    }

    // If we've gotten here alright, then our target it still valid
    if (NPC_ValidEnemy(NPCS.NPC->enemy))
        return true;

    newenemy = NPC_PickEnemyExt(checkAlerts);

    // if we found one, take it as the enemy
    if (NPC_ValidEnemy(newenemy)) {
        G_SetEnemy(NPCS.NPC, newenemy);
        return true;
    }

    return false;
}

/*
-------------------------
NPC_CheckEnemyExt
-------------------------
*/

bool NPC_CheckEnemyExt(bool checkAlerts)
{
    // Make sure we're ready to think again
    /*
        if ( NPCInfo->enemyCheckDebounceTime > level.time )
            return false;

        //Get our next think time
        NPCInfo->enemyCheckDebounceTime = level.time + NPC_GetCheckDelta();

        //Attempt to find an enemy
        return NPC_FindEnemy();
    */
    return NPC_FindEnemy(checkAlerts);
}

/*
-------------------------
NPC_FacePosition
-------------------------
*/

bool NPC_FacePosition(vec3_t position, bool doPitch)
{
    vec3_t muzzle;
    vec3_t angles;
    float yawDelta;
    bool facing = true;

    // Get the positions
    if (NPCS.NPC->client && (NPCS.NPC->client->NPC_class == class_t::CLASS_RANCOR ||
                             NPCS.NPC->client->NPC_class == class_t::CLASS_WAMPA)) // || NPC->client->NPC_class == CLASS_SAND_CREATURE) )
    {
        CalcEntitySpot(NPCS.NPC, spot_t::SPOT_ORIGIN, muzzle);
        muzzle[2] += NPCS.NPC->r.maxs[2] * 0.75f;
    }
    else if (NPCS.NPC->client && NPCS.NPC->client->NPC_class == class_t::CLASS_GALAKMECH) {
        CalcEntitySpot(NPCS.NPC, spot_t::SPOT_WEAPON, muzzle);
    }
    else {
        CalcEntitySpot(NPCS.NPC, spot_t::SPOT_HEAD_LEAN, muzzle); // SPOT_HEAD
    }

    // Find the desired angles
    GetAnglesForDirection(muzzle, position, angles);

    NPCS.NPCInfo->desiredYaw = AngleNormalize360(angles[YAW]);
    NPCS.NPCInfo->desiredPitch = AngleNormalize360(angles[PITCH]);

    if (NPCS.NPC->enemy && NPCS.NPC->enemy->client && NPCS.NPC->enemy->client->NPC_class == class_t::CLASS_ATST) {
        // FIXME: this is kind of dumb, but it was the easiest way to get it to look sort of ok
        NPCS.NPCInfo->desiredYaw += Q_flrand(-5, 5) + Q_sin(level.time * 0.004f) * 7;
        NPCS.NPCInfo->desiredPitch += Q_flrand(-2, 2);
    }
    // Face that yaw
    NPC_UpdateAngles(true, true);

    // Find the delta between our goal and our current facing
    yawDelta = AngleNormalize360(NPCS.NPCInfo->desiredYaw - (SHORT2ANGLE(NPCS.ucmd.angles[YAW] + NPCS.client->ps.delta_angles[YAW])));

    // See if we are facing properly
    if (Q_fabs(yawDelta) > VALID_ATTACK_CONE)
        facing = false;

    if (doPitch) {
        // Find the delta between our goal and our current facing
        float currentAngles = (SHORT2ANGLE(NPCS.ucmd.angles[PITCH] + NPCS.client->ps.delta_angles[PITCH]));
        float pitchDelta = NPCS.NPCInfo->desiredPitch - currentAngles;

        // See if we are facing properly
        if (Q_fabs(pitchDelta) > VALID_ATTACK_CONE)
            facing = false;
    }

    return facing;
}

/*
-------------------------
NPC_FaceEntity
-------------------------
*/

bool NPC_FaceEntity(gentity_t* ent, bool doPitch)
{
    vec3_t entPos;

    // Get the positions
    CalcEntitySpot(ent, spot_t::SPOT_HEAD_LEAN, entPos);

    return NPC_FacePosition(entPos, doPitch);
}

/*
-------------------------
NPC_FaceEnemy
-------------------------
*/

bool NPC_FaceEnemy(bool doPitch)
{
    vec3_t enemyPos;

    if (NPCS.NPC == nullptr)
        return false;

    if (NPCS.NPC->enemy == nullptr)
        return false;

    // a shooter aims at the chest of the model it sees, as a player aims; its eyes otherwise (plan step 20 part 10)
    if (!G_AimSpot(NPCS.NPC, NPCS.NPC->enemy, enemyPos)) {
        CalcEntitySpot(NPCS.NPC->enemy, spot_t::SPOT_HEAD_LEAN, enemyPos);
    }
    return NPC_FacePosition(enemyPos, doPitch);
}

/*
-------------------------
NPC_CheckCanAttackExt
-------------------------
*/

bool NPC_CheckCanAttackExt(void)
{
    // We don't want them to shoot
    if (Any(NPCS.NPCInfo->scriptFlags & scriptFlags_t::SCF_DONT_FIRE))
        return false;

    // Turn to face
    if (NPC_FaceEnemy(true) == false)
        return false;

    // Must have a clear line of sight to the target
    if (NPC_ClearShot(NPCS.NPC->enemy) == false)
        return false;

    return true;
}

/*
-------------------------
NPC_ClearLookTarget
-------------------------
*/

void NPC_ClearLookTarget(gentity_t* self)
{
    if (!self->client) {
        return;
    }

    if (Any(self->client->ps.eFlags2 &
            entityFlags2_t::EF2_HELD_BY_MONSTER)) { // lookTarget is set by and to the monster that's holding you, no other operations can change that
        return;
    }

    self->client->renderInfo.lookTarget = ENTITYNUM_NONE; // ENTITYNUM_WORLD;
    self->client->renderInfo.lookTargetClearTime = 0;
}

/*
-------------------------
NPC_SetLookTarget
-------------------------
*/
void NPC_SetLookTarget(gentity_t* self, int entNum, int clearTime)
{
    if (!self->client) {
        return;
    }

    if (Any(self->client->ps.eFlags2 &
            entityFlags2_t::EF2_HELD_BY_MONSTER)) { // lookTarget is set by and to the monster that's holding you, no other operations can change that
        return;
    }

    self->client->renderInfo.lookTarget = entNum;
    self->client->renderInfo.lookTargetClearTime = clearTime;
}

/*
-------------------------
NPC_CheckLookTarget
-------------------------
*/
bool NPC_CheckLookTarget(gentity_t* self)
{
    if (self->client) {
        if (self->client->renderInfo.lookTarget >= 0 && self->client->renderInfo.lookTarget < ENTITYNUM_WORLD) { // within valid range
            if (!g_entities[self->client->renderInfo.lookTarget].inuse) {                                        // lookTarget not inuse or not valid anymore
                NPC_ClearLookTarget(self);
            }
            else if (self->client->renderInfo.lookTargetClearTime && self->client->renderInfo.lookTargetClearTime < level.time) { // Time to clear lookTarget
                NPC_ClearLookTarget(self);
            }
            else if (g_entities[self->client->renderInfo.lookTarget].client && self->enemy &&
                     (&g_entities[self->client->renderInfo.lookTarget] != self->enemy)) { // should always look at current enemy if engaged in battle... FIXME:
                                                                                          // this could override certain scripted lookTargets...???
                NPC_ClearLookTarget(self);
            }
            else {
                return true;
            }
        }
    }

    return false;
}

/*
-------------------------
NPC_CheckCharmed
-------------------------
*/
extern void G_AddVoiceEvent(gentity_t* self, entity_event_t event, int speakDebounceTime);
void NPC_CheckCharmed(void)
{

    if (NPCS.NPCInfo->charmedTime && NPCS.NPCInfo->charmedTime < level.time && NPCS.NPC->client) { // we were charmed, set us back!
        NPCS.NPC->client->playerTeam = (npcteam_t)NPCS.NPC->genericValue1;
        NPCS.NPC->client->enemyTeam = (npcteam_t)NPCS.NPC->genericValue2;
        NPCS.NPC->s.teamowner = NPCS.NPC->genericValue3;

        NPCS.NPC->client->leader = nullptr;
        if (NPCS.NPCInfo->tempBehavior == bState_t::BS_FOLLOW_LEADER) {
            NPCS.NPCInfo->tempBehavior = bState_t::BS_DEFAULT;
        }
        G_ClearEnemy(NPCS.NPC);
        NPCS.NPCInfo->charmedTime = 0;
        // say something to let player know you've snapped out of it
        G_AddVoiceEvent(NPCS.NPC, Q_irand(entity_event_t::EV_CONFUSE1, entity_event_t::EV_CONFUSE3), 2000);
    }
}

void G_GetBoltPosition(gentity_t* self, int boltIndex, vec3_t pos, int modelIndex)
{
    mdxaBone_t boltMatrix;
    vec3_t result, angles;

    if (!self || !self->inuse) {
        return;
    }

    if (self->client) { // clients don't actually even keep r.currentAngles maintained
        VectorSet(angles, 0, self->client->ps.viewangles[YAW], 0);
    }
    else {
        VectorSet(angles, 0, self->r.currentAngles[YAW], 0);
    }

    if (/*!self || ...haha (sorry, i'm tired)*/ !self->ghoul2) {
        return;
    }

    trap->G2API_GetBoltMatrix(self->ghoul2, modelIndex, boltIndex, &boltMatrix, angles, self->r.currentOrigin, level.time, nullptr, self->modelScale);
    if (pos) {
        BG_GiveMeVectorFromMatrix(&boltMatrix, orientations_t::ORIGIN, result);
        VectorCopy(result, pos);
    }
}

float NPC_EntRangeFromBolt(gentity_t* targEnt, int boltIndex)
{
    vec3_t org = {0.0f};

    if (!targEnt) {
        return Q3_INFINITE;
    }

    G_GetBoltPosition(NPCS.NPC, boltIndex, org, 0);

    return (Distance(targEnt->r.currentOrigin, org));
}

float NPC_EnemyRangeFromBolt(int boltIndex)
{
    return (NPC_EntRangeFromBolt(NPCS.NPC->enemy, boltIndex));
}

int NPC_GetEntsNearBolt(int* radiusEnts, float radius, int boltIndex, vec3_t boltOrg)
{
    vec3_t mins, maxs;
    int i;

    // get my handRBolt's position
    vec3_t org = {0.0f};

    G_GetBoltPosition(NPCS.NPC, boltIndex, org, 0);

    VectorCopy(org, boltOrg);

    // Setup the bbox to search in
    for (i = 0; i < 3; i++) {
        mins[i] = boltOrg[i] - radius;
        maxs[i] = boltOrg[i] + radius;
    }

    // Get the number of entities in a given space
    return (trap->EntitiesInBox(mins, maxs, radiusEnts, 128));
}

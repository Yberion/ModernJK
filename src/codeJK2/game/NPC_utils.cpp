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

#include "g_headers.h"

#include "b_local.h"
#include "g_icarus.h"
#include "Q3_Interface.h"

extern int ICARUS_entFilter;
EnumArray<int, team_t, team_t::TEAM_NUM_TEAMS> teamNumbers;
EnumArray<int, team_t, team_t::TEAM_NUM_TEAMS> teamStrength;
EnumArray<int, team_t, team_t::TEAM_NUM_TEAMS> teamCounter;

constexpr float VALID_ATTACK_CONE = 2.0f; // Degrees
void GetAnglesForDirection(const vec3_t p1, const vec3_t p2, vec3_t out);
extern void Q3_DebugPrint(WL_e level, const char* format, ...);

/*
void CalcEntitySpot ( gentity_t *ent, spot_t spot, vec3_t point )

Added: Uses shootAngles if a NPC has them

*/
extern void ViewHeightFix(const gentity_t* const ent);
extern void AddLeanOfs(const gentity_t* const ent, vec3_t point);
extern void SubtractLeanOfs(const gentity_t* const ent, vec3_t point);
void CalcEntitySpot(const gentity_t* ent, const spot_t spot, vec3_t point)
{
    vec3_t forward, up, right;
    vec3_t start, end;
    trace_t tr;

    if (!ent) {
        return;
    }
    ViewHeightFix(ent);
    switch (spot) {
    case spot_t::SPOT_ORIGIN:
        if (VectorCompare(ent->currentOrigin, vec3_origin)) { // brush
            VectorSubtract(ent->absmax, ent->absmin, point);  // size
            VectorMA(ent->absmin, 0.5, point, point);
        }
        else {
            VectorCopy(ent->currentOrigin, point);
        }
        break;

    case spot_t::SPOT_CHEST:
    case spot_t::SPOT_HEAD:
        if (ent->client && VectorLengthSquared(ent->client->renderInfo.eyePoint) &&
            (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)) { // Actual tag_head eyespot!
            // FIXME: Stasis aliens may have a problem here...
            VectorCopy(ent->client->renderInfo.eyePoint, point);
            if (ent->client->NPC_class == class_t::CLASS_ATST) { // adjust up some
                point[2] += 28;                                  // magic number :)
            }
            if (ent->NPC) { // always aim from the center of my bbox, so we don't wiggle when we lean forward or backwards
                point[0] = ent->currentOrigin[0];
                point[1] = ent->currentOrigin[1];
            }
            else if (!ent->s.number) {
                SubtractLeanOfs(ent, point);
            }
        }
        else {
            VectorCopy(ent->currentOrigin, point);
            if (ent->client) {
                point[2] += ent->client->ps.viewheight;
            }
        }
        if (spot == spot_t::SPOT_CHEST && ent->client) {
            if (ent->client->NPC_class != class_t::CLASS_ATST) { // adjust up some
                point[2] -= ent->maxs[2] * 0.2f;
            }
        }
        break;

    case spot_t::SPOT_HEAD_LEAN:
        if (ent->client && VectorLengthSquared(ent->client->renderInfo.eyePoint) &&
            (ent->client->ps.viewEntity <= 0 || ent->client->ps.viewEntity >= ENTITYNUM_WORLD)) { // Actual tag_head eyespot!
            // FIXME: Stasis aliens may have a problem here...
            VectorCopy(ent->client->renderInfo.eyePoint, point);
            if (ent->client->NPC_class == class_t::CLASS_ATST) { // adjust up some
                point[2] += 28;                                  // magic number :)
            }
            if (ent->NPC) { // always aim from the center of my bbox, so we don't wiggle when we lean forward or backwards
                point[0] = ent->currentOrigin[0];
                point[1] = ent->currentOrigin[1];
            }
            else if (!ent->s.number) {
                SubtractLeanOfs(ent, point);
            }
            // NOTE: automatically takes leaning into account!
        }
        else {
            VectorCopy(ent->currentOrigin, point);
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
        VectorCopy(ent->currentOrigin, point);
        point[2] += (ent->mins[2] * 0.5);
        break;

    case spot_t::SPOT_WEAPON:
        if (ent->NPC && !VectorCompare(ent->NPC->shootAngles, vec3_origin) && !VectorCompare(ent->NPC->shootAngles, ent->client->ps.viewangles)) {
            AngleVectors(ent->NPC->shootAngles, forward, right, up);
        }
        else {
            AngleVectors(ent->client->ps.viewangles, forward, right, up);
        }
        CalcMuzzlePoint((gentity_t*)ent, forward, right, up, point, 0);
        // NOTE: automatically takes leaning into account!
        break;

    case spot_t::SPOT_GROUND:
        // if entity is on the ground, just use it's absmin
        if (ent->s.groundEntityNum != -1) {
            VectorCopy(ent->currentOrigin, point);
            point[2] = ent->absmin[2];
            break;
        }

        // if it is reasonably close to the ground, give the point underneath of it
        VectorCopy(ent->currentOrigin, start);
        start[2] = ent->absmin[2];
        VectorCopy(start, end);
        end[2] -= 64;
        gi.trace(&tr, start, ent->mins, ent->maxs, end, ent->s.number, MASK_PLAYERSOLID, EG2_Collision::G2_NOCOLLIDE, 0);
        if (tr.fraction < 1.0) {
            VectorCopy(tr.endpos, point);
            break;
        }

        // otherwise just use the origin
        VectorCopy(ent->currentOrigin, point);
        break;

    default:
        VectorCopy(ent->currentOrigin, point);
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
extern cvar_t* g_timescale;
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
    if (!NPC->enemy && ((level.time < NPCInfo->aimTime) || NPC->client->renderInfo.renderFlags & RF_LOCKEDANGLE)) {
        if (doPitch)
            targetPitch = NPCInfo->lockedDesiredPitch;

        if (doYaw)
            targetYaw = NPCInfo->lockedDesiredYaw;
    }
    else {
        // we're changing the lockedDesired Pitch/Yaw below so it's lost it's original meaning, get rid of the lock flag
        NPC->client->renderInfo.renderFlags &= ~RF_LOCKEDANGLE;

        if (doPitch) {
            targetPitch = NPCInfo->desiredPitch;
            NPCInfo->lockedDesiredPitch = NPCInfo->desiredPitch;
        }

        if (doYaw) {
            targetYaw = NPCInfo->desiredYaw;
            NPCInfo->lockedDesiredYaw = NPCInfo->desiredYaw;
        }
    }

    if (NPC->s.weapon == weapon_t::WP_EMPLACED_GUN) {
        // FIXME: this seems to do nothing, actually...
        yawSpeed = 20;
    }
    else {
        yawSpeed = NPCInfo->stats.yawSpeed;
    }

    if (NPC->s.weapon == weapon_t::WP_SABER && NPC->client->ps.forcePowersActive & Bit(forcePowers_t::FP_SPEED)) {
        yawSpeed *= 1.0f / g_timescale->value;
    }

    if (doYaw) {
        // decay yaw error
        error = AngleDelta(NPC->client->ps.viewangles[YAW], targetYaw);
        if (Q_fabsf(error) > MIN_ANGLE_ERROR) {
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

        ucmd.angles[YAW] = ANGLE2SHORT(targetYaw + error) - client->ps.delta_angles[YAW];
    }

    // FIXME: have a pitchSpeed?
    if (doPitch) {
        // decay pitch error
        error = AngleDelta(NPC->client->ps.viewangles[PITCH], targetPitch);
        if (Q_fabsf(error) > MIN_ANGLE_ERROR) {
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

        ucmd.angles[PITCH] = ANGLE2SHORT(targetPitch + error) - client->ps.delta_angles[PITCH];
    }

    ucmd.angles[ROLL] = ANGLE2SHORT(NPC->client->ps.viewangles[ROLL]) - client->ps.delta_angles[ROLL];

    if (exact && Q3_TaskIDPending(NPC, taskID_t::TID_ANGLE_FACE)) {
        Q3_TaskIDComplete(NPC, taskID_t::TID_ANGLE_FACE);
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
        if (Q_fabsf(error) > MIN_ANGLE_ERROR) {
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
        if (Q_fabsf(error) > MIN_ANGLE_ERROR) {
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
    if (NPCInfo->aimErrorDebounceTime < level.time) {
        NPCInfo->aimOfs[0] = 0.3 * Q_flrand(NPC->enemy->mins[0], NPC->enemy->maxs[0]);
        NPCInfo->aimOfs[1] = 0.3 * Q_flrand(NPC->enemy->mins[1], NPC->enemy->maxs[1]);
        if (NPC->enemy->maxs[2] > 0) {
            NPCInfo->aimOfs[2] = NPC->enemy->maxs[2] * Q_flrand(0.0f, -1.0f);
        }
    }
    VectorAdd(enemy_org, NPCInfo->aimOfs, enemy_org);
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

    if (NPCInfo->aimErrorDebounceTime < level.time) {
        if (Q_irand(0, 1)) {
            NPCInfo->lastAimErrorYaw = ((float)(6 - NPCInfo->stats.aim)) * Q_flrand(-1, 1);
        }
        if (Q_irand(0, 1)) {
            NPCInfo->lastAimErrorPitch = ((float)(6 - NPCInfo->stats.aim)) * Q_flrand(-1, 1);
        }
        NPCInfo->aimErrorDebounceTime = level.time + Q_irand(250, 2000);
    }

    if (doYaw) {
        // decay yaw diff
        diff = AngleDelta(NPC->client->ps.viewangles[YAW], targetYaw);

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
        error = NPCInfo->lastAimErrorYaw;

        /*
        if(Q_irand(0, 1))
        {
            error *= -1;
        }
        */

        ucmd.angles[YAW] = ANGLE2SHORT(targetYaw + diff + error) - client->ps.delta_angles[YAW];
    }

    if (doPitch) {
        // decay pitch diff
        diff = AngleDelta(NPC->client->ps.viewangles[PITCH], targetPitch);
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

        error = NPCInfo->lastAimErrorPitch;

        ucmd.angles[PITCH] = ANGLE2SHORT(targetPitch + diff + error) - client->ps.delta_angles[PITCH];
    }

    ucmd.angles[ROLL] = ANGLE2SHORT(NPC->client->ps.viewangles[ROLL]) - client->ps.delta_angles[ROLL];

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
        error = AngleDelta(NPCInfo->shootAngles[YAW], targetYaw);
        if (error) {
            decay = 60.0 + 80.0 * NPCInfo->stats.aim;
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
        NPCInfo->shootAngles[YAW] = targetYaw + error;
    }

    if (doPitch) {
        // decay pitch error
        error = AngleDelta(NPCInfo->shootAngles[PITCH], targetPitch);
        if (error) {
            decay = 60.0 + 80.0 * NPCInfo->stats.aim;
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
        NPCInfo->shootAngles[PITCH] = targetPitch + error;
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

    for (team_t t = team_t::TEAM_FREE; t < team_t::TEAM_NUM_TEAMS; t++) { // Get the average health
        teamStrength[t] = Q_floorf(((float)(teamStrength[t])) / ((float)(teamNumbers[t])));
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
        if ((ICARUS_entFilter == -1) || (ICARUS_entFilter == self->s.number)) {
            Q3_DebugPrint(WL_e::WL_VERBOSE, "%s attempting to run bSet %s (%s)\n", self->targetname, GetStringForID(BSETTable, to_underlying(bset)), bs_name);
        }
        ICARUS_RunScript(self, va("%s/%s", Q3_SCRIPT_DIR, bs_name));
    }
    return true;
}

/*
=============================================================================

    Extended Functions

=============================================================================
*/

/*
-------------------------
NPC_ValidEnemy
-------------------------
*/

bool NPC_ValidEnemy(gentity_t* ent)
{
    // Must be a valid pointer
    if (ent == nullptr)
        return false;

    // Must not be me
    if (ent == NPC)
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
        if (Any(ent->svFlags & svFlags_t::SVF_NONNPC_ENEMY)) { // still potentially valid
            if (ent->noDamageTeam == NPC->client->playerTeam) {
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

    // Can't be on the same team
    if (ent->client->playerTeam == NPC->client->playerTeam)
        return false;

    // if haven't seen him in a while, give up
    // if ( NPCInfo->enemyLastSeenTime != 0 && level.time - NPCInfo->enemyLastSeenTime > 7000 )//FIXME: make a stat?
    //	return false;

    return true;
}

/*
-------------------------
NPC_TargetVisible
-------------------------
*/

bool NPC_TargetVisible(gentity_t* ent)
{
    // Make sure we're in a valid range
    if (DistanceSquared(ent->currentOrigin, NPC->currentOrigin) > (NPCInfo->stats.visrange * NPCInfo->stats.visrange))
        return false;

    // Check our FOV
    if (InFOV(ent, NPC, NPCInfo->stats.hfov, NPCInfo->stats.vfov) == false)
        return false;

    // Check for sight
    if (NPC_ClearLOS(ent) == false)
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
        int distance = DistanceSquared( NPC->currentOrigin, g_entities[0].currentOrigin );

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
    gentity_t* radiusEnts[MAX_RADIUS_ENTS];
    vec3_t mins, maxs;
    int nearestEntID = -1;
    float nearestDist = (float)WORLD_SIZE * (float)WORLD_SIZE;
    float distance;
    int numEnts;
    int i;

    // Setup the bbox to search in
    for (i = 0; i < 3; i++) {
        mins[i] = ent->currentOrigin[i] - NPCInfo->stats.visrange;
        maxs[i] = ent->currentOrigin[i] + NPCInfo->stats.visrange;
    }

    // Get a number of entities in a given space
    numEnts = gi.EntitiesInBox(mins, maxs, radiusEnts, MAX_RADIUS_ENTS);

    for (i = 0; i < numEnts; i++) {
        // Don't consider self
        if (radiusEnts[i] == ent)
            continue;

        // Must be valid
        if (NPC_ValidEnemy(radiusEnts[i]) == false)
            continue;

        // Must be visible
        if (NPC_TargetVisible(radiusEnts[i]) == false)
            continue;

        distance = DistanceSquared(ent->currentOrigin, radiusEnts[i]->currentOrigin);

        // Found one closer to us
        if (distance < nearestDist) {
            nearestEntID = radiusEnts[i]->s.number;
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

gentity_t* NPC_PickEnemyExt(bool checkAlerts = false)
{
    // If we've asked for the closest enemy
    int entID = NPC_FindNearestEnemy(NPC);

    // If we have a valid enemy, use it
    if (entID >= 0)
        return &g_entities[entID];

    if (checkAlerts) {
        int alertEvent = NPC_CheckAlertEvents(true, true, -1, true, alertEventLevel_e::AEL_DISCOVERED);

        // There is an event to look at
        if (alertEvent >= 0) {
            alertEvent_t* event = &level.alertEvents[alertEvent];

            // Don't pay attention to our own alerts
            if (event->owner == NPC)
                return nullptr;

            if (event->level >= alertEventLevel_e::AEL_DISCOVERED) {
                // If it's the player, attack him
                if (event->owner == &g_entities[0])
                    return event->owner;

                // If it's on our team, then take its enemy as well
                if ((event->owner->client) && (event->owner->client->playerTeam == NPC->client->playerTeam))
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
    return NPC_TargetVisible(&g_entities[0]);
}

/*
-------------------------
NPC_CheckPlayerDistance
-------------------------
*/

static bool NPC_CheckPlayerDistance(void)
{
    // Make sure we have an enemy
    if (NPC->enemy == nullptr)
        return false;

    // Only do this for non-players
    if (NPC->enemy->s.number == 0)
        return false;

    // must be set up to get mad at player
    if (!NPC->client || NPC->client->enemyTeam != team_t::TEAM_PLAYER)
        return false;

    // Must be within our FOV
    if (InFOV(&g_entities[0], NPC, NPCInfo->stats.hfov, NPCInfo->stats.vfov) == false)
        return false;

    float distance = DistanceSquared(NPC->currentOrigin, NPC->enemy->currentOrigin);

    if (distance > DistanceSquared(NPC->currentOrigin, g_entities[0].currentOrigin)) {
        G_SetEnemy(NPC, &g_entities[0]);
        return true;
    }

    return false;
}

/*
-------------------------
NPC_FindEnemy
-------------------------
*/

bool NPC_FindEnemy(bool checkAlerts = false)
{
    // We're ignoring all enemies for now
    if (Any(NPC->svFlags & svFlags_t::SVF_IGNORE_ENEMIES)) {
        G_ClearEnemy(NPC);
        return false;
    }

    // we can't pick up any enemies for now
    if (NPCInfo->confusionTime > level.time) {
        return false;
    }

    // Don't want a new enemy
    if ((ValidEnemy(NPC->enemy)) && Any(NPC->svFlags & svFlags_t::SVF_LOCKEDENEMY))
        return true;

    // See if the player is closer than our current enemy
    if (NPC_CheckPlayerDistance()) {
        return true;
    }

    // Otherwise, turn off the flag
    NPC->svFlags &= ~svFlags_t::SVF_LOCKEDENEMY;

    // If we've gotten here alright, then our target it still valid
    if (NPC_ValidEnemy(NPC->enemy))
        return true;

    gentity_t* newenemy = NPC_PickEnemyExt(checkAlerts);

    // if we found one, take it as the enemy
    if (NPC_ValidEnemy(newenemy)) {
        G_SetEnemy(NPC, newenemy);
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
    bool facing = true;

    // Get the positions
    if (NPC->client && NPC->client->NPC_class == class_t::CLASS_GALAKMECH) {
        CalcEntitySpot(NPC, spot_t::SPOT_WEAPON, muzzle);
    }
    else {
        CalcEntitySpot(NPC, spot_t::SPOT_HEAD_LEAN, muzzle); // SPOT_HEAD
    }

    // Find the desired angles
    vec3_t angles;

    GetAnglesForDirection(muzzle, position, angles);

    NPCInfo->desiredYaw = AngleNormalize360(angles[YAW]);
    NPCInfo->desiredPitch = AngleNormalize360(angles[PITCH]);

    if (NPC->enemy && NPC->enemy->client && NPC->enemy->client->NPC_class == class_t::CLASS_ATST) {
        // FIXME: this is kind of dumb, but it was the easiest way to get it to look sort of ok
        NPCInfo->desiredYaw += Q_flrand(-5, 5) + Q_sinf(level.time * 0.004f) * 7;
        NPCInfo->desiredPitch += Q_flrand(-2, 2);
    }
    // Face that yaw
    NPC_UpdateAngles(true, true);

    // Find the delta between our goal and our current facing
    float yawDelta = AngleNormalize360(NPCInfo->desiredYaw - (SHORT2ANGLE(ucmd.angles[YAW] + client->ps.delta_angles[YAW])));

    // See if we are facing properly
    if (Q_fabsf(yawDelta) > VALID_ATTACK_CONE)
        facing = false;

    if (doPitch) {
        // Find the delta between our goal and our current facing
        float currentAngles = (SHORT2ANGLE(ucmd.angles[PITCH] + client->ps.delta_angles[PITCH]));
        float pitchDelta = NPCInfo->desiredPitch - currentAngles;

        // See if we are facing properly
        if (Q_fabsf(pitchDelta) > VALID_ATTACK_CONE)
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
    if (NPC == nullptr)
        return false;

    if (NPC->enemy == nullptr)
        return false;

    return NPC_FaceEntity(NPC->enemy, doPitch);
}

/*
-------------------------
NPC_CheckCanAttackExt
-------------------------
*/

bool NPC_CheckCanAttackExt(void)
{
    // We don't want them to shoot
    if (Any(NPCInfo->scriptFlags & scriptFlags_t::SCF_DONT_FIRE))
        return false;

    // Turn to face
    if (NPC_FaceEnemy(true) == false)
        return false;

    // Must have a clear line of sight to the target
    if (NPC_ClearShot(NPC->enemy) == false)
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
    if (NPC->client->playerTeam == team_t::TEAM_PLAYER && NPCInfo->charmedTime && NPCInfo->charmedTime < level.time &&
        NPC->client) { // we were charmed, set us back!
        // NOTE: presumptions here...
        team_t savTeam = NPC->client->enemyTeam;
        NPC->client->enemyTeam = NPC->client->playerTeam;
        NPC->client->playerTeam = savTeam;
        NPC->client->leader = nullptr;
        if (NPCInfo->tempBehavior == bState_t::BS_FOLLOW_LEADER) {
            NPCInfo->tempBehavior = bState_t::BS_DEFAULT;
        }
        G_ClearEnemy(NPC);
        NPCInfo->charmedTime = 0;
        // say something to let player know you've snapped out of it
        G_AddVoiceEvent(NPC, Q_irand(entity_event_t::EV_CONFUSE1, entity_event_t::EV_CONFUSE3), 2000);
    }
}

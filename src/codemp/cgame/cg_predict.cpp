/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
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

// cg_predict.cpp -- this file generates cg.predictedPlayerState by either
// interpolating between snapshots from the server or locally predicting
// ahead the client's movement.
// It also handles local physics interaction, like fragments bouncing off walls

#include "cg_local.h"

static pmove_t cg_pmove;

static int cg_numSolidEntities;
static centity_t* cg_solidEntities[MAX_ENTITIES_IN_SNAPSHOT];
static int cg_numTriggerEntities;
static centity_t* cg_triggerEntities[MAX_ENTITIES_IN_SNAPSHOT];

// is this client piloting this veh?
static QINLINE bool CG_Piloting(int vehNum)
{
    centity_t* veh;

    if (!vehNum) {
        return false;
    }

    veh = &cg_entities[vehNum];

    if (veh->currentState.owner != cg.predictedPlayerState.clientNum) { // the owner should be the current pilot
        return false;
    }

    return true;
}

/*
====================
CG_BuildSolidList

When a new cg.snap has been set, this function builds a sublist
of the entities that are actually solid, to make for more
efficient collision detection
====================
*/
void CG_BuildSolidList(void)
{
    int i;
    centity_t* cent;
    snapshot_t* snap;
    entityState_t* ent;
    vec3_t difference;
    float dsquared;

    cg_numSolidEntities = 0;
    cg_numTriggerEntities = 0;

    if (cg.nextSnap && !cg.nextFrameTeleport && !cg.thisFrameTeleport) {
        snap = cg.nextSnap;
    }
    else {
        snap = cg.snap;
    }

    for (i = 0; i < snap->numEntities; i++) {
        cent = &cg_entities[snap->entities[i].number];
        ent = &cent->currentState;

        if (ent->eType == entityType_t::ET_ITEM || ent->eType == entityType_t::ET_PUSH_TRIGGER || ent->eType == entityType_t::ET_TELEPORT_TRIGGER) {
            cg_triggerEntities[cg_numTriggerEntities] = cent;
            cg_numTriggerEntities++;
            continue;
        }

        if (cent->nextState.solid) {
            cg_solidEntities[cg_numSolidEntities] = cent;
            cg_numSolidEntities++;
            continue;
        }
    }

    // rww - Horrible, terrible, awful hack.
    // We don't send your client entity from the server,
    // so it isn't added into the solid list from the snapshot,
    // and in addition, it has no solid data. So we will force
    // adding it in based on a hardcoded player bbox size.
    // This will cause issues if the player box size is ever
    // changed..
    if (cg_numSolidEntities < MAX_ENTITIES_IN_SNAPSHOT) {
        vec3_t playerMins = {-15, -15, DEFAULT_MINS_2};
        vec3_t playerMaxs = {15, 15, DEFAULT_MAXS_2};
        int i, j, k;

        i = playerMaxs[0];
        if (i < 1)
            i = 1;
        if (i > 255)
            i = 255;

        // z is not symetric
        j = (-playerMins[2]);
        if (j < 1)
            j = 1;
        if (j > 255)
            j = 255;

        // and z playerMaxs can be negative...
        k = (playerMaxs[2] + 32);
        if (k < 1)
            k = 1;
        if (k > 255)
            k = 255;

        cg_solidEntities[cg_numSolidEntities] = &cg_entities[cg.predictedPlayerState.clientNum];
        cg_solidEntities[cg_numSolidEntities]->currentState.solid = (k << 16) | (j << 8) | i;

        cg_numSolidEntities++;
    }

    dsquared = /*RMG_distancecull.value*/ 5000 + 500;
    dsquared *= dsquared;

    for (i = 0; i < cg_numpermanents; i++) {
        cent = cg_permanents[i];
        VectorSubtract(cent->lerpOrigin, snap->ps.origin, difference);
        if (cent->currentState.eType == entityType_t::ET_TERRAIN ||
            ((difference[0] * difference[0]) + (difference[1] * difference[1]) + (difference[2] * difference[2])) <= dsquared) {
            cent->currentValid = true;
            if (cent->nextState.solid) {
                cg_solidEntities[cg_numSolidEntities] = cent;
                cg_numSolidEntities++;
            }
        }
        else {
            cent->currentValid = false;
        }
    }
}

static QINLINE bool CG_VehicleClipCheck(centity_t* ignored, trace_t* trace)
{
    if (!trace || trace->entityNum < 0 || trace->entityNum >= ENTITYNUM_WORLD) { // it's alright then
        return true;
    }

    if (ignored->currentState.eType != entityType_t::ET_PLAYER && ignored->currentState.eType != entityType_t::ET_NPC) { // can't possibly be valid then
        return true;
    }

    if (ignored->currentState.m_iVehicleNum) { // see if the ignore ent is a vehicle/rider - if so, see if the ent we supposedly hit is a vehicle/rider.
        // if they belong to each other, we don't want to collide them.
        centity_t* otherguy = &cg_entities[trace->entityNum];

        if (otherguy->currentState.eType != entityType_t::ET_PLAYER && otherguy->currentState.eType != entityType_t::ET_NPC) { // can't possibly be valid then
            return true;
        }

        if (otherguy->currentState.m_iVehicleNum) { // alright, both of these are either a vehicle or a player who is on a vehicle
            int index;

            if (ignored->currentState.eType == entityType_t::ET_PLAYER ||
                (ignored->currentState.eType == entityType_t::ET_NPC &&
                 ignored->currentState.NPC_class != class_t::CLASS_VEHICLE)) { // must be a player or NPC riding a vehicle
                index = ignored->currentState.m_iVehicleNum;
            }
            else { // a vehicle
                index = ignored->currentState.m_iVehicleNum - 1;
            }

            if (index == otherguy->currentState.number) { // this means we're riding or being ridden by this guy, so don't collide
                return false;
            }
            else { // see if I'm hitting one of my own passengers
                if (otherguy->currentState.eType == entityType_t::ET_PLAYER ||
                    (otherguy->currentState.eType == entityType_t::ET_NPC &&
                     otherguy->currentState.NPC_class != class_t::CLASS_VEHICLE)) {             // must be a player or NPC riding a vehicle
                    if (otherguy->currentState.m_iVehicleNum == ignored->currentState.number) { // this means we're other guy is riding the ignored ent
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

/*
====================
CG_ClipMoveToEntities

====================
*/
extern void BG_VehicleAdjustBBoxForOrientation(Vehicle_t* veh, vec3_t origin, vec3_t mins, vec3_t maxs, int clientNum, contentsFlags_t tracemask,
                                               void (*localTrace)(trace_t* results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end,
                                                                  int passEntityNum, contentsFlags_t contentMask)); // bg_pmove.cpp
static void CG_ClipMoveToEntities(const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int skipNumber, contentsFlags_t mask, trace_t* tr,
                                  bool g2Check)
{
    int i, x, zd, zu;
    trace_t trace, oldTrace{};
    entityState_t* ent;
    clipHandle_t cmodel;
    vec3_t bmins, bmaxs;
    vec3_t origin, angles;
    centity_t* cent;
    centity_t* ignored = nullptr;

    if (skipNumber != -1 && skipNumber != ENTITYNUM_NONE) {
        ignored = &cg_entities[skipNumber];
    }

    for (i = 0; i < cg_numSolidEntities; i++) {
        cent = cg_solidEntities[i];
        ent = &cent->currentState;

        if (ent->number == skipNumber) {
            continue;
        }

        if (ent->number > MAX_CLIENTS && (ent->genericenemyindex - MAX_GENTITIES == cg.predictedPlayerState.clientNum ||
                                          ent->genericenemyindex - MAX_GENTITIES == cg.predictedVehicleState.clientNum))
        //		if (ent->number > MAX_CLIENTS && cg.snap && ent->genericenemyindex && (ent->genericenemyindex-MAX_GENTITIES) == cg.snap->ps.clientNum)
        { // rww - method of keeping objects from colliding in client-prediction (in case of ownership)
            continue;
        }

        if (ent->solid == SOLID_BMODEL) {
            // special value for bmodel
            cmodel = trap->CM_InlineModel(ent->modelindex);
            VectorCopy(cent->lerpAngles, angles);
            BG_EvaluateTrajectory(&cent->currentState.pos, cg.physicsTime, origin);
        }
        else {
            // encoded bbox
            x = (ent->solid & 255);
            zd = ((ent->solid >> 8) & 255);
            zu = ((ent->solid >> 16) & 255) - 32;

            bmins[0] = bmins[1] = -x;
            bmaxs[0] = bmaxs[1] = x;
            bmins[2] = -zd;
            bmaxs[2] = zu;

            if (ent->eType == entityType_t::ET_NPC && ent->NPC_class == class_t::CLASS_VEHICLE &&
                cent->m_pVehicle) { // try to dynamically adjust his bbox dynamically, if possible
                float* old = cent->m_pVehicle->m_vOrientation;
                cent->m_pVehicle->m_vOrientation = &cent->lerpAngles[0];
                BG_VehicleAdjustBBoxForOrientation(cent->m_pVehicle, cent->lerpOrigin, bmins, bmaxs, cent->currentState.number, MASK_PLAYERSOLID, nullptr);
                cent->m_pVehicle->m_vOrientation = old;
            }

            cmodel = trap->CM_TempModel(bmins, bmaxs, 0);
            VectorCopy(vec3_origin, angles);

            VectorCopy(cent->lerpOrigin, origin);
        }

        trap->CM_TransformedTrace(&trace, start, end, mins, maxs, cmodel, mask, origin, angles, 0);
        trace.entityNum = trace.fraction != 1.0 ? ent->number : ENTITYNUM_NONE;

        if (g2Check || (ignored && ignored->currentState.m_iVehicleNum)) {
            // keep these older variables around for a bit, incase we need to replace them in the Ghoul2 Collision check
            // or in the vehicle owner trace check
            oldTrace = *tr;
        }

        if (trace.allsolid || trace.fraction < tr->fraction) {
            trace.entityNum = ent->number;
            *tr = trace;
        }
        else if (trace.startsolid) {
            tr->startsolid = true;

            // rww 12-02-02
            tr->entityNum = trace.entityNum = ent->number;
        }
        if (tr->allsolid) {
            if (ignored && ignored->currentState.m_iVehicleNum) {
                trace.entityNum = ent->number;
                if (CG_VehicleClipCheck(ignored, &trace)) { // this isn't our vehicle, we're really stuck
                    return;
                }
                else { // it's alright, keep going
                    trace = oldTrace;
                    *tr = trace;
                }
            }
            else {
                return;
            }
        }

        if (g2Check) {
            if (trace.entityNum == ent->number && cent->ghoul2) {
                CG_G2TraceCollide(&trace, mins, maxs, start, end);

                if (trace.entityNum == ENTITYNUM_NONE) { // g2 trace failed, so put it back where it was.
                    trace = oldTrace;
                    *tr = trace;
                }
            }
        }

        if (ignored && ignored->currentState.m_iVehicleNum) { // see if this is the vehicle we hit
            centity_t* hit = &cg_entities[trace.entityNum];
            if (!CG_VehicleClipCheck(ignored, &trace)) { // looks like it
                trace = oldTrace;
                *tr = trace;
            }
            else if (hit->currentState.eType == entityType_t::ET_MISSILE &&
                     hit->currentState.owner == ignored->currentState.number) { // hack, don't hit own missiles
                trace = oldTrace;
                *tr = trace;
            }
        }
    }
}

/*
================
CG_Trace
================
*/
void CG_Trace(trace_t* result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int skipNumber, contentsFlags_t mask)
{
    trace_t t;

    trap->CM_Trace(&t, start, end, mins, maxs, 0, mask, 0);
    t.entityNum = t.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
    // check all other solid models
    CG_ClipMoveToEntities(start, mins, maxs, end, skipNumber, mask, &t, false);

    *result = t;
}

/*
================
CG_G2Trace
================
*/
void CG_G2Trace(trace_t* result, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int skipNumber, contentsFlags_t mask)
{
    trace_t t;

    trap->CM_Trace(&t, start, end, mins, maxs, 0, mask, 0);
    t.entityNum = t.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
    // check all other solid models
    CG_ClipMoveToEntities(start, mins, maxs, end, skipNumber, mask, &t, true);

    *result = t;
}

/*
================
CG_PointContents
================
*/
contentsFlags_t CG_PointContents(const vec3_t point, int passEntityNum)
{
    int i;
    entityState_t* ent;
    centity_t* cent;
    clipHandle_t cmodel;
    contentsFlags_t contents;

    contents = trap->CM_PointContents(point, 0);

    for (i = 0; i < cg_numSolidEntities; i++) {
        cent = cg_solidEntities[i];

        ent = &cent->currentState;

        if (ent->number == passEntityNum) {
            continue;
        }

        if (ent->solid != SOLID_BMODEL) { // special value for bmodel
            continue;
        }

        cmodel = trap->CM_InlineModel(ent->modelindex);
        if (!cmodel) {
            continue;
        }

        contents |= trap->CM_TransformedPointContents(point, cmodel, cent->lerpOrigin, cent->lerpAngles);
    }

    return contents;
}

/*
========================
CG_InterpolatePlayerState

Generates cg.predictedPlayerState by interpolating between
cg.snap->ps and cg.nextFrame->ps
========================
*/
static void CG_InterpolatePlayerState(bool grabAngles)
{
    float f;
    int i;
    playerState_t* out;
    snapshot_t *prev, *next;
    vec3_t nextOrigin;

    out = &cg.predictedPlayerState;
    prev = cg.snap;
    next = cg.nextSnap;

    *out = cg.snap->ps;
    CG_CarryWithMover(out->origin, out->groundEntityNum, prev->serverTime); // standing on a mover

    // if we are still allowing local input, short circuit the view angles
    if (grabAngles) {
        usercmd_t cmd;
        int cmdNum;

        cmdNum = trap->GetCurrentCmdNumber();
        trap->GetUserCmd(cmdNum, &cmd);

        PM_UpdateViewAngles(out, &cmd);
    }

    // if the next frame is a teleport, we can't lerp to it
    if (cg.nextFrameTeleport) {
        return;
    }

    if (!next || next->serverTime <= prev->serverTime) {
        return;
    }

    f = (float)(cg.time - prev->serverTime) / (next->serverTime - prev->serverTime);

    i = next->ps.bobCycle;
    if (i < prev->ps.bobCycle) {
        i += 256; // handle wraparound
    }
    out->bobCycle = prev->ps.bobCycle + f * (i - prev->ps.bobCycle);

    VectorCopy(next->ps.origin, nextOrigin);
    CG_CarryWithMover(nextOrigin, next->ps.groundEntityNum, next->serverTime);
    for (i = 0; i < 3; i++) {
        out->origin[i] = out->origin[i] + f * (nextOrigin[i] - out->origin[i]);
        if (!grabAngles) {
            out->viewangles[i] = LerpAngle(prev->ps.viewangles[i], next->ps.viewangles[i], f);
        }
        out->velocity[i] = prev->ps.velocity[i] + f * (next->ps.velocity[i] - prev->ps.velocity[i]);
    }
}

static void CG_InterpolateVehiclePlayerState(bool grabAngles)
{
    float f;
    int i;
    playerState_t* out;
    snapshot_t *prev, *next;

    out = &cg.predictedVehicleState;
    prev = cg.snap;
    next = cg.nextSnap;

    *out = cg.snap->vps;

    // if we are still allowing local input, short circuit the view angles
    if (grabAngles) {
        usercmd_t cmd;
        int cmdNum;

        cmdNum = trap->GetCurrentCmdNumber();
        trap->GetUserCmd(cmdNum, &cmd);

        PM_UpdateViewAngles(out, &cmd);
    }

    // if the next frame is a teleport, we can't lerp to it
    if (cg.nextFrameTeleport) {
        return;
    }

    if (!next || next->serverTime <= prev->serverTime) {
        return;
    }

    f = (float)(cg.time - prev->serverTime) / (next->serverTime - prev->serverTime);

    i = next->vps.bobCycle;
    if (i < prev->vps.bobCycle) {
        i += 256; // handle wraparound
    }
    out->bobCycle = prev->vps.bobCycle + f * (i - prev->vps.bobCycle);

    for (i = 0; i < 3; i++) {
        out->origin[i] = prev->vps.origin[i] + f * (next->vps.origin[i] - prev->vps.origin[i]);
        if (!grabAngles) {
            out->viewangles[i] = LerpAngle(prev->vps.viewangles[i], next->vps.viewangles[i], f);
        }
        out->velocity[i] = prev->vps.velocity[i] + f * (next->vps.velocity[i] - prev->vps.velocity[i]);
    }
}

/*
===================
CG_TouchItem
===================
*/
static void CG_TouchItem(centity_t* cent)
{
    gitem_t* item;

    if (!cg_predictItems.integer) {
        return;
    }
    if (!BG_PlayerTouchesItem(&cg.predictedPlayerState, &cent->currentState, cg.time)) {
        return;
    }

    if (cent->currentState.brokenLimbs) { // dropped item
        return;
    }

    if (Any(cent->currentState.eFlags & entityFlags_t::EF_ITEMPLACEHOLDER)) {
        return;
    }

    if (Any(cent->currentState.eFlags & entityFlags_t::EF_NODRAW)) {
        return;
    }

    // never pick an item up twice in a prediction
    if (cent->miscTime == cg.time) {
        return;
    }

    if (!BG_CanItemBeGrabbed(cgs.gametype, &cent->currentState, &cg.predictedPlayerState)) {
        return; // can't hold it
    }

    item = &bg_itemlist[cent->currentState.modelindex];

    // Currently there is no reliable way of knowing if the client has touched a certain item before another if they are next to each other, or rather
    // if the server has touched them in the same order. This results often in grabbing an item in the prediction and the server giving you the other
    // item. So for now prediction of armor, health, and ammo is disabled.
    /*
        if (item->giType == itemType_t::IT_ARMOR)
        { //rww - this will be stomped next update, but it's set so that we don't try to pick up two shields in one prediction and have the server cancel one
        //	cg.predictedPlayerState.stats[statIndex_t::STAT_ARMOR] += item->quantity;

            //FIXME: This can't be predicted properly at the moment
            return;
        }

        if (item->giType == itemType_t::IT_HEALTH)
        { //same as above, for health
        //	cg.predictedPlayerState.stats[statIndex_t::STAT_HEALTH] += item->quantity;

            //FIXME: This can't be predicted properly at the moment
            return;
        }

        if (item->giType == itemType_t::IT_AMMO)
        { //same as above, for ammo
        //	cg.predictedPlayerState.ammo[item->giTag] += item->quantity;

            //FIXME: This can't be predicted properly at the moment
            return;
        }

        if (item->giType == itemType_t::IT_HOLDABLE)
        { //same as above, for holdables
        //	cg.predictedPlayerState.stats[statIndex_t::STAT_HOLDABLE_ITEMS] |= (1 << item->giTag);
        }
    */
    // Special case for flags.
    // We don't predict touching our own flag
    // Make sure the item type is also a flag too
    if (cgs.gametype == gametype_t::GT_CTF || cgs.gametype == gametype_t::GT_CTY) {
        if (PS_Team(cg.predictedPlayerState) == team_t::TEAM_RED && item->giType == itemType_t::IT_TEAM && GI_Powerup(*item) == powerup_t::PW_REDFLAG)
            return;
        if (PS_Team(cg.predictedPlayerState) == team_t::TEAM_BLUE && item->giType == itemType_t::IT_TEAM && GI_Powerup(*item) == powerup_t::PW_BLUEFLAG)
            return;
    }

    if (item->giType == itemType_t::IT_POWERUP &&
        (GI_Powerup(*item) == powerup_t::PW_FORCE_ENLIGHTENED_LIGHT || GI_Powerup(*item) == powerup_t::PW_FORCE_ENLIGHTENED_DARK)) {
        if (GI_Powerup(*item) == powerup_t::PW_FORCE_ENLIGHTENED_LIGHT) {
            if (cg.predictedPlayerState.fd.forceSide != FORCE_LIGHTSIDE) {
                return;
            }
        }
        else {
            if (cg.predictedPlayerState.fd.forceSide != FORCE_DARKSIDE) {
                return;
            }
        }
    }

    // grab it
    BG_AddPredictableEventToPlayerstate(entity_event_t::EV_ITEM_PICKUP, cent->currentState.number, &cg.predictedPlayerState);

    // remove it from the frame so it won't be drawn
    cent->currentState.eFlags |= entityFlags_t::EF_NODRAW;

    // don't touch it again this prediction
    cent->miscTime = cg.time;

    // if its a weapon, give them some predicted ammo so the autoswitch will work
    if (item->giType == itemType_t::IT_WEAPON) {
        cg.predictedPlayerState.stats[statIndex_t::STAT_WEAPONS] |= Bit(GI_Weapon(*item));
        if (!cg.predictedPlayerState.ammo[weaponData[GI_Weapon(*item)].ammoIndex]) {
            cg.predictedPlayerState.ammo[weaponData[GI_Weapon(*item)].ammoIndex] = 1;
        }
    }
}

/*
=========================
CG_TouchTriggerPrediction

Predict push triggers and items
=========================
*/
static void CG_TouchTriggerPrediction(void)
{
    int i;
    trace_t trace;
    entityState_t* ent;
    clipHandle_t cmodel;
    centity_t* cent;
    bool spectator;

    // dead clients don't activate triggers
    if (cg.predictedPlayerState.stats[statIndex_t::STAT_HEALTH] <= 0) {
        return;
    }

    spectator = (cg.predictedPlayerState.pm_type == pmtype_t::PM_SPECTATOR);

    if (cg.predictedPlayerState.pm_type != pmtype_t::PM_NORMAL && cg.predictedPlayerState.pm_type != pmtype_t::PM_JETPACK &&
        cg.predictedPlayerState.pm_type != pmtype_t::PM_FLOAT && !spectator) {
        return;
    }

    for (i = 0; i < cg_numTriggerEntities; i++) {
        cent = cg_triggerEntities[i];
        ent = &cent->currentState;

        if (ent->eType == entityType_t::ET_ITEM && !spectator) {
            CG_TouchItem(cent);
            continue;
        }

        if (ent->solid != SOLID_BMODEL) {
            continue;
        }

        cmodel = trap->CM_InlineModel(ent->modelindex);
        if (!cmodel) {
            continue;
        }

        trap->CM_Trace(&trace, cg.predictedPlayerState.origin, cg.predictedPlayerState.origin, cg_pmove.mins, cg_pmove.maxs, cmodel,
                       contentsFlags_t::CONTENTS_ALL, 0);

        if (!trace.startsolid) {
            continue;
        }

        if (ent->eType == entityType_t::ET_TELEPORT_TRIGGER) {
            cg.hyperspace = true;
        }
        else if (ent->eType == entityType_t::ET_PUSH_TRIGGER) {
            BG_TouchJumpPad(&cg.predictedPlayerState, ent);
        }
    }

    // if we didn't touch a jump pad this pmove frame
    if (cg.predictedPlayerState.jumppad_frame != cg.predictedPlayerState.pmove_framecount) {
        cg.predictedPlayerState.jumppad_frame = 0;
        cg.predictedPlayerState.jumppad_ent = 0;
    }
}

#if 0
static QINLINE void CG_EntityStateToPlayerState( entityState_t *s, playerState_t *ps )
{
	//currently unused vars commented out for speed.. only uncomment if you need them.
	ps->clientNum = s->number;
	VectorCopy( s->pos.trBase, ps->origin );
	VectorCopy( s->pos.trDelta, ps->velocity );
	ps->saberLockFrame = s->forceFrame;
	ps->legsAnim = s->legsAnim;
	ps->torsoAnim = s->torsoAnim;
	ps->legsFlip = s->legsFlip;
	ps->torsoFlip = s->torsoFlip;
	ps->clientNum = s->clientNum;
	ps->saberMove = s->saberMove;

	/*
	VectorCopy( s->apos.trBase, ps->viewangles );

	ps->fd.forceMindtrickTargetIndex = s->trickedentindex;
	ps->fd.forceMindtrickTargetIndex2 = s->trickedentindex2;
	ps->fd.forceMindtrickTargetIndex3 = s->trickedentindex3;
	ps->fd.forceMindtrickTargetIndex4 = s->trickedentindex4;

	ps->electrifyTime = s->emplacedOwner;

	ps->speed = s->speed;

	ps->genericEnemyIndex = s->genericenemyindex;

	ps->activeForcePass = s->activeForcePass;

	ps->movementDir = s->angles2[YAW];

	ps->eFlags = s->eFlags;

	ps->saberInFlight = s->saberInFlight;
	ps->saberEntityNum = s->saberEntityNum;

	ps->fd.forcePowersActive = s->forcePowersActive;

	if (s->bolt1)
	{
		ps->duelInProgress = true;
	}
	else
	{
		ps->duelInProgress = false;
	}

	if (s->bolt2)
	{
		ps->dualBlade = true;
	}
	else
	{
		ps->dualBlade = false;
	}

	ps->emplacedIndex = s->otherEntityNum2;

	ps->saberHolstered = s->saberHolstered; //reuse bool in entitystate for players differently

	ps->genericEnemyIndex = -1; //no real option for this

	//The client has no knowledge of health levels (except for the client entity)
	if (Any( s->eFlags & entityFlags_t::EF_DEAD ))
	{
		ps->stats[statIndex_t::STAT_HEALTH] = 0;
	}
	else
	{
		ps->stats[statIndex_t::STAT_HEALTH] = 100;
	}

	if ( ps->externalEvent ) {
		s->event = ps->externalEvent;
		s->eventParm = ps->externalEventParm;
	} else if ( ps->entityEventSequence < ps->eventSequence ) {
		int		seq;

		if ( ps->entityEventSequence < ps->eventSequence - MAX_PS_EVENTS) {
			ps->entityEventSequence = ps->eventSequence - MAX_PS_EVENTS;
		}
		seq = ps->entityEventSequence & (MAX_PS_EVENTS-1);
		s->event = BG_PackEvent( ps->events[ seq ], ( ps->entityEventSequence & 3 ) << 8 );
		s->eventParm = ps->eventParms[ seq ];
		ps->entityEventSequence++;
	}

	ps->weapon = s->weapon;
	ps->groundEntityNum = s->groundEntityNum;

	for ( i = 0 ; i < MAX_POWERUPS ; i++ ) {
		if (s->powerups & (1 << i))
		{
			ps->powerups[i] = 30;
		}
		else
		{
			ps->powerups[i] = 0;
		}
	}

	ps->loopSound = s->loopSound;
	ps->generic1 = s->generic1;
	*/
}
#endif

playerState_t cgSendPSPool[MAX_GENTITIES];
playerState_t* cgSendPS[MAX_GENTITIES];

// #define _PROFILE_ES_TO_PS

#ifdef _PROFILE_ES_TO_PS
int g_cgEStoPSTime = 0;
#endif

// Assign all the entity playerstate pointers to the corresponding one
// so that we can access playerstate stuff in bg code (and then translate
// it back to entitystate data)
void CG_PmoveClientPointerUpdate()
{
    int i;

    memset(&cgSendPSPool[0], 0, sizeof(cgSendPSPool));

    for (i = 0; i < MAX_GENTITIES; i++) {
        cgSendPS[i] = &cgSendPSPool[i];

        // These will be invalid at this point on Xbox
        cg_entities[i].playerState = cgSendPS[i];
    }

    // Set up bg entity data
    cg_pmove.baseEnt = (bgEntity_t*)cg_entities;
    cg_pmove.entSize = sizeof(centity_t);

    cg_pmove.ghoul2 = nullptr;
}

// check if local client is on an eweb
bool CG_UsingEWeb(void)
{
    if (cg.predictedPlayerState.weapon == weapon_t::WP_EMPLACED_GUN && cg.predictedPlayerState.emplacedIndex &&
        cg_entities[cg.predictedPlayerState.emplacedIndex].currentState.weapon == weapon_t::WP_NONE) {
        return true;
    }

    return false;
}

/*
=================
CG_PredictPlayerState

Generates cg.predictedPlayerState for the current cg.time
cg.predictedPlayerState is guaranteed to be valid after exiting.

For demo playback, this will be an interpolation between two valid
playerState_t.

For normal gameplay, it will be the result of predicted usercmd_t on
top of the most recent playerState_t received from the server.

Each new snapshot will usually have one or more new usercmd over the last,
but we simulate all unacknowledged commands each time, not just the new ones.
This means that on an internet connection, quite a few pmoves may be issued
each frame.

OPTIMIZE: don't re-simulate unless the newly arrived snapshot playerState_t
differs from the predicted one.  Would require saving all intermediate
playerState_t during prediction.

We detect prediction errors and allow them to be decayed off over several frames
to ease the jerk.
=================
*/
extern void CG_Cube(vec3_t mins, vec3_t maxs, vec3_t color, float alpha);
extern vmCvar_t cg_showVehBounds;

// cg_showmiss: a legs or torso animation, or the crouch, shown A, then B, then A again in three successive frames, with the
// command time each frame shows: the mark of a display that takes a decision its tick does not
typedef struct {
    int legs, torso, ducked;
    int time;
} shown_t;

static void CG_CheckShownFlicker(const playerState_t* ps)
{
    static shown_t shown[2]; // the last two frames, newest first
    const shown_t now = {to_underlying(ps->legsAnim) | (ps->legsFlip ? 0x10000 : 0), to_underlying(ps->torsoAnim) | (ps->torsoFlip ? 0x10000 : 0),
                         Any(ps->pm_flags & pmoveFlags_t::PMF_DUCKED) ? 1 : 0, ps->commandTime};

    if (cg_showMiss.integer) {
        if (now.legs == shown[1].legs && now.legs != shown[0].legs) {
            Q_Println("shown flicker: legs anim {}, {}, {} at {}, {}, {}", shown[1].legs, shown[0].legs, now.legs, shown[1].time, shown[0].time, now.time);
        }
        if (now.torso == shown[1].torso && now.torso != shown[0].torso) {
            Q_Println("shown flicker: torso anim {}, {}, {} at {}, {}, {}", shown[1].torso, shown[0].torso, now.torso, shown[1].time, shown[0].time, now.time);
        }
        if (now.ducked == shown[1].ducked && now.ducked != shown[0].ducked) {
            Q_Println("shown flicker: crouch {}, {}, {} at {}, {}, {}", shown[1].ducked, shown[0].ducked, now.ducked, shown[1].time, shown[0].time, now.time);
        }
    }
    shown[1] = shown[0];
    shown[0] = now;
}

// The predicted states of the last PREDICTED_STATES command times, the server's in place of the client's once a snapshot
// holds them: the local player is drawn between the last two of them (CG_InterpolateTicks), and cg_showmiss
// compares a snapshot's animations with those the client predicted for the same command time.
constexpr int PREDICTED_STATES = 64;
constexpr int PREDICTED_TOUCH = 4;
static struct {
    playerState_t ps;
    bool valid;
    // for predictmiss: the command the tick was predicted with, the command time of the snapshot the prediction started
    // from, and the entities its movement touched (none for a snapshot's state)
    usercmd_t cmd;
    int from;
    int numTouch;
    int touch[PREDICTED_TOUCH];
} cg_predicted[PREDICTED_STATES];

static void CG_RecordPredicted(const playerState_t* ps)
{
    const unsigned slot = (unsigned)ps->commandTime / TICK_MSEC % PREDICTED_STATES;
    cg_predicted[slot].ps = *ps;
    cg_predicted[slot].valid = true;
    cg_predicted[slot].numTouch = 0;
    cg_predicted[slot].from = ps->commandTime;
    memset(&cg_predicted[slot].cmd, 0, sizeof(cg_predicted[slot].cmd));
}

// after a predicted movement step; from: the command time of the snapshot the frame's prediction started from
static void CG_RecordPredictedMove(const pmove_t* pm, int from)
{
    const unsigned slot = (unsigned)pm->ps->commandTime / TICK_MSEC % PREDICTED_STATES;
    int i;

    CG_RecordPredicted(pm->ps);
    cg_predicted[slot].cmd = pm->cmd;
    cg_predicted[slot].from = from;
    for (i = 0; i < pm->numtouch; i++) {
        if (pm->touchents[i] < ENTITYNUM_WORLD && cg_predicted[slot].numTouch < PREDICTED_TOUCH) {
            cg_predicted[slot].touch[cg_predicted[slot].numTouch++] = pm->touchents[i];
        }
    }
}

// the state predicted for commandTime, nullptr if it is no longer held
static const playerState_t* CG_PredictedAt(int commandTime)
{
    const unsigned slot = (unsigned)commandTime / TICK_MSEC % PREDICTED_STATES;
    return (cg_predicted[slot].valid && cg_predicted[slot].ps.commandTime == commandTime) ? &cg_predicted[slot].ps : nullptr;
}

static int CG_ShownAnim(animNumber_t anim, bool flip)
{
    return to_underlying(anim) | (flip ? 0x10000 : 0);
}

// cg_showmiss: a difference means the server sets the animation outside the shared movement code (the idle animation, the
// legs on a slope from the ghoul2 feet), or predicts it otherwise
static void CG_CheckPredictedAnims(const playerState_t* base)
{
    static int lastBase = -1;
    const playerState_t* predicted;

    if (!cg_showMiss.integer || base->commandTime == lastBase) {
        return;
    }
    lastBase = base->commandTime;
    predicted = CG_PredictedAt(base->commandTime);
    if (!predicted) {
        return; // not predicted (the first snapshot, a gap)
    }
    if (CG_ShownAnim(predicted->legsAnim, predicted->legsFlip) != CG_ShownAnim(base->legsAnim, base->legsFlip)) {
        Q_Println("anim miss: legs predicted {}, server {} at {}", CG_ShownAnim(predicted->legsAnim, predicted->legsFlip),
                  CG_ShownAnim(base->legsAnim, base->legsFlip), base->commandTime);
    }
    if (CG_ShownAnim(predicted->torsoAnim, predicted->torsoFlip) != CG_ShownAnim(base->torsoAnim, base->torsoFlip)) {
        Q_Println("anim miss: torso predicted {}, server {} at {}", CG_ShownAnim(predicted->torsoAnim, predicted->torsoFlip),
                  CG_ShownAnim(base->torsoAnim, base->torsoFlip), base->commandTime);
    }
}

// The player state is not sent whole: a snapshot holds 0 in the fields that are not in the field tables. Those the
// shared movement code keeps from one step to the next (the force jump's drain delay, the zoom's lock time, the use
// key's delay, the rocket lock's last time, its step count with the jump pad's) would start again from 0 at every
// snapshot, where the server's carry on: they are taken from the client's own prediction of the snapshot's tick, when
// it has one and no respawn or teleport lies between. A value the game sets outside the movement code stays unknown.
static void CG_CarryUnsentFields(playerState_t* base)
{
    const playerState_t* own = CG_PredictedAt(base->commandTime);

    if (!own || own->persistant[persEnum_t::PERS_SPAWN_COUNT] != base->persistant[persEnum_t::PERS_SPAWN_COUNT] ||
        Any((own->eFlags ^ base->eFlags) & entityFlags_t::EF_TELEPORT_BIT)) {
        return;
    }
    base->fd.forcePowerDebounce = own->fd.forcePowerDebounce;
    base->zoomLockTime = own->zoomLockTime;
    base->useTime = own->useTime;
    base->rocketLastValidTime = own->rocketLastValidTime;
    base->pmove_framecount = own->pmove_framecount;
    base->jumppad_frame = own->jumppad_frame;
}

// The newest snapshot received, when it is ahead of cgame's own next one. cgame steps through the snapshots as the drawn
// time reaches them (CG_ProcessSnapshots): its next snapshot is the first one after the drawn time, which is the newest
// received as long as the drawn time is less than an interval behind it. With loss or jitter the client draws further
// behind, and one or two newer snapshots wait. The local player's prediction does not need to wait for them: started
// from the newest, it has fewer ticks to predict and shows sooner what the server decided about the player. nullptr when
// there is none, or when a teleport, a respawn, another followed player or a restart of the map lies between: those
// stay with cgame's own transition, as before.
static snapshot_t* CG_SnapshotAhead(const snapshot_t* next)
{
    static snapshot_t ahead;
    static int aheadNum = -1;

    if (cg.latestSnapshotTime <= next->serverTime) {
        return nullptr;
    }
    if (aheadNum != cg.latestSnapshotNum) {
        aheadNum = -1;
        if (!trap->GetSnapshot(cg.latestSnapshotNum, &ahead)) {
            return nullptr;
        }
        aheadNum = cg.latestSnapshotNum;
    }
    if (ahead.serverTime <= next->serverTime || ahead.ps.clientNum != next->ps.clientNum ||
        Any((ahead.ps.eFlags ^ next->ps.eFlags) & entityFlags_t::EF_TELEPORT_BIT) ||
        ahead.ps.persistant[persEnum_t::PERS_SPAWN_COUNT] != next->ps.persistant[persEnum_t::PERS_SPAWN_COUNT] ||
        Any((ahead.snapFlags ^ next->snapFlags) & snapFlags_t::SNAPFLAG_SERVERCOUNT) || Any(ahead.ps.pm_flags & pmoveFlags_t::PMF_FOLLOW) ||
        ahead.ps.m_iVehicleNum != next->ps.m_iVehicleNum) {
        return nullptr;
    }
    return &ahead;
}

// Where the server has its movers at a snapshot: it moves them on the 20 Hz instants of its tick only (the game's logic
// clock, level.time, G_RunFrame), so between two instants a mover is where its trajectory was at the last
// one. Placed at the snapshot's own time, a lift going down would be up to 25 units under the player standing on
// it, and the prediction would let the player fall at every tick.
int CG_MoverTime(int serverTime)
{
    return serverTime / 50 * 50;
}

// predictmiss (console command): for every snapshot whose command time the client had predicted,
// compares the last prediction of that tick with the server's state, 32-bit word by word. A word that differs is a field
// the server sets outside the shared movement code, or computes otherwise, or does not send (the snapshot then holds 0):
// every cause of a prediction error is in that list, with the fields the client is not meant to predict. Counts nothing
// until its first call; each call writes predictmiss.txt (in the game folder of the home path) and starts again:
//  - per word (by offset in playerState_t; docs/modernization/tools/predict/ps_fields.sh puts the names), the ticks it differed in and its
//    last values; a word the server's state never held a value in is marked (a field that is not sent) and does not
//    make a tick count as differing, nor do the two words cgame writes into the snapshot's state itself;
//  - the ticks whose position differs, sorted: a teleport or a respawn; within 200 ms of a hit (a knockback, a damage
//    event); in contact with an entity (the server's or the predicted ground entity a player or a mover, one touched by
//    the predicted movement, or another player within 64 units); or none of these;
//  - in full, the first misses of the last two kinds and the first ticks where only an animation differs: every word
//    that differs with both values, the command, the snapshot the prediction started from and its state, the entities
//    touched, the nearest player.
#define PS_WORDS (sizeof(playerState_t) / 4)
constexpr int PREDICT_MISS_POSITIONS = 80;
constexpr int PREDICT_MISS_ANIMS = 30;
// the kind of a miss: the causes of a position that differs, then an animation only
enum class predictMissKind_t { MISS_TELEPORT, MISS_HIT, MISS_ENTITY, MISS_OTHER, MISS_ANIM, MISS_KINDS };
template <> inline constexpr bool enableEnumArithmetic<predictMissKind_t> = true;
constexpr int MISS_CAUSES = to_underlying(predictMissKind_t::MISS_ANIM); // the causes of a position, the kinds before MISS_ANIM
static const EnumArray<const char*, predictMissKind_t, predictMissKind_t::MISS_KINDS> cg_predictMissKinds = {
    "a teleport or a respawn", "within 200 ms of a hit", "in contact with an entity", "none of these", "animation only"};

typedef struct {
    predictMissKind_t kind;
    float d;
    playerState_t predicted, server, start;
    bool haveStart;
    usercmd_t cmd;
    int numTouch, touch[PREDICTED_TOUCH];
    int nearest; // the nearest other player or NPC, -1 if none
    float nearestDist, nearestSpeed;
} predictMissDetail_t;

static struct {
    bool on;
    int checked, differing;
    int count[PS_WORDS];
    int predicted[PS_WORDS], server[PS_WORDS]; // of the last tick the word differed in
    bool set[PS_WORDS];                        // the server's state held a value there
    int lastHit;                               // the command time of the last tick that showed a hit
    EnumArray<int, predictMissKind_t, MISS_CAUSES> origin;
    EnumArray<float, predictMissKind_t, MISS_CAUSES> originSum, originMax;
    int animOnly;
    int positions, anims, details; // kept in cg_predictMissDetails
    int ahead;                     // frames whose prediction started from a snapshot ahead of cgame's
} cg_predictMiss;
static predictMissDetail_t cg_predictMissDetails[PREDICT_MISS_POSITIONS + PREDICT_MISS_ANIMS];

static bool CG_PredictMissWordCounts(size_t i)
{
    return cg_predictMiss.set[i] && i != offsetof(playerState_t, entityEventSequence) / 4 && i != offsetof(playerState_t, slopeRecalcTime) / 4;
}

static void CG_KeepPredictMiss(predictMissKind_t kind, float d, const playerState_t* predicted, const playerState_t* base)
{
    const unsigned slot = (unsigned)base->commandTime / TICK_MSEC % PREDICTED_STATES;
    predictMissDetail_t* m = &cg_predictMissDetails[cg_predictMiss.details++];
    const playerState_t* start = CG_PredictedAt(cg_predicted[slot].from);
    int i;

    m->kind = kind;
    m->d = d;
    m->predicted = *predicted;
    m->server = *base;
    m->haveStart = start != nullptr && start != predicted;
    if (m->haveStart) {
        m->start = *start;
    }
    m->cmd = cg_predicted[slot].cmd;
    m->numTouch = cg_predicted[slot].numTouch;
    memcpy(m->touch, cg_predicted[slot].touch, sizeof(m->touch));
    m->nearest = -1;
    for (i = 0; i < cg.snap->numEntities; i++) {
        const centity_t* cent = &cg_entities[cg.snap->entities[i].number];

        if ((cent->currentState.eType == entityType_t::ET_PLAYER || cent->currentState.eType == entityType_t::ET_NPC) &&
            cent->currentState.number != predicted->clientNum) {
            const float dist = Distance(cent->lerpOrigin, predicted->origin);

            if (m->nearest < 0 || dist < m->nearestDist) {
                m->nearest = cent->currentState.number;
                m->nearestDist = dist;
                m->nearestSpeed = VectorLength(cent->currentState.pos.trDelta);
            }
        }
    }
}

static bool CG_PlayerWithin(const vec3_t origin, int self, float range)
{
    int i;

    for (i = 0; i < cg.snap->numEntities; i++) {
        const centity_t* cent = &cg_entities[cg.snap->entities[i].number];

        if ((cent->currentState.eType == entityType_t::ET_PLAYER || cent->currentState.eType == entityType_t::ET_NPC) && cent->currentState.number != self &&
            Distance(cent->lerpOrigin, origin) < range) {
            return true;
        }
    }
    return false;
}

static void CG_CountPredictMiss(const playerState_t* base)
{
    static int lastBase = -1;
    const playerState_t* predicted;
    int a[PS_WORDS], b[PS_WORDS];
    bool any = false;
    vec3_t riding;
    size_t i;

    if (!cg_predictMiss.on || base->commandTime == lastBase) {
        return;
    }
    lastBase = base->commandTime;
    predicted = CG_PredictedAt(base->commandTime);
    if (!predicted) {
        return; // not predicted (the first snapshot, a gap)
    }
    memcpy(a, predicted, sizeof(a));
    memcpy(b, base, sizeof(b));
    for (i = 0; i < PS_WORDS; i++) {
        if (b[i]) {
            cg_predictMiss.set[i] = true;
        }
        if (a[i] != b[i]) {
            cg_predictMiss.count[i]++;
            cg_predictMiss.predicted[i] = a[i];
            cg_predictMiss.server[i] = b[i];
            if (CG_PredictMissWordCounts(i)) {
                any = true;
            }
        }
    }
    cg_predictMiss.checked++;
    if (any) {
        cg_predictMiss.differing++;
    }
    if (Any(base->pm_flags & pmoveFlags_t::PMF_TIME_KNOCKBACK) || base->damageEvent != predicted->damageEvent) {
        cg_predictMiss.lastHit = base->commandTime;
    }
    // a player standing on a mover is moved with it by the server at the instants; the prediction leaves that to the
    // drawing (CG_AdjustPositionForMover), so it is added here before the comparison (within 0.01 unit: two float sums)
    VectorCopy(predicted->origin, riding);
    if (predicted->groundEntityNum == base->groundEntityNum) {
        CG_AdjustPositionForMover(predicted->origin, predicted->groundEntityNum,
                                  CG_MoverTime(cg_predicted[(unsigned)base->commandTime / TICK_MSEC % PREDICTED_STATES].from), CG_MoverTime(base->commandTime),
                                  riding);
    }
    if (Distance(riding, base->origin) > (VectorCompare(riding, predicted->origin) ? 0.0f : 0.01f)) {
        const unsigned slot = (unsigned)base->commandTime / TICK_MSEC % PREDICTED_STATES;
        const float d = Distance(riding, base->origin);
        predictMissKind_t cause;

        if (Any((predicted->eFlags ^ base->eFlags) & entityFlags_t::EF_TELEPORT_BIT) ||
            predicted->persistant[persEnum_t::PERS_SPAWN_COUNT] != base->persistant[persEnum_t::PERS_SPAWN_COUNT]) {
            cause = predictMissKind_t::MISS_TELEPORT;
        }
        else if (cg_predictMiss.lastHit && base->commandTime - cg_predictMiss.lastHit <= 200) {
            cause = predictMissKind_t::MISS_HIT;
        }
        else if (base->groundEntityNum < ENTITYNUM_WORLD || predicted->groundEntityNum < ENTITYNUM_WORLD || cg_predicted[slot].numTouch ||
                 CG_PlayerWithin(predicted->origin, predicted->clientNum, 64.0f)) {
            cause = predictMissKind_t::MISS_ENTITY;
        }
        else {
            cause = predictMissKind_t::MISS_OTHER;
        }
        cg_predictMiss.origin[cause]++;
        cg_predictMiss.originSum[cause] += d;
        if (d > cg_predictMiss.originMax[cause]) {
            cg_predictMiss.originMax[cause] = d;
        }
        if (cause >= predictMissKind_t::MISS_ENTITY && cg_predictMiss.positions < PREDICT_MISS_POSITIONS) {
            cg_predictMiss.positions++;
            CG_KeepPredictMiss(cause, d, predicted, base);
        }
    }
    else if (predicted->legsAnim != base->legsAnim || predicted->torsoAnim != base->torsoAnim || predicted->legsFlip != base->legsFlip ||
             predicted->torsoFlip != base->torsoFlip) {
        cg_predictMiss.animOnly++;
        if (cg_predictMiss.anims < PREDICT_MISS_ANIMS) {
            cg_predictMiss.anims++;
            CG_KeepPredictMiss(predictMissKind_t::MISS_ANIM, 0.0f, predicted, base);
        }
    }
}

static void CG_PredictMissLine(fileHandle_t f, const char* line)
{
    trap->FS_Write(line, strlen(line), f);
}

static void CG_WritePredictMiss(fileHandle_t f)
{
    char line[1024];
    size_t i;
    int n;

    Q_FormatTo(line, "predictmiss: {} ticks compared, {} with a difference in a field the server sets\n", cg_predictMiss.checked, cg_predictMiss.differing);
    CG_PredictMissLine(f, line);
    for (predictMissKind_t cause = predictMissKind_t::MISS_TELEPORT; cause < predictMissKind_t::MISS_ANIM; cause++) {
        Q_FormatTo(line, "predictmiss: position, {}: {} ticks, mean {:.2f} units, max {:.2f}\n", cg_predictMissKinds[cause], cg_predictMiss.origin[cause],
                   cg_predictMiss.origin[cause] ? cg_predictMiss.originSum[cause] / cg_predictMiss.origin[cause] : 0.0f, cg_predictMiss.originMax[cause]);
        CG_PredictMissLine(f, line);
    }
    Q_FormatTo(line, "predictmiss: an animation only: {} ticks\n", cg_predictMiss.animOnly);
    CG_PredictMissLine(f, line);
    Q_FormatTo(line, "predictmiss: {} frames predicted from a snapshot ahead of the drawn ones\n", cg_predictMiss.ahead);
    CG_PredictMissLine(f, line);
    // per word: offset, ticks, last predicted and server values
    for (i = 0; i < PS_WORDS; i++) {
        if (cg_predictMiss.count[i]) {
            float fp, fs;

            memcpy(&fp, &cg_predictMiss.predicted[i], 4);
            memcpy(&fs, &cg_predictMiss.server[i], 4);
            Q_FormatTo(line, "field {:4} {:6}  {} ({:g})  {} ({:g}){}\n", (int)(i * 4), cg_predictMiss.count[i], cg_predictMiss.predicted[i], fp,
                       cg_predictMiss.server[i], fs, cg_predictMiss.set[i] ? "" : "  never set by the server");
            CG_PredictMissLine(f, line);
        }
    }
    for (n = 0; n < cg_predictMiss.details; n++) {
        const predictMissDetail_t* m = &cg_predictMissDetails[n];
        int a[PS_WORDS], b[PS_WORDS];

        Q_FormatTo(line,
                   "miss {}: tick {}, {}, {:.2f} units ({:.2f} {:.2f} {:.2f}); command {} {} {} buttons {:x}; touched {}: {} {} {} {}; nearest player {} at "
                   "{:.1f} units, speed {:.0f}\n",
                   n, m->server.commandTime, cg_predictMissKinds[m->kind], m->d, m->predicted.origin[0] - m->server.origin[0],
                   m->predicted.origin[1] - m->server.origin[1], m->predicted.origin[2] - m->server.origin[2], m->cmd.forwardmove, m->cmd.rightmove,
                   m->cmd.upmove, to_underlying(m->cmd.buttons), m->numTouch, m->numTouch > 0 ? m->touch[0] : -1, m->numTouch > 1 ? m->touch[1] : -1,
                   m->numTouch > 2 ? m->touch[2] : -1, m->numTouch > 3 ? m->touch[3] : -1, m->nearest, m->nearest < 0 ? 0.0f : m->nearestDist,
                   m->nearest < 0 ? 0.0f : m->nearestSpeed);
        CG_PredictMissLine(f, line);
        if (m->haveStart) {
            Q_FormatTo(line,
                       "  start: {} ticks before; origin {:.2f} {:.2f} {:.2f} velocity {:.2f} {:.2f} {:.2f} ground {} pm_flags {:x} pm_time {} pm_type {} legs "
                       "{} torso {} weaponTime {} saberMove {}\n",
                       (m->server.commandTime - m->start.commandTime) / TICK_MSEC, m->start.origin[0], m->start.origin[1], m->start.origin[2],
                       m->start.velocity[0], m->start.velocity[1], m->start.velocity[2], m->start.groundEntityNum, to_underlying(m->start.pm_flags),
                       m->start.pm_time, to_underlying(m->start.pm_type), to_underlying(m->start.legsAnim), to_underlying(m->start.torsoAnim),
                       m->start.weaponTime, to_underlying(m->start.saberMove));
        }
        else {
            Q_FormatTo(line, "  start: not held\n");
        }
        CG_PredictMissLine(f, line);
        memcpy(a, &m->predicted, sizeof(a));
        memcpy(b, &m->server, sizeof(b));
        for (i = 0; i < PS_WORDS; i++) {
            if (a[i] != b[i] && CG_PredictMissWordCounts(i)) {
                float fp, fs;

                memcpy(&fp, &a[i], 4);
                memcpy(&fs, &b[i], 4);
                Q_FormatTo(line, "  diff {:4}  {} ({:g})  {} ({:g})\n", (int)(i * 4), a[i], fp, b[i], fs);
                CG_PredictMissLine(f, line);
            }
        }
    }
}

void CG_PredictMiss_f(void)
{
    if (!cg_predictMiss.on) {
        Q_Println("predictmiss: counting from now; call it again to write predictmiss.txt");
    }
    else {
        fileHandle_t f;

        Q_Println("predictmiss: {} ticks compared, {} with a difference in a field the server sets", cg_predictMiss.checked, cg_predictMiss.differing);
        for (predictMissKind_t cause = predictMissKind_t::MISS_TELEPORT; cause < predictMissKind_t::MISS_ANIM; cause++) {
            Q_Println("predictmiss: position, {}: {} ticks, mean {:.2f} units, max {:.2f}", cg_predictMissKinds[cause], cg_predictMiss.origin[cause],
                      cg_predictMiss.origin[cause] ? cg_predictMiss.originSum[cause] / cg_predictMiss.origin[cause] : 0.0f, cg_predictMiss.originMax[cause]);
        }
        Q_Println("predictmiss: an animation only: {} ticks", cg_predictMiss.animOnly);
        Q_Println("predictmiss: {} frames predicted from a snapshot ahead of the drawn ones", cg_predictMiss.ahead);
        trap->FS_Open("predictmiss.txt", &f, fsMode_t::FS_WRITE);
        if (f) {
            CG_WritePredictMiss(f);
            trap->FS_Close(f);
            Q_Println("predictmiss: {} misses in full, written to predictmiss.txt in the game folder of the home path", cg_predictMiss.details);
        }
        else {
            Q_Println("predictmiss: could not write predictmiss.txt");
        }
    }
    memset(&cg_predictMiss, 0, sizeof(cg_predictMiss));
    cg_predictMiss.on = true;
}

// The commands are made one tick ahead (CL_CreateNewCommands), so the
// tick in progress is already predicted. The local player is drawn between the last two ticks at the command clock's
// time (the engine's clock the commands are made on, ahead of the server's; cg.cmdTime), its
// position and velocity interpolated and its discrete state (animations, flags, events) the newest tick's, with the view
// angles of now (the engine's command of the frame). Nothing is guessed: every state is a full tick of 10 ms, as the server
// computes it (a partial step of the movement between two ticks gives other collisions, landings and random decisions
// than the tick that follows: the player shakes against walls, animations flicker). Not interpolated across a
// teleport or a respawn; a pilot is drawn at the newest tick.
static void CG_InterpolateTicks(playerState_t* out, int current)
{
    const playerState_t* prev = CG_PredictedAt(out->commandTime - TICK_MSEC);
    usercmd_t now;
    int i;

    // the engine's command of the frame: the view angles of now, and the command clock as its time
    const bool haveNow = trap->GetUserCmd(current + 1, &now);

    if (haveNow) {
        cg.cmdTime = now.serverTime;
    }
    if (prev && !CG_Piloting(out->m_iVehicleNum) && cg.cmdTime >= prev->commandTime && cg.cmdTime < out->commandTime &&
        !Any((prev->eFlags ^ out->eFlags) & entityFlags_t::EF_TELEPORT_BIT) &&
        prev->persistant[persEnum_t::PERS_SPAWN_COUNT] == out->persistant[persEnum_t::PERS_SPAWN_COUNT]) {
        const float f = (float)(cg.cmdTime - prev->commandTime) / TICK_MSEC;

        for (i = 0; i < 3; i++) {
            out->origin[i] = prev->origin[i] + f * (out->origin[i] - prev->origin[i]);
            out->velocity[i] = prev->velocity[i] + f * (out->velocity[i] - prev->velocity[i]);
        }
    }
    if (haveNow) {
        // the view angles of the frame, except on an axis the movement set in the newest tick (a wall run, a lunge, a jump
        // or back attack, a saber lock, the pitch limit: its delta_angles changed), which stays as the tick left it; else
        // the view would follow the mouse until the next tick and step back there
        // shortcut: a view held while the mouse is still is not seen as held, so the first tick of a mouse movement shows;
        // a flag set by PM_SetPMViewAngle and kept with each predicted state would close it
        vec3_t held;
        int delta[3];

        VectorCopy(out->viewangles, held);
        for (i = 0; i < 3; i++) {
            delta[i] = out->delta_angles[i];
        }
        PM_UpdateViewAngles(out, &now);
        for (i = 0; prev && i < 3; i++) {
            // on 16 bits: a snapshot's value is the server's truncated to them
            if ((short)delta[i] != (short)prev->delta_angles[i]) {
                out->viewangles[i] = held[i];
            }
        }
    }
}

// the pmove fields set before each command (cg_pmove.ps is the predicted player state)
static void CG_PreparePmove(centity_t* pEnt, clientInfo_t* ci)
{
    cg_pmove.animations = bgAllAnims[pEnt->localAnimIndex].anims;
    cg_pmove.gametype = cgs.gametype;

    cg_pmove.debugMelee = cgs.debugMelee;
    cg_pmove.stepSlideFix = cgs.stepSlideFix;
    cg_pmove.noSpecMove = cgs.noSpecMove;

    cg_pmove.nonHumanoid = (pEnt->localAnimIndex > 0);

    if (cg.snap && cg.snap->ps.saberLockTime > cg_pmove.cmd.serverTime) // as the movement code tests it
    {
        centity_t* blockOpp = &cg_entities[cg.snap->ps.saberLockEnemy];

        if (blockOpp) {
            vec3_t lockDir, lockAng;

            VectorSubtract(blockOpp->lerpOrigin, cg.snap->ps.origin, lockDir);
            vectoangles(lockDir, lockAng);

            VectorCopy(lockAng, cg_pmove.ps->viewangles);
        }
    }

    // THIS is pretty much bad, but...
    cg_pmove.ps->fd.saberAnimLevelBase = cg_pmove.ps->fd.saberAnimLevel;
    if (cg_pmove.ps->saberHolstered == 1) {
        if (ci->saber[0].numBlades > 0) {
            cg_pmove.ps->fd.saberAnimLevelBase = saber_styles_t::SS_STAFF;
        }
        else if (ci->saber[1].model[0]) {
            cg_pmove.ps->fd.saberAnimLevelBase = saber_styles_t::SS_DUAL;
        }
    }
}
pmove_t cg_vehPmove;
bool cg_vehPmoveSet = false;

void CG_PredictPlayerState(void)
{
    int cmdNum, current, i;
    int baseTime; // the command time of the snapshot's state the prediction starts from
    playerState_t oldPlayerState;
    playerState_t oldVehicleState = {0};
    bool moved;
    usercmd_t oldestCmd;
    usercmd_t latestCmd;
    centity_t* pEnt;
    clientInfo_t* ci;

    cg.hyperspace = false; // will be set if touching a trigger_teleport
    cg.cmdTime = cg.time;  // unless the player is predicted (CG_InterpolateTicks)

    // if this is the first frame we must guarantee
    // predictedPlayerState is valid even if there is some
    // other error condition
    if (!cg.validPPS) {
        cg.validPPS = true;
        cg.predictedPlayerState = cg.snap->ps;
        cg.predictedFullState = cg.snap->ps;
        if (CG_Piloting(cg.snap->ps.m_iVehicleNum)) {
            cg.predictedVehicleState = cg.snap->vps;
        }
    }

    // demo playback just copies the moves
    if (cg.demoPlayback || Any(cg.snap->ps.pm_flags & pmoveFlags_t::PMF_FOLLOW)) {
        CG_InterpolatePlayerState(false);
        if (CG_Piloting(cg.predictedPlayerState.m_iVehicleNum)) {
            CG_InterpolateVehiclePlayerState(false);
        }
        return;
    }

    // non-predicting local movement will grab the latest angles
    if (cg_noPredict.integer || CG_UsingEWeb()) {
        CG_InterpolatePlayerState(true);
        if (CG_Piloting(cg.predictedPlayerState.m_iVehicleNum)) {
            CG_InterpolateVehiclePlayerState(true);
        }
        return;
    }

    // prepare for pmove
    cg_pmove.ps = &cg.predictedPlayerState;
    cg_pmove.trace = CG_Trace;
    cg_pmove.pointcontents = CG_PointContents;

    pEnt = &cg_entities[cg.predictedPlayerState.clientNum];
    // rww - bgghoul2
    if (cg_pmove.ghoul2 != pEnt->ghoul2) // only update it if the g2 instance has changed
    {
        if (cg.snap && pEnt->ghoul2 && !Any(cg.snap->ps.pm_flags & pmoveFlags_t::PMF_FOLLOW) && PS_Team(cg.snap->ps) != team_t::TEAM_SPECTATOR) {
            cg_pmove.ghoul2 = pEnt->ghoul2;
            cg_pmove.g2Bolts_LFoot = trap->G2API_AddBolt(pEnt->ghoul2, 0, "*l_leg_foot");
            cg_pmove.g2Bolts_RFoot = trap->G2API_AddBolt(pEnt->ghoul2, 0, "*r_leg_foot");
        }
        else {
            cg_pmove.ghoul2 = nullptr;
        }
    }

    ci = &cgs.clientinfo[cg.predictedPlayerState.clientNum];

    // I'll just do this every frame in case the scale changes in realtime (don't need to update the g2 inst for that)
    VectorCopy(pEnt->modelScale, cg_pmove.modelScale);
    // rww end bgghoul2

    if (cg_pmove.ps->pm_type == pmtype_t::PM_DEAD) {
        cg_pmove.tracemask = MASK_PLAYERSOLID & ~contentsFlags_t::CONTENTS_BODY;
    }
    else {
        cg_pmove.tracemask = MASK_PLAYERSOLID;
    }
    if (PS_Team(cg.snap->ps) == team_t::TEAM_SPECTATOR || cg.snap->ps.pm_type == pmtype_t::PM_SPECTATOR) {
        cg_pmove.tracemask &= ~contentsFlags_t::CONTENTS_BODY; // spectators can fly through bodies
    }
    cg_pmove.noFootsteps = Any(cgs.dmflags & dmFlags_t::DF_NO_FOOTSTEPS);

    // save the state before the pmove so we can detect transitions
    oldPlayerState = cg.predictedPlayerState;
    if (CG_Piloting(cg.predictedPlayerState.m_iVehicleNum)) {
        oldVehicleState = cg.predictedVehicleState;
    }

    current = trap->GetCurrentCmdNumber();

    // if we don't have the commands right after the snapshot, we
    // can't accurately predict a current position, so just freeze at
    // the last good position we had
    cmdNum = current - CMD_BACKUP + 1;
    trap->GetUserCmd(cmdNum, &oldestCmd);
    if (oldestCmd.serverTime > cg.snap->ps.commandTime && oldestCmd.serverTime < cg.time) { // special check for map_restart
        if (cg_showMiss.integer) {
            Q_Println("exceeded PACKET_BACKUP on commands");
        }
        return;
    }

    // get the latest command so we can know which commands are from previous map_restarts
    trap->GetUserCmd(current, &latestCmd);

    // get the most recent information we have, even if
    // the server time is beyond our current cg.time,
    // because predicted player positions are going to
    // be ahead of everything else anyway
    if (cg.nextSnap && !cg.nextFrameTeleport && !cg.thisFrameTeleport) {
        snapshot_t* ahead = CG_SnapshotAhead(cg.nextSnap);
        snapshot_t* base = ahead ? ahead : cg.nextSnap;

        if (ahead) {
            cg_predictMiss.ahead++;
        }
        base->ps.slopeRecalcTime = cg.predictedPlayerState.slopeRecalcTime; // this is the only value we want to maintain seperately on server/client
        cg.predictedPlayerState = base->ps;
        if (CG_Piloting(base->ps.m_iVehicleNum)) {
            cg.predictedVehicleState = base->vps;
        }
        cg.physicsTime = CG_MoverTime(base->serverTime);
    }
    else {
        cg.snap->ps.slopeRecalcTime = cg.predictedPlayerState.slopeRecalcTime; // this is the only value we want to maintain seperately on server/client
        cg.predictedPlayerState = cg.snap->ps;
        if (CG_Piloting(cg.snap->ps.m_iVehicleNum)) {
            cg.predictedVehicleState = cg.snap->vps;
        }
        cg.physicsTime = CG_MoverTime(cg.snap->serverTime);
    }
    CG_CheckPredictedAnims(&cg.predictedPlayerState);
    CG_CountPredictMiss(&cg.predictedPlayerState);
    CG_CarryUnsentFields(&cg.predictedPlayerState);
    CG_RecordPredicted(&cg.predictedPlayerState);
    baseTime = cg.predictedPlayerState.commandTime;

    for (i = 0; i < MAX_GENTITIES; i++) {
        // Written this way for optimal speed, even though it doesn't look pretty.
        //(we don't want to spend the time assigning pointers as it does take
        // a small precious fraction of time and adds up in the loop.. so says
        // the precision timer!)

        if (cg_entities[i].currentState.eType == entityType_t::ET_PLAYER || cg_entities[i].currentState.eType == entityType_t::ET_NPC) {
            // Need a new playerState_t on Xbox
            VectorCopy(cg_entities[i].currentState.pos.trBase, cgSendPS[i]->origin);
            VectorCopy(cg_entities[i].currentState.pos.trDelta, cgSendPS[i]->velocity);
            cgSendPS[i]->saberLockFrame = cg_entities[i].currentState.forceFrame;
            cgSendPS[i]->legsAnim = ES_LegsAnim(cg_entities[i].currentState);
            cgSendPS[i]->torsoAnim = ES_TorsoAnim(cg_entities[i].currentState);
            cgSendPS[i]->legsFlip = cg_entities[i].currentState.legsFlip;
            cgSendPS[i]->torsoFlip = cg_entities[i].currentState.torsoFlip;
            cgSendPS[i]->clientNum = cg_entities[i].currentState.clientNum;
            cgSendPS[i]->saberMove = cg_entities[i].currentState.saberMove;
        }
    }

    if (CG_Piloting(cg.predictedPlayerState.m_iVehicleNum)) {
        cg_entities[cg.predictedPlayerState.clientNum].playerState = &cg.predictedPlayerState;
        cg_entities[cg.predictedPlayerState.m_iVehicleNum].playerState = &cg.predictedVehicleState;

        // use the player command time, because we are running with the player cmds (this is even the case
        // on the server)
        cg.predictedVehicleState.commandTime = cg.predictedPlayerState.commandTime;
    }

    // run cmds; one more turn after the newest command only checks the prediction error. The check compares the state
    // this frame computes for the last frame's newest tick with the last frame's; run before each command instead, it
    // would miss a frame that brings a snapshot without a new command (the commands are made per tick): its correction
    // would be neither printed by cg_showmiss nor smoothed (cg_errorDecay)
    moved = false;
    for (cmdNum = current - CMD_BACKUP + 1; cmdNum <= current + 1; cmdNum++) {
        if (cmdNum <= current) {
            // get the command
            trap->GetUserCmd(cmdNum, &cg_pmove.cmd);

            // don't do anything if the time is before the snapshot player time
            if (cg_pmove.cmd.serverTime <= cg.predictedPlayerState.commandTime) {
                continue;
            }

            // don't do anything if the command was from a previous map_restart
            if (cg_pmove.cmd.serverTime > latestCmd.serverTime) {
                continue;
            }
        }

        // check for a prediction error from last frame
        // on a lan, this will often be the exact value
        // from the snapshot, but on a wan we will have
        // to predict several commands to get to the point
        // we want to compare
        if (CG_Piloting(oldPlayerState.m_iVehicleNum) && cg.predictedVehicleState.commandTime == oldVehicleState.commandTime) {
            vec3_t delta;
            float len;

            if (cg.thisFrameTeleport) {
                // a teleport will not cause an error decay
                VectorClear(cg.predictedError);
                if (cg_showVehMiss.integer) {
                    Q_Println("VEH PredictionTeleport");
                }
                cg.thisFrameTeleport = false;
            }
            else {
                vec3_t adjusted;
                CG_AdjustPositionForMover(cg.predictedVehicleState.origin, cg.predictedVehicleState.groundEntityNum, cg.physicsTime, cg.oldTime, adjusted);

                if (cg_showVehMiss.integer) {
                    if (!VectorCompare(oldVehicleState.origin, adjusted)) {
                        Q_Println("VEH prediction error");
                    }
                }
                VectorSubtract(oldVehicleState.origin, adjusted, delta);
                len = VectorLength(delta);
                if (len > 0.1) {
                    if (cg_showVehMiss.integer) {
                        Q_Println("VEH Prediction miss: {:f}", len);
                    }
                    if (cg_errorDecay.integer) {
                        int t;
                        float f;

                        t = cg.time - cg.predictedErrorTime;
                        f = (cg_errorDecay.value - t) / cg_errorDecay.value;
                        if (f < 0) {
                            f = 0;
                        }
                        if (f > 0 && cg_showVehMiss.integer) {
                            Q_Println("VEH Double prediction decay: {:f}", f);
                        }
                        VectorScale(cg.predictedError, f, cg.predictedError);
                    }
                    else {
                        VectorClear(cg.predictedError);
                    }
                    VectorAdd(delta, cg.predictedError, cg.predictedError);
                    cg.predictedErrorTime = cg.oldTime;
                }
                //
                if (cg_showVehMiss.integer) {
                    if (!VectorCompare(oldVehicleState.vehOrientation, cg.predictedVehicleState.vehOrientation)) {
                        Q_Println("VEH orient prediction error");
                        Q_Println("VEH pitch prediction miss: {:f}",
                                  AngleSubtract(oldVehicleState.vehOrientation[0], cg.predictedVehicleState.vehOrientation[0]));
                        Q_Println("VEH yaw prediction miss: {:f}",
                                  AngleSubtract(oldVehicleState.vehOrientation[1], cg.predictedVehicleState.vehOrientation[1]));
                        Q_Println("VEH roll prediction miss: {:f}",
                                  AngleSubtract(oldVehicleState.vehOrientation[2], cg.predictedVehicleState.vehOrientation[2]));
                    }
                }
            }
        }
        else if (!oldPlayerState.m_iVehicleNum &&                                          // don't do pred err on ps while riding veh
                 cg.predictedPlayerState.commandTime == cg.predictedFullState.commandTime) // the last frame's full tick
        {
            vec3_t delta;
            float len;

            if (cg.thisFrameTeleport) {
                // a teleport will not cause an error decay
                VectorClear(cg.predictedError);
                if (cg_showMiss.integer) {
                    Q_Println("PredictionTeleport");
                }
                cg.thisFrameTeleport = false;
            }
            else {
                vec3_t adjusted;
                CG_AdjustPositionForMover(cg.predictedPlayerState.origin, cg.predictedPlayerState.groundEntityNum, cg.physicsTime, cg.oldTime, adjusted);

                if (cg_showMiss.integer) {
                    if (!VectorCompare(cg.predictedFullState.origin, adjusted)) {
                        Q_Println("prediction error");
                    }
                }
                VectorSubtract(cg.predictedFullState.origin, adjusted, delta);
                len = VectorLength(delta);
                if (len > 0.1) {
                    if (cg_showMiss.integer) {
                        Q_Println("Prediction miss: {:f}", len);
                    }
                    if (cg_errorDecay.integer) {
                        int t;
                        float f;

                        t = cg.time - cg.predictedErrorTime;
                        f = (cg_errorDecay.value - t) / cg_errorDecay.value;
                        if (f < 0) {
                            f = 0;
                        }
                        if (f > 0 && cg_showMiss.integer) {
                            Q_Println("Double prediction decay: {:f}", f);
                        }
                        VectorScale(cg.predictedError, f, cg.predictedError);
                    }
                    else {
                        VectorClear(cg.predictedError);
                    }
                    VectorAdd(delta, cg.predictedError, cg.predictedError);
                    cg.predictedErrorTime = cg.oldTime;
                }
            }
        }

        if (cmdNum > current) {
            break; // the turn of the check only
        }

        CG_PreparePmove(pEnt, ci);
        Pmove(&cg_pmove);
        CG_RecordPredictedMove(&cg_pmove, baseTime);

        if (CG_Piloting(cg.predictedPlayerState.m_iVehicleNum) &&
            cg.predictedPlayerState.pm_type != pmtype_t::PM_INTERMISSION) { // we're riding a vehicle, let's predict it
            centity_t* veh = &cg_entities[cg.predictedPlayerState.m_iVehicleNum];
            int x, zd, zu;

            if (veh->m_pVehicle) { // make sure pointer is set up to go to our predicted state
                veh->m_pVehicle->m_vOrientation = &cg.predictedVehicleState.vehOrientation[0];

                // keep this updated based on what the playerstate says
                veh->m_pVehicle->m_iRemovedSurfaces = cg.predictedVehicleState.vehSurfaces;

                trap->GetUserCmd(cmdNum, &veh->m_pVehicle->m_ucmd);

                if (Any(veh->m_pVehicle->m_ucmd.buttons & buttonFlags_t::BUTTON_TALK)) { // forced input if "chat bubble" is up
                    veh->m_pVehicle->m_ucmd.buttons = buttonFlags_t::BUTTON_TALK;
                    veh->m_pVehicle->m_ucmd.forwardmove = 0;
                    veh->m_pVehicle->m_ucmd.rightmove = 0;
                    veh->m_pVehicle->m_ucmd.upmove = 0;
                }
                cg_vehPmove.ps = &cg.predictedVehicleState;
                cg_vehPmove.animations = bgAllAnims[veh->localAnimIndex].anims;

                memcpy(&cg_vehPmove.cmd, &veh->m_pVehicle->m_ucmd, sizeof(usercmd_t));
                /*
                cg_vehPmove.cmd.rightmove = 0; //no vehicle can move right/left
                cg_vehPmove.cmd.upmove = 0; //no vehicle can move up/down
                */

                cg_vehPmove.gametype = cgs.gametype;
                cg_vehPmove.ghoul2 = veh->ghoul2;

                cg_vehPmove.nonHumanoid = (veh->localAnimIndex > 0);

                /*
                x = (veh->currentState.solid & 255);
                zd = (veh->currentState.solid & 255);
                zu = (veh->currentState.solid & 255) - 32;

                cg_vehPmove.mins[0] = cg_vehPmove.mins[1] = -x;
                cg_vehPmove.maxs[0] = cg_vehPmove.maxs[1] = x;
                cg_vehPmove.mins[2] = -zd;
                cg_vehPmove.maxs[2] = zu;
                */
                // I think this was actually wrong.. just copy-pasted from id code. Oh well.
                x = (veh->currentState.solid) & 255;
                zd = (veh->currentState.solid >> 8) & 255;
                zu = (veh->currentState.solid >> 15) & 255;

                zu -= 32; // I don't quite get the reason for this.
                zd = -zd;

                // z/y must be symmetrical (blah)
                cg_vehPmove.mins[0] = cg_vehPmove.mins[1] = -x;
                cg_vehPmove.maxs[0] = cg_vehPmove.maxs[1] = x;
                cg_vehPmove.mins[2] = zd;
                cg_vehPmove.maxs[2] = zu;

                VectorCopy(veh->modelScale, cg_vehPmove.modelScale);

                if (!cg_vehPmoveSet) { // do all the one-time things
                    cg_vehPmove.trace = CG_Trace;
                    cg_vehPmove.pointcontents = CG_PointContents;
                    cg_vehPmove.tracemask = MASK_PLAYERSOLID;
                    cg_vehPmove.debugLevel = 0;
                    cg_vehPmove.g2Bolts_LFoot = -1;
                    cg_vehPmove.g2Bolts_RFoot = -1;

                    cg_vehPmove.baseEnt = (bgEntity_t*)cg_entities;
                    cg_vehPmove.entSize = sizeof(centity_t);

                    cg_vehPmoveSet = true;
                }

                cg_vehPmove.noFootsteps = Any(cgs.dmflags & dmFlags_t::DF_NO_FOOTSTEPS);

                cg_entities[cg.predictedPlayerState.clientNum].playerState = &cg.predictedPlayerState;
                veh->playerState = &cg.predictedVehicleState;

                // update boarding value sent from server. boarding is not predicted, but no big deal
                veh->m_pVehicle->m_iBoarding = cg.predictedVehicleState.vehBoarding;

                Pmove(&cg_vehPmove);
                /*
                if ( !cg_paused.integer )
                {
                    Q_Println("{} - PITCH change {:4.2f}", cg.time, AngleSubtract(veh->m_pVehicle->m_vOrientation[0],veh->m_pVehicle->m_vPrevOrientation[0]));
                }
                */
                if (cg_showVehBounds.integer) {
                    vec3_t NPCDEBUG_RED = {1.0, 0.0, 0.0};
                    vec3_t absmin, absmax;
                    VectorAdd(cg_vehPmove.ps->origin, cg_vehPmove.mins, absmin);
                    VectorAdd(cg_vehPmove.ps->origin, cg_vehPmove.maxs, absmax);
                    CG_Cube(absmin, absmax, NPCDEBUG_RED, 0.25);
                }
            }
        }

        moved = true;

        // add push trigger movement effects
        CG_TouchTriggerPrediction();

        // check for predictable events that changed from previous predictions
        // CG_CheckChangedPredictableEvents(&cg.predictedPlayerState);
    }

    // the newest tick is the full state (the next frame measures the prediction error against it); the local player is
    // drawn between the last two ticks
    cg.predictedFullState = cg.predictedPlayerState;
    CG_InterpolateTicks(&cg.predictedPlayerState, current);

    if (cg_showMiss.integer > 1) {
        Q_Print("[{} : {}] ", cg_pmove.cmd.serverTime, cg.time);
    }

    if (!moved) {
        // with the commands made one tick ahead, the server can run the newest before the next is made (a short round
        // trip): drawn from the snapshot, interpolated as above
        if (cg_showMiss.integer > 1) {
            Q_Println("not moved");
        }
        // the events and the changes of that snapshot are still played here: while the prediction is on, the snapshot's
        // transition leaves them to this function (CG_TransitionSnapshot), and the next frame would compare with a
        // state that already holds them (a jump or a saber ignition without its sound)
        CG_TransitionPlayerState(&cg.predictedPlayerState, &oldPlayerState);
        goto revertES;
    }

    if (CG_Piloting(cg.predictedPlayerState.m_iVehicleNum)) {
        CG_AdjustPositionForMover(cg.predictedVehicleState.origin, cg.predictedVehicleState.groundEntityNum, cg.physicsTime, cg.time,
                                  cg.predictedVehicleState.origin);
    }
    else {
        // adjust for the movement of the groundentity
        CG_AdjustPositionForMover(cg.predictedPlayerState.origin, cg.predictedPlayerState.groundEntityNum, cg.physicsTime, cg.time,
                                  cg.predictedPlayerState.origin);
        CG_AdjustPositionForMover(cg.predictedFullState.origin, cg.predictedFullState.groundEntityNum, cg.physicsTime, cg.time, cg.predictedFullState.origin);
    }

    if (cg_showMiss.integer) {
        if (cg.predictedPlayerState.eventSequence > oldPlayerState.eventSequence + MAX_PS_EVENTS) {
            Q_PrintlnWarning("dropped event");
        }
    }

    // fire events and other transition triggered things
    CG_TransitionPlayerState(&cg.predictedPlayerState, &oldPlayerState);
    CG_CheckShownFlicker(&cg.predictedPlayerState);

    if (cg_showMiss.integer) {
        if (cg.eventSequence > cg.predictedPlayerState.eventSequence) {
            Q_PrintlnWarning("double event");
            cg.eventSequence = cg.predictedPlayerState.eventSequence;
        }
    }

    if (cg.predictedPlayerState.m_iVehicleNum && !CG_Piloting(cg.predictedPlayerState.m_iVehicleNum)) { // a passenger on this vehicle, bolt them in
        centity_t* veh = &cg_entities[cg.predictedPlayerState.m_iVehicleNum];
        VectorCopy(veh->lerpAngles, cg.predictedPlayerState.viewangles);
        VectorCopy(veh->lerpOrigin, cg.predictedPlayerState.origin);
    }

revertES:
    if (CG_Piloting(cg.predictedPlayerState.m_iVehicleNum)) {
        centity_t* veh = &cg_entities[cg.predictedPlayerState.m_iVehicleNum];

        if (veh->m_pVehicle) {
            // switch ptr back for this ent in case we stop riding it
            veh->m_pVehicle->m_vOrientation = &cgSendPS[veh->currentState.number]->vehOrientation[0];
        }

        cg_entities[cg.predictedPlayerState.clientNum].playerState = cgSendPS[cg.predictedPlayerState.clientNum];
        veh->playerState = cgSendPS[veh->currentState.number];
    }

    // copy some stuff back into the entstates to help actually "predict" them if applicable
    for (i = 0; i < MAX_GENTITIES; i++) {
        if (cg_entities[i].currentState.eType == entityType_t::ET_PLAYER || cg_entities[i].currentState.eType == entityType_t::ET_NPC) {
            ES_SetTorsoAnim(cg_entities[i].currentState, cgSendPS[i]->torsoAnim);
            ES_SetLegsAnim(cg_entities[i].currentState, cgSendPS[i]->legsAnim);
            cg_entities[i].currentState.forceFrame = cgSendPS[i]->saberLockFrame;
            cg_entities[i].currentState.saberMove = cgSendPS[i]->saberMove;
        }
    }
}

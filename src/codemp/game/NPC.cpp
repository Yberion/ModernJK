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
// NPC.cpp - generic functions
//
#include "b_local.h"
#include "anims.h"
#include "say.h"
#include <icarus/Q3_Interface.h>

extern vec3_t playerMins;
extern vec3_t playerMaxs;
extern void G_SoundOnEnt(gentity_t* ent, soundChannel_t channel, const char* soundPath);
extern void PM_SetTorsoAnimTimer(gentity_t* ent, int* torsoAnimTimer, int time);
extern void PM_SetLegsAnimTimer(gentity_t* ent, int* legsAnimTimer, int time);
extern void NPC_BSNoClip(void);
extern void G_AddVoiceEvent(gentity_t* self, entity_event_t event, int speakDebounceTime);
extern void NPC_ApplyRoff(void);
extern void NPC_TempLookTarget(gentity_t* self, int lookEntNum, int minLookTime, int maxLookTime);
extern void NPC_CheckPlayerAim(void);
extern void NPC_CheckAllClear(void);
extern void G_AddVoiceEvent(gentity_t* self, entity_event_t event, int speakDebounceTime);
extern bool NPC_CheckLookTarget(gentity_t* self);
extern void NPC_SetLookTarget(gentity_t* self, int entNum, int clearTime);
extern void Mark1_dying(gentity_t* self);
extern void NPC_BSCinematic(void);
extern int GetTime(int lastTime);
extern void NPC_BSGM_Default(void);
extern void NPC_CheckCharmed(void);
extern bool Boba_Flying(gentity_t* self);

// Local Variables
npcStatic_t NPCS;

void NPC_SetAnim(gentity_t* ent, setAnimParts_t type, animNumber_t anim, setAnimFlags_t priority);
void pitch_roll_for_slope(gentity_t* forwhom, vec3_t pass_slope);
extern void GM_Dying(gentity_t* self);

extern int eventClearTime;

void CorpsePhysics(gentity_t* self)
{
    // run the bot through the server like it was a real client
    memset(&NPCS.ucmd, 0, sizeof(NPCS.ucmd));
    ClientThink(self->s.number, &NPCS.ucmd);
    // VectorCopy( self->s.origin, self->s.origin2 );
    // rww - don't get why this is happening.

    if (self->client->NPC_class == class_t::CLASS_GALAKMECH) {
        GM_Dying(self);
    }
    // FIXME: match my pitch and roll for the slope of my groundPlane
    if (self->client->ps.groundEntityNum != ENTITYNUM_NONE && !Any(self->s.eFlags & entityFlags_t::EF_DISINTEGRATION)) { // on the ground
        // FIXME: check 4 corners
        pitch_roll_for_slope(self, nullptr);
    }

    if (eventClearTime == level.time + ALERT_CLEAR_TIME) { // events were just cleared out so add me again
        if (!Any(self->client->ps.eFlags & entityFlags_t::EF_NODRAW)) {
            AddSightEvent(self->enemy, self->r.currentOrigin, 384, alertEventLevel_e::AEL_DISCOVERED, 0.0f);
        }
    }

    if (level.time - self->s.time > 3000) {                                        // been dead for 3 seconds
        if (g_dismember.integer < 11'381'138 && !g_saberRealisticCombat.integer) { // can't be dismembered once dead
            if (self->client->NPC_class != class_t::CLASS_PROTOCOL) {
                //	self->client->dismembered = true;
            }
        }
    }

    // if ( level.time - self->s.time > 500 )
    if (self->client->respawnTime < (level.time + 500)) { // don't turn "nonsolid" until about 1 second after actual death

        if (Any(self->client->ps.eFlags & entityFlags_t::EF_DISINTEGRATION)) {
            self->r.contents = contentsFlags_t::CONTENTS_NONE;
        }
        else if ((self->client->NPC_class != class_t::CLASS_MARK1) &&
                 (self->client->NPC_class != class_t::CLASS_INTERROGATOR)) // The Mark1 & Interrogator stays solid.
        {
            self->r.contents = contentsFlags_t::CONTENTS_CORPSE;
            // self->r.maxs[2] = -8;
        }

        if (self->message) {
            self->r.contents |= contentsFlags_t::CONTENTS_TRIGGER;
        }
    }
}

/*
----------------------------------------
NPC_RemoveBody

Determines when it's ok to ditch the corpse
----------------------------------------
*/
constexpr int REMOVE_DISTANCE = 128;
[[maybe_unused]] constexpr int REMOVE_DISTANCE_SQR = REMOVE_DISTANCE * REMOVE_DISTANCE;

void NPC_RemoveBody(gentity_t* self)
{
    CorpsePhysics(self);

    self->nextthink = level.time + FRAMETIME;

    if (self->NPC->nextBStateThink <= level.time) {
        trap->ICARUS_MaintainTaskManager(self->s.number);
    }
    self->NPC->nextBStateThink = level.time + FRAMETIME;

    if (self->message) { // I still have a key
        return;
    }

    // I don't consider this a hack, it's creative coding . . .
    // I agree, very creative... need something like this for ATST and GALAKMECH too!
    if (self->client->NPC_class == class_t::CLASS_MARK1) {
        Mark1_dying(self);
    }

    // Since these blow up, remove the bounding box.
    if (self->client->NPC_class == class_t::CLASS_REMOTE || self->client->NPC_class == class_t::CLASS_SENTRY ||
        self->client->NPC_class == class_t::CLASS_PROBE || self->client->NPC_class == class_t::CLASS_INTERROGATOR ||
        self->client->NPC_class == class_t::CLASS_MARK2) {
        // if ( !self->taskManager || !self->taskManager->IsRunning() )
        if (!trap->ICARUS_IsRunning(self->s.number)) {
            if (!self->activator || !self->activator->client ||
                !Any(self->activator->client->ps.eFlags2 & entityFlags2_t::EF2_HELD_BY_MONSTER)) { // not being held by a Rancor
                G_FreeEntity(self);
            }
        }
        return;
    }

    // FIXME: don't ever inflate back up?
    self->r.maxs[2] = self->client->renderInfo.eyePoint[2] - self->r.currentOrigin[2] + 4;
    if (self->r.maxs[2] < -8) {
        self->r.maxs[2] = -8;
    }

    if (self->client->NPC_class == class_t::CLASS_GALAKMECH) { // never disappears
        return;
    }
    if (self->NPC && self->NPC->timeOfDeath <= level.time) {
        self->NPC->timeOfDeath = level.time + 1000;
        // Only do all of this nonsense for Scav boys ( and girls )
        ///	if ( self->client->playerTeam == NPCTEAM_SCAVENGERS || self->client->playerTeam == NPCTEAM_KLINGON
        //		|| self->client->playerTeam == NPCTEAM_HIROGEN || self->client->playerTeam == NPCTEAM_MALON )
        // should I check NPC_class here instead of TEAM ? - dmv
        if (self->client->playerTeam == npcteam_t::NPCTEAM_ENEMY || self->client->NPC_class == class_t::CLASS_PROTOCOL) {
            self->nextthink = level.time + FRAMETIME; // try back in a second

            /*
            if ( DistanceSquared( g_entities[0].r.currentOrigin, self->r.currentOrigin ) <= REMOVE_DISTANCE_SQR )
            {
                return;
            }

            if ( (InFOV( self, &g_entities[0], 110, 90 )) ) // generous FOV check
            {
                if ( (NPC_ClearLOS2( &g_entities[0], self->r.currentOrigin )) )
                {
                    return;
                }
            }
            */
            // Don't care about this for MP I guess.
        }

        // FIXME: there are some conditions - such as heavy combat - in which we want
        //			to remove the bodies... but in other cases it's just weird, like
        //			when they're right behind you in a closed room and when they've been
        //			placed as dead NPCs by a designer...
        //			For now we just assume that a corpse with no enemy was
        //			placed in the map as a corpse
        if (self->enemy) {
            // if ( !self->taskManager || !self->taskManager->IsRunning() )
            if (!trap->ICARUS_IsRunning(self->s.number)) {
                if (!self->activator || !self->activator->client ||
                    !Any(self->activator->client->ps.eFlags2 & entityFlags2_t::EF2_HELD_BY_MONSTER)) { // not being held by a Rancor
                    if (self->client && self->client->ps.saberEntityNum > 0 && self->client->ps.saberEntityNum < ENTITYNUM_WORLD) {
                        gentity_t* saberent = &g_entities[self->client->ps.saberEntityNum];
                        if (saberent) {
                            G_FreeEntity(saberent);
                        }
                    }
                    G_FreeEntity(self);
                }
            }
        }
    }
}

/*
----------------------------------------
NPC_RemoveBody

Determines when it's ok to ditch the corpse
----------------------------------------
*/

int BodyRemovalPadTime(gentity_t* ent)
{
    int time;

    if (!ent || !ent->client)
        return 0;

    // team no longer indicates species/race, so in this case we'd use NPC_class, but
    switch (ent->client->NPC_class) {
    case class_t::CLASS_MOUSE:
    case class_t::CLASS_GONK:
    case class_t::CLASS_R2D2:
    case class_t::CLASS_R5D2:
    // case class_t::CLASS_PROTOCOL:
    case class_t::CLASS_MARK1:
    case class_t::CLASS_MARK2:
    case class_t::CLASS_PROBE:
    case class_t::CLASS_SEEKER:
    case class_t::CLASS_REMOTE:
    case class_t::CLASS_SENTRY:
    case class_t::CLASS_INTERROGATOR:
        time = 0;
        break;
    default:
        // never go away
        //	time = Q3_INFINITE;
        // for now I'm making default 10000
        time = 10000;
        break;
    }

    return time;
}

/*
----------------------------------------
NPC_RemoveBodyEffect

Effect to be applied when ditching the corpse
----------------------------------------
*/

static void NPC_RemoveBodyEffect(void)
{
    //	vec3_t		org;
    //	gentity_t	*tent;

    if (!NPCS.NPC || !NPCS.NPC->client || Any(NPCS.NPC->s.eFlags & entityFlags_t::EF_NODRAW))
        return;

    // team no longer indicates species/race, so in this case we'd use NPC_class, but

    // stub code
    switch (NPCS.NPC->client->NPC_class) {
    case class_t::CLASS_PROBE:
    case class_t::CLASS_SEEKER:
    case class_t::CLASS_REMOTE:
    case class_t::CLASS_SENTRY:
    case class_t::CLASS_GONK:
    case class_t::CLASS_R2D2:
    case class_t::CLASS_R5D2:
    // case class_t::CLASS_PROTOCOL:
    case class_t::CLASS_MARK1:
    case class_t::CLASS_MARK2:
    case class_t::CLASS_INTERROGATOR:
    case class_t::CLASS_ATST: // yeah, this is a little weird, but for now I'm listing all droids
                              //	VectorCopy( NPC->r.currentOrigin, org );
                              //	org[2] -= 16;
                              //	tent = G_TempEntity( org, EV_BOT_EXPLODE );
                              //	tent->owner = NPC;
        break;
    default:
        break;
    }
}

/*
====================================================================
void pitch_roll_for_slope( gentity_t *forwhom, vec3_t pass_slope )

MG

This will adjust the pitch and roll of a monster to match
a given slope - if a non-'0 0 0' slope is passed, it will
use that value, otherwise it will use the ground underneath
the monster.  If it doesn't find a surface, it does nothinh\g
and returns.
====================================================================
*/

void pitch_roll_for_slope(gentity_t* forwhom, vec3_t pass_slope)
{
    vec3_t slope;
    vec3_t nvf, ovf, ovr, startspot, endspot, new_angles = {0, 0, 0};
    float pitch, mod, dot;

    // if we don't have a slope, get one
    if (!pass_slope || VectorCompare(vec3_origin, pass_slope)) {
        trace_t trace;

        VectorCopy(forwhom->r.currentOrigin, startspot);
        startspot[2] += forwhom->r.mins[2] + 4;
        VectorCopy(startspot, endspot);
        endspot[2] -= 300;
        trap->Trace(&trace, forwhom->r.currentOrigin, vec3_origin, vec3_origin, endspot, forwhom->s.number, MASK_SOLID, false, g2TraceFlags_t::G2TRFLAG_NONE,
                    0);
        //		if(trace_fraction>0.05&&forwhom.movetype==MOVETYPE_STEP)
        //			forwhom.flags(-)FL_ONGROUND;

        if (trace.fraction >= 1.0)
            return;

        if (VectorCompare(vec3_origin, trace.plane.normal))
            return;

        VectorCopy(trace.plane.normal, slope);
    }
    else {
        VectorCopy(pass_slope, slope);
    }

    AngleVectors(forwhom->r.currentAngles, ovf, ovr, nullptr);

    vectoangles(slope, new_angles);
    pitch = new_angles[PITCH] + 90;
    new_angles[ROLL] = new_angles[PITCH] = 0;

    AngleVectors(new_angles, nvf, nullptr, nullptr);

    mod = DotProduct(nvf, ovr);

    if (mod < 0)
        mod = -1;
    else
        mod = 1;

    dot = DotProduct(nvf, ovf);

    if (forwhom->client) {
        float oldmins2;

        forwhom->client->ps.viewangles[PITCH] = dot * pitch;
        forwhom->client->ps.viewangles[ROLL] = ((1 - Q_fabsf(dot)) * pitch * mod);
        oldmins2 = forwhom->r.mins[2];
        forwhom->r.mins[2] = -24 + 12 * Q_fabs(forwhom->client->ps.viewangles[PITCH]) / 180.0f;
        // FIXME: if it gets bigger, move up
        if (oldmins2 > forwhom->r.mins[2]) { // our mins is now lower, need to move up
            // FIXME: trace?
            forwhom->client->ps.origin[2] += (oldmins2 - forwhom->r.mins[2]);
            forwhom->r.currentOrigin[2] = forwhom->client->ps.origin[2];
            trap->LinkEntity((sharedEntity_t*)forwhom);
        }
    }
    else {
        forwhom->r.currentAngles[PITCH] = dot * pitch;
        forwhom->r.currentAngles[ROLL] = ((1 - Q_fabsf(dot)) * pitch * mod);
    }
}

/*
----------------------------------------
DeadThink
----------------------------------------
*/
static void DeadThink(void)
{
    trace_t trace;

    // HACKHACKHACKHACKHACK
    // We should really have a seperate G2 bounding box (seperate from the physics bbox) for G2 collisions only
    // FIXME: don't ever inflate back up?
    NPCS.NPC->r.maxs[2] = NPCS.NPC->client->renderInfo.eyePoint[2] - NPCS.NPC->r.currentOrigin[2] + 4;
    if (NPCS.NPC->r.maxs[2] < -8) {
        NPCS.NPC->r.maxs[2] = -8;
    }
    if (VectorCompare(NPCS.NPC->client->ps.velocity, vec3_origin)) { // not flying through the air
        if (NPCS.NPC->r.mins[0] > -32) {
            NPCS.NPC->r.mins[0] -= 1;
            trap->Trace(&trace, NPCS.NPC->r.currentOrigin, NPCS.NPC->r.mins, NPCS.NPC->r.maxs, NPCS.NPC->r.currentOrigin, NPCS.NPC->s.number,
                        NPCS.NPC->clipmask, false, g2TraceFlags_t::G2TRFLAG_NONE, 0);
            if (trace.allsolid) {
                NPCS.NPC->r.mins[0] += 1;
            }
        }
        if (NPCS.NPC->r.maxs[0] < 32) {
            NPCS.NPC->r.maxs[0] += 1;
            trap->Trace(&trace, NPCS.NPC->r.currentOrigin, NPCS.NPC->r.mins, NPCS.NPC->r.maxs, NPCS.NPC->r.currentOrigin, NPCS.NPC->s.number,
                        NPCS.NPC->clipmask, false, g2TraceFlags_t::G2TRFLAG_NONE, 0);
            if (trace.allsolid) {
                NPCS.NPC->r.maxs[0] -= 1;
            }
        }
        if (NPCS.NPC->r.mins[1] > -32) {
            NPCS.NPC->r.mins[1] -= 1;
            trap->Trace(&trace, NPCS.NPC->r.currentOrigin, NPCS.NPC->r.mins, NPCS.NPC->r.maxs, NPCS.NPC->r.currentOrigin, NPCS.NPC->s.number,
                        NPCS.NPC->clipmask, false, g2TraceFlags_t::G2TRFLAG_NONE, 0);
            if (trace.allsolid) {
                NPCS.NPC->r.mins[1] += 1;
            }
        }
        if (NPCS.NPC->r.maxs[1] < 32) {
            NPCS.NPC->r.maxs[1] += 1;
            trap->Trace(&trace, NPCS.NPC->r.currentOrigin, NPCS.NPC->r.mins, NPCS.NPC->r.maxs, NPCS.NPC->r.currentOrigin, NPCS.NPC->s.number,
                        NPCS.NPC->clipmask, false, g2TraceFlags_t::G2TRFLAG_NONE, 0);
            if (trace.allsolid) {
                NPCS.NPC->r.maxs[1] -= 1;
            }
        }
    }
    // HACKHACKHACKHACKHACK

    // FIXME: tilt and fall off of ledges?
    // NPC_PostDeathThink();

    /*
    if ( !NPCInfo->timeOfDeath && NPC->client != nullptr && NPCInfo != nullptr )
    {
        //haven't finished death anim yet and were NOT given a specific amount of time to wait before removal
        animNumber_t	legsAnim	= NPC->client->ps.legsAnim;
        animation_t		*animations	= knownAnimFileSets[NPC->client->clientInfo.animFileIndex].animations;

        NPC->bounceCount = -1; // This is a cheap hack for optimizing the pointcontents check below

        //ghoul doesn't tell us this anymore
        //if ( NPC->client->renderInfo.legsFrame == animations[legsAnim].firstFrame + (animations[legsAnim].numFrames - 1) )
        {
            //reached the end of the death anim
            NPCInfo->timeOfDeath = level.time + BodyRemovalPadTime( NPC );
        }
    }
    else
    */
    {
        // death anim done (or were given a specific amount of time to wait before removal), wait the requisite amount of time them remove
        if (level.time >= NPCS.NPCInfo->timeOfDeath + BodyRemovalPadTime(NPCS.NPC)) {
            if (Any(NPCS.NPC->client->ps.eFlags & entityFlags_t::EF_NODRAW)) {
                if (!trap->ICARUS_IsRunning(NPCS.NPC->s.number))
                // if ( !NPC->taskManager || !NPC->taskManager->IsRunning() )
                {
                    NPCS.NPC->think = G_FreeEntity;
                    NPCS.NPC->nextthink = level.time + FRAMETIME;
                }
            }
            else {
                class_t npc_class;

                // Start the body effect first, then delay 400ms before ditching the corpse
                NPC_RemoveBodyEffect();

                // FIXME: keep it running through physics somehow?
                NPCS.NPC->think = NPC_RemoveBody;
                NPCS.NPC->nextthink = level.time + FRAMETIME;
                //	if ( NPC->client->playerTeam == NPCTEAM_FORGE )
                //		NPCInfo->timeOfDeath = level.time + FRAMETIME * 8;
                //	else if ( NPC->client->playerTeam == NPCTEAM_BOTS )
                npc_class = NPCS.NPC->client->NPC_class;
                // check for droids
                if (npc_class == class_t::CLASS_SEEKER || npc_class == class_t::CLASS_REMOTE || npc_class == class_t::CLASS_PROBE ||
                    npc_class == class_t::CLASS_MOUSE || npc_class == class_t::CLASS_GONK || npc_class == class_t::CLASS_R2D2 ||
                    npc_class == class_t::CLASS_R5D2 || npc_class == class_t::CLASS_MARK2 ||
                    npc_class == class_t::CLASS_SENTRY) // npc_class == class_t::CLASS_PROTOCOL ||
                {
                    NPCS.NPC->client->ps.eFlags |= entityFlags_t::EF_NODRAW;
                    NPCS.NPCInfo->timeOfDeath = level.time + FRAMETIME * 8;
                }
                else
                    NPCS.NPCInfo->timeOfDeath = level.time + FRAMETIME * 4;
            }
            return;
        }
    }

    // If the player is on the ground and the resting position contents haven't been set yet...(BounceCount tracks the contents)
    if (NPCS.NPC->bounceCount < 0 && NPCS.NPC->s.groundEntityNum >= 0) {
        // if client is in a nodrop area, make him/her nodraw
        const contentsFlags_t contents = trap->PointContents(NPCS.NPC->r.currentOrigin, -1);
        NPCS.NPC->bounceCount = to_underlying(contents);

        if (Any(contents & contentsFlags_t::CONTENTS_NODROP)) {
            NPCS.NPC->client->ps.eFlags |= entityFlags_t::EF_NODRAW;
        }
    }

    CorpsePhysics(NPCS.NPC);
}

/*
===============
SetNPCGlobals

local function to set globals used throughout the AI code
===============
*/
void SetNPCGlobals(gentity_t* ent)
{
    NPCS.NPC = ent;
    NPCS.NPCInfo = ent->NPC;
    NPCS.client = ent->client;
    memset(&NPCS.ucmd, 0, sizeof(usercmd_t));
}

npcStatic_t _saved_NPCS;

void SaveNPCGlobals(void)
{
    memcpy(&_saved_NPCS, &NPCS, sizeof(_saved_NPCS));
}

void RestoreNPCGlobals(void)
{
    memcpy(&NPCS, &_saved_NPCS, sizeof(_saved_NPCS));
}

// We MUST do this, other funcs were using NPC illegally when "self" wasn't the global NPC
void ClearNPCGlobals(void)
{
    NPCS.NPC = nullptr;
    NPCS.NPCInfo = nullptr;
    NPCS.client = nullptr;
}
//===============

extern bool showBBoxes;
vec3_t NPCDEBUG_RED = {1.0, 0.0, 0.0};
vec3_t NPCDEBUG_GREEN = {0.0, 1.0, 0.0};
vec3_t NPCDEBUG_BLUE = {0.0, 0.0, 1.0};
vec3_t NPCDEBUG_LIGHT_BLUE = {0.3f, 0.7f, 1.0};
extern void G_Cube(vec3_t mins, vec3_t maxs, vec3_t color, float alpha);
extern void G_Line(vec3_t start, vec3_t end, vec3_t color, float alpha);
extern void G_Cylinder(vec3_t start, vec3_t end, float radius, vec3_t color);

void NPC_ShowDebugInfo(void)
{
    if (showBBoxes) {
        gentity_t* found = nullptr;
        vec3_t mins, maxs;

        while ((found = G_Find(found, FOFS(classname), "NPC")) != nullptr) {
            if (trap->InPVS(found->r.currentOrigin, g_entities[0].r.currentOrigin)) {
                VectorAdd(found->r.currentOrigin, found->r.mins, mins);
                VectorAdd(found->r.currentOrigin, found->r.maxs, maxs);
                G_Cube(mins, maxs, NPCDEBUG_RED, 0.25);
            }
        }
    }
}

void NPC_ApplyScriptFlags(void)
{
    if (Any(NPCS.NPCInfo->scriptFlags & scriptFlags_t::SCF_CROUCHED)) {
        if (NPCS.NPCInfo->charmedTime > level.time &&
            (NPCS.ucmd.forwardmove || NPCS.ucmd.rightmove)) { // ugh, if charmed and moving, ignore the crouched command
        }
        else {
            NPCS.ucmd.upmove = -127;
        }
    }

    if (Any(NPCS.NPCInfo->scriptFlags & scriptFlags_t::SCF_RUNNING)) {
        NPCS.ucmd.buttons &= ~buttonFlags_t::BUTTON_WALKING;
    }
    else if (Any(NPCS.NPCInfo->scriptFlags & scriptFlags_t::SCF_WALKING)) {
        if (NPCS.NPCInfo->charmedTime > level.time && (NPCS.ucmd.forwardmove || NPCS.ucmd.rightmove)) { // ugh, if charmed and moving, ignore the walking
                                                                                                        // command
        }
        else {
            NPCS.ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
        }
    }
    /*
        if(NPCInfo->scriptFlags & SCF_CAREFUL)
        {
            ucmd.buttons |= BUTTON_CAREFUL;
        }
    */
    if (Any(NPCS.NPCInfo->scriptFlags & scriptFlags_t::SCF_LEAN_RIGHT)) {
        NPCS.ucmd.buttons |= buttonFlags_t::BUTTON_USE;
        NPCS.ucmd.rightmove = 127;
        NPCS.ucmd.forwardmove = 0;
        NPCS.ucmd.upmove = 0;
    }
    else if (Any(NPCS.NPCInfo->scriptFlags & scriptFlags_t::SCF_LEAN_LEFT)) {
        NPCS.ucmd.buttons |= buttonFlags_t::BUTTON_USE;
        NPCS.ucmd.rightmove = -127;
        NPCS.ucmd.forwardmove = 0;
        NPCS.ucmd.upmove = 0;
    }

    if (Any(NPCS.NPCInfo->scriptFlags & scriptFlags_t::SCF_ALT_FIRE) && Any(NPCS.ucmd.buttons & buttonFlags_t::BUTTON_ATTACK)) { // Use altfire instead
        NPCS.ucmd.buttons |= buttonFlags_t::BUTTON_ALT_ATTACK;
    }
}

void NPC_HandleAIFlags(void)
{
    // FIXME: make these flags checks a function call like NPC_CheckAIFlagsAndTimers
    if (Any(NPCS.NPCInfo->aiFlags & npcAiFlags_t::NPCAI_LOST)) { // Print that you need help!
        // FIXME: shouldn't remove this just yet if cg_draw needs it
        NPCS.NPCInfo->aiFlags &= ~npcAiFlags_t::NPCAI_LOST;

        /*
        if ( showWaypoints )
        {
            G_DebugPrintln(WL_e::WL_WARNING, "{} can't navigate to target {} (my wp: {}, goal wp: {})", NPC->targetname, NPCInfo->goalEntity->targetname,
        NPC->waypoint, NPCInfo->goalEntity->waypoint);
        }
        */

        if (NPCS.NPCInfo->goalEntity && NPCS.NPCInfo->goalEntity == NPCS.NPC->enemy) { // We can't nav to our enemy
            // Drop enemy and see if we should search for him
            NPC_LostEnemyDecideChase();
        }
    }

    // MRJ Request:
    /*
    if ( Any(NPCInfo->aiFlags & npcAiFlags_t::NPCAI_GREET_ALLIES) && !NPC->enemy )//what if "enemy" is the greetEnt?
    {//If no enemy, look for teammates to greet
        //FIXME: don't say hi to the same guy over and over again.
        if ( NPCInfo->greetingDebounceTime < level.time )
        {//Has been at least 2 seconds since we greeted last
            if ( !NPCInfo->greetEnt )
            {//Find a teammate whom I'm facing and who is facing me and within 128
                NPCInfo->greetEnt = NPC_PickAlly( true, 128, true, true );
            }

            if ( NPCInfo->greetEnt && !Q_irand(0, 5) )
            {//Start greeting someone
                bool	greeted = false;

                //TODO:  If have a greetscript, run that instead?

                //FIXME: make them greet back?
                if( !Q_irand( 0, 2 ) )
                {//Play gesture anim (press gesture button?)
                    greeted = true;
                    NPC_SetAnim( NPC, setAnimParts_t::SETANIM_TORSO, Q_irand( animNumber_t::BOTH_GESTURE1, BOTH_GESTURE3 ),
    setAnimFlags_t::SETANIM_FLAG_NORMAL|setAnimFlags_t::SETANIM_FLAG_HOLD );
                    //NOTE: play full-body gesture if not moving?
                }

                if( !Q_irand( 0, 2 ) )
                {//Play random voice greeting sound
                    greeted = true;
                    //FIXME: need NPC sound sets

                    //G_AddVoiceEvent( NPC, Q_irand(EV_GREET1, EV_GREET3), 2000 );
                }

                if( !Q_irand( 0, 1 ) )
                {//set looktarget to them for a second or two
                    greeted = true;
                    NPC_TempLookTarget( NPC, NPCInfo->greetEnt->s.number, 1000, 3000 );
                }

                if ( greeted )
                {//Did at least one of the things above
                    //Don't greet again for 2 - 4 seconds
                    NPCInfo->greetingDebounceTime = level.time + Q_irand( 2000, 4000 );
                    NPCInfo->greetEnt = nullptr;
                }
            }
        }
    }
    */
    // been told to play a victory sound after a delay
    if (NPCS.NPCInfo->greetingDebounceTime && NPCS.NPCInfo->greetingDebounceTime < level.time) {
        // the event drawn first on every compiler (the order of a call's arguments is unspecified)
        const entity_event_t victory = Q_irand(entity_event_t::EV_VICTORY1, entity_event_t::EV_VICTORY3);
        G_AddVoiceEvent(NPCS.NPC, victory, Q_irand(2000, 4000));
        NPCS.NPCInfo->greetingDebounceTime = 0;
    }

    if (NPCS.NPCInfo->ffireCount > 0) {
        if (NPCS.NPCInfo->ffireFadeDebounce < level.time) {
            NPCS.NPCInfo->ffireCount--;
            // Q_Println("drop: {} < {}", NPCInfo->ffireCount, 3+((2-g_npcspskill.integer)*2));
            NPCS.NPCInfo->ffireFadeDebounce = level.time + 3000;
        }
    }
    if (d_patched.integer) {                              // use patch-style navigation
        if (NPCS.NPCInfo->consecutiveBlockedMoves > 20) { // been stuck for a while, try again?
            NPCS.NPCInfo->consecutiveBlockedMoves = 0;
        }
    }
}

void NPC_AvoidWallsAndCliffs(void)
{
    //...
}

void NPC_CheckAttackScript(void)
{
    if (!Any(NPCS.ucmd.buttons & buttonFlags_t::BUTTON_ATTACK)) {
        return;
    }

    G_ActivateBehavior(NPCS.NPC, bSet_t::BSET_ATTACK);
}

float NPC_MaxDistSquaredForWeapon(void);
void NPC_CheckAttackHold(void)
{
    vec3_t vec;

    // If they don't have an enemy they shouldn't hold their attack anim.
    if (!NPCS.NPC->enemy) {
        NPCS.NPCInfo->attackHoldTime = 0;
        return;
    }

    /*	if ( ( NPC->client->ps.weapon == WP_BORG_ASSIMILATOR ) || ( NPC->client->ps.weapon == WP_BORG_DRILL ) )
        {//FIXME: don't keep holding this if can't hit enemy?

            // If they don't have shields ( been disabled) they shouldn't hold their attack anim.
            if ( !Any(NPC->NPC->aiFlags & npcAiFlags_t::NPCAI_SHIELDS) )
            {
                NPCInfo->attackHoldTime = 0;
                return;
            }

            VectorSubtract(NPC->enemy->r.currentOrigin, NPC->r.currentOrigin, vec);
            if( VectorLengthSquared(vec) > NPC_MaxDistSquaredForWeapon() )
            {
                NPCInfo->attackHoldTime = 0;
                PM_SetTorsoAnimTimer(NPC, &NPC->client->ps.torsoAnimTimer, 0);
            }
            else if( NPCInfo->attackHoldTime && NPCInfo->attackHoldTime > level.time )
            {
                ucmd.buttons |= buttonFlags_t::BUTTON_ATTACK;
            }
            else if ( ( NPCInfo->attackHold ) && Any( ucmd.buttons & buttonFlags_t::BUTTON_ATTACK ) )
            {
                NPCInfo->attackHoldTime = level.time + NPCInfo->attackHold;
                PM_SetTorsoAnimTimer(NPC, &NPC->client->ps.torsoAnimTimer, NPCInfo->attackHold);
            }
            else
            {
                NPCInfo->attackHoldTime = 0;
                PM_SetTorsoAnimTimer(NPC, &NPC->client->ps.torsoAnimTimer, 0);
            }
        }
        else*/
    { // everyone else...?  FIXME: need to tie this into AI somehow?
        VectorSubtract(NPCS.NPC->enemy->r.currentOrigin, NPCS.NPC->r.currentOrigin, vec);
        if (VectorLengthSquared(vec) > NPC_MaxDistSquaredForWeapon()) {
            NPCS.NPCInfo->attackHoldTime = 0;
        }
        else if (NPCS.NPCInfo->attackHoldTime && NPCS.NPCInfo->attackHoldTime > level.time) {
            NPCS.ucmd.buttons |= buttonFlags_t::BUTTON_ATTACK;
        }
        else if ((NPCS.NPCInfo->attackHold) && Any(NPCS.ucmd.buttons & buttonFlags_t::BUTTON_ATTACK)) {
            NPCS.NPCInfo->attackHoldTime = level.time + NPCS.NPCInfo->attackHold;
        }
        else {
            NPCS.NPCInfo->attackHoldTime = 0;
        }
    }
}

/*
void NPC_KeepCurrentFacing(void)

Fills in a default ucmd to keep current angles facing
*/
void NPC_KeepCurrentFacing(void)
{
    if (!NPCS.ucmd.angles[YAW]) {
        NPCS.ucmd.angles[YAW] = ANGLE2SHORT(NPCS.client->ps.viewangles[YAW]) - NPCS.client->ps.delta_angles[YAW];
    }

    if (!NPCS.ucmd.angles[PITCH]) {
        NPCS.ucmd.angles[PITCH] = ANGLE2SHORT(NPCS.client->ps.viewangles[PITCH]) - NPCS.client->ps.delta_angles[PITCH];
    }
}

/*
-------------------------
NPC_BehaviorSet_Charmed
-------------------------
*/

void NPC_BehaviorSet_Charmed(bState_t bState)
{
    switch (bState) {
    case bState_t::BS_FOLLOW_LEADER: // # 40: Follow your leader and shoot any enemies you come across
        NPC_BSFollowLeader();
        break;
    case bState_t::BS_REMOVE:
        NPC_BSRemove();
        break;
    case bState_t::BS_SEARCH: // # 43: Using current waypoint as a base, search the immediate branches of waypoints for enemies
        NPC_BSSearch();
        break;
    case bState_t::BS_WANDER: // # 46: Wander down random waypoint paths
        NPC_BSWander();
        break;
    case bState_t::BS_FLEE:
        NPC_BSFlee();
        break;
    default:
    case bState_t::BS_DEFAULT: // whatever
        NPC_BSDefault();
        break;
    }
}
/*
-------------------------
NPC_BehaviorSet_Default
-------------------------
*/

void NPC_BehaviorSet_Default(bState_t bState)
{
    switch (bState) {
    case bState_t::BS_ADVANCE_FIGHT: // head toward captureGoal, shoot anything that gets in the way
        NPC_BSAdvanceFight();
        break;
    case bState_t::BS_SLEEP: // Follow a path, looking for enemies
        NPC_BSSleep();
        break;
    case bState_t::BS_FOLLOW_LEADER: // # 40: Follow your leader and shoot any enemies you come across
        NPC_BSFollowLeader();
        break;
    case bState_t::BS_JUMP: // 41: Face navgoal and jump to it.
        NPC_BSJump();
        break;
    case bState_t::BS_REMOVE:
        NPC_BSRemove();
        break;
    case bState_t::BS_SEARCH: // # 43: Using current waypoint as a base, search the immediate branches of waypoints for enemies
        NPC_BSSearch();
        break;
    case bState_t::BS_NOCLIP:
        NPC_BSNoClip();
        break;
    case bState_t::BS_WANDER: // # 46: Wander down random waypoint paths
        NPC_BSWander();
        break;
    case bState_t::BS_FLEE:
        NPC_BSFlee();
        break;
    case bState_t::BS_WAIT:
        NPC_BSWait();
        break;
    case bState_t::BS_CINEMATIC:
        NPC_BSCinematic();
        break;
    default:
    case bState_t::BS_DEFAULT: // whatever
        NPC_BSDefault();
        break;
    }
}

/*
-------------------------
NPC_BehaviorSet_Interrogator
-------------------------
*/
void NPC_BehaviorSet_Interrogator(bState_t bState)
{
    switch (bState) {
    case bState_t::BS_STAND_GUARD:
    case bState_t::BS_PATROL:
    case bState_t::BS_STAND_AND_SHOOT:
    case bState_t::BS_HUNT_AND_KILL:
    case bState_t::BS_DEFAULT:
        NPC_BSInterrogator_Default();
        break;
    default:
        NPC_BehaviorSet_Default(bState);
        break;
    }
}

void NPC_BSImperialProbe_Attack(void);
void NPC_BSImperialProbe_Patrol(void);
void NPC_BSImperialProbe_Wait(void);

/*
-------------------------
NPC_BehaviorSet_ImperialProbe
-------------------------
*/
void NPC_BehaviorSet_ImperialProbe(bState_t bState)
{
    switch (bState) {
    case bState_t::BS_STAND_GUARD:
    case bState_t::BS_PATROL:
    case bState_t::BS_STAND_AND_SHOOT:
    case bState_t::BS_HUNT_AND_KILL:
    case bState_t::BS_DEFAULT:
        NPC_BSImperialProbe_Default();
        break;
    default:
        NPC_BehaviorSet_Default(bState);
        break;
    }
}

void NPC_BSSeeker_Default(void);

/*
-------------------------
NPC_BehaviorSet_Seeker
-------------------------
*/
void NPC_BehaviorSet_Seeker(bState_t bState)
{
    switch (bState) {
    case bState_t::BS_STAND_GUARD:
    case bState_t::BS_PATROL:
    case bState_t::BS_STAND_AND_SHOOT:
    case bState_t::BS_HUNT_AND_KILL:
    case bState_t::BS_DEFAULT:
        NPC_BSSeeker_Default();
        break;
    default:
        NPC_BehaviorSet_Default(bState);
        break;
    }
}

void NPC_BSRemote_Default(void);

/*
-------------------------
NPC_BehaviorSet_Remote
-------------------------
*/
void NPC_BehaviorSet_Remote(bState_t bState)
{
    NPC_BSRemote_Default();
}

void NPC_BSSentry_Default(void);

/*
-------------------------
NPC_BehaviorSet_Sentry
-------------------------
*/
void NPC_BehaviorSet_Sentry(bState_t bState)
{
    switch (bState) {
    case bState_t::BS_STAND_GUARD:
    case bState_t::BS_PATROL:
    case bState_t::BS_STAND_AND_SHOOT:
    case bState_t::BS_HUNT_AND_KILL:
    case bState_t::BS_DEFAULT:
        NPC_BSSentry_Default();
        break;
    default:
        NPC_BehaviorSet_Default(bState);
        break;
    }
}

/*
-------------------------
NPC_BehaviorSet_Grenadier
-------------------------
*/
void NPC_BehaviorSet_Grenadier(bState_t bState)
{
    switch (bState) {
    case bState_t::BS_STAND_GUARD:
    case bState_t::BS_PATROL:
    case bState_t::BS_STAND_AND_SHOOT:
    case bState_t::BS_HUNT_AND_KILL:
    case bState_t::BS_DEFAULT:
        NPC_BSGrenadier_Default();
        break;

    default:
        NPC_BehaviorSet_Default(bState);
        break;
    }
}
/*
-------------------------
NPC_BehaviorSet_Sniper
-------------------------
*/
void NPC_BehaviorSet_Sniper(bState_t bState)
{
    switch (bState) {
    case bState_t::BS_STAND_GUARD:
    case bState_t::BS_PATROL:
    case bState_t::BS_STAND_AND_SHOOT:
    case bState_t::BS_HUNT_AND_KILL:
    case bState_t::BS_DEFAULT:
        NPC_BSSniper_Default();
        break;

    default:
        NPC_BehaviorSet_Default(bState);
        break;
    }
}
/*
-------------------------
NPC_BehaviorSet_Stormtrooper
-------------------------
*/

void NPC_BehaviorSet_Stormtrooper(bState_t bState)
{
    switch (bState) {
    case bState_t::BS_STAND_GUARD:
    case bState_t::BS_PATROL:
    case bState_t::BS_STAND_AND_SHOOT:
    case bState_t::BS_HUNT_AND_KILL:
    case bState_t::BS_DEFAULT:
        NPC_BSST_Default();
        break;

    case bState_t::BS_INVESTIGATE:
        NPC_BSST_Investigate();
        break;

    case bState_t::BS_SLEEP:
        NPC_BSST_Sleep();
        break;

    default:
        NPC_BehaviorSet_Default(bState);
        break;
    }
}

/*
-------------------------
NPC_BehaviorSet_Jedi
-------------------------
*/

void NPC_BehaviorSet_Jedi(bState_t bState)
{
    switch (bState) {
    case bState_t::BS_STAND_GUARD:
    case bState_t::BS_PATROL:
    case bState_t::BS_STAND_AND_SHOOT:
    case bState_t::BS_HUNT_AND_KILL:
    case bState_t::BS_DEFAULT:
        NPC_BSJedi_Default();
        break;

    case bState_t::BS_FOLLOW_LEADER:
        NPC_BSJedi_FollowLeader();
        break;

    default:
        NPC_BehaviorSet_Default(bState);
        break;
    }
}

/*
-------------------------
NPC_BehaviorSet_Droid
-------------------------
*/
void NPC_BehaviorSet_Droid(bState_t bState)
{
    switch (bState) {
    case bState_t::BS_DEFAULT:
    case bState_t::BS_STAND_GUARD:
    case bState_t::BS_PATROL:
        NPC_BSDroid_Default();
        break;
    default:
        NPC_BehaviorSet_Default(bState);
        break;
    }
}

/*
-------------------------
NPC_BehaviorSet_Mark1
-------------------------
*/
void NPC_BehaviorSet_Mark1(bState_t bState)
{
    switch (bState) {
    case bState_t::BS_DEFAULT:
    case bState_t::BS_STAND_GUARD:
    case bState_t::BS_PATROL:
        NPC_BSMark1_Default();
        break;
    default:
        NPC_BehaviorSet_Default(bState);
        break;
    }
}

/*
-------------------------
NPC_BehaviorSet_Mark2
-------------------------
*/
void NPC_BehaviorSet_Mark2(bState_t bState)
{
    switch (bState) {
    case bState_t::BS_DEFAULT:
    case bState_t::BS_PATROL:
    case bState_t::BS_STAND_AND_SHOOT:
    case bState_t::BS_HUNT_AND_KILL:
        NPC_BSMark2_Default();
        break;
    default:
        NPC_BehaviorSet_Default(bState);
        break;
    }
}

/*
-------------------------
NPC_BehaviorSet_ATST
-------------------------
*/
void NPC_BehaviorSet_ATST(bState_t bState)
{
    switch (bState) {
    case bState_t::BS_DEFAULT:
    case bState_t::BS_PATROL:
    case bState_t::BS_STAND_AND_SHOOT:
    case bState_t::BS_HUNT_AND_KILL:
        NPC_BSATST_Default();
        break;
    default:
        NPC_BehaviorSet_Default(bState);
        break;
    }
}

/*
-------------------------
NPC_BehaviorSet_MineMonster
-------------------------
*/
void NPC_BehaviorSet_MineMonster(bState_t bState)
{
    switch (bState) {
    case bState_t::BS_STAND_GUARD:
    case bState_t::BS_PATROL:
    case bState_t::BS_STAND_AND_SHOOT:
    case bState_t::BS_HUNT_AND_KILL:
    case bState_t::BS_DEFAULT:
        NPC_BSMineMonster_Default();
        break;
    default:
        NPC_BehaviorSet_Default(bState);
        break;
    }
}

/*
-------------------------
NPC_BehaviorSet_Howler
-------------------------
*/
void NPC_BehaviorSet_Howler(bState_t bState)
{
    switch (bState) {
    case bState_t::BS_STAND_GUARD:
    case bState_t::BS_PATROL:
    case bState_t::BS_STAND_AND_SHOOT:
    case bState_t::BS_HUNT_AND_KILL:
    case bState_t::BS_DEFAULT:
        NPC_BSHowler_Default();
        break;
    default:
        NPC_BehaviorSet_Default(bState);
        break;
    }
}

/*
-------------------------
NPC_BehaviorSet_Rancor
-------------------------
*/
void NPC_BehaviorSet_Rancor(bState_t bState)
{
    switch (bState) {
    case bState_t::BS_STAND_GUARD:
    case bState_t::BS_PATROL:
    case bState_t::BS_STAND_AND_SHOOT:
    case bState_t::BS_HUNT_AND_KILL:
    case bState_t::BS_DEFAULT:
        NPC_BSRancor_Default();
        break;
    default:
        NPC_BehaviorSet_Default(bState);
        break;
    }
}

/*
-------------------------
NPC_RunBehavior
-------------------------
*/
extern void NPC_BSEmplaced(void);
extern bool NPC_CheckSurrender(void);
extern void Boba_FlyStop(gentity_t* self);
extern void NPC_BSWampa_Default(void);
extern bool Jedi_CultistDestroyer(gentity_t* self);
void NPC_RunBehavior(npcteam_t team, bState_t bState)
{
    if (NPCS.NPC->s.NPC_class == class_t::CLASS_VEHICLE && NPCS.NPC->m_pVehicle) { // vehicles don't do AI!
        return;
    }

    if (bState == bState_t::BS_CINEMATIC) {
        NPC_BSCinematic();
    }
    else if (NPCS.NPC->client->ps.weapon == weapon_t::WP_EMPLACED_GUN) {
        NPC_BSEmplaced();
        NPC_CheckCharmed();
        return;
    }
    //	else if ( NPCS.NPC->client->ps.weapon == weapon_t::WP_SABER )		// this is an _extremely_ shitty comparison.. FIXME: make a CLASS_CULTIST? --eez
    else if (NPCS.NPC->client->NPC_class == class_t::CLASS_JEDI || NPCS.NPC->client->NPC_class == class_t::CLASS_REBORN ||
             NPCS.NPC->client->ps.weapon == weapon_t::WP_SABER) { // jedi
        NPC_BehaviorSet_Jedi(bState);
    }
    else if (NPCS.NPC->client->NPC_class == class_t::CLASS_WAMPA) { // wampa
        NPC_BSWampa_Default();
    }
    else if (NPCS.NPC->client->NPC_class == class_t::CLASS_RANCOR) { // rancor
        NPC_BehaviorSet_Rancor(bState);
    }
    else if (NPCS.NPC->client->NPC_class == class_t::CLASS_REMOTE) {
        NPC_BehaviorSet_Remote(bState);
    }
    else if (NPCS.NPC->client->NPC_class == class_t::CLASS_SEEKER) {
        NPC_BehaviorSet_Seeker(bState);
    }
    else if (NPCS.NPC->client->NPC_class == class_t::CLASS_BOBAFETT) { // bounty hunter
        if (Boba_Flying(NPCS.NPC)) {
            NPC_BehaviorSet_Seeker(bState);
        }
        else {
            NPC_BehaviorSet_Jedi(bState);
        }
    }
    else if (Jedi_CultistDestroyer(NPCS.NPC)) {
        NPC_BSJedi_Default();
    }
    else if (Any(NPCS.NPCInfo->scriptFlags & scriptFlags_t::SCF_FORCED_MARCH)) { // being forced to march
        NPC_BSDefault();
    }
    else {
        switch (team) {

            //	case NPCTEAM_SCAVENGERS:
            //	case NPCTEAM_IMPERIAL:
            //	case NPCTEAM_KLINGON:
            //	case NPCTEAM_HIROGEN:
            //	case NPCTEAM_MALON:
            // not sure if TEAM_ENEMY is appropriate here, I think I should be using NPC_class to check for behavior - dmv
        case npcteam_t::NPCTEAM_ENEMY:
            // special cases for enemy droids
            switch (NPCS.NPC->client->NPC_class) {
            case class_t::CLASS_ATST:
                NPC_BehaviorSet_ATST(bState);
                return;
            case class_t::CLASS_PROBE:
                NPC_BehaviorSet_ImperialProbe(bState);
                return;
            case class_t::CLASS_REMOTE:
                NPC_BehaviorSet_Remote(bState);
                return;
            case class_t::CLASS_SENTRY:
                NPC_BehaviorSet_Sentry(bState);
                return;
            case class_t::CLASS_INTERROGATOR:
                NPC_BehaviorSet_Interrogator(bState);
                return;
            case class_t::CLASS_MINEMONSTER:
                NPC_BehaviorSet_MineMonster(bState);
                return;
            case class_t::CLASS_HOWLER:
                NPC_BehaviorSet_Howler(bState);
                return;
            case class_t::CLASS_MARK1:
                NPC_BehaviorSet_Mark1(bState);
                return;
            case class_t::CLASS_MARK2:
                NPC_BehaviorSet_Mark2(bState);
                return;
            case class_t::CLASS_GALAKMECH:
                NPC_BSGM_Default();
                return;
            default:
                break;
            }

            if (NPCS.NPC->enemy && NPCS.NPC->s.weapon == weapon_t::WP_NONE && bState != bState_t::BS_HUNT_AND_KILL &&
                !trap->ICARUS_TaskIDPending(
                    (sharedEntity_t*)NPCS.NPC,
                    taskID_t::TID_MOVE_NAV)) { // if in battle and have no weapon, run away, fixme: when in BS_HUNT_AND_KILL, they just stand there
                if (bState != bState_t::BS_FLEE) {
                    NPC_StartFlee(NPCS.NPC->enemy, NPCS.NPC->enemy->r.currentOrigin, alertEventLevel_e::AEL_DANGER_GREAT, 5000, 10000);
                }
                else {
                    NPC_BSFlee();
                }
                return;
            }
            if (NPCS.NPC->client->ps.weapon == weapon_t::WP_SABER) { // special melee exception
                NPC_BehaviorSet_Default(bState);
                return;
            }
            if (NPCS.NPC->client->ps.weapon == weapon_t::WP_DISRUPTOR && Any(NPCS.NPCInfo->scriptFlags & scriptFlags_t::SCF_ALT_FIRE)) { // a sniper
                NPC_BehaviorSet_Sniper(bState);
                return;
            }
            if (NPCS.NPC->client->ps.weapon == weapon_t::WP_THERMAL ||
                NPCS.NPC->client->ps.weapon == weapon_t::WP_STUN_BATON) // FIXME: separate AI for melee fighters
            {                                                           // a grenadier
                NPC_BehaviorSet_Grenadier(bState);
                return;
            }
            if (NPC_CheckSurrender()) {
                return;
            }
            NPC_BehaviorSet_Stormtrooper(bState);
            break;

        case npcteam_t::NPCTEAM_NEUTRAL:

            // special cases for enemy droids
            if (NPCS.NPC->client->NPC_class == class_t::CLASS_PROTOCOL || NPCS.NPC->client->NPC_class == class_t::CLASS_UGNAUGHT ||
                NPCS.NPC->client->NPC_class == class_t::CLASS_JAWA) {
                NPC_BehaviorSet_Default(bState);
            }
            else if (NPCS.NPC->client->NPC_class == class_t::CLASS_VEHICLE) {
                // TODO: Add vehicle behaviors here.
                NPC_UpdateAngles(true, true); // just face our spawn angles for now
            }
            else {
                // Just one of the average droids
                NPC_BehaviorSet_Droid(bState);
            }
            break;

        default:
            if (NPCS.NPC->client->NPC_class == class_t::CLASS_SEEKER) {
                NPC_BehaviorSet_Seeker(bState);
            }
            else {
                if (NPCS.NPCInfo->charmedTime > level.time) {
                    NPC_BehaviorSet_Charmed(bState);
                }
                else {
                    NPC_BehaviorSet_Default(bState);
                }
                NPC_CheckCharmed();
            }
            break;
        }
    }
}

/*
===============
NPC_ExecuteBState

  MCG

NPC Behavior state thinking

===============
*/
void NPC_ExecuteBState(gentity_t* self) //, int msec )
{
    bState_t bState;

    NPC_HandleAIFlags();

    // FIXME: these next three bits could be a function call, some sort of setup/cleanup func
    // Lookmode must be reset every think cycle
    if (NPCS.NPC->delayScriptTime && NPCS.NPC->delayScriptTime <= level.time) {
        G_ActivateBehavior(NPCS.NPC, bSet_t::BSET_DELAYED);
        NPCS.NPC->delayScriptTime = 0;
    }

    // Clear this and let bState set it itself, so it automatically handles changing bStates... but we need a set bState wrapper func
    NPCS.NPCInfo->combatMove = false;

    // Execute our bState
    if (NPCS.NPCInfo->tempBehavior != bState_t::BS_DEFAULT) { // Overrides normal behavior until cleared
        bState = NPCS.NPCInfo->tempBehavior;
    }
    else {
        if (NPCS.NPCInfo->behaviorState == bState_t::BS_DEFAULT)
            NPCS.NPCInfo->behaviorState = NPCS.NPCInfo->defaultBehavior;

        bState = NPCS.NPCInfo->behaviorState;
    }

    // Pick the proper bstate for us and run it
    NPC_RunBehavior(self->client->playerTeam, bState);

    //	if(bState != BS_POINT_COMBAT && NPCInfo->combatPoint != -1)
    //	{
    // level.combatPoints[NPCInfo->combatPoint].occupied = false;
    // NPCInfo->combatPoint = -1;
    //	}

    // Here we need to see what the scripted stuff told us to do
    // Only process snapshot if independent and in combat mode- this would pick enemies and go after needed items
    //	ProcessSnapshot();

    // Ignore my needs if I'm under script control- this would set needs for items
    //	CheckSelf();

    // Back to normal?  All decisions made?

    // FIXME: don't walk off ledges unless we can get to our goal faster that way, or that's our goal's surface
    // NPCPredict();

    if (NPCS.NPC->enemy) {
        if (!NPCS.NPC->enemy->inuse) { // just in case bState doesn't catch this
            G_ClearEnemy(NPCS.NPC);
        }
    }

    if (NPCS.NPC->client->ps.saberLockTime && NPCS.NPC->client->ps.saberLockEnemy != ENTITYNUM_NONE) {
        NPC_SetLookTarget(NPCS.NPC, NPCS.NPC->client->ps.saberLockEnemy, level.time + 1000);
    }
    else if (!NPC_CheckLookTarget(NPCS.NPC)) {
        if (NPCS.NPC->enemy) {
            NPC_SetLookTarget(NPCS.NPC, NPCS.NPC->enemy->s.number, 0);
        }
    }

    if (NPCS.NPC->enemy) {
        if (Any(NPCS.NPC->enemy->flags & gentityFlags_t::FL_DONT_SHOOT)) {
            NPCS.ucmd.buttons &= ~buttonFlags_t::BUTTON_ATTACK;
            NPCS.ucmd.buttons &= ~buttonFlags_t::BUTTON_ALT_ATTACK;
        }
        else if (NPCS.NPC->client->playerTeam != npcteam_t::NPCTEAM_ENEMY && NPCS.NPC->enemy->NPC &&
                 (NPCS.NPC->enemy->NPC->surrenderTime > level.time ||
                  Any(NPCS.NPC->enemy->NPC->scriptFlags & scriptFlags_t::SCF_FORCED_MARCH))) { // don't shoot someone who's surrendering if you're a good guy
            NPCS.ucmd.buttons &= ~buttonFlags_t::BUTTON_ATTACK;
            NPCS.ucmd.buttons &= ~buttonFlags_t::BUTTON_ALT_ATTACK;
        }

        if (NPCS.client->ps.weaponstate == weaponstate_t::WEAPON_IDLE) {
            NPCS.client->ps.weaponstate = weaponstate_t::WEAPON_READY;
        }
    }
    else {
        if (NPCS.client->ps.weaponstate == weaponstate_t::WEAPON_READY) {
            NPCS.client->ps.weaponstate = weaponstate_t::WEAPON_IDLE;
        }
    }

    if (!Any(NPCS.ucmd.buttons & buttonFlags_t::BUTTON_ATTACK) &&
        NPCS.NPC->attackDebounceTime > level.time) {        // We just shot but aren't still shooting, so hold the gun up for a while
        if (NPCS.client->ps.weapon == weapon_t::WP_SABER) { // One-handed
            NPC_SetAnim(NPCS.NPC, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (NPCS.client->ps.weapon == weapon_t::WP_BRYAR_PISTOL) { // Sniper pose
            NPC_SetAnim(NPCS.NPC, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        /*//FIXME: What's the proper solution here?
        else
        {//heavy weapon
            NPC_SetAnim(NPC,setAnimParts_t::SETANIM_TORSO,animNumber_t::TORSO_WEAPONREADY3,setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        */
    }
    else if (!NPCS.NPC->enemy) // HACK!
    {
        if (ES_TorsoAnim(NPCS.NPC->s) == animNumber_t::TORSO_WEAPONREADY1 ||
            ES_TorsoAnim(NPCS.NPC->s) ==
                animNumber_t::TORSO_WEAPONREADY3) { // we look ready for action, using one of the first 2 weapon, let's rest our weapon on our shoulder
            NPC_SetAnim(NPCS.NPC, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONIDLE3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
    }

    NPC_CheckAttackHold();
    NPC_ApplyScriptFlags();

    // cliff and wall avoidance
    NPC_AvoidWallsAndCliffs();

    // run the bot through the server like it was a real client
    //=== Save the ucmd for the second no-think Pmove ============================
    NPCS.ucmd.serverTime = level.time - 50;
    memcpy(&NPCS.NPCInfo->last_ucmd, &NPCS.ucmd, sizeof(usercmd_t));
    if (!NPCS.NPCInfo->attackHoldTime) {
        NPCS.NPCInfo->last_ucmd.buttons &= ~(buttonFlags_t::BUTTON_ATTACK | buttonFlags_t::BUTTON_ALT_ATTACK); // so we don't fire twice in one think
    }
    //============================================================================
    NPC_CheckAttackScript();
    NPC_KeepCurrentFacing();

    if (!NPCS.NPC->next_roff_time || NPCS.NPC->next_roff_time < level.time) { // If we were following a roff, we don't do normal pmoves.
        ClientThink(NPCS.NPC->s.number, &NPCS.ucmd);
    }
    else {
        NPC_ApplyRoff();
    }

    // end of thinking cleanup
    NPCS.NPCInfo->touchedByPlayer = nullptr;

    NPC_CheckPlayerAim();
    NPC_CheckAllClear();

    /*if( ucmd.forwardmove || ucmd.rightmove )
    {
        std::optional<animNumber_t> la, ta;

        for(animNumber_t i = animNumber_t::FACE_TALK0; i < animNumber_t::MAX_ANIMATIONS; i++)
        {
            if( NPC->client->ps.legsAnim == i )
            {
                la = i;
            }

            if( NPC->client->ps.torsoAnim == i )
            {
                ta = i;
            }

            if(la && ta)
            {
                break;
            }
        }

        if(la && ta)
        {//FIXME: should never play same frame twice or restart an anim before finishing it
            Q_Println("LegsAnim: {}({}) TorsoAnim: {}({})", BG_AnimName( *la ), NPC->renderInfo.legsFrame, BG_AnimName( *ta ),
    NPC->client->renderInfo.torsoFrame);
        }
    }*/
}

void NPC_CheckInSolid(void)
{
    trace_t trace;
    vec3_t point;
    VectorCopy(NPCS.NPC->r.currentOrigin, point);
    point[2] -= 0.25;

    trap->Trace(&trace, NPCS.NPC->r.currentOrigin, NPCS.NPC->r.mins, NPCS.NPC->r.maxs, point, NPCS.NPC->s.number, NPCS.NPC->clipmask, false,
                g2TraceFlags_t::G2TRFLAG_NONE, 0);
    if (!trace.startsolid && !trace.allsolid) {
        VectorCopy(NPCS.NPC->r.currentOrigin, NPCS.NPCInfo->lastClearOrigin);
    }
    else {
        if (VectorLengthSquared(NPCS.NPCInfo->lastClearOrigin)) {
            //			Q_Println("{} stuck in solid at {}: fixing...", NPC->script_targetname, vtos(NPC->r.currentOrigin));
            G_SetOrigin(NPCS.NPC, NPCS.NPCInfo->lastClearOrigin);
            trap->LinkEntity((sharedEntity_t*)NPCS.NPC);
        }
    }
}

void G_DroidSounds(gentity_t* self)
{
    if (self->client) { // make the noises
        if (TIMER_Done(self, "patrolNoise") && !Q_irand(0, 20)) {
            switch (self->client->NPC_class) {
            case class_t::CLASS_R2D2: // droid
                G_SoundOnEnt(self, soundChannel_t::CHAN_AUTO, Q_Format("sound/chars/r2d2/misc/r2d2talk0{}.wav", Q_irand(1, 3)).c_str());
                break;
            case class_t::CLASS_R5D2: // droid
                G_SoundOnEnt(self, soundChannel_t::CHAN_AUTO, Q_Format("sound/chars/r5d2/misc/r5talk{}.wav", Q_irand(1, 4)).c_str());
                break;
            case class_t::CLASS_PROBE: // droid
                G_SoundOnEnt(self, soundChannel_t::CHAN_AUTO, Q_Format("sound/chars/probe/misc/probetalk{}.wav", Q_irand(1, 3)).c_str());
                break;
            case class_t::CLASS_MOUSE: // droid
                G_SoundOnEnt(self, soundChannel_t::CHAN_AUTO, Q_Format("sound/chars/mouse/misc/mousego{}.wav", Q_irand(1, 3)).c_str());
                break;
            case class_t::CLASS_GONK: // droid
                G_SoundOnEnt(self, soundChannel_t::CHAN_AUTO, Q_Format("sound/chars/gonk/misc/gonktalk{}.wav", Q_irand(1, 2)).c_str());
                break;
            default:
                break;
            }
            TIMER_Set(self, "patrolNoise", Q_irand(2000, 4000));
        }
    }
}

/*
===============
NPC_Think

Main NPC AI - called once per frame
===============
*/
#if AI_TIMERS
extern int AITime;
#endif                          //	AI_TIMERS
void NPC_Think(gentity_t* self) //, int msec )
{
    vec3_t oldMoveDir;
    int i = 0;
    gentity_t* player;

    self->nextthink = level.time + FRAMETIME;

    SetNPCGlobals(self);

    memset(&NPCS.ucmd, 0, sizeof(NPCS.ucmd));

    VectorCopy(self->client->ps.moveDir, oldMoveDir);
    if (self->s.NPC_class != class_t::CLASS_VEHICLE) { // YOU ARE BREAKING MY PREDICTION. Bad clear.
        VectorClear(self->client->ps.moveDir);
    }

    if (!self || !self->NPC || !self->client) {
        return;
    }

    // dead NPCs have a special think, don't run scripts (for now)
    // FIXME: this breaks deathscripts
    if (self->health <= 0) {
        DeadThink();
        if (NPCS.NPCInfo->nextBStateThink <= level.time) {
            trap->ICARUS_MaintainTaskManager(self->s.number);
        }
        VectorCopy(self->r.currentOrigin, self->client->ps.origin);
        return;
    }

    // see if NPC ai is frozen
    if (d_npcfreeze.value || Any(NPCS.NPC->r.svFlags & svFlags_t::SVF_ICARUS_FREEZE)) {
        NPC_UpdateAngles(true, true);
        ClientThink(self->s.number, &NPCS.ucmd);
        // VectorCopy(self->s.origin, self->s.origin2 );
        VectorCopy(self->r.currentOrigin, self->client->ps.origin);
        return;
    }

    self->nextthink = level.time + FRAMETIME / 2;

    while (i < MAX_CLIENTS) {
        player = &g_entities[i];

        if (player->inuse && player->client && player->client->sess.sessionTeam != team_t::TEAM_SPECTATOR &&
            !Any(player->client->ps.pm_flags & pmoveFlags_t::PMF_FOLLOW)) {
            // if ( player->client->ps.viewEntity == self->s.number )
            if (0) // rwwFIXMEFIXME: Allow controlling ents
            {      // being controlled by player
                G_DroidSounds(self);
                // FIXME: might want to at least make sounds or something?
                // NPC_UpdateAngles(true, true);
                // Which ucmd should we send?  Does it matter, since it gets overridden anyway?
                NPCS.NPCInfo->last_ucmd.serverTime = level.time - 50;
                ClientThink(NPCS.NPC->s.number, &NPCS.ucmd);
                // VectorCopy(self->s.origin, self->s.origin2 );
                VectorCopy(self->r.currentOrigin, self->client->ps.origin);
                return;
            }
        }
        i++;
    }

    if (self->client->NPC_class == class_t::CLASS_VEHICLE) {
        if (self->client->ps.m_iVehicleNum) { // we don't think on our own
            // well, run scripts, though...
            trap->ICARUS_MaintainTaskManager(self->s.number);
            return;
        }
        else {
            VectorClear(self->client->ps.moveDir);
            self->client->pers.cmd.forwardmove = 0;
            self->client->pers.cmd.rightmove = 0;
            self->client->pers.cmd.upmove = 0;
            self->client->pers.cmd.buttons = buttonFlags_t::BUTTON_NONE;
            memcpy(&self->m_pVehicle->m_ucmd, &self->client->pers.cmd, sizeof(usercmd_t));
        }
    }
    else if (NPCS.NPC->s.m_iVehicleNum) { // droid in a vehicle?
        G_DroidSounds(self);
    }

    if (NPCS.NPCInfo->nextBStateThink <= level.time && !NPCS.NPC->s.m_iVehicleNum) // NPCs sitting in Vehicles do NOTHING
    {
#if AI_TIMERS
        int startTime = GetTime(0);
#endif                                                   //	AI_TIMERS
        if (NPCS.NPC->s.eType != entityType_t::ET_NPC) { // Something drastic happened in our script
            return;
        }

        if (NPCS.NPC->s.weapon == weapon_t::WP_SABER && g_npcspskill.integer >= 2 &&
            NPCS.NPCInfo->rank > rank_t::RANK_LT_JG) { // Jedi think faster on hard difficulty, except low-rank (reborn)
            NPCS.NPCInfo->nextBStateThink = level.time + FRAMETIME / 2;
        }
        else { // Maybe even 200 ms?
            NPCS.NPCInfo->nextBStateThink = level.time + FRAMETIME;
        }

        // nextthink is set before this so something in here can override it
        if (self->s.NPC_class != class_t::CLASS_VEHICLE || !self->m_pVehicle) { // ok, let's not do this at all for vehicles.
            NPC_ExecuteBState(self);
        }

#if AI_TIMERS
        int addTime = GetTime(startTime);
        if (addTime > 50) {
            Q_PrintlnError("NPC number {}, {} {} at {}, weaponnum: {}, using {} of AI time!!!", NPC->s.number, NPC->NPC_type, NPC->targetname,
                           vtos(NPC->r.currentOrigin), NPC->s.weapon, addTime);
        }
        AITime += addTime;
#endif //	AI_TIMERS
    }
    else {
        VectorCopy(oldMoveDir, self->client->ps.moveDir);
        // or use client->pers.lastCommand?
        NPCS.NPCInfo->last_ucmd.serverTime = level.time - 50;
        if (!NPCS.NPC->next_roff_time || NPCS.NPC->next_roff_time < level.time) { // If we were following a roff, we don't do normal pmoves.
            // FIXME: firing angles (no aim offset) or regular angles?
            NPC_UpdateAngles(true, true);
            memcpy(&NPCS.ucmd, &NPCS.NPCInfo->last_ucmd, sizeof(usercmd_t));
            ClientThink(NPCS.NPC->s.number, &NPCS.ucmd);
        }
        else {
            NPC_ApplyRoff();
        }
        // VectorCopy(self->s.origin, self->s.origin2 );
    }
    // must update icarus *every* frame because of certain animation completions in the pmove stuff that can leave a 50ms gap between ICARUS animation commands
    trap->ICARUS_MaintainTaskManager(self->s.number);
    VectorCopy(self->r.currentOrigin, self->client->ps.origin);
}

void NPC_InitAI(void)
{
    /*
    trap->Cvar_Register(&g_saberRealisticCombat, "g_saberRealisticCombat", "0", cvarFlags_t::CVAR_CHEAT);

    trap->Cvar_Register(&debugNoRoam, "d_noroam", "0", cvarFlags_t::CVAR_CHEAT);
    trap->Cvar_Register(&debugNPCAimingBeam, "d_npcaiming", "0", cvarFlags_t::CVAR_CHEAT);
    trap->Cvar_Register(&debugBreak, "d_break", "0", cvarFlags_t::CVAR_CHEAT);
    trap->Cvar_Register(&d_npcai, "d_npcai", "0", cvarFlags_t::CVAR_CHEAT);
    trap->Cvar_Register(&debugNPCFreeze, "d_npcfreeze", "0", cvarFlags_t::CVAR_CHEAT);
    trap->Cvar_Register(&d_JediAI, "d_JediAI", "0", cvarFlags_t::CVAR_CHEAT);
    trap->Cvar_Register(&d_noGroupAI, "d_noGroupAI", "0", cvarFlags_t::CVAR_CHEAT);
    trap->Cvar_Register(&d_asynchronousGroupAI, "d_asynchronousGroupAI", "0", cvarFlags_t::CVAR_CHEAT);

    //0 = never (BORING)
    //1 = kyle only
    //2 = kyle and last enemy jedi
    //3 = kyle and any enemy jedi
    //4 = kyle and last enemy in a group
    //5 = kyle and any enemy
    //6 = also when kyle takes pain or enemy jedi dodges player saber swing or does an acrobatic evasion

    trap->Cvar_Register(&d_slowmodeath, "d_slowmodeath", "0", cvarFlags_t::CVAR_CHEAT);

    trap->Cvar_Register(&d_saberCombat, "d_saberCombat", "0", cvarFlags_t::CVAR_CHEAT);

    trap->Cvar_Register(&g_npcspskill, "g_npcspskill", "0", cvarFlags_t::CVAR_ARCHIVE | cvarFlags_t::CVAR_USERINFO);
    */
}

/*
==================================
void NPC_InitAnimTable( void )

  Need to initialize this table.
  If someone tried to play an anim
  before table is filled in with
  values, causes tasks that wait for
  anim completion to never finish.
  (frameLerp of 0 * numFrames of 0 = 0)
==================================
*/
/*
void NPC_InitAnimTable( void )
{
    int i;

    for ( i = 0; i < MAX_ANIM_FILES; i++ )
    {
        for ( int j = 0; j < animNumber_t::MAX_ANIMATIONS; j++ )
        {
            level.knownAnimFileSets[i].animations[j].firstFrame = 0;
            level.knownAnimFileSets[i].animations[j].frameLerp = 100;
            level.knownAnimFileSets[i].animations[j].initialLerp = 100;
            level.knownAnimFileSets[i].animations[j].numFrames = 0;
        }
    }
}
*/

void NPC_InitGame(void)
{
    //	globals.NPCs = (gNPC_t *) trap->TagMalloc(game.maxclients * sizeof(game.bots[0]), TAG_GAME);
    //	trap->Cvar_Register(&debugNPCName, "d_npc", "0", cvarFlags_t::CVAR_CHEAT);

    NPC_LoadParms();
    NPC_InitAI();
    //	NPC_InitAnimTable();
    /*
    ResetTeamCounters();
    for ( int team = npcteam_t::NPCTEAM_FREE; team < npcteam_t::NPCTEAM_NUM_TEAMS; team++ )
    {
        teamLastEnemyTime[team] = -10000;
    }
    */
}

void NPC_SetAnim(gentity_t* ent, setAnimParts_t setAnimParts, animNumber_t anim, setAnimFlags_t setAnimFlags)
{ // FIXME : once torsoAnim and legsAnim are in the same structure for NCP and Players
    // rename PM_SETAnimFinal to PM_SetAnim and have both NCP and Players call PM_SetAnim
    G_SetAnim(ent, nullptr, setAnimParts, anim, setAnimFlags, 0);
    /*
        if(ent->client)
        {//Players, NPCs
            if (Any( setAnimFlags & setAnimFlags_t::SETANIM_FLAG_OVERRIDE ))
            {
                if (Any( setAnimParts & setAnimParts_t::SETANIM_TORSO ))
                {
                    if( Any( setAnimFlags & setAnimFlags_t::SETANIM_FLAG_RESTART ) || ent->client->ps.torsoAnim != anim )
                    {
                        PM_SetTorsoAnimTimer( ent, &ent->client->ps.torsoTimer, 0 );
                    }
                }
                if (Any( setAnimParts & setAnimParts_t::SETANIM_LEGS ))
                {
                    if( Any( setAnimFlags & setAnimFlags_t::SETANIM_FLAG_RESTART ) || ent->client->ps.legsAnim != anim )
                    {
                        PM_SetLegsAnimTimer( ent, &ent->client->ps.legsAnimTimer, 0 );
                    }
                }
            }

            PM_SetAnimFinal(&ent->client->ps.torsoAnim,&ent->client->ps.legsAnim,setAnimParts,anim,setAnimFlags,
                &ent->client->ps.torsoAnimTimer,&ent->client->ps.legsAnimTimer,ent);
        }
        else
        {//bodies, etc.
            if (Any( setAnimFlags & setAnimFlags_t::SETANIM_FLAG_OVERRIDE ))
            {
                if (Any( setAnimParts & setAnimParts_t::SETANIM_TORSO ))
                {
                    if( Any( setAnimFlags & setAnimFlags_t::SETANIM_FLAG_RESTART ) || ES_TorsoAnim( ent->s ) != anim )
                    {
                        PM_SetTorsoAnimTimer( ent, &ent->s.torsoAnimTimer, 0 );
                    }
                }
                if (Any( setAnimParts & setAnimParts_t::SETANIM_LEGS ))
                {
                    if( Any( setAnimFlags & setAnimFlags_t::SETANIM_FLAG_RESTART ) || ES_LegsAnim( ent->s ) != anim )
                    {
                        PM_SetLegsAnimTimer( ent, &ent->s.legsAnimTimer, 0 );
                    }
                }
            }

            PM_SetAnimFinal(&ent->s.torsoAnim,&ent->s.legsAnim,setAnimParts,anim,setAnimFlags,
                &ent->s.torsoAnimTimer,&ent->s.legsAnimTimer,ent);
        }
        */
}

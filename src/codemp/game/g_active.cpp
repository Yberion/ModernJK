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

#include "g_local.h"
#include "bg_saga.h"

extern void Jedi_Cloak(gentity_t* self);
extern void Jedi_Decloak(gentity_t* self);

bool PM_SaberInTransition(saberMoveName_t move);
bool PM_SaberInStart(saberMoveName_t move);
bool PM_SaberInReturn(saberMoveName_t move);
bool WP_SaberStyleValidForSaber(saberInfo_t* saber1, saberInfo_t* saber2, int saberHolstered, saber_styles_t saberAnimLevel);
bool saberCheckKnockdown_DuelLoss(gentity_t* saberent, gentity_t* saberOwner, gentity_t* other);

void P_SetTwitchInfo(gclient_t* client)
{
    client->ps.painTime = level.time;
    client->ps.painDirection ^= 1;
}

/*
===============
G_DamageFeedback

Called just before a snapshot is sent to the given player.
Totals up all damage and generates both the playerState_t
damage values to that client for pain blends and kicks, and
global pain sound events for all clients.
===============
*/
void P_DamageFeedback(gentity_t* player)
{
    gclient_t* client;
    float count;
    vec3_t angles;

    client = player->client;
    if (client->ps.pm_type == pmtype_t::PM_DEAD || client->tempSpectate >= level.time) {
        return;
    }

    // total points of damage shot at the player this frame
    count = client->damage_blood + client->damage_armor;
    if (count == 0) {
        return; // didn't take any damage
    }

    if (count > 255) {
        count = 255;
    }

    // send the information to the client

    // world damage (falling, slime, etc) uses a special code
    // to make the blend blob centered instead of positional
    if (client->damage_fromWorld) {
        client->ps.damagePitch = 255;
        client->ps.damageYaw = 255;

        client->damage_fromWorld = false;
    }
    else {
        vectoangles(client->damage_from, angles);
        client->ps.damagePitch = angles[PITCH] / 360.0 * 256;
        client->ps.damageYaw = angles[YAW] / 360.0 * 256;

        // cap them since we can't send negative values in here across the net
        if (client->ps.damagePitch < 0) {
            client->ps.damagePitch = 0;
        }
        if (client->ps.damageYaw < 0) {
            client->ps.damageYaw = 0;
        }
    }

    // play an appropriate pain sound
    if ((level.time > player->pain_debounce_time) && !Any(player->flags & gentityFlags_t::FL_GODMODE) && !Any(player->s.eFlags & entityFlags_t::EF_DEAD) &&
        (player->client->tempSpectate < level.time)) {

        // don't do more than two pain sounds a second
        // nmckenzie: also don't make him loud and whiny if he's only getting nicked.
        if (level.time - client->ps.painTime < 500 || count < 10) {
            return;
        }
        P_SetTwitchInfo(client);
        player->pain_debounce_time = level.time + 700;

        G_AddEvent(player, entity_event_t::EV_PAIN, player->health);
        client->ps.damageEvent++;

        if (client->damage_armor && !client->damage_blood) {
            client->ps.damageType = 1; // pure shields
        }
        else if (client->damage_armor) {
            client->ps.damageType = 2; // shields and health
        }
        else {
            client->ps.damageType = 0; // pure health
        }
    }

    client->ps.damageCount = count;

    //
    // clear totals
    //
    client->damage_blood = 0;
    client->damage_armor = 0;
    client->damage_knockback = 0;
}

/*
=============
P_WorldEffects

Check for lava / slime contents and drowning
=============
*/
void P_WorldEffects(gentity_t* ent)
{
    bool envirosuit = false;
    int waterlevel;

    if (ent->client->noclip) {
        ent->client->airOutTime = level.time + 12000; // don't need air
        return;
    }

    waterlevel = ent->waterlevel;

    envirosuit = ent->client->ps.powerups[powerup_t::PW_BATTLESUIT] > level.time;

    //
    // check for drowning
    //
    if (waterlevel == 3) {
        // envirosuit give air
        if (envirosuit)
            ent->client->airOutTime = level.time + 10000;

        // if out of air, start drowning
        if (ent->client->airOutTime < level.time) {
            // drown!
            ent->client->airOutTime += 1000;
            if (ent->health > 0 && ent->client->tempSpectate < level.time) {
                // take more damage the longer underwater
                ent->damage += 2;
                if (ent->damage > 15)
                    ent->damage = 15;

                // play a gurp sound instead of a normal pain sound
                if (ent->health <= ent->damage) {
                    G_Sound(ent, sound::SoundClass::Voice, sound::Slot::Voice, G_SoundIndex(/*"*drown.wav"*/ "sound/player/gurp1.wav"));
                }
                else if (Q_irand(0, 1)) {
                    G_Sound(ent, sound::SoundClass::Voice, sound::Slot::Voice, G_SoundIndex("sound/player/gurp1.wav"));
                }
                else {
                    G_Sound(ent, sound::SoundClass::Voice, sound::Slot::Voice, G_SoundIndex("sound/player/gurp2.wav"));
                }

                // don't play a normal pain sound
                ent->pain_debounce_time = level.time + 200;

                G_Damage(ent, nullptr, nullptr, nullptr, nullptr, ent->damage, damageFlags_t::DAMAGE_NO_ARMOR, meansOfDeath_t::MOD_WATER);
            }
        }
    }
    else {
        ent->client->airOutTime = level.time + 12000;
        ent->damage = 2;
    }

    //
    // check for sizzle damage (move to pmove?)
    //
    if (waterlevel && Any(ent->watertype & (contentsFlags_t::CONTENTS_LAVA | contentsFlags_t::CONTENTS_SLIME))) {
        if (ent->health > 0 && ent->client->tempSpectate < level.time && ent->pain_debounce_time <= level.time) {
            if (envirosuit)
                G_AddEvent(ent, entity_event_t::EV_POWERUP_BATTLESUIT, 0);
            else {
                if (Any(ent->watertype & contentsFlags_t::CONTENTS_LAVA))
                    G_Damage(ent, nullptr, nullptr, nullptr, nullptr, 30 * waterlevel, damageFlags_t::DAMAGE_NORMAL, meansOfDeath_t::MOD_LAVA);

                if (Any(ent->watertype & contentsFlags_t::CONTENTS_SLIME))
                    G_Damage(ent, nullptr, nullptr, nullptr, nullptr, 10 * waterlevel, damageFlags_t::DAMAGE_NORMAL, meansOfDeath_t::MOD_SLIME);
            }
        }
    }
}

//==============================================================
extern void G_ApplyKnockback(gentity_t* targ, vec3_t newDir, float knockback);
void DoImpact(gentity_t* self, gentity_t* other, bool damageSelf)
{
    float magnitude, my_mass;
    vec3_t velocity;
    contentsFlags_t cont;
    bool easyBreakBrush = true;

    if (self->client) {
        VectorCopy(self->client->ps.velocity, velocity);
        if (!self->mass) {
            my_mass = 10;
        }
        else {
            my_mass = self->mass;
        }
    }
    else {
        VectorCopy(self->s.pos.trDelta, velocity);
        if (self->s.pos.trType == trType_t::TR_GRAVITY) {
            velocity[2] -= 0.25f * g_gravity.value;
        }
        if (!self->mass) {
            my_mass = 1;
        }
        else if (self->mass <= 10) {
            my_mass = 10;
        }
        else {
            my_mass = self->mass; /// 10;
        }
    }

    magnitude = VectorLength(velocity) * my_mass / 10;

    /*
    if(pointcontents(self.absmax)==CONTENT_WATER)//FIXME: or other watertypes
        magnitude/=3;							//water absorbs 2/3 velocity

    if(self.classname=="barrel"&&self.aflag)//rolling barrels are made for impacts!
        magnitude*=3;

    if(self.frozen>0&&magnitude<300&&self.flags&FL_ONGROUND&&loser==world&&self.velocity_z<-20&&self.last_onground+0.3<time)
        magnitude=300;
    */
    if (other->material == material_t::MAT_GLASS || other->material == material_t::MAT_GLASS_METAL || other->material == material_t::MAT_GRATE1 ||
        (Any(other->flags & gentityFlags_t::FL_BBRUSH) && (other->spawnflags & 8 /*THIN*/)) || Any(other->r.svFlags & svFlags_t::SVF_GLASS_BRUSH)) {
        easyBreakBrush = true;
    }

    if (!self->client || self->client->ps.lastOnGround + 300 < level.time || (self->client->ps.lastOnGround + 100 < level.time && easyBreakBrush)) {
        vec3_t dir1, dir2;
        float force = 0, dot;

        if (easyBreakBrush)
            magnitude *= 2;

        // damage them
        if (magnitude >= 100 && other->s.number < ENTITYNUM_WORLD) {
            VectorCopy(velocity, dir1);
            VectorNormalize(dir1);
            if (VectorCompare(other->r.currentOrigin, vec3_origin)) { // a brush with no origin
                VectorCopy(dir1, dir2);
            }
            else {
                VectorSubtract(other->r.currentOrigin, self->r.currentOrigin, dir2);
                VectorNormalize(dir2);
            }

            dot = DotProduct(dir1, dir2);

            if (dot >= 0.2) {
                force = dot;
            }
            else {
                force = 0;
            }

            force *= (magnitude / 50);

            cont = trap->PointContents(other->r.absmax, other->s.number);
            if (Any(cont & contentsFlags_t::CONTENTS_WATER)) //|| (self.classname=="barrel"&&self.aflag))//FIXME: or other watertypes
            {
                force /= 3; // water absorbs 2/3 velocity
            }

            /*
            if(self.frozen>0&&force>10)
                force=10;
            */

            if ((force >= 1 && other->s.number >= MAX_CLIENTS) || force >= 10) {
                /*
                            dprint("Damage other (");
                            dprint(loser.classname);
                            dprint("): ");
                            dprint(ftos(force));
                            dprint("\n");
                */
                if (Any(other->r.svFlags & svFlags_t::SVF_GLASS_BRUSH)) {
                    other->splashRadius = (float)(self->r.maxs[0] - self->r.mins[0]) / 4.0f;
                }
                if (other->takedamage) {
                    G_Damage(other, self, self, velocity, self->r.currentOrigin, force, damageFlags_t::DAMAGE_NO_ARMOR,
                             meansOfDeath_t::MOD_CRUSH); // FIXME: MOD_IMPACT
                }
                else {
                    G_ApplyKnockback(other, dir2, force);
                }
            }
        }

        if (damageSelf && self->takedamage) {
            // Now damage me
            // FIXME: more lenient falling damage, especially for when driving a vehicle
            if (self->client && self->client->ps.fd.forceJumpZStart) {                 // we were force-jumping
                if (self->r.currentOrigin[2] >= self->client->ps.fd.forceJumpZStart) { // we landed at same height or higher than we landed
                    magnitude = 0;
                }
                else { // FIXME: take off some of it, at least?
                    magnitude = (self->client->ps.fd.forceJumpZStart - self->r.currentOrigin[2]) / 3;
                }
            }
            // if(self.classname!="monster_mezzoman"&&self.netname!="spider")//Cats always land on their feet
            if ((magnitude >= 100 + self->health && self->s.number >= MAX_CLIENTS && self->s.weapon != weapon_t::WP_SABER) ||
                (magnitude >= 700)) //&& self.safe_time < level.time ))//health here is used to simulate structural integrity
            {
                if ((self->s.weapon == weapon_t::WP_SABER) && self->client && self->client->ps.groundEntityNum < ENTITYNUM_NONE &&
                    magnitude < 1000) { // players and jedi take less impact damage
                    // allow for some lenience on high falls
                    magnitude /= 2;
                    /*
                    if ( self.absorb_time >= time )//crouching on impact absorbs 1/2 the damage
                    {
                        magnitude/=2;
                    }
                    */
                }
                magnitude /= 40;
                magnitude = magnitude - force / 2; // If damage other, subtract half of that damage off of own injury
                if (magnitude >= 1) {
                    // FIXME: Put in a thingtype impact sound function
                    /*
                                    dprint("Damage self (");
                                    dprint(self.classname);
                                    dprint("): ");
                                    dprint(ftos(magnitude));
                                    dprint("\n");
                    */
                    /*
                    if ( self.classname=="player_sheep "&& self.flags&FL_ONGROUND && self.velocity_z > -50 )
                        return;
                    */
                    G_Damage(self, nullptr, nullptr, nullptr, self->r.currentOrigin, magnitude / 2, damageFlags_t::DAMAGE_NO_ARMOR,
                             meansOfDeath_t::MOD_FALLING); // FIXME: MOD_IMPACT
                }
            }
        }

        // FIXME: slow my velocity some?

        // NOTENOTE We don't use lastimpact as of yet
        //		self->lastImpact = level.time;

        /*
        if(self.flags&FL_ONGROUND)
            self.last_onground=time;
        */
    }
}

void Client_CheckImpactBBrush(gentity_t* self, gentity_t* other)
{
    if (!other || !other->inuse) {
        return;
    }
    if (!self || !self->inuse || !self->client || self->client->tempSpectate >= level.time ||
        self->client->sess.sessionTeam == team_t::TEAM_SPECTATOR) { // hmm.. let's not let spectators ram into breakables.
        return;
    }

    /*
    if (BG_InSpecialJump(self->client->ps.legsAnim))
    { //don't do this either, qa says it creates "balance issues"
        return;
    }
    */

    if (other->material == material_t::MAT_GLASS || other->material == material_t::MAT_GLASS_METAL || other->material == material_t::MAT_GRATE1 ||
        (Any(other->flags & gentityFlags_t::FL_BBRUSH) && (other->spawnflags & 8 /*THIN*/)) ||
        (Any(other->flags & gentityFlags_t::FL_BBRUSH) && (other->health <= 10)) ||
        Any(other->r.svFlags & svFlags_t::SVF_GLASS_BRUSH)) { // clients only do impact damage against easy-break breakables
        DoImpact(self, other, false);
    }
}

/*
===============
G_SetClientSound
===============
*/
void G_SetClientSound(gentity_t* ent)
{
    if (ent->client && ent->client->isHacking) { // loop hacking sound
        ent->client->ps.loopSound = level.snd_hack;
        ent->s.loopIsSoundset = false;
    }
    else if (ent->client && ent->client->isMedHealed > level.time) { // loop healing sound
        ent->client->ps.loopSound = level.snd_medHealed;
        ent->s.loopIsSoundset = false;
    }
    else if (ent->client && ent->client->isMedSupplied > level.time) { // loop supplying sound
        ent->client->ps.loopSound = level.snd_medSupplied;
        ent->s.loopIsSoundset = false;
    }
    else if (ent->client && ent->waterlevel && Any(ent->watertype & (contentsFlags_t::CONTENTS_LAVA | contentsFlags_t::CONTENTS_SLIME))) {
        ent->client->ps.loopSound = level.snd_fry;
        ent->s.loopIsSoundset = false;
    }
    else if (ent->client) {
        ent->client->ps.loopSound = 0;
        ent->s.loopIsSoundset = false;
    }
    else {
        ent->s.loopSound = 0;
        ent->s.loopIsSoundset = false;
    }
}

//==============================================================

/*
==============
ClientImpacts
==============
*/
void ClientImpacts(gentity_t* ent, pmove_t* pmove)
{
    int i, j;
    trace_t trace;
    gentity_t* other;

    memset(&trace, 0, sizeof(trace));
    for (i = 0; i < pmove->numtouch; i++) {
        for (j = 0; j < i; j++) {
            if (pmove->touchents[j] == pmove->touchents[i]) {
                break;
            }
        }
        if (j != i) {
            continue; // duplicated
        }
        other = &g_entities[pmove->touchents[i]];

        if (Any(ent->r.svFlags & svFlags_t::SVF_BOT) && (ent->touch)) {
            ent->touch(ent, other, &trace);
        }

        if (!other->touch) {
            continue;
        }

        other->touch(other, ent, &trace);
    }
}

/*
============
G_TouchTriggers

Find all trigger entities that ent's current position touches.
Spectators will only interact with teleporters.
============
*/
void G_TouchTriggers(gentity_t* ent)
{
    int i, num;
    int touch[MAX_GENTITIES];
    gentity_t* hit;
    trace_t trace;
    vec3_t mins, maxs;
    static vec3_t range = {40, 40, 52};

    if (!ent->client) {
        return;
    }

    // dead clients don't activate triggers!
    if (ent->client->ps.stats[statIndex_t::STAT_HEALTH] <= 0) {
        return;
    }

    VectorSubtract(ent->client->ps.origin, range, mins);
    VectorAdd(ent->client->ps.origin, range, maxs);

    num = trap->EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

    // can't use ent->r.absmin, because that has a one unit pad
    VectorAdd(ent->client->ps.origin, ent->r.mins, mins);
    VectorAdd(ent->client->ps.origin, ent->r.maxs, maxs);

    for (i = 0; i < num; i++) {
        hit = &g_entities[touch[i]];

        if (!hit->touch && !ent->touch) {
            continue;
        }
        if (!Any(hit->r.contents & contentsFlags_t::CONTENTS_TRIGGER)) {
            continue;
        }

        // ignore most entities if a spectator
        if (ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR) {
            if (hit->s.eType != entityType_t::ET_TELEPORT_TRIGGER &&
                // this is ugly but adding a new ET_? type will
                // most likely cause network incompatibilities
                hit->touch != Touch_DoorTrigger) {
                continue;
            }
        }

        // use seperate code for determining if an item is picked up
        // so you don't have to actually contact its bounding box
        if (hit->s.eType == entityType_t::ET_ITEM) {
            if (!BG_PlayerTouchesItem(&ent->client->ps, &hit->s, level.time)) {
                continue;
            }
        }
        else {
            if (!trap->EntityContact(mins, maxs, (sharedEntity_t*)hit, false)) {
                continue;
            }
        }

        memset(&trace, 0, sizeof(trace));

        if (hit->touch) {
            hit->touch(hit, ent, &trace);
        }

        if (Any(ent->r.svFlags & svFlags_t::SVF_BOT) && (ent->touch)) {
            ent->touch(ent, hit, &trace);
        }
    }

    // if we didn't touch a jump pad this pmove frame
    if (ent->client->ps.jumppad_frame != ent->client->ps.pmove_framecount) {
        ent->client->ps.jumppad_frame = 0;
        ent->client->ps.jumppad_ent = 0;
    }
}

/*
============
G_MoverTouchTriggers

Find all trigger entities that ent's current position touches.
Spectators will only interact with teleporters.
============
*/
void G_MoverTouchPushTriggers(gentity_t* ent, vec3_t oldOrg)
{
    int i, num;
    float step, stepSize, dist;
    int touch[MAX_GENTITIES];
    gentity_t* hit;
    trace_t trace;
    vec3_t mins, maxs, dir, size, checkSpot;
    const vec3_t range = {40, 40, 52};

    // non-moving movers don't hit triggers!
    if (!VectorLengthSquared(ent->s.pos.trDelta)) {
        return;
    }

    VectorSubtract(ent->r.mins, ent->r.maxs, size);
    stepSize = VectorLength(size);
    if (stepSize < 1) {
        stepSize = 1;
    }

    VectorSubtract(ent->r.currentOrigin, oldOrg, dir);
    dist = VectorNormalize(dir);
    for (step = 0; step <= dist; step += stepSize) {
        VectorMA(ent->r.currentOrigin, step, dir, checkSpot);
        VectorSubtract(checkSpot, range, mins);
        VectorAdd(checkSpot, range, maxs);

        num = trap->EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);

        // can't use ent->r.absmin, because that has a one unit pad
        VectorAdd(checkSpot, ent->r.mins, mins);
        VectorAdd(checkSpot, ent->r.maxs, maxs);

        for (i = 0; i < num; i++) {
            hit = &g_entities[touch[i]];

            if (hit->s.eType != entityType_t::ET_PUSH_TRIGGER) {
                continue;
            }

            if (hit->touch == nullptr) {
                continue;
            }

            if (!Any(hit->r.contents & contentsFlags_t::CONTENTS_TRIGGER)) {
                continue;
            }

            if (!trap->EntityContact(mins, maxs, (sharedEntity_t*)hit, false)) {
                continue;
            }

            memset(&trace, 0, sizeof(trace));

            if (hit->touch != nullptr) {
                hit->touch(hit, ent, &trace);
            }
        }
    }
}

static void SV_PMTrace(trace_t* results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum,
                       contentsFlags_t contentMask)
{
    trap->Trace(results, start, mins, maxs, end, passEntityNum, contentMask, false, g2TraceFlags_t::G2TRFLAG_NONE, 10);
}

/*
=================
SpectatorThink
=================
*/
void SpectatorThink(gentity_t* ent, usercmd_t* ucmd)
{
    pmove_t pmove;
    gclient_t* client;

    client = ent->client;

    if (client->sess.spectatorState != spectatorState_t::SPECTATOR_FOLLOW) {
        client->ps.pm_type = pmtype_t::PM_SPECTATOR;
        client->ps.speed = 400; // faster than normal
        client->ps.basespeed = 400;

        // hmm, shouldn't have an anim if you're a spectator, make sure
        // it gets cleared.
        client->ps.legsAnim = animNumber_t::FACE_TALK0;
        client->ps.legsTimer = 0;
        client->ps.torsoAnim = animNumber_t::FACE_TALK0;
        client->ps.torsoTimer = 0;

        // set up for pmove
        memset(&pmove, 0, sizeof(pmove));
        pmove.ps = &client->ps;
        pmove.cmd = *ucmd;
        pmove.tracemask = MASK_PLAYERSOLID & ~contentsFlags_t::CONTENTS_BODY; // spectators can fly through bodies
        pmove.trace = SV_PMTrace;
        pmove.pointcontents = trap->PointContents;

        pmove.noSpecMove = g_noSpecMove.integer;

        pmove.animations = nullptr;
        pmove.nonHumanoid = false;

        // Set up bg entity data
        pmove.baseEnt = (bgEntity_t*)g_entities;
        pmove.entSize = sizeof(gentity_t);

        // perform a pmove
        Pmove(&pmove);
        // save results of pmove
        VectorCopy(client->ps.origin, ent->s.origin);

        if (ent->client->tempSpectate < level.time) {
            G_TouchTriggers(ent);
        }
        trap->UnlinkEntity((sharedEntity_t*)ent);
    }

    client->oldbuttons = client->buttons;
    client->buttons = ucmd->buttons;

    if (client->tempSpectate < level.time) {
        // attack button cycles through spectators
        if (Any(client->buttons & buttonFlags_t::BUTTON_ATTACK) && !Any(client->oldbuttons & buttonFlags_t::BUTTON_ATTACK))
            Cmd_FollowCycle_f(ent, 1);

        else if (client->sess.spectatorState == spectatorState_t::SPECTATOR_FOLLOW && Any(client->buttons & buttonFlags_t::BUTTON_ALT_ATTACK) &&
                 !Any(client->oldbuttons & buttonFlags_t::BUTTON_ALT_ATTACK))
            Cmd_FollowCycle_f(ent, -1);

        if (client->sess.spectatorState == spectatorState_t::SPECTATOR_FOLLOW && (ucmd->upmove > 0)) { // jump now removes you from follow mode
            StopFollowing(ent);
        }
    }
}

/*
=================
ClientInactivityTimer

Returns false if the client is dropped
=================
*/
bool ClientInactivityTimer(gclient_t* client)
{
    if (!g_inactivity.integer) {
        // give everyone some time, so if the operator sets g_inactivity during
        // gameplay, everyone isn't kicked
        client->inactivityTime = level.time + 60 * 1000;
        client->inactivityWarning = false;
    }
    else if (client->pers.cmd.forwardmove || client->pers.cmd.rightmove || client->pers.cmd.upmove ||
             Any(client->pers.cmd.buttons & (buttonFlags_t::BUTTON_ATTACK | buttonFlags_t::BUTTON_ALT_ATTACK))) {
        client->inactivityTime = level.time + g_inactivity.integer * 1000;
        client->inactivityWarning = false;
    }
    else if (!client->pers.localClient) {
        if (level.time > client->inactivityTime) {
            trap->DropClient(client - level.clients, "Dropped due to inactivity");
            return false;
        }
        if (level.time > client->inactivityTime - 10000 && !client->inactivityWarning) {
            client->inactivityWarning = true;
            trap->SendServerCommand(client - level.clients, "cp \"Ten seconds until inactivity drop!\n\"");
        }
    }
    return true;
}

/*
==================
ClientTimerActions

Actions that happen once a second
==================
*/
void ClientTimerActions(gentity_t* ent, int msec)
{
    gclient_t* client;

    client = ent->client;
    client->timeResidual += msec;

    while (client->timeResidual >= 1000) {
        client->timeResidual -= 1000;

        // count down health when over max
        if (ent->health > client->ps.stats[statIndex_t::STAT_MAX_HEALTH]) {
            ent->health--;
        }

        // count down armor when over max
        if (client->ps.stats[statIndex_t::STAT_ARMOR] > client->ps.stats[statIndex_t::STAT_MAX_HEALTH]) {
            client->ps.stats[statIndex_t::STAT_ARMOR]--;
        }
    }
}

/*
====================
ClientIntermissionThink
====================
*/
void ClientIntermissionThink(gclient_t* client)
{
    client->ps.eFlags &= ~entityFlags_t::EF_TALK;
    client->ps.eFlags &= ~entityFlags_t::EF_FIRING;

    // the level will exit when everyone wants to or after timeouts

    // swap and latch button actions
    client->oldbuttons = client->buttons;
    client->buttons = client->pers.cmd.buttons;
    if (Any(client->buttons & (buttonFlags_t::BUTTON_ATTACK | buttonFlags_t::BUTTON_USE_HOLDABLE) & (client->oldbuttons ^ client->buttons))) {
        // this used to be an ^1 but once a player says ready, it should stick
        client->readyToExit = true;
    }
}

extern void NPC_SetAnim(gentity_t* ent, setAnimParts_t setAnimParts, animNumber_t anim, setAnimFlags_t setAnimFlags);
void G_VehicleAttachDroidUnit(gentity_t* vehEnt)
{
    if (vehEnt && vehEnt->m_pVehicle && vehEnt->m_pVehicle->m_pDroidUnit != nullptr) {
        gentity_t* droidEnt = (gentity_t*)vehEnt->m_pVehicle->m_pDroidUnit;
        mdxaBone_t boltMatrix;
        vec3_t fwd;

        trap->G2API_GetBoltMatrix(vehEnt->ghoul2, 0, vehEnt->m_pVehicle->m_iDroidUnitTag, &boltMatrix, vehEnt->r.currentAngles, vehEnt->r.currentOrigin,
                                  level.time, nullptr, vehEnt->modelScale);
        BG_GiveMeVectorFromMatrix(&boltMatrix, orientations_t::ORIGIN, droidEnt->r.currentOrigin);
        BG_GiveMeVectorFromMatrix(&boltMatrix, orientations_t::NEGATIVE_Y, fwd);
        vectoangles(fwd, droidEnt->r.currentAngles);

        if (droidEnt->client) {
            VectorCopy(droidEnt->r.currentAngles, droidEnt->client->ps.viewangles);
            VectorCopy(droidEnt->r.currentOrigin, droidEnt->client->ps.origin);
        }

        G_SetOrigin(droidEnt, droidEnt->r.currentOrigin);
        trap->LinkEntity((sharedEntity_t*)droidEnt);

        if (droidEnt->NPC) {
            NPC_SetAnim(droidEnt, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_STAND2,
                        setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
        }
    }
}

// called gameside only from pmove code (convenience)
extern bool BG_SabersOff(playerState_t* ps);
void G_CheapWeaponFire(int entNum, entity_event_t ev)
{
    gentity_t* ent = &g_entities[entNum];

    if (!ent->inuse || !ent->client) {
        return;
    }

    switch (ev) {
    case entity_event_t::EV_FIRE_WEAPON:
        if (ent->m_pVehicle && ent->m_pVehicle->m_pVehicleInfo->type == vehicleType_t::VH_SPEEDER && ent->client &&
            ent->client->ps.m_iVehicleNum) { // a speeder with a pilot
            gentity_t* rider = &g_entities[ent->client->ps.m_iVehicleNum - 1];
            if (rider->inuse && rider->client) { // pilot is valid...
                if (rider->client->ps.weapon != weapon_t::WP_MELEE &&
                    (rider->client->ps.weapon != weapon_t::WP_SABER ||
                     !BG_SabersOff(&rider->client->ps))) { // can only attack on speeder when using melee or when saber is holstered
                    break;
                }
            }
        }

        FireWeapon(ent, false);
        ent->client->dangerTime = level.time;
        ent->client->ps.eFlags &= ~entityFlags_t::EF_INVULNERABLE;
        ent->client->invulnerableTimer = 0;
        break;
    case entity_event_t::EV_ALT_FIRE:
        FireWeapon(ent, true);
        ent->client->dangerTime = level.time;
        ent->client->ps.eFlags &= ~entityFlags_t::EF_INVULNERABLE;
        ent->client->invulnerableTimer = 0;
        break;
    default:
        break;
    }
}

/*
================
ClientEvents

Events will be passed on to the clients for presentation,
but any server game effects are handled here
================
*/
bool BG_InKnockDownOnly(animNumber_t anim);

void ClientEvents(gentity_t* ent, int oldEventSequence)
{
    int i; //, j;
    entity_event_t event;
    gclient_t* client;
    int damage;
    vec3_t dir;
    //	vec3_t	origin, angles;
    //	bool	fired;
    //	gitem_t *item;
    //	gentity_t *drop;

    client = ent->client;

    if (oldEventSequence < client->ps.eventSequence - MAX_PS_EVENTS) {
        oldEventSequence = client->ps.eventSequence - MAX_PS_EVENTS;
    }
    for (i = oldEventSequence; i < client->ps.eventSequence; i++) {
        event = client->ps.events[i & (MAX_PS_EVENTS - 1)];

        switch (event) {
        case entity_event_t::EV_FALL:
        case entity_event_t::EV_ROLL: {
            int delta = client->ps.eventParms[i & (MAX_PS_EVENTS - 1)];
            bool knockDownage = false;

            if (ent->client && ent->client->ps.fallingToDeath) {
                break;
            }

            if (ent->s.eType != entityType_t::ET_PLAYER) {
                break; // not in the player model
            }

            if (Any(G_DmFlags() & dmFlags_t::DF_NO_FALLING)) {
                break;
            }

            if (BG_InKnockDownOnly(ent->client->ps.legsAnim)) {
                if (delta <= 14) {
                    break;
                }
                knockDownage = true;
            }
            else {
                if (delta <= 44) {
                    break;
                }
            }

            if (knockDownage) {
                damage = delta * 1; // you suffer for falling unprepared. A lot. Makes throws and things useful, and more realistic I suppose.
            }
            else {
                if (level.gametype == gametype_t::GT_SIEGE && delta > 60) { // longer falls hurt more
                    damage = delta * 1;                                     // good enough for now, I guess
                }
                else {
                    damage = delta * 0.16; // good enough for now, I guess
                }
            }

            VectorSet(dir, 0, 0, 1);
            ent->pain_debounce_time = level.time + 200; // no normal pain sound
            G_Damage(ent, nullptr, nullptr, nullptr, nullptr, damage, damageFlags_t::DAMAGE_NO_ARMOR, meansOfDeath_t::MOD_FALLING);

            if (ent->health < 1) {
                G_Sound(ent, sound::SoundClass::Body, sound::Slot::None, G_SoundIndex("sound/player/fallsplat.wav"));
            }
        } break;
        case entity_event_t::EV_FIRE_WEAPON:
            FireWeapon(ent, false);
            ent->client->dangerTime = level.time;
            ent->client->ps.eFlags &= ~entityFlags_t::EF_INVULNERABLE;
            ent->client->invulnerableTimer = 0;
            break;

        case entity_event_t::EV_ALT_FIRE:
            FireWeapon(ent, true);
            ent->client->dangerTime = level.time;
            ent->client->ps.eFlags &= ~entityFlags_t::EF_INVULNERABLE;
            ent->client->invulnerableTimer = 0;
            break;

        case entity_event_t::EV_SABER_ATTACK:
            ent->client->dangerTime = level.time;
            ent->client->ps.eFlags &= ~entityFlags_t::EF_INVULNERABLE;
            ent->client->invulnerableTimer = 0;
            break;

        // rww - Note that these must be in the same order (ITEM#-wise) as they are in holdable_t
        case entity_event_t::EV_USE_ITEM1: // seeker droid
            ItemUse_Seeker(ent);
            break;
        case entity_event_t::EV_USE_ITEM2: // shield
            ItemUse_Shield(ent);
            break;
        case entity_event_t::EV_USE_ITEM3: // medpack
            ItemUse_MedPack(ent);
            break;
        case entity_event_t::EV_USE_ITEM4: // big medpack
            ItemUse_MedPack_Big(ent);
            break;
        case entity_event_t::EV_USE_ITEM5: // binoculars
            ItemUse_Binoculars(ent);
            break;
        case entity_event_t::EV_USE_ITEM6: // sentry gun
            ItemUse_Sentry(ent);
            break;
        case entity_event_t::EV_USE_ITEM7: // jetpack
            ItemUse_Jetpack(ent);
            break;
        case entity_event_t::EV_USE_ITEM8: // health disp
            // ItemUse_UseDisp(ent, holdable_t::HI_HEALTHDISP);
            break;
        case entity_event_t::EV_USE_ITEM9: // ammo disp
            // ItemUse_UseDisp(ent, holdable_t::HI_AMMODISP);
            break;
        case entity_event_t::EV_USE_ITEM10: // eweb
            ItemUse_UseEWeb(ent);
            break;
        case entity_event_t::EV_USE_ITEM11: // cloak
            ItemUse_UseCloak(ent);
            break;
        default:
            break;
        }
    }
}

/*
==============
SendPendingPredictableEvents
==============
*/
void SendPendingPredictableEvents(playerState_t* ps)
{
    gentity_t* t;
    int event, seq;
    int extEvent, number;

    // if there are still events pending
    if (ps->entityEventSequence < ps->eventSequence) {
        // create a temporary entity for this event which is sent to everyone
        // except the client who generated the event
        seq = ps->entityEventSequence & (MAX_PS_EVENTS - 1);
        event = BG_PackEvent(ps->events[seq], (ps->entityEventSequence & 3) << 8);
        // set external event to zero before calling BG_PlayerStateToEntityState
        extEvent = ps->externalEvent;
        ps->externalEvent = 0;
        // create temporary entity for event
        t = G_TempEntity(ps->origin, ps->events[seq]);
        if (ps->events[seq] > entity_event_t::EV_NONE && ps->events[seq] < entity_event_t::EV_NUM_ENTITY_EVENTS) {
            g_stats.events[ps->events[seq]]--; // a copy of a Pmove event, whose number depends on the commands per frame: not counted
        }
        number = t->s.number;
        BG_PlayerStateToEntityState(ps, &t->s, true);
        t->s.number = number;
        t->s.eType = BG_EventEntityType(event);
        t->s.eFlags |= entityFlags_t::EF_PLAYER_EVENT;
        t->s.otherEntityNum = ps->clientNum;
        // send to everyone except the client who generated the event
        t->r.svFlags |= svFlags_t::SVF_NOTSINGLECLIENT;
        t->r.singleClient = ps->clientNum;
        // set back external event
        ps->externalEvent = extEvent;
    }
}

static const float maxJediMasterDistance = 2500.0f * 2500.0f; // x^2, optimisation
static const float maxJediMasterFOV = 100.0f;
static const float maxForceSightDistance = Square(1500.0f) * 1500.0f; // x^2, optimisation
static const float maxForceSightFOV = 100.0f;

void G_UpdateClientBroadcasts(gentity_t* self)
{
    int i;
    gentity_t* other;

    // we are always sent to ourselves
    // we are always sent to other clients if we are in their PVS
    // if we are not in their PVS, we must set the broadcastClients bit field
    // if we do not wish to be sent to any particular entity, we must set the broadcastClients bit field and the
    //	SVF_BROADCASTCLIENTS bit flag
    self->r.broadcastClients[0] = 0u;
    self->r.broadcastClients[1] = 0u;

    for (i = 0, other = g_entities; i < MAX_CLIENTS; i++, other++) {
        bool send = false;
        float dist;
        vec3_t angles;

        if (!other->inuse || other->client->pers.connected != clientConnected_t::CON_CONNECTED) {
            // no need to compute visibility for non-connected clients
            continue;
        }

        if (other == self) {
            // we are always sent to ourselves anyway, this is purely an optimisation
            continue;
        }

        if (!(level.gametype == gametype_t::GT_JEDIMASTER && self->client->ps.isJediMaster) &&
            !(other->client->ps.fd.forcePowersActive & Bit(forcePowers_t::FP_SEE))) {
            // neither test below applies: the distance and the angles (two arc tangents per pair of clients, at every
            // command) would serve nothing
            continue;
        }

        VectorSubtract(self->client->ps.origin, other->client->ps.origin, angles);
        dist = VectorLengthSquared(angles);
        vectoangles(angles, angles);

        // broadcast jedi master to everyone if we are in distance/field of view
        if (level.gametype == gametype_t::GT_JEDIMASTER && self->client->ps.isJediMaster) {
            if (dist < maxJediMasterDistance && InFieldOfVision(other->client->ps.viewangles, maxJediMasterFOV, angles)) {
                send = true;
            }
        }

        // broadcast this client to everyone using force sight if we are in distance/field of view
        if ((other->client->ps.fd.forcePowersActive & Bit(forcePowers_t::FP_SEE))) {
            if (dist < maxForceSightDistance && InFieldOfVision(other->client->ps.viewangles, maxForceSightFOV, angles)) {
                send = true;
            }
        }

        if (send) {
            Q_AddToBitflags(self->r.broadcastClients, i, 32);
        }
    }

    trap->LinkEntity((sharedEntity_t*)self);
}

void G_AddPushVecToUcmd(gentity_t* self, usercmd_t* ucmd)
{
    vec3_t forward, right, moveDir;
    float pushSpeed, fMove, rMove;

    if (!self->client) {
        return;
    }
    pushSpeed = VectorLengthSquared(self->client->pushVec);
    if (!pushSpeed) { // not being pushed
        return;
    }

    AngleVectors(self->client->ps.viewangles, forward, right, nullptr);
    VectorScale(forward, ucmd->forwardmove / 127.0f * self->client->ps.speed, moveDir);
    VectorMA(moveDir, ucmd->rightmove / 127.0f * self->client->ps.speed, right, moveDir);
    // moveDir is now our intended move velocity

    VectorAdd(moveDir, self->client->pushVec, moveDir);
    self->client->ps.speed = VectorNormalize(moveDir);
    // moveDir is now our intended move velocity plus our push Vector

    fMove = 127.0 * DotProduct(forward, moveDir);
    rMove = 127.0 * DotProduct(right, moveDir);
    ucmd->forwardmove = Q_floor(fMove); // If in the same dir , will be positive
    ucmd->rightmove = Q_floor(rMove);   // If in the same dir , will be positive

    if (self->client->pushVecTime < level.time) {
        VectorClear(self->client->pushVec);
    }
}

bool G_StandingAnim(animNumber_t anim)
{ // NOTE: does not check idles or special (cinematic) stands
    switch (anim) {
    case animNumber_t::BOTH_STAND1:
    case animNumber_t::BOTH_STAND2:
    case animNumber_t::BOTH_STAND3:
    case animNumber_t::BOTH_STAND4:
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool G_ActionButtonPressed(buttonFlags_t buttons)
{
    if (Any(buttons & buttonFlags_t::BUTTON_ATTACK)) {
        return true;
    }
    else if (Any(buttons & buttonFlags_t::BUTTON_USE_HOLDABLE)) {
        return true;
    }
    else if (Any(buttons & buttonFlags_t::BUTTON_GESTURE)) {
        return true;
    }
    else if (Any(buttons & buttonFlags_t::BUTTON_USE)) {
        return true;
    }
    else if (Any(buttons & buttonFlags_t::BUTTON_FORCEGRIP)) {
        return true;
    }
    else if (Any(buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {
        return true;
    }
    else if (Any(buttons & buttonFlags_t::BUTTON_FORCEPOWER)) {
        return true;
    }
    else if (Any(buttons & buttonFlags_t::BUTTON_FORCE_LIGHTNING)) {
        return true;
    }
    else if (Any(buttons & buttonFlags_t::BUTTON_FORCE_DRAIN)) {
        return true;
    }

    return false;
}

void G_CheckClientIdle(gentity_t* ent, usercmd_t* ucmd)
{
    vec3_t viewChange;
    bool actionPressed;
    buttonFlags_t buttons;

    if (!ent || !ent->client || ent->health <= 0 || ent->client->ps.stats[statIndex_t::STAT_HEALTH] <= 0 ||
        ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR || Any(ent->client->ps.pm_flags & pmoveFlags_t::PMF_FOLLOW)) {
        return;
    }

    buttons = ucmd->buttons;

    if (Any(ent->r.svFlags & svFlags_t::SVF_BOT)) { // they press use all the time..
        buttons &= ~buttonFlags_t::BUTTON_USE;
    }
    actionPressed = G_ActionButtonPressed(buttons);

    VectorSubtract(ent->client->ps.viewangles, ent->client->idleViewAngles, viewChange);
    if (!VectorCompare(vec3_origin, ent->client->ps.velocity) || actionPressed || ucmd->forwardmove || ucmd->rightmove || ucmd->upmove ||
        !G_StandingAnim(ent->client->ps.legsAnim) || (ent->health + ent->client->ps.stats[statIndex_t::STAT_ARMOR]) != ent->client->idleHealth ||
        VectorLength(viewChange) > 10 || ent->client->ps.legsTimer > 0 || ent->client->ps.torsoTimer > 0 || ent->client->ps.weaponTime > 0 ||
        ent->client->ps.weaponstate == weaponstate_t::WEAPON_CHARGING || ent->client->ps.weaponstate == weaponstate_t::WEAPON_CHARGING_ALT ||
        ent->client->ps.zoomMode || (ent->client->ps.weaponstate != weaponstate_t::WEAPON_READY && ent->client->ps.weapon != weapon_t::WP_SABER) ||
        ent->client->ps.forceHandExtend != forceHandAnims_t::HANDEXTEND_NONE || ent->client->ps.saberBlocked != saberBlockedType_t::BLOCKED_NONE ||
        ent->client->ps.saberBlockTime >= level.time || ent->client->ps.weapon == weapon_t::WP_MELEE ||
        (ent->client->ps.weapon != UC_Weapon(ent->client->pers.cmd) && ent->s.eType != entityType_t::ET_NPC)) { // FIXME: also check for turning?
        bool brokeOut = false;

        if (!VectorCompare(vec3_origin, ent->client->ps.velocity) || actionPressed || ucmd->forwardmove || ucmd->rightmove || ucmd->upmove ||
            (ent->health + ent->client->ps.stats[statIndex_t::STAT_ARMOR]) != ent->client->idleHealth || ent->client->ps.zoomMode ||
            (ent->client->ps.weaponstate != weaponstate_t::WEAPON_READY && ent->client->ps.weapon != weapon_t::WP_SABER) ||
            (ent->client->ps.weaponTime > 0 && ent->client->ps.weapon == weapon_t::WP_SABER) || ent->client->ps.weaponstate == weaponstate_t::WEAPON_CHARGING ||
            ent->client->ps.weaponstate == weaponstate_t::WEAPON_CHARGING_ALT || ent->client->ps.forceHandExtend != forceHandAnims_t::HANDEXTEND_NONE ||
            ent->client->ps.saberBlocked != saberBlockedType_t::BLOCKED_NONE || ent->client->ps.saberBlockTime >= level.time ||
            ent->client->ps.weapon == weapon_t::WP_MELEE ||
            (ent->client->ps.weapon != UC_Weapon(ent->client->pers.cmd) && ent->s.eType != entityType_t::ET_NPC)) {
            // if in an idle, break out
            switch (ent->client->ps.legsAnim) {
            case animNumber_t::BOTH_STAND1IDLE1:
            case animNumber_t::BOTH_STAND2IDLE1:
            case animNumber_t::BOTH_STAND2IDLE2:
            case animNumber_t::BOTH_STAND3IDLE1:
            case animNumber_t::BOTH_STAND5IDLE1:
                ent->client->ps.legsTimer = 0;
                brokeOut = true;
                break;
            default:
                break;
            }
            switch (ent->client->ps.torsoAnim) {
            case animNumber_t::BOTH_STAND1IDLE1:
            case animNumber_t::BOTH_STAND2IDLE1:
            case animNumber_t::BOTH_STAND2IDLE2:
            case animNumber_t::BOTH_STAND3IDLE1:
            case animNumber_t::BOTH_STAND5IDLE1:
                ent->client->ps.torsoTimer = 0;
                ent->client->ps.weaponTime = 0;
                ent->client->ps.saberMove = saberMoveName_t::LS_READY;
                brokeOut = true;
                break;
            default:
                break;
            }
        }
        //
        ent->client->idleHealth = (ent->health + ent->client->ps.stats[statIndex_t::STAT_ARMOR]);
        VectorCopy(ent->client->ps.viewangles, ent->client->idleViewAngles);
        if (ent->client->idleTime < level.time) {
            ent->client->idleTime = level.time;
        }

        if (brokeOut && (ent->client->ps.weaponstate == weaponstate_t::WEAPON_CHARGING || ent->client->ps.weaponstate == weaponstate_t::WEAPON_CHARGING_ALT)) {
            ent->client->ps.torsoAnim = animNumber_t::TORSO_RAISEWEAP1;
        }
    }
    else if (level.time - ent->client->idleTime > 5000) { // been idle for 5 seconds
        std::optional<animNumber_t> idleAnim;
        switch (ent->client->ps.legsAnim) {
        case animNumber_t::BOTH_STAND1:
            idleAnim = animNumber_t::BOTH_STAND1IDLE1;
            break;
        case animNumber_t::BOTH_STAND2:
            idleAnim = animNumber_t::BOTH_STAND2IDLE1; // Q_irand(animNumber_t::BOTH_STAND2IDLE1,animNumber_t::BOTH_STAND2IDLE2);
            break;
        case animNumber_t::BOTH_STAND3:
            idleAnim = animNumber_t::BOTH_STAND3IDLE1;
            break;
        case animNumber_t::BOTH_STAND5:
            idleAnim = animNumber_t::BOTH_STAND5IDLE1;
            break;
        default:
            break;
        }

        if (idleAnim == animNumber_t::BOTH_STAND2IDLE1 && Q_irand(1, 10) <= 5) {
            idleAnim = animNumber_t::BOTH_STAND2IDLE2;
        }

        if (/*PM_HasAnimation( ent, *idleAnim )*/ idleAnim && *idleAnim > animNumber_t::FACE_TALK0 && *idleAnim < animNumber_t::MAX_ANIMATIONS) {
            G_SetAnim(ent, ucmd, setAnimParts_t::SETANIM_BOTH, *idleAnim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD, 0);

            // don't idle again after this anim for a while
            // ent->client->idleTime = level.time + PM_AnimLength( ent->client->clientInfo.animFileIndex, *idleAnim ) + Q_irand( 0, 2000 );
            ent->client->idleTime = level.time + ent->client->ps.legsTimer + Q_irand(0, 2000);
        }
    }
}

void NPC_Accelerate(gentity_t* ent, bool fullWalkAcc, bool fullRunAcc)
{
    if (!ent->client || !ent->NPC) {
        return;
    }

    if (!ent->NPC->stats.acceleration) { // No acceleration means just start and stop
        ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
    }
    // FIXME:  in cinematics always accel/decel?
    else if (ent->NPC->desiredSpeed <= ent->NPC->stats.walkSpeed) { // Only accelerate if at walkSpeeds
        if (ent->NPC->desiredSpeed > ent->NPC->currentSpeed + ent->NPC->stats.acceleration) {
            // ent->client->ps.friction = 0;
            ent->NPC->currentSpeed += ent->NPC->stats.acceleration;
        }
        else if (ent->NPC->desiredSpeed > ent->NPC->currentSpeed) {
            // ent->client->ps.friction = 0;
            ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
        }
        else if (fullWalkAcc && ent->NPC->desiredSpeed < ent->NPC->currentSpeed - ent->NPC->stats.acceleration) { // decelerate even when walking
            ent->NPC->currentSpeed -= ent->NPC->stats.acceleration;
        }
        else if (ent->NPC->desiredSpeed < ent->NPC->currentSpeed) { // stop on a dime
            ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
        }
    }
    else //  if ( ent->NPC->desiredSpeed > ent->NPC->stats.walkSpeed )
    {    // Only decelerate if at runSpeeds
        if (fullRunAcc && ent->NPC->desiredSpeed > ent->NPC->currentSpeed + ent->NPC->stats.acceleration) { // Accelerate to runspeed
            // ent->client->ps.friction = 0;
            ent->NPC->currentSpeed += ent->NPC->stats.acceleration;
        }
        else if (ent->NPC->desiredSpeed > ent->NPC->currentSpeed) { // accelerate instantly
            // ent->client->ps.friction = 0;
            ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
        }
        else if (fullRunAcc && ent->NPC->desiredSpeed < ent->NPC->currentSpeed - ent->NPC->stats.acceleration) {
            ent->NPC->currentSpeed -= ent->NPC->stats.acceleration;
        }
        else if (ent->NPC->desiredSpeed < ent->NPC->currentSpeed) {
            ent->NPC->currentSpeed = ent->NPC->desiredSpeed;
        }
    }
}

/*
-------------------------
NPC_GetWalkSpeed
-------------------------
*/

static int NPC_GetWalkSpeed(gentity_t* ent)
{
    int walkSpeed = 0;

    if ((ent->client == nullptr) || (ent->NPC == nullptr))
        return 0;

    switch (ent->client->playerTeam) {
    case npcteam_t::NPCTEAM_PLAYER: // To shutup compiler, will add entries later (this is stub code)
    default:
        walkSpeed = ent->NPC->stats.walkSpeed;
        break;
    }

    return walkSpeed;
}

/*
-------------------------
NPC_GetRunSpeed
-------------------------
*/
static int NPC_GetRunSpeed(gentity_t* ent)
{
    int runSpeed = 0;

    if ((ent->client == nullptr) || (ent->NPC == nullptr))
        return 0;

    // team no longer indicates species/race.  Use NPC_class to adjust speed for specific npc types
    switch (ent->client->NPC_class) {
    case class_t::CLASS_PROBE: // droid cases here to shut-up compiler
    case class_t::CLASS_GONK:
    case class_t::CLASS_R2D2:
    case class_t::CLASS_R5D2:
    case class_t::CLASS_MARK1:
    case class_t::CLASS_MARK2:
    case class_t::CLASS_PROTOCOL:
    case class_t::CLASS_ATST: // hmm, not really your average droid
    case class_t::CLASS_MOUSE:
    case class_t::CLASS_SEEKER:
    case class_t::CLASS_REMOTE:
        runSpeed = ent->NPC->stats.runSpeed;
        break;

    default:
        runSpeed = ent->NPC->stats.runSpeed * 1.3f; // rww - seems to slow in MP for some reason.
        break;
    }

    return runSpeed;
}

// Seems like a slightly less than ideal method for this, could it be done on the client?
extern bool FlyingCreature(gentity_t* ent);
void G_CheckMovingLoopingSounds(gentity_t* ent, usercmd_t* ucmd)
{
    if (ent->client) {
        if ((ent->NPC && !VectorCompare(vec3_origin, ent->client->ps.moveDir))                                    // moving using moveDir
            || ucmd->forwardmove || ucmd->rightmove                                                               // moving using ucmds
            || (ucmd->upmove && FlyingCreature(ent))                                                              // flier using ucmds to move
            || (FlyingCreature(ent) && !VectorCompare(vec3_origin, ent->client->ps.velocity) && ent->health > 0)) // flier using velocity to move
        {
            switch (ent->client->NPC_class) {
            case class_t::CLASS_R2D2:
                ent->s.loopSound = G_SoundIndex("sound/chars/r2d2/misc/r2_move_lp.wav");
                break;
            case class_t::CLASS_R5D2:
                ent->s.loopSound = G_SoundIndex("sound/chars/r2d2/misc/r2_move_lp2.wav");
                break;
            case class_t::CLASS_MARK2:
                ent->s.loopSound = G_SoundIndex("sound/chars/mark2/misc/mark2_move_lp");
                break;
            case class_t::CLASS_MOUSE:
                ent->s.loopSound = G_SoundIndex("sound/chars/mouse/misc/mouse_lp");
                break;
            case class_t::CLASS_PROBE:
                ent->s.loopSound = G_SoundIndex("sound/chars/probe/misc/probedroidloop");
            default:
                break;
            }
        }
        else { // not moving under your own control, stop loopSound
            if (ent->client->NPC_class == class_t::CLASS_R2D2 || ent->client->NPC_class == class_t::CLASS_R5D2 ||
                ent->client->NPC_class == class_t::CLASS_MARK2 || ent->client->NPC_class == class_t::CLASS_MOUSE ||
                ent->client->NPC_class == class_t::CLASS_PROBE) {
                ent->s.loopSound = 0;
            }
        }
    }
}

void G_HeldByMonster(gentity_t* ent, usercmd_t* ucmd)
{
    if (ent && ent->client && ent->client->ps.hasLookTarget) // NOTE: lookTarget is an entity number, so this presumes that client 0 is NOT a Rancor...
    {
        gentity_t* monster = &g_entities[ent->client->ps.lookTarget];
        if (monster && monster->client) {
            // take the monster's waypoint as your own
            ent->waypoint = monster->waypoint;
            if (monster->s.NPC_class == class_t::CLASS_RANCOR) { // only possibility right now, may add Wampa and Sand Creature later
                BG_AttachToRancor(monster->ghoul2,               // ghoul2 info
                                  monster->r.currentAngles[YAW], monster->r.currentOrigin, level.time, nullptr, monster->modelScale,
                                  Any(monster->client->ps.eFlags2 & entityFlags2_t::EF2_GENERIC_NPC_FLAG), ent->client->ps.origin, ent->client->ps.viewangles,
                                  nullptr);
            }
            VectorClear(ent->client->ps.velocity);
            G_SetOrigin(ent, ent->client->ps.origin);
            SetClientViewAngle(ent, ent->client->ps.viewangles);
            G_SetAngles(ent, ent->client->ps.viewangles);
            trap->LinkEntity((sharedEntity_t*)ent); // redundant?
        }
    }
    // don't allow movement, weapon switching, and most kinds of button presses
    ucmd->forwardmove = 0;
    ucmd->rightmove = 0;
    ucmd->upmove = 0;
}

enum class tauntTypes_t { TAUNT_TAUNT = 0, TAUNT_BOW, TAUNT_MEDITATE, TAUNT_FLOURISH, TAUNT_GLOAT };

void G_SetTauntAnim(gentity_t* ent, tauntTypes_t taunt)
{
    if (ent->client->pers.cmd.upmove || ent->client->pers.cmd.forwardmove || ent->client->pers.cmd.rightmove) { // hack, don't do while moving
        return;
    }
    if (taunt != tauntTypes_t::TAUNT_TAUNT) {                                                      // normal taunt always allowed
        if (level.gametype != gametype_t::GT_DUEL && level.gametype != gametype_t::GT_POWERDUEL) { // no taunts unless in Duel
            return;
        }
    }

    // fix: rocket lock bug
    BG_ClearRocketLock(&ent->client->ps);

    if (ent->client->ps.torsoTimer < 1 && ent->client->ps.forceHandExtend == forceHandAnims_t::HANDEXTEND_NONE && ent->client->ps.legsTimer < 1 &&
        ent->client->ps.weaponTime < 1 && ent->client->ps.saberLockTime < level.time) {
        std::optional<animNumber_t> anim;
        switch (taunt) {
        case tauntTypes_t::TAUNT_TAUNT:
            if (ent->client->ps.weapon != weapon_t::WP_SABER) {
                anim = animNumber_t::BOTH_ENGAGETAUNT;
            }
            else if (ent->client->saber[0].tauntAnim != animNumber_t::ANIM_NONE) {
                anim = ent->client->saber[0].tauntAnim;
            }
            else if (ent->client->saber[1].model[0] && ent->client->saber[1].tauntAnim != animNumber_t::ANIM_NONE) {
                anim = ent->client->saber[1].tauntAnim;
            }
            else {
                switch (ent->client->ps.fd.saberAnimLevel) {
                case saber_styles_t::SS_FAST:
                case saber_styles_t::SS_TAVION:
                    if (ent->client->ps.saberHolstered == 1 && ent->client->saber[1].model[0]) { // turn off second saber
                        G_Sound(ent, sound::SoundClass::Saber, sound::Slot::Weapon, ent->client->saber[1].soundOff);
                    }
                    else if (ent->client->ps.saberHolstered == 0) { // turn off first
                        G_Sound(ent, sound::SoundClass::Saber, sound::Slot::Weapon, ent->client->saber[0].soundOff);
                    }
                    ent->client->ps.saberHolstered = 2;
                    anim = animNumber_t::BOTH_GESTURE1;
                    break;
                case saber_styles_t::SS_MEDIUM:
                case saber_styles_t::SS_STRONG:
                case saber_styles_t::SS_DESANN:
                    anim = animNumber_t::BOTH_ENGAGETAUNT;
                    break;
                case saber_styles_t::SS_DUAL:
                    if (ent->client->ps.saberHolstered == 1 && ent->client->saber[1].model[0]) { // turn on second saber
                        G_Sound(ent, sound::SoundClass::Saber, sound::Slot::Weapon, ent->client->saber[1].soundOn);
                    }
                    else if (ent->client->ps.saberHolstered == 2) { // turn on first
                        G_Sound(ent, sound::SoundClass::Saber, sound::Slot::Weapon, ent->client->saber[0].soundOn);
                    }
                    ent->client->ps.saberHolstered = 0;
                    anim = animNumber_t::BOTH_DUAL_TAUNT;
                    break;
                case saber_styles_t::SS_STAFF:
                    if (ent->client->ps.saberHolstered > 0) { // turn on all blades
                        G_Sound(ent, sound::SoundClass::Saber, sound::Slot::Weapon, ent->client->saber[0].soundOn);
                    }
                    ent->client->ps.saberHolstered = 0;
                    anim = animNumber_t::BOTH_STAFF_TAUNT;
                    break;
                default:
                    break;
                }
            }
            break;
        case tauntTypes_t::TAUNT_BOW:
            if (ent->client->saber[0].bowAnim != animNumber_t::ANIM_NONE) {
                anim = ent->client->saber[0].bowAnim;
            }
            else if (ent->client->saber[1].model[0] && ent->client->saber[1].bowAnim != animNumber_t::ANIM_NONE) {
                anim = ent->client->saber[1].bowAnim;
            }
            else {
                anim = animNumber_t::BOTH_BOW;
            }
            if (ent->client->ps.saberHolstered == 1 && ent->client->saber[1].model[0]) { // turn off second saber
                G_Sound(ent, sound::SoundClass::Saber, sound::Slot::Weapon, ent->client->saber[1].soundOff);
            }
            else if (ent->client->ps.saberHolstered == 0) { // turn off first
                G_Sound(ent, sound::SoundClass::Saber, sound::Slot::Weapon, ent->client->saber[0].soundOff);
            }
            ent->client->ps.saberHolstered = 2;
            break;
        case tauntTypes_t::TAUNT_MEDITATE:
            if (ent->client->saber[0].meditateAnim != animNumber_t::ANIM_NONE) {
                anim = ent->client->saber[0].meditateAnim;
            }
            else if (ent->client->saber[1].model[0] && ent->client->saber[1].meditateAnim != animNumber_t::ANIM_NONE) {
                anim = ent->client->saber[1].meditateAnim;
            }
            else {
                anim = animNumber_t::BOTH_MEDITATE;
            }
            if (ent->client->ps.saberHolstered == 1 && ent->client->saber[1].model[0]) { // turn off second saber
                G_Sound(ent, sound::SoundClass::Saber, sound::Slot::Weapon, ent->client->saber[1].soundOff);
            }
            else if (ent->client->ps.saberHolstered == 0) { // turn off first
                G_Sound(ent, sound::SoundClass::Saber, sound::Slot::Weapon, ent->client->saber[0].soundOff);
            }
            ent->client->ps.saberHolstered = 2;
            break;
        case tauntTypes_t::TAUNT_FLOURISH:
            if (ent->client->ps.weapon == weapon_t::WP_SABER) {
                if (ent->client->ps.saberHolstered == 1 && ent->client->saber[1].model[0]) { // turn on second saber
                    G_Sound(ent, sound::SoundClass::Saber, sound::Slot::Weapon, ent->client->saber[1].soundOn);
                }
                else if (ent->client->ps.saberHolstered == 2) { // turn on first
                    G_Sound(ent, sound::SoundClass::Saber, sound::Slot::Weapon, ent->client->saber[0].soundOn);
                }
                ent->client->ps.saberHolstered = 0;
                if (ent->client->saber[0].flourishAnim != animNumber_t::ANIM_NONE) {
                    anim = ent->client->saber[0].flourishAnim;
                }
                else if (ent->client->saber[1].model[0] && ent->client->saber[1].flourishAnim != animNumber_t::ANIM_NONE) {
                    anim = ent->client->saber[1].flourishAnim;
                }
                else {
                    switch (ent->client->ps.fd.saberAnimLevel) {
                    case saber_styles_t::SS_FAST:
                    case saber_styles_t::SS_TAVION:
                        anim = animNumber_t::BOTH_SHOWOFF_FAST;
                        break;
                    case saber_styles_t::SS_MEDIUM:
                        anim = animNumber_t::BOTH_SHOWOFF_MEDIUM;
                        break;
                    case saber_styles_t::SS_STRONG:
                    case saber_styles_t::SS_DESANN:
                        anim = animNumber_t::BOTH_SHOWOFF_STRONG;
                        break;
                    case saber_styles_t::SS_DUAL:
                        anim = animNumber_t::BOTH_SHOWOFF_DUAL;
                        break;
                    case saber_styles_t::SS_STAFF:
                        anim = animNumber_t::BOTH_SHOWOFF_STAFF;
                        break;
                    default:
                        break;
                    }
                }
            }
            break;
        case tauntTypes_t::TAUNT_GLOAT:
            if (ent->client->saber[0].gloatAnim != animNumber_t::ANIM_NONE) {
                anim = ent->client->saber[0].gloatAnim;
            }
            else if (ent->client->saber[1].model[0] && ent->client->saber[1].gloatAnim != animNumber_t::ANIM_NONE) {
                anim = ent->client->saber[1].gloatAnim;
            }
            else {
                switch (ent->client->ps.fd.saberAnimLevel) {
                case saber_styles_t::SS_FAST:
                case saber_styles_t::SS_TAVION:
                    anim = animNumber_t::BOTH_VICTORY_FAST;
                    break;
                case saber_styles_t::SS_MEDIUM:
                    anim = animNumber_t::BOTH_VICTORY_MEDIUM;
                    break;
                case saber_styles_t::SS_STRONG:
                case saber_styles_t::SS_DESANN:
                    if (ent->client->ps.saberHolstered) { // turn on first
                        G_Sound(ent, sound::SoundClass::Saber, sound::Slot::Weapon, ent->client->saber[0].soundOn);
                    }
                    ent->client->ps.saberHolstered = 0;
                    anim = animNumber_t::BOTH_VICTORY_STRONG;
                    break;
                case saber_styles_t::SS_DUAL:
                    if (ent->client->ps.saberHolstered == 1 && ent->client->saber[1].model[0]) { // turn on second saber
                        G_Sound(ent, sound::SoundClass::Saber, sound::Slot::Weapon, ent->client->saber[1].soundOn);
                    }
                    else if (ent->client->ps.saberHolstered == 2) { // turn on first
                        G_Sound(ent, sound::SoundClass::Saber, sound::Slot::Weapon, ent->client->saber[0].soundOn);
                    }
                    ent->client->ps.saberHolstered = 0;
                    anim = animNumber_t::BOTH_VICTORY_DUAL;
                    break;
                case saber_styles_t::SS_STAFF:
                    if (ent->client->ps.saberHolstered) { // turn on first
                        G_Sound(ent, sound::SoundClass::Saber, sound::Slot::Weapon, ent->client->saber[0].soundOn);
                    }
                    ent->client->ps.saberHolstered = 0;
                    anim = animNumber_t::BOTH_VICTORY_STAFF;
                    break;
                default:
                    break;
                }
            }
            break;
        }
        if (anim) {
            if (ent->client->ps.groundEntityNum != ENTITYNUM_NONE) {
                ent->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_TAUNT;
                PS_SetDodgeAnim(ent->client->ps, *anim);
                ent->client->ps.forceHandExtendTime = level.time + BG_AnimLength(ent->localAnimIndex, *anim);
            }
            if (taunt != tauntTypes_t::TAUNT_MEDITATE && taunt != tauntTypes_t::TAUNT_BOW) { // no sound for meditate or bow
                G_AddEvent(ent, entity_event_t::EV_TAUNT, std::to_underlying(taunt));
            }
        }
    }
}

/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame on fast clients.

If "g_synchronousClients 1" is set, this will be called exactly
once for each server frame, which makes for smooth demo recording.
==============
*/
void ClientThink_real(gentity_t* ent)
{
    gclient_t* client;
    pmove_t pmove;
    int oldEventSequence;
    int msec;
    usercmd_t* ucmd;
    bool isNPC = false;
    bool controlledByPlayer = false;
    bool killJetFlags = true;
    bool isFollowing;

    client = ent->client;

    if (ent->s.eType == entityType_t::ET_NPC) {
        isNPC = true;
    }

    // don't think if the client is not yet connected (and thus not yet spawned in)
    if (client->pers.connected != clientConnected_t::CON_CONNECTED && !isNPC) {
        return;
    }

    // This code was moved here from clientThink to fix a problem with g_synchronousClients
    // being set to 1 when in vehicles.
    if (ent->s.number < MAX_CLIENTS && ent->client->ps.m_iVehicleNum) { // driving a vehicle
        if (g_entities[ent->client->ps.m_iVehicleNum].client) {
            gentity_t* veh = &g_entities[ent->client->ps.m_iVehicleNum];

            if (veh->m_pVehicle && veh->m_pVehicle->m_pPilot == (bgEntity_t*)ent) { // only take input from the pilot...
                veh->client->ps.commandTime = ent->client->ps.commandTime;
                memcpy(&veh->m_pVehicle->m_ucmd, &ent->client->pers.cmd, sizeof(usercmd_t));
                if (Any(veh->m_pVehicle->m_ucmd.buttons & buttonFlags_t::BUTTON_TALK)) { // forced input if "chat bubble" is up
                    veh->m_pVehicle->m_ucmd.buttons = buttonFlags_t::BUTTON_TALK;
                    veh->m_pVehicle->m_ucmd.forwardmove = 0;
                    veh->m_pVehicle->m_ucmd.rightmove = 0;
                    veh->m_pVehicle->m_ucmd.upmove = 0;
                }
            }
        }
    }

    isFollowing = Any(client->ps.pm_flags & pmoveFlags_t::PMF_FOLLOW) ? true : false;

    if (!isFollowing) {
        if (level.gametype == gametype_t::GT_SIEGE && client->siegeClass != -1 &&
            bgSiegeClasses[client->siegeClass].saberStance) { // the class says we have to use this stance set.
            if (!(bgSiegeClasses[client->siegeClass].saberStance &
                  Bit(client->ps.fd.saberAnimLevel))) { // the current stance is not in the bitmask, so find the first one that is.
                saber_styles_t i = saber_styles_t::SS_FAST;

                while (i < saber_styles_t::SS_NUM_SABER_STYLES) {
                    if (bgSiegeClasses[client->siegeClass].saberStance & Bit(i)) {
                        if (i == saber_styles_t::SS_DUAL && client->ps.saberHolstered == 1) { // one saber should be off, adjust saberAnimLevel accordinly
                            client->ps.fd.saberAnimLevelBase = i;
                            client->ps.fd.saberAnimLevel = saber_styles_t::SS_FAST;
                            client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
                        }
                        else if (i == saber_styles_t::SS_STAFF && client->ps.saberHolstered == 1 &&
                                 client->saber[0].singleBladeStyle !=
                                     saber_styles_t::SS_NONE) { // one saber or blade should be off, adjust saberAnimLevel accordinly
                            client->ps.fd.saberAnimLevelBase = i;
                            client->ps.fd.saberAnimLevel = client->saber[0].singleBladeStyle;
                            client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
                        }
                        else {
                            client->ps.fd.saberAnimLevelBase = client->ps.fd.saberAnimLevel = i;
                            client->ps.fd.saberDrawAnimLevel = i;
                        }
                        break;
                    }

                    i++;
                }
            }
        }
        else if (client->saber[0].model[0] && client->saber[1].model[0]) { // with two sabs always use akimbo style
            if (client->ps.saberHolstered == 1) {                          // one saber should be off, adjust saberAnimLevel accordinly
                client->ps.fd.saberAnimLevelBase = saber_styles_t::SS_DUAL;
                client->ps.fd.saberAnimLevel = saber_styles_t::SS_FAST;
                client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
            }
            else {
                if (!WP_SaberStyleValidForSaber(&client->saber[0], &client->saber[1], client->ps.saberHolstered,
                                                client->ps.fd.saberAnimLevel)) { // only use dual style if the style we're trying to use isn't valid
                    client->ps.fd.saberAnimLevelBase = client->ps.fd.saberAnimLevel = saber_styles_t::SS_DUAL;
                }
                client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
            }
        }
        else {
            if (client->saber[0].stylesLearned == Bit(saber_styles_t::SS_STAFF)) { // then *always* use the staff style
                client->ps.fd.saberAnimLevelBase = saber_styles_t::SS_STAFF;
            }
            if (client->ps.fd.saberAnimLevelBase == saber_styles_t::SS_STAFF) { // using staff style
                if (client->ps.saberHolstered == 1 &&
                    client->saber[0].singleBladeStyle != saber_styles_t::SS_NONE) { // one blade should be off, adjust saberAnimLevel accordinly
                    client->ps.fd.saberAnimLevel = client->saber[0].singleBladeStyle;
                    client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
                }
                else {
                    client->ps.fd.saberAnimLevel = saber_styles_t::SS_STAFF;
                    client->ps.fd.saberDrawAnimLevel = client->ps.fd.saberAnimLevel;
                }
            }
        }
    }

    // mark the time, so the connection sprite can be removed
    ucmd = &ent->client->pers.cmd;

    if (client && !isFollowing && Any(client->ps.eFlags2 & entityFlags2_t::EF2_HELD_BY_MONSTER)) {
        G_HeldByMonster(ent, ucmd);
    }

    // sanity check the command time to prevent speedup cheating
    if (ucmd->serverTime > level.tickTime + 200) {
        ucmd->serverTime = level.tickTime + 200;
        //		Q_Println("serverTime <<<<<");
    }
    if (ucmd->serverTime < level.tickTime - 1000) {
        ucmd->serverTime = level.tickTime - 1000;
        //		Q_Println("serverTime >>>>>");
    }

    if (isNPC && (ucmd->serverTime - client->ps.commandTime) < 1) {
        ucmd->serverTime = client->ps.commandTime + 100;
    }

    msec = ucmd->serverTime - client->ps.commandTime;
    // following others may result in bad times, but we still want
    // to check for follow toggles
    if (msec < 1 && client->sess.spectatorState != spectatorState_t::SPECTATOR_FOLLOW) {
        return;
    }

    if (msec > 200) {
        msec = 200;
    }

    //
    // check for exiting intermission
    //
    if (level.intermissiontime) {
        if (ent->s.number < MAX_CLIENTS || client->NPC_class == class_t::CLASS_VEHICLE) { // players and vehicles do nothing in intermissions
            ClientIntermissionThink(client);
            return;
        }
    }

    // spectators don't do much
    if (client->sess.sessionTeam == team_t::TEAM_SPECTATOR || client->tempSpectate >= level.time) {
        if (client->sess.spectatorState == spectatorState_t::SPECTATOR_SCOREBOARD) {
            return;
        }
        SpectatorThink(ent, ucmd);
        return;
    }

    if (ent && ent->client && Any(ent->client->ps.eFlags & entityFlags_t::EF_INVULNERABLE)) {
        if (ent->client->invulnerableTimer <= level.time) {
            ent->client->ps.eFlags &= ~entityFlags_t::EF_INVULNERABLE;
        }
    }

    if (ent->s.eType != entityType_t::ET_NPC) {
        // check for inactivity timer, but never drop the local client of a non-dedicated server
        if (!ClientInactivityTimer(client)) {
            return;
        }
    }

    // Check if we should have a fullbody push effect around the player
    if (client->pushEffectTime > level.time) {
        client->ps.eFlags |= entityFlags_t::EF_BODYPUSH;
    }
    else if (client->pushEffectTime) {
        client->pushEffectTime = 0;
        client->ps.eFlags &= ~entityFlags_t::EF_BODYPUSH;
    }

    if (client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] & Bit(holdable_t::HI_JETPACK)) {
        client->ps.eFlags |= entityFlags_t::EF_JETPACK;
    }
    else {
        client->ps.eFlags &= ~entityFlags_t::EF_JETPACK;
    }

    if (client->noclip) {
        client->ps.pm_type = pmtype_t::PM_NOCLIP;
    }
    else if (Any(client->ps.eFlags & entityFlags_t::EF_DISINTEGRATION)) {
        client->ps.pm_type = pmtype_t::PM_NOCLIP;
    }
    else if (client->ps.stats[statIndex_t::STAT_HEALTH] <= 0) {
        client->ps.pm_type = pmtype_t::PM_DEAD;
    }
    else {
        if (client->ps.forceGripChangeMovetype != pmtype_t::PM_NORMAL) {
            client->ps.pm_type = client->ps.forceGripChangeMovetype;
        }
        else {
            if (client->jetPackOn) {
                client->ps.pm_type = pmtype_t::PM_JETPACK;
                client->ps.eFlags |= entityFlags_t::EF_JETPACK_ACTIVE;
                killJetFlags = false;
            }
            else {
                client->ps.pm_type = pmtype_t::PM_NORMAL;
            }
        }
    }

    if (killJetFlags) {
        client->ps.eFlags &= ~entityFlags_t::EF_JETPACK_ACTIVE;
        client->ps.eFlags &= ~entityFlags_t::EF_JETPACK_FLAMING;
    }

    constexpr float SLOWDOWN_DIST = 128.0f;
    constexpr float MIN_NPC_SPEED = 16.0f;

    if (client->bodyGrabIndex != ENTITYNUM_NONE) {
        gentity_t* grabbed = &g_entities[client->bodyGrabIndex];

        if (!grabbed->inuse || grabbed->s.eType != entityType_t::ET_BODY || Any(grabbed->s.eFlags & entityFlags_t::EF_DISINTEGRATION) ||
            Any(grabbed->s.eFlags & entityFlags_t::EF_NODRAW)) {
            if (grabbed->inuse && grabbed->s.eType == entityType_t::ET_BODY) {
                grabbed->s.ragAttach = 0;
            }
            client->bodyGrabIndex = ENTITYNUM_NONE;
        }
        else {
            mdxaBone_t rhMat;
            vec3_t rhOrg, tAng;
            vec3_t bodyDir;
            float bodyDist;

            ent->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_DRAGGING;

            if (ent->client->ps.forceHandExtendTime < level.time + 500) {
                ent->client->ps.forceHandExtendTime = level.time + 1000;
            }

            VectorSet(tAng, 0, ent->client->ps.viewangles[YAW], 0);
            trap->G2API_GetBoltMatrix(ent->ghoul2, 0, 0, &rhMat, tAng, ent->client->ps.origin, level.time, nullptr,
                                      ent->modelScale); // 0 is always going to be right hand bolt
            BG_GiveMeVectorFromMatrix(&rhMat, orientations_t::ORIGIN, rhOrg);

            VectorSubtract(rhOrg, grabbed->r.currentOrigin, bodyDir);
            bodyDist = VectorLength(bodyDir);

            if (bodyDist > 40.0f) { // can no longer reach
                grabbed->s.ragAttach = 0;
                client->bodyGrabIndex = ENTITYNUM_NONE;
            }
            else if (bodyDist > 24.0f) {
                bodyDir[2] = 0; // don't want it floating
                // VectorScale(bodyDir, 0.1f, bodyDir);
                VectorAdd(grabbed->epVelocity, bodyDir, grabbed->epVelocity);
                G_Sound(grabbed, sound::SoundClass::Body, sound::Slot::None, G_SoundIndex("sound/player/roll1.wav"));
            }
        }
    }
    else if (ent->client->ps.forceHandExtend == forceHandAnims_t::HANDEXTEND_DRAGGING) {
        ent->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_WEAPONREADY;
    }

    if (ent->NPC && ent->s.NPC_class != class_t::CLASS_VEHICLE) // vehicles manage their own speed
    {
        // FIXME: swoop should keep turning (and moving forward?) for a little bit?
        if (ent->NPC->combatMove == false) {
            // if ( !Any(ucmd->buttons & buttonFlags_t::BUTTON_USE) )
            if (1) {                                                                                       // Not leaning
                bool Flying = (ucmd->upmove && Any(ent->client->ps.eFlags2 & entityFlags2_t::EF2_FLYING)); // ent->client->moveType == MT_FLYSWIM);
                bool Climbing = (ucmd->upmove && Any(ent->watertype & contentsFlags_t::CONTENTS_LADDER));

                // client->ps.friction = 6;

                if (ucmd->forwardmove || ucmd->rightmove || Flying) {
                    // if ( ent->NPC->behaviorState != BS_FORMATION )
                    { // In - Formation NPCs set thier desiredSpeed themselves
                        if (Any(ucmd->buttons & buttonFlags_t::BUTTON_WALKING)) {
                            ent->NPC->desiredSpeed = NPC_GetWalkSpeed(ent); // ent->NPC->stats.walkSpeed;
                        }
                        else // running
                        {
                            ent->NPC->desiredSpeed = NPC_GetRunSpeed(ent); // ent->NPC->stats.runSpeed;
                        }

                        if (ent->NPC->currentSpeed >= 80 && !controlledByPlayer) { // At higher speeds, need to slow down close to stuff
                            // Slow down as you approach your goal
                            if (ent->NPC->distToGoal < SLOWDOWN_DIST && !Any(ent->NPC->aiFlags & npcAiFlags_t::NPCAI_NO_SLOWDOWN)) // 128
                            {
                                if (ent->NPC->desiredSpeed > MIN_NPC_SPEED) {
                                    float slowdownSpeed = ((float)ent->NPC->desiredSpeed) * ent->NPC->distToGoal / SLOWDOWN_DIST;

                                    ent->NPC->desiredSpeed = Q_ceil(slowdownSpeed);
                                    if (ent->NPC->desiredSpeed < MIN_NPC_SPEED) { // don't slow down too much
                                        ent->NPC->desiredSpeed = MIN_NPC_SPEED;
                                    }
                                }
                            }
                        }
                    }
                }
                else if (Climbing) {
                    ent->NPC->desiredSpeed = ent->NPC->stats.walkSpeed;
                }
                else { // We want to stop
                    ent->NPC->desiredSpeed = 0;
                }

                NPC_Accelerate(ent, false, false);

                if (ent->NPC->currentSpeed <= 24 && ent->NPC->desiredSpeed < ent->NPC->currentSpeed) { // No-one walks this slow
                    client->ps.speed = ent->NPC->currentSpeed = 0;                                     // Full stop
                    ucmd->forwardmove = 0;
                    ucmd->rightmove = 0;
                }
                else {
                    if (ent->NPC->currentSpeed <= ent->NPC->stats.walkSpeed) { // Play the walkanim
                        ucmd->buttons |= buttonFlags_t::BUTTON_WALKING;
                    }
                    else {
                        ucmd->buttons &= ~buttonFlags_t::BUTTON_WALKING;
                    }

                    if (ent->NPC->currentSpeed > 0) { // We should be moving
                        if (Climbing || Flying) {
                            if (!ucmd->upmove) {                           // We need to force them to take a couple more steps until stopped
                                ucmd->upmove = ent->NPC->last_ucmd.upmove; // was last_upmove;
                            }
                        }
                        else if (!ucmd->forwardmove && !ucmd->rightmove) {       // We need to force them to take a couple more steps until stopped
                            ucmd->forwardmove = ent->NPC->last_ucmd.forwardmove; // was last_forwardmove;
                            ucmd->rightmove = ent->NPC->last_ucmd.rightmove;     // was last_rightmove;
                        }
                    }

                    client->ps.speed = ent->NPC->currentSpeed;
                    //	if ( player && player->client && player->client->ps.viewEntity == ent->s.number )
                    //	{
                    //	}
                    //	else
                    // rwwFIXMEFIXME: do this and also check for all real client
                    if (1) {
                        // Slow down on turns - don't orbit!!!
                        float turndelta = 0;
                        // if the NPC is locked into a Yaw, we want to check the lockedDesiredYaw...otherwise the NPC can't walk backwards, because it always
                        // thinks it trying to turn according to desiredYaw
                        // if( client->renderInfo.renderFlags & RF_LOCKEDANGLE ) // yeah I know the RF_ flag is a pretty ugly hack...
                        if (0) // rwwFIXMEFIXME: ...
                        {
                            turndelta = (180 - Q_fabs(AngleDelta(ent->r.currentAngles[YAW], ent->NPC->lockedDesiredYaw))) / 180;
                        }
                        else {
                            turndelta = (180 - Q_fabs(AngleDelta(ent->r.currentAngles[YAW], ent->NPC->desiredYaw))) / 180;
                        }

                        if (turndelta < 0.75f) {
                            client->ps.speed = 0;
                        }
                        else if (ent->NPC->distToGoal < 100 && turndelta < 1.0) { // Turn is greater than 45 degrees or closer than 100 to goal
                            client->ps.speed = Q_floor(((float)(client->ps.speed)) * turndelta);
                        }
                    }
                }
            }
        }
        else {
            ent->NPC->desiredSpeed = Any(ucmd->buttons & buttonFlags_t::BUTTON_WALKING) ? NPC_GetWalkSpeed(ent) : NPC_GetRunSpeed(ent);

            client->ps.speed = ent->NPC->desiredSpeed;
        }

        if (Any(ucmd->buttons & buttonFlags_t::BUTTON_WALKING)) { // sort of a hack I guess since MP handles walking differently from SP (has some proxy cheat
                                                                  // prevention methods)
            /*
            if (ent->client->ps.speed > 64)
            {
                ent->client->ps.speed = 64;
            }
            */

            if (ucmd->forwardmove > 64) {
                ucmd->forwardmove = 64;
            }
            else if (ucmd->forwardmove < -64) {
                ucmd->forwardmove = -64;
            }

            if (ucmd->rightmove > 64) {
                ucmd->rightmove = 64;
            }
            else if (ucmd->rightmove < -64) {
                ucmd->rightmove = -64;
            }

            // ent->client->ps.speed = ent->client->ps.basespeed = NPC_GetRunSpeed( ent );
        }
        client->ps.basespeed = client->ps.speed;
    }
    else if (!client->ps.m_iVehicleNum && (!ent->NPC || ent->s.NPC_class != class_t::CLASS_VEHICLE)) // if riding a vehicle it will manage our speed and such
    {
        // set speed
        client->ps.speed = g_speed.value;

        // Check for a siege class speed multiplier
        if (level.gametype == gametype_t::GT_SIEGE && client->siegeClass != -1) {
            client->ps.speed *= bgSiegeClasses[client->siegeClass].speed;
        }

        if (client->bodyGrabIndex != ENTITYNUM_NONE) { // can't go nearly as fast when dragging a body around
            client->ps.speed *= 0.2f;
        }

        client->ps.basespeed = client->ps.speed;
    }

    if (!ent->NPC || !Any(ent->NPC->aiFlags & npcAiFlags_t::NPCAI_CUSTOM_GRAVITY)) {                                                 // use global gravity
        if (ent->NPC && ent->s.NPC_class == class_t::CLASS_VEHICLE && ent->m_pVehicle && ent->m_pVehicle->m_pVehicleInfo->gravity) { // use custom veh gravity
            client->ps.gravity = ent->m_pVehicle->m_pVehicleInfo->gravity;
        }
        else {
            if (ent->client->inSpaceIndex && ent->client->inSpaceIndex != ENTITYNUM_NONE) { // in space, so no gravity...
                client->ps.gravity = 1.0f;
                if (ent->s.number < MAX_CLIENTS) {
                    VectorScale(client->ps.velocity, 0.8f, client->ps.velocity);
                }
            }
            else {
                if (Any(client->ps.eFlags2 & entityFlags2_t::EF2_SHIP_DEATH)) { // float there
                    VectorClear(client->ps.velocity);
                    client->ps.gravity = 1.0f;
                }
                else {
                    client->ps.gravity = g_gravity.value;
                }
            }
        }
    }

    if (ent->client->ps.duelInProgress) {
        gentity_t* duelAgainst = &g_entities[ent->client->ps.duelIndex];

        // Keep the time updated, so once this duel ends this player can't engage in a duel for another
        // 10 seconds. This will give other people a chance to engage in duels in case this player wants
        // to engage again right after he's done fighting and someone else is waiting.
        ent->client->ps.fd.privateDuelTime = level.time + 10000;

        if (ent->client->ps.duelTime < level.time) {
            // Bring out the sabers
            if (ent->client->ps.weapon == weapon_t::WP_SABER && ent->client->ps.saberHolstered && ent->client->ps.duelTime) {
                ent->client->ps.saberHolstered = 0;

                if (ent->client->saber[0].soundOn) {
                    G_Sound(ent, sound::SoundClass::Saber, sound::Slot::None, ent->client->saber[0].soundOn);
                }
                if (ent->client->saber[1].soundOn) {
                    G_Sound(ent, sound::SoundClass::Saber, sound::Slot::None, ent->client->saber[1].soundOn);
                }

                G_AddEvent(ent, entity_event_t::EV_PRIVATE_DUEL, 2);

                ent->client->ps.duelTime = 0;
            }

            if (duelAgainst && duelAgainst->client && duelAgainst->inuse && duelAgainst->client->ps.weapon == weapon_t::WP_SABER &&
                duelAgainst->client->ps.saberHolstered && duelAgainst->client->ps.duelTime) {
                duelAgainst->client->ps.saberHolstered = 0;

                if (duelAgainst->client->saber[0].soundOn) {
                    G_Sound(duelAgainst, sound::SoundClass::Saber, sound::Slot::None, duelAgainst->client->saber[0].soundOn);
                }
                if (duelAgainst->client->saber[1].soundOn) {
                    G_Sound(duelAgainst, sound::SoundClass::Saber, sound::Slot::None, duelAgainst->client->saber[1].soundOn);
                }

                G_AddEvent(duelAgainst, entity_event_t::EV_PRIVATE_DUEL, 2);

                duelAgainst->client->ps.duelTime = 0;
            }
        }
        else {
            client->ps.speed = 0;
            client->ps.basespeed = 0;
            ucmd->forwardmove = 0;
            ucmd->rightmove = 0;
            ucmd->upmove = 0;
        }

        if (!duelAgainst || !duelAgainst->client || !duelAgainst->inuse || duelAgainst->client->ps.duelIndex != ent->s.number) {
            ent->client->ps.duelInProgress = 0;
            G_AddEvent(ent, entity_event_t::EV_PRIVATE_DUEL, 0);
        }
        else if (duelAgainst->health < 1 || duelAgainst->client->ps.stats[statIndex_t::STAT_HEALTH] < 1) {
            ent->client->ps.duelInProgress = 0;
            duelAgainst->client->ps.duelInProgress = 0;

            G_AddEvent(ent, entity_event_t::EV_PRIVATE_DUEL, 0);
            G_AddEvent(duelAgainst, entity_event_t::EV_PRIVATE_DUEL, 0);

            // Winner gets full health.. providing he's still alive
            if (ent->health > 0 && ent->client->ps.stats[statIndex_t::STAT_HEALTH] > 0) {
                if (ent->health < ent->client->ps.stats[statIndex_t::STAT_MAX_HEALTH]) {
                    ent->client->ps.stats[statIndex_t::STAT_HEALTH] = ent->health = ent->client->ps.stats[statIndex_t::STAT_MAX_HEALTH];
                }

                if (g_spawnInvulnerability.integer) {
                    ent->client->ps.eFlags |= entityFlags_t::EF_INVULNERABLE;
                    ent->client->invulnerableTimer = level.time + g_spawnInvulnerability.integer;
                }
            }

            /*
            trap->SendServerCommand( ent-g_entities, Q_Format("print \"{} {}\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME",
            "PLDUELWINNER")).c_str() ); trap->SendServerCommand( duelAgainst-g_entities, Q_Format("print \"{} {}\n\"", ent->client->pers.netname,
            G_GetStringEdString("MP_SVGAME", "PLDUELWINNER")).c_str() );
            */
            // Private duel announcements are now made globally because we only want one duel at a time.
            if (ent->health > 0 && ent->client->ps.stats[statIndex_t::STAT_HEALTH] > 0) {
                trap->SendServerCommand(-1, Q_Format("cp \"{} {} {}!\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELWINNER"),
                                                     duelAgainst->client->pers.netname)
                                                .c_str());
            }
            else { // it was a draw, because we both managed to die in the same frame
                trap->SendServerCommand(-1, Q_Format("cp \"{}\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELTIE")).c_str());
            }
        }
        else {
            vec3_t vSub;
            float subLen = 0;

            VectorSubtract(ent->client->ps.origin, duelAgainst->client->ps.origin, vSub);
            subLen = VectorLength(vSub);

            if (subLen >= 1024) {
                ent->client->ps.duelInProgress = 0;
                duelAgainst->client->ps.duelInProgress = 0;

                G_AddEvent(ent, entity_event_t::EV_PRIVATE_DUEL, 0);
                G_AddEvent(duelAgainst, entity_event_t::EV_PRIVATE_DUEL, 0);

                trap->SendServerCommand(-1, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELSTOP")).c_str());
            }
        }
    }

    if (ent->client->doingThrow > level.time) {
        gentity_t* throwee = &g_entities[ent->client->throwingIndex];

        if (!throwee->inuse || !throwee->client || throwee->health < 1 || throwee->client->sess.sessionTeam == team_t::TEAM_SPECTATOR ||
            Any(throwee->client->ps.pm_flags & pmoveFlags_t::PMF_FOLLOW) || throwee->client->throwingIndex != ent->s.number) {
            ent->client->doingThrow = 0;
            ent->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_NONE;

            if (throwee->inuse && throwee->client) {
                throwee->client->ps.heldByClient = 0;
                throwee->client->beingThrown = 0;

                if (throwee->client->ps.forceHandExtend != forceHandAnims_t::HANDEXTEND_POSTTHROWN) {
                    throwee->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_NONE;
                }
            }
        }
    }

    if (ent->client->beingThrown > level.time) {
        gentity_t* thrower = &g_entities[ent->client->throwingIndex];

        if (!thrower->inuse || !thrower->client || thrower->health < 1 || thrower->client->sess.sessionTeam == team_t::TEAM_SPECTATOR ||
            Any(thrower->client->ps.pm_flags & pmoveFlags_t::PMF_FOLLOW) || thrower->client->throwingIndex != ent->s.number) {
            ent->client->ps.heldByClient = 0;
            ent->client->beingThrown = 0;

            if (ent->client->ps.forceHandExtend != forceHandAnims_t::HANDEXTEND_POSTTHROWN) {
                ent->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_NONE;
            }

            if (thrower->inuse && thrower->client) {
                thrower->client->doingThrow = 0;
                thrower->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_NONE;
            }
        }
        else if (thrower->inuse && thrower->client && thrower->ghoul2 && trap->G2API_HaveWeGhoul2Models(thrower->ghoul2)) {
#if 0
			int lHandBolt = trap->G2API_AddBolt(thrower->ghoul2, 0, "*l_hand");
			int pelBolt = trap->G2API_AddBolt(thrower->ghoul2, 0, "pelvis");


			if (lHandBolt != -1 && pelBolt != -1)
#endif
            {
                float pDif = 40.0f;
                vec3_t boltOrg, pBoltOrg;
                vec3_t tAngles;
                vec3_t vDif;
                vec3_t entDir, otherAngles;
                vec3_t fwd, right;

                // Always look at the thrower.
                VectorSubtract(thrower->client->ps.origin, ent->client->ps.origin, entDir);
                VectorCopy(ent->client->ps.viewangles, otherAngles);
                otherAngles[YAW] = vectoyaw(entDir);
                SetClientViewAngle(ent, otherAngles);

                VectorCopy(thrower->client->ps.viewangles, tAngles);
                tAngles[PITCH] = tAngles[ROLL] = 0;

                // Get the direction between the pelvis and position of the hand
#if 0
				mdxaBone_t boltMatrix, pBoltMatrix;

				trap->G2API_GetBoltMatrix(thrower->ghoul2, 0, lHandBolt, &boltMatrix, tAngles, thrower->client->ps.origin, level.time, 0, thrower->modelScale);
				boltOrg[0] = boltMatrix.matrix[0][3];
				boltOrg[1] = boltMatrix.matrix[1][3];
				boltOrg[2] = boltMatrix.matrix[2][3];

				trap->G2API_GetBoltMatrix(thrower->ghoul2, 0, pelBolt, &pBoltMatrix, tAngles, thrower->client->ps.origin, level.time, 0, thrower->modelScale);
				pBoltOrg[0] = pBoltMatrix.matrix[0][3];
				pBoltOrg[1] = pBoltMatrix.matrix[1][3];
				pBoltOrg[2] = pBoltMatrix.matrix[2][3];
#else // above tends to not work once in a while, for various reasons I suppose.
                VectorCopy(thrower->client->ps.origin, pBoltOrg);
                AngleVectors(tAngles, fwd, right, nullptr);
                boltOrg[0] = pBoltOrg[0] + fwd[0] * 8 + right[0] * pDif;
                boltOrg[1] = pBoltOrg[1] + fwd[1] * 8 + right[1] * pDif;
                boltOrg[2] = pBoltOrg[2];
#endif
                // G_TestLine(boltOrg, pBoltOrg, 0x0000ff, 50);

                VectorSubtract(ent->client->ps.origin, boltOrg, vDif);
                if (VectorLength(vDif) > 32.0f &&
                    (thrower->client->doingThrow - level.time) < 4500) { // the hand is too far away, and can no longer hold onto us, so escape.
                    ent->client->ps.heldByClient = 0;
                    ent->client->beingThrown = 0;
                    thrower->client->doingThrow = 0;

                    thrower->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_NONE;
                    G_EntitySound(thrower, sound::SoundClass::Voice, sound::Slot::Voice, G_SoundIndex("*pain25.wav"));

                    ent->client->ps.forceDodgeAnim = 2;
                    ent->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_KNOCKDOWN;
                    ent->client->ps.forceHandExtendTime = level.time + 500;
                    ent->client->ps.velocity[2] = 400;
                    G_PreDefSound(ent->client->ps.origin, pdSounds_t::PDSOUND_FORCEJUMP);
                }
                else if ((client->beingThrown - level.time) < 4000) { // step into the next part of the throw, and go flying back
                    float vScale = 400.0f;
                    ent->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_POSTTHROWN;
                    ent->client->ps.forceHandExtendTime = level.time + 1200;
                    ent->client->ps.forceDodgeAnim = 0;

                    thrower->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_POSTTHROW;
                    thrower->client->ps.forceHandExtendTime = level.time + 200;

                    ent->client->ps.heldByClient = 0;

                    ent->client->ps.heldByClient = 0;
                    ent->client->beingThrown = 0;
                    thrower->client->doingThrow = 0;

                    AngleVectors(thrower->client->ps.viewangles, vDif, nullptr, nullptr);
                    ent->client->ps.velocity[0] = vDif[0] * vScale;
                    ent->client->ps.velocity[1] = vDif[1] * vScale;
                    ent->client->ps.velocity[2] = 400;

                    G_EntitySound(ent, sound::SoundClass::Voice, sound::Slot::Voice, G_SoundIndex("*pain100.wav"));
                    G_EntitySound(thrower, sound::SoundClass::Voice, sound::Slot::Voice, G_SoundIndex("*jump1.wav"));

                    // Set the thrower as the "other killer", so if we die from fall/impact damage he is credited.
                    ent->client->ps.otherKiller = thrower->s.number;
                    ent->client->ps.otherKillerTime = level.time + 8000;
                    ent->client->ps.otherKillerDebounceTime = level.time + 100;
                }
                else { // see if we can move to be next to the hand.. if it's not clear, break the throw.
                    vec3_t intendedOrigin;
                    trace_t tr;
                    trace_t tr2;

                    VectorSubtract(boltOrg, pBoltOrg, vDif);
                    VectorNormalize(vDif);

                    VectorClear(ent->client->ps.velocity);
                    intendedOrigin[0] = pBoltOrg[0] + vDif[0] * pDif;
                    intendedOrigin[1] = pBoltOrg[1] + vDif[1] * pDif;
                    intendedOrigin[2] = thrower->client->ps.origin[2];

                    trap->Trace(&tr, intendedOrigin, ent->r.mins, ent->r.maxs, intendedOrigin, ent->s.number, ent->clipmask, false,
                                g2TraceFlags_t::G2TRFLAG_NONE, 0);
                    trap->Trace(&tr2, ent->client->ps.origin, ent->r.mins, ent->r.maxs, intendedOrigin, ent->s.number, contentsFlags_t::CONTENTS_SOLID, false,
                                g2TraceFlags_t::G2TRFLAG_NONE, 0);

                    if (tr.fraction == 1.0 && !tr.startsolid && tr2.fraction == 1.0 && !tr2.startsolid) {
                        VectorCopy(intendedOrigin, ent->client->ps.origin);

                        if ((client->beingThrown - level.time) < 4800) {
                            ent->client->ps.heldByClient = thrower->s.number + 1;
                        }
                    }
                    else { // if the guy can't be put here then it's time to break the throw off.
                        ent->client->ps.heldByClient = 0;
                        ent->client->beingThrown = 0;
                        thrower->client->doingThrow = 0;

                        thrower->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_NONE;
                        G_EntitySound(thrower, sound::SoundClass::Voice, sound::Slot::Voice, G_SoundIndex("*pain25.wav"));

                        ent->client->ps.forceDodgeAnim = 2;
                        ent->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_KNOCKDOWN;
                        ent->client->ps.forceHandExtendTime = level.time + 500;
                        ent->client->ps.velocity[2] = 400;
                        G_PreDefSound(ent->client->ps.origin, pdSounds_t::PDSOUND_FORCEJUMP);
                    }
                }
            }
        }
    }
    else if (ent->client->ps.heldByClient) {
        ent->client->ps.heldByClient = 0;
    }

    /*
    if ( client->ps.powerups[PW_HASTE] ) {
        client->ps.speed *= 1.3;
    }
    */

    // Will probably never need this again, since we have g2 properly serverside now.
    // But just in case.
    /*
    if (client->ps.usingATST && ent->health > 0)
    { //we have special shot clip boxes as an ATST
        ent->r.contents |= contentsFlags_t::CONTENTS_NOSHOT;
        ATST_ManageDamageBoxes(ent);
    }
    else
    {
        ent->r.contents &= ~contentsFlags_t::CONTENTS_NOSHOT;
        client->damageBoxHandle_Head = 0;
        client->damageBoxHandle_RLeg = 0;
        client->damageBoxHandle_LLeg = 0;
    }
    */

    // rww - moved this stuff into the pmove code so that it's predicted properly
    // BG_AdjustClientSpeed(&client->ps, &client->pers.cmd, level.time);

    // set up for pmove
    oldEventSequence = client->ps.eventSequence;

    memset(&pmove, 0, sizeof(pmove));

    if (Any(ent->flags & gentityFlags_t::FL_FORCE_GESTURE)) {
        ent->flags &= ~gentityFlags_t::FL_FORCE_GESTURE;
        ent->client->pers.cmd.buttons |= buttonFlags_t::BUTTON_GESTURE;
    }

    if (ent->client && ent->client->ps.fallingToDeath && (level.time - FALL_FADE_TIME) > ent->client->ps.fallingToDeath) { // die!
        if (ent->health > 0) {
            gentity_t* otherKiller = ent;
            if (ent->client->ps.otherKillerTime > level.time && ent->client->ps.otherKiller != ENTITYNUM_NONE) {
                otherKiller = &g_entities[ent->client->ps.otherKiller];

                if (!otherKiller->inuse) {
                    otherKiller = ent;
                }
            }
            G_Damage(ent, otherKiller, otherKiller, nullptr, ent->client->ps.origin, 9999, damageFlags_t::DAMAGE_NO_PROTECTION, meansOfDeath_t::MOD_FALLING);
            // player_die(ent, ent, ent, 100000, meansOfDeath_t::MOD_FALLING);
            //		if (!ent->NPC)
            //		{
            //			ClientRespawn(ent);
            //		}
            //		ent->client->ps.fallingToDeath = 0;

            G_MuteSound(ent->s.number, sound::Slot::Voice); // stop screaming, because you are dead!
        }
    }

    if (ent->client->ps.otherKillerTime > level.time && ent->client->ps.groundEntityNum != ENTITYNUM_NONE &&
        ent->client->ps.otherKillerDebounceTime < level.time) {
        ent->client->ps.otherKillerTime = 0;
        ent->client->ps.otherKiller = ENTITYNUM_NONE;
    }
    else if (ent->client->ps.otherKillerTime > level.time && ent->client->ps.groundEntityNum == ENTITYNUM_NONE) {
        if (ent->client->ps.otherKillerDebounceTime < (level.time + 100)) {
            ent->client->ps.otherKillerDebounceTime = level.time + 100;
        }
    }

    //	WP_ForcePowersUpdate( ent, msec, ucmd); //update any active force powers
    //	WP_SaberPositionUpdate(ent, ucmd); //check the server-side saber point, do apprioriate server-side actions (effects are cs-only)

    // NOTE: can't put USE here *before* PMove!!
    if (ent->client->ps.useDelay > level.time && ent->client->ps.m_iVehicleNum) { // when in a vehicle, debounce the use...
        ucmd->buttons &= ~buttonFlags_t::BUTTON_USE;
    }

    // FIXME: need to do this before check to avoid walls and cliffs (or just cliffs?)
    G_AddPushVecToUcmd(ent, ucmd);

    // play/stop any looping sounds tied to controlled movement
    G_CheckMovingLoopingSounds(ent, ucmd);

    pmove.ps = &client->ps;
    pmove.cmd = *ucmd;
    if (pmove.ps->pm_type == pmtype_t::PM_DEAD) {
        pmove.tracemask = MASK_PLAYERSOLID & ~contentsFlags_t::CONTENTS_BODY;
    }
    else if (Any(ent->r.svFlags & svFlags_t::SVF_BOT)) {
        pmove.tracemask = MASK_PLAYERSOLID | contentsFlags_t::CONTENTS_MONSTERCLIP;
    }
    else {
        pmove.tracemask = MASK_PLAYERSOLID;
    }
    pmove.trace = SV_PMTrace;
    pmove.pointcontents = trap->PointContents;
    pmove.debugLevel = g_debugMove.integer;
    pmove.noFootsteps = Any(G_DmFlags() & dmFlags_t::DF_NO_FOOTSTEPS);

    pmove.animations = bgAllAnims[ent->localAnimIndex].anims; // nullptr;

    // rww - bgghoul2
    pmove.ghoul2 = nullptr;

#ifdef _DEBUG
    if (g_disableServerG2.integer) {
    }
    else
#endif
        if (ent->ghoul2) {
        if (ent->localAnimIndex > 1) { // if it isn't humanoid then we will be having none of this.
            pmove.ghoul2 = nullptr;
        }
        else {
            pmove.ghoul2 = ent->ghoul2;
            pmove.g2Bolts_LFoot = trap->G2API_AddBolt(ent->ghoul2, 0, "*l_leg_foot");
            pmove.g2Bolts_RFoot = trap->G2API_AddBolt(ent->ghoul2, 0, "*r_leg_foot");
        }
    }

    // point the saber data to the right place
#if 0
	k = 0;
	while (k < MAX_SABERS)
	{
		if (ent->client->saber[k].model[0])
		{
			pm.saber[k] = &ent->client->saber[k];
		}
		else
		{
			pm.saber[k] = nullptr;
		}
		k++;
	}
#endif

    // I'll just do this every frame in case the scale changes in realtime (don't need to update the g2 inst for that)
    VectorCopy(ent->modelScale, pmove.modelScale);
    // rww end bgghoul2

    pmove.gametype = level.gametype;
    pmove.debugMelee = g_debugMelee.integer;
    pmove.stepSlideFix = g_stepSlideFix.integer;

    pmove.noSpecMove = g_noSpecMove.integer;

    pmove.nonHumanoid = (ent->localAnimIndex > 0);

    VectorCopy(client->ps.origin, client->oldOrigin);

    /*
    if (level.intermissionQueued != 0 && g_singlePlayer.integer) {
        if ( level.time - level.intermissionQueued >= 1000  ) {
            pm.cmd.buttons = buttonFlags_t::BUTTON_NONE;
            pm.cmd.forwardmove = 0;
            pm.cmd.rightmove = 0;
            pm.cmd.upmove = 0;
            if ( level.time - level.intermissionQueued >= 2000 && level.time - level.intermissionQueued <= 2500 ) {
                trap->SendConsoleCommand( cbufExec_t::EXEC_APPEND, "centerview\n");
            }
            ent->client->ps.pm_type = pmtype_t::PM_SPINTERMISSION;
        }
    }
    */

    // Set up bg entity data
    pmove.baseEnt = (bgEntity_t*)g_entities;
    pmove.entSize = sizeof(gentity_t);

    if (ent->client->ps.saberLockTime > level.time) {
        gentity_t* blockOpp = &g_entities[ent->client->ps.saberLockEnemy];

        if (blockOpp && blockOpp->inuse && blockOpp->client) {
            vec3_t lockDir, lockAng;

            // VectorClear( ent->client->ps.velocity );
            VectorSubtract(blockOpp->r.currentOrigin, ent->r.currentOrigin, lockDir);
            // lockAng[YAW] = vectoyaw( defDir );
            vectoangles(lockDir, lockAng);
            SetClientViewAngle(ent, lockAng);
        }

        if (ent->client->ps.saberLockHitCheckTime < level.time) { // have moved to next frame since last lock push
            ent->client->ps.saberLockHitCheckTime = level.time;   // so we don't push more than once per server frame
            if (Any(ent->client->buttons & buttonFlags_t::BUTTON_ATTACK) && !Any(ent->client->oldbuttons & buttonFlags_t::BUTTON_ATTACK)) {
                if (ent->client->ps.saberLockHitIncrementTime < level.time) { // have moved to next frame since last saberlock attack button press
                    int lockHits = 0;
                    ent->client->ps.saberLockHitIncrementTime = level.time; // so we don't register an attack key press more than once per server frame
                    // NOTE: FP_SABER_OFFENSE level already taken into account in PM_SaberLocked
                    if ((ent->client->ps.fd.forcePowersActive & Bit(forcePowers_t::FP_RAGE))) { // raging: push harder
                        lockHits = 1 + std::to_underlying(ent->client->ps.fd.forcePowerLevel[forcePowers_t::FP_RAGE]);
                    }
                    else { // normal attack
                        switch (ent->client->ps.fd.saberAnimLevel) {
                        case saber_styles_t::SS_FAST:
                            lockHits = 1;
                            break;
                        case saber_styles_t::SS_MEDIUM:
                        case saber_styles_t::SS_TAVION:
                        case saber_styles_t::SS_DUAL:
                        case saber_styles_t::SS_STAFF:
                            lockHits = 2;
                            break;
                        case saber_styles_t::SS_STRONG:
                        case saber_styles_t::SS_DESANN:
                            lockHits = 3;
                            break;
                        default:
                            break;
                        }
                    }
                    if (ent->client->ps.fd.forceRageRecoveryTime > level.time && Q_irand(0, 1)) { // finished raging: weak
                        lockHits -= 1;
                    }
                    lockHits += ent->client->saber[0].lockBonus;
                    if (ent->client->saber[1].model[0] && !ent->client->ps.saberHolstered) {
                        lockHits += ent->client->saber[1].lockBonus;
                    }
                    ent->client->ps.saberLockHits += lockHits;
                    if (g_saberLockRandomNess.integer) {
                        ent->client->ps.saberLockHits += Q_irand(0, g_saberLockRandomNess.integer);
                        if (ent->client->ps.saberLockHits < 0) {
                            ent->client->ps.saberLockHits = 0;
                        }
                    }
                }
            }
            if (ent->client->ps.saberLockHits > 0) {
                if (!ent->client->ps.saberLockAdvance) {
                    ent->client->ps.saberLockHits--;
                }
                ent->client->ps.saberLockAdvance = true;
            }
        }
    }
    else {
        ent->client->ps.saberLockFrame = 0;
        // check for taunt
        if ((pmove.cmd.generic_cmd == std::to_underlying(genCmds_t::GENCMD_ENGAGE_DUEL)) &&
            (level.gametype == gametype_t::GT_DUEL || level.gametype == gametype_t::GT_POWERDUEL)) { // already in a duel, make it a taunt command
            pmove.cmd.buttons |= buttonFlags_t::BUTTON_GESTURE;
        }
    }

    if (ent->s.number >= MAX_CLIENTS) {
        VectorCopy(ent->r.mins, pmove.mins);
        VectorCopy(ent->r.maxs, pmove.maxs);
#if 1
        if (ent->s.NPC_class == class_t::CLASS_VEHICLE && ent->m_pVehicle) {
            if (ent->m_pVehicle->m_pPilot) {                                    // vehicles want to use their last pilot ucmd I guess
                if ((level.time - ent->m_pVehicle->m_ucmd.serverTime) > 2000) { // Previous owner disconnected, maybe
                    ent->m_pVehicle->m_ucmd.serverTime = level.time;
                    ent->client->ps.commandTime = level.time - 100;
                    msec = 100;
                }

                memcpy(&pmove.cmd, &ent->m_pVehicle->m_ucmd, sizeof(usercmd_t));

                // no veh can strafe
                pmove.cmd.rightmove = 0;
                // no crouching or jumping!
                pmove.cmd.upmove = 0;

                // NOTE: button presses were getting lost!
                assert(g_entities[ent->m_pVehicle->m_pPilot->s.number].client);
                pmove.cmd.buttons = (g_entities[ent->m_pVehicle->m_pPilot->s.number].client->pers.cmd.buttons &
                                     (buttonFlags_t::BUTTON_ATTACK | buttonFlags_t::BUTTON_ALT_ATTACK));
            }
            if (ent->m_pVehicle->m_pVehicleInfo->type == vehicleType_t::VH_WALKER) {
                if (ent->client->ps.groundEntityNum != ENTITYNUM_NONE) { // ATST crushes anything underneath it
                    gentity_t* under = &g_entities[ent->client->ps.groundEntityNum];
                    if (under && under->health && under->takedamage) {
                        vec3_t down = {0, 0, -1};
                        // FIXME: we'll be doing traces down from each foot, so we'll have a real impact origin
                        G_Damage(under, ent, ent, down, under->r.currentOrigin, 100, damageFlags_t::DAMAGE_NORMAL, meansOfDeath_t::MOD_CRUSH);
                    }
                }
            }
        }
#endif
    }

    {
        const saberMoveName_t oldSaberMove = ent->client->ps.saberMove;

        Pmove(&pmove);
        G_StatsSaberMove(ent, oldSaberMove);
    }

    if (ent->client->solidHack) {
        if (ent->client->solidHack > level.time) { // whee!
            ent->r.contents = contentsFlags_t::CONTENTS_NONE;
        }
        else {
            ent->r.contents = contentsFlags_t::CONTENTS_BODY;
            ent->client->solidHack = 0;
        }
    }

    if (ent->NPC) {
        VectorCopy(ent->client->ps.viewangles, ent->r.currentAngles);
    }

    if (pmove.checkDuelLoss) {
        if (pmove.checkDuelLoss > 0 && (pmove.checkDuelLoss <= MAX_CLIENTS ||
                                        (pmove.checkDuelLoss < (MAX_GENTITIES - 1) && g_entities[pmove.checkDuelLoss - 1].s.eType == entityType_t::ET_NPC))) {
            gentity_t* clientLost = &g_entities[pmove.checkDuelLoss - 1];

            if (clientLost && clientLost->inuse && clientLost->client && Q_irand(0, 40) > clientLost->health) {
                vec3_t attDir;
                VectorSubtract(ent->client->ps.origin, clientLost->client->ps.origin, attDir);
                VectorNormalize(attDir);

                VectorClear(clientLost->client->ps.velocity);
                clientLost->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_NONE;
                clientLost->client->ps.forceHandExtendTime = 0;

                gGAvoidDismember = 1;
                G_Damage(clientLost, ent, ent, attDir, clientLost->client->ps.origin, 9999, damageFlags_t::DAMAGE_NO_PROTECTION, meansOfDeath_t::MOD_SABER);

                if (clientLost->health < 1) {
                    gGAvoidDismember = 2;
                    G_CheckForDismemberment(clientLost, ent, clientLost->client->ps.origin, 999, (clientLost->client->ps.legsAnim), false);
                }

                gGAvoidDismember = 0;
            }
            else if (clientLost && clientLost->inuse && clientLost->client &&
                     clientLost->client->ps.forceHandExtend != forceHandAnims_t::HANDEXTEND_KNOCKDOWN &&
                     clientLost->client->ps.saberEntityNum) { // if we didn't knock down it was a circle lock. So as punishment, make them lose their saber and
                                                              // go into a proper anim
                saberCheckKnockdown_DuelLoss(&g_entities[clientLost->client->ps.saberEntityNum], clientLost, ent);
            }
        }

        pmove.checkDuelLoss = 0;
    }

    if (pmove.cmd.generic_cmd && (pmove.cmd.generic_cmd != ent->client->lastGenCmd || ent->client->lastGenCmdTime < level.time)) {
        ent->client->lastGenCmd = pmove.cmd.generic_cmd;
        if (pmove.cmd.generic_cmd != std::to_underlying(genCmds_t::GENCMD_FORCE_THROW) &&
            pmove.cmd.generic_cmd != std::to_underlying(genCmds_t::GENCMD_FORCE_PULL)) { // these are the only two where you wouldn't care about a delay between
            ent->client->lastGenCmdTime = level.time + 300;                              // default 100ms debounce between issuing the same command.
        }

        switch (static_cast<genCmds_t>(pmove.cmd.generic_cmd)) {
        case genCmds_t::GENCMD_NONE:
            break;
        case genCmds_t::GENCMD_SABERSWITCH:
            Cmd_ToggleSaber_f(ent);
            break;
        case genCmds_t::GENCMD_ENGAGE_DUEL:
            if (level.gametype == gametype_t::GT_DUEL || level.gametype == gametype_t::GT_POWERDUEL) { // already in a duel, made it a taunt command
            }
            else {
                Cmd_EngageDuel_f(ent);
            }
            break;
        case genCmds_t::GENCMD_FORCE_HEAL:
            ForceHeal(ent);
            break;
        case genCmds_t::GENCMD_FORCE_SPEED:
            ForceSpeed(ent, 0);
            break;
        case genCmds_t::GENCMD_FORCE_THROW:
            ForceThrow(ent, false);
            break;
        case genCmds_t::GENCMD_FORCE_PULL:
            ForceThrow(ent, true);
            break;
        case genCmds_t::GENCMD_FORCE_DISTRACT:
            ForceTelepathy(ent);
            break;
        case genCmds_t::GENCMD_FORCE_RAGE:
            ForceRage(ent);
            break;
        case genCmds_t::GENCMD_FORCE_PROTECT:
            ForceProtect(ent);
            break;
        case genCmds_t::GENCMD_FORCE_ABSORB:
            ForceAbsorb(ent);
            break;
        case genCmds_t::GENCMD_FORCE_HEALOTHER:
            ForceTeamHeal(ent);
            break;
        case genCmds_t::GENCMD_FORCE_FORCEPOWEROTHER:
            ForceTeamForceReplenish(ent);
            break;
        case genCmds_t::GENCMD_FORCE_SEEING:
            ForceSeeing(ent);
            break;
        case genCmds_t::GENCMD_USE_SEEKER:
            if ((ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] & Bit(holdable_t::HI_SEEKER)) &&
                G_ItemUsable(&ent->client->ps, holdable_t::HI_SEEKER)) {
                ItemUse_Seeker(ent);
                G_AddEvent(ent, BG_UseItemEvent(holdable_t::HI_SEEKER), 0);
                ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] &= ~Bit(holdable_t::HI_SEEKER);
            }
            break;
        case genCmds_t::GENCMD_USE_FIELD:
            if ((ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] & Bit(holdable_t::HI_SHIELD)) &&
                G_ItemUsable(&ent->client->ps, holdable_t::HI_SHIELD)) {
                ItemUse_Shield(ent);
                G_AddEvent(ent, BG_UseItemEvent(holdable_t::HI_SHIELD), 0);
                ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] &= ~Bit(holdable_t::HI_SHIELD);
            }
            break;
        case genCmds_t::GENCMD_USE_BACTA:
            if ((ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] & Bit(holdable_t::HI_MEDPAC)) &&
                G_ItemUsable(&ent->client->ps, holdable_t::HI_MEDPAC)) {
                ItemUse_MedPack(ent);
                G_AddEvent(ent, BG_UseItemEvent(holdable_t::HI_MEDPAC), 0);
                ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] &= ~Bit(holdable_t::HI_MEDPAC);
            }
            break;
        case genCmds_t::GENCMD_USE_BACTABIG:
            if ((ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] & Bit(holdable_t::HI_MEDPAC_BIG)) &&
                G_ItemUsable(&ent->client->ps, holdable_t::HI_MEDPAC_BIG)) {
                ItemUse_MedPack_Big(ent);
                G_AddEvent(ent, BG_UseItemEvent(holdable_t::HI_MEDPAC_BIG), 0);
                ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] &= ~Bit(holdable_t::HI_MEDPAC_BIG);
            }
            break;
        case genCmds_t::GENCMD_USE_ELECTROBINOCULARS:
            if ((ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] & Bit(holdable_t::HI_BINOCULARS)) &&
                G_ItemUsable(&ent->client->ps, holdable_t::HI_BINOCULARS)) {
                ItemUse_Binoculars(ent);
                if (ent->client->ps.zoomMode == 0) {
                    G_AddEvent(ent, BG_UseItemEvent(holdable_t::HI_BINOCULARS), 1);
                }
                else {
                    G_AddEvent(ent, BG_UseItemEvent(holdable_t::HI_BINOCULARS), 2);
                }
            }
            break;
        case genCmds_t::GENCMD_ZOOM:
            if ((ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] & Bit(holdable_t::HI_BINOCULARS)) &&
                G_ItemUsable(&ent->client->ps, holdable_t::HI_BINOCULARS)) {
                ItemUse_Binoculars(ent);
                if (ent->client->ps.zoomMode == 0) {
                    G_AddEvent(ent, BG_UseItemEvent(holdable_t::HI_BINOCULARS), 1);
                }
                else {
                    G_AddEvent(ent, BG_UseItemEvent(holdable_t::HI_BINOCULARS), 2);
                }
            }
            break;
        case genCmds_t::GENCMD_USE_SENTRY:
            if ((ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] & Bit(holdable_t::HI_SENTRY_GUN)) &&
                G_ItemUsable(&ent->client->ps, holdable_t::HI_SENTRY_GUN)) {
                ItemUse_Sentry(ent);
                G_AddEvent(ent, BG_UseItemEvent(holdable_t::HI_SENTRY_GUN), 0);
                ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] &= ~Bit(holdable_t::HI_SENTRY_GUN);
            }
            break;
        case genCmds_t::GENCMD_USE_JETPACK:
            if ((ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] & Bit(holdable_t::HI_JETPACK)) &&
                G_ItemUsable(&ent->client->ps, holdable_t::HI_JETPACK)) {
                ItemUse_Jetpack(ent);
                G_AddEvent(ent, BG_UseItemEvent(holdable_t::HI_JETPACK), 0);
                /*
                if (ent->client->ps.zoomMode == 0)
                {
                    G_AddEvent(ent, BG_UseItemEvent( holdable_t::HI_BINOCULARS ), 1);
                }
                else
                {
                    G_AddEvent(ent, BG_UseItemEvent( holdable_t::HI_BINOCULARS ), 2);
                }
                */
            }
            break;
        case genCmds_t::GENCMD_USE_HEALTHDISP:
            if ((ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] & Bit(holdable_t::HI_HEALTHDISP)) &&
                G_ItemUsable(&ent->client->ps, holdable_t::HI_HEALTHDISP)) {
                // ItemUse_UseDisp(ent, holdable_t::HI_HEALTHDISP);
                G_AddEvent(ent, BG_UseItemEvent(holdable_t::HI_HEALTHDISP), 0);
            }
            break;
        case genCmds_t::GENCMD_USE_AMMODISP:
            if ((ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] & Bit(holdable_t::HI_AMMODISP)) &&
                G_ItemUsable(&ent->client->ps, holdable_t::HI_AMMODISP)) {
                // ItemUse_UseDisp(ent, holdable_t::HI_AMMODISP);
                G_AddEvent(ent, BG_UseItemEvent(holdable_t::HI_AMMODISP), 0);
            }
            break;
        case genCmds_t::GENCMD_USE_EWEB:
            if ((ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] & Bit(holdable_t::HI_EWEB)) && G_ItemUsable(&ent->client->ps, holdable_t::HI_EWEB)) {
                ItemUse_UseEWeb(ent);
                G_AddEvent(ent, BG_UseItemEvent(holdable_t::HI_EWEB), 0);
            }
            break;
        case genCmds_t::GENCMD_USE_CLOAK:
            if ((ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] & Bit(holdable_t::HI_CLOAK)) && G_ItemUsable(&ent->client->ps, holdable_t::HI_CLOAK)) {
                if (ent->client->ps.powerups[powerup_t::PW_CLOAKED]) { // decloak
                    Jedi_Decloak(ent);
                }
                else { // cloak
                    Jedi_Cloak(ent);
                }
            }
            break;
        case genCmds_t::GENCMD_SABERATTACKCYCLE:
            Cmd_SaberAttackCycle_f(ent);
            break;
        case genCmds_t::GENCMD_TAUNT:
            G_SetTauntAnim(ent, tauntTypes_t::TAUNT_TAUNT);
            break;
        case genCmds_t::GENCMD_BOW:
            G_SetTauntAnim(ent, tauntTypes_t::TAUNT_BOW);
            break;
        case genCmds_t::GENCMD_MEDITATE:
            G_SetTauntAnim(ent, tauntTypes_t::TAUNT_MEDITATE);
            break;
        case genCmds_t::GENCMD_FLOURISH:
            G_SetTauntAnim(ent, tauntTypes_t::TAUNT_FLOURISH);
            break;
        case genCmds_t::GENCMD_GLOAT:
            G_SetTauntAnim(ent, tauntTypes_t::TAUNT_GLOAT);
            break;
        default:
            break;
        }
    }

    // save results of pmove
    if (ent->client->ps.eventSequence != oldEventSequence) {
        ent->eventTime = level.time;
    }
    // the player's entity state carries its velocity and the time of its command (g_smoothClients 1 of OpenJK, the cvar
    // removed: the form the message codec's position prediction and tables were made for)
    BG_PlayerStateToEntityStateExtraPolate(&ent->client->ps, &ent->s, ent->client->ps.commandTime, false);
    // rww - 12-03-02 - Don't snap the origin of players! It screws prediction all up.

    if (isNPC) {
        ent->s.eType = entityType_t::ET_NPC;
    }

    SendPendingPredictableEvents(&ent->client->ps);

    if (!Any(ent->client->ps.eFlags & entityFlags_t::EF_FIRING)) {
        client->fireHeld = false; // for grapple
    }

    // use the snapped origin for linking so it matches client predicted versions
    VectorCopy(ent->s.pos.trBase, ent->r.currentOrigin);

    if (ent->s.eType != entityType_t::ET_NPC || ent->s.NPC_class != class_t::CLASS_VEHICLE || !ent->m_pVehicle ||
        !Any(ent->m_pVehicle->m_iRemovedSurfaces)) { // let vehicles that are getting broken apart do their own crazy sizing stuff
        VectorCopy(pmove.mins, ent->r.mins);
        VectorCopy(pmove.maxs, ent->r.maxs);
    }

    ent->waterlevel = pmove.waterlevel;
    ent->watertype = pmove.watertype;

    // execute client events
    ClientEvents(ent, oldEventSequence);

    if (pmove.useEvent != entity_event_t::EV_NONE) {
        // TODO: Use
        //		TryUse( ent );
    }
    if (Any(ent->client->pers.cmd.buttons & buttonFlags_t::BUTTON_USE) && ent->client->ps.useDelay < level.time) {
        TryUse(ent);
        ent->client->ps.useDelay = level.time + 100;
    }

    // link entity now, after any personal teleporters have been used
    trap->LinkEntity((sharedEntity_t*)ent);
    if (!ent->client->noclip) {
        G_TouchTriggers(ent);
    }

    // NOTE: now copy the exact origin over otherwise clients can be snapped into solid
    VectorCopy(ent->client->ps.origin, ent->r.currentOrigin);

    // test for solid areas in the AAS file
    //	BotTestAAS(ent->r.currentOrigin);

    // touch other objects
    ClientImpacts(ent, &pmove);

    // save results of triggers and client events
    if (ent->client->ps.eventSequence != oldEventSequence) {
        ent->eventTime = level.time;
    }

    // swap and latch button actions
    client->oldbuttons = client->buttons;
    client->buttons = ucmd->buttons;
    client->latched_buttons |= client->buttons & ~client->oldbuttons;

    //	G_VehicleAttachDroidUnit( ent );

    // Did we kick someone in our pmove sequence?
    if (client->ps.forceKickFlip) {
        gentity_t* faceKicked = &g_entities[client->ps.forceKickFlip - 1];

        if (faceKicked && faceKicked->client && (!OnSameTeam(ent, faceKicked) || g_friendlyFire.integer) &&
            (!faceKicked->client->ps.duelInProgress || faceKicked->client->ps.duelIndex == ent->s.number) &&
            (!ent->client->ps.duelInProgress || ent->client->ps.duelIndex == faceKicked->s.number)) {
            if (faceKicked && faceKicked->client && faceKicked->health && faceKicked->takedamage) { // push them away and do pain
                vec3_t oppDir;
                int strength = (int)VectorNormalize2(client->ps.velocity, oppDir);

                strength *= 0.05;

                VectorScale(oppDir, -1, oppDir);

                G_Damage(faceKicked, ent, ent, oppDir, client->ps.origin, strength, damageFlags_t::DAMAGE_NO_ARMOR, meansOfDeath_t::MOD_MELEE);

                if (faceKicked->client->ps.weapon != weapon_t::WP_SABER || faceKicked->client->ps.fd.saberAnimLevel != saber_styles_t::SS_STRONG ||
                    (!BG_SaberInAttack(faceKicked->client->ps.saberMove) && !PM_SaberInStart(faceKicked->client->ps.saberMove) &&
                     !PM_SaberInReturn(faceKicked->client->ps.saberMove) && !PM_SaberInTransition(faceKicked->client->ps.saberMove))) {
                    if (faceKicked->health > 0 && faceKicked->client->ps.stats[statIndex_t::STAT_HEALTH] > 0 &&
                        faceKicked->client->ps.forceHandExtend != forceHandAnims_t::HANDEXTEND_KNOCKDOWN) {
                        if (BG_KnockDownable(&faceKicked->client->ps) && Q_irand(1, 10) <= 3) { // only actually knock over sometimes, but always do velocity
                                                                                                // hit
                            faceKicked->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_KNOCKDOWN;
                            faceKicked->client->ps.forceHandExtendTime = level.time + 1100;
                            faceKicked->client->ps.forceDodgeAnim = 0; // this toggles between 1 and 0, when it's 1 we should play the get up anim
                        }

                        faceKicked->client->ps.otherKiller = ent->s.number;
                        faceKicked->client->ps.otherKillerTime = level.time + 5000;
                        faceKicked->client->ps.otherKillerDebounceTime = level.time + 100;

                        faceKicked->client->ps.velocity[0] = oppDir[0] * (strength * 40);
                        faceKicked->client->ps.velocity[1] = oppDir[1] * (strength * 40);
                        faceKicked->client->ps.velocity[2] = 200;
                    }
                }

                G_Sound(faceKicked, sound::SoundClass::Weapon, sound::Slot::None, G_SoundIndex(Q_Format("sound/weapons/melee/punch{}", Q_irand(1, 4)).c_str()));
            }
        }

        client->ps.forceKickFlip = 0;
    }

    // check for respawning
    if (client->ps.stats[statIndex_t::STAT_HEALTH] <= 0 && !Any(client->ps.eFlags2 & entityFlags2_t::EF2_HELD_BY_MONSTER) // can't respawn while being eaten
        && ent->s.eType != entityType_t::ET_NPC) {
        // wait for the attack button to be pressed
        if (level.time > client->respawnTime && !gDoSlowMoDuel) {
            // forcerespawn is to prevent users from waiting out powerups
            int forceRes = g_forceRespawn.integer;

            if (level.gametype == gametype_t::GT_POWERDUEL) {
                forceRes = 1;
            }
            else if (level.gametype == gametype_t::GT_SIEGE && g_siegeRespawn.integer) { // wave respawning on
                forceRes = 1;
            }

            if (forceRes > 0 && (level.time - client->respawnTime) > forceRes * 1000) {
                ClientRespawn(ent);
                return;
            }

            // pressing attack or use is the normal respawn method
            if (Any(ucmd->buttons & (buttonFlags_t::BUTTON_ATTACK | buttonFlags_t::BUTTON_USE_HOLDABLE))) {
                ClientRespawn(ent);
            }
        }
        else if (gDoSlowMoDuel) {
            client->respawnTime = level.time + 1000;
        }
        return;
    }

    // perform once-a-second actions
    ClientTimerActions(ent, msec);

    G_UpdateClientBroadcasts(ent);

    // try some idle anims on ent if getting no input and not moving for some time
    G_CheckClientIdle(ent, ucmd);

    // This code was moved here from clientThink to fix a problem with g_synchronousClients
    // being set to 1 when in vehicles.
    if (ent->s.number < MAX_CLIENTS && ent->client->ps.m_iVehicleNum) { // driving a vehicle
        // run it
        if (g_entities[ent->client->ps.m_iVehicleNum].inuse && g_entities[ent->client->ps.m_iVehicleNum].client) {
            ClientThink(ent->client->ps.m_iVehicleNum, &g_entities[ent->client->ps.m_iVehicleNum].m_pVehicle->m_ucmd);
        }
        else { // vehicle no longer valid?
            ent->client->ps.m_iVehicleNum = 0;
        }
    }
}

/*
==================
G_CheckClientTimeouts

Checks whether a client has exceded any timeouts and act accordingly
==================
*/
void G_CheckClientTimeouts(gentity_t* ent)
{
    // Only timeout supported right now is the timeout to spectator mode
    if (!g_timeouttospec.integer) {
        return;
    }

    // Already a spectator, no need to boot them to spectator
    if (ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR) {
        return;
    }

    // See how long its been since a command was received by the client and if its
    // longer than the timeout to spectator then force this client into spectator mode
    if (level.time - ent->client->pers.cmd.serverTime > g_timeouttospec.integer * 1000) {
        SetTeam(ent, "spectator");
    }
}

/*
==================
ClientThink

A new command has arrived from the client
==================
*/
void ClientThink(int clientNum, usercmd_t* ucmd)
{
    gentity_t* ent;

    ent = g_entities + clientNum;
    if (clientNum < MAX_CLIENTS) {
        trap->GetUsercmd(clientNum, &ent->client->pers.cmd);
    }

    // mark the time we got info, so we can display the
    // phone jack if they don't get any for a while
    ent->client->lastCmdTime = level.time;

    if (ucmd) {
        ent->client->pers.cmd = *ucmd;
    }

    /* 	This was moved to clientthink_real, but since its sort of a risky change i left it here for
        now as a more concrete reference - BSD

        if ( clientNum < MAX_CLIENTS
            && ent->client->ps.m_iVehicleNum )
        {//driving a vehicle
            if (g_entities[ent->client->ps.m_iVehicleNum].client)
            {
                gentity_t *veh = &g_entities[ent->client->ps.m_iVehicleNum];

                if (veh->m_pVehicle &&
                    veh->m_pVehicle->m_pPilot == (bgEntity_t *)ent)
                { //only take input from the pilot...
                    veh->client->ps.commandTime = ent->client->ps.commandTime;
                    memcpy(&veh->m_pVehicle->m_ucmd, &ent->client->pers.cmd, sizeof(usercmd_t));
                    if ( Any( veh->m_pVehicle->m_ucmd.buttons & buttonFlags_t::BUTTON_TALK ) )
                    { //forced input if "chat bubble" is up
                        veh->m_pVehicle->m_ucmd.buttons = buttonFlags_t::BUTTON_TALK;
                        veh->m_pVehicle->m_ucmd.forwardmove = 0;
                        veh->m_pVehicle->m_ucmd.rightmove = 0;
                        veh->m_pVehicle->m_ucmd.upmove = 0;
                    }
                }
            }
        }
    */
    // every command runs when the engine gives it: a player's at its tick (the last one again when none came), a bot's
    // as it is made
    ClientThink_real(ent);

    /*	This was moved to clientthink_real, but since its sort of a risky change i left it here for
        now as a more concrete reference - BSD

        if ( clientNum < MAX_CLIENTS
            && ent->client->ps.m_iVehicleNum )
        {//driving a vehicle
            //run it
            if (g_entities[ent->client->ps.m_iVehicleNum].inuse &&
                g_entities[ent->client->ps.m_iVehicleNum].client)
            {
                ClientThink(ent->client->ps.m_iVehicleNum, &g_entities[ent->client->ps.m_iVehicleNum].m_pVehicle->m_ucmd);
            }
            else
            { //vehicle no longer valid?
                ent->client->ps.m_iVehicleNum = 0;
            }
        }
    */
}

/*
==================
SpectatorClientEndFrame

==================
*/
void SpectatorClientEndFrame(gentity_t* ent)
{
    gclient_t* cl;

    if (ent->s.eType == entityType_t::ET_NPC) {
        assert(0);
        return;
    }

    // if we are doing a chase cam or a remote view, grab the latest info
    if (ent->client->sess.spectatorState == spectatorState_t::SPECTATOR_FOLLOW) {
        int clientNum; //, flags;

        clientNum = ent->client->sess.spectatorClient;

        // team follow1 and team follow2 go to whatever clients are playing
        if (clientNum == -1) {
            clientNum = level.follow1;
        }
        else if (clientNum == -2) {
            clientNum = level.follow2;
        }
        if (clientNum >= 0) {
            cl = &level.clients[clientNum];
            if (cl->pers.connected == clientConnected_t::CON_CONNECTED && cl->sess.sessionTeam != team_t::TEAM_SPECTATOR) {
                // flags = (cl->mGameFlags & ~(voteFlags_t::PSG_VOTED | voteFlags_t::PSG_TEAMVOTED)) | (ent->client->mGameFlags & (voteFlags_t::PSG_VOTED |
                // voteFlags_t::PSG_TEAMVOTED)); ent->client->mGameFlags = flags;
                ent->client->ps.eFlags = cl->ps.eFlags;
                ent->client->ps = cl->ps;
                ent->client->ps.pm_flags |= pmoveFlags_t::PMF_FOLLOW;
                return;
            }
            else {
                // drop them to free spectators unless they are dedicated camera followers
                if (ent->client->sess.spectatorClient >= 0) {
                    ent->client->sess.spectatorState = spectatorState_t::SPECTATOR_FREE;
                    ClientBegin(ent->client - level.clients, true);
                }
            }
        }
    }

    if (ent->client->sess.spectatorState == spectatorState_t::SPECTATOR_SCOREBOARD) {
        ent->client->ps.pm_flags |= pmoveFlags_t::PMF_SCOREBOARD;
    }
    else {
        ent->client->ps.pm_flags &= ~pmoveFlags_t::PMF_SCOREBOARD;
    }
}

/*
==============
ClientEndFrame

Called at the end of each server frame for each connected client
A fast client will have multiple ClientThink for each ClientEdFrame,
while a slow client may have multiple ClientEndFrame between ClientThink.
==============
*/
void ClientEndFrame(gentity_t* ent)
{
    bool isNPC = false;

    if (ent->s.eType == entityType_t::ET_NPC) {
        isNPC = true;
    }

    if (ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR) {
        SpectatorClientEndFrame(ent);
        return;
    }

    // turn off any expired powerups (on the 20 Hz instants of the tick, as at sv_fps 20)
    if (G_GRID20_STEP) {
        for (int& until : ent->client->ps.powerups) {
            if (until < level.time) {
                until = 0;
            }
        }
    }

    // save network bandwidth
#if 0
	if ( !g_synchronousClients->integer && (ent->client->ps.pm_type == pmtype_t::PM_NORMAL || ent->client->ps.pm_type == pmtype_t::PM_JETPACK || ent->client->ps.pm_type == pmtype_t::PM_FLOAT) ) {
		// FIXME: this must change eventually for non-sync demo recording
		VectorClear( ent->client->ps.viewangles );
	}
#endif

    //
    // If the end of unit layout is displayed, don't give
    // the player any normal movement attributes
    //
    if (level.intermissiontime) {
        if (ent->s.number < MAX_CLIENTS || ent->client->NPC_class == class_t::CLASS_VEHICLE) { // players and vehicles do nothing in intermissions
            return;
        }
    }

    if (G_GRID20_STEP) { // on the 20 Hz instants of the tick: the damage of the ticks between adds up, as in one frame of sv_fps 20
        // burn from lava, etc
        P_WorldEffects(ent);

        // apply all the damage taken this frame
        P_DamageFeedback(ent);
    }

    // add the EF_CONNECTION flag if we haven't gotten commands recently
    if (level.time - ent->client->lastCmdTime > 1000)
        ent->client->ps.eFlags |= entityFlags_t::EF_CONNECTION;
    else
        ent->client->ps.eFlags &= ~entityFlags_t::EF_CONNECTION;

    ent->client->ps.stats[statIndex_t::STAT_HEALTH] = ent->health; // FIXME: get rid of ent->health...

    G_SetClientSound(ent);

    // set the latest infor
    // the player's entity state carries its velocity and the time of its command (g_smoothClients 1 of OpenJK, the cvar
    // removed: the form the message codec's position prediction and tables were made for)
    BG_PlayerStateToEntityStateExtraPolate(&ent->client->ps, &ent->s, ent->client->ps.commandTime, false);
    // rww - 12-03-02 - Don't snap the origin of players! It screws prediction all up.

    if (isNPC) {
        ent->s.eType = entityType_t::ET_NPC;
    }

    SendPendingPredictableEvents(&ent->client->ps);

    // set the bit for the reachability area the client is currently in
    //	i = trap->AAS_PointReachabilityAreaIndex( ent->client->ps.origin );
    //	ent->client->areabits[i >> 3] |= 1 << (i & 7);
}

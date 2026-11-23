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

// g_sound.cpp -- the sounds the game module sends: a sound event at an entity or a point, with the sound's class and the
// slot of its entity it takes (packed by BG_PackSoundEvent in the field the cgame reads), a force power's sound on its
// tracker, a slot or an entity's sounds stopped, an NPC's speech event.

#include "g_local.h"

/*
=================
G_SoundTempEntity

Special event entity that keeps sound trackers in mind
=================
*/
gentity_t* G_SoundTempEntity(vec3_t origin, entity_event_t event)
{
    gentity_t* e;
    vec3_t snapped;

    e = G_Spawn();

    e->s.eType = BG_EventEntityType(BG_PackEvent(event));
    e->inuse = true;

    e->classname = "tempEntity";
    e->eventTime = level.time;
    e->freeAfterEvent = true;

    VectorCopy(origin, snapped);
    SnapVector(snapped); // save network bandwidth
    G_SetOrigin(e, snapped);

    // find cluster for PVS
    trap->LinkEntity((sharedEntity_t*)e);

    return e;
}

/*
=============
G_MuteSound

The sound in an entity's slot stopped, or every sound of the entity without a slot (a sound tracker's)
=============
*/
void G_MuteSound(int entnum, sound::Slot slot)
{
    gentity_t *te, *e;

    te = G_TempEntity(vec3_origin, entity_event_t::EV_MUTE_SOUND);
    te->r.svFlags = svFlags_t::SVF_BROADCAST;
    te->s.trickedentindex2 = entnum;
    te->s.trickedentindex = BG_PackSoundEvent({.slot = slot});

    e = &g_entities[entnum];

    if (e && Any(e->s.eFlags & entityFlags_t::EF_SOUNDTRACKER)) {
        G_FreeEntity(e);
        e->s.eFlags = entityFlags_t::EF_NONE;
    }
}

/*
=============
G_Sound
=============
*/
void G_Sound(gentity_t* ent, sound::SoundClass soundClass, sound::Slot slot, int soundIndex)
{
    gentity_t* te;

    assert(soundIndex);

    te = G_SoundTempEntity(ent->r.currentOrigin, entity_event_t::EV_GENERAL_SOUND);
    te->s.eventParm = soundIndex;
    te->s.saberEntityNum = BG_PackSoundEvent({.soundClass = soundClass, .slot = slot});
}

/*
=============
G_Sound

On a tracking channel (the force powers' sounds), which the client may stop; the channels of speed, rage and sight
loop until they are stopped
=============
*/
void G_Sound(gentity_t* ent, trackchan_t channel, int soundIndex)
{
    gentity_t* te;

    assert(soundIndex);

    te = G_SoundTempEntity(ent->r.currentOrigin, entity_event_t::EV_GENERAL_SOUND);
    te->s.eventParm = soundIndex;
    te->s.saberEntityNum = BG_PackSoundEvent(
        {.soundClass = sound::SoundClass::Force,
         .loop = channel == trackchan_t::TRACK_CHANNEL_2 || channel == trackchan_t::TRACK_CHANNEL_3 || channel == trackchan_t::TRACK_CHANNEL_5});

    if (ent && ent->client &&
        channel > trackchan_t::TRACK_CHANNEL_NONE) { // let the client remember the index of the player entity so he can kill the most recent sound on request
        if (g_entities[ent->client->ps.fd.killSoundEntIndex[channel - trackchan_t::TRACK_CHANNEL_NONE]].inuse &&
            ent->client->ps.fd.killSoundEntIndex[channel - trackchan_t::TRACK_CHANNEL_NONE] > MAX_CLIENTS) {
            G_MuteSound(ent->client->ps.fd.killSoundEntIndex[channel - trackchan_t::TRACK_CHANNEL_NONE], sound::Slot::None);
            if (ent->client->ps.fd.killSoundEntIndex[channel - trackchan_t::TRACK_CHANNEL_NONE] > MAX_CLIENTS &&
                g_entities[ent->client->ps.fd.killSoundEntIndex[channel - trackchan_t::TRACK_CHANNEL_NONE]].inuse) {
                G_FreeEntity(&g_entities[ent->client->ps.fd.killSoundEntIndex[channel - trackchan_t::TRACK_CHANNEL_NONE]]);
            }
            ent->client->ps.fd.killSoundEntIndex[channel - trackchan_t::TRACK_CHANNEL_NONE] = 0;
        }

        ent->client->ps.fd.killSoundEntIndex[channel - trackchan_t::TRACK_CHANNEL_NONE] = te->s.number;
        te->s.trickedentindex = ent->s.number;
        te->s.eFlags = entityFlags_t::EF_SOUNDTRACKER;
        // fix: let other players know about this
        // for case that they will meet this one
        te->r.svFlags |= svFlags_t::SVF_BROADCAST;
        // te->freeAfterEvent = false;
    }
}

/*
=============
G_SoundAtLoc
=============
*/
void G_SoundAtLoc(vec3_t loc, sound::SoundClass soundClass, sound::Slot slot, int soundIndex)
{
    gentity_t* te;

    te = G_TempEntity(loc, entity_event_t::EV_GENERAL_SOUND);
    te->s.eventParm = soundIndex;
    te->s.saberEntityNum = BG_PackSoundEvent({.soundClass = soundClass, .slot = slot});
}

/*
=============
G_EntitySound
=============
*/
void G_EntitySound(gentity_t* ent, sound::SoundClass soundClass, sound::Slot slot, int soundIndex)
{
    gentity_t* te;

    te = G_TempEntity(ent->r.currentOrigin, entity_event_t::EV_ENTITY_SOUND);
    te->s.eventParm = soundIndex;
    te->s.clientNum = ent->s.number;
    te->s.trickedentindex = BG_PackSoundEvent({.soundClass = soundClass, .slot = slot});
}

// To make porting from SP easier.
void G_SoundOnEnt(gentity_t* ent, sound::SoundClass soundClass, sound::Slot slot, const char* soundPath)
{
    gentity_t* te;

    te = G_TempEntity(ent->r.currentOrigin, entity_event_t::EV_ENTITY_SOUND);
    te->s.eventParm = G_SoundIndex((char*)soundPath);
    te->s.clientNum = ent->s.number;
    te->s.trickedentindex = BG_PackSoundEvent({.soundClass = soundClass, .slot = slot});
}

void G_SpeechEvent(gentity_t* self, entity_event_t event)
{
    G_AddEvent(self, event, 0);
}

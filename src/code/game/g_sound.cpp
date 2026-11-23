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

// g_sound.cpp -- the sounds the game module plays: through the cgame at once (the game and the cgame share the module), a
// sound at an entity with its class and the slot it takes, an NPC's lines chosen by their speech event; or as an event, a
// sound at an entity or a point with its class, or a broadcast heard at full volume.

#include <cgame/cg_local.h>
#include "Q3_Interface.h"
#include "g_local.h"
#include "g_functions.h"

//===Bypass network for sounds on specific channels====================

#include <cgame/cg_media.h> //access to cgs
extern bool CG_TryPlayCustomSound(vec3_t origin, int entityNum, sound::SoundClass soundClass, sound::Slot slot, const char* soundName,
                                  customSoundSet_t customSoundSet);
extern cvar_t* g_timescale;
// NOTE: Do NOT Try to use this before the cgame DLL is valid, it will NOT work!
void G_SoundOnEnt(gentity_t* ent, sound::SoundClass soundClass, sound::Slot slot, const char* soundPath)
{
    int index = G_SoundIndex((char*)soundPath);

    if (!ent) {
        return;
    }
    if (g_timescale->integer > 50) { // Skip the sound!
        return;
    }

    CG_SetSoundPosition(ent->s.number, ent->currentOrigin);
    if (cgs.sound_precache[index]) {
        CG_StartSound(nullptr, ent->s.number, soundClass, slot, cgs.sound_precache[index]);
    }
    else {
        CG_TryPlayCustomSound(nullptr, ent->s.number, soundClass, slot, soundPath, customSoundSet_t::CS_TRY_ALL);
    }
}

void G_SoundIndexOnEnt(gentity_t* ent, sound::SoundClass soundClass, sound::Slot slot, int index)
{
    if (!ent) {
        return;
    }

    CG_SetSoundPosition(ent->s.number, ent->currentOrigin);
    if (cgs.sound_precache[index]) {
        CG_StartSound(nullptr, ent->s.number, soundClass, slot, cgs.sound_precache[index]);
    }
}

extern cvar_t* g_skippingcin;
void G_SpeechEvent(gentity_t* self, entity_event_t event)
{
    if (in_camera && g_skippingcin && g_skippingcin->integer) { // Skipping a cinematic, so skip NPC voice sounds...
        return;
    }
    // update entity pos, too
    CG_SetSoundPosition(self->s.number, self->currentOrigin);
    switch (event) {
    case entity_event_t::EV_ANGER1: // Say when acquire an enemy when didn't have one before
    case entity_event_t::EV_ANGER2:
    case entity_event_t::EV_ANGER3:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*anger{}.wav", event - entity_event_t::EV_ANGER1 + 1).c_str(), customSoundSet_t::CS_COMBAT);
        break;
    case entity_event_t::EV_VICTORY1: // Say when killed an enemy
    case entity_event_t::EV_VICTORY2:
    case entity_event_t::EV_VICTORY3:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*victory{}.wav", event - entity_event_t::EV_VICTORY1 + 1).c_str(), customSoundSet_t::CS_COMBAT);
        break;
    case entity_event_t::EV_CONFUSE1: // Say when confused
    case entity_event_t::EV_CONFUSE2:
    case entity_event_t::EV_CONFUSE3:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*confuse{}.wav", event - entity_event_t::EV_CONFUSE1 + 1).c_str(), customSoundSet_t::CS_COMBAT);
        break;
    case entity_event_t::EV_PUSHED1: // Say when pushed
    case entity_event_t::EV_PUSHED2:
    case entity_event_t::EV_PUSHED3:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*pushed{}.wav", event - entity_event_t::EV_PUSHED1 + 1).c_str(), customSoundSet_t::CS_COMBAT);
        break;
    case entity_event_t::EV_CHOKE1: // Say when choking
    case entity_event_t::EV_CHOKE2:
    case entity_event_t::EV_CHOKE3:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*choke{}.wav", event - entity_event_t::EV_CHOKE1 + 1).c_str(), customSoundSet_t::CS_COMBAT);
        break;
    case entity_event_t::EV_FFWARN: // Warn ally to stop shooting you
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice, "*ffwarn.wav", customSoundSet_t::CS_COMBAT);
        break;
    case entity_event_t::EV_FFTURN: // Turn on ally after being shot by them
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice, "*ffturn.wav", customSoundSet_t::CS_COMBAT);
        break;
    // extra sounds for ST
    case entity_event_t::EV_CHASE1:
    case entity_event_t::EV_CHASE2:
    case entity_event_t::EV_CHASE3:
        if (!CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                                   Q_Format("*chase{}.wav", event - entity_event_t::EV_CHASE1 + 1).c_str(), customSoundSet_t::CS_EXTRA)) {
            CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice, Q_Format("*anger{}.wav", Q_irand(1, 3)).c_str(),
                                  customSoundSet_t::CS_COMBAT);
        }
        break;
    case entity_event_t::EV_COVER1:
    case entity_event_t::EV_COVER2:
    case entity_event_t::EV_COVER3:
    case entity_event_t::EV_COVER4:
    case entity_event_t::EV_COVER5:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*cover{}.wav", event - entity_event_t::EV_COVER1 + 1).c_str(), customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_DETECTED1:
    case entity_event_t::EV_DETECTED2:
    case entity_event_t::EV_DETECTED3:
    case entity_event_t::EV_DETECTED4:
    case entity_event_t::EV_DETECTED5:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*detected{}.wav", event - entity_event_t::EV_DETECTED1 + 1).c_str(), customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_GIVEUP1:
    case entity_event_t::EV_GIVEUP2:
    case entity_event_t::EV_GIVEUP3:
    case entity_event_t::EV_GIVEUP4:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*giveup{}.wav", event - entity_event_t::EV_GIVEUP1 + 1).c_str(), customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_LOOK1:
    case entity_event_t::EV_LOOK2:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*look{}.wav", event - entity_event_t::EV_LOOK1 + 1).c_str(), customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_LOST1:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice, "*lost1.wav", customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_OUTFLANK1:
    case entity_event_t::EV_OUTFLANK2:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*outflank{}.wav", event - entity_event_t::EV_OUTFLANK1 + 1).c_str(), customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_ESCAPING1:
    case entity_event_t::EV_ESCAPING2:
    case entity_event_t::EV_ESCAPING3:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*escaping{}.wav", event - entity_event_t::EV_ESCAPING1 + 1).c_str(), customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_SIGHT1:
    case entity_event_t::EV_SIGHT2:
    case entity_event_t::EV_SIGHT3:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*sight{}.wav", event - entity_event_t::EV_SIGHT1 + 1).c_str(), customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_SOUND1:
    case entity_event_t::EV_SOUND2:
    case entity_event_t::EV_SOUND3:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*sound{}.wav", event - entity_event_t::EV_SOUND1 + 1).c_str(), customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_SUSPICIOUS1:
    case entity_event_t::EV_SUSPICIOUS2:
    case entity_event_t::EV_SUSPICIOUS3:
    case entity_event_t::EV_SUSPICIOUS4:
    case entity_event_t::EV_SUSPICIOUS5:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*suspicious{}.wav", event - entity_event_t::EV_SUSPICIOUS1 + 1).c_str(), customSoundSet_t::CS_EXTRA);
        break;
    // extra sounds for Jedi
    case entity_event_t::EV_COMBAT1:
    case entity_event_t::EV_COMBAT2:
    case entity_event_t::EV_COMBAT3:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*combat{}.wav", event - entity_event_t::EV_COMBAT1 + 1).c_str(), customSoundSet_t::CS_JEDI);
        break;
    case entity_event_t::EV_JDETECTED1:
    case entity_event_t::EV_JDETECTED2:
    case entity_event_t::EV_JDETECTED3:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*jdetected{}.wav", event - entity_event_t::EV_JDETECTED1 + 1).c_str(), customSoundSet_t::CS_JEDI);
        break;
    case entity_event_t::EV_TAUNT1:
    case entity_event_t::EV_TAUNT2:
    case entity_event_t::EV_TAUNT3:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*taunt{}.wav", event - entity_event_t::EV_TAUNT1 + 1).c_str(), customSoundSet_t::CS_JEDI);
        break;
    case entity_event_t::EV_JCHASE1:
    case entity_event_t::EV_JCHASE2:
    case entity_event_t::EV_JCHASE3:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*jchase{}.wav", event - entity_event_t::EV_JCHASE1 + 1).c_str(), customSoundSet_t::CS_JEDI);
        break;
    case entity_event_t::EV_JLOST1:
    case entity_event_t::EV_JLOST2:
    case entity_event_t::EV_JLOST3:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*jlost{}.wav", event - entity_event_t::EV_JLOST1 + 1).c_str(), customSoundSet_t::CS_JEDI);
        break;
    case entity_event_t::EV_DEFLECT1:
    case entity_event_t::EV_DEFLECT2:
    case entity_event_t::EV_DEFLECT3:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*deflect{}.wav", event - entity_event_t::EV_DEFLECT1 + 1).c_str(), customSoundSet_t::CS_JEDI);
        break;
    case entity_event_t::EV_GLOAT1:
    case entity_event_t::EV_GLOAT2:
    case entity_event_t::EV_GLOAT3:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice,
                              Q_Format("*gloat{}.wav", event - entity_event_t::EV_GLOAT1 + 1).c_str(), customSoundSet_t::CS_JEDI);
        break;
    case entity_event_t::EV_PUSHFAIL:
        CG_TryPlayCustomSound(nullptr, self->s.number, sound::SoundClass::Voice, sound::Slot::Voice, "*pushfail.wav", customSoundSet_t::CS_JEDI);
        break;
    default:
        break;
    }
}

/*
=============
G_Sound
=============
*/
void G_Sound(gentity_t* ent, sound::SoundClass soundClass, int soundIndex)
{
    gentity_t* te;

    te = G_TempEntity(ent->currentOrigin, entity_event_t::EV_GENERAL_SOUND);
    te->s.eventParm = soundIndex;
    te->s.otherEntityNum2 = BG_PackSoundEvent({.soundClass = soundClass});
}

/*
=============
G_Sound
=============
*/
void G_SoundAtSpot(vec3_t org, sound::SoundClass soundClass, int soundIndex, bool broadcast)
{
    gentity_t* te;

    te = G_TempEntity(org, entity_event_t::EV_GENERAL_SOUND);
    te->s.eventParm = soundIndex;
    te->s.otherEntityNum2 = BG_PackSoundEvent({.soundClass = soundClass});
    if (broadcast) {
        te->svFlags |= svFlags_t::SVF_BROADCAST;
    }
}

/*
=============
G_SoundBroadcast

  Plays sound that can permeate PVS blockage
=============
*/
void G_SoundBroadcast(gentity_t* ent, int soundIndex)
{
    gentity_t* te;

    te = G_TempEntity(ent->currentOrigin, entity_event_t::EV_GLOBAL_SOUND); // full volume
    te->s.eventParm = soundIndex;
    te->svFlags |= svFlags_t::SVF_BROADCAST;
}

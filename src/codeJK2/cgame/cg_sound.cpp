/*
===========================================================================
Copyright (C) 2026, ModernJK contributors

This file is part of the ModernJK source code.

ModernJK is free software; you can redistribute it and/or modify it
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

// cg_sound.cpp -- the sounds the single player's cgame and game play, over the engine's sound (gi.sound): a sound at a
// point or following an entity, a sound at the listener, the looping sounds asked for at every frame and kept playing by
// their handle, the entities' positions and velocities, the listener. Nothing plays before the cgame starts, as the
// engine refused the sounds the game asked for while it spawned a level.

#include "cg_local.h"

#include <unordered_map>
#include <vector>

namespace {

// a looping sound asked for: the voice playing it, kept while it is asked for at every frame
struct loopingSound_t {
    int entityNum;
    sfxHandle_t sfx;
    sound::SoundClass soundClass;
    sound::VoiceHandle voice;
    vec3_t origin;
    float volume;
    bool asked; // asked for since the last update
};

std::vector<loopingSound_t> loopingSounds;
bool soundsStarted = false;

// each blade's contacts with the world, keyed by its entity, saber and blade
std::unordered_map<int, contactSound_t> contactSounds;

int BladeKey(int entityNum, int saberNum, int bladeNum)
{
    return (((entityNum * MAX_SABERS) + saberNum) * MAX_BLADES) + bladeNum;
}

// the creatures whose voices are heard from near, as the original game's EAX path played them: the howler's and the
// sand creature's
bool IsQuietVoice(int entityNum)
{
    if (entityNum < 0 || entityNum >= ENTITYNUM_WORLD) {
        return false;
    }

    const gentity_t* gent = cg_entities[entityNum].gent;
    return gent && gent->NPC_type && (Q_stristr(gent->NPC_type, "howler") || Q_stristr(gent->NPC_type, "sand_creature"));
}

} // namespace

void CG_StartSounds(bool started)
{
    soundsStarted = started;
}

sound::Vector CG_SoundVector(const vec3_t v)
{
    return {v[0], v[1], v[2]};
}

void CG_EntitySoundVelocity(const centity_t* cent, vec3_t velocity)
{
    if (cent->gent && cent->gent->client) {
        VectorCopy(cent->gent->client->ps.velocity, velocity);
    }
    else {
        EvaluateTrajectoryDelta(&cent->currentState.pos, cg.time, velocity);
    }
}

void CG_SetSoundPosition(int entityNum, const vec3_t origin)
{
    vec3_t velocity = {0, 0, 0};

    if (entityNum >= 0 && entityNum < ENTITYNUM_NONE) {
        CG_EntitySoundVelocity(&cg_entities[entityNum], velocity);
    }

    gi.sound->UpdateEntity(entityNum, CG_SoundVector(origin), CG_SoundVector(velocity));
}

sound::VoiceHandle CG_StartSound(const vec3_t origin, int entityNum, sound::SoundClass soundClass, sound::Slot slot, sfxHandle_t sfx, float volume)
{
    if (!soundsStarted) {
        return sound::VoiceHandle::None;
    }

    sound::Request request;
    request.sound = sfx;
    request.soundClass = soundClass == sound::SoundClass::Voice && IsQuietVoice(entityNum) ? sound::SoundClass::VoiceQuiet : soundClass;
    request.slot = slot;
    request.volume = volume;

    if (entityNum >= 0 && entityNum < ENTITYNUM_NONE) {
        request.entity = entityNum;
    }

    if (origin) {
        request.place = sound::Place::Point;
        request.origin = CG_SoundVector(origin);
    }
    else {
        request.place = sound::Place::Entity;
    }

    return gi.sound->Play(request);
}

sound::VoiceHandle CG_StartLocalSound(sfxHandle_t sfx, sound::SoundClass soundClass, sound::Slot slot, float volume)
{
    if (!soundsStarted) {
        return sound::VoiceHandle::None;
    }

    sound::Request request;
    request.sound = sfx;
    request.soundClass = soundClass;
    request.place = sound::Place::Listener;
    request.slot = slot;
    request.volume = volume;
    return gi.sound->Play(request);
}

// the same sound asked for twice on one entity in a frame is two loops: each request takes the first of that entity's
// loops of that sound not yet asked for in the frame
void CG_AddLoopingSound(int entityNum, const vec3_t origin, sfxHandle_t sfx, sound::SoundClass soundClass, float volume)
{
    if (sfx == sound::kNoSound || !soundsStarted) {
        return;
    }

    for (loopingSound_t& loop : loopingSounds) {
        if (!loop.asked && loop.entityNum == entityNum && loop.sfx == sfx) {
            VectorCopy(origin, loop.origin);
            loop.soundClass = soundClass;
            loop.volume = volume;
            loop.asked = true;
            return;
        }
    }

    loopingSound_t loop{
        .entityNum = entityNum, .sfx = sfx, .soundClass = soundClass, .voice = sound::VoiceHandle::None, .origin = {}, .volume = volume, .asked = true};
    VectorCopy(origin, loop.origin);
    loopingSounds.push_back(loop);
}

// the loops heard from their point as the old mixer played them, moving at their entity's velocity; a loop the engine
// stopped (a level's end, the sound restarted) started again
void CG_UpdateLoopingSounds(void)
{
    for (std::size_t i = 0; i < loopingSounds.size();) {
        loopingSound_t& loop = loopingSounds[i];

        if (!loop.asked) {
            gi.sound->Stop(loop.voice);
            loopingSounds.erase(loopingSounds.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }

        vec3_t velocity = {0, 0, 0};

        if (loop.entityNum >= 0 && loop.entityNum < ENTITYNUM_NONE) {
            CG_EntitySoundVelocity(&cg_entities[loop.entityNum], velocity);
        }

        if (!gi.sound->IsPlaying(loop.voice)) {
            sound::Request request;
            request.sound = loop.sfx;
            request.soundClass = loop.soundClass;
            request.place = sound::Place::Point;
            request.origin = CG_SoundVector(loop.origin);
            request.loop = true;
            request.volume = loop.volume;
            loop.voice = gi.sound->Play(request);
        }

        gi.sound->MoveLoop(loop.voice, CG_SoundVector(loop.origin), CG_SoundVector(velocity));
        loop.asked = false;
        ++i;
    }
}

void CG_StartContactSound(int entityNum, int saberNum, int bladeNum, const vec3_t origin)
{
    BG_ContactSound(
        contactSounds[BladeKey(entityNum, saberNum, bladeNum)], cg.time, [](sound::VoiceHandle voice) { return gi.sound->IsPlaying(voice); },
        [origin](float volume) {
            return CG_StartSound(origin, -1, sound::SoundClass::Saber, sound::Slot::None,
                                 gi.sound->Register(Q_Format("sound/weapons/saber/saberhitwall{}.wav", Q_irand(1, 3)).c_str()), volume);
        });
}

void CG_EndContactSound(int entityNum, int saberNum, int bladeNum)
{
    const auto contact = contactSounds.find(BladeKey(entityNum, saberNum, bladeNum));

    if (contact != contactSounds.end()) {
        contact->second.touching = false;
    }
}

void CG_StopLoopingSounds(void)
{
    for (const loopingSound_t& loop : loopingSounds) {
        gi.sound->Stop(loop.voice);
    }

    loopingSounds.clear();
    contactSounds.clear();
}

void CG_SetSoundListener(int entityNum, const vec3_t origin, const vec3_t axis[3], bool underwater)
{
    sound::Listener listener;
    listener.entity = entityNum;
    listener.origin = CG_SoundVector(origin);
    listener.axes = {CG_SoundVector(axis[0]), CG_SoundVector(axis[1]), CG_SoundVector(axis[2])};
    listener.velocity = CG_SoundVector(cg.predicted_player_state.velocity);
    listener.underwater = underwater;
    gi.sound->SetListener(listener);
}

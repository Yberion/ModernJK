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

// cg_sound.cpp -- the sounds cgame plays, over the engine's sound (trap->sound): a sound at a point or following an
// entity, a sound at the listener, the looping sounds asked for at every frame and kept playing by their handle, the
// entities' positions and velocities, the listener.

#include "cg_local.h"

#include <vector>

namespace {

// a looping sound asked for: the voice playing it, kept while it is asked for at every frame
struct loopingSound_t {
    int entityNum;
    sfxHandle_t sfx;
    sound::SoundClass soundClass;
    sound::VoiceHandle voice;
    vec3_t origin;
    bool asked; // asked for since the last update
};

std::vector<loopingSound_t> loopingSounds;

// the creatures whose voices are heard from near, as the original game's EAX path played them: the howler's and the
// sand creature's
bool IsQuietVoice(int entityNum)
{
    if (entityNum < MAX_CLIENTS || entityNum >= ENTITYNUM_WORLD) {
        return false;
    }

    const clientInfo_t* ci = cg_entities[entityNum].npcClient;
    return ci && (Q_stristr(ci->modelName, "howler") || Q_stristr(ci->modelName, "sand_creature"));
}

} // namespace

sound::Vector CG_SoundVector(const vec3_t v)
{
    return {v[0], v[1], v[2]};
}

void CG_EntitySoundVelocity(const centity_t* cent, vec3_t velocity)
{
    if (cg.snap && cent->currentState.number == cg.predictedPlayerState.clientNum) {
        VectorCopy(cg.predictedPlayerState.velocity, velocity);
    }
    else if (cent->currentState.pos.trType == trType_t::TR_INTERPOLATE) {
        // the players and the NPCs: their velocity is in the trajectory's delta
        VectorCopy(cent->currentState.pos.trDelta, velocity);
    }
    else {
        BG_EvaluateTrajectoryDelta(&cent->currentState.pos, cg.time, velocity);
    }
}

sound::VoiceHandle CG_StartSound(const vec3_t origin, int entityNum, sound::SoundClass soundClass, sound::Slot slot, sfxHandle_t sfx, float volume)
{
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

    return trap->sound->Play(request);
}

sound::VoiceHandle CG_StartLocalSound(sfxHandle_t sfx, sound::SoundClass soundClass, sound::Slot slot, float volume)
{
    sound::Request request;
    request.sound = sfx;
    request.soundClass = soundClass;
    request.place = sound::Place::Listener;
    request.slot = slot;
    request.volume = volume;
    return trap->sound->Play(request);
}

// the same sound asked for twice on one entity in a frame (a saber's two blades) is two loops: each request takes the
// first of that entity's loops of that sound not yet asked for in the frame
void CG_AddLoopingSound(int entityNum, const vec3_t origin, sfxHandle_t sfx, sound::SoundClass soundClass)
{
    if (sfx == sound::kNoSound) {
        return;
    }

    for (loopingSound_t& loop : loopingSounds) {
        if (!loop.asked && loop.entityNum == entityNum && loop.sfx == sfx) {
            VectorCopy(origin, loop.origin);
            loop.soundClass = soundClass;
            loop.asked = true;
            return;
        }
    }

    loopingSound_t loop{.entityNum = entityNum, .sfx = sfx, .soundClass = soundClass, .voice = sound::VoiceHandle::None, .origin = {}, .asked = true};
    VectorCopy(origin, loop.origin);
    loopingSounds.push_back(loop);
}

// the loops heard from their point as the old mixer played them, the listener's own included, moving at their
// entity's velocity; a loop the engine stopped (a level's end, the sound restarted) started again
void CG_UpdateLoopingSounds(void)
{
    for (std::size_t i = 0; i < loopingSounds.size();) {
        loopingSound_t& loop = loopingSounds[i];

        if (!loop.asked) {
            trap->sound->Stop(loop.voice);
            loopingSounds.erase(loopingSounds.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }

        vec3_t velocity = {0, 0, 0};

        if (loop.entityNum >= 0 && loop.entityNum < ENTITYNUM_NONE) {
            CG_EntitySoundVelocity(&cg_entities[loop.entityNum], velocity);
        }

        if (!trap->sound->IsPlaying(loop.voice)) {
            sound::Request request;
            request.sound = loop.sfx;
            request.soundClass = loop.soundClass;
            request.place = sound::Place::Point;
            request.origin = CG_SoundVector(loop.origin);
            request.loop = true;
            loop.voice = trap->sound->Play(request);
        }

        trap->sound->MoveLoop(loop.voice, CG_SoundVector(loop.origin), CG_SoundVector(velocity));
        loop.asked = false;
        ++i;
    }
}

void CG_StopLoopingSounds(void)
{
    for (const loopingSound_t& loop : loopingSounds) {
        trap->sound->Stop(loop.voice);
    }

    loopingSounds.clear();
}

void CG_SetSoundListener(int entityNum, const vec3_t origin, const matrix3_t axis, bool underwater)
{
    sound::Listener listener;
    listener.entity = entityNum;
    listener.origin = CG_SoundVector(origin);
    listener.axes = {CG_SoundVector(axis[0]), CG_SoundVector(axis[1]), CG_SoundVector(axis[2])};
    listener.velocity = CG_SoundVector(cg.predictedPlayerState.velocity);
    listener.underwater = underwater;
    trap->sound->SetListener(listener);
}

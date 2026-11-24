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

#pragma once

// cg_sound.h -- the sounds a cgame plays, the same in the three games (the multiplayer, Jedi Academy's and Jedi Outcast's
// single player): cg_sound.cpp beside it, compiled by each cgame against its own headers, and a small part each tree
// writes in its own cgame/cg_sound.cpp (the engine's sound, an entity's velocity, an NPC's type).

#include <shared/qcommon/q_math.h>
#include <shared/sound/sound.h>

//
// cg_sound.cpp (src/shared/cgame)
//
// the sounds may play: from the cgame's start to its shutdown
void CG_StartSounds(bool started);
// a vec3_t as the sound's vector
sound::Vector CG_SoundVector(const vec3_t v);
// where an entity's sounds are heard from, and how fast it moves
void CG_SetSoundPosition(int entityNum, const vec3_t origin);
// a sound heard from a point (origin), or following its entity (origin nullptr), in the entity's slot; entityNum -1 or
// ENTITYNUM_NONE for no entity
sound::VoiceHandle CG_StartSound(const vec3_t origin, int entityNum, sound::SoundClass soundClass, sound::Slot slot, sound::SoundHandle sfx,
                                 float volume = 1.0f);
// a sound at the listener, not positioned
sound::VoiceHandle CG_StartLocalSound(sound::SoundHandle sfx, sound::SoundClass soundClass, sound::Slot slot, float volume = 1.0f);
// a looping sound asked for in this frame, heard from origin: kept playing while it is asked for at every frame
void CG_AddLoopingSound(int entityNum, const vec3_t origin, sound::SoundHandle sfx, sound::SoundClass soundClass, float volume = 1.0f);
// at the frame's end: the loops asked for started or moved, the others stopped
void CG_UpdateLoopingSounds(void);
// a blade touching the world at this frame, at origin: its next sound started when it is due (BG_ContactSound)
void CG_StartContactSound(int entityNum, int saberNum, int bladeNum, const vec3_t origin);
// a blade no longer touching the world
void CG_EndContactSound(int entityNum, int saberNum, int bladeNum);
// every loop stopped and the blades' contacts forgotten (a level's start, a map restart, the cgame's shutdown)
void CG_StopLoopingSounds(void);
// the ears, once per frame: where they are, which way they face, how fast they move
void CG_SetSoundListener(int entityNum, const vec3_t origin, const vec3_t axis[3], const vec3_t velocity, bool underwater);

//
// each tree's cgame/cg_sound.cpp
//
// the engine's sound
sound::Api& CG_SoundApi(void);
// an NPC's type (its model's name in the multiplayer), for the creatures whose voices are heard from near; nullptr for
// an entity that is not an NPC
const char* CG_SoundNpcType(int entityNum);

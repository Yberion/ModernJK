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

// cg_sound.cpp -- the single player's part of the cgame's sounds (src/shared/cgame/cg_sound.h): the engine's sound
// (gi.sound), an entity's velocity, an NPC's type.

#include "cg_local.h"

sound::Api& CG_SoundApi(void)
{
    return *gi.sound;
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

const char* CG_SoundNpcType(int entityNum)
{
    if (entityNum < 0 || entityNum >= ENTITYNUM_WORLD) {
        return nullptr;
    }

    const gentity_t* gent = cg_entities[entityNum].gent;
    return gent ? gent->NPC_type : nullptr;
}

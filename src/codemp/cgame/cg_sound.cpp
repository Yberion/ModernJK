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

// cg_sound.cpp -- the multiplayer's part of the cgame's sounds (src/shared/cgame/cg_sound.h): the engine's sound
// (trap->sound), an entity's velocity, an NPC's type.

#include "cg_local.h"

sound::Api& CG_SoundApi(void)
{
    return *trap->sound;
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

// the players are never NPCs
const char* CG_SoundNpcType(int entityNum)
{
    if (entityNum < MAX_CLIENTS || entityNum >= ENTITYNUM_WORLD) {
        return nullptr;
    }

    const clientInfo_t* ci = cg_entities[entityNum].npcClient;
    return ci ? ci->modelName : nullptr;
}

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

// ui_shared.h -- the single player's values of the menus' shared code (Jedi Academy's and Jedi Outcast's): the sliders'
// size its menus are drawn for, an item's Ghoul2 instance held by value, and the Ghoul2 functions the engine gives its
// menus; then the menus' types (src/shared/ui/ui_shared.h).

#pragma once

#include <qcommon/q_shared.h>
#include <rd-common/tr_types.h>
#include <shared/qcommon/q_keycodes.h>

inline constexpr int NUM_CROSSHAIRS = 9;

inline constexpr double SLIDER_WIDTH = 128.0;
inline constexpr double SLIDER_THUMB_HEIGHT = 16.0;

// an item's Ghoul2 instance, held by the item
using uiGhoul2_t = CGhoul2Info_v;

// the display context's functions of this game alone: the Ghoul2 functions of the menus' models
struct displayContextGame_t {
    qhandle_t (*registerSkin)(const char* name);

    // rww - ghoul2 stuff. Add whatever you need here, remember to set it in _UI_Init or it will crash when you try to use it.
    bool (*g2_SetSkin)(CGhoul2Info* ghlInfo, qhandle_t customSkin, qhandle_t renderSkin);
    bool (*g2_SetBoneAnim)(CGhoul2Info* ghlInfo, const char* boneName, const int startFrame, const int endFrame, const boneFlags_t flags, const float animSpeed,
                           const int currentTime, const float setFrame, const int blendTime);
    bool (*g2_RemoveGhoul2Model)(CGhoul2Info_v& ghlInfo, const int modelIndex);
    int (*g2_InitGhoul2Model)(CGhoul2Info_v& ghoul2, const char* fileName, int, qhandle_t customSkin, qhandle_t customShader, int modelFlags, int lodBias);
    void (*g2_CleanGhoul2Models)(CGhoul2Info_v& ghoul2);
    int (*g2_AddBolt)(CGhoul2Info* ghlInfo, const char* boneName);
    bool (*g2_GetBoltMatrix)(CGhoul2Info_v& ghoul2, const int modelIndex, const int boltIndex, mdxaBone_t* matrix, const vec3_t angles, const vec3_t position,
                             const int frameNum, qhandle_t* modelList, const vec3_t scale);
    void (*g2_GiveMeVectorFromMatrix)(mdxaBone_t& boltMatrix, Eorientations flags, vec3_t& vec);

    // Utility functions that don't immediately redirect to ghoul2 functions
    int (*g2hilev_SetAnim)(CGhoul2Info* ghlInfo, const char* boneName, animNumber_t animNum, const bool freeze);
};

#include <shared/ui/ui_shared.h>

/*
===========================================================================
Copyright (C) 2026, OpenJK contributors

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

#pragma once

// q_renderer.h -- the renderers' and ghoul2's constants and sets: one definition for the trees that share them
// (multiplayer, and Jedi Academy's and Jedi Outcast's single player).

#include "qcommon/q_enum.h"
#include "qcommon/q_fileformats.h"

enum class cullResult_t {
	CULL_IN,		// completely unclipped
	CULL_CLIP,		// clipped by one or more planes
	CULL_OUT,		// completely outside the clipping planes
};
using enum cullResult_t;

inline constexpr int ENTITY_WIDTH = 12;
inline constexpr int MODEL_WIDTH = 10;
inline constexpr int BOLT_WIDTH = 10;
inline constexpr int MODEL_AND = (1<<MODEL_WIDTH)-1;
inline constexpr int BOLT_AND = (1<<BOLT_WIDTH)-1;
inline constexpr int ENTITY_AND = (1<<ENTITY_WIDTH)-1;
inline constexpr int BOLT_SHIFT = 0;
inline constexpr int MODEL_SHIFT = BOLT_SHIFT + BOLT_WIDTH;
inline constexpr int ENTITY_SHIFT = MODEL_SHIFT + MODEL_WIDTH;

inline constexpr int MAX_LODS = 8;

inline constexpr int MAX_DLIGHTS = 32;			// can't be increased, because bit flags are used on surfaces
inline constexpr int REFENTITYNUM_BITS = 11;		// can't be increased without changing drawsurf bit packing
inline constexpr int REFENTITYNUM_MASK = (1<<REFENTITYNUM_BITS) - 1;
inline constexpr int MAX_REFENTITIES = (1<<REFENTITYNUM_BITS) - 1;
inline constexpr int REFENTITYNUM_WORLD = (1<<REFENTITYNUM_BITS) - 1;
inline constexpr int MAX_RENDER_STRINGS = 8;
inline constexpr int MAX_RENDER_STRING_LENGTH = 32;

inline constexpr int MAX_SHADER_STAGES = 8;
inline constexpr int TR_MAX_TEXMODS = 4;
inline constexpr int MAX_SHADER_DEFORMS = 3;
inline constexpr int SURFSPRITE_NONE = 0;
inline constexpr int SURFSPRITE_VERTICAL = 1;
inline constexpr int SURFSPRITE_ORIENTED = 2;
inline constexpr int SURFSPRITE_EFFECT = 3;
inline constexpr int SURFSPRITE_WEATHERFX = 4;
inline constexpr int SURFSPRITE_FLATTENED = 5;
inline constexpr int SURFSPRITE_FACING_NORMAL = 0;
inline constexpr int SURFSPRITE_FACING_UP = 1;
inline constexpr int SURFSPRITE_FACING_DOWN = 2;
inline constexpr int SURFSPRITE_FACING_ANY = 3;
inline constexpr int NUM_TEXTURE_BUNDLES = 2;
inline constexpr int LIGHTMAP_2D = -4;		// shader is for 2D rendering
inline constexpr int LIGHTMAP_BY_VERTEX = -3;		// pre-lit triangle models
inline constexpr int LIGHTMAP_WHITEIMAGE = -2;
inline constexpr int LIGHTMAP_NONE = -1;
inline constexpr int MAX_PATCH_SIZE = 32;			// max dimensions of a patch mesh in map file
inline constexpr int VERTEXSIZE = 6+(MAXLIGHTMAPS*3);
inline constexpr int VERTEX_LM = 5;
inline constexpr int VERTEX_COLOR = 5+(MAXLIGHTMAPS*2);
inline constexpr int CONTENTS_NODE = -1;
inline constexpr int MAX_MOD_KNOWN = 1024;
inline constexpr int MAX_LIGHTMAPS = 256;
inline constexpr int QSORT_FOGNUM_SHIFT = 2;
inline constexpr int QSORT_REFENTITYNUM_SHIFT = 7;
inline constexpr int QSORT_SHADERNUM_SHIFT = QSORT_REFENTITYNUM_SHIFT+REFENTITYNUM_BITS;
inline constexpr int FOG_TABLE_SIZE = 256;
inline constexpr int FUNCTABLE_SIZE = 1024;
inline constexpr int FUNCTABLE_SIZE2 = 10;
inline constexpr int FUNCTABLE_MASK = FUNCTABLE_SIZE-1;
inline constexpr int NUM_SCRATCH_IMAGES = 16;
inline constexpr int NUM_TEX_COORDS = MAXLIGHTMAPS+1;

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

#pragma once

// q_fileformats.h -- the constants and sets of the game's file formats (BSP, MD3, ghoul2's models): one definition for
// the trees that share them (multiplayer, and Jedi Academy's and Jedi Outcast's single player).

#include "qcommon/q_enum.h"

enum class bspLump_t { // the indices of the file's lumps
    LUMP_ENTITIES = 0,
    LUMP_SHADERS = 1,
    LUMP_PLANES = 2,
    LUMP_NODES = 3,
    LUMP_LEAFS = 4,
    LUMP_LEAFSURFACES = 5,
    LUMP_LEAFBRUSHES = 6,
    LUMP_MODELS = 7,
    LUMP_BRUSHES = 8,
    LUMP_BRUSHSIDES = 9,
    LUMP_DRAWVERTS = 10,
    LUMP_DRAWINDEXES = 11,
    LUMP_FOGS = 12,
    LUMP_SURFACES = 13,
    LUMP_LIGHTMAPS = 14,
    LUMP_LIGHTGRID = 15,
    LUMP_VISIBILITY = 16,
    LUMP_LIGHTARRAY = 17,
    HEADER_LUMPS = 18
};

inline constexpr int MC_BITS_X = 16;
inline constexpr int MC_BITS_Y = 16;
inline constexpr int MC_BITS_Z = 16;
inline constexpr int MC_BITS_VECT = 16;
inline constexpr int MC_COMP_BYTES = ((MC_BITS_X + MC_BITS_Y + MC_BITS_Z + MC_BITS_VECT * 9) + 7) / 8;

inline constexpr int SHADER_MAX_VERTEXES = 1000;
inline constexpr int SHADER_MAX_INDEXES = 6 * SHADER_MAX_VERTEXES;
inline constexpr int MD3_VERSION = 15;
inline constexpr int MD3_MAX_LODS = 3;
inline constexpr double MD3_XYZ_SCALE = 1.0 / 64;
inline constexpr int BSP_VERSION = 1;
inline constexpr int MAXLIGHTMAPS = 4;
inline constexpr int GLYPH_COUNT = 256;

inline constexpr int MDXM_VERSION = 6;
inline constexpr int MDXA_VERSION = 6;
inline constexpr int iG2_TRISIDE_LONGEST = 0;
inline constexpr int iG2_TRISIDE_SHORTEST = 2;
inline constexpr int iG2_BITS_PER_BONEREF = 5;
inline constexpr int iMAX_G2_BONEWEIGHTS_PER_VERT = 4; // can't just be blindly increased, affects cache size etc
inline constexpr int iG2_BONEWEIGHT_TOPBITS_SHIFT =
    (iG2_BITS_PER_BONEREF * iMAX_G2_BONEWEIGHTS_PER_VERT) - 8; // 8 bits because of 8 in the BoneWeight[] array entry

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

// This file must be identical in the quake and utils directories

// contents flags are seperate bits
// a given brush can contribute multiple content bits

// these definitions also need to be in q_shared.h!

enum class contentsFlags_t : uint32_t {
    CONTENTS_NONE = 0x00000000u,
    CONTENTS_SOLID = 0x00000001u,          // Default setting. An eye is never valid in a solid
    CONTENTS_LAVA = 0x00000002u,           //
    CONTENTS_WATER = 0x00000004u,          //
    CONTENTS_FOG = 0x00000008u,            //
    CONTENTS_PLAYERCLIP = 0x00000010u,     //
    CONTENTS_MONSTERCLIP = 0x00000020u,    // Physically block bots
    CONTENTS_BOTCLIP = 0x00000040u,        // A hint for bots - do not enter this brush by navigation (if possible)
    CONTENTS_SHOTCLIP = 0x00000080u,       //
    CONTENTS_BODY = 0x00000100u,           // should never be on a brush, only in game
    CONTENTS_CORPSE = 0x00000200u,         // should never be on a brush, only in game
    CONTENTS_TRIGGER = 0x00000400u,        //
    CONTENTS_NODROP = 0x00000800u,         // don't leave bodies or items (death fog, lava)
    CONTENTS_TERRAIN = 0x00001000u,        // volume contains terrain data
    CONTENTS_LADDER = 0x00002000u,         //
    CONTENTS_ABSEIL = 0x00004000u,         // (SOF2) used like ladder to define where an NPC can abseil
    CONTENTS_OPAQUE = 0x00008000u,         // defaults to on, when off, solid can be seen through
    CONTENTS_OUTSIDE = 0x00010000u,        // volume is considered to be in the outside (i.e. not indoors)
    CONTENTS_SLIME = 0x00020000u,          // CHC needs this since we use same tools
    CONTENTS_LIGHTSABER = 0x00040000u,     // ""
    CONTENTS_TELEPORTER = 0x00080000u,     // ""
    CONTENTS_ITEM = 0x00100000u,           // ""
    CONTENTS_NOSHOT = 0x00200000u,         // shots pass through me
    CONTENTS_UNUSED00400000 = 0x00400000u, //
    CONTENTS_UNUSED00800000 = 0x00800000u, //
    CONTENTS_UNUSED01000000 = 0x01000000u, //
    CONTENTS_UNUSED02000000 = 0x02000000u, //
    CONTENTS_UNUSED04000000 = 0x04000000u, //
    CONTENTS_DETAIL = 0x08000000u,         // brushes not used for the bsp
    CONTENTS_INSIDE = 0x10000000u,         // volume is considered to be inside (i.e. indoors)
    CONTENTS_UNUSED20000000 = 0x20000000u, //
    CONTENTS_UNUSED40000000 = 0x40000000u, //
    CONTENTS_TRANSLUCENT = 0x80000000u,    // don't consume surface fragments inside
    CONTENTS_ALL = 0xFFFFFFFFu,
};
template <> inline constexpr bool enableEnumFlags<contentsFlags_t> = true;

#include <shared/qcommon/q_surfaceflags.h>

inline constexpr int MATERIAL_BITS = 5;
inline constexpr int MATERIAL_MASK = 0x1f; // mask to get the material type

// A surface's material is a number in the low MATERIAL_BITS bits of its flags (an index of the material tables)
[[nodiscard]] inline surfaceMaterial_t SURF_Material(surfaceFlags_t flags)
{
    return static_cast<surfaceMaterial_t>(to_underlying(flags) & MATERIAL_MASK);
}
[[nodiscard]] inline surfaceFlags_t SURF_MaterialBits(surfaceMaterial_t material)
{
    return static_cast<surfaceFlags_t>(to_underlying(material));
}

// Defined as a macro here so one change will affect all the relevant files

#define MATERIALS                                                                                                                                              \
    "none", "solidwood", "hollowwood", "solidmetal", "hollowmetal", "shortgrass", "longgrass", "dirt", "sand", "gravel", "glass", "concrete", "marble",        \
        "water", "snow", "ice", "flesh", "mud", "bpglass", "dryleaves", "greenleaves", "fabric", "canvas", "rock", "rubber", "plastic", "tiles", "carpet",     \
        "plaster", "shatterglass", "armor", "computer" /* this was missing, see enums above, plus ShaderEd2 pulldown options */

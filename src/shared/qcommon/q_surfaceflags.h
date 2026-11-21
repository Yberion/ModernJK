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

// q_surfaceflags.h -- the flags of a surface (a map's shaders, the BSP files): the same in the three games: one
// definition for the trees that share them (multiplayer, and Jedi Academy's and Jedi Outcast's single player).

#include "q_enum.h"

enum class surfaceFlags_t : uint32_t {
    SURF_NONE = 0x00000000u,
    SURF_UNUSED00000001 = 0x00000001u, //
    SURF_UNUSED00000002 = 0x00000002u, //
    SURF_UNUSED00000004 = 0x00000004u, //
    SURF_UNUSED00000008 = 0x00000008u, //
    SURF_UNUSED00000010 = 0x00000010u, //
    SURF_UNUSED00000020 = 0x00000020u, //
    SURF_UNUSED00000040 = 0x00000040u, //
    SURF_UNUSED00000080 = 0x00000080u, //
    SURF_UNUSED00000100 = 0x00000100u, //
    SURF_UNUSED00000200 = 0x00000200u, //
    SURF_UNUSED00000400 = 0x00000400u, //
    SURF_UNUSED00000800 = 0x00000800u, //
    SURF_UNUSED00001000 = 0x00001000u, //
    SURF_SKY = 0x00002000u,            // lighting from environment map
    SURF_SLICK = 0x00004000u,          // affects game physics
    SURF_METALSTEPS = 0x00008000u,     // CHC needs this since we use same tools (though this flag is temp?)
    SURF_FORCEFIELD = 0x00010000u,     // CHC ""			(but not temp)
    SURF_UNUSED00020000 = 0x00020000u, //
    SURF_NODAMAGE = 0x00040000u,       // never give falling damage
    SURF_NOIMPACT = 0x00080000u,       // don't make missile explosions
    SURF_NOMARKS = 0x00100000u,        // don't leave missile marks
    SURF_NODRAW = 0x00200000u,         // don't generate a drawsurface at all
    SURF_NOSTEPS = 0x00400000u,        // no footstep sounds
    SURF_NODLIGHT = 0x00800000u,       // don't dlight even if solid (solid lava, skies)
    SURF_NOMISCENTS = 0x01000000u,     // no client models allowed on this surface
    SURF_FORCESIGHT = 0x02000000u,     // not visible without Force Sight
    SURF_UNUSED04000000 = 0x04000000u, //
    SURF_UNUSED08000000 = 0x08000000u, //
    SURF_UNUSED10000000 = 0x10000000u, //
    SURF_UNUSED20000000 = 0x20000000u, //
    SURF_UNUSED40000000 = 0x40000000u, //
    SURF_UNUSED80000000 = 0x80000000u, //
};
template <> inline constexpr bool enableEnumFlags<surfaceFlags_t> = true;

// the values are written in the maps (a surface's flags in the BSP files)
enum class surfaceMaterial_t {
    MATERIAL_NONE = 0,          // for when the artist hasn't set anything up =)
    MATERIAL_SOLIDWOOD = 1,     // freshly cut timber
    MATERIAL_HOLLOWWOOD = 2,    // termite infested creaky wood
    MATERIAL_SOLIDMETAL = 3,    // solid girders
    MATERIAL_HOLLOWMETAL = 4,   // hollow metal machines
    MATERIAL_SHORTGRASS = 5,    // manicured lawn
    MATERIAL_LONGGRASS = 6,     // long jungle grass
    MATERIAL_DIRT = 7,          // hard mud
    MATERIAL_SAND = 8,          // sandy beach
    MATERIAL_GRAVEL = 9,        // lots of small stones
    MATERIAL_GLASS = 10,        //
    MATERIAL_CONCRETE = 11,     // hardened concrete pavement
    MATERIAL_MARBLE = 12,       // marble floors
    MATERIAL_WATER = 13,        // light covering of water on a surface
    MATERIAL_SNOW = 14,         // freshly laid snow
    MATERIAL_ICE = 15,          // packed snow/solid ice
    MATERIAL_FLESH = 16,        // hung meat, corpses in the world
    MATERIAL_MUD = 17,          // wet soil
    MATERIAL_BPGLASS = 18,      // bulletproof glass
    MATERIAL_DRYLEAVES = 19,    // dried up leaves on the floor
    MATERIAL_GREENLEAVES = 20,  // fresh leaves still on a tree
    MATERIAL_FABRIC = 21,       // Cotton sheets
    MATERIAL_CANVAS = 22,       // tent material
    MATERIAL_ROCK = 23,         //
    MATERIAL_RUBBER = 24,       // hard tire like rubber
    MATERIAL_PLASTIC = 25,      //
    MATERIAL_TILES = 26,        // tiled floor
    MATERIAL_CARPET = 27,       // lush carpet
    MATERIAL_PLASTER = 28,      // drywall style plaster
    MATERIAL_SHATTERGLASS = 29, // glass with the Crisis Zone style shattering
    MATERIAL_ARMOR = 30,        // body armor
    MATERIAL_COMPUTER = 31,     // computers/electronic equipment
    MATERIAL_LAST = 32,         // number of materials
};
template <> inline constexpr bool enableEnumArithmetic<surfaceMaterial_t> = true;

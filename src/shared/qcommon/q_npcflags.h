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

// q_npcflags.h -- the flags of the NPCs' sight checks, combat points and navigation: one definition for the trees that
// share them (multiplayer, and Jedi Academy's and Jedi Outcast's single player).

#include "q_enum.h"

#include <cstdint>

enum class visCheckFlags_t {
    CHECK_NONE = 0,
    CHECK_PVS = 1,
    CHECK_360 = 2,
    CHECK_FOV = 4,
    CHECK_SHOOT = 8,
    CHECK_VISRANGE = 16,
};
template <> inline constexpr bool enableEnumFlags<visCheckFlags_t> = true;

// The flags of a combat point (its spawnflags)
enum class combatPointFlags_t {
    CPF_NONE = 0,
    CPF_DUCK = 0x00000001,
    CPF_FLEE = 0x00000002,
    CPF_INVESTIGATE = 0x00000004,
    CPF_SQUAD = 0x00000008,
    CPF_LEAN = 0x00000010,
    CPF_SNIPE = 0x00000020,
};
template <> inline constexpr bool enableEnumFlags<combatPointFlags_t> = true;

enum class navInfoFlags_t {
    NIF_NONE = 0x00000000,
    NIF_FAILED = 0x00000001,    // failed to find a way to the goal
    NIF_MACRO_NAV = 0x00000002, // using macro navigation
    NIF_COLLISION = 0x00000004, // resolving collision with an entity
    NIF_BLOCKED = 0x00000008,   // blocked from moving
};
template <> inline constexpr bool enableEnumFlags<navInfoFlags_t> = true;

// The flags of a navigation node, shared by gameside and in-engine NPC nav routines
enum class navNodeFlags_t {
    NF_ANY = 0,
    // NF_CLEAR_LOS	= 0x00000001,
    NF_CLEAR_PATH = 0x00000002,
    NF_RECALC = 0x00000004,
};
template <> inline constexpr bool enableEnumFlags<navNodeFlags_t> = true;

// Edge flags: a byte of the nav file's edge record, hence the underlying type
enum class navEdgeFlags_t : uint8_t {
    EFLAG_NONE = 0,
    EFLAG_BLOCKED = 0x00000001,
    EFLAG_FAILED = 0x00000002,
};
template <> inline constexpr bool enableEnumFlags<navEdgeFlags_t> = true;

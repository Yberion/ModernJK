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

// q_gamesets.h -- the sets of values the three games share (movement, the NPCs' debugging, the number fonts): one
// definition for the trees that share them (multiplayer, and Jedi Academy's and Jedi Outcast's single player).

#include "qcommon/q_enum.h"

enum class aiDebugLevel_t {
    DEBUG_LEVEL_DETAIL = 4,
    DEBUG_LEVEL_INFO = 3,
    DEBUG_LEVEL_WARNING = 2,
    DEBUG_LEVEL_ERROR = 1,
    DEBUG_LEVEL_NONE = 0,
};

enum class flyMode_t {
    FLY_NONE,
    FLY_NORMAL,
    FLY_VEHICLE,
    FLY_HOVER,
};

// the fonts of CG_DrawNumField
enum class numFont_t { NUM_FONT_BIG = 1, NUM_FONT_SMALL, NUM_FONT_CHUNKY };

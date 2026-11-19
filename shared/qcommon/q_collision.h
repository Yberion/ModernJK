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

// q_collision.h -- the constants of the collision code: one definition for the trees that share them (multiplayer, and
// Jedi Academy's and Jedi Outcast's single player).

#include "qcommon/q_enum.h"

inline constexpr double SURFACE_CLIP_EPSILON = 0.125;

inline constexpr int MAX_FACETS = 1024;
inline constexpr int MAX_PATCH_PLANES = 4096; // Was 2048 on vanilla jka
inline constexpr int SUBDIVIDE_DISTANCE = 16;	//4	// never more than this units away from curve
inline constexpr double PLANE_TRI_EPSILON = 0.1;
inline constexpr double WRAP_POINT_EPSILON = 0.1;

inline constexpr int MAX_POINTS_ON_WINDING = 64;
inline constexpr int MAX_MAP_BOUNDS = 65535;

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

// q_forcelevels.h -- the levels of the force powers, the same set in the three games (multiplayer, and Jedi Academy's
// and Jedi Outcast's single player): one definition, included by both q_shared.h.

#include "qcommon/q_enum.h"

enum class forcePowerLevels_t
{
	FORCE_LEVEL_0,
	FORCE_LEVEL_1,
	FORCE_LEVEL_2,
	FORCE_LEVEL_3,
	NUM_FORCE_POWER_LEVELS,
	// past the count: the strength of Desann's and Tavion's saber attacks, the single player's mind trick at level 4
	// (cheats), Jedi Outcast's saberAnimLevel (4 Desann's style, 5 Tavion's), the bound of a siege class's levels
	FORCE_LEVEL_4 = 4,
	FORCE_LEVEL_5 = 5
};
using enum forcePowerLevels_t;
template <> inline constexpr bool enableEnumArithmetic<forcePowerLevels_t> = true;

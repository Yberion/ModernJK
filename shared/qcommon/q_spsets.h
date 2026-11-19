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

// q_spsets.h -- the sets of values Jedi Academy's and Jedi Outcast's single player share (the thrown saber, a script's
// lean, breakables, a character's custom sounds, the datapad): one definition for the two single-player trees.

#include "qcommon/q_enum.h"

//saberEntityState
enum class saberEntityState_t
{
	SES_NONE = 0,		// the saber was never thrown (the state a new entity starts with, saved as such)
	SES_LEAVING = 1,
	SES_HOVERING = 2,
	SES_RETURNING = 3,
};

enum class lean_t
{
	LEAN_NONE,
	LEAN_RIGHT,
	LEAN_LEFT,
};

enum class breakableModelType_t
{
	MDL_OTHER,
	MDL_ARMOR_HEALTH,
	MDL_AMMO,
};

// the sets of a character's custom sounds; CS_TRY_ALL, no set given, searches them all
enum class customSoundSet_t
{
	CS_BASIC,
	CS_COMBAT,
	CS_EXTRA,
	CS_JEDI,
	CS_TRY_ALL
};

// the background of the HUD's and the datapad's icons
enum class iconBackground_t
{
	ICON_WEAPONS,
	ICON_FORCE,
	ICON_INVENTORY
};

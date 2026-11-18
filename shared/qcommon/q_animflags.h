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

// q_animflags.h -- the parts and the flags of an animation set on a model (BG_SetAnim, PM_SetAnim, NPC_SetAnim): one
// definition for the trees that share them (multiplayer, and Jedi Academy's and Jedi Outcast's single player).

#include "qcommon/q_enum.h"

enum class setAnimParts_t
{
	SETANIM_NONE	= 0,
	SETANIM_TORSO = 1,
	SETANIM_LEGS  = 2,
	SETANIM_BOTH  = SETANIM_TORSO | SETANIM_LEGS, //3
};
using enum setAnimParts_t;
template <> inline constexpr bool enableEnumFlags<setAnimParts_t> = true;

enum class setAnimFlags_t
{
	SETANIM_FLAG_NORMAL		= 0, //Only set if timer is 0
	SETANIM_FLAG_OVERRIDE	= 1, //Override previous
	SETANIM_FLAG_HOLD		= 2, //Set the new timer
	SETANIM_FLAG_RESTART	= 4, //Allow restarting the anim if playing the same one (weapon fires)
	SETANIM_FLAG_HOLDLESS	= 8, //Set the new timer
};
using enum setAnimFlags_t;
template <> inline constexpr bool enableEnumFlags<setAnimFlags_t> = true;

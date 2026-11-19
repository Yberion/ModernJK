/*
===========================================================================
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

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

inline constexpr int ARMOR_EFFECT_TIME = 500;

#include "qcommon/q_saberflags.h"
//saberEntityState
inline constexpr int SES_LEAVING = 1;
inline constexpr int SES_RETURNING = 1; //3
//This is a hack because ATM the saberEntityState is only non-0 if out or 0 if in, and we
//at least want NPCs knowing when their saber is out regardless.


inline constexpr int JSF_AMBUSH = 16;	//ambusher Jedi

inline constexpr float SABER_RADIUS_STANDARD = 3.0f;
inline constexpr float SABER_REFLECT_MISSILE_CONE = 0.2f;

inline constexpr int FORCE_POWER_MAX = 100;
inline constexpr int MAX_GRIP_DISTANCE = 256;
inline constexpr int MAX_TRICK_DISTANCE = 512;
inline constexpr int FORCE_JUMP_CHARGE_TIME = 6400; //3000.0f
inline constexpr int GRIP_DRAIN_AMOUNT = 30;
inline constexpr int FORCE_LIGHTNING_RADIUS = 300;
inline constexpr int MAX_DRAIN_DISTANCE = 512;

enum class forceJump_t
{
	FJ_FORWARD,
	FJ_BACKWARD,
	FJ_RIGHT,
	FJ_LEFT,
	FJ_UP
};
using enum forceJump_t;

enum class evasionType_t
{
	EVASION_NONE = 0,
	EVASION_PARRY,
	EVASION_DUCK_PARRY,
	EVASION_JUMP_PARRY,
	EVASION_DODGE,
	EVASION_JUMP,
	EVASION_DUCK,
	EVASION_FJUMP,
	EVASION_CARTWHEEL,
	EVASION_OTHER,
	NUM_EVASION_TYPES
};
using enum evasionType_t;

inline constexpr float SABERMINS_X = -3.0f; //-24.0f
inline constexpr float SABERMINS_Y = -3.0f; //-24.0f
inline constexpr float SABERMINS_Z = -3.0f; //-8.0f
inline constexpr float SABERMAXS_X = 3.0f; //24.0f
inline constexpr float SABERMAXS_Y = 3.0f; //24.0f
inline constexpr float SABERMAXS_Z = 3.0f; //8.0f
inline constexpr float SABER_MIN_THROW_DIST = 80.0f;

extern EnumArray<EnumArray<int, forcePowers_t, NUM_FORCE_POWERS>, forcePowerLevels_t, NUM_FORCE_POWER_LEVELS> forcePowerNeeded;
extern EnumArray<float, forcePowerLevels_t, NUM_FORCE_POWER_LEVELS> forceJumpHeight;
extern EnumArray<float, forcePowerLevels_t, NUM_FORCE_POWER_LEVELS> forceJumpStrength;
extern EnumArray<float, forcePowerLevels_t, NUM_FORCE_POWER_LEVELS> forcePushPullRadius;

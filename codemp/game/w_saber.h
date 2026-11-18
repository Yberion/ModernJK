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

#define ARMOR_EFFECT_TIME	500

#include "qcommon/q_saberflags.h"
//saberEntityState
#define SES_LEAVING		1
#define SES_HOVERING	1//2
#define SES_RETURNING	1//3
//This is a hack because ATM the saberEntityState is only non-0 if out or 0 if in, and we
//at least want NPCs knowing when their saber is out regardless.


#define JSF_AMBUSH		16	//ambusher Jedi

#define SABER_RADIUS_STANDARD	3.0f
#define	SABER_REFLECT_MISSILE_CONE	0.2f

#define	FORCE_POWER_MAX	100
#define MAX_GRIP_DISTANCE 256
#define MAX_TRICK_DISTANCE 512
#define FORCE_JUMP_CHARGE_TIME 6400//3000.0f
#define GRIP_DRAIN_AMOUNT 30
#define FORCE_LIGHTNING_RADIUS 300
#define MAX_DRAIN_DISTANCE 512

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

#define SABERMINS_X -3.0f//-24.0f
#define SABERMINS_Y -3.0f//-24.0f
#define SABERMINS_Z -3.0f//-8.0f
#define SABERMAXS_X 3.0f//24.0f
#define SABERMAXS_Y 3.0f//24.0f
#define SABERMAXS_Z 3.0f//8.0f
#define	SABER_MIN_THROW_DIST	80.0f

extern EnumArray<EnumArray<int, forcePowers_t, NUM_FORCE_POWERS>, forcePowerLevels_t, NUM_FORCE_POWER_LEVELS> forcePowerNeeded;
extern EnumArray<float, forcePowerLevels_t, NUM_FORCE_POWER_LEVELS> forceJumpHeight;
extern EnumArray<float, forcePowerLevels_t, NUM_FORCE_POWER_LEVELS> forceJumpStrength;
extern EnumArray<float, forcePowerLevels_t, NUM_FORCE_POWER_LEVELS> forcePushPullRadius;

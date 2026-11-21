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

// q_saberflags.h -- the flags of a saber (its .sab file) and of the saber's events in a frame: one definition for the
// trees that share them (multiplayer, and Jedi Academy's and Jedi Outcast's single player).

#include "qcommon/q_enum.h"

//SABER FLAGS
enum class saberFlags_t
{
	SFL_NONE					= 0,
	//Old bools converted to a flag now
	SFL_NOT_LOCKABLE			= 1<<0, //can't get into a saberlock
	SFL_NOT_THROWABLE			= 1<<1, //can't be thrown - FIXME: maybe make this a max level of force saber throw that can be used with this saber?
	SFL_NOT_DISARMABLE			= 1<<2, //can't be dropped
	SFL_NOT_ACTIVE_BLOCKING		= 1<<3, //don't to try to block incoming shots with this saber
	SFL_TWO_HANDED				= 1<<4, //uses both hands
	SFL_SINGLE_BLADE_THROWABLE	= 1<<5, //can throw this saber if only the first blade is on
	SFL_RETURN_DAMAGE			= 1<<6, //when returning from a saber throw, it keeps spinning and doing damage
	//NEW FLAGS
	SFL_ON_IN_WATER				= 1<<7, //if set, weapon stays active even in water
	SFL_BOUNCE_ON_WALLS			= 1<<8, //if set, the saber will bounce back when it hits solid architecture (good for real-sword type mods)
	SFL_BOLT_TO_WRIST			= 1<<9, //if set, saber model is bolted to wrist, not in hand... useful for things like claws & shields, etc.
	//SFL_STICK_ON_IMPACT		= 1<<?, //if set, the saber will stick in the wall when thrown and hits solid architecture (good for sabers that are meant to be thrown).
	//SFL_NO_ATTACK				= 1<<?, //if set, you cannot attack with the saber (for sabers/weapons that are meant to be thrown only, not used as melee weapons).
	//Move Restrictions
	SFL_NO_PULL_ATTACK			= 1<<10, //if set, cannot do pull+attack move (move not available in MP anyway)
	SFL_NO_BACK_ATTACK			= 1<<11, //if set, cannot do back-stab moves
	SFL_NO_STABDOWN				= 1<<12, //if set, cannot do stabdown move (when enemy is on ground)
	SFL_NO_WALL_RUNS			= 1<<13, //if set, cannot side-run or forward-run on walls
	SFL_NO_WALL_FLIPS			= 1<<14, //if set, cannot do backflip off wall or side-flips off walls
	SFL_NO_WALL_GRAB			= 1<<15, //if set, cannot grab wall & jump off
	SFL_NO_ROLLS				= 1<<16, //if set, cannot roll
	SFL_NO_FLIPS				= 1<<17, //if set, cannot do flips
	SFL_NO_CARTWHEELS			= 1<<18, //if set, cannot do cartwheels
	SFL_NO_KICKS				= 1<<19, //if set, cannot do kicks (can't do kicks anyway if using a throwable saber/sword)
	SFL_NO_MIRROR_ATTACKS		= 1<<20, //if set, cannot do the simultaneous attack left/right moves (only available in Dual Lightsaber Combat Style)
	SFL_NO_ROLL_STAB			= 1<<21, //if set, cannot do roll-stab move at end of roll
};
template <> inline constexpr bool enableEnumFlags<saberFlags_t> = true;

//SABER FLAGS2
enum class saberFlags2_t
{
	SFL2_NONE					= 0,
	//Primary Blade Style
	SFL2_NO_WALL_MARKS			= 1<<0, //if set, stops the saber from drawing marks on the world (good for real-sword type mods)
	SFL2_NO_DLIGHT				= 1<<1, //if set, stops the saber from drawing a dynamic light (good for real-sword type mods)
	SFL2_NO_BLADE				= 1<<2, //if set, stops the saber from drawing a blade (good for real-sword type mods)
	SFL2_NO_CLASH_FLARE			= 1<<3, //if set, the saber will not do the big, white clash flare with other sabers
	SFL2_NO_DISMEMBERMENT		= 1<<4, //if set, the saber never does dismemberment (good for pointed/blunt melee weapons)
	SFL2_NO_IDLE_EFFECT			= 1<<5, //if set, the saber will not do damage or any effects when it is idle (not in an attack anim).  (good for real-sword type mods)
	SFL2_ALWAYS_BLOCK			= 1<<6, //if set, the blades will always be blocking (good for things like shields that should always block)
	SFL2_NO_MANUAL_DEACTIVATE	= 1<<7, //if set, the blades cannot manually be toggled on and off
	SFL2_TRANSITION_DAMAGE		= 1<<8, //if set, the blade does damage in start, transition and return anims (like strong style does)
	//Secondary Blade Style
	SFL2_NO_WALL_MARKS2			= 1<<9, //if set, stops the saber from drawing marks on the world (good for real-sword type mods)
	SFL2_NO_DLIGHT2				= 1<<10, //if set, stops the saber from drawing a dynamic light (good for real-sword type mods)
	SFL2_NO_BLADE2				= 1<<11, //if set, stops the saber from drawing a blade (good for real-sword type mods)
	SFL2_NO_CLASH_FLARE2		= 1<<12, //if set, the saber will not do the big, white clash flare with other sabers
	SFL2_NO_DISMEMBERMENT2		= 1<<13, //if set, the saber never does dismemberment (good for pointed/blunt melee weapons)
	SFL2_NO_IDLE_EFFECT2		= 1<<14, //if set, the saber will not do damage or any effects when it is idle (not in an attack anim).  (good for real-sword type mods)
	SFL2_ALWAYS_BLOCK2			= 1<<15, //if set, the blades will always be blocking (good for things like shields that should always block)
	SFL2_NO_MANUAL_DEACTIVATE2	= 1<<16, //if set, the blades cannot manually be toggled on and off
	SFL2_TRANSITION_DAMAGE2		= 1<<17, //if set, the blade does damage in start, transition and return anims (like strong style does)
};
template <> inline constexpr bool enableEnumFlags<saberFlags2_t> = true;

//saberEventFlags
enum class saberEventFlags_t
{
	SEF_NONE		= 0,
	SEF_HITENEMY	= 0x1,		//Hit the enemy
	SEF_HITOBJECT	= 0x2,		//Hit some other object
	SEF_HITWALL		= 0x4,		//Hit a wall
	SEF_PARRIED		= 0x8,		//Parried a saber swipe
	SEF_DEFLECTED	= 0x10,	//Deflected a missile or saberInFlight
	SEF_BLOCKED		= 0x20,	//Was blocked by a parry
	SEF_EVENTS		= SEF_HITENEMY|SEF_HITOBJECT|SEF_HITWALL|SEF_PARRIED|SEF_DEFLECTED|SEF_BLOCKED,
	SEF_LOCKED		= 0x40,	//Sabers locked with someone else
	SEF_INWATER		= 0x80,	//Saber is in water
	SEF_LOCK_WON	= 0x100,	//Won a saberLock
};
template <> inline constexpr bool enableEnumFlags<saberEventFlags_t> = true;

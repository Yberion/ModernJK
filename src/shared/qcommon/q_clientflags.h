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

// q_clientflags.h -- the flags of the cinematics, of cgame's local entities and camera overrides, of the menus' model
// items: one definition for the trees that share them (multiplayer, and Jedi Academy's and Jedi Outcast's single
// player).

#include "qcommon/q_enum.h"

enum class cinematicFlags_t
{
	CIN_NONE	= 0,
	CIN_system	= 1,
	CIN_loop	= 2,
	CIN_hold	= 4,
	CIN_silent	= 8,
	CIN_shader	= 16,
};
template <> inline constexpr bool enableEnumFlags<cinematicFlags_t> = true;

enum class leFlags_t {
	LEF_NONE			= 0,
	LEF_PUFF_DONT_SCALE = 0x0001,			// do not scale size over time
	LEF_TUMBLE			= 0x0002,			// tumble over time, used for ejecting shells
	LEF_FADE_RGB		= 0x0004,			// explicitly fade
	LEF_NO_RANDOM_ROTATE= 0x0008			// MakeExplosion adds random rotate which could be bad in some cases
};
template <> inline constexpr bool enableEnumFlags<leFlags_t> = true;

enum class cgOverrideFlags_t
{
	CG_OVERRIDE_NONE = 0,
	CG_OVERRIDE_3RD_PERSON_ENT	= 0x00000001,
	CG_OVERRIDE_3RD_PERSON_RNG	= 0x00000002,
	CG_OVERRIDE_3RD_PERSON_ANG	= 0x00000004,
	CG_OVERRIDE_3RD_PERSON_VOF	= 0x00000008,
	CG_OVERRIDE_3RD_PERSON_POF	= 0x00000010,
	CG_OVERRIDE_3RD_PERSON_CDP	= 0x00000020,
	CG_OVERRIDE_3RD_PERSON_APH	= 0x00000040,
	CG_OVERRIDE_FOV				= 0x00000080,
};
template <> inline constexpr bool enableEnumFlags<cgOverrideFlags_t> = true;

enum class itemModelFlags_t
{
	ITF_NONE = 0,
	ITF_G2VALID			= 0x0001,					// indicates whether or not g2 instance is valid.
	ITF_ISCHARACTER		= 0x0002,					// a character item, uses customRGBA
	ITF_ISSABER			= 0x0004,					// first saber item, draws blade
	ITF_ISSABER2		= 0x0008,					// second saber item, draws blade

	ITF_ISANYSABER		= (ITF_ISSABER|ITF_ISSABER2),	//either saber
};
template <> inline constexpr bool enableEnumFlags<itemModelFlags_t> = true;

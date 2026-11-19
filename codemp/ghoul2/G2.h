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

#include "qcommon/q_enum.h"

enum class boneFlags_t
{
	BONE_NONE = 0,
	BONE_ANGLES_PREMULT			= 0x0001,
	BONE_ANGLES_POSTMULT		= 0x0002,
	BONE_ANGLES_REPLACE			= 0x0004,

//added for a trace optimization. set in routines where a bone is
//set to be transformed in any way. -rww
	BONE_NEED_TRANSFORM			= 0x8000,

//rww - RAGDOLL_BEGIN
	BONE_ANGLES_RAGDOLL			= 0x2000,  // the rag flags give more details
//rww - RAGDOLL_END
	BONE_ANGLES_IK				= 0x4000,  // the rag flags give more details

	BONE_ANGLES_TOTAL			= ( BONE_ANGLES_PREMULT | BONE_ANGLES_POSTMULT | BONE_ANGLES_REPLACE ),
	BONE_ANIM_OVERRIDE			= 0x0008,
	BONE_ANIM_OVERRIDE_LOOP		= 0x0010,
	BONE_ANIM_OVERRIDE_FREEZE	= ( 0x0040 + BONE_ANIM_OVERRIDE ),
	BONE_ANIM_BLEND				= 0x0080,
	BONE_ANIM_TOTAL				= ( BONE_ANIM_OVERRIDE | BONE_ANIM_OVERRIDE_LOOP | BONE_ANIM_OVERRIDE_FREEZE | BONE_ANIM_BLEND)
};
template <> inline constexpr bool enableEnumFlags<boneFlags_t> = true;


// defines to setup the
#include "qcommon/q_renderer.h"



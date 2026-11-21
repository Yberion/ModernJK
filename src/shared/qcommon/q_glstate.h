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

// q_glstate.h -- the renderers' OpenGL state bits (blend factors, depth, alpha test, polygon mode): one definition for
// the trees that share them (multiplayer, and Jedi Academy's and Jedi Outcast's single player).

#include "qcommon/q_enum.h"

enum class glStateBits_t : uint32_t
{
	GLS_NONE = 0,
	GLS_SRCBLEND_ZERO						= 0x00000001,
	GLS_SRCBLEND_ONE						= 0x00000002,
	GLS_SRCBLEND_DST_COLOR					= 0x00000003,
	GLS_SRCBLEND_ONE_MINUS_DST_COLOR		= 0x00000004,
	GLS_SRCBLEND_SRC_ALPHA					= 0x00000005,
	GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA		= 0x00000006,
	GLS_SRCBLEND_DST_ALPHA					= 0x00000007,
	GLS_SRCBLEND_ONE_MINUS_DST_ALPHA		= 0x00000008,
	GLS_SRCBLEND_ALPHA_SATURATE				= 0x00000009,
	GLS_SRCBLEND_BITS					= 0x0000000f,

	GLS_DSTBLEND_ZERO						= 0x00000010,
	GLS_DSTBLEND_ONE						= 0x00000020,
	GLS_DSTBLEND_SRC_COLOR					= 0x00000030,
	GLS_DSTBLEND_ONE_MINUS_SRC_COLOR		= 0x00000040,
	GLS_DSTBLEND_SRC_ALPHA					= 0x00000050,
	GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA		= 0x00000060,
	GLS_DSTBLEND_DST_ALPHA					= 0x00000070,
	GLS_DSTBLEND_ONE_MINUS_DST_ALPHA		= 0x00000080,
	GLS_DSTBLEND_BITS					= 0x000000f0,

	GLS_DEPTHMASK_TRUE						= 0x00000100,

	GLS_POLYMODE_LINE						= 0x00001000,

	GLS_DEPTHTEST_DISABLE					= 0x00010000,
	GLS_DEPTHFUNC_EQUAL						= 0x00020000,

	GLS_ATEST_GT_0							= 0x10000000,
	GLS_ATEST_LT_80							= 0x20000000,
	GLS_ATEST_GE_80							= 0x40000000,
	GLS_ATEST_GE_C0							= 0x80000000,
	GLS_ATEST_BITS						= 0xF0000000,

	GLS_DEFAULT			= GLS_DEPTHMASK_TRUE,
	GLS_ALPHA			= (GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA)
};
template <> inline constexpr bool enableEnumFlags<glStateBits_t> = true;

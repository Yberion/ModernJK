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

// q_icarusflags.h -- the flags of ICARUS's blocks and tokenizer, and its limits: one definition for the trees that
// share them (multiplayer, and Jedi Academy's and Jedi Outcast's single player).

#include "qcommon/q_enum.h"

// The flags of a block, set by the sequencer
enum class blockFlags_t
{
	BF_NONE			= 0,
	BF_ELSE			= 0x00000001,	//Block has an else id	//FIXME: This was a sloppy fix for a problem that arose from conditionals
};
template <> inline constexpr bool enableEnumFlags<blockFlags_t> = true;

enum class tokenizerFlags_t
{
	TKF_NONE = 0,
	TKF_IGNOREDIRECTIVES			= 0x00000001,		// skip over lines starting with #
	TKF_USES_EOL					= 0x00000002,		// generate end of line tokens
	TKF_NODIRECTIVES				= 0x00000004,		// don't treat # in any special way
	TKF_WANTUNDEFINED				= 0x00000008,		// if token not found in symbols create undefined token
	TKF_WIDEUNDEFINEDSYMBOLS		= 0x00000010,		// when undefined token encountered, accumulate until space
	TKF_RAWSYMBOLSONLY				= 0x00000020,
	TKF_NUMERICIDENTIFIERSTART		= 0x00000040,
	TKF_IGNOREKEYWORDS				= 0x00000080,
	TKF_NOCASEKEYWORDS				= 0x00000100,
	TKF_NOUNDERSCOREINIDENTIFIER	= 0x00000200,
	TKF_NODASHINIDENTIFIER			= 0x00000400,
	TKF_COMMENTTOKENS				= 0x00000800,
};
template <> inline constexpr bool enableEnumFlags<tokenizerFlags_t> = true;

inline constexpr int IBI_HEADER_ID_LENGTH = 4; // Length of IBI_HEADER_ID + 1 for the null terminating byte.

inline constexpr double ICARUS_VERSION = 1.33;
inline constexpr int MAX_STRING_SIZE = 256;
inline constexpr int MAX_VAR_NAME = 64;

inline constexpr int MAX_IDENTIFIER_LENGTH = 128;

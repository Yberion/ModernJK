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

// q_sound.h -- the constants and sets of the client's sound: one definition for the trees that share them (multiplayer,
// and Jedi Academy's and Jedi Outcast's single player).

#include "qcommon/q_enum.h"

enum class wavFormat_t {	// a WAV header's format tag
	WAV_FORMAT_NONE		= 0,	// no format read yet (GetWavinfo clears the info), or a file without one
	WAV_FORMAT_PCM		= 1,
	WAV_FORMAT_ADPCM	= 2,	// not actually implemented, but is the value that you get in a header
	WAV_FORMAT_MP3		= 3,	// not actually used this way, but just ensures we don't match one of the legit formats
};
using enum wavFormat_t;

inline constexpr int PAINTBUFFER_SIZE = 1024;
inline constexpr int MAX_CHANNELS = 32;
inline constexpr int MAX_RAW_SAMPLES = 16384;

inline constexpr int iMAX_ACTION_TRANSITIONS = 4;	// these can be increased easily enough, I just need to know about them
inline constexpr int iMAX_EXPLORE_TRANSITIONS = 4;	//

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

// q_limits.h -- the engine's limits, fixed entity numbers and configstring indices: one definition for the trees that
// share them (multiplayer, and Jedi Academy's and Jedi Outcast's single player).

#include "q_enum.h"

inline constexpr int NUM_CON_TIMES = 4;

inline constexpr int MAX_STRING_CHARS = 1024;  // max length of a string passed to Cmd_TokenizeString
inline constexpr int MAX_STRING_TOKENS = 1024; // max tokens resulting from Cmd_TokenizeString
inline constexpr int MAX_TOKEN_CHARS = 1024;   // max length of an individual token
inline constexpr int MAX_INFO_STRING = 1024;
inline constexpr int MAX_INFO_KEY = 1024;
inline constexpr int MAX_INFO_VALUE = 1024;
inline constexpr int BIG_INFO_STRING = 8192;  // used for system info key only
inline constexpr int MAX_MAP_AREA_BYTES = 32; // bit vector of area visibility
inline constexpr int LS_STYLES_START = 0;
inline constexpr int LS_NUM_STYLES = 32;
inline constexpr int PULSE_DIVISOR = 75;
inline constexpr int SCREEN_WIDTH = 640;
inline constexpr int SCREEN_HEIGHT = 480;
inline constexpr int SMALLCHAR_WIDTH = 8;
inline constexpr int SMALLCHAR_HEIGHT = 16;
inline constexpr int BIGCHAR_WIDTH = 16;
inline constexpr int BIGCHAR_HEIGHT = 16;
inline constexpr int TRUNCATE_LENGTH = 64;
inline constexpr int MAX_CVAR_VALUE_STRING = 256;
inline constexpr int MAX_G2_COLLISIONS = 16;
inline constexpr int MAX_TERRAINS = 1;     // 32 //rwwRMG: inserted
inline constexpr int GENTITYNUM_BITS = 10; // don't need to send any more
inline constexpr int MAX_GENTITIES = 1 << GENTITYNUM_BITS;
inline constexpr int ENTITYNUM_NONE = MAX_GENTITIES - 1;
inline constexpr int ENTITYNUM_WORLD = MAX_GENTITIES - 2;
inline constexpr int ENTITYNUM_MAX_NORMAL = MAX_GENTITIES - 2;
inline constexpr int MAX_SUB_BSP = 32;  // rwwRMG - added
inline constexpr int CS_SERVERINFO = 0; // an info string with all the serverinfo cvars
inline constexpr int CS_SYSTEMINFO = 1; // an info string for server system to client system configuration (timescale, etc)
inline constexpr int MAX_GAMESTATE_CHARS = 16000;
inline constexpr int MAX_STATS = 16;
inline constexpr int MAX_PERSISTANT = 16;
inline constexpr int MAX_POWERUPS = 16;
inline constexpr int MAX_PS_EVENTS = 2;
inline constexpr int MOVE_RUN = 120; // if forwardmove or rightmove are >= MOVE_RUN,

inline constexpr int MAX_PACKET_USERCMDS = 32; // max number of usercmd_t in a packet
inline constexpr char CONSOLE_PROMPT_CHAR = ']';
inline constexpr int MAX_EDIT_LINE = 256;
inline constexpr int COMMAND_HISTORY = 32;

inline constexpr int iSE_VERSION = 1;

inline constexpr int MAX_ENT_CLUSTERS = 16;

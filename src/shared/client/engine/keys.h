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

// keys.h -- the clients' keys, the same in the three games: cl_keys.cpp beside it, compiled by each client against its
// own headers, and a small part each tree writes in its own client/cl_keys.cpp (where a key goes beside the console:
// the tree's modules, its own menus, a video playing).
#pragma once

#include <shared/client/field.h>
#include <shared/client/keynames.h>
#include <shared/qcommon/q_keycodes.h>

#include <cstdint>
#include <optional>

typedef struct qkey_s {
    bool down;
    int repeats; // if > 1, it is autorepeating
    char* binding;
} qkey_t;

typedef struct keyGlobals_s {
    bool anykeydown;
    bool key_overstrikeMode;
    int keyDownCount;

    EnumArray<qkey_t, fakeAscii_t, fakeAscii_t::MAX_KEYS> keys;
} keyGlobals_t;

//
// cl_keys.cpp (src/shared/client/engine)
//
extern keyGlobals_t kg;
extern keyNames_t keynames;

// console
extern field_t g_consoleField;
extern int nextHistoryLine; // the last line in the history buffer, not masked
extern int historyLine;     // the line being displayed from history buffer will be <= nextHistoryLine
extern field_t historyEditLines[COMMAND_HISTORY];

void Field_KeyDownEvent(field_t* edit, fakeAscii_t key);
void Field_CharEvent(field_t* edit, int ch);
void Field_Draw(field_t* edit, int x, int y, bool showCursor, bool noColorEscape);
void Field_BigDraw(field_t* edit, int x, int y, bool showCursor, bool noColorEscape);

void Key_SetBinding(fakeAscii_t keynum, const char* binding);
const char* Key_GetBinding(fakeAscii_t keynum);
bool Key_IsDown(fakeAscii_t keynum);
std::optional<fakeAscii_t> Key_StringToKeynum(const char* str);
// the code of a key the key table has none for, by the name such a key is written with in a bind
std::optional<fakeAscii_t> Key_DynamicKey(const char* name);
bool Key_GetOverstrikeMode(void);
void Key_SetOverstrikeMode(bool state);
void Key_ClearStates(void);
std::optional<fakeAscii_t> Key_GetKey(const char* binding);

//
// each tree's client/cl_keys.cpp
//
// a key pressed while a video plays and nothing takes the keys: true when it stopped the video, or left it, and goes
// no further
bool CL_KeyStopsVideo(void);
// Escape, while neither the console nor the UI takes the keys: true when it closed what took them (the multiplayer's
// chat and cgame)
bool CL_EscapeClosesCatcher(void);
// Escape, while nothing takes the keys: the menu over the game, or the main menu
void CL_EscapeOpensMenu(void);
// whether a menu of the client itself takes the keys, the bound commands then left alone (the multiplayer's download
// question)
bool CL_ClientMenuTakesKeys(void);
// a key pressed or released, to what takes the keys beside the console (the client's own menu, the UI, the cgame, the
// multiplayer's chat): false when nothing does
bool CL_ModuleKeyEvent(fakeAscii_t key, bool down);
// a typed character, in the engine's one-byte set, to what takes the characters beside the console: false when
// nothing does
bool CL_ModuleCharEvent(int character);

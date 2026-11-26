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

#include <shared/qcommon/q_keycodes.h>

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

typedef struct keyname_s {
    word upper, lower;
    const char* name;
    fakeAscii_t keynum;
    bool menukey;
} keyname_t;

extern keyGlobals_t kg;
extern EnumArray<keyname_t, fakeAscii_t, fakeAscii_t::MAX_KEYS> keynames;

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
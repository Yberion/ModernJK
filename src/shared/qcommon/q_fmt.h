/*
===========================================================================
Copyright (C) 2026, ModernJK contributors

This file is part of the ModernJK source code.

ModernJK is free software; you can redistribute it and/or modify it
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

// q_fmt.h -- the printing and formatting functions, the same in every engine and module of every tree. A message
// goes through the function of its level, and the engine writes the level's colour and prefix, so that a level looks
// the same wherever it is printed from. Each engine and module defines Com_VPrintf and Com_VError, where its messages
// and errors go: the engine's own console, or the engine through the module's import table.
#pragma once

#include <stdarg.h>

#include "q_platform.h"

// the codes of Com_Error; each tree defines its own set in its qcommon/q_shared.h
enum class errorParm_t;

// the level of a message
enum class printLevel_t {
    PRINT_MESSAGE,
    PRINT_DEVELOPER, // printed only with developer
    PRINT_WARNING,   // written after "^3WARNING: "
    PRINT_ERROR      // written after "^1ERROR: "; stops nothing, unlike Com_Error
};

// a message
void QDECL Com_Printf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
// a developer's message, printed only with developer
void QDECL Com_DPrintf(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
// a warning
void QDECL Com_Warning(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
// an error that stops nothing
void QDECL Com_PrintError(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
// an error that stops: the game, the connection or the program, by its code
NORETURN void QDECL Com_Error(errorParm_t code, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

// where the messages and the errors of an engine or a module go, defined by each of them
void Com_VPrintf(printLevel_t level, const char* fmt, va_list args) __attribute__((format(printf, 2, 0)));
NORETURN void Com_VError(errorParm_t code, const char* fmt, va_list args) __attribute__((format(printf, 2, 0)));

// the text formatted into dest, cut to its size with a warning; returns the text's full length
int QDECL Com_sprintf(char* dest, int size, const char* fmt, ...) __attribute__((format(printf, 3, 4)));
// the text formatted into the next of four static buffers, so that nested calls do not overwrite each other
char* QDECL va(const char* format, ...) __attribute__((format(printf, 1, 2)));

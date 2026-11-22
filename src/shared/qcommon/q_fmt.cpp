/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
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

// q_fmt.cpp -- the functions of q_fmt.h that are the same in every engine and module: the levels' functions, which
// hand their arguments to the binary's Com_VPrintf and Com_VError, and the formatting into a buffer (from each tree's
// q_shared.cpp, where the multiplayer and the single player had the same code).
#include <stdio.h>

#include "q_fmt.h"
#include "q_string.h"

void QDECL Com_Printf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Com_VPrintf(printLevel_t::PRINT_MESSAGE, fmt, args);
    va_end(args);
}

void QDECL Com_DPrintf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Com_VPrintf(printLevel_t::PRINT_DEVELOPER, fmt, args);
    va_end(args);
}

void QDECL Com_Warning(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Com_VPrintf(printLevel_t::PRINT_WARNING, fmt, args);
    va_end(args);
}

void QDECL Com_PrintError(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    Com_VPrintf(printLevel_t::PRINT_ERROR, fmt, args);
    va_end(args);
}

void QDECL Com_Error(errorParm_t code, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    // does not return: va_end is never reached
    Com_VError(code, fmt, args);
}

/*
============================================================================

                    LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

int QDECL Com_sprintf(char* dest, int size, const char* fmt, ...)
{
    int len;
    va_list argptr;

    va_start(argptr, fmt);
    len = Q_vsnprintf(dest, size, fmt, argptr);
    va_end(argptr);

    if (len >= size)
        Com_Printf("Com_sprintf: Output length %d too short, require %d bytes.\n", size, len + 1);

    return len;
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday
============
*/
constexpr int MAX_VA_STRING = 32000;
constexpr int MAX_VA_BUFFERS = 4;

char* QDECL va(const char* format, ...)
{
    va_list argptr;
    static char string[MAX_VA_BUFFERS][MAX_VA_STRING]; // in case va is called by nested functions
    static int index = 0;
    char* buf;

    va_start(argptr, format);
    buf = (char*)&string[index++ & 3];
    Q_vsnprintf(buf, sizeof(*string), format, argptr);
    va_end(argptr);

    return buf;
}

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

// Engine functions and globals that OpenJK's msg.cpp refers to, reduced to what the benchmark needs:
// no console output (cl_shownet stays nullptr), no file system (no netf / psf override file: default field widths,
// as in the recordings), errors abort.
#include <qcommon/q_shared.h>
#include <qcommon/qcommon.h>
#include <server/server.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

cvar_t* cl_shownet = nullptr;
server_t sv;

void QDECL Com_Printf(const char* fmt, ...)
{
    (void)fmt;
}

void QDECL Com_Warning(const char* fmt, ...)
{
    (void)fmt;
}

void NORETURN QDECL Com_Error(errorParm_t code, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "legacy Com_Error %d: ", to_underlying(code));
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    abort();
}

char* QDECL va(const char* format, ...)
{
    static char buf[1024];
    va_list ap;
    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    return buf;
}

void Q_strncpyz(char* dest, const char* src, int destsize)
{
    strncpy(dest, src, destsize - 1);
    dest[destsize - 1] = 0;
}

long FS_FOpenFileRead(const char*, fileHandle_t* file, bool)
{
    *file = 0;
    return -1;
}
int FS_Read(void*, int, fileHandle_t)
{
    return 0;
}
void FS_FCloseFile(fileHandle_t) {}
void* Z_Malloc(int iSize, memtag_t, bool, int)
{
    return calloc(1, iSize);
}
sharedEntity_t* SV_GentityNum(int)
{
    return nullptr;
}

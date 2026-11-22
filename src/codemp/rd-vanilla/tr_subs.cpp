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

// tr_subs.cpp - common function replacements for modular renderer
#include "tr_local.h"

// the renderer's messages and errors, formatted by the engine (q_fmt.h)
void Q_VPrint(printLevel_t level, lineEnd_t end, fmt::string_view fmt, fmt::format_args args)
{
    ri.Print(level, end, fmt, args);
}

void Q_VError(errorParm_t code, fmt::string_view fmt, fmt::format_args args)
{
    ri.Error(code, fmt, args);
}

// HUNK
void* Hunk_AllocateTempMemory(int size)
{
    return ri.Hunk_AllocateTempMemory(size);
}

void Hunk_FreeTempMemory(void* buf)
{
    ri.Hunk_FreeTempMemory(buf);
}

void* Hunk_Alloc(int size, ha_pref preference)
{
    return ri.Hunk_Alloc(size, preference);
}

int Hunk_MemoryRemaining(void)
{
    return ri.Hunk_MemoryRemaining();
}

// ZONE
void* Z_Malloc(int iSize, memtag_t eTag, bool bZeroit, int iAlign)
{
    return ri.Z_Malloc(iSize, eTag, bZeroit, iAlign);
}

void Z_Free(void* ptr)
{
    ri.Z_Free(ptr);
}

int Z_MemSize(memtag_t eTag)
{
    return ri.Z_MemSize(eTag);
}

void Z_MorphMallocTag(void* pvBuffer, memtag_t eDesiredTag)
{
    ri.Z_MorphMallocTag(pvBuffer, eDesiredTag);
}

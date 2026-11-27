/*
===========================================================================
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

// files_engine.h -- what the file system (shared/qcommon/files.cpp, one file for the multiplayer and the
// single-player engines) takes from the engine it is built in: here the single-player one. A difference between the
// two engines goes in this file and in codemp/qcommon/files_engine.h, not in the shared one.

#define FS_DEFAULT_CFG "default.cfg"                // the configuration a game folder must hold
#define FS_AUTOEXEC_CFG "autoexec_sp.cfg"           // read from outside the pk3 files only, as Q3CONFIG_CFG is
#define Q3CONFIG_CFG Q3CONFIG_NAME                  // the name the file system knows this engine's configuration file by
inline constexpr bool FS_MODULES_FROM_PAKS = false; // the modules are loaded from folders only

// the memory of a file read whole carries the file's name (in the builds that label memory)
static inline void FS_LabelFile(const void* buffer, const char* name)
{
    Z_Label(buffer, name);
}

// What the multiplayer engine has and this one has not, as the file system uses it:

// no temporary memory on the hunk
static inline void* Hunk_AllocateTempMemory(int size)
{
    return Z_Malloc(size, memtag::TAG_TEMP_WORKSPACE, false);
}
static inline void Hunk_FreeTempMemory(void* buffer)
{
    Z_Free(buffer);
}

// no description of a cvar
static inline cvar_t* Cvar_Get(const char* name, const char* value, cvarFlags_t flags, const char*)
{
    return Cvar_Get(name, value, flags);
}

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

// files.h -- the file system's interface, one for the multiplayer and the single-player engines, as its
// implementation is (files.cpp). Included by each engine's qcommon/qcommon.h, which defines the types it uses.
// The single-player engine has no pure server and no download: the functions for them are there and unused.

/*
==============================================================

FILESYSTEM

No stdio calls should be used by any part of the game, because
we need to deal with all sorts of directory and seperator char
issues.
==============================================================
*/

// referenced flags
// these are in loop specific order so don't change the order
enum class fsRefFlags_t {
    FS_NONE = 0,
    FS_GENERAL_REF = 0x01,
    FS_UI_REF = 0x02,
    FS_CGAME_REF = 0x04,
    FS_GAME_REF = 0x08,
    FS_ALL_REF = FS_GENERAL_REF | FS_UI_REF | FS_CGAME_REF | FS_GAME_REF,
};
template <> inline constexpr bool enableEnumFlags<fsRefFlags_t> = true;
// number of id paks that will never be autodownloaded from base
inline constexpr int NUM_ID_PAKS = 9;

inline constexpr int MAX_FILE_HANDLES = 64;

bool FS_Initialized();

void FS_InitFilesystem(void);
void FS_Shutdown(bool closemfp, bool keepModuleFiles = false);

bool FS_ConditionalRestart(int checksumFeed);
void FS_Restart(int checksumFeed, bool inPlace = false);
// shutdown and restart the filesystem so changes to fs_gamedir can take effect

char** FS_ListFiles(const char* directory, const char* extension, int* numfiles);
// directory should not have either a leading or trailing /
// if extension is "/", only subdirectories will be returned
// the returned files will not include any directories or /

void FS_FreeFileList(char** fileList);
// rwwRMG - changed to fileList to not conflict with list type

void FS_Remove(const char* osPath);
void FS_HomeRemove(const char* homePath);

void FS_Rmdir(const char* osPath, bool recursive);
void FS_HomeRmdir(const char* homePath, bool recursive);

bool FS_FileExists(const char* file);

char* FS_BuildOSPath(const char* base, const char* game, const char* qpath);
bool FS_CompareZipChecksum(const char* zipfile);

int FS_GetFileList(const char* path, const char* extension, char* listbuf, int bufsize);
int FS_GetModList(char* listbuf, int bufsize);

fileHandle_t FS_FOpenFileWrite(const char* qpath, bool safe = true);
// will properly create any needed paths and deal with seperater character issues

fileHandle_t FS_FOpenFileAppend(const char* filename);

int FS_filelength(fileHandle_t f);
fileHandle_t FS_SV_FOpenFileWrite(const char* filename);
fileHandle_t FS_SV_FOpenFileAppend(const char* filename);
int FS_SV_FOpenFileRead(const char* filename, fileHandle_t* fp);
void FS_SV_Rename(const char* from, const char* to, bool safe);
long FS_FOpenFileRead(const char* qpath, fileHandle_t* file, bool uniqueFILE);
// if uniqueFILE is true, then a new FILE will be fopened even if the file
// is found in an already open pak file.  If uniqueFILE is false, you must call
// FS_FCloseFile instead of fclose, otherwise the pak FILE would be improperly closed
// It is generally safe to always set uniqueFILE to true, because the majority of
// file IO goes through FS_ReadFile, which Does The Right Thing already.

int FS_FileIsInPAK(const char* filename, int* pChecksum);
// returns 1 if a file is in the PAK file, otherwise -1

bool FS_FindPureDLL(const char* name);

int FS_Write(const void* buffer, int len, fileHandle_t f);

int FS_Read(void* buffer, int len, fileHandle_t f);
// properly handles partial reads and reads from other dlls

void FS_FCloseFile(fileHandle_t f);
// note: you can't just fclose from another DLL, due to MS libc issues

long FS_ReadFile(const char* qpath, void** buffer);
// returns the length of the file
// a null buffer will just return the file length without loading
// as a quick check for existance. -1 length == not present
// A 0 byte will always be appended at the end, so string ops are safe.
// the buffer should be considered read-only, because it may be cached
// for other uses.

void FS_ForceFlush(fileHandle_t f);
// forces flush on files we're writing to.

void FS_FreeFile(void* buffer);
// frees the memory returned by FS_ReadFile

void FS_WriteFile(const char* qpath, const void* buffer, int size);
// writes a complete file, creating any subdirectories needed

int FS_filelength(fileHandle_t f);
// doesn't work for files that are opened from a pack file

int FS_FTell(fileHandle_t f);
// where are we?

void FS_Flush(fileHandle_t f);

void FS_FilenameCompletion(const char* dir, const char* ext, bool stripExt, callbackFunc_t callback, bool allowNonPureFilesOnDisk);

const char* FS_GetCurrentGameDir(bool emptybase = false);

#ifdef MACOS_X
bool FS_LoadMachOBundle(const char* name);
#endif

void QDECL FS_Printf(fileHandle_t f, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
// like fprintf

int FS_FOpenFileByMode(const char* qpath, fileHandle_t* f, fsMode_t mode);
// opens a file for reading, writing, or appending depending on the value of mode

int FS_Seek(fileHandle_t f, long offset, fsOrigin_t origin);
// seek on a file

bool FS_FilenameCompare(const char* s1, const char* s2);

// These 2 are generally only used by the save games, filenames are local (eg "saves/blah.sav")
void FS_DeleteUserGenFile(const char* filename);
bool FS_MoveUserGenFile(const char* filename_src, const char* filename_dst);

const char* FS_LoadedPakNames(void);
const char* FS_LoadedPakChecksums(void);
const char* FS_LoadedPakPureChecksums(void);
// Returns a space separated string containing the checksums of all loaded pk3 files.
// Servers with sv_pure set will get this string and pass it to clients.

const char* FS_ReferencedPakNames(void);
const char* FS_ReferencedPakChecksums(void);
const char* FS_ReferencedPakPureChecksums(void);
// Returns a space separated string containing the checksums of all loaded
// AND referenced pk3 files. Servers with sv_pure set will get this string
// back from clients for pure validation

void FS_ClearPakReferences(fsRefFlags_t flags);
// clears referenced booleans on loaded pk3s

void FS_PureServerSetReferencedPaks(const char* pakSums, const char* pakNames);
void FS_PureServerSetLoadedPaks(const char* pakSums, const char* pakNames);
// If the string is empty, all data sources will be allowed.
// If not empty, only pk3 files that match one of the space
// separated checksums will be checked for files, with the
// sole exception of .cfg files.

bool FS_CheckDirTraversal(const char* checkdir);
bool FS_idPak(const char* pak, const char* base);
bool FS_ComparePaks(char* neededpaks, int len, bool dlstring);
void FS_Rename(const char* from, const char* to);

bool FS_WriteToTemporaryFile(const void* data, size_t dataLength, char** tempFileName);
const char* FS_MV_VerifyDownloadPath(const char* pk3file);

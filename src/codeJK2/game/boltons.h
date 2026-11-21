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

#ifndef __BOLTONS_H__
#define __BOLTONS_H__

inline constexpr int MAX_GAME_BOLTONS = 64;

typedef struct boltOn_s { // a tagged object
    char name[32];        // If there is a name, it is considered active (look up in boltOns.cfg)
    modelInfo_t model;
    targetModel_t targetModel; //	name of object to attach to
    char targetTag[20];        //	name of tag on target object to attach to
    vec3_t angleOffsets;       //	angle offsets to apply when attaching to target object's tag
    vec3_t originOffsets;      //	angle offsets to apply when attaching to target object's tag
} boltOn_t;

extern boltOn_t knownBoltOns[MAX_GAME_BOLTONS];
extern int numBoltOns;

extern void G_RegisterBoltOns(void);
// extern byte G_AddBoltOn( gentity_t *ent, const char *boltOnName );
// extern void G_RemoveBoltOn( gentity_t *ent, const char *boltOnName );
// extern void Q3_SetActiveBoltOn( int entID, const char *boltOnName );
// extern void Q3_SetActiveBoltOnStartFrame( int entID, int startFrame );
// extern void Q3_SetActiveBoltOnEndFrame( int entID, int endFrame );
// extern void Q3_SetActiveBoltOnAnimLoop( int entID, bool loopAnim );
extern void G_DropBoltOn(gentity_t* ent, const char* boltOnName);
extern byte G_BoltOnNumberForName(gentity_t* ent, const char* boltOnName);
extern void G_InitBoltOnData(gentity_t* ent);

#endif // #ifndef __BOLTONS_H__

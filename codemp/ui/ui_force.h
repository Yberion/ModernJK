/*
===========================================================================
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

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

#include "qcommon/qfiles.h"

#define NUM_FORCE_STAR_IMAGES  9
#define FORCE_NONJEDI	0
#define FORCE_JEDI		1

extern int uiForceSide;
extern int uiJediNonJedi;
extern forceMasteries_t uiForceRank;
extern forceMasteries_t uiMaxRank;
extern int uiForceUsed;
extern int uiForceAvailable;
extern bool gTouchedForce;
extern EnumArray<bool, forcePowers_t, NUM_FORCE_POWERS> uiForcePowersDisabled;
extern EnumArray<forcePowerLevels_t, forcePowers_t, NUM_FORCE_POWERS> uiForcePowersRank;
extern EnumArray<int, forcePowers_t, NUM_FORCE_POWERS> uiForcePowerDarkLight;
extern EnumArray<int, saber_colors_t, NUM_SABER_COLORS> uiSaberColorShaders;
// Dots above or equal to a given rank carry a certain color.

void UI_InitForceShaders(void);
void UI_ReadLegalForce(void);
void UI_DrawTotalForceStars(rectDef_t *rect, float scale, vec4_t color, int textStyle);
void UI_DrawForceStars(rectDef_t *rect, float scale, vec4_t color, int textStyle, forcePowers_t findex, forcePowerLevels_t val, forcePowerLevels_t min, forcePowerLevels_t max) ;
void UI_UpdateClientForcePowers(const char *teamArg);
void UI_SaveForceTemplate();
void UI_UpdateForcePowers();
bool UI_SkinColor_HandleKey(int flags, float *special, fakeAscii_t key, team_t num, team_t min, team_t max, int type);
bool UI_ForceSide_HandleKey(int flags, float *special, fakeAscii_t key, int num, int min, int max, int type);
bool UI_JediNonJedi_HandleKey(int flags, float *special, fakeAscii_t key, int num, int min, int max, int type);
bool UI_ForceMaxRank_HandleKey(int flags, float *special, fakeAscii_t key, forceMasteries_t num, forceMasteries_t min, forceMasteries_t max, int type);
bool UI_ForcePowerRank_HandleKey(int flags, float *special, fakeAscii_t key, forcePowerLevels_t num, forcePowerLevels_t min, forcePowerLevels_t max, int type);
forcePowers_t UI_ForceRankPower( int ownerDraw );
extern void UI_ForceConfigHandle( int oldindex, int newindex );

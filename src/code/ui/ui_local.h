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

#ifndef __UI_LOCAL_H__
#define __UI_LOCAL_H__

#include <string.h>
#include <limits.h>
#include <optional>

#include "../qcommon/q_shared.h"
#include "../rd-common/tr_types.h"
#include "../qcommon/qcommon.h"
#include "ui_public.h"
#include "ui_shared.h"

inline constexpr int MAX_DEFERRED_SCRIPT = 1024;

//
// ui_qmenu.c
//

typedef struct {
    int cursor;
    int scroll;
    int widthInChars;
    char buffer[MAX_EDIT_LINE];
    int maxchars;
    int style;
    int textEnum;   // Label
    int textcolor;  // Normal color
    int textcolor2; // Highlight color
} uifield_t;

extern void Menu_Cache(void);

//
// ui_field.c
//
extern void Field_Clear(uifield_t* edit);
extern void Field_CharEvent(uifield_t* edit, int ch);
extern void Field_Draw(uifield_t* edit, int x, int y, int width, int size, int color, int color2, bool showCursor);

//
// ui_menu.c
//
extern void UI_MainMenu(void);
extern void UI_InGameMenu(const char* holoFlag);
extern void AssetCache(void);
extern void UI_DataPadMenu(void);

//
// ui_connect.c
//
extern void UI_DrawConnect(const char* servername, const char* updateInfoString);
extern void UI_UpdateConnectionString(const char* string);
extern void UI_UpdateConnectionMessageString(char* string);

//
// ui_atoms.cpp
//

typedef struct {
    int frametime;
    int realtime;
    int cursorx;
    int cursory;

    glconfig_t glconfig;
    bool debugMode;
    qhandle_t whiteShader;
    qhandle_t menuBackShader;
    qhandle_t cursor;
    float scalex;
    float scaley;
    // float				bias;
    bool firstdraw;
} uiStatic_t;

extern void UI_FillRect(float x, float y, float width, float height, const float* color);
extern void UI_DrawString(int x, int y, const char* str, int style, vec4_t color);
extern void UI_DrawHandlePic(float x, float y, float w, float h, qhandle_t hShader);
extern void UI_UpdateScreen(void);
extern int UI_RegisterFont(const char* fontName);
extern void UI_SetColor(const float* rgba);
extern char* UI_Cvar_VariableString(const char* var_name);

extern uiStatic_t uis;
extern uiimport_t ui;

inline constexpr int MAX_MODS = 64;

typedef struct {
    const char* modName;
    const char* modDescr;
} modInfo_t;

inline constexpr int SKIN_LENGTH = 16;
inline constexpr int ACTION_BUFFER_SIZE = 128;

typedef struct {
    char name[SKIN_LENGTH];
} skinName_t;

typedef struct {
    char shader[MAX_QPATH];
    char actionText[ACTION_BUFFER_SIZE];
} playerColor_t;

typedef struct {
    char Name[64];
    int SkinHeadCount;
    int SkinHeadMax;
    skinName_t* SkinHead;
    int SkinTorsoCount;
    int SkinTorsoMax;
    skinName_t* SkinTorso;
    int SkinLegCount;
    int SkinLegMax;
    skinName_t* SkinLeg;
    int ColorMax;
    int ColorCount;
    playerColor_t* Color;
} playerSpeciesInfo_t;

// the titles of the datapad's moves database
enum class datapadMoveTitle_t { MD_ACROBATICS = 0, MD_SINGLE_FAST, MD_SINGLE_MEDIUM, MD_SINGLE_STRONG, MD_DUAL_SABERS, MD_SABER_STAFF, MD_MOVE_TITLE_MAX };

// Force Warnings
enum class forceWarning_t { FW_VERY_LIGHT = 0, FW_SEMI_LIGHT, FW_NEUTRAL, FW_SEMI_DARK, FW_VERY_DARK, NUM_FORCE_WARNINGS };
template <> inline constexpr bool enableEnumArithmetic<forceWarning_t> = true;

typedef struct {
    displayContextDef_t uiDC;

    int effectsColor;
    int currentCrosshair;

    modInfo_t modList[MAX_MODS];
    int modIndex;
    int modCount;

    int playerSpeciesMax;
    int playerSpeciesCount;
    playerSpeciesInfo_t* playerSpecies;
    int playerSpeciesIndex;

    char deferredScript[MAX_DEFERRED_SCRIPT];
    itemDef_t* deferredScriptItem;

    itemDef_t* runScriptItem;

    bool inGameLoad;
    // Used by Force Power allocation screen
    short forcePowerUpdated; // Enum of which power had the point allocated
    // Used by Weapon allocation screen
    std::optional<weapon_t> selectedWeapon1;     // 1st weapon chosen
    char selectedWeapon1ItemName[64];            // Item name of weapon chosen
    ammo_t selectedWeapon1AmmoIndex;             // Holds index to ammo
    std::optional<weapon_t> selectedWeapon2;     // 2nd weapon chosen
    char selectedWeapon2ItemName[64];            // Item name of weapon chosen
    ammo_t selectedWeapon2AmmoIndex;             // Holds index to ammo
    std::optional<weapon_t> selectedThrowWeapon; // throwable weapon chosen
    char selectedThrowWeaponItemName[64];        // Item name of weapon chosen
    ammo_t selectedThrowWeaponAmmoIndex;         // Holds index to ammo

    itemDef_t* weapon1ItemButton;
    qhandle_t litWeapon1Icon;
    qhandle_t unlitWeapon1Icon;
    itemDef_t* weapon2ItemButton;
    qhandle_t litWeapon2Icon;
    qhandle_t unlitWeapon2Icon;

    itemDef_t* weaponThrowButton;
    qhandle_t litThrowableIcon;
    qhandle_t unlitThrowableIcon;
    datapadMoveTitle_t movesTitleIndex;
    const char* movesBaseAnim;
    int moveAnimTime;
    int languageCount;
    int languageCountIndex;

    EnumArray<forcePowerLevels_t, forcePowers_t, forcePowers_t::NUM_FORCE_POWERS> forcePowerLevel;
} uiInfo_t;

extern uiInfo_t uiInfo;

//
// ui_main.cpp
//
void _UI_Init(bool inGameLoad);
void _UI_DrawRect(float x, float y, float width, float height, float size, const float* color);
void _UI_MouseEvent(int dx, int dy);
void _UI_KeyEvent(int key, bool down);
void UI_Report(void);

extern char GoToMenu[];

//
// ui_syscalls.cpp
//
int trap_CIN_PlayCinematic(const char* arg0, int xpos, int ypos, int width, int height, cinematicFlags_t bits, const char* psAudioFile /* = nullptr */);
int trap_CIN_StopCinematic(int handle);
void trap_Cvar_Set(const char* var_name, const char* value);
float trap_Cvar_VariableValue(const char* var_name);
void trap_GetGlconfig(glconfig_t* glconfig);
void trap_Key_ClearStates(void);
keyCatchFlags_t trap_Key_GetCatcher(void);
bool trap_Key_GetOverstrikeMode(void);
void trap_Key_SetBinding(fakeAscii_t keynum, const char* binding);
void trap_Key_SetCatcher(keyCatchFlags_t catcher);
void trap_Key_SetOverstrikeMode(bool state);
void trap_R_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
void trap_R_ModelBounds(clipHandle_t model, vec3_t mins, vec3_t maxs);
void trap_R_SetColor(const float* rgba);
void trap_R_ClearScene(void);
void trap_R_AddRefEntityToScene(const refEntity_t* re);
void trap_R_RenderScene(const refdef_t* fd);
void trap_S_StopSounds(void);
sfxHandle_t trap_S_RegisterSound(const char* sample, bool compressed);
void trap_S_StartLocalSound(sfxHandle_t sfx, soundChannel_t channelNum);

void _UI_Refresh(int realtime);

inline constexpr int MAX_FONTS = 64;
extern int registeredFontsCount;
extern int registeredFonts[MAX_FONTS];

#endif

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

inline constexpr int MAX_SIEGE_INFO_SIZE = 16384;

inline constexpr team_t SIEGETEAM_TEAM1 = team_t::TEAM_RED;
inline constexpr team_t SIEGETEAM_TEAM2 = team_t::TEAM_BLUE;

inline constexpr int SIEGE_POINTS_OBJECTIVECOMPLETED = 20;
inline constexpr int SIEGE_POINTS_FINALOBJECTIVECOMPLETED = 30;
inline constexpr int SIEGE_POINTS_TEAMWONROUND = 10;

inline constexpr int SIEGE_ROUND_BEGIN_TIME = 5000; // delay 5 secs after players are in game.

inline constexpr int MAX_SIEGE_CLASSES = 128; // up to 128 classes
inline constexpr int MAX_SIEGE_CLASSES_PER_TEAM = 16;

inline constexpr int MAX_SIEGE_TEAMS = 16; // up to 16 diffent teams

inline constexpr int MAX_EXDATA_ENTS_TO_SEND = MAX_CLIENTS; // max number of extended data for ents to send

// The basic siege player classes
enum class siegePlayerClassFlags_t { SPC_INFANTRY = 0, SPC_VANGUARD, SPC_SUPPORT, SPC_JEDI, SPC_DEMOLITIONIST, SPC_HEAVY_WEAPONS, SPC_MAX };
template <> inline constexpr bool enableEnumArithmetic<siegePlayerClassFlags_t> = true;

enum class siegeClassFlags_t {
    CFL_MORESABERDMG = 0,
    CFL_STRONGAGAINSTPHYSICAL,
    CFL_FASTFORCEREGEN,
    CFL_STATVIEWER,
    CFL_HEAVYMELEE,
    CFL_SINGLE_ROCKET, // has only 1 rocket to use with rocketlauncher
    CFL_CUSTOMSKEL,    // class uses a custom skeleton, be sure to load on server etc
    CFL_EXTRA_AMMO
};

inline constexpr int SIEGE_CLASS_DESC_LEN = 4096;
typedef struct siegeClassDesc_s {
    char desc[SIEGE_CLASS_DESC_LEN];
} siegeClassDesc_t;

typedef struct siegeClass_s {
    char name[512];
    char forcedModel[256];
    char forcedSkin[256];
    char saber1[SABER_NAME_LENGTH], saber2[SABER_NAME_LENGTH];
    int saberStance;
    int weapons;
    EnumArray<forcePowerLevels_t, forcePowers_t, forcePowers_t::NUM_FORCE_POWERS> forcePowerLevels;
    int classflags;
    int maxhealth;
    int starthealth;
    int maxarmor;
    int startarmor;
    float speed;
    bool hasForcedSaberColor;
    int forcedSaberColor;
    bool hasForcedSaber2Color;
    int forcedSaber2Color;
    int invenItems;
    int powerups;
    int uiPortraitShader;
    char uiPortrait[256];
    int classShader;
    siegePlayerClassFlags_t playerClass; // SPC_INFANTRY . ..
} siegeClass_t;

typedef struct siegeTeam_s {
    char name[512];
    siegeClass_t* classes[MAX_SIEGE_CLASSES_PER_TEAM];
    int numClasses;
    int friendlyShader;
} siegeTeam_t;

extern siegeClass_t bgSiegeClasses[MAX_SIEGE_CLASSES];
extern int bgNumSiegeClasses;

extern siegeTeam_t bgSiegeTeams[MAX_SIEGE_TEAMS];
extern int bgNumSiegeTeams;

int BG_SiegeGetValueGroup(char* buf, const char* group, char* outbuf);
int BG_SiegeGetPairedValue(char* buf, const char* key, char* outbuf);
void BG_SiegeStripTabs(char* buf);

void BG_SiegeLoadClasses(siegeClassDesc_t* descBuffer);
void BG_SiegeLoadTeams(void);

siegeTeam_t* BG_SiegeFindThemeForTeam(team_t team);
void BG_PrecacheSabersForSiegeTeam(team_t team);
siegeClass_t* BG_SiegeFindClassByName(const char* classname);
bool BG_SiegeCheckClassLegality(team_t team, char* classname);
void BG_SiegeSetTeamTheme(team_t team, char* themeName);
int BG_SiegeFindClassIndexByName(const char* classname);

// for ui
int BG_GetUIPortrait(const team_t team, const siegePlayerClassFlags_t classIndex, const short cntIndex);
char* BG_GetUIPortraitFile(const team_t team, const siegePlayerClassFlags_t classIndex, const short cntIndex);
siegeClass_t* BG_GetClassOnBaseClass(const team_t team, const siegePlayerClassFlags_t classIndex, const short cntIndex);
int BG_SiegeCountBaseClass(const team_t team, const siegePlayerClassFlags_t classIndex);

extern char siege_info[MAX_SIEGE_INFO_SIZE];
extern int siege_valid;

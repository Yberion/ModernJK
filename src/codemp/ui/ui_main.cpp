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

/*
=======================================================================

USER INTERFACE MAIN

=======================================================================
*/

// use this to get a demo build without an explicit demo build, i.e. to get the demo ui files to build
// #define PRE_RELEASE_TADEMO

#include <stdlib.h>
#include <algorithm>
#include <iterator>
#include <span>
#include <string>
#include <string_view>

#include <ghoul2/G2.h>
#include "ui_local.h"
#include <qcommon/qfiles.h>
#include <qcommon/game_version.h>
#include "ui_force.h"
#include <cgame/animtable.h> //we want this to be compiled into the module because we access it in the shared module.
#include <game/bg_saga.h>
#include "ui_shared.h"

#include <shared/platform/menus/display.h>
#include <shared/rd-common/output/samples.h>

extern void UI_SaberAttachToChar(itemDef_t* item);

EnumArray<const char*, forcePowers_t, forcePowers_t::NUM_FORCE_POWERS> forcepowerDesc = {
    "@MENUS_OF_EFFECT_JEDI_ONLY_NEFFECT",  "@MENUS_DURATION_IMMEDIATE_NAREA",  "@MENUS_DURATION_5_SECONDS_NAREA",     "@MENUS_DURATION_INSTANTANEOUS",
    "@MENUS_INSTANTANEOUS_EFFECT_NAREA",   "@MENUS_DURATION_VARIABLE_20",      "@MENUS_DURATION_INSTANTANEOUS_NAREA", "@MENUS_OF_EFFECT_LIVING_PERSONS",
    "@MENUS_DURATION_VARIABLE_10",         "@MENUS_DURATION_VARIABLE_NAREA",   "@MENUS_DURATION_CONTINUOUS_NAREA",    "@MENUS_OF_EFFECT_JEDI_ALLIES_NEFFECT",
    "@MENUS_EFFECT_JEDI_ALLIES_NEFFECT",   "@MENUS_VARIABLE_NAREA_OF_EFFECT",  "@MENUS_EFFECT_NAREA_OF_EFFECT",       "@SP_INGAME_FORCE_SABER_OFFENSE_DESC",
    "@SP_INGAME_FORCE_SABER_DEFENSE_DESC", "@SP_INGAME_FORCE_SABER_THROW_DESC"};

// Movedata Sounds
enum class datapadMoveSound_t { MDS_NONE = 0, MDS_FORCE_JUMP, MDS_ROLL, MDS_SABER, MDS_MOVE_SOUNDS_MAX };

// Some hard coded badness
// At some point maybe this should be externalized to a .dat file
EnumArray<const char*, datapadMoveTitle_t, datapadMoveTitle_t::MD_MOVE_TITLE_MAX> datapadMoveTitleData = {
    "@MENUS_ACROBATICS", "@MENUS_SINGLE_FAST", "@MENUS_SINGLE_MEDIUM", "@MENUS_SINGLE_STRONG", "@MENUS_DUAL_SABERS", "@MENUS_SABER_STAFF",
};

EnumArray<const char*, datapadMoveTitle_t, datapadMoveTitle_t::MD_MOVE_TITLE_MAX> datapadMoveTitleBaseAnims = {
    "BOTH_RUN1", "BOTH_SABERFAST_STANCE", "BOTH_STAND2", "BOTH_SABERSLOW_STANCE", "BOTH_SABERDUAL_STANCE", "BOTH_SABERSTAFF_STANCE",
};

constexpr int MAX_MOVES = 16;

typedef struct datpadmovedata_s {
    const char* title;
    const char* desc;
    const char* anim;
    datapadMoveSound_t sound;
} datpadmovedata_t;

static EnumArray<datpadmovedata_t[MAX_MOVES], datapadMoveTitle_t, datapadMoveTitle_t::MD_MOVE_TITLE_MAX> datapadMoveData = {
    {{
         // Acrobatics
         {"@MENUS_FORCE_JUMP1", "@MENUS_FORCE_JUMP1_DESC", "BOTH_FORCEJUMP1", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_FORCE_FLIP", "@MENUS_FORCE_FLIP_DESC", "BOTH_FLIP_F", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_ROLL", "@MENUS_ROLL_DESC", "BOTH_ROLL_F", datapadMoveSound_t::MDS_ROLL},
         {"@MENUS_BACKFLIP_OFF_WALL", "@MENUS_BACKFLIP_OFF_WALL_DESC", "BOTH_WALL_FLIP_BACK1", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_SIDEFLIP_OFF_WALL", "@MENUS_SIDEFLIP_OFF_WALL_DESC", "BOTH_WALL_FLIP_RIGHT", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_WALL_RUN", "@MENUS_WALL_RUN_DESC", "BOTH_WALL_RUN_RIGHT", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_WALL_GRAB_JUMP", "@MENUS_WALL_GRAB_JUMP_DESC", "BOTH_FORCEWALLREBOUND_FORWARD", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_RUN_UP_WALL_BACKFLIP", "@MENUS_RUN_UP_WALL_BACKFLIP_DESC", "BOTH_FORCEWALLRUNFLIP_START", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_JUMPUP_FROM_KNOCKDOWN", "@MENUS_JUMPUP_FROM_KNOCKDOWN_DESC", "BOTH_KNOCKDOWN3", datapadMoveSound_t::MDS_NONE},
         {"@MENUS_JUMPKICK_FROM_KNOCKDOWN", "@MENUS_JUMPKICK_FROM_KNOCKDOWN_DESC", "BOTH_KNOCKDOWN2", datapadMoveSound_t::MDS_NONE},
         {"@MENUS_ROLL_FROM_KNOCKDOWN", "@MENUS_ROLL_FROM_KNOCKDOWN_DESC", "BOTH_KNOCKDOWN1", datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
     },
     {
         // Single Saber, Fast Style
         {"@MENUS_STAB_BACK", "@MENUS_STAB_BACK_DESC", "BOTH_A2_STABBACK1", datapadMoveSound_t::MDS_SABER},
         {"@MENUS_LUNGE_ATTACK", "@MENUS_LUNGE_ATTACK_DESC", "BOTH_LUNGE2_B__T_", datapadMoveSound_t::MDS_SABER},
         {"@MENUS_FAST_ATTACK_KATA", "@MENUS_FAST_ATTACK_KATA_DESC", "BOTH_A1_SPECIAL", datapadMoveSound_t::MDS_SABER},
         {"@MENUS_ATTACK_ENEMYONGROUND", "@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_CARTWHEEL", "@MENUS_CARTWHEEL_DESC", "BOTH_ARIAL_RIGHT", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_BOTH_ROLL_STAB", "@MENUS_BOTH_ROLL_STAB2_DESC", "BOTH_ROLL_STAB", datapadMoveSound_t::MDS_SABER},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
     },
     {
         // Single Saber, Medium Style
         {"@MENUS_SLASH_BACK", "@MENUS_SLASH_BACK_DESC", "BOTH_ATTACK_BACK", datapadMoveSound_t::MDS_SABER},
         {"@MENUS_FLIP_ATTACK", "@MENUS_FLIP_ATTACK_DESC", "BOTH_JUMPFLIPSLASHDOWN1", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_MEDIUM_ATTACK_KATA", "@MENUS_MEDIUM_ATTACK_KATA_DESC", "BOTH_A2_SPECIAL", datapadMoveSound_t::MDS_SABER},
         {"@MENUS_ATTACK_ENEMYONGROUND", "@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_CARTWHEEL", "@MENUS_CARTWHEEL_DESC", "BOTH_ARIAL_RIGHT", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_BOTH_ROLL_STAB", "@MENUS_BOTH_ROLL_STAB2_DESC", "BOTH_ROLL_STAB", datapadMoveSound_t::MDS_SABER},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
     },
     {
         // Single Saber, Strong Style
         {"@MENUS_SLASH_BACK", "@MENUS_SLASH_BACK_DESC", "BOTH_ATTACK_BACK", datapadMoveSound_t::MDS_SABER},
         {"@MENUS_JUMP_ATTACK", "@MENUS_JUMP_ATTACK_DESC", "BOTH_FORCELEAP2_T__B_", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_STRONG_ATTACK_KATA", "@MENUS_STRONG_ATTACK_KATA_DESC", "BOTH_A3_SPECIAL", datapadMoveSound_t::MDS_SABER},
         {"@MENUS_ATTACK_ENEMYONGROUND", "@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_CARTWHEEL", "@MENUS_CARTWHEEL_DESC", "BOTH_ARIAL_RIGHT", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_BOTH_ROLL_STAB", "@MENUS_BOTH_ROLL_STAB2_DESC", "BOTH_ROLL_STAB", datapadMoveSound_t::MDS_SABER},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
     },
     {
         // Dual Sabers
         {"@MENUS_SLASH_BACK", "@MENUS_SLASH_BACK_DESC", "BOTH_ATTACK_BACK", datapadMoveSound_t::MDS_SABER},
         {"@MENUS_FLIP_FORWARD_ATTACK", "@MENUS_FLIP_FORWARD_ATTACK_DESC", "BOTH_JUMPATTACK6", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_DUAL_SABERS_TWIRL", "@MENUS_DUAL_SABERS_TWIRL_DESC", "BOTH_SPINATTACK6", datapadMoveSound_t::MDS_SABER},
         {"@MENUS_ATTACK_ENEMYONGROUND", "@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN_DUAL", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_DUAL_SABER_BARRIER", "@MENUS_DUAL_SABER_BARRIER_DESC", "BOTH_A6_SABERPROTECT", datapadMoveSound_t::MDS_SABER},
         {"@MENUS_DUAL_STAB_FRONT_BACK", "@MENUS_DUAL_STAB_FRONT_BACK_DESC", "BOTH_A6_FB", datapadMoveSound_t::MDS_SABER},
         {"@MENUS_DUAL_STAB_LEFT_RIGHT", "@MENUS_DUAL_STAB_LEFT_RIGHT_DESC", "BOTH_A6_LR", datapadMoveSound_t::MDS_SABER},
         {"@MENUS_CARTWHEEL", "@MENUS_CARTWHEEL_DESC", "BOTH_ARIAL_RIGHT", datapadMoveSound_t::MDS_FORCE_JUMP},
         {"@MENUS_BOTH_ROLL_STAB", "@MENUS_BOTH_ROLL_STAB_DESC", "BOTH_ROLL_STAB", datapadMoveSound_t::MDS_SABER},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
         {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
     },
     {// Saber Staff
      {"@MENUS_STAB_BACK", "@MENUS_STAB_BACK_DESC", "BOTH_A2_STABBACK1", datapadMoveSound_t::MDS_SABER},
      {"@MENUS_BACK_FLIP_ATTACK", "@MENUS_BACK_FLIP_ATTACK_DESC", "BOTH_JUMPATTACK7", datapadMoveSound_t::MDS_FORCE_JUMP},
      {"@MENUS_SABER_STAFF_TWIRL", "@MENUS_SABER_STAFF_TWIRL_DESC", "BOTH_SPINATTACK7", datapadMoveSound_t::MDS_SABER},
      {"@MENUS_ATTACK_ENEMYONGROUND", "@MENUS_ATTACK_ENEMYONGROUND_DESC", "BOTH_STABDOWN_STAFF", datapadMoveSound_t::MDS_FORCE_JUMP},
      {"@MENUS_SPINNING_KATA", "@MENUS_SPINNING_KATA_DESC", "BOTH_A7_SOULCAL", datapadMoveSound_t::MDS_SABER},
      {"@MENUS_KICK1", "@MENUS_KICK1_DESC", "BOTH_A7_KICK_F", datapadMoveSound_t::MDS_FORCE_JUMP},
      {"@MENUS_JUMP_KICK", "@MENUS_JUMP_KICK_DESC", "BOTH_A7_KICK_F_AIR", datapadMoveSound_t::MDS_FORCE_JUMP},
      {"@MENUS_BUTTERFLY_ATTACK", "@MENUS_BUTTERFLY_ATTACK_DESC", "BOTH_BUTTERFLY_FR1", datapadMoveSound_t::MDS_SABER},
      {"@MENUS_BOTH_ROLL_STAB", "@MENUS_BOTH_ROLL_STAB2_DESC", "BOTH_ROLL_STAB", datapadMoveSound_t::MDS_SABER},
      {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
      {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
      {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
      {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
      {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
      {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE},
      {nullptr, nullptr, nullptr, datapadMoveSound_t::MDS_NONE}}}};

static siegeClassDesc_t g_UIClassDescriptions[MAX_SIEGE_CLASSES];
static siegeTeam_t *siegeTeam1 = nullptr, *siegeTeam2 = nullptr;
static int g_UIGloballySelectedSiegeClass = -1;

// Cut down version of the stuff used in the game code
// This is just the bare essentials of what we need to load animations properly for ui ghoul2 models.
// This function doesn't need to be sync'd with the BG_ version in bg_panimate.cpp unless some sort of fundamental change
// is made. Just make sure the variables/functions accessed in ui_shared.cpp exist in both modules.
bool UIPAFtextLoaded = false;
animSet_t uiHumanoidAnimations; // humanoid animations are the only ones that are statically allocated.

bgLoadedAnim_t bgAllAnims[MAX_ANIM_FILES];
int uiNumAllAnims = 1; // start off at 0, because 0 will always be assigned to humanoid.

animSet_t* UI_AnimsetAlloc(void)
{
    assert(uiNumAllAnims < MAX_ANIM_FILES);
    bgAllAnims[uiNumAllAnims].anims = (animSet_t*)BG_Alloc(sizeof(animSet_t));

    return bgAllAnims[uiNumAllAnims].anims;
}

/*
======================
UI_ParseAnimationFile

Read a configuration file containing animation counts and rates
models/players/visor/animation.cfg, etc

======================
*/
static char UIPAFtext[60000];
int UI_ParseAnimationFile(const char* filename, animSet_t* animset, bool isHumanoid)
{
    char* text_p;
    int len;
    int i;
    char* token;
    float fps;
    int usedIndex = -1;
    int nextIndex = uiNumAllAnims;

    fileHandle_t f;

    if (!isHumanoid) {
        i = 1;
        while (i < uiNumAllAnims) { // see if it's been loaded already
            if (!Q_stricmp(bgAllAnims[i].filename, filename)) {
                animset = bgAllAnims[i].anims;
                return i; // alright, we already have it.
            }
            i++;
        }

        // Looks like it has not yet been loaded. Allocate space for the anim set if we need to, and continue along.
        if (!animset) {
            if (strstr(filename, "players/_humanoid/")) { // then use the static humanoid set.
                animset = &uiHumanoidAnimations;
                isHumanoid = true;
                nextIndex = 0;
            }
            else {
                animset = UI_AnimsetAlloc();

                if (!animset) {
                    assert(!"Anim set alloc failed!");
                    return -1;
                }
            }
        }
    }
#ifdef _DEBUG
    else {
        assert(animset);
    }
#endif

    // load the file
    if (!UIPAFtextLoaded || !isHumanoid) { // rww - We are always using the same animation config now. So only load it once.
        len = trap->FS_Open(filename, &f, fsMode_t::FS_READ);
        if (!f) {
            return -1;
        }
        if (len >= std::ssize(UIPAFtext) - 1) {
            trap->FS_Close(f);
            Q_Error(errorParm_t::ERR_DROP, "{} exceeds the allowed ui-side animation buffer!", filename);
        }

        trap->FS_Read(UIPAFtext, len, f);
        UIPAFtext[len] = 0;
        trap->FS_Close(f);
    }
    else {
        return 0; // humanoid index
    }

    // parse the text
    text_p = UIPAFtext;

    // FIXME: have some way of playing anims backwards... negative numFrames?

    // initialize anim array so that from 0 to MAX_ANIMATIONS, set default values of 0 1 0 100
    for (animNumber_t anim = animNumber_t::FACE_TALK0; anim < animNumber_t::MAX_ANIMATIONS; anim++) {
        (*animset)[anim].firstFrame = 0;
        (*animset)[anim].numFrames = 0;
        (*animset)[anim].loopFrames = -1;
        (*animset)[anim].frameLerp = 100;
        //		(*animset)[anim].initialLerp = 100;
    }

    COM_BeginParseSession("UI_ParseAnimationFile");

    // read information for each frame
    while (1) {
        token = COM_Parse((const char**)(&text_p));

        if (!token || !token[0]) {
            break;
        }

        const std::optional<animNumber_t> animNum = BG_AnimFromName(token);
        if (!animNum) {
// #ifndef FINAL_BUILD
#ifdef _DEBUG
            // Q_PrintlnWarning("Unknown token {} in {}", token, filename);
#endif
            continue;
        }

        token = COM_Parse((const char**)(&text_p));
        if (!token) {
            break;
        }
        (*animset)[*animNum].firstFrame = atoi(token);

        token = COM_Parse((const char**)(&text_p));
        if (!token) {
            break;
        }
        (*animset)[*animNum].numFrames = atoi(token);

        token = COM_Parse((const char**)(&text_p));
        if (!token) {
            break;
        }
        (*animset)[*animNum].loopFrames = atoi(token);

        token = COM_Parse((const char**)(&text_p));
        if (!token) {
            break;
        }
        fps = Q_atof(token);
        if (fps == 0) {
            fps = 1; // Don't allow divide by zero error
        }
        if (fps < 0) { // backwards
            (*animset)[*animNum].frameLerp = Q_floor(1000.0f / fps);
        }
        else {
            (*animset)[*animNum].frameLerp = Q_ceil(1000.0f / fps);
        }

        //		(*animset)[*animNum].initialLerp = Q_ceilf(1000.0f / Q_fabsf(fps));
    }

#ifdef _DEBUG
    // Check the array, and print the ones that have nothing in them.
    /*
    for(animNumber_t anim = animNumber_t::FACE_TALK0; anim < animNumber_t::MAX_ANIMATIONS; anim++)
    {
        if (BG_AnimName( anim ) != nullptr)		// This animation reference exists.
        {
            if ((*animset)[anim].firstFrame <= 0 && (*animset)[anim].numFrames <=0)
            {	// This is an empty animation reference.
                Q_Println("***ANIMTABLE reference #{} ({}) is empty!",  anim , BG_AnimName( anim ));
            }
        }
    }
    */
#endif // _DEBUG

    if (isHumanoid) {
        bgAllAnims[0].anims = animset;
        Q_strncpyz(bgAllAnims[0].filename, filename, sizeof(bgAllAnims[0].filename));
        UIPAFtextLoaded = true;

        usedIndex = 0;
    }
    else {
        bgAllAnims[nextIndex].anims = animset;
        Q_strncpyz(bgAllAnims[nextIndex].filename, filename, sizeof(bgAllAnims[nextIndex].filename));

        usedIndex = nextIndex;

        if (nextIndex) { // don't bother increasing the number if this ended up as a humanoid load.
            uiNumAllAnims++;
        }
        else {
            UIPAFtextLoaded = true;
            usedIndex = 0;
        }
    }

    return usedIndex;
}

// menuDef_t *Menus_FindByName(const char *p);
void Menu_ShowItemByName(menuDef_t* menu, const char* p, bool bShow);

void UpdateForceUsed();

char holdSPString[MAX_STRING_CHARS] = {0};

uiInfo_t uiInfo;

static void UI_StartServerRefresh(bool full);
static void UI_StopServerRefresh(void);
static void UI_DoServerRefresh(void);
static void UI_BuildServerDisplayList(int force);
static void UI_BuildServerStatus(bool force);
static void UI_BuildFindPlayerList(bool force);
static int QDECL UI_ServersQsortCompare(const void* arg1, const void* arg2);
static int UI_MapCountByGameType(bool singlePlayer);
static int UI_HeadCountByColor(void);
static void UI_ParseGameInfo(const char* teamFile);
static const char* UI_SelectedMap(int index, int* actual);
static int UI_GetIndexFromSelection(int actual);
static void UI_SiegeClassCnt(const team_t team);

team_t uiSkinColor = team_t::TEAM_FREE;
team_t uiHoldSkinColor = team_t::TEAM_FREE; // Stores the skin color so that in non-team games, the player screen remembers the team you chose, in case you're
                                            // coming back from the force powers screen.

static const char* skillLevels[] = {
    "SKILL1", // "Initiate"
    "SKILL2", // "Padawan"
    "SKILL3", // "Jedi"
    "SKILL4", // "Jedi Knight"
    "SKILL5"  // "Jedi Master"
};
static const int numSkillLevels = ARRAY_LEN(skillLevels);

static const EnumArray<const char*, gametype_t, gametype_t::GT_MAX_GAME_TYPE> gameTypes = {
    "FFA", "Holocron", "JediMaster", "Duel", "PowerDuel", "SP", "Team FFA", "Siege", "CTF", "CTY",
};
static const int numGameTypes = static_cast<int>(gameTypes.size());

static const char* netNames[] = {"???", "UDP", nullptr};

static const int numNetNames = ARRAY_LEN(netNames) - 1;

const char* UI_GetStringEdString(const char* refSection, const char* refName);

const char* UI_TeamName(team_t team)
{
    if (team == team_t::TEAM_RED)
        return "RED";
    else if (team == team_t::TEAM_BLUE)
        return "BLUE";
    else if (team == team_t::TEAM_SPECTATOR)
        return "SPECTATOR";
    return "FREE";
}

team_t UI_CvarTeam(const char* name)
{
    return static_cast<team_t>((int)trap->Cvar_VariableValue(name));
}

siegePlayerClassFlags_t UI_CvarSiegeClass(void)
{
    return static_cast<siegePlayerClassFlags_t>((int)trap->Cvar_VariableValue("ui_siege_class"));
}

gametype_t UI_CvarGametype(void)
{
    return static_cast<gametype_t>((int)trap->Cvar_VariableValue("g_gametype"));
}

// returns either string or nullptr for OOR...
//
static const char* GetCRDelineatedString(const char* psStripFileRef, const char* psStripStringRef, int iIndex)
{
    static char sTemp[256];
    const char* psList = UI_GetStringEdString(psStripFileRef, psStripStringRef);
    char* p;

    while (iIndex--) {
        psList = strchr(psList, '\n');
        if (!psList) {
            return nullptr; // OOR
        }
        psList++;
    }

    Q_strncpyz(sTemp, psList, sizeof(sTemp));
    p = strchr(sTemp, '\n');
    if (p) {
        *p = '\0';
    }

    return sTemp;
}

static const char* GetMonthAbbrevString(int iMonth)
{
    const char* p = GetCRDelineatedString("MP_INGAME", "MONTHS", iMonth);

    return p ? p : "Jan"; // sanity
}

// the server browser's source as the UI sees it (ui_netSource), one per master server
enum class uiNetSource_t { UIAS_LOCAL, UIAS_GLOBAL1, UIAS_GLOBAL2, UIAS_GLOBAL3, UIAS_GLOBAL4, UIAS_GLOBAL5, UIAS_FAVORITES };
template <> inline constexpr bool enableEnumArithmetic<uiNetSource_t> = true;

// ui_netSource's number, converted once
static uiNetSource_t UI_NetSource(void)
{
    return static_cast<uiNetSource_t>(ui_netSource.integer);
}

// Convert ui's net source to AS_* used by trap calls.
lanSource_t UI_SourceForLAN(void)
{
    switch (UI_NetSource()) {
    default:
    case uiNetSource_t::UIAS_LOCAL:
        return lanSource_t::AS_LOCAL;
    case uiNetSource_t::UIAS_GLOBAL1:
    case uiNetSource_t::UIAS_GLOBAL2:
    case uiNetSource_t::UIAS_GLOBAL3:
    case uiNetSource_t::UIAS_GLOBAL4:
    case uiNetSource_t::UIAS_GLOBAL5:
        return lanSource_t::AS_GLOBAL;
    case uiNetSource_t::UIAS_FAVORITES:
        return lanSource_t::AS_FAVORITES;
    }
}

/*
static const char *netSources[] = {
    "Local",
    "Internet",
    "Favorites"
//	"Mplayer"
};
static const int numNetSources = ARRAY_LEN(netSources);
*/
static const int numNetSources = 7; // now hard-entered in StringEd file
static const char* GetNetSourceString(uiNetSource_t iSource)
{
    static char result[256] = {0};

    Q_strncpyz(result, GetCRDelineatedString("MP_INGAME", "NET_SOURCES", std::to_underlying(UI_SourceForLAN())), sizeof(result));

    if (iSource >= uiNetSource_t::UIAS_GLOBAL1 && iSource <= uiNetSource_t::UIAS_GLOBAL5) {
        Q_strcat(result, sizeof(result), Q_Format(" {}", iSource).c_str());
    }

    return result;
}

void AssetCache(void)
{
    int n;
    // if (Assets.textFont == nullptr) {
    // }
    // Q_Println("Menu Size: {} bytes", sizeof(Menus));
    uiInfo.uiDC.Assets.gradientBar = trap->R_RegisterShaderNoMip(ASSET_GRADIENTBAR);
    uiInfo.uiDC.Assets.fxBasePic = trap->R_RegisterShaderNoMip(ART_FX_BASE);
    uiInfo.uiDC.Assets.fxPic[0] = trap->R_RegisterShaderNoMip(ART_FX_RED);
    uiInfo.uiDC.Assets.fxPic[1] = trap->R_RegisterShaderNoMip(ART_FX_ORANGE); // trap->R_RegisterShaderNoMip( ART_FX_YELLOW );
    uiInfo.uiDC.Assets.fxPic[2] = trap->R_RegisterShaderNoMip(ART_FX_YELLOW); // trap->R_RegisterShaderNoMip( ART_FX_GREEN );
    uiInfo.uiDC.Assets.fxPic[3] = trap->R_RegisterShaderNoMip(ART_FX_GREEN);  // trap->R_RegisterShaderNoMip( ART_FX_TEAL );
    uiInfo.uiDC.Assets.fxPic[4] = trap->R_RegisterShaderNoMip(ART_FX_BLUE);
    uiInfo.uiDC.Assets.fxPic[5] = trap->R_RegisterShaderNoMip(ART_FX_PURPLE); // trap->R_RegisterShaderNoMip( ART_FX_CYAN );
    uiInfo.uiDC.Assets.fxPic[6] = trap->R_RegisterShaderNoMip(ART_FX_WHITE);
    uiInfo.uiDC.Assets.scrollBar = trap->R_RegisterShaderNoMip(ASSET_SCROLLBAR);
    uiInfo.uiDC.Assets.scrollBarArrowDown = trap->R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWDOWN);
    uiInfo.uiDC.Assets.scrollBarArrowUp = trap->R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWUP);
    uiInfo.uiDC.Assets.scrollBarArrowLeft = trap->R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWLEFT);
    uiInfo.uiDC.Assets.scrollBarArrowRight = trap->R_RegisterShaderNoMip(ASSET_SCROLLBAR_ARROWRIGHT);
    uiInfo.uiDC.Assets.scrollBarThumb = trap->R_RegisterShaderNoMip(ASSET_SCROLL_THUMB);
    uiInfo.uiDC.Assets.sliderBar = trap->R_RegisterShaderNoMip(ASSET_SLIDER_BAR);
    uiInfo.uiDC.Assets.sliderThumb = trap->R_RegisterShaderNoMip(ASSET_SLIDER_THUMB);

    // Icons for various server settings.
    uiInfo.uiDC.Assets.needPass = trap->R_RegisterShaderNoMip("gfx/menus/needpass");
    uiInfo.uiDC.Assets.noForce = trap->R_RegisterShaderNoMip("gfx/menus/noforce");
    uiInfo.uiDC.Assets.forceRestrict = trap->R_RegisterShaderNoMip("gfx/menus/forcerestrict");
    uiInfo.uiDC.Assets.saberOnly = trap->R_RegisterShaderNoMip("gfx/menus/saberonly");
    uiInfo.uiDC.Assets.trueJedi = trap->R_RegisterShaderNoMip("gfx/menus/truejedi");

    for (n = 0; n < NUM_CROSSHAIRS; n++) {
        uiInfo.uiDC.Assets.crosshairShader[n] = trap->R_RegisterShaderNoMip(Q_Format("gfx/2d/crosshair{:c}", 'a' + n).c_str());
    }
}

void _UI_DrawSides(float x, float y, float w, float h, float size)
{
    size *= uiInfo.uiDC.xscale;
    trap->R_DrawStretchPic(x, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
    trap->R_DrawStretchPic(x + w - size, y, size, h, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
}

void _UI_DrawTopBottom(float x, float y, float w, float h, float size)
{
    size *= uiInfo.uiDC.yscale;
    trap->R_DrawStretchPic(x, y, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
    trap->R_DrawStretchPic(x, y + h - size, w, size, 0, 0, 0, 0, uiInfo.uiDC.whiteShader);
}
/*
================
UI_DrawRect

Coordinates are 640*480 virtual values
=================
*/
void _UI_DrawRect(float x, float y, float width, float height, float size, const float* color)
{
    trap->R_SetColor(color);

    _UI_DrawTopBottom(x, y, width, height, size);
    _UI_DrawSides(x, y, width, height, size);

    trap->R_SetColor(nullptr);
}

int MenuFontToHandle(menuFont_t iMenuFont)
{
    switch (iMenuFont) {
    case menuFont_t::FONT_SMALL:
        return uiInfo.uiDC.Assets.qhSmallFont;
    case menuFont_t::FONT_MEDIUM:
        return uiInfo.uiDC.Assets.qhMediumFont;
    case menuFont_t::FONT_LARGE:
        return uiInfo.uiDC.Assets.qhBigFont;
    case menuFont_t::FONT_SMALL2:
        return uiInfo.uiDC.Assets.qhSmall2Font;
    default:
        break;
    }

    return uiInfo.uiDC.Assets.qhMediumFont; // 0;
}

int Text_Width(const char* text, float scale, menuFont_t iMenuFont)
{
    int iFontIndex = MenuFontToHandle(iMenuFont);

    return trap->R_Font_StrLenPixels(text, iFontIndex, scale);
}

int Text_Height(const char* text, float scale, menuFont_t iMenuFont)
{
    int iFontIndex = MenuFontToHandle(iMenuFont);

    return trap->R_Font_HeightPixels(iFontIndex, scale);
}

void Text_Paint(float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit, int style, menuFont_t iMenuFont)
{
    int iStyleOR = 0;

    int iFontIndex = MenuFontToHandle(iMenuFont);
    //
    // kludge.. convert JK2 menu styles to SOF2 printstring ctrl codes...
    //
    switch (style) {
    case ITEM_TEXTSTYLE_NORMAL:
        iStyleOR = 0;
        break; // JK2 normal text
    case ITEM_TEXTSTYLE_BLINK:
        iStyleOR = (int)STYLE_BLINK;
        break; // JK2 fast blinking
    case ITEM_TEXTSTYLE_PULSE:
        iStyleOR = (int)STYLE_BLINK;
        break; // JK2 slow pulsing
    case ITEM_TEXTSTYLE_SHADOWED:
        iStyleOR = (int)STYLE_DROPSHADOW;
        break; // JK2 drop shadow
    case ITEM_TEXTSTYLE_OUTLINED:
        iStyleOR = (int)STYLE_DROPSHADOW;
        break; // JK2 drop shadow
    case ITEM_TEXTSTYLE_OUTLINESHADOWED:
        iStyleOR = (int)STYLE_DROPSHADOW;
        break; // JK2 drop shadow
    case ITEM_TEXTSTYLE_SHADOWEDMORE:
        iStyleOR = (int)STYLE_DROPSHADOW;
        break; // JK2 drop shadow
    }

    trap->R_Font_DrawString(x,                     // int ox
                            y,                     // int oy
                            text,                  // const char *text
                            color,                 // paletteRGBA_c c
                            iStyleOR | iFontIndex, // const int iFontHandle
                            !limit ? -1 : limit,   // iCharLimit (-1 = none)
                            scale                  // const float scale = 1.0f
    );
}

void Text_PaintWithCursor(float x, float y, float scale, vec4_t color, const char* text, int cursorPos, char cursor, int limit, int style, menuFont_t iMenuFont)
{
    Text_Paint(x, y, scale, color, text, 0, limit, style, iMenuFont);

    // now print the cursor as well...  (excuse the braces, it's for porting C++ to C)
    //
    {
        char sTemp[1024];
        int iCopyCount = limit > 0 ? Q_min((int)strlen(text), limit) : (int)strlen(text);
        iCopyCount = Q_min(iCopyCount, cursorPos);
        iCopyCount = Q_min(iCopyCount, (int)sizeof(sTemp) - 1);

        // copy text into temp buffer for pixel measure...
        //
        strncpy(sTemp, text, iCopyCount);
        sTemp[iCopyCount] = '\0';

        {
            int iFontIndex = MenuFontToHandle(iMenuFont);
            int iNextXpos = trap->R_Font_StrLenPixels(sTemp, iFontIndex, scale);

            Text_Paint(x + iNextXpos, y, scale, color, Q_Format("{}", cursor).c_str(), 0, limit, style | ITEM_TEXTSTYLE_BLINK, iMenuFont);
        }
    }
}

// maxX param is initially an X limit, but is also used as feedback. 0 = text was clipped to fit within, else maxX = next pos
//
static void Text_Paint_Limit(float* maxX, float x, float y, float scale, vec4_t color, const char* text, float adjust, int limit, menuFont_t iMenuFont)
{
    // this is kinda dirty, but...
    //
    int iFontIndex = MenuFontToHandle(iMenuFont);

    // float fMax = *maxX;
    int iPixelLen = trap->R_Font_StrLenPixels(text, iFontIndex, scale);
    if (x + iPixelLen > *maxX) {
        // whole text won't fit, so we need to print just the amount that does...
        //  Ok, this is slow and tacky, but only called occasionally, and it works...
        //
        char sTemp[4096] = {0}; // lazy assumption
        const char* psText = text;
        char* psOut = &sTemp[0];
        char* psOutLastGood = psOut;
        unsigned int uiLetter;

        while (*psText && (x + trap->R_Font_StrLenPixels(sTemp, iFontIndex, scale) <= *maxX) && psOut < &sTemp[sizeof(sTemp) - 1] // sanity
        ) {
            int iAdvanceCount;
            psOutLastGood = psOut;

            uiLetter = trap->R_AnyLanguage_ReadCharFromString(psText, &iAdvanceCount, nullptr);
            psText += iAdvanceCount;

            if (uiLetter > 255) {
                *psOut++ = uiLetter >> 8;
                *psOut++ = uiLetter & 0xFF;
            }
            else {
                *psOut++ = uiLetter & 0xFF;
            }
        }
        *psOutLastGood = '\0';

        *maxX = 0; // feedback
        Text_Paint(x, y, scale, color, sTemp, adjust, limit, ITEM_TEXTSTYLE_NORMAL, iMenuFont);
    }
    else {
        // whole text fits fine, so print it all...
        //
        *maxX = x + iPixelLen; // feedback the next position, as the caller expects
        Text_Paint(x, y, scale, color, text, adjust, limit, ITEM_TEXTSTYLE_NORMAL, iMenuFont);
    }
}

void UI_LoadNonIngame()
{
    const char* menuSet = UI_Cvar_VariableString("ui_menuFilesMP");
    if (menuSet == nullptr || menuSet[0] == '\0') {
        menuSet = "ui/jampmenus.txt";
    }
    UI_LoadMenus(menuSet, false);
    uiInfo.inGameLoad = false;
}

/*
===============
UI_BuildPlayerList
===============
*/
static void UI_BuildPlayerList()
{
    uiClientState_t cs;
    int n, count, playerTeamNumber;
    team_t team, team2;
    char info[MAX_INFO_STRING];

    trap->GetClientState(&cs);
    trap->GetConfigString(CS_PLAYERS + cs.clientNum, info, MAX_INFO_STRING);
    uiInfo.playerNumber = cs.clientNum;
    uiInfo.teamLeader = atoi(Info_ValueForKey(info, "tl"));
    team = static_cast<team_t>(atoi(Info_ValueForKey(info, "t")));
    trap->GetConfigString(CS_SERVERINFO, info, sizeof(info));
    count = atoi(Info_ValueForKey(info, "sv_maxclients"));
    uiInfo.playerCount = 0;
    uiInfo.myTeamCount = 0;
    playerTeamNumber = 0;
    for (n = 0; n < count; n++) {
        trap->GetConfigString(CS_PLAYERS + n, info, MAX_INFO_STRING);

        if (info[0]) {
            Q_strncpyz(uiInfo.playerNames[uiInfo.playerCount], Info_ValueForKey(info, "n"), MAX_NETNAME);
            Q_StripColor(uiInfo.playerNames[uiInfo.playerCount]);
            uiInfo.playerIndexes[uiInfo.playerCount] = n;
            uiInfo.playerCount++;
            team2 = static_cast<team_t>(atoi(Info_ValueForKey(info, "t")));
            if (team2 == team && n != uiInfo.playerNumber) {
                Q_strncpyz(uiInfo.teamNames[uiInfo.myTeamCount], Info_ValueForKey(info, "n"), MAX_NETNAME);
                Q_StripColor(uiInfo.teamNames[uiInfo.myTeamCount]);
                uiInfo.teamClientNums[uiInfo.myTeamCount] = n;
                if (uiInfo.playerNumber == n) {
                    playerTeamNumber = uiInfo.myTeamCount;
                }
                uiInfo.myTeamCount++;
            }
        }
    }

    if (!uiInfo.teamLeader) {
        trap->Cvar_Set("cg_selectedPlayer", Q_Format("{}", playerTeamNumber).c_str());
    }

    n = trap->Cvar_VariableValue("cg_selectedPlayer");
    if (n < 0 || n > uiInfo.myTeamCount) {
        n = 0;
    }

    if (n < uiInfo.myTeamCount) {
        trap->Cvar_Set("cg_selectedPlayerName", uiInfo.teamNames[n]);
    }
    else {
        trap->Cvar_Set("cg_selectedPlayerName", "Everyone");
    }

    if (team == team_t::TEAM_FREE || team == team_t::TEAM_SPECTATOR || !uiInfo.teamLeader) {
        n = uiInfo.myTeamCount;
        trap->Cvar_Set("cg_selectedPlayer", Q_Format("{}", n).c_str());
        trap->Cvar_Set("cg_selectedPlayerName", "N/A");
    }
}

void UI_SetActiveMenu(uiMenuCommand_t menu)
{
    char buf[256];

    // this should be the ONLY way the menu system is brought up
    // enusure minumum menu data is cached
    if (Menu_Count() > 0) {
        vec3_t v;
        v[0] = v[1] = v[2] = 0;
        switch (menu) {
        case uiMenuCommand_t::UIMENU_NONE:
            trap->Key_SetCatcher(trap->Key_GetCatcher() & ~keyCatchFlags_t::KEYCATCH_UI);
            trap->Key_ClearStates();
            trap->Cvar_Set("cl_paused", "0");
            Menus_CloseAll();

            return;
        case uiMenuCommand_t::UIMENU_MAIN: {
            //	trap->Cvar_Set( "sv_killserver", "1" );
            trap->Key_SetCatcher(keyCatchFlags_t::KEYCATCH_UI);
            //	Menu_PlaySound(trap->sound->Register("sound/misc/menu_background.wav"));
            //	trap->sound->StartMusic("sound/misc/menu_background.wav", "");
            if (uiInfo.inGameLoad)
                UI_LoadNonIngame();

            Menus_CloseAll();
            Menus_ActivateByName("main");
            trap->Cvar_VariableStringBuffer("com_errorMessage", buf, sizeof(buf));

            if (buf[0]) {
                if (!ui_singlePlayerActive.integer) {
                    Menus_ActivateByName("error_popmenu");
                }
                else {
                    trap->Cvar_Set("com_errorMessage", "");
                }
            }
            return;
        }

        case uiMenuCommand_t::UIMENU_TEAM:
            trap->Key_SetCatcher(keyCatchFlags_t::KEYCATCH_UI);
            Menus_ActivateByName("team");
            return;
        case uiMenuCommand_t::UIMENU_POSTGAME:
            // trap->Cvar_Set( "sv_killserver", "1" );
            trap->Key_SetCatcher(keyCatchFlags_t::KEYCATCH_UI);
            if (uiInfo.inGameLoad)
                UI_LoadNonIngame();
            Menus_CloseAll();
            Menus_ActivateByName("endofgame");
            return;
        case uiMenuCommand_t::UIMENU_INGAME:
            trap->Cvar_Set("cl_paused", "1");
            trap->Key_SetCatcher(keyCatchFlags_t::KEYCATCH_UI);
            UI_BuildPlayerList();
            Menus_CloseAll();
            Menus_ActivateByName("ingame");
            return;
        case uiMenuCommand_t::UIMENU_PLAYERCONFIG:
            // trap->Cvar_Set( "cl_paused", "1" );
            trap->Key_SetCatcher(keyCatchFlags_t::KEYCATCH_UI);
            UI_BuildPlayerList();
            Menus_CloseAll();
            Menus_ActivateByName("ingame_player");
            UpdateForceUsed();
            return;
        case uiMenuCommand_t::UIMENU_PLAYERFORCE:
            // trap->Cvar_Set( "cl_paused", "1" );
            trap->Key_SetCatcher(keyCatchFlags_t::KEYCATCH_UI);
            UI_BuildPlayerList();
            Menus_CloseAll();
            Menus_ActivateByName("ingame_playerforce");
            UpdateForceUsed();
            return;
        case uiMenuCommand_t::UIMENU_SIEGEMESSAGE:
            // trap->Cvar_Set( "cl_paused", "1" );
            trap->Key_SetCatcher(keyCatchFlags_t::KEYCATCH_UI);
            Menus_CloseAll();
            Menus_ActivateByName("siege_popmenu");
            return;
        case uiMenuCommand_t::UIMENU_SIEGEOBJECTIVES:
            // trap->Cvar_Set( "cl_paused", "1" );
            trap->Key_SetCatcher(keyCatchFlags_t::KEYCATCH_UI);
            Menus_CloseAll();
            Menus_ActivateByName("ingame_siegeobjectives");
            return;
        case uiMenuCommand_t::UIMENU_VOICECHAT:
            // trap->Cvar_Set( "cl_paused", "1" );
            // No chatin non-siege games.

            if (UI_CvarGametype() < gametype_t::GT_TEAM) {
                return;
            }

            trap->Key_SetCatcher(keyCatchFlags_t::KEYCATCH_UI);
            Menus_CloseAll();
            Menus_ActivateByName("ingame_voicechat");
            return;
        case uiMenuCommand_t::UIMENU_CLOSEALL:
            Menus_CloseAll();
            return;
        case uiMenuCommand_t::UIMENU_CLASSSEL:
            trap->Key_SetCatcher(keyCatchFlags_t::KEYCATCH_UI);
            Menus_CloseAll();
            Menus_ActivateByName("ingame_siegeclass");
            return;
        }
    }
}

void UI_DrawCenteredPic(qhandle_t image, int w, int h)
{
    int x, y;
    x = (SCREEN_WIDTH - w) / 2;
    y = (SCREEN_HEIGHT - h) / 2;
    UI_DrawHandlePic(x, y, w, h, image);
}

int frameCount = 0;
int startTime;

static void UI_BuildPlayerList();
char parsedFPMessage[1024];

extern int FPMessageTime;

void Text_PaintCenter(float x, float y, float scale, vec4_t color, const char* text, float adjust, menuFont_t iMenuFont);

const char* UI_GetStringEdString(const char* refSection, const char* refName)
{
    static char text[1024] = {0};

    trap->SE_GetStringTextString(Q_Format("{}_{}", refSection, refName).c_str(), text, sizeof(text));
    return text;
}

void UI_SetColor(const float* rgba)
{
    trap->R_SetColor(rgba);
}

/*
=================
_UI_Shutdown
=================
*/
void UI_CleanupGhoul2(void);
void UI_FreeAllSpecies(void);

void UI_Shutdown(void)
{
    trap->LAN_SaveCachedServers();
    UI_CleanupGhoul2();
    UI_FreeAllSpecies();
}

char* defaultMenu = nullptr;

char* GetMenuBuffer(const char* filename)
{
    int len;
    fileHandle_t f;
    static char buf[MAX_MENUFILE];

    len = trap->FS_Open(filename, &f, fsMode_t::FS_READ);
    if (!f) {
        Q_Println(S_COLOR_RED "menu file not found: {}, using default", filename);
        return defaultMenu;
    }
    if (len >= MAX_MENUFILE) {
        Q_Println(S_COLOR_RED "menu file too large: {} is {}, max allowed is {}", filename, len, MAX_MENUFILE);
        trap->FS_Close(f);
        return defaultMenu;
    }

    trap->FS_Read(buf, len, f);
    buf[len] = 0;
    trap->FS_Close(f);
    // COM_Compress(buf);
    return buf;
}

bool Asset_Parse()
{
    const char* token;

    if (!PC_ParseString(&token))
        return false;
    if (Q_stricmp(token, "{") != 0) {
        return false;
    }

    while (1) {
        if (!PC_ParseString(&token))
            return false;

        if (Q_stricmp(token, "}") == 0) {
            return true;
        }

        // font
        if (Q_stricmp(token, "font") == 0) {
            int pointSize;
            const char* fontName;
            if (!PC_ParseStringMem(&fontName) || !PC_ParseInt(&pointSize)) {
                return false;
            }
            // trap->R_RegisterFont(tempStr, pointSize, &uiInfo.uiDC.Assets.textFont);
            uiInfo.uiDC.Assets.qhMediumFont = trap->R_RegisterFont(fontName);
            uiInfo.uiDC.Assets.fontRegistered = true;
            continue;
        }

        if (Q_stricmp(token, "smallFont") == 0) {
            int pointSize;
            const char* fontName;
            if (!PC_ParseStringMem(&fontName) || !PC_ParseInt(&pointSize)) {
                return false;
            }
            // trap->R_RegisterFont(fontName, pointSize, &uiInfo.uiDC.Assets.smallFont);
            uiInfo.uiDC.Assets.qhSmallFont = trap->R_RegisterFont(fontName);
            continue;
        }

        if (Q_stricmp(token, "small2Font") == 0) {
            int pointSize;
            const char* fontName;
            if (!PC_ParseStringMem(&fontName) || !PC_ParseInt(&pointSize)) {
                return false;
            }
            // trap->R_RegisterFont(fontName, pointSize, &uiInfo.uiDC.Assets.smallFont);
            uiInfo.uiDC.Assets.qhSmall2Font = trap->R_RegisterFont(fontName);
            continue;
        }

        if (Q_stricmp(token, "bigFont") == 0) {
            int pointSize;
            const char* fontName;
            if (!PC_ParseStringMem(&fontName) || !PC_ParseInt(&pointSize)) {
                return false;
            }
            // trap->R_RegisterFont(fontName, pointSize, &uiInfo.uiDC.Assets.bigFont);
            uiInfo.uiDC.Assets.qhBigFont = trap->R_RegisterFont(fontName);
            continue;
        }

        if (Q_stricmp(token, "cursor") == 0) {
            if (!PC_ParseStringMem(&uiInfo.uiDC.Assets.cursorStr)) {
                Q_Println(S_COLOR_YELLOW "Bad 1st parameter for keyword 'cursor'");
                return false;
            }
            uiInfo.uiDC.Assets.cursor = trap->R_RegisterShaderNoMip(uiInfo.uiDC.Assets.cursorStr);
            continue;
        }

        // gradientbar
        if (Q_stricmp(token, "gradientbar") == 0) {
            if (!PC_ParseString(&token)) {
                return false;
            }
            uiInfo.uiDC.Assets.gradientBar = trap->R_RegisterShaderNoMip(token);
            continue;
        }

        // enterMenuSound
        if (Q_stricmp(token, "menuEnterSound") == 0) {
            if (!PC_ParseString(&token)) {
                return false;
            }
            uiInfo.uiDC.Assets.menuEnterSound = trap->sound->Register(token);
            continue;
        }

        // exitMenuSound
        if (Q_stricmp(token, "menuExitSound") == 0) {
            if (!PC_ParseString(&token)) {
                return false;
            }
            uiInfo.uiDC.Assets.menuExitSound = trap->sound->Register(token);
            continue;
        }

        // itemFocusSound
        if (Q_stricmp(token, "itemFocusSound") == 0) {
            if (!PC_ParseString(&token)) {
                return false;
            }
            uiInfo.uiDC.Assets.itemFocusSound = trap->sound->Register(token);
            continue;
        }

        // menuBuzzSound
        if (Q_stricmp(token, "menuBuzzSound") == 0) {
            if (!PC_ParseString(&token)) {
                return false;
            }
            uiInfo.uiDC.Assets.menuBuzzSound = trap->sound->Register(token);
            continue;
        }

        if (Q_stricmp(token, "fadeClamp") == 0) {
            if (!PC_ParseFloat(&uiInfo.uiDC.Assets.fadeClamp)) {
                return false;
            }
            continue;
        }

        if (Q_stricmp(token, "fadeCycle") == 0) {
            if (!PC_ParseInt(&uiInfo.uiDC.Assets.fadeCycle)) {
                return false;
            }
            continue;
        }

        if (Q_stricmp(token, "fadeAmount") == 0) {
            if (!PC_ParseFloat(&uiInfo.uiDC.Assets.fadeAmount)) {
                return false;
            }
            continue;
        }

        if (Q_stricmp(token, "shadowX") == 0) {
            if (!PC_ParseFloat(&uiInfo.uiDC.Assets.shadowX)) {
                return false;
            }
            continue;
        }

        if (Q_stricmp(token, "shadowY") == 0) {
            if (!PC_ParseFloat(&uiInfo.uiDC.Assets.shadowY)) {
                return false;
            }
            continue;
        }

        if (Q_stricmp(token, "shadowColor") == 0) {
            if (!PC_ParseColor(&uiInfo.uiDC.Assets.shadowColor)) {
                return false;
            }
            uiInfo.uiDC.Assets.shadowFadeClamp = uiInfo.uiDC.Assets.shadowColor[3];
            continue;
        }

        if (Q_stricmp(token, "moveRollSound") == 0) {
            if (PC_ParseString(&token)) {
                uiInfo.uiDC.Assets.moveRollSound = trap->sound->Register(token);
            }
            continue;
        }

        if (Q_stricmp(token, "moveJumpSound") == 0) {
            if (PC_ParseString(&token)) {
                uiInfo.uiDC.Assets.moveJumpSound = trap->sound->Register(token);
            }

            continue;
        }
        if (Q_stricmp(token, "datapadmoveSaberSound1") == 0) {
            if (PC_ParseString(&token)) {
                uiInfo.uiDC.Assets.datapadmoveSaberSound1 = trap->sound->Register(token);
            }

            continue;
        }

        if (Q_stricmp(token, "datapadmoveSaberSound2") == 0) {
            if (PC_ParseString(&token)) {
                uiInfo.uiDC.Assets.datapadmoveSaberSound2 = trap->sound->Register(token);
            }

            continue;
        }

        if (Q_stricmp(token, "datapadmoveSaberSound3") == 0) {
            if (PC_ParseString(&token)) {
                uiInfo.uiDC.Assets.datapadmoveSaberSound3 = trap->sound->Register(token);
            }

            continue;
        }

        if (Q_stricmp(token, "datapadmoveSaberSound4") == 0) {
            if (PC_ParseString(&token)) {
                uiInfo.uiDC.Assets.datapadmoveSaberSound4 = trap->sound->Register(token);
            }

            continue;
        }

        if (Q_stricmp(token, "datapadmoveSaberSound5") == 0) {
            if (PC_ParseString(&token)) {
                uiInfo.uiDC.Assets.datapadmoveSaberSound5 = trap->sound->Register(token);
            }

            continue;
        }

        if (Q_stricmp(token, "datapadmoveSaberSound6") == 0) {
            if (PC_ParseString(&token)) {
                uiInfo.uiDC.Assets.datapadmoveSaberSound6 = trap->sound->Register(token);
            }

            continue;
        }

        // precaching various sound files used in the menus
        if (Q_stricmp(token, "precacheSound") == 0) {
            const char* tempStr;
            if (PC_Script_Parse(&tempStr)) {
                char* soundFile;
                do {
                    soundFile = COM_ParseExt(&tempStr, false);
                    if (soundFile[0] != 0 && soundFile[0] != ';') {
                        trap->sound->Register(soundFile);
                    }
                } while (soundFile[0]);
            }
            continue;
        }
    }
    return false;
}

void UI_Report(void)
{
    String_Report();
    // Font_Report();
}

void UI_LoadMenus(const char* menuFile, bool reset)
{
    std::string text;
    //	int start = trap->Milliseconds();

    if (!UI_ReadMenuFile(menuFile, text)) {
        Q_Println(S_COLOR_YELLOW "menu file not found: {}, using default", menuFile);
        menuFile = "ui/jampmenus.txt";
        if (!UI_ReadMenuFile(menuFile, text)) {
            Q_Error(errorParm_t::ERR_DROP, S_COLOR_RED "default menu file not found: ui/jampmenus.txt, unable to continue!");
        }
    }

    if (reset) {
        Menu_Reset();
    }

    UI_ParseMenuList(menuFile, text.c_str());

    //	Q_Println("UI menu load time = {} milli seconds", trap->Milliseconds() - start);
}

void UI_Load(void)
{
    const char* menuSet;
    char lastName[1024];
    menuDef_t* menu = Menu_GetFocused();

    if (menu && menu->window.name) {
        Q_strncpyz(lastName, menu->window.name, sizeof(lastName));
    }
    else {
        lastName[0] = 0;
    }

    if (uiInfo.inGameLoad) {
        menuSet = "ui/jampingame.txt";
    }
    else {
        menuSet = UI_Cvar_VariableString("ui_menuFilesMP");
    }
    if (menuSet == nullptr || menuSet[0] == '\0') {
        menuSet = "ui/jampmenus.txt";
    }

    String_Init();

#ifdef PRE_RELEASE_TADEMO
    UI_ParseGameInfo("demogameinfo.txt");
#else
    UI_ParseGameInfo("ui/jamp/gameinfo.txt");
#endif
    UI_LoadArenas();
    UI_LoadBots();

    UI_LoadMenus(menuSet, true);
    Menus_CloseAll();
    Menus_ActivateByName(lastName);
}

char sAll[15] = {0};
char sJediAcademy[30] = {0};
const char* UI_FilterDescription(int value)
{
    if (value <= 0 || value > uiInfo.modCount) {
        return sAll;
    }

    return uiInfo.modList[value - 1].modDescr;
}

const char* UI_FilterDir(int value)
{
    if (value <= 0 || value > uiInfo.modCount) {
        return "";
    }

    return uiInfo.modList[value - 1].modName;
}

static const char* handicapValues[] = {"None", "95", "90", "85", "80", "75", "70", "65", "60", "55",   "50",
                                       "45",   "40", "35", "30", "25", "20", "15", "10", "5",  nullptr};

static void UI_DrawHandicap(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    int i, h;

    h = Com_Clamp(5, 100, trap->Cvar_VariableValue("handicap"));
    i = 20 - h / 5;

    Text_Paint(rect->x, rect->y, scale, color, handicapValues[i], 0, 0, textStyle, iMenuFont);
}

// a row's value cut to the width the menu gives it (the item's special), as a long name is
static void UI_DrawCutText(std::string text, rectDef_t* rect, float width, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    while (text.size() > 1 && width > 0.0f && static_cast<float>(Text_Width(text.c_str(), scale, iMenuFont)) > width) {
        text.pop_back();
    }

    Text_Paint(rect->x, rect->y, scale, color, text.c_str(), 0, 0, textStyle, iMenuFont);
}

// the sound's output (s_device): the system's default, or the output's name cut to the width the menu gives the value
// (the item's special)
static void UI_DrawSoundDevice(rectDef_t* rect, float width, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    std::string text = UI_Cvar_VariableString("s_device");

    if (text.empty()) {
        text = UI_GetStringEdString("MODERNJK", "SOUND_DEVICE_DEFAULT");
    }

    UI_DrawCutText(std::move(text), rect, width, scale, color, textStyle, iMenuFont);
}

// the display's settings as the video menu edits them before "Apply": the copies ui_r_displayState, ui_r_screen,
// ui_r_windowWidth, ui_r_windowHeight and ui_r_exclusiveMode
static platform::WindowSettings UI_DisplayChoice()
{
    return {.state = platform::DisplayStateOf(static_cast<int>(trap->Cvar_VariableValue("ui_r_displayState"))),
            .screen = static_cast<int>(trap->Cvar_VariableValue("ui_r_screen")),
            .width = static_cast<int>(trap->Cvar_VariableValue("ui_r_windowWidth")),
            .height = static_cast<int>(trap->Cvar_VariableValue("ui_r_windowHeight")),
            .exclusive = platform::ParseMode(UI_Cvar_VariableString("ui_r_exclusiveMode"))};
}

// whether the refresh rate row offers a choice, which the menu reads to grey the row (ui_r_refreshChoice)
static void UI_SetRefreshChoice(const platform::WindowSettings& settings)
{
    trap->Cvar_SetValue("ui_r_refreshChoice", platform::RefreshHasChoice(settings) ? 1 : 0);
}

// the display's settings the screen, resolution and refresh rate rows change, into the menu's copies
static void UI_SetDisplayChoice(const platform::WindowSettings& settings)
{
    trap->Cvar_SetValue("ui_r_screen", settings.screen);
    trap->Cvar_SetValue("ui_r_windowWidth", settings.width);
    trap->Cvar_SetValue("ui_r_windowHeight", settings.height);
    trap->Cvar_Set("ui_r_exclusiveMode", platform::ModeText(settings.exclusive).c_str());
    UI_SetRefreshChoice(settings);
}

// the screen, resolution and refresh rate rows: the value each shows for the settings the menu edits, cut to the width
// the menu gives it
static void UI_DrawDisplayRow(int ownerDraw, rectDef_t* rect, float width, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    const std::span<const platform::Screen> screens = trap->ext.Screens();
    const platform::WindowSettings choice = UI_DisplayChoice();
    std::string text;

    if (ownerDraw == UI_DISPLAY_SCREEN) {
        text = platform::ScreenText(screens, choice.screen);
    }
    else if (ownerDraw == UI_DISPLAY_RESOLUTION) {
        text = platform::ResolutionText(screens, choice, UI_GetStringEdString("MODERNJK", "DISPLAY_DESKTOP"));
    }
    else {
        text = platform::RefreshText(screens, choice);
    }

    UI_DrawCutText(std::move(text), rect, width, scale, color, textStyle, iMenuFont);
}

// the multisampling's row: Off, or the samples a pixel ("4x"), as the renderer applies them with the driver's limit
static void UI_DrawMultisample(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    const int samples = render_output::AllowedSamples(static_cast<int>(trap->Cvar_VariableValue("r_ext_multisample")),
                                                      static_cast<int>(trap->Cvar_VariableValue("r_ext_multisample_avail")));
    const std::string text = samples == 0 ? std::string(UI_GetStringEdString("MENUS", "OFF")) : std::string(Q_Format("{}x", samples).c_str());
    Text_Paint(rect->x, rect->y, scale, color, text.c_str(), 0, 0, textStyle, iMenuFont);
}

static void UI_DrawClanName(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    Text_Paint(rect->x, rect->y, scale, color, UI_Cvar_VariableString("ui_teamName"), 0, 0, textStyle, iMenuFont);
}

static void UI_SetCapFragLimits(bool uiVars)
{
    int cap = 5;
    int frag = 10;

    if (uiVars) {
        trap->Cvar_Set("ui_captureLimit", Q_Format("{}", cap).c_str());
        trap->Cvar_Set("ui_fragLimit", Q_Format("{}", frag).c_str());
    }
    else {
        trap->Cvar_Set("capturelimit", Q_Format("{}", cap).c_str());
        trap->Cvar_Set("fraglimit", Q_Format("{}", frag).c_str());
    }
}

static const char* UI_GetGameTypeName(gametype_t gtEnum)
{
    switch (gtEnum) {
    case gametype_t::GT_FFA:
        return UI_GetStringEdString("MENUS", "FREE_FOR_ALL"); //"Free For All";
    case gametype_t::GT_HOLOCRON:
        return UI_GetStringEdString("MENUS", "HOLOCRON_FFA"); //"Holocron FFA";
    case gametype_t::GT_JEDIMASTER:
        return UI_GetStringEdString("MENUS", "SAGA"); //"Jedi Master";??
    case gametype_t::GT_SINGLE_PLAYER:
        return UI_GetStringEdString("MENUS", "SAGA"); //"Team FFA";
    case gametype_t::GT_DUEL:
        return UI_GetStringEdString("MENUS", "DUEL"); //"Team FFA";
    case gametype_t::GT_POWERDUEL:
        return UI_GetStringEdString("MENUS", "POWERDUEL"); //"Team FFA";
    case gametype_t::GT_TEAM:
        return UI_GetStringEdString("MENUS", "TEAM_FFA"); //"Team FFA";
    case gametype_t::GT_SIEGE:
        return UI_GetStringEdString("MENUS", "SIEGE"); //"Siege";
    case gametype_t::GT_CTF:
        return UI_GetStringEdString("MENUS", "CAPTURE_THE_FLAG"); //"Capture the Flag";
    case gametype_t::GT_CTY:
        return UI_GetStringEdString("MENUS", "CAPTURE_THE_YSALIMARI"); //"Capture the Ysalamiri";
    default:
        break;
    }
    return UI_GetStringEdString("MENUS", "SAGA"); //"Team FFA";
}

// ui_gameType assumes gametype 0 is -1 ALL and will not show
static void UI_DrawGameType(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    Text_Paint(rect->x, rect->y, scale, color, UI_GetGameTypeName(uiInfo.gameTypes[ui_gametype.integer].gtEnum), 0, 0, textStyle, iMenuFont);
}

static void UI_DrawNetGameType(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    if (ui_netGametype.integer < 0 || ui_netGametype.integer >= uiInfo.numGameTypes) {
        trap->Cvar_Set("ui_netGametype", "0");
        trap->Cvar_Update(&ui_netGametype);
        trap->Cvar_Set("ui_actualNetGametype", "0");
        trap->Cvar_Update(&ui_actualNetGametype);
    }
    Text_Paint(rect->x, rect->y, scale, color, UI_GetGameTypeName(uiInfo.gameTypes[ui_netGametype.integer].gtEnum), 0, 0, textStyle, iMenuFont);
}

static void UI_DrawAutoSwitch(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    int switchVal = trap->Cvar_VariableValue("cg_autoswitch");
    const char* switchString = "AUTOSWITCH1";
    const char* stripString = nullptr;

    switch (switchVal) {
    case 2:
        switchString = "AUTOSWITCH2";
        break;
    case 3:
        switchString = "AUTOSWITCH3";
        break;
    case 0:
        switchString = "AUTOSWITCH0";
        break;
    default:
        break;
    }

    stripString = UI_GetStringEdString("MP_INGAME", (char*)switchString);

    if (stripString) {
        Text_Paint(rect->x, rect->y, scale, color, stripString, 0, 0, textStyle, iMenuFont);
    }
}

static void UI_DrawJoinGameType(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    if (ui_joinGametype.integer < 0 || ui_joinGametype.integer > uiInfo.numJoinGameTypes) {
        trap->Cvar_Set("ui_joinGametype", "0");
        trap->Cvar_Update(&ui_joinGametype);
    }

    Text_Paint(rect->x, rect->y, scale, color, UI_GetGameTypeName(uiInfo.joinGameTypes[ui_joinGametype.integer].gtEnum), 0, 0, textStyle, iMenuFont);
}

static int UI_TeamIndexFromName(const char* name)
{
    int i;

    if (name && *name) {
        for (i = 0; i < uiInfo.teamCount; i++) {
            if (Q_stricmp(name, uiInfo.teamList[i].teamName) == 0) {
                return i;
            }
        }
    }

    return 0;
}

static void UI_DrawClanLogo(rectDef_t* rect, float scale, vec4_t color)
{
    int i;
    i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
    if (i >= 0 && i < uiInfo.teamCount) {
        trap->R_SetColor(color);

        if (uiInfo.teamList[i].teamIcon == -1) {
            uiInfo.teamList[i].teamIcon = trap->R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
            uiInfo.teamList[i].teamIcon_Metal = trap->R_RegisterShaderNoMip(Q_Format("{}_metal", uiInfo.teamList[i].imageName).c_str());
            uiInfo.teamList[i].teamIcon_Name = trap->R_RegisterShaderNoMip(Q_Format("{}_name", uiInfo.teamList[i].imageName).c_str());
        }

        UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon);
        trap->R_SetColor(nullptr);
    }
}

static void UI_DrawClanCinematic(rectDef_t* rect, float scale, vec4_t color)
{
    int i;
    i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
    if (i >= 0 && i < uiInfo.teamCount) {
        if (uiInfo.teamList[i].cinematic >= -2) {
            if (uiInfo.teamList[i].cinematic == -1) {
                uiInfo.teamList[i].cinematic = trap->CIN_PlayCinematic(Q_Format("{}.roq", uiInfo.teamList[i].imageName).c_str(), 0, 0, 0, 0,
                                                                       (cinematicFlags_t::CIN_loop | cinematicFlags_t::CIN_silent));
            }
            if (uiInfo.teamList[i].cinematic >= 0) {
                trap->CIN_RunCinematic(uiInfo.teamList[i].cinematic);
                trap->CIN_SetExtents(uiInfo.teamList[i].cinematic, rect->x, rect->y, rect->w, rect->h);
                trap->CIN_DrawCinematic(uiInfo.teamList[i].cinematic);
            }
            else {
                trap->R_SetColor(color);
                UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon_Metal);
                trap->R_SetColor(nullptr);
                uiInfo.teamList[i].cinematic = -2;
            }
        }
        else {
            trap->R_SetColor(color);
            UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon);
            trap->R_SetColor(nullptr);
        }
    }
}

static void UI_DrawPreviewCinematic(rectDef_t* rect, float scale, vec4_t color)
{
    if (uiInfo.previewMovie > -2) {
        uiInfo.previewMovie = trap->CIN_PlayCinematic(Q_Format("{}.roq", uiInfo.movieList[uiInfo.movieIndex]).c_str(), 0, 0, 0, 0,
                                                      (cinematicFlags_t::CIN_loop | cinematicFlags_t::CIN_silent));
        if (uiInfo.previewMovie >= 0) {
            trap->CIN_RunCinematic(uiInfo.previewMovie);
            trap->CIN_SetExtents(uiInfo.previewMovie, rect->x, rect->y, rect->w, rect->h);
            trap->CIN_DrawCinematic(uiInfo.previewMovie);
        }
        else {
            uiInfo.previewMovie = -2;
        }
    }
}

static void UI_DrawSkill(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    int i;
    i = trap->Cvar_VariableValue("g_spSkill");
    if (i < 1 || i > numSkillLevels) {
        i = 1;
    }
    Text_Paint(rect->x, rect->y, scale, color, (char*)UI_GetStringEdString("MP_INGAME", (char*)skillLevels[i - 1]), 0, 0, textStyle, iMenuFont);
}

static void UI_DrawGenericNum(rectDef_t* rect, float scale, vec4_t color, int textStyle, int val, int min, int max, int type, menuFont_t iMenuFont)
{
    int i;
    char s[256];

    i = val;
    if (i < min || i > max) {
        i = min;
    }

    Q_FormatTo(s, "{}", val);
    Text_Paint(rect->x, rect->y, scale, color, s, 0, 0, textStyle, iMenuFont);
}

static void UI_DrawForceMastery(rectDef_t* rect, float scale, vec4_t color, int textStyle, forceMasteries_t val, forceMasteries_t min, forceMasteries_t max,
                                menuFont_t iMenuFont)
{
    forceMasteries_t i;
    char* s;

    i = val;
    if (i < min) {
        i = min;
    }
    if (i > max) {
        i = max;
    }

    s = (char*)UI_GetStringEdString("MP_INGAME", forceMasteryLevels[i]);
    Text_Paint(rect->x, rect->y, scale, color, s, 0, 0, textStyle, iMenuFont);
}

static void UI_DrawSkinColor(rectDef_t* rect, float scale, vec4_t color, int textStyle, team_t val, team_t min, team_t max, menuFont_t iMenuFont)
{
    char s[256];

    switch (val) {
    case team_t::TEAM_RED:
        trap->SE_GetStringTextString("MENUS_TEAM_RED", s, sizeof(s));
        //		Q_FormatTo(s, "Red\0");
        break;
    case team_t::TEAM_BLUE:
        trap->SE_GetStringTextString("MENUS_TEAM_BLUE", s, sizeof(s));
        //		Q_FormatTo(s, "Blue\0");
        break;
    default:
        trap->SE_GetStringTextString("MENUS_DEFAULT", s, sizeof(s));
        //		Q_FormatTo(s, "Default\0");
        break;
    }

    Text_Paint(rect->x, rect->y, scale, color, s, 0, 0, textStyle, iMenuFont);
}

static void UI_DrawForceSide(rectDef_t* rect, float scale, vec4_t color, int textStyle, int val, int min, int max, menuFont_t iMenuFont)
{
    char s[256];
    menuDef_t* menu;

    char info[MAX_INFO_VALUE];

    info[0] = '\0';
    trap->GetConfigString(CS_SERVERINFO, info, sizeof(info));

    if (atoi(Info_ValueForKey(info, "g_forceBasedTeams"))) {
        switch (UI_CvarTeam("ui_myteam")) {
        case team_t::TEAM_RED:
            uiForceSide = FORCE_DARKSIDE;
            color[0] = 0.2f;
            color[1] = 0.2f;
            color[2] = 0.2f;
            break;
        case team_t::TEAM_BLUE:
            uiForceSide = FORCE_LIGHTSIDE;
            color[0] = 0.2f;
            color[1] = 0.2f;
            color[2] = 0.2f;
            break;
        default:
            break;
        }
    }

    if (val == FORCE_LIGHTSIDE) {
        trap->SE_GetStringTextString("MENUS_FORCEDESC_LIGHT", s, sizeof(s));
        menu = Menus_FindByName("forcealloc");
        if (menu) {
            Menu_ShowItemByName(menu, "lightpowers", true);
            Menu_ShowItemByName(menu, "darkpowers", false);
            Menu_ShowItemByName(menu, "darkpowers_team", false);

            Menu_ShowItemByName(menu, "lightpowers_team", true); //(ui_gameType.integer >= gametype_t::GT_TEAM));
        }
        menu = Menus_FindByName("ingame_playerforce");
        if (menu) {
            Menu_ShowItemByName(menu, "lightpowers", true);
            Menu_ShowItemByName(menu, "darkpowers", false);
            Menu_ShowItemByName(menu, "darkpowers_team", false);

            Menu_ShowItemByName(menu, "lightpowers_team", true); //(ui_gameType.integer >= gametype_t::GT_TEAM));
        }
    }
    else {
        trap->SE_GetStringTextString("MENUS_FORCEDESC_DARK", s, sizeof(s));
        menu = Menus_FindByName("forcealloc");
        if (menu) {
            Menu_ShowItemByName(menu, "lightpowers", false);
            Menu_ShowItemByName(menu, "lightpowers_team", false);
            Menu_ShowItemByName(menu, "darkpowers", true);

            Menu_ShowItemByName(menu, "darkpowers_team", true); //(ui_gameType.integer >= gametype_t::GT_TEAM));
        }
        menu = Menus_FindByName("ingame_playerforce");
        if (menu) {
            Menu_ShowItemByName(menu, "lightpowers", false);
            Menu_ShowItemByName(menu, "lightpowers_team", false);
            Menu_ShowItemByName(menu, "darkpowers", true);

            Menu_ShowItemByName(menu, "darkpowers_team", true); //(ui_gameType.integer >= gametype_t::GT_TEAM));
        }
    }

    Text_Paint(rect->x, rect->y, scale, color, s, 0, 0, textStyle, iMenuFont);
}

bool UI_HasSetSaberOnly(const char* info, const gametype_t gametype)
{
    weapon_t i = weapon_t::WP_NONE;
    int wDisable = 0;

    if (gametype == gametype_t::GT_JEDIMASTER) { // set to 0
        return false;
    }

    if (gametype == gametype_t::GT_DUEL || gametype == gametype_t::GT_POWERDUEL) {
        wDisable = atoi(Info_ValueForKey(info, "g_duelWeaponDisable"));
    }
    else {
        wDisable = atoi(Info_ValueForKey(info, "g_weaponDisable"));
    }

    while (i < weapon_t::WP_NUM_WEAPONS) {
        if (!(wDisable & Bit(i)) && i != weapon_t::WP_SABER && i != weapon_t::WP_NONE) {
            return false;
        }

        i++;
    }

    return true;
}

static bool UI_AllForceDisabled(int force)
{

    if (force) {
        for (forcePowers_t fp = forcePowers_t::FP_FIRST; fp < forcePowers_t::NUM_FORCE_POWERS; fp++) {
            if (!(force & Bit(fp))) {
                return false;
            }
        }

        return true;
    }

    return false;
}

bool UI_TrueJediEnabled(void)
{
    char info[MAX_INFO_STRING] = {0};
    int disabledForce = 0, trueJedi = 0;
    gametype_t gametype = gametype_t::GT_FFA;
    bool saberOnly = false, allForceDisabled = false;

    trap->GetConfigString(CS_SERVERINFO, info, sizeof(info));

    // already have serverinfo at this point for stuff below. Don't bother trying to use ui_forcePowerDisable.
    // if (ui_forcePowerDisable.integer)
    // if (atoi(Info_ValueForKey(info, "g_forcePowerDisable")))
    disabledForce = atoi(Info_ValueForKey(info, "g_forcePowerDisable"));
    allForceDisabled = UI_AllForceDisabled(disabledForce);
    gametype = static_cast<gametype_t>(atoi(Info_ValueForKey(info, "g_gametype")));
    saberOnly = UI_HasSetSaberOnly(info, gametype);

    if (gametype == gametype_t::GT_HOLOCRON || gametype == gametype_t::GT_JEDIMASTER || saberOnly || allForceDisabled) {
        trueJedi = 0;
    }
    else {
        trueJedi = atoi(Info_ValueForKey(info, "g_jediVmerc"));
    }
    return (trueJedi != 0);
}

static void UI_DrawJediNonJedi(rectDef_t* rect, float scale, vec4_t color, int textStyle, int val, int min, int max, menuFont_t iMenuFont)
{
    int i;
    char s[256];
    // menuDef_t *menu;

    char info[MAX_INFO_VALUE];

    i = val;
    if (i < min || i > max) {
        i = min;
    }

    info[0] = '\0';
    trap->GetConfigString(CS_SERVERINFO, info, sizeof(info));

    if (!UI_TrueJediEnabled()) { // true jedi mode is not on, do not draw this button type
        return;
    }

    if (val == FORCE_NONJEDI)
        trap->SE_GetStringTextString("MENUS_NO", s, sizeof(s));
    else
        trap->SE_GetStringTextString("MENUS_YES", s, sizeof(s));

    Text_Paint(rect->x, rect->y, scale, color, s, 0, 0, textStyle, iMenuFont);
}

static void UI_DrawTeamName(rectDef_t* rect, float scale, vec4_t color, bool blue, int textStyle, menuFont_t iMenuFont)
{
    int i;
    i = UI_TeamIndexFromName(UI_Cvar_VariableString((blue) ? "ui_blueTeam" : "ui_redTeam"));
    if (i >= 0 && i < uiInfo.teamCount) {
        Text_Paint(rect->x, rect->y, scale, color, Q_Format("{}: {}", (blue) ? "Blue" : "Red", uiInfo.teamList[i].teamName).c_str(), 0, 0, textStyle,
                   iMenuFont);
    }
}

static void UI_DrawTeamMember(rectDef_t* rect, float scale, vec4_t color, bool blue, int num, int textStyle, menuFont_t iMenuFont)
{
    // 0 - None
    // 1 - Human
    // 2..NumCharacters - Bot
    int value = trap->Cvar_VariableValue(Q_Format("ui_{}team{}", blue ? "blue" : "red", num).c_str());
    const char* text;
    int maxcl = trap->Cvar_VariableValue("sv_maxClients");
    vec4_t finalColor;
    int numval = num;

    numval *= 2;

    if (blue) {
        numval -= 1;
    }

    finalColor[0] = color[0];
    finalColor[1] = color[1];
    finalColor[2] = color[2];
    finalColor[3] = color[3];

    if (numval > maxcl) {
        finalColor[0] *= 0.5;
        finalColor[1] *= 0.5;
        finalColor[2] *= 0.5;

        value = -1;
    }

    if (uiInfo.gameTypes[ui_netGametype.integer].gtEnum == gametype_t::GT_SIEGE) {
        if (value > 1) {
            value = 1;
        }
    }

    if (value <= 1) {
        if (value == -1) {
            // text = "Closed";
            text = UI_GetStringEdString("MENUS", "CLOSED");
        }
        else {
            // text = "Human";
            text = UI_GetStringEdString("MENUS", "HUMAN");
        }
    }
    else {
        value -= 2;
        if (value >= UI_GetNumBots()) {
            value = 1;
        }
        text = UI_GetBotNameByNumber(value);
    }

    Text_Paint(rect->x, rect->y, scale, finalColor, text, 0, 0, textStyle, iMenuFont);
}

static void UI_DrawMapPreview(rectDef_t* rect, float scale, vec4_t color, bool net)
{
    int map = (net) ? ui_currentNetMap.integer : ui_currentMap.integer;
    if (map < 0 || map > uiInfo.mapCount) {
        if (net) {
            trap->Cvar_Set("ui_currentNetMap", "0");
            trap->Cvar_Update(&ui_currentNetMap);
        }
        else {
            trap->Cvar_Set("ui_currentMap", "0");
            trap->Cvar_Update(&ui_currentMap);
        }
        map = 0;
    }

    if (uiInfo.mapList[map].levelShot == -1) {
        uiInfo.mapList[map].levelShot = trap->R_RegisterShaderNoMip(uiInfo.mapList[map].imageName);
    }

    if (uiInfo.mapList[map].levelShot > 0) {
        UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.mapList[map].levelShot);
    }
    else {
        UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, trap->R_RegisterShaderNoMip("menu/art/unknownmap_mp"));
    }
}

static void UI_DrawMapCinematic(rectDef_t* rect, float scale, vec4_t color, bool net)
{
    int map = (net) ? ui_currentNetMap.integer : ui_currentMap.integer;
    if (map < 0 || map > uiInfo.mapCount) {
        if (net) {
            trap->Cvar_Set("ui_currentNetMap", "0");
            trap->Cvar_Update(&ui_currentNetMap);
        }
        else {
            trap->Cvar_Set("ui_currentMap", "0");
            trap->Cvar_Update(&ui_currentMap);
        }
        map = 0;
    }

    if (uiInfo.mapList[map].cinematic >= -1) {
        if (uiInfo.mapList[map].cinematic == -1) {
            uiInfo.mapList[map].cinematic = trap->CIN_PlayCinematic(Q_Format("{}.roq", uiInfo.mapList[map].mapLoadName).c_str(), 0, 0, 0, 0,
                                                                    (cinematicFlags_t::CIN_loop | cinematicFlags_t::CIN_silent));
        }
        if (uiInfo.mapList[map].cinematic >= 0) {
            trap->CIN_RunCinematic(uiInfo.mapList[map].cinematic);
            trap->CIN_SetExtents(uiInfo.mapList[map].cinematic, rect->x, rect->y, rect->w, rect->h);
            trap->CIN_DrawCinematic(uiInfo.mapList[map].cinematic);
        }
        else {
            uiInfo.mapList[map].cinematic = -2;
        }
    }
    else {
        UI_DrawMapPreview(rect, scale, color, net);
    }
}

static void UI_SetForceDisabled(int force)
{
    forcePowers_t i = forcePowers_t::FP_FIRST;

    if (force) {
        while (i < forcePowers_t::NUM_FORCE_POWERS) {
            if (force & Bit(i)) {
                uiForcePowersDisabled[i] = true;

                if (i != forcePowers_t::FP_LEVITATION && i != forcePowers_t::FP_SABER_OFFENSE && i != forcePowers_t::FP_SABER_DEFENSE) {
                    uiForcePowersRank[i] = forcePowerLevels_t::FORCE_LEVEL_0;
                }
                else {
                    if (i == forcePowers_t::FP_LEVITATION) {
                        uiForcePowersRank[i] = forcePowerLevels_t::FORCE_LEVEL_1;
                    }
                    else {
                        uiForcePowersRank[i] = forcePowerLevels_t::FORCE_LEVEL_3;
                    }
                }
            }
            else {
                uiForcePowersDisabled[i] = false;
            }
            i++;
        }
    }
    else {
        i = forcePowers_t::FP_FIRST;

        while (i < forcePowers_t::NUM_FORCE_POWERS) {
            uiForcePowersDisabled[i] = false;
            i++;
        }
    }
}
// The game type on create server has changed - make the HUMAN/BOTS fields active
void UpdateBotButtons(void)
{
    menuDef_t* menu;

    menu = Menu_GetFocused();

    if (!menu) {
        return;
    }

    if (uiInfo.gameTypes[ui_netGametype.integer].gtEnum == gametype_t::GT_SIEGE) {
        Menu_ShowItemByName(menu, "humanbotfield", false);
        Menu_ShowItemByName(menu, "humanbotnonfield", true);
    }
    else {
        Menu_ShowItemByName(menu, "humanbotfield", true);
        Menu_ShowItemByName(menu, "humanbotnonfield", false);
    }
}

void UpdateForceStatus()
{
    menuDef_t* menu;

    // Currently we don't make a distinction between those that wish to play Jedi of lower than maximum skill.
    /*	if (ui_forcePowerDisable.integer)
        {
            uiForceRank = forceMasteries_t::FORCE_MASTERY_UNINITIATED;
            uiForceAvailable = 0;
            uiForceUsed = 0;
        }
        else
        {
            uiForceRank = uiMaxRank;
            uiForceUsed = 0;
            uiForceAvailable = forceMasteryPoints[uiForceRank];
        }
    */
    menu = Menus_FindByName("ingame_player");
    if (menu) {
        char info[MAX_INFO_STRING];
        int disabledForce = 0;
        bool trueJedi = false, allForceDisabled = false;

        trap->GetConfigString(CS_SERVERINFO, info, sizeof(info));

        // already have serverinfo at this point for stuff below. Don't bother trying to use ui_forcePowerDisable.
        // if (ui_forcePowerDisable.integer)
        // if (atoi(Info_ValueForKey(info, "g_forcePowerDisable")))
        disabledForce = atoi(Info_ValueForKey(info, "g_forcePowerDisable"));
        allForceDisabled = UI_AllForceDisabled(disabledForce);
        trueJedi = UI_TrueJediEnabled();

        if (!trueJedi || allForceDisabled) {
            Menu_ShowItemByName(menu, "jedinonjedi", false);
        }
        else {
            Menu_ShowItemByName(menu, "jedinonjedi", true);
        }
        if (allForceDisabled == true || (trueJedi && uiJediNonJedi == FORCE_NONJEDI)) { // No force stuff
            Menu_ShowItemByName(menu, "noforce", true);
            Menu_ShowItemByName(menu, "yesforce", false);
            // We don't want the saber explanation to say "configure saber attack 1" since we can't.
            Menu_ShowItemByName(menu, "sabernoneconfigme", false);
        }
        else {
            UI_SetForceDisabled(disabledForce);
            Menu_ShowItemByName(menu, "noforce", false);
            Menu_ShowItemByName(menu, "yesforce", true);
        }

        // Moved this to happen after it's done with force power disabling stuff
        if (uiForcePowersRank[forcePowers_t::FP_SABER_OFFENSE] > forcePowerLevels_t::FORCE_LEVEL_0 || ui_freeSaber.integer) { // Show lightsaber stuff.
            Menu_ShowItemByName(menu, "nosaber", false);
            Menu_ShowItemByName(menu, "yessaber", true);
        }
        else {
            Menu_ShowItemByName(menu, "nosaber", true);
            Menu_ShowItemByName(menu, "yessaber", false);
        }

        // The leftmost button should be "apply" unless you are in spectator, where you can join any team.
        if (UI_CvarTeam("ui_myteam") != team_t::TEAM_SPECTATOR) {
            Menu_ShowItemByName(menu, "playerapply", true);
            Menu_ShowItemByName(menu, "playerforcejoin", false);
            Menu_ShowItemByName(menu, "playerforcered", true);
            Menu_ShowItemByName(menu, "playerforceblue", true);
            Menu_ShowItemByName(menu, "playerforcespectate", true);
        }
        else {
            // Set or reset buttons based on choices
            if (atoi(Info_ValueForKey(info, "g_gametype")) >= std::to_underlying(gametype_t::GT_TEAM)) { // This is a team-based game.
                Menu_ShowItemByName(menu, "playerforcespectate", true);

                // This is disabled, always show both sides from spectator.
                if (0 && atoi(Info_ValueForKey(info, "g_forceBasedTeams"))) { // Show red or blue based on what side is chosen.
                    if (uiForceSide == FORCE_LIGHTSIDE) {
                        Menu_ShowItemByName(menu, "playerforcered", false);
                        Menu_ShowItemByName(menu, "playerforceblue", true);
                    }
                    else if (uiForceSide == FORCE_DARKSIDE) {
                        Menu_ShowItemByName(menu, "playerforcered", true);
                        Menu_ShowItemByName(menu, "playerforceblue", false);
                    }
                    else {
                        Menu_ShowItemByName(menu, "playerforcered", true);
                        Menu_ShowItemByName(menu, "playerforceblue", true);
                    }
                }
                else {
                    Menu_ShowItemByName(menu, "playerforcered", true);
                    Menu_ShowItemByName(menu, "playerforceblue", true);
                }
            }
            else {
                Menu_ShowItemByName(menu, "playerforcered", false);
                Menu_ShowItemByName(menu, "playerforceblue", false);
            }

            Menu_ShowItemByName(menu, "playerapply", false);
            Menu_ShowItemByName(menu, "playerforcejoin", true);
            Menu_ShowItemByName(menu, "playerforcespectate", true);
        }
    }

    if (!UI_TrueJediEnabled()) { // Take the current team and force a skin color based on it.
        char info[MAX_INFO_STRING];

        switch (UI_CvarTeam("ui_myteam")) {
        case team_t::TEAM_RED:
            uiSkinColor = team_t::TEAM_RED;
            break;
        case team_t::TEAM_BLUE:
            uiSkinColor = team_t::TEAM_BLUE;
            break;
        default:
            trap->GetConfigString(CS_SERVERINFO, info, sizeof(info));

            if (atoi(Info_ValueForKey(info, "g_gametype")) >= std::to_underlying(gametype_t::GT_TEAM)) {
                uiSkinColor = team_t::TEAM_FREE;
            }
            else // A bit of a hack so non-team games will remember which skin set you chose in the player menu
            {
                uiSkinColor = uiHoldSkinColor;
            }
            break;
        }
    }
}

static void UI_DrawNetSource(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    if (ui_netSource.integer < 0 || ui_netSource.integer >= numNetSources) {
        trap->Cvar_Set("ui_netSource", "0");
        trap->Cvar_Update(&ui_netSource);
    }

    trap->SE_GetStringTextString("MENUS_SOURCE", holdSPString, sizeof(holdSPString));
    Text_Paint(rect->x, rect->y, scale, color, Q_Format("{} {}", holdSPString, GetNetSourceString(UI_NetSource())).c_str(), 0, 0, textStyle, iMenuFont);
}

static void UI_DrawNetMapPreview(rectDef_t* rect, float scale, vec4_t color)
{
    if (uiInfo.serverStatus.currentServerPreview > 0) {
        UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.serverStatus.currentServerPreview);
    }
    else {
        UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, trap->R_RegisterShaderNoMip("menu/art/unknownmap_mp"));
    }
}

static void UI_DrawNetMapCinematic(rectDef_t* rect, float scale, vec4_t color)
{
    if (ui_currentNetMap.integer < 0 || ui_currentNetMap.integer > uiInfo.mapCount) {
        trap->Cvar_Set("ui_currentNetMap", "0");
        trap->Cvar_Update(&ui_currentNetMap);
    }

    if (uiInfo.serverStatus.currentServerCinematic >= 0) {
        trap->CIN_RunCinematic(uiInfo.serverStatus.currentServerCinematic);
        trap->CIN_SetExtents(uiInfo.serverStatus.currentServerCinematic, rect->x, rect->y, rect->w, rect->h);
        trap->CIN_DrawCinematic(uiInfo.serverStatus.currentServerCinematic);
    }
    else {
        UI_DrawNetMapPreview(rect, scale, color);
    }
}

static void UI_DrawNetFilter(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    trap->SE_GetStringTextString("MENUS_GAME", holdSPString, sizeof(holdSPString));

    Text_Paint(rect->x, rect->y, scale, color, Q_Format("{} {}", holdSPString, UI_FilterDescription(ui_serverFilterType.integer)).c_str(), 0, 0, textStyle,
               iMenuFont);
}

static void UI_DrawTier(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    int i;
    i = trap->Cvar_VariableValue("ui_currentTier");
    if (i < 0 || i >= uiInfo.tierCount) {
        i = 0;
    }
    Text_Paint(rect->x, rect->y, scale, color, Q_Format("Tier: {}", uiInfo.tierList[i].tierName).c_str(), 0, 0, textStyle, iMenuFont);
}

static void UI_DrawTierMap(rectDef_t* rect, int index)
{
    int i;
    i = trap->Cvar_VariableValue("ui_currentTier");
    if (i < 0 || i >= uiInfo.tierCount) {
        i = 0;
    }

    if (uiInfo.tierList[i].mapHandles[index] == -1) {
        uiInfo.tierList[i].mapHandles[index] = trap->R_RegisterShaderNoMip(Q_Format("levelshots/{}", uiInfo.tierList[i].maps[index]).c_str());
    }

    UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.tierList[i].mapHandles[index]);
}

static const char* UI_EnglishMapName(const char* map)
{
    int i;
    for (i = 0; i < uiInfo.mapCount; i++) {
        if (Q_stricmp(map, uiInfo.mapList[i].mapLoadName) == 0) {
            return uiInfo.mapList[i].mapName;
        }
    }
    return "";
}

static void UI_DrawTierMapName(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    int i, j;
    i = trap->Cvar_VariableValue("ui_currentTier");
    if (i < 0 || i >= uiInfo.tierCount) {
        i = 0;
    }
    j = trap->Cvar_VariableValue("ui_currentMap");
    if (j < 0 || j >= MAPS_PER_TIER) {
        j = 0;
    }

    Text_Paint(rect->x, rect->y, scale, color, UI_EnglishMapName(uiInfo.tierList[i].maps[j]), 0, 0, textStyle, iMenuFont);
}

static void UI_DrawTierGameType(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    int i, j;
    i = trap->Cvar_VariableValue("ui_currentTier");
    if (i < 0 || i >= uiInfo.tierCount) {
        i = 0;
    }
    j = trap->Cvar_VariableValue("ui_currentMap");
    if (j < 0 || j >= MAPS_PER_TIER) {
        j = 0;
    }

    Text_Paint(rect->x, rect->y, scale, color, uiInfo.gameTypes[uiInfo.tierList[i].gameTypes[j]].gameType, 0, 0, textStyle, iMenuFont);
}

static const char* UI_AIFromName(const char* name)
{
    int j;
    for (j = 0; j < uiInfo.aliasCount; j++) {
        if (Q_stricmp(uiInfo.aliasList[j].name, name) == 0) {
            return uiInfo.aliasList[j].ai;
        }
    }
    return "Kyle";
}

/*
static bool updateOpponentModel = true;
static void UI_DrawOpponent(rectDef_t *rect) {
  static playerInfo_t info2;
  char model[MAX_QPATH];
  char headmodel[MAX_QPATH];
  char team[256];
    vec3_t	viewangles;
    vec3_t	moveangles;

    if (updateOpponentModel) {

        strcpy(model, UI_Cvar_VariableString("ui_opponentModel"));
      strcpy(headmodel, UI_Cvar_VariableString("ui_opponentModel"));
        team[0] = '\0';

    memset( &info2, 0, sizeof(playerInfo_t) );
    viewangles[YAW]   = 180 - 10;
    viewangles[PITCH] = 0;
    viewangles[ROLL]  = 0;
    VectorClear( moveangles );
    UI_PlayerInfo_SetModel( &info2, model, headmodel, "");
    UI_PlayerInfo_SetInfo( &info2, animNumber_t::TORSO_WEAPONREADY3, animNumber_t::TORSO_WEAPONREADY3, viewangles, vec3_origin, weapon_t::WP_BRYAR_PISTOL, false
); UI_RegisterClientModelname( &info2, model, headmodel, team); updateOpponentModel = false;
  }

  UI_DrawPlayer( rect->x, rect->y, rect->w, rect->h, &info2, uiInfo.uiDC.realTime / 2);

}
*/
static void UI_NextOpponent()
{
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
    int j = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
    i++;
    if (i >= uiInfo.teamCount) {
        i = 0;
    }
    if (i == j) {
        i++;
        if (i >= uiInfo.teamCount) {
            i = 0;
        }
    }
    trap->Cvar_Set("ui_opponentName", uiInfo.teamList[i].teamName);
}

static void UI_PriorOpponent()
{
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
    int j = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
    i--;
    if (i < 0) {
        i = uiInfo.teamCount - 1;
    }
    if (i == j) {
        i--;
        if (i < 0) {
            i = uiInfo.teamCount - 1;
        }
    }
    trap->Cvar_Set("ui_opponentName", uiInfo.teamList[i].teamName);
}

static void UI_DrawPlayerLogo(rectDef_t* rect, vec3_t color)
{
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
    if (uiInfo.teamList[i].teamIcon == -1) {
        uiInfo.teamList[i].teamIcon = trap->R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
        uiInfo.teamList[i].teamIcon_Metal = trap->R_RegisterShaderNoMip(Q_Format("{}_metal", uiInfo.teamList[i].imageName).c_str());
        uiInfo.teamList[i].teamIcon_Name = trap->R_RegisterShaderNoMip(Q_Format("{}_name", uiInfo.teamList[i].imageName).c_str());
    }

    trap->R_SetColor(color);
    UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon);
    trap->R_SetColor(nullptr);
}

static void UI_DrawPlayerLogoMetal(rectDef_t* rect, vec3_t color)
{
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
    if (uiInfo.teamList[i].teamIcon == -1) {
        uiInfo.teamList[i].teamIcon = trap->R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
        uiInfo.teamList[i].teamIcon_Metal = trap->R_RegisterShaderNoMip(Q_Format("{}_metal", uiInfo.teamList[i].imageName).c_str());
        uiInfo.teamList[i].teamIcon_Name = trap->R_RegisterShaderNoMip(Q_Format("{}_name", uiInfo.teamList[i].imageName).c_str());
    }

    trap->R_SetColor(color);
    UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon_Metal);
    trap->R_SetColor(nullptr);
}

static void UI_DrawPlayerLogoName(rectDef_t* rect, vec3_t color)
{
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
    if (uiInfo.teamList[i].teamIcon == -1) {
        uiInfo.teamList[i].teamIcon = trap->R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
        uiInfo.teamList[i].teamIcon_Metal = trap->R_RegisterShaderNoMip(Q_Format("{}_metal", uiInfo.teamList[i].imageName).c_str());
        uiInfo.teamList[i].teamIcon_Name = trap->R_RegisterShaderNoMip(Q_Format("{}_name", uiInfo.teamList[i].imageName).c_str());
    }

    trap->R_SetColor(color);
    UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon_Name);
    trap->R_SetColor(nullptr);
}

static void UI_DrawOpponentLogo(rectDef_t* rect, vec3_t color)
{
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
    if (uiInfo.teamList[i].teamIcon == -1) {
        uiInfo.teamList[i].teamIcon = trap->R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
        uiInfo.teamList[i].teamIcon_Metal = trap->R_RegisterShaderNoMip(Q_Format("{}_metal", uiInfo.teamList[i].imageName).c_str());
        uiInfo.teamList[i].teamIcon_Name = trap->R_RegisterShaderNoMip(Q_Format("{}_name", uiInfo.teamList[i].imageName).c_str());
    }

    trap->R_SetColor(color);
    UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon);
    trap->R_SetColor(nullptr);
}

static void UI_DrawOpponentLogoMetal(rectDef_t* rect, vec3_t color)
{
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
    if (uiInfo.teamList[i].teamIcon == -1) {
        uiInfo.teamList[i].teamIcon = trap->R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
        uiInfo.teamList[i].teamIcon_Metal = trap->R_RegisterShaderNoMip(Q_Format("{}_metal", uiInfo.teamList[i].imageName).c_str());
        uiInfo.teamList[i].teamIcon_Name = trap->R_RegisterShaderNoMip(Q_Format("{}_name", uiInfo.teamList[i].imageName).c_str());
    }

    trap->R_SetColor(color);
    UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon_Metal);
    trap->R_SetColor(nullptr);
}

static void UI_DrawOpponentLogoName(rectDef_t* rect, vec3_t color)
{
    int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));
    if (uiInfo.teamList[i].teamIcon == -1) {
        uiInfo.teamList[i].teamIcon = trap->R_RegisterShaderNoMip(uiInfo.teamList[i].imageName);
        uiInfo.teamList[i].teamIcon_Metal = trap->R_RegisterShaderNoMip(Q_Format("{}_metal", uiInfo.teamList[i].imageName).c_str());
        uiInfo.teamList[i].teamIcon_Name = trap->R_RegisterShaderNoMip(Q_Format("{}_name", uiInfo.teamList[i].imageName).c_str());
    }

    trap->R_SetColor(color);
    UI_DrawHandlePic(rect->x, rect->y, rect->w, rect->h, uiInfo.teamList[i].teamIcon_Name);
    trap->R_SetColor(nullptr);
}

static void UI_DrawAllMapsSelection(rectDef_t* rect, float scale, vec4_t color, int textStyle, bool net, menuFont_t iMenuFont)
{
    int map = (net) ? ui_currentNetMap.integer : ui_currentMap.integer;
    if (map >= 0 && map < uiInfo.mapCount) {
        Text_Paint(rect->x, rect->y, scale, color, uiInfo.mapList[map].mapName, 0, 0, textStyle, iMenuFont);
    }
}

static void UI_DrawOpponentName(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    Text_Paint(rect->x, rect->y, scale, color, UI_Cvar_VariableString("ui_opponentName"), 0, 0, textStyle, iMenuFont);
}

static int UI_OwnerDrawWidth(int ownerDraw, float scale)
{
    int i, h, value, iUse = 0;
    forcePowers_t findex;
    const char* text;
    const char* s = nullptr;
    formatText_t sText; // holds the text s points into

    switch (ownerDraw) {
    case UI_HANDICAP:
        h = Com_Clamp(5, 100, trap->Cvar_VariableValue("handicap"));
        i = 20 - h / 5;
        s = handicapValues[i];
        break;
    case UI_SKIN_COLOR:
        switch (uiSkinColor) {
        case team_t::TEAM_RED:
            //			s = "Red";
            s = (char*)UI_GetStringEdString("MENUS", "TEAM_RED");
            break;
        case team_t::TEAM_BLUE:
            //			s = "Blue";
            s = (char*)UI_GetStringEdString("MENUS", "TEAM_BLUE");
            break;
        default:
            //			s = "Default";
            s = (char*)UI_GetStringEdString("MENUS", "DEFAULT");
            break;
        }
        break;
    case UI_FORCE_SIDE:
        i = uiForceSide;
        if (i < 1 || i > 2) {
            i = 1;
        }

        if (i == FORCE_LIGHTSIDE) {
            //			s = "Light";
            s = (char*)UI_GetStringEdString("MENUS", "FORCEDESC_LIGHT");
        }
        else {
            //			s = "Dark";
            s = (char*)UI_GetStringEdString("MENUS", "FORCEDESC_DARK");
        }
        break;
    case UI_JEDI_NONJEDI:
        i = uiJediNonJedi;
        if (i < 0 || i > 1) {
            i = 0;
        }

        if (i == FORCE_NONJEDI) {
            //			s = "Non-Jedi";
            s = (char*)UI_GetStringEdString("MENUS", "NO");
        }
        else {
            //			s = "Jedi";
            s = (char*)UI_GetStringEdString("MENUS", "YES");
        }
        break;
    case UI_FORCE_RANK: {
        forceMasteries_t rank = uiForceRank;
        if (rank < forceMasteries_t::FORCE_MASTERY_INITIATE || rank > forceMasteries_t::FORCE_MASTERY_JEDI_MASTER) {
            rank = forceMasteries_t::FORCE_MASTERY_INITIATE;
        }

        s = (char*)UI_GetStringEdString("MP_INGAME", forceMasteryLevels[rank]);
    } break;
    case UI_FORCE_RANK_HEAL:
    case UI_FORCE_RANK_LEVITATION:
    case UI_FORCE_RANK_SPEED:
    case UI_FORCE_RANK_PUSH:
    case UI_FORCE_RANK_PULL:
    case UI_FORCE_RANK_TELEPATHY:
    case UI_FORCE_RANK_GRIP:
    case UI_FORCE_RANK_LIGHTNING:
    case UI_FORCE_RANK_RAGE:
    case UI_FORCE_RANK_PROTECT:
    case UI_FORCE_RANK_ABSORB:
    case UI_FORCE_RANK_TEAM_HEAL:
    case UI_FORCE_RANK_TEAM_FORCE:
    case UI_FORCE_RANK_DRAIN:
    case UI_FORCE_RANK_SEE:
    case UI_FORCE_RANK_SABERATTACK:
    case UI_FORCE_RANK_SABERDEFEND:
    case UI_FORCE_RANK_SABERTHROW:
        findex = UI_ForceRankPower(ownerDraw);
        // this will give us the index as long as UI_FORCE_RANK is always one below the first force rank index
        s = (sText = Q_Format("{}", uiForcePowersRank[findex])).c_str();
        break;
    case UI_CLANNAME:
        s = UI_Cvar_VariableString("ui_teamName");
        break;
    case UI_GAMETYPE:
        s = uiInfo.gameTypes[ui_gametype.integer].gameType;
        break;
    case UI_SKILL:
        i = trap->Cvar_VariableValue("g_spSkill");
        if (i < 1 || i > numSkillLevels) {
            i = 1;
        }
        s = (char*)UI_GetStringEdString("MP_INGAME", (char*)skillLevels[i - 1]);
        break;
    case UI_BLUETEAMNAME:
        i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_blueTeam"));
        if (i >= 0 && i < uiInfo.teamCount) {
            s = (sText = Q_Format("{}: {}", (char*)UI_GetStringEdString("MENUS", "TEAM_BLUE"), uiInfo.teamList[i].teamName)).c_str();
        }
        break;
    case UI_REDTEAMNAME:
        i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_redTeam"));
        if (i >= 0 && i < uiInfo.teamCount) {
            s = (sText = Q_Format("{}: {}", (char*)UI_GetStringEdString("MENUS", "TEAM_RED"), uiInfo.teamList[i].teamName)).c_str();
        }
        break;
    case UI_BLUETEAM1:
    case UI_BLUETEAM2:
    case UI_BLUETEAM3:
    case UI_BLUETEAM4:
    case UI_BLUETEAM5:
    case UI_BLUETEAM6:
    case UI_BLUETEAM7:
    case UI_BLUETEAM8:
        if (ownerDraw <= UI_BLUETEAM5) {
            iUse = ownerDraw - UI_BLUETEAM1 + 1;
        }
        else {
            iUse = ownerDraw - 274; // unpleasent hack because I don't want to move up all the UI_BLAHTEAM# defines
        }

        value = trap->Cvar_VariableValue(Q_Format("ui_blueteam{}", iUse).c_str());
        if (value <= 1) {
            text = "Human";
        }
        else {
            value -= 2;
            if (value >= uiInfo.aliasCount) {
                value = 1;
            }
            text = uiInfo.aliasList[value].name;
        }
        s = (sText = Q_Format("{}. {}", iUse, text)).c_str();
        break;
    case UI_REDTEAM1:
    case UI_REDTEAM2:
    case UI_REDTEAM3:
    case UI_REDTEAM4:
    case UI_REDTEAM5:
    case UI_REDTEAM6:
    case UI_REDTEAM7:
    case UI_REDTEAM8:
        if (ownerDraw <= UI_REDTEAM5) {
            iUse = ownerDraw - UI_REDTEAM1 + 1;
        }
        else {
            iUse = ownerDraw - 277; // unpleasent hack because I don't want to move up all the UI_BLAHTEAM# defines
        }

        value = trap->Cvar_VariableValue(Q_Format("ui_redteam{}", iUse).c_str());
        if (value <= 1) {
            text = "Human";
        }
        else {
            value -= 2;
            if (value >= uiInfo.aliasCount) {
                value = 1;
            }
            text = uiInfo.aliasList[value].name;
        }
        s = (sText = Q_Format("{}. {}", iUse, text)).c_str();
        break;
    case UI_NETSOURCE:
        if (ui_netSource.integer < 0 || ui_netSource.integer >= numNetSources) {
            trap->Cvar_Set("ui_netSource", "0");
            trap->Cvar_Update(&ui_netSource);
        }
        trap->SE_GetStringTextString("MENUS_SOURCE", holdSPString, sizeof(holdSPString));
        s = (sText = Q_Format("{} {}", holdSPString, GetNetSourceString(UI_NetSource()))).c_str();
        break;
    case UI_NETFILTER:
        trap->SE_GetStringTextString("MENUS_GAME", holdSPString, sizeof(holdSPString));
        s = (sText = Q_Format("{} {}", holdSPString, UI_FilterDescription(ui_serverFilterType.integer))).c_str();
        break;
    case UI_TIER:
        break;
    case UI_TIER_MAPNAME:
        break;
    case UI_TIER_GAMETYPE:
        break;
    case UI_ALLMAPS_SELECTION:
        break;
    case UI_OPPONENT_NAME:
        break;
    case UI_KEYBINDSTATUS:
        if (Display_KeyBindPending()) {
            s = UI_GetStringEdString("MP_INGAME", "WAITING_FOR_NEW_KEY");
        }
        else {
            //	s = "Press ENTER or CLICK to change, Press BACKSPACE to clear";
        }
        break;
    case UI_SERVERREFRESHDATE:
        s = UI_Cvar_VariableString(Q_Format("ui_lastServerRefresh_{}", ui_netSource.integer).c_str());
        break;
    default:
        break;
    }

    if (s) {
        return Text_Width(s, scale, menuFont_t::FONT_NONE);
    }
    return 0;
}

static void UI_DrawBotName(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    int value = uiInfo.botIndex;
    const char* text = "";
    if (value >= UI_GetNumBots()) {
        value = 0;
    }
    text = UI_GetBotNameByNumber(value);
    Text_Paint(rect->x, rect->y, scale, color, text, 0, 0, textStyle, iMenuFont);
}

static void UI_DrawBotSkill(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    if (uiInfo.skillIndex >= 0 && uiInfo.skillIndex < numSkillLevels) {
        Text_Paint(rect->x, rect->y, scale, color, (char*)UI_GetStringEdString("MP_INGAME", (char*)skillLevels[uiInfo.skillIndex]), 0, 0, textStyle, iMenuFont);
    }
}

static void UI_DrawRedBlue(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    Text_Paint(rect->x, rect->y, scale, color, (uiInfo.redBlue == 0) ? UI_GetStringEdString("MP_INGAME", "RED") : UI_GetStringEdString("MP_INGAME", "BLUE"), 0,
               0, textStyle, iMenuFont);
}

static void UI_DrawCrosshair(rectDef_t* rect, float scale, vec4_t color)
{
    float size = 32.0f;

    trap->R_SetColor(color);
    if (uiInfo.currentCrosshair < 0 || uiInfo.currentCrosshair >= NUM_CROSSHAIRS) {
        uiInfo.currentCrosshair = 0;
    }

    size = Q_min(rect->w, rect->h);
    UI_DrawHandlePic(rect->x, rect->y, size, size, uiInfo.uiDC.Assets.crosshairShader[uiInfo.currentCrosshair]);
    trap->R_SetColor(nullptr);
}

static void UI_DrawSelectedPlayer(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    if (uiInfo.uiDC.realTime > uiInfo.playerRefresh) {
        uiInfo.playerRefresh = uiInfo.uiDC.realTime + 3000;
        UI_BuildPlayerList();
    }
    Text_Paint(rect->x, rect->y, scale, color, UI_Cvar_VariableString("cg_selectedPlayerName"), 0, 0, textStyle, iMenuFont);
}

static void UI_DrawServerRefreshDate(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    if (uiInfo.serverStatus.refreshActive) {
        vec4_t lowLight, newColor;
        lowLight[0] = 0.8 * color[0];
        lowLight[1] = 0.8 * color[1];
        lowLight[2] = 0.8 * color[2];
        lowLight[3] = 0.8 * color[3];
        LerpColor(color, lowLight, newColor, 0.5 + 0.5 * Q_sin((float)(uiInfo.uiDC.realTime / PULSE_DIVISOR)));

        trap->SE_GetStringTextString("MP_INGAME_GETTINGINFOFORSERVERS", holdSPString, sizeof(holdSPString));
        Text_Paint(rect->x, rect->y, scale, newColor, Q_Format(fmt::runtime(holdSPString), trap->LAN_GetServerCount(UI_SourceForLAN())).c_str(), 0, 0,
                   textStyle, iMenuFont);
    }
    else {
        char buff[64];
        Q_strncpyz(buff, UI_Cvar_VariableString(Q_Format("ui_lastServerRefresh_{}", ui_netSource.integer).c_str()), sizeof(buff));
        trap->SE_GetStringTextString("MP_INGAME_SERVER_REFRESHTIME", holdSPString, sizeof(holdSPString));

        Text_Paint(rect->x, rect->y, scale, color, Q_Format("{}: {}", holdSPString, buff).c_str(), 0, 0, textStyle, iMenuFont);
    }
}

static void UI_DrawServerMOTD(rectDef_t* rect, float scale, vec4_t color, menuFont_t iMenuFont)
{
    if (uiInfo.serverStatus.motdLen) {
        float maxX;

        if (uiInfo.serverStatus.motdWidth == -1) {
            uiInfo.serverStatus.motdWidth = 0;
            uiInfo.serverStatus.motdPaintX = rect->x + 1;
            uiInfo.serverStatus.motdPaintX2 = -1;
        }

        if (uiInfo.serverStatus.motdOffset > uiInfo.serverStatus.motdLen) {
            uiInfo.serverStatus.motdOffset = 0;
            uiInfo.serverStatus.motdPaintX = rect->x + 1;
            uiInfo.serverStatus.motdPaintX2 = -1;
        }

        if (uiInfo.uiDC.realTime > uiInfo.serverStatus.motdTime) {
            uiInfo.serverStatus.motdTime = uiInfo.uiDC.realTime + 10;
            if (uiInfo.serverStatus.motdPaintX <= rect->x + 2) {
                if (uiInfo.serverStatus.motdOffset < uiInfo.serverStatus.motdLen) {
                    uiInfo.serverStatus.motdPaintX += Text_Width(&uiInfo.serverStatus.motd[uiInfo.serverStatus.motdOffset], scale, menuFont_t::FONT_SMALL) - 1;
                    uiInfo.serverStatus.motdOffset++;
                }
                else {
                    uiInfo.serverStatus.motdOffset = 0;
                    if (uiInfo.serverStatus.motdPaintX2 >= 0) {
                        uiInfo.serverStatus.motdPaintX = uiInfo.serverStatus.motdPaintX2;
                    }
                    else {
                        uiInfo.serverStatus.motdPaintX = rect->x + rect->w - 2;
                    }
                    uiInfo.serverStatus.motdPaintX2 = -1;
                }
            }
            else {
                // serverStatus.motdPaintX--;
                uiInfo.serverStatus.motdPaintX -= 2;
                if (uiInfo.serverStatus.motdPaintX2 >= 0) {
                    // serverStatus.motdPaintX2--;
                    uiInfo.serverStatus.motdPaintX2 -= 2;
                }
            }
        }

        maxX = rect->x + rect->w - 2;
        Text_Paint_Limit(&maxX, uiInfo.serverStatus.motdPaintX, rect->y + rect->h - 3, scale, color, &uiInfo.serverStatus.motd[uiInfo.serverStatus.motdOffset],
                         0, 0, iMenuFont);
        if (uiInfo.serverStatus.motdPaintX2 >= 0) {
            float maxX2 = rect->x + rect->w - 2;
            Text_Paint_Limit(&maxX2, uiInfo.serverStatus.motdPaintX2, rect->y + rect->h - 3, scale, color, uiInfo.serverStatus.motd, 0,
                             uiInfo.serverStatus.motdOffset, iMenuFont);
        }
        if (uiInfo.serverStatus.motdOffset && maxX > 0) {
            // if we have an offset ( we are skipping the first part of the string ) and we fit the string
            if (uiInfo.serverStatus.motdPaintX2 == -1) {
                uiInfo.serverStatus.motdPaintX2 = rect->x + rect->w - 2;
            }
        }
        else {
            uiInfo.serverStatus.motdPaintX2 = -1;
        }
    }
}

static void UI_DrawKeyBindStatus(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    if (Display_KeyBindPending()) {
        Text_Paint(rect->x, rect->y, scale, color, UI_GetStringEdString("MP_INGAME", "WAITING_FOR_NEW_KEY"), 0, 0, textStyle, iMenuFont);
    }
    else {
        //		Text_Paint(rect->x, rect->y, scale, color, "Press ENTER or CLICK to change, Press BACKSPACE to clear", 0, 0, textStyle,iMenuFont);
    }
}

static void UI_DrawGLInfo(rectDef_t* rect, float scale, vec4_t color, int textStyle, menuFont_t iMenuFont)
{
    char buff[4096] = {0};
    char* extensionName;
    int y, i = 0;

    Text_Paint(rect->x + 2, rect->y, scale, color, Q_Format("GL_VENDOR: {}", uiInfo.uiDC.glconfig.vendor_string).c_str(), 0, rect->w, textStyle, iMenuFont);
    Text_Paint(rect->x + 2, rect->y + 15, scale, color,
               Q_Format("GL_VERSION: {}: {}", uiInfo.uiDC.glconfig.version_string, uiInfo.uiDC.glconfig.renderer_string).c_str(), 0, rect->w, textStyle,
               iMenuFont);
    Text_Paint(rect->x + 2, rect->y + 30, scale, color,
               Q_Format("GL_PIXELFORMAT: color({}-bits) Z({}-bits) stencil({}-bits)", uiInfo.uiDC.glconfig.colorBits, uiInfo.uiDC.glconfig.depthBits,
                        uiInfo.uiDC.glconfig.stencilBits)
                   .c_str(),
               0, rect->w, textStyle, iMenuFont);

    // build null terminated extension strings
    Q_strncpyz(buff, uiInfo.uiDC.glconfig.extensions_string, sizeof(buff));
    y = rect->y + 45;

    extensionName = strtok(buff, " ");
    while (y < rect->y + rect->h && extensionName != nullptr) {
        if ((i % 2) == 0) {
            Text_Paint(rect->x + 2, y, scale, color, extensionName, 0, (rect->w / 2), textStyle, iMenuFont);
        }
        else {
            Text_Paint(rect->x + rect->w / 2, y, scale, color, extensionName, 0, (rect->w / 2), textStyle, iMenuFont);
            y += 11;
        }

        extensionName = strtok(nullptr, " ");
        i++;
    }
}

/*
=================
UI_Version
=================
*/
static void UI_Version(rectDef_t* rect, float scale, vec4_t color, menuFont_t iMenuFont)
{
    int width;

    width = uiInfo.uiDC.textWidth(JK_VERSION, scale, iMenuFont);

    uiInfo.uiDC.drawText(rect->x - width, rect->y, scale, color, JK_VERSION, 0, 0, 0, iMenuFont);
}

/*
=================
UI_OwnerDraw
=================
*/
// FIXME: table drive
//
static void UI_OwnerDraw(float x, float y, float w, float h, float text_x, float text_y, int ownerDraw, int ownerDrawFlags, int align, float special,
                         float scale, vec4_t color, qhandle_t shader, int textStyle, menuFont_t iMenuFont)
{
    rectDef_t rect;
    forcePowers_t findex;
    forcePowerLevels_t drawRank = forcePowerLevels_t::FORCE_LEVEL_0;
    int iUse = 0;

    rect.x = x + text_x;
    rect.y = y + text_y;
    rect.w = w;
    rect.h = h;

    switch (ownerDraw) {
    case UI_HANDICAP:
        UI_DrawHandicap(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_SOUND_DEVICE:
        UI_DrawSoundDevice(&rect, special, scale, color, textStyle, iMenuFont);
        break;
    case UI_DISPLAY_SCREEN:
    case UI_DISPLAY_RESOLUTION:
    case UI_DISPLAY_REFRESH:
        UI_DrawDisplayRow(ownerDraw, &rect, special, scale, color, textStyle, iMenuFont);
        break;
    case UI_MULTISAMPLE:
        UI_DrawMultisample(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_SKIN_COLOR:
        UI_DrawSkinColor(&rect, scale, color, textStyle, uiSkinColor, team_t::TEAM_FREE, team_t::TEAM_BLUE, iMenuFont);
        break;
    case UI_FORCE_SIDE:
        UI_DrawForceSide(&rect, scale, color, textStyle, uiForceSide, 1, 2, iMenuFont);
        break;
    case UI_JEDI_NONJEDI:
        UI_DrawJediNonJedi(&rect, scale, color, textStyle, uiJediNonJedi, 0, 1, iMenuFont);
        break;
    case UI_FORCE_POINTS:
        UI_DrawGenericNum(&rect, scale, color, textStyle, uiForceAvailable, 1, forceMasteryPoints[forceMasteries_t::FORCE_MASTERY_JEDI_MASTER], ownerDraw,
                          iMenuFont);
        break;
    case UI_FORCE_MASTERY_SET:
        UI_DrawForceMastery(&rect, scale, color, textStyle, uiForceRank, forceMasteries_t::FORCE_MASTERY_UNINITIATED,
                            forceMasteries_t::FORCE_MASTERY_JEDI_MASTER, iMenuFont);
        break;
    case UI_FORCE_RANK:
        UI_DrawForceMastery(&rect, scale, color, textStyle, uiForceRank, forceMasteries_t::FORCE_MASTERY_UNINITIATED,
                            forceMasteries_t::FORCE_MASTERY_JEDI_MASTER, iMenuFont);
        break;
    case UI_FORCE_RANK_HEAL:
    case UI_FORCE_RANK_LEVITATION:
    case UI_FORCE_RANK_SPEED:
    case UI_FORCE_RANK_PUSH:
    case UI_FORCE_RANK_PULL:
    case UI_FORCE_RANK_TELEPATHY:
    case UI_FORCE_RANK_GRIP:
    case UI_FORCE_RANK_LIGHTNING:
    case UI_FORCE_RANK_RAGE:
    case UI_FORCE_RANK_PROTECT:
    case UI_FORCE_RANK_ABSORB:
    case UI_FORCE_RANK_TEAM_HEAL:
    case UI_FORCE_RANK_TEAM_FORCE:
    case UI_FORCE_RANK_DRAIN:
    case UI_FORCE_RANK_SEE:
    case UI_FORCE_RANK_SABERATTACK:
    case UI_FORCE_RANK_SABERDEFEND:
    case UI_FORCE_RANK_SABERTHROW:

        //		uiForceRank
        /*
                uiForceUsed
                // Only fields for white stars
                if (uiForceUsed<3)
                {
                    Menu_ShowItemByName(menu, "lightpowers_team", true);
                }
                else if (uiForceUsed<6)
                {
                    Menu_ShowItemByName(menu, "lightpowers_team", true);
                }
        */

        findex = UI_ForceRankPower(ownerDraw);
        // this will give us the index as long as UI_FORCE_RANK is always one below the first force rank index
        if (uiForcePowerDarkLight[findex] && uiForceSide != uiForcePowerDarkLight[findex]) {
            color[0] *= 0.5;
            color[1] *= 0.5;
            color[2] *= 0.5;
        }
        /*		else if (uiForceRank < UI_ForceColorMinRank[bgForcePowerCost[findex][forcePowerLevels_t::FORCE_LEVEL_1]])
                {
                    color[0] *= 0.5;
                    color[1] *= 0.5;
                    color[2] *= 0.5;
                }
        */
        drawRank = uiForcePowersRank[findex];

        UI_DrawForceStars(&rect, scale, color, textStyle, findex, drawRank, forcePowerLevels_t::FORCE_LEVEL_0, forcePowerLevels_t::NUM_FORCE_POWER_LEVELS - 1);
        break;
    case UI_EFFECTS:
        break;
    case UI_PLAYERMODEL:
        // UI_DrawPlayerModel(&rect);
        break;
    case UI_CLANNAME:
        UI_DrawClanName(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_CLANLOGO:
        UI_DrawClanLogo(&rect, scale, color);
        break;
    case UI_CLANCINEMATIC:
        UI_DrawClanCinematic(&rect, scale, color);
        break;
    case UI_PREVIEWCINEMATIC:
        UI_DrawPreviewCinematic(&rect, scale, color);
        break;
    case UI_GAMETYPE:
        UI_DrawGameType(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_NETGAMETYPE:
        UI_DrawNetGameType(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_AUTOSWITCHLIST:
        UI_DrawAutoSwitch(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_JOINGAMETYPE:
        UI_DrawJoinGameType(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_MAPPREVIEW:
        UI_DrawMapPreview(&rect, scale, color, true);
        break;
    case UI_MAP_TIMETOBEAT:
        break;
    case UI_MAPCINEMATIC:
        UI_DrawMapCinematic(&rect, scale, color, false);
        break;
    case UI_STARTMAPCINEMATIC:
        UI_DrawMapCinematic(&rect, scale, color, true);
        break;
    case UI_SKILL:
        UI_DrawSkill(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_TOTALFORCESTARS:
        //      UI_DrawTotalForceStars(&rect, scale, color, textStyle);
        break;
    case UI_BLUETEAMNAME:
        UI_DrawTeamName(&rect, scale, color, true, textStyle, iMenuFont);
        break;
    case UI_REDTEAMNAME:
        UI_DrawTeamName(&rect, scale, color, false, textStyle, iMenuFont);
        break;
    case UI_BLUETEAM1:
    case UI_BLUETEAM2:
    case UI_BLUETEAM3:
    case UI_BLUETEAM4:
    case UI_BLUETEAM5:
    case UI_BLUETEAM6:
    case UI_BLUETEAM7:
    case UI_BLUETEAM8:
        if (ownerDraw <= UI_BLUETEAM5) {
            iUse = ownerDraw - UI_BLUETEAM1 + 1;
        }
        else {
            iUse = ownerDraw - 274; // unpleasent hack because I don't want to move up all the UI_BLAHTEAM# defines
        }
        UI_DrawTeamMember(&rect, scale, color, true, iUse, textStyle, iMenuFont);
        break;
    case UI_REDTEAM1:
    case UI_REDTEAM2:
    case UI_REDTEAM3:
    case UI_REDTEAM4:
    case UI_REDTEAM5:
    case UI_REDTEAM6:
    case UI_REDTEAM7:
    case UI_REDTEAM8:
        if (ownerDraw <= UI_REDTEAM5) {
            iUse = ownerDraw - UI_REDTEAM1 + 1;
        }
        else {
            iUse = ownerDraw - 277; // unpleasent hack because I don't want to move up all the UI_BLAHTEAM# defines
        }
        UI_DrawTeamMember(&rect, scale, color, false, iUse, textStyle, iMenuFont);
        break;
    case UI_NETSOURCE:
        UI_DrawNetSource(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_NETMAPPREVIEW:
        UI_DrawNetMapPreview(&rect, scale, color);
        break;
    case UI_NETMAPCINEMATIC:
        UI_DrawNetMapCinematic(&rect, scale, color);
        break;
    case UI_NETFILTER:
        UI_DrawNetFilter(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_TIER:
        UI_DrawTier(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_OPPONENTMODEL:
        // UI_DrawOpponent(&rect);
        break;
    case UI_TIERMAP1:
        UI_DrawTierMap(&rect, 0);
        break;
    case UI_TIERMAP2:
        UI_DrawTierMap(&rect, 1);
        break;
    case UI_TIERMAP3:
        UI_DrawTierMap(&rect, 2);
        break;
    case UI_PLAYERLOGO:
        UI_DrawPlayerLogo(&rect, color);
        break;
    case UI_PLAYERLOGO_METAL:
        UI_DrawPlayerLogoMetal(&rect, color);
        break;
    case UI_PLAYERLOGO_NAME:
        UI_DrawPlayerLogoName(&rect, color);
        break;
    case UI_OPPONENTLOGO:
        UI_DrawOpponentLogo(&rect, color);
        break;
    case UI_OPPONENTLOGO_METAL:
        UI_DrawOpponentLogoMetal(&rect, color);
        break;
    case UI_OPPONENTLOGO_NAME:
        UI_DrawOpponentLogoName(&rect, color);
        break;
    case UI_TIER_MAPNAME:
        UI_DrawTierMapName(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_TIER_GAMETYPE:
        UI_DrawTierGameType(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_ALLMAPS_SELECTION:
        UI_DrawAllMapsSelection(&rect, scale, color, textStyle, true, iMenuFont);
        break;
    case UI_MAPS_SELECTION:
        UI_DrawAllMapsSelection(&rect, scale, color, textStyle, false, iMenuFont);
        break;
    case UI_OPPONENT_NAME:
        UI_DrawOpponentName(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_BOTNAME:
        UI_DrawBotName(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_BOTSKILL:
        UI_DrawBotSkill(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_REDBLUE:
        UI_DrawRedBlue(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_CROSSHAIR:
        UI_DrawCrosshair(&rect, scale, color);
        break;
    case UI_SELECTEDPLAYER:
        UI_DrawSelectedPlayer(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_SERVERREFRESHDATE:
        UI_DrawServerRefreshDate(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_SERVERMOTD:
        UI_DrawServerMOTD(&rect, scale, color, iMenuFont);
        break;
    case UI_GLINFO:
        UI_DrawGLInfo(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_KEYBINDSTATUS:
        UI_DrawKeyBindStatus(&rect, scale, color, textStyle, iMenuFont);
        break;
    case UI_VERSION:
        UI_Version(&rect, scale, color, iMenuFont);
        break;
    default:
        break;
    }
}

static bool UI_OwnerDrawVisible(int flags)
{
    bool vis = true;

    while (flags) {
        if (flags & UI_SHOW_FFA) {
            if (UI_CvarGametype() != gametype_t::GT_FFA && UI_CvarGametype() != gametype_t::GT_HOLOCRON && UI_CvarGametype() != gametype_t::GT_JEDIMASTER) {
                vis = false;
            }
            flags &= ~UI_SHOW_FFA;
        }
        if (flags & UI_SHOW_NOTFFA) {
            if (UI_CvarGametype() == gametype_t::GT_FFA || UI_CvarGametype() == gametype_t::GT_HOLOCRON || UI_CvarGametype() == gametype_t::GT_JEDIMASTER) {
                vis = false;
            }
            flags &= ~UI_SHOW_NOTFFA;
        }
        if (flags & UI_SHOW_LEADER) {
            // these need to show when this client can give orders to a player or a group
            if (!uiInfo.teamLeader) {
                vis = false;
            }
            else {
                // if showing yourself
                if (cg_selectedPlayer.integer < uiInfo.myTeamCount && uiInfo.teamClientNums[cg_selectedPlayer.integer] == uiInfo.playerNumber) {
                    vis = false;
                }
            }
            flags &= ~UI_SHOW_LEADER;
        }
        if (flags & UI_SHOW_NOTLEADER) {
            // these need to show when this client is assigning their own status or they are NOT the leader
            if (uiInfo.teamLeader) {
                // if not showing yourself
                if (!(cg_selectedPlayer.integer < uiInfo.myTeamCount && uiInfo.teamClientNums[cg_selectedPlayer.integer] == uiInfo.playerNumber)) {
                    vis = false;
                }
                // these need to show when this client can give orders to a player or a group
            }
            flags &= ~UI_SHOW_NOTLEADER;
        }
        if (flags & UI_SHOW_FAVORITESERVERS) {
            // this assumes you only put this type of display flag on something showing in the proper context
            if (UI_NetSource() != uiNetSource_t::UIAS_FAVORITES) {
                vis = false;
            }
            flags &= ~UI_SHOW_FAVORITESERVERS;
        }
        if (flags & UI_SHOW_NOTFAVORITESERVERS) {
            // this assumes you only put this type of display flag on something showing in the proper context
            if (UI_NetSource() == uiNetSource_t::UIAS_FAVORITES) {
                vis = false;
            }
            flags &= ~UI_SHOW_NOTFAVORITESERVERS;
        }
        if (flags & UI_SHOW_ANYTEAMGAME) {
            if (uiInfo.gameTypes[ui_gametype.integer].gtEnum <= gametype_t::GT_TEAM) {
                vis = false;
            }
            flags &= ~UI_SHOW_ANYTEAMGAME;
        }
        if (flags & UI_SHOW_ANYNONTEAMGAME) {
            if (uiInfo.gameTypes[ui_gametype.integer].gtEnum > gametype_t::GT_TEAM) {
                vis = false;
            }
            flags &= ~UI_SHOW_ANYNONTEAMGAME;
        }
        if (flags & UI_SHOW_NETANYTEAMGAME) {
            if (uiInfo.gameTypes[ui_netGametype.integer].gtEnum <= gametype_t::GT_TEAM) {
                vis = false;
            }
            flags &= ~UI_SHOW_NETANYTEAMGAME;
        }
        if (flags & UI_SHOW_NETANYNONTEAMGAME) {
            if (uiInfo.gameTypes[ui_netGametype.integer].gtEnum > gametype_t::GT_TEAM) {
                vis = false;
            }
            flags &= ~UI_SHOW_NETANYNONTEAMGAME;
        }
        else {
            flags = 0;
        }
    }
    return vis;
}

static bool UI_Handicap_HandleKey(int flags, float* special, fakeAscii_t key)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        int h;
        h = Com_Clamp(5, 100, trap->Cvar_VariableValue("handicap"));
        if (key == fakeAscii_t::A_MOUSE2) {
            h -= 5;
        }
        else {
            h += 5;
        }
        if (h > 100) {
            h = 5;
        }
        else if (h < 5) {
            h = 100;
        }
        trap->Cvar_Set("handicap", Q_Format("{}", h).c_str());
        return true;
    }
    return false;
}

// the next output at each click, Enter or right arrow, the one before with the right button or the left arrow, in the
// order the system's default then each output the system lists
static bool UI_SoundDevice_HandleKey(fakeAscii_t key)
{
    const bool back = key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_CURSOR_LEFT;

    if (!back && key != fakeAscii_t::A_MOUSE1 && key != fakeAscii_t::A_ENTER && key != fakeAscii_t::A_KP_ENTER && key != fakeAscii_t::A_CURSOR_RIGHT) {
        return false;
    }

    const std::span<const std::string_view> outputs = trap->sound->OutputDevices();
    const std::string_view chosen = UI_Cvar_VariableString("s_device");
    const auto found = std::ranges::find(outputs, chosen);
    const int count = static_cast<int>(outputs.size()) + 1; // the default first
    const int index = chosen.empty() || found == outputs.end() ? 0 : static_cast<int>(found - outputs.begin()) + 1;
    const int next = (index + (back ? count - 1 : 1)) % count;
    trap->Cvar_Set("s_device", next == 0 ? "" : std::string(outputs[static_cast<std::size_t>(next - 1)]).c_str());
    return true;
}

// a click on the screen, resolution or refresh rate row: its next value at each click, Enter or right arrow, the one
// before with the right button or the left arrow; false when nothing changes, so that "Apply" is not shown for it
static bool UI_DisplayRow_HandleKey(int ownerDraw, fakeAscii_t key)
{
    const bool back = key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_CURSOR_LEFT;

    if (!back && key != fakeAscii_t::A_MOUSE1 && key != fakeAscii_t::A_ENTER && key != fakeAscii_t::A_KP_ENTER && key != fakeAscii_t::A_CURSOR_RIGHT) {
        return false;
    }

    const std::span<const platform::Screen> screens = trap->ext.Screens();
    const platform::WindowSettings choice = UI_DisplayChoice();
    platform::WindowSettings next;

    if (ownerDraw == UI_DISPLAY_SCREEN) {
        next = platform::NextScreen(screens, choice, back);
    }
    else if (ownerDraw == UI_DISPLAY_RESOLUTION) {
        next = platform::NextResolution(screens, choice, back);
    }
    else {
        next = platform::NextRefreshRate(screens, choice, back);
    }

    if (next == choice) {
        return false;
    }

    UI_SetDisplayChoice(next);
    return true;
}

// a click on the multisampling's row: the next of Off and the samples the driver allows at each click, Enter or right
// arrow, the one before with the right button or the left arrow; applied at once by the renderer
static bool UI_Multisample_HandleKey(fakeAscii_t key)
{
    const bool back = key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_CURSOR_LEFT;

    if (!back && key != fakeAscii_t::A_MOUSE1 && key != fakeAscii_t::A_ENTER && key != fakeAscii_t::A_KP_ENTER && key != fakeAscii_t::A_CURSOR_RIGHT) {
        return false;
    }

    trap->Cvar_SetValue("r_ext_multisample", render_output::NextSamples(static_cast<int>(trap->Cvar_VariableValue("r_ext_multisample")),
                                                                        static_cast<int>(trap->Cvar_VariableValue("r_ext_multisample_avail")), back));
    return true;
}

extern void Item_RunScript(itemDef_t* item, const char* s); // from ui_shared;

// For hot keys on the chat main menu.
static bool UI_Chat_Main_HandleKey(fakeAscii_t key)
{
    menuDef_t* menu;
    itemDef_t* item;

    menu = Menu_GetFocused();

    if (!menu) {
        return (false);
    }

    if ((key == fakeAscii_t::A_1) || (key == fakeAscii_t::A_PLING)) {
        item = Menu_FindItemByName(menu, "attack");
    }
    else if ((key == fakeAscii_t::A_2) || (key == fakeAscii_t::A_AT)) {
        item = Menu_FindItemByName(menu, "defend");
    }
    else if ((key == fakeAscii_t::A_3) || (key == fakeAscii_t::A_HASH)) {
        item = Menu_FindItemByName(menu, "request");
    }
    else if ((key == fakeAscii_t::A_4) || (key == fakeAscii_t::A_STRING)) {
        item = Menu_FindItemByName(menu, "reply");
    }
    else if ((key == fakeAscii_t::A_5) || (key == fakeAscii_t::A_PERCENT)) {
        item = Menu_FindItemByName(menu, "spot");
    }
    else if ((key == fakeAscii_t::A_6) || (key == fakeAscii_t::A_CARET)) {
        item = Menu_FindItemByName(menu, "tactics");
    }
    else {
        return (false);
    }

    if (item) {
        Item_RunScript(item, item->action);
    }

    return (true);
}

// For hot keys on the chat main menu.
static bool UI_Chat_Attack_HandleKey(fakeAscii_t key)
{
    menuDef_t* menu;
    itemDef_t* item;

    menu = Menu_GetFocused();

    if (!menu) {
        return (false);
    }

    if ((key == fakeAscii_t::A_1) || (key == fakeAscii_t::A_PLING)) {
        item = Menu_FindItemByName(menu, "att_01");
    }
    else if ((key == fakeAscii_t::A_2) || (key == fakeAscii_t::A_AT)) {
        item = Menu_FindItemByName(menu, "att_02");
    }
    else if ((key == fakeAscii_t::A_3) || (key == fakeAscii_t::A_HASH)) {
        item = Menu_FindItemByName(menu, "att_03");
    }
    else {
        return (false);
    }

    if (item) {
        Item_RunScript(item, item->action);
    }

    return (true);
}

// For hot keys on the chat main menu.
static bool UI_Chat_Defend_HandleKey(fakeAscii_t key)
{
    menuDef_t* menu;
    itemDef_t* item;

    menu = Menu_GetFocused();

    if (!menu) {
        return (false);
    }

    if ((key == fakeAscii_t::A_1) || (key == fakeAscii_t::A_PLING)) {
        item = Menu_FindItemByName(menu, "def_01");
    }
    else if ((key == fakeAscii_t::A_2) || (key == fakeAscii_t::A_AT)) {
        item = Menu_FindItemByName(menu, "def_02");
    }
    else if ((key == fakeAscii_t::A_3) || (key == fakeAscii_t::A_HASH)) {
        item = Menu_FindItemByName(menu, "def_03");
    }
    else if ((key == fakeAscii_t::A_4) || (key == fakeAscii_t::A_STRING)) {
        item = Menu_FindItemByName(menu, "def_04");
    }
    else {
        return (false);
    }

    if (item) {
        Item_RunScript(item, item->action);
    }

    return (true);
}

// For hot keys on the chat main menu.
static bool UI_Chat_Request_HandleKey(fakeAscii_t key)
{
    menuDef_t* menu;
    itemDef_t* item;

    menu = Menu_GetFocused();

    if (!menu) {
        return (false);
    }

    if ((key == fakeAscii_t::A_1) || (key == fakeAscii_t::A_PLING)) {
        item = Menu_FindItemByName(menu, "req_01");
    }
    else if ((key == fakeAscii_t::A_2) || (key == fakeAscii_t::A_AT)) {
        item = Menu_FindItemByName(menu, "req_02");
    }
    else if ((key == fakeAscii_t::A_3) || (key == fakeAscii_t::A_HASH)) {
        item = Menu_FindItemByName(menu, "req_03");
    }
    else if ((key == fakeAscii_t::A_4) || (key == fakeAscii_t::A_STRING)) {
        item = Menu_FindItemByName(menu, "req_04");
    }
    else if ((key == fakeAscii_t::A_5) || (key == fakeAscii_t::A_PERCENT)) {
        item = Menu_FindItemByName(menu, "req_05");
    }
    else if ((key == fakeAscii_t::A_6) || (key == fakeAscii_t::A_CARET)) {
        item = Menu_FindItemByName(menu, "req_06");
    }
    else {
        return (false);
    }

    if (item) {
        Item_RunScript(item, item->action);
    }

    return (true);
}

// For hot keys on the chat main menu.
static bool UI_Chat_Reply_HandleKey(fakeAscii_t key)
{
    menuDef_t* menu;
    itemDef_t* item;

    menu = Menu_GetFocused();

    if (!menu) {
        return (false);
    }

    if ((key == fakeAscii_t::A_1) || (key == fakeAscii_t::A_PLING)) {
        item = Menu_FindItemByName(menu, "rep_01");
    }
    else if ((key == fakeAscii_t::A_2) || (key == fakeAscii_t::A_AT)) {
        item = Menu_FindItemByName(menu, "rep_02");
    }
    else if ((key == fakeAscii_t::A_3) || (key == fakeAscii_t::A_HASH)) {
        item = Menu_FindItemByName(menu, "rep_03");
    }
    else if ((key == fakeAscii_t::A_4) || (key == fakeAscii_t::A_STRING)) {
        item = Menu_FindItemByName(menu, "rep_04");
    }
    else if ((key == fakeAscii_t::A_5) || (key == fakeAscii_t::A_PERCENT)) {
        item = Menu_FindItemByName(menu, "rep_05");
    }
    else {
        return (false);
    }

    if (item) {
        Item_RunScript(item, item->action);
    }

    return (true);
}

// For hot keys on the chat main menu.
static bool UI_Chat_Spot_HandleKey(fakeAscii_t key)
{
    menuDef_t* menu;
    itemDef_t* item;

    menu = Menu_GetFocused();

    if (!menu) {
        return (false);
    }

    if ((key == fakeAscii_t::A_1) || (key == fakeAscii_t::A_PLING)) {
        item = Menu_FindItemByName(menu, "spot_01");
    }
    else if ((key == fakeAscii_t::A_2) || (key == fakeAscii_t::A_AT)) {
        item = Menu_FindItemByName(menu, "spot_02");
    }
    else if ((key == fakeAscii_t::A_3) || (key == fakeAscii_t::A_HASH)) {
        item = Menu_FindItemByName(menu, "spot_03");
    }
    else if ((key == fakeAscii_t::A_4) || (key == fakeAscii_t::A_STRING)) {
        item = Menu_FindItemByName(menu, "spot_04");
    }
    else if ((key == fakeAscii_t::A_5) || (key == fakeAscii_t::A_PERCENT)) {
        item = Menu_FindItemByName(menu, "spot_05");
    }
    else {
        return (false);
    }

    if (item) {
        Item_RunScript(item, item->action);
    }

    return (true);
}

// For hot keys on the chat main menu.
static bool UI_Chat_Tactical_HandleKey(fakeAscii_t key)
{
    menuDef_t* menu;
    itemDef_t* item;

    menu = Menu_GetFocused();

    if (!menu) {
        return (false);
    }

    if ((key == fakeAscii_t::A_1) || (key == fakeAscii_t::A_PLING)) {
        item = Menu_FindItemByName(menu, "tac_01");
    }
    else if ((key == fakeAscii_t::A_2) || (key == fakeAscii_t::A_AT)) {
        item = Menu_FindItemByName(menu, "tac_02");
    }
    else if ((key == fakeAscii_t::A_3) || (key == fakeAscii_t::A_HASH)) {
        item = Menu_FindItemByName(menu, "tac_03");
    }
    else if ((key == fakeAscii_t::A_4) || (key == fakeAscii_t::A_STRING)) {
        item = Menu_FindItemByName(menu, "tac_04");
    }
    else if ((key == fakeAscii_t::A_5) || (key == fakeAscii_t::A_PERCENT)) {
        item = Menu_FindItemByName(menu, "tac_05");
    }
    else if ((key == fakeAscii_t::A_6) || (key == fakeAscii_t::A_CARET)) {
        item = Menu_FindItemByName(menu, "tac_06");
    }
    else {
        return (false);
    }

    if (item) {
        Item_RunScript(item, item->action);
    }

    return (true);
}

static bool UI_GameType_HandleKey(int flags, float* special, fakeAscii_t key, bool resetMap)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        int oldCount = UI_MapCountByGameType(true);
        int value = ui_gametype.integer;

        // hard coded mess here
        if (key == fakeAscii_t::A_MOUSE2) {
            value--;
            if (value == 2) {
                value = 1;
            }
            else if (value < 2) {
                value = uiInfo.numGameTypes - 1;
            }
        }
        else {
            value++;
            if (value >= uiInfo.numGameTypes) {
                value = 1;
            }
            else if (value == 2) {
                value = 3;
            }
        }

        trap->Cvar_Set("ui_gametype", Q_Format("{}", value).c_str());
        trap->Cvar_Update(&ui_gametype);
        UI_SetCapFragLimits(true);
        if (resetMap && oldCount != UI_MapCountByGameType(true)) {
            trap->Cvar_Set("ui_currentMap", "0");
            trap->Cvar_Update(&ui_currentMap);
            Menu_SetFeederSelection(nullptr, FEEDER_MAPS, 0, nullptr);
        }
        return true;
    }
    return false;
}

// If we're in the solo menu, don't let them see siege maps.
static bool UI_InSoloMenu(void)
{
    menuDef_t* menu;
    itemDef_t* item;
    const char* name = "solo_gametypefield";

    menu = Menu_GetFocused(); // Get current menu (either video or ingame video, I would assume)

    if (!menu) {
        return (false);
    }

    item = Menu_FindItemByName(menu, name);
    if (item) {
        return true;
    }

    return (false);
}

static bool UI_NetGameType_HandleKey(int flags, float* special, fakeAscii_t key)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        int value = ui_netGametype.integer;

        if (key == fakeAscii_t::A_MOUSE2) {
            value--;
            if (UI_InSoloMenu()) {
                if (uiInfo.gameTypes[value].gtEnum == gametype_t::GT_SIEGE) {
                    value--;
                }
            }
        }
        else {
            value++;
            if (UI_InSoloMenu()) {
                if (uiInfo.gameTypes[value].gtEnum == gametype_t::GT_SIEGE) {
                    value++;
                }
            }
        }

        if (value < 0) {
            value = uiInfo.numGameTypes - 1;
        }
        else if (value >= uiInfo.numGameTypes) {
            value = 0;
        }

        trap->Cvar_Set("ui_netGametype", Q_Format("{}", value).c_str());
        trap->Cvar_Update(&ui_netGametype);
        trap->Cvar_Set("ui_actualNetGametype", Q_Format("{}", uiInfo.gameTypes[ui_netGametype.integer].gtEnum).c_str());
        trap->Cvar_Update(&ui_actualNetGametype);
        trap->Cvar_Set("ui_currentNetMap", "0");
        trap->Cvar_Update(&ui_currentNetMap);
        UI_MapCountByGameType(false);
        Menu_SetFeederSelection(nullptr, FEEDER_ALLMAPS, 0, nullptr);
        return true;
    }
    return false;
}

static bool UI_AutoSwitch_HandleKey(int flags, float* special, fakeAscii_t key)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        int switchVal = trap->Cvar_VariableValue("cg_autoswitch");

        if (key == fakeAscii_t::A_MOUSE2) {
            switchVal--;
        }
        else {
            switchVal++;
        }

        if (switchVal < 0) {
            switchVal = 2;
        }
        else if (switchVal >= 3) {
            switchVal = 0;
        }

        trap->Cvar_Set("cg_autoswitch", Q_Format("{}", switchVal).c_str());
        return true;
    }
    return false;
}

static bool UI_JoinGameType_HandleKey(int flags, float* special, fakeAscii_t key)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        int value = ui_joinGametype.integer;

        if (key == fakeAscii_t::A_MOUSE2) {
            value--;
        }
        else {
            value++;
        }

        if (value < 0) {
            value = uiInfo.numJoinGameTypes - 1;
        }
        else if (value >= uiInfo.numJoinGameTypes) {
            value = 0;
        }

        trap->Cvar_Set("ui_joinGametype", Q_Format("{}", value).c_str());
        trap->Cvar_Update(&ui_joinGametype);
        UI_BuildServerDisplayList(true);
        return true;
    }
    return false;
}

static bool UI_Skill_HandleKey(int flags, float* special, fakeAscii_t key)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        int i = trap->Cvar_VariableValue("g_spSkill");

        if (key == fakeAscii_t::A_MOUSE2) {
            i--;
        }
        else {
            i++;
        }

        if (i < 1) {
            i = numSkillLevels;
        }
        else if (i > numSkillLevels) {
            i = 1;
        }

        trap->Cvar_Set("g_spSkill", Q_Format("{}", i).c_str());
        trap->Cvar_Update(&g_spSkill);
        return true;
    }
    return false;
}

static bool UI_TeamName_HandleKey(int flags, float* special, fakeAscii_t key, bool blue)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        int i;
        i = UI_TeamIndexFromName(UI_Cvar_VariableString((blue) ? "ui_blueTeam" : "ui_redTeam"));

        if (key == fakeAscii_t::A_MOUSE2) {
            i--;
        }
        else {
            i++;
        }

        if (i >= uiInfo.teamCount) {
            i = 0;
        }
        else if (i < 0) {
            i = uiInfo.teamCount - 1;
        }

        trap->Cvar_Set((blue) ? "ui_blueTeam" : "ui_redTeam", uiInfo.teamList[i].teamName);

        return true;
    }
    return false;
}

static bool UI_TeamMember_HandleKey(int flags, float* special, fakeAscii_t key, bool blue, int num)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        // 0 - None
        // 1 - Human
        // 2..NumCharacters - Bot
        const formatText_t cvarName = Q_Format("ui_{}team{}", blue ? "blue" : "red", num);
        const char* cvar = cvarName.c_str();
        int value = trap->Cvar_VariableValue(cvar);
        int maxcl = trap->Cvar_VariableValue("sv_maxClients");
        int numval = num;

        numval *= 2;

        if (blue) {
            numval -= 1;
        }

        if (numval > maxcl) {
            return false;
        }

        if (value < 1) {
            value = 1;
        }

        if (key == fakeAscii_t::A_MOUSE2) {
            value--;
        }
        else {
            value++;
        }

        /*if (ui_actualNetGameType.integer >= gametype_t::GT_TEAM) {
        if (value >= uiInfo.characterCount + 2) {
        value = 0;
        } else if (value < 0) {
        value = uiInfo.characterCount + 2 - 1;
        }
        } else {*/
        if (value >= UI_GetNumBots() + 2) {
            value = 1;
        }
        else if (value < 1) {
            value = UI_GetNumBots() + 2 - 1;
        }
        //}

        trap->Cvar_Set(cvar, Q_Format("{}", value).c_str());
        return true;
    }
    return false;
}

static bool UI_NetSource_HandleKey(int flags, float* special, fakeAscii_t key)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        uiNetSource_t value = UI_NetSource();

        if (key == fakeAscii_t::A_MOUSE2) {
            value--;
        }
        else {
            value++;
        }

        if (value >= uiNetSource_t::UIAS_GLOBAL1 && value <= uiNetSource_t::UIAS_GLOBAL5) {
            char masterstr[2], cvarname[sizeof("sv_master1")];

            while (value >= uiNetSource_t::UIAS_GLOBAL1 && value <= uiNetSource_t::UIAS_GLOBAL5) {
                Q_FormatTo(cvarname, "sv_master{}", value);
                trap->Cvar_VariableStringBuffer(cvarname, masterstr, sizeof(masterstr));
                if (*masterstr)
                    break;

                if (key == fakeAscii_t::A_MOUSE2) {
                    value--;
                }
                else {
                    value++;
                }
            }
        }

        if (value > uiNetSource_t::UIAS_FAVORITES) {
            value = uiNetSource_t::UIAS_LOCAL;
        }
        else if (value < uiNetSource_t::UIAS_LOCAL) {
            value = uiNetSource_t::UIAS_FAVORITES;
        }

        trap->Cvar_Set("ui_netSource", Q_Format("{}", value).c_str());
        trap->Cvar_Update(&ui_netSource);

        UI_BuildServerDisplayList(true);
        if (!(UI_NetSource() >= uiNetSource_t::UIAS_GLOBAL1 && UI_NetSource() <= uiNetSource_t::UIAS_GLOBAL5)) {
            UI_StartServerRefresh(true);
        }
        return true;
    }
    return false;
}

static bool UI_NetFilter_HandleKey(int flags, float* special, fakeAscii_t key)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        int value = ui_serverFilterType.integer;

        if (key == fakeAscii_t::A_MOUSE2) {
            value--;
        }
        else {
            value++;
        }

        if (value > uiInfo.modCount) {
            value = 0;
        }
        else if (value < 0) {
            value = uiInfo.modCount;
        }

        trap->Cvar_Set("ui_serverFilterType", Q_Format("{}", value).c_str());
        trap->Cvar_Update(&ui_serverFilterType);

        UI_BuildServerDisplayList(true);
        return true;
    }
    return false;
}

static bool UI_OpponentName_HandleKey(int flags, float* special, fakeAscii_t key)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        if (key == fakeAscii_t::A_MOUSE2) {
            UI_PriorOpponent();
        }
        else {
            UI_NextOpponent();
        }
        return true;
    }
    return false;
}

static bool UI_BotName_HandleKey(int flags, float* special, fakeAscii_t key)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        //		int game = trap->Cvar_VariableValue("g_gametype");
        int value = uiInfo.botIndex;

        if (key == fakeAscii_t::A_MOUSE2) {
            value--;
        }
        else {
            value++;
        }

        /*
        if (game >= gametype_t::GT_TEAM) {
        if (value >= uiInfo.characterCount + 2) {
        value = 0;
        } else if (value < 0) {
        value = uiInfo.characterCount + 2 - 1;
        }
        } else {
        */
        if (value >= UI_GetNumBots() /* + 2*/) {
            value = 0;
        }
        else if (value < 0) {
            value = UI_GetNumBots() /* + 2*/ - 1;
        }
        //}
        uiInfo.botIndex = value;
        return true;
    }
    return false;
}

static bool UI_BotSkill_HandleKey(int flags, float* special, fakeAscii_t key)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        if (key == fakeAscii_t::A_MOUSE2) {
            uiInfo.skillIndex--;
        }
        else {
            uiInfo.skillIndex++;
        }
        if (uiInfo.skillIndex >= numSkillLevels) {
            uiInfo.skillIndex = 0;
        }
        else if (uiInfo.skillIndex < 0) {
            uiInfo.skillIndex = numSkillLevels - 1;
        }
        return true;
    }
    return false;
}

static bool UI_RedBlue_HandleKey(int flags, float* special, fakeAscii_t key)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        uiInfo.redBlue ^= 1;
        return true;
    }
    return false;
}

static bool UI_Crosshair_HandleKey(int flags, float* special, fakeAscii_t key)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        if (key == fakeAscii_t::A_MOUSE2) {
            uiInfo.currentCrosshair--;
        }
        else {
            uiInfo.currentCrosshair++;
        }

        if (uiInfo.currentCrosshair >= NUM_CROSSHAIRS) {
            uiInfo.currentCrosshair = 0;
        }
        else if (uiInfo.currentCrosshair < 0) {
            uiInfo.currentCrosshair = NUM_CROSSHAIRS - 1;
        }
        trap->Cvar_Set("cg_drawCrosshair", Q_Format("{}", uiInfo.currentCrosshair).c_str());
        return true;
    }
    return false;
}

static bool UI_SelectedPlayer_HandleKey(int flags, float* special, fakeAscii_t key)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        int selected;

        UI_BuildPlayerList();
        if (!uiInfo.teamLeader) {
            return false;
        }
        selected = trap->Cvar_VariableValue("cg_selectedPlayer");

        if (key == fakeAscii_t::A_MOUSE2) {
            selected--;
        }
        else {
            selected++;
        }

        if (selected > uiInfo.myTeamCount) {
            selected = 0;
        }
        else if (selected < 0) {
            selected = uiInfo.myTeamCount;
        }

        if (selected == uiInfo.myTeamCount) {
            trap->Cvar_Set("cg_selectedPlayerName", "Everyone");
        }
        else {
            trap->Cvar_Set("cg_selectedPlayerName", uiInfo.teamNames[selected]);
        }
        trap->Cvar_Set("cg_selectedPlayer", Q_Format("{}", selected).c_str());
    }
    return false;
}

/*
static bool UI_VoiceChat_HandleKey(int flags, float *special, fakeAscii_t key)
{

    bool ret = false;

    switch(key)
    {
        case fakeAscii_t::A_1:
        case fakeAscii_t::A_KP_1:
            ret = true;
            break;
        case fakeAscii_t::A_2:
        case fakeAscii_t::A_KP_2:
            ret = true;
            break;

    }

    return ret;
}
*/

static bool UI_OwnerDrawHandleKey(int ownerDraw, int flags, float* special, fakeAscii_t key)
{
    forcePowers_t findex;
    int iUse = 0;

    switch (ownerDraw) {
    case UI_HANDICAP:
        return UI_Handicap_HandleKey(flags, special, key);
        break;
    case UI_SOUND_DEVICE:
        return UI_SoundDevice_HandleKey(key);
    case UI_DISPLAY_SCREEN:
    case UI_DISPLAY_RESOLUTION:
    case UI_DISPLAY_REFRESH:
        return UI_DisplayRow_HandleKey(ownerDraw, key);
    case UI_MULTISAMPLE:
        return UI_Multisample_HandleKey(key);
    case UI_SKIN_COLOR:
        return UI_SkinColor_HandleKey(flags, special, key, uiSkinColor, team_t::TEAM_FREE, team_t::TEAM_BLUE, ownerDraw);
        break;
    case UI_FORCE_SIDE:
        return UI_ForceSide_HandleKey(flags, special, key, uiForceSide, 1, 2, ownerDraw);
        break;
    case UI_JEDI_NONJEDI:
        return UI_JediNonJedi_HandleKey(flags, special, key, uiJediNonJedi, 0, 1, ownerDraw);
        break;
    case UI_FORCE_MASTERY_SET:
        return UI_ForceMaxRank_HandleKey(flags, special, key, uiForceRank, forceMasteries_t::FORCE_MASTERY_INITIATE,
                                         forceMasteries_t::FORCE_MASTERY_JEDI_MASTER, ownerDraw);
        break;
    case UI_FORCE_RANK:
        break;
    case UI_CHAT_MAIN:
        return UI_Chat_Main_HandleKey(key);
        break;
    case UI_CHAT_ATTACK:
        return UI_Chat_Attack_HandleKey(key);
        break;
    case UI_CHAT_DEFEND:
        return UI_Chat_Defend_HandleKey(key);
        break;
    case UI_CHAT_REQUEST:
        return UI_Chat_Request_HandleKey(key);
        break;
    case UI_CHAT_REPLY:
        return UI_Chat_Reply_HandleKey(key);
        break;
    case UI_CHAT_SPOT:
        return UI_Chat_Spot_HandleKey(key);
        break;
    case UI_CHAT_TACTICAL:
        return UI_Chat_Tactical_HandleKey(key);
        break;
    case UI_FORCE_RANK_HEAL:
    case UI_FORCE_RANK_LEVITATION:
    case UI_FORCE_RANK_SPEED:
    case UI_FORCE_RANK_PUSH:
    case UI_FORCE_RANK_PULL:
    case UI_FORCE_RANK_TELEPATHY:
    case UI_FORCE_RANK_GRIP:
    case UI_FORCE_RANK_LIGHTNING:
    case UI_FORCE_RANK_RAGE:
    case UI_FORCE_RANK_PROTECT:
    case UI_FORCE_RANK_ABSORB:
    case UI_FORCE_RANK_TEAM_HEAL:
    case UI_FORCE_RANK_TEAM_FORCE:
    case UI_FORCE_RANK_DRAIN:
    case UI_FORCE_RANK_SEE:
    case UI_FORCE_RANK_SABERATTACK:
    case UI_FORCE_RANK_SABERDEFEND:
    case UI_FORCE_RANK_SABERTHROW:
        findex = UI_ForceRankPower(ownerDraw);
        // this will give us the index as long as UI_FORCE_RANK is always one below the first force rank index
        return UI_ForcePowerRank_HandleKey(flags, special, key, uiForcePowersRank[findex], forcePowerLevels_t::FORCE_LEVEL_0,
                                           forcePowerLevels_t::NUM_FORCE_POWER_LEVELS - 1, ownerDraw);
        break;
    case UI_EFFECTS:
        break;
    case UI_GAMETYPE:
        return UI_GameType_HandleKey(flags, special, key, true);
        break;
    case UI_NETGAMETYPE:
        return UI_NetGameType_HandleKey(flags, special, key);
        break;
    case UI_AUTOSWITCHLIST:
        return UI_AutoSwitch_HandleKey(flags, special, key);
        break;
    case UI_JOINGAMETYPE:
        return UI_JoinGameType_HandleKey(flags, special, key);
        break;
    case UI_SKILL:
        return UI_Skill_HandleKey(flags, special, key);
        break;
    case UI_BLUETEAMNAME:
        return UI_TeamName_HandleKey(flags, special, key, true);
        break;
    case UI_REDTEAMNAME:
        return UI_TeamName_HandleKey(flags, special, key, false);
        break;
    case UI_BLUETEAM1:
    case UI_BLUETEAM2:
    case UI_BLUETEAM3:
    case UI_BLUETEAM4:
    case UI_BLUETEAM5:
    case UI_BLUETEAM6:
    case UI_BLUETEAM7:
    case UI_BLUETEAM8:
        if (ownerDraw <= UI_BLUETEAM5) {
            iUse = ownerDraw - UI_BLUETEAM1 + 1;
        }
        else {
            iUse = ownerDraw - 274; // unpleasent hack because I don't want to move up all the UI_BLAHTEAM# defines
        }

        UI_TeamMember_HandleKey(flags, special, key, true, iUse);
        break;
    case UI_REDTEAM1:
    case UI_REDTEAM2:
    case UI_REDTEAM3:
    case UI_REDTEAM4:
    case UI_REDTEAM5:
    case UI_REDTEAM6:
    case UI_REDTEAM7:
    case UI_REDTEAM8:
        if (ownerDraw <= UI_REDTEAM5) {
            iUse = ownerDraw - UI_REDTEAM1 + 1;
        }
        else {
            iUse = ownerDraw - 277; // unpleasent hack because I don't want to move up all the UI_BLAHTEAM# defines
        }
        UI_TeamMember_HandleKey(flags, special, key, false, iUse);
        break;
    case UI_NETSOURCE:
        UI_NetSource_HandleKey(flags, special, key);
        break;
    case UI_NETFILTER:
        UI_NetFilter_HandleKey(flags, special, key);
        break;
    case UI_OPPONENT_NAME:
        UI_OpponentName_HandleKey(flags, special, key);
        break;
    case UI_BOTNAME:
        return UI_BotName_HandleKey(flags, special, key);
        break;
    case UI_BOTSKILL:
        return UI_BotSkill_HandleKey(flags, special, key);
        break;
    case UI_REDBLUE:
        UI_RedBlue_HandleKey(flags, special, key);
        break;
    case UI_CROSSHAIR:
        UI_Crosshair_HandleKey(flags, special, key);
        break;
    case UI_SELECTEDPLAYER:
        UI_SelectedPlayer_HandleKey(flags, special, key);
        break;
    //	case UI_VOICECHAT:
    //		UI_VoiceChat_HandleKey(flags, special, key);
    //		break;
    default:
        break;
    }

    return false;
}

static float UI_GetValue(int ownerDraw)
{
    return 0;
}

/*
=================
UI_ServersQsortCompare
=================
*/
static int QDECL UI_ServersQsortCompare(const void* arg1, const void* arg2)
{
    return trap->LAN_CompareServers(UI_SourceForLAN(), uiInfo.serverStatus.sortKey, uiInfo.serverStatus.sortDir, *(int*)arg1, *(int*)arg2);
}

/*
=================
UI_ServersSort
=================
*/
void UI_ServersSort(serverSortKey_t column, bool force)
{
    if (!force) {
        if (uiInfo.serverStatus.sortKey == column) {
            return;
        }
    }

    uiInfo.serverStatus.sortKey = column;
    qsort(&uiInfo.serverStatus.displayServers[0], uiInfo.serverStatus.numDisplayServers, sizeof(int), UI_ServersQsortCompare);
}

constexpr int MODSBUFSIZE = MAX_MODS * MAX_QPATH;

/*
===============
UI_LoadMods
===============
*/
static void UI_LoadMods()
{
    int numdirs;
    char dirlist[MODSBUFSIZE];
    char* dirptr;
    char* descptr;
    int i;
    int dirlen;
    char version[MAX_CVAR_VALUE_STRING] = {0};

    trap->SE_GetStringTextString("MENUS_ALL", sAll, sizeof(sAll));

    // To still display base game with old engine
    Q_strncpyz(version, UI_Cvar_VariableString("version"), sizeof(version));
    if (strstr(version, "2003")) {
        trap->SE_GetStringTextString("MENUS_JEDI_ACADEMY", sJediAcademy, sizeof(sJediAcademy));
        uiInfo.modList[0].modName = String_Alloc("");
        uiInfo.modList[0].modDescr = String_Alloc(sJediAcademy);
        uiInfo.modCount = 1;
    }
    else
        uiInfo.modCount = 0;

    numdirs = trap->FS_GetFileList("$modlist", "", dirlist, sizeof(dirlist));
    dirptr = dirlist;
    for (i = 0; i < numdirs; i++) {
        dirlen = strlen(dirptr) + 1;
        descptr = dirptr + dirlen;
        uiInfo.modList[uiInfo.modCount].modName = String_Alloc(dirptr);
        uiInfo.modList[uiInfo.modCount].modDescr = String_Alloc(descptr);
        dirptr += dirlen + strlen(descptr) + 1;
        uiInfo.modCount++;
        if (uiInfo.modCount >= MAX_MODS) {
            break;
        }
    }
}

/*
===============
UI_LoadMovies
===============
*/
static void UI_LoadMovies()
{
    char movielist[4096];
    char* moviename;
    int i, len;

    uiInfo.movieCount = trap->FS_GetFileList("video", "roq", movielist, 4096);

    if (uiInfo.movieCount) {
        if (uiInfo.movieCount > MAX_MOVIES) {
            uiInfo.movieCount = MAX_MOVIES;
        }
        moviename = movielist;
        for (i = 0; i < uiInfo.movieCount; i++) {
            len = strlen(moviename);
            if (!Q_stricmp(moviename + len - 4, ".roq")) {
                moviename[len - 4] = '\0';
            }
            Q_strupr(moviename);
            uiInfo.movieList[i] = String_Alloc(moviename);
            moviename += len + 1;
        }
    }
}

/*
===============
UI_LoadDemos
===============
*/
constexpr int MAX_DEMO_FOLDER_DEPTH = 8;
typedef struct loadDemoContext_s {
    int depth;
    bool warned;
    char demoList[MAX_DEMOLIST];
    char directoryList[MAX_DEMOLIST];
    char* dirListHead;
} loadDemoContext_t;

static void UI_LoadDemosInDirectory(loadDemoContext_t* ctx, const char* directory)
{
    char* demoname = nullptr;
    char demoExt[32] = {0};
    int protocol = trap->Cvar_VariableValue("com_protocol");
    int protocolLegacy = trap->Cvar_VariableValue("com_legacyprotocol");
    char* dirListEnd;
    int j;

    if (ctx->depth > MAX_DEMO_FOLDER_DEPTH) {
        if (!ctx->warned) {
            ctx->warned = true;
            Q_PrintlnWarning("Maximum demo folder depth ({}) was reached.", MAX_DEMO_FOLDER_DEPTH);
        }

        return;
    }

    ctx->depth++;

    if (!protocol)
        protocol = trap->Cvar_VariableValue("protocol");
    if (protocolLegacy == protocol)
        protocolLegacy = 0;

    Q_FormatTo(demoExt, ".{}{}", DEMO_EXTENSION, protocol);

    uiInfo.demoCount += trap->FS_GetFileList(directory, demoExt, ctx->demoList, sizeof(ctx->demoList));

    demoname = ctx->demoList;

    for (j = 0; j < 2; j++) {
        if (uiInfo.demoCount > MAX_DEMOS)
            uiInfo.demoCount = MAX_DEMOS;

        for (; uiInfo.loadedDemos < uiInfo.demoCount; uiInfo.loadedDemos++) {
            char dirPath[MAX_QPATH];
            size_t len;

            Q_strncpyz(dirPath, directory + strlen(DEMO_DIRECTORY), sizeof(dirPath));
            Q_strcat(dirPath, sizeof(dirPath), "/");
            len = strlen(demoname);
            Q_FormatTo(std::span(uiInfo.demoList[uiInfo.loadedDemos], sizeof(uiInfo.demoList[0])), "{}{}", dirPath + 1, demoname);
            demoname += len + 1;
        }

        if (!j) {
            if (protocolLegacy > 0 && uiInfo.demoCount < MAX_DEMOS) {
                Q_FormatTo(demoExt, ".{}{}", DEMO_EXTENSION, protocolLegacy);
                uiInfo.demoCount += trap->FS_GetFileList(directory, demoExt, ctx->demoList, sizeof(ctx->demoList));
                demoname = ctx->demoList;
            }
            else
                break;
        }
    }

    dirListEnd = ctx->directoryList + sizeof(ctx->directoryList);
    if (ctx->dirListHead < dirListEnd) {
        int i;
        int dirListSpaceRemaining = dirListEnd - ctx->dirListHead;
        int numFiles = trap->FS_GetFileList(directory, "/", ctx->dirListHead, dirListSpaceRemaining);
        char* dirList;
        char* childDirListBase;
        char* fileName;

        // Find end of this list so we have a base pointer for the child folders to use
        dirList = ctx->dirListHead;
        for (i = 0; i < numFiles; i++) {
            ctx->dirListHead += strlen(ctx->dirListHead) + 1;
        }
        ctx->dirListHead++;

        // Iterate through child directories
        childDirListBase = ctx->dirListHead;
        fileName = dirList;
        for (i = 0; i < numFiles; i++) {
            size_t len = strlen(fileName);

            if (Q_stricmp(fileName, ".") && Q_stricmp(fileName, "..") && len)
                UI_LoadDemosInDirectory(ctx, Q_Format("{}/{}", directory, fileName).c_str());

            ctx->dirListHead = childDirListBase;
            fileName += len + 1;
        }

        assert((fileName + 1) == childDirListBase);
    }

    ctx->depth--;
}

static void InitLoadDemoContext(loadDemoContext_t* ctx)
{
    ctx->warned = false;
    ctx->depth = 0;
    ctx->dirListHead = ctx->directoryList;
}

static void UI_LoadDemos(void)
{
    loadDemoContext_t loadDemoContext;
    InitLoadDemoContext(&loadDemoContext);

    uiInfo.demoCount = 0;
    uiInfo.loadedDemos = 0;
    memset(uiInfo.demoList, 0, sizeof(uiInfo.demoList));
    UI_LoadDemosInDirectory(&loadDemoContext, DEMO_DIRECTORY);
}

static bool UI_SetNextMap(int actual, int index)
{
    int i;
    for (i = actual + 1; i < uiInfo.mapCount; i++) {
        if (uiInfo.mapList[i].active) {
            Menu_SetFeederSelection(nullptr, FEEDER_MAPS, index + 1, "skirmish");
            return true;
        }
    }
    return false;
}

static void UI_StartSkirmish(bool next)
{
    int i, k, delay, temp;
    gametype_t g;
    float skill;
    char buff[MAX_STRING_CHARS];

    temp = trap->Cvar_VariableValue("g_gametype");
    trap->Cvar_Set("ui_gameType", Q_Format("{}", temp).c_str());

    if (next) {
        int actual;
        int index = trap->Cvar_VariableValue("ui_mapIndex");
        UI_MapCountByGameType(true);
        UI_SelectedMap(index, &actual);
        if (UI_SetNextMap(actual, index)) {
        }
        else {
            UI_GameType_HandleKey(0, nullptr, fakeAscii_t::A_MOUSE1, false);
            UI_MapCountByGameType(true);
            Menu_SetFeederSelection(nullptr, FEEDER_MAPS, 0, "skirmish");
        }
    }

    g = uiInfo.gameTypes[ui_gametype.integer].gtEnum;
    trap->Cvar_SetValue("g_gametype", std::to_underlying(g));
    trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, Q_Format("wait ; wait ; map {}\n", uiInfo.mapList[ui_currentMap.integer].mapLoadName).c_str());
    skill = trap->Cvar_VariableValue("g_spSkill");
    trap->Cvar_Set("ui_scoreMap", uiInfo.mapList[ui_currentMap.integer].mapName);

    k = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_opponentName"));

    trap->Cvar_Set("ui_singlePlayerActive", "1");

    // set up sp overrides, will be replaced on postgame
    temp = trap->Cvar_VariableValue("capturelimit");
    trap->Cvar_Set("ui_saveCaptureLimit", Q_Format("{}", temp).c_str());
    temp = trap->Cvar_VariableValue("fraglimit");
    trap->Cvar_Set("ui_saveFragLimit", Q_Format("{}", temp).c_str());
    temp = trap->Cvar_VariableValue("duel_fraglimit");
    trap->Cvar_Set("ui_saveDuelLimit", Q_Format("{}", temp).c_str());

    UI_SetCapFragLimits(false);

    temp = trap->Cvar_VariableValue("cg_drawTimer");
    trap->Cvar_Set("ui_drawTimer", Q_Format("{}", temp).c_str());
    temp = trap->Cvar_VariableValue("g_doWarmup");
    trap->Cvar_Set("ui_doWarmup", Q_Format("{}", temp).c_str());
    temp = trap->Cvar_VariableValue("g_friendlyFire");
    trap->Cvar_Set("ui_friendlyFire", Q_Format("{}", temp).c_str());
    temp = trap->Cvar_VariableValue("sv_maxClients");
    trap->Cvar_Set("ui_maxClients", Q_Format("{}", temp).c_str());
    temp = trap->Cvar_VariableValue("g_warmup");
    trap->Cvar_Set("ui_Warmup", Q_Format("{}", temp).c_str());
    temp = trap->Cvar_VariableValue("sv_pure");
    trap->Cvar_Set("ui_pure", Q_Format("{}", temp).c_str());

    trap->Cvar_Set("cg_cameraOrbit", "0");
    //	trap->Cvar_Set("cg_thirdPerson", "0");
    trap->Cvar_Set("cg_drawTimer", "1");
    trap->Cvar_Set("g_doWarmup", "1");
    trap->Cvar_Set("g_warmup", "15");
    trap->Cvar_Set("sv_pure", "0");
    trap->Cvar_Set("g_friendlyFire", "0");
    //	trap->Cvar_Set("g_redTeam", UI_Cvar_VariableString("ui_teamName"));
    //	trap->Cvar_Set("g_blueTeam", UI_Cvar_VariableString("ui_opponentName"));

    if (trap->Cvar_VariableValue("ui_recordSPDemo")) {
        Q_FormatTo(std::span(buff, MAX_STRING_CHARS), "{}_{}", uiInfo.mapList[ui_currentMap.integer].mapLoadName, g);
        trap->Cvar_Set("ui_recordSPDemoName", buff);
    }

    delay = 500;

    if (g == gametype_t::GT_DUEL || g == gametype_t::GT_POWERDUEL) {
        temp = uiInfo.mapList[ui_currentMap.integer].teamMembers * 2;
        trap->Cvar_Set("sv_maxClients", Q_Format("{}", temp).c_str());
        Q_FormatTo(buff, "wait ; addbot {} {:f} , {} \n", uiInfo.mapList[ui_currentMap.integer].opponentName, skill, delay);
        trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, buff);
    }
    else if (g == gametype_t::GT_HOLOCRON || g == gametype_t::GT_JEDIMASTER) {
        temp = uiInfo.mapList[ui_currentMap.integer].teamMembers * 2;
        trap->Cvar_Set("sv_maxClients", Q_Format("{}", temp).c_str());
        for (i = 0; i < uiInfo.mapList[ui_currentMap.integer].teamMembers; i++) {
            Q_FormatTo(buff, "addbot \"{}\" {:f} {} {} {}\n", UI_AIFromName(uiInfo.teamList[k].teamMembers[i]), skill,
                       (g == gametype_t::GT_HOLOCRON) ? "" : "Blue", delay, uiInfo.teamList[k].teamMembers[i]);
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, buff);
            delay += 500;
        }
        k = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
        for (i = 0; i < uiInfo.mapList[ui_currentMap.integer].teamMembers - 1; i++) {
            Q_FormatTo(buff, "addbot \"{}\" {:f} {} {} {}\n", UI_AIFromName(uiInfo.teamList[k].teamMembers[i]), skill,
                       (g == gametype_t::GT_HOLOCRON) ? "" : "Red", delay, uiInfo.teamList[k].teamMembers[i]);
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, buff);
            delay += 500;
        }
    }
    else {
        temp = uiInfo.mapList[ui_currentMap.integer].teamMembers * 2;
        trap->Cvar_Set("sv_maxClients", Q_Format("{}", temp).c_str());
        for (i = 0; i < uiInfo.mapList[ui_currentMap.integer].teamMembers; i++) {
            Q_FormatTo(buff, "addbot \"{}\" {:f} {} {} {}\n", UI_AIFromName(uiInfo.teamList[k].teamMembers[i]), skill, (g == gametype_t::GT_FFA) ? "" : "Blue",
                       delay, uiInfo.teamList[k].teamMembers[i]);
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, buff);
            delay += 500;
        }
        k = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
        for (i = 0; i < uiInfo.mapList[ui_currentMap.integer].teamMembers - 1; i++) {
            Q_FormatTo(buff, "addbot \"{}\" {:f} {} {} {}\n", UI_AIFromName(uiInfo.teamList[k].teamMembers[i]), skill, (g == gametype_t::GT_FFA) ? "" : "Red",
                       delay, uiInfo.teamList[k].teamMembers[i]);
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, buff);
            delay += 500;
        }
    }
    if (g >= gametype_t::GT_TEAM) {
        trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, "wait 5; team Red\n");
    }
}

static void UI_Update(const char* name)
{
    int val = trap->Cvar_VariableValue(name);

    if (!Q_stricmp(name, "ui_SetName")) {
        char buf[MAX_NETNAME] = {0};
        Q_strncpyz(buf, UI_Cvar_VariableString("ui_Name"), sizeof(buf));
        trap->Cvar_Set("name", buf);
    }
    else if (!Q_stricmp(name, "ui_GetName")) {
        char buf[MAX_NETNAME] = {0};
        Q_strncpyz(buf, UI_Cvar_VariableString("name"), sizeof(buf));
        trap->Cvar_Set("ui_Name", buf);
    }
    else if (Q_stricmp(name, "ui_r_displayState") == 0) {
        UI_SetRefreshChoice(UI_DisplayChoice());
    }
    else if (Q_stricmp(name, "ui_r_lodbias") == 0) {
        switch (val) {
        case 0:
            trap->Cvar_SetValue("ui_r_subdivisions", 4);
            break;
        case 1:
            trap->Cvar_SetValue("ui_r_subdivisions", 12);
            break;

        case 2:
            trap->Cvar_SetValue("ui_r_subdivisions", 20);
            break;
        }
    }
    else if (Q_stricmp(name, "ui_r_glCustom") == 0) {
        switch (val) {
        case 0: // high quality

            trap->Cvar_SetValue("ui_r_subdivisions", 4);
            trap->Cvar_SetValue("ui_r_lodbias", 0);
            trap->Cvar_SetValue("ui_r_picmip", 0);
            trap->Cvar_SetValue("ui_r_texturebits", 32);
            trap->Cvar_SetValue("ui_r_fastSky", 0);
            trap->Cvar_SetValue("ui_r_inGameVideo", 1);
            //	trap->Cvar_SetValue( "ui_cg_shadows", 2 );//stencil
            trap->Cvar_Set("ui_r_texturemode", "GL_LINEAR_MIPMAP_LINEAR");
            break;

        case 1: // normal
            trap->Cvar_SetValue("ui_r_subdivisions", 4);
            trap->Cvar_SetValue("ui_r_lodbias", 0);
            trap->Cvar_SetValue("ui_r_picmip", 1);
            trap->Cvar_SetValue("ui_r_texturebits", 0);
            trap->Cvar_SetValue("ui_r_fastSky", 0);
            trap->Cvar_SetValue("ui_r_inGameVideo", 1);
            //	trap->Cvar_SetValue( "ui_cg_shadows", 2 );
            trap->Cvar_Set("ui_r_texturemode", "GL_LINEAR_MIPMAP_LINEAR");
            break;

        case 2: // fast

            trap->Cvar_SetValue("ui_r_subdivisions", 12);
            trap->Cvar_SetValue("ui_r_lodbias", 1);
            trap->Cvar_SetValue("ui_r_picmip", 2);
            trap->Cvar_SetValue("ui_r_texturebits", 0);
            trap->Cvar_SetValue("ui_r_fastSky", 1);
            trap->Cvar_SetValue("ui_r_inGameVideo", 0);
            //	trap->Cvar_SetValue( "ui_cg_shadows", 1 );
            trap->Cvar_Set("ui_r_texturemode", "GL_LINEAR_MIPMAP_NEAREST");
            break;

        case 3: // fastest

            trap->Cvar_SetValue("ui_r_subdivisions", 20);
            trap->Cvar_SetValue("ui_r_lodbias", 2);
            trap->Cvar_SetValue("ui_r_picmip", 3);
            trap->Cvar_SetValue("ui_r_texturebits", 16);
            trap->Cvar_SetValue("ui_r_fastSky", 1);
            trap->Cvar_SetValue("ui_r_inGameVideo", 0);
            //	trap->Cvar_SetValue( "ui_cg_shadows", 0 );
            trap->Cvar_Set("ui_r_texturemode", "GL_LINEAR_MIPMAP_NEAREST");
            break;
        }
    }
    else if (Q_stricmp(name, "ui_mousePitch") == 0) {
        if (val == 0)
            trap->Cvar_SetValue("m_pitch", 0.022f);
        else
            trap->Cvar_SetValue("m_pitch", -0.022f);
    }
    else if (Q_stricmp(name, "ui_mousePitchVeh") == 0) {
        if (val == 0)
            trap->Cvar_SetValue("m_pitchVeh", 0.022f);
        else
            trap->Cvar_SetValue("m_pitchVeh", -0.022f);
    }
}

int gUISelectedMap = 0;

// Whether the video page changed a renderer setting that takes effect only when the renderer starts again.
static bool UI_VideoNeedsRestart(void)
{
    static constexpr const char* restartSettings[] = {"r_picmip",       "r_texturebits",    "r_detailtextures", "r_ext_compress_textures",
                                                      "r_subdivisions", "r_allowExtensions"};

    for (const char* setting : restartSettings) {
        if (trap->Cvar_VariableValue(Q_Format("ui_{}", setting).c_str()) != trap->Cvar_VariableValue(setting)) {
            return true;
        }
    }

    return false;
}

void UI_UpdateVideoSetup(void);

/*
===============
UI_DeferMenuScript

Return true if the menu script should be deferred for later
===============
*/
static bool UI_DeferMenuScript(const char** args)
{
    const char* name;

    // Whats the reason for being deferred?
    if (!String_Parse(args, &name)) {
        return false;
    }

    // Handle the custom cases
    // "Apply" on the video page: when no setting needs the renderer to start again, the settings are applied at once
    // and the rest of the script, the question that tells of the restart and of the return to the main menu, is left out
    if (!Q_stricmp(name, "VideoApply")) {
        if (UI_VideoNeedsRestart()) {
            return false;
        }

        UI_UpdateVideoSetup();
        return true;
    }

    if (!Q_stricmp(name, "VideoSetup")) {
        const char* warningMenuName;
        bool deferred;

        // No warning menu specified
        if (!String_Parse(args, &warningMenuName)) {
            return false;
        }

        // Defer if the video options were modified
        deferred = trap->Cvar_VariableValue("ui_r_modified") ? true : false;

        if (deferred) {
            // Open the warning menu
            Menus_OpenByName(warningMenuName);
        }

        return deferred;
    }
    else if (!Q_stricmp(name, "RulesBackout")) {
        bool deferred;

        deferred = trap->Cvar_VariableValue("ui_rules_backout") ? true : false;

        trap->Cvar_Set("ui_rules_backout", "0");

        return deferred;
    }

    return false;
}

/*
=================
UI_UpdateVideoSetup

Copies the temporary user interface version of the video cvars into
their real counterparts.  This is to create a interface which allows
you to discard your changes if you did something you didnt want.
The renderer starts again only when a setting that needs it changed:
the display's are applied at the next frame, the others at once.
=================
*/
void UI_UpdateVideoSetup(void)
{
    const bool restart = UI_VideoNeedsRestart();

    trap->Cvar_Set("r_displayState", UI_Cvar_VariableString("ui_r_displayState"));
    trap->Cvar_Set("r_screen", UI_Cvar_VariableString("ui_r_screen"));
    trap->Cvar_Set("r_windowWidth", UI_Cvar_VariableString("ui_r_windowWidth"));
    trap->Cvar_Set("r_windowHeight", UI_Cvar_VariableString("ui_r_windowHeight"));
    trap->Cvar_Set("r_exclusiveMode", UI_Cvar_VariableString("ui_r_exclusiveMode"));
    trap->Cvar_Set("r_lodbias", UI_Cvar_VariableString("ui_r_lodbias"));
    trap->Cvar_Set("r_picmip", UI_Cvar_VariableString("ui_r_picmip"));
    trap->Cvar_Set("r_texturebits", UI_Cvar_VariableString("ui_r_texturebits"));
    trap->Cvar_Set("r_texturemode", UI_Cvar_VariableString("ui_r_texturemode"));
    trap->Cvar_Set("r_detailtextures", UI_Cvar_VariableString("ui_r_detailtextures"));
    trap->Cvar_Set("r_ext_compress_textures", UI_Cvar_VariableString("ui_r_ext_compress_textures"));
    trap->Cvar_Set("r_subdivisions", UI_Cvar_VariableString("ui_r_subdivisions"));
    trap->Cvar_Set("r_fastSky", UI_Cvar_VariableString("ui_r_fastSky"));
    trap->Cvar_Set("r_inGameVideo", UI_Cvar_VariableString("ui_r_inGameVideo"));
    trap->Cvar_Set("r_allowExtensions", UI_Cvar_VariableString("ui_r_allowExtensions"));
    trap->Cvar_Set("cg_shadows", UI_Cvar_VariableString("ui_cg_shadows"));
    trap->Cvar_Set("ui_r_modified", "0");

    if (restart) {
        trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, "vid_restart;");
    }
}

/*
=================
UI_GetDisplaySetup

The display's settings into the video page's display rows: when the page opens, and when the display changed while
the page has no change of its own to apply (Alt+Enter, the console)
=================
*/
static void UI_GetDisplaySetup(void)
{
    // Make sure the cvars are registered as read only.
    trap->Cvar_Register(nullptr, "ui_r_displayState", "1", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);
    trap->Cvar_Register(nullptr, "ui_r_screen", "0", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);
    trap->Cvar_Register(nullptr, "ui_r_windowWidth", "1280", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);
    trap->Cvar_Register(nullptr, "ui_r_windowHeight", "720", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);
    trap->Cvar_Register(nullptr, "ui_r_exclusiveMode", "", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);
    trap->Cvar_Register(nullptr, "ui_r_refreshChoice", "0", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);

    // the state as the engine reads it, so that a value it takes as borderless fullscreen shows as such
    const platform::DisplayState state = platform::DisplayStateOf(static_cast<int>(trap->Cvar_VariableValue("r_displayState")));
    trap->Cvar_SetValue("ui_r_displayState", platform::DisplayStateSetting(state));
    trap->Cvar_Set("ui_r_screen", UI_Cvar_VariableString("r_screen"));
    trap->Cvar_Set("ui_r_windowWidth", UI_Cvar_VariableString("r_windowWidth"));
    trap->Cvar_Set("ui_r_windowHeight", UI_Cvar_VariableString("r_windowHeight"));
    trap->Cvar_Set("ui_r_exclusiveMode", UI_Cvar_VariableString("r_exclusiveMode"));
    UI_SetRefreshChoice(UI_DisplayChoice());
}

/*
=================
UI_GetVideoSetup

Retrieves the current actual video settings into the temporary user
interface versions of the cvars.
=================
*/
void UI_GetVideoSetup(void)
{
    trap->Cvar_Register(nullptr, "ui_r_glCustom", "4", cvarFlags_t::CVAR_INTERNAL | cvarFlags_t::CVAR_ARCHIVE);

    UI_GetDisplaySetup();

    // Make sure the cvars are registered as read only.
    trap->Cvar_Register(nullptr, "ui_r_lodbias", "0", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);
    trap->Cvar_Register(nullptr, "ui_r_picmip", "0", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);
    trap->Cvar_Register(nullptr, "ui_r_texturebits", "0", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);
    trap->Cvar_Register(nullptr, "ui_r_texturemode", "0", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);
    trap->Cvar_Register(nullptr, "ui_r_detailtextures", "0", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);
    trap->Cvar_Register(nullptr, "ui_r_ext_compress_textures", "0", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);
    trap->Cvar_Register(nullptr, "ui_r_subdivisions", "0", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);
    trap->Cvar_Register(nullptr, "ui_r_fastSky", "0", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);
    trap->Cvar_Register(nullptr, "ui_r_inGameVideo", "0", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);
    trap->Cvar_Register(nullptr, "ui_r_allowExtensions", "0", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);
    trap->Cvar_Register(nullptr, "ui_cg_shadows", "0", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);
    trap->Cvar_Register(nullptr, "ui_r_modified", "0", cvarFlags_t::CVAR_ROM | cvarFlags_t::CVAR_INTERNAL);

    // Copy over the real video cvars into their temporary counterparts
    trap->Cvar_Set("ui_r_lodbias", UI_Cvar_VariableString("r_lodbias"));
    trap->Cvar_Set("ui_r_picmip", UI_Cvar_VariableString("r_picmip"));
    trap->Cvar_Set("ui_r_texturebits", UI_Cvar_VariableString("r_texturebits"));
    trap->Cvar_Set("ui_r_texturemode", UI_Cvar_VariableString("r_texturemode"));
    trap->Cvar_Set("ui_r_detailtextures", UI_Cvar_VariableString("r_detailtextures"));
    trap->Cvar_Set("ui_r_ext_compress_textures", UI_Cvar_VariableString("r_ext_compress_textures"));
    trap->Cvar_Set("ui_r_subdivisions", UI_Cvar_VariableString("r_subdivisions"));
    trap->Cvar_Set("ui_r_fastSky", UI_Cvar_VariableString("r_fastSky"));
    trap->Cvar_Set("ui_r_inGameVideo", UI_Cvar_VariableString("r_inGameVideo"));
    trap->Cvar_Set("ui_r_allowExtensions", UI_Cvar_VariableString("r_allowExtensions"));
    trap->Cvar_Set("ui_cg_shadows", UI_Cvar_VariableString("cg_shadows"));
    trap->Cvar_Set("ui_r_modified", "0");
}

// If the game type is siege, hide the addbot button. I would have done a cvar text on that item,
// but it already had one on it.
static void UI_SetBotButton(void)
{
    const gametype_t gameType = UI_CvarGametype();
    int server;
    menuDef_t* menu;
    itemDef_t* item;
    const char* name = "addBot";

    server = trap->Cvar_VariableValue("sv_running");

    // If in siege or a client, don't show add bot button
    if ((gameType == gametype_t::GT_SIEGE) || (server == 0)) // If it's not siege, don't worry about it
    {
        menu = Menu_GetFocused(); // Get current menu (either video or ingame video, I would assume)

        if (!menu) {
            return;
        }

        item = Menu_FindItemByName(menu, name);
        if (item) {
            Menu_ShowItemByName(menu, name, false);
        }
    }
}

// Update the model cvar and everything is good.
static void UI_UpdateCharacterCvars(void)
{
    char skin[MAX_QPATH];
    char model[MAX_QPATH];
    char head[MAX_QPATH];
    char torso[MAX_QPATH];
    char legs[MAX_QPATH];

    trap->Cvar_VariableStringBuffer("ui_char_model", model, sizeof(model));
    trap->Cvar_VariableStringBuffer("ui_char_skin_head", head, sizeof(head));
    trap->Cvar_VariableStringBuffer("ui_char_skin_torso", torso, sizeof(torso));
    trap->Cvar_VariableStringBuffer("ui_char_skin_legs", legs, sizeof(legs));

    Q_FormatTo(skin, "{}/{}|{}|{}", model, head, torso, legs);

    trap->Cvar_Set("model", skin);

    trap->Cvar_Set("char_color_red", UI_Cvar_VariableString("ui_char_color_red"));
    trap->Cvar_Set("char_color_green", UI_Cvar_VariableString("ui_char_color_green"));
    trap->Cvar_Set("char_color_blue", UI_Cvar_VariableString("ui_char_color_blue"));
    trap->Cvar_Set("ui_selectedModelIndex", "-1");
}

static void UI_GetCharacterCvars(void)
{
    char* model;
    char* skin;
    int i;

    trap->Cvar_Set("ui_char_color_red", UI_Cvar_VariableString("char_color_red"));
    trap->Cvar_Set("ui_char_color_green", UI_Cvar_VariableString("char_color_green"));
    trap->Cvar_Set("ui_char_color_blue", UI_Cvar_VariableString("char_color_blue"));

    model = UI_Cvar_VariableString("model");
    skin = strrchr(model, '/');
    if (skin && strchr(model, '|')) // we have a multipart custom jedi
    {
        char skinhead[MAX_QPATH];
        char skintorso[MAX_QPATH];
        char skinlower[MAX_QPATH];
        char* p2;

        *skin = 0;
        skin++;
        // now get the the individual files

        // advance to second
        p2 = strchr(skin, '|');
        assert(p2);
        *p2 = 0;
        p2++;
        Q_strncpyz(skinhead, skin, sizeof(skinhead));

        // advance to third
        skin = strchr(p2, '|');
        assert(skin);
        *skin = 0;
        skin++;
        Q_strncpyz(skintorso, p2, sizeof(skintorso));

        Q_strncpyz(skinlower, skin, sizeof(skinlower));

        trap->Cvar_Set("ui_char_model", model);
        trap->Cvar_Set("ui_char_skin_head", skinhead);
        trap->Cvar_Set("ui_char_skin_torso", skintorso);
        trap->Cvar_Set("ui_char_skin_legs", skinlower);

        for (i = 0; i < uiInfo.playerSpeciesCount; i++) {
            if (!Q_stricmp(model, uiInfo.playerSpecies[i].Name)) {
                uiInfo.playerSpeciesIndex = i;
                break;
            }
        }
    }
    else {
        model = UI_Cvar_VariableString("ui_char_model");
        for (i = 0; i < uiInfo.playerSpeciesCount; i++) {
            if (!Q_stricmp(model, uiInfo.playerSpecies[i].Name)) {
                uiInfo.playerSpeciesIndex = i;
                return; // FOUND IT, don't fall through
            }
        }
        // nope, didn't find it.
        uiInfo.playerSpeciesIndex = 0; // jic
        trap->Cvar_Set("ui_char_model", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name);
        trap->Cvar_Set("ui_char_skin_head", "head_a1");
        trap->Cvar_Set("ui_char_skin_torso", "torso_a1");
        trap->Cvar_Set("ui_char_skin_legs", "lower_a1");
    }
}

void UI_SetSiegeObjectiveGraphicPos(menuDef_t* menu, const char* itemName, const char* cvarName)
{
    itemDef_t* item;
    char cvarBuf[1024];
    const char* holdVal;
    const char* holdBuf;

    item = Menu_FindItemByName(menu, itemName);

    if (item) {
        // get cvar data
        trap->Cvar_VariableStringBuffer(cvarName, cvarBuf, sizeof(cvarBuf));

        holdBuf = cvarBuf;
        if (String_Parse(&holdBuf, &holdVal)) {
            item->window.rectClient.x = Q_atof(holdVal);
            if (String_Parse(&holdBuf, &holdVal)) {
                item->window.rectClient.y = Q_atof(holdVal);
                if (String_Parse(&holdBuf, &holdVal)) {
                    item->window.rectClient.w = Q_atof(holdVal);
                    if (String_Parse(&holdBuf, &holdVal)) {
                        item->window.rectClient.h = Q_atof(holdVal);

                        item->window.rect.x = item->window.rectClient.x;
                        item->window.rect.y = item->window.rectClient.y;

                        item->window.rect.w = item->window.rectClient.w;
                        item->window.rect.h = item->window.rectClient.h;
                    }
                }
            }
        }
    }
}

void UI_FindCurrentSiegeTeamClass(void)
{
    menuDef_t* menu;
    team_t myTeam = UI_CvarTeam("ui_myteam");
    const char* itemname;
    itemDef_t* item;
    siegePlayerClassFlags_t baseClass;

    menu = Menu_GetFocused(); // Get current menu

    if (!menu) {
        return;
    }

    if ((myTeam != team_t::TEAM_RED) && (myTeam != team_t::TEAM_BLUE)) {
        return;
    }

    // If the player is on a team,
    if (myTeam == team_t::TEAM_RED) {
        itemDef_t* item;
        item = (itemDef_t*)Menu_FindItemByName(menu, "onteam1");
        if (item) {
            Item_RunScript(item, item->action);
        }
    }
    else if (myTeam == team_t::TEAM_BLUE) {
        itemDef_t* item;
        item = (itemDef_t*)Menu_FindItemByName(menu, "onteam2");
        if (item) {
            Item_RunScript(item, item->action);
        }
    }

    baseClass = UI_CvarSiegeClass();

    // Find correct class button and activate it.
    switch (baseClass) {
    case siegePlayerClassFlags_t::SPC_INFANTRY:
        itemname = "class1_button";
        break;
    case siegePlayerClassFlags_t::SPC_HEAVY_WEAPONS:
        itemname = "class2_button";
        break;
    case siegePlayerClassFlags_t::SPC_DEMOLITIONIST:
        itemname = "class3_button";
        break;
    case siegePlayerClassFlags_t::SPC_VANGUARD:
        itemname = "class4_button";
        break;
    case siegePlayerClassFlags_t::SPC_SUPPORT:
        itemname = "class5_button";
        break;
    case siegePlayerClassFlags_t::SPC_JEDI:
        itemname = "class6_button";
        break;
    default:
        return;
    }

    item = (itemDef_t*)Menu_FindItemByName(menu, itemname);
    if (item) {
        Item_RunScript(item, item->action);
    }
}

void UI_UpdateSiegeObjectiveGraphics(void)
{
    menuDef_t* menu;
    int objI;

    menu = Menu_GetFocused(); // Get current menu

    if (!menu) {
        return;
    }

    // Hiding a bunch of fields because the opening section of the siege menu was getting too long
    Menu_ShowGroup(menu, "class_button", false);
    Menu_ShowGroup(menu, "class_count", false);
    Menu_ShowGroup(menu, "feeders", false);
    Menu_ShowGroup(menu, "classdescription", false);
    Menu_ShowGroup(menu, "minidesc", false);
    Menu_ShowGroup(menu, "obj_longdesc", false);
    Menu_ShowGroup(menu, "objective_pic", false);
    Menu_ShowGroup(menu, "stats", false);
    Menu_ShowGroup(menu, "forcepowerlevel", false);

    // Get objective icons for each team
    for (team_t teamI = SIEGETEAM_TEAM1; teamI <= SIEGETEAM_TEAM2; teamI++) {
        for (objI = 1; objI < 8; objI++) {
            Menu_SetItemBackground(menu, Q_Format("tm{}_icon{}", teamI, objI).c_str(), Q_Format("*team{}_objective{}_mapicon", teamI, objI).c_str());
            Menu_SetItemBackground(menu, Q_Format("tm{}_l_icon{}", teamI, objI).c_str(), Q_Format("*team{}_objective{}_mapicon", teamI, objI).c_str());
        }
    }

    // Now get their placement on the map
    for (team_t teamI = SIEGETEAM_TEAM1; teamI <= SIEGETEAM_TEAM2; teamI++) {
        for (objI = 1; objI < 8; objI++) {
            UI_SetSiegeObjectiveGraphicPos(menu, Q_Format("tm{}_icon{}", teamI, objI).c_str(), Q_Format("team{}_objective{}_mappos", teamI, objI).c_str());
        }
    }
}

saber_colors_t TranslateSaberColor(const char* name);

static void UI_UpdateSaberCvars(void)
{
    saber_colors_t colorI;

    trap->Cvar_Set("saber1", UI_Cvar_VariableString("ui_saber"));
    trap->Cvar_Set("saber2", UI_Cvar_VariableString("ui_saber2"));

    colorI = TranslateSaberColor(UI_Cvar_VariableString("ui_saber_color"));
    trap->Cvar_Set("color1", Q_Format("{}", colorI).c_str());
    trap->Cvar_Set("g_saber_color", UI_Cvar_VariableString("ui_saber_color"));

    colorI = TranslateSaberColor(UI_Cvar_VariableString("ui_saber2_color"));
    trap->Cvar_Set("color2", Q_Format("{}", colorI).c_str());
    trap->Cvar_Set("g_saber2_color", UI_Cvar_VariableString("ui_saber2_color"));
}

// More hard coded goodness for the menus.
static void UI_SetSaberBoxesandHilts(void)
{
    menuDef_t* menu;
    itemDef_t* item;
    bool getBig = false;
    char sType[MAX_QPATH];

    menu = Menu_GetFocused(); // Get current menu (either video or ingame video, I would assume)

    if (!menu) {
        return;
    }

    trap->Cvar_VariableStringBuffer("ui_saber_type", sType, sizeof(sType));

    if (Q_stricmp("dual", sType) != 0) {
        //		trap->Cvar_Set("ui_saber", "single_1");
        //		trap->Cvar_Set("ui_saber2", "single_1");
        getBig = true;
    }

    else if (Q_stricmp("staff", sType) != 0) {
        //		trap->Cvar_Set("ui_saber", "dual_1");
        //		trap->Cvar_Set("ui_saber2", "none");
        getBig = true;
    }

    if (!getBig) {
        return;
    }

    item = (itemDef_t*)Menu_FindItemByName(menu, "box2middle");

    if (item) {
        item->window.rect.x = 212;
        item->window.rect.y = 126;
        item->window.rect.w = 219;
        item->window.rect.h = 44;
    }

    item = (itemDef_t*)Menu_FindItemByName(menu, "box2bottom");

    if (item) {
        item->window.rect.x = 212;
        item->window.rect.y = 170;
        item->window.rect.w = 219;
        item->window.rect.h = 60;
    }

    item = (itemDef_t*)Menu_FindItemByName(menu, "box3middle");

    if (item) {
        item->window.rect.x = 418;
        item->window.rect.y = 126;
        item->window.rect.w = 219;
        item->window.rect.h = 44;
    }

    item = (itemDef_t*)Menu_FindItemByName(menu, "box3bottom");

    if (item) {
        item->window.rect.x = 418;
        item->window.rect.y = 170;
        item->window.rect.w = 219;
        item->window.rect.h = 60;
    }
}

extern bool UI_SaberSkinForSaber(const char* saberName, char* saberSkin);
extern bool ItemParse_asset_model_go(itemDef_t* item, const char* name, int* runTimeLength);
extern bool ItemParse_model_g2skin_go(itemDef_t* item, const char* skinName);

static void UI_UpdateSaberType(void)
{
    char sType[MAX_QPATH];
    trap->Cvar_VariableStringBuffer("ui_saber_type", sType, sizeof(sType));

    if (Q_stricmp("single", sType) == 0 || Q_stricmp("staff", sType) == 0) {
        trap->Cvar_Set("ui_saber2", "");
    }
}

static void UI_UpdateSaberHilt(bool secondSaber)
{
    menuDef_t* menu;
    itemDef_t* item;
    char model[MAX_QPATH];
    char modelPath[MAX_QPATH];
    char skinPath[MAX_QPATH];
    const char* itemName;
    const char* saberCvarName;
    int animRunLength;

    menu = Menu_GetFocused(); // Get current menu (either video or ingame video, I would assume)

    if (!menu) {
        return;
    }

    if (secondSaber) {
        itemName = "saber2";
        saberCvarName = "ui_saber2";
    }
    else {
        itemName = "saber";
        saberCvarName = "ui_saber";
    }

    item = (itemDef_t*)Menu_FindItemByName(menu, itemName);

    if (!item) {
        Q_Error(errorParm_t::ERR_FATAL, "UI_UpdateSaberHilt: Could not find item ({}) in menu ({})", itemName, menu->window.name);
    }

    trap->Cvar_VariableStringBuffer(saberCvarName, model, sizeof(model));

    item->text = model;
    // read this from the sabers.cfg
    if (UI_SaberModelForSaber(model, modelPath)) {                 // successfully found a model
        ItemParse_asset_model_go(item, modelPath, &animRunLength); // set the model
        // get the customSkin, if any
        // COM_StripExtension( modelPath, skinPath );
        // COM_DefaultExtension( skinPath, sizeof( skinPath ), ".skin" );
        if (UI_SaberSkinForSaber(model, skinPath)) {
            ItemParse_model_g2skin_go(item, skinPath); // apply the skin
        }
        else {
            ItemParse_model_g2skin_go(item, nullptr); // apply the skin
        }
    }
}

static void UI_UpdateSaberColor(bool secondSaber) {}

const char* SaberColorToString(saber_colors_t color);

static void UI_GetSaberCvars(void)
{
    //	trap->Cvar_Set ( "ui_saber_type", UI_Cvar_VariableString ( "g_saber_type" ) );
    trap->Cvar_Set("ui_saber", UI_Cvar_VariableString("saber1"));
    trap->Cvar_Set("ui_saber2", UI_Cvar_VariableString("saber2"));

    trap->Cvar_Set("g_saber_color", SaberColorToString((saber_colors_t)(trap->Cvar_VariableValue("color1"))));
    trap->Cvar_Set("g_saber2_color", SaberColorToString((saber_colors_t)(trap->Cvar_VariableValue("color2"))));

    trap->Cvar_Set("ui_saber_color", UI_Cvar_VariableString("g_saber_color"));
    trap->Cvar_Set("ui_saber2_color", UI_Cvar_VariableString("g_saber2_color"));
}

extern bool ItemParse_model_g2anim_go(itemDef_t* item, const char* animName);

void UI_UpdateCharacterSkin(void)
{
    menuDef_t* menu;
    itemDef_t* item;
    char skin[MAX_QPATH];
    char model[MAX_QPATH];
    char head[MAX_QPATH];
    char torso[MAX_QPATH];
    char legs[MAX_QPATH];

    menu = Menu_GetFocused(); // Get current menu

    if (!menu) {
        return;
    }

    item = (itemDef_t*)Menu_FindItemByName(menu, "character");

    if (!item) {
        Q_Error(errorParm_t::ERR_FATAL, "UI_UpdateCharacterSkin: Could not find item (character) in menu ({})", menu->window.name);
    }

    trap->Cvar_VariableStringBuffer("ui_char_model", model, sizeof(model));
    trap->Cvar_VariableStringBuffer("ui_char_skin_head", head, sizeof(head));
    trap->Cvar_VariableStringBuffer("ui_char_skin_torso", torso, sizeof(torso));
    trap->Cvar_VariableStringBuffer("ui_char_skin_legs", legs, sizeof(legs));

    Q_FormatTo(skin, "models/players/{}/|{}|{}|{}", model, head, torso, legs);

    ItemParse_model_g2skin_go(item, skin);
}

static void UI_ResetCharacterListBoxes(void)
{

    itemDef_t* item;
    menuDef_t* menu;
    listBoxDef_t* listPtr;

    menu = Menu_GetFocused();

    if (menu) {
        item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "headlistbox");
        if (item) {
            listPtr = item->typeData.listbox;
            if (listPtr) {
                listPtr->cursorPos = 0;
            }
            item->cursorPos = 0;
        }

        item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "torsolistbox");
        if (item) {
            listPtr = item->typeData.listbox;
            if (listPtr) {
                listPtr->cursorPos = 0;
            }
            item->cursorPos = 0;
        }

        item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "lowerlistbox");
        if (item) {
            listPtr = item->typeData.listbox;
            if (listPtr) {
                listPtr->cursorPos = 0;
            }
            item->cursorPos = 0;
        }

        item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "colorbox");
        if (item) {
            listPtr = item->typeData.listbox;
            if (listPtr) {
                listPtr->cursorPos = 0;
            }
            item->cursorPos = 0;
        }
    }
}

const char* saberSingleHiltInfo[MAX_SABER_HILTS];
const char* saberStaffHiltInfo[MAX_SABER_HILTS];

bool UI_SaberProperNameForSaber(const char* saberName, char* saberProperName);
void WP_SaberGetHiltInfo(const char* singleHilts[MAX_SABER_HILTS], const char* staffHilts[MAX_SABER_HILTS]);

static void UI_UpdateCharacter(bool changedModel)
{
    menuDef_t* menu;
    itemDef_t* item;
    char modelPath[MAX_QPATH];
    int animRunLength;

    menu = Menu_GetFocused(); // Get current menu

    if (!menu) {
        return;
    }

    item = (itemDef_t*)Menu_FindItemByName(menu, "character");

    if (!item) {
        Q_Error(errorParm_t::ERR_FATAL, "UI_UpdateCharacter: Could not find item (character) in menu ({})", menu->window.name);
    }

    ItemParse_model_g2anim_go(item, ui_char_anim.string);

    Q_FormatTo(modelPath, "models/players/{}/model.glm", UI_Cvar_VariableString("ui_char_model"));
    ItemParse_asset_model_go(item, modelPath, &animRunLength);

    if (changedModel) { // set all skins to first skin since we don't know you always have all skins
        // FIXME: could try to keep the same spot in each list as you swtich models
        UI_FeederSelection(FEEDER_PLAYER_SKIN_HEAD, 0, item); // fixme, this is not really the right item!!
        UI_FeederSelection(FEEDER_PLAYER_SKIN_TORSO, 0, item);
        UI_FeederSelection(FEEDER_PLAYER_SKIN_LEGS, 0, item);
        UI_FeederSelection(FEEDER_COLORCHOICES, 0, item);
    }
    UI_UpdateCharacterSkin();
}

/*
==================
UI_CheckServerName
==================
*/
static void UI_CheckServerName(void)
{
    bool changed = false;

    char hostname[MAX_HOSTNAMELENGTH] = {0};
    char* c = hostname;

    trap->Cvar_VariableStringBuffer("sv_hostname", hostname, sizeof(hostname));

    while (*c) {
        if ((*c == '\\') || (*c == ';') || (*c == '"')) {
            *c = '.';
            changed = true;
        }
        c++;
    }
    if (changed) {
        trap->Cvar_Set("sv_hostname", hostname);
    }
}

/*
==================
UI_CheckPassword
==================
*/
static bool UI_CheckPassword(void)
{
    static char info[MAX_STRING_CHARS];

    int index = uiInfo.serverStatus.currentServer;
    if ((index < 0) || (index >= uiInfo.serverStatus.numDisplayServers)) { // warning?
        return false;
    }

    trap->LAN_GetServerInfo(UI_SourceForLAN(), uiInfo.serverStatus.displayServers[index], info, MAX_STRING_CHARS);

    if (atoi(Info_ValueForKey(info, "needpass"))) {
        Menus_OpenByName("password_request");
        return false;
    }

    // This isn't going to make it (too late in dev), like James said I should check to see when we receive
    // a packet *if* we do indeed get a 0 ping just make it 1 so then a 0 ping is guaranteed to be bad
    /*
    // also check ping!
    ping = atoi(Info_ValueForKey(info, "ping"));
    // NOTE : PING -- it's very questionable as to whether a ping of < 0 or <= 0 indicates a bad server
    // what I do know, is that getting "ping" from the ServerInfo on a bad server returns 0.
    // So I'm left with no choice but to not allow you to enter a server with a ping of 0
    if( ping <= 0 )
    {
        Menus_OpenByName("bad_server");
        return false;
    }
    */

    return true;
}

/*
==================
UI_JoinServer
==================
*/
static void UI_JoinServer(void)
{
    char buff[1024] = {0};

    //	trap->Cvar_Set("cg_thirdPerson", "0");
    trap->Cvar_Set("cg_cameraOrbit", "0");
    trap->Cvar_Set("ui_singlePlayerActive", "0");
    if (uiInfo.serverStatus.currentServer >= 0 && uiInfo.serverStatus.currentServer < uiInfo.serverStatus.numDisplayServers) {
        trap->LAN_GetServerAddressString(UI_SourceForLAN() /*ui_netSource.integer*/, uiInfo.serverStatus.displayServers[uiInfo.serverStatus.currentServer],
                                         buff, sizeof(buff));
        trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, Q_Format("connect {}\n", buff).c_str());
    }
}

int UI_SiegeClassNum(siegeClass_t* scl)
{
    int i = 0;
    for (i = 0; i < bgNumSiegeClasses; i++) {
        if (&bgSiegeClasses[i] == scl)
            return i;
    }

    return 0;
}

// called every time a class is selected from a feeder, sets info for shaders to be displayed in the menu about the class -rww
void UI_SiegeSetCvarsForClass(siegeClass_t* scl)
{
    int i = 0;
    int count = 0;
    char shader[MAX_QPATH];

    // let's clear the things out first
    while (i < std::to_underlying(weapon_t::WP_NUM_WEAPONS)) {
        trap->Cvar_Set(Q_Format("ui_class_weapon{}", i).c_str(), "gfx/2d/select");
        i++;
    }
    // now for inventory items
    for (holdable_t h = holdable_t::HI_NONE; h < holdable_t::HI_NUM_HOLDABLE; h++) {
        trap->Cvar_Set(Q_Format("ui_class_item{}", h).c_str(), "gfx/2d/select");
    }
    // now for force powers
    i = 0;
    while (i < std::to_underlying(forcePowers_t::NUM_FORCE_POWERS)) {
        trap->Cvar_Set(Q_Format("ui_class_power{}", i).c_str(), "gfx/2d/select");
        i++;
    }

    // now health and armor
    trap->Cvar_Set("ui_class_health", "0");
    trap->Cvar_Set("ui_class_armor", "0");

    trap->Cvar_Set("ui_class_icon", "");

    if (!scl) { // no select?
        return;
    }

    // set cvars for which weaps we have
    weapon_t wp = weapon_t::WP_NONE;
    trap->Cvar_Set(Q_Format("ui_class_weapondesc{}", count).c_str(), " "); // Blank it out to start with
    while (wp < weapon_t::WP_NUM_WEAPONS) {

        if (scl->weapons & Bit(wp)) {
            if (wp == weapon_t::WP_SABER) { // we want to see what kind of saber they have, and set the cvar based on that
                char saberType[1024];

                if (scl->saber1[0] && scl->saber2[0]) {
                    Q_strncpyz(saberType, "gfx/hud/w_icon_duallightsaber", sizeof(saberType));
                } // fixme: need saber data access on ui to determine if staff, "gfx/hud/w_icon_saberstaff"
                else {
                    char buf[1024];
                    if (scl->saber1[0] && UI_SaberTypeForSaber(scl->saber1, buf)) {
                        if (!Q_stricmp(buf, "SABER_STAFF")) {
                            Q_strncpyz(saberType, "gfx/hud/w_icon_saberstaff", sizeof(saberType));
                        }
                        else {
                            Q_strncpyz(saberType, "gfx/hud/w_icon_lightsaber", sizeof(saberType));
                        }
                    }
                    else {
                        Q_strncpyz(saberType, "gfx/hud/w_icon_lightsaber", sizeof(saberType));
                    }
                }

                trap->Cvar_Set(Q_Format("ui_class_weapon{}", count).c_str(), saberType);
                trap->Cvar_Set(Q_Format("ui_class_weapondesc{}", count).c_str(), "@MENUS_AN_ELEGANT_WEAPON_FOR");
                count++;
                trap->Cvar_Set(Q_Format("ui_class_weapondesc{}", count).c_str(), " "); // Blank it out to start with
            }
            else {
                gitem_t* item = BG_FindItemForWeapon(wp);
                trap->Cvar_Set(Q_Format("ui_class_weapon{}", count).c_str(), item->icon);
                trap->Cvar_Set(Q_Format("ui_class_weapondesc{}", count).c_str(), item->description);
                count++;
                trap->Cvar_Set(Q_Format("ui_class_weapondesc{}", count).c_str(), " "); // Blank it out to start with
            }
        }

        wp++;
    }

    // now for inventory items
    count = 0;

    for (holdable_t h = holdable_t::HI_NONE; h < holdable_t::HI_NUM_HOLDABLE; h++) {
        if (scl->invenItems & Bit(h)) {
            gitem_t* item = BG_FindItemForHoldable(h);
            trap->Cvar_Set(Q_Format("ui_class_item{}", count).c_str(), item->icon);
            trap->Cvar_Set(Q_Format("ui_class_itemdesc{}", count).c_str(), item->description);
            count++;
        }
        else {
            trap->Cvar_Set(Q_Format("ui_class_itemdesc{}", count).c_str(), " ");
        }
    }

    // now for force powers
    forcePowers_t fp = forcePowers_t::FP_FIRST;
    count = 0;

    while (fp < forcePowers_t::NUM_FORCE_POWERS) {
        trap->Cvar_Set(Q_Format("ui_class_powerlevel{}", fp).c_str(), "0"); // Zero this out to start.
        if (std::to_underlying(fp) < 9) {
            trap->Cvar_Set(Q_Format("ui_class_powerlevelslot{}", fp).c_str(), "0"); // Zero this out to start.
        }

        if (scl->forcePowerLevels[fp] != forcePowerLevels_t::FORCE_LEVEL_0) {
            trap->Cvar_Set(Q_Format("ui_class_powerlevel{}", count).c_str(), Q_Format("{}", scl->forcePowerLevels[fp]).c_str());
            trap->Cvar_Set(Q_Format("ui_class_power{}", count).c_str(), HolocronIcons[fp]);
            count++;
        }

        fp++;
    }

    // now health and armor
    trap->Cvar_Set("ui_class_health", Q_Format("{}", scl->maxhealth).c_str());
    trap->Cvar_Set("ui_class_armor", Q_Format("{}", scl->maxarmor).c_str());
    trap->Cvar_Set("ui_class_speed", Q_Format("{:3.2f}", scl->speed).c_str());

    // now get the icon path based on the shader index
    if (scl->classShader) {
        trap->R_ShaderNameFromIndex(shader, scl->classShader);
    }
    else { // no shader
        shader[0] = 0;
    }
    trap->Cvar_Set("ui_class_icon", shader);
}

static int g_siegedFeederForcedSet = 0;
void UI_UpdateCvarsForClass(const team_t team, const siegePlayerClassFlags_t baseClass, const int index)
{
    siegeClass_t* holdClass = nullptr;
    const char* holdBuf;

    // Is it a valid team
    if ((team == SIEGETEAM_TEAM1) || (team == SIEGETEAM_TEAM2)) {

        // Is it a valid base class?
        if ((baseClass >= siegePlayerClassFlags_t::SPC_INFANTRY) && (baseClass < siegePlayerClassFlags_t::SPC_MAX)) {
            // A valid index?
            if ((index >= 0) && (index < BG_SiegeCountBaseClass(team, baseClass))) {
                if (!g_siegedFeederForcedSet) {
                    holdClass = BG_GetClassOnBaseClass(team, baseClass, index);
                    if (holdClass) // clicked a valid item
                    {
                        g_UIGloballySelectedSiegeClass = UI_SiegeClassNum(holdClass);
                        trap->Cvar_Set("ui_classDesc", g_UIClassDescriptions[g_UIGloballySelectedSiegeClass].desc);
                        g_siegedFeederForcedSet = 1;
                        Menu_SetFeederSelection(nullptr, FEEDER_SIEGE_BASE_CLASS, -1, nullptr);
                        UI_SiegeSetCvarsForClass(holdClass);

                        holdBuf = BG_GetUIPortraitFile(team, baseClass, index);
                        if (holdBuf) {
                            trap->Cvar_Set("ui_classPortrait", holdBuf);
                        }
                    }
                }
                g_siegedFeederForcedSet = 0;
            }
            else {
                trap->Cvar_Set("ui_classDesc", " ");
            }
        }
    }
}

void UI_ClampMaxPlayers(void)
{
    // duel requires 2 players
    if (uiInfo.gameTypes[ui_netGametype.integer].gtEnum == gametype_t::GT_DUEL) {
        if ((int)trap->Cvar_VariableValue("sv_maxClients") < 2)
            trap->Cvar_Set("sv_maxClients", "2");
    }

    // power duel requires 3 players
    else if (uiInfo.gameTypes[ui_netGametype.integer].gtEnum == gametype_t::GT_POWERDUEL) {
        if ((int)trap->Cvar_VariableValue("sv_maxClients") < 3)
            trap->Cvar_Set("sv_maxClients", "3");
    }

    // can never exceed MAX_CLIENTS
    if ((int)trap->Cvar_VariableValue("sv_maxClients") > MAX_CLIENTS) {
        trap->Cvar_Set("sv_maxClients", XSTRING(MAX_CLIENTS));
    }
}

void UI_UpdateSiegeStatusIcons(void)
{
    menuDef_t* menu = Menu_GetFocused();

    if (menu) {
        int i = 0;
        for (i = 0; i < 7; i++)
            Menu_SetItemBackground(menu, Q_Format("wpnicon0{}", i).c_str(), Q_Format("*ui_class_weapon{}", i).c_str());
        for (i = 0; i < 7; i++)
            Menu_SetItemBackground(menu, Q_Format("itemicon0{}", i).c_str(), Q_Format("*ui_class_item{}", i).c_str());
        for (i = 0; i < 10; i++)
            Menu_SetItemBackground(menu, Q_Format("forceicon0{}", i).c_str(), Q_Format("*ui_class_power{}", i).c_str());
        for (i = 10; i < 15; i++)
            Menu_SetItemBackground(menu, Q_Format("forceicon{}", i).c_str(), Q_Format("*ui_class_power{}", i).c_str());
    }
}

// a menu's script command of the UI's own; true: the script goes on
static bool UI_RunMenuScript(const char** args)
{
    const char *name, *name2;
    char buff[1024];

    if (String_Parse(args, &name)) {
        if (Q_stricmp(name, "StartServer") == 0) {
            int i, added = 0;
            float skill;
            int warmupTime = 0;
            int doWarmup = 0;

            //	trap->Cvar_Set("cg_thirdPerson", "0");
            trap->Cvar_Set("cg_cameraOrbit", "0");
            // for Solo games I set this to 1 in the menu and don't want it stomped here,
            // this cvar seems to be reset to 0 in all the proper places so... -dmv
            //	trap->Cvar_Set("ui_singlePlayerActive", "0");

            // if a solo game is started, automatically turn dedicated off here (don't want to do it in the menu, might get annoying)
            if (trap->Cvar_VariableValue("ui_singlePlayerActive")) {
                trap->Cvar_Set("dedicated", "0");
            }
            else {
                trap->Cvar_SetValue("dedicated", Com_Clamp(0, 2, ui_dedicated.integer));
            }
            trap->Cvar_SetValue("g_gametype", Com_Clamp(0, std::to_underlying(gametype_t::GT_MAX_GAME_TYPE),
                                                        std::to_underlying(uiInfo.gameTypes[ui_netGametype.integer].gtEnum)));
            // trap->Cvar_Set("g_redTeam", UI_Cvar_VariableString("ui_teamName"));
            // trap->Cvar_Set("g_blueTeam", UI_Cvar_VariableString("ui_opponentName"));
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, Q_Format("wait ; wait ; map {}\n", uiInfo.mapList[ui_currentNetMap.integer].mapLoadName).c_str());
            skill = trap->Cvar_VariableValue("g_spSkill");

            // Cap the warmup values in case the user tries a dumb setting.
            warmupTime = trap->Cvar_VariableValue("g_warmup");
            doWarmup = trap->Cvar_VariableValue("g_doWarmup");

            if (doWarmup && warmupTime < 1) {
                trap->Cvar_Set("g_doWarmup", "0");
            }
            if (warmupTime < 5) {
                trap->Cvar_Set("g_warmup", "5");
            }
            if (warmupTime > 120) {
                trap->Cvar_Set("g_warmup", "120");
            }

            if (UI_CvarGametype() == gametype_t::GT_DUEL || UI_CvarGametype() == gametype_t::GT_POWERDUEL) { // always set fraglimit 1 when starting a duel game
                trap->Cvar_Set("fraglimit", "1");
                trap->Cvar_Set("timelimit", "0");
            }

            for (i = 0; i < PLAYERS_PER_TEAM; i++) {
                int bot = trap->Cvar_VariableValue(Q_Format("ui_blueteam{}", i + 1).c_str());
                int maxcl = trap->Cvar_VariableValue("sv_maxClients");

                if (bot > 1) {
                    int numval = i + 1;

                    numval *= 2;

                    numval -= 1;

                    if (numval <= maxcl) {
                        if (ui_actualNetGametype.integer >= std::to_underlying(gametype_t::GT_TEAM)) {
                            Q_FormatTo(buff, "addbot \"{}\" {:f} {}\n", UI_GetBotNameByNumber(bot - 2), skill, "Blue");
                        }
                        else {
                            Q_FormatTo(buff, "addbot \"{}\" {:f} \n", UI_GetBotNameByNumber(bot - 2), skill);
                        }
                        trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, buff);
                        added++;
                    }
                }
                bot = trap->Cvar_VariableValue(Q_Format("ui_redteam{}", i + 1).c_str());
                if (bot > 1) {
                    int numval = i + 1;

                    numval *= 2;

                    if (numval <= maxcl) {
                        if (ui_actualNetGametype.integer >= std::to_underlying(gametype_t::GT_TEAM)) {
                            Q_FormatTo(buff, "addbot \"{}\" {:f} {}\n", UI_GetBotNameByNumber(bot - 2), skill, "Red");
                        }
                        else {
                            Q_FormatTo(buff, "addbot \"{}\" {:f} \n", UI_GetBotNameByNumber(bot - 2), skill);
                        }
                        trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, buff);
                        added++;
                    }
                }
                if (added >= maxcl) { // this means the client filled up all their slots in the UI with bots. So stretch out an extra slot for them, and then
                                      // stop adding bots.
                    trap->Cvar_Set("sv_maxClients", Q_Format("{}", added + 1).c_str());
                    break;
                }
            }
        }
        else if (Q_stricmp(name, "updateSPMenu") == 0) {
            UI_SetCapFragLimits(true);
            UI_MapCountByGameType(true);
            trap->Cvar_SetValue("ui_mapIndex", UI_GetIndexFromSelection(ui_currentMap.integer));
            trap->Cvar_Update(&ui_mapIndex);
            Menu_SetFeederSelection(nullptr, FEEDER_MAPS, ui_mapIndex.integer, "skirmish");
            UI_GameType_HandleKey(0, nullptr, fakeAscii_t::A_MOUSE1, false);
            UI_GameType_HandleKey(0, nullptr, fakeAscii_t::A_MOUSE2, false);
        }
        else if (Q_stricmp(name, "resetDefaults") == 0) {
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, "cvar_restart\n");
            Controls_SetDefaults();
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, "exec mpdefault.cfg\n");
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, "vid_restart\n");
        }
        else if (Q_stricmp(name, "loadArenas") == 0) {
            UI_LoadArenas();
            UI_MapCountByGameType(false);
            Menu_SetFeederSelection(nullptr, FEEDER_ALLMAPS, gUISelectedMap, "createserver");
            uiForceRank = static_cast<forceMasteries_t>((int)trap->Cvar_VariableValue("g_maxForceRank"));
        }
        else if (Q_stricmp(name, "saveControls") == 0) {
            Controls_SetConfig();
        }
        else if (Q_stricmp(name, "loadControls") == 0) {
            Controls_GetConfig();
        }
        else if (Q_stricmp(name, "clearError") == 0) {
            trap->Cvar_Set("com_errorMessage", "");
        }
        else if (Q_stricmp(name, "loadGameInfo") == 0) {
            UI_ParseGameInfo("ui/jamp/gameinfo.txt");
        }
        else if (Q_stricmp(name, "RefreshServers") == 0) {
            UI_StartServerRefresh(true);
            UI_BuildServerDisplayList(true);
        }
        else if (Q_stricmp(name, "RefreshFilter") == 0) {
            UI_StartServerRefresh(false);
            UI_BuildServerDisplayList(true);
        }
        else if (Q_stricmp(name, "LoadDemos") == 0) {
            UI_LoadDemos();
        }
        else if (Q_stricmp(name, "LoadMovies") == 0) {
            UI_LoadMovies();
        }
        else if (Q_stricmp(name, "LoadMods") == 0) {
            UI_LoadMods();
        }
        else if (Q_stricmp(name, "playMovie") == 0) {
            if (uiInfo.previewMovie >= 0) {
                trap->CIN_StopCinematic(uiInfo.previewMovie);
            }
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, Q_Format("cinematic {}.roq 2\n", uiInfo.movieList[uiInfo.movieIndex]).c_str());
        }
        else if (Q_stricmp(name, "RunMod") == 0) {
            trap->Cvar_Set("fs_game", uiInfo.modList[uiInfo.modIndex].modName);
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, "vid_restart;");
        }
        else if (Q_stricmp(name, "RunDemo") == 0) {
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, Q_Format("demo \"{}\"\n", uiInfo.demoList[uiInfo.demoIndex]).c_str());
        }
        else if (Q_stricmp(name, "Quake3") == 0) {
            trap->Cvar_Set("fs_game", "");
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, "vid_restart;");
        }
        else if (Q_stricmp(name, "closeJoin") == 0) {
            if (uiInfo.serverStatus.refreshActive) {
                UI_StopServerRefresh();
                uiInfo.serverStatus.nextDisplayRefresh = 0;
                uiInfo.nextServerStatusRefresh = 0;
                uiInfo.nextFindPlayerRefresh = 0;
                UI_BuildServerDisplayList(true);
            }
            else {
                Menus_CloseByName("joinserver");
                Menus_OpenByName("main");
            }
        }
        else if (Q_stricmp(name, "StopRefresh") == 0) {
            UI_StopServerRefresh();
            uiInfo.serverStatus.nextDisplayRefresh = 0;
            uiInfo.nextServerStatusRefresh = 0;
            uiInfo.nextFindPlayerRefresh = 0;
        }
        else if (Q_stricmp(name, "UpdateFilter") == 0) {
            trap->Cvar_Update(&ui_netSource);
            if (UI_NetSource() == uiNetSource_t::UIAS_LOCAL || !uiInfo.serverStatus.numDisplayServers) {
                UI_StartServerRefresh(true);
            }
            UI_BuildServerDisplayList(true);
            UI_FeederSelection(FEEDER_SERVERS, 0, nullptr);

            UI_LoadMods();
        }
        else if (Q_stricmp(name, "ServerStatus") == 0) {
            trap->LAN_GetServerAddressString(UI_SourceForLAN(), uiInfo.serverStatus.displayServers[uiInfo.serverStatus.currentServer],
                                             uiInfo.serverStatusAddress, sizeof(uiInfo.serverStatusAddress));
            UI_BuildServerStatus(true);
        }
        else if (Q_stricmp(name, "FoundPlayerServerStatus") == 0) {
            Q_strncpyz(uiInfo.serverStatusAddress, uiInfo.foundPlayerServerAddresses[uiInfo.currentFoundPlayerServer], sizeof(uiInfo.serverStatusAddress));
            UI_BuildServerStatus(true);
            Menu_SetFeederSelection(nullptr, FEEDER_FINDPLAYER, 0, nullptr);
        }
        else if (Q_stricmp(name, "FindPlayer") == 0) {
            UI_BuildFindPlayerList(true);
            // clear the displayed server status info
            uiInfo.serverStatusInfo.numLines = 0;
            Menu_SetFeederSelection(nullptr, FEEDER_FINDPLAYER, 0, nullptr);
        }
        else if (Q_stricmp(name, "checkservername") == 0) {
            UI_CheckServerName();
        }
        else if (Q_stricmp(name, "checkpassword") == 0) {
            if (UI_CheckPassword()) {
                UI_JoinServer();
            }
        }
        else if (Q_stricmp(name, "JoinServer") == 0) {
            UI_JoinServer();
        }
        else if (Q_stricmp(name, "FoundPlayerJoinServer") == 0) {
            trap->Cvar_Set("ui_singlePlayerActive", "0");
            if (uiInfo.currentFoundPlayerServer >= 0 && uiInfo.currentFoundPlayerServer < uiInfo.numFoundPlayerServers) {
                trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND,
                                      Q_Format("connect {}\n", uiInfo.foundPlayerServerAddresses[uiInfo.currentFoundPlayerServer]).c_str());
            }
        }
        else if (Q_stricmp(name, "Quit") == 0) {
            trap->Cvar_Set("ui_singlePlayerActive", "0");
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_NOW, "quit");
        }
        else if (Q_stricmp(name, "Controls") == 0) {
            trap->Cvar_Set("cl_paused", "1");
            trap->Key_SetCatcher(keyCatchFlags_t::KEYCATCH_UI);
            Menus_CloseAll();
            Menus_ActivateByName("setup_menu2");
        }
        else if (Q_stricmp(name, "Leave") == 0) {
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, "disconnect\n");
            trap->Key_SetCatcher(keyCatchFlags_t::KEYCATCH_UI);
            Menus_CloseAll();
            Menus_ActivateByName("main");
        }
        else if (Q_stricmp(name, "getvideosetup") == 0) {
            UI_GetVideoSetup();
        }
        else if (Q_stricmp(name, "getsaberhiltinfo") == 0) {
            WP_SaberGetHiltInfo(saberSingleHiltInfo, saberStaffHiltInfo);
        }
        // On the solo game creation screen, we can't see siege maps
        else if (Q_stricmp(name, "checkforsiege") == 0) {
            if (uiInfo.gameTypes[ui_netGametype.integer].gtEnum == gametype_t::GT_SIEGE) {
                // fake out the handler to advance to the next game type
                UI_NetGameType_HandleKey(0, nullptr, fakeAscii_t::A_MOUSE1);
            }
        }
        else if (Q_stricmp(name, "updatevideosetup") == 0) {
            UI_UpdateVideoSetup();
        }
        else if (Q_stricmp(name, "ServerSort") == 0) {
            int sortColumn;
            if (Int_Parse(args, &sortColumn)) {
                const serverSortKey_t sortKey = static_cast<serverSortKey_t>(sortColumn); // the menu script's number
                // if same column we're already sorting on then flip the direction
                if (sortKey == uiInfo.serverStatus.sortKey) {
                    uiInfo.serverStatus.sortDir = !uiInfo.serverStatus.sortDir;
                }
                // make sure we sort again
                UI_ServersSort(sortKey, true);
            }
        }
        else if (Q_stricmp(name, "nextSkirmish") == 0) {
            UI_StartSkirmish(true);
        }
        else if (Q_stricmp(name, "SkirmishStart") == 0) {
            UI_StartSkirmish(false);
        }
        else if (Q_stricmp(name, "closeingame") == 0) {
            trap->Key_SetCatcher(trap->Key_GetCatcher() & ~keyCatchFlags_t::KEYCATCH_UI);
            trap->Key_ClearStates();
            trap->Cvar_Set("cl_paused", "0");
            Menus_CloseAll();
        }
        else if (Q_stricmp(name, "voteMap") == 0) {
            if (ui_currentNetMap.integer >= 0 && ui_currentNetMap.integer < uiInfo.mapCount) {
                trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, Q_Format("callvote map {}\n", uiInfo.mapList[ui_currentNetMap.integer].mapLoadName).c_str());
            }
        }
        else if (Q_stricmp(name, "voteKick") == 0) {
            if (uiInfo.playerIndex >= 0 && uiInfo.playerIndex < uiInfo.playerCount) {
                // trap->Cmd_ExecuteText( cbufExec_t::EXEC_APPEND, Q_Format("callvote kick \"{}\"\n", uiInfo.playerNames[uiInfo.playerIndex]).c_str() );
                trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, Q_Format("callvote clientkick \"{}\"\n", uiInfo.playerIndexes[uiInfo.playerIndex]).c_str());
            }
        }
        else if (Q_stricmp(name, "voteGame") == 0) {
            if (ui_netGametype.integer >= 0 && ui_netGametype.integer < uiInfo.numGameTypes) {
                trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, Q_Format("callvote g_gametype {}\n", uiInfo.gameTypes[ui_netGametype.integer].gtEnum).c_str());
            }
        }
        else if (Q_stricmp(name, "voteLeader") == 0) {
            if (uiInfo.teamIndex >= 0 && uiInfo.teamIndex < uiInfo.myTeamCount) {
                trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, Q_Format("callteamvote leader \"{}\"\n", uiInfo.teamNames[uiInfo.teamIndex]).c_str());
            }
        }
        else if (Q_stricmp(name, "addBot") == 0) {
            if (UI_CvarGametype() >= gametype_t::GT_TEAM) {
                trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, Q_Format("addbot \"{}\" {} {}\n", UI_GetBotNameByNumber(uiInfo.botIndex), uiInfo.skillIndex + 1,
                                                                        (uiInfo.redBlue == 0) ? "Red" : "Blue")
                                                                   .c_str());
            }
            else {
                trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, Q_Format("addbot \"{}\" {} {}\n", UI_GetBotNameByNumber(uiInfo.botIndex), uiInfo.skillIndex + 1,
                                                                        (uiInfo.redBlue == 0) ? "Red" : "Blue")
                                                                   .c_str());
            }
        }
        else if (Q_stricmp(name, "addFavorite") == 0) {
            if (UI_NetSource() != uiNetSource_t::UIAS_FAVORITES) {
                char name[MAX_HOSTNAMELENGTH] = {0};
                char addr[MAX_ADDRESSLENGTH] = {0};
                int res;

                trap->LAN_GetServerInfo(UI_SourceForLAN(), uiInfo.serverStatus.displayServers[uiInfo.serverStatus.currentServer], buff, MAX_STRING_CHARS);
                name[0] = addr[0] = '\0';
                Q_strncpyz(name, Info_ValueForKey(buff, "hostname"), sizeof(name));
                Q_strncpyz(addr, Info_ValueForKey(buff, "addr"), sizeof(addr));
                if (strlen(name) > 0 && strlen(addr) > 0) {
                    res = trap->LAN_AddServer(lanSource_t::AS_FAVORITES, name, addr);
                    if (res == 0) {
                        // server already in the list
                        Q_Println("Favorite already in list");
                    }
                    else if (res == -1) {
                        // list full
                        Q_Println("Favorite list full");
                    }
                    else {
                        // successfully added
                        Q_Println("Added favorite server {}", addr);
                    }
                }
            }
        }
        else if (Q_stricmp(name, "deleteFavorite") == 0) {
            if (UI_NetSource() == uiNetSource_t::UIAS_FAVORITES) {
                char addr[MAX_ADDRESSLENGTH] = {0};
                trap->LAN_GetServerInfo(lanSource_t::AS_FAVORITES, uiInfo.serverStatus.displayServers[uiInfo.serverStatus.currentServer], buff,
                                        MAX_STRING_CHARS);
                addr[0] = '\0';
                Q_strncpyz(addr, Info_ValueForKey(buff, "addr"), sizeof(addr));
                if (strlen(addr) > 0) {
                    trap->LAN_RemoveServer(lanSource_t::AS_FAVORITES, addr);
                }
            }
        }
        else if (Q_stricmp(name, "createFavorite") == 0) {
            //	if (UI_NetSource() == uiNetSource_t::UIAS_FAVORITES)
            // rww - don't know why this check was here.. why would you want to only add new favorites when the filter was favorites?
            {
                char name[MAX_HOSTNAMELENGTH] = {0};
                char addr[MAX_ADDRESSLENGTH] = {0};
                int res;

                name[0] = addr[0] = '\0';
                Q_strncpyz(name, UI_Cvar_VariableString("ui_favoriteName"), sizeof(name));
                Q_strncpyz(addr, UI_Cvar_VariableString("ui_favoriteAddress"), sizeof(addr));
                if (/*strlen(name) > 0 &&*/ strlen(addr) > 0) {
                    res = trap->LAN_AddServer(lanSource_t::AS_FAVORITES, name, addr);
                    if (res == 0) {
                        // server already in the list
                        Q_Println("Favorite already in list");
                    }
                    else if (res == -1) {
                        // list full
                        Q_Println("Favorite list full");
                    }
                    else {
                        // successfully added
                        Q_Println("Added favorite server {}", addr);
                    }
                }
            }
        }
        else if (Q_stricmp(name, "orders") == 0) {
            const char* orders;
            if (String_Parse(args, &orders)) {
                int selectedPlayer = trap->Cvar_VariableValue("cg_selectedPlayer");
                if (selectedPlayer < uiInfo.myTeamCount) {
                    Q_FormatFromPrintf(buff, orders, "a team order");
                    trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, Q_Format(fmt::runtime(buff), uiInfo.teamClientNums[selectedPlayer]).c_str());
                    trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, "\n");
                }
                else {
                    int i;
                    for (i = 0; i < uiInfo.myTeamCount; i++) {
                        if (uiInfo.playerNumber == uiInfo.teamClientNums[i]) {
                            continue;
                        }
                        char order[sizeof(buff)];
                        Q_FormatFromPrintf(order, orders, "a team order");
                        Q_FormatTo(buff, fmt::runtime(order), uiInfo.teamClientNums[i]);
                        trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, buff);
                        trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, "\n");
                    }
                }
                trap->Key_SetCatcher(trap->Key_GetCatcher() & ~keyCatchFlags_t::KEYCATCH_UI);
                trap->Key_ClearStates();
                trap->Cvar_Set("cl_paused", "0");
                Menus_CloseAll();
            }
        }
        else if (Q_stricmp(name, "voiceOrdersTeam") == 0) {
            const char* orders;
            if (String_Parse(args, &orders)) {
                int selectedPlayer = trap->Cvar_VariableValue("cg_selectedPlayer");
                if (selectedPlayer == uiInfo.myTeamCount) {
                    trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, orders);
                    trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, "\n");
                }
                trap->Key_SetCatcher(trap->Key_GetCatcher() & ~keyCatchFlags_t::KEYCATCH_UI);
                trap->Key_ClearStates();
                trap->Cvar_Set("cl_paused", "0");
                Menus_CloseAll();
            }
        }
        else if (Q_stricmp(name, "voiceOrders") == 0) {
            const char* orders;
            if (String_Parse(args, &orders)) {
                int selectedPlayer = trap->Cvar_VariableValue("cg_selectedPlayer");

                if (selectedPlayer == uiInfo.myTeamCount) {
                    selectedPlayer = -1;
                    Q_FormatFromPrintf(buff, orders, "a team order");
                    trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, Q_Format(fmt::runtime(buff), selectedPlayer).c_str());
                }
                else {
                    Q_FormatFromPrintf(buff, orders, "a team order");
                    trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, Q_Format(fmt::runtime(buff), uiInfo.teamClientNums[selectedPlayer]).c_str());
                }
                trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, "\n");

                trap->Key_SetCatcher(trap->Key_GetCatcher() & ~keyCatchFlags_t::KEYCATCH_UI);
                trap->Key_ClearStates();
                trap->Cvar_Set("cl_paused", "0");
                Menus_CloseAll();
            }
        }
        else if (Q_stricmp(name, "setForce") == 0) {
            const char* teamArg;

            if (String_Parse(args, &teamArg)) {
                if (Q_stricmp("none", teamArg) == 0) {
                    UI_UpdateClientForcePowers(nullptr);
                }
                else if (Q_stricmp("same", teamArg) == 0) { // stay on current team
                    team_t myTeam = UI_CvarTeam("ui_myteam");
                    if (myTeam != team_t::TEAM_SPECTATOR) {
                        UI_UpdateClientForcePowers(UI_TeamName(myTeam)); // will cause him to respawn, if it's been 5 seconds since last one
                    }
                    else {
                        UI_UpdateClientForcePowers(nullptr); // just update powers
                    }
                }
                else {
                    UI_UpdateClientForcePowers(teamArg);
                }
            }
            else {
                UI_UpdateClientForcePowers(nullptr);
            }
        }
        else if (Q_stricmp(name, "setsiegeclassandteam") == 0) {
            team_t team = UI_CvarTeam("ui_holdteam");
            team_t oldteam = UI_CvarTeam("ui_startsiegeteam");
            bool goTeam = true;
            char newclassString[512];
            char startclassString[512];

            trap->Cvar_VariableStringBuffer("ui_mySiegeClass", newclassString, sizeof(newclassString));
            trap->Cvar_VariableStringBuffer("ui_startsiegeclass", startclassString, sizeof(startclassString));

            // Was just a spectator - is still just a spectator
            if ((oldteam == team) && (oldteam == team_t::TEAM_SPECTATOR)) {
                goTeam = false;
            }
            // If new team and class match old team and class, just return to the game.
            else if (oldteam == team) { // Classes match?
                if (g_UIGloballySelectedSiegeClass != -1) {
                    if (!strcmp(startclassString, bgSiegeClasses[g_UIGloballySelectedSiegeClass].name)) {
                        goTeam = false;
                    }
                }
            }

            if (goTeam) {
                if (team == team_t::TEAM_RED) // Team red
                {
                    trap->Cvar_Set("ui_team", Q_Format("{}", team).c_str());
                }
                else if (team == team_t::TEAM_BLUE) // Team blue
                {
                    trap->Cvar_Set("ui_team", Q_Format("{}", team).c_str());
                }
                else if (team == team_t::TEAM_SPECTATOR) // Team spectator
                {
                    trap->Cvar_Set("ui_team", Q_Format("{}", team).c_str());
                }

                if (g_UIGloballySelectedSiegeClass != -1) {
                    trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND,
                                          Q_Format("siegeclass \"{}\"\n", bgSiegeClasses[g_UIGloballySelectedSiegeClass].name).c_str());
                }
            }
        }
        else if (Q_stricmp(name, "setBotButton") == 0) {
            UI_SetBotButton();
        }
        else if (Q_stricmp(name, "saveTemplate") == 0) {
            UI_SaveForceTemplate();
        }
        else if (Q_stricmp(name, "refreshForce") == 0) {
            UI_UpdateForcePowers();
        }
        else if (Q_stricmp(name, "glCustom") == 0) {
            trap->Cvar_Set("ui_r_glCustom", "4");
        }
        else if (Q_stricmp(name, "setMovesListDefault") == 0) {
            uiInfo.movesTitleIndex = datapadMoveTitle_t::MD_SINGLE_MEDIUM;
        }
        else if (Q_stricmp(name, "resetMovesList") == 0) {
            menuDef_t* menu;
            menu = Menus_FindByName("rulesMenu_moves");
            // update saber models
            if (menu) {
                itemDef_t* item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "character");
                if (item) {
                    UI_SaberAttachToChar(item);
                }
            }

            trap->Cvar_Set("ui_move_desc", " ");
        }
        else if (Q_stricmp(name, "resetcharacterlistboxes") == 0) {
            UI_ResetCharacterListBoxes();
        }
        else if (Q_stricmp(name, "setMoveCharacter") == 0) {
            itemDef_t* item;
            menuDef_t* menu;
            modelDef_t* modelPtr;
            int animRunLength;

            UI_GetCharacterCvars();

            uiInfo.movesTitleIndex = datapadMoveTitle_t::MD_ACROBATICS;

            menu = Menus_FindByName("rulesMenu_moves");

            if (menu) {
                item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "character");
                if (item) {
                    modelPtr = item->typeData.model;
                    if (modelPtr) {
                        char modelPath[MAX_QPATH];

                        uiInfo.movesBaseAnim = datapadMoveTitleBaseAnims[uiInfo.movesTitleIndex];
                        ItemParse_model_g2anim_go(item, uiInfo.movesBaseAnim);
                        uiInfo.moveAnimTime = 0;

                        Q_FormatTo(modelPath, "models/players/{}/model.glm", UI_Cvar_VariableString("ui_char_model"));
                        ItemParse_asset_model_go(item, modelPath, &animRunLength);

                        UI_UpdateCharacterSkin();
                        UI_SaberAttachToChar(item);
                    }
                }
            }
        }
        else if (Q_stricmp(name, "character") == 0) {
            UI_UpdateCharacter(false);
        }
        else if (Q_stricmp(name, "characterchanged") == 0) {
            UI_UpdateCharacter(true);
        }
        else if (Q_stricmp(name, "updatecharcvars") == 0 || (Q_stricmp(name, "updatecharmodel") == 0)) {
            UI_UpdateCharacterCvars();
        }
        else if (Q_stricmp(name, "getcharcvars") == 0) {
            UI_GetCharacterCvars();
        }
        else if (Q_stricmp(name, "char_skin") == 0) {
            UI_UpdateCharacterSkin();
        }
        else if (Q_stricmp(name, "setui_dualforcepower") == 0) {
            int forcePowerDisable = trap->Cvar_VariableValue("g_forcePowerDisable");
            int forceBitFlag = 0;

            // Turn off all powers but a few
            for (forcePowers_t fp = forcePowers_t::FP_FIRST; fp < forcePowers_t::NUM_FORCE_POWERS; fp++) {
                if ((fp != forcePowers_t::FP_LEVITATION) && (fp != forcePowers_t::FP_PUSH) && (fp != forcePowers_t::FP_PULL) &&
                    (fp != forcePowers_t::FP_SABERTHROW) && (fp != forcePowers_t::FP_SABER_DEFENSE) && (fp != forcePowers_t::FP_SABER_OFFENSE)) {
                    forceBitFlag |= Bit(fp);
                }
            }

            if (forcePowerDisable == 0) {
                trap->Cvar_Set("ui_dualforcepower", "0");
            }
            else if (forcePowerDisable == forceBitFlag) {
                trap->Cvar_Set("ui_dualforcepower", "2");
            }
            else {
                trap->Cvar_Set("ui_dualforcepower", "1");
            }
        }
        else if (Q_stricmp(name, "dualForcePowers") == 0) {
            int dualforcePower, forcePowerDisable = 0;
            dualforcePower = trap->Cvar_VariableValue("ui_dualforcepower");

            if (dualforcePower == 0) // All force powers
            {
                forcePowerDisable = 0;
            }
            else if (dualforcePower == 1) // Remove All force powers
            {
                // It was set to something, so might as well make sure it got all flags set.
                for (forcePowers_t fp = forcePowers_t::FP_FIRST; fp < forcePowers_t::NUM_FORCE_POWERS; fp++) {
                    forcePowerDisable |= Bit(fp);
                }
            }
            else if (dualforcePower == 2) // Limited force powers
            {
                forcePowerDisable = 0;

                // Turn off all powers but a few
                for (forcePowers_t fp = forcePowers_t::FP_FIRST; fp < forcePowers_t::NUM_FORCE_POWERS; fp++) {
                    if ((fp != forcePowers_t::FP_LEVITATION) && (fp != forcePowers_t::FP_PUSH) && (fp != forcePowers_t::FP_PULL) &&
                        (fp != forcePowers_t::FP_SABERTHROW) && (fp != forcePowers_t::FP_SABER_DEFENSE) && (fp != forcePowers_t::FP_SABER_OFFENSE)) {
                        forcePowerDisable |= Bit(fp);
                    }
                }
            }

            trap->Cvar_Set("g_forcePowerDisable", Q_Format("{}", forcePowerDisable).c_str());
        }
        else if (Q_stricmp(name, "forcePowersDisable") == 0) {
            int forcePowerDisable;

            forcePowerDisable = trap->Cvar_VariableValue("g_forcePowerDisable");

            // It was set to something, so might as well make sure it got all flags set.
            if (forcePowerDisable) {
                for (forcePowers_t fp = forcePowers_t::FP_FIRST; fp < forcePowers_t::NUM_FORCE_POWERS; fp++) {
                    forcePowerDisable |= Bit(fp);
                }

                trap->Cvar_Set("g_forcePowerDisable", Q_Format("{}", forcePowerDisable).c_str());
            }
        }
        else if (Q_stricmp(name, "weaponDisable") == 0) {
            int weaponDisable;
            const char* cvarString;

            if (uiInfo.gameTypes[ui_netGametype.integer].gtEnum == gametype_t::GT_DUEL ||
                uiInfo.gameTypes[ui_netGametype.integer].gtEnum == gametype_t::GT_POWERDUEL) {
                cvarString = "g_duelWeaponDisable";
            }
            else {
                cvarString = "g_weaponDisable";
            }

            weaponDisable = trap->Cvar_VariableValue(cvarString);

            // It was set to something, so might as well make sure it got all flags set.
            if (weaponDisable) {
                for (weapon_t wp = weapon_t::WP_NONE; wp < weapon_t::WP_NUM_WEAPONS; wp++) {
                    if (wp != weapon_t::WP_SABER) {
                        weaponDisable |= Bit(wp);
                    }
                }

                trap->Cvar_Set(cvarString, Q_Format("{}", weaponDisable).c_str());
            }
        }
        // If this is siege, change all the bots to humans, because we faked it earlier
        //  swapping humans for bots on the menu
        else if (Q_stricmp(name, "setSiegeNoBots") == 0) {
            int blueValue, redValue, i;

            if (uiInfo.gameTypes[ui_netGametype.integer].gtEnum == gametype_t::GT_SIEGE) {
                // hmm, I guess I'll set bot_minplayers to 0 here too. -rww
                trap->Cvar_Set("bot_minplayers", "0");

                for (i = 1; i < 9; i++) {
                    blueValue = trap->Cvar_VariableValue(Q_Format("ui_blueteam{}", i).c_str());
                    if (blueValue > 1) {
                        trap->Cvar_Set(Q_Format("ui_blueteam{}", i).c_str(), "1");
                    }

                    redValue = trap->Cvar_VariableValue(Q_Format("ui_redteam{}", i).c_str());
                    if (redValue > 1) {
                        trap->Cvar_Set(Q_Format("ui_redteam{}", i).c_str(), "1");
                    }
                }
            }
        }
        else if (Q_stricmp(name, "clearmouseover") == 0) {
            itemDef_t* item;
            menuDef_t* menu = Menu_GetFocused();

            if (menu) {
                int count, j;
                const char* itemName = nullptr;
                if (String_Parse(args, &itemName)) {
                    count = Menu_ItemsMatchingGroup(menu, itemName);

                    for (j = 0; j < count; j++) {
                        item = Menu_GetMatchingItemByNumber(menu, j, itemName);
                        if (item != nullptr) {
                            item->window.flags &= ~windowFlags_t::WINDOW_MOUSEOVER;
                        }
                    }
                }
            }
        }
        else if (Q_stricmp(name, "updateForceStatus") == 0) {
            UpdateForceStatus();
        }
        else if (Q_stricmp(name, "update") == 0) {
            if (String_Parse(args, &name2)) {
                UI_Update(name2);
            }
        }
        else if (Q_stricmp(name, "setBotButtons") == 0) {
            UpdateBotButtons();
        }
        else if (Q_stricmp(name, "getsabercvars") == 0) {
            UI_GetSaberCvars();
        }
        else if (Q_stricmp(name, "setsaberboxesandhilts") == 0) {
            UI_SetSaberBoxesandHilts();
        }
        else if (Q_stricmp(name, "saber_type") == 0) {
            UI_UpdateSaberType();
        }
        else if (Q_stricmp(name, "saber_hilt") == 0) {
            UI_UpdateSaberHilt(false);
        }
        else if (Q_stricmp(name, "saber_color") == 0) {
            UI_UpdateSaberColor(false);
        }
        else if (Q_stricmp(name, "setscreensaberhilt") == 0) {
            menuDef_t* menu;
            itemDef_t* item;

            menu = Menu_GetFocused(); // Get current menu
            if (menu) {
                item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "hiltbut");
                if (item) {
                    if (saberSingleHiltInfo[item->cursorPos]) {
                        trap->Cvar_Set("ui_saber", saberSingleHiltInfo[item->cursorPos]);
                    }
                }
            }
        }
        else if (Q_stricmp(name, "setscreensaberhilt1") == 0) {
            menuDef_t* menu;
            itemDef_t* item;

            menu = Menu_GetFocused(); // Get current menu
            if (menu) {
                item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "hiltbut1");
                if (item) {
                    if (saberSingleHiltInfo[item->cursorPos]) {
                        trap->Cvar_Set("ui_saber", saberSingleHiltInfo[item->cursorPos]);
                    }
                }
            }
        }
        else if (Q_stricmp(name, "setscreensaberhilt2") == 0) {
            menuDef_t* menu;
            itemDef_t* item;

            menu = Menu_GetFocused(); // Get current menu
            if (menu) {
                item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "hiltbut2");
                if (item) {
                    if (saberSingleHiltInfo[item->cursorPos]) {
                        trap->Cvar_Set("ui_saber2", saberSingleHiltInfo[item->cursorPos]);
                    }
                }
            }
        }
        else if (Q_stricmp(name, "setscreensaberstaff") == 0) {
            menuDef_t* menu;
            itemDef_t* item;

            menu = Menu_GetFocused(); // Get current menu
            if (menu) {
                item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "hiltbut_staves");
                if (item) {
                    if (saberSingleHiltInfo[item->cursorPos]) {
                        trap->Cvar_Set("ui_saber", saberStaffHiltInfo[item->cursorPos]);
                    }
                }
            }
        }
        else if (Q_stricmp(name, "saber2_hilt") == 0) {
            UI_UpdateSaberHilt(true);
        }
        else if (Q_stricmp(name, "saber2_color") == 0) {
            UI_UpdateSaberColor(true);
        }
        else if (Q_stricmp(name, "updatesabercvars") == 0) {
            UI_UpdateSaberCvars();
        }
        else if (Q_stricmp(name, "updatesiegeobjgraphics") == 0) {
            team_t team = UI_CvarTeam("ui_team");
            trap->Cvar_Set("ui_holdteam", Q_Format("{}", team).c_str());

            UI_UpdateSiegeObjectiveGraphics();
        }
        else if (Q_stricmp(name, "setsiegeobjbuttons") == 0) {
            const char* itemArg;
            const char* cvarLitArg;
            const char* cvarNormalArg;
            char string[512];
            char string2[512];
            menuDef_t* menu;
            itemDef_t* item;

            menu = Menu_GetFocused(); // Get current menu
            if (menu) {
                // Set the new item to the background
                if (String_Parse(args, &itemArg)) {

                    // Set the old button to it's original background
                    trap->Cvar_VariableStringBuffer("currentObjMapIconItem", string, sizeof(string));
                    item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, string);
                    if (item) {
                        // A cvar holding the name of a cvar - how crazy is that?
                        trap->Cvar_VariableStringBuffer("currentObjMapIconBackground", string, sizeof(string));
                        trap->Cvar_VariableStringBuffer(string, string2, sizeof(string2));
                        Menu_SetItemBackground(menu, item->window.name, string2);

                        // Re-enable this button
                        Menu_ItemDisable(menu, item->window.name, false);
                    }

                    // Set the new item to the given background
                    item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, itemArg);
                    if (item) { // store item name
                        trap->Cvar_Set("currentObjMapIconItem", item->window.name);
                        if (String_Parse(args, &cvarNormalArg)) { // Store normal background
                            trap->Cvar_Set("currentObjMapIconBackground", cvarNormalArg);
                            // Get higlight background
                            if (String_Parse(args, &cvarLitArg)) { // set hightlight background
                                trap->Cvar_VariableStringBuffer(cvarLitArg, string, sizeof(string));
                                Menu_SetItemBackground(menu, item->window.name, string);
                                // Disable button
                                Menu_ItemDisable(menu, item->window.name, true);
                            }
                        }
                    }
                }
            }
        }
        else if (Q_stricmp(name, "updatesiegeclasscnt") == 0) {
            const char* teamArg;

            if (String_Parse(args, &teamArg)) {
                UI_SiegeClassCnt(static_cast<team_t>(atoi(teamArg)));
            }
        }
        else if (Q_stricmp(name, "updatesiegecvars") == 0) {
            siegePlayerClassFlags_t baseClass;
            team_t team;

            team = UI_CvarTeam("ui_holdteam");
            baseClass = UI_CvarSiegeClass();

            UI_UpdateCvarsForClass(team, baseClass, 0);
        }
        // Save current team and class
        else if (Q_stricmp(name, "setteamclassicons") == 0) {
            team_t team = UI_CvarTeam("ui_holdteam");
            char classString[512];

            trap->Cvar_VariableStringBuffer("ui_mySiegeClass", classString, sizeof(classString));

            trap->Cvar_Set("ui_startsiegeteam", Q_Format("{}", team).c_str());
            trap->Cvar_Set("ui_startsiegeclass", classString);

            // If player is already on a team, set up icons to show it.
            UI_FindCurrentSiegeTeamClass();
        }
        else if (Q_stricmp(name, "updatesiegeweapondesc") == 0) {
            menuDef_t* menu;
            itemDef_t* item;

            menu = Menu_GetFocused(); // Get current menu
            if (menu) {
                item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "base_class_weapons_feed");
                if (item) {
                    char info[MAX_INFO_VALUE];
                    trap->Cvar_VariableStringBuffer(Q_Format("ui_class_weapondesc{}", item->cursorPos).c_str(), info, sizeof(info));
                    trap->Cvar_Set("ui_itemforceinvdesc", info);
                }
            }
        }
        else if (Q_stricmp(name, "updatesiegeinventorydesc") == 0) {
            menuDef_t* menu;
            itemDef_t* item;

            menu = Menu_GetFocused(); // Get current menu
            if (menu) {
                item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "base_class_inventory_feed");
                if (item) {
                    char info[MAX_INFO_VALUE];
                    trap->Cvar_VariableStringBuffer(Q_Format("ui_class_itemdesc{}", item->cursorPos).c_str(), info, sizeof(info));
                    trap->Cvar_Set("ui_itemforceinvdesc", info);
                }
            }
        }
        else if (Q_stricmp(name, "updatesiegeforcedesc") == 0) {
            menuDef_t* menu;
            itemDef_t* item;

            menu = Menu_GetFocused(); // Get current menu
            if (menu) {
                item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "base_class_force_feed");
                if (item) {
                    char info[MAX_STRING_CHARS];

                    trap->Cvar_VariableStringBuffer(Q_Format("ui_class_power{}", item->cursorPos).c_str(), info, sizeof(info));

                    // count them up
                    for (forcePowers_t fp = forcePowers_t::FP_FIRST; fp < forcePowers_t::NUM_FORCE_POWERS; fp++) {
                        if (!strcmp(HolocronIcons[fp], info)) {
                            trap->Cvar_Set("ui_itemforceinvdesc", forcepowerDesc[fp]);
                        }
                    }
                }
            }
        }
        else if (Q_stricmp(name, "resetitemdescription") == 0) {
            menuDef_t* menu;
            itemDef_t* item;

            menu = Menu_GetFocused(); // Get current menu
            if (menu) {
                item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "itemdescription");
                if (item) {
                    listBoxDef_t* listPtr = item->typeData.listbox;
                    if (listPtr) {
                        listPtr->startPos = 0;
                        listPtr->cursorPos = 0;
                    }
                    item->cursorPos = 0;
                }
            }
        }
        else if (Q_stricmp(name, "resetsiegelistboxes") == 0) {
            menuDef_t* menu;
            itemDef_t* item;

            menu = Menu_GetFocused(); // Get current menu
            if (menu) {
                item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "description");
                if (item) {
                    listBoxDef_t* listPtr = item->typeData.listbox;
                    if (listPtr) {
                        listPtr->startPos = 0;
                    }
                    item->cursorPos = 0;
                }
            }

            menu = Menu_GetFocused(); // Get current menu
            if (menu) {
                item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "base_class_weapons_feed");
                if (item) {
                    listBoxDef_t* listPtr = item->typeData.listbox;
                    if (listPtr) {
                        listPtr->startPos = 0;
                    }
                    item->cursorPos = 0;
                }

                item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "base_class_inventory_feed");
                if (item) {
                    listBoxDef_t* listPtr = item->typeData.listbox;
                    if (listPtr) {
                        listPtr->startPos = 0;
                    }
                    item->cursorPos = 0;
                }

                item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "base_class_force_feed");
                if (item) {
                    listBoxDef_t* listPtr = item->typeData.listbox;
                    if (listPtr) {
                        listPtr->startPos = 0;
                    }
                    item->cursorPos = 0;
                }
            }
        }
        else if (Q_stricmp(name, "updatesiegestatusicons") == 0) {
            UI_UpdateSiegeStatusIcons();
        }
        else if (Q_stricmp(name, "setcurrentNetMap") == 0) {
            menuDef_t* menu;
            itemDef_t* item;

            menu = Menu_GetFocused(); // Get current menu
            if (menu) {
                item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "maplist");
                if (item) {
                    listBoxDef_t* listPtr = item->typeData.listbox;
                    if (listPtr) {
                        trap->Cvar_Set("ui_currentNetMap", Q_Format("{}", listPtr->cursorPos).c_str());
                    }
                }
            }
        }
        else if (Q_stricmp(name, "resetmaplist") == 0) {
            menuDef_t* menu;
            itemDef_t* item;

            menu = Menu_GetFocused(); // Get current menu
            if (menu) {
                item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "maplist");
                if (item) {
                    uiInfo.uiDC.feederSelection(item->special, item->cursorPos, item);
                }
            }
        }
        else if (Q_stricmp(name, "getmousepitch") == 0) {
            trap->Cvar_Set("ui_mousePitch", (trap->Cvar_VariableValue("m_pitch") >= 0) ? "0" : "1");
        }
        else if (Q_stricmp(name, "clampmaxplayers") == 0) {
            UI_ClampMaxPlayers();
        }
        else if (Q_stricmp(name, "LaunchSP") == 0) {
            // TODO for MAC_PORT
        }
        else {
            Q_Println("unknown UI script {}", name);
        }
    }
    return true;
}

static void UI_GetTeamColor(vec4_t* color) {}

void UI_SetSiegeTeams(void)
{
    char info[MAX_INFO_VALUE];
    const char* mapname = nullptr;
    char levelname[MAX_QPATH];
    char btime[1024];
    char teams[2048];
    char teamInfo[MAX_SIEGE_INFO_SIZE];
    char team1[1024];
    char team2[1024];
    int len = 0;
    gametype_t gametype;
    fileHandle_t f;

    // Get the map name from the server info
    if (trap->GetConfigString(CS_SERVERINFO, info, sizeof(info))) {
        mapname = Info_ValueForKey(info, "mapname");
    }

    if (!mapname || !mapname[0]) {
        return;
    }

    gametype = static_cast<gametype_t>(atoi(Info_ValueForKey(info, "g_gametype")));

    // If the server we are connected to is not siege we cannot choose a class anyway
    if (gametype != gametype_t::GT_SIEGE) {
        return;
    }

    Q_FormatTo(levelname, "maps/{}.siege", mapname);

    if (!levelname[0]) {
        return;
    }

    len = trap->FS_Open(levelname, &f, fsMode_t::FS_READ);

    if (!f) {
        return;
    }
    if (len >= MAX_SIEGE_INFO_SIZE) {
        trap->FS_Close(f);
        return;
    }

    trap->FS_Read(siege_info, len, f);
    siege_info[len] = 0; // ensure null terminated

    trap->FS_Close(f);

    // Found the .siege file

    if (BG_SiegeGetValueGroup(siege_info, "Teams", teams)) {
        char buf[1024];

        trap->Cvar_VariableStringBuffer("cg_siegeTeam1", buf, 1024);
        if (buf[0] && Q_stricmp(buf, "none")) {
            Q_strncpyz(team1, buf, sizeof(team1));
        }
        else {
            BG_SiegeGetPairedValue(teams, "team1", team1);
        }

        trap->Cvar_VariableStringBuffer("cg_siegeTeam2", buf, 1024);
        if (buf[0] && Q_stricmp(buf, "none")) {
            Q_strncpyz(team2, buf, sizeof(team2));
        }
        else {
            BG_SiegeGetPairedValue(teams, "team2", team2);
        }
    }
    else {
        return;
    }

    // Set the team themes so we know what classes to make available for selection
    if (BG_SiegeGetValueGroup(siege_info, team1, teamInfo)) {
        if (BG_SiegeGetPairedValue(teamInfo, "UseTeam", btime)) {
            BG_SiegeSetTeamTheme(SIEGETEAM_TEAM1, btime);
        }
    }
    if (BG_SiegeGetValueGroup(siege_info, team2, teamInfo)) {
        if (BG_SiegeGetPairedValue(teamInfo, "UseTeam", btime)) {
            BG_SiegeSetTeamTheme(SIEGETEAM_TEAM2, btime);
        }
    }

    siegeTeam1 = BG_SiegeFindThemeForTeam(SIEGETEAM_TEAM1);
    siegeTeam2 = BG_SiegeFindThemeForTeam(SIEGETEAM_TEAM2);

    // set the default description for the default selection
    if (!siegeTeam1 || !siegeTeam1->classes[0]) {
        Q_Error(errorParm_t::ERR_DROP, "Error loading teams in UI");
    }

    Menu_SetFeederSelection(nullptr, FEEDER_SIEGE_TEAM1, 0, nullptr);
    Menu_SetFeederSelection(nullptr, FEEDER_SIEGE_TEAM2, -1, nullptr);
}

static void UI_SiegeClassCnt(const team_t team)
{
    UI_SetSiegeTeams();

    trap->Cvar_Set("ui_infantry_cnt", Q_Format("{}", BG_SiegeCountBaseClass(team, siegePlayerClassFlags_t::SPC_INFANTRY)).c_str());
    trap->Cvar_Set("ui_vanguard_cnt", Q_Format("{}", BG_SiegeCountBaseClass(team, siegePlayerClassFlags_t::SPC_VANGUARD)).c_str());
    trap->Cvar_Set("ui_support_cnt", Q_Format("{}", BG_SiegeCountBaseClass(team, siegePlayerClassFlags_t::SPC_SUPPORT)).c_str());
    trap->Cvar_Set("ui_jedi_cnt", Q_Format("{}", BG_SiegeCountBaseClass(team, siegePlayerClassFlags_t::SPC_JEDI)).c_str());
    trap->Cvar_Set("ui_demo_cnt", Q_Format("{}", BG_SiegeCountBaseClass(team, siegePlayerClassFlags_t::SPC_DEMOLITIONIST)).c_str());
    trap->Cvar_Set("ui_heavy_cnt", Q_Format("{}", BG_SiegeCountBaseClass(team, siegePlayerClassFlags_t::SPC_HEAVY_WEAPONS)).c_str());
}

/*
==================
UI_MapCountByGameType
==================
*/
static int UI_MapCountByGameType(bool singlePlayer)
{
    int i, c;
    gametype_t game;
    c = 0;
    game = singlePlayer ? uiInfo.gameTypes[ui_gametype.integer].gtEnum : uiInfo.gameTypes[ui_netGametype.integer].gtEnum;
    if (game == gametype_t::GT_TEAM)
        game = gametype_t::GT_FFA;

    // Since GT_CTY uses the same entities as CTF, use the same map sets
    if (game == gametype_t::GT_CTY)
        game = gametype_t::GT_CTF;

    for (i = 0; i < uiInfo.mapCount; i++) {
        uiInfo.mapList[i].active = false;
        if (uiInfo.mapList[i].typeBits & Bit(game)) {
            if (singlePlayer) {
                if (!(uiInfo.mapList[i].typeBits & Bit(gametype_t::GT_SINGLE_PLAYER))) {
                    continue;
                }
            }
            c++;
            uiInfo.mapList[i].active = true;
        }
    }
    return c;
}

bool UI_hasSkinForBase(const char* base, const char* team)
{
    char test[1024];
    fileHandle_t f;

    Q_FormatTo(test, "models/players/{}/{}/lower_default.skin", base, team);
    trap->FS_Open(test, &f, fsMode_t::FS_READ);
    if (f != 0) {
        trap->FS_Close(f);
        return true;
    }
    Q_FormatTo(test, "models/players/characters/{}/{}/lower_default.skin", base, team);
    trap->FS_Open(test, &f, fsMode_t::FS_READ);
    if (f != 0) {
        trap->FS_Close(f);
        return true;
    }
    return false;
}

/*
==================
UI_HeadCountByColor
==================
*/
static int UI_HeadCountByColor(void)
{
    int i, c;
    const char* teamname;

    c = 0;

    switch (uiSkinColor) {
    case team_t::TEAM_BLUE:
        teamname = "/blue";
        break;
    case team_t::TEAM_RED:
        teamname = "/red";
        break;
    default:
        teamname = "/default";
    }

    // Count each head with this color
    for (i = 0; i < uiInfo.q3HeadCount; i++) {
        if (uiInfo.q3HeadNames[i][0] && strstr(uiInfo.q3HeadNames[i], teamname)) {
            c++;
        }
    }
    return c;
}

int Q_isprintext(int c);
int Q_isgraph(int c);
/*
==================
UI_ServerInfoIsValid

Return false if the infostring contains nonprinting characters,
or if the hostname is blank/undefined
==================
*/
static bool UI_ServerInfoIsValid(char* info)
{
    const char* c;

    for (c = info; *c; c++) {
        if (!Q_isprintext(*(unsigned char*)c)) // isprint
            return false;
    }

    for (c = Info_ValueForKey(info, "hostname"); *c; c++) {
        if (Q_isgraph(*(unsigned char*)c)) // isgraph
            return true;
    }

    return false;
}

/*
==================
UI_InsertServerIntoDisplayList
==================
*/
static void UI_InsertServerIntoDisplayList(int num, int position)
{
    int i;
    static char info[MAX_STRING_CHARS] = {0};

    if (position < 0 || position > uiInfo.serverStatus.numDisplayServers) {
        return;
    }

    trap->LAN_GetServerInfo(UI_SourceForLAN(), num, info, sizeof(info));

    uiInfo.serverStatus.numDisplayServers++;
    for (i = uiInfo.serverStatus.numDisplayServers; i > position; i--) {
        uiInfo.serverStatus.displayServers[i] = uiInfo.serverStatus.displayServers[i - 1];
    }
    uiInfo.serverStatus.displayServers[position] = num;
}

/*
==================
UI_RemoveServerFromDisplayList
==================
*/
static void UI_RemoveServerFromDisplayList(int num)
{
    int i, j;

    for (i = 0; i < uiInfo.serverStatus.numDisplayServers; i++) {
        if (uiInfo.serverStatus.displayServers[i] == num) {
            uiInfo.serverStatus.numDisplayServers--;
            for (j = i; j < uiInfo.serverStatus.numDisplayServers; j++) {
                uiInfo.serverStatus.displayServers[j] = uiInfo.serverStatus.displayServers[j + 1];
            }
            return;
        }
    }
}

/*
==================
UI_BinaryServerInsertion
==================
*/
static void UI_BinaryServerInsertion(int num)
{
    int mid, offset, res, len;

    // use binary search to insert server
    len = uiInfo.serverStatus.numDisplayServers;
    mid = len;
    offset = 0;
    res = 0;
    while (mid > 0) {
        mid = len >> 1;
        //
        res = trap->LAN_CompareServers(UI_SourceForLAN(), uiInfo.serverStatus.sortKey, uiInfo.serverStatus.sortDir, num,
                                       uiInfo.serverStatus.displayServers[offset + mid]);
        // if equal
        if (res == 0) {
            UI_InsertServerIntoDisplayList(num, offset + mid);
            return;
        }
        // if larger
        else if (res == 1) {
            offset += mid;
            len -= mid;
        }
        // if smaller
        else {
            len -= mid;
        }
    }
    if (res == 1) {
        offset++;
    }
    UI_InsertServerIntoDisplayList(num, offset);
}

/*
==================
UI_BuildServerDisplayList
==================
*/
static void UI_BuildServerDisplayList(int force)
{
    int i, count, clients, maxClients, game, len, passw /*, visible*/;
    bool answered;
    char info[MAX_STRING_CHARS];
    //	bool startRefresh = true; TTimo: unused
    lanSource_t lanSource;

    if (!(force || uiInfo.uiDC.realTime > uiInfo.serverStatus.nextDisplayRefresh)) {
        return;
    }
    // if we shouldn't reset
    if (force == 2) {
        force = 0;
    }

    // do motd updates here too
    trap->Cvar_VariableStringBuffer("cl_motdString", uiInfo.serverStatus.motd, sizeof(uiInfo.serverStatus.motd));
    len = strlen(uiInfo.serverStatus.motd);
    if (len == 0) {
        Q_strncpyz(uiInfo.serverStatus.motd, "Welcome to Jedi Academy MP!", sizeof(uiInfo.serverStatus.motd));
        len = strlen(uiInfo.serverStatus.motd);
    }
    if (len != uiInfo.serverStatus.motdLen) {
        uiInfo.serverStatus.motdLen = len;
        uiInfo.serverStatus.motdWidth = -1;
    }

    lanSource = UI_SourceForLAN();

    if (force) {
        // clear number of displayed servers
        uiInfo.serverStatus.numDisplayServers = 0;
        uiInfo.serverStatus.numPlayersOnServers = 0;
        // set list box index to zero
        Menu_SetFeederSelection(nullptr, FEEDER_SERVERS, 0, nullptr);
        // mark all servers as visible so we store ping updates for them
        trap->LAN_MarkServerVisible(lanSource, -1, true);
    }

    // get the server count (comes from the master)
    count = trap->LAN_GetServerCount(lanSource);
    if (count == -1 || (UI_NetSource() == uiNetSource_t::UIAS_LOCAL && count == 0)) {
        // still waiting on a response from the master
        uiInfo.serverStatus.numDisplayServers = 0;
        uiInfo.serverStatus.numPlayersOnServers = 0;
        uiInfo.serverStatus.nextDisplayRefresh = uiInfo.uiDC.realTime + 500;
        return;
    }

    trap->Cvar_Update(&ui_browserFilterInvalidInfo);
    trap->Cvar_Update(&ui_browserShowEmpty);
    trap->Cvar_Update(&ui_browserShowFull);
    trap->Cvar_Update(&ui_browserShowPasswordProtected);
    trap->Cvar_Update(&ui_serverFilterType);
    trap->Cvar_Update(&ui_joinGametype);

    //	visible = false;
    for (i = 0; i < count; i++) {
        // if we already got info for this server
        if (!trap->LAN_ServerIsVisible(lanSource, i)) {
            continue;
        }
        //		visible = true;
        // has this server answered
        answered = (trap->ext.LAN_GetServerState(lanSource, i) == lanServerState_t::LANSERVER_ANSWERED);
        if (answered || UI_NetSource() == uiNetSource_t::UIAS_FAVORITES) {

            trap->LAN_GetServerInfo(lanSource, i, info, MAX_STRING_CHARS);

            // don't list servers with invalid info
            if (ui_browserFilterInvalidInfo.integer != 0 && !UI_ServerInfoIsValid(info)) {
                trap->LAN_MarkServerVisible(lanSource, i, false);
                continue;
            }

            clients = atoi(Info_ValueForKey(info, "clients"));
            uiInfo.serverStatus.numPlayersOnServers += clients;

            if (ui_browserShowEmpty.integer == 0) {
                if (clients == 0) {
                    trap->LAN_MarkServerVisible(lanSource, i, false);
                    continue;
                }
            }

            if (ui_browserShowFull.integer == 0) {
                maxClients = atoi(Info_ValueForKey(info, "sv_maxclients"));
                if (clients == maxClients) {
                    trap->LAN_MarkServerVisible(lanSource, i, false);
                    continue;
                }
            }

            if (ui_browserShowPasswordProtected.integer == 0) {
                passw = atoi(Info_ValueForKey(info, "needpass"));
                if (passw && !ui_browserShowPasswordProtected.integer) {
                    trap->LAN_MarkServerVisible(lanSource, i, false);
                    continue;
                }
            }

            if (uiInfo.joinGameTypes[ui_joinGametype.integer].gtEnum != gametype_t::GT_UNKNOWN) {
                game = atoi(Info_ValueForKey(info, "gametype"));
                if (static_cast<gametype_t>(game) != uiInfo.joinGameTypes[ui_joinGametype.integer].gtEnum) {
                    trap->LAN_MarkServerVisible(lanSource, i, false);
                    continue;
                }
            }

            if (ui_serverFilterType.integer > 0 && ui_serverFilterType.integer <= uiInfo.modCount) {
                if (Q_stricmp(Info_ValueForKey(info, "game"), UI_FilterDir(ui_serverFilterType.integer)) != 0) {
                    trap->LAN_MarkServerVisible(lanSource, i, false);
                    continue;
                }
            }
            // make sure we never add a favorite server twice
            if (UI_NetSource() == uiNetSource_t::UIAS_FAVORITES) {
                UI_RemoveServerFromDisplayList(i);
            }
            // insert the server into the list
            UI_BinaryServerInsertion(i);
            // done with this server
            if (answered) {
                trap->LAN_MarkServerVisible(lanSource, i, false);
            }
        }
    }

    uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime;

    // if there were no servers visible for ping updates
    //	if (!visible) {
    //		UI_StopServerRefresh();
    //		uiInfo.serverStatus.nextDisplayRefresh = 0;
    //	}
}

typedef struct serverStatusCvar_s {
    const char *name, *altName;
} serverStatusCvar_t;

serverStatusCvar_t serverStatusCvars[] = {{"sv_hostname", "Name"}, {"Address", ""},   {"gamename", "Game name"}, {"g_gametype", "Game type"},
                                          {"mapname", "Map"},      {"version", ""},   {"protocol", ""},          {"timelimit", ""},
                                          {"fraglimit", ""},       {nullptr, nullptr}};

/*
==================
UI_SortServerStatusInfo
==================
*/
static void UI_SortServerStatusInfo(serverStatusInfo_t* info)
{
    int i, j, index, numLines;
    const char *tmp1, *tmp2;

    // FIXME: if "gamename" == "base" or "missionpack" then
    // replace the gametype number by FFA, CTF etc.
    //
    index = 0;
    numLines = Com_Clampi(0, MAX_SERVERSTATUS_LINES, info->numLines);
    for (i = 0; serverStatusCvars[i].name; i++) {
        for (j = 0; j < numLines; j++) {
            if (!info->lines[j][1] || info->lines[j][1][0]) {
                continue;
            }
            if (!Q_stricmp(serverStatusCvars[i].name, info->lines[j][0])) {
                // swap lines
                tmp1 = info->lines[index][0];
                tmp2 = info->lines[index][3];
                info->lines[index][0] = info->lines[j][0];
                info->lines[index][3] = info->lines[j][3];
                info->lines[j][0] = tmp1;
                info->lines[j][3] = tmp2;
                //
                if (strlen(serverStatusCvars[i].altName)) {
                    info->lines[index][0] = serverStatusCvars[i].altName;
                }
                index++;
            }
        }
    }
}

/*
==================
UI_GetServerStatusInfo
==================
*/
static int UI_GetServerStatusInfo(const char* serverAddress, serverStatusInfo_t* info)
{
    char *p, *score, *ping, *name;
    int i, len;

    if (!info) {
        trap->LAN_ServerStatus(serverAddress, nullptr, 0);
        return false;
    }
    memset(info, 0, sizeof(*info));
    if (trap->LAN_ServerStatus(serverAddress, info->text, sizeof(info->text))) {
        Q_strncpyz(info->address, serverAddress, sizeof(info->address));
        p = info->text;
        info->numLines = 0;
        info->lines[info->numLines][0] = "Address";
        info->lines[info->numLines][1] = "";
        info->lines[info->numLines][2] = "";
        info->lines[info->numLines][3] = info->address;
        info->numLines++;
        // get the cvars
        while (p && *p) {
            p = strchr(p, '\\');
            if (!p)
                break;
            *p++ = '\0';
            if (*p == '\\')
                break;
            info->lines[info->numLines][0] = p;
            info->lines[info->numLines][1] = "";
            info->lines[info->numLines][2] = "";
            p = strchr(p, '\\');
            if (!p)
                break;
            *p++ = '\0';
            info->lines[info->numLines][3] = p;

            info->numLines++;
            if (info->numLines >= MAX_SERVERSTATUS_LINES)
                break;
        }
        // get the player list
        if (info->numLines < MAX_SERVERSTATUS_LINES - 3) {
            // empty line
            info->lines[info->numLines][0] = "";
            info->lines[info->numLines][1] = "";
            info->lines[info->numLines][2] = "";
            info->lines[info->numLines][3] = "";
            info->numLines++;
            // header
            info->lines[info->numLines][0] = "num";
            info->lines[info->numLines][1] = "score";
            info->lines[info->numLines][2] = "ping";
            info->lines[info->numLines][3] = "name";
            info->numLines++;
            // parse players
            i = 0;
            len = 0;
            while (p && *p) {
                if (*p == '\\')
                    *p++ = '\0';
                score = p;
                p = strchr(p, ' ');
                if (!p)
                    break;
                *p++ = '\0';
                ping = p;
                p = strchr(p, ' ');
                if (!p)
                    break;
                *p++ = '\0';
                name = p;
                Q_FormatTo(std::span(&info->pings[len], sizeof(info->pings) - len), "{}", i);
                info->lines[info->numLines][0] = &info->pings[len];
                len += strlen(&info->pings[len]) + 1;
                info->lines[info->numLines][1] = score;
                info->lines[info->numLines][2] = ping;
                info->lines[info->numLines][3] = name;
                info->numLines++;
                if (info->numLines >= MAX_SERVERSTATUS_LINES)
                    break;
                p = strchr(p, '\\');
                if (!p)
                    break;
                *p++ = '\0';
                //
                i++;
            }
        }
        UI_SortServerStatusInfo(info);
        return true;
    }
    return false;
}

/*
==================
UI_BuildFindPlayerList
==================
*/
static void UI_BuildFindPlayerList(bool force)
{
    static int numFound;
    int i, j, resend;
    serverStatusInfo_t info;
    char name[MAX_NAME_LENGTH + 2];
    char infoString[MAX_STRING_CHARS];
    lanSource_t lanSource;

    if (!force) {
        if (!uiInfo.nextFindPlayerRefresh || uiInfo.nextFindPlayerRefresh > uiInfo.uiDC.realTime) {
            return;
        }
    }
    else {
        memset(&uiInfo.pendingServerStatus, 0, sizeof(uiInfo.pendingServerStatus));
        uiInfo.numFoundPlayerServers = 0;
        uiInfo.currentFoundPlayerServer = 0;
        trap->Cvar_VariableStringBuffer("ui_findPlayer", uiInfo.findPlayerName, sizeof(uiInfo.findPlayerName));
        Q_StripColor(uiInfo.findPlayerName);
        // should have a string of some length
        if (!strlen(uiInfo.findPlayerName)) {
            uiInfo.nextFindPlayerRefresh = 0;
            return;
        }
        // set resend time
        resend = ui_serverStatusTimeOut.integer / 2 - 10;
        if (resend < 50) {
            resend = 50;
        }
        trap->Cvar_Set("cl_serverStatusResendTime", Q_Format("{}", resend).c_str());
        // reset all server status requests
        trap->LAN_ServerStatus(nullptr, nullptr, 0);
        //
        uiInfo.numFoundPlayerServers = 1;

        trap->SE_GetStringTextString("MENUS_SEARCHING", holdSPString, sizeof(holdSPString));
        trap->Cvar_Set("ui_playerServersFound", Q_Format(fmt::runtime(holdSPString), uiInfo.pendingServerStatus.num, numFound).c_str());
        //	Q_FormatTo(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers-1], "searching {}...", uiInfo.pendingServerStatus.num);
        numFound = 0;
    }
    for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
        // if this pending server is valid
        if (uiInfo.pendingServerStatus.server[i].valid) {
            // try to get the server status for this server
            if (UI_GetServerStatusInfo(uiInfo.pendingServerStatus.server[i].adrstr, &info)) {
                //
                numFound++;
                // parse through the server status lines
                for (j = 0; j < info.numLines; j++) {
                    // should have ping info
                    if (!info.lines[j][2] || !info.lines[j][2][0]) {
                        continue;
                    }
                    // clean string first
                    Q_strncpyz(name, info.lines[j][3], sizeof(name));
                    Q_StripColor(name);
                    // if the player name is a substring
                    if (Q_stristr(name, uiInfo.findPlayerName)) {
                        // add to found server list if we have space (always leave space for a line with the number found)
                        if (uiInfo.numFoundPlayerServers < MAX_FOUNDPLAYER_SERVERS - 1) {
                            //
                            Q_strncpyz(uiInfo.foundPlayerServerAddresses[uiInfo.numFoundPlayerServers - 1], uiInfo.pendingServerStatus.server[i].adrstr,
                                       sizeof(uiInfo.foundPlayerServerAddresses[0]));
                            Q_strncpyz(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers - 1], uiInfo.pendingServerStatus.server[i].name,
                                       sizeof(uiInfo.foundPlayerServerNames[0]));
                            uiInfo.numFoundPlayerServers++;
                        }
                        else {
                            // can't add any more so we're done
                            uiInfo.pendingServerStatus.num = uiInfo.serverStatus.numDisplayServers;
                        }
                    }
                }

                trap->SE_GetStringTextString("MENUS_SEARCHING", holdSPString, sizeof(holdSPString));
                trap->Cvar_Set("ui_playerServersFound", Q_Format(fmt::runtime(holdSPString), uiInfo.pendingServerStatus.num, numFound).c_str());
                //	Q_FormatTo(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers-1], "searching {}/{}...", uiInfo.pendingServerStatus.num, numFound);
                // retrieved the server status so reuse this spot
                uiInfo.pendingServerStatus.server[i].valid = false;
            }
        }
        // if empty pending slot or timed out
        if (!uiInfo.pendingServerStatus.server[i].valid ||
            uiInfo.pendingServerStatus.server[i].startTime < uiInfo.uiDC.realTime - ui_serverStatusTimeOut.integer) {
            if (uiInfo.pendingServerStatus.server[i].valid) {
                // timed out
            }
            // reset server status request for this address
            UI_GetServerStatusInfo(uiInfo.pendingServerStatus.server[i].adrstr, nullptr);
            // reuse pending slot
            uiInfo.pendingServerStatus.server[i].valid = false;
            // if we didn't try to get the status of all servers in the main browser yet
            if (uiInfo.pendingServerStatus.num < uiInfo.serverStatus.numDisplayServers) {
                uiInfo.pendingServerStatus.server[i].startTime = uiInfo.uiDC.realTime;
                lanSource = UI_SourceForLAN();
                trap->LAN_GetServerAddressString(lanSource, uiInfo.serverStatus.displayServers[uiInfo.pendingServerStatus.num],
                                                 uiInfo.pendingServerStatus.server[i].adrstr, sizeof(uiInfo.pendingServerStatus.server[i].adrstr));
                trap->LAN_GetServerInfo(lanSource, uiInfo.serverStatus.displayServers[uiInfo.pendingServerStatus.num], infoString, sizeof(infoString));
                Q_strncpyz(uiInfo.pendingServerStatus.server[i].name, Info_ValueForKey(infoString, "hostname"),
                           sizeof(uiInfo.pendingServerStatus.server[0].name));
                uiInfo.pendingServerStatus.server[i].valid = true;
                uiInfo.pendingServerStatus.num++;

                trap->SE_GetStringTextString("MENUS_SEARCHING", holdSPString, sizeof(holdSPString));
                trap->Cvar_Set("ui_playerServersFound", Q_Format(fmt::runtime(holdSPString), uiInfo.pendingServerStatus.num, numFound).c_str());

                //	Q_FormatTo(uiInfo.foundPlayerServerNames[uiInfo.numFoundPlayerServers-1], "searching {}/{}...", uiInfo.pendingServerStatus.num, numFound);
            }
        }
    }
    for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
        if (uiInfo.pendingServerStatus.server[i].valid) {
            break;
        }
    }
    // if still trying to retrieve server status info
    if (i < MAX_SERVERSTATUSREQUESTS) {
        uiInfo.nextFindPlayerRefresh = uiInfo.uiDC.realTime + 25;
    }
    else {
        trap->SE_GetStringTextString("MENUS_SERVERS_FOUNDWITH", holdSPString, sizeof(holdSPString));
        // add a line that shows the number of servers found
        if (!uiInfo.numFoundPlayerServers) {
            trap->Cvar_Set("ui_playerServersFound", Q_Format(fmt::runtime(holdSPString), 0, "s", uiInfo.findPlayerName).c_str());
        }
        else {
            trap->Cvar_Set("ui_playerServersFound", Q_Format(fmt::runtime(holdSPString), uiInfo.numFoundPlayerServers - 1,
                                                             uiInfo.numFoundPlayerServers == 2 ? "" : "s", uiInfo.findPlayerName)
                                                        .c_str());
        }
        uiInfo.nextFindPlayerRefresh = 0;
        // show the server status info for the selected server
        UI_FeederSelection(FEEDER_FINDPLAYER, uiInfo.currentFoundPlayerServer, nullptr);
    }
}

/*
==================
UI_BuildServerStatus
==================
*/
static void UI_BuildServerStatus(bool force)
{

    if (uiInfo.nextFindPlayerRefresh) {
        return;
    }
    if (!force) {
        if (!uiInfo.nextServerStatusRefresh || uiInfo.nextServerStatusRefresh > uiInfo.uiDC.realTime) {
            return;
        }
    }
    else {
        Menu_SetFeederSelection(nullptr, FEEDER_SERVERSTATUS, 0, nullptr);
        uiInfo.serverStatusInfo.numLines = 0;
        // reset all server status requests
        trap->LAN_ServerStatus(nullptr, nullptr, 0);
    }
    if (uiInfo.serverStatus.currentServer < 0 || uiInfo.serverStatus.currentServer > uiInfo.serverStatus.numDisplayServers ||
        uiInfo.serverStatus.numDisplayServers == 0) {
        return;
    }
    if (UI_GetServerStatusInfo(uiInfo.serverStatusAddress, &uiInfo.serverStatusInfo)) {
        uiInfo.nextServerStatusRefresh = 0;
        UI_GetServerStatusInfo(uiInfo.serverStatusAddress, nullptr);
    }
    else {
        uiInfo.nextServerStatusRefresh = uiInfo.uiDC.realTime + 500;
    }
}

/*
==================
UI_FeederCount
==================
*/
static int UI_FeederCount(float feederID)
{
    int count = 0, i;
    siegePlayerClassFlags_t baseClass;
    team_t team;
    static char info[MAX_STRING_CHARS];

    switch ((int)feederID) {
    case FEEDER_SABER_SINGLE_INFO:

        for (i = 0; i < MAX_SABER_HILTS; i++) {
            if (saberSingleHiltInfo[i]) {
                count++;
            }
            else { // done
                break;
            }
        }
        return count;

    case FEEDER_SABER_STAFF_INFO:

        for (i = 0; i < MAX_SABER_HILTS; i++) {
            if (saberStaffHiltInfo[i]) {
                count++;
            }
            else { // done
                break;
            }
        }
        return count;

    case FEEDER_Q3HEADS:
        return UI_HeadCountByColor();

    case FEEDER_SIEGE_TEAM1:
        if (!siegeTeam1) {
            UI_SetSiegeTeams();
            if (!siegeTeam1) {
                return 0;
            }
        }
        return siegeTeam1->numClasses;
    case FEEDER_SIEGE_TEAM2:
        if (!siegeTeam2) {
            UI_SetSiegeTeams();
            if (!siegeTeam2) {
                return 0;
            }
        }
        return siegeTeam2->numClasses;

    case FEEDER_FORCECFG:
        if (uiForceSide == FORCE_LIGHTSIDE) {
            return uiInfo.forceConfigCount - uiInfo.forceConfigLightIndexBegin;
        }
        else {
            return uiInfo.forceConfigLightIndexBegin + 1;
        }
        // return uiInfo.forceConfigCount;

    case FEEDER_CINEMATICS:
        return uiInfo.movieCount;

    case FEEDER_MAPS:
    case FEEDER_ALLMAPS:
        return UI_MapCountByGameType(feederID == FEEDER_MAPS ? true : false);

    case FEEDER_SERVERS:
        return uiInfo.serverStatus.numDisplayServers;

    case FEEDER_SERVERSTATUS:
        return Com_Clampi(0, MAX_SERVERSTATUS_LINES, uiInfo.serverStatusInfo.numLines);

    case FEEDER_FINDPLAYER:
        return uiInfo.numFoundPlayerServers;

    case FEEDER_PLAYER_LIST:
        if (uiInfo.uiDC.realTime > uiInfo.playerRefresh) {
            uiInfo.playerRefresh = uiInfo.uiDC.realTime + 3000;
            UI_BuildPlayerList();
        }
        return uiInfo.playerCount;

    case FEEDER_TEAM_LIST:
        if (uiInfo.uiDC.realTime > uiInfo.playerRefresh) {
            uiInfo.playerRefresh = uiInfo.uiDC.realTime + 3000;
            UI_BuildPlayerList();
        }
        return uiInfo.myTeamCount;

    case FEEDER_MODS:
        return uiInfo.modCount;

    case FEEDER_DEMOS:
        return uiInfo.demoCount;

    case FEEDER_MOVES:

        for (i = 0; i < MAX_MOVES; i++) {
            if (datapadMoveData[uiInfo.movesTitleIndex][i].title) {
                count++;
            }
        }

        return count;

    case FEEDER_MOVES_TITLES:
        return std::to_underlying(datapadMoveTitle_t::MD_MOVE_TITLE_MAX);

    case FEEDER_PLAYER_SPECIES:
        return uiInfo.playerSpeciesCount;

    case FEEDER_PLAYER_SKIN_HEAD:
        return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount;

    case FEEDER_PLAYER_SKIN_TORSO:
        return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount;

    case FEEDER_PLAYER_SKIN_LEGS:
        return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount;

    case FEEDER_COLORCHOICES:
        return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].ColorCount;

    case FEEDER_SIEGE_BASE_CLASS:
        team = UI_CvarTeam("ui_team");
        baseClass = UI_CvarSiegeClass();

        if ((team == SIEGETEAM_TEAM1) || (team == SIEGETEAM_TEAM2)) {
            // Is it a valid base class?
            if ((baseClass >= siegePlayerClassFlags_t::SPC_INFANTRY) && (baseClass < siegePlayerClassFlags_t::SPC_MAX)) {
                return (BG_SiegeCountBaseClass(team, baseClass));
            }
        }
        return 0;

    // Get the count of weapons
    case FEEDER_SIEGE_CLASS_WEAPONS:
        // count them up
        for (i = 0; i < std::to_underlying(weapon_t::WP_NUM_WEAPONS); i++) {
            trap->Cvar_VariableStringBuffer(Q_Format("ui_class_weapon{}", i).c_str(), info, sizeof(info));
            if (Q_stricmp(info, "gfx/2d/select") != 0) {
                count++;
            }
        }

        return count;

    // Get the count of inventory
    case FEEDER_SIEGE_CLASS_INVENTORY:
        // count them up
        for (holdable_t h = holdable_t::HI_NONE; h < holdable_t::HI_NUM_HOLDABLE; h++) {
            trap->Cvar_VariableStringBuffer(Q_Format("ui_class_item{}", h).c_str(), info, sizeof(info));
            // A hack so health and ammo dispenser icons don't show up.
            if ((Q_stricmp(info, "gfx/2d/select") != 0) && (Q_stricmp(info, "gfx/hud/i_icon_healthdisp") != 0) &&
                (Q_stricmp(info, "gfx/hud/i_icon_ammodisp") != 0)) {
                count++;
            }
        }
        return count;

    // Get the count of force powers
    case FEEDER_SIEGE_CLASS_FORCE:
        // count them up
        for (i = 0; i < std::to_underlying(forcePowers_t::NUM_FORCE_POWERS); i++) {
            trap->Cvar_VariableStringBuffer(Q_Format("ui_class_power{}", i).c_str(), info, sizeof(info));
            if (Q_stricmp(info, "gfx/2d/select") != 0) {
                count++;
            }
        }
        return count;
    }

    return 0;
}

static const char* UI_SelectedMap(int index, int* actual)
{
    int i, c;
    c = 0;
    *actual = 0;

    for (i = 0; i < uiInfo.mapCount; i++) {
        if (uiInfo.mapList[i].active) {
            if (c == index) {
                *actual = i;
                return uiInfo.mapList[i].mapName;
            }
            else {
                c++;
            }
        }
    }
    return "";
}

/*
==================
UI_HeadCountByColor
==================
*/
static const char* UI_SelectedTeamHead(int index, int* actual)
{
    const char* teamname;
    int i, c = 0;

    switch (uiSkinColor) {
    case team_t::TEAM_BLUE:
        teamname = "/blue";
        break;
    case team_t::TEAM_RED:
        teamname = "/red";
        break;
    default:
        teamname = "/default";
        break;
    }

    // Count each head with this color

    for (i = 0; i < uiInfo.q3HeadCount; i++) {
        if (uiInfo.q3HeadNames[i][0] && strstr(uiInfo.q3HeadNames[i], teamname)) {
            if (c == index) {
                *actual = i;
                return uiInfo.q3HeadNames[i];
            }
            else {
                c++;
            }
        }
    }
    return "";
}

static int UI_GetIndexFromSelection(int actual)
{
    int i, c;
    c = 0;
    for (i = 0; i < uiInfo.mapCount; i++) {
        if (uiInfo.mapList[i].active) {
            if (i == actual) {
                return c;
            }
            c++;
        }
    }
    return 0;
}

static void UI_UpdatePendingPings()
{
    trap->LAN_ResetPings(UI_SourceForLAN());
    uiInfo.serverStatus.refreshActive = true;
    uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime + 1000;
}

static const char* UI_FeederItemText(float feederID, int index, int column, qhandle_t* handle1, qhandle_t* handle2, qhandle_t* handle3)
{
    static char info[MAX_STRING_CHARS]; // don't change this size without changing the sizes inside the SaberProperName calls
    static char hostname[MAX_HOSTNAMELENGTH] = {0};
    static char clientBuff[32];
    static char needPass[32];
    static int lastColumn = -1;
    static int lastTime = 0;
    *handle1 = *handle2 = *handle3 = -1;

    if (feederID == FEEDER_SABER_SINGLE_INFO) {
        // char *saberProperName=0;
        UI_SaberProperNameForSaber(saberSingleHiltInfo[index], info);
        return info;
    }
    else if (feederID == FEEDER_SABER_STAFF_INFO) {
        // char *saberProperName=0;
        UI_SaberProperNameForSaber(saberStaffHiltInfo[index], info);
        return info;
    }
    else if (feederID == FEEDER_Q3HEADS) {
        int actual;
        return UI_SelectedTeamHead(index, &actual);
    }
    else if (feederID == FEEDER_SIEGE_TEAM1) {
        return ""; // nothing I guess, the description part can cover this
        /*
        if (!siegeTeam1)
        {
            UI_SetSiegeTeams();
            if (!siegeTeam1)
            {
                return "";
            }
        }

        if (siegeTeam1->classes[index])
        {
            return siegeTeam1->classes[index]->name;
        }
        return "";
        */
    }
    else if (feederID == FEEDER_SIEGE_TEAM2) {
        return ""; // nothing I guess, the description part can cover this
        /*
        if (!siegeTeam1)
        {
            UI_SetSiegeTeams();
            if (!siegeTeam1)
            {
                return "";
            }
        }

        if (siegeTeam2->classes[index])
        {
            return siegeTeam2->classes[index]->name;
        }
        return "";
        */
    }
    else if (feederID == FEEDER_FORCECFG) {
        if (index >= 0 && index < uiInfo.forceConfigCount) {
            if (index == 0) { // always show "custom"
                return uiInfo.forceConfigNames[index];
            }
            else {
                if (uiForceSide == FORCE_LIGHTSIDE) {
                    index += uiInfo.forceConfigLightIndexBegin;
                    if (index < 0) {
                        return nullptr;
                    }
                    if (index >= uiInfo.forceConfigCount) {
                        return nullptr;
                    }
                    return uiInfo.forceConfigNames[index];
                }
                else if (uiForceSide == FORCE_DARKSIDE) {
                    index += uiInfo.forceConfigDarkIndexBegin;
                    if (index < 0) {
                        return nullptr;
                    }
                    if (index > uiInfo.forceConfigLightIndexBegin) { // dark gets read in before light
                        return nullptr;
                    }
                    if (index >= uiInfo.forceConfigCount) {
                        return nullptr;
                    }
                    return uiInfo.forceConfigNames[index];
                }
                else {
                    return nullptr;
                }
            }
        }
    }
    else if (feederID == FEEDER_MAPS || feederID == FEEDER_ALLMAPS) {
        int actual;
        return UI_SelectedMap(index, &actual);
    }
    else if (feederID == FEEDER_SERVERS) {
        if (index >= 0 && index < uiInfo.serverStatus.numDisplayServers) {
            int game;
            bool answered;
            if (lastColumn != column || lastTime > uiInfo.uiDC.realTime + 5000) {
                trap->LAN_GetServerInfo(UI_SourceForLAN(), uiInfo.serverStatus.displayServers[index], info, MAX_STRING_CHARS);
                lastColumn = column;
                lastTime = uiInfo.uiDC.realTime;
            }
            answered = (trap->ext.LAN_GetServerState(UI_SourceForLAN(), uiInfo.serverStatus.displayServers[index]) == lanServerState_t::LANSERVER_ANSWERED);
            switch (static_cast<serverSortKey_t>(column)) { // the feeder's column in the menu file
            case serverSortKey_t::SORT_HOST:
                if (!answered) {
                    return Info_ValueForKey(info, "addr");
                }
                else {
                    gametype_t gametype = static_cast<gametype_t>(atoi(Info_ValueForKey(info, "gametype")));
                    // check for password
                    if (atoi(Info_ValueForKey(info, "needpass"))) {
                        *handle3 = uiInfo.uiDC.Assets.needPass;
                    }
                    // check for saberonly and restricted force powers
                    if (gametype != gametype_t::GT_JEDIMASTER) {
                        bool saberOnly = true;
                        int restrictedForce = 0; // fdisable, a mask of force powers
                        bool allForceDisabled = false;
                        int wDisable;
                        weapon_t i = weapon_t::WP_NONE;

                        // check force
                        restrictedForce = atoi(Info_ValueForKey(info, "fdisable"));
                        if (UI_AllForceDisabled(restrictedForce)) { // all force powers are disabled
                            allForceDisabled = true;
                            *handle2 = uiInfo.uiDC.Assets.noForce;
                        }
                        else if (restrictedForce) { // at least one force power is disabled
                            *handle2 = uiInfo.uiDC.Assets.forceRestrict;
                        }

                        // check weaps
                        wDisable = atoi(Info_ValueForKey(info, "wdisable"));

                        while (i < weapon_t::WP_NUM_WEAPONS) {
                            if (!(wDisable & Bit(i)) && i != weapon_t::WP_SABER && i != weapon_t::WP_NONE) {
                                saberOnly = false;
                            }

                            i++;
                        }
                        if (saberOnly) {
                            *handle1 = uiInfo.uiDC.Assets.saberOnly;
                        }
                        else if (atoi(Info_ValueForKey(info, "truejedi")) != 0) {
                            if (gametype != gametype_t::GT_HOLOCRON && gametype != gametype_t::GT_JEDIMASTER && !saberOnly &&
                                !allForceDisabled) { // truejedi is on and allowed in this mode
                                *handle1 = uiInfo.uiDC.Assets.trueJedi;
                            }
                        }
                    }
                    if (UI_NetSource() == uiNetSource_t::UIAS_LOCAL) {
                        int nettype = atoi(Info_ValueForKey(info, "nettype"));

                        if (nettype < 0 || nettype >= numNetNames) {
                            nettype = 0;
                        }

                        Q_FormatTo(hostname, "{} [{}]", Info_ValueForKey(info, "hostname"), netNames[nettype]);
                        return hostname;
                    }
                    else {
                        if (atoi(Info_ValueForKey(info, "sv_allowAnonymous")) != 0) { // anonymous server
                            Q_FormatTo(hostname, "(A) {}", Info_ValueForKey(info, "hostname"));
                        }
                        else {
                            Q_FormatTo(hostname, "{}", Info_ValueForKey(info, "hostname"));
                        }
                        return hostname;
                    }
                }
            case serverSortKey_t::SORT_MAP:
                return Info_ValueForKey(info, "mapname");
            case serverSortKey_t::SORT_CLIENTS:
                Q_FormatTo(clientBuff, "{} ({})", Info_ValueForKey(info, "clients"), Info_ValueForKey(info, "sv_maxclients"));
                return clientBuff;
            case serverSortKey_t::SORT_GAME:
                game = atoi(Info_ValueForKey(info, "gametype"));
                if (game >= 0 && game < numGameTypes) {
                    Q_strncpyz(needPass, gameTypes[static_cast<gametype_t>(game)], sizeof(needPass));
                }
                else {
                    if (!answered)
                        Q_strncpyz(needPass, "Inactive", sizeof(needPass));
                    Q_strncpyz(needPass, "Unknown", sizeof(needPass));
                }

                return needPass;
            case serverSortKey_t::SORT_PING:
                if (!answered) {
                    return "...";
                }
                else {
                    return Info_ValueForKey(info, "ping");
                }
            }
        }
    }
    else if (feederID == FEEDER_SERVERSTATUS) {
        if (index >= 0 && index < uiInfo.serverStatusInfo.numLines) {
            if (column >= 0 && column < 4) {
                return uiInfo.serverStatusInfo.lines[index][column];
            }
        }
    }
    else if (feederID == FEEDER_FINDPLAYER) {
        if (index >= 0 && index < uiInfo.numFoundPlayerServers) {
            // return uiInfo.foundPlayerServerAddresses[index];
            return uiInfo.foundPlayerServerNames[index];
        }
    }
    else if (feederID == FEEDER_PLAYER_LIST) {
        if (index >= 0 && index < uiInfo.playerCount) {
            return uiInfo.playerNames[index];
        }
    }
    else if (feederID == FEEDER_TEAM_LIST) {
        if (index >= 0 && index < uiInfo.myTeamCount) {
            return uiInfo.teamNames[index];
        }
    }
    else if (feederID == FEEDER_MODS) {
        if (index >= 0 && index < uiInfo.modCount) {
            if (uiInfo.modList[index].modDescr && *uiInfo.modList[index].modDescr) {
                return uiInfo.modList[index].modDescr;
            }
            else {
                return uiInfo.modList[index].modName;
            }
        }
    }
    else if (feederID == FEEDER_CINEMATICS) {
        if (index >= 0 && index < uiInfo.movieCount) {
            return uiInfo.movieList[index];
        }
    }
    else if (feederID == FEEDER_DEMOS) {
        if (index >= 0 && index < uiInfo.demoCount) {
            return uiInfo.demoList[index];
        }
    }
    else if (feederID == FEEDER_MOVES) {
        return datapadMoveData[uiInfo.movesTitleIndex][index].title;
    }
    else if (feederID == FEEDER_MOVES_TITLES) {
        return datapadMoveTitleData[static_cast<datapadMoveTitle_t>(index)];
    }
    else if (feederID == FEEDER_PLAYER_SPECIES) {
        if (index >= 0 && index < uiInfo.playerSpeciesCount) {
            return uiInfo.playerSpecies[index].Name;
        }
    }
    else if (feederID == FEEDER_LANGUAGES) {
        return nullptr;
    }
    else if (feederID == FEEDER_COLORCHOICES) {
        if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].ColorCount) {
            *handle1 = trap->R_RegisterShaderNoMip(uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Color[index].shader);
            return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Color[index].shader;
        }
    }
    else if (feederID == FEEDER_PLAYER_SKIN_HEAD) {
        if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount) {
            *handle1 = trap->R_RegisterShaderNoMip(Q_Format("models/players/{}/icon_{}", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
                                                            uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHead[index].name)
                                                       .c_str());
            return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHead[index].name;
        }
    }
    else if (feederID == FEEDER_PLAYER_SKIN_TORSO) {
        if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount) {
            *handle1 = trap->R_RegisterShaderNoMip(Q_Format("models/players/{}/icon_{}", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
                                                            uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorso[index].name)
                                                       .c_str());
            return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorso[index].name;
        }
    }
    else if (feederID == FEEDER_PLAYER_SKIN_LEGS) {
        if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount) {
            *handle1 = trap->R_RegisterShaderNoMip(Q_Format("models/players/{}/icon_{}", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
                                                            uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLeg[index].name)
                                                       .c_str());
            return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLeg[index].name;
        }
    }
    else if (feederID == FEEDER_SIEGE_BASE_CLASS) {
        return "";
    }
    else if (feederID == FEEDER_SIEGE_CLASS_WEAPONS) {
        return "";
    }
    return "";
}

static qhandle_t UI_FeederItemImage(float feederID, int index)
{
    int validCnt, i;
    static char info[MAX_STRING_CHARS];

    if (feederID == FEEDER_SABER_SINGLE_INFO) {
        return 0;
    }
    else if (feederID == FEEDER_SABER_STAFF_INFO) {
        return 0;
    }
    else if (feederID == FEEDER_Q3HEADS) {
        int actual = 0;
        UI_SelectedTeamHead(index, &actual);
        index = actual;

        if (index >= 0 && index < uiInfo.q3HeadCount) { // we want it to load them as it draws them, like the TA feeder
                                                        // return uiInfo.q3HeadIcons[index];
            int selModel = trap->Cvar_VariableValue("ui_selectedModelIndex");

            if (selModel != -1) {
                if (uiInfo.q3SelectedHead != selModel) {
                    uiInfo.q3SelectedHead = selModel;
                    // UI_FeederSelection(FEEDER_Q3HEADS, uiInfo.q3SelectedHead);
                    Menu_SetFeederSelection(nullptr, FEEDER_Q3HEADS, selModel, nullptr);
                }
            }

            if (!uiInfo.q3HeadIcons[index]) { // this isn't the best way of doing this I guess, but I didn't want a whole seperate string array
                                              // for storing shader names. I can't just replace q3HeadNames with the shader name, because we
                                              // print what's in q3HeadNames and the icon name would look funny.
                char iconNameFromSkinName[256];
                int i = 0;
                int skinPlace;

                i = strlen(uiInfo.q3HeadNames[index]);

                while (uiInfo.q3HeadNames[index][i] != '/') {
                    i--;
                }

                i++;
                skinPlace = i; // remember that this is where the skin name begins

                // now, build a full path out of what's in q3HeadNames, into iconNameFromSkinName
                Q_FormatTo(iconNameFromSkinName, "models/players/{}", uiInfo.q3HeadNames[index]);

                i = strlen(iconNameFromSkinName);

                while (iconNameFromSkinName[i] != '/') {
                    i--;
                }

                i++;
                iconNameFromSkinName[i] = 0; // terminate, and append..
                Q_strcat(iconNameFromSkinName, 256, "icon_");

                // and now, for the final step, append the skin name from q3HeadNames onto the end of iconNameFromSkinName
                i = strlen(iconNameFromSkinName);

                while (uiInfo.q3HeadNames[index][skinPlace]) {
                    iconNameFromSkinName[i] = uiInfo.q3HeadNames[index][skinPlace];
                    i++;
                    skinPlace++;
                }
                iconNameFromSkinName[i] = 0;

                // and now we are ready to register (thankfully this will only happen once)
                uiInfo.q3HeadIcons[index] = trap->R_RegisterShaderNoMip(iconNameFromSkinName);
            }
            return uiInfo.q3HeadIcons[index];
        }
    }
    else if (feederID == FEEDER_SIEGE_TEAM1) {
        if (!siegeTeam1) {
            UI_SetSiegeTeams();
            if (!siegeTeam1) {
                return 0;
            }
        }

        if (siegeTeam1->classes[index]) {
            return siegeTeam1->classes[index]->uiPortraitShader;
        }
        return 0;
    }
    else if (feederID == FEEDER_SIEGE_TEAM2) {
        if (!siegeTeam2) {
            UI_SetSiegeTeams();
            if (!siegeTeam2) {
                return 0;
            }
        }

        if (siegeTeam2->classes[index]) {
            return siegeTeam2->classes[index]->uiPortraitShader;
        }
        return 0;
    }
    else if (feederID == FEEDER_ALLMAPS || feederID == FEEDER_MAPS) {
        int actual;
        UI_SelectedMap(index, &actual);
        index = actual;
        if (index >= 0 && index < uiInfo.mapCount) {
            if (uiInfo.mapList[index].levelShot == -1) {
                uiInfo.mapList[index].levelShot = trap->R_RegisterShaderNoMip(uiInfo.mapList[index].imageName);
            }
            return uiInfo.mapList[index].levelShot;
        }
    }
    else if (feederID == FEEDER_PLAYER_SKIN_HEAD) {
        if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount) {
            // return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadIcons[index];
            return trap->R_RegisterShaderNoMip(Q_Format("models/players/{}/icon_{}", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
                                                        uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHead[index].name)
                                                   .c_str());
        }
    }
    else if (feederID == FEEDER_PLAYER_SKIN_TORSO) {
        if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount) {
            // return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoIcons[index];
            return trap->R_RegisterShaderNoMip(Q_Format("models/players/{}/icon_{}", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
                                                        uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorso[index].name)
                                                   .c_str());
        }
    }
    else if (feederID == FEEDER_PLAYER_SKIN_LEGS) {
        if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount) {
            // return uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegIcons[index];
            return trap->R_RegisterShaderNoMip(Q_Format("models/players/{}/icon_{}", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Name,
                                                        uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLeg[index].name)
                                                   .c_str());
        }
    }
    else if (feederID == FEEDER_COLORCHOICES) {
        if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].ColorCount) {
            return trap->R_RegisterShaderNoMip(uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Color[index].shader);
        }
    }

    else if (feederID == FEEDER_SIEGE_BASE_CLASS) {
        siegePlayerClassFlags_t baseClass;
        team_t team;

        team = UI_CvarTeam("ui_team");
        baseClass = UI_CvarSiegeClass();

        if ((team == SIEGETEAM_TEAM1) || (team == SIEGETEAM_TEAM2)) {
            // Is it a valid base class?
            if ((baseClass >= siegePlayerClassFlags_t::SPC_INFANTRY) && (baseClass < siegePlayerClassFlags_t::SPC_MAX)) {
                if (index >= 0) {
                    return (BG_GetUIPortrait(team, baseClass, index));
                }
            }
        }
    }
    else if (feederID == FEEDER_SIEGE_CLASS_WEAPONS) {
        validCnt = 0;
        // count them up
        for (i = 0; i < std::to_underlying(weapon_t::WP_NUM_WEAPONS); i++) {
            trap->Cvar_VariableStringBuffer(Q_Format("ui_class_weapon{}", i).c_str(), info, sizeof(info));
            if (Q_stricmp(info, "gfx/2d/select") != 0) {
                if (validCnt == index) {
                    return (trap->R_RegisterShaderNoMip(info));
                }
                validCnt++;
            }
        }
    }
    else if (feederID == FEEDER_SIEGE_CLASS_INVENTORY) {
        validCnt = 0;
        // count them up
        for (holdable_t h = holdable_t::HI_NONE; h < holdable_t::HI_NUM_HOLDABLE; h++) {
            trap->Cvar_VariableStringBuffer(Q_Format("ui_class_item{}", h).c_str(), info, sizeof(info));
            // A hack so health and ammo dispenser icons don't show up.
            if ((Q_stricmp(info, "gfx/2d/select") != 0) && (Q_stricmp(info, "gfx/hud/i_icon_healthdisp") != 0) &&
                (Q_stricmp(info, "gfx/hud/i_icon_ammodisp") != 0)) {
                if (validCnt == index) {
                    return (trap->R_RegisterShaderNoMip(info));
                }
                validCnt++;
            }
        }
    }
    else if (feederID == FEEDER_SIEGE_CLASS_FORCE) {
        int slotI = 0;
        static char info2[MAX_STRING_CHARS];
        menuDef_t* menu;
        itemDef_t* item;

        validCnt = 0;

        menu = Menu_GetFocused(); // Get current menu
        if (menu) {
            item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "base_class_force_feed");
            if (item) {
                listBoxDef_t* listPtr = item->typeData.listbox;
                if (listPtr) {
                    slotI = listPtr->startPos;
                }
            }
        }

        // count them up
        for (i = 0; i < std::to_underlying(forcePowers_t::NUM_FORCE_POWERS); i++) {
            trap->Cvar_VariableStringBuffer(Q_Format("ui_class_power{}", i).c_str(), info, sizeof(info));
            if (Q_stricmp(info, "gfx/2d/select") != 0) {
                if (validCnt == index) {
                    trap->Cvar_VariableStringBuffer(Q_Format("ui_class_powerlevel{}", validCnt).c_str(), info2, sizeof(info2));

                    trap->Cvar_Set(Q_Format("ui_class_powerlevelslot{}", index - slotI).c_str(), info2);
                    return (trap->R_RegisterShaderNoMip(info));
                }
                validCnt++;
            }
        }
    }

    return 0;
}

bool UI_FeederSelection(float feederFloat, int index, itemDef_t* item)
{
    static char info[MAX_STRING_CHARS];
    const int feederID = feederFloat;

    if (feederID == FEEDER_Q3HEADS) {
        int actual = 0;
        UI_SelectedTeamHead(index, &actual);
        uiInfo.q3SelectedHead = index;
        trap->Cvar_Set("ui_selectedModelIndex", Q_Format("{}", index).c_str());
        index = actual;
        if (index >= 0 && index < uiInfo.q3HeadCount) {
            trap->Cvar_Set("model", uiInfo.q3HeadNames[index]); // standard model
            trap->Cvar_Set("char_color_red", "255");            // standard colors
            trap->Cvar_Set("char_color_green", "255");
            trap->Cvar_Set("char_color_blue", "255");
        }
    }
    else if (feederID == FEEDER_MOVES) {
        itemDef_t* item;
        menuDef_t* menu;
        modelDef_t* modelPtr;

        menu = Menus_FindByName("rulesMenu_moves");

        if (menu) {
            item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "character");
            if (item) {
                modelPtr = item->typeData.model;
                if (modelPtr) {
                    char modelPath[MAX_QPATH];
                    int animRunLength;

                    ItemParse_model_g2anim_go(item, datapadMoveData[uiInfo.movesTitleIndex][index].anim);

                    Q_FormatTo(modelPath, "models/players/{}/model.glm", UI_Cvar_VariableString("ui_char_model"));
                    ItemParse_asset_model_go(item, modelPath, &animRunLength);
                    UI_UpdateCharacterSkin();

                    uiInfo.moveAnimTime = uiInfo.uiDC.realTime + animRunLength;

                    if (datapadMoveData[uiInfo.movesTitleIndex][index].anim) {

                        // Play sound for anim
                        if (datapadMoveData[uiInfo.movesTitleIndex][index].sound == datapadMoveSound_t::MDS_FORCE_JUMP) {
                            Menu_PlaySound(uiInfo.uiDC.Assets.moveJumpSound, sound::Slot::Menu, 0.75f);
                        }
                        else if (datapadMoveData[uiInfo.movesTitleIndex][index].sound == datapadMoveSound_t::MDS_ROLL) {
                            Menu_PlaySound(uiInfo.uiDC.Assets.moveRollSound, sound::Slot::Menu, 0.75f);
                        }
                        else if (datapadMoveData[uiInfo.movesTitleIndex][index].sound == datapadMoveSound_t::MDS_SABER) {
                            // Randomly choose one sound
                            int soundI = Q_irand(1, 6);
                            sfxHandle_t* soundPtr;
                            soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound1;
                            if (soundI == 2) {
                                soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound2;
                            }
                            else if (soundI == 3) {
                                soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound3;
                            }
                            else if (soundI == 4) {
                                soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound4;
                            }
                            else if (soundI == 5) {
                                soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound5;
                            }
                            else if (soundI == 6) {
                                soundPtr = &uiInfo.uiDC.Assets.datapadmoveSaberSound6;
                            }

                            Menu_PlaySound(*soundPtr, sound::Slot::Menu, 0.75f);
                        }

                        if (datapadMoveData[uiInfo.movesTitleIndex][index].desc) {
                            trap->Cvar_Set("ui_move_desc", datapadMoveData[uiInfo.movesTitleIndex][index].desc);
                        }
                    }
                    UI_SaberAttachToChar(item);
                }
            }
        }
    }
    else if (feederID == FEEDER_MOVES_TITLES) {
        itemDef_t* item;
        menuDef_t* menu;
        modelDef_t* modelPtr;

        uiInfo.movesTitleIndex = static_cast<datapadMoveTitle_t>(index);
        uiInfo.movesBaseAnim = datapadMoveTitleBaseAnims[uiInfo.movesTitleIndex];
        menu = Menus_FindByName("rulesMenu_moves");

        if (menu) {
            item = (itemDef_t*)Menu_FindItemByName((menuDef_t*)menu, "character");
            if (item) {
                modelPtr = item->typeData.model;
                if (modelPtr) {
                    char modelPath[MAX_QPATH];
                    int animRunLength;

                    uiInfo.movesBaseAnim = datapadMoveTitleBaseAnims[uiInfo.movesTitleIndex];
                    ItemParse_model_g2anim_go(item, uiInfo.movesBaseAnim);

                    Q_FormatTo(modelPath, "models/players/{}/model.glm", UI_Cvar_VariableString("ui_char_model"));
                    ItemParse_asset_model_go(item, modelPath, &animRunLength);

                    UI_UpdateCharacterSkin();
                }
            }
        }
    }
    else if (feederID == FEEDER_SIEGE_TEAM1) {
        if (!g_siegedFeederForcedSet) {
            g_UIGloballySelectedSiegeClass = UI_SiegeClassNum(siegeTeam1->classes[index]);
            trap->Cvar_Set("ui_classDesc", g_UIClassDescriptions[g_UIGloballySelectedSiegeClass].desc);

            // g_siegedFeederForcedSet = 1;
            // Menu_SetFeederSelection(nullptr, FEEDER_SIEGE_TEAM2, -1, nullptr);

            UI_SiegeSetCvarsForClass(siegeTeam1->classes[index]);
        }
        g_siegedFeederForcedSet = 0;
    }
    else if (feederID == FEEDER_SIEGE_TEAM2) {
        if (!g_siegedFeederForcedSet) {
            g_UIGloballySelectedSiegeClass = UI_SiegeClassNum(siegeTeam2->classes[index]);
            trap->Cvar_Set("ui_classDesc", g_UIClassDescriptions[g_UIGloballySelectedSiegeClass].desc);

            // g_siegedFeederForcedSet = 1;
            // Menu_SetFeederSelection(nullptr, FEEDER_SIEGE_TEAM2, -1, nullptr);

            UI_SiegeSetCvarsForClass(siegeTeam2->classes[index]);
        }
        g_siegedFeederForcedSet = 0;
    }
    else if (feederID == FEEDER_FORCECFG) {
        int newindex = index;

        if (uiForceSide == FORCE_LIGHTSIDE) {
            newindex += uiInfo.forceConfigLightIndexBegin;
            if (newindex >= uiInfo.forceConfigCount) {
                return false;
            }
        }
        else { // else dark
            newindex += uiInfo.forceConfigDarkIndexBegin;
            if (newindex >= uiInfo.forceConfigCount || newindex > uiInfo.forceConfigLightIndexBegin) { // dark gets read in before light
                return false;
            }
        }

        if (index >= 0 && index < uiInfo.forceConfigCount) {
            UI_ForceConfigHandle(uiInfo.forceConfigSelected, index);
            uiInfo.forceConfigSelected = index;
        }
    }
    else if (feederID == FEEDER_MAPS || feederID == FEEDER_ALLMAPS) {
        int actual, map;
        const char* checkValid = nullptr;

        map = (feederID == FEEDER_ALLMAPS) ? ui_currentNetMap.integer : ui_currentMap.integer;
        if (uiInfo.mapList[map].cinematic >= 0) {
            trap->CIN_StopCinematic(uiInfo.mapList[map].cinematic);
            uiInfo.mapList[map].cinematic = -1;
        }
        checkValid = UI_SelectedMap(index, &actual);

        if (!checkValid || !checkValid[0]) { // this isn't a valid map to select, so reselect the current
            index = ui_mapIndex.integer;
            UI_SelectedMap(index, &actual);
        }

        trap->Cvar_Set("ui_mapIndex", Q_Format("{}", index).c_str());
        gUISelectedMap = index;
        ui_mapIndex.integer = index;

        if (feederID == FEEDER_MAPS) {
            trap->Cvar_Set("ui_currentMap", Q_Format("{}", actual).c_str());
            trap->Cvar_Update(&ui_currentMap);
            uiInfo.mapList[ui_currentMap.integer].cinematic =
                trap->CIN_PlayCinematic(Q_Format("{}.roq", uiInfo.mapList[ui_currentMap.integer].mapLoadName).c_str(), 0, 0, 0, 0,
                                        (cinematicFlags_t::CIN_loop | cinematicFlags_t::CIN_silent));
            // trap->Cvar_Set("ui_opponentModel", uiInfo.mapList[ui_currentMap.integer].opponentName);
            // updateOpponentModel = true;
        }
        else {
            trap->Cvar_Set("ui_currentNetMap", Q_Format("{}", actual).c_str());
            trap->Cvar_Update(&ui_currentNetMap);
            uiInfo.mapList[ui_currentNetMap.integer].cinematic =
                trap->CIN_PlayCinematic(Q_Format("{}.roq", uiInfo.mapList[ui_currentNetMap.integer].mapLoadName).c_str(), 0, 0, 0, 0,
                                        (cinematicFlags_t::CIN_loop | cinematicFlags_t::CIN_silent));
        }
    }
    else if (feederID == FEEDER_SERVERS) {
        const char* mapName = nullptr;
        uiInfo.serverStatus.currentServer = index;
        trap->LAN_GetServerInfo(UI_SourceForLAN(), uiInfo.serverStatus.displayServers[index], info, MAX_STRING_CHARS);
        uiInfo.serverStatus.currentServerPreview = trap->R_RegisterShaderNoMip(Q_Format("levelshots/{}", Info_ValueForKey(info, "mapname")).c_str());
        if (uiInfo.serverStatus.currentServerCinematic >= 0) {
            trap->CIN_StopCinematic(uiInfo.serverStatus.currentServerCinematic);
            uiInfo.serverStatus.currentServerCinematic = -1;
        }
        mapName = Info_ValueForKey(info, "mapname");
        if (mapName && *mapName) {
            uiInfo.serverStatus.currentServerCinematic =
                trap->CIN_PlayCinematic(Q_Format("{}.roq", mapName).c_str(), 0, 0, 0, 0, (cinematicFlags_t::CIN_loop | cinematicFlags_t::CIN_silent));
        }
    }
    else if (feederID == FEEDER_SERVERSTATUS) {
        //
    }
    else if (feederID == FEEDER_FINDPLAYER) {
        uiInfo.currentFoundPlayerServer = index;
        //
        if (index < uiInfo.numFoundPlayerServers - 1) {
            // build a new server status for this server
            Q_strncpyz(uiInfo.serverStatusAddress, uiInfo.foundPlayerServerAddresses[uiInfo.currentFoundPlayerServer], sizeof(uiInfo.serverStatusAddress));
            Menu_SetFeederSelection(nullptr, FEEDER_SERVERSTATUS, 0, nullptr);
            UI_BuildServerStatus(true);
        }
    }
    else if (feederID == FEEDER_PLAYER_LIST) {
        uiInfo.playerIndex = index;
    }
    else if (feederID == FEEDER_TEAM_LIST) {
        uiInfo.teamIndex = index;
    }
    else if (feederID == FEEDER_MODS) {
        uiInfo.modIndex = index;
    }
    else if (feederID == FEEDER_CINEMATICS) {
        uiInfo.movieIndex = index;
        if (uiInfo.previewMovie >= 0) {
            trap->CIN_StopCinematic(uiInfo.previewMovie);
        }
        uiInfo.previewMovie = -1;
    }
    else if (feederID == FEEDER_DEMOS) {
        uiInfo.demoIndex = index;
    }
    else if (feederID == FEEDER_COLORCHOICES) {
        if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].ColorCount) {
            Item_RunScript(item, uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].Color[index].actionText);
        }
    }
    else if (feederID == FEEDER_PLAYER_SKIN_HEAD) {
        if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHeadCount) {
            trap->Cvar_Set("ui_char_skin_head", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinHead[index].name);
        }
    }
    else if (feederID == FEEDER_PLAYER_SKIN_TORSO) {
        if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorsoCount) {
            trap->Cvar_Set("ui_char_skin_torso", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinTorso[index].name);
        }
    }
    else if (feederID == FEEDER_PLAYER_SKIN_LEGS) {
        if (index >= 0 && index < uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLegCount) {
            trap->Cvar_Set("ui_char_skin_legs", uiInfo.playerSpecies[uiInfo.playerSpeciesIndex].SkinLeg[index].name);
        }
    }
    else if (feederID == FEEDER_PLAYER_SPECIES) {
        if (index >= 0 && index < uiInfo.playerSpeciesCount) {
            uiInfo.playerSpeciesIndex = index;
        }
    }
    else if (feederID == FEEDER_LANGUAGES) {
        uiInfo.languageCountIndex = index;
    }
    else if (feederID == FEEDER_SIEGE_BASE_CLASS) {
        siegePlayerClassFlags_t baseClass;
        team_t team;

        team = UI_CvarTeam("ui_team");
        baseClass = UI_CvarSiegeClass();

        UI_UpdateCvarsForClass(team, baseClass, index);
    }
    else if (feederID == FEEDER_SIEGE_CLASS_WEAPONS) {
        //		trap->Cvar_VariableStringBuffer( Q_Format("ui_class_weapondesc{}", index).c_str(), info, sizeof(info) );
        //		trap->Cvar_Set( "ui_itemforceinvdesc", info );
    }
    else if (feederID == FEEDER_SIEGE_CLASS_INVENTORY) {
        //		trap->Cvar_VariableStringBuffer( Q_Format("ui_class_itemdesc{}", index).c_str(), info, sizeof(info) );
        //		trap->Cvar_Set( "ui_itemforceinvdesc", info );
    }
    else if (feederID == FEEDER_SIEGE_CLASS_FORCE) {
        //		int validCnt = 0;

        trap->Cvar_VariableStringBuffer(Q_Format("ui_class_power{}", index).c_str(), info, sizeof(info));

        // count them up
        for (forcePowers_t fp = forcePowers_t::FP_FIRST; fp < forcePowers_t::NUM_FORCE_POWERS; fp++) {
            if (!strcmp(HolocronIcons[fp], info)) {
                trap->Cvar_Set("ui_itemforceinvdesc", forcepowerDesc[fp]);
            }
        }
    }
    return true;
}

static bool GameType_Parse(const char** p, bool join)
{
    char* token;

    token = COM_ParseExt((const char**)p, true);

    if (token[0] != '{') {
        return false;
    }

    if (join) {
        uiInfo.numJoinGameTypes = 0;
    }
    else {
        uiInfo.numGameTypes = 0;
    }

    while (1) {
        token = COM_ParseExt((const char**)p, true);

        if (Q_stricmp(token, "}") == 0) {
            return true;
        }

        if (!token || token[0] == 0) {
            return false;
        }

        if (token[0] == '{') {
            // two tokens per line, character name and sex
            if (join) {
                int gtEnum;
                if (!String_Parse(p, &uiInfo.joinGameTypes[uiInfo.numJoinGameTypes].gameType) || !Int_Parse(p, &gtEnum)) {
                    return false;
                }
                uiInfo.joinGameTypes[uiInfo.numJoinGameTypes].gtEnum = static_cast<gametype_t>(gtEnum);
            }
            else {
                int gtEnum;
                if (!String_Parse(p, &uiInfo.gameTypes[uiInfo.numGameTypes].gameType) || !Int_Parse(p, &gtEnum)) {
                    return false;
                }
                uiInfo.gameTypes[uiInfo.numGameTypes].gtEnum = static_cast<gametype_t>(gtEnum);
            }

            if (join) {
                if (uiInfo.numJoinGameTypes < MAX_GAMETYPES) {
                    uiInfo.numJoinGameTypes++;
                }
                else {
                    Q_Println("Too many net game types, last one replace!");
                }
            }
            else {
                if (uiInfo.numGameTypes < MAX_GAMETYPES) {
                    uiInfo.numGameTypes++;
                }
                else {
                    Q_Println("Too many game types, last one replace!");
                }
            }

            token = COM_ParseExt((const char**)p, true);
            if (token[0] != '}') {
                return false;
            }
        }
    }
    return false;
}

static bool MapList_Parse(const char** p)
{
    char* token;

    token = COM_ParseExt((const char**)p, true);

    if (token[0] != '{') {
        return false;
    }

    uiInfo.mapCount = 0;

    while (1) {
        token = COM_ParseExt((const char**)p, true);

        if (Q_stricmp(token, "}") == 0) {
            return true;
        }

        if (!token || token[0] == 0) {
            return false;
        }

        if (token[0] == '{') {
            if (!String_Parse(p, &uiInfo.mapList[uiInfo.mapCount].mapName) || !String_Parse(p, &uiInfo.mapList[uiInfo.mapCount].mapLoadName) ||
                !Int_Parse(p, &uiInfo.mapList[uiInfo.mapCount].teamMembers)) {
                return false;
            }

            if (!String_Parse(p, &uiInfo.mapList[uiInfo.mapCount].opponentName)) {
                return false;
            }

            uiInfo.mapList[uiInfo.mapCount].typeBits = 0;

            while (1) {
                token = COM_ParseExt((const char**)p, true);
                if (token[0] >= '0' && token[0] <= '9') {
                    uiInfo.mapList[uiInfo.mapCount].typeBits |= (1 << (token[0] - 0x030));
                }
                else {
                    break;
                }
            }

            // mapList[mapCount].imageName = String_Alloc(Q_Format("levelshots/{}", mapList[mapCount].mapLoadName).c_str());
            // if (uiInfo.mapCount == 0) {
            //  only load the first cinematic, selection loads the others
            //  uiInfo.mapList[uiInfo.mapCount].cinematic = trap->CIN_PlayCinematic(Q_Format("{}.roq", uiInfo.mapList[uiInfo.mapCount].mapLoadName).c_str(),
            //  false, false, true, 0, 0, 0, 0);
            //}
            uiInfo.mapList[uiInfo.mapCount].cinematic = -1;
            uiInfo.mapList[uiInfo.mapCount].levelShot =
                trap->R_RegisterShaderNoMip(Q_Format("levelshots/{}_small", uiInfo.mapList[uiInfo.mapCount].mapLoadName).c_str());

            if (uiInfo.mapCount < MAX_MAPS) {
                uiInfo.mapCount++;
            }
            else {
                Q_Println("Too many maps, last one replaced!");
            }
        }
    }
    return false;
}

static void UI_ParseGameInfo(const char* teamFile)
{
    char* token;
    const char* p;
    char* buff = nullptr;
    // int mode = 0; TTimo: unused

    buff = GetMenuBuffer(teamFile);
    if (!buff) {
        return;
    }

    p = buff;

    COM_BeginParseSession("UI_ParseGameInfo");

    while (1) {
        token = COM_ParseExt((const char**)(&p), true);
        if (!token || token[0] == 0 || token[0] == '}') {
            break;
        }

        if (Q_stricmp(token, "}") == 0) {
            break;
        }

        if (Q_stricmp(token, "gametypes") == 0) {

            if (GameType_Parse(&p, false)) {
                continue;
            }
            else {
                break;
            }
        }

        if (Q_stricmp(token, "joingametypes") == 0) {

            if (GameType_Parse(&p, true)) {
                continue;
            }
            else {
                break;
            }
        }

        if (Q_stricmp(token, "maps") == 0) {
            // start a new menu
            MapList_Parse(&p);
        }
    }
}

static void UI_Pause(bool b)
{
    if (b) {
        // pause the game and set the ui keycatcher
        trap->Cvar_Set("cl_paused", "1");
        trap->Key_SetCatcher(keyCatchFlags_t::KEYCATCH_UI);
    }
    else {
        // unpause the game and clear the ui keycatcher
        trap->Key_SetCatcher(trap->Key_GetCatcher() & ~keyCatchFlags_t::KEYCATCH_UI);
        trap->Key_ClearStates();
        trap->Cvar_Set("cl_paused", "0");
    }
}

static int UI_PlayCinematic(const char* name, float x, float y, float w, float h)
{
    return trap->CIN_PlayCinematic(name, x, y, w, h, (cinematicFlags_t::CIN_loop | cinematicFlags_t::CIN_silent));
}

static void UI_StopCinematic(int handle)
{
    if (handle >= 0) {
        trap->CIN_StopCinematic(handle);
    }
    else {
        handle = abs(handle);
        if (handle == UI_MAPCINEMATIC) {
            if (uiInfo.mapList[ui_currentMap.integer].cinematic >= 0) {
                trap->CIN_StopCinematic(uiInfo.mapList[ui_currentMap.integer].cinematic);
                uiInfo.mapList[ui_currentMap.integer].cinematic = -1;
            }
        }
        else if (handle == UI_NETMAPCINEMATIC) {
            if (uiInfo.serverStatus.currentServerCinematic >= 0) {
                trap->CIN_StopCinematic(uiInfo.serverStatus.currentServerCinematic);
                uiInfo.serverStatus.currentServerCinematic = -1;
            }
        }
        else if (handle == UI_CLANCINEMATIC) {
            int i = UI_TeamIndexFromName(UI_Cvar_VariableString("ui_teamName"));
            if (i >= 0 && i < uiInfo.teamCount) {
                if (uiInfo.teamList[i].cinematic >= 0) {
                    trap->CIN_StopCinematic(uiInfo.teamList[i].cinematic);
                    uiInfo.teamList[i].cinematic = -1;
                }
            }
        }
    }
}

static void UI_DrawCinematic(int handle, float x, float y, float w, float h)
{
    trap->CIN_SetExtents(handle, x, y, w, h);
    trap->CIN_DrawCinematic(handle);
}

static void UI_RunCinematicFrame(int handle)
{
    trap->CIN_RunCinematic(handle);
}

/*
=================
UI_LoadForceConfig_List
=================
Looks in the directory for force config files (.fcf) and loads the name in
*/
void UI_LoadForceConfig_List(void)
{
    int numfiles = 0;
    char filelist[2048];
    char configname[128];
    char* fileptr = nullptr;
    int j = 0;
    int filelen = 0;
    bool lightSearch = false;

    uiInfo.forceConfigCount = 0;
    Q_FormatTo(uiInfo.forceConfigNames[uiInfo.forceConfigCount], "Custom");
    uiInfo.forceConfigCount++;
    // Always reserve index 0 as the "custom" config

nextSearch:
    if (lightSearch) { // search light side folder
        numfiles = trap->FS_GetFileList("forcecfg/light", "fcf", filelist, 2048);
        uiInfo.forceConfigLightIndexBegin = uiInfo.forceConfigCount - 1;
    }
    else { // search dark side folder
        numfiles = trap->FS_GetFileList("forcecfg/dark", "fcf", filelist, 2048);
        uiInfo.forceConfigDarkIndexBegin = uiInfo.forceConfigCount - 1;
    }

    fileptr = filelist;

    for (j = 0; j < numfiles && uiInfo.forceConfigCount < MAX_FORCE_CONFIGS; j++, fileptr += filelen + 1) {
        filelen = strlen(fileptr);
        COM_StripExtension(fileptr, configname, sizeof(configname));

        if (lightSearch) {
            uiInfo.forceConfigSide[uiInfo.forceConfigCount] = true; // light side config
        }
        else {
            uiInfo.forceConfigSide[uiInfo.forceConfigCount] = false; // dark side config
        }

        Q_FormatTo(uiInfo.forceConfigNames[uiInfo.forceConfigCount], "{}", configname);
        uiInfo.forceConfigCount++;
    }

    if (!lightSearch) {
        lightSearch = true;
        goto nextSearch;
    }
}

/*
=================
bIsImageFile
builds path and scans for valid image extentions
=================
*/
static bool bIsImageFile(const char* dirptr, const char* skinname)
{
    char fpath[MAX_QPATH];
    int f;

    Q_FormatTo(std::span(fpath, MAX_QPATH), "models/players/{}/icon_{}.jpg", dirptr, skinname);
    trap->FS_Open(fpath, &f, fsMode_t::FS_READ);
    if (!f) { // not there, try png
        Q_FormatTo(std::span(fpath, MAX_QPATH), "models/players/{}/icon_{}.png", dirptr, skinname);
        trap->FS_Open(fpath, &f, fsMode_t::FS_READ);
    }
    if (!f) { // not there, try tga
        Q_FormatTo(std::span(fpath, MAX_QPATH), "models/players/{}/icon_{}.tga", dirptr, skinname);
        trap->FS_Open(fpath, &f, fsMode_t::FS_READ);
    }
    if (f) {
        trap->FS_Close(f);
        return true;
    }

    return false;
}

/*
=================
PlayerModel_BuildList
=================
*/
static void UI_BuildQ3Model_List(void)
{
    int numdirs;
    int numfiles;
    char dirlist[2048];
    char filelist[2048];
    char skinname[64];
    char* dirptr;
    char* fileptr;
    char* check;
    int i;
    int j, k, p, s;
    int dirlen;
    int filelen;

    uiInfo.q3HeadCount = 0;

    // iterate directory of all player models
    numdirs = trap->FS_GetFileList("models/players", "/", dirlist, 2048);
    dirptr = dirlist;
    for (i = 0; i < numdirs && uiInfo.q3HeadCount < MAX_Q3PLAYERMODELS; i++, dirptr += dirlen + 1) {
        dirlen = strlen(dirptr);

        if (dirlen && dirptr[dirlen - 1] == '/')
            dirptr[dirlen - 1] = '\0';

        if (!strcmp(dirptr, ".") || !strcmp(dirptr, ".."))
            continue;

        numfiles = trap->FS_GetFileList(Q_Format("models/players/{}", dirptr).c_str(), "skin", filelist, 2048);
        fileptr = filelist;
        for (j = 0; j < numfiles && uiInfo.q3HeadCount < MAX_Q3PLAYERMODELS; j++, fileptr += filelen + 1) {
            int skinLen = 0;

            filelen = strlen(fileptr);

            COM_StripExtension(fileptr, skinname, sizeof(skinname));

            skinLen = strlen(skinname);
            k = 0;
            while (k < skinLen && skinname[k] && skinname[k] != '_') {
                k++;
            }
            if (skinname[k] == '_') {
                p = 0;

                while (skinname[k]) {
                    skinname[p] = skinname[k];
                    k++;
                    p++;
                }
                skinname[p] = '\0';
            }

            /*
            Q_FormatTo(std::span(fpath, 2048), "models/players/{}/icon{}.jpg", dirptr, skinname);

            trap->FS_Open(fpath, &f, fsMode_t::FS_READ);

            if (f)
            */
            check = &skinname[1];
            if (bIsImageFile(dirptr, check)) { // if it exists
                bool iconExists = false;

                // trap->FS_Close(f);

                if (skinname[0] == '_') { // change character to append properly
                    skinname[0] = '/';
                }

                s = 0;

                while (s < uiInfo.q3HeadCount) { // check for dupes
                    if (!Q_stricmp(Q_Format("{}{}", dirptr, skinname).c_str(), uiInfo.q3HeadNames[s])) {
                        iconExists = true;
                        break;
                    }
                    s++;
                }

                if (iconExists) {
                    continue;
                }

                Q_FormatTo(uiInfo.q3HeadNames[uiInfo.q3HeadCount], "{}{}", dirptr, skinname);
                uiInfo.q3HeadIcons[uiInfo.q3HeadCount++] = 0; // trap->R_RegisterShaderNoMip(fpath);
                // rww - we are now registering them as they are drawn like the TA feeder, so as to decrease UI load time.
            }

            if (uiInfo.q3HeadCount >= MAX_Q3PLAYERMODELS) {
                return;
            }
        }
    }
}

void UI_SiegeInit(void)
{
    // Load the player class types
    BG_SiegeLoadClasses(g_UIClassDescriptions);

    if (!bgNumSiegeClasses) { // We didn't find any?!
        Q_Error(errorParm_t::ERR_DROP, "Couldn't find any player classes for Siege");
    }

    // Now load the teams since we have class data.
    BG_SiegeLoadTeams();

    if (!bgNumSiegeTeams) { // React same as with classes.
        Q_Error(errorParm_t::ERR_DROP, "Couldn't find any player teams for Siege");
    }
}

/*
=================
UI_ParseColorData
=================
*/
// static bool UI_ParseColorData(char* buf, playerSpeciesInfo_t &species)
static bool UI_ParseColorData(char* buf, playerSpeciesInfo_t* species, char* file)
{
    const char* token;
    const char* p;

    p = buf;
    COM_BeginParseSession(file);
    species->ColorCount = 0;
    species->ColorMax = 16;
    species->Color = (playerColor_t*)malloc(species->ColorMax * sizeof(playerColor_t));

    while (p) {
        token = COM_ParseExt(&p, true); // looking for the shader
        if (token[0] == 0) {
            return species->ColorCount;
        }
        if (species->ColorCount >= species->ColorMax) {
            species->ColorMax *= 2;
            species->Color = (playerColor_t*)realloc(species->Color, species->ColorMax * sizeof(playerColor_t));
        }

        memset(&species->Color[species->ColorCount], 0, sizeof(playerColor_t));

        Q_strncpyz(species->Color[species->ColorCount].shader, token, MAX_QPATH);

        token = COM_ParseExt(&p, true); // looking for action block {
        if (token[0] != '{') {
            return false;
        }

        token = COM_ParseExt(&p, true); // looking for action commands
        while (token[0] != '}') {
            if (token[0] == 0) { // EOF
                return false;
            }
            Q_strcat(species->Color[species->ColorCount].actionText, ACTION_BUFFER_SIZE, token);
            Q_strcat(species->Color[species->ColorCount].actionText, ACTION_BUFFER_SIZE, " ");
            token = COM_ParseExt(&p, true); // looking for action commands or final }
        }
        species->ColorCount++; // next color please
    }
    return true; // never get here
}

static void UI_FreeSpecies(playerSpeciesInfo_t* species)
{
    free(species->SkinHead);
    free(species->SkinTorso);
    free(species->SkinLeg);
    free(species->Color);
    memset(species, 0, sizeof(playerSpeciesInfo_t));
}

void UI_FreeAllSpecies(void)
{
    int i;

    for (i = 0; i < uiInfo.playerSpeciesCount; i++) {
        UI_FreeSpecies(&uiInfo.playerSpecies[i]);
    }
    free(uiInfo.playerSpecies);
}

/*
=================
UI_BuildPlayerModel_List
=================
*/
static void UI_BuildPlayerModel_List(bool inGameLoad)
{
    static const size_t DIR_LIST_SIZE = 16384;

    int numdirs;
    size_t dirListSize = DIR_LIST_SIZE;
    char stackDirList[8192];
    char* dirlist;
    char* dirptr;
    int dirlen;
    int i;
    int j;

    dirlist = (char*)malloc(DIR_LIST_SIZE);
    if (!dirlist) {
        Q_PrintlnWarning("Failed to allocate {} bytes of memory for player model directory list. Using stack allocated buffer of {} bytes instead.",
                         DIR_LIST_SIZE, sizeof(stackDirList));

        dirlist = stackDirList;
        dirListSize = sizeof(stackDirList);
    }

    uiInfo.playerSpeciesCount = 0;
    uiInfo.playerSpeciesIndex = 0;
    uiInfo.playerSpeciesMax = 8;
    uiInfo.playerSpecies = (playerSpeciesInfo_t*)malloc(uiInfo.playerSpeciesMax * sizeof(playerSpeciesInfo_t));

    // iterate directory of all player models
    numdirs = trap->FS_GetFileList("models/players", "/", dirlist, dirListSize);
    dirptr = dirlist;
    for (i = 0; i < numdirs; i++, dirptr += dirlen + 1) {
        char* fileptr;
        int filelen;
        int f = 0;
        char fpath[MAX_QPATH];

        dirlen = strlen(dirptr);

        if (dirlen) {
            if (dirptr[dirlen - 1] == '/')
                dirptr[dirlen - 1] = '\0';
        }
        else {
            continue;
        }

        if (!strcmp(dirptr, ".") || !strcmp(dirptr, ".."))
            continue;

        Q_FormatTo(fpath, "models/players/{}/PlayerChoice.txt", dirptr);
        filelen = trap->FS_Open(fpath, &f, fsMode_t::FS_READ);

        if (f) {
            char filelist[2048];
            playerSpeciesInfo_t* species;
            char skinname[64];
            int numfiles;
            int iSkinParts = 0;
            char* buffer = nullptr;

            buffer = (char*)malloc(filelen + 1);
            if (!buffer) {
                trap->FS_Close(f);
                Q_Error(errorParm_t::ERR_FATAL, "Could not allocate buffer to read {}", fpath);
            }

            trap->FS_Read(buffer, filelen, f);
            trap->FS_Close(f);

            buffer[filelen] = 0;

            // record this species
            if (uiInfo.playerSpeciesCount >= uiInfo.playerSpeciesMax) {
                uiInfo.playerSpeciesMax *= 2;
                uiInfo.playerSpecies = (playerSpeciesInfo_t*)realloc(uiInfo.playerSpecies, uiInfo.playerSpeciesMax * sizeof(playerSpeciesInfo_t));
            }
            species = &uiInfo.playerSpecies[uiInfo.playerSpeciesCount];
            memset(species, 0, sizeof(playerSpeciesInfo_t));
            Q_strncpyz(species->Name, dirptr, MAX_QPATH);

            if (!UI_ParseColorData(buffer, species, fpath)) {
                Q_Println(S_COLOR_RED "UI_BuildPlayerModel_List: Errors parsing '{}'", fpath);
            }

            species->SkinHeadMax = 8;
            species->SkinTorsoMax = 8;
            species->SkinLegMax = 8;

            species->SkinHead = (skinName_t*)malloc(species->SkinHeadMax * sizeof(skinName_t));
            species->SkinTorso = (skinName_t*)malloc(species->SkinTorsoMax * sizeof(skinName_t));
            species->SkinLeg = (skinName_t*)malloc(species->SkinLegMax * sizeof(skinName_t));

            free(buffer);

            numfiles = trap->FS_GetFileList(Q_Format("models/players/{}", dirptr).c_str(), ".skin", filelist, sizeof(filelist));
            fileptr = filelist;
            for (j = 0; j < numfiles; j++, fileptr += filelen + 1) {
                if (trap->Cvar_VariableValue("fs_copyfiles") > 0) {
                    trap->FS_Open(Q_Format("models/players/{}/{}", dirptr, fileptr).c_str(), &f, fsMode_t::FS_READ);
                    if (f)
                        trap->FS_Close(f);
                }

                filelen = strlen(fileptr);
                COM_StripExtension(fileptr, skinname, sizeof(skinname));

                if (bIsImageFile(dirptr, skinname)) { // if it exists
                    if (Q_stricmpn(skinname, "head_", 5) == 0) {
                        if (species->SkinHeadCount >= species->SkinHeadMax) {
                            species->SkinHeadMax *= 2;
                            species->SkinHead = (skinName_t*)realloc(species->SkinHead, species->SkinHeadMax * sizeof(skinName_t));
                        }
                        Q_strncpyz(species->SkinHead[species->SkinHeadCount++].name, skinname, SKIN_LENGTH);
                        iSkinParts |= 1 << 0;
                    }
                    else if (Q_stricmpn(skinname, "torso_", 6) == 0) {
                        if (species->SkinTorsoCount >= species->SkinTorsoMax) {
                            species->SkinTorsoMax *= 2;
                            species->SkinTorso = (skinName_t*)realloc(species->SkinTorso, species->SkinTorsoMax * sizeof(skinName_t));
                        }
                        Q_strncpyz(species->SkinTorso[species->SkinTorsoCount++].name, skinname, SKIN_LENGTH);
                        iSkinParts |= 1 << 1;
                    }
                    else if (Q_stricmpn(skinname, "lower_", 6) == 0) {
                        if (species->SkinLegCount >= species->SkinLegMax) {
                            species->SkinLegMax *= 2;
                            species->SkinLeg = (skinName_t*)realloc(species->SkinLeg, species->SkinLegMax * sizeof(skinName_t));
                        }
                        Q_strncpyz(species->SkinLeg[species->SkinLegCount++].name, skinname, SKIN_LENGTH);
                        iSkinParts |= 1 << 2;
                    }
                }
            }
            if (iSkinParts != 7) { // didn't get a skin for each, then skip this model.
                UI_FreeSpecies(species);
                continue;
            }
            uiInfo.playerSpeciesCount++;
            if (!inGameLoad && ui_PrecacheModels.integer) {
                int g2Model;
                void* ghoul2 = nullptr;
                Q_FormatTo(fpath, "models/players/{}/model.glm", dirptr);
                g2Model = trap->G2API_InitGhoul2Model(&ghoul2, fpath, 0, 0, 0, 0, 0);
                if (g2Model >= 0) {
                    //					trap->G2API_RemoveGhoul2Model( &ghoul2, 0 );
                    trap->G2API_CleanGhoul2Models(&ghoul2);
                }
            }
        }
    }

    if (dirlist != stackDirList) {
        free(dirlist);
    }
}

static qhandle_t UI_RegisterShaderNoMip(const char* name)
{
    if (*name == '*') {
        char buf[MAX_CVAR_VALUE_STRING];

        trap->Cvar_VariableStringBuffer(name + 1, buf, sizeof(buf));

        if (buf[0])
            return trap->R_RegisterShaderNoMip(buf);
    }

    return trap->R_RegisterShaderNoMip(name);
}

/*
=================
UI_ReadGlconfig

The rendering configuration, with the scales of the menus' 640 by 480 to the picture: at the start, and again when the
display changed
=================
*/
static void UI_ReadGlconfig(void)
{
    trap->GetGlconfig(&uiInfo.uiDC.glconfig);

    // for 640x480 virtualized screen
    uiInfo.uiDC.yscale = uiInfo.uiDC.glconfig.vidHeight * (1.0 / 480.0);
    uiInfo.uiDC.xscale = uiInfo.uiDC.glconfig.vidWidth * (1.0 / 640.0);
    if (uiInfo.uiDC.glconfig.vidWidth * 480 > uiInfo.uiDC.glconfig.vidHeight * 640) {
        // wide screen
        uiInfo.uiDC.bias = 0.5 * (uiInfo.uiDC.glconfig.vidWidth - (uiInfo.uiDC.glconfig.vidHeight * (640.0 / 480.0)));
    }
    else {
        // no wide screen
        uiInfo.uiDC.bias = 0;
    }
}

/*
=================
UI_DisplayChanged

The display changed: its size read again; the video page's display rows show it, unless the page has a change of its
own to apply, which "Apply" sets as the page shows it
=================
*/
static void UI_DisplayChanged(void)
{
    UI_ReadGlconfig();

    if (trap->Cvar_VariableValue("ui_r_modified") == 0) {
        UI_GetDisplaySetup();
    }
}

/*
=================
UI_Init
=================
*/
void UI_Init(bool inGameLoad)
{
    const char* menuSet;
    char debugSeed[16];

    // com_debugSeed when it is set, as every stream, else the clock's
    trap->Cvar_VariableStringBuffer("com_debugSeed", debugSeed, sizeof(debugSeed));
    if (const int seed = atoi(debugSeed); seed != 0) {
        Rand_InitFixed(seed, randStream_t::RAND_UI);
    }
    else {
        Rand_Init(trap->Milliseconds());
    }

    // Get the list of possible languages
    uiInfo.languageCount = trap->SE_GetNumLanguages(); // this does a dir scan, so use carefully

    uiInfo.inGameLoad = inGameLoad;

    // initialize all these cvars to "0"
    UI_SiegeSetCvarsForClass(nullptr);

    UI_SiegeInit();

    UI_UpdateForcePowers();

    UI_RegisterCvars();
    UI_InitMemory();

    // cache redundant calulations
    UI_ReadGlconfig();

    // UI_Load();
    uiInfo.uiDC.registerShaderNoMip = UI_RegisterShaderNoMip;
    uiInfo.uiDC.setColor = &UI_SetColor;
    uiInfo.uiDC.drawHandlePic = &UI_DrawHandlePic;
    uiInfo.uiDC.drawStretchPic = trap->R_DrawStretchPic;
    uiInfo.uiDC.drawText = &Text_Paint;
    uiInfo.uiDC.textWidth = &Text_Width;
    uiInfo.uiDC.textHeight = &Text_Height;
    uiInfo.uiDC.registerModel = trap->R_RegisterModel;
    uiInfo.uiDC.modelBounds = trap->R_ModelBounds;
    uiInfo.uiDC.fillRect = &UI_FillRect;
    uiInfo.uiDC.drawRect = &_UI_DrawRect;
    uiInfo.uiDC.drawSides = &_UI_DrawSides;
    uiInfo.uiDC.drawTopBottom = &_UI_DrawTopBottom;
    uiInfo.uiDC.clearScene = trap->R_ClearScene;
    uiInfo.uiDC.drawSides = &_UI_DrawSides;
    uiInfo.uiDC.addRefEntityToScene = trap->R_AddRefEntityToScene;
    uiInfo.uiDC.renderScene = trap->R_RenderScene;
    uiInfo.uiDC.RegisterFont = trap->R_RegisterFont;
    uiInfo.uiDC.Font_StrLenPixels = trap->R_Font_StrLenPixels;
    uiInfo.uiDC.Font_StrLenChars = trap->R_Font_StrLenChars;
    uiInfo.uiDC.Font_HeightPixels = trap->R_Font_HeightPixels;
    uiInfo.uiDC.Font_DrawString = trap->R_Font_DrawString;
    uiInfo.uiDC.Language_IsAsian = trap->R_Language_IsAsian;
    uiInfo.uiDC.Language_UsesSpaces = trap->R_Language_UsesSpaces;
    uiInfo.uiDC.AnyLanguage_ReadCharFromString = trap->R_AnyLanguage_ReadCharFromString;
    uiInfo.uiDC.ownerDrawItem = &UI_OwnerDraw;
    uiInfo.uiDC.getValue = &UI_GetValue;
    uiInfo.uiDC.ownerDrawVisible = &UI_OwnerDrawVisible;
    uiInfo.uiDC.runScript = &UI_RunMenuScript;
    uiInfo.uiDC.deferScript = &UI_DeferMenuScript;
    uiInfo.uiDC.getTeamColor = &UI_GetTeamColor;
    uiInfo.uiDC.setCVar = trap->Cvar_Set;
    uiInfo.uiDC.getCVarString = trap->Cvar_VariableStringBuffer;
    uiInfo.uiDC.getCVarValue = trap->Cvar_VariableValue;
    uiInfo.uiDC.drawTextWithCursor = &Text_PaintWithCursor;
    uiInfo.uiDC.setOverstrikeMode = trap->Key_SetOverstrikeMode;
    uiInfo.uiDC.getOverstrikeMode = trap->Key_GetOverstrikeMode;
    uiInfo.uiDC.ownerDrawHandleKey = &UI_OwnerDrawHandleKey;
    uiInfo.uiDC.feederCount = &UI_FeederCount;
    uiInfo.uiDC.feederItemImage = &UI_FeederItemImage;
    uiInfo.uiDC.feederItemText = &UI_FeederItemText;
    uiInfo.uiDC.feederSelection = &UI_FeederSelection;
    uiInfo.uiDC.setBinding = trap->Key_SetBinding;
    uiInfo.uiDC.getBindingBuf = trap->Key_GetBindingBuf;
    uiInfo.uiDC.keynumToStringBuf = trap->Key_KeynumToStringBuf;
    uiInfo.uiDC.executeText = trap->Cmd_ExecuteText;
    uiInfo.uiDC.Pause = &UI_Pause;
    uiInfo.uiDC.ownerDrawWidth = &UI_OwnerDrawWidth;
    uiInfo.uiDC.sound = trap->sound;
    uiInfo.uiDC.playCinematic = &UI_PlayCinematic;
    uiInfo.uiDC.stopCinematic = &UI_StopCinematic;
    uiInfo.uiDC.drawCinematic = &UI_DrawCinematic;
    uiInfo.uiDC.runCinematicFrame = &UI_RunCinematicFrame;
    uiInfo.uiDC.ext.Font_StrLenPixels = trap->ext.R_Font_StrLenPixels;

    Init_Display(&uiInfo.uiDC);

    UI_BuildPlayerModel_List(inGameLoad);

    String_Init();

    uiInfo.uiDC.cursor = trap->R_RegisterShaderNoMip("menu/art/3_cursor2");
    uiInfo.uiDC.whiteShader = trap->R_RegisterShaderNoMip("white");

    AssetCache();

    uiInfo.teamCount = 0;
    uiInfo.characterCount = 0;
    uiInfo.aliasCount = 0;

    UI_ParseGameInfo("ui/jamp/gameinfo.txt");

    menuSet = UI_Cvar_VariableString("ui_menuFilesMP");
    if (menuSet == nullptr || menuSet[0] == '\0') {
        menuSet = "ui/jampmenus.txt";
    }

#if 1
    if (inGameLoad) {
        UI_LoadMenus("ui/jampingame.txt", true);
    }
    else if (!ui_bypassMainMenuLoad.integer) {
        UI_LoadMenus(menuSet, true);
    }
#else // this was adding quite a giant amount of time to the load time
    UI_LoadMenus(menuSet, true);
    UI_LoadMenus("ui/jampingame.txt", true);
#endif

    {
        char buf[MAX_NETNAME] = {0};
        Q_strncpyz(buf, UI_Cvar_VariableString("name"), sizeof(buf));
        trap->Cvar_Register(nullptr, "ui_Name", buf, cvarFlags_t::CVAR_INTERNAL);
    }

    Menus_CloseAll();

    trap->LAN_LoadCachedServers();

    UI_BuildQ3Model_List();
    UI_LoadBots();

    UI_LoadForceConfig_List();

    UI_InitForceShaders();

    // sets defaults for ui temp cvars
    uiInfo.currentCrosshair = (int)trap->Cvar_VariableValue("cg_drawCrosshair");
    trap->Cvar_Set("ui_mousePitch", (trap->Cvar_VariableValue("m_pitch") >= 0) ? "0" : "1");
    trap->Cvar_Set("ui_mousePitchVeh", (trap->Cvar_VariableValue("m_pitchVeh") >= 0) ? "0" : "1");

    uiInfo.serverStatus.currentServerCinematic = -1;
    uiInfo.previewMovie = -1;

    trap->Cvar_Register(nullptr, "debug_protocol", "", cvarFlags_t::CVAR_NONE);

    trap->Cvar_Set("ui_actualNetGameType", Q_Format("{}", ui_netGametype.integer).c_str());
    trap->Cvar_Update(&ui_actualNetGametype);
}

constexpr int UI_FPS_FRAMES = 4;
void UI_Refresh(int realtime)
{
    static int index;
    static int previousTimes[UI_FPS_FRAMES];

    // if ( !Any( trap->Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_UI ) ) {
    //	return;
    // }

    trap->G2API_SetTime(realtime, 0);
    trap->G2API_SetTime(realtime, 1);
    // ghoul2 timer must be explicitly updated during ui rendering.

    uiInfo.uiDC.frameTime = realtime - uiInfo.uiDC.realTime;
    uiInfo.uiDC.realTime = realtime;

    previousTimes[index % UI_FPS_FRAMES] = uiInfo.uiDC.frameTime;
    index++;
    if (index > UI_FPS_FRAMES) {
        int i, total;
        // average multiple frames together to smooth changes out a bit
        total = 0;
        for (i = 0; i < UI_FPS_FRAMES; i++) {
            total += previousTimes[i];
        }
        if (!total) {
            total = 1;
        }
        uiInfo.uiDC.FPS = 1000 * UI_FPS_FRAMES / total;
    }

    UI_UpdateCvars();

    if (Menu_Count() > 0) {
        // paint all the menus
        Menu_PaintAll();
        // refresh server browser list
        UI_DoServerRefresh();
        // refresh server status
        UI_BuildServerStatus(false);
        // refresh find player list
        UI_BuildFindPlayerList(false);
    }
    // the pointer is the system's own, with the game's image: nothing to draw
    UI_SetColor(nullptr);

    if (ui_rankChange.integer) {
        FPMessageTime = realtime + 3000;

        if (!parsedFPMessage[0] /*&& uiMaxRank > static_cast<forceMasteries_t>( ui_rankChange.integer )*/) {
            const char* printMessage = UI_GetStringEdString("MP_INGAME", "SET_NEW_RANK");

            int i = 0;
            int p = 0;
            int linecount = 0;

            while (printMessage[i] && p < 1024) {
                parsedFPMessage[p] = printMessage[i];
                p++;
                i++;
                linecount++;

                if (linecount > 64 && printMessage[i] == ' ') {
                    parsedFPMessage[p] = '\n';
                    p++;
                    linecount = 0;
                }
            }
            parsedFPMessage[p] = '\0';
        }

        // if (uiMaxRank > static_cast<forceMasteries_t>( ui_rankChange.integer ))
        {
            uiMaxRank = static_cast<forceMasteries_t>(ui_rankChange.integer);
            uiForceRank = uiMaxRank;

            /*
            while (x < forcePowers_t::NUM_FORCE_POWERS)
            {
                //For now just go ahead and clear force powers upon rank change
                uiForcePowersRank[x] = forcePowerLevels_t::FORCE_LEVEL_0;
                x++;
            }
            uiForcePowersRank[forcePowers_t::FP_LEVITATION] = forcePowerLevels_t::FORCE_LEVEL_1;
            uiForceUsed = 0;
            */

            // Use BG_LegalizedForcePowers and transfer the result into the UI force settings
            UI_ReadLegalForce();
        }

        if (ui_freeSaber.integer && uiForcePowersRank[forcePowers_t::FP_SABER_OFFENSE] < forcePowerLevels_t::FORCE_LEVEL_1) {
            uiForcePowersRank[forcePowers_t::FP_SABER_OFFENSE] = forcePowerLevels_t::FORCE_LEVEL_1;
        }
        if (ui_freeSaber.integer && uiForcePowersRank[forcePowers_t::FP_SABER_DEFENSE] < forcePowerLevels_t::FORCE_LEVEL_1) {
            uiForcePowersRank[forcePowers_t::FP_SABER_DEFENSE] = forcePowerLevels_t::FORCE_LEVEL_1;
        }
        trap->Cvar_Set("ui_rankChange", "0");

        // remember to update the force power count after changing the max rank
        UpdateForceUsed();
    }

    if (ui_freeSaber.integer) {
        bgForcePowerCost[forcePowers_t::FP_SABER_OFFENSE][forcePowerLevels_t::FORCE_LEVEL_1] = 0;
        bgForcePowerCost[forcePowers_t::FP_SABER_DEFENSE][forcePowerLevels_t::FORCE_LEVEL_1] = 0;
    }
    else {
        bgForcePowerCost[forcePowers_t::FP_SABER_OFFENSE][forcePowerLevels_t::FORCE_LEVEL_1] = 1;
        bgForcePowerCost[forcePowers_t::FP_SABER_DEFENSE][forcePowerLevels_t::FORCE_LEVEL_1] = 1;
    }

    /*
    if (parsedFPMessage[0] && FPMessageTime > realtime)
    {
        vec4_t txtCol;
        int txtStyle = ITEM_TEXTSTYLE_SHADOWED;

        if ((FPMessageTime - realtime) < 2000)
        {
            txtCol[0] = colorWhite[0];
            txtCol[1] = colorWhite[1];
            txtCol[2] = colorWhite[2];
            txtCol[3] = (((float)FPMessageTime - (float)realtime)/2000);

            txtStyle = 0;
        }
        else
        {
            txtCol[0] = colorWhite[0];
            txtCol[1] = colorWhite[1];
            txtCol[2] = colorWhite[2];
            txtCol[3] = colorWhite[3];
        }

        Text_Paint(10, 0, 1, txtCol, parsedFPMessage, 0, 1024, txtStyle, menuFont_t::FONT_MEDIUM);
    }
    */
    // For now, don't bother.
}

/*
=================
UI_KeyEvent
=================
*/
void UI_KeyEvent(fakeAscii_t key, bool down)
{
    if (Menu_Count() > 0) {
        menuDef_t* menu = Menu_GetFocused();
        if (menu) {
            // Escape closes the menus opened over the game; but a row waiting for the key to bind, or a field being
            // edited, takes it first: the bind is cancelled or the editing ends, and the menu stays
            if (key == fakeAscii_t::A_ESCAPE && down && !Menus_AnyFullScreenVisible() && !Menu_TakesEscape()) {
                Menus_CloseAll();
            }
            else {
                Menu_HandleKey(menu, std::to_underlying(key), down);
            }
        }
        else {
            trap->Key_SetCatcher(trap->Key_GetCatcher() & ~keyCatchFlags_t::KEYCATCH_UI);
            trap->Key_ClearStates();
            trap->Cvar_Set("cl_paused", "0");
        }
    }

    // if ((s > 0) && (s != menu_null_sound)) {
    //   Menu_PlaySound(s);
    //}
}

/*
=================
UI_CharEvent

A typed character, for the field being edited
=================
*/
void UI_CharEvent(int character)
{
    if (Menu_Count() > 0 && Menu_GetFocused()) {
        Menu_HandleChar(character);
    }
}

/*
=================
UI_MouseEvent
=================
*/
void UI_MouseEvent(int x, int y)
{
    // the place is in the window's pixels, the menus count in their 640 by 480
    uiInfo.uiDC.cursorx = Com_Clampi(0, SCREEN_WIDTH, static_cast<int>(x / uiInfo.uiDC.xscale));
    uiInfo.uiDC.cursory = Com_Clampi(0, SCREEN_HEIGHT, static_cast<int>(y / uiInfo.uiDC.yscale));

    if (Menu_Count() > 0) {
        // menuDef_t *menu = Menu_GetFocused();
        // Menu_HandleMouseMove(menu, uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory);
        Display_MouseMove(nullptr, uiInfo.uiDC.cursorx, uiInfo.uiDC.cursory);
    }
}

static void UI_ReadableSize(char* buf, int bufsize, int value)
{
    if (value > 1024 * 1024 * 1024) { // gigs
        Q_FormatTo(std::span(buf, bufsize), "{}", value / (1024 * 1024 * 1024));
        Q_FormatTo(std::span(buf + strlen(buf), bufsize - strlen(buf)), ".{:02} GB", (value % (1024 * 1024 * 1024)) * 100 / (1024 * 1024 * 1024));
    }
    else if (value > 1024 * 1024) { // megs
        Q_FormatTo(std::span(buf, bufsize), "{}", value / (1024 * 1024));
        Q_FormatTo(std::span(buf + strlen(buf), bufsize - strlen(buf)), ".{:02} MB", (value % (1024 * 1024)) * 100 / (1024 * 1024));
    }
    else if (value > 1024) { // kilos
        Q_FormatTo(std::span(buf, bufsize), "{} KB", value / 1024);
    }
    else { // bytes
        Q_FormatTo(std::span(buf, bufsize), "{} bytes", value);
    }
}

// Assumes time is in msec
static void UI_PrintTime(char* buf, int bufsize, int time)
{
    time /= 1000; // change to seconds

    if (time > 3600) { // in the hours range
        Q_FormatTo(std::span(buf, bufsize), "{} hr {:2} min", time / 3600, (time % 3600) / 60);
    }
    else if (time > 60) { // mins
        Q_FormatTo(std::span(buf, bufsize), "{:2} min {:2} sec", time / 60, time % 60);
    }
    else { // secs
        Q_FormatTo(std::span(buf, bufsize), "{:2} sec", time);
    }
}

void Text_PaintCenter(float x, float y, float scale, vec4_t color, const char* text, float adjust, menuFont_t iMenuFont)
{
    int len = Text_Width(text, scale, iMenuFont);
    Text_Paint(x - len / 2, y, scale, color, text, 0, 0, ITEM_TEXTSTYLE_SHADOWEDMORE, iMenuFont);
}

static void UI_DisplayDownloadInfo(const char* downloadName, float centerPoint, float yStart, float scale, menuFont_t iMenuFont)
{
    char sDownLoading[256];
    char sEstimatedTimeLeft[256];
    char sTransferRate[256];
    char sOf[20];
    char sCopied[256];
    char sSec[20];
    //
    int downloadSize, downloadCount, downloadTime;
    char dlSizeBuf[64], totalSizeBuf[64], xferRateBuf[64], dlTimeBuf[64];
    int xferRate;
    int leftWidth;
    const char* s;
    formatText_t sText; // holds the text s points into

    vec4_t colorLtGreyAlpha = {0, 0, 0, .5};

    UI_FillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, colorLtGreyAlpha);

    s = GetCRDelineatedString("MENUS", "DOWNLOAD_STUFF", 0); // "Downloading:"
    Q_strncpyz(sDownLoading, s ? s : "", sizeof(sDownLoading));
    s = GetCRDelineatedString("MENUS", "DOWNLOAD_STUFF", 1); // "Estimated time left:"
    Q_strncpyz(sEstimatedTimeLeft, s ? s : "", sizeof(sEstimatedTimeLeft));
    s = GetCRDelineatedString("MENUS", "DOWNLOAD_STUFF", 2); // "Transfer rate:"
    Q_strncpyz(sTransferRate, s ? s : "", sizeof(sTransferRate));
    s = GetCRDelineatedString("MENUS", "DOWNLOAD_STUFF", 3); // "of"
    Q_strncpyz(sOf, s ? s : "", sizeof(sOf));
    s = GetCRDelineatedString("MENUS", "DOWNLOAD_STUFF", 4); // "copied"
    Q_strncpyz(sCopied, s ? s : "", sizeof(sCopied));
    s = GetCRDelineatedString("MENUS", "DOWNLOAD_STUFF", 5); // "sec."
    Q_strncpyz(sSec, s ? s : "", sizeof(sSec));

    downloadSize = trap->Cvar_VariableValue("cl_downloadSize");
    downloadCount = trap->Cvar_VariableValue("cl_downloadCount");
    downloadTime = trap->Cvar_VariableValue("cl_downloadTime");

    leftWidth = 320;

    UI_SetColor(colorWhite);

    Text_PaintCenter(centerPoint, yStart + 112, scale, colorWhite, sDownLoading, 0, iMenuFont);
    Text_PaintCenter(centerPoint, yStart + 192, scale, colorWhite, sEstimatedTimeLeft, 0, iMenuFont);
    Text_PaintCenter(centerPoint, yStart + 248, scale, colorWhite, sTransferRate, 0, iMenuFont);

    if (downloadSize > 0) {
        s = (sText = Q_Format("{} ({}%)", downloadName, (int)((float)downloadCount * 100.0f / downloadSize))).c_str();
    }
    else {
        s = downloadName;
    }

    Text_PaintCenter(centerPoint, yStart + 136, scale, colorWhite, s, 0, iMenuFont);

    UI_ReadableSize(dlSizeBuf, sizeof dlSizeBuf, downloadCount);
    UI_ReadableSize(totalSizeBuf, sizeof totalSizeBuf, downloadSize);

    if (downloadCount < 4096 || !downloadTime) {
        Text_PaintCenter(leftWidth, yStart + 216, scale, colorWhite, "estimating", 0, iMenuFont);
        Text_PaintCenter(leftWidth, yStart + 160, scale, colorWhite, Q_Format("({} {} {} {})", dlSizeBuf, sOf, totalSizeBuf, sCopied).c_str(), 0, iMenuFont);
    }
    else {
        if ((uiInfo.uiDC.realTime - downloadTime) / 1000) {
            xferRate = downloadCount / ((uiInfo.uiDC.realTime - downloadTime) / 1000);
        }
        else {
            xferRate = 0;
        }
        UI_ReadableSize(xferRateBuf, sizeof xferRateBuf, xferRate);

        // Extrapolate estimated completion time
        if (downloadSize && xferRate) {
            int n = downloadSize / xferRate; // estimated time for entire d/l in secs

            // We do it in K (/1024) because we'd overflow around 4MB
            UI_PrintTime(dlTimeBuf, sizeof dlTimeBuf, (n - (((downloadCount / 1024) * n) / (downloadSize / 1024))) * 1000);

            Text_PaintCenter(leftWidth, yStart + 216, scale, colorWhite, dlTimeBuf, 0, iMenuFont);
            Text_PaintCenter(leftWidth, yStart + 160, scale, colorWhite, Q_Format("({} {} {} {})", dlSizeBuf, sOf, totalSizeBuf, sCopied).c_str(), 0,
                             iMenuFont);
        }
        else {
            Text_PaintCenter(leftWidth, yStart + 216, scale, colorWhite, "estimating", 0, iMenuFont);
            if (downloadSize) {
                Text_PaintCenter(leftWidth, yStart + 160, scale, colorWhite, Q_Format("({} {} {} {})", dlSizeBuf, sOf, totalSizeBuf, sCopied).c_str(), 0,
                                 iMenuFont);
            }
            else {
                Text_PaintCenter(leftWidth, yStart + 160, scale, colorWhite, Q_Format("({} {})", dlSizeBuf, sCopied).c_str(), 0, iMenuFont);
            }
        }

        if (xferRate) {
            Text_PaintCenter(leftWidth, yStart + 272, scale, colorWhite, Q_Format("{}/{}", xferRateBuf, sSec).c_str(), 0, iMenuFont);
        }
    }
}

/*
========================
UI_DrawConnectScreen

This will also be overlaid on the cgame info screen during loading
to prevent it from blinking away too rapidly on local or lan games.
========================
*/
void UI_DrawConnectScreen(bool overlay)
{
    const char* s;
    formatText_t sText; // holds the text s points into
    uiClientState_t cstate;
    char info[MAX_INFO_VALUE];
    char text[256];
    float centerPoint, yStart, scale;

    char sStringEdTemp[256];

    menuDef_t* menu = Menus_FindByName("Connect");

    if (!overlay && menu) {
        Menu_Paint(menu, true);
    }

    if (!overlay) {
        centerPoint = 320;
        yStart = 130;
        scale = 1.0f; // -ste
    }
    else {
        centerPoint = 320;
        yStart = 32;
        scale = 1.0f; // -ste
        return;
    }

    // see what information we should display
    trap->GetClientState(&cstate);

    info[0] = '\0';
    if (trap->GetConfigString(CS_SERVERINFO, info, sizeof(info))) {
        trap->SE_GetStringTextString("MENUS_LOADING_MAPNAME", sStringEdTemp, sizeof(sStringEdTemp));
        Text_PaintCenter(centerPoint, yStart, scale, colorWhite,
                         Q_Format(fmt::runtime(/*"Loading {}"*/ sStringEdTemp), Info_ValueForKey(info, "mapname")).c_str(), 0, menuFont_t::FONT_MEDIUM);
    }

    if (!Q_stricmp(cstate.servername, "localhost")) {
        trap->SE_GetStringTextString("MENUS_STARTING_UP", sStringEdTemp, sizeof(sStringEdTemp));
        Text_PaintCenter(centerPoint, yStart + 48, scale, colorWhite, sStringEdTemp, ITEM_TEXTSTYLE_SHADOWEDMORE, menuFont_t::FONT_MEDIUM);
    }
    else {
        trap->SE_GetStringTextString("MENUS_CONNECTING_TO", sStringEdTemp, sizeof(sStringEdTemp));
        Q_strncpyz(text, Q_Format(fmt::runtime(/*"Connecting to {}"*/ sStringEdTemp), cstate.servername).c_str(), sizeof(text));
        Text_PaintCenter(centerPoint, yStart + 48, scale, colorWhite, text, ITEM_TEXTSTYLE_SHADOWEDMORE, menuFont_t::FONT_MEDIUM);
    }

    // display global MOTD at bottom
    Text_PaintCenter(centerPoint, 425, scale, colorWhite, Info_ValueForKey(cstate.updateInfoString, "motd"), 0, menuFont_t::FONT_MEDIUM);
    // print any server info (server full, bad version, etc)
    if (cstate.connState < connstate_t::CA_CONNECTED) {
        Text_PaintCenter(centerPoint, yStart + 176, scale, colorWhite, cstate.messageString, 0, menuFont_t::FONT_MEDIUM);
    }

    switch (cstate.connState) {
    case connstate_t::CA_CONNECTING: {
        trap->SE_GetStringTextString("MENUS_AWAITING_CONNECTION", sStringEdTemp, sizeof(sStringEdTemp));
        s = (sText = Q_Format(fmt::runtime(/*"Awaiting connection...{}"*/ sStringEdTemp), cstate.connectPacketCount)).c_str();
    } break;
    case connstate_t::CA_CHALLENGING: {
        trap->SE_GetStringTextString("MENUS_AWAITING_CHALLENGE", sStringEdTemp, sizeof(sStringEdTemp));
        s = (sText = Q_Format(fmt::runtime(/*"Awaiting challenge...{}"*/ sStringEdTemp), cstate.connectPacketCount)).c_str();
    } break;
    case connstate_t::CA_CONNECTED: {
        char downloadName[MAX_INFO_VALUE];

        trap->Cvar_VariableStringBuffer("cl_downloadName", downloadName, sizeof(downloadName));
        if (*downloadName) {
            UI_DisplayDownloadInfo(downloadName, centerPoint, yStart, scale, menuFont_t::FONT_MEDIUM);
            return;
        }
    }
        trap->SE_GetStringTextString("MENUS_AWAITING_GAMESTATE", sStringEdTemp, sizeof(sStringEdTemp));
        s = /*"Awaiting gamestate..."*/ sStringEdTemp;
        break;
    case connstate_t::CA_LOADING:
        return;
    case connstate_t::CA_PRIMED:
        return;
    default:
        return;
    }

    if (Q_stricmp(cstate.servername, "localhost")) {
        Text_PaintCenter(centerPoint, yStart + 80, scale, colorWhite, s, 0, menuFont_t::FONT_MEDIUM);
    }
    // password required / connection rejected information goes here
}

/*
=================
ArenaServers_StopRefresh
=================
*/
static void UI_StopServerRefresh(void)
{
    int count;

    if (!uiInfo.serverStatus.refreshActive) {
        // not currently refreshing
        return;
    }
    uiInfo.serverStatus.refreshActive = false;
    Q_Println("{} servers listed in browser with {} players.", uiInfo.serverStatus.numDisplayServers, uiInfo.serverStatus.numPlayersOnServers);
    count = trap->LAN_GetServerCount(UI_SourceForLAN());
    if (count - uiInfo.serverStatus.numDisplayServers > 0) {
        Q_Println("{} servers not listed due to filters, packet loss, invalid info, or pings higher than {}", count - uiInfo.serverStatus.numDisplayServers,
                  (int)trap->Cvar_VariableValue("cl_maxPing"));
    }
}

/*
=================
UI_DoServerRefresh
=================
*/
static void UI_DoServerRefresh(void)
{
    bool wait = false;

    if (!uiInfo.serverStatus.refreshActive) {
        return;
    }
    if (UI_NetSource() != uiNetSource_t::UIAS_FAVORITES) {
        if (UI_NetSource() == uiNetSource_t::UIAS_LOCAL) {
            if (!trap->LAN_GetServerCount(lanSource_t::AS_LOCAL)) {
                wait = true;
            }
        }
        else {
            if (trap->LAN_GetServerCount(lanSource_t::AS_GLOBAL) < 0) {
                wait = true;
            }
        }
    }

    if (uiInfo.uiDC.realTime < uiInfo.serverStatus.refreshtime) {
        if (wait) {
            return;
        }
    }

    // if still trying to retrieve pings
    if (trap->LAN_UpdateVisiblePings(UI_SourceForLAN())) {
        uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime + 1000;
    }
    else if (!wait) {
        // get the last servers in the list
        UI_BuildServerDisplayList(2);
        // stop the refresh
        UI_StopServerRefresh();
    }
    //
    UI_BuildServerDisplayList(false);
}

/*
=================
UI_StartServerRefresh
=================
*/
static void UI_StartServerRefresh(bool full)
{
    char* ptr;
    lanSource_t lanSource;

    qtime_t q;
    trap->RealTime(&q);
    trap->Cvar_Set(Q_Format("ui_lastServerRefresh_{}", ui_netSource.integer).c_str(),
                   Q_Format("{}-{}, {} @ {}:{:02}", GetMonthAbbrevString(q.tm_mon), q.tm_mday, 1900 + q.tm_year, q.tm_hour, q.tm_min).c_str());

    if (!full) {
        UI_UpdatePendingPings();
        return;
    }

    uiInfo.serverStatus.refreshActive = true;
    uiInfo.serverStatus.nextDisplayRefresh = uiInfo.uiDC.realTime + 1000;
    // clear number of displayed servers
    uiInfo.serverStatus.numDisplayServers = 0;
    uiInfo.serverStatus.numPlayersOnServers = 0;
    lanSource = UI_SourceForLAN();
    // mark all servers as visible so we store ping updates for them
    trap->LAN_MarkServerVisible(lanSource, -1, true);
    // reset all the pings
    trap->LAN_ResetPings(lanSource);
    //
    if (UI_NetSource() == uiNetSource_t::UIAS_LOCAL) {
        trap->Cmd_ExecuteText(cbufExec_t::EXEC_NOW, "localservers\n");
        uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime + 1000;
        return;
    }

    uiInfo.serverStatus.refreshtime = uiInfo.uiDC.realTime + 5000;
    if (UI_NetSource() >= uiNetSource_t::UIAS_GLOBAL1 && UI_NetSource() <= uiNetSource_t::UIAS_GLOBAL5) {
        ptr = UI_Cvar_VariableString("debug_protocol");
        if (strlen(ptr)) {
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_NOW,
                                  Q_Format("globalservers {} {} full empty\n", ui_netSource.integer, ptr).c_str()); // Internet n asks sv_master<n>
        }
        else {
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_NOW,
                                  Q_Format("globalservers {} {} full empty\n", ui_netSource.integer, (int)trap->Cvar_VariableValue("protocol")).c_str());
        }
    }
}

/*
============
GetModuleAPI
============
*/

uiImport_t* trap = nullptr;

// the module's messages and errors, formatted by the engine (q_fmt.h)
void Q_VPrint(printLevel_t level, lineEnd_t end, fmt::string_view fmt, fmt::format_args args)
{
    trap->Print(level, end, fmt, args);
}

void Q_VError(errorParm_t code, fmt::string_view fmt, fmt::format_args args)
{
    trap->Error(code, fmt, args);
}

extern "C" Q_EXPORT uiExport_t* QDECL GetModuleAPI(int apiVersion, uiImport_t* import)
{
    static uiExport_t uie = {nullptr};

    assert(import);
    trap = import;

    memset(&uie, 0, sizeof(uie));

    if (apiVersion != UI_API_VERSION) {
        Q_Println("Mismatched UI_API_VERSION: expected {}, got {}", UI_API_VERSION, apiVersion);
        return nullptr;
    }

    uie.Init = UI_Init;
    uie.Shutdown = UI_Shutdown;
    uie.KeyEvent = UI_KeyEvent;
    uie.CharEvent = UI_CharEvent;
    uie.MouseEvent = UI_MouseEvent;
    uie.DisplayChanged = UI_DisplayChanged;
    uie.Refresh = UI_Refresh;
    uie.IsFullscreen = Menus_AnyFullScreenVisible;
    uie.SetActiveMenu = UI_SetActiveMenu;
    uie.ConsoleCommand = UI_ConsoleCommand;
    uie.DrawConnectScreen = UI_DrawConnectScreen;
    uie.MenuReset = Menu_Reset;

    return &uie;
}

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

/*
=======================================================================

FORCE INTERFACE

=======================================================================
*/

// use this to get a demo build without an explicit demo build, i.e. to get the demo ui files to build
#include "ui_local.h"
#include "qcommon/qfiles.h"
#include "ui_force.h"

int uiForceSide = FORCE_LIGHTSIDE;
int uiJediNonJedi = -1;
forceMasteries_t uiForceRank = forceMasteries_t::FORCE_MASTERY_JEDI_KNIGHT;
forceMasteries_t uiMaxRank = forceMasteries_t::FORCE_MASTERY_JEDI_MASTER;
int uiMaxPoints = 20;
int uiForceUsed = 0;
int uiForceAvailable = 0;

extern const char* UI_TeamName(team_t team);

bool gTouchedForce = false;

void Menu_ShowItemByName(menuDef_t* menu, const char* p, bool bShow);

EnumArray<bool, forcePowers_t, forcePowers_t::NUM_FORCE_POWERS> uiForcePowersDisabled = {
    false, // FP_HEAL,//instant
    false, // FP_LEVITATION,//hold/duration
    false, // FP_SPEED,//duration
    false, // FP_PUSH,//hold/duration
    false, // FP_PULL,//hold/duration
    false, // FP_TELEPATHY,//instant
    false, // FP_GRIP,//hold/duration
    false, // FP_LIGHTNING,//hold/duration
    false, // FP_RAGE,//duration
    false, // FP_PROTECT,
    false, // FP_ABSORB,
    false, // FP_TEAM_HEAL,
    false, // FP_TEAM_FORCE,
    false, // FP_DRAIN,
    false, // FP_SEE,
    false, // FP_SABER_OFFENSE,
    false, // FP_SABER_DEFENSE,
    false  // FP_SABERTHROW,
};

EnumArray<forcePowerLevels_t, forcePowers_t, forcePowers_t::NUM_FORCE_POWERS> uiForcePowersRank = {
    forcePowerLevels_t::FORCE_LEVEL_0, // forcePowers_t::FP_HEAL = 0,//instant
    forcePowerLevels_t::FORCE_LEVEL_1, // FP_LEVITATION,//hold/duration, this one defaults to 1 (gives a free point)
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_SPEED,//duration
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_PUSH,//hold/duration
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_PULL,//hold/duration
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_TELEPATHY,//instant
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_GRIP,//hold/duration
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_LIGHTNING,//hold/duration
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_RAGE,//duration
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_PROTECT,
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_ABSORB,
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_TEAM_HEAL,
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_TEAM_FORCE,
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_DRAIN,
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_SEE,
    forcePowerLevels_t::FORCE_LEVEL_1, // FP_SABER_OFFENSE, //default to 1 point in attack
    forcePowerLevels_t::FORCE_LEVEL_1, // FP_SABER_DEFENSE, //defualt to 1 point in defense
    forcePowerLevels_t::FORCE_LEVEL_0  // FP_SABERTHROW,
};

EnumArray<int, forcePowers_t, forcePowers_t::NUM_FORCE_POWERS> uiForcePowerDarkLight = // 0 == neutral
    {
        // nothing should be usable at rank 0..
        FORCE_LIGHTSIDE, // FP_HEAL,//instant
        0,               // FP_LEVITATION,//hold/duration
        0,               // FP_SPEED,//duration
        0,               // FP_PUSH,//hold/duration
        0,               // FP_PULL,//hold/duration
        FORCE_LIGHTSIDE, // FP_TELEPATHY,//instant
        FORCE_DARKSIDE,  // FP_GRIP,//hold/duration
        FORCE_DARKSIDE,  // FP_LIGHTNING,//hold/duration
        FORCE_DARKSIDE,  // FP_RAGE,//duration
        FORCE_LIGHTSIDE, // FP_PROTECT,//duration
        FORCE_LIGHTSIDE, // FP_ABSORB,//duration
        FORCE_LIGHTSIDE, // FP_TEAM_HEAL,//instant
        FORCE_DARKSIDE,  // FP_TEAM_FORCE,//instant
        FORCE_DARKSIDE,  // FP_DRAIN,//hold/duration
        0,               // FP_SEE,//duration
        0,               // FP_SABER_OFFENSE,
        0,               // FP_SABER_DEFENSE,
        0                // FP_SABERTHROW,
                         // NUM_FORCE_POWERS
};

int uiForceStarShaders[NUM_FORCE_STAR_IMAGES][2];
EnumArray<int, saber_colors_t, saber_colors_t::NUM_SABER_COLORS> uiSaberColorShaders;
void UI_InitForceShaders(void)
{
    uiForceStarShaders[0][0] = trap->R_RegisterShaderNoMip("forcestar0");
    uiForceStarShaders[0][1] = trap->R_RegisterShaderNoMip("forcestar0");
    uiForceStarShaders[1][0] = trap->R_RegisterShaderNoMip("forcecircle1");
    uiForceStarShaders[1][1] = trap->R_RegisterShaderNoMip("forcestar1");
    uiForceStarShaders[2][0] = trap->R_RegisterShaderNoMip("forcecircle2");
    uiForceStarShaders[2][1] = trap->R_RegisterShaderNoMip("forcestar2");
    uiForceStarShaders[3][0] = trap->R_RegisterShaderNoMip("forcecircle3");
    uiForceStarShaders[3][1] = trap->R_RegisterShaderNoMip("forcestar3");
    uiForceStarShaders[4][0] = trap->R_RegisterShaderNoMip("forcecircle4");
    uiForceStarShaders[4][1] = trap->R_RegisterShaderNoMip("forcestar4");
    uiForceStarShaders[5][0] = trap->R_RegisterShaderNoMip("forcecircle5");
    uiForceStarShaders[5][1] = trap->R_RegisterShaderNoMip("forcestar5");
    uiForceStarShaders[6][0] = trap->R_RegisterShaderNoMip("forcecircle6");
    uiForceStarShaders[6][1] = trap->R_RegisterShaderNoMip("forcestar6");
    uiForceStarShaders[7][0] = trap->R_RegisterShaderNoMip("forcecircle7");
    uiForceStarShaders[7][1] = trap->R_RegisterShaderNoMip("forcestar7");
    uiForceStarShaders[8][0] = trap->R_RegisterShaderNoMip("forcecircle8");
    uiForceStarShaders[8][1] = trap->R_RegisterShaderNoMip("forcestar8");

    uiSaberColorShaders[saber_colors_t::SABER_RED] = trap->R_RegisterShaderNoMip("menu/art/saber_red");
    uiSaberColorShaders[saber_colors_t::SABER_ORANGE] = trap->R_RegisterShaderNoMip("menu/art/saber_orange");
    uiSaberColorShaders[saber_colors_t::SABER_YELLOW] = trap->R_RegisterShaderNoMip("menu/art/saber_yellow");
    uiSaberColorShaders[saber_colors_t::SABER_GREEN] = trap->R_RegisterShaderNoMip("menu/art/saber_green");
    uiSaberColorShaders[saber_colors_t::SABER_BLUE] = trap->R_RegisterShaderNoMip("menu/art/saber_blue");
    uiSaberColorShaders[saber_colors_t::SABER_PURPLE] = trap->R_RegisterShaderNoMip("menu/art/saber_purple");
}

// Draw the stars spent on the current force power
void UI_DrawForceStars(rectDef_t* rect, float scale, vec4_t color, int textStyle, forcePowers_t forceindex, forcePowerLevels_t val, forcePowerLevels_t min,
                       forcePowerLevels_t max)
{
    int pad = 4;
    int xPos, width = 16;
    int starcolor;

    if (val < min || val > max) {
        val = min;
    }

    if (1) // if (val)
    {
        xPos = rect->x;

        for (forcePowerLevels_t i = forcePowerLevels_t::FORCE_LEVEL_1; i <= max; i++) {
            starcolor = bgForcePowerCost[forceindex][i];

            if (uiForcePowersDisabled[forceindex]) {
                vec4_t grColor = {0.2f, 0.2f, 0.2f, 1.0f};
                trap->R_SetColor(grColor);
            }

            if (val >= i) { // Draw a star.
                UI_DrawHandlePic(xPos, rect->y + 6, width, width, uiForceStarShaders[starcolor][1]);
            }
            else { // Draw a circle.
                UI_DrawHandlePic(xPos, rect->y + 6, width, width, uiForceStarShaders[starcolor][0]);
            }

            if (uiForcePowersDisabled[forceindex]) {
                trap->R_SetColor(nullptr);
            }

            xPos += width + pad;
        }
    }
}

// Set the client's force power layout.
void UI_UpdateClientForcePowers(const char* teamArg)
{
    trap->Cvar_Set("forcepowers",
                   va("%i-%i-%i%i%i%i%i%i%i%i%i%i%i%i%i%i%i%i%i%i", to_underlying(uiForceRank), uiForceSide,
                      to_underlying(uiForcePowersRank[forcePowers_t::FP_HEAL]), to_underlying(uiForcePowersRank[forcePowers_t::FP_LEVITATION]),
                      to_underlying(uiForcePowersRank[forcePowers_t::FP_SPEED]), to_underlying(uiForcePowersRank[forcePowers_t::FP_PUSH]),
                      to_underlying(uiForcePowersRank[forcePowers_t::FP_PULL]), to_underlying(uiForcePowersRank[forcePowers_t::FP_TELEPATHY]),
                      to_underlying(uiForcePowersRank[forcePowers_t::FP_GRIP]), to_underlying(uiForcePowersRank[forcePowers_t::FP_LIGHTNING]),
                      to_underlying(uiForcePowersRank[forcePowers_t::FP_RAGE]), to_underlying(uiForcePowersRank[forcePowers_t::FP_PROTECT]),
                      to_underlying(uiForcePowersRank[forcePowers_t::FP_ABSORB]), to_underlying(uiForcePowersRank[forcePowers_t::FP_TEAM_HEAL]),
                      to_underlying(uiForcePowersRank[forcePowers_t::FP_TEAM_FORCE]), to_underlying(uiForcePowersRank[forcePowers_t::FP_DRAIN]),
                      to_underlying(uiForcePowersRank[forcePowers_t::FP_SEE]), to_underlying(uiForcePowersRank[forcePowers_t::FP_SABER_OFFENSE]),
                      to_underlying(uiForcePowersRank[forcePowers_t::FP_SABER_DEFENSE]), to_underlying(uiForcePowersRank[forcePowers_t::FP_SABERTHROW])));

    if (gTouchedForce) {
        if (teamArg && teamArg[0]) {
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, va("forcechanged \"%s\"\n", teamArg));
        }
        else {
            trap->Cmd_ExecuteText(cbufExec_t::EXEC_APPEND, "forcechanged\n");
        }
    }

    gTouchedForce = false;
}

int UI_TranslateFCFIndex(int index)
{
    if (uiForceSide == FORCE_LIGHTSIDE) {
        return index - uiInfo.forceConfigLightIndexBegin;
    }

    return index - uiInfo.forceConfigDarkIndexBegin;
}

void UI_SaveForceTemplate()
{
    char* selectedName = UI_Cvar_VariableString("ui_SaveFCF");
    char fcfString[512];
    char forceStringValue[4];
    fileHandle_t f;
    int strPlace = 0;
    forcePowers_t forcePlace = forcePowers_t::FP_FIRST;
    int i = 0;
    bool foundFeederItem = false;

    if (!selectedName || !selectedName[0]) {
        Com_Printf("You did not provide a name for the template.\n");
        return;
    }

    if (uiForceSide == FORCE_LIGHTSIDE) { // write it into the light side folder
        trap->FS_Open(va("forcecfg/light/%s.fcf", selectedName), &f, fsMode_t::FS_WRITE);
    }
    else { // if it isn't light it must be dark
        trap->FS_Open(va("forcecfg/dark/%s.fcf", selectedName), &f, fsMode_t::FS_WRITE);
    }

    if (!f) {
        Com_Printf("There was an error writing the template file (read-only?).\n");
        return;
    }

    Com_sprintf(fcfString, sizeof(fcfString), "%i-%i-", to_underlying(uiForceRank), uiForceSide);
    strPlace = strlen(fcfString);

    while (forcePlace < forcePowers_t::NUM_FORCE_POWERS) {
        Com_sprintf(forceStringValue, sizeof(forceStringValue), "%i", to_underlying(uiForcePowersRank[forcePlace]));
        // Just use the force digit even if multiple digits. Shouldn't be longer than 1.
        fcfString[strPlace] = forceStringValue[0];
        strPlace++;
        forcePlace++;
    }
    fcfString[strPlace] = '\n';
    fcfString[strPlace + 1] = 0;

    trap->FS_Write(fcfString, strlen(fcfString), f);
    trap->FS_Close(f);

    Com_Printf("Template saved as \"%s\".\n", selectedName);

    // Now, update the FCF list
    UI_LoadForceConfig_List();

    // Then, scroll through and select the template for the file we just saved
    while (i < uiInfo.forceConfigCount) {
        if (!Q_stricmp(uiInfo.forceConfigNames[i], selectedName)) {
            if ((uiForceSide == FORCE_LIGHTSIDE && uiInfo.forceConfigSide[i]) || (uiForceSide == FORCE_DARKSIDE && !uiInfo.forceConfigSide[i])) {
                Menu_SetFeederSelection(nullptr, FEEDER_FORCECFG, UI_TranslateFCFIndex(i), nullptr);
                foundFeederItem = true;
            }
        }

        i++;
    }

    // Else, go back to 0
    if (!foundFeederItem) {
        Menu_SetFeederSelection(nullptr, FEEDER_FORCECFG, 0, nullptr);
    }
}

//
extern bool UI_TrueJediEnabled(void);
void UpdateForceUsed()
{
    forcePowers_t curpower;
    menuDef_t* menu;

    // Currently we don't make a distinction between those that wish to play Jedi of lower than maximum skill.
    uiForceRank = uiMaxRank;

    uiForceUsed = 0;
    uiForceAvailable = forceMasteryPoints[uiForceRank];

    // Make sure that we have one freebie in jump.
    if (uiForcePowersRank[forcePowers_t::FP_LEVITATION] < forcePowerLevels_t::FORCE_LEVEL_1) {
        uiForcePowersRank[forcePowers_t::FP_LEVITATION] = forcePowerLevels_t::FORCE_LEVEL_1;
    }

    if (UI_TrueJediEnabled()) { // true jedi mode is set
        if (uiJediNonJedi == -1) {
            forcePowers_t x = forcePowers_t::FP_FIRST;
            bool clear = false, update = false;
            uiJediNonJedi = FORCE_NONJEDI;
            while (x < forcePowers_t::NUM_FORCE_POWERS) { // if any force power is set, we must be a jedi
                if (x == forcePowers_t::FP_LEVITATION || x == forcePowers_t::FP_SABER_OFFENSE) {
                    if (uiForcePowersRank[x] > forcePowerLevels_t::FORCE_LEVEL_1) {
                        uiJediNonJedi = FORCE_JEDI;
                        break;
                    }
                    else if (uiForcePowersRank[x] > forcePowerLevels_t::FORCE_LEVEL_0) {
                        clear = true;
                    }
                }
                else if (uiForcePowersRank[x] > forcePowerLevels_t::FORCE_LEVEL_0) {
                    uiJediNonJedi = FORCE_JEDI;
                    break;
                }
                x++;
            }
            if (uiJediNonJedi == FORCE_JEDI) {
                if (uiForcePowersRank[forcePowers_t::FP_SABER_OFFENSE] < forcePowerLevels_t::FORCE_LEVEL_1) {
                    uiForcePowersRank[forcePowers_t::FP_SABER_OFFENSE] = forcePowerLevels_t::FORCE_LEVEL_1;
                    update = true;
                }
            }
            else if (clear) {
                x = forcePowers_t::FP_FIRST;
                while (x < forcePowers_t::NUM_FORCE_POWERS) { // clear all force
                    uiForcePowersRank[x] = forcePowerLevels_t::FORCE_LEVEL_0;
                    x++;
                }
                update = true;
            }
            if (update) {
                team_t myTeam;
                myTeam = UI_CvarTeam("ui_myteam");
                if (myTeam != team_t::TEAM_SPECTATOR) {
                    UI_UpdateClientForcePowers(UI_TeamName(myTeam)); // will cause him to respawn, if it's been 5 seconds since last one
                }
                else {
                    UI_UpdateClientForcePowers(nullptr); // just update powers
                }
            }
        }
    }

    menu = Menus_FindByName("ingame_playerforce");
    // Set the cost of the saberattack according to whether its free.
    if (ui_freeSaber.integer) { // Make saber free
        bgForcePowerCost[forcePowers_t::FP_SABER_OFFENSE][forcePowerLevels_t::FORCE_LEVEL_1] = 0;
        bgForcePowerCost[forcePowers_t::FP_SABER_DEFENSE][forcePowerLevels_t::FORCE_LEVEL_1] = 0;
        // Make sure that we have one freebie in saber if applicable.
        if (uiForcePowersRank[forcePowers_t::FP_SABER_OFFENSE] < forcePowerLevels_t::FORCE_LEVEL_1) {
            uiForcePowersRank[forcePowers_t::FP_SABER_OFFENSE] = forcePowerLevels_t::FORCE_LEVEL_1;
        }
        if (uiForcePowersRank[forcePowers_t::FP_SABER_DEFENSE] < forcePowerLevels_t::FORCE_LEVEL_1) {
            uiForcePowersRank[forcePowers_t::FP_SABER_DEFENSE] = forcePowerLevels_t::FORCE_LEVEL_1;
        }
        if (menu) {
            Menu_ShowItemByName(menu, "setFP_SABER_DEFENSE", true);
            Menu_ShowItemByName(menu, "setfp_saberthrow", true);
            Menu_ShowItemByName(menu, "effectentry", true);
            Menu_ShowItemByName(menu, "effectfield", true);
            Menu_ShowItemByName(menu, "nosaber", false);
        }
    }
    else { // Make saber normal cost
        bgForcePowerCost[forcePowers_t::FP_SABER_OFFENSE][forcePowerLevels_t::FORCE_LEVEL_1] = 1;
        bgForcePowerCost[forcePowers_t::FP_SABER_DEFENSE][forcePowerLevels_t::FORCE_LEVEL_1] = 1;
        // Also, check if there is no saberattack.  If there isn't, there had better not be any defense or throw!
        if (uiForcePowersRank[forcePowers_t::FP_SABER_OFFENSE] < forcePowerLevels_t::FORCE_LEVEL_1) {
            uiForcePowersRank[forcePowers_t::FP_SABER_DEFENSE] = forcePowerLevels_t::FORCE_LEVEL_0;
            uiForcePowersRank[forcePowers_t::FP_SABERTHROW] = forcePowerLevels_t::FORCE_LEVEL_0;
            if (menu) {
                Menu_ShowItemByName(menu, "setfp_saberdefend", false);
                Menu_ShowItemByName(menu, "setfp_saberthrow", false);
                Menu_ShowItemByName(menu, "effectentry", false);
                Menu_ShowItemByName(menu, "effectfield", false);
                Menu_ShowItemByName(menu, "nosaber", true);
            }
        }
        else {
            if (menu) {
                Menu_ShowItemByName(menu, "setfp_saberdefend", true);
                Menu_ShowItemByName(menu, "setfp_saberthrow", true);
                Menu_ShowItemByName(menu, "effectentry", true);
                Menu_ShowItemByName(menu, "effectfield", true);
                Menu_ShowItemByName(menu, "nosaber", false);
            }
        }
    }

    // Make sure that we're still legal.
    for (curpower = forcePowers_t::FP_FIRST; curpower < forcePowers_t::NUM_FORCE_POWERS; curpower++) { // Make sure that our ranks are within legal limits.
        if (uiForcePowersRank[curpower] < forcePowerLevels_t::FORCE_LEVEL_0)
            uiForcePowersRank[curpower] = forcePowerLevels_t::FORCE_LEVEL_0;
        else if (uiForcePowersRank[curpower] >= forcePowerLevels_t::NUM_FORCE_POWER_LEVELS)
            uiForcePowersRank[curpower] = (forcePowerLevels_t::NUM_FORCE_POWER_LEVELS - 1);

        for (forcePowerLevels_t currank = forcePowerLevels_t::FORCE_LEVEL_1; currank <= uiForcePowersRank[curpower]; currank++) { // Check on this force power
            if (uiForcePowersRank[curpower] >
                forcePowerLevels_t::FORCE_LEVEL_0) { // Do not charge the player for the one freebie in jump, or if there is one in saber.
                if ((curpower == forcePowers_t::FP_LEVITATION && currank == forcePowerLevels_t::FORCE_LEVEL_1) ||
                    (curpower == forcePowers_t::FP_SABER_OFFENSE && currank == forcePowerLevels_t::FORCE_LEVEL_1 && ui_freeSaber.integer) ||
                    (curpower == forcePowers_t::FP_SABER_DEFENSE && currank == forcePowerLevels_t::FORCE_LEVEL_1 && ui_freeSaber.integer)) {
                    // Do nothing (written this way for clarity)
                }
                else {                                                            // Check if we can accrue the cost of this power.
                    if (bgForcePowerCost[curpower][currank] > uiForceAvailable) { // We can't afford this power.  Break to the next one.
                        // Remove this power from the player's roster.
                        uiForcePowersRank[curpower] = currank - 1;
                        break;
                    }
                    else { // Sure we can afford it.
                        uiForceUsed += bgForcePowerCost[curpower][currank];
                        uiForceAvailable -= bgForcePowerCost[curpower][currank];
                    }
                }
            }
        }
    }
}

// Mostly parts of other functions merged into one another.
// Puts the current UI stuff into a string, legalizes it, and then reads it back out.
void UI_ReadLegalForce(void)
{
    char fcfString[512];
    char forceStringValue[4];
    int strPlace = 0;
    forcePowers_t forcePlace = forcePowers_t::FP_FIRST;
    int i = 0;
    char singleBuf[64];
    char info[MAX_INFO_VALUE];
    int c = 0;
    int iBuf = 0;
    forcePowerLevels_t forcePowerRank = forcePowerLevels_t::FORCE_LEVEL_0;
    forcePowerLevels_t currank = forcePowerLevels_t::FORCE_LEVEL_0;
    int forceTeam = 0;
    bool updateForceLater = false;

    // First, stick them into a string.
    Com_sprintf(fcfString, sizeof(fcfString), "%i-%i-", to_underlying(uiForceRank), uiForceSide);
    strPlace = strlen(fcfString);

    while (forcePlace < forcePowers_t::NUM_FORCE_POWERS) {
        Com_sprintf(forceStringValue, sizeof(forceStringValue), "%i", to_underlying(uiForcePowersRank[forcePlace]));
        // Just use the force digit even if multiple digits. Shouldn't be longer than 1.
        fcfString[strPlace] = forceStringValue[0];
        strPlace++;
        forcePlace++;
    }
    fcfString[strPlace] = '\n';
    fcfString[strPlace + 1] = 0;

    info[0] = '\0';
    trap->GetConfigString(CS_SERVERINFO, info, sizeof(info));

    if (atoi(Info_ValueForKey(info, "g_forceBasedTeams"))) {
        switch (UI_CvarTeam("ui_myteam")) {
        case team_t::TEAM_RED:
            forceTeam = FORCE_DARKSIDE;
            break;
        case team_t::TEAM_BLUE:
            forceTeam = FORCE_LIGHTSIDE;
            break;
        default:
            break;
        }
    }
    // Second, legalize them.
    if (!BG_LegalizedForcePowers(fcfString, sizeof(fcfString), uiMaxRank, ui_freeSaber.integer, forceTeam,
                                 static_cast<gametype_t>(atoi(Info_ValueForKey(info, "g_gametype"))), 0)) { // if they were illegal, we should refresh them.
        updateForceLater = true;
    }

    // Lastly, put them back into the UI storage from the legalized string
    i = 0;

    while (fcfString[i] && fcfString[i] != '-') {
        singleBuf[c] = fcfString[i];
        c++;
        i++;
    }
    singleBuf[c] = 0;
    c = 0;
    i++;

    const forceMasteries_t rank = static_cast<forceMasteries_t>(atoi(singleBuf));

    if (rank > uiMaxRank || rank < forceMasteries_t::FORCE_MASTERY_UNINITIATED) { // this force config uses a rank level higher than our currently restricted
                                                                                  // level.. so we can't use it
                                                                                  // FIXME: Print a message indicating this to the user
        //	return;
    }

    uiForceRank = rank;

    while (fcfString[i] && fcfString[i] != '-') {
        singleBuf[c] = fcfString[i];
        c++;
        i++;
    }
    singleBuf[c] = 0;
    c = 0;
    i++;

    uiForceSide = atoi(singleBuf);

    if (uiForceSide != FORCE_LIGHTSIDE && uiForceSide != FORCE_DARKSIDE) {
        uiForceSide = FORCE_LIGHTSIDE;
        return;
    }

    // clear out the existing powers
    for (forcePowers_t fp = forcePowers_t::FP_FIRST; fp < forcePowers_t::NUM_FORCE_POWERS; fp++) {
        uiForcePowersRank[fp] = forcePowerLevels_t::FORCE_LEVEL_0;
    }
    uiForceUsed = 0;
    uiForceAvailable = forceMasteryPoints[uiForceRank];
    gTouchedForce = true;

    for (forcePowers_t fp = forcePowers_t::FP_FIRST; fcfString[i] && fp < forcePowers_t::NUM_FORCE_POWERS; fp++, i++) {
        singleBuf[0] = fcfString[i];
        singleBuf[1] = 0;
        iBuf = atoi(singleBuf); // So, that means that Force Power "fp" wants to be set to rank "iBuf".

        if (iBuf < 0) {
            iBuf = 0;
        }

        forcePowerRank = static_cast<forcePowerLevels_t>(iBuf);

        if (forcePowerRank > forcePowerLevels_t::FORCE_LEVEL_3 || forcePowerRank < forcePowerLevels_t::FORCE_LEVEL_0) { // err..  not correct
            continue;                                                                                                   // skip this power
        }

        if (uiForcePowerDarkLight[fp] &&
            uiForcePowerDarkLight[fp] != uiForceSide) { // Apparently the user has crafted a force config that has powers that don't fit with the config's side.
            continue;                                   // skip this power
        }

        // Accrue cost for each assigned rank for this power.
        for (currank = forcePowerLevels_t::FORCE_LEVEL_1; currank <= forcePowerRank; currank++) {
            if (bgForcePowerCost[fp][currank] > uiForceAvailable) { // Break out, we can't afford any more power.
                break;
            }
            // Pay for this rank of this power.
            uiForceUsed += bgForcePowerCost[fp][currank];
            uiForceAvailable -= bgForcePowerCost[fp][currank];

            uiForcePowersRank[fp]++;
        }
    }

    if (uiForcePowersRank[forcePowers_t::FP_LEVITATION] < forcePowerLevels_t::FORCE_LEVEL_1) {
        uiForcePowersRank[forcePowers_t::FP_LEVITATION] = forcePowerLevels_t::FORCE_LEVEL_1;
    }
    if (uiForcePowersRank[forcePowers_t::FP_SABER_OFFENSE] < forcePowerLevels_t::FORCE_LEVEL_1 && ui_freeSaber.integer) {
        uiForcePowersRank[forcePowers_t::FP_SABER_OFFENSE] = forcePowerLevels_t::FORCE_LEVEL_1;
    }
    if (uiForcePowersRank[forcePowers_t::FP_SABER_DEFENSE] < forcePowerLevels_t::FORCE_LEVEL_1 && ui_freeSaber.integer) {
        uiForcePowersRank[forcePowers_t::FP_SABER_DEFENSE] = forcePowerLevels_t::FORCE_LEVEL_1;
    }

    UpdateForceUsed();

    if (updateForceLater) {
        gTouchedForce = true;
        UI_UpdateClientForcePowers(nullptr);
    }
}

void UI_UpdateForcePowers()
{
    char* forcePowers = UI_Cvar_VariableString("forcepowers");
    char readBuf[256];
    int i = 0, i_r = 0;
    forcePowers_t i_f = forcePowers_t::FP_FIRST;

    uiForceSide = 0;

    if (forcePowers && forcePowers[0]) {
        while (forcePowers[i]) {
            i_r = 0;

            while (forcePowers[i] && forcePowers[i] != '-' && i_r < 255) {
                readBuf[i_r] = forcePowers[i];
                i_r++;
                i++;
            }
            readBuf[i_r] = '\0';
            if (i_r >= 255 || !forcePowers[i] || forcePowers[i] != '-') {
                uiForceSide = 0;
                goto validitycheck;
            }
            uiForceRank = static_cast<forceMasteries_t>(atoi(readBuf));
            i_r = 0;

            if (uiForceRank > uiMaxRank) {
                uiForceRank = uiMaxRank;
            }

            i++;

            while (forcePowers[i] && forcePowers[i] != '-' && i_r < 255) {
                readBuf[i_r] = forcePowers[i];
                i_r++;
                i++;
            }
            readBuf[i_r] = '\0';
            if (i_r >= 255 || !forcePowers[i] || forcePowers[i] != '-') {
                uiForceSide = 0;
                goto validitycheck;
            }
            uiForceSide = atoi(readBuf);
            i_r = 0;

            i++;

            i_f = forcePowers_t::FP_HEAL;

            while (forcePowers[i] && i_f < forcePowers_t::NUM_FORCE_POWERS) {
                readBuf[0] = forcePowers[i];
                readBuf[1] = '\0';
                uiForcePowersRank[i_f] = static_cast<forcePowerLevels_t>(atoi(readBuf));

                if (i_f == forcePowers_t::FP_LEVITATION && uiForcePowersRank[i_f] < forcePowerLevels_t::FORCE_LEVEL_1) {
                    uiForcePowersRank[i_f] = forcePowerLevels_t::FORCE_LEVEL_1;
                }

                if (i_f == forcePowers_t::FP_SABER_OFFENSE && uiForcePowersRank[i_f] < forcePowerLevels_t::FORCE_LEVEL_1 && ui_freeSaber.integer) {
                    uiForcePowersRank[i_f] = forcePowerLevels_t::FORCE_LEVEL_1;
                }

                if (i_f == forcePowers_t::FP_SABER_DEFENSE && uiForcePowersRank[i_f] < forcePowerLevels_t::FORCE_LEVEL_1 && ui_freeSaber.integer) {
                    uiForcePowersRank[i_f] = forcePowerLevels_t::FORCE_LEVEL_1;
                }

                i_f++;
                i++;
            }

            if (i_f < forcePowers_t::NUM_FORCE_POWERS) { // info for all the powers wasn't there..
                uiForceSide = 0;
                goto validitycheck;
            }
            i++;
        }
    }

validitycheck:

    if (!uiForceSide) {
        uiForceSide = 1;
        uiForceRank = forceMasteries_t::FORCE_MASTERY_INITIATE;
        forcePowers_t fp = forcePowers_t::FP_FIRST;
        while (fp < forcePowers_t::NUM_FORCE_POWERS) {
            if (fp == forcePowers_t::FP_LEVITATION) {
                uiForcePowersRank[fp] = forcePowerLevels_t::FORCE_LEVEL_1;
            }
            else if (fp == forcePowers_t::FP_SABER_OFFENSE && ui_freeSaber.integer) {
                uiForcePowersRank[fp] = forcePowerLevels_t::FORCE_LEVEL_1;
            }
            else if (fp == forcePowers_t::FP_SABER_DEFENSE && ui_freeSaber.integer) {
                uiForcePowersRank[fp] = forcePowerLevels_t::FORCE_LEVEL_1;
            }
            else {
                uiForcePowersRank[fp] = forcePowerLevels_t::FORCE_LEVEL_0;
            }

            fp++;
        }

        UI_UpdateClientForcePowers(nullptr);
    }

    UpdateForceUsed();
}
extern team_t uiSkinColor;
extern team_t uiHoldSkinColor;

bool UI_SkinColor_HandleKey(int flags, float* special, fakeAscii_t key, team_t num, team_t min, team_t max, int type)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        team_t i = num;

        if (key == fakeAscii_t::A_MOUSE2) {
            i--;
        }
        else {
            i++;
        }

        if (i < min) {
            i = max;
        }
        else if (i > max) {
            i = min;
        }

        num = i;

        uiSkinColor = num;

        uiHoldSkinColor = uiSkinColor;

        UI_FeederSelection(FEEDER_Q3HEADS, uiInfo.q3SelectedHead, nullptr);

        return true;
    }
    return false;
}

bool UI_ForceSide_HandleKey(int flags, float* special, fakeAscii_t key, int num, int min, int max, int type)
{
    char info[MAX_INFO_VALUE];

    info[0] = '\0';
    trap->GetConfigString(CS_SERVERINFO, info, sizeof(info));

    if (atoi(Info_ValueForKey(info, "g_forceBasedTeams"))) {
        switch (UI_CvarTeam("ui_myteam")) {
        case team_t::TEAM_RED:
            return false;
        case team_t::TEAM_BLUE:
            return false;
        default:
            break;
        }
    }

    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        int i = num;
        forcePowers_t x = forcePowers_t::FP_FIRST;

        // update the feeder item selection, it might be different depending on side
        Menu_SetFeederSelection(nullptr, FEEDER_FORCECFG, 0, nullptr);

        if (key == fakeAscii_t::A_MOUSE2) {
            i--;
        }
        else {
            i++;
        }

        if (i < min) {
            i = max;
        }
        else if (i > max) {
            i = min;
        }

        num = i;

        uiForceSide = num;

        // Resetting power ranks based on if light or dark side is chosen
        while (x < forcePowers_t::NUM_FORCE_POWERS) {
            if (uiForcePowerDarkLight[x] && uiForceSide != uiForcePowerDarkLight[x]) {
                uiForcePowersRank[x] = forcePowerLevels_t::FORCE_LEVEL_0;
            }
            x++;
        }

        UpdateForceUsed();

        gTouchedForce = true;
        return true;
    }
    return false;
}

bool UI_JediNonJedi_HandleKey(int flags, float* special, fakeAscii_t key, int num, int min, int max, int type)
{
    char info[MAX_INFO_VALUE];

    info[0] = '\0';
    trap->GetConfigString(CS_SERVERINFO, info, sizeof(info));

    if (!UI_TrueJediEnabled()) { // true jedi mode is not set
        return false;
    }

    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        int i = num;
        forcePowers_t x = forcePowers_t::FP_FIRST;

        if (key == fakeAscii_t::A_MOUSE2) {
            i--;
        }
        else {
            i++;
        }

        if (i < min) {
            i = max;
        }
        else if (i > max) {
            i = min;
        }

        num = i;

        uiJediNonJedi = num;

        // Resetting power ranks based on if light or dark side is chosen
        if (!num) { // not a jedi?
            team_t myTeam = UI_CvarTeam("ui_myteam");
            while (x < forcePowers_t::NUM_FORCE_POWERS) { // clear all force powers
                uiForcePowersRank[x] = forcePowerLevels_t::FORCE_LEVEL_0;
                x++;
            }
            if (myTeam != team_t::TEAM_SPECTATOR) {
                UI_UpdateClientForcePowers(UI_TeamName(myTeam)); // will cause him to respawn, if it's been 5 seconds since last one
            }
            else {
                UI_UpdateClientForcePowers(nullptr); // just update powers
            }
        }
        else if (num) { // a jedi, set the minimums, hopefuly they know to set the rest!
            if (uiForcePowersRank[forcePowers_t::FP_LEVITATION] < forcePowerLevels_t::FORCE_LEVEL_1) { // force jump 1 minimum
                uiForcePowersRank[forcePowers_t::FP_LEVITATION] = forcePowerLevels_t::FORCE_LEVEL_1;
            }
            if (uiForcePowersRank[forcePowers_t::FP_SABER_OFFENSE] < forcePowerLevels_t::FORCE_LEVEL_1) { // saber attack 1, minimum
                uiForcePowersRank[forcePowers_t::FP_SABER_OFFENSE] = forcePowerLevels_t::FORCE_LEVEL_1;
            }
        }

        UpdateForceUsed();

        gTouchedForce = true;
        return true;
    }
    return false;
}

bool UI_ForceMaxRank_HandleKey(int flags, float* special, fakeAscii_t key, forceMasteries_t num, forceMasteries_t min, forceMasteries_t max, int type)
{
    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        forceMasteries_t i = num;

        if (key == fakeAscii_t::A_MOUSE2) {
            i--;
        }
        else {
            i++;
        }

        if (i < min) {
            i = max;
        }
        else if (i > max) {
            i = min;
        }

        num = i;

        uiMaxRank = num;

        trap->Cvar_Set("g_maxForceRank", va("%i", to_underlying(num)));

        // The update force used will remove overallocated powers automatically.
        UpdateForceUsed();

        gTouchedForce = true;

        return true;
    }
    return false;
}

// the force power of an owner-draw from UI_FORCE_RANK_HEAL to UI_FORCE_RANK_SABERTHROW (menudef.h), in the powers' order
forcePowers_t UI_ForceRankPower(int ownerDraw)
{
    return static_cast<forcePowers_t>(ownerDraw - UI_FORCE_RANK - 1);
}

// This function will either raise or lower a power by one rank.
bool UI_ForcePowerRank_HandleKey(int flags, float* special, fakeAscii_t key, forcePowerLevels_t num, forcePowerLevels_t min, forcePowerLevels_t max, int type)
{
    bool raising;

    if (key == fakeAscii_t::A_MOUSE1 || key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER ||
        key == fakeAscii_t::A_BACKSPACE) {
        forcePowers_t forcepower;
        forcePowerLevels_t rank;

        // this will give us the index as long as UI_FORCE_RANK is always one below the first force rank index
        forcepower = UI_ForceRankPower(type);

        // the power is disabled on the server
        if (uiForcePowersDisabled[forcepower]) {
            return true;
        }

        // If we are not on the same side as a power, or if we are not of any rank at all.
        if (uiForcePowerDarkLight[forcepower] && uiForceSide != uiForcePowerDarkLight[forcepower]) {
            return true;
        }
        else if (forcepower == forcePowers_t::FP_SABER_DEFENSE ||
                 forcepower == forcePowers_t::FP_SABERTHROW) { // Saberdefend and saberthrow can't be bought if there is no saberattack
            if (uiForcePowersRank[forcePowers_t::FP_SABER_OFFENSE] < forcePowerLevels_t::FORCE_LEVEL_1) {
                return true;
            }
        }

        if (type == UI_FORCE_RANK_LEVITATION) {
            min += 1;
        }
        if (type == UI_FORCE_RANK_SABERATTACK && ui_freeSaber.integer) {
            min += 1;
        }
        if (type == UI_FORCE_RANK_SABERDEFEND && ui_freeSaber.integer) {
            min += 1;
        }

        if (key == fakeAscii_t::A_MOUSE2 || key == fakeAscii_t::A_BACKSPACE) { // Lower a point.
            if (uiForcePowersRank[forcepower] <= min) {
                return true;
            }
            raising = false;
        }
        else { // Raise a point.
            if (uiForcePowersRank[forcepower] >= max) {
                return true;
            }
            raising = true;
        }

        if (raising) { // Check if we can accrue the cost of this power.
            rank = uiForcePowersRank[forcepower] + 1;
            if (bgForcePowerCost[forcepower][rank] > uiForceAvailable) { // We can't afford this power.  Abandon ship.
                return true;
            }
            else { // Sure we can afford it.
                uiForceUsed += bgForcePowerCost[forcepower][rank];
                uiForceAvailable -= bgForcePowerCost[forcepower][rank];
                uiForcePowersRank[forcepower] = rank;
            }
        }
        else { // Lower the point.
            rank = uiForcePowersRank[forcepower];
            uiForceUsed -= bgForcePowerCost[forcepower][rank];
            uiForceAvailable += bgForcePowerCost[forcepower][rank];
            uiForcePowersRank[forcepower]--;
        }

        UpdateForceUsed();

        gTouchedForce = true;

        return true;
    }
    return false;
}

forceMasteries_t gCustRank = forceMasteries_t::FORCE_MASTERY_UNINITIATED;
int gCustSide = 0;

EnumArray<forcePowerLevels_t, forcePowers_t, forcePowers_t::NUM_FORCE_POWERS> gCustPowersRank = {
    forcePowerLevels_t::FORCE_LEVEL_0, // forcePowers_t::FP_HEAL = 0,//instant
    forcePowerLevels_t::FORCE_LEVEL_1, // FP_LEVITATION,//hold/duration, this one defaults to 1 (gives a free point)
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_SPEED,//duration
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_PUSH,//hold/duration
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_PULL,//hold/duration
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_TELEPATHY,//instant
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_GRIP,//hold/duration
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_LIGHTNING,//hold/duration
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_RAGE,//duration
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_PROTECT,
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_ABSORB,
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_TEAM_HEAL,
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_TEAM_FORCE,
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_DRAIN,
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_SEE,
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_SABER_OFFENSE,
    forcePowerLevels_t::FORCE_LEVEL_0, // FP_SABER_DEFENSE,
    forcePowerLevels_t::FORCE_LEVEL_0  // FP_SABERTHROW,
};

/*
=================
UI_ForceConfigHandle
=================
*/
void UI_ForceConfigHandle(int oldindex, int newindex)
{
    fileHandle_t f;
    int len = 0;
    int i = 0;
    int c = 0;
    int iBuf = 0;
    forcePowerLevels_t forcePowerRank, currank;
    char fcfBuffer[8192];
    char singleBuf[64];
    char info[MAX_INFO_VALUE];
    int forceTeam = 0;

    if (oldindex == 0) { // switching out from custom config, so first shove the current values into the custom storage
        forcePowers_t fp = forcePowers_t::FP_FIRST;

        while (fp < forcePowers_t::NUM_FORCE_POWERS) {
            gCustPowersRank[fp] = uiForcePowersRank[fp];
            fp++;
        }
        gCustRank = uiForceRank;
        gCustSide = uiForceSide;
    }

    if (newindex == 0) { // switching back to custom, shove the values back in from the custom storage
        forcePowers_t fp = forcePowers_t::FP_FIRST;
        uiForceUsed = 0;
        gTouchedForce = true;

        while (fp < forcePowers_t::NUM_FORCE_POWERS) {
            uiForcePowersRank[fp] = gCustPowersRank[fp];
            uiForceUsed += to_underlying(uiForcePowersRank[fp]);
            fp++;
        }
        uiForceRank = gCustRank;
        uiForceSide = gCustSide;

        UpdateForceUsed();
        return;
    }

    // If we made it here, we want to load in a new config
    if (uiForceSide == FORCE_LIGHTSIDE) { // we should only be displaying lightside configs, so.. look in the light folder
        newindex += uiInfo.forceConfigLightIndexBegin;
        if (newindex >= uiInfo.forceConfigCount)
            return;
        len = trap->FS_Open(va("forcecfg/light/%s.fcf", uiInfo.forceConfigNames[newindex]), &f, fsMode_t::FS_READ);
    }
    else { // else dark
        newindex += uiInfo.forceConfigDarkIndexBegin;
        if (newindex >= uiInfo.forceConfigCount || newindex > uiInfo.forceConfigLightIndexBegin) { // dark gets read in before light
            return;
        }
        len = trap->FS_Open(va("forcecfg/dark/%s.fcf", uiInfo.forceConfigNames[newindex]), &f, fsMode_t::FS_READ);
    }

    if (len <= 0) { // This should not have happened. But, before we quit out, attempt searching the other light/dark folder for the file.
        if (uiForceSide == FORCE_LIGHTSIDE)
            len = trap->FS_Open(va("forcecfg/dark/%s.fcf", uiInfo.forceConfigNames[newindex]), &f, fsMode_t::FS_READ);
        else
            len = trap->FS_Open(va("forcecfg/light/%s.fcf", uiInfo.forceConfigNames[newindex]), &f, fsMode_t::FS_READ);

        if (len <= 0) { // still failure? Oh well.
            return;
        }
    }

    if (len >= 8192) {
        trap->FS_Close(f);
        return;
    }

    trap->FS_Read(fcfBuffer, len, f);
    fcfBuffer[len] = 0;
    trap->FS_Close(f);

    i = 0;

    info[0] = '\0';
    trap->GetConfigString(CS_SERVERINFO, info, sizeof(info));

    if (atoi(Info_ValueForKey(info, "g_forceBasedTeams"))) {
        switch (UI_CvarTeam("ui_myteam")) {
        case team_t::TEAM_RED:
            forceTeam = FORCE_DARKSIDE;
            break;
        case team_t::TEAM_BLUE:
            forceTeam = FORCE_LIGHTSIDE;
            break;
        default:
            break;
        }
    }

    BG_LegalizedForcePowers(fcfBuffer, sizeof(fcfBuffer), uiMaxRank, ui_freeSaber.integer, forceTeam,
                            static_cast<gametype_t>(atoi(Info_ValueForKey(info, "g_gametype"))), 0);
    // legalize the config based on the max rank

    // now that we're done with the handle, it's time to parse our force data out of the string
    // we store strings in rank-side-xxxxxxxxx format (where the x's are individual force power levels)
    while (fcfBuffer[i] && fcfBuffer[i] != '-') {
        singleBuf[c] = fcfBuffer[i];
        c++;
        i++;
    }
    singleBuf[c] = 0;
    c = 0;
    i++;

    const forceMasteries_t rank = static_cast<forceMasteries_t>(atoi(singleBuf));

    if (rank > uiMaxRank || rank < forceMasteries_t::FORCE_MASTERY_UNINITIATED) { // this force config uses a rank level higher than our currently restricted
                                                                                  // level.. so we can't use it
                                                                                  // FIXME: Print a message indicating this to the user
        return;
    }

    uiForceRank = rank;

    while (fcfBuffer[i] && fcfBuffer[i] != '-') {
        singleBuf[c] = fcfBuffer[i];
        c++;
        i++;
    }
    singleBuf[c] = 0;
    c = 0;
    i++;

    uiForceSide = atoi(singleBuf);

    if (uiForceSide != FORCE_LIGHTSIDE && uiForceSide != FORCE_DARKSIDE) {
        uiForceSide = FORCE_LIGHTSIDE;
        return;
    }

    // clear out the existing powers
    for (forcePowers_t fp = forcePowers_t::FP_FIRST; fp < forcePowers_t::NUM_FORCE_POWERS; fp++) {
        /*
        if (fp==forcePowers_t::FP_LEVITATION)
        {
            uiForcePowersRank[fp]=forcePowerLevels_t::FORCE_LEVEL_1;
        }
        else if (fp==forcePowers_t::FP_SABER_OFFENSE && ui_freeSaber.integer)
        {
            uiForcePowersRank[fp]=forcePowerLevels_t::FORCE_LEVEL_1;
        }
        else if (fp==forcePowers_t::FP_SABER_DEFENSE && ui_freeSaber.integer)
        {
            uiForcePowersRank[fp]=forcePowerLevels_t::FORCE_LEVEL_1;
        }
        else
        {
            uiForcePowersRank[fp] = forcePowerLevels_t::FORCE_LEVEL_0;
        }
        */
        // rww - don't need to do these checks. Just trust whatever the saber config says.
        uiForcePowersRank[fp] = forcePowerLevels_t::FORCE_LEVEL_0;
    }
    uiForceUsed = 0;
    uiForceAvailable = forceMasteryPoints[uiForceRank];
    gTouchedForce = true;

    for (forcePowers_t fp = forcePowers_t::FP_FIRST; fcfBuffer[i] && fp < forcePowers_t::NUM_FORCE_POWERS; fp++, i++) {
        singleBuf[0] = fcfBuffer[i];
        singleBuf[1] = 0;
        iBuf = atoi(singleBuf); // So, that means that Force Power "fp" wants to be set to rank "iBuf".

        if (iBuf < 0) {
            iBuf = 0;
        }

        forcePowerRank = static_cast<forcePowerLevels_t>(iBuf);

        if (forcePowerRank > forcePowerLevels_t::FORCE_LEVEL_3 || forcePowerRank < forcePowerLevels_t::FORCE_LEVEL_0) { // err..  not correct
            continue;                                                                                                   // skip this power
        }

        if (uiForcePowerDarkLight[fp] &&
            uiForcePowerDarkLight[fp] != uiForceSide) { // Apparently the user has crafted a force config that has powers that don't fit with the config's side.
            continue;                                   // skip this power
        }

        // Accrue cost for each assigned rank for this power.
        for (currank = forcePowerLevels_t::FORCE_LEVEL_1; currank <= forcePowerRank; currank++) {
            if (bgForcePowerCost[fp][currank] > uiForceAvailable) { // Break out, we can't afford any more power.
                break;
            }
            // Pay for this rank of this power.
            uiForceUsed += bgForcePowerCost[fp][currank];
            uiForceAvailable -= bgForcePowerCost[fp][currank];

            uiForcePowersRank[fp]++;
        }
    }

    if (uiForcePowersRank[forcePowers_t::FP_LEVITATION] < forcePowerLevels_t::FORCE_LEVEL_1) {
        uiForcePowersRank[forcePowers_t::FP_LEVITATION] = forcePowerLevels_t::FORCE_LEVEL_1;
    }
    if (uiForcePowersRank[forcePowers_t::FP_SABER_OFFENSE] < forcePowerLevels_t::FORCE_LEVEL_1 && ui_freeSaber.integer) {
        uiForcePowersRank[forcePowers_t::FP_SABER_OFFENSE] = forcePowerLevels_t::FORCE_LEVEL_1;
    }
    if (uiForcePowersRank[forcePowers_t::FP_SABER_DEFENSE] < forcePowerLevels_t::FORCE_LEVEL_1 && ui_freeSaber.integer) {
        uiForcePowersRank[forcePowers_t::FP_SABER_DEFENSE] = forcePowerLevels_t::FORCE_LEVEL_1;
    }

    UpdateForceUsed();
}

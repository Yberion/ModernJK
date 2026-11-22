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

#include "exe_headers.h"

#include "server.h"
#include <game/weapons.h>
#include <game/g_items.h>
#include <game/statindex.h>
#include <qcommon/game_version.h>

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/

bool qbLoadTransition = false;

//=========================================================
// don't call this directly, it should only be called from SV_Map_f() or SV_MapTransition_f()
//
static bool SV_Map_(ForceReload_e eForceReload)
{
    const char* map = nullptr;
    char expanded[MAX_QPATH] = {0};

    map = Cmd_Argv(1);
    if (!*map) {
        Q_Println("no map specified");
        return false;
    }

    // make sure the level exists before trying to change, so that
    // a typo at the server console won't end the game
    if (strchr(map, '\\')) {
        Q_Print("Can't have mapnames with a \\\n");
        return false;
    }

    Q_FormatTo(expanded, "maps/{}.bsp", map);

    if (FS_ReadFile(expanded, nullptr) == -1) {
        Q_Println("Can't find map {}", expanded);
        extern cvar_t* com_buildScript;
        if (com_buildScript && com_buildScript->integer) { // yes, it's happened, someone deleted a map during my build...
            Q_Error(errorParm_t::ERR_FATAL, "Can't find map {}", expanded);
        }
        return false;
    }

    if (map[0] != '_') {
        SG_WipeSavegame("auto");
    }

    SV_SpawnServer(map, eForceReload, true); // start up the map
    return true;
}

// Save out some player data for later restore if this is a spawn point with KEEP_PREV (spawnflags&1) set...
//
// (now also called by auto-save code to setup the cvars correctly
void SV_Player_EndOfLevelSave(void)
{
    // I could just call GetClientState() but that's in sv_bot.cpp, and I'm not sure if that's going to be deleted for
    //	the single player build, so here's the guts again...
    //
    client_t* cl = &svs.clients[0]; // 0 because only ever us as a player

    if (cl && cl->gentity && cl->gentity->client // crash fix for voy4->brig transition when you kill Foster.
                                                 //	Shouldn't happen, but does sometimes...
    ) {
        Cvar_Set(sCVARNAME_PLAYERSAVE, ""); // default to blank

        //		clientSnapshot_t*	pFrame = &cl->frames[cl->netchan.outgoingSequence & PACKET_MASK];
        playerState_t* pState = cl->gentity->client;
        const char* s2;
        formatText_t s2Text; // holds the text s2 points into
        const char* s;
        formatText_t sText; // holds the text s points into
#ifdef JK2_MODE
        s = (sText = Q_Format("{} {} {} {} {} {} {} {:f} {:f} {:f} {} {} {:d} {} {} {}", pState->stats[statIndex_t::STAT_HEALTH],
                              pState->stats[statIndex_t::STAT_ARMOR], pState->stats[statIndex_t::STAT_WEAPONS], pState->stats[statIndex_t::STAT_ITEMS],
                              pState->weapon, pState->weaponstate, pState->batteryCharge, pState->viewangles[0], pState->viewangles[1], pState->viewangles[2],
                              pState->forcePowersKnown, pState->forcePower, pState->saberActive, pState->saberAnimLevel, pState->saberLockEnemy,
                              pState->saberLockTime))
                .c_str();
#else
        //				|general info				  |-force powers |-saber 1		|-saber 2										  |-general saber
        s = (sText = Q_Format(
                 "{} {} {} {} {} {} {} {:f} {:f} {:f} {} {} {} {} {} {} {:d} {:d} {:d} {:d} {:d} {:d} {:d} {:d} {} {} {} {} {} {} {} {} {} {:d} {:d} {:d} {:d} "
                 "{:d} {:d} {:d} {:d} {} {} {} {} {} {} {} {} {} {} {} {}",
                 pState->stats[statIndex_t::STAT_HEALTH], pState->stats[statIndex_t::STAT_ARMOR], pState->stats[statIndex_t::STAT_WEAPONS],
                 pState->stats[statIndex_t::STAT_ITEMS], pState->weapon, pState->weaponstate, pState->batteryCharge, pState->viewangles[0],
                 pState->viewangles[1], pState->viewangles[2],
                 // force power data
                 pState->forcePowersKnown, pState->forcePower, pState->forcePowerMax, pState->forcePowerRegenRate, pState->forcePowerRegenAmount,
                 // saber 1 data
                 pState->saber[0].name, pState->saber[0].blade[0].active, pState->saber[0].blade[1].active, pState->saber[0].blade[2].active,
                 pState->saber[0].blade[3].active, pState->saber[0].blade[4].active, pState->saber[0].blade[5].active, pState->saber[0].blade[6].active,
                 pState->saber[0].blade[7].active, pState->saber[0].blade[0].color, pState->saber[0].blade[1].color, pState->saber[0].blade[2].color,
                 pState->saber[0].blade[3].color, pState->saber[0].blade[4].color, pState->saber[0].blade[5].color, pState->saber[0].blade[6].color,
                 pState->saber[0].blade[7].color,
                 // saber 2 data
                 pState->saber[1].name, pState->saber[1].blade[0].active, pState->saber[1].blade[1].active, pState->saber[1].blade[2].active,
                 pState->saber[1].blade[3].active, pState->saber[1].blade[4].active, pState->saber[1].blade[5].active, pState->saber[1].blade[6].active,
                 pState->saber[1].blade[7].active, pState->saber[1].blade[0].color, pState->saber[1].blade[1].color, pState->saber[1].blade[2].color,
                 pState->saber[1].blade[3].color, pState->saber[1].blade[4].color, pState->saber[1].blade[5].color, pState->saber[1].blade[6].color,
                 pState->saber[1].blade[7].color,
                 // general saber data
                 pState->saberStylesKnown, pState->saberAnimLevel, pState->saberLockEnemy, pState->saberLockTime))
                .c_str();
#endif
        Cvar_Set(sCVARNAME_PLAYERSAVE, s);

        // ammo
        s2 = "";
        for (const int ammo : pState->ammo) {
            s2 = (s2Text = Q_Format("{} {}", s2, ammo)).c_str();
        }
        Cvar_Set("playerammo", s2);

        // inventory
        s2 = "";
        for (inventory_t inv = inventory_t::INV_ELECTROBINOCULARS; inv < inventory_t::INV_MAX; inv++) {
            s2 = (s2Text = Q_Format("{} {}", s2, pState->inventory[inv])).c_str();
        }
        Cvar_Set("playerinv", s2);

        // the new JK2 stuff - force powers, etc...
        //
        s2 = "";
        for (forcePowers_t fp = forcePowers_t::FP_FIRST; fp < forcePowers_t::NUM_FORCE_POWERS; fp++) {
            s2 = (s2Text = Q_Format("{} {}", s2, pState->forcePowerLevel[fp])).c_str();
        }
        Cvar_Set("playerfplvl", s2);
    }
}

// Restart the server on a different map
//
static void SV_MapTransition_f(void)
{
    const char* spawntarget;

#ifdef JK2_MODE
    SCR_PrecacheScreenshot();
#endif
    SV_Player_EndOfLevelSave();

    spawntarget = Cmd_Argv(2);
    if (*spawntarget != '\0') {
        Cvar_Set("spawntarget", spawntarget);
    }
    else {
        Cvar_Set("spawntarget", "");
    }

    SV_Map_(ForceReload_e::eForceReload_NOTHING);
}

/*
==================
SV_Map_f

Restart the server on a different map, but clears a cvar so that typing "map blah" doesn't try and preserve
player weapons/ammo/etc from the previous level that you haven't really exited (ie ignores KEEP_PREV on spawn points)
==================
*/
#ifdef JK2_MODE
extern void SCR_UnprecacheScreenshot();
#endif
static void SV_Map_f(void)
{
    Cvar_Set(sCVARNAME_PLAYERSAVE, "");
    Cvar_Set("spawntarget", "");
    Cvar_Set("tier_storyinfo", "0");
    Cvar_Set("tiers_complete", "");
#ifdef JK2_MODE
    SCR_UnprecacheScreenshot();
#endif

    ForceReload_e eForceReload = ForceReload_e::eForceReload_NOTHING; // default for normal load

    const char* cmd = Cmd_Argv(0);
    if (!Q_stricmp(cmd, "devmapbsp"))
        eForceReload = ForceReload_e::eForceReload_BSP;
    else if (!Q_stricmp(cmd, "devmapmdl"))
        eForceReload = ForceReload_e::eForceReload_MODELS;
    else if (!Q_stricmp(cmd, "devmapall"))
        eForceReload = ForceReload_e::eForceReload_ALL;

    bool cheat = (!Q_stricmpn(cmd, "devmap", 6));

    // retain old cheat state
    if (!cheat && Cvar_VariableIntegerValue("helpUsObi"))
        cheat = true;

    if (SV_Map_(eForceReload)) {
        // set the cheat value
        // if the level was started with "map <levelname>", then
        // cheats will not be allowed.  If started with "devmap <levelname>"
        // then cheats will be allowed
        Cvar_Set("helpUsObi", cheat ? "1" : "0");
    }
#ifdef JK2_MODE
    Cvar_Set("cg_missionstatusscreen", "0");
#endif
}

/*
==================
SV_LoadTransition_f
==================
*/
void SV_LoadTransition_f(void)
{
    const char* map;
    const char* spawntarget;

    map = Cmd_Argv(1);
    if (!*map) {
        return;
    }

    qbLoadTransition = true;

#ifdef JK2_MODE
    SCR_PrecacheScreenshot();
#endif
    SV_Player_EndOfLevelSave();

    // Save the full current state of the current map so we can return to it later
    SG_WriteSavegame(Q_Format("hub/{}", sv_mapname->string).c_str(), false);

    // set the spawntarget if there is one
    spawntarget = Cmd_Argv(2);
    if (*spawntarget != '\0') {
        Cvar_Set("spawntarget", spawntarget);
    }
    else {
        Cvar_Set("spawntarget", "");
    }

    if (!SV_TryLoadTransition(map)) { // couldn't load a savegame
        SV_Map_(ForceReload_e::eForceReload_NOTHING);
    }
    qbLoadTransition = false;
}
//===============================================================

char* ivtos(const vec3_t v)
{
    static int index;
    static char str[8][32];
    char* s;

    // use an array so that multiple vtos won't collide
    s = str[index];
    index = (index + 1) & 7;

    Q_FormatTo(std::span(s, 32), "( {} {} {} )", (int)v[0], (int)v[1], (int)v[2]);

    return s;
}

/*
================
SV_Status_f
================
*/
static void SV_Status_f(void)
{
    client_t* cl;

    // make sure server is running
    if (!com_sv_running->integer) {
        Q_Println("Server is not running.");
        return;
    }

    cl = &svs.clients[0];

    if (!cl) {
        Q_Println("Server is not running.");
        return;
    }

#if defined(_WIN32)
#define STATUS_OS "Windows"
#elif defined(__linux__)
#define STATUS_OS "Linux"
#elif defined(MACOS_X)
#define STATUS_OS "OSX"
#else
#define STATUS_OS "Unknown"
#endif

    Q_Println("name    : {}^7", cl->name);
    Q_Println("score   : {}", cl->gentity->client->persistant[persEnum_t::PERS_SCORE]);
    Q_Println("version : {} {} {}", STATUS_OS, JK_VERSION, PROTOCOL_VERSION);
#ifdef JK2_MODE
    Q_Println("game    : Jedi Outcast {}", FS_GetCurrentGameDir());
#else
    Q_Println("game    : Jedi Academy {}", FS_GetCurrentGameDir());
#endif
    Q_Println("map     : {} at {}", sv_mapname->string, ivtos(cl->gentity->client->origin));
}

/*
===========
SV_Serverinfo_f

Examine the serverinfo string
===========
*/
static void SV_Serverinfo_f(void)
{
    Q_Println("Server info settings:");
    Info_Print(Cvar_InfoString(cvarFlags_t::CVAR_SERVERINFO));
}

/*
===========
SV_Systeminfo_f

Examine or change the serverinfo string
===========
*/
static void SV_Systeminfo_f(void)
{
    Q_Println("System info settings:");
    Info_Print(Cvar_InfoString(cvarFlags_t::CVAR_SYSTEMINFO));
}

/*
===========
SV_DumpUser_f

Examine all a users info strings FIXME: move to game
===========
*/
static void SV_DumpUser_f(void)
{
    client_t* cl;

    // make sure server is running
    if (!com_sv_running->integer) {
        Q_Println("Server is not running.");
        return;
    }

    if (Cmd_Argc() != 1) {
        Q_Println("Usage: info");
        return;
    }

    cl = &svs.clients[0];
    if (cl->state == clientState_t::CS_FREE) {
        Q_Println("Client is not active");
        return;
    }

    Q_Println("userinfo");
    Q_Println("--------");
    Info_Print(cl->userinfo);
}

//===========================================================

/*
==================
SV_CompleteMapName
==================
*/
static void SV_CompleteMapName(char* args, int argNum)
{
    if (argNum == 2)
        Field_CompleteFilename("maps", "bsp", true, false);
}

/*
==================
SV_CompleteMapName
==================
*/
static void SV_CompleteSaveName(char* args, int argNum)
{
    if (argNum == 2)
        Field_CompleteFilename("saves", "sav", true, true);
}

/*
==================
SV_AddOperatorCommands
==================
*/
void SV_AddOperatorCommands(void)
{
    static bool initialized;

    if (initialized) {
        return;
    }
    initialized = true;

    Cmd_AddCommand("status", SV_Status_f);
    Cmd_AddCommand("serverinfo", SV_Serverinfo_f);
    Cmd_AddCommand("systeminfo", SV_Systeminfo_f);
    Cmd_AddCommand("dumpuser", SV_DumpUser_f);
    Cmd_AddCommand("sectorlist", SV_SectorList_f);
    Cmd_AddCommand("map", SV_Map_f);
    Cmd_SetCommandCompletionFunc("map", SV_CompleteMapName);
    Cmd_AddCommand("devmap", SV_Map_f);
    Cmd_SetCommandCompletionFunc("devmap", SV_CompleteMapName);
    Cmd_AddCommand("devmapbsp", SV_Map_f);
    Cmd_SetCommandCompletionFunc("devmapbsp", SV_CompleteMapName);
    Cmd_AddCommand("devmapmdl", SV_Map_f);
    Cmd_SetCommandCompletionFunc("devmapmdl", SV_CompleteMapName);
    Cmd_AddCommand("devmapsnd", SV_Map_f);
    Cmd_SetCommandCompletionFunc("devmapsnd", SV_CompleteMapName);
    Cmd_AddCommand("devmapall", SV_Map_f);
    Cmd_SetCommandCompletionFunc("devmapall", SV_CompleteMapName);
    Cmd_AddCommand("maptransition", SV_MapTransition_f);
    Cmd_AddCommand("load", SV_LoadGame_f);
    Cmd_SetCommandCompletionFunc("load", SV_CompleteSaveName);
    Cmd_AddCommand("loadtransition", SV_LoadTransition_f);
    Cmd_AddCommand("save", SV_SaveGame_f);
    Cmd_AddCommand("wipe", SV_WipeGame_f);

    // #ifdef _DEBUG
    //	extern void UI_Dump_f(void);
    //	Cmd_AddCommand ("ui_dump", UI_Dump_f);
    // #endif
}

/*
==================
SV_RemoveOperatorCommands
==================
*/
void SV_RemoveOperatorCommands(void)
{
#if 0
	// removing these won't let the server start again
	Cmd_RemoveCommand ("status");
	Cmd_RemoveCommand ("serverinfo");
	Cmd_RemoveCommand ("systeminfo");
	Cmd_RemoveCommand ("dumpuser");
	Cmd_RemoveCommand ("serverrecord");
	Cmd_RemoveCommand ("serverstop");
	Cmd_RemoveCommand ("sectorlist");
#endif
}

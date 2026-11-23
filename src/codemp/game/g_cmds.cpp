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

#include <iterator>

#include "g_local.h"
#include "bg_saga.h"

#include <ui/menudef.h> // for the voice chats

// rww - for getting bot commands...
int AcceptBotCommand(char* cmd, gentity_t* pl);
// end rww

void WP_SetSaber(int entNum, saberInfo_t* sabers, int saberNum, const char* saberName);

void Cmd_NPC_f(gentity_t* ent);
void SetTeamQuick(gentity_t* ent, team_t team, bool doBegin);

/*
==================
DeathmatchScoreboardMessage

==================
*/
void DeathmatchScoreboardMessage(gentity_t* ent)
{
    char entry[256];
    char string[MAX_STRING_CHARS - 1];
    int stringlength, prefix;
    int i, j;
    gclient_t* cl;
    int numSorted, scoreFlags, accuracy, perfect;

    // send the latest information on all clients
    string[0] = '\0';
    stringlength = 0;
    scoreFlags = 0;

    numSorted = level.numConnectedClients;

    // This is dumb that we are capping to 20 clients but support 32 in server
    if (numSorted > MAX_CLIENT_SCORE_SEND) {
        numSorted = MAX_CLIENT_SCORE_SEND;
    }

    // estimate prefix length to avoid oversize of final string
    prefix = Q_FormatTo(entry, "scores {} {} {}", level.teamScores[team_t::TEAM_RED], level.teamScores[team_t::TEAM_BLUE], level.numConnectedClients);

    for (i = 0; i < numSorted; i++) {
        int ping;

        cl = &level.clients[level.sortedClients[i]];

        if (cl->pers.connected == clientConnected_t::CON_CONNECTING) {
            ping = -1;
        }
        else {
            ping = cl->ps.ping < 999 ? cl->ps.ping : 999;
        }

        if (cl->accuracy_shots) {
            accuracy = cl->accuracy_hits * 100 / cl->accuracy_shots;
        }
        else {
            accuracy = 0;
        }
        perfect = (cl->ps.persistant[persEnum_t::PERS_RANK] == 0 && cl->ps.persistant[persEnum_t::PERS_KILLED] == 0) ? 1 : 0;

        j = Q_FormatTo(entry, " {} {} {} {} {} {} {} {} {} {} {} {} {} {}", level.sortedClients[i], cl->ps.persistant[persEnum_t::PERS_SCORE], ping,
                       (level.time - cl->pers.enterTime) / 60000, scoreFlags, g_entities[level.sortedClients[i]].s.powerups, accuracy,
                       cl->ps.persistant[persEnum_t::PERS_IMPRESSIVE_COUNT], cl->ps.persistant[persEnum_t::PERS_EXCELLENT_COUNT],
                       cl->ps.persistant[persEnum_t::PERS_GAUNTLET_FRAG_COUNT], cl->ps.persistant[persEnum_t::PERS_DEFEND_COUNT],
                       cl->ps.persistant[persEnum_t::PERS_ASSIST_COUNT], perfect, cl->ps.persistant[persEnum_t::PERS_CAPTURES]);

        if (stringlength + j + prefix >= std::ssize(string))
            break;

        strcpy(string + stringlength, entry);
        stringlength += j;
    }

    trap->SendServerCommand(
        ent - g_entities,
        Q_Format("scores {} {} {}{}", level.numConnectedClients, level.teamScores[team_t::TEAM_RED], level.teamScores[team_t::TEAM_BLUE], string).c_str());
}

/*
==================
Cmd_Score_f

Request current scoreboard information
==================
*/
void Cmd_Score_f(gentity_t* ent)
{
    DeathmatchScoreboardMessage(ent);
}

/*
==================
ConcatArgs
==================
*/
char* ConcatArgs(int start)
{
    int i, c, tlen;
    static char line[MAX_STRING_CHARS];
    int len;
    char arg[MAX_STRING_CHARS];

    len = 0;
    c = trap->Argc();
    for (i = start; i < c; i++) {
        trap->Argv(i, arg, sizeof(arg));
        tlen = strlen(arg);
        if (len + tlen >= MAX_STRING_CHARS - 1) {
            break;
        }
        memcpy(line + len, arg, tlen);
        len += tlen;
        if (i != c - 1) {
            line[len] = ' ';
            len++;
        }
    }

    line[len] = 0;

    return line;
}

/*
==================
StringIsInteger
==================
*/
bool StringIsInteger(const char* s)
{
    int i = 0, len = 0;
    bool foundDigit = false;

    for (i = 0, len = strlen(s); i < len; i++) {
        if (!isdigit(s[i]))
            return false;

        foundDigit = true;
    }

    return foundDigit;
}

/*
==================
ClientNumberFromString

Returns a player number for either a number or name string
Returns -1 if invalid
==================
*/
int ClientNumberFromString(gentity_t* to, const char* s, bool allowconnecting)
{
    gclient_t* cl;
    int idnum;
    char cleanInput[MAX_NETNAME];

    if (StringIsInteger(s)) { // numeric values could be slot numbers
        idnum = atoi(s);
        if (idnum >= 0 && idnum < level.maxclients) {
            cl = &level.clients[idnum];
            if (cl->pers.connected == clientConnected_t::CON_CONNECTED)
                return idnum;
            else if (allowconnecting && cl->pers.connected == clientConnected_t::CON_CONNECTING)
                return idnum;
        }
    }

    Q_strncpyz(cleanInput, s, sizeof(cleanInput));
    Q_StripColor(cleanInput);

    for (idnum = 0, cl = level.clients; idnum < level.maxclients; idnum++, cl++) { // check for a name match
        if (cl->pers.connected != clientConnected_t::CON_CONNECTED)
            if (!allowconnecting || cl->pers.connected < clientConnected_t::CON_CONNECTING)
                continue;

        if (!Q_stricmp(cl->pers.netname_nocolor, cleanInput))
            return idnum;
    }

    trap->SendServerCommand(to - g_entities, Q_Format("print \"User {} is not on the server\n\"", s).c_str());
    return -1;
}

/*
==================
Cmd_Give_f

Give items to a client
==================
*/
void G_Give(gentity_t* ent, const char* name, const char* args, int argc)
{
    gitem_t* it;
    bool give_all = false;
    gentity_t* it_ent;
    trace_t trace;

    if (!Q_stricmp(name, "all"))
        give_all = true;

    if (give_all) {
        for (holdable_t h = holdable_t::HI_NONE; h < holdable_t::HI_NUM_HOLDABLE; h++)
            ent->client->ps.stats[statIndex_t::STAT_HOLDABLE_ITEMS] |= Bit(h);
    }

    if (give_all || !Q_stricmp(name, "health")) {
        if (argc == 3)
            ent->health = Com_Clampi(1, ent->client->ps.stats[statIndex_t::STAT_MAX_HEALTH], atoi(args));
        else {
            if (level.gametype == gametype_t::GT_SIEGE && ent->client->siegeClass != -1)
                ent->health = bgSiegeClasses[ent->client->siegeClass].maxhealth;
            else
                ent->health = ent->client->ps.stats[statIndex_t::STAT_MAX_HEALTH];
        }
        if (!give_all)
            return;
    }

    if (give_all || !Q_stricmp(name, "armor") || !Q_stricmp(name, "shield")) {
        if (argc == 3)
            ent->client->ps.stats[statIndex_t::STAT_ARMOR] = Com_Clampi(0, ent->client->ps.stats[statIndex_t::STAT_MAX_HEALTH], atoi(args));
        else {
            if (level.gametype == gametype_t::GT_SIEGE && ent->client->siegeClass != -1)
                ent->client->ps.stats[statIndex_t::STAT_ARMOR] = bgSiegeClasses[ent->client->siegeClass].maxarmor;
            else
                ent->client->ps.stats[statIndex_t::STAT_ARMOR] = ent->client->ps.stats[statIndex_t::STAT_MAX_HEALTH];
        }

        if (!give_all)
            return;
    }

    if (give_all || !Q_stricmp(name, "force")) {
        if (argc == 3)
            ent->client->ps.fd.forcePower = Com_Clampi(0, ent->client->ps.fd.forcePowerMax, atoi(args));
        else
            ent->client->ps.fd.forcePower = ent->client->ps.fd.forcePowerMax;

        if (!give_all)
            return;
    }

    if (give_all || !Q_stricmp(name, "weapons")) {
        ent->client->ps.stats[statIndex_t::STAT_WEAPONS] = Bit(LAST_USEABLE_WEAPON + 1) - Bit(weapon_t::WP_NONE);
        if (!give_all)
            return;
    }

    if (!give_all && !Q_stricmp(name, "weaponnum")) {
        ent->client->ps.stats[statIndex_t::STAT_WEAPONS] |= (1 << atoi(args));
        return;
    }

    if (give_all || !Q_stricmp(name, "ammo")) {
        int num = 999;
        if (argc == 3)
            num = Com_Clampi(0, 999, atoi(args));
        for (ammo_t a = ammo_t::AMMO_BLASTER; a < ammo_t::AMMO_MAX; a++)
            ent->client->ps.ammo[a] = num;
        if (!give_all)
            return;
    }

    if (!Q_stricmp(name, "excellent")) {
        ent->client->ps.persistant[persEnum_t::PERS_EXCELLENT_COUNT]++;
        return;
    }
    if (!Q_stricmp(name, "impressive")) {
        ent->client->ps.persistant[persEnum_t::PERS_IMPRESSIVE_COUNT]++;
        return;
    }
    if (!Q_stricmp(name, "gauntletaward")) {
        ent->client->ps.persistant[persEnum_t::PERS_GAUNTLET_FRAG_COUNT]++;
        return;
    }
    if (!Q_stricmp(name, "defend")) {
        ent->client->ps.persistant[persEnum_t::PERS_DEFEND_COUNT]++;
        return;
    }
    if (!Q_stricmp(name, "assist")) {
        ent->client->ps.persistant[persEnum_t::PERS_ASSIST_COUNT]++;
        return;
    }

    // spawn a specific item right on the player
    if (!give_all) {
        it = BG_FindItem(name);
        if (!it)
            return;

        it_ent = G_Spawn();
        VectorCopy(ent->r.currentOrigin, it_ent->s.origin);
        it_ent->classname = it->classname;
        G_SpawnItem(it_ent, it);
        if (!it_ent || !it_ent->inuse)
            return;
        FinishSpawningItem(it_ent);
        if (!it_ent || !it_ent->inuse)
            return;
        memset(&trace, 0, sizeof(trace));
        Touch_Item(it_ent, ent, &trace);
        if (it_ent->inuse)
            G_FreeEntity(it_ent);
    }
}

void Cmd_Give_f(gentity_t* ent)
{
    char name[MAX_TOKEN_CHARS] = {0};

    trap->Argv(1, name, sizeof(name));
    G_Give(ent, name, ConcatArgs(2), trap->Argc());
}

void Cmd_GiveOther_f(gentity_t* ent)
{
    char name[MAX_TOKEN_CHARS] = {0};
    int i;
    char otherindex[MAX_TOKEN_CHARS];
    gentity_t* otherEnt = nullptr;

    if (trap->Argc() < 3) {
        trap->SendServerCommand(ent - g_entities, "print \"Usage: giveother <player id> <givestring>\n\"");
        return;
    }

    trap->Argv(1, otherindex, sizeof(otherindex));
    i = ClientNumberFromString(ent, otherindex, false);
    if (i == -1) {
        return;
    }

    otherEnt = &g_entities[i];
    if (!otherEnt->inuse || !otherEnt->client) {
        return;
    }

    if ((otherEnt->health <= 0 || otherEnt->client->tempSpectate >= level.time || otherEnt->client->sess.sessionTeam == team_t::TEAM_SPECTATOR)) {
        // Intentionally displaying for the command user
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBEALIVE")).c_str());
        return;
    }

    trap->Argv(2, name, sizeof(name));

    G_Give(otherEnt, name, ConcatArgs(3), trap->Argc() - 1);
}

/*
==================
Cmd_God_f

Sets client to godmode

argv(0) god
==================
*/
void Cmd_God_f(gentity_t* ent)
{
    const char* msg = nullptr;

    ent->flags ^= gentityFlags_t::FL_GODMODE;
    if (!Any(ent->flags & gentityFlags_t::FL_GODMODE))
        msg = "godmode OFF";
    else
        msg = "godmode ON";

    trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", msg).c_str());
}

/*
==================
Cmd_Notarget_f

Sets client to notarget

argv(0) notarget
==================
*/
void Cmd_Notarget_f(gentity_t* ent)
{
    const char* msg = nullptr;

    ent->flags ^= gentityFlags_t::FL_NOTARGET;
    if (!Any(ent->flags & gentityFlags_t::FL_NOTARGET))
        msg = "notarget OFF";
    else
        msg = "notarget ON";

    trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", msg).c_str());
}

/*
==================
Cmd_Noclip_f

argv(0) noclip
==================
*/
void Cmd_Noclip_f(gentity_t* ent)
{
    const char* msg = nullptr;

    ent->client->noclip = !ent->client->noclip;
    if (!ent->client->noclip)
        msg = "noclip OFF";
    else
        msg = "noclip ON";

    trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", msg).c_str());
}

/*
==================
Cmd_LevelShot_f

This is just to help generate the level pictures
for the menus.  It goes to the intermission immediately
and sends over a command to the client to resize the view,
hide the scoreboard, and take a special screenshot
==================
*/
void Cmd_LevelShot_f(gentity_t* ent)
{
    if (!ent->client->pers.localClient) {
        trap->SendServerCommand(ent - g_entities, "print \"The levelshot command must be executed by a local client\n\"");
        return;
    }

    // doesn't work in single player
    if (level.gametype == gametype_t::GT_SINGLE_PLAYER) {
        trap->SendServerCommand(ent - g_entities, "print \"Must not be in singleplayer mode for levelshot\n\"");
        return;
    }

    BeginIntermission();
    trap->SendServerCommand(ent - g_entities, "clientLevelShot");
}

#if 0
/*
==================
Cmd_TeamTask_f

From TA.
==================
*/
void Cmd_TeamTask_f( gentity_t *ent ) {
	char userinfo[MAX_INFO_STRING];
	char		arg[MAX_TOKEN_CHARS];
	int task;
	int client = ent->client - level.clients;

	if ( trap->Argc() != 2 ) {
		return;
	}
	trap->Argv( 1, arg, sizeof( arg ) );
	task = atoi( arg );

	trap->GetUserinfo(client, userinfo, sizeof(userinfo));
	Info_SetValueForKey(userinfo, "teamtask", Q_Format("{}", task).c_str());
	trap->SetUserinfo(client, userinfo);
	ClientUserinfoChanged(client);
}
#endif

void G_Kill(gentity_t* ent)
{
    if ((level.gametype == gametype_t::GT_DUEL || level.gametype == gametype_t::GT_POWERDUEL) && level.numPlayingClients > 1 && !level.warmupTime) {
        if (!g_allowDuelSuicide.integer) {
            trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "ATTEMPTDUELKILL")).c_str());
            return;
        }
    }

    ent->flags &= ~gentityFlags_t::FL_GODMODE;
    ent->client->ps.stats[statIndex_t::STAT_HEALTH] = ent->health = -999;
    player_die(ent, ent, ent, 100000, meansOfDeath_t::MOD_SUICIDE);
}

/*
=================
Cmd_Kill_f
=================
*/
void Cmd_Kill_f(gentity_t* ent)
{
    G_Kill(ent);
}

void Cmd_KillOther_f(gentity_t* ent)
{
    int i;
    char otherindex[MAX_TOKEN_CHARS];
    gentity_t* otherEnt = nullptr;

    if (trap->Argc() < 2) {
        trap->SendServerCommand(ent - g_entities, "print \"Usage: killother <player id>\n\"");
        return;
    }

    trap->Argv(1, otherindex, sizeof(otherindex));
    i = ClientNumberFromString(ent, otherindex, false);
    if (i == -1) {
        return;
    }

    otherEnt = &g_entities[i];
    if (!otherEnt->inuse || !otherEnt->client) {
        return;
    }

    if ((otherEnt->health <= 0 || otherEnt->client->tempSpectate >= level.time || otherEnt->client->sess.sessionTeam == team_t::TEAM_SPECTATOR)) {
        // Intentionally displaying for the command user
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBEALIVE")).c_str());
        return;
    }

    G_Kill(otherEnt);
}

/*
=================
BroadCastTeamChange

Let everyone know about a team change
=================
*/
void BroadcastTeamChange(gclient_t* client, std::optional<team_t> oldTeam)
{
    client->ps.fd.forceDoInit = 1; // every time we change teams make sure our force powers are set right

    if (level.gametype == gametype_t::GT_SIEGE) { // don't announce these things in siege
        return;
    }

    if (client->sess.sessionTeam == team_t::TEAM_RED) {
        trap->SendServerCommand(
            -1, Q_Format("cp \"{}" S_COLOR_WHITE " {}\n\"", client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEREDTEAM")).c_str());
    }
    else if (client->sess.sessionTeam == team_t::TEAM_BLUE) {
        trap->SendServerCommand(
            -1, Q_Format("cp \"{}" S_COLOR_WHITE " {}\n\"", client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEBLUETEAM")).c_str());
    }
    else if (client->sess.sessionTeam == team_t::TEAM_SPECTATOR && oldTeam != team_t::TEAM_SPECTATOR) {
        trap->SendServerCommand(
            -1, Q_Format("cp \"{}" S_COLOR_WHITE " {}\n\"", client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHESPECTATORS")).c_str());
    }
    else if (client->sess.sessionTeam == team_t::TEAM_FREE) {
        trap->SendServerCommand(-1,
                                Q_Format("cp \"{}" S_COLOR_WHITE " {}\n\"", client->pers.netname, G_GetStringEdString("MP_SVGAME", "JOINEDTHEBATTLE")).c_str());
    }

    G_LogPrintln("ChangeTeam: {} [{}] ({}) \"{}^7\" {} -> {}", (int)(client - level.clients), client->sess.IP, client->pers.guid, client->pers.netname,
                 TeamName(oldTeam.value_or(team_t::TEAM_FREE)), TeamName(client->sess.sessionTeam));
}

bool G_PowerDuelCheckFail(gentity_t* ent)
{
    int loners = 0;
    int doubles = 0;

    if (!ent->client || ent->client->sess.duelTeam == duelTeam_t::DUELTEAM_FREE) {
        return true;
    }

    G_PowerDuelCount(&loners, &doubles, false);

    if (ent->client->sess.duelTeam == duelTeam_t::DUELTEAM_LONE && loners >= 1) {
        return true;
    }

    if (ent->client->sess.duelTeam == duelTeam_t::DUELTEAM_DOUBLE && doubles >= 2) {
        return true;
    }

    return false;
}

/*
=================
SetTeam
=================
*/
bool g_dontPenalizeTeam = false;
bool g_preventTeamBegin = false;
void SetTeam(gentity_t* ent, const char* s)
{
    team_t oldTeam;
    team_t team;
    gclient_t* client;
    int clientNum;
    spectatorState_t specState;
    int specClient;
    int teamLeader;

    // fix: this prevents rare creation of invalid players
    if (!ent->inuse) {
        return;
    }

    //
    // see what change is requested
    //
    client = ent->client;

    clientNum = client - level.clients;
    specClient = 0;
    specState = spectatorState_t::SPECTATOR_NOT;
    if (!Q_stricmp(s, "scoreboard") || !Q_stricmp(s, "score")) {
        team = team_t::TEAM_SPECTATOR;
        specState = spectatorState_t::SPECTATOR_FREE; // SPECTATOR_SCOREBOARD disabling this for now since it is totally broken on client side
    }
    else if (!Q_stricmp(s, "follow1")) {
        team = team_t::TEAM_SPECTATOR;
        specState = spectatorState_t::SPECTATOR_FOLLOW;
        specClient = -1;
    }
    else if (!Q_stricmp(s, "follow2")) {
        team = team_t::TEAM_SPECTATOR;
        specState = spectatorState_t::SPECTATOR_FOLLOW;
        specClient = -2;
    }
    else if (!Q_stricmp(s, "spectator") || !Q_stricmp(s, "s")) {
        team = team_t::TEAM_SPECTATOR;
        specState = spectatorState_t::SPECTATOR_FREE;
    }
    else if (level.gametype >= gametype_t::GT_TEAM) {
        // if running a team game, assign player to one of the teams
        specState = spectatorState_t::SPECTATOR_NOT;
        if (!Q_stricmp(s, "red") || !Q_stricmp(s, "r")) {
            team = team_t::TEAM_RED;
        }
        else if (!Q_stricmp(s, "blue") || !Q_stricmp(s, "b")) {
            team = team_t::TEAM_BLUE;
        }
        else {
            // pick the team with the least number of players
            // For now, don't do this. The legalize function will set powers properly now.
            /*
            if (g_forceBasedTeams.integer)
            {
                if (ent->client->ps.fd.forceSide == FORCE_LIGHTSIDE)
                {
                    team = team_t::TEAM_BLUE;
                }
                else
                {
                    team = team_t::TEAM_RED;
                }
            }
            else
            {
            */
            team = PickTeam(clientNum);
            //}
        }

        if (g_teamForceBalance.integer && !g_jediVmerc.integer) {
            EnumArray<int, team_t, team_t::TEAM_NUM_TEAMS> counts;

            // JAC: Invalid clientNum was being used
            counts[team_t::TEAM_BLUE] = TeamCount(ent - g_entities, team_t::TEAM_BLUE);
            counts[team_t::TEAM_RED] = TeamCount(ent - g_entities, team_t::TEAM_RED);

            // We allow a spread of two
            if (team == team_t::TEAM_RED && counts[team_t::TEAM_RED] - counts[team_t::TEAM_BLUE] > 1) {
                // For now, don't do this. The legalize function will set powers properly now.
                /*
                if (g_forceBasedTeams.integer && ent->client->ps.fd.forceSide == FORCE_DARKSIDE)
                {
                    trap->SendServerCommand( ent->client->ps.clientNum,
                        Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYRED_SWITCH")).c_str() );
                }
                else
                */
                {
                    // JAC: Invalid clientNum was being used
                    trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYRED")).c_str());
                }
                return; // ignore the request
            }
            if (team == team_t::TEAM_BLUE && counts[team_t::TEAM_BLUE] - counts[team_t::TEAM_RED] > 1) {
                // For now, don't do this. The legalize function will set powers properly now.
                /*
                if (g_forceBasedTeams.integer && ent->client->ps.fd.forceSide == FORCE_LIGHTSIDE)
                {
                    trap->SendServerCommand( ent->client->ps.clientNum,
                        Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYBLUE_SWITCH")).c_str() );
                }
                else
                */
                {
                    // JAC: Invalid clientNum was being used
                    trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "TOOMANYBLUE")).c_str());
                }
                return; // ignore the request
            }

            // It's ok, the team we are switching to has less or same number of players
        }

        // For now, don't do this. The legalize function will set powers properly now.
        /*
        if (g_forceBasedTeams.integer)
        {
            if (team == team_t::TEAM_BLUE && ent->client->ps.fd.forceSide != FORCE_LIGHTSIDE)
            {
                trap->SendServerCommand( ent-g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBELIGHT")).c_str() );
                return;
            }
            if (team == team_t::TEAM_RED && ent->client->ps.fd.forceSide != FORCE_DARKSIDE)
            {
                trap->SendServerCommand( ent-g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBEDARK")).c_str() );
                return;
            }
        }
        */
    }
    else {
        // force them to spectators if there aren't any spots free
        team = team_t::TEAM_FREE;
    }

    oldTeam = client->sess.sessionTeam;

    if (level.gametype == gametype_t::GT_SIEGE) {
        if (client->tempSpectate >= level.time && team == team_t::TEAM_SPECTATOR) { // sorry, can't do that.
            return;
        }

        if (team == oldTeam && team != team_t::TEAM_SPECTATOR)
            return;

        client->sess.siegeDesiredTeam = team;
        // oh well, just let them go.
        /*
        if (team != team_t::TEAM_SPECTATOR)
        { //can't switch to anything in siege unless you want to switch to being a fulltime spectator
            //fill them in on their objectives for this team now
            trap->SendServerCommand(ent-g_entities, Q_Format("sb {}", client->sess.siegeDesiredTeam).c_str());

            trap->SendServerCommand( ent-g_entities, Q_Format("print \"You will be on the selected team the next time the round begins.\n\"").c_str() );
            return;
        }
        */
        if (client->sess.sessionTeam != team_t::TEAM_SPECTATOR &&
            team != team_t::TEAM_SPECTATOR) { // not a spectator now, and not switching to spec, so you have to wait til you die.
            // trap->SendServerCommand( ent-g_entities, Q_Format("print \"You will be on the selected team the next time you respawn.\n\"").c_str() );
            bool doBegin;
            if (ent->client->tempSpectate >= level.time) {
                doBegin = false;
            }
            else {
                doBegin = true;
            }

            if (doBegin) {
                // Kill them so they automatically respawn in the team they wanted.
                if (ent->health > 0) {
                    ent->flags &= ~gentityFlags_t::FL_GODMODE;
                    ent->client->ps.stats[statIndex_t::STAT_HEALTH] = ent->health = 0;
                    player_die(ent, ent, ent, 100000, meansOfDeath_t::MOD_TEAM_CHANGE);
                }
            }

            if (ent->client->sess.sessionTeam != ent->client->sess.siegeDesiredTeam) {
                SetTeamQuick(ent, ent->client->sess.siegeDesiredTeam, false);
            }

            return;
        }
    }

    // override decision if limiting the players
    if ((level.gametype == gametype_t::GT_DUEL) && level.numNonSpectatorClients >= 2) {
        team = team_t::TEAM_SPECTATOR;
    }
    else if ((level.gametype == gametype_t::GT_POWERDUEL) && (level.numPlayingClients >= 3 || G_PowerDuelCheckFail(ent))) {
        team = team_t::TEAM_SPECTATOR;
    }
    else if (g_maxGameClients.integer > 0 && level.numNonSpectatorClients >= g_maxGameClients.integer) {
        team = team_t::TEAM_SPECTATOR;
    }

    //
    // decide if we will allow the change
    //
    if (team == oldTeam && team != team_t::TEAM_SPECTATOR) {
        return;
    }

    //
    // execute the team change
    //

    // If it's siege then show the mission briefing for the team you just joined.
    //	if (level.gametype == gametype_t::GT_SIEGE && team != team_t::TEAM_SPECTATOR)
    //	{
    //		trap->SendServerCommand(clientNum, Q_Format("sb {}", team).c_str());
    //	}

    // if the player was dead leave the body
    if (client->ps.stats[statIndex_t::STAT_HEALTH] <= 0 && client->sess.sessionTeam != team_t::TEAM_SPECTATOR) {
        MaintainBodyQueue(ent);
    }

    // he starts at 'base'
    client->pers.teamState.state = playerTeamStateState_t::TEAM_BEGIN;
    if (oldTeam != team_t::TEAM_SPECTATOR) {
        // Kill him (makes sure he loses flags, etc)
        ent->flags &= ~gentityFlags_t::FL_GODMODE;
        ent->client->ps.stats[statIndex_t::STAT_HEALTH] = ent->health = 0;
        g_dontPenalizeTeam = true;
        player_die(ent, ent, ent, 100000, meansOfDeath_t::MOD_SUICIDE);
        g_dontPenalizeTeam = false;
    }
    // they go to the end of the line for tournaments
    if (team == team_t::TEAM_SPECTATOR && oldTeam != team)
        AddTournamentQueue(client);

    // clear votes if going to spectator (specs can't vote)
    if (team == team_t::TEAM_SPECTATOR)
        G_ClearVote(ent);
    // also clear team votes if switching red/blue or going to spec
    G_ClearTeamVote(ent, oldTeam);

    client->sess.sessionTeam = team;
    client->sess.spectatorState = specState;
    client->sess.spectatorClient = specClient;

    client->sess.teamLeader = false;
    if (team == team_t::TEAM_RED || team == team_t::TEAM_BLUE) {
        teamLeader = TeamLeader(team);
        // if there is no team leader or the team leader is a bot and this client is not a bot
        if (teamLeader == -1 || (!Any(g_entities[clientNum].r.svFlags & svFlags_t::SVF_BOT) && Any(g_entities[teamLeader].r.svFlags & svFlags_t::SVF_BOT))) {
            // SetLeader( team, clientNum );
        }
    }
    // make sure there is a team leader on the team the player came from
    if (oldTeam == team_t::TEAM_RED || oldTeam == team_t::TEAM_BLUE) {
        CheckTeamLeader(oldTeam);
    }

    BroadcastTeamChange(client, oldTeam);

    // make a disappearing effect where they were before teleporting them to the appropriate spawn point,
    // if we were not on the spec team
    if (oldTeam != team_t::TEAM_SPECTATOR) {
        gentity_t* tent = G_TempEntity(client->ps.origin, entity_event_t::EV_PLAYER_TELEPORT_OUT);
        tent->s.clientNum = clientNum;
    }

    // get and distribute relevent paramters
    if (!ClientUserinfoChanged(clientNum))
        return;

    if (!g_preventTeamBegin) {
        ClientBegin(clientNum, false);
    }
}

/*
=================
StopFollowing

If the client being followed leaves the game, or you just want to drop
to free floating spectator mode
=================
*/
extern void G_LeaveVehicle(gentity_t* ent, bool ConCheck);
void StopFollowing(gentity_t* ent)
{
    PS_SetTeam(ent->client->ps, team_t::TEAM_SPECTATOR);
    ent->client->sess.sessionTeam = team_t::TEAM_SPECTATOR;
    ent->client->sess.spectatorState = spectatorState_t::SPECTATOR_FREE;
    ent->client->ps.pm_flags &= ~pmoveFlags_t::PMF_FOLLOW;
    ent->r.svFlags &= ~svFlags_t::SVF_BOT;
    ent->client->ps.clientNum = ent - g_entities;
    ent->client->ps.weapon = weapon_t::WP_NONE;
    G_LeaveVehicle(ent, false); // clears m_iVehicleNum as well
    ent->client->ps.emplacedIndex = 0;
    // ent->client->ps.m_iVehicleNum = 0;
    ent->client->ps.viewangles[ROLL] = 0.0f;
    ent->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_NONE;
    ent->client->ps.forceHandExtendTime = 0;
    ent->client->ps.zoomMode = 0;
    ent->client->ps.zoomLocked = false;
    ent->client->ps.zoomLockTime = 0;
    ent->client->ps.saberMove = saberMoveName_t::LS_NONE;
    ent->client->ps.legsAnim = animNumber_t::FACE_TALK0;
    ent->client->ps.legsTimer = 0;
    ent->client->ps.torsoAnim = animNumber_t::FACE_TALK0;
    ent->client->ps.torsoTimer = 0;
    ent->client->ps.isJediMaster = false;                                // major exploit if you are spectating somebody and they are JM and you reconnect
    ent->client->ps.cloakFuel = 100;                                     // so that fuel goes away after stop following them
    ent->client->ps.jetpackFuel = 100;                                   // so that fuel goes away after stop following them
    ent->health = ent->client->ps.stats[statIndex_t::STAT_HEALTH] = 100; // so that you don't keep dead angles if you were spectating a dead person
    ent->client->ps.bobCycle = 0;
    ent->client->ps.pm_type = pmtype_t::PM_SPECTATOR;
    ent->client->ps.eFlags &= ~entityFlags_t::EF_DISINTEGRATION;
    ent->client->ps.powerups = {};
}

/*
=================
Cmd_Team_f
=================
*/
void Cmd_Team_f(gentity_t* ent)
{
    team_t oldTeam;
    char s[MAX_TOKEN_CHARS];

    oldTeam = ent->client->sess.sessionTeam;

    if (trap->Argc() != 2) {
        switch (oldTeam) {
        case team_t::TEAM_BLUE:
            trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "PRINTBLUETEAM")).c_str());
            break;
        case team_t::TEAM_RED:
            trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "PRINTREDTEAM")).c_str());
            break;
        case team_t::TEAM_FREE:
            trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "PRINTFREETEAM")).c_str());
            break;
        case team_t::TEAM_SPECTATOR:
            trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "PRINTSPECTEAM")).c_str());
            break;
        default:
            break;
        }
        return;
    }

    if (ent->client->switchTeamTime > level.time) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")).c_str());
        return;
    }

    if (gEscaping) {
        return;
    }

    // if they are playing a tournament game, count as a loss
    if (level.gametype == gametype_t::GT_DUEL && ent->client->sess.sessionTeam == team_t::TEAM_FREE) { // in a tournament game
        // disallow changing teams
        trap->SendServerCommand(ent - g_entities, "print \"Cannot switch teams in Duel\n\"");
        return;
        // FIXME: why should this be a loss???
        // ent->client->sess.losses++;
    }

    if (level.gametype ==
        gametype_t::GT_POWERDUEL) { // don't let clients change teams manually at all in powerduel, it will be taken care of through automated stuff
        trap->SendServerCommand(ent - g_entities, "print \"Cannot switch teams in Power Duel\n\"");
        return;
    }

    trap->Argv(1, s, sizeof(s));

    SetTeam(ent, s);

    // fix: update team switch time only if team change really happend
    if (oldTeam != ent->client->sess.sessionTeam)
        ent->client->switchTeamTime = level.time + 5000;
}

/*
=================
Cmd_DuelTeam_f
=================
*/
void Cmd_DuelTeam_f(gentity_t* ent)
{
    duelTeam_t oldTeam;
    char s[MAX_TOKEN_CHARS];

    if (level.gametype != gametype_t::GT_POWERDUEL) { // don't bother doing anything if this is not power duel
        return;
    }

    /*
    if (ent->client->sess.sessionTeam != team_t::TEAM_SPECTATOR)
    {
        trap->SendServerCommand( ent-g_entities, Q_Format("print \"You cannot change your duel team unless you are a spectator.\n\"").c_str());
        return;
    }
    */

    if (trap->Argc() != 2) { // No arg so tell what team we're currently on.
        oldTeam = ent->client->sess.duelTeam;
        switch (oldTeam) {
        case duelTeam_t::DUELTEAM_FREE:
            trap->SendServerCommand(ent - g_entities, Q_Format("print \"None\n\"").c_str());
            break;
        case duelTeam_t::DUELTEAM_LONE:
            trap->SendServerCommand(ent - g_entities, Q_Format("print \"Single\n\"").c_str());
            break;
        case duelTeam_t::DUELTEAM_DOUBLE:
            trap->SendServerCommand(ent - g_entities, Q_Format("print \"Double\n\"").c_str());
            break;
        default:
            break;
        }
        return;
    }

    if (ent->client->switchDuelTeamTime > level.time) { // debounce for changing
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")).c_str());
        return;
    }

    trap->Argv(1, s, sizeof(s));

    oldTeam = ent->client->sess.duelTeam;

    if (!Q_stricmp(s, "free")) {
        ent->client->sess.duelTeam = duelTeam_t::DUELTEAM_FREE;
    }
    else if (!Q_stricmp(s, "single")) {
        ent->client->sess.duelTeam = duelTeam_t::DUELTEAM_LONE;
    }
    else if (!Q_stricmp(s, "double")) {
        ent->client->sess.duelTeam = duelTeam_t::DUELTEAM_DOUBLE;
    }
    else {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"'{}' not a valid duel team.\n\"", s).c_str());
    }

    if (oldTeam == ent->client->sess.duelTeam) { // didn't actually change, so don't care.
        return;
    }

    if (ent->client->sess.sessionTeam != team_t::TEAM_SPECTATOR) { // ok..die
        duelTeam_t curTeam = ent->client->sess.duelTeam;
        ent->client->sess.duelTeam = oldTeam;
        G_Damage(ent, ent, ent, nullptr, ent->client->ps.origin, 99999, damageFlags_t::DAMAGE_NO_PROTECTION, meansOfDeath_t::MOD_SUICIDE);
        ent->client->sess.duelTeam = curTeam;
    }
    // reset wins and losses
    ent->client->sess.wins = 0;
    ent->client->sess.losses = 0;

    // get and distribute relevent paramters
    if (ClientUserinfoChanged(ent->s.number))
        return;

    ent->client->switchDuelTeamTime = level.time + 5000;
}

std::optional<team_t> G_TeamForSiegeClass(const char* clName)
{
    int i = 0;
    team_t team = SIEGETEAM_TEAM1;
    siegeTeam_t* stm = BG_SiegeFindThemeForTeam(team);
    siegeClass_t* scl;

    if (!stm) {
        return std::nullopt;
    }

    while (team <= SIEGETEAM_TEAM2) {
        scl = stm->classes[i];

        if (scl && scl->name[0]) {
            if (!Q_stricmp(clName, scl->name)) {
                return team;
            }
        }

        i++;
        if (i >= MAX_SIEGE_CLASSES || i >= stm->numClasses) {
            if (team == SIEGETEAM_TEAM2) {
                break;
            }
            team = SIEGETEAM_TEAM2;
            stm = BG_SiegeFindThemeForTeam(team);
            i = 0;
        }
    }

    return std::nullopt;
}

/*
=================
Cmd_SiegeClass_f
=================
*/
void Cmd_SiegeClass_f(gentity_t* ent)
{
    char className[64];
    std::optional<team_t> team;
    int preScore;
    bool startedAsSpec = false;

    if (level.gametype != gametype_t::GT_SIEGE) { // classes are only valid for this gametype
        return;
    }

    if (!ent->client) {
        return;
    }

    if (trap->Argc() < 1) {
        return;
    }

    if (ent->client->switchClassTime > level.time) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NOCLASSSWITCH")).c_str());
        return;
    }

    if (ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR) {
        startedAsSpec = true;
    }

    trap->Argv(1, className, sizeof(className));

    team = G_TeamForSiegeClass(className);

    if (!team) { // not a valid class name
        return;
    }

    if (ent->client->sess.sessionTeam != team) { // try changing it then
        g_preventTeamBegin = true;
        if (team == team_t::TEAM_RED) {
            SetTeam(ent, "red");
        }
        else if (team == team_t::TEAM_BLUE) {
            SetTeam(ent, "blue");
        }
        g_preventTeamBegin = false;

        if (ent->client->sess.sessionTeam != team) { // failed, oh well
            if (ent->client->sess.sessionTeam != team_t::TEAM_SPECTATOR || ent->client->sess.siegeDesiredTeam != team) {
                trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NOCLASSTEAM")).c_str());
                return;
            }
        }
    }

    // preserve 'is score
    preScore = ent->client->ps.persistant[persEnum_t::PERS_SCORE];

    // Make sure the class is valid for the team
    BG_SiegeCheckClassLegality(*team, className);

    // Set the session data
    strcpy(ent->client->sess.siegeClass, className);

    // get and distribute relevent paramters
    if (!ClientUserinfoChanged(ent->s.number))
        return;

    if (ent->client->tempSpectate < level.time) {
        // Kill him (makes sure he loses flags, etc)
        if (ent->health > 0 && !startedAsSpec) {
            ent->flags &= ~gentityFlags_t::FL_GODMODE;
            ent->client->ps.stats[statIndex_t::STAT_HEALTH] = ent->health = 0;
            player_die(ent, ent, ent, 100000, meansOfDeath_t::MOD_SUICIDE);
        }

        if (ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR || startedAsSpec) { // respawn them instantly.
            ClientBegin(ent->s.number, false);
        }
    }
    // set it back after we do all the stuff
    ent->client->ps.persistant[persEnum_t::PERS_SCORE] = preScore;

    ent->client->switchClassTime = level.time + 5000;
}

/*
=================
Cmd_ForceChanged_f
=================
*/
void Cmd_ForceChanged_f(gentity_t* ent)
{
    char fpChStr[1024];
    const char* buf;
    //	Cmd_Kill_f(ent);
    if (ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR) { // if it's a spec, just make the changes now
        // trap->SendServerCommand( ent-g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "FORCEAPPLIED")).c_str() );
        // No longer print it, as the UI calls this a lot.
        WP_InitForcePowers(ent);
        goto argCheck;
    }

    buf = G_GetStringEdString("MP_SVGAME", "FORCEPOWERCHANGED");

    strcpy(fpChStr, buf);

    trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}{}\n\"", S_COLOR_GREEN, fpChStr).c_str());

    ent->client->ps.fd.forceDoInit = 1;
argCheck:
    if (level.gametype == gametype_t::GT_DUEL ||
        level.gametype == gametype_t::GT_POWERDUEL) { // If this is duel, don't even bother changing team in relation to this.
        return;
    }

    if (trap->Argc() > 1) {
        char arg[MAX_TOKEN_CHARS];

        trap->Argv(1, arg, sizeof(arg));

        if (arg[0]) { // if there's an arg, assume it's a combo team command from the UI.
            Cmd_Team_f(ent);
        }
    }
}

extern bool WP_SaberStyleValidForSaber(saberInfo_t* saber1, saberInfo_t* saber2, int saberHolstered, saber_styles_t saberAnimLevel);
extern bool WP_UseFirstValidSaberStyle(saberInfo_t* saber1, saberInfo_t* saber2, int saberHolstered, saber_styles_t* saberAnimLevel);
bool G_SetSaber(gentity_t* ent, int saberNum, const char* saberName, bool siegeOverride)
{
    char truncSaberName[MAX_QPATH] = {0};

    if (!siegeOverride && level.gametype == gametype_t::GT_SIEGE && ent->client->siegeClass != -1 &&
        (bgSiegeClasses[ent->client->siegeClass].saberStance || bgSiegeClasses[ent->client->siegeClass].saber1[0] ||
         bgSiegeClasses[ent->client->siegeClass].saber2[0])) { // don't let it be changed if the siege class has forced any saber-related things
        return false;
    }

    Q_strncpyz(truncSaberName, saberName, sizeof(truncSaberName));

    if (saberNum == 0 && (!Q_stricmp("none", truncSaberName) || !Q_stricmp("remove", truncSaberName))) { // can't remove saber 0 like this
        Q_strncpyz(truncSaberName, DEFAULT_SABER, sizeof(truncSaberName));
    }

    // Set the saber with the arg given. If the arg is
    // not a valid sabername defaults will be used.
    WP_SetSaber(ent->s.number, ent->client->saber, saberNum, truncSaberName);

    if (!ent->client->saber[0].model[0]) {
        assert(0); // should never happen!
        Q_strncpyz(ent->client->pers.saber1, DEFAULT_SABER, sizeof(ent->client->pers.saber1));
    }
    else
        Q_strncpyz(ent->client->pers.saber1, ent->client->saber[0].name, sizeof(ent->client->pers.saber1));

    if (!ent->client->saber[1].model[0])
        Q_strncpyz(ent->client->pers.saber2, "none", sizeof(ent->client->pers.saber2));
    else
        Q_strncpyz(ent->client->pers.saber2, ent->client->saber[1].name, sizeof(ent->client->pers.saber2));

    if (!WP_SaberStyleValidForSaber(&ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, ent->client->ps.fd.saberAnimLevel)) {
        WP_UseFirstValidSaberStyle(&ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &ent->client->ps.fd.saberAnimLevel);
        ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = ent->client->ps.fd.saberAnimLevel;
    }

    return true;
}

/*
=================
Cmd_Follow_f
=================
*/
void Cmd_Follow_f(gentity_t* ent)
{
    int i;
    char arg[MAX_TOKEN_CHARS];

    if (ent->client->sess.spectatorState == spectatorState_t::SPECTATOR_NOT && ent->client->switchTeamTime > level.time) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")).c_str());
        return;
    }

    if (trap->Argc() != 2) {
        if (ent->client->sess.spectatorState == spectatorState_t::SPECTATOR_FOLLOW) {
            StopFollowing(ent);
        }
        return;
    }

    trap->Argv(1, arg, sizeof(arg));
    i = ClientNumberFromString(ent, arg, false);
    if (i == -1) {
        return;
    }

    // can't follow self
    if (&level.clients[i] == ent->client) {
        return;
    }

    // can't follow another spectator
    if (level.clients[i].sess.sessionTeam == team_t::TEAM_SPECTATOR) {
        return;
    }

    if (level.clients[i].tempSpectate >= level.time) {
        return;
    }

    // if they are playing a tournament game, count as a loss
    if ((level.gametype == gametype_t::GT_DUEL || level.gametype == gametype_t::GT_POWERDUEL) && ent->client->sess.sessionTeam == team_t::TEAM_FREE) {
        // WTF???
        ent->client->sess.losses++;
    }

    // first set them to spectator
    if (ent->client->sess.sessionTeam != team_t::TEAM_SPECTATOR) {
        SetTeam(ent, "spectator");
        // fix: update team switch time only if team change really happend
        if (ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR)
            ent->client->switchTeamTime = level.time + 5000;
    }

    ent->client->sess.spectatorState = spectatorState_t::SPECTATOR_FOLLOW;
    ent->client->sess.spectatorClient = i;
}

/*
=================
Cmd_FollowCycle_f
=================
*/
void Cmd_FollowCycle_f(gentity_t* ent, int dir)
{
    int clientnum;
    int original;
    bool looped = false;

    if (ent->client->sess.spectatorState == spectatorState_t::SPECTATOR_NOT && ent->client->switchTeamTime > level.time) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NOSWITCH")).c_str());
        return;
    }

    // if they are playing a tournament game, count as a loss
    if ((level.gametype == gametype_t::GT_DUEL || level.gametype == gametype_t::GT_POWERDUEL) && ent->client->sess.sessionTeam == team_t::TEAM_FREE) { // WTF???
        ent->client->sess.losses++;
    }
    // first set them to spectator
    if (ent->client->sess.spectatorState == spectatorState_t::SPECTATOR_NOT) {
        SetTeam(ent, "spectator");
        // fix: update team switch time only if team change really happend
        if (ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR)
            ent->client->switchTeamTime = level.time + 5000;
    }

    if (dir != 1 && dir != -1) {
        Q_Error(errorParm_t::ERR_DROP, "Cmd_FollowCycle_f: bad dir {}", dir);
    }

    clientnum = ent->client->sess.spectatorClient;
    original = clientnum;

    do {
        clientnum += dir;
        if (clientnum >= level.maxclients) {
            // Avoid /team follow1 crash
            if (looped) {
                clientnum = original;
                break;
            }
            else {
                clientnum = 0;
                looped = true;
            }
        }
        if (clientnum < 0) {
            if (looped) {
                clientnum = original;
                break;
            }
            else {
                clientnum = level.maxclients - 1;
                looped = true;
            }
        }

        // can only follow connected clients
        if (level.clients[clientnum].pers.connected != clientConnected_t::CON_CONNECTED) {
            continue;
        }

        // can't follow another spectator
        if (level.clients[clientnum].sess.sessionTeam == team_t::TEAM_SPECTATOR) {
            continue;
        }

        // can't follow another spectator
        if (level.clients[clientnum].tempSpectate >= level.time) {
            return;
        }

        // this is good, we can use it
        ent->client->sess.spectatorClient = clientnum;
        ent->client->sess.spectatorState = spectatorState_t::SPECTATOR_FOLLOW;
        return;
    } while (clientnum != original);

    // leave it where it was
}

void Cmd_FollowNext_f(gentity_t* ent)
{
    Cmd_FollowCycle_f(ent, 1);
}

void Cmd_FollowPrev_f(gentity_t* ent)
{
    Cmd_FollowCycle_f(ent, -1);
}

/*
==================
G_Say
==================
*/

static void G_SayTo(gentity_t* ent, gentity_t* other, sayMode_t mode, int color, const char* name, const char* message, char* locMsg)
{
    if (!other) {
        return;
    }
    if (!other->inuse) {
        return;
    }
    if (!other->client) {
        return;
    }
    if (other->client->pers.connected != clientConnected_t::CON_CONNECTED) {
        return;
    }
    if (mode == sayMode_t::SAY_TEAM && !OnSameTeam(ent, other)) {
        return;
    }
    /*
    // no chatting to players in tournaments
    if ( (level.gametype == gametype_t::GT_DUEL || level.gametype == gametype_t::GT_POWERDUEL)
        && other->client->sess.sessionTeam == team_t::TEAM_FREE
        && ent->client->sess.sessionTeam != team_t::TEAM_FREE ) {
        //Hmm, maybe some option to do so if allowed?  Or at least in developer mode...
        return;
    }
    */
    // They've requested I take this out.

    if (level.gametype == gametype_t::GT_SIEGE && ent->client &&
        (ent->client->tempSpectate >= level.time || ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR) &&
        other->client->sess.sessionTeam != team_t::TEAM_SPECTATOR &&
        other->client->tempSpectate < level.time) { // siege temp spectators should not communicate to ingame players
        return;
    }

    if (locMsg) {
        trap->SendServerCommand(other - g_entities, Q_Format("{} \"{}\" \"{}\" \"{:c}\" \"{}\" {}", mode == sayMode_t::SAY_TEAM ? "ltchat" : "lchat", name,
                                                             locMsg, color, message, ent->s.number)
                                                        .c_str());
    }
    else {
        trap->SendServerCommand(
            other - g_entities,
            Q_Format("{} \"{}{}{:c}{}\" {}", mode == sayMode_t::SAY_TEAM ? "tchat" : "chat", name, Q_COLOR_ESCAPE, color, message, ent->s.number).c_str());
    }
}

void G_Say(gentity_t* ent, gentity_t* target, sayMode_t mode, const char* chatText)
{
    int j;
    gentity_t* other;
    int color;
    char name[64];
    // don't let text be too long for malicious reasons
    char text[MAX_SAY_TEXT];
    char location[64];
    char* locMsg = nullptr;

    if (level.gametype < gametype_t::GT_TEAM && mode == sayMode_t::SAY_TEAM) {
        mode = sayMode_t::SAY_ALL;
    }

    Q_strncpyz(text, chatText, sizeof(text));

    Q_strstrip(text, "\n\r", "  ");

    switch (mode) {
    default:
    case sayMode_t::SAY_ALL:
        G_LogPrintln("say: {}: {}", ent->client->pers.netname, text);
        Q_FormatTo(name, "{}{}{}" EC ": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
        color = COLOR_GREEN;
        break;
    case sayMode_t::SAY_TEAM:
        G_LogPrintln("sayteam: {}: {}", ent->client->pers.netname, text);
        if (Team_GetLocationMsg(ent, location, sizeof(location))) {
            Q_FormatTo(name, EC "({}{}{}" EC ")" EC ": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
            locMsg = location;
        }
        else {
            Q_FormatTo(name, EC "({}{}{}" EC ")" EC ": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
        }
        color = COLOR_CYAN;
        break;
    case sayMode_t::SAY_TELL:
        if (target && target->inuse && target->client && level.gametype >= gametype_t::GT_TEAM &&
            target->client->sess.sessionTeam == ent->client->sess.sessionTeam && Team_GetLocationMsg(ent, location, sizeof(location))) {
            Q_FormatTo(name, EC "[{}{}{}" EC "]" EC ": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
            locMsg = location;
        }
        else {
            Q_FormatTo(name, EC "[{}{}{}" EC "]" EC ": ", ent->client->pers.netname, Q_COLOR_ESCAPE, COLOR_WHITE);
        }
        color = COLOR_MAGENTA;
        break;
    }

    if (target) {
        G_SayTo(ent, target, mode, color, name, text, locMsg);
        return;
    }

    // echo the text to the console
    if (dedicated.integer) {
        Q_Println("{}{}", name, text);
    }

    // send it to all the appropriate clients
    for (j = 0; j < level.maxclients; j++) {
        other = &g_entities[j];
        G_SayTo(ent, other, mode, color, name, text, locMsg);
    }
}

/*
==================
Cmd_Say_f
==================
*/
static void Cmd_Say_f(gentity_t* ent)
{
    char* p = nullptr;

    if (trap->Argc() < 2)
        return;

    p = ConcatArgs(1);

    if (strlen(p) >= MAX_SAY_TEXT) {
        p[MAX_SAY_TEXT - 1] = '\0';
        G_SecurityLogPrintln("Cmd_Say_f from {} ({}) has been truncated: {}", ent->s.number, ent->client->pers.netname, p);
    }

    G_Say(ent, nullptr, sayMode_t::SAY_ALL, p);
}

/*
==================
Cmd_SayTeam_f
==================
*/
static void Cmd_SayTeam_f(gentity_t* ent)
{
    char* p = nullptr;

    if (trap->Argc() < 2)
        return;

    p = ConcatArgs(1);

    if (strlen(p) >= MAX_SAY_TEXT) {
        p[MAX_SAY_TEXT - 1] = '\0';
        G_SecurityLogPrintln("Cmd_SayTeam_f from {} ({}) has been truncated: {}", ent->s.number, ent->client->pers.netname, p);
    }

    G_Say(ent, nullptr, (level.gametype >= gametype_t::GT_TEAM) ? sayMode_t::SAY_TEAM : sayMode_t::SAY_ALL, p);
}

/*
==================
Cmd_Tell_f
==================
*/
static void Cmd_Tell_f(gentity_t* ent)
{
    int targetNum;
    gentity_t* target;
    char* p;
    char arg[MAX_TOKEN_CHARS];

    if (trap->Argc() < 3) {
        trap->SendServerCommand(ent - g_entities, "print \"Usage: tell <player id> <message>\n\"");
        return;
    }

    trap->Argv(1, arg, sizeof(arg));
    targetNum = ClientNumberFromString(ent, arg, false);
    if (targetNum == -1) {
        return;
    }

    target = &g_entities[targetNum];
    if (!target->inuse || !target->client) {
        return;
    }

    p = ConcatArgs(2);

    if (strlen(p) >= MAX_SAY_TEXT) {
        p[MAX_SAY_TEXT - 1] = '\0';
        G_SecurityLogPrintln("Cmd_Tell_f from {} ({}) has been truncated: {}", ent->s.number, ent->client->pers.netname, p);
    }

    G_LogPrintln("tell: {} to {}: {}", ent->client->pers.netname, target->client->pers.netname, p);
    G_Say(ent, target, sayMode_t::SAY_TELL, p);
    // don't tell to the player self if it was already directed to this player
    // also don't send the chat back to a bot
    if (ent != target && !Any(ent->r.svFlags & svFlags_t::SVF_BOT)) {
        G_Say(ent, ent, sayMode_t::SAY_TELL, p);
    }
}

// siege voice command
static void Cmd_VoiceCommand_f(gentity_t* ent)
{
    gentity_t* te;
    char arg[MAX_TOKEN_CHARS];
    const char* s;
    formatText_t sText; // holds the text s points into
    int i = 0;

    if (level.gametype < gametype_t::GT_TEAM) {
        return;
    }

    if (trap->Argc() < 2) {
        return;
    }

    if (ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR || ent->client->tempSpectate >= level.time) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NOVOICECHATASSPEC")).c_str());
        return;
    }

    trap->Argv(1, arg, sizeof(arg));

    if (arg[0] == '*') { // hmm.. don't expect a * to be prepended already. maybe someone is trying to be sneaky.
        return;
    }

    s = (sText = Q_Format("*{}", arg)).c_str();

    // now, make sure it's a valid sound to be playing like this.. so people can't go around
    // screaming out death sounds or whatever.
    while (i < MAX_CUSTOM_SIEGE_SOUNDS) {
        if (!bg_customSiegeSoundNames[i]) {
            break;
        }
        if (!Q_stricmp(bg_customSiegeSoundNames[i], s)) { // it matches this one, so it's ok
            break;
        }
        i++;
    }

    if (i == MAX_CUSTOM_SIEGE_SOUNDS || !bg_customSiegeSoundNames[i]) { // didn't find it in the list
        return;
    }

    te = G_TempEntity(vec3_origin, entity_event_t::EV_VOICECMD_SOUND);
    te->s.groundEntityNum = ent->s.number;
    te->s.eventParm = G_SoundIndex((char*)bg_customSiegeSoundNames[i]);
    te->r.svFlags |= svFlags_t::SVF_BROADCAST;
}

static const char* gc_orders[] = {"hold your position", "hold this position", "come here", "cover me", "guard location", "search and destroy", "report"};
static size_t numgc_orders = ARRAY_LEN(gc_orders);

void Cmd_GameCommand_f(gentity_t* ent)
{
    int targetNum;
    unsigned int order;
    gentity_t* target;
    char arg[MAX_TOKEN_CHARS] = {0};

    if (trap->Argc() != 3) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"Usage: gc <player id> <order 0-{}>\n\"", numgc_orders - 1).c_str());
        return;
    }

    trap->Argv(2, arg, sizeof(arg));
    order = atoi(arg);

    if (order >= numgc_orders) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"Bad order: {}\n\"", static_cast<int>(order)).c_str());
        return;
    }

    trap->Argv(1, arg, sizeof(arg));
    targetNum = ClientNumberFromString(ent, arg, false);
    if (targetNum == -1)
        return;

    target = &g_entities[targetNum];
    if (!target->inuse || !target->client)
        return;

    G_LogPrintln("tell: {} to {}: {}", ent->client->pers.netname, target->client->pers.netname, gc_orders[order]);
    G_Say(ent, target, sayMode_t::SAY_TELL, gc_orders[order]);
    // don't tell to the player self if it was already directed to this player
    // also don't send the chat back to a bot
    if (ent != target && !Any(ent->r.svFlags & svFlags_t::SVF_BOT))
        G_Say(ent, ent, sayMode_t::SAY_TELL, gc_orders[order]);
}

/*
==================
Cmd_Where_f
==================
*/
void Cmd_Where_f(gentity_t* ent)
{
    // JAC: This wasn't working for non-spectators since s.origin doesn't update for active players.
    if (ent->client && ent->client->sess.sessionTeam != team_t::TEAM_SPECTATOR) { // active players use currentOrigin
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", vtos(ent->r.currentOrigin)).c_str());
    }
    else {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", vtos(ent->s.origin)).c_str());
    }
    // trap->SendServerCommand( ent-g_entities, Q_Format("print \"{}\n\"", vtos( ent->s.origin )).c_str() );
}

static const EnumArray<const char*, gametype_t, gametype_t::GT_MAX_GAME_TYPE> gameNames = {
    "Free For All", "Holocron FFA", "Jedi Master", "Duel", "Power Duel", "Single Player", "Team FFA", "Siege", "Capture the Flag", "Capture the Ysalamiri"};

/*
==================
Cmd_CallVote_f
==================
*/
extern void SiegeClearSwitchData(void); // g_saga.cpp

bool G_VoteCapturelimit(gentity_t* ent, int numArgs, const char* arg1, const char* arg2)
{
    int n = Com_Clampi(0, 0x7FFFFFFF, atoi(arg2));
    Q_FormatTo(level.voteString, "{} {}", arg1, n);
    Q_FormatTo(level.voteDisplayString, "{}", level.voteString);
    Q_strncpyz(level.voteStringClean, level.voteString, sizeof(level.voteStringClean));
    return true;
}

bool G_VoteClientkick(gentity_t* ent, int numArgs, const char* arg1, const char* arg2)
{
    int n = atoi(arg2);

    if (n < 0 || n >= level.maxclients) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"invalid client number {}.\n\"", n).c_str());
        return false;
    }

    if (g_entities[n].client->pers.connected == clientConnected_t::CON_DISCONNECTED) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"there is no client with the client number {}.\n\"", n).c_str());
        return false;
    }

    Q_FormatTo(level.voteString, "{} {}", arg1, arg2);
    Q_FormatTo(level.voteDisplayString, "{} {}", arg1, g_entities[n].client->pers.netname);
    Q_strncpyz(level.voteStringClean, level.voteString, sizeof(level.voteStringClean));
    return true;
}

bool G_VoteFraglimit(gentity_t* ent, int numArgs, const char* arg1, const char* arg2)
{
    int n = Com_Clampi(0, 0x7FFFFFFF, atoi(arg2));
    Q_FormatTo(level.voteString, "{} {}", arg1, n);
    Q_FormatTo(level.voteDisplayString, "{}", level.voteString);
    Q_strncpyz(level.voteStringClean, level.voteString, sizeof(level.voteStringClean));
    return true;
}

bool G_VoteGametype(gentity_t* ent, int numArgs, const char* arg1, const char* arg2)
{
    const int number = atoi(arg2);
    gametype_t gt;

    // ffa, ctf, tdm, etc
    if (arg2[0] && isalpha(arg2[0])) {
        gt = BG_GetGametypeForString(arg2);
        if (gt == gametype_t::GT_UNKNOWN) {
            trap->SendServerCommand(ent - g_entities, Q_Format("print \"Gametype ({}) unrecognised, defaulting to FFA/Deathmatch\n\"", arg2).c_str());
            gt = gametype_t::GT_FFA;
        }
    }
    // numeric but out of range
    else if (number < 0 || number >= std::to_underlying(gametype_t::GT_MAX_GAME_TYPE)) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"Gametype ({}) is out of range, defaulting to FFA/Deathmatch\n\"", number).c_str());
        gt = gametype_t::GT_FFA;
    }
    else {
        gt = static_cast<gametype_t>(number);
    }

    // logically invalid gametypes, or gametypes not fully implemented in MP
    if (gt == gametype_t::GT_SINGLE_PLAYER) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"This gametype is not supported ({}).\n\"", arg2).c_str());
        return false;
    }

    level.votingGametype = true;
    level.votingGametypeTo = gt;

    Q_FormatTo(level.voteString, "{} {}", arg1, gt);
    Q_FormatTo(level.voteDisplayString, "{} {}", arg1, gameNames[gt]);
    Q_strncpyz(level.voteStringClean, level.voteString, sizeof(level.voteStringClean));
    return true;
}

bool G_VoteKick(gentity_t* ent, int numArgs, const char* arg1, const char* arg2)
{
    int clientid = ClientNumberFromString(ent, arg2, true);
    gentity_t* target = nullptr;

    if (clientid == -1)
        return false;

    target = &g_entities[clientid];
    if (!target || !target->inuse || !target->client)
        return false;

    Q_FormatTo(level.voteString, "clientkick {}", clientid);
    Q_FormatTo(level.voteDisplayString, "kick {}", target->client->pers.netname);
    Q_strncpyz(level.voteStringClean, level.voteString, sizeof(level.voteStringClean));
    return true;
}

const char* G_GetArenaInfoByMap(const char* map);

void Cmd_MapList_f(gentity_t* ent)
{
    int i, toggle = 0;
    char map[24] = "--", buf[512] = {0};

    Q_strcat(buf, sizeof(buf), "Map list:");

    for (i = 0; i < level.arenas.num; i++) {
        Q_strncpyz(map, Info_ValueForKey(level.arenas.infos[i], "map"), sizeof(map));
        Q_StripColor(map);

        if (G_DoesMapSupportGametype(map, level.gametype)) {
            const formatText_t tmpMsgText = Q_Format(" ^{}{}", (++toggle & 1) ? COLOR_GREEN : COLOR_YELLOW, map);
            const char* tmpMsg = tmpMsgText.c_str();
            if (strlen(buf) + strlen(tmpMsg) >= sizeof(buf)) {
                trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\"", buf).c_str());
                buf[0] = '\0';
            }
            Q_strcat(buf, sizeof(buf), tmpMsg);
        }
    }

    trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", buf).c_str());
}

bool G_VoteMap(gentity_t* ent, int numArgs, const char* arg1, const char* arg2)
{
    char s[MAX_CVAR_VALUE_STRING] = {0}, bspName[MAX_QPATH] = {0};
    const char *mapName = nullptr, *mapName2 = nullptr;
    fileHandle_t fp = NULL_FILE;
    const char* arenaInfo;

    // didn't specify a map, show available maps
    if (numArgs < 3) {
        Cmd_MapList_f(ent);
        return false;
    }

    if (strchr(arg2, '\\')) {
        trap->SendServerCommand(ent - g_entities, "print \"Can't have mapnames with a \\\n\"");
        return false;
    }

    Q_FormatTo(bspName, "maps/{}.bsp", arg2);
    if (trap->FS_Open(bspName, &fp, fsMode_t::FS_READ) <= 0) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"Can't find map {} on server\n\"", bspName).c_str());
        if (fp != NULL_FILE)
            trap->FS_Close(fp);
        return false;
    }
    trap->FS_Close(fp);

    if (!G_DoesMapSupportGametype(arg2, level.gametype)) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTE_MAPNOTSUPPORTEDBYGAME")).c_str());
        return false;
    }

    // preserve the map rotation
    trap->Cvar_VariableStringBuffer("nextmap", s, sizeof(s));
    if (*s)
        Q_FormatTo(level.voteString, "{} {}; set nextmap \"{}\"", arg1, arg2, s);
    else
        Q_FormatTo(level.voteString, "{} {}", arg1, arg2);

    arenaInfo = G_GetArenaInfoByMap(arg2);
    if (arenaInfo) {
        mapName = Info_ValueForKey(arenaInfo, "longname");
        mapName2 = Info_ValueForKey(arenaInfo, "map");
    }

    if (!mapName || !mapName[0])
        mapName = "ERROR";

    if (!mapName2 || !mapName2[0])
        mapName2 = "ERROR";

    Q_FormatTo(level.voteDisplayString, "map {} ({})", mapName, mapName2);
    Q_strncpyz(level.voteStringClean, level.voteString, sizeof(level.voteStringClean));
    return true;
}

bool G_VoteMapRestart(gentity_t* ent, int numArgs, const char* arg1, const char* arg2)
{
    int n = Com_Clampi(0, 60, atoi(arg2));
    if (numArgs < 3)
        n = 5;
    Q_FormatTo(level.voteString, "{} {}", arg1, n);
    Q_strncpyz(level.voteDisplayString, level.voteString, sizeof(level.voteDisplayString));
    Q_strncpyz(level.voteStringClean, level.voteString, sizeof(level.voteStringClean));
    return true;
}

bool G_VoteNextmap(gentity_t* ent, int numArgs, const char* arg1, const char* arg2)
{
    char s[MAX_CVAR_VALUE_STRING];

    trap->Cvar_VariableStringBuffer("nextmap", s, sizeof(s));
    if (!*s) {
        trap->SendServerCommand(ent - g_entities, "print \"nextmap not set.\n\"");
        return false;
    }
    SiegeClearSwitchData();
    Q_FormatTo(level.voteString, "vstr nextmap");
    Q_strncpyz(level.voteDisplayString, level.voteString, sizeof(level.voteDisplayString));
    Q_strncpyz(level.voteStringClean, level.voteString, sizeof(level.voteStringClean));
    return true;
}

bool G_VoteTimelimit(gentity_t* ent, int numArgs, const char* arg1, const char* arg2)
{
    float tl = Com_Clamp(0.0f, 35790.0f, Q_atof(arg2));
    if (Q_isintegral(tl))
        Q_FormatTo(level.voteString, "{} {}", arg1, (int)tl);
    else
        Q_FormatTo(level.voteString, "{} {:.3f}", arg1, tl);
    Q_strncpyz(level.voteDisplayString, level.voteString, sizeof(level.voteDisplayString));
    Q_strncpyz(level.voteStringClean, level.voteString, sizeof(level.voteStringClean));
    return true;
}

bool G_VoteWarmup(gentity_t* ent, int numArgs, const char* arg1, const char* arg2)
{
    int n = Com_Clampi(0, 1, atoi(arg2));
    Q_FormatTo(level.voteString, "{} {}", arg1, n);
    Q_strncpyz(level.voteDisplayString, level.voteString, sizeof(level.voteDisplayString));
    Q_strncpyz(level.voteStringClean, level.voteString, sizeof(level.voteStringClean));
    return true;
}

typedef struct voteString_s {
    const char* string;
    const char* aliases; // space delimited list of aliases, will always show the real vote string
    bool (*func)(gentity_t* ent, int numArgs, const char* arg1, const char* arg2);
    int numArgs;           // number of REQUIRED arguments, not total/optional arguments
    uint32_t validGT;      // bit-flag of valid gametypes
    bool voteDelay;        // if true, will delay executing the vote string after it's accepted by g_voteDelay
    const char* shortHelp; // nullptr if no arguments needed
} voteString_t;

static voteString_t validVoteStrings[] = {
    //	vote string				aliases										# args	valid gametypes							exec delay		short help
    {"capturelimit", "caps", G_VoteCapturelimit, 1, Bit(gametype_t::GT_CTF) | Bit(gametype_t::GT_CTY), true, "<num>"},
    {"clientkick", nullptr, G_VoteClientkick, 1, GTB_ALL, false, "<clientnum>"},
    {"fraglimit", "frags", G_VoteFraglimit, 1, GTB_ALL & ~(Bit(gametype_t::GT_SIEGE) | Bit(gametype_t::GT_CTF) | Bit(gametype_t::GT_CTY)), true, "<num>"},
    {"g_doWarmup", "dowarmup warmup", G_VoteWarmup, 1, GTB_ALL, true, "<0-1>"},
    {"g_gametype", "gametype gt mode", G_VoteGametype, 1, GTB_ALL, true, "<num or name>"},
    {"kick", nullptr, G_VoteKick, 1, GTB_ALL, false, "<client name>"},
    {"map", nullptr, G_VoteMap, 0, GTB_ALL, true, "<name>"},
    {"map_restart", "restart", G_VoteMapRestart, 0, GTB_ALL, true, "<optional delay>"},
    {"nextmap", nullptr, G_VoteNextmap, 0, GTB_ALL, true, nullptr},
    {"timelimit", "time", G_VoteTimelimit, 1, GTB_ALL & ~Bit(gametype_t::GT_SIEGE), true, "<num>"},
};
static const int validVoteStringsSize = ARRAY_LEN(validVoteStrings);

void Svcmd_ToggleAllowVote_f(void)
{
    if (trap->Argc() == 1) {
        int i = 0;
        for (i = 0; i < validVoteStringsSize; i++) {
            if ((g_allowVote.integer & (1 << i)))
                Q_Println("{:2} [X] {}", i, validVoteStrings[i].string);
            else
                Q_Println("{:2} [ ] {}", i, validVoteStrings[i].string);
        }
        return;
    }
    else {
        char arg[8] = {0};
        int index;

        trap->Argv(1, arg, sizeof(arg));
        index = atoi(arg);

        if (index < 0 || index >= validVoteStringsSize) {
            Q_Println("ToggleAllowVote: Invalid range: {} [0, {}]", index, validVoteStringsSize - 1);
            return;
        }

        trap->Cvar_Set("g_allowVote", Q_Format("{}", (1 << index) ^ (g_allowVote.integer & ((1 << validVoteStringsSize) - 1))).c_str());
        trap->Cvar_Update(&g_allowVote);

        Q_Println("{} {}^7", validVoteStrings[index].string, ((g_allowVote.integer & (1 << index)) ? "^2Enabled" : "^1Disabled"));
    }
}

void Cmd_CallVote_f(gentity_t* ent)
{
    int i = 0, numArgs = 0;
    char arg1[MAX_CVAR_VALUE_STRING] = {0};
    char arg2[MAX_CVAR_VALUE_STRING] = {0};
    voteString_t* vote = nullptr;

    // not allowed to vote at all
    if (!g_allowVote.integer) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTE")).c_str());
        return;
    }

    // vote in progress
    else if (level.voteTime) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "VOTEINPROGRESS")).c_str());
        return;
    }

    // can't vote as a spectator, except in (power)duel
    else if (level.gametype != gametype_t::GT_DUEL && level.gametype != gametype_t::GT_POWERDUEL && ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NOSPECVOTE")).c_str());
        return;
    }

    // make sure it is a valid command to vote on
    numArgs = trap->Argc();
    trap->Argv(1, arg1, sizeof(arg1));
    if (numArgs > 1)
        Q_strncpyz(arg2, ConcatArgs(2), sizeof(arg2));

    // filter ; \n \r
    if (Q_strchrs(arg1, ";\r\n") || Q_strchrs(arg2, ";\r\n")) {
        trap->SendServerCommand(ent - g_entities, "print \"Invalid vote string.\n\"");
        return;
    }

    // check for invalid votes
    for (i = 0; i < validVoteStringsSize; i++) {
        if (!(g_allowVote.integer & (1 << i)))
            continue;

        if (!Q_stricmp(arg1, validVoteStrings[i].string))
            break;

        // see if they're using an alias, and set arg1 to the actual vote string
        if (validVoteStrings[i].aliases) {
            char tmp[MAX_TOKEN_CHARS] = {0}, *p = nullptr;
            const char* delim = " ";
            Q_strncpyz(tmp, validVoteStrings[i].aliases, sizeof(tmp));
            p = strtok(tmp, delim);
            while (p != nullptr) {
                if (!Q_stricmp(arg1, p)) {
                    Q_strncpyz(arg1, validVoteStrings[i].string, sizeof(arg1));
                    goto validVote;
                }
                p = strtok(nullptr, delim);
            }
        }
    }
    // invalid vote string, abandon ship
    if (i == validVoteStringsSize) {
        char buf[1024] = {0};
        int toggle = 0;
        trap->SendServerCommand(ent - g_entities, "print \"Invalid vote string.\n\"");
        trap->SendServerCommand(ent - g_entities, "print \"Allowed vote strings are: \"");
        for (i = 0; i < validVoteStringsSize; i++) {
            if (!(g_allowVote.integer & (1 << i)))
                continue;

            toggle = !toggle;
            if (validVoteStrings[i].shortHelp) {
                Q_strcat(buf, sizeof(buf),
                         Q_Format("^{}{} {} ", toggle ? COLOR_GREEN : COLOR_YELLOW, validVoteStrings[i].string, validVoteStrings[i].shortHelp).c_str());
            }
            else {
                Q_strcat(buf, sizeof(buf), Q_Format("^{}{} ", toggle ? COLOR_GREEN : COLOR_YELLOW, validVoteStrings[i].string).c_str());
            }
        }

        // FIXME: buffer and send in multiple messages in case of overflow
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", buf).c_str());
        return;
    }

validVote:
    vote = &validVoteStrings[i];
    if (!(vote->validGT & Bit(level.gametype))) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{} is not applicable in this gametype.\n\"", arg1).c_str());
        return;
    }

    if (numArgs < vote->numArgs + 2) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{} requires more arguments: {}\n\"", arg1, vote->shortHelp).c_str());
        return;
    }

    level.votingGametype = false;

    level.voteExecuteDelay = vote->voteDelay ? g_voteDelay.integer : 0;

    // there is still a vote to be executed, execute it and store the new vote
    if (level.voteExecuteTime) {
        level.voteExecuteTime = 0;
        trap->SendConsoleCommand(cbufExec_t::EXEC_APPEND, Q_Format("{}\n", level.voteString).c_str());
    }

    // pass the args onto vote-specific handlers for parsing/filtering
    if (vote->func) {
        if (!vote->func(ent, numArgs, arg1, arg2))
            return;
    }
    // otherwise assume it's a command
    else {
        Q_FormatTo(level.voteString, "{} \"{}\"", arg1, arg2);
        Q_strncpyz(level.voteDisplayString, level.voteString, sizeof(level.voteDisplayString));
        Q_strncpyz(level.voteStringClean, level.voteString, sizeof(level.voteStringClean));
    }
    Q_strstrip(level.voteStringClean, "\"\n\r", nullptr);

    trap->SendServerCommand(
        -1, Q_Format("print \"{}^7 {} ({})\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLCALLEDVOTE"), level.voteStringClean).c_str());

    // start the voting, the caller automatically votes yes
    level.voteTime = level.time;
    level.voteYes = 1;
    level.voteNo = 0;

    for (i = 0; i < level.maxclients; i++) {
        level.clients[i].mGameFlags &= ~voteFlags_t::PSG_VOTED;
        level.clients[i].pers.vote = 0;
    }

    ent->client->mGameFlags |= voteFlags_t::PSG_VOTED;
    ent->client->pers.vote = 1;

    trap->SetConfigstring(CS_VOTE_TIME, Q_Format("{}", level.voteTime).c_str());
    trap->SetConfigstring(CS_VOTE_STRING, level.voteDisplayString);
    trap->SetConfigstring(CS_VOTE_YES, Q_Format("{}", level.voteYes).c_str());
    trap->SetConfigstring(CS_VOTE_NO, Q_Format("{}", level.voteNo).c_str());
}

/*
==================
Cmd_Vote_f
==================
*/
void Cmd_Vote_f(gentity_t* ent)
{
    char msg[64] = {0};

    if (!level.voteTime) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEINPROG")).c_str());
        return;
    }
    if (Any(ent->client->mGameFlags & voteFlags_t::PSG_VOTED)) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "VOTEALREADY")).c_str());
        return;
    }
    if (level.gametype != gametype_t::GT_DUEL && level.gametype != gametype_t::GT_POWERDUEL) {
        if (ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR) {
            trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEASSPEC")).c_str());
            return;
        }
    }

    trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "PLVOTECAST")).c_str());

    ent->client->mGameFlags |= voteFlags_t::PSG_VOTED;

    trap->Argv(1, msg, sizeof(msg));

    if (tolower(msg[0]) == 'y' || msg[0] == '1') {
        level.voteYes++;
        ent->client->pers.vote = 1;
        trap->SetConfigstring(CS_VOTE_YES, Q_Format("{}", level.voteYes).c_str());
    }
    else {
        level.voteNo++;
        ent->client->pers.vote = 2;
        trap->SetConfigstring(CS_VOTE_NO, Q_Format("{}", level.voteNo).c_str());
    }

    // a majority will be determined in CheckVote, which will also account
    // for players entering or leaving
}

bool G_TeamVoteLeader(gentity_t* ent, int cs_offset, team_t team, int numArgs, const char* arg1, const char* arg2)
{
    int clientid = numArgs == 2 ? ent->s.number : ClientNumberFromString(ent, arg2, false);
    gentity_t* target = nullptr;

    if (clientid == -1)
        return false;

    target = &g_entities[clientid];
    if (!target || !target->inuse || !target->client)
        return false;

    if (target->client->sess.sessionTeam != team) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"User {} is not on your team\n\"", arg2).c_str());
        return false;
    }

    Q_FormatTo(level.teamVoteString[cs_offset], "leader {}", clientid);
    Q_strncpyz(level.teamVoteDisplayString[cs_offset], level.teamVoteString[cs_offset], sizeof(level.teamVoteDisplayString[cs_offset]));
    Q_strncpyz(level.teamVoteStringClean[cs_offset], level.teamVoteString[cs_offset], sizeof(level.teamVoteStringClean[cs_offset]));
    return true;
}

/*
==================
Cmd_CallTeamVote_f
==================
*/
void Cmd_CallTeamVote_f(gentity_t* ent)
{
    team_t team = ent->client->sess.sessionTeam;
    int i = 0, cs_offset = 0, numArgs = 0;
    char arg1[MAX_CVAR_VALUE_STRING] = {0};
    char arg2[MAX_CVAR_VALUE_STRING] = {0};

    if (g_gametype.integer < std::to_underlying(gametype_t::GT_TEAM)) {
        trap->SendServerCommand(ent - g_entities, "print \"Cannot call a team vote in a non-team gametype!\n\"");
        return;
    }

    if (team == team_t::TEAM_RED)
        cs_offset = 0;
    else if (team == team_t::TEAM_BLUE)
        cs_offset = 1;
    else
        return;

    // not allowed to vote at all
    if (!g_allowTeamVote.integer) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTE")).c_str());
        return;
    }

    // vote in progress
    else if (level.teamVoteTime[cs_offset]) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEALREADY")).c_str());
        return;
    }

    // can't vote as a spectator
    else if (ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NOSPECVOTE")).c_str());
        return;
    }

    // make sure it is a valid command to vote on
    numArgs = trap->Argc();
    trap->Argv(1, arg1, sizeof(arg1));
    if (numArgs > 1)
        Q_strncpyz(arg2, ConcatArgs(2), sizeof(arg2));

    // filter ; \n \r
    if (Q_strchrs(arg1, ";\r\n") || Q_strchrs(arg2, ";\r\n")) {
        trap->SendServerCommand(ent - g_entities, "print \"Invalid team vote string.\n\"");
        return;
    }

    // pass the args onto vote-specific handlers for parsing/filtering
    if (!Q_stricmp(arg1, "leader")) {
        if (!G_TeamVoteLeader(ent, cs_offset, team, numArgs, arg1, arg2))
            return;
    }
    else {
        trap->SendServerCommand(ent - g_entities, "print \"Invalid team vote string.\n\"");
        trap->SendServerCommand(
            ent - g_entities,
            Q_Format("print \"Allowed team vote strings are: ^{}{} {}\n\"", COLOR_GREEN, "leader", "<optional client name or number>").c_str());
        return;
    }

    Q_strstrip(level.teamVoteStringClean[cs_offset], "\"\n\r", nullptr);

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected == clientConnected_t::CON_DISCONNECTED)
            continue;
        if (level.clients[i].sess.sessionTeam == team)
            trap->SendServerCommand(
                i, Q_Format("print \"{}^7 called a team vote ({})\n\"", ent->client->pers.netname, level.teamVoteStringClean[cs_offset]).c_str());
    }

    // start the voting, the caller autoamtically votes yes
    level.teamVoteTime[cs_offset] = level.time;
    level.teamVoteYes[cs_offset] = 1;
    level.teamVoteNo[cs_offset] = 0;

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected == clientConnected_t::CON_DISCONNECTED)
            continue;
        if (level.clients[i].sess.sessionTeam == team) {
            level.clients[i].mGameFlags &= ~voteFlags_t::PSG_TEAMVOTED;
            level.clients[i].pers.teamvote = 0;
        }
    }
    ent->client->mGameFlags |= voteFlags_t::PSG_TEAMVOTED;
    ent->client->pers.teamvote = 1;

    trap->SetConfigstring(CS_TEAMVOTE_TIME + cs_offset, Q_Format("{}", level.teamVoteTime[cs_offset]).c_str());
    trap->SetConfigstring(CS_TEAMVOTE_STRING + cs_offset, level.teamVoteDisplayString[cs_offset]);
    trap->SetConfigstring(CS_TEAMVOTE_YES + cs_offset, Q_Format("{}", level.teamVoteYes[cs_offset]).c_str());
    trap->SetConfigstring(CS_TEAMVOTE_NO + cs_offset, Q_Format("{}", level.teamVoteNo[cs_offset]).c_str());
}

/*
==================
Cmd_TeamVote_f
==================
*/
void Cmd_TeamVote_f(gentity_t* ent)
{
    team_t team = ent->client->sess.sessionTeam;
    int cs_offset = 0;
    char msg[64] = {0};

    if (team == team_t::TEAM_RED)
        cs_offset = 0;
    else if (team == team_t::TEAM_BLUE)
        cs_offset = 1;
    else
        return;

    if (!level.teamVoteTime[cs_offset]) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NOTEAMVOTEINPROG")).c_str());
        return;
    }
    if (Any(ent->client->mGameFlags & voteFlags_t::PSG_TEAMVOTED)) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "TEAMVOTEALREADYCAST")).c_str());
        return;
    }
    if (ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NOVOTEASSPEC")).c_str());
        return;
    }

    trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "PLTEAMVOTECAST")).c_str());

    ent->client->mGameFlags |= voteFlags_t::PSG_TEAMVOTED;

    trap->Argv(1, msg, sizeof(msg));

    if (tolower(msg[0]) == 'y' || msg[0] == '1') {
        level.teamVoteYes[cs_offset]++;
        ent->client->pers.teamvote = 1;
        trap->SetConfigstring(CS_TEAMVOTE_YES + cs_offset, Q_Format("{}", level.teamVoteYes[cs_offset]).c_str());
    }
    else {
        level.teamVoteNo[cs_offset]++;
        ent->client->pers.teamvote = 2;
        trap->SetConfigstring(CS_TEAMVOTE_NO + cs_offset, Q_Format("{}", level.teamVoteNo[cs_offset]).c_str());
    }

    // a majority will be determined in TeamCheckVote, which will also account
    // for players entering or leaving
}

/*
=================
Cmd_SetViewpos_f
=================
*/
void Cmd_SetViewpos_f(gentity_t* ent)
{
    vec3_t origin, angles;
    char buffer[MAX_TOKEN_CHARS];
    int i;

    if (trap->Argc() != 5) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"usage: setviewpos x y z yaw\n\"").c_str());
        return;
    }

    VectorClear(angles);
    for (i = 0; i < 3; i++) {
        trap->Argv(i + 1, buffer, sizeof(buffer));
        origin[i] = Q_atof(buffer);
    }

    trap->Argv(4, buffer, sizeof(buffer));
    angles[YAW] = Q_atof(buffer);

    TeleportPlayer(ent, origin, angles);
}

void G_LeaveVehicle(gentity_t* ent, bool ConCheck)
{

    if (ent->client->ps.m_iVehicleNum) { // tell it I'm getting off
        gentity_t* veh = &g_entities[ent->client->ps.m_iVehicleNum];

        if (veh->inuse && veh->client && veh->m_pVehicle) {
            if (ConCheck) { // check connection
                clientConnected_t pCon = ent->client->pers.connected;
                ent->client->pers.connected = clientConnected_t::CON_DISCONNECTED;
                veh->m_pVehicle->m_pVehicleInfo->Eject(veh->m_pVehicle, (bgEntity_t*)ent, true);
                ent->client->pers.connected = pCon;
            }
            else { // or not.
                veh->m_pVehicle->m_pVehicleInfo->Eject(veh->m_pVehicle, (bgEntity_t*)ent, true);
            }
        }
    }

    ent->client->ps.m_iVehicleNum = 0;
}

int G_ItemUsable(playerState_t* ps, holdable_t forcedUse)
{
    vec3_t fwd, fwdorg, dest, pos;
    vec3_t yawonly;
    vec3_t mins, maxs;
    vec3_t trtest;
    trace_t tr;

    // fix: dead players shouldn't use items
    if (ps->stats[statIndex_t::STAT_HEALTH] <= 0) {
        return 0;
    }

    if (ps->m_iVehicleNum) {
        return 0;
    }

    if (Any(ps->pm_flags & pmoveFlags_t::PMF_USE_ITEM_HELD)) { // force to let go first
        return 0;
    }

    if (forcedUse == holdable_t::HI_NONE) {
        forcedUse = GI_Holdable(bg_itemlist[ps->stats[statIndex_t::STAT_HOLDABLE_ITEM]]);
    }

    if (!BG_IsItemSelectable(ps, forcedUse)) {
        return 0;
    }

    switch (forcedUse) {
    case holdable_t::HI_MEDPAC:
    case holdable_t::HI_MEDPAC_BIG:
        if (ps->stats[statIndex_t::STAT_HEALTH] >= ps->stats[statIndex_t::STAT_MAX_HEALTH]) {
            return 0;
        }

        if (ps->stats[statIndex_t::STAT_HEALTH] <= 0) {
            return 0;
        }

        return 1;
    case holdable_t::HI_SEEKER:
        if (Any(ps->eFlags & entityFlags_t::EF_SEEKERDRONE)) {
            G_AddEvent(&g_entities[ps->clientNum], entity_event_t::EV_ITEMUSEFAIL, std::to_underlying(itemUseFail_t::SEEKER_ALREADYDEPLOYED));
            return 0;
        }

        return 1;
    case holdable_t::HI_SENTRY_GUN:
        if (ps->fd.sentryDeployed) {
            G_AddEvent(&g_entities[ps->clientNum], entity_event_t::EV_ITEMUSEFAIL, std::to_underlying(itemUseFail_t::SENTRY_ALREADYPLACED));
            return 0;
        }

        yawonly[ROLL] = 0;
        yawonly[PITCH] = 0;
        yawonly[YAW] = ps->viewangles[YAW];

        VectorSet(mins, -8, -8, 0);
        VectorSet(maxs, 8, 8, 24);

        AngleVectors(yawonly, fwd, nullptr, nullptr);

        fwdorg[0] = ps->origin[0] + fwd[0] * 64;
        fwdorg[1] = ps->origin[1] + fwd[1] * 64;
        fwdorg[2] = ps->origin[2] + fwd[2] * 64;

        trtest[0] = fwdorg[0] + fwd[0] * 16;
        trtest[1] = fwdorg[1] + fwd[1] * 16;
        trtest[2] = fwdorg[2] + fwd[2] * 16;

        trap->Trace(&tr, ps->origin, mins, maxs, trtest, ps->clientNum, MASK_PLAYERSOLID, false, g2TraceFlags_t::G2TRFLAG_NONE, 0);

        if ((tr.fraction != 1 && tr.entityNum != ps->clientNum) || tr.startsolid || tr.allsolid) {
            G_AddEvent(&g_entities[ps->clientNum], entity_event_t::EV_ITEMUSEFAIL, std::to_underlying(itemUseFail_t::SENTRY_NOROOM));
            return 0;
        }

        return 1;
    case holdable_t::HI_SHIELD:
        mins[0] = -8;
        mins[1] = -8;
        mins[2] = 0;

        maxs[0] = 8;
        maxs[1] = 8;
        maxs[2] = 8;

        AngleVectors(ps->viewangles, fwd, nullptr, nullptr);
        fwd[2] = 0;
        VectorMA(ps->origin, 64, fwd, dest);
        trap->Trace(&tr, ps->origin, mins, maxs, dest, ps->clientNum, MASK_SHOT, false, g2TraceFlags_t::G2TRFLAG_NONE, 0);
        if (tr.fraction > 0.9 && !tr.startsolid && !tr.allsolid) {
            VectorCopy(tr.endpos, pos);
            VectorSet(dest, pos[0], pos[1], pos[2] - 4096);
            trap->Trace(&tr, pos, mins, maxs, dest, ps->clientNum, MASK_SOLID, false, g2TraceFlags_t::G2TRFLAG_NONE, 0);
            if (!tr.startsolid && !tr.allsolid) {
                return 1;
            }
        }
        G_AddEvent(&g_entities[ps->clientNum], entity_event_t::EV_ITEMUSEFAIL, std::to_underlying(itemUseFail_t::SHIELD_NOROOM));
        return 0;
    case holdable_t::HI_JETPACK: // do something?
        return 1;
    case holdable_t::HI_HEALTHDISP:
        return 1;
    case holdable_t::HI_AMMODISP:
        return 1;
    case holdable_t::HI_EWEB:
        return 1;
    case holdable_t::HI_CLOAK:
        return 1;
    default:
        return 1;
    }
}

void saberKnockDown(gentity_t* saberent, gentity_t* saberOwner, gentity_t* other);

void Cmd_ToggleSaber_f(gentity_t* ent)
{
    if (ent->client->ps.fd.forceGripCripple) { // if they are being gripped, don't let them unholster their saber
        if (ent->client->ps.saberHolstered) {
            return;
        }
    }

    if (ent->client->ps.saberInFlight) {
        if (ent->client->ps.saberEntityNum) { // turn it off in midair
            saberKnockDown(&g_entities[ent->client->ps.saberEntityNum], ent, ent);
        }
        return;
    }

    if (ent->client->ps.forceHandExtend != forceHandAnims_t::HANDEXTEND_NONE) {
        return;
    }

    if (ent->client->ps.weapon != weapon_t::WP_SABER) {
        return;
    }

    //	if (ent->client->ps.duelInProgress && !ent->client->ps.saberHolstered)
    //	{
    //		return;
    //	}

    if (ent->client->ps.duelTime >= level.time) {
        return;
    }

    if (ent->client->ps.saberLockTime >= level.time) {
        return;
    }

    if (ent->client && ent->client->ps.weaponTime < 1) {
        if (ent->client->ps.saberHolstered == 2) {
            ent->client->ps.saberHolstered = 0;

            if (ent->client->saber[0].soundOn) {
                G_Sound(ent, sound::SoundClass::Saber, sound::Slot::None, ent->client->saber[0].soundOn);
            }
            if (ent->client->saber[1].soundOn) {
                G_Sound(ent, sound::SoundClass::Saber, sound::Slot::None, ent->client->saber[1].soundOn);
            }
        }
        else {
            ent->client->ps.saberHolstered = 2;
            if (ent->client->saber[0].soundOff) {
                G_Sound(ent, sound::SoundClass::Saber, sound::Slot::None, ent->client->saber[0].soundOff);
            }
            if (ent->client->saber[1].soundOff && ent->client->saber[1].model[0]) {
                G_Sound(ent, sound::SoundClass::Saber, sound::Slot::None, ent->client->saber[1].soundOff);
            }
            // prevent anything from being done for 400ms after holster
            ent->client->ps.weaponTime = 400;
        }
    }
}

extern vmCvar_t d_saberStanceDebug;

extern bool WP_SaberCanTurnOffSomeBlades(saberInfo_t* saber);
void Cmd_SaberAttackCycle_f(gentity_t* ent)
{
    saber_styles_t selectLevel = saber_styles_t::SS_NONE;
    bool usingSiegeStyle = false;

    if (!ent || !ent->client) {
        return;
    }

    if (level.intermissionQueued || level.intermissiontime) {
        trap->SendServerCommand(ent - g_entities,
                                Q_Format("print \"{} (saberAttackCycle)\n\"", G_GetStringEdString("MP_SVGAME", "CANNOT_TASK_INTERMISSION")).c_str());
        return;
    }

    if (ent->health <= 0 || ent->client->tempSpectate >= level.time || ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR) {
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBEALIVE")).c_str());
        return;
    }

    if (ent->client->ps.weapon != weapon_t::WP_SABER) {
        return;
    }
    /*
    if (ent->client->ps.weaponTime > 0)
    { //no switching attack level when busy
        return;
    }
    */

    if (ent->client->saber[0].model[0] && ent->client->saber[1].model[0]) { // no cycling for akimbo
        if (WP_SaberCanTurnOffSomeBlades(&ent->client->saber[1])) {         // can turn second saber off
            if (ent->client->ps.saberHolstered == 1) {                      // have one holstered
                // unholster it
                G_Sound(ent, sound::SoundClass::Saber, sound::Slot::None, ent->client->saber[1].soundOn);
                ent->client->ps.saberHolstered = 0;
                // g_active should take care of this, but...
                ent->client->ps.fd.saberAnimLevel = saber_styles_t::SS_DUAL;
            }
            else if (ent->client->ps.saberHolstered == 0) {                                              // have none holstered
                if (Any(ent->client->saber[1].saberFlags2 & saberFlags2_t::SFL2_NO_MANUAL_DEACTIVATE)) { // can't turn it off manually
                }
                else if (ent->client->saber[1].bladeStyle2Start > 0 &&
                         Any(ent->client->saber[1].saberFlags2 & saberFlags2_t::SFL2_NO_MANUAL_DEACTIVATE2)) { // can't turn it off manually
                }
                else {
                    // turn it off
                    G_Sound(ent, sound::SoundClass::Saber, sound::Slot::None, ent->client->saber[1].soundOff);
                    ent->client->ps.saberHolstered = 1;
                    // g_active should take care of this, but...
                    ent->client->ps.fd.saberAnimLevel = saber_styles_t::SS_FAST;
                }
            }

            if (d_saberStanceDebug.integer) {
                trap->SendServerCommand(ent - g_entities, Q_Format("print \"SABERSTANCEDEBUG: Attempted to toggle dual saber blade.\n\"").c_str());
            }
            return;
        }
    }
    else if (ent->client->saber[0].numBlades > 1 && WP_SaberCanTurnOffSomeBlades(&ent->client->saber[0])) { // use staff stance then.
        if (ent->client->ps.saberHolstered == 1) {                                                          // second blade off
            if (ent->client->ps.saberInFlight) { // can't turn second blade back on if it's in the air, you naughty boy!
                if (d_saberStanceDebug.integer) {
                    trap->SendServerCommand(ent - g_entities, Q_Format("print \"SABERSTANCEDEBUG: Attempted to toggle staff blade in air.\n\"").c_str());
                }
                return;
            }
            // turn it on
            G_Sound(ent, sound::SoundClass::Saber, sound::Slot::None, ent->client->saber[0].soundOn);
            ent->client->ps.saberHolstered = 0;
            // g_active should take care of this, but...
            if (ent->client->saber[0].stylesForbidden) { // have a style we have to use
                WP_UseFirstValidSaberStyle(&ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &selectLevel);
                if (ent->client->ps.weaponTime <= 0) { // not busy, set it now
                    ent->client->ps.fd.saberAnimLevel = selectLevel;
                }
                else { // can't set it now or we might cause unexpected chaining, so queue it
                    ent->client->saberCycleQueue = selectLevel;
                }
            }
        }
        else if (ent->client->ps.saberHolstered == 0) {                                              // both blades on
            if (Any(ent->client->saber[0].saberFlags2 & saberFlags2_t::SFL2_NO_MANUAL_DEACTIVATE)) { // can't turn it off manually
            }
            else if (ent->client->saber[0].bladeStyle2Start > 0 &&
                     Any(ent->client->saber[0].saberFlags2 & saberFlags2_t::SFL2_NO_MANUAL_DEACTIVATE2)) { // can't turn it off manually
            }
            else {
                // turn second one off
                G_Sound(ent, sound::SoundClass::Saber, sound::Slot::None, ent->client->saber[0].soundOff);
                ent->client->ps.saberHolstered = 1;
                // g_active should take care of this, but...
                if (ent->client->saber[0].singleBladeStyle != saber_styles_t::SS_NONE) {
                    if (ent->client->ps.weaponTime <= 0) { // not busy, set it now
                        ent->client->ps.fd.saberAnimLevel = ent->client->saber[0].singleBladeStyle;
                    }
                    else { // can't set it now or we might cause unexpected chaining, so queue it
                        ent->client->saberCycleQueue = ent->client->saber[0].singleBladeStyle;
                    }
                }
            }
        }
        if (d_saberStanceDebug.integer) {
            trap->SendServerCommand(ent - g_entities, Q_Format("print \"SABERSTANCEDEBUG: Attempted to toggle staff blade.\n\"").c_str());
        }
        return;
    }

    if (ent->client->saberCycleQueue != saber_styles_t::SS_NONE) { // resume off of the queue if we haven't gotten a chance to update it yet
        selectLevel = ent->client->saberCycleQueue;
    }
    else {
        selectLevel = ent->client->ps.fd.saberAnimLevel;
    }

    if (level.gametype == gametype_t::GT_SIEGE && ent->client->siegeClass != -1 &&
        bgSiegeClasses[ent->client->siegeClass].saberStance) { // we have a flag of useable stances so cycle through it instead
        saber_styles_t i = selectLevel + 1;

        usingSiegeStyle = true;

        while (i != selectLevel) {                          // cycle around upward til we hit the next style or end up back on this one
            if (i >= saber_styles_t::SS_NUM_SABER_STYLES) { // loop back around to the first valid
                i = saber_styles_t::SS_FAST;
            }

            if (bgSiegeClasses[ent->client->siegeClass].saberStance & Bit(i)) { // we can use this one, select it and break out.
                selectLevel = i;
                break;
            }
            i++;
        }

        if (d_saberStanceDebug.integer) {
            trap->SendServerCommand(ent - g_entities, Q_Format("print \"SABERSTANCEDEBUG: Attempted to cycle given class stance.\n\"").c_str());
        }
    }
    else {
        selectLevel++;
        if (selectLevel > BG_MaxSaberStyle(ent->client->ps.fd.forcePowerLevel[forcePowers_t::FP_SABER_OFFENSE])) {
            selectLevel = saber_styles_t::SS_FAST;
        }
        if (d_saberStanceDebug.integer) {
            trap->SendServerCommand(ent - g_entities, Q_Format("print \"SABERSTANCEDEBUG: Attempted to cycle stance normally.\n\"").c_str());
        }
    }
    /*
    #ifndef FINAL_BUILD
        switch ( selectLevel )
        {
        case forcePowerLevels_t::FORCE_LEVEL_1:
            trap->SendServerCommand( ent-g_entities, Q_Format("print \"Lightsaber Combat Style: {}fast\n\"", S_COLOR_BLUE).c_str() );
            break;
        case forcePowerLevels_t::FORCE_LEVEL_2:
            trap->SendServerCommand( ent-g_entities, Q_Format("print \"Lightsaber Combat Style: {}medium\n\"", S_COLOR_YELLOW).c_str() );
            break;
        case forcePowerLevels_t::FORCE_LEVEL_3:
            trap->SendServerCommand( ent-g_entities, Q_Format("print \"Lightsaber Combat Style: {}strong\n\"", S_COLOR_RED).c_str() );
            break;
        }
    #endif
    */
    if (!usingSiegeStyle) {
        // make sure it's valid, change it if not
        WP_UseFirstValidSaberStyle(&ent->client->saber[0], &ent->client->saber[1], ent->client->ps.saberHolstered, &selectLevel);
    }

    if (ent->client->ps.weaponTime <= 0) { // not busy, set it now
        ent->client->ps.fd.saberAnimLevelBase = ent->client->ps.fd.saberAnimLevel = selectLevel;
    }
    else { // can't set it now or we might cause unexpected chaining, so queue it
        ent->client->ps.fd.saberAnimLevelBase = ent->client->saberCycleQueue = selectLevel;
    }
}

bool G_OtherPlayersDueling(void)
{
    int i = 0;
    gentity_t* ent;

    while (i < MAX_CLIENTS) {
        ent = &g_entities[i];

        if (ent && ent->inuse && ent->client && ent->client->ps.duelInProgress) {
            return true;
        }
        i++;
    }

    return false;
}

void Cmd_EngageDuel_f(gentity_t* ent)
{
    trace_t tr;
    vec3_t forward, fwdOrg;

    if (!g_privateDuel.integer) {
        return;
    }

    if (level.gametype == gametype_t::GT_DUEL || level.gametype == gametype_t::GT_POWERDUEL) { // rather pointless in this mode..
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NODUEL_GAMETYPE")).c_str());
        return;
    }

    // if (level.gametype >= gametype_t::GT_TEAM && level.gametype != gametype_t::GT_SIEGE)
    if (level.gametype >= gametype_t::GT_TEAM) { // no private dueling in team modes
        trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NODUEL_GAMETYPE")).c_str());
        return;
    }

    if (ent->client->ps.duelTime >= level.time) {
        return;
    }

    if (ent->client->ps.weapon != weapon_t::WP_SABER) {
        return;
    }

    /*
    if (!ent->client->ps.saberHolstered)
    { //must have saber holstered at the start of the duel
        return;
    }
    */
    // NOTE: No longer doing this..

    if (ent->client->ps.saberInFlight) {
        return;
    }

    if (ent->client->ps.duelInProgress) {
        return;
    }

    // New: Don't let a player duel if he just did and hasn't waited 10 seconds yet (note: If someone challenges him, his duel timer will reset so he can
    // accept)
    /*if (ent->client->ps.fd.privateDuelTime > level.time)
    {
        trap->SendServerCommand( ent-g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "CANTDUEL_JUSTDID")).c_str() );
        return;
    }

    if (G_OtherPlayersDueling())
    {
        trap->SendServerCommand( ent-g_entities, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "CANTDUEL_BUSY")).c_str() );
        return;
    }*/

    AngleVectors(ent->client->ps.viewangles, forward, nullptr, nullptr);

    fwdOrg[0] = ent->client->ps.origin[0] + forward[0] * 256;
    fwdOrg[1] = ent->client->ps.origin[1] + forward[1] * 256;
    fwdOrg[2] = (ent->client->ps.origin[2] + ent->client->ps.viewheight) + forward[2] * 256;

    trap->Trace(&tr, ent->client->ps.origin, nullptr, nullptr, fwdOrg, ent->s.number, MASK_PLAYERSOLID, false, g2TraceFlags_t::G2TRFLAG_NONE, 0);

    if (tr.fraction != 1 && tr.entityNum < MAX_CLIENTS) {
        gentity_t* challenged = &g_entities[tr.entityNum];

        if (!challenged || !challenged->client || !challenged->inuse || challenged->health < 1 || challenged->client->ps.stats[statIndex_t::STAT_HEALTH] < 1 ||
            challenged->client->ps.weapon != weapon_t::WP_SABER || challenged->client->ps.duelInProgress || challenged->client->ps.saberInFlight) {
            return;
        }

        if (level.gametype >= gametype_t::GT_TEAM && OnSameTeam(ent, challenged)) {
            return;
        }

        if (challenged->client->ps.duelIndex == ent->s.number && challenged->client->ps.duelTime >= level.time) {
            trap->SendServerCommand(/*challenged-g_entities*/ -1, Q_Format("print \"{} {} {}!\n\"", challenged->client->pers.netname,
                                                                           G_GetStringEdString("MP_SVGAME", "PLDUELACCEPT"), ent->client->pers.netname)
                                                                      .c_str());

            ent->client->ps.duelInProgress = true;
            challenged->client->ps.duelInProgress = true;

            ent->client->ps.duelTime = level.time + 2000;
            challenged->client->ps.duelTime = level.time + 2000;

            G_AddEvent(ent, entity_event_t::EV_PRIVATE_DUEL, 1);
            G_AddEvent(challenged, entity_event_t::EV_PRIVATE_DUEL, 1);

            // Holster their sabers now, until the duel starts (then they'll get auto-turned on to look cool)

            if (!ent->client->ps.saberHolstered) {
                if (ent->client->saber[0].soundOff) {
                    G_Sound(ent, sound::SoundClass::Saber, sound::Slot::None, ent->client->saber[0].soundOff);
                }
                if (ent->client->saber[1].soundOff && ent->client->saber[1].model[0]) {
                    G_Sound(ent, sound::SoundClass::Saber, sound::Slot::None, ent->client->saber[1].soundOff);
                }
                ent->client->ps.weaponTime = 400;
                ent->client->ps.saberHolstered = 2;
            }
            if (!challenged->client->ps.saberHolstered) {
                if (challenged->client->saber[0].soundOff) {
                    G_Sound(challenged, sound::SoundClass::Saber, sound::Slot::None, challenged->client->saber[0].soundOff);
                }
                if (challenged->client->saber[1].soundOff && challenged->client->saber[1].model[0]) {
                    G_Sound(challenged, sound::SoundClass::Saber, sound::Slot::None, challenged->client->saber[1].soundOff);
                }
                challenged->client->ps.weaponTime = 400;
                challenged->client->ps.saberHolstered = 2;
            }
        }
        else {
            // Print the message that a player has been challenged in private, only announce the actual duel initiation in private
            trap->SendServerCommand(challenged - g_entities,
                                    Q_Format("cp \"{} {}\n\"", ent->client->pers.netname, G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGE")).c_str());
            trap->SendServerCommand(ent - g_entities,
                                    Q_Format("cp \"{} {}\n\"", G_GetStringEdString("MP_SVGAME", "PLDUELCHALLENGED"), challenged->client->pers.netname).c_str());
        }

        challenged->client->ps.fd.privateDuelTime =
            0; // reset the timer in case this player just got out of a duel. He should still be able to accept the challenge.

        ent->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_DUELCHALLENGE;
        ent->client->ps.forceHandExtendTime = level.time + 1000;

        ent->client->ps.duelIndex = challenged->s.number;
        ent->client->ps.duelTime = level.time + 5000;
    }
}

#ifndef FINAL_BUILD

void Cmd_DebugSetSaberMove_f(gentity_t* self)
{
    int argNum = trap->Argc();
    char arg[MAX_STRING_CHARS];

    if (argNum < 2) {
        return;
    }

    trap->Argv(1, arg, sizeof(arg));

    if (!arg[0]) {
        return;
    }

    const int move = atoi(arg);

    if (move < std::to_underlying(saberMoveName_t::LS_NONE)) { // no move has a negative number
        return;
    }

    self->client->ps.saberMove = static_cast<saberMoveName_t>(move);
    self->client->ps.saberBlocked = saberBlockedType_t::BLOCKED_BOUNCE_MOVE;

    if (self->client->ps.saberMove >= saberMoveName_t::LS_MOVE_MAX) {
        self->client->ps.saberMove = saberMoveName_t::LS_MOVE_MAX - 1;
    }

    Q_Println("Anim for move: {}", BG_AnimName(saberMoveData[self->client->ps.saberMove].animToUse));
}

void Cmd_DebugSetBodyAnim_f(gentity_t* self)
{
    int argNum = trap->Argc();
    char arg[MAX_STRING_CHARS];
    animNumber_t i = animNumber_t::FACE_TALK0;

    if (argNum < 2) {
        return;
    }

    trap->Argv(1, arg, sizeof(arg));

    if (!arg[0]) {
        return;
    }

    while (i < animNumber_t::MAX_ANIMATIONS) {
        if (!Q_stricmp(arg, BG_AnimName(i))) {
            break;
        }
        i++;
    }

    if (i == animNumber_t::MAX_ANIMATIONS) {
        Q_Println("Animation '{}' does not exist", arg);
        return;
    }

    G_SetAnim(self, nullptr, setAnimParts_t::SETANIM_BOTH, i, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD, 0);

    Q_Println("Set body anim to {}", arg);
}
#endif

void StandardSetBodyAnim(gentity_t* self, animNumber_t anim, setAnimFlags_t flags)
{
    G_SetAnim(self, nullptr, setAnimParts_t::SETANIM_BOTH, anim, flags, 0);
}

void DismembermentTest(gentity_t* self);

void Bot_SetForcedMovement(int bot, int forward, int right, int up);

#ifndef FINAL_BUILD
extern void DismembermentByNum(gentity_t* self, int num);
extern void G_SetVehDamageFlags(gentity_t* veh, int shipSurf, int damageLevel);
#endif

bool TryGrapple(gentity_t* ent)
{
    if (ent->client->ps.weaponTime > 0) { // weapon busy
        return false;
    }
    if (ent->client->ps.forceHandExtend != forceHandAnims_t::HANDEXTEND_NONE) { // force power or knockdown or something
        return false;
    }
    if (ent->client->grappleState) { // already grappling? but weapontime should be > 0 then..
        return false;
    }

    if (ent->client->ps.weapon != weapon_t::WP_SABER && ent->client->ps.weapon != weapon_t::WP_MELEE) {
        return false;
    }

    if (ent->client->ps.weapon == weapon_t::WP_SABER && !ent->client->ps.saberHolstered) {
        Cmd_ToggleSaber_f(ent);
        if (!ent->client->ps.saberHolstered) { // must have saber holstered
            return false;
        }
    }

    // G_SetAnim(ent, &ent->client->pers.cmd, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_KYLE_PA_1,
    // setAnimFlags_t::SETANIM_FLAG_OVERRIDE|setAnimFlags_t::SETANIM_FLAG_HOLD, 0);
    G_SetAnim(ent, &ent->client->pers.cmd, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_KYLE_GRAB,
              setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD, 0);
    if (ent->client->ps.torsoAnim == animNumber_t::BOTH_KYLE_GRAB) { // providing the anim set succeeded..
        ent->client->ps.torsoTimer += 500;                           // make the hand stick out a little longer than it normally would
        if (ent->client->ps.legsAnim == ent->client->ps.torsoAnim) {
            ent->client->ps.legsTimer = ent->client->ps.torsoTimer;
        }
        ent->client->ps.weaponTime = ent->client->ps.torsoTimer;
        ent->client->dangerTime = level.time;
        return true;
    }

    return false;
}

void Cmd_TargetUse_f(gentity_t* ent)
{
    if (trap->Argc() > 1) {
        char sArg[MAX_STRING_CHARS] = {0};
        gentity_t* targ;

        trap->Argv(1, sArg, sizeof(sArg));
        targ = G_Find(nullptr, FOFS(targetname), sArg);

        while (targ) {
            if (targ->use)
                targ->use(targ, ent, ent);
            targ = G_Find(targ, FOFS(targetname), sArg);
        }
    }
}

void Cmd_TheDestroyer_f(gentity_t* ent)
{
    if (!ent->client->ps.saberHolstered || ent->client->ps.weapon != weapon_t::WP_SABER)
        return;

    Cmd_ToggleSaber_f(ent);
}

void Cmd_BotMoveForward_f(gentity_t* ent)
{
    int arg = 4000;
    int bCl = 0;
    char sarg[MAX_STRING_CHARS];

    assert(trap->Argc() > 1);
    trap->Argv(1, sarg, sizeof(sarg));

    assert(sarg[0]);
    bCl = atoi(sarg);
    Bot_SetForcedMovement(bCl, arg, -1, -1);
}

void Cmd_BotMoveBack_f(gentity_t* ent)
{
    int arg = -4000;
    int bCl = 0;
    char sarg[MAX_STRING_CHARS];

    assert(trap->Argc() > 1);
    trap->Argv(1, sarg, sizeof(sarg));

    assert(sarg[0]);
    bCl = atoi(sarg);
    Bot_SetForcedMovement(bCl, arg, -1, -1);
}

void Cmd_BotMoveRight_f(gentity_t* ent)
{
    int arg = 4000;
    int bCl = 0;
    char sarg[MAX_STRING_CHARS];

    assert(trap->Argc() > 1);
    trap->Argv(1, sarg, sizeof(sarg));

    assert(sarg[0]);
    bCl = atoi(sarg);
    Bot_SetForcedMovement(bCl, -1, arg, -1);
}

void Cmd_BotMoveLeft_f(gentity_t* ent)
{
    int arg = -4000;
    int bCl = 0;
    char sarg[MAX_STRING_CHARS];

    assert(trap->Argc() > 1);
    trap->Argv(1, sarg, sizeof(sarg));

    assert(sarg[0]);
    bCl = atoi(sarg);
    Bot_SetForcedMovement(bCl, -1, arg, -1);
}

void Cmd_BotMoveUp_f(gentity_t* ent)
{
    int arg = 4000;
    int bCl = 0;
    char sarg[MAX_STRING_CHARS];

    assert(trap->Argc() > 1);
    trap->Argv(1, sarg, sizeof(sarg));

    assert(sarg[0]);
    bCl = atoi(sarg);
    Bot_SetForcedMovement(bCl, -1, -1, arg);
}

void Cmd_AddBot_f(gentity_t* ent)
{
    // because addbot isn't a recognized command unless you're the server, but it is in the menus regardless
    trap->SendServerCommand(ent - g_entities, Q_Format("print \"{}.\n\"", G_GetStringEdString("MP_SVGAME", "ONLY_ADD_BOTS_AS_SERVER")).c_str());
}

/*
=================
ClientCommand
=================
*/

enum class commandFlags_t {
    CMD_NONE = 0,
    CMD_NOINTERMISSION = 1 << 0,
    CMD_CHEAT = 1 << 1,
    CMD_ALIVE = 1 << 2,
};
template <> inline constexpr bool enableEnumFlags<commandFlags_t> = true;

typedef struct command_s {
    const char* name;
    void (*func)(gentity_t* ent);
    commandFlags_t flags;
} command_t;

int cmdcmp(const void* a, const void* b)
{
    return Q_stricmp((const char*)a, ((command_t*)b)->name);
}

command_t commands[] = {
    {"addbot", Cmd_AddBot_f, commandFlags_t::CMD_NONE},
    {"callteamvote", Cmd_CallTeamVote_f, commandFlags_t::CMD_NOINTERMISSION},
    {"callvote", Cmd_CallVote_f, commandFlags_t::CMD_NOINTERMISSION},
    {"debugBMove_Back", Cmd_BotMoveBack_f, commandFlags_t::CMD_CHEAT | commandFlags_t::CMD_ALIVE},
    {"debugBMove_Forward", Cmd_BotMoveForward_f, commandFlags_t::CMD_CHEAT | commandFlags_t::CMD_ALIVE},
    {"debugBMove_Left", Cmd_BotMoveLeft_f, commandFlags_t::CMD_CHEAT | commandFlags_t::CMD_ALIVE},
    {"debugBMove_Right", Cmd_BotMoveRight_f, commandFlags_t::CMD_CHEAT | commandFlags_t::CMD_ALIVE},
    {"debugBMove_Up", Cmd_BotMoveUp_f, commandFlags_t::CMD_CHEAT | commandFlags_t::CMD_ALIVE},
    {"duelteam", Cmd_DuelTeam_f, commandFlags_t::CMD_NOINTERMISSION},
    {"follow", Cmd_Follow_f, commandFlags_t::CMD_NOINTERMISSION},
    {"follownext", Cmd_FollowNext_f, commandFlags_t::CMD_NOINTERMISSION},
    {"followprev", Cmd_FollowPrev_f, commandFlags_t::CMD_NOINTERMISSION},
    {"forcechanged", Cmd_ForceChanged_f, commandFlags_t::CMD_NONE},
    {"gc", Cmd_GameCommand_f, commandFlags_t::CMD_NOINTERMISSION},
    {"give", Cmd_Give_f, commandFlags_t::CMD_CHEAT | commandFlags_t::CMD_ALIVE | commandFlags_t::CMD_NOINTERMISSION},
    {"giveother", Cmd_GiveOther_f, commandFlags_t::CMD_CHEAT | commandFlags_t::CMD_NOINTERMISSION},
    {"god", Cmd_God_f, commandFlags_t::CMD_CHEAT | commandFlags_t::CMD_ALIVE | commandFlags_t::CMD_NOINTERMISSION},
    {"kill", Cmd_Kill_f, commandFlags_t::CMD_ALIVE | commandFlags_t::CMD_NOINTERMISSION},
    {"killother", Cmd_KillOther_f, commandFlags_t::CMD_CHEAT | commandFlags_t::CMD_NOINTERMISSION},
    //	{ "kylesmash",			TryGrapple,					CMD_NONE },
    {"levelshot", Cmd_LevelShot_f, commandFlags_t::CMD_CHEAT | commandFlags_t::CMD_ALIVE | commandFlags_t::CMD_NOINTERMISSION},
    {"maplist", Cmd_MapList_f, commandFlags_t::CMD_NOINTERMISSION},
    {"noclip", Cmd_Noclip_f, commandFlags_t::CMD_CHEAT | commandFlags_t::CMD_ALIVE | commandFlags_t::CMD_NOINTERMISSION},
    {"notarget", Cmd_Notarget_f, commandFlags_t::CMD_CHEAT | commandFlags_t::CMD_ALIVE | commandFlags_t::CMD_NOINTERMISSION},
    {"npc", Cmd_NPC_f, commandFlags_t::CMD_CHEAT | commandFlags_t::CMD_ALIVE},
    {"say", Cmd_Say_f, commandFlags_t::CMD_NONE},
    {"say_team", Cmd_SayTeam_f, commandFlags_t::CMD_NONE},
    {"score", Cmd_Score_f, commandFlags_t::CMD_NONE},
    {"setviewpos", Cmd_SetViewpos_f, commandFlags_t::CMD_CHEAT | commandFlags_t::CMD_NOINTERMISSION},
    {"siegeclass", Cmd_SiegeClass_f, commandFlags_t::CMD_NOINTERMISSION},
    {"team", Cmd_Team_f, commandFlags_t::CMD_NOINTERMISSION},
    //	{ "teamtask",			Cmd_TeamTask_f,				CMD_NOINTERMISSION },
    {"teamvote", Cmd_TeamVote_f, commandFlags_t::CMD_NOINTERMISSION},
    {"tell", Cmd_Tell_f, commandFlags_t::CMD_NONE},
    {"thedestroyer", Cmd_TheDestroyer_f, commandFlags_t::CMD_CHEAT | commandFlags_t::CMD_ALIVE | commandFlags_t::CMD_NOINTERMISSION},
    {"t_use", Cmd_TargetUse_f, commandFlags_t::CMD_CHEAT | commandFlags_t::CMD_ALIVE},
    {"voice_cmd", Cmd_VoiceCommand_f, commandFlags_t::CMD_NOINTERMISSION},
    {"vote", Cmd_Vote_f, commandFlags_t::CMD_NOINTERMISSION},
    {"where", Cmd_Where_f, commandFlags_t::CMD_NOINTERMISSION},
};
static const size_t numCommands = ARRAY_LEN(commands);

void ClientCommand(int clientNum)
{
    gentity_t* ent = nullptr;
    char cmd[MAX_TOKEN_CHARS] = {0};
    command_t* command = nullptr;

    ent = g_entities + clientNum;
    if (!ent->client || ent->client->pers.connected != clientConnected_t::CON_CONNECTED) {
        G_SecurityLogPrintln("ClientCommand({}) without an active connection", clientNum);
        return; // not fully in game yet
    }

    trap->Argv(0, cmd, sizeof(cmd));

    // rww - redirect bot commands
    if (strstr(cmd, "bot_") && AcceptBotCommand(cmd, ent))
        return;
    // end rww

    command = (command_t*)Q_LinearSearch(cmd, commands, numCommands, sizeof(commands[0]), cmdcmp);
    if (!command) {
        trap->SendServerCommand(clientNum, Q_Format("print \"Unknown command {}\n\"", cmd).c_str());
        return;
    }

    else if (Any(command->flags & commandFlags_t::CMD_NOINTERMISSION) && (level.intermissionQueued || level.intermissiontime)) {
        trap->SendServerCommand(clientNum, Q_Format("print \"{} ({})\n\"", G_GetStringEdString("MP_SVGAME", "CANNOT_TASK_INTERMISSION"), cmd).c_str());
        return;
    }

    else if (Any(command->flags & commandFlags_t::CMD_CHEAT) && !sv_cheats.integer) {
        trap->SendServerCommand(clientNum, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "NOCHEATS")).c_str());
        return;
    }

    else if (Any(command->flags & commandFlags_t::CMD_ALIVE) &&
             (ent->health <= 0 || ent->client->tempSpectate >= level.time || ent->client->sess.sessionTeam == team_t::TEAM_SPECTATOR)) {
        trap->SendServerCommand(clientNum, Q_Format("print \"{}\n\"", G_GetStringEdString("MP_SVGAME", "MUSTBEALIVE")).c_str());
        return;
    }

    else
        command->func(ent);
}

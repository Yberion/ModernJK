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

#include "g_local.h"

/*
=======================================================================

  SESSION DATA

Session data is the only data that stays persistant across level loads
and tournament restarts.
=======================================================================
*/

// TODO: Replace with reading/writing to file(s)

/*
================
G_WriteClientSessionData

Called on game shutdown
================
*/
void G_WriteClientSessionData(gclient_t* client)
{
    char s[MAX_CVAR_VALUE_STRING] = {0}, siegeClass[64] = {0}, IP[NET_ADDRSTRMAXLEN] = {0};
    const char* var;
    formatText_t varText; // holds the text var points into
    int i = 0;

    // for the strings, replace ' ' with 1

    Q_strncpyz(siegeClass, client->sess.siegeClass, sizeof(siegeClass));
    for (i = 0; siegeClass[i]; i++) {
        if (siegeClass[i] == ' ')
            siegeClass[i] = 1;
    }
    if (!siegeClass[0])
        Q_strncpyz(siegeClass, "none", sizeof(siegeClass));

    Q_strncpyz(IP, client->sess.IP, sizeof(IP));
    for (i = 0; IP[i]; i++) {
        if (IP[i] == ' ')
            IP[i] = 1;
    }

    // Make sure there is no space on the last entry
    Q_strcat(s, sizeof(s), Q_Format("{} ", to_underlying(client->sess.sessionTeam)).c_str());
    Q_strcat(s, sizeof(s), Q_Format("{} ", client->sess.spectatorNum).c_str());
    Q_strcat(s, sizeof(s), Q_Format("{} ", to_underlying(client->sess.spectatorState)).c_str());
    Q_strcat(s, sizeof(s), Q_Format("{} ", client->sess.spectatorClient).c_str());
    Q_strcat(s, sizeof(s), Q_Format("{} ", client->sess.wins).c_str());
    Q_strcat(s, sizeof(s), Q_Format("{} ", client->sess.losses).c_str());
    Q_strcat(s, sizeof(s), Q_Format("{:d} ", client->sess.teamLeader).c_str());
    Q_strcat(s, sizeof(s), Q_Format("{} ", client->sess.setForce).c_str());
    Q_strcat(s, sizeof(s), Q_Format("{} ", to_underlying(client->sess.saberLevel)).c_str());
    Q_strcat(s, sizeof(s), Q_Format("{} ", to_underlying(client->sess.selectedFP)).c_str());
    Q_strcat(s, sizeof(s), Q_Format("{} ", to_underlying(client->sess.duelTeam)).c_str());
    Q_strcat(s, sizeof(s), Q_Format("{} ", to_underlying(client->sess.siegeDesiredTeam)).c_str());
    Q_strcat(s, sizeof(s), Q_Format("{} ", siegeClass).c_str());
    Q_strcat(s, sizeof(s), Q_Format("{}", IP).c_str());

    var = (varText = Q_Format("session{}", client - level.clients)).c_str();

    trap->Cvar_Set(var, s);
}

/*
================
G_ReadSessionData

Called on a reconnect
================
*/
void G_ReadSessionData(gclient_t* client)
{
    char s[MAX_CVAR_VALUE_STRING] = {0};
    const char* var;
    formatText_t varText; // holds the text var points into
    int i = 0, tempSessionTeam = 0, tempSpectatorState, tempTeamLeader, tempDuelTeam = 0, tempSaberLevel = 0, tempSelectedFP = 0, tempSiegeDesiredTeam = 0;

    var = (varText = Q_Format("session{}", client - level.clients)).c_str();
    trap->Cvar_VariableStringBuffer(var, s, sizeof(s));

    sscanf(s, "%i %i %i %i %i %i %i %i %i %i %i %i %s %s",
           &tempSessionTeam, //&client->sess.sessionTeam,
           &client->sess.spectatorNum,
           &tempSpectatorState, //&client->sess.spectatorState,
           &client->sess.spectatorClient, &client->sess.wins, &client->sess.losses,
           &tempTeamLeader, //&client->sess.teamLeader,
           &client->sess.setForce,
           &tempSaberLevel,       //&client->sess.saberLevel,
           &tempSelectedFP,       //&client->sess.selectedFP,
           &tempDuelTeam,         //&client->sess.duelTeam,
           &tempSiegeDesiredTeam, //&client->sess.siegeDesiredTeam,
           client->sess.siegeClass, client->sess.IP);

    client->sess.sessionTeam = (team_t)tempSessionTeam;
    client->sess.spectatorState = (spectatorState_t)tempSpectatorState;
    client->sess.teamLeader = tempTeamLeader;
    client->sess.duelTeam = static_cast<duelTeam_t>(tempDuelTeam);
    client->sess.saberLevel = static_cast<saber_styles_t>(tempSaberLevel);
    client->sess.selectedFP = static_cast<forcePowers_t>(tempSelectedFP);
    client->sess.siegeDesiredTeam = static_cast<team_t>(tempSiegeDesiredTeam);

    // convert back to spaces from unused chars, as session data is written that way.
    for (i = 0; client->sess.siegeClass[i]; i++) {
        if (client->sess.siegeClass[i] == 1)
            client->sess.siegeClass[i] = ' ';
    }

    for (i = 0; client->sess.IP[i]; i++) {
        if (client->sess.IP[i] == 1)
            client->sess.IP[i] = ' ';
    }

    client->ps.fd.saberAnimLevel = client->sess.saberLevel;
    client->ps.fd.saberDrawAnimLevel = client->sess.saberLevel;
    client->ps.fd.forcePowerSelected = client->sess.selectedFP;
}

/*
================
G_InitSessionData

Called on a first-time connect
================
*/
void G_InitSessionData(gclient_t* client, char* userinfo, bool isBot)
{
    clientSession_t* sess;
    const char* value;

    sess = &client->sess;

    client->sess.siegeDesiredTeam = team_t::TEAM_FREE;

    // initial team determination
    if (level.gametype >= gametype_t::GT_TEAM) {
        if (g_teamAutoJoin.integer && !Any(g_entities[client - level.clients].r.svFlags & svFlags_t::SVF_BOT)) {
            sess->sessionTeam = PickTeam(-1);
            client->ps.fd.forceDoInit = 1; // every time we change teams make sure our force powers are set right
        }
        else {
            // always spawn as spectator in team games
            if (!isBot) {
                sess->sessionTeam = team_t::TEAM_SPECTATOR;
            }
            else { // Bots choose their team on creation
                value = Info_ValueForKey(userinfo, "team");
                if (value[0] == 'r' || value[0] == 'R') {
                    sess->sessionTeam = team_t::TEAM_RED;
                }
                else if (value[0] == 'b' || value[0] == 'B') {
                    sess->sessionTeam = team_t::TEAM_BLUE;
                }
                else {
                    sess->sessionTeam = PickTeam(-1);
                }
                client->ps.fd.forceDoInit = 1; // every time we change teams make sure our force powers are set right
            }
        }
    }
    else {
        value = Info_ValueForKey(userinfo, "team");
        if (value[0] == 's') {
            // a willing spectator, not a waiting-in-line
            sess->sessionTeam = team_t::TEAM_SPECTATOR;
        }
        else {
            switch (level.gametype) {
            default:
            case gametype_t::GT_FFA:
            case gametype_t::GT_HOLOCRON:
            case gametype_t::GT_JEDIMASTER:
            case gametype_t::GT_SINGLE_PLAYER:
                if (g_maxGameClients.integer > 0 && level.numNonSpectatorClients >= g_maxGameClients.integer) {
                    sess->sessionTeam = team_t::TEAM_SPECTATOR;
                }
                else {
                    sess->sessionTeam = team_t::TEAM_FREE;
                }
                break;
            case gametype_t::GT_DUEL:
                // if the game is full, go into a waiting mode
                if (level.numNonSpectatorClients >= 2) {
                    sess->sessionTeam = team_t::TEAM_SPECTATOR;
                }
                else {
                    sess->sessionTeam = team_t::TEAM_FREE;
                }
                break;
            case gametype_t::GT_POWERDUEL:
                // sess->duelTeam = duelTeam_t::DUELTEAM_LONE; //default
                {
                    int loners = 0;
                    int doubles = 0;

                    G_PowerDuelCount(&loners, &doubles, true);

                    if (!doubles || loners > (doubles / 2)) {
                        sess->duelTeam = duelTeam_t::DUELTEAM_DOUBLE;
                    }
                    else {
                        sess->duelTeam = duelTeam_t::DUELTEAM_LONE;
                    }
                }
                sess->sessionTeam = team_t::TEAM_SPECTATOR;
                break;
            }
        }
    }

    sess->spectatorState = spectatorState_t::SPECTATOR_FREE;
    AddTournamentQueue(client);

    sess->siegeClass[0] = 0;

    G_WriteClientSessionData(client);
}

/*
==================
G_InitWorldSession

==================
*/
void G_InitWorldSession(void)
{
    char s[MAX_STRING_CHARS];
    int gt;

    trap->Cvar_VariableStringBuffer("session", s, sizeof(s));
    gt = atoi(s);

    // if the gametype changed since the last session, don't use any
    // client sessions
    if (level.gametype != static_cast<gametype_t>(gt)) {
        level.newSession = true;
        Q_Println("Gametype changed, clearing session data.");
    }
}

/*
==================
G_WriteSessionData

==================
*/
void G_WriteSessionData(void)
{
    int i;

    trap->Cvar_Set("session", Q_Format("{}", to_underlying(level.gametype)).c_str());

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected == clientConnected_t::CON_CONNECTED) {
            G_WriteClientSessionData(&level.clients[i]);
        }
    }
}

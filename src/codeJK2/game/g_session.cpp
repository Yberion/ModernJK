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

#include "g_headers.h"

#include "g_local.h"
#include "objectives.h"

/*
=======================================================================

  SESSION DATA

Session data is the only data that stays persistant across level loads
and tournament restarts.
=======================================================================
*/

/*
================
G_WriteClientSessionData

Called on game shutdown
================
*/
void G_WriteClientSessionData(gclient_t* client)
{
    const char* s;
    formatText_t sText; // holds the text s points into
    const char* s2;
    formatText_t s2Text; // holds the text s2 points into
    const char* var;
    formatText_t varText; // holds the text var points into
    weapon_t i;

    s = (sText = Q_Format("{}", to_underlying(client->sess.sessionTeam))).c_str();
    var = (varText = Q_Format("session{}", client - level.clients)).c_str();
    gi.cvar_set(var, s);

    s2 = "";
    // Throw all status info into a string
    for (objectiveNumber_t o = objectiveNumber_t::KEJIM_POST_OBJ1; o < objectiveNumber_t::MAX_OBJECTIVES; o++) {
        s2 = (s2Text = Q_Format("{} {} {}", s2, client->sess.mission_objectives[o].display, client->sess.mission_objectives[o].status)).c_str();
    }
    var = (varText = Q_Format("sessionobj{}", client - level.clients)).c_str();
    gi.cvar_set(var, s2);

    // Throw all mission stats in to a string
    s2 = (s2Text = Q_Format("{} {} {} {} {} {} {} {} {} {} {} {}", client->sess.missionStats.secretsFound, client->sess.missionStats.totalSecrets,
                            client->sess.missionStats.shotsFired, client->sess.missionStats.hits, client->sess.missionStats.enemiesSpawned,
                            client->sess.missionStats.enemiesKilled, client->sess.missionStats.saberThrownCnt, client->sess.missionStats.saberBlocksCnt,
                            client->sess.missionStats.legAttacksCnt, client->sess.missionStats.armAttacksCnt, client->sess.missionStats.torsoAttacksCnt,
                            client->sess.missionStats.otherAttacksCnt))
             .c_str();

    var = (varText = Q_Format("missionstats{}", client - level.clients)).c_str();
    gi.cvar_set(var, s2);

    s2 = "";
    for (forcePowers_t fp = forcePowers_t::FP_FIRST; fp < forcePowers_t::NUM_FORCE_POWERS; fp++) {
        s2 = (s2Text = Q_Format("{} {}", s2, client->sess.missionStats.forceUsed[fp])).c_str();
    }
    var = (varText = Q_Format("sessionpowers{}", client - level.clients)).c_str();
    gi.cvar_set(var, s2);

    s2 = "";
    for (i = weapon_t::WP_NONE; i < weapon_t::WP_NUM_WEAPONS; i++) {
        s2 = (s2Text = Q_Format("{} {}", s2, client->sess.missionStats.weaponUsed[i])).c_str();
    }
    var = (varText = Q_Format("sessionweapons{}", client - level.clients)).c_str();
    gi.cvar_set(var, s2);
}

/*
================
G_ReadSessionData

Called on a reconnect
================
*/
void G_ReadSessionData(gclient_t* client)
{
    char s[MAX_STRING_CHARS];
    const char* var;
    formatText_t varText; // holds the text var points into
    weapon_t i;

    var = (varText = Q_Format("session{}", client - level.clients)).c_str();
    gi.Cvar_VariableStringBuffer(var, s, sizeof(s));

    int tmp;
    sscanf(s, "%i", &tmp);
    client->sess.sessionTeam = (team_t)tmp;

    var = (varText = Q_Format("sessionobj{}", client - level.clients)).c_str();
    gi.Cvar_VariableStringBuffer(var, s, sizeof(s));

    var = s;
    var++;
    for (objectiveNumber_t o = objectiveNumber_t::KEJIM_POST_OBJ1; o < objectiveNumber_t::MAX_OBJECTIVES; o++) {
        sscanf(var, "%i %i", &client->sess.mission_objectives[o].display, &client->sess.mission_objectives[o].status);
        var += 4;
    }

    var = (varText = Q_Format("missionstats{}", client - level.clients)).c_str();
    gi.Cvar_VariableStringBuffer(var, s, sizeof(s));
    sscanf(s, "%i %i %i %i %i %i %i %i %i %i %i %i", &client->sess.missionStats.secretsFound, &client->sess.missionStats.totalSecrets,
           &client->sess.missionStats.shotsFired, &client->sess.missionStats.hits, &client->sess.missionStats.enemiesSpawned,
           &client->sess.missionStats.enemiesKilled, &client->sess.missionStats.saberThrownCnt, &client->sess.missionStats.saberBlocksCnt,
           &client->sess.missionStats.legAttacksCnt, &client->sess.missionStats.armAttacksCnt, &client->sess.missionStats.torsoAttacksCnt,
           &client->sess.missionStats.otherAttacksCnt);

    var = (varText = Q_Format("sessionpowers{}", client - level.clients)).c_str();
    gi.Cvar_VariableStringBuffer(var, s, sizeof(s));

    forcePowers_t fp = forcePowers_t::FP_FIRST;
    var = strtok(s, " ");
    while (var != nullptr && fp < forcePowers_t::NUM_FORCE_POWERS) {
        /* While there are tokens in "s" */
        client->sess.missionStats.forceUsed[fp++] = atoi(var);
        /* Get next token: */
        var = strtok(nullptr, " ");
    }
    assert(fp == forcePowers_t::NUM_FORCE_POWERS);

    var = (varText = Q_Format("sessionweapons{}", client - level.clients)).c_str();
    gi.Cvar_VariableStringBuffer(var, s, sizeof(s));

    i = weapon_t::WP_NONE;
    var = strtok(s, " ");
    while (var != nullptr && i < weapon_t::WP_NUM_WEAPONS) {
        /* While there are tokens in "s" */
        client->sess.missionStats.weaponUsed[i++] = atoi(var);
        /* Get next token: */
        var = strtok(nullptr, " ");
    }
    assert(i == weapon_t::WP_NUM_WEAPONS);
}

/*
================
G_InitSessionData

Called on a first-time connect
================
*/
void G_InitSessionData(gclient_t* client, char* userinfo)
{
    clientSession_t* sess;

    sess = &client->sess;

    sess->sessionTeam = team_t::TEAM_FREE;

    G_WriteClientSessionData(client);
}

/*
==================
G_InitWorldSession

==================
*/
void G_InitWorldSession(void) {}

/*
==================
G_WriteSessionData

==================
*/
void G_WriteSessionData(void)
{
    int i;

    gi.cvar_set("session", nullptr);

    for (i = 0; i < level.maxclients; i++) {
        if (level.clients[i].pers.connected == clientConnected_t::CON_CONNECTED) {
            G_WriteClientSessionData(&level.clients[i]);
        }
    }
}

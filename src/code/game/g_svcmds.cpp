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

#include <cgame/cg_local.h>
#include "Q3_Interface.h"

#include "g_local.h"
#include "wp_saber.h"
#include "g_functions.h"

extern void G_NextTestAxes(void);
extern void G_ChangePlayerModel(gentity_t* ent, const char* newModel);
extern void G_InitPlayerFromCvars(gentity_t* ent);
extern void Q3_SetViewEntity(int entID, const char* name);
extern bool G_ClearViewEntity(gentity_t* ent);
extern void G_Knockdown(gentity_t* self, gentity_t* attacker, const vec3_t pushDir, float strength, bool breakSaberLock);

extern void WP_SetSaber(gentity_t* ent, int saberNum, const char* saberName);
extern void WP_RemoveSaber(gentity_t* ent, int saberNum);
extern saber_colors_t TranslateSaberColor(const char* name);
extern bool WP_SaberBladeUseSecondBladeStyle(saberInfo_t* saber, int bladeNum);
extern bool WP_UseFirstValidSaberStyle(gentity_t* ent, saber_styles_t* saberAnimLevel);

extern void G_SetWeapon(gentity_t* self, weapon_t wp);
extern stringID_table_t WPTable[];

extern cvar_t* g_char_model;
extern cvar_t* g_char_skin_head;
extern cvar_t* g_char_skin_torso;
extern cvar_t* g_char_skin_legs;
extern cvar_t* g_char_color_red;
extern cvar_t* g_char_color_green;
extern cvar_t* g_char_color_blue;
extern cvar_t* g_saber;
extern cvar_t* g_saber2;
extern cvar_t* g_saber_color;
extern cvar_t* g_saber2_color;

/*
===================
Svcmd_EntityList_f
===================
*/
void Svcmd_EntityList_f(void)
{
    int e;
    gentity_t* check;

    check = g_entities;
    for (e = 0; e < globals.num_entities; e++, check++) {
        if (!check->inuse) {
            continue;
        }
        Q_Print("{:3}:", e);
        switch (check->s.eType) {
        case entityType_t::ET_GENERAL:
            Q_Print("ET_GENERAL          ");
            break;
        case entityType_t::ET_PLAYER:
            Q_Print("ET_PLAYER           ");
            break;
        case entityType_t::ET_ITEM:
            Q_Print("ET_ITEM             ");
            break;
        case entityType_t::ET_MISSILE:
            Q_Print("ET_MISSILE          ");
            break;
        case entityType_t::ET_MOVER:
            Q_Print("ET_MOVER            ");
            break;
        case entityType_t::ET_BEAM:
            Q_Print("ET_BEAM             ");
            break;
        case entityType_t::ET_PORTAL:
            Q_Print("ET_PORTAL           ");
            break;
        case entityType_t::ET_SPEAKER:
            Q_Print("ET_SPEAKER          ");
            break;
        case entityType_t::ET_PUSH_TRIGGER:
            Q_Print("ET_PUSH_TRIGGER     ");
            break;
        case entityType_t::ET_TELEPORT_TRIGGER:
            Q_Print("ET_TELEPORT_TRIGGER ");
            break;
        case entityType_t::ET_INVISIBLE:
            Q_Print("ET_INVISIBLE        ");
            break;
        case entityType_t::ET_THINKER:
            Q_Print("ET_THINKER          ");
            break;
        case entityType_t::ET_CLOUD:
            Q_Print("ET_CLOUD            ");
            break;
        case entityType_t::ET_TERRAIN:
            Q_Print("ET_TERRAIN          ");
            break;
        default:
            Q_Print("{:<3}                ", check->s.eType);
            break;
        }

        if (check->classname) {
            Q_Print("{}", check->classname);
        }
        Q_Println("");
    }
}

//---------------------------
extern void G_StopCinematicSkip(void);
extern void G_StartCinematicSkip(void);
extern void ExitEmplacedWeapon(gentity_t* ent);
static void Svcmd_ExitView_f(void)
{
    extern cvar_t* g_skippingcin;
    static int exitViewDebounce = 0;
    if (exitViewDebounce > level.time) {
        return;
    }
    exitViewDebounce = level.time + 500;
    if (in_camera) {                // see if we need to exit an in-game cinematic
        if (g_skippingcin->integer) // already doing cinematic skip?
        {                           // yes...   so stop skipping...
            G_StopCinematicSkip();
        }
        else { // no... so start skipping...
            G_StartCinematicSkip();
        }
    }
    else if (!G_ClearViewEntity(player)) { // didn't exit control of a droid or turret
        // okay, now try exiting emplaced guns or AT-ST's
        if (Any(player->s.eFlags & entityFlags_t::EF_LOCKED_TO_WEAPON)) { // get out of emplaced gun
            ExitEmplacedWeapon(player);
        }
        else if (player->client && player->client->NPC_class == class_t::CLASS_ATST) { // a player trying to get out of his ATST
            GEntity_UseFunc(player->activator, player, player);
        }
    }
}

gentity_t* G_GetSelfForPlayerCmd(void)
{
    if (g_entities[0].client->ps.viewEntity > 0 && g_entities[0].client->ps.viewEntity < ENTITYNUM_WORLD &&
        g_entities[g_entities[0].client->ps.viewEntity].client &&
        g_entities[g_entities[0].client->ps.viewEntity].s.weapon == weapon_t::WP_SABER) { // you're controlling another NPC
        return (&g_entities[g_entities[0].client->ps.viewEntity]);
    }
    else {
        return (&g_entities[0]);
    }
}

static void Svcmd_Saber_f()
{
    const char* saber = gi.argv(1);
    const char* saber2 = gi.argv(2);
    char name[MAX_CVAR_VALUE_STRING] = {0};

    if (gi.argc() < 2) {
        Q_Println("Usage: saber <saber1> <saber2>");
        gi.Cvar_VariableStringBuffer("g_saber", name, sizeof(name));
        Q_Println("g_saber is set to {}", name);
        gi.Cvar_VariableStringBuffer("g_saber2", name, sizeof(name));
        if (name[0])
            Q_Println("g_saber2 is set to {}", name);
        return;
    }

    if (!g_entities[0].client || !saber || !saber[0]) {
        return;
    }

    gi.cvar_set("g_saber", saber);
    WP_SetSaber(&g_entities[0], 0, saber);
    if (saber2 && saber2[0] &&
        !Any(g_entities[0].client->ps.saber[0].saberFlags & saberFlags_t::SFL_TWO_HANDED)) { // want to use a second saber and first one is not twoHanded
        gi.cvar_set("g_saber2", saber2);
        WP_SetSaber(&g_entities[0], 1, saber2);
    }
    else {
        gi.cvar_set("g_saber2", "");
        WP_RemoveSaber(&g_entities[0], 1);
    }
}

static void Svcmd_SaberBlade_f()
{
    if (gi.argc() < 2) {
        Q_Println("USAGE: saberblade <sabernum> <bladenum> [0 = off, 1 = on, no arg = toggle]");
        return;
    }
    if (g_entities[0].client == nullptr) {
        return;
    }
    int sabernum = atoi(gi.argv(1)) - 1;
    if (sabernum < 0 || sabernum > 1) {
        return;
    }
    if (sabernum > 0 && !g_entities[0].client->ps.dualSabers) {
        return;
    }
    // FIXME: what if don't even have a single saber at all?
    int bladenum = atoi(gi.argv(2)) - 1;
    if (bladenum < 0 || bladenum >= g_entities[0].client->ps.saber[sabernum].numBlades) {
        return;
    }
    bool turnOn;
    if (gi.argc() > 2) { // explicit
        turnOn = (atoi(gi.argv(3)) != 0);
    }
    else { // toggle
        turnOn = !g_entities[0].client->ps.saber[sabernum].blade[bladenum].active;
    }

    g_entities[0].client->ps.SaberBladeActivate(sabernum, bladenum, turnOn);
}

static void Svcmd_SaberColor_f()
{ // FIXME: just list the colors, each additional listing sets that blade
    int saberNum = atoi(gi.argv(1));
    const char* color[MAX_BLADES];
    int bladeNum;

    for (bladeNum = 0; bladeNum < MAX_BLADES; bladeNum++) {
        color[bladeNum] = gi.argv(2 + bladeNum);
    }

    if (saberNum < 1 || saberNum > 2 || gi.argc() < 3) {
        Q_Println("Usage:  saberColor <saberNum> <blade1 color> <blade2 color> ... <blade8 color>");
        Q_Println("valid saberNums:  1 or 2");
        Q_Println("valid colors:  red, orange, yellow, green, blue, and purple");

        return;
    }
    saberNum--;

    gentity_t* self = G_GetSelfForPlayerCmd();

    for (bladeNum = 0; bladeNum < MAX_BLADES; bladeNum++) {
        if (!color[bladeNum] || !color[bladeNum][0]) {
            break;
        }
        else {
            self->client->ps.saber[saberNum].blade[bladeNum].color = TranslateSaberColor(color[bladeNum]);
        }
    }

    if (saberNum == 0) {
        gi.cvar_set("g_saber_color", color[0]);
    }
    else if (saberNum == 1) {
        gi.cvar_set("g_saber2_color", color[0]);
    }
}

struct SetForceCmd {
    const char* desc;
    const char* cmdname;
    const forcePowerLevels_t maxlevel;
};

EnumArray<SetForceCmd, forcePowers_t, forcePowers_t::NUM_FORCE_POWERS> SetForceTable = {{
    {"forceHeal", "setForceHeal", forcePowerLevels_t::FORCE_LEVEL_3},
    {"forceJump", "setForceJump", forcePowerLevels_t::FORCE_LEVEL_3},
    {"forceSpeed", "setForceSpeed", forcePowerLevels_t::FORCE_LEVEL_3},
    {"forcePush", "setForcePush", forcePowerLevels_t::FORCE_LEVEL_3},
    {"forcePull", "setForcePull", forcePowerLevels_t::FORCE_LEVEL_3},
    {"forceMindTrick", "setForceMindTrick", forcePowerLevels_t::FORCE_LEVEL_4},
    {"forceGrip", "setForceGrip", forcePowerLevels_t::FORCE_LEVEL_3},
    {"forceLightning", "setForceLightning", forcePowerLevels_t::FORCE_LEVEL_3},
    {"saberThrow", "setSaberThrow", forcePowerLevels_t::FORCE_LEVEL_3},
    {"saberDefense", "setSaberDefense", forcePowerLevels_t::FORCE_LEVEL_3},
    {"saberOffense", "setSaberOffense",
     forcePowerLevels_t::FORCE_LEVEL_0 + (saber_styles_t::SS_NUM_SABER_STYLES - saber_styles_t::SS_FAST)}, // one level per style, past FORCE_LEVEL_5
    {"forceRage", "setForceRage", forcePowerLevels_t::FORCE_LEVEL_3},
    {"forceProtect", "setForceProtect", forcePowerLevels_t::FORCE_LEVEL_3},
    {"forceAbsorb", "setForceAbsorb", forcePowerLevels_t::FORCE_LEVEL_3},
    {"forceDrain", "setForceDrain", forcePowerLevels_t::FORCE_LEVEL_3},
    {"forceSight", "setForceSight", forcePowerLevels_t::FORCE_LEVEL_3},
}};

static void Svcmd_ForceSetLevel_f(forcePowers_t forcePower)
{
    if (!g_entities[0].client) {
        return;
    }
    const char* newVal = gi.argv(1);
    if (!VALIDSTRING(newVal)) {
        Q_Println("Current {} level is {}", SetForceTable[forcePower].desc, g_entities[0].client->ps.forcePowerLevel[forcePower]);
        Q_Println("Usage:  {} <level> (0 - {})", SetForceTable[forcePower].cmdname, SetForceTable[forcePower].maxlevel);
        return;
    }
    const forcePowerLevels_t val = static_cast<forcePowerLevels_t>(atoi(newVal));
    if (val > forcePowerLevels_t::FORCE_LEVEL_0) {
        g_entities[0].client->ps.forcePowersKnown |= Bit(forcePower);
    }
    else {
        g_entities[0].client->ps.forcePowersKnown &= ~Bit(forcePower);
    }
    g_entities[0].client->ps.forcePowerLevel[forcePower] = val;
    if (g_entities[0].client->ps.forcePowerLevel[forcePower] < forcePowerLevels_t::FORCE_LEVEL_0) {
        g_entities[0].client->ps.forcePowerLevel[forcePower] = forcePowerLevels_t::FORCE_LEVEL_0;
    }
    else if (g_entities[0].client->ps.forcePowerLevel[forcePower] > SetForceTable[forcePower].maxlevel) {
        g_entities[0].client->ps.forcePowerLevel[forcePower] = SetForceTable[forcePower].maxlevel;
    }
}

extern bool PM_SaberInStart(saberMoveName_t move);
extern bool PM_SaberInTransition(saberMoveName_t move);
extern bool PM_SaberInAttack(saberMoveName_t move);
extern bool WP_SaberCanTurnOffSomeBlades(saberInfo_t* saber);
void Svcmd_SaberAttackCycle_f(void)
{
    if (!g_entities[0].client) {
        return;
    }

    gentity_t* self = G_GetSelfForPlayerCmd();
    if (self->s.weapon != weapon_t::WP_SABER) { // saberAttackCycle button also switches to saber
        gi.SendConsoleCommand("weapon 1");
        return;
    }

    if (self->client->ps.dualSabers) {                                  // can't cycle styles with dualSabers, so just toggle second saber on/off
        if (WP_SaberCanTurnOffSomeBlades(&self->client->ps.saber[1])) { // can turn second saber off
            if (self->client->ps.saber[1].ActiveManualOnly()) {         // turn it off
                bool skipThisBlade;
                for (int bladeNum = 0; bladeNum < self->client->ps.saber[1].numBlades; bladeNum++) {
                    skipThisBlade = false;
                    if (WP_SaberBladeUseSecondBladeStyle(&self->client->ps.saber[1], bladeNum)) { // check to see if we should check the secondary style's flags
                        if (Any(self->client->ps.saber[1].saberFlags2 & saberFlags2_t::SFL2_NO_MANUAL_DEACTIVATE2)) {
                            skipThisBlade = true;
                        }
                    }
                    else { // use the primary style's flags
                        if (Any(self->client->ps.saber[1].saberFlags2 & saberFlags2_t::SFL2_NO_MANUAL_DEACTIVATE)) {
                            skipThisBlade = true;
                        }
                    }
                    if (!skipThisBlade) {
                        self->client->ps.saber[1].BladeActivate(bladeNum, false);
                        G_SoundIndexOnEnt(self, soundChannel_t::CHAN_WEAPON, self->client->ps.saber[1].soundOff);
                    }
                }
            }
            else if (!self->client->ps.saber[0].ActiveManualOnly()) { // first one is off, too, so just turn that one on
                if (!self->client->ps.saberInFlight) {                // but only if it's in your hand!
                    self->client->ps.saber[0].Activate();
                }
            }
            else { // turn on the second one
                self->client->ps.saber[1].Activate();
            }
            return;
        }
    }
    else if (self->client->ps.saber[0].numBlades > 1 &&
             WP_SaberCanTurnOffSomeBlades(&self->client->ps.saber[0])) // self->client->ps.saber[0].type == saberType_t::SABER_STAFF )
    {                                                                  // can't cycle styles with saberstaff, so just toggles saber blades on/off
        if (self->client->ps.saberInFlight) {                          // can't turn second blade back on if it's in the air, you naughty boy!
            return;
        }
        /*
        if ( self->client->ps.saber[0].singleBladeStyle == saber_styles_t::SS_NONE )
        {//can't use just one blade?
            return;
        }
        */
        bool playedSound = false;
        if (!self->client->ps.saber[0].blade[0].active) { // first one is not even on
            // turn only it on
            self->client->ps.SaberBladeActivate(0, 0, true);
            return;
        }

        bool skipThisBlade;
        for (int bladeNum = 1; bladeNum < self->client->ps.saber[0].numBlades; bladeNum++) {
            if (!self->client->ps.saber[0].blade[bladeNum].active) { // extra is off, turn it on
                self->client->ps.saber[0].BladeActivate(bladeNum, true);
            }
            else { // turn extra off
                skipThisBlade = false;
                if (WP_SaberBladeUseSecondBladeStyle(&self->client->ps.saber[1], bladeNum)) { // check to see if we should check the secondary style's flags
                    if (Any(self->client->ps.saber[1].saberFlags2 & saberFlags2_t::SFL2_NO_MANUAL_DEACTIVATE2)) {
                        skipThisBlade = true;
                    }
                }
                else { // use the primary style's flags
                    if (Any(self->client->ps.saber[1].saberFlags2 & saberFlags2_t::SFL2_NO_MANUAL_DEACTIVATE)) {
                        skipThisBlade = true;
                    }
                }
                if (!skipThisBlade) {
                    self->client->ps.saber[0].BladeActivate(bladeNum, false);
                    if (!playedSound) {
                        G_SoundIndexOnEnt(self, soundChannel_t::CHAN_WEAPON, self->client->ps.saber[0].soundOff);
                        playedSound = true;
                    }
                }
            }
        }
        return;
    }

    int allowedStyles = self->client->ps.saberStylesKnown;
    if (self->client->ps.dualSabers && self->client->ps.saber[0].Active() && self->client->ps.saber[1].Active()) {
        allowedStyles |= Bit(saber_styles_t::SS_DUAL);
        for (saber_styles_t styleNum = saber_styles_t::SS_NONE + 1; styleNum < saber_styles_t::SS_NUM_SABER_STYLES; styleNum++) {
            if (styleNum == saber_styles_t::SS_TAVION &&
                ((self->client->ps.saber[0].stylesLearned & Bit(saber_styles_t::SS_TAVION)) ||
                 (self->client->ps.saber[1].stylesLearned & Bit(saber_styles_t::SS_TAVION))) // was given this style by one of my sabers
                && !(self->client->ps.saber[0].stylesForbidden & Bit(saber_styles_t::SS_TAVION)) &&
                !(self->client->ps.saber[1].stylesForbidden &
                  Bit(saber_styles_t::SS_TAVION))) { // if have both sabers on, allow tavion only if one of our sabers specifically wanted to use it... (unless
                                                     // specifically forbidden)
            }
            else if (styleNum == saber_styles_t::SS_DUAL && !(self->client->ps.saber[0].stylesForbidden & Bit(saber_styles_t::SS_DUAL)) &&
                     !(self->client->ps.saber[1].stylesForbidden &
                       Bit(saber_styles_t::SS_DUAL))) { // if have both sabers on, only dual style is allowed (unless specifically forbidden)
            }
            else {
                allowedStyles &= ~Bit(styleNum);
            }
        }
    }

    if (!allowedStyles) {
        return;
    }

    saber_styles_t saberAnimLevel;
    if (!self->s.number) {
        saberAnimLevel = cg.saberAnimLevelPending;
    }
    else {
        saberAnimLevel = self->client->ps.saberAnimLevel;
    }
    saberAnimLevel++;
    int sanityCheck = 0;
    while (self->client->ps.saberAnimLevel != saberAnimLevel && !(allowedStyles & Bit(saberAnimLevel)) &&
           sanityCheck < std::to_underlying(saber_styles_t::SS_NUM_SABER_STYLES) + 1) {
        saberAnimLevel++;
        if (saberAnimLevel > saber_styles_t::SS_STAFF) {
            saberAnimLevel = saber_styles_t::SS_FAST;
        }
        sanityCheck++;
    }

    if (!(allowedStyles & Bit(saberAnimLevel))) {
        return;
    }

    WP_UseFirstValidSaberStyle(self, &saberAnimLevel);
    if (!self->s.number) {
        cg.saberAnimLevelPending = saberAnimLevel;
    }
    else {
        self->client->ps.saberAnimLevel = saberAnimLevel;
    }

#ifndef FINAL_BUILD
    switch (saberAnimLevel) {
    case saber_styles_t::SS_FAST:
        Q_Println(S_COLOR_BLUE "Lightsaber Combat Style: Fast");
        // LIGHTSABERCOMBATSTYLE_FAST
        break;
    case saber_styles_t::SS_MEDIUM:
        Q_Println(S_COLOR_YELLOW "Lightsaber Combat Style: Medium");
        // LIGHTSABERCOMBATSTYLE_MEDIUM
        break;
    case saber_styles_t::SS_STRONG:
        Q_Println(S_COLOR_RED "Lightsaber Combat Style: Strong");
        // LIGHTSABERCOMBATSTYLE_STRONG
        break;
    case saber_styles_t::SS_DESANN:
        Q_Println(S_COLOR_CYAN "Lightsaber Combat Style: Desann");
        // LIGHTSABERCOMBATSTYLE_DESANN
        break;
    case saber_styles_t::SS_TAVION:
        Q_Println(S_COLOR_MAGENTA "Lightsaber Combat Style: Tavion");
        // LIGHTSABERCOMBATSTYLE_TAVION
        break;
    case saber_styles_t::SS_DUAL:
        Q_Println(S_COLOR_MAGENTA "Lightsaber Combat Style: Dual");
        // LIGHTSABERCOMBATSTYLE_TAVION
        break;
    case saber_styles_t::SS_STAFF:
        Q_Println(S_COLOR_MAGENTA "Lightsaber Combat Style: Staff");
        // LIGHTSABERCOMBATSTYLE_TAVION
        break;
    default:
        break;
    }
    // Q_Println("");
#endif
}

bool G_ReleaseEntity(gentity_t* grabber)
{
    if (grabber && grabber->client && grabber->client->ps.heldClient < ENTITYNUM_WORLD) {
        gentity_t* heldClient = &g_entities[grabber->client->ps.heldClient];
        grabber->client->ps.heldClient = ENTITYNUM_NONE;
        if (heldClient && heldClient->client) {
            heldClient->client->ps.heldByClient = ENTITYNUM_NONE;

            heldClient->owner = nullptr;
        }
        return true;
    }
    return false;
}

void G_GrabEntity(gentity_t* grabber, const char* target)
{
    if (!grabber || !grabber->client) {
        return;
    }
    gentity_t* heldClient = G_Find(nullptr, FOFS(targetname), (char*)target);
    if (heldClient && heldClient->client && heldClient != grabber) // don't grab yourself, it's not polite
    {                                                              // found him
        grabber->client->ps.heldClient = heldClient->s.number;
        heldClient->client->ps.heldByClient = grabber->s.number;

        heldClient->owner = grabber;
    }
}

static void Svcmd_ICARUS_f(void)
{
    Quake3Game()->Svcmd();
}

template <forcePowers_t power> static void Svcmd_ForceSetLevel_f(void)
{
    Svcmd_ForceSetLevel_f(power);
}

static void Svcmd_SetForceAll_f(void)
{
    for (forcePowers_t i = forcePowers_t::FP_HEAL; i < forcePowers_t::NUM_FORCE_POWERS; i++) {
        Svcmd_ForceSetLevel_f(i);
    }

    if (gi.argc() > 1) {
        for (saber_styles_t i = saber_styles_t::SS_NONE + 1; i < saber_styles_t::SS_NUM_SABER_STYLES; i++) {
            g_entities[0].client->ps.saberStylesKnown |= Bit(i);
        }
    }
}

static void Svcmd_SetSaberAll_f(void)
{
    Svcmd_ForceSetLevel_f(forcePowers_t::FP_SABERTHROW);
    Svcmd_ForceSetLevel_f(forcePowers_t::FP_SABER_DEFENSE);
    Svcmd_ForceSetLevel_f(forcePowers_t::FP_SABER_OFFENSE);
    for (saber_styles_t i = saber_styles_t::SS_NONE + 1; i < saber_styles_t::SS_NUM_SABER_STYLES; i++) {
        g_entities[0].client->ps.saberStylesKnown |= Bit(i);
    }
}

static void Svcmd_RunScript_f(void)
{
    const char* cmd2 = gi.argv(1);

    if (cmd2 && cmd2[0]) {
        const char* cmd3 = gi.argv(2);
        if (cmd3 && cmd3[0]) {
            gentity_t* found = nullptr;
            if ((found = G_Find(nullptr, FOFS(targetname), cmd2)) != nullptr) {
                Quake3Game()->RunScript(found, cmd3);
            }
            else {
                // can't find cmd2
                Q_Println(S_COLOR_RED "runscript: can't find targetname {}", cmd2);
            }
        }
        else {
            Quake3Game()->RunScript(&g_entities[0], cmd2);
        }
    }
    else {
        Q_Println(S_COLOR_RED "usage: runscript <ent targetname> scriptname");
    }
}

static void Svcmd_PlayerTeam_f(void)
{
    const char* cmd2 = gi.argv(1);

    if (!*cmd2 || !cmd2[0]) {
        Q_Println(S_COLOR_RED "'playerteam' - change player team, requires a team name!");
        Q_Println(S_COLOR_RED "Current team is: {}", GetStringForID(TeamTable, std::to_underlying(g_entities[0].client->playerTeam)));
        Q_Println(S_COLOR_RED "Valid team names are:");
        for (team_t n = team_t::TEAM_FREE + 1; n < team_t::TEAM_NUM_TEAMS; n++) {
            Q_Println(S_COLOR_RED "{}", GetStringForID(TeamTable, std::to_underlying(n)));
        }
    }
    else {
        const int id = GetIDForString(TeamTable, cmd2);

        if (id == -1) {
            Q_Println(S_COLOR_RED "'playerteam' unrecognized team name {}!", cmd2);
            Q_Println(S_COLOR_RED "Current team is: {}", GetStringForID(TeamTable, std::to_underlying(g_entities[0].client->playerTeam)));
            Q_Println(S_COLOR_RED "Valid team names are:");
            for (team_t n = team_t::TEAM_FREE; n < team_t::TEAM_NUM_TEAMS; n++) {
                Q_Println(S_COLOR_RED "{}", GetStringForID(TeamTable, std::to_underlying(n)));
            }
        }
        else {
            g_entities[0].client->playerTeam = static_cast<team_t>(id);
            // FIXME: convert Imperial, Malon, Hirogen and Klingon to Scavenger?
        }
    }
}

static void Svcmd_Control_f(void)
{
    const char* cmd2 = gi.argv(1);
    if (!*cmd2 || !cmd2[0]) {
        if (!G_ClearViewEntity(&g_entities[0])) {
            Q_Println(S_COLOR_RED "control <NPC_targetname>");
        }
    }
    else {
        Q3_SetViewEntity(0, cmd2);
    }
}

static void Svcmd_Grab_f(void)
{
    const char* cmd2 = gi.argv(1);
    if (!*cmd2 || !cmd2[0]) {
        if (!G_ReleaseEntity(&g_entities[0])) {
            Q_Println(S_COLOR_RED "grab <NPC_targetname>");
        }
    }
    else {
        G_GrabEntity(&g_entities[0], cmd2);
    }
}

static void Svcmd_Knockdown_f(void)
{
    G_Knockdown(&g_entities[0], &g_entities[0], vec3_origin, 300, true);
}

static void Svcmd_PlayerModel_f(void)
{
    if (gi.argc() == 1) {
        Q_Println(S_COLOR_RED "USAGE: playerModel <NPC Name>\n       playerModel <g2model> <skinhead> <skintorso> <skinlower>\n       playerModel player "
                              "(builds player from customized menu settings)" S_COLOR_WHITE);
        Q_Print("playerModel = {} ",
                Q_Format("{} {} {} {}\n", g_char_model->string, g_char_skin_head->string, g_char_skin_torso->string, g_char_skin_legs->string));
    }
    else if (gi.argc() == 2) {
        G_ChangePlayerModel(&g_entities[0], gi.argv(1));
    }
    else if (gi.argc() == 5) {
        // instead of setting it directly via a command, we now store it in cvars
        // G_ChangePlayerModel( &g_entities[0], Q_Format("{}|{}|{}|{}", gi.argv(1), gi.argv(2), gi.argv(3), gi.argv(4)).c_str() );
        gi.cvar_set("g_char_model", gi.argv(1));
        gi.cvar_set("g_char_skin_head", gi.argv(2));
        gi.cvar_set("g_char_skin_torso", gi.argv(3));
        gi.cvar_set("g_char_skin_legs", gi.argv(4));
        G_InitPlayerFromCvars(&g_entities[0]);
    }
}

static void Svcmd_PlayerTint_f(void)
{
    if (gi.argc() == 4) {
        g_entities[0].client->renderInfo.customRGBA[0] = atoi(gi.argv(1));
        g_entities[0].client->renderInfo.customRGBA[1] = atoi(gi.argv(2));
        g_entities[0].client->renderInfo.customRGBA[2] = atoi(gi.argv(3));
        gi.cvar_set("g_char_color_red", gi.argv(1));
        gi.cvar_set("g_char_color_green", gi.argv(2));
        gi.cvar_set("g_char_color_blue", gi.argv(3));
    }
    else {
        Q_Println(S_COLOR_RED "USAGE: playerTint <red 0 - 255> <green 0 - 255> <blue 0 - 255>");
        Q_Println("playerTint = {}", Q_Format("{} {} {}", g_char_color_red->integer, g_char_color_green->integer, g_char_color_blue->integer));
    }
}

static void Svcmd_IKnowKungfu_f(void)
{
    gi.cvar_set("g_debugMelee", "1");
    G_SetWeapon(&g_entities[0], weapon_t::WP_MELEE);
    for (forcePowers_t i = forcePowers_t::FP_FIRST; i < forcePowers_t::NUM_FORCE_POWERS; i++) {
        g_entities[0].client->ps.forcePowersKnown |= Bit(i);
        if (i == forcePowers_t::FP_TELEPATHY) {
            g_entities[0].client->ps.forcePowerLevel[i] = forcePowerLevels_t::FORCE_LEVEL_4;
        }
        else {
            g_entities[0].client->ps.forcePowerLevel[i] = forcePowerLevels_t::FORCE_LEVEL_3;
        }
    }
}

static void Svcmd_Secrets_f(void)
{
    const gentity_t* pl = &g_entities[0];
    if (pl->client->sess.missionStats.totalSecrets < 1) {
        Q_Println("There are" S_COLOR_RED " NO " S_COLOR_WHITE "secrets on this map!");
    }
    else if (pl->client->sess.missionStats.secretsFound == pl->client->sess.missionStats.totalSecrets) {
        Q_Println("You've found all " S_COLOR_GREEN "{}" S_COLOR_WHITE " secrets on this map!", pl->client->sess.missionStats.secretsFound);
    }
    else {
        Q_Println("You've found " S_COLOR_GREEN "{}" S_COLOR_WHITE " out of " S_COLOR_GREEN "{}" S_COLOR_WHITE " secrets!",
                  pl->client->sess.missionStats.secretsFound, pl->client->sess.missionStats.totalSecrets);
    }
}

// PADAWAN - g_spskill 0 + cg_crosshairForceHint 1 + handicap 100
// JEDI - g_spskill 1 + cg_crosshairForceHint 1 + handicap 100
// JEDI KNIGHT - g_spskill 2 + cg_crosshairForceHint 0 + handicap 100
// JEDI MASTER - g_spskill 2 + cg_crosshairForceHint 0 + handicap 50

extern cvar_t* g_spskill;
static void Svcmd_Difficulty_f(void)
{
    if (gi.argc() == 1) {
        if (g_spskill->integer == 0) {
            Q_Println(S_COLOR_GREEN "Current Difficulty: Padawan" S_COLOR_WHITE);
        }
        else if (g_spskill->integer == 1) {
            Q_Println(S_COLOR_GREEN "Current Difficulty: Jedi" S_COLOR_WHITE);
        }
        else if (g_spskill->integer == 2) {
            int crosshairHint = gi.Cvar_VariableIntegerValue("cg_crosshairForceHint");
            int handicap = gi.Cvar_VariableIntegerValue("handicap");
            if (handicap == 100 && crosshairHint == 0) {
                Q_Println(S_COLOR_GREEN "Current Difficulty: Jedi Knight" S_COLOR_WHITE);
            }
            else if (handicap == 50 && crosshairHint == 0) {
                Q_Println(S_COLOR_GREEN "Current Difficulty: Jedi Master" S_COLOR_WHITE);
            }
            else {
                Q_Println(S_COLOR_GREEN "Current Difficulty: Jedi Knight (Custom)" S_COLOR_WHITE);
                Q_Println(S_COLOR_GREEN "Crosshair Force Hint: {}" S_COLOR_WHITE, crosshairHint != 0 ? 1 : 0);
                Q_Println(S_COLOR_GREEN "Handicap: {}" S_COLOR_WHITE, handicap);
            }
        }
        else {
            Q_Println(S_COLOR_RED "Invalid difficulty cvar set! g_spskill ({}) [0-2] is valid range only" S_COLOR_WHITE, g_spskill->integer);
        }
    }
}

enum class commandFlags_t {
    CMD_NONE = 0x00000000u,
    CMD_CHEAT = 0x00000001u,
    CMD_ALIVE = 0x00000002u,
};
template <> inline constexpr bool enableEnumFlags<commandFlags_t> = true;

typedef struct svcmd_s {
    const char* name;
    void (*func)(void);
    commandFlags_t flags;
} svcmd_t;

static int svcmdcmp(const void* a, const void* b)
{
    return Q_stricmp((const char*)a, ((svcmd_t*)b)->name);
}

// FIXME some of these should be made CMD_ALIVE too!
static svcmd_t svcmds[] = {
    {"entitylist", Svcmd_EntityList_f, commandFlags_t::CMD_NONE},
    {"game_memory", Svcmd_GameMem_f, commandFlags_t::CMD_NONE},

    {"nav", Svcmd_Nav_f, commandFlags_t::CMD_CHEAT},
    {"npc", Svcmd_NPC_f, commandFlags_t::CMD_CHEAT},
    {"use", Svcmd_Use_f, commandFlags_t::CMD_CHEAT},
    {"ICARUS", Svcmd_ICARUS_f, commandFlags_t::CMD_CHEAT},

    {"saberColor", Svcmd_SaberColor_f, commandFlags_t::CMD_CHEAT},
    {"saber", Svcmd_Saber_f, commandFlags_t::CMD_CHEAT},
    {"saberBlade", Svcmd_SaberBlade_f, commandFlags_t::CMD_CHEAT},

    {"setForceJump", Svcmd_ForceSetLevel_f<forcePowers_t::FP_LEVITATION>, commandFlags_t::CMD_CHEAT},
    {"setSaberThrow", Svcmd_ForceSetLevel_f<forcePowers_t::FP_SABERTHROW>, commandFlags_t::CMD_CHEAT},
    {"setForceHeal", Svcmd_ForceSetLevel_f<forcePowers_t::FP_HEAL>, commandFlags_t::CMD_CHEAT},
    {"setForcePush", Svcmd_ForceSetLevel_f<forcePowers_t::FP_PUSH>, commandFlags_t::CMD_CHEAT},
    {"setForcePull", Svcmd_ForceSetLevel_f<forcePowers_t::FP_PULL>, commandFlags_t::CMD_CHEAT},
    {"setForceSpeed", Svcmd_ForceSetLevel_f<forcePowers_t::FP_SPEED>, commandFlags_t::CMD_CHEAT},
    {"setForceGrip", Svcmd_ForceSetLevel_f<forcePowers_t::FP_GRIP>, commandFlags_t::CMD_CHEAT},
    {"setForceLightning", Svcmd_ForceSetLevel_f<forcePowers_t::FP_LIGHTNING>, commandFlags_t::CMD_CHEAT},
    {"setMindTrick", Svcmd_ForceSetLevel_f<forcePowers_t::FP_TELEPATHY>, commandFlags_t::CMD_CHEAT},
    {"setSaberDefense", Svcmd_ForceSetLevel_f<forcePowers_t::FP_SABER_DEFENSE>, commandFlags_t::CMD_CHEAT},
    {"setSaberOffense", Svcmd_ForceSetLevel_f<forcePowers_t::FP_SABER_OFFENSE>, commandFlags_t::CMD_CHEAT},
    {"setForceRage", Svcmd_ForceSetLevel_f<forcePowers_t::FP_RAGE>, commandFlags_t::CMD_CHEAT},
    {"setForceDrain", Svcmd_ForceSetLevel_f<forcePowers_t::FP_DRAIN>, commandFlags_t::CMD_CHEAT},
    {"setForceProtect", Svcmd_ForceSetLevel_f<forcePowers_t::FP_PROTECT>, commandFlags_t::CMD_CHEAT},
    {"setForceAbsorb", Svcmd_ForceSetLevel_f<forcePowers_t::FP_ABSORB>, commandFlags_t::CMD_CHEAT},
    {"setForceSight", Svcmd_ForceSetLevel_f<forcePowers_t::FP_SEE>, commandFlags_t::CMD_CHEAT},
    {"setForceAll", Svcmd_SetForceAll_f, commandFlags_t::CMD_CHEAT},
    {"setSaberAll", Svcmd_SetSaberAll_f, commandFlags_t::CMD_CHEAT},

    {"saberAttackCycle", Svcmd_SaberAttackCycle_f, commandFlags_t::CMD_NONE},

    {"runscript", Svcmd_RunScript_f, commandFlags_t::CMD_CHEAT},

    {"playerTeam", Svcmd_PlayerTeam_f, commandFlags_t::CMD_CHEAT},

    {"control", Svcmd_Control_f, commandFlags_t::CMD_CHEAT},
    {"grab", Svcmd_Grab_f, commandFlags_t::CMD_CHEAT},
    {"knockdown", Svcmd_Knockdown_f, commandFlags_t::CMD_CHEAT},

    {"playerModel", Svcmd_PlayerModel_f, commandFlags_t::CMD_NONE},
    {"playerTint", Svcmd_PlayerTint_f, commandFlags_t::CMD_NONE},

    {"nexttestaxes", G_NextTestAxes, commandFlags_t::CMD_NONE},

    {"exitview", Svcmd_ExitView_f, commandFlags_t::CMD_NONE},

    {"iknowkungfu", Svcmd_IKnowKungfu_f, commandFlags_t::CMD_CHEAT},

    {"secrets", Svcmd_Secrets_f, commandFlags_t::CMD_NONE},
    {"difficulty", Svcmd_Difficulty_f, commandFlags_t::CMD_NONE},

    //{ "say",						Svcmd_Say_f,						true },
    //{ "toggleallowvote",			Svcmd_ToggleAllowVote_f,			false },
    //{ "toggleuserinfovalidation",	Svcmd_ToggleUserinfoValidation_f,	false },
};
static const size_t numsvcmds = ARRAY_LEN(svcmds);

/*
=================
ConsoleCommand
=================
*/
bool ConsoleCommand(void)
{
    const char* cmd = gi.argv(0);
    const svcmd_t* command = (const svcmd_t*)Q_LinearSearch(cmd, svcmds, numsvcmds, sizeof(svcmds[0]), svcmdcmp);

    if (!command)
        return false;

    if (Any(command->flags & commandFlags_t::CMD_CHEAT) && !g_cheats->integer) {
        Q_Println("Cheats are not enabled on this server.");
        return true;
    }
    else if (Any(command->flags & commandFlags_t::CMD_ALIVE) && (g_entities[0].health <= 0)) {
        Q_Println("You must be alive to use this command.");
        return true;
    }
    else
        command->func();
    return true;
}

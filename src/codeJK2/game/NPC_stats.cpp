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

// NPC_stats.cpp

#include "g_headers.h"

#include "b_local.h"
#include "b_public.h"
#include "anims.h"

extern bool NPCsPrecached;
extern vec3_t playerMins;
extern vec3_t playerMaxs;

EnumArray<const char*, team_t, team_t::TEAM_NUM_TEAMS> TeamNames = {"", "player", "enemy", "neutral"};

// this list was made using the model directories, this MUST be in the same order as the CLASS_ enum in teams.h
EnumArray<const char*, class_t, class_t::CLASS_NUM_CLASSES> ClassNames = {
    "", // class none
    "atst",     "bartender", "bespin_cop",    "claw",         "commando",   "desann",       "fish",     "flier2",       "galak",    "glider",
    "gonk",     "gran",      "howler",        "imperial",     "impworker",  "interrogator", "jan",      "jedi",         "kyle",     "lando",
    "lizard",   "luke",      "mark1",         "mark2",        "galak_mech", "minemonster",  "monmotha", "morgankatarn", "mouse",    "murjj",
    "prisoner", "probe",     "protocol",      "r2d2",         "r5d2",       "rebel",        "reborn",   "reelo",        "remote",   "rodian",
    "seeker",   "sentry",    "shadowtrooper", "stormtrooper", "swamp",      "swamptrooper", "tavion",   "trandoshan",   "ugnaught", "weequay",
};

/*
NPC_ReactionTime
*/
// FIXME use grandom in here
int NPC_ReactionTime(void)
{
    return 200 * (6 - NPCInfo->stats.reactions);
}

//
// parse support routines
//

bool G_ParseLiteral(const char** data, const char* string)
{
    const char* token;

    token = COM_ParseExt(data, true);
    if (token[0] == 0) {
        Q_Println("unexpected EOF");
        return true;
    }

    if (Q_stricmp(token, string)) {
        Q_Println("required string '{}' missing", string);
        return true;
    }

    return false;
}

//
// NPC parameters file : scripts/NPCs.cfg
//
constexpr int MAX_NPC_DATA_SIZE = 0x40000;
char NPCParms[MAX_NPC_DATA_SIZE];

team_t TranslateTeamName(const char* name)
{
    for (team_t n = team_t::TEAM_FREE + 1; n < team_t::TEAM_NUM_TEAMS; n++) {
        if (Q_stricmp(TeamNames[n], name) == 0) {
            return n;
        }
    }

    return team_t::TEAM_FREE;
}

class_t TranslateClassName(const char* name)
{
    for (class_t n = (class_t::CLASS_NONE + 1); n < class_t::CLASS_NUM_CLASSES; n++) {
        if (Q_stricmp(ClassNames[n], name) == 0) {
            return n;
        }
    }

    return class_t::CLASS_NONE; // I hope this never happens, maybe print a warning
}

/*
static rank_t TranslateRankName( const char *name )

  Should be used to determine pip bolt-ons
*/
static rank_t TranslateRankName(const char* name)
{
    if (!Q_stricmp(name, "civilian")) {
        return rank_t::RANK_CIVILIAN;
    }

    if (!Q_stricmp(name, "crewman")) {
        return rank_t::RANK_CREWMAN;
    }

    if (!Q_stricmp(name, "ensign")) {
        return rank_t::RANK_ENSIGN;
    }

    if (!Q_stricmp(name, "ltjg")) {
        return rank_t::RANK_LT_JG;
    }

    if (!Q_stricmp(name, "lt")) {
        return rank_t::RANK_LT;
    }

    if (!Q_stricmp(name, "ltcomm")) {
        return rank_t::RANK_LT_COMM;
    }

    if (!Q_stricmp(name, "commander")) {
        return rank_t::RANK_COMMANDER;
    }

    if (!Q_stricmp(name, "captain")) {
        return rank_t::RANK_CAPTAIN;
    }

    return rank_t::RANK_CIVILIAN;
}
static saber_colors_t TranslateSaberColor(const char* name)
{
    if (!Q_stricmp(name, "red")) {
        return saber_colors_t::SABER_RED;
    }
    if (!Q_stricmp(name, "orange")) {
        return saber_colors_t::SABER_ORANGE;
    }
    if (!Q_stricmp(name, "yellow")) {
        return saber_colors_t::SABER_YELLOW;
    }
    if (!Q_stricmp(name, "green")) {
        return saber_colors_t::SABER_GREEN;
    }
    if (!Q_stricmp(name, "blue")) {
        return saber_colors_t::SABER_BLUE;
    }
    if (!Q_stricmp(name, "purple")) {
        return saber_colors_t::SABER_PURPLE;
    }
    if (!Q_stricmp(name, "random")) {
        return Q_irand(saber_colors_t::SABER_ORANGE, saber_colors_t::SABER_PURPLE);
    }
    return saber_colors_t::SABER_BLUE;
}

/* static int MethodNameToNumber( const char *name ) {
    if ( !Q_stricmp( name, "EXPONENTIAL" ) ) {
        return METHOD_EXPONENTIAL;
    }
    if ( !Q_stricmp( name, "LINEAR" ) ) {
        return METHOD_LINEAR;
    }
    if ( !Q_stricmp( name, "LOGRITHMIC" ) ) {
        return METHOD_LOGRITHMIC;
    }
    if ( !Q_stricmp( name, "ALWAYS" ) ) {
        return METHOD_ALWAYS;
    }
    if ( !Q_stricmp( name, "NEVER" ) ) {
        return METHOD_NEVER;
    }
    return -1;
}

static int ItemNameToNumber( const char *name, int itemType ) {
//	int		n;

    for ( n = 0; n < bg_numItems; n++ ) {
        if ( bg_itemlist[n].type != itemType ) {
            continue;
        }
        if ( Q_stricmp( bg_itemlist[n].classname, name ) == 0 ) {
            return bg_itemlist[n].tag;
        }
    }
    return -1;
}
*/

static movetype_t MoveTypeNameToEnum(const char* name)
{
    if (!Q_stricmp("runjump", name)) {
        return movetype_t::MT_RUNJUMP;
    }
    else if (!Q_stricmp("walk", name)) {
        return movetype_t::MT_WALK;
    }
    else if (!Q_stricmp("flyswim", name)) {
        return movetype_t::MT_FLYSWIM;
    }
    else if (!Q_stricmp("static", name)) {
        return movetype_t::MT_STATIC;
    }

    return movetype_t::MT_STATIC;
}

extern void CG_RegisterClientRenderInfo(clientInfo_t* ci, renderInfo_t* ri);
extern void CG_RegisterClientModels(int entityNum);
extern void CG_RegisterNPCCustomSounds(clientInfo_t* ci);
extern void CG_RegisterNPCEffects(team_t team);
extern void CG_ParseAnimationSndFile(const char* filename, int animFileIndex);

// #define CONVENIENT_ANIMATION_FILE_DEBUG_THING

#ifdef CONVENIENT_ANIMATION_FILE_DEBUG_THING
void SpewDebugStuffToFile(const EnumArray<animation_t, animNumber_t, animNumber_t::MAX_ANIMATIONS>& bgGlobalAnimations)
{
    char BGPAFtext[40000];
    fileHandle_t f;
    animNumber_t i = animNumber_t::BOTH_1CRUFTFORGIL;

    gi.FS_FOpenFile("file_of_debug_stuff_SP.txt", &f, fsMode_t::FS_WRITE);

    if (!f) {
        return;
    }

    BGPAFtext[0] = 0;

    while (i < animNumber_t::MAX_ANIMATIONS) {
        strcat(BGPAFtext, Q_Format("{} {}\n", to_underlying(i), bgGlobalAnimations[i].frameLerp).c_str());
        i++;
    }

    gi.FS_Write(BGPAFtext, strlen(BGPAFtext), f);
    gi.FS_FCloseFile(f);
}
#endif

/*
======================
CG_ParseAnimationFile

Read a configuration file containing animation coutns and rates
models/players/visor/animation.cfg, etc

======================
*/
bool G_ParseAnimationFile(const char* af_filename)
{
    const char* text_p;
    int len;
    animNumber_t i;
    const char* token;
    float fps;
    // int			skip;
    char text[40000];
    std::optional<animNumber_t> animNum;
    auto& animations = level.knownAnimFileSets[level.numKnownAnimFileSets].animations;

    len = gi.RE_GetAnimationCFG(af_filename, nullptr, 0);
    if (len <= 0) {
        return false;
    }
    if (len <= 0) {
        return false;
    }
    if (len >= (int)sizeof(text) - 1) {
        Q_Error(errorParm_t::ERR_DROP, "G_ParseAnimationFile: File {} too long\n ({} > {})", af_filename, len, sizeof(text) - 1);
        return false;
    }
    len = gi.RE_GetAnimationCFG(af_filename, text, sizeof(text));

    // parse the text
    text_p = text;
    // skip = 0;	// quiet the compiler warning

    // FIXME: have some way of playing anims backwards... negative numFrames?

    // initialize anim array so that from 0 to MAX_ANIMATIONS, set default values of 0 1 0 100
    for (i = animNumber_t::BOTH_1CRUFTFORGIL; i < animNumber_t::MAX_ANIMATIONS; i++) {
        animations[i].firstFrame = 0;
        animations[i].numFrames = 0;
        animations[i].loopFrames = -1;
        animations[i].frameLerp = 100;
        animations[i].initialLerp = 100;
    }

    // read information for each frame
    COM_BeginParseSession();
    while (1) {
        token = COM_Parse(&text_p);

        if (!token || !token[0]) {
            break;
        }

        animNum = BG_AnimFromName(token);
        if (!animNum) {
// #ifndef FINAL_BUILD
#ifdef _DEBUG
            Q_PrintlnWarning("Unknown token {} in {}", token, af_filename);
#endif
            continue;
        }

        token = COM_Parse(&text_p);
        if (!token) {
            break;
        }
        animations[*animNum].firstFrame = atoi(token);

        token = COM_Parse(&text_p);
        if (!token) {
            break;
        }
        animations[*animNum].numFrames = atoi(token);

        token = COM_Parse(&text_p);
        if (!token) {
            break;
        }
        animations[*animNum].loopFrames = atoi(token);

        token = COM_Parse(&text_p);
        if (!token) {
            break;
        }
        fps = Q_atof(token);
        if (fps == 0) {
            fps = 1; // Don't allow divide by zero error
        }
        if (fps < 0) { // backwards
            animations[*animNum].frameLerp = Q_floorf(1000.0f / fps);
        }
        else {
            animations[*animNum].frameLerp = Q_ceilf(1000.0f / fps);
        }

        animations[*animNum].initialLerp = Q_ceilf(1000.0f / Q_fabsf(fps));
    }
    COM_EndParseSession();

#ifdef CONVENIENT_ANIMATION_FILE_DEBUG_THING
    if (strstr(af_filename, "humanoid")) {
        SpewDebugStuffToFile(animations);
    }
#endif

    return true;
}

bool G_ParseAnimFileSet(const char* filename, const char* animCFG, int* animFileIndex)
{
    char afilename[MAX_QPATH];
    char strippedName[MAX_QPATH];
    int i;
    char* slash;

    Q_strncpyz(strippedName, filename, sizeof(strippedName));
    slash = strchr(strippedName, '/');
    if (slash) {
        // truncate modelName to find just the dir not the extension
        *slash = 0;
    }

    // if this anims file was loaded before, don't parse it again, just point to the correct table of info
    for (i = 0; i < level.numKnownAnimFileSets; i++) {
        if (Q_stricmp(level.knownAnimFileSets[i].filename, strippedName) == 0) {
            *animFileIndex = i;
            CG_ParseAnimationSndFile(strippedName, *animFileIndex);
            return true;
        }
    }

    if (level.numKnownAnimFileSets == MAX_ANIM_FILES) { // TOO MANY!
        Q_Error(errorParm_t::ERR_DROP, "G_ParseAnimFileSet: MAX_ANIM_FILES");
    }

    // Okay, time to parse in a new one
    Q_strncpyz(level.knownAnimFileSets[level.numKnownAnimFileSets].filename, strippedName,
               sizeof(level.knownAnimFileSets[level.numKnownAnimFileSets].filename));

    // Load and parse animations.cfg file
    Q_FormatTo(afilename, "models/players/{}/animation.cfg", animCFG);
    if (!G_ParseAnimationFile(afilename)) {
        *animFileIndex = -1;
        return false;
    }

    // set index and increment
    *animFileIndex = level.numKnownAnimFileSets++;

    CG_ParseAnimationSndFile(strippedName, *animFileIndex);

    return true;
}

void G_LoadAnimFileSet(gentity_t* ent, const char* modelName)
{
    // load its animation config
    char animName[MAX_QPATH];
    char* GLAName;
    char* slash = nullptr;
    const char* strippedName;

    if (ent->playerModel == -1) {
        return;
    }
    // get the location of the animation.cfg
    GLAName = gi.G2API_GetGLAName(&ent->ghoul2[ent->playerModel]);
    // now load and parse the animation.cfg, animsounds.cfg and set the animFileIndex
    if (!GLAName) {
        Q_Println(S_COLOR_RED "Failed find animation file name models/players/{}/animation.cfg", modelName);
        strippedName = "broken";
    }
    else {
        Q_strncpyz(animName, GLAName, sizeof(animName));
        slash = strrchr(animName, '/');
        if (slash) {
            *slash = 0;
        }
        strippedName = COM_SkipPath(animName);
    }

    // now load and parse the animation.cfg, animsounds.cfg and set the animFileIndex
    if (!G_ParseAnimFileSet(modelName, strippedName, &ent->client->clientInfo.animFileIndex)) {
        Q_Println(S_COLOR_RED "Failed to load animation file set models/players/{}/animation.cfg", modelName);
    }
}

void NPC_PrecacheAnimationCFG(const char* NPC_type)
{
    char filename[MAX_QPATH];
    const char* token;
    const char* value;
    const char* p;
    int junk;

    if (!Q_stricmp("random", NPC_type)) { // sorry, can't precache a random just yet
        return;
    }

    p = NPCParms;
    COM_BeginParseSession();

    // look for the right NPC
    while (p) {
        token = COM_ParseExt(&p, true);
        if (token[0] == 0) {
            COM_EndParseSession();
            return;
        }

        if (!Q_stricmp(token, NPC_type)) {
            break;
        }

        SkipBracedSection(&p);
    }

    if (!p) {
        COM_EndParseSession();
        return;
    }

    if (G_ParseLiteral(&p, "{")) {
        COM_EndParseSession();
        return;
    }

    // parse the NPC info block
    while (1) {
        token = COM_ParseExt(&p, true);
        if (!token[0]) {
            Q_PrintlnError("unexpected EOF while parsing '{}'", NPC_type);
            COM_EndParseSession();
            return;
        }

        if (!Q_stricmp(token, "}")) {
            break;
        }

        // legsmodel
        if (!Q_stricmp(token, "legsmodel")) {
            if (COM_ParseString(&p, &value)) {
                continue;
            }
            // must copy data out of this pointer into a different part of memory because the funcs we're about to call will call COM_ParseExt
            Q_strncpyz(filename, value, sizeof(filename));
            G_ParseAnimFileSet(filename, filename, &junk);
            COM_EndParseSession();
            return;
        }

        // playerModel
        if (!Q_stricmp(token, "playerModel")) {
            if (COM_ParseString(&p, &value)) {
                continue;
            }
            char animName[MAX_QPATH];
            char* GLAName;
            char* slash = nullptr;
            char* strippedName;

            int handle = gi.G2API_PrecacheGhoul2Model(Q_Format("models/players/{}/model.glm", value).c_str());
            if (handle > 0) // FIXME: isn't 0 a valid handle?
            {
                GLAName = gi.G2API_GetAnimFileNameIndex(handle);
                if (GLAName) {
                    Q_strncpyz(animName, GLAName, sizeof(animName));
                    slash = strrchr(animName, '/');
                    if (slash) {
                        *slash = 0;
                    }
                    strippedName = COM_SkipPath(animName);

                    // must copy data out of this pointer into a different part of memory because the funcs we're about to call will call COM_ParseExt
                    Q_strncpyz(filename, value, sizeof(filename));
                    G_ParseAnimFileSet(value, strippedName, &junk); // false );
                    COM_EndParseSession();
                    // FIXME: still not precaching the animsounds.cfg?
                    return;
                }
            }
        }
    }
    COM_EndParseSession();
}

extern int NPC_WeaponsForTeam(team_t team, int spawnflags, const char* NPC_type);
void NPC_PrecacheWeapons(team_t playerTeam, int spawnflags, const char* NPCtype)
{
    int weapons = NPC_WeaponsForTeam(playerTeam, spawnflags, NPCtype);
    gitem_t* item;
    for (weapon_t curWeap = weapon_t::WP_SABER; curWeap < weapon_t::WP_NUM_WEAPONS; curWeap++) {
        if ((weapons & Bit(curWeap))) {
            item = FindItemForWeapon(curWeap); // precache the weapon
            CG_RegisterItemSounds(ITEM_INDEX(item));
            CG_RegisterItemVisuals(ITEM_INDEX(item));
            // precache the in-hand/in-world ghoul2 weapon model

            char weaponModel[MAX_QPATH];
            Q_strncpyz(weaponModel, weaponData[curWeap].weaponMdl, sizeof(weaponModel));
            if (char* spot = strstr(weaponModel, ".md3")) {
                *spot = 0;
                spot = strstr(weaponModel, "_w"); // i'm using the in view weapon array instead of scanning the item list, so put the _w back on
                if (!spot) {
                    strcat(weaponModel, "_w");
                }
                strcat(weaponModel, ".glm"); // and change to ghoul2
            }
            gi.G2API_PrecacheGhoul2Model(weaponModel); // correct way is item->world_model
        }
    }
}

/*
void NPC_Precache ( char *NPCName )

Precaches NPC skins, tgas and md3s.

*/
void NPC_Precache(gentity_t* spawner)
{
    clientInfo_t ci = {};
    renderInfo_t ri = {};
    team_t playerTeam = team_t::TEAM_FREE;
    const char* token;
    const char* value;
    const char* p;
    char* patch;
    char sound[MAX_QPATH];
    bool md3Model = false;
    char playerModel[MAX_QPATH];
    char customSkin[MAX_QPATH];

    if (!Q_stricmp("random", spawner->NPC_type)) { // sorry, can't precache a random just yet
        return;
    }
    Q_strncpyz(customSkin, "default", sizeof(customSkin));

    p = NPCParms;
    COM_BeginParseSession();

    // look for the right NPC
    while (p) {
        token = COM_ParseExt(&p, true);
        if (token[0] == 0) {
            COM_EndParseSession();
            return;
        }

        if (!Q_stricmp(token, spawner->NPC_type)) {
            break;
        }

        SkipBracedSection(&p);
    }

    if (!p) {
        COM_EndParseSession();
        return;
    }

    if (G_ParseLiteral(&p, "{")) {
        COM_EndParseSession();
        return;
    }

    // parse the NPC info block
    while (1) {
        COM_EndParseSession(); // if still in session (or using continue;)
        COM_BeginParseSession();
        token = COM_ParseExt(&p, true);
        if (!token[0]) {
            Q_PrintlnError("unexpected EOF while parsing '{}'", spawner->NPC_type);
            COM_EndParseSession();
            return;
        }

        if (!Q_stricmp(token, "}")) {
            break;
        }

        // headmodel
        if (!Q_stricmp(token, "headmodel")) {
            if (COM_ParseString(&p, &value)) {
                continue;
            }

            if (!Q_stricmp("none", value)) {
            }
            else {
                Q_strncpyz(ri.headModelName, value, sizeof(ri.headModelName));
            }
            md3Model = true;
            continue;
        }

        // torsomodel
        if (!Q_stricmp(token, "torsomodel")) {
            if (COM_ParseString(&p, &value)) {
                continue;
            }

            if (!Q_stricmp("none", value)) {
            }
            else {
                Q_strncpyz(ri.torsoModelName, value, sizeof(ri.torsoModelName));
            }
            md3Model = true;
            continue;
        }

        // legsmodel
        if (!Q_stricmp(token, "legsmodel")) {
            if (COM_ParseString(&p, &value)) {
                continue;
            }
            Q_strncpyz(ri.legsModelName, value, sizeof(ri.legsModelName));
            md3Model = true;
            continue;
        }

        // playerModel
        if (!Q_stricmp(token, "playerModel")) {
            if (COM_ParseString(&p, &value)) {
                continue;
            }
            Q_strncpyz(playerModel, value, sizeof(playerModel));
            md3Model = false;
            continue;
        }

        // customSkin
        if (!Q_stricmp(token, "customSkin")) {
            if (COM_ParseString(&p, &value)) {
                continue;
            }
            Q_strncpyz(customSkin, value, sizeof(customSkin));
            continue;
        }

        // playerTeam
        if (!Q_stricmp(token, "playerTeam")) {
            if (COM_ParseString(&p, &value)) {
                continue;
            }
            playerTeam = TranslateTeamName(value);
            continue;
        }

        // snd
        if (!Q_stricmp(token, "snd")) {
            if (COM_ParseString(&p, &value)) {
                continue;
            }
            if (!Any(spawner->svFlags & svFlags_t::SVF_NO_BASIC_SOUNDS)) {
                // FIXME: store this in some sound field or parse in the soundTable like the animTable...
                Q_strncpyz(sound, value, sizeof(sound));
                patch = strstr(sound, "/");
                if (patch) {
                    *patch = 0;
                }
                ci.customBasicSoundDir = G_NewString(sound);
            }
            continue;
        }

        // sndcombat
        if (!Q_stricmp(token, "sndcombat")) {
            if (COM_ParseString(&p, &value)) {
                continue;
            }
            if (!Any(spawner->svFlags & svFlags_t::SVF_NO_COMBAT_SOUNDS)) {
                // FIXME: store this in some sound field or parse in the soundTable like the animTable...
                Q_strncpyz(sound, value, sizeof(sound));
                patch = strstr(sound, "/");
                if (patch) {
                    *patch = 0;
                }
                ci.customCombatSoundDir = G_NewString(sound);
            }
            continue;
        }

        // sndextra
        if (!Q_stricmp(token, "sndextra")) {
            if (COM_ParseString(&p, &value)) {
                continue;
            }
            if (!Any(spawner->svFlags & svFlags_t::SVF_NO_EXTRA_SOUNDS)) {
                // FIXME: store this in some sound field or parse in the soundTable like the animTable...
                Q_strncpyz(sound, value, sizeof(sound));
                patch = strstr(sound, "/");
                if (patch) {
                    *patch = 0;
                }
                ci.customExtraSoundDir = G_NewString(sound);
            }
            continue;
        }

        // sndjedi
        if (!Q_stricmp(token, "sndjedi")) {
            if (COM_ParseString(&p, &value)) {
                continue;
            }
            if (!Any(spawner->svFlags & svFlags_t::SVF_NO_EXTRA_SOUNDS)) {
                // FIXME: store this in some sound field or parse in the soundTable like the animTable...
                Q_strncpyz(sound, value, sizeof(sound));
                patch = strstr(sound, "/");
                if (patch) {
                    *patch = 0;
                }
                ci.customJediSoundDir = G_NewString(sound);
            }
            continue;
        }
    }

    COM_EndParseSession();

    if (md3Model) {
        CG_RegisterClientRenderInfo(&ci, &ri);
    }
    else {
        char skinName[MAX_QPATH];
        // precache ghoul2 model
        gi.G2API_PrecacheGhoul2Model(Q_Format("models/players/{}/model.glm", playerModel).c_str());
        // precache skin
        Q_FormatTo(skinName, "models/players/{}/model_{}.skin", playerModel, customSkin);
        // lets see if it's out there
        gi.RE_RegisterSkin(skinName);
    }

    // precache this NPC's possible weapons
    NPC_PrecacheWeapons(playerTeam, spawner->spawnflags, spawner->NPC_type);

    CG_RegisterNPCCustomSounds(&ci);
    CG_RegisterNPCEffects(playerTeam);

    // FIXME: Look for a "sounds" directory and precache death, pain, alert sounds
}

void NPC_BuildRandom(gentity_t* NPC)
{
    int sex, color, head;

    sex = Q_irand(0, 2);
    color = Q_irand(0, 2);
    switch (sex) {
    case 0: // female
        head = Q_irand(0, 2);
        switch (head) {
        default:
        case 0:
            Q_strncpyz(NPC->client->renderInfo.headModelName, "garren", sizeof(NPC->client->renderInfo.headModelName));
            break;
        case 1:
            Q_strncpyz(NPC->client->renderInfo.headModelName, "garren/salma", sizeof(NPC->client->renderInfo.headModelName));
            break;
        case 2:
            Q_strncpyz(NPC->client->renderInfo.headModelName, "garren/mackey", sizeof(NPC->client->renderInfo.headModelName));
            color = Q_irand(3, 5); // torso needs to be afam
            break;
        }
        switch (color) {
        default:
        case 0:
            Q_strncpyz(NPC->client->renderInfo.torsoModelName, "crewfemale/gold", sizeof(NPC->client->renderInfo.torsoModelName));
            break;
        case 1:
            Q_strncpyz(NPC->client->renderInfo.torsoModelName, "crewfemale", sizeof(NPC->client->renderInfo.torsoModelName));
            break;
        case 2:
            Q_strncpyz(NPC->client->renderInfo.torsoModelName, "crewfemale/blue", sizeof(NPC->client->renderInfo.torsoModelName));
            break;
        case 3:
            Q_strncpyz(NPC->client->renderInfo.torsoModelName, "crewfemale/aframG", sizeof(NPC->client->renderInfo.torsoModelName));
            break;
        case 4:
            Q_strncpyz(NPC->client->renderInfo.torsoModelName, "crewfemale/aframR", sizeof(NPC->client->renderInfo.torsoModelName));
            break;
        case 5:
            Q_strncpyz(NPC->client->renderInfo.torsoModelName, "crewfemale/aframB", sizeof(NPC->client->renderInfo.torsoModelName));
            break;
        }
        Q_strncpyz(NPC->client->renderInfo.legsModelName, "crewfemale", sizeof(NPC->client->renderInfo.legsModelName));
        break;
    default:
    case 1: // male
    case 2: // male
        head = Q_irand(0, 4);
        switch (head) {
        default:
        case 0:
            Q_strncpyz(NPC->client->renderInfo.headModelName, "chakotay/nelson", sizeof(NPC->client->renderInfo.headModelName));
            break;
        case 1:
            Q_strncpyz(NPC->client->renderInfo.headModelName, "paris/chase", sizeof(NPC->client->renderInfo.headModelName));
            break;
        case 2:
            Q_strncpyz(NPC->client->renderInfo.headModelName, "doctor/pasty", sizeof(NPC->client->renderInfo.headModelName));
            break;
        case 3:
            Q_strncpyz(NPC->client->renderInfo.headModelName, "kim/durk", sizeof(NPC->client->renderInfo.headModelName));
            break;
        case 4:
            Q_strncpyz(NPC->client->renderInfo.headModelName, "paris/kray", sizeof(NPC->client->renderInfo.headModelName));
            break;
        }
        switch (color) {
        default:
        case 0:
            Q_strncpyz(NPC->client->renderInfo.torsoModelName, "crewthin/red", sizeof(NPC->client->renderInfo.torsoModelName));
            break;
        case 1:
            Q_strncpyz(NPC->client->renderInfo.torsoModelName, "crewthin", sizeof(NPC->client->renderInfo.torsoModelName));
            break;
        case 2:
            Q_strncpyz(NPC->client->renderInfo.torsoModelName, "crewthin/blue", sizeof(NPC->client->renderInfo.torsoModelName));
            break;
            // NOTE: 3 - 5 should be red, gold & blue, afram hands
        }
        Q_strncpyz(NPC->client->renderInfo.legsModelName, "crewthin", sizeof(NPC->client->renderInfo.legsModelName));
        break;
    }

    NPC->s.modelScale[0] = NPC->s.modelScale[1] = NPC->s.modelScale[2] = Q_irand(87, 102) / 100.0f;
    NPC->NPC->rank = rank_t::RANK_CREWMAN;
    NPC->client->playerTeam = team_t::TEAM_PLAYER;
    NPC->client->clientInfo.customBasicSoundDir = "kyle";
}

extern void G_SetG2PlayerModel(gentity_t* const ent, const char* modelName, const char* customSkin, const char* surfOff, const char* surfOn);
bool NPC_ParseParms(const char* NPCName, gentity_t* NPC)
{
    const char* token;
    const char* value;
    const char* p;
    int n;
    float f;
    char* patch;
    char sound[MAX_QPATH];
    char playerModel[MAX_QPATH];
    char customSkin[MAX_QPATH];
    clientInfo_t* ci = &NPC->client->clientInfo;
    renderInfo_t* ri = &NPC->client->renderInfo;
    gNPCstats_t* stats = nullptr;
    bool md3Model = true;
    char surfOff[1024];
    char surfOn[1024];

    Q_strncpyz(customSkin, "default", sizeof(customSkin));
    if (!NPCName || !NPCName[0]) {
        NPCName = "Player";
    }

    if (NPC->NPC) {
        stats = &NPC->NPC->stats;
        /*
            NPC->NPC->allWeaponOrder[0]	= weapon_t::WP_BRYAR_PISTOL;
            NPC->NPC->allWeaponOrder[1]	= weapon_t::WP_SABER;
            NPC->NPC->allWeaponOrder[2]	= WP_IMOD;
            NPC->NPC->allWeaponOrder[3]	= WP_SCAVENGER_RIFLE;
            NPC->NPC->allWeaponOrder[4]	= WP_TRICORDER;
            NPC->NPC->allWeaponOrder[6]	= weapon_t::WP_NONE;
            NPC->NPC->allWeaponOrder[6]	= weapon_t::WP_NONE;
            NPC->NPC->allWeaponOrder[7]	= weapon_t::WP_NONE;
        */
        // fill in defaults
        stats->aggression = 3;
        stats->aim = 3;
        stats->earshot = 1024;
        stats->evasion = 3;
        stats->hfov = 90;
        stats->intelligence = 3;
        stats->move = 3;
        stats->reactions = 3;
        stats->vfov = 60;
        stats->vigilance = 0.1f;
        stats->visrange = 1024;

        stats->health = 0;

        stats->moveType = movetype_t::MT_RUNJUMP;
        stats->yawSpeed = 90;
        stats->walkSpeed = 90;
        stats->runSpeed = 300;
        stats->acceleration = 15; // Increase/descrease speed this much per frame (20fps)
    }
    else {
        stats = nullptr;
    }

    Q_strncpyz(ci->name, NPCName, sizeof(ci->name));

    NPC->playerModel = -1;

    // Set defaults
    // FIXME: should probably put default torso and head models, but what about enemies
    // that don't have any- like Stasis?
    // Q_strncpyz( ri->headModelName,	DEFAULT_HEADMODEL,  sizeof(ri->headModelName));
    // Q_strncpyz( ri->torsoModelName, DEFAULT_TORSOMODEL, sizeof(ri->torsoModelName));
    // Q_strncpyz( ri->legsModelName,	DEFAULT_LEGSMODEL,  sizeof(ri->legsModelName));
    memset(ri->headModelName, 0, sizeof(ri->headModelName));
    memset(ri->torsoModelName, 0, sizeof(ri->torsoModelName));
    memset(ri->legsModelName, 0, sizeof(ri->legsModelName));
    // FIXME: should we have one for weapon too?
    memset((char*)surfOff, 0, sizeof(surfOff));
    memset((char*)surfOn, 0, sizeof(surfOn));

    /*
    ri->headYawRangeLeft = 50;
    ri->headYawRangeRight = 50;
    ri->headPitchRangeUp = 40;
    ri->headPitchRangeDown = 50;
    ri->torsoYawRangeLeft = 60;
    ri->torsoYawRangeRight = 60;
    ri->torsoPitchRangeUp = 30;
    ri->torsoPitchRangeDown = 70;
    */

    ri->headYawRangeLeft = 80;
    ri->headYawRangeRight = 80;
    ri->headPitchRangeUp = 45;
    ri->headPitchRangeDown = 45;
    ri->torsoYawRangeLeft = 60;
    ri->torsoYawRangeRight = 60;
    ri->torsoPitchRangeUp = 30;
    ri->torsoPitchRangeDown = 50;

    VectorCopy(playerMins, NPC->mins);
    VectorCopy(playerMaxs, NPC->maxs);
    NPC->client->crouchheight = CROUCH_MAXS_2;
    NPC->client->standheight = DEFAULT_MAXS_2;

    NPC->client->dismemberProbHead = 100;
    NPC->client->dismemberProbArms = 100;
    NPC->client->dismemberProbHands = 100;
    NPC->client->dismemberProbWaist = 100;
    NPC->client->dismemberProbLegs = 100;

    if (!Q_stricmp("random", NPCName)) { // Randomly assemble a starfleet guy
        NPC_BuildRandom(NPC);
    }
    else {
        p = NPCParms;
        COM_BeginParseSession();

        // look for the right NPC
        while (p) {
            token = COM_ParseExt(&p, true);
            if (token[0] == 0) {
                COM_EndParseSession();
                return false;
            }

            if (!Q_stricmp(token, NPCName)) {
                break;
            }

            SkipBracedSection(&p);
        }
        if (!p) {
            COM_EndParseSession();
            return false;
        }

        if (G_ParseLiteral(&p, "{")) {
            COM_EndParseSession();
            return false;
        }

        // parse the NPC info block
        while (1) {
            token = COM_ParseExt(&p, true);
            if (!token[0]) {
                Q_PrintlnError("unexpected EOF while parsing '{}'", NPCName);
                COM_EndParseSession();
                return false;
            }

            if (!Q_stricmp(token, "}")) {
                break;
            }
            //===MODEL PROPERTIES===========================================================
            // headmodel
            if (!Q_stricmp(token, "headmodel")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }

                if (!Q_stricmp("none", value)) {
                    ri->headModelName[0] = '\0';
                    // Zero the head clamp range so the torso & legs don't lag behind
                    ri->headYawRangeLeft = ri->headYawRangeRight = ri->headPitchRangeUp = ri->headPitchRangeDown = 0;
                }
                else {
                    Q_strncpyz(ri->headModelName, value, sizeof(ri->headModelName));
                }
                continue;
            }

            // torsomodel
            if (!Q_stricmp(token, "torsomodel")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }

                if (!Q_stricmp("none", value)) {
                    ri->torsoModelName[0] = '\0';
                    // Zero the torso clamp range so the legs don't lag behind
                    ri->torsoYawRangeLeft = ri->torsoYawRangeRight = ri->torsoPitchRangeUp = ri->torsoPitchRangeDown = 0;
                }
                else {
                    Q_strncpyz(ri->torsoModelName, value, sizeof(ri->torsoModelName));
                }
                continue;
            }

            // legsmodel
            if (!Q_stricmp(token, "legsmodel")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }
                Q_strncpyz(ri->legsModelName, value, sizeof(ri->legsModelName));
                // Need to do this here to get the right index
                G_ParseAnimFileSet(ri->legsModelName, ri->legsModelName, &ci->animFileIndex);
                continue;
            }

            // playerModel
            if (!Q_stricmp(token, "playerModel")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }
                Q_strncpyz(playerModel, value, sizeof(playerModel));
                md3Model = false;
                continue;
            }

            // customSkin
            if (!Q_stricmp(token, "customSkin")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }
                Q_strncpyz(customSkin, value, sizeof(customSkin));
                continue;
            }

            // surfOff
            if (!Q_stricmp(token, "surfOff")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }
                if (surfOff[0]) {
                    Q_strcat(surfOff, sizeof(surfOff), ",");
                    Q_strcat(surfOff, sizeof(surfOff), value);
                }
                else {
                    Q_strncpyz(surfOff, value, sizeof(surfOff));
                }
                continue;
            }

            // surfOn
            if (!Q_stricmp(token, "surfOn")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }
                if (surfOn[0]) {
                    Q_strcat(surfOn, sizeof(surfOn), ",");
                    Q_strcat(surfOn, sizeof(surfOn), value);
                }
                else {
                    Q_strncpyz(surfOn, value, sizeof(surfOn));
                }
                continue;
            }

            // headYawRangeLeft
            if (!Q_stricmp(token, "headYawRangeLeft")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_PrintlnWarning("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                ri->headYawRangeLeft = n;
                continue;
            }

            // headYawRangeRight
            if (!Q_stricmp(token, "headYawRangeRight")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_PrintlnWarning("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                ri->headYawRangeRight = n;
                continue;
            }

            // headPitchRangeUp
            if (!Q_stricmp(token, "headPitchRangeUp")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_PrintlnWarning("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                ri->headPitchRangeUp = n;
                continue;
            }

            // headPitchRangeDown
            if (!Q_stricmp(token, "headPitchRangeDown")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_PrintlnWarning("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                ri->headPitchRangeDown = n;
                continue;
            }

            // torsoYawRangeLeft
            if (!Q_stricmp(token, "torsoYawRangeLeft")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_PrintlnWarning("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                ri->torsoYawRangeLeft = n;
                continue;
            }

            // torsoYawRangeRight
            if (!Q_stricmp(token, "torsoYawRangeRight")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_PrintlnWarning("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                ri->torsoYawRangeRight = n;
                continue;
            }

            // torsoPitchRangeUp
            if (!Q_stricmp(token, "torsoPitchRangeUp")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_PrintlnWarning("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                ri->torsoPitchRangeUp = n;
                continue;
            }

            // torsoPitchRangeDown
            if (!Q_stricmp(token, "torsoPitchRangeDown")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_PrintlnWarning("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                ri->torsoPitchRangeDown = n;
                continue;
            }

            // Uniform XYZ scale
            if (!Q_stricmp(token, "scale")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (n != 100) {
                    NPC->s.modelScale[0] = NPC->s.modelScale[1] = NPC->s.modelScale[2] = n / 100.0f;
                }
                continue;
            }

            // X scale
            if (!Q_stricmp(token, "scaleX")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (n != 100) {
                    NPC->s.modelScale[0] = n / 100.0f;
                }
                continue;
            }

            // Y scale
            if (!Q_stricmp(token, "scaleY")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (n != 100) {
                    NPC->s.modelScale[1] = n / 100.0f;
                }
                continue;
            }

            // Z scale
            if (!Q_stricmp(token, "scaleZ")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (n != 100) {
                    NPC->s.modelScale[2] = n / 100.0f;
                }
                continue;
            }

            //===AI STATS=====================================================================
            // aggression
            if (!Q_stricmp(token, "aggression")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 1 || n > 5) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    stats->aggression = n;
                }
                continue;
            }

            // aim
            if (!Q_stricmp(token, "aim")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 1 || n > 5) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    stats->aim = n;
                }
                continue;
            }

            // earshot
            if (!Q_stricmp(token, "earshot")) {
                if (COM_ParseFloat(&p, &f)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (f < 0.0f) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    stats->earshot = f;
                }
                continue;
            }

            // evasion
            if (!Q_stricmp(token, "evasion")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 1 || n > 5) {
                    Q_PrintlnWarning("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    stats->evasion = n;
                }
                continue;
            }

            // hfov
            if (!Q_stricmp(token, "hfov")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 30 || n > 180) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    stats->hfov = n; // / 2;	//FIXME: Why was this being done?!
                }
                continue;
            }

            // intelligence
            if (!Q_stricmp(token, "intelligence")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 1 || n > 5) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    stats->intelligence = n;
                }
                continue;
            }

            // move
            if (!Q_stricmp(token, "move")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 1 || n > 5) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    stats->move = n;
                }
                continue;
            }

            // reactions
            if (!Q_stricmp(token, "reactions")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 1 || n > 5) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    stats->reactions = n;
                }
                continue;
            }

            // shootDistance
            if (!Q_stricmp(token, "saberColor")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }
                if (NPC->client) {
                    NPC->client->ps.saberColor = TranslateSaberColor(value);
                }
                continue;
            }

            // shootDistance
            if (!Q_stricmp(token, "shootDistance")) {
                if (COM_ParseFloat(&p, &f)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (f < 0.0f) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    stats->shootDistance = f;
                }
                continue;
            }

            // shootDistance
            if (!Q_stricmp(token, "health")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_PrintlnWarning("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    stats->health = n;
                }
                continue;
            }

            // vfov
            if (!Q_stricmp(token, "vfov")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 30 || n > 180) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    stats->vfov = n / 2;
                }
                continue;
            }

            // vigilance
            if (!Q_stricmp(token, "vigilance")) {
                if (COM_ParseFloat(&p, &f)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (f < 0.0f) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    stats->vigilance = f;
                }
                continue;
            }

            // visrange
            if (!Q_stricmp(token, "visrange")) {
                if (COM_ParseFloat(&p, &f)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (f < 0.0f) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    stats->visrange = f;
                }
                continue;
            }

            // race
            //		if ( !Q_stricmp( token, "race" ) )
            //		{
            //			if ( COM_ParseString( &p, &value ) )
            //			{
            //				continue;
            //			}
            //			NPC->client->race = TranslateRaceName(value);
            //			continue;
            //		}

            // rank
            if (!Q_stricmp(token, "rank")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }
                if (NPC->NPC) {
                    NPC->NPC->rank = TranslateRankName(value);
                }
                continue;
            }

            // fullName
            if (!Q_stricmp(token, "fullName")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }
                NPC->fullName = G_NewString(value);
                continue;
            }

            // playerTeam
            if (!Q_stricmp(token, "playerTeam")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }
                NPC->client->playerTeam = TranslateTeamName(value);
                continue;
            }

            // enemyTeam
            if (!Q_stricmp(token, "enemyTeam")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }
                NPC->client->enemyTeam = TranslateTeamName(value);
                continue;
            }

            // class
            if (!Q_stricmp(token, "class")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }
                NPC->client->NPC_class = TranslateClassName(value);
                continue;
            }

            // dismemberment probability for head
            if (!Q_stricmp(token, "dismemberProbHead")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    NPC->client->dismemberProbHead = n;
                }
                continue;
            }

            // dismemberment probability for arms
            if (!Q_stricmp(token, "dismemberProbArms")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    NPC->client->dismemberProbArms = n;
                }
                continue;
            }

            // dismemberment probability for hands
            if (!Q_stricmp(token, "dismemberProbHands")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    NPC->client->dismemberProbHands = n;
                }
                continue;
            }

            // dismemberment probability for waist
            if (!Q_stricmp(token, "dismemberProbWaist")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    NPC->client->dismemberProbWaist = n;
                }
                continue;
            }

            // dismemberment probability for legs
            if (!Q_stricmp(token, "dismemberProbLegs")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    NPC->client->dismemberProbLegs = n;
                }
                continue;
            }

            //===MOVEMENT STATS============================================================

            if (!Q_stricmp(token, "width")) {
                if (COM_ParseInt(&p, &n)) {
                    continue;
                }

                NPC->mins[0] = NPC->mins[1] = -n;
                NPC->maxs[0] = NPC->maxs[1] = n;
                continue;
            }

            if (!Q_stricmp(token, "height")) {
                if (COM_ParseInt(&p, &n)) {
                    continue;
                }

                NPC->mins[2] = DEFAULT_MINS_2; // Cannot change
                NPC->maxs[2] = NPC->client->standheight = n + DEFAULT_MINS_2;
                NPC->radius = n;
                continue;
            }

            if (!Q_stricmp(token, "crouchheight")) {
                if (COM_ParseInt(&p, &n)) {
                    continue;
                }

                NPC->client->crouchheight = n + DEFAULT_MINS_2;
                continue;
            }

            if (!Q_stricmp(token, "movetype")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }

                if (NPC->NPC) {
                    stats->moveType = MoveTypeNameToEnum(value);
                }
                continue;
            }

            // yawSpeed
            if (!Q_stricmp(token, "yawSpeed")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n <= 0) {
                    Q_Println("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    stats->yawSpeed = ((float)(n));
                }
                continue;
            }

            // walkSpeed
            if (!Q_stricmp(token, "walkSpeed")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_PrintlnWarning("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    stats->walkSpeed = n;
                }
                continue;
            }

            // runSpeed
            if (!Q_stricmp(token, "runSpeed")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_PrintlnWarning("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    stats->runSpeed = n;
                }
                continue;
            }

            // acceleration
            if (!Q_stricmp(token, "acceleration")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < 0) {
                    Q_PrintlnWarning("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    stats->acceleration = n;
                }
                continue;
            }
            //===MISC===============================================================================
            // default behavior
            if (!Q_stricmp(token, "behavior")) {
                if (COM_ParseInt(&p, &n)) {
                    SkipRestOfLine(&p);
                    continue;
                }
                if (n < to_underlying(bState_t::BS_DEFAULT) || n >= to_underlying(bState_t::NUM_BSTATES)) {
                    Q_PrintlnWarning("bad {} in NPC '{}'", token, NPCName);
                    continue;
                }
                if (NPC->NPC) {
                    NPC->NPC->defaultBehavior = (bState_t)(n);
                }
                continue;
            }

            // snd
            if (!Q_stricmp(token, "snd")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }
                if (!Any(NPC->svFlags & svFlags_t::SVF_NO_BASIC_SOUNDS)) {
                    // FIXME: store this in some sound field or parse in the soundTable like the animTable...
                    Q_strncpyz(sound, value, sizeof(sound));
                    patch = strstr(sound, "/");
                    if (patch) {
                        *patch = 0;
                    }
                    ci->customBasicSoundDir = G_NewString(sound);
                }
                continue;
            }

            // sndcombat
            if (!Q_stricmp(token, "sndcombat")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }
                if (!Any(NPC->svFlags & svFlags_t::SVF_NO_COMBAT_SOUNDS)) {
                    // FIXME: store this in some sound field or parse in the soundTable like the animTable...
                    Q_strncpyz(sound, value, sizeof(sound));
                    patch = strstr(sound, "/");
                    if (patch) {
                        *patch = 0;
                    }
                    ci->customCombatSoundDir = G_NewString(sound);
                }
                continue;
            }

            // sndextra
            if (!Q_stricmp(token, "sndextra")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }
                if (!Any(NPC->svFlags & svFlags_t::SVF_NO_EXTRA_SOUNDS)) {
                    // FIXME: store this in some sound field or parse in the soundTable like the animTable...
                    Q_strncpyz(sound, value, sizeof(sound));
                    patch = strstr(sound, "/");
                    if (patch) {
                        *patch = 0;
                    }
                    ci->customExtraSoundDir = G_NewString(sound);
                }
                continue;
            }

            // sndjedi
            if (!Q_stricmp(token, "sndjedi")) {
                if (COM_ParseString(&p, &value)) {
                    continue;
                }
                if (!Any(NPC->svFlags & svFlags_t::SVF_NO_EXTRA_SOUNDS)) {
                    // FIXME: store this in some sound field or parse in the soundTable like the animTable...
                    Q_strncpyz(sound, value, sizeof(sound));
                    patch = strstr(sound, "/");
                    if (patch) {
                        *patch = 0;
                    }
                    ci->customJediSoundDir = G_NewString(sound);
                }
                continue;
            }

            Q_PrintlnWarning("unknown keyword '{}' while parsing '{}'", token, NPCName);
            SkipRestOfLine(&p);
        }
        COM_EndParseSession();
    }

    ci->infoValid = false;

    /*
    Ghoul2 Insert Start
    */
    if (!md3Model) {
        NPC->weaponModel = -1;
        G_SetG2PlayerModel(NPC, playerModel, customSkin, surfOff, surfOn);
    }
    /*
    Ghoul2 Insert End
    */
    if (NPCsPrecached) { // Spawning in after initial precache, our models are precached, we just need to set our clientInfo
        CG_RegisterClientModels(NPC->s.number);
        CG_RegisterNPCCustomSounds(ci);
        CG_RegisterNPCEffects(NPC->client->playerTeam);
    }

    return true;
}

void NPC_LoadParms(void)
{
    int len, totallen, npcExtFNLen, mainBlockLen, fileCnt, i;
    const char filename[] = "ext_data/NPCs.cfg";
    char *buffer, *holdChar, *marker;
    char npcExtensionListBuf[2048]; //	The list of file names read in

    // First, load in the npcs.cfg
    len = gi.FS_ReadFile(filename, (void**)&buffer);
    if (len == -1) {
        Q_Println("file not found");
        return;
    }

    if (len >= MAX_NPC_DATA_SIZE) {
        Q_Error(errorParm_t::ERR_DROP, "ext_data/NPCs.cfg is too large");
    }

    strncpy(NPCParms, buffer, sizeof(NPCParms) - 1);
    gi.FS_FreeFile(buffer);

    // remember where to store the next one
    totallen = mainBlockLen = len;
    marker = NPCParms + totallen;

    // now load in the extra .npc extensions
    fileCnt = gi.FS_GetFileList("ext_data", ".npc", npcExtensionListBuf, sizeof(npcExtensionListBuf));

    holdChar = npcExtensionListBuf;
    for (i = 0; i < fileCnt; i++, holdChar += npcExtFNLen + 1) {
        npcExtFNLen = strlen(holdChar);

        len = gi.FS_ReadFile(Q_Format("ext_data/{}", holdChar).c_str(), (void**)&buffer);

        if (len == -1) {
            Q_Println("error reading file");
        }
        else {
            if (totallen + len >= MAX_NPC_DATA_SIZE) {
                Q_Error(errorParm_t::ERR_DROP, "NPC extensions (*.npc) are too large");
            }
            strcat(marker, buffer);
            gi.FS_FreeFile(buffer);

            totallen += len;
            marker = NPCParms + totallen;
        }
    }
}

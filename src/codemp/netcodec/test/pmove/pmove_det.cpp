/*
===========================================================================
Copyright (C) 2026, ModernJK contributors

This file is part of the ModernJK source code.

ModernJK is free software; you can redistribute it and/or modify it
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

// Determinism test of the game's player movement (bg_pmove.cpp, bg_slidemove.cpp), outside the engine: the same synthetic
// commands through Pmove, in a small world of brushes, and a hash of the player state after every command. Built
// twice, as the server's game module (-D_GAME) and as cgame (-D_CGAME), with each compiler: cgame's prediction is
// right only if a client build gives the same states as the server build. Pmove does not round the velocity, so
// this game code runs one mode, 1: no rounding (modes 0, the engine's Sys_SnapVector, and 2, ties to even, are not
// built). The original 1.01 game code, built in 32-bit with x87 math, gave states up to 2.7 units apart between a GCC
// server and an MSVC client: the x87's 80-bit registers are spilled to memory where each compiler chooses.
// Output: one line per scenario and mode: the hash of the movement fields (origin, velocity, view angles, ground
// entity, pm_flags, pm_time, commandTime) and of the whole player state after every command, and the final origin and
// velocity as float bit patterns. --dump=<file>: the movement fields after every command (float bits), to compare two
// builds command by command (scripts/pmove_compare.py).
// Usage: netcodec_pmove_game [--dump=file] [commands]   (same for netcodec_pmove_cgame)
#if defined(_GAME)
#include <game/g_local.h>
#elif defined(_CGAME)
#include <cgame/cg_local.h>
#else
#error "build with _GAME or _CGAME"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ------------------------------------------------------------------ the world: convex brushes, solid

typedef struct {
    vec3_t normal;
    float dist;
} hplane_t;

typedef struct {
    int numPlanes;
    hplane_t planes[8];
} hbrush_t;

static hbrush_t world[16];
static int numBrushes;

static void AddPlane(hbrush_t* b, float nx, float ny, float nz, float dist)
{
    hplane_t* p = &b->planes[b->numPlanes++];
    VectorSet(p->normal, nx, ny, nz);
    p->dist = dist;
}

// an axis-aligned box; extra: an optional slanted plane (normal, dist), else nullptr
static void AddBox(float x0, float y0, float z0, float x1, float y1, float z1, const float* extra)
{
    hbrush_t* b = &world[numBrushes++];
    b->numPlanes = 0;
    AddPlane(b, 1, 0, 0, x1);
    AddPlane(b, -1, 0, 0, -x0);
    AddPlane(b, 0, 1, 0, y1);
    AddPlane(b, 0, -1, 0, -y0);
    AddPlane(b, 0, 0, 1, z1);
    AddPlane(b, 0, 0, -1, -z0);
    if (extra)
        AddPlane(b, extra[0], extra[1], extra[2], extra[3]);
}

static void BuildWorld(void)
{
    // a ramp of 30 degrees rising along +x from x = 600: top plane through (600, 0, 0)
    const float s = 0.5f, c = 0.86602540378f;
    const float ramp[4] = {-s, 0.0f, c, -s * 600.0f};
    numBrushes = 0;
    AddBox(-2048, -2048, -64, 2048, 2048, 0, nullptr);  // floor
    AddBox(-2112, -2048, 0, -2048, 2048, 512, nullptr); // walls
    AddBox(2048, -2048, 0, 2112, 2048, 512, nullptr);
    AddBox(-2048, -2112, 0, 2048, -2048, 512, nullptr);
    AddBox(-2048, 2048, 0, 2048, 2112, 512, nullptr);
    AddBox(-2048, -2048, 512, 2048, 2048, 576, nullptr); // ceiling
    AddBox(256, -256, 0, 512, 256, 16, nullptr);         // a step
    AddBox(-512, -128, 0, -384, 0, 40, nullptr);         // a crate (too high to step on)
    AddBox(600, -200, 0, 1000, 200, 231, ramp);          // the ramp
    AddBox(-1200, 600, 0, -1000, 800, 120, nullptr);     // a pillar
}

constexpr float SURF_EPS = 0.125f; // SURFACE_CLIP_EPSILON of the engine's collision code

// box trace against the brushes, as the engine's CM_TraceThroughBrush
static void World_Trace(trace_t* tr, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, contentsFlags_t contentMask)
{
    int i, j;
    (void)passEntityNum;
    memset(tr, 0, sizeof(*tr));
    tr->fraction = 1.0f;
    tr->entityNum = ENTITYNUM_NONE;
    if (!Any(contentMask & contentsFlags_t::CONTENTS_SOLID)) {
        VectorCopy(end, tr->endpos);
        return;
    }
    for (i = 0; i < numBrushes; i++) {
        const hbrush_t* b = &world[i];
        float enterFrac = -1.0f, leaveFrac = 1.0f;
        const hplane_t* clip = nullptr;
        bool getout = false, startout = false, skip = false;
        for (j = 0; j < b->numPlanes; j++) {
            const hplane_t* p = &b->planes[j];
            vec3_t corner;
            float dist, d1, d2, f;
            corner[0] = p->normal[0] < 0 ? maxs[0] : mins[0];
            corner[1] = p->normal[1] < 0 ? maxs[1] : mins[1];
            corner[2] = p->normal[2] < 0 ? maxs[2] : mins[2];
            dist = p->dist - DotProduct(corner, p->normal);
            d1 = DotProduct(start, p->normal) - dist;
            d2 = DotProduct(end, p->normal) - dist;
            if (d2 > 0)
                getout = true;
            if (d1 > 0)
                startout = true;
            if (d1 > 0 && (d2 >= SURF_EPS || d2 >= d1)) { // completely in front of this plane
                skip = true;
                break;
            }
            if (d1 <= 0 && d2 <= 0)
                continue;
            if (d1 > d2) { // entering
                f = (d1 - SURF_EPS) / (d1 - d2);
                if (f < 0)
                    f = 0;
                if (f > enterFrac) {
                    enterFrac = f;
                    clip = p;
                }
            }
            else { // leaving
                f = (d1 + SURF_EPS) / (d1 - d2);
                if (f > 1)
                    f = 1;
                if (f < leaveFrac)
                    leaveFrac = f;
            }
        }
        if (skip)
            continue;
        if (!startout) {
            tr->startsolid = true;
            if (!getout) {
                tr->allsolid = true;
                tr->fraction = 0;
            }
            tr->entityNum = ENTITYNUM_WORLD;
            continue;
        }
        if (enterFrac < leaveFrac && enterFrac > -1 && enterFrac < tr->fraction) {
            if (enterFrac < 0)
                enterFrac = 0;
            tr->fraction = enterFrac;
            VectorCopy(clip->normal, tr->plane.normal);
            tr->plane.dist = clip->dist;
            tr->plane.type = PlaneTypeForNormal(clip->normal);
            SetPlaneSignbits(&tr->plane);
            tr->contents = contentsFlags_t::CONTENTS_SOLID;
            tr->entityNum = ENTITYNUM_WORLD;
        }
    }
    for (i = 0; i < 3; i++)
        tr->endpos[i] = start[i] + tr->fraction * (end[i] - start[i]);
}

static contentsFlags_t World_Contents(const vec3_t point, int passEntityNum)
{
    (void)point;
    (void)passEntityNum;
    return contentsFlags_t::CONTENTS_NONE;
}

// ------------------------------------------------------------------ what the bg code needs from the module

static int harnessTime;

static NORETURN void QDECL HarnessError(errorParm_t code, const char* fmt, ...)
{
    (void)code;
    fprintf(stderr, "Com_Error: %s\n", fmt);
    exit(2);
}
static void QDECL HarnessPrintf(const char* fmt, ...)
{
    (void)fmt;
}
// function pointers in the modules (q_shared.h)
NORETURN_PTR void (*Com_Error)(errorParm_t level, const char* error, ...) = HarnessError;
void (*Com_Printf)(const char* msg, ...) = HarnessPrintf;

// debug builds register a cvar from BG_AddPredictableEventToPlayerstate (bg_misc.cpp)
static void CvarRegisterTrap(vmCvar_t* vmCvar, const char* varName, const char* defaultValue, cvarFlags_t flags)
{
    (void)varName;
    (void)defaultValue;
    (void)flags;
    memset(vmCvar, 0, sizeof(*vmCvar));
}

#if defined(_GAME)
static gameImport_t imports;
gameImport_t* trap = &imports;
level_locals_t level;
gentity_t g_entities[MAX_GENTITIES];
vmCvar_t bg_fighterAltControl, g_fixRunWalkAnims, g_fixSaberMoveData, g_fixWeaponAttackAnim;
bool FighterIsLanded(Vehicle_t* pVeh, playerState_t* parentPS)
{
    (void)pVeh;
    (void)parentPS;
    return false;
}
void G_AddEvent(gentity_t* ent, entity_event_t event, int eventParm)
{
    (void)ent;
    (void)event;
    (void)eventParm;
}
void G_CheapWeaponFire(int entNum, entity_event_t ev)
{
    (void)entNum;
    (void)ev;
}
void G_Damage(gentity_t* targ, gentity_t* inflictor, gentity_t* attacker, vec3_t dir, vec3_t point, int damage, damageFlags_t dflags, meansOfDeath_t mod)
{
    (void)targ;
    (void)inflictor;
    (void)attacker;
    (void)dir;
    (void)point;
    (void)damage;
    (void)dflags;
    (void)mod;
}
void G_FlyVehicleSurfaceDestruction(gentity_t* veh, trace_t* trace, int magnitude, bool force)
{
    (void)veh;
    (void)trace;
    (void)magnitude;
    (void)force;
}
gentity_t* G_PlayEffect(effectTypes_t fxID, vec3_t org, vec3_t ang)
{
    (void)fxID;
    (void)org;
    (void)ang;
    return nullptr;
}
gentity_t* G_PlayEffectID(const int fxID, vec3_t org, vec3_t ang)
{
    (void)fxID;
    (void)org;
    (void)ang;
    return nullptr;
}
int G_SoundIndex(const char* name)
{
    (void)name;
    return 0;
}
int G_ModelIndex(const char* name)
{
    (void)name;
    return 0;
}
int G_EffectIndex(const char* name)
{
    (void)name;
    return 0;
}
bool G_CanBeEnemy(gentity_t* self, gentity_t* enemy)
{
    (void)self;
    (void)enemy;
    return false;
}
void Client_CheckImpactBBrush(gentity_t* self, gentity_t* other)
{
    (void)self;
    (void)other;
}
void G_SetSharedVehicleFunctions(vehicleInfo_t* pVehInfo)
{
    (void)pVehInfo;
}
void NPC_SetAnim(gentity_t* ent, setAnimParts_t type, animNumber_t anim, setAnimFlags_t priority)
{
    (void)ent;
    (void)type;
    (void)anim;
    (void)priority;
}
bool TryGrapple(gentity_t* ent)
{
    (void)ent;
    return false;
}
void WP_GetVehicleCamPos(gentity_t* ent, gentity_t* pilot, vec3_t camPos)
{
    (void)ent;
    (void)pilot;
    (void)camPos;
}
#define HARNESS_ENTITIES g_entities
#define HARNESS_ENTSIZE sizeof(gentity_t)
#else
static cgameImport_t imports;
cgameImport_t* trap = &imports;
cgs_t cgs;
centity_t cg_entities[MAX_GENTITIES];
vmCvar_t bg_fighterAltControl;
bool BG_FighterUpdate(Vehicle_t* pVeh, const usercmd_t* pUcmd, vec3_t trMins, vec3_t trMaxs, float gravity,
                      void (*traceFunc)(trace_t* results, const vec3_t start, const vec3_t lmins, const vec3_t lmaxs, const vec3_t end, int passEntityNum,
                                        contentsFlags_t contentMask))
{
    (void)pVeh;
    (void)pUcmd;
    (void)trMins;
    (void)trMaxs;
    (void)gravity;
    (void)traceFunc;
    return false;
}
const char* CG_ConfigString(int index)
{
    (void)index;
    return "";
}
void CG_GetVehicleCamPos(vec3_t camPos)
{
    (void)camPos;
}
#define HARNESS_ENTITIES cg_entities
#define HARNESS_ENTSIZE sizeof(centity_t)
#endif
int BG_GetTime(void)
{
    return harnessTime;
}
void G_SetAnimalVehicleFunctions(vehicleInfo_t* pVehInfo)
{
    (void)pVehInfo;
}
void G_SetFighterVehicleFunctions(vehicleInfo_t* pVehInfo)
{
    (void)pVehInfo;
}
void G_SetSpeederVehicleFunctions(vehicleInfo_t* pVehInfo)
{
    (void)pVehInfo;
}
void G_SetWalkerVehicleFunctions(vehicleInfo_t* pVehInfo)
{
    (void)pVehInfo;
}

// ------------------------------------------------------------------ commands and hashes

static uint64_t rng;
static uint64_t Next64(void)
{ // splitmix64
    uint64_t z = (rng += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static int Rand(int n)
{
    return (int)(Next64() % (uint64_t)n);
}

static uint64_t Fnv(uint64_t h, const void* p, size_t n)
{
    const unsigned char* b = (const unsigned char*)p;
    size_t i;
    for (i = 0; i < n; i++)
        h = (h ^ b[i]) * 0x100000001B3ULL;
    return h;
}

static uint64_t MoveHash(uint64_t h, const playerState_t* ps)
{
    h = Fnv(h, ps->origin, sizeof(ps->origin));
    h = Fnv(h, ps->velocity, sizeof(ps->velocity));
    h = Fnv(h, ps->viewangles, sizeof(ps->viewangles));
    h = Fnv(h, &ps->groundEntityNum, sizeof(ps->groundEntityNum));
    h = Fnv(h, &ps->pm_flags, sizeof(ps->pm_flags));
    h = Fnv(h, &ps->pm_time, sizeof(ps->pm_time));
    h = Fnv(h, &ps->commandTime, sizeof(ps->commandTime));
    return h;
}

static uint32_t Bits(float f)
{
    uint32_t b;
    memcpy(&b, &f, 4);
    return b;
}

static animSet_t anims;

// frame times of the scenarios, in ms: 125 fps, 333 fps, 60 fps, and random from 3 to 20
static int FrameMsec(int scenario, int i)
{
    switch (scenario) {
    case 0:
        return 8;
    case 1:
        return 3;
    case 2:
        return (i % 3 == 0) ? 16 : 17;
    default:
        return 3 + Rand(18);
    }
}

static const char* const kModeNames[4] = {"engine rounding", "no rounding", "ties to even", "1.01 rounding"};

static void Run(int scenario, int mode, int commands, FILE* dump)
{
    playerState_t ps;
    pmove_t pm;
    usercmd_t cmd;
    uint64_t moveHash = 0xCBF29CE484222325ULL, fullHash = 0xCBF29CE484222325ULL;
    int i, segment = 0;
    signed char fwd = 0, right = 0;
    int crouch = 0, yawRate = 0;

    rng = 0x5EED0000ULL + (uint64_t)scenario;
    memset(&ps, 0, sizeof(ps));
    memset(&cmd, 0, sizeof(cmd));
    ps.clientNum = 0;
    ps.pm_type = pmtype_t::PM_NORMAL;
    ps.gravity = 800;
    ps.speed = 250;
    ps.basespeed = 250;
    ps.groundEntityNum = ENTITYNUM_NONE;
    ps.stats[statIndex_t::STAT_HEALTH] = ps.stats[statIndex_t::STAT_MAX_HEALTH] = 100;
    ps.stats[statIndex_t::STAT_WEAPONS] = Bit(weapon_t::WP_BRYAR_PISTOL);
    ps.weapon = weapon_t::WP_BRYAR_PISTOL;
    ps.weaponstate = weaponstate_t::WEAPON_READY;
    ps.viewheight = DEFAULT_VIEWHEIGHT;
    ps.standheight = DEFAULT_MAXS_2;
    ps.crouchheight = CROUCH_MAXS_2;
    ps.fd.forcePowerLevel[forcePowers_t::FP_LEVITATION] = forcePowerLevels_t::FORCE_LEVEL_1;
    ps.fd.forcePower = 100;
    ps.commandTime = 1000;
    VectorSet(ps.origin, -100.0f, 37.0f, 64.0f);

    memset(&HARNESS_ENTITIES[0], 0, sizeof(HARNESS_ENTITIES[0]));
    HARNESS_ENTITIES[0].playerState = &ps;
    HARNESS_ENTITIES[0].localAnimIndex = 0;
    VectorSet(HARNESS_ENTITIES[0].modelScale, 1, 1, 1);

    memset(&pm, 0, sizeof(pm));
    pm.ps = &ps;
    pm.tracemask = MASK_PLAYERSOLID;
    pm.trace = World_Trace;
    pm.pointcontents = World_Contents;
    pm.animations = &anims;
    pm.gametype = gametype_t::GT_FFA;
    pm.stepSlideFix = 1;
    pm.baseEnt = (bgEntity_t*)HARNESS_ENTITIES;
    pm.entSize = HARNESS_ENTSIZE;
    VectorSet(pm.modelScale, 1, 1, 1);
    VectorSet(pm.mins, -15, -15, DEFAULT_MINS_2);
    VectorSet(pm.maxs, 15, 15, DEFAULT_MAXS_2);

    cmd.serverTime = ps.commandTime;
    for (i = 0; i < commands; i++) {
        if (--segment <= 0) { // a new input segment: 0.1 to 1.5 s of the same keys
            static const signed char moves[3] = {-127, 0, 127};
            segment = 12 + Rand(180);
            fwd = moves[Rand(3)];
            right = moves[Rand(3)];
            crouch = Rand(8) == 0;
            yawRate = Rand(161) - 80; // short angle units per command
        }
        cmd.serverTime += FrameMsec(scenario, i);
        harnessTime = cmd.serverTime;
        cmd.forwardmove = fwd;
        cmd.rightmove = right;
        cmd.upmove = crouch ? -127 : (Rand(40) == 0 ? 127 : 0);
        cmd.angles[YAW] = (int)(short)(cmd.angles[YAW] + yawRate + Rand(21) - 10);
        cmd.angles[PITCH] = (int)(short)(Rand(2001) - 1000);
        cmd.buttons = buttonFlags_t::BUTTON_NONE;
        pm.cmd = cmd;
        Pmove(&pm);
        moveHash = MoveHash(moveHash, &ps);
        fullHash = Fnv(fullHash, &ps, sizeof(ps));
        if (dump)
            fprintf(dump, "%d %d %d %08X %08X %08X %08X %08X %08X %08X %08X %08X %d %d %d\n", scenario, mode, i, Bits(ps.origin[0]), Bits(ps.origin[1]),
                    Bits(ps.origin[2]), Bits(ps.velocity[0]), Bits(ps.velocity[1]), Bits(ps.velocity[2]), Bits(ps.viewangles[0]), Bits(ps.viewangles[1]),
                    Bits(ps.viewangles[2]), ps.groundEntityNum, to_underlying(ps.pm_flags), ps.pm_time);
    }
    printf("scenario %d (%s) %s: move %016llX full %016llX origin %08X %08X %08X velocity %08X %08X %08X\n", scenario,
           scenario == 0   ? "125 fps"
           : scenario == 1 ? "333 fps"
           : scenario == 2 ? "60 fps"
                           : "random 3-20 ms",
           kModeNames[mode], (unsigned long long)moveHash, (unsigned long long)fullHash, Bits(ps.origin[0]), Bits(ps.origin[1]), Bits(ps.origin[2]),
           Bits(ps.velocity[0]), Bits(ps.velocity[1]), Bits(ps.velocity[2]));
}

int main(int argc, char** argv)
{
    int commands = 20000, a, s;
    FILE* dump = nullptr;
    for (a = 1; a < argc; a++) {
        if (strncmp(argv[a], "--dump=", 7) == 0)
            dump = fopen(argv[a] + 7, "w");
        else
            commands = atoi(argv[a]);
    }
    imports.Cvar_Register = CvarRegisterTrap;
    for (animation_t& anim : anims) {
        anim.firstFrame = 0;
        anim.numFrames = 10;
        anim.frameLerp = 50;
        anim.loopFrames = -1;
    }
    bgAllAnims[0].anims = &anims;
    BuildWorld();
    printf("pmove determinism (%s build), %d commands per run\n",
#if defined(_GAME)
           "game"
#else
           "cgame"
#endif
           ,
           commands);
    for (s = 0; s < 4; s++) {
        Run(s, 1, commands, dump);
    }
    if (dump)
        fclose(dump);
    return 0;
}

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

// bg_pmove.cpp -- both games player movement code
// takes a playerstate and a usercmd as input and returns a modifed playerstate

// define GAME_INCLUDE so that g_public.h does not define the
// short, server-visible gclient_t and gentity_t structures,
// because we define the full size ones in this file
#define GAME_INCLUDE
#include <code/qcommon/q_shared.h>
#include "b_local.h"
#include "g_shared.h"
#include "bg_local.h"
#include "g_local.h"
#include "g_functions.h"
#include "anims.h"
#include <cgame/cg_local.h> // yeah I know this is naughty, but we're shipping soon...
#include "wp_saber.h"
#include <float.h>

extern bool G_DoDismemberment(gentity_t* self, vec3_t point, meansOfDeath_t mod, int damage, hitLocation_t hitLoc, bool force = false);
extern bool G_EntIsUnlockedDoor(int entityNum);
extern bool G_EntIsDoor(int entityNum);
extern bool InFront(vec3_t spot, vec3_t from, vec3_t fromAngles, float threshHold = 0.0f);
extern void G_AddVoiceEvent(gentity_t* self, entity_event_t event, int speakDebounceTime);
extern bool Q3_TaskIDPending(gentity_t* ent, taskID_t taskType);
extern void WP_SaberInitBladeData(gentity_t* ent);
extern bool WP_SaberLose(gentity_t* self, vec3_t throwDir);
extern void G_SoundOnEnt(gentity_t* ent, soundChannel_t channel, const char* soundPath);
extern int Jedi_ReCalcParryTime(gentity_t* self, evasionType_t evasionType);
extern bool PM_HasAnimation(gentity_t* ent, animNumber_t animation);
extern saberMoveName_t PM_SaberAnimTransitionAnim(saberMoveName_t curmove, saberMoveName_t newmove);
extern saberMoveName_t PM_AttackMoveForQuad(saberQuadrant_t quad);
extern bool PM_SaberInTransition(saberMoveName_t move);
extern bool PM_SaberInTransitionAny(saberMoveName_t move);
extern bool PM_SaberInBounce(saberMoveName_t move);
extern bool PM_SaberInSpecialAttack(animNumber_t anim);
extern bool PM_SaberInAttack(saberMoveName_t move);
extern bool PM_InAnimForSaberMove(animNumber_t anim, saberMoveName_t saberMove);
extern saberMoveName_t PM_SaberBounceForAttack(saberMoveName_t move);
extern saberMoveName_t PM_SaberAttackForMovement(int forwardmove, int rightmove, saberMoveName_t move);
extern saberMoveName_t PM_BrokenParryForParry(saberMoveName_t move);
extern saberMoveName_t PM_KnockawayForParry(saberBlockedType_t move);
extern bool PM_SaberInParry(saberMoveName_t move);
extern bool PM_SaberInKnockaway(saberMoveName_t move);
extern bool PM_SaberInBrokenParry(saberMoveName_t move);
extern bool PM_SaberInReflect(saberMoveName_t move);
extern bool PM_SaberInIdle(saberMoveName_t move);
extern bool PM_SaberInStart(saberMoveName_t move);
extern bool PM_SaberKataDone(saberMoveName_t curmove, saberMoveName_t newmove);
extern bool PM_SaberInSpecial(saberMoveName_t move);
extern bool PM_InDeathAnim(void);
extern bool PM_StandingAnim(animNumber_t anim);
extern saberMoveName_t PM_SaberFlipOverAttackMove(void);
extern saberMoveName_t PM_SaberJumpAttackMove(void);

bool PM_InKnockDown(playerState_t* ps);
bool PM_InKnockDownOnGround(playerState_t* ps);
bool PM_InGetUp(playerState_t* ps);
bool PM_InRoll(playerState_t* ps);
bool PM_SpinningSaberAnim(animNumber_t anim);
bool PM_GettingUpFromKnockDown(float standheight, float crouchheight);
bool PM_SpinningAnim(animNumber_t anim);
bool PM_FlippingAnim(animNumber_t anim);
bool PM_PainAnim(animNumber_t anim);
bool PM_RollingAnim(animNumber_t anim);
bool PM_SwimmingAnim(animNumber_t anim);

extern EnumArray<int, forcePowerLevels_t, forcePowerLevels_t::NUM_FORCE_POWER_LEVELS> parryDebounce;
extern bool player_locked;
extern bool MatrixMode;
bool waterForceJump;
extern cvar_t* g_timescale;

static void PM_SetWaterLevelAtPoint(vec3_t org, int* waterlevel, contentsFlags_t* watertype);

#include <shared/qcommon/q_gamesets.h>
flyMode_t Flying = flyMode_t::FLY_NONE;

pmove_t* pm;
pml_t pml;

// movement parameters
const float pm_stopspeed = 100.0f;
const float pm_duckScale = 0.50f;
const float pm_swimScale = 0.50f;
float pm_ladderScale = 0.7f;

const float pm_accelerate = 12.0f;
const float pm_airaccelerate = 4.0f;
const float pm_wateraccelerate = 4.0f;
const float pm_flyaccelerate = 8.0f;

const float pm_friction = 6.0f;
const float pm_waterfriction = 1.0f;
const float pm_flightfriction = 3.0f;

const float pm_frictionModifier = 3.0f; // Used for "careful" mode (when pressing use)
const float pm_airDecelRate = 1.35f;    // Used for air decelleration away from current movement velocity

int c_pmove = 0;

extern void PM_SetTorsoAnimTimer(gentity_t* ent, int* torsoAnimTimer, int time);
extern void PM_SetLegsAnimTimer(gentity_t* ent, int* legsAnimTimer, int time);
// extern void PM_SetAnim(pmove_t	*pm,setAnimParts_t setAnimParts,animNumber_t anim,setAnimFlags_t setAnimFlags);
extern void PM_TorsoAnimation(void);
extern std::optional<animNumber_t> PM_TorsoAnimForFrame(gentity_t* ent, int torsoFrame);
extern int PM_AnimLength(int index, animNumber_t anim);
extern bool PM_InDeathAnim(void);
extern bool PM_InOnGroundAnim(playerState_t* ps);
extern EnumArray<weaponInfo_t, weapon_t, MAX_WEAPONS> cg_weapons;
extern animNumber_t PM_PickAnim(gentity_t* self, animNumber_t minAnim, animNumber_t maxAnim);

extern void DoImpact(gentity_t* self, gentity_t* other, bool damageSelf);

extern EnumArray<EnumArray<saberMoveName_t, saberQuadrant_t, saberQuadrant_t::Q_NUM_QUADS>, saberQuadrant_t, saberQuadrant_t::Q_NUM_QUADS> transitionMove;

extern bool G_ControlledByPlayer(gentity_t* self);
bool PM_ControlledByPlayer(void)
{
    return G_ControlledByPlayer(pm->gent);
}
/*
===============
PM_AddEvent

===============
*/
void PM_AddEvent(entity_event_t newEvent)
{
    AddEventToPlayerstate(newEvent, 0, pm->ps);
}

/*
===============
bool PM_ClientImpact( int otherEntityNum, bool damageSelf )

===============
*/
bool PM_ClientImpact(int otherEntityNum, bool damageSelf)
{
    gentity_t* traceEnt;

    if (!pm->gent) {
        return false;
    }

    if ((VectorLength(pm->ps->velocity) * (pm->gent->mass / 10)) >= 100 &&
        pm->ps->lastOnGround + 100 < level.time) // was 300 ||(other->material>=material_t::MAT_GLASS&&pm->gent->lastImpact+100<=level.time))
    {
        DoImpact(pm->gent, &g_entities[otherEntityNum], damageSelf);
    }

    if (otherEntityNum >= ENTITYNUM_WORLD) {
        return false;
    }

    traceEnt = &g_entities[otherEntityNum];
    if (!traceEnt || !Any(traceEnt->contents & pm->tracemask)) { // it's dead or not in my way anymore
        return true;
    }

    return false;
}
/*
===============
PM_AddTouchEnt
===============
*/
void PM_AddTouchEnt(int entityNum)
{
    int i;

    if (entityNum == ENTITYNUM_WORLD) {
        return;
    }
    if (pm->numtouch == MAXTOUCH) {
        return;
    }

    // see if it is already added
    for (i = 0; i < pm->numtouch; i++) {
        if (pm->touchents[i] == entityNum) {
            return;
        }
    }

    // add it
    pm->touchents[pm->numtouch] = entityNum;
    pm->numtouch++;
}

/*
==================
PM_ClipVelocity

Slide off of the impacting surface

  This will pull you down onto slopes if heading away from
  them and push you up them as you go up them.
  Also stops you when you hit walls.

==================
*/
void PM_ClipVelocity(vec3_t in, vec3_t normal, vec3_t out, float overbounce)
{
    float backoff;
    float change;
    int i;

    backoff = DotProduct(in, normal);

    if (backoff < 0) {
        backoff *= overbounce;
    }
    else {
        backoff /= overbounce;
    }

    for (i = 0; i < 3; i++) {
        change = normal[i] * backoff;
        out[i] = in[i] - change;
    }
}

/*
==================
PM_Friction

Handles both ground friction and water friction
==================
*/
static void PM_Friction(void)
{
    vec3_t vec;
    float* vel;
    float speed, newspeed, control;
    float drop, friction = pm->ps->friction;

    vel = pm->ps->velocity;

    VectorCopy(vel, vec);
    if (pml.walking) {
        vec[2] = 0; // ignore slope movement
    }

    speed = VectorLength(vec);
    if (speed < 1) {
        vel[0] = 0;
        vel[1] = 0; // allow sinking underwater
        // FIXME: still have z friction underwater?
        return;
    }

    drop = 0;

    // apply ground friction, even if on ladder
    if (Flying != flyMode_t::FLY_NORMAL) {
        if (Any(pm->watertype & contentsFlags_t::CONTENTS_LADDER) || pm->waterlevel <= 1) {
            if (Any(pm->watertype & contentsFlags_t::CONTENTS_LADDER) || (pml.walking && !Any(pml.groundTrace.surfaceFlags & surfaceFlags_t::SURF_SLICK))) {
                // if getting knocked back, no friction
                if (!Any(pm->ps->pm_flags & pmoveFlags_t::PMF_TIME_KNOCKBACK) && !Any(pm->ps->pm_flags & pmoveFlags_t::PMF_TIME_NOFRICTION)) {
                    // If the use key is pressed. slow the player more quickly
                    if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_USE))
                        friction *= pm_frictionModifier;

                    control = speed < pm_stopspeed ? pm_stopspeed : speed;
                    drop += control * friction * pml.frametime;
                }
            }
        }
    }

    if (Flying == flyMode_t::FLY_VEHICLE) {
        if (!Any(pm->ps->pm_flags & pmoveFlags_t::PMF_TIME_KNOCKBACK) && !Any(pm->ps->pm_flags & pmoveFlags_t::PMF_TIME_NOFRICTION)) {
            control = speed < pm_stopspeed ? pm_stopspeed : speed;
            drop += control * friction * pml.frametime;
        }
    }

    // apply water friction even if just wading
    if (!waterForceJump) {
        if (pm->waterlevel && !Any(pm->watertype & contentsFlags_t::CONTENTS_LADDER)) {
            drop += speed * pm_waterfriction * pm->waterlevel * pml.frametime;
        }
    }

    // apply flying friction
    if (pm->ps->pm_type == pmtype_t::PM_SPECTATOR) {
        drop += speed * pm_flightfriction * pml.frametime;
    }

    // scale the velocity
    newspeed = speed - drop;
    if (newspeed < 0)
        newspeed = 0;

    newspeed /= speed;

    vel[0] = vel[0] * newspeed;
    vel[1] = vel[1] * newspeed;
    vel[2] = vel[2] * newspeed;
}

/*
==============
PM_Accelerate

Handles user intended acceleration
==============
*/

static void PM_Accelerate(vec3_t wishdir, float wishspeed, float accel)
{
    int i;
    float addspeed, accelspeed, currentspeed;

    currentspeed = DotProduct(pm->ps->velocity, wishdir);

    addspeed = wishspeed - currentspeed;

    if (addspeed <= 0) {
        return;
    }
    accelspeed = (accel * pml.frametime) * wishspeed;

    if (accelspeed > addspeed) {
        accelspeed = addspeed;
    }
    for (i = 0; i < 3; i++) {
        pm->ps->velocity[i] += accelspeed * wishdir[i];
    }
}

/*
============
PM_CmdScale

Returns the scale factor to apply to cmd movements
This allows the clients to use axial -127 to 127 values for all directions
without getting a sqrt(2) distortion in speed.
============
*/
static float PM_CmdScale(usercmd_t* cmd)
{
    int max;
    float total;
    float scale;

    max = abs(cmd->forwardmove);

    if (abs(cmd->rightmove) > max) {
        max = abs(cmd->rightmove);
    }
    if (abs(cmd->upmove) > max) {
        max = abs(cmd->upmove);
    }
    if (!max) {
        return 0;
    }
    total = Q_sqrt((double)((cmd->forwardmove * cmd->forwardmove) + (cmd->rightmove * cmd->rightmove) + (cmd->upmove * cmd->upmove)));

    scale = (float)pm->ps->speed * max / (127.0f * total);

    return scale;
}

/*
================
PM_SetMovementDir

Determine the rotation of the legs reletive
to the facing dir
================
*/
static void PM_SetMovementDir(void)
{
    if (pm->cmd.forwardmove || pm->cmd.rightmove) {
        if (pm->cmd.rightmove == 0 && pm->cmd.forwardmove > 0) {
            pm->ps->movementDir = 0;
        }
        else if (pm->cmd.rightmove < 0 && pm->cmd.forwardmove > 0) {
            pm->ps->movementDir = 1;
        }
        else if (pm->cmd.rightmove < 0 && pm->cmd.forwardmove == 0) {
            pm->ps->movementDir = 2;
        }
        else if (pm->cmd.rightmove < 0 && pm->cmd.forwardmove < 0) {
            pm->ps->movementDir = 3;
        }
        else if (pm->cmd.rightmove == 0 && pm->cmd.forwardmove < 0) {
            pm->ps->movementDir = 4;
        }
        else if (pm->cmd.rightmove > 0 && pm->cmd.forwardmove < 0) {
            pm->ps->movementDir = 5;
        }
        else if (pm->cmd.rightmove > 0 && pm->cmd.forwardmove == 0) {
            pm->ps->movementDir = 6;
        }
        else if (pm->cmd.rightmove > 0 && pm->cmd.forwardmove > 0) {
            pm->ps->movementDir = 7;
        }
    }
    else {
        // if they aren't actively going directly sideways,
        // change the animation to the diagonal so they
        // don't stop too crooked
        if (pm->ps->movementDir == 2) {
            pm->ps->movementDir = 1;
        }
        else if (pm->ps->movementDir == 6) {
            pm->ps->movementDir = 7;
        }
    }
}

/*
=============
PM_CheckJump
=============
*/
#define METROID_JUMP 1
bool PM_InSpecialJump(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_WALL_RUN_RIGHT:
    case animNumber_t::BOTH_WALL_RUN_RIGHT_STOP:
    case animNumber_t::BOTH_WALL_RUN_RIGHT_FLIP:
    case animNumber_t::BOTH_WALL_RUN_LEFT:
    case animNumber_t::BOTH_WALL_RUN_LEFT_STOP:
    case animNumber_t::BOTH_WALL_RUN_LEFT_FLIP:
    case animNumber_t::BOTH_WALL_FLIP_RIGHT:
    case animNumber_t::BOTH_WALL_FLIP_LEFT:
    case animNumber_t::BOTH_FLIP_BACK1:
    case animNumber_t::BOTH_FLIP_BACK2:
    case animNumber_t::BOTH_FLIP_BACK3:
    case animNumber_t::BOTH_WALL_FLIP_BACK1:
    case animNumber_t::BOTH_BUTTERFLY_LEFT:
    case animNumber_t::BOTH_BUTTERFLY_RIGHT:
    case animNumber_t::BOTH_FJSS_TR_BL:
    case animNumber_t::BOTH_FJSS_TL_BR:
    case animNumber_t::BOTH_FORCELEAP2_T__B_:
    case animNumber_t::BOTH_JUMPFLIPSLASHDOWN1: // #
    case animNumber_t::BOTH_JUMPFLIPSTABDOWN:   // #
    case animNumber_t::BOTH_ARIAL_LEFT:
    case animNumber_t::BOTH_ARIAL_RIGHT:
    case animNumber_t::BOTH_ARIAL_F1:
    case animNumber_t::BOTH_CARTWHEEL_LEFT:
    case animNumber_t::BOTH_CARTWHEEL_RIGHT:
        return true;
    default:
        break;
    }
    return false;
}

extern void CG_PlayerLockedWeaponSpeech(int jumping);
bool PM_ForceJumpingUp(gentity_t* gent)
{
    if (!gent || !gent->client) {
        return false;
    }

    if (gent->NPC) {                                                                       // this is ONLY for the player
        if (player && player->client && player->client->ps.viewEntity == gent->s.number) { // okay to jump if an NPC controlled by the player
        }
        else {
            return false;
        }
    }

    if (!(gent->client->ps.forcePowersActive & Bit(forcePowers_t::FP_LEVITATION)) && gent->client->ps.forceJumpCharge) { // already jumped and let go
        return false;
    }

    if (PM_InSpecialJump(gent->client->ps.legsAnim)) {
        return false;
    }

    if (PM_InKnockDown(&gent->client->ps)) {
        return false;
    }

    if (!gent->s.number && in_camera) { // player can't use force powers in cinematic
        return false;
    }
    if (gent->client->ps.groundEntityNum == ENTITYNUM_NONE // in air
        && (/*(gent->client->ps.waterHeightLevel==waterHeightLevel_t::WHL_SHOULDERS&&gent->client->usercmd.upmove>0) ||*/ (
               Any(gent->client->ps.pm_flags & pmoveFlags_t::PMF_JUMPING) && gent->client->ps.velocity[2] > 0)) // jumped & going up or at water surface
        && gent->client->ps.forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_0   // force-jump capable
        && !Any(gent->client->ps.pm_flags & pmoveFlags_t::PMF_TRIGGER_PUSHED))                                  // not pushed by a trigger
    {
        if (Any(gent->flags & gentityFlags_t::FL_LOCK_PLAYER_WEAPONS)) // yes this locked weapons check also includes force powers, if we need a separate check
                                                                       // later I'll make one
        {
            CG_PlayerLockedWeaponSpeech(true);
            return false;
        }
        return true;
    }
    return false;
}

static void PM_JumpForDir(void)
{
    animNumber_t anim = animNumber_t::BOTH_JUMP1;
    if (pm->cmd.forwardmove > 0) {
        anim = animNumber_t::BOTH_JUMP1;
        pm->ps->pm_flags &= ~pmoveFlags_t::PMF_BACKWARDS_JUMP;
    }
    else if (pm->cmd.forwardmove < 0) {
        anim = animNumber_t::BOTH_JUMPBACK1;
        pm->ps->pm_flags |= pmoveFlags_t::PMF_BACKWARDS_JUMP;
    }
    else if (pm->cmd.rightmove > 0) {
        anim = animNumber_t::BOTH_JUMPRIGHT1;
        pm->ps->pm_flags &= ~pmoveFlags_t::PMF_BACKWARDS_JUMP;
    }
    else if (pm->cmd.rightmove < 0) {
        anim = animNumber_t::BOTH_JUMPLEFT1;
        pm->ps->pm_flags &= ~pmoveFlags_t::PMF_BACKWARDS_JUMP;
    }
    else {
        anim = animNumber_t::BOTH_JUMP1;
        pm->ps->pm_flags &= ~pmoveFlags_t::PMF_BACKWARDS_JUMP;
    }
    if (!PM_InDeathAnim()) {
        PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, anim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE, 100); // Only blend over 100ms
    }
}
extern bool WP_ForcePowerAvailable(gentity_t* self, forcePowers_t forcePower, int overrideAmt);
extern void WP_ForcePowerDrain(gentity_t* self, forcePowers_t forcePower, int overrideAmt);

bool PM_GentCantJump(gentity_t* gent)
{ // FIXME: ugh, hacky, set a flag on NPC or something, please...
    if (gent && gent->client &&
        (gent->client->NPC_class == class_t::CLASS_ATST || gent->client->NPC_class == class_t::CLASS_GONK || gent->client->NPC_class == class_t::CLASS_MARK1 ||
         gent->client->NPC_class == class_t::CLASS_MARK2 || gent->client->NPC_class == class_t::CLASS_MOUSE ||
         gent->client->NPC_class == class_t::CLASS_PROBE || gent->client->NPC_class == class_t::CLASS_PROTOCOL ||
         gent->client->NPC_class == class_t::CLASS_R2D2 || gent->client->NPC_class == class_t::CLASS_R5D2 || gent->client->NPC_class == class_t::CLASS_SEEKER ||
         gent->client->NPC_class == class_t::CLASS_REMOTE || gent->client->NPC_class == class_t::CLASS_SENTRY)) {
        return true;
    }
    return false;
}

static bool PM_CheckJump(void)
{
    // Don't allow jump until all buttons are up
    if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_RESPAWNED)) {
        return false;
    }

    if (PM_InKnockDown(pm->ps) || PM_InRoll(pm->ps)) { // in knockdown
        return false;
    }

    if (PM_GentCantJump(pm->gent)) {
        return false;
    }
    /*
    if ( Any( pm->cmd.buttons & BUTTON_FORCEJUMP ) )
    {
        pm->ps->pm_flags |= pmoveFlags_t::PMF_JUMP_HELD;
    }
    */

#if METROID_JUMP
    if (pm->waterlevel < 3) //|| (pm->ps->waterHeightLevel==waterHeightLevel_t::WHL_SHOULDERS&&pm->cmd.upmove>0) )
    {
        if (pm->ps->gravity > 0) { // can't do this in zero-G
            // FIXME: still able to pogo-jump...
            if (PM_ForceJumpingUp(pm->gent) &&
                Any(pm->ps->pm_flags & pmoveFlags_t::PMF_JUMP_HELD)) //||pm->ps->waterHeightLevel==waterHeightLevel_t::WHL_SHOULDERS) )
            {                                                        // force jumping && holding jump
                /*
                if ( !pm->ps->forceJumpZStart && (pm->ps->waterHeightLevel==waterHeightLevel_t::WHL_SHOULDERS&&pm->cmd.upmove>0) )
                {
                    pm->ps->forceJumpZStart = pm->ps->origin[2];
                }
                */
                float curHeight = pm->ps->origin[2] - pm->ps->forceJumpZStart;
                // check for max force jump level and cap off & cut z vel
                if ((curHeight <= forceJumpHeight[forcePowerLevels_t::FORCE_LEVEL_0] || // still below minimum jump height
                     (pm->ps->forcePower && pm->cmd.upmove >= 10)) &&                   ////still have force power available and still trying to jump up
                    curHeight < forceJumpHeight[pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION]]) // still below maximum jump height
                {                                                                                       // can still go up
                    // FIXME: after a certain amount of time of held jump, play force jump sound and flip if a dir is being held
                    // FIXME: if hit a wall... should we cut velocity or allow them to slide up it?
                    // FIXME: constantly drain force power at a rate by which the usage for maximum height would use up the full cost of force jump
                    if (curHeight > forceJumpHeight[forcePowerLevels_t::FORCE_LEVEL_0]) {     // passed normal jump height  *2?
                        if (!(pm->ps->forcePowersActive & Bit(forcePowers_t::FP_LEVITATION))) // haven't started forcejump yet
                        {
                            // start force jump
                            pm->ps->forcePowersActive |= Bit(forcePowers_t::FP_LEVITATION);
                            if (pm->gent) {
                                G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_BODY, "sound/weapons/force/jump.wav");
                            }
                            // play flip
                            // FIXME: do this only when they stop the jump (below) or when they're just about to hit the peak of the jump
                            if ((pm->cmd.forwardmove || pm->cmd.rightmove) && // pushing in a dir
                                                                              // pm->ps->legsAnim != animNumber_t::BOTH_ARIAL_F1 &&//not already flipping
                                pm->ps->legsAnim != animNumber_t::BOTH_FLIP_F && pm->ps->legsAnim != animNumber_t::BOTH_FLIP_B &&
                                pm->ps->legsAnim != animNumber_t::BOTH_FLIP_R && pm->ps->legsAnim != animNumber_t::BOTH_FLIP_L &&
                                cg.renderingThirdPerson // third person only
                                && !cg.zoomMode)        // not zoomed in
                            {                           // FIXME: this could end up playing twice if the jump is very long...
                                animNumber_t anim = animNumber_t::BOTH_FORCEINAIR1;
                                setAnimParts_t parts = setAnimParts_t::SETANIM_BOTH;

                                if (pm->cmd.forwardmove > 0) {
                                    /*
                                    if ( pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] < forcePowerLevels_t::FORCE_LEVEL_2 )
                                    {
                                        anim = animNumber_t::BOTH_ARIAL_F1;
                                    }
                                    else
                                    */
                                    {
                                        anim = animNumber_t::BOTH_FLIP_F;
                                    }
                                }
                                else if (pm->cmd.forwardmove < 0) {
                                    anim = animNumber_t::BOTH_FLIP_B;
                                }
                                else if (pm->cmd.rightmove > 0) {
                                    anim = animNumber_t::BOTH_FLIP_R;
                                }
                                else if (pm->cmd.rightmove < 0) {
                                    anim = animNumber_t::BOTH_FLIP_L;
                                }
                                if (pm->ps->weaponTime) { // FIXME: really only care if we're in a saber attack anim...
                                    parts = setAnimParts_t::SETANIM_LEGS;
                                }

                                PM_SetAnim(pm, parts, anim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                            }
                            else if (pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] >
                                     forcePowerLevels_t::FORCE_LEVEL_1) { // FIXME: really want to know how far off ground we are, probably...
                                vec3_t facingFwd, facingRight, facingAngles = {0, pm->ps->viewangles[YAW], 0};
                                std::optional<animNumber_t> anim;
                                AngleVectors(facingAngles, facingFwd, facingRight, nullptr);
                                float dotR = DotProduct(facingRight, pm->ps->velocity);
                                float dotF = DotProduct(facingFwd, pm->ps->velocity);
                                if (Q_fabsf(dotR) > Q_fabsf(dotF) * 1.5) {
                                    if (dotR > 150) {
                                        anim = animNumber_t::BOTH_FORCEJUMPRIGHT1;
                                    }
                                    else if (dotR < -150) {
                                        anim = animNumber_t::BOTH_FORCEJUMPLEFT1;
                                    }
                                }
                                else {
                                    if (dotF > 150) {
                                        anim = animNumber_t::BOTH_FORCEJUMP1;
                                    }
                                    else if (dotF < -150) {
                                        anim = animNumber_t::BOTH_FORCEJUMPBACK1;
                                    }
                                }
                                if (anim) {
                                    setAnimParts_t parts = setAnimParts_t::SETANIM_BOTH;
                                    if (pm->ps->weaponTime) { // FIXME: really only care if we're in a saber attack anim...
                                        parts = setAnimParts_t::SETANIM_LEGS;
                                    }

                                    PM_SetAnim(pm, parts, *anim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                                }
                            }
                        }
                        else {
                            if (!pm->ps->legsAnimTimer) { // not in the middle of a legsAnim
                                animNumber_t anim = pm->ps->legsAnim;
                                std::optional<animNumber_t> newAnim;
                                switch (anim) {
                                case animNumber_t::BOTH_FORCEJUMP1:
                                    newAnim = animNumber_t::BOTH_FORCELAND1; // animNumber_t::BOTH_FORCEINAIR1;
                                    break;
                                case animNumber_t::BOTH_FORCEJUMPBACK1:
                                    newAnim = animNumber_t::BOTH_FORCELANDBACK1; // animNumber_t::BOTH_FORCEINAIRBACK1;
                                    break;
                                case animNumber_t::BOTH_FORCEJUMPLEFT1:
                                    newAnim = animNumber_t::BOTH_FORCELANDLEFT1; // animNumber_t::BOTH_FORCEINAIRLEFT1;
                                    break;
                                case animNumber_t::BOTH_FORCEJUMPRIGHT1:
                                    newAnim = animNumber_t::BOTH_FORCELANDRIGHT1; // animNumber_t::BOTH_FORCEINAIRRIGHT1;
                                    break;
                                default:
                                    break;
                                }
                                if (newAnim) {
                                    setAnimParts_t parts = setAnimParts_t::SETANIM_BOTH;
                                    if (pm->ps->weaponTime) { // FIXME: really only care if we're in a saber attack anim...
                                        parts = setAnimParts_t::SETANIM_LEGS;
                                    }

                                    PM_SetAnim(pm, parts, *newAnim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                                }
                            }
                        }
                    }

                    // need to scale this down, start with height velocity (based on max force jump height) and scale down to regular jump vel
                    pm->ps->velocity[2] = (forceJumpHeight[pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION]] - curHeight) /
                                          forceJumpHeight[pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION]] *
                                          forceJumpStrength[pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION]]; // JUMP_VELOCITY;
                    pm->ps->velocity[2] /= 10;
                    pm->ps->velocity[2] += JUMP_VELOCITY;
                    pm->ps->pm_flags |= pmoveFlags_t::PMF_JUMP_HELD;
                }
                else if (curHeight > forceJumpHeight[forcePowerLevels_t::FORCE_LEVEL_0] &&
                         curHeight < forceJumpHeight[pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION]] -
                                         forceJumpHeight[forcePowerLevels_t::FORCE_LEVEL_0]) { // still have some headroom, don't totally stop it
                    if (pm->ps->velocity[2] > JUMP_VELOCITY) {
                        pm->ps->velocity[2] = JUMP_VELOCITY;
                    }
                }
                else {
                    pm->ps->velocity[2] = 0;
                }
                pm->cmd.upmove = 0;
                return false;
            }
        }
    }

#endif

    // Not jumping
    if (pm->cmd.upmove < 10) {
        return false;
    }

    // must wait for jump to be released
    if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_JUMP_HELD)) {
        // clear upmove so cmdscale doesn't lower running speed
        pm->cmd.upmove = 0;
        return false;
    }

    if (pm->ps->gravity <= 0) { // in low grav, you push in the dir you're facing as long as there is something behind you to shove off of
        vec3_t forward, back;
        trace_t trace;

        AngleVectors(pm->ps->viewangles, forward, nullptr, nullptr);
        VectorMA(pm->ps->origin, -8, forward, back);
        pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, back, pm->ps->clientNum, pm->tracemask, EG2_Collision::G2_NOCOLLIDE, 0);

        pm->cmd.upmove = 0;

        if (trace.fraction < 1.0f) {
            VectorMA(pm->ps->velocity, JUMP_VELOCITY / 2, forward, pm->ps->velocity);
            // FIXME: kicking off wall anim?  At least check what anim we're in?
            PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_FORCEJUMP1,
                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_RESTART);
        }
        else { // else no surf close enough to push off of
            return false;
        }

        if (pm->ps->groundEntityNum == ENTITYNUM_NONE) { // need to set some things and return
            // Jumping
            pm->ps->forceJumpZStart = 0;
            pml.groundPlane = false;
            pml.walking = false;
            pm->ps->pm_flags |= (pmoveFlags_t::PMF_JUMPING | pmoveFlags_t::PMF_JUMP_HELD);
            pm->ps->groundEntityNum = ENTITYNUM_NONE;
            pm->ps->jumpZStart = pm->ps->origin[2];

            if (pm->gent) {
                if (!Q3_TaskIDPending(pm->gent, taskID_t::TID_CHAN_VOICE)) {
                    PM_AddEvent(entity_event_t::EV_JUMP);
                }
            }
            else {
                PM_AddEvent(entity_event_t::EV_JUMP);
            }

            return true;
        } // else no surf close enough to push off of
    }
    else if (pm->cmd.upmove > 0 && pm->waterlevel < 2 && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_0 &&
             !Any(pm->ps->pm_flags & pmoveFlags_t::PMF_JUMP_HELD)
             //&& !PM_InKnockDown( pm->ps )
             && pm->gent && WP_ForcePowerAvailable(pm->gent, forcePowers_t::FP_LEVITATION, 0) &&
             ((pm->ps->clientNum && !PM_ControlledByPlayer()) ||
              ((!pm->ps->clientNum || PM_ControlledByPlayer()) && cg.renderingThirdPerson && !cg.zoomMode &&
               !Any(pm->gent->flags & gentityFlags_t::FL_LOCK_PLAYER_WEAPONS)))) // yes this locked weapons check also includes force powers, if we need a
                                                                                 // separate check later I'll make one
    {
        if (pm->gent->NPC && pm->gent->NPC->rank != rank_t::RANK_CREWMAN &&
            pm->gent->NPC->rank <= rank_t::RANK_LT_JG) { // reborn who are not acrobats can't do any of these acrobatics
        }
        else if (pm->ps->groundEntityNum != ENTITYNUM_NONE) { // on the ground
            // check for left-wall and right-wall special jumps
            std::optional<animNumber_t> anim;
            float vertPush = 0;
            if (pm->cmd.rightmove > 0 && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1) { // strafing right
                if (pm->cmd.forwardmove > 0) {                                                                                        // wall-run
                    vertPush = forceJumpStrength[forcePowerLevels_t::FORCE_LEVEL_2] / 2.0f;
                    anim = animNumber_t::BOTH_WALL_RUN_RIGHT;
                }
                else if (pm->cmd.forwardmove == 0) { // wall-flip
                    vertPush = forceJumpStrength[forcePowerLevels_t::FORCE_LEVEL_2] / 2.25f;
                    anim = animNumber_t::BOTH_WALL_FLIP_RIGHT;
                }
            }
            else if (pm->cmd.rightmove < 0 && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1) { // strafing left
                if (pm->cmd.forwardmove > 0) {                                                                                             // wall-run
                    vertPush = forceJumpStrength[forcePowerLevels_t::FORCE_LEVEL_2] / 2.0f;
                    anim = animNumber_t::BOTH_WALL_RUN_LEFT;
                }
                else if (pm->cmd.forwardmove == 0) { // wall-flip
                    vertPush = forceJumpStrength[forcePowerLevels_t::FORCE_LEVEL_2] / 2.25f;
                    anim = animNumber_t::BOTH_WALL_FLIP_LEFT;
                }
            }
            else if (pm->ps->clientNum && !PM_ControlledByPlayer() && pm->cmd.forwardmove > 0 &&
                     pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1) { // run up wall, flip backwards
                if (VectorLengthSquared(pm->ps->velocity) >
                    40000 /*200*200*/) { // have to be moving... FIXME: make sure it's opposite the wall... or at least forward?
                    vertPush = forceJumpStrength[forcePowerLevels_t::FORCE_LEVEL_2] / 2.25f;
                    anim = animNumber_t::BOTH_WALL_FLIP_BACK1;
                }
            }
            else if (pm->cmd.forwardmove < 0 && !Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)) // pm->ps->clientNum &&
            {                                                                                         // double-tap back-jump does backflip
                if (pm->ps->velocity[2] >= 0) {                                                       // must be going up already
                    vertPush = JUMP_VELOCITY;
                    anim = PM_PickAnim(pm->gent, animNumber_t::BOTH_FLIP_BACK1, animNumber_t::BOTH_FLIP_BACK3);
                }
            }
            else if (VectorLengthSquared(pm->ps->velocity) < 256 /*16 squared*/) { // not moving
                if (pm->ps->weapon == weapon_t::WP_SABER && Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK) &&
                    pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_2) {
                    /*
                    //Only tavion does these now
                    if ( pm->ps->clientNum && Q_irand( 0, 1 ) )
                    {//butterfly... FIXME: does direction matter?
                        vertPush = JUMP_VELOCITY;
                        if ( Q_irand( 0, 1 ) )
                        {
                            anim = animNumber_t::BOTH_BUTTERFLY_LEFT;
                        }
                        else
                        {
                            anim = animNumber_t::BOTH_BUTTERFLY_RIGHT;
                        }
                    }
                    else
                    */
                    if (pm->ps->clientNum && !PM_ControlledByPlayer()) // NOTE: pretty much useless, so player never does these
                    {                                                  // jump-spin FIXME: does direction matter?
                        vertPush = forceJumpStrength[forcePowerLevels_t::FORCE_LEVEL_2] / 1.5f;
                        anim = Q_irand(animNumber_t::BOTH_FJSS_TR_BL, animNumber_t::BOTH_FJSS_TL_BR);
                    }
                }
            }

            if (anim && PM_HasAnimation(pm->gent, *anim)) {
                vec3_t fwd, right, traceto, mins = {pm->mins[0], pm->mins[1], 0}, maxs = {pm->maxs[0], pm->maxs[1], 24},
                                            fwdAngles = {0, pm->ps->viewangles[YAW], 0};
                trace_t trace;
                bool doTrace = false;
                contentsFlags_t contents = contentsFlags_t::CONTENTS_SOLID;

                AngleVectors(fwdAngles, fwd, right, nullptr);

                // trace-check for a wall, if necc.
                switch (*anim) {
                case animNumber_t::BOTH_WALL_FLIP_LEFT:
                    // contents |= contentsFlags_t::CONTENTS_BODY;
                    // NOTE: purposely falls through to next case!
                case animNumber_t::BOTH_WALL_RUN_LEFT:
                    doTrace = true;
                    VectorMA(pm->ps->origin, -16, right, traceto);
                    break;

                case animNumber_t::BOTH_WALL_FLIP_RIGHT:
                    // contents |= contentsFlags_t::CONTENTS_BODY;
                    // NOTE: purposely falls through to next case!
                case animNumber_t::BOTH_WALL_RUN_RIGHT:
                    doTrace = true;
                    VectorMA(pm->ps->origin, 16, right, traceto);
                    break;

                case animNumber_t::BOTH_WALL_FLIP_BACK1:
                    // contents |= contentsFlags_t::CONTENTS_BODY;
                    doTrace = true;
                    VectorMA(pm->ps->origin, 16, fwd, traceto);
                    break;
                default:
                    break;
                }

                vec3_t idealNormal;
                if (doTrace) {
                    // FIXME: all these jump ones should check for head clearance
                    pm->trace(&trace, pm->ps->origin, mins, maxs, traceto, pm->ps->clientNum, contents, EG2_Collision::G2_NOCOLLIDE, 0);
                    VectorSubtract(pm->ps->origin, traceto, idealNormal);
                    VectorNormalize(idealNormal);
                    if (anim == animNumber_t::BOTH_WALL_FLIP_LEFT) { // sigh.. check for bottomless pit to the right
                        trace_t trace2;
                        vec3_t start;
                        VectorMA(pm->ps->origin, 128, right, traceto);
                        pm->trace(&trace2, pm->ps->origin, mins, maxs, traceto, pm->ps->clientNum, contents, EG2_Collision::G2_NOCOLLIDE, 0);
                        if (!trace2.allsolid && !trace2.startsolid) {
                            VectorCopy(trace2.endpos, traceto);
                            VectorCopy(traceto, start);
                            traceto[2] -= 384;
                            pm->trace(&trace2, start, mins, maxs, traceto, pm->ps->clientNum, contents, EG2_Collision::G2_NOCOLLIDE, 0);
                            if (!trace2.allsolid && !trace2.startsolid && trace2.fraction >= 1.0f) { // bottomless pit!
                                trace.fraction = 1.0f;                                               // way to stop it from doing the side-flip
                            }
                        }
                    }
                    else if (anim == animNumber_t::BOTH_WALL_FLIP_RIGHT) { // sigh.. check for bottomless pit to the left
                        trace_t trace2;
                        vec3_t start;
                        VectorMA(pm->ps->origin, -128, right, traceto);
                        pm->trace(&trace2, pm->ps->origin, mins, maxs, traceto, pm->ps->clientNum, contents, EG2_Collision::G2_NOCOLLIDE, 0);
                        if (!trace2.allsolid && !trace2.startsolid) {
                            VectorCopy(trace2.endpos, traceto);
                            VectorCopy(traceto, start);
                            traceto[2] -= 384;
                            pm->trace(&trace2, start, mins, maxs, traceto, pm->ps->clientNum, contents, EG2_Collision::G2_NOCOLLIDE, 0);
                            if (!trace2.allsolid && !trace2.startsolid && trace2.fraction >= 1.0f) { // bottomless pit!
                                trace.fraction = 1.0f;                                               // way to stop it from doing the side-flip
                            }
                        }
                    }
                    else if (anim == animNumber_t::BOTH_WALL_FLIP_BACK1) { // sigh.. check for bottomless pit to the rear
                        trace_t trace2;
                        vec3_t start;
                        VectorMA(pm->ps->origin, -128, fwd, traceto);
                        pm->trace(&trace2, pm->ps->origin, mins, maxs, traceto, pm->ps->clientNum, contents, EG2_Collision::G2_NOCOLLIDE, 0);
                        if (!trace2.allsolid && !trace2.startsolid) {
                            VectorCopy(trace2.endpos, traceto);
                            VectorCopy(traceto, start);
                            traceto[2] -= 384;
                            pm->trace(&trace2, start, mins, maxs, traceto, pm->ps->clientNum, contents, EG2_Collision::G2_NOCOLLIDE, 0);
                            if (!trace2.allsolid && !trace2.startsolid && trace2.fraction >= 1.0f) { // bottomless pit!
                                trace.fraction = 1.0f;                                               // way to stop it from doing the side-flip
                            }
                        }
                    }
                }
                gentity_t* traceEnt = &g_entities[trace.entityNum];

                if (!doTrace || (trace.fraction < 1.0f && ((trace.entityNum < ENTITYNUM_WORLD && traceEnt && traceEnt->s.solid != SOLID_BMODEL) ||
                                                           DotProduct(trace.plane.normal, idealNormal) > 0.7))) { // there is a wall there
                    if ((anim != animNumber_t::BOTH_WALL_RUN_LEFT && anim != animNumber_t::BOTH_WALL_RUN_RIGHT) ||
                        trace.plane.normal[2] == 0.0f) { // wall-runs can only run on perfectly flat walls, sorry.
                        // move me to side
                        if (anim == animNumber_t::BOTH_WALL_FLIP_LEFT) {
                            pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
                            VectorMA(pm->ps->velocity, 150, right, pm->ps->velocity);
                        }
                        else if (anim == animNumber_t::BOTH_WALL_FLIP_RIGHT) {
                            pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
                            VectorMA(pm->ps->velocity, -150, right, pm->ps->velocity);
                        }
                        else if (anim == animNumber_t::BOTH_FLIP_BACK1 || anim == animNumber_t::BOTH_FLIP_BACK2 || anim == animNumber_t::BOTH_FLIP_BACK3 ||
                                 anim == animNumber_t::BOTH_WALL_FLIP_BACK1) {
                            pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
                            VectorMA(pm->ps->velocity, -150, fwd, pm->ps->velocity);
                        }
                        // kick if jumping off an ent
                        if (doTrace && anim != animNumber_t::BOTH_WALL_RUN_LEFT && anim != animNumber_t::BOTH_WALL_RUN_RIGHT) {
                            if (pm->gent && trace.entityNum < ENTITYNUM_WORLD) {
                                if (traceEnt && traceEnt->client && traceEnt->health > 0 && traceEnt->takedamage) { // push them away and do pain
                                    vec3_t oppDir;
                                    float strength = VectorNormalize2(pm->ps->velocity, oppDir);
                                    VectorScale(oppDir, -1, oppDir);
                                    // FIXME: need knockdown anim
                                    G_Damage(traceEnt, pm->gent, pm->gent, oppDir, traceEnt->currentOrigin, 10,
                                             damageFlags_t::DAMAGE_NO_ARMOR | damageFlags_t::DAMAGE_NO_HIT_LOC | damageFlags_t::DAMAGE_NO_KNOCKBACK,
                                             meansOfDeath_t::MOD_MELEE);
                                    NPC_SetAnim(traceEnt, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_KNOCKDOWN2,
                                                setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                                    G_Throw(traceEnt, oppDir, strength);
                                    G_Sound(traceEnt, G_SoundIndex(Q_Format("sound/weapons/melee/punch{}", Q_irand(1, 4)).c_str()));
                                }
                            }
                        }
                        // up
                        if (vertPush) {
                            pm->ps->velocity[2] = vertPush;
                        }
                        // animate me
                        setAnimParts_t parts = setAnimParts_t::SETANIM_LEGS;
                        if (anim == animNumber_t::BOTH_BUTTERFLY_LEFT || anim == animNumber_t::BOTH_BUTTERFLY_RIGHT || anim == animNumber_t::BOTH_FJSS_TR_BL ||
                            anim == animNumber_t::BOTH_FJSS_TL_BR) {
                            parts = setAnimParts_t::SETANIM_BOTH;
                            pm->cmd.buttons &= ~buttonFlags_t::BUTTON_ATTACK;
                            pm->ps->saberMove = saberMoveName_t::LS_NONE;
                            pm->gent->client->saberTrail.inAction = true; // FIXME: reset this when done!
                            pm->gent->client->saberTrail.duration = 300;  // FIXME: reset this when done!
                        }
                        else if (!pm->ps->weaponTime) {
                            parts = setAnimParts_t::SETANIM_BOTH;
                        }
                        PM_SetAnim(pm, parts, *anim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD, 0);
                        if (anim == animNumber_t::BOTH_BUTTERFLY_LEFT || anim == animNumber_t::BOTH_BUTTERFLY_RIGHT || anim == animNumber_t::BOTH_FJSS_TR_BL ||
                            anim == animNumber_t::BOTH_FJSS_TL_BR) {
                            pm->ps->weaponTime = pm->ps->torsoAnimTimer;
                        }
                        else if (anim == animNumber_t::BOTH_WALL_FLIP_LEFT || anim == animNumber_t::BOTH_WALL_FLIP_RIGHT ||
                                 anim == animNumber_t::BOTH_WALL_FLIP_BACK1) { // let us do some more moves after this
                            pm->ps->saberAttackChainCount = 0;
                        }
                        pm->ps->forceJumpZStart = pm->ps->origin[2]; // so we don't take damage if we land at same height
                        pm->ps->pm_flags |= (pmoveFlags_t::PMF_JUMPING | pmoveFlags_t::PMF_SLOW_MO_FALL);
                        pm->cmd.upmove = 0;
                        G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_BODY, "sound/weapons/force/jump.wav");
                        WP_ForcePowerDrain(pm->gent, forcePowers_t::FP_LEVITATION, 0);
                    }
                }
            }
        }
        else { // in the air
            animNumber_t legsAnim = pm->ps->legsAnim;
            if (legsAnim == animNumber_t::BOTH_WALL_RUN_LEFT || legsAnim == animNumber_t::BOTH_WALL_RUN_RIGHT) { // running on a wall
                vec3_t right, traceto, mins = {pm->mins[0], pm->mins[0], 0}, maxs = {pm->maxs[0], pm->maxs[0], 24}, fwdAngles = {0, pm->ps->viewangles[YAW], 0};
                trace_t trace;
                std::optional<animNumber_t> anim;

                AngleVectors(fwdAngles, nullptr, right, nullptr);

                if (legsAnim == animNumber_t::BOTH_WALL_RUN_LEFT) {
                    if (pm->ps->legsAnimTimer > 400) { // not at the end of the anim
                        float animLen = PM_AnimLength(pm->gent->client->clientInfo.animFileIndex, animNumber_t::BOTH_WALL_RUN_LEFT);
                        if (pm->ps->legsAnimTimer < animLen - 400) { // not at start of anim
                            VectorMA(pm->ps->origin, -16, right, traceto);
                            anim = animNumber_t::BOTH_WALL_RUN_LEFT_FLIP;
                        }
                    }
                }
                else if (legsAnim == animNumber_t::BOTH_WALL_RUN_RIGHT) {
                    if (pm->ps->legsAnimTimer > 400) { // not at the end of the anim
                        float animLen = PM_AnimLength(pm->gent->client->clientInfo.animFileIndex, animNumber_t::BOTH_WALL_RUN_RIGHT);
                        if (pm->ps->legsAnimTimer < animLen - 400) { // not at start of anim
                            VectorMA(pm->ps->origin, 16, right, traceto);
                            anim = animNumber_t::BOTH_WALL_RUN_RIGHT_FLIP;
                        }
                    }
                }
                if (anim) {
                    pm->trace(&trace, pm->ps->origin, mins, maxs, traceto, pm->ps->clientNum, contentsFlags_t::CONTENTS_SOLID | contentsFlags_t::CONTENTS_BODY,
                              EG2_Collision::G2_NOCOLLIDE, 0);
                    if (trace.fraction < 1.0f) { // flip off wall
                        if (anim == animNumber_t::BOTH_WALL_RUN_LEFT_FLIP) {
                            pm->ps->velocity[0] *= 0.5f;
                            pm->ps->velocity[1] *= 0.5f;
                            VectorMA(pm->ps->velocity, 150, right, pm->ps->velocity);
                        }
                        else if (anim == animNumber_t::BOTH_WALL_RUN_RIGHT_FLIP) {
                            pm->ps->velocity[0] *= 0.5f;
                            pm->ps->velocity[1] *= 0.5f;
                            VectorMA(pm->ps->velocity, -150, right, pm->ps->velocity);
                        }
                        setAnimParts_t parts = setAnimParts_t::SETANIM_LEGS;
                        if (!pm->ps->weaponTime) {
                            parts = setAnimParts_t::SETANIM_BOTH;
                        }
                        PM_SetAnim(pm, parts, *anim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD, 0);
                        // FIXME: do damage to traceEnt, like above?
                        pm->ps->pm_flags |= pmoveFlags_t::PMF_JUMPING | pmoveFlags_t::PMF_SLOW_MO_FALL;
                        pm->cmd.upmove = 0;
                    }
                }
                if (pm->cmd.upmove != 0) { // jump failed, so don't try to do normal jump code, just return
                    return false;
                }
            }
            /*
            else if ( pm->cmd.forwardmove < 0
                && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1
                && !Any(pm->ps->pm_flags&pmoveFlags_t::PMF_JUMP_HELD) //not holding jump
                && (level.time - pm->ps->lastOnGround) <= 500 //just jumped
                )//&& !Any(pm->cmd.buttons&BUTTON_ATTACK) )
            {//double-tap back-jump does backflip
                vec3_t fwd, fwdAngles = {0, pm->ps->viewangles[YAW], 0};

                AngleVectors( fwdAngles, fwd, nullptr, nullptr );
                pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
                VectorMA( pm->ps->velocity, -150, fwd, pm->ps->velocity );
                //pm->ps->velocity[2] = JUMP_VELOCITY;
                setAnimParts_t parts = setAnimParts_t::SETANIM_LEGS;
                if ( !pm->ps->weaponTime )
                {
                    parts = setAnimParts_t::SETANIM_BOTH;
                }
                PM_SetAnim( pm, parts, PM_PickAnim( pm->gent, animNumber_t::BOTH_FLIP_BACK1, animNumber_t::BOTH_FLIP_BACK3 ),
            setAnimFlags_t::SETANIM_FLAG_OVERRIDE|setAnimFlags_t::SETANIM_FLAG_HOLD, 0 ); pm->ps->forceJumpZStart = pm->ps->origin[2];//so we don't take damage
            if we land at same height pm->ps->pm_flags |= pmoveFlags_t::PMF_JUMPING|pmoveFlags_t::PMF_SLOW_MO_FALL; pm->cmd.upmove = 0; G_SoundOnEnt( pm->gent,
            soundChannel_t::CHAN_BODY, "sound/weapons/force/jump.wav" ); WP_ForcePowerDrain( pm->gent, forcePowers_t::FP_LEVITATION, 0 );
            }
            */
            else if (pm->cmd.forwardmove > 0 // pushing forward
                     && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1 &&
                     (level.time - pm->ps->lastOnGround) <= 500                                                          // just jumped
                     && (pm->ps->legsAnim == animNumber_t::BOTH_JUMP1 || pm->ps->legsAnim == animNumber_t::BOTH_INAIR1)) // not in a flip or spin or anything
            {                                                                                                            // run up wall, flip backwards
                // FIXME: have to be moving... make sure it's opposite the wall... or at least forward?
                if (PM_HasAnimation(pm->gent, animNumber_t::BOTH_WALL_FLIP_BACK1)) {
                    vec3_t fwd, traceto, mins = {pm->mins[0], pm->mins[1], 0}, maxs = {pm->maxs[0], pm->maxs[1], 24},
                                         fwdAngles = {0, pm->ps->viewangles[YAW], 0};
                    trace_t trace;
                    vec3_t idealNormal;

                    AngleVectors(fwdAngles, fwd, nullptr, nullptr);
                    VectorMA(pm->ps->origin, 32, fwd, traceto);

                    pm->trace(&trace, pm->ps->origin, mins, maxs, traceto, pm->ps->clientNum, contentsFlags_t::CONTENTS_SOLID, EG2_Collision::G2_NOCOLLIDE,
                              0); // FIXME: clip brushes too?
                    VectorSubtract(pm->ps->origin, traceto, idealNormal);
                    VectorNormalize(idealNormal);
                    gentity_t* traceEnt = &g_entities[trace.entityNum];

                    if (trace.fraction < 1.0f && ((trace.entityNum < ENTITYNUM_WORLD && traceEnt && traceEnt->s.solid != SOLID_BMODEL) ||
                                                  DotProduct(trace.plane.normal, idealNormal) > 0.7)) { // there is a wall there
                        pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
                        VectorMA(pm->ps->velocity, -150, fwd, pm->ps->velocity);
                        // pm->ps->velocity[2] = forceJumpStrength[forcePowerLevels_t::FORCE_LEVEL_2]/2.25f;
                        // animate me
                        setAnimParts_t parts = setAnimParts_t::SETANIM_LEGS;
                        if (!pm->ps->weaponTime) {
                            parts = setAnimParts_t::SETANIM_BOTH;
                        }
                        PM_SetAnim(pm, parts, animNumber_t::BOTH_WALL_FLIP_BACK1, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD, 0);
                        pm->ps->forceJumpZStart = pm->ps->origin[2]; // so we don't take damage if we land at same height
                        pm->ps->pm_flags |= pmoveFlags_t::PMF_JUMPING | pmoveFlags_t::PMF_SLOW_MO_FALL;
                        pm->cmd.upmove = 0;
                        G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_BODY, "sound/weapons/force/jump.wav");
                        WP_ForcePowerDrain(pm->gent, forcePowers_t::FP_LEVITATION, 0);
                    }
                }
            }
            else {
                // FIXME: if in a butterfly, kick people away?
            }
        }
    }

    if (pm->gent && pm->cmd.upmove > 0 && pm->ps->weapon == weapon_t::WP_SABER &&
        (pm->ps->weaponTime > 0 || Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)) &&
        ((pm->ps->clientNum && !PM_ControlledByPlayer()) ||
         ((!pm->ps->clientNum || PM_ControlledByPlayer()) && cg.renderingThirdPerson && !cg.zoomMode))) { // okay, we just jumped and we're in an attack
        if (!PM_RollingAnim(pm->ps->legsAnim) && !PM_InKnockDown(pm->ps) && !PM_InDeathAnim() && !PM_PainAnim(pm->ps->torsoAnim) &&
            !PM_FlippingAnim(pm->ps->legsAnim) && !PM_SpinningAnim(pm->ps->legsAnim) && !PM_SaberInSpecialAttack(pm->ps->torsoAnim) &&
            (PM_SaberInTransitionAny(pm->ps->saberMove) || PM_SaberInAttack(pm->ps->saberMove)) &&
            PM_InAnimForSaberMove(pm->ps->torsoAnim, pm->ps->saberMove)) { // not in an anim we shouldn't interrupt
            // see if it's not too late to start a special jump-attack
            float animLength = PM_AnimLength(g_entities[pm->ps->clientNum].client->clientInfo.animFileIndex, pm->ps->torsoAnim);
            if (animLength - pm->ps->torsoAnimTimer < 500) { // just started the saberMove
                // check for special-case jump attacks
                if (pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_2 ||
                    pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_5) // using medium attacks or Tavion)
                {                                                                // using medium attacks
                    if (pm->gent->enemy && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1 // can force jump
                        && !Any(pm->gent->flags & gentityFlags_t::FL_LOCK_PLAYER_WEAPONS) // yes this locked weapons check also includes force powers, if we
                                                                                          // need a separate check later I'll make one
                        && (pm->ps->groundEntityNum != ENTITYNUM_NONE || level.time - pm->ps->lastOnGround <= 500)) // on ground or just jumped
                    {                                                                                               // flip over-forward down-attack
                        if (((!pm->ps->clientNum || PM_ControlledByPlayer()) && pm->cmd.forwardmove >= 0 && !pm->cmd.rightmove) ||
                            (pm->gent->NPC && (pm->gent->NPC->rank == rank_t::RANK_CREWMAN ||
                                               pm->gent->NPC->rank >= rank_t::RANK_LT))) { // only player or acrobat or boss and higher can do this
                            vec3_t fwdAngles = {0, pm->ps->viewangles[YAW], 0};
                            if (pm->gent->enemy->health > 0 && pm->gent->enemy->maxs[2] > 12 &&
                                (!pm->gent->enemy->client || !PM_InKnockDownOnGround(&pm->gent->enemy->client->ps)) &&
                                DistanceSquared(pm->gent->currentOrigin, pm->gent->enemy->currentOrigin) < 10000 &&
                                InFront(pm->gent->enemy->currentOrigin, pm->gent->currentOrigin, fwdAngles,
                                        0.3f)) { // enemy must be alive, not low to ground, close and in front
                                PM_SetSaberMove(PM_SaberFlipOverAttackMove());
                            }
                        }
                    }
                }
                else if (pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_3 ||
                         (pm->gent->client->NPC_class == class_t::CLASS_DESANN && !Q_irand(0, 1))) {                 // using strong attacks
                    if (pm->cmd.forwardmove > 0                                                                      // going forward
                        && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1 // can force jump
                        && !Any(pm->gent->flags & gentityFlags_t::FL_LOCK_PLAYER_WEAPONS) // yes this locked weapons check also includes force powers, if we
                                                                                          // need a separate check later I'll make one
                        && (pm->ps->legsAnim == animNumber_t::BOTH_STAND2 || pm->ps->legsAnim == animNumber_t::BOTH_SABERFAST_STANCE ||
                            pm->ps->legsAnim == animNumber_t::BOTH_SABERSLOW_STANCE ||
                            level.time - pm->ps->lastStationary <= 500) // standing or just started moving
                        && (pm->ps->groundEntityNum != ENTITYNUM_NONE || (pm->ps->clientNum && !PM_ControlledByPlayer() &&
                                                                          level.time - pm->ps->lastOnGround <= 500))) // on ground or just jumped if non-player
                    {                                                                                                 // strong attack: jump-hack
                        if ((!pm->ps->clientNum || PM_ControlledByPlayer()) ||
                            (pm->gent && pm->gent->NPC &&
                             (pm->gent->NPC->rank == rank_t::RANK_CREWMAN ||
                              pm->gent->NPC->rank >= rank_t::RANK_LT))) { // only player or acrobat or boss and higher can do this
                            PM_SetSaberMove(PM_SaberJumpAttackMove());
                        }
                    }
                }
            }
        }
    }
    if (pm->ps->groundEntityNum == ENTITYNUM_NONE) {
        return false;
    }
    if (pm->cmd.upmove > 0) { // no special jumps
        /*
        gentity_t *groundEnt = &g_entities[pm->ps->groundEntityNum];
        if ( groundEnt && groundEnt->NPC )
        {//Can't jump off of someone's head
            return false;
        }
        */

        pm->ps->velocity[2] = JUMP_VELOCITY;
        pm->ps->forceJumpZStart = pm->ps->origin[2]; // so we don't take damage if we land at same height
        pm->ps->pm_flags |= pmoveFlags_t::PMF_JUMPING;
    }

    if (d_JediAI->integer) {
        if (pm->ps->clientNum && pm->ps->weapon == weapon_t::WP_SABER) {
            Q_Println("jumping");
        }
    }
    // Jumping
    pml.groundPlane = false;
    pml.walking = false;
    pm->ps->pm_flags |= pmoveFlags_t::PMF_JUMP_HELD;
    pm->ps->groundEntityNum = ENTITYNUM_NONE;
    pm->ps->jumpZStart = pm->ps->origin[2];

    if (pm->gent) {
        if (!Q3_TaskIDPending(pm->gent, taskID_t::TID_CHAN_VOICE)) {
            PM_AddEvent(entity_event_t::EV_JUMP);
        }
    }
    else {
        PM_AddEvent(entity_event_t::EV_JUMP);
    }

    // Set the animations
    if (pm->ps->gravity > 0 && !PM_InSpecialJump(pm->ps->legsAnim) && !PM_InGetUp(pm->ps)) {
        PM_JumpForDir();
    }

    return true;
}

/*
=============
PM_CheckWaterJump
=============
*/
static bool PM_CheckWaterJump(void)
{
    vec3_t spot;
    contentsFlags_t cont;
    vec3_t flatforward;

    if (pm->ps->pm_time) {
        return false;
    }

    if (pm->cmd.forwardmove <= 0 && pm->cmd.upmove <= 0) { // they must not want to get out?
        return false;
    }
    // check for water jump
    if (pm->waterlevel != 2) {
        return false;
    }

    if (Any(pm->watertype & contentsFlags_t::CONTENTS_LADDER)) {
        if (pm->ps->velocity[2] <= 0)
            return false;
    }

    flatforward[0] = pml.forward[0];
    flatforward[1] = pml.forward[1];
    flatforward[2] = 0;
    VectorNormalize(flatforward);

    VectorMA(pm->ps->origin, 30, flatforward, spot);
    spot[2] += 24;
    cont = pm->pointcontents(spot, pm->ps->clientNum);
    if (!Any(cont & contentsFlags_t::CONTENTS_SOLID)) {
        return false;
    }

    spot[2] += 16;
    cont = pm->pointcontents(spot, pm->ps->clientNum);
    if (Any(cont & (contentsFlags_t::CONTENTS_SOLID | contentsFlags_t::CONTENTS_PLAYERCLIP | contentsFlags_t::CONTENTS_WATER | contentsFlags_t::CONTENTS_SLIME |
                    contentsFlags_t::CONTENTS_LAVA | contentsFlags_t::CONTENTS_BODY))) {
        return false;
    }

    // jump out of water
    VectorScale(pml.forward, 200, pm->ps->velocity);
    pm->ps->velocity[2] = 350 + ((pm->ps->waterheight - pm->ps->origin[2]) * 2);

    pm->ps->pm_flags |= pmoveFlags_t::PMF_TIME_WATERJUMP;
    pm->ps->pm_time = 2000;

    return true;
}

//============================================================================

/*
===================
PM_WaterJumpMove

Flying out of the water
===================
*/
static void PM_WaterJumpMove(void)
{
    // waterjump has no control, but falls

    PM_StepSlideMove(1);

    pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
    if (pm->ps->velocity[2] < 0) {
        // cancel as soon as we are falling down again
        pm->ps->pm_flags &= ~pmoveFlags_t::PMF_ALL_TIMES;
        pm->ps->pm_time = 0;
    }
}

/*
===================
PM_WaterMove

===================
*/
static void PM_WaterMove(void)
{
    int i;
    vec3_t wishvel;
    float wishspeed;
    vec3_t wishdir;
    float scale;
    float vel;

    if (PM_CheckWaterJump()) {
        PM_WaterJumpMove();
        return;
    }
    else if (pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_0 && pm->waterlevel < 3) {
        if (PM_CheckJump()) {
            // jumped away
            return;
        }
    }
#if 0
	// jump = head for surface
	if ( pm->cmd.upmove >= 10 ) {
		if (pm->ps->velocity[2] > -300) {
			if ( pm->watertype == contentsFlags_t::CONTENTS_WATER ) {
				pm->ps->velocity[2] = 100;
			} else if (pm->watertype == contentsFlags_t::CONTENTS_SLIME) {
				pm->ps->velocity[2] = 80;
			} else {
				pm->ps->velocity[2] = 50;
			}
		}
	}
#endif
    PM_Friction();

    scale = PM_CmdScale(&pm->cmd);
    //
    // user intentions
    //
    if (!scale) {
        wishvel[0] = 0;
        wishvel[1] = 0;
        if (Any(pm->watertype & contentsFlags_t::CONTENTS_LADDER)) {
            wishvel[2] = 0;
        }
        else {
            wishvel[2] = -60; // sink towards bottom
        }
    }
    else {
        for (i = 0; i < 3; i++) {
            wishvel[i] = scale * pml.forward[i] * pm->cmd.forwardmove + scale * pml.right[i] * pm->cmd.rightmove;
        }
        wishvel[2] += scale * pm->cmd.upmove;
        if (!Any(pm->watertype & contentsFlags_t::CONTENTS_LADDER)) // ladder
        {
            float depth = (pm->ps->origin[2] + pm->gent->client->standheight) - pm->ps->waterheight;
            if (depth >= 12) {     // too high!
                wishvel[2] -= 120; // sink towards bottom
                if (wishvel[2] > 0) {
                    wishvel[2] = 0;
                }
            }
            else if (pm->ps->waterHeightLevel >= waterHeightLevel_t::WHL_UNDER) //! depth && pm->waterlevel == 3 )
            {
            }
            else if (depth < 12) { // still deep
                wishvel[2] -= 60;  // sink towards bottom
                if (wishvel[2] > 30) {
                    wishvel[2] = 30;
                }
            }
        }
    }

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);

    if (Any(pm->watertype & contentsFlags_t::CONTENTS_LADDER)) // ladder
    {
        if (wishspeed > pm->ps->speed * pm_ladderScale) {
            wishspeed = pm->ps->speed * pm_ladderScale;
        }
        PM_Accelerate(wishdir, wishspeed, pm_flyaccelerate);
    }
    else {
        if (pm->ps->gravity < 0) { // float up
            pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
        }
        if (wishspeed > pm->ps->speed * pm_swimScale) {
            wishspeed = pm->ps->speed * pm_swimScale;
        }
        PM_Accelerate(wishdir, wishspeed, pm_wateraccelerate);
    }

    // make sure we can go up slopes easily under water
    if (pml.groundPlane && DotProduct(pm->ps->velocity, pml.groundTrace.plane.normal) < 0) {
        vel = VectorLength(pm->ps->velocity);
        // slide along the ground plane
        PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal, pm->ps->velocity, OVERCLIP);

        VectorNormalize(pm->ps->velocity);
        VectorScale(pm->ps->velocity, vel, pm->ps->velocity);
    }

    PM_SlideMove(false);
}

/*
===================
PM_FlyVehicleMove

===================
*/
static void PM_FlyVehicleMove(void)
{
    int i;
    vec3_t wishvel;
    float wishspeed;
    vec3_t wishdir;
    float scale;
    float zVel;

    // normal slowdown
    if (pm->ps->gravity && pm->ps->velocity[2] < 0 && pm->ps->groundEntityNum == ENTITYNUM_NONE) { // falling
        zVel = pm->ps->velocity[2];
        PM_Friction();
        pm->ps->velocity[2] = zVel;
    }
    else {
        PM_Friction();
        if (pm->ps->velocity[2] < 0 && pm->ps->groundEntityNum != ENTITYNUM_NONE) {
            pm->ps->velocity[2] = 0; // ignore slope movement
        }
    }

    scale = PM_CmdScale(&pm->cmd);
    //
    // user intentions
    //
    if (!scale) {
        wishvel[0] = 0;
        wishvel[1] = 0;
        wishvel[2] = 0;
    }
    else {
        for (i = 0; i < 3; i++) {
            wishvel[i] = scale * pml.forward[i] * pm->cmd.forwardmove + scale * pml.right[i] * pm->cmd.rightmove;
        }

        wishvel[2] += scale * pm->cmd.upmove;
    }

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);

    PM_Accelerate(wishdir, wishspeed, 100);

    PM_StepSlideMove(1);
}

/*
===================
PM_FlyMove

Only with the flight powerup
===================
*/
static void PM_FlyMove(void)
{
    int i;
    vec3_t wishvel;
    float wishspeed;
    vec3_t wishdir;
    float scale;
    float accel;
    bool lowGravMove = false;

    // normal slowdown
    PM_Friction();

    if (pm->ps->gravity <= 0 && (!pm->ps->clientNum || (pm->gent && pm->gent->NPC && pm->gent->NPC->stats.moveType == movetype_t::MT_RUNJUMP))) {
        PM_CheckJump();
        accel = 1.0f;
        pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
        pm->ps->jumpZStart = pm->ps->origin[2]; // so we don't take a lot of damage when the gravity comes back on
        lowGravMove = true;
    }
    else {
        accel = pm_flyaccelerate;
    }

    scale = PM_CmdScale(&pm->cmd);
    //
    // user intentions
    //
    if (!scale) {
        wishvel[0] = 0;
        wishvel[1] = 0;
        wishvel[2] = 0;
    }
    else {
        for (i = 0; i < 3; i++) {
            wishvel[i] = scale * pml.forward[i] * pm->cmd.forwardmove + scale * pml.right[i] * pm->cmd.rightmove;
        }
        if (lowGravMove) {
            wishvel[2] += scale * pm->cmd.upmove;
            VectorScale(wishvel, 0.5f, wishvel);
        }
    }

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);

    PM_Accelerate(wishdir, wishspeed, accel);

    PM_StepSlideMove(1);
}

bool PM_GroundSlideOkay(float zNormal)
{
    if (zNormal > 0) {
        if (pm->ps->velocity[2] > 0) {
            if (pm->ps->legsAnim == animNumber_t::BOTH_WALL_RUN_RIGHT || pm->ps->legsAnim == animNumber_t::BOTH_WALL_RUN_LEFT ||
                pm->ps->legsAnim == animNumber_t::BOTH_WALL_RUN_RIGHT_STOP || pm->ps->legsAnim == animNumber_t::BOTH_WALL_RUN_LEFT_STOP) {
                return false;
            }
        }
    }
    return true;
}
/*
===================
PM_AirMove

===================
*/
static void PM_AirMove(void)
{
    int i;
    vec3_t wishvel;
    float fmove, smove;
    vec3_t wishdir;
    float wishspeed;
    // float		scale;
    usercmd_t cmd;
    float gravMod = 1.0f;

#if METROID_JUMP
    PM_CheckJump();
#endif

    PM_Friction();

    fmove = pm->cmd.forwardmove;
    smove = pm->cmd.rightmove;

    cmd = pm->cmd;
    PM_CmdScale(&cmd);

    // set the movementDir so clients can rotate the legs for strafing
    PM_SetMovementDir();

    // project moves down to flat plane
    pml.forward[2] = 0;
    pml.right[2] = 0;
    VectorNormalize(pml.forward);
    VectorNormalize(pml.right);

    if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_SLOW_MO_FALL)) { // no air-control
        VectorClear(wishvel);
    }
    else {
        for (i = 0; i < 2; i++) {
            wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
        }
        wishvel[2] = 0;
    }

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);

    /*
    if ( pm->gent && pm->gent->client && pm->gent->client->playerTeam == TEAM_STASIS )
    {//FIXME: do a check for movetype_float
        //Can move fairly well in air while falling
        PM_Accelerate (wishdir, wishspeed, pm_accelerate/2.0f);
    }
    else
    {
    */
    if ((DotProduct(pm->ps->velocity, wishdir)) < 0.0f) { // Encourage deceleration away from the current velocity
        wishspeed *= pm_airDecelRate;
    }

    // not on ground, so little effect on velocity
    PM_Accelerate(wishdir, wishspeed, pm_airaccelerate);
    //}

    // we may have a ground plane that is very steep, even
    // though we don't have a groundentity
    // slide along the steep plane
    if (pml.groundPlane) {
        if (PM_GroundSlideOkay(pml.groundTrace.plane.normal[2])) {
            PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal, pm->ps->velocity, OVERCLIP);
        }
    }

    if (!pm->ps->clientNum && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_0 && pm->ps->forceJumpZStart &&
        pm->ps->velocity[2] > 0) { // I am force jumping and I'm not holding the button anymore
        float curHeight = pm->ps->origin[2] - pm->ps->forceJumpZStart + (pm->ps->velocity[2] * pml.frametime);
        float maxJumpHeight = forceJumpHeight[pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION]];
        if (curHeight >= maxJumpHeight) { // reached top, cut velocity
            pm->ps->velocity[2] = 0;
            /*
            //put on a cvar?
            float stepCrouchAdd = STEPSIZE+(DEFAULT_MAXS_2-CROUCH_MAXS_2);
            Q_Println(S_COLOR_RED"capping force jump height {:4.2f} > {:4.2f} ({:4.2f}/{:4.2f})", curHeight, maxJumpHeight,
            forceJumpHeight[pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION]],
            forceJumpHeight[pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION]]+stepCrouchAdd);
            */
        }
    }
    /*
    if ( g_timescale != nullptr )
    {
        if ( g_timescale->value < 1.0f )
        {
            if ( pm->ps->clientNum == 0 )
            {
                gravMod *= (1.0f/g_timescale->value);
            }
        }
    }
    */

    PM_StepSlideMove(gravMod);
}

/*
===================
PM_WalkMove

===================
*/
static void PM_WalkMove(void)
{
    int i;
    vec3_t wishvel;
    float fmove, smove;
    vec3_t wishdir;
    float wishspeed;
    float scale;
    usercmd_t cmd;
    float accelerate;
    float vel;

    if (pm->ps->gravity < 0) { // float away
        pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
        pm->ps->groundEntityNum = ENTITYNUM_NONE;
        pml.groundPlane = false;
        pml.walking = false;
        if (pm->waterlevel > 1) {
            PM_WaterMove();
        }
        else {
            PM_AirMove();
        }
        return;
    }

    if (pm->waterlevel > 2 && DotProduct(pml.forward, pml.groundTrace.plane.normal) > 0) {
        // begin swimming
        PM_WaterMove();
        return;
    }

    if (PM_CheckJump()) {
        // jumped away
        if (pm->waterlevel > 1) {
            PM_WaterMove();
        }
        else {
            PM_AirMove();
        }
        return;
    }

    if (pm->ps->groundEntityNum != ENTITYNUM_NONE &&              // on ground
        pm->ps->velocity[2] <= 0 &&                               // not going up
        Any(pm->ps->pm_flags & pmoveFlags_t::PMF_TIME_KNOCKBACK)) // knockback fimter on (stops friction)
    {
        pm->ps->pm_flags &= ~pmoveFlags_t::PMF_TIME_KNOCKBACK;
    }

    bool slide = false;
    if (pm->ps->pm_type == pmtype_t::PM_DEAD) {                   // corpse
        if (g_entities[pm->ps->groundEntityNum].client) {         // on a client
            if (g_entities[pm->ps->groundEntityNum].health > 0) { // a living client
                // no friction
                slide = true;
            }
        }
    }
    if (!slide) {
        PM_Friction();
    }

    fmove = pm->cmd.forwardmove;
    smove = pm->cmd.rightmove;

    cmd = pm->cmd;
    scale = PM_CmdScale(&cmd);

    // set the movementDir so clients can rotate the legs for strafing
    PM_SetMovementDir();

    // project moves down to flat plane
    pml.forward[2] = 0;
    pml.right[2] = 0;

    // project the forward and right directions onto the ground plane
    PM_ClipVelocity(pml.forward, pml.groundTrace.plane.normal, pml.forward, OVERCLIP);
    PM_ClipVelocity(pml.right, pml.groundTrace.plane.normal, pml.right, OVERCLIP);
    //
    VectorNormalize(pml.forward);
    VectorNormalize(pml.right);

    if (pm->ps->clientNum && !VectorCompare(pm->ps->moveDir, vec3_origin)) { // NPC
        // FIXME: what if the ucmd was set directly.... sigh.... check movedir timestamp?
        VectorScale(pm->ps->moveDir, pm->ps->speed, wishvel);
        VectorCopy(pm->ps->moveDir, wishdir);
        wishspeed = pm->ps->speed;
    }
    else {
        for (i = 0; i < 3; i++) {
            wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
        }
        // when going up or down slopes the wish velocity should Not be zero
        //	wishvel[2] = 0;

        VectorCopy(wishvel, wishdir);
        wishspeed = VectorNormalize(wishdir);
        wishspeed *= scale;
    }

    // clamp the speed lower if ducking
    if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED) && !PM_InKnockDown(pm->ps)) {
        if (wishspeed > pm->ps->speed * pm_duckScale) {
            wishspeed = pm->ps->speed * pm_duckScale;
        }
    }

    // clamp the speed lower if wading or walking on the bottom
    if (pm->waterlevel) {
        float waterScale;

        waterScale = pm->waterlevel / 3.0;
        waterScale = 1.0 - (1.0 - pm_swimScale) * waterScale;
        if (wishspeed > pm->ps->speed * waterScale) {
            wishspeed = pm->ps->speed * waterScale;
        }
    }

    // when a player gets hit, they temporarily lose
    // full control, which allows them to be moved a bit
    if (Any(pml.groundTrace.surfaceFlags & surfaceFlags_t::SURF_SLICK) || Any(pm->ps->pm_flags & pmoveFlags_t::PMF_TIME_KNOCKBACK) ||
        Any(pm->ps->pm_flags & pmoveFlags_t::PMF_TIME_NOFRICTION)) {
        accelerate = pm_airaccelerate;
    }
    else {
        accelerate = pm_accelerate;
    }

    PM_Accelerate(wishdir, wishspeed, accelerate);

    // Q_Println("velocity = {:1.1f} {:1.1f} {:1.1f}", pm->ps->velocity[0], pm->ps->velocity[1], pm->ps->velocity[2]);
    // Q_Println("velocity1 = {:1.1f}", VectorLength(pm->ps->velocity));

    if (Any(pml.groundTrace.surfaceFlags & surfaceFlags_t::SURF_SLICK) || Any(pm->ps->pm_flags & pmoveFlags_t::PMF_TIME_KNOCKBACK) ||
        Any(pm->ps->pm_flags & pmoveFlags_t::PMF_TIME_NOFRICTION)) {
        if (pm->ps->gravity >= 0 && pm->ps->groundEntityNum != ENTITYNUM_NONE && !VectorLengthSquared(pm->ps->velocity) &&
            pml.groundTrace.plane.normal[2] ==
                1.0) { // on ground and not moving and on level ground, no reason to do stupid fucking gravity with the clipvelocity!!!!
        }
        else {
            if (!Any(pm->ps->eFlags & entityFlags_t::EF_FORCE_GRIPPED)) {
                pm->ps->velocity[2] -= pm->ps->gravity * pml.frametime;
            }
        }
    }
    else {
        // don't reset the z velocity for slopes
        //		pm->ps->velocity[2] = 0;
    }

    vel = VectorLength(pm->ps->velocity);

    // slide along the ground plane
    PM_ClipVelocity(pm->ps->velocity, pml.groundTrace.plane.normal, pm->ps->velocity, OVERCLIP);

    // don't decrease velocity when going up or down a slope
    VectorNormalize(pm->ps->velocity);
    VectorScale(pm->ps->velocity, vel, pm->ps->velocity);

    // don't do anything if standing still
    if (!pm->ps->velocity[0] && !pm->ps->velocity[1]) {
        return;
    }

    if (pm->ps->gravity <= 0) { // need to apply gravity since we're going to float up from ground
        PM_StepSlideMove(1);
    }
    else {
        PM_StepSlideMove(0);
    }

    // Q_Println("velocity2 = {:1.1f}", VectorLength(pm->ps->velocity));
}

/*
==============
PM_DeadMove
==============
*/
static void PM_DeadMove(void)
{
    float forward;

    if (!pml.walking) {
        return;
    }

    // extra friction

    forward = VectorLength(pm->ps->velocity);
    forward -= 20;
    if (forward <= 0) {
        VectorClear(pm->ps->velocity);
    }
    else {
        VectorNormalize(pm->ps->velocity);
        VectorScale(pm->ps->velocity, forward, pm->ps->velocity);
    }
}

/*
===============
PM_NoclipMove
===============
*/
static void PM_NoclipMove(void)
{
    float speed, drop, friction, control, newspeed;
    int i;
    vec3_t wishvel;
    float fmove, smove;
    vec3_t wishdir;
    float wishspeed;
    float scale;

    if (pm->gent && pm->gent->client) {
        pm->ps->viewheight = pm->gent->client->standheight + STANDARD_VIEWHEIGHT_OFFSET;
        //		if ( !pm->gent->mins[0] || !pm->gent->mins[1] || !pm->gent->mins[2] || !pm->gent->maxs[0] || !pm->gent->maxs[1] || !pm->gent->maxs[2] )
        //		{
        //			assert(0);
        //		}

        VectorCopy(pm->gent->mins, pm->mins);
        VectorCopy(pm->gent->maxs, pm->maxs);
    }
    else {
        pm->ps->viewheight = DEFAULT_MAXS_2 + STANDARD_VIEWHEIGHT_OFFSET; // DEFAULT_VIEWHEIGHT;

        if (!DEFAULT_MINS_0 || !DEFAULT_MINS_1 || !DEFAULT_MAXS_0 || !DEFAULT_MAXS_1 || !DEFAULT_MINS_2 || !DEFAULT_MAXS_2) {
            assert(0);
        }

        pm->mins[0] = DEFAULT_MINS_0;
        pm->mins[1] = DEFAULT_MINS_1;
        pm->mins[2] = DEFAULT_MINS_2;

        pm->maxs[0] = DEFAULT_MAXS_0;
        pm->maxs[1] = DEFAULT_MAXS_1;
        pm->maxs[2] = DEFAULT_MAXS_2;
    }

    // friction

    speed = VectorLength(pm->ps->velocity);
    if (speed < 1) {
        VectorCopy(vec3_origin, pm->ps->velocity);
    }
    else {
        drop = 0;

        friction = pm_friction * 1.5; // extra friction
        control = speed < pm_stopspeed ? pm_stopspeed : speed;
        drop += control * friction * pml.frametime;

        // scale the velocity
        newspeed = speed - drop;
        if (newspeed < 0)
            newspeed = 0;
        newspeed /= speed;

        VectorScale(pm->ps->velocity, newspeed, pm->ps->velocity);
    }

    // accelerate
    scale = PM_CmdScale(&pm->cmd);
    if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)) { // turbo boost
        scale *= 10;
    }
    if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) { // turbo boost
        scale *= 10;
    }

    fmove = pm->cmd.forwardmove;
    smove = pm->cmd.rightmove;

    for (i = 0; i < 3; i++)
        wishvel[i] = pml.forward[i] * fmove + pml.right[i] * smove;
    wishvel[2] += pm->cmd.upmove;

    VectorCopy(wishvel, wishdir);
    wishspeed = VectorNormalize(wishdir);
    wishspeed *= scale;

    PM_Accelerate(wishdir, wishspeed, pm_accelerate);

    // move
    VectorMA(pm->ps->origin, pml.frametime, pm->ps->velocity, pm->ps->origin);
}

//============================================================================

/*
================
PM_FootstepForSurface

Returns an event number apropriate for the groundsurface
================
*/
static entity_event_t PM_FootstepForSurface(void)
{
    if (Any(pml.groundTrace.surfaceFlags & surfaceFlags_t::SURF_NOSTEPS)) {
        return entity_event_t::EV_NONE;
    }
    if (Any(pml.groundTrace.surfaceFlags & surfaceFlags_t::SURF_METALSTEPS)) {
        return entity_event_t::EV_FOOTSTEP_METAL;
    }
    return entity_event_t::EV_FOOTSTEP;
}

static float PM_DamageForDelta(int delta)
{
    float damage = delta;
    if (pm->gent->NPC) {
        if (pm->ps->weapon == weapon_t::WP_SABER) { // FIXME: for now Jedi take no falling damage, but really they should if pushed off?
            damage = 0;
        }
    }
    else if (!pm->ps->clientNum) {
        if (damage < 50) {
            if (damage > 24) {
                damage = damage - 25;
            }
        }
        else {
            damage /= 2;
        }
    }
    return damage / 2;
}

static void PM_CrashLandDamage(int damage)
{
    if (pm->gent) {
        if (pm->gent->NPC && Any(pm->gent->NPC->aiFlags & npcAiFlags_t::NPCAI_DIE_ON_IMPACT)) {
            damage = 1000;
        }
        else {
            damage = PM_DamageForDelta(damage);
        }

        if (damage && !Any(pm->gent->flags & gentityFlags_t::FL_NO_IMPACT_DMG)) {
            pm->gent->painDebounceTime = level.time + 200; // no normal pain sound
            G_Damage(pm->gent, nullptr, nullptr, nullptr, nullptr, damage, damageFlags_t::DAMAGE_NO_ARMOR, meansOfDeath_t::MOD_FALLING);
        }
    }
}

/*
static float PM_CrashLandDelta( vec3_t org, vec3_t prevOrg, vec3_t prev_vel, float grav, int waterlevel )
{
    float		delta;
    float		dist;
    float		vel, acc;
    float		t;
    float		a, b, c, den;

    // calculate the exact velocity on landing
    dist = org[2] - prevOrg[2];
    vel = prev_vel[2];
    acc = -grav;

    a = acc / 2;
    b = vel;
    c = -dist;

    den =  b * b - 4 * a * c;
    if ( den < 0 )
    {
        return 0;
    }
    t = (-b - Q_sqrtf( den ) ) / ( 2 * a );

    delta = vel + t * acc;
    delta = delta*delta * 0.0001;

    // never take falling damage if completely underwater
    if ( waterlevel == 3 )
    {
        return 0;
    }
    // reduce falling damage if there is standing water
    if ( waterlevel == 2 )
    {
        delta *= 0.25;
    }
    if ( waterlevel == 1 )
    {
        delta *= 0.5;
    }

    return delta;
}
*/

static float PM_CrashLandDelta(vec3_t prev_vel, int waterlevel)
{
    float delta;

    if (pm->waterlevel == 3) {
        return 0;
    }
    delta = Q_fabsf(prev_vel[2]) / 10; // VectorLength( prev_vel )

    // reduce falling damage if there is standing water
    if (pm->waterlevel == 2) {
        delta *= 0.25;
    }
    if (pm->waterlevel == 1) {
        delta *= 0.5;
    }

    return delta;
}

std::optional<animNumber_t> PM_GetLandingAnim(void)
{
    animNumber_t anim = pm->ps->legsAnim;
    if (PM_SpinningAnim(anim) || PM_SaberInSpecialAttack(anim)) {
        return std::nullopt;
    }
    switch (anim) {
    case animNumber_t::BOTH_FORCEJUMPLEFT1:
    case animNumber_t::BOTH_FORCEINAIRLEFT1:
        anim = animNumber_t::BOTH_FORCELANDLEFT1;
        break;
    case animNumber_t::BOTH_FORCEJUMPRIGHT1:
    case animNumber_t::BOTH_FORCEINAIRRIGHT1:
        anim = animNumber_t::BOTH_FORCELANDRIGHT1;
        break;
    case animNumber_t::BOTH_FORCEJUMP1:
    case animNumber_t::BOTH_FORCEINAIR1:
        anim = animNumber_t::BOTH_FORCELAND1;
        break;
    case animNumber_t::BOTH_FORCEJUMPBACK1:
    case animNumber_t::BOTH_FORCEINAIRBACK1:
        anim = animNumber_t::BOTH_FORCELANDBACK1;
        break;
    case animNumber_t::BOTH_JUMPLEFT1:
    case animNumber_t::BOTH_INAIRLEFT1:
        anim = animNumber_t::BOTH_LANDLEFT1;
        break;
    case animNumber_t::BOTH_JUMPRIGHT1:
    case animNumber_t::BOTH_INAIRRIGHT1:
        anim = animNumber_t::BOTH_LANDRIGHT1;
        break;
    case animNumber_t::BOTH_JUMP1:
    case animNumber_t::BOTH_INAIR1:
        anim = animNumber_t::BOTH_LAND1;
        break;
    case animNumber_t::BOTH_JUMPBACK1:
    case animNumber_t::BOTH_INAIRBACK1:
        anim = animNumber_t::BOTH_LANDBACK1;
        break;
    case animNumber_t::BOTH_BUTTERFLY_LEFT:
    case animNumber_t::BOTH_BUTTERFLY_RIGHT:
    case animNumber_t::BOTH_FJSS_TR_BL:
    case animNumber_t::BOTH_FJSS_TL_BR:
    case animNumber_t::BOTH_LUNGE2_B__T_:
    case animNumber_t::BOTH_FORCELEAP2_T__B_:
    case animNumber_t::BOTH_ARIAL_LEFT:
    case animNumber_t::BOTH_ARIAL_RIGHT:
    case animNumber_t::BOTH_ARIAL_F1:
    case animNumber_t::BOTH_CARTWHEEL_LEFT:
    case animNumber_t::BOTH_CARTWHEEL_RIGHT:
    case animNumber_t::BOTH_JUMPFLIPSLASHDOWN1: // #
    case animNumber_t::BOTH_JUMPFLIPSTABDOWN:   // #
        return std::nullopt;
    case animNumber_t::BOTH_WALL_RUN_LEFT:  // #
    case animNumber_t::BOTH_WALL_RUN_RIGHT: // #
        if (pm->ps->legsAnimTimer > 500) {  // only land at end of anim
            return std::nullopt;
        }
        // NOTE: falls through on purpose!
    default:
        if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_BACKWARDS_JUMP)) {
            anim = animNumber_t::BOTH_LANDBACK1;
        }
        else {
            anim = animNumber_t::BOTH_LAND1;
        }
        break;
    }
    return anim;
}

static bool PM_TryRoll(void)
{
    float rollDist = 192; // was 64;
    if (PM_SaberInAttack(pm->ps->saberMove) || PM_SaberInSpecialAttack(pm->ps->torsoAnim) || PM_SpinningSaberAnim(pm->ps->legsAnim) ||
        (!pm->ps->clientNum && PM_SaberInStart(pm->ps->saberMove))) { // attacking or spinning (or, if player, starting an attack)
        return false;
    }
    if (!pm->ps->clientNum && (!cg.renderingThirdPerson || cg.zoomMode)) { // player can't do this in 1st person
        return false;
    }
    if (!pm->gent) {
        return false;
    }
    if (pm->ps->clientNum && pm->ps->weapon != weapon_t::WP_SABER && pm->ps->weapon != weapon_t::WP_NONE) { // only jedi
        return false;
    }
    if (pm->gent && pm->gent->NPC) {
        if (pm->gent->NPC->rank != rank_t::RANK_CREWMAN &&
            pm->gent->NPC->rank < rank_t::RANK_LT_JG) { // reborn who are not acrobats or fencers can't do any of these acrobatics
            return false;
        }
        if (Any(pm->gent->NPC->scriptFlags & scriptFlags_t::SCF_NO_ACROBATICS)) {
            return false;
        }
    }
    vec3_t fwd, right, traceto, mins = {pm->mins[0], pm->mins[1], pm->mins[2] + STEPSIZE},
                                maxs = {pm->maxs[0], pm->maxs[1], (float)pm->gent->client->crouchheight}, fwdAngles = {0, pm->ps->viewangles[YAW], 0};
    trace_t trace;
    std::optional<animNumber_t> anim;
    AngleVectors(fwdAngles, fwd, right, nullptr);
    // FIXME: trace ahead for clearance to roll
    if (pm->cmd.forwardmove) {
        if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_BACKWARDS_RUN)) {
            anim = animNumber_t::BOTH_ROLL_B;
            VectorMA(pm->ps->origin, -rollDist, fwd, traceto);
        }
        else {
            anim = animNumber_t::BOTH_ROLL_F;
            VectorMA(pm->ps->origin, rollDist, fwd, traceto);
        }
    }
    else if (pm->cmd.rightmove > 0) {
        anim = animNumber_t::BOTH_ROLL_R;
        VectorMA(pm->ps->origin, rollDist, right, traceto);
    }
    else if (pm->cmd.rightmove < 0) {
        anim = animNumber_t::BOTH_ROLL_L;
        VectorMA(pm->ps->origin, -rollDist, right, traceto);
    }
    else { //???
    }
    if (anim) {
        bool roll = false;
        contentsFlags_t clipmask = contentsFlags_t::CONTENTS_SOLID;
        if (pm->ps->clientNum) {
            clipmask |= (contentsFlags_t::CONTENTS_MONSTERCLIP | contentsFlags_t::CONTENTS_BOTCLIP);
        }
        else {
            if (pm->gent && pm->gent->enemy && pm->gent->enemy->health > 0) { // player can always roll in combat
                roll = true;
            }
            else {
                clipmask |= contentsFlags_t::CONTENTS_PLAYERCLIP;
            }
        }
        if (!roll) {
            pm->trace(&trace, pm->ps->origin, mins, maxs, traceto, pm->ps->clientNum, clipmask, EG2_Collision::G2_NOCOLLIDE, 0);
            if (trace.fraction >= 1.0f) { // okay, clear, check for a bottomless drop
                vec3_t top;
                VectorCopy(traceto, top);
                traceto[2] -= 256;
                pm->trace(&trace, top, mins, maxs, traceto, pm->ps->clientNum, contentsFlags_t::CONTENTS_SOLID, EG2_Collision::G2_NOCOLLIDE, 0);
                if (trace.fraction < 1.0f) { // not a bottomless drop
                    roll = true;
                }
            }
            else {                       // hit an architectural obstruction
                if (pm->ps->clientNum) { // NPCs don't care about rolling into walls, just off ledges
                    if (!Any(trace.contents & contentsFlags_t::CONTENTS_BOTCLIP)) {
                        roll = true;
                    }
                }
                else if (G_EntIsDoor(trace.entityNum)) {        // okay to roll into a door
                    if (G_EntIsUnlockedDoor(trace.entityNum)) { // if it's an auto-door
                        roll = true;
                    }
                }
                else { // check other conditions
                    gentity_t* traceEnt = &g_entities[trace.entityNum];
                    if (traceEnt && Any(traceEnt->svFlags & svFlags_t::SVF_GLASS_BRUSH)) { // okay to roll through glass
                        roll = true;
                    }
                }
            }
        }
        if (roll) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, *anim,
                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_HOLDLESS);
            pm->ps->weaponTime = pm->ps->torsoAnimTimer - 200; // just to make sure it's cleared when roll is done
            PM_AddEvent(entity_event_t::EV_ROLL);
            pm->ps->saberMove = saberMoveName_t::LS_NONE;
            return true;
        }
    }
    return false;
}

/*
=================
PM_CrashLand

Check for hard landings that generate sound events
=================
*/
static void PM_CrashLand(void)
{
    float delta = 0;
    bool forceLanding = false;

    if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_TRIGGER_PUSHED)) {
        delta = 21; //?
        forceLanding = true;
    }
    else {
        if (pm->gent && pm->gent->NPC &&
            Any(pm->gent->NPC->aiFlags & npcAiFlags_t::NPCAI_DIE_ON_IMPACT)) { // have to do death on impact if we are falling to our death, FIXME: should we
                                                                               // avoid any additional damage this func?
            PM_CrashLandDamage(1000);
        }

        if (pm->ps->jumpZStart &&
            (pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] >= forcePowerLevels_t::FORCE_LEVEL_1 || !pm->ps->clientNum)) { // we were force-jumping
            if (pm->ps->origin[2] >= pm->ps->jumpZStart) { // we landed at same height or higher than we landed
                if (pm->ps->forceJumpZStart) {             // we were force-jumping
                    forceLanding = true;
                }
                delta = 0;
            }
            else { // take off some of it, at least
                delta = (pm->ps->jumpZStart - pm->ps->origin[2]);
                float dropAllow = forceJumpHeight[pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION]];
                if (dropAllow < 128) { // always allow a drop from 128, at least
                    dropAllow = 128;
                }
                if (delta > forceJumpHeight[forcePowerLevels_t::FORCE_LEVEL_1]) { // will have to use force jump ability to absorb some of it
                    forceLanding = true;                                          // absorbed some - just to force the correct animation to play below
                }
                delta = (delta - dropAllow) / 2;
            }
            if (delta < 1) {
                delta = 1;
            }
        }

        if (!delta) {
            delta = PM_CrashLandDelta(pml.previous_velocity, pm->waterlevel);
        }
    }

    // FIXME: if duck just as you land, roll and take half damage

    if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED) &&
        (level.time - pm->ps->lastOnGround) > 500) {                 // must be crouched and have been inthe air for half a second minimum
        if (!PM_InOnGroundAnim(pm->ps) && !PM_InKnockDown(pm->ps)) { // roll!
            if (PM_TryRoll()) {                                      // absorb some impact
                delta /= 2;
            }
        }
    }

    if (delta < 1) {
        return;
    }

    bool deadFallSound = false;
    if (!PM_InDeathAnim()) {
        if (pm->cmd.upmove >= 0 && !PM_InKnockDown(pm->ps) && !PM_InRoll(pm->ps)) { // not crouching
            if (delta > 10 || Any(pm->ps->pm_flags & pmoveFlags_t::PMF_BACKWARDS_JUMP) || (pm->ps->forcePowersActive & Bit(forcePowers_t::FP_LEVITATION)) ||
                forceLanding) // EV_FALL_SHORT or jumping back or force-land
            {                 // decide which landing animation to use
                const std::optional<animNumber_t> anim = PM_GetLandingAnim();
                if (anim) {
                    if (PM_FlippingAnim(pm->ps->torsoAnim) || PM_SpinningAnim(pm->ps->torsoAnim)) { // interrupt these if we're going to play a land
                        pm->ps->torsoAnimTimer = 0;
                    }
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, *anim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD,
                               100); // Only blend over 100ms
                }
            }
        }
    }
    else {
        pm->ps->gravity = 1.0;
        // PM_CrashLandDamage( delta );
        if (pm->gent) {
            if ((!Any(pml.groundTrace.surfaceFlags & surfaceFlags_t::SURF_NODAMAGE)) &&
                //				(!(pml.groundTrace.contents & contentsFlags_t::CONTENTS_NODROP)) &&
                (!Any(pm->pointcontents(pm->ps->origin, pm->ps->clientNum) & contentsFlags_t::CONTENTS_NODROP))) {
                if (pm->waterlevel < 2) { // don't play fallsplat when impact in the water
                    deadFallSound = true;
                    G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_BODY, "sound/player/fallsplat.wav");
                }
                if (gi.VoiceVolume[pm->ps->clientNum] && pm->gent->NPC &&
                    Any(pm->gent->NPC->aiFlags & npcAiFlags_t::NPCAI_DIE_ON_IMPACT)) { // I was talking, so cut it off... with a jump sound?
                    G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_VOICE_ATTEN, "*pain100.wav");
                }
            }
        }
        if (pm->ps->legsAnim == animNumber_t::BOTH_FALLDEATH1 || pm->ps->legsAnim == animNumber_t::BOTH_FALLDEATH1INAIR) { // FIXME: add a little bounce?
            // FIXME: cut voice channel?
            pmtype_t old_pm_type = pm->ps->pm_type;
            pm->ps->pm_type = pmtype_t::PM_NORMAL;
            // Hack because for some reason PM_SetAnim just returns if you're dead...???
            PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_FALLDEATH1LAND,
                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
            pm->ps->pm_type = old_pm_type;
            return;
        }
    }

    // create a local entity event to play the sound

    if (pm->gent && pm->gent->client && pm->gent->client->respawnTime >= level.time - 500) { // just spawned in, don't make a noise
        return;
    }

    if (delta >= 75) {
        if (!deadFallSound) {
            PM_AddEvent(entity_event_t::EV_FALL_FAR);
        }
        if (!Any(pml.groundTrace.surfaceFlags & surfaceFlags_t::SURF_NODAMAGE)) {
            PM_CrashLandDamage(delta);
        }
        if (pm->gent) {
            if (pm->gent->s.number == 0) {
                vec3_t bottom;

                VectorCopy(pm->ps->origin, bottom);
                bottom[2] += pm->mins[2];
                //	if ( pm->gent->client && pm->gent->client->playerTeam != TEAM_DISGUISE )
                {
                    AddSoundEvent(pm->gent, bottom, 256, alertEventLevel_e::AEL_SUSPICIOUS);
                }
            }
            else if (pm->ps->stats[statIndex_t::STAT_HEALTH] <= 0 && pm->gent && pm->gent->enemy) {
                AddSoundEvent(pm->gent->enemy, pm->ps->origin, 256, alertEventLevel_e::AEL_DISCOVERED);
            }
        }
    }
    else if (delta >= 50) {
        // this is a pain grunt, so don't play it if dead
        if (pm->ps->stats[statIndex_t::STAT_HEALTH] > 0) {
            if (!deadFallSound) {
                PM_AddEvent(entity_event_t::EV_FALL_MEDIUM); // damage is dealt in g_active, ClientEvents
            }
            if (pm->gent) {
                if (!Any(pml.groundTrace.surfaceFlags & surfaceFlags_t::SURF_NODAMAGE)) {
                    PM_CrashLandDamage(delta);
                }
                if (pm->gent->s.number == 0) {
                    vec3_t bottom;

                    VectorCopy(pm->ps->origin, bottom);
                    bottom[2] += pm->mins[2];
                    //	if ( pm->gent->client && pm->gent->client->playerTeam != TEAM_DISGUISE )
                    {
                        AddSoundEvent(pm->gent, bottom, 256, alertEventLevel_e::AEL_MINOR);
                    }
                }
            }
        }
    }
    else if (delta >= 30) {
        if (!deadFallSound) {
            PM_AddEvent(entity_event_t::EV_FALL_SHORT);
        }
        if (pm->gent) {
            if (pm->gent->s.number == 0) {
                vec3_t bottom;

                VectorCopy(pm->ps->origin, bottom);
                bottom[2] += pm->mins[2];
                //		if ( pm->gent->client && pm->gent->client->playerTeam != TEAM_DISGUISE )
                {
                    AddSoundEvent(pm->gent, bottom, 128, alertEventLevel_e::AEL_MINOR);
                }
            }
            else {
                if (!Any(pml.groundTrace.surfaceFlags & surfaceFlags_t::SURF_NODAMAGE)) {
                    PM_CrashLandDamage(delta);
                }
            }
        }
    }
    else {
        if (!deadFallSound) {
            if (forceLanding) { // we were force-jumping
                PM_AddEvent(entity_event_t::EV_FALL_SHORT);
            }
            else {
                PM_AddEvent(PM_FootstepForSurface());
            }
        }
    }

    // start footstep cycle over
    pm->ps->bobCycle = 0;
    if (pm->gent && pm->gent->client) { // stop the force push effect when you land
        pm->gent->forcePushTime = 0;
    }
}

/*
=============
PM_CorrectAllSolid
=============
*/
static void PM_CorrectAllSolid(void)
{
    if (pm->debugLevel) {
        Q_Println("{}:allsolid", c_pmove); // NOTENOTE: If this ever happens, I'd really like to see this print!
    }

    // FIXME: jitter around

    pm->ps->groundEntityNum = ENTITYNUM_NONE;
    pml.groundPlane = false;
    pml.walking = false;
}

bool FlyingCreature(gentity_t* ent)
{
    if (ent->client->ps.gravity <= 0 && Any(ent->svFlags & svFlags_t::SVF_CUSTOM_GRAVITY)) {
        return true;
    }
    return false;
}

static void PM_FallToDeath(void)
{
    if (!pm->gent) {
        return;
    }
    if (PM_HasAnimation(pm->gent, animNumber_t::BOTH_FALLDEATH1)) {
        PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_FALLDEATH1, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
    }
    else {
        PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_DEATH1, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
    }
    G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_VOICE, "*falling1.wav"); // CHAN_VOICE_ATTEN
    if (pm->gent->NPC) {
        pm->gent->NPC->aiFlags |= npcAiFlags_t::NPCAI_DIE_ON_IMPACT;
        pm->gent->NPC->nextBStateThink = Q3_INFINITE;
    }
    pm->ps->friction = 1;
}

animNumber_t PM_ForceJumpAnimForJumpAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_JUMP1:            // # Jump - wind-up and leave ground
        anim = animNumber_t::BOTH_FORCEJUMP1; // # Jump - wind-up and leave ground
        break;
    case animNumber_t::BOTH_INAIR1:            // # In air loop (from jump)
        anim = animNumber_t::BOTH_FORCEINAIR1; // # In air loop (from jump)
        break;
    case animNumber_t::BOTH_LAND1:            // # Landing (from in air loop)
        anim = animNumber_t::BOTH_FORCELAND1; // # Landing (from in air loop)
        break;
    case animNumber_t::BOTH_JUMPBACK1:            // # Jump backwards - wind-up and leave ground
        anim = animNumber_t::BOTH_FORCEJUMPBACK1; // # Jump backwards - wind-up and leave ground
        break;
    case animNumber_t::BOTH_INAIRBACK1:            // # In air loop (from jump back)
        anim = animNumber_t::BOTH_FORCEINAIRBACK1; // # In air loop (from jump back)
        break;
    case animNumber_t::BOTH_LANDBACK1:            // # Landing backwards(from in air loop)
        anim = animNumber_t::BOTH_FORCELANDBACK1; // # Landing backwards(from in air loop)
        break;
    case animNumber_t::BOTH_JUMPLEFT1:            // # Jump left - wind-up and leave ground
        anim = animNumber_t::BOTH_FORCEJUMPLEFT1; // # Jump left - wind-up and leave ground
        break;
    case animNumber_t::BOTH_INAIRLEFT1:            // # In air loop (from jump left)
        anim = animNumber_t::BOTH_FORCEINAIRLEFT1; // # In air loop (from jump left)
        break;
    case animNumber_t::BOTH_LANDLEFT1:            // # Landing left(from in air loop)
        anim = animNumber_t::BOTH_FORCELANDLEFT1; // # Landing left(from in air loop)
        break;
    case animNumber_t::BOTH_JUMPRIGHT1:            // # Jump right - wind-up and leave ground
        anim = animNumber_t::BOTH_FORCEJUMPRIGHT1; // # Jump right - wind-up and leave ground
        break;
    case animNumber_t::BOTH_INAIRRIGHT1:            // # In air loop (from jump right)
        anim = animNumber_t::BOTH_FORCEINAIRRIGHT1; // # In air loop (from jump right)
        break;
    case animNumber_t::BOTH_LANDRIGHT1:            // # Landing right(from in air loop)
        anim = animNumber_t::BOTH_FORCELANDRIGHT1; // # Landing right(from in air loop)
        break;
    default:
        break;
    }
    return anim;
}
/*
=============
PM_GroundTraceMissed

The ground trace didn't hit a surface, so we are in freefall
=============
*/
static void PM_GroundTraceMissed(void)
{
    trace_t trace;
    vec3_t point;
    bool cliff_fall = false;

    // FIXME: if in a contents_falldeath brush, play the falling death anim and sound?
    if (pm->ps->clientNum != 0 && pm->gent && pm->gent->NPC && pm->gent->client &&
        pm->gent->client->NPC_class != class_t::CLASS_DESANN) // desann never falls to his death
    {
        if (pm->ps->groundEntityNum == ENTITYNUM_NONE) {
            if (pm->ps->stats[statIndex_t::STAT_HEALTH] > 0 && !Any(pm->gent->NPC->aiFlags & npcAiFlags_t::NPCAI_DIE_ON_IMPACT) &&
                !Any(pm->gent->NPC->scriptFlags & scriptFlags_t::SCF_NO_FALLTODEATH)) {
                if ((level.time - pm->gent->client->respawnTime > 2000) // been in the world for at least 2 seconds
                    && (!pm->gent->NPC->timeOfDeath || level.time - pm->gent->NPC->timeOfDeath < 1000) &&
                    pm->gent->e_ThinkFunc != thinkFunc_t::thinkF_NPC_RemoveBody // Have to do this now because timeOfDeath is used by thinkF_NPC_RemoveBody to
                                                                                // debounce removal checks
                    && !(pm->gent->client->ps.forcePowersActive & Bit(forcePowers_t::FP_LEVITATION))) {
                    if (!FlyingCreature(pm->gent) && g_gravity->value > 0) {
                        if (!Any(pm->gent->flags & gentityFlags_t::FL_UNDYING) && !Any(pm->gent->flags & gentityFlags_t::FL_GODMODE)) {
                            if (!Any(pm->ps->eFlags & entityFlags_t::EF_FORCE_GRIPPED) && !Any(pm->ps->pm_flags & pmoveFlags_t::PMF_TRIGGER_PUSHED)) {
                                if (!pm->ps->forceJumpZStart ||
                                    pm->ps->forceJumpZStart > pm->ps->origin[2]) // && Q_fabsf(pm->ps->velocity[0])<10 && Q_fabsf(pm->ps->velocity[1])<10 &&
                                                                                 // pm->ps->velocity[2]<0)//either not force-jumping or force-jumped and now
                                                                                 // fell below original jump start height
                                {
                                    /*if ( pm->ps->legsAnim = animNumber_t::BOTH_FALLDEATH1
                                    && pm->ps->legsAnim != animNumber_t::BOTH_DEATH1
                                    && PM_HasAnimation( pm->gent, animNumber_t::BOTH_FALLDEATH1 )*/
                                    // New method: predict impact, 400 ahead
                                    vec3_t vel;
                                    float time;

                                    VectorCopy(pm->ps->velocity, vel);
                                    float speed = VectorLength(vel);
                                    if (!speed) { // damn divide by zero
                                        speed = 1;
                                    }
                                    time = 400 / speed;
                                    vel[2] -= 0.5 * time * pm->ps->gravity;
                                    speed = VectorLength(vel);
                                    if (!speed) { // damn divide by zero
                                        speed = 1;
                                    }
                                    time = 400 / speed;
                                    VectorScale(vel, time, vel);
                                    VectorAdd(pm->ps->origin, vel, point);

                                    pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask, EG2_Collision::G2_NOCOLLIDE,
                                              0);

                                    if (!trace.allsolid && !trace.startsolid && (pm->ps->origin[2] - trace.endpos[2]) >= 128) //>=128 so we don't die on steps!
                                    {
                                        if (trace.fraction == 1.0) {                                             // didn't hit, we're probably going to die
                                            if (pm->ps->velocity[2] < 0 && pm->ps->origin[2] - point[2] > 256) { // going down, into a bottomless pit,
                                                                                                                 // apparently
                                                PM_FallToDeath();
                                                cliff_fall = true;
                                            }
                                        }
                                        else if (trace.entityNum < ENTITYNUM_NONE &&
                                                 pm->ps->weapon != weapon_t::WP_SABER) { // Jedi don't scream and die if they're heading for a hard impact
                                            gentity_t* traceEnt = &g_entities[trace.entityNum];
                                            if (trace.entityNum == ENTITYNUM_WORLD || (traceEnt && traceEnt->bmodel)) { // hit architecture, find impact force
                                                float dmg;

                                                VectorCopy(pm->ps->velocity, vel);
                                                time = Distance(trace.endpos, pm->ps->origin) / VectorLength(vel);
                                                vel[2] -= 0.5 * time * pm->ps->gravity;

                                                if (trace.plane.normal[2] > 0.5) { // use falling damage
                                                    int waterlevel;
                                                    contentsFlags_t junk;
                                                    PM_SetWaterLevelAtPoint(trace.endpos, &waterlevel, &junk);
                                                    dmg = PM_CrashLandDelta(vel, waterlevel);
                                                    if (dmg >= 30) { // there is a minimum fall threshhold
                                                        dmg = PM_DamageForDelta(dmg);
                                                    }
                                                    else {
                                                        dmg = 0;
                                                    }
                                                }
                                                else { // use impact damage
                                                    // guestimate
                                                    if (pm->gent->client && pm->gent->client->ps.forceJumpZStart) { // we were force-jumping
                                                        if (pm->gent->currentOrigin[2] >=
                                                            pm->gent->client->ps.forceJumpZStart) { // we landed at same height or higher than we landed
                                                            dmg = 0;
                                                        }
                                                        else { // FIXME: take off some of it, at least?
                                                            dmg = (pm->gent->client->ps.forceJumpZStart - pm->gent->currentOrigin[2]) / 3;
                                                        }
                                                    }
                                                    dmg = 10 * VectorLength(pm->ps->velocity);
                                                    if (!pm->ps->clientNum) {
                                                        dmg /= 2;
                                                    }
                                                    dmg *= 0.01875f; // magic number
                                                }
                                                if (dmg >= pm->ps->stats[statIndex_t::STAT_HEALTH]) // armor?
                                                {
                                                    PM_FallToDeath();
                                                    cliff_fall = true;
                                                }
                                            }
                                        }
                                    }

                                    /*
                                    vec3_t	start;
                                    //okay, kind of expensive temp hack here, but let's check to see if we should scream
                                    //FIXME: we should either do a better check (predict using actual velocity) or we should wait until they've been over a
                                    bottomless pit for a certain amount of time... VectorCopy( pm->ps->origin, start ); if ( pm->ps->forceJumpZStart < start[2]
                                    )
                                    {//Jedi who are force-jumping should only do this from landing point down?
                                        start[2] = pm->ps->forceJumpZStart;
                                    }
                                    VectorCopy( start, point );
                                    point[2] -= 400;//320
                                    pm->trace (&trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask);
                                    //FIXME: somehow, people can still get stuck on ledges and not splat when hit...?
                                    if ( !trace.allsolid && !trace.startsolid && trace.fraction == 1.0 )
                                    {
                                        PM_FallToDeath();
                                        cliff_fall = true;
                                    }
                                    */
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (!cliff_fall) {
        if ((pm->ps->legsAnim == animNumber_t::BOTH_WALL_RUN_RIGHT || pm->ps->legsAnim == animNumber_t::BOTH_WALL_RUN_LEFT ||
             pm->ps->legsAnim == animNumber_t::BOTH_WALL_RUN_RIGHT_STOP || pm->ps->legsAnim == animNumber_t::BOTH_WALL_RUN_LEFT_STOP ||
             pm->ps->legsAnim == animNumber_t::BOTH_WALL_RUN_RIGHT_FLIP || pm->ps->legsAnim == animNumber_t::BOTH_WALL_RUN_LEFT_FLIP ||
             pm->ps->legsAnim == animNumber_t::BOTH_WALL_FLIP_RIGHT || pm->ps->legsAnim == animNumber_t::BOTH_WALL_FLIP_LEFT ||
             pm->ps->legsAnim == animNumber_t::BOTH_CEILING_DROP) &&
            !pm->ps->legsAnimTimer) { // if flip anim is done, okay to use inair
            PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_FORCEINAIR1, setAnimFlags_t::SETANIM_FLAG_OVERRIDE, 350); // Only blend over 100ms
        }
        else if (!PM_InRoll(pm->ps) && !PM_SpinningAnim(pm->ps->legsAnim) && !PM_FlippingAnim(pm->ps->legsAnim) && !PM_InSpecialJump(pm->ps->legsAnim)) {
            if (pm->ps->groundEntityNum != ENTITYNUM_NONE) {
                // we just transitioned into freefall
                if (pm->debugLevel) {
                    Q_Println("{}:lift", c_pmove);
                }

                // if they aren't in a jumping animation and the ground is a ways away, force into it
                // if we didn't do the trace, the player would be backflipping down staircases
                VectorCopy(pm->ps->origin, point);
                point[2] -= 64;

                pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask, EG2_Collision::G2_NOCOLLIDE, 0);
                if (trace.fraction == 1.0) { // FIXME: if velocity[2] < 0 and didn't jump, use some falling anim
                    if (pm->ps->velocity[2] <= 0 && !Any(pm->ps->pm_flags & pmoveFlags_t::PMF_JUMP_HELD)) {
                        if (!PM_InDeathAnim()) {
                            vec3_t moveDir, lookAngles, lookDir, lookRight;
                            animNumber_t anim = animNumber_t::BOTH_INAIR1;

                            VectorCopy(pm->ps->velocity, moveDir);
                            moveDir[2] = 0;
                            VectorNormalize(moveDir);

                            VectorCopy(pm->ps->viewangles, lookAngles);
                            lookAngles[PITCH] = lookAngles[ROLL] = 0;
                            AngleVectors(lookAngles, lookDir, lookRight, nullptr);

                            float dot = DotProduct(moveDir, lookDir);
                            if (dot > 0.5) { // redundant
                                anim = animNumber_t::BOTH_INAIR1;
                            }
                            else if (dot < -0.5) {
                                anim = animNumber_t::BOTH_INAIRBACK1;
                            }
                            else {
                                dot = DotProduct(moveDir, lookRight);
                                if (dot > 0.5) {
                                    anim = animNumber_t::BOTH_INAIRRIGHT1;
                                }
                                else if (dot < -0.5) {
                                    anim = animNumber_t::BOTH_INAIRLEFT1;
                                }
                                else { // redundant
                                    anim = animNumber_t::BOTH_INAIR1;
                                }
                            }
                            if (pm->ps->forcePowersActive & Bit(forcePowers_t::FP_LEVITATION)) {
                                anim = PM_ForceJumpAnimForJumpAnim(anim);
                            }
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, anim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE, 100); // Only blend over 100ms
                        }
                        pm->ps->pm_flags &= ~pmoveFlags_t::PMF_BACKWARDS_JUMP;
                    }
                    else if (!(pm->ps->forcePowersActive & Bit(forcePowers_t::FP_LEVITATION))) {
                        if (pm->cmd.forwardmove >= 0) {
                            if (!PM_InDeathAnim()) {
                                PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_JUMP1, setAnimFlags_t::SETANIM_FLAG_OVERRIDE,
                                           100); // Only blend over 100ms
                            }
                            pm->ps->pm_flags &= ~pmoveFlags_t::PMF_BACKWARDS_JUMP;
                        }
                        else if (pm->cmd.forwardmove < 0) {
                            if (!PM_InDeathAnim()) {
                                PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_JUMPBACK1, setAnimFlags_t::SETANIM_FLAG_OVERRIDE,
                                           100); // Only blend over 100ms
                            }
                            pm->ps->pm_flags |= pmoveFlags_t::PMF_BACKWARDS_JUMP;
                        }
                    }
                }
            }
        }
    }

    if (pm->ps->groundEntityNum != ENTITYNUM_NONE) {
        pm->ps->jumpZStart = pm->ps->origin[2];
    }
    pm->ps->groundEntityNum = ENTITYNUM_NONE;
    pml.groundPlane = false;
    pml.walking = false;
}

/*
=============
PM_GroundTrace
=============
*/
static void PM_GroundTrace(void)
{
    vec3_t point;
    trace_t trace;

    if (Any(pm->ps->eFlags & entityFlags_t::EF_LOCKED_TO_WEAPON)) {
        pml.groundPlane = true;
        pml.walking = true;
        pm->ps->groundEntityNum = ENTITYNUM_WORLD;
        pm->ps->lastOnGround = level.time;
        /*
        pml.groundTrace.allsolid = false;
        pml.groundTrace.contents = contentsFlags_t::CONTENTS_SOLID;
        VectorCopy( pm->ps->origin, pml.groundTrace.endpos );
        pml.groundTrace.entityNum = ENTITYNUM_WORLD;
        pml.groundTrace.fraction = 0.0f;;
        pml.groundTrace.G2CollisionMap = nullptr;
        pml.groundTrace.plane.dist = 0.0f;
        VectorSet( pml.groundTrace.plane.normal, 0, 0, 1 );
        pml.groundTrace.plane.pad = 0;
        pml.groundTrace.plane.signbits = 0;
        pml.groundTrace.plane.type = planeType_t::PLANE_X;;
        pml.groundTrace.startsolid = false;
        pml.groundTrace.surfaceFlags = surfaceFlags_t::SURF_NONE;
        */
        return;
    }
    else if (pm->ps->legsAnimTimer > 300 && (pm->ps->legsAnim == animNumber_t::BOTH_WALL_RUN_RIGHT ||
                                             pm->ps->legsAnim == animNumber_t::BOTH_WALL_RUN_LEFT)) { // wall-running forces you to be in the air
        pml.groundPlane = false;
        pml.walking = false;
        pm->ps->groundEntityNum = ENTITYNUM_NONE;
        return;
    }

    point[0] = pm->ps->origin[0];
    point[1] = pm->ps->origin[1];
    point[2] = pm->ps->origin[2] - 0.25;

    pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, point, pm->ps->clientNum, pm->tracemask, EG2_Collision::G2_NOCOLLIDE, 0);
    pml.groundTrace = trace;

    // do something corrective if the trace starts in a solid...
    if (trace.allsolid) {
        PM_CorrectAllSolid();
        return;
    }

    // if the trace didn't hit anything, we are in free fall
    if (trace.fraction == 1.0 || g_gravity->value <= 0) {
        PM_GroundTraceMissed();
        pml.groundPlane = false;
        pml.walking = false;
        return;
    }

    // check if getting thrown off the ground
    if (((pm->ps->velocity[2] > 0 && Any(pm->ps->pm_flags & pmoveFlags_t::PMF_TIME_KNOCKBACK)) || pm->ps->velocity[2] > 100) &&
        DotProduct(pm->ps->velocity, trace.plane.normal) > 10) { // either thrown off ground (PMF_TIME_KNOCKBACK) or going off the ground at a large velocity
        if (pm->debugLevel) {
            Q_Println("{}:kickoff", c_pmove);
        }
        // go into jump animation
        if (PM_FlippingAnim(pm->ps->legsAnim)) { // we're flipping
        }
        else if (PM_InSpecialJump(pm->ps->legsAnim)) { // special jumps
        }
        else if (PM_InKnockDown(pm->ps)) { // in knockdown
        }
        else if (PM_InRoll(pm->ps)) { // in knockdown
        }
        else {
            PM_JumpForDir();
        }

        pm->ps->groundEntityNum = ENTITYNUM_NONE;
        pml.groundPlane = false;
        pml.walking = false;
        return;
    }

    // slopes that are too steep will not be considered onground
    if (trace.plane.normal[2] < MIN_WALK_NORMAL) {
        if (pm->debugLevel) {
            Q_Println("{}:steep", c_pmove);
        }
        // FIXME: if they can't slide down the slope, let them
        // walk (sharp crevices)
        pm->ps->groundEntityNum = ENTITYNUM_NONE;
        pml.groundPlane = true;
        pml.walking = false;
        return;
    }

    // FIXME: if the ground surface is a "cover surface (like tall grass), add a "cover" flag to me
    pml.groundPlane = true;
    pml.walking = true;

    // hitting solid ground will end a waterjump
    if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_TIME_WATERJUMP)) {
        pm->ps->pm_flags &= ~(pmoveFlags_t::PMF_TIME_WATERJUMP | pmoveFlags_t::PMF_TIME_LAND);
        pm->ps->pm_time = 0;
    }

    if (pm->ps->groundEntityNum == ENTITYNUM_NONE) {
        // just hit the ground
        if (pm->debugLevel) {
            Q_Println("{}:Land", c_pmove);
        }

        // if ( !PM_ClientImpact( trace.entityNum, true ) )
        {
            PM_CrashLand();

            // don't do landing time if we were just going down a slope
            if (pml.previous_velocity[2] < -200) {
                // don't allow another jump for a little while
                pm->ps->pm_flags |= pmoveFlags_t::PMF_TIME_LAND;
                pm->ps->pm_time = 250;
            }
            if (!pm->cmd.forwardmove && !pm->cmd.rightmove) {
                pm->ps->velocity[2] = 0; // wouldn't normally want this because of slopes, but we aren't tyring to move...
            }
        }
    }

    pm->ps->groundEntityNum = trace.entityNum;
    pm->ps->lastOnGround = level.time;
    if (!pm->ps->clientNum) { // if a player, clear the jumping "flag" so can't double-jump
        pm->ps->forceJumpCharge = 0;
    }

    // don't reset the z velocity for slopes
    //	pm->ps->velocity[2] = 0;

    PM_AddTouchEnt(trace.entityNum);
}

/*
=============
PM_SetWaterLevelAtPoint	FIXME: avoid this twice?  certainly if not moving
=============
*/
static void PM_SetWaterLevelAtPoint(vec3_t org, int* waterlevel, contentsFlags_t* watertype)
{
    vec3_t point;
    contentsFlags_t cont;
    int sample1;
    int sample2;

    //
    // get waterlevel, accounting for ducking
    //
    *waterlevel = 0;
    *watertype = contentsFlags_t::CONTENTS_NONE;

    point[0] = org[0];
    point[1] = org[1];
    point[2] = org[2] + DEFAULT_MINS_2 + 1;
    cont = pm->pointcontents(point, pm->ps->clientNum);

    if (Any(cont & (MASK_WATER | contentsFlags_t::CONTENTS_LADDER))) {
        sample2 = pm->ps->viewheight - DEFAULT_MINS_2;
        sample1 = sample2 / 2;

        *watertype = cont;
        *waterlevel = 1;
        point[2] = org[2] + DEFAULT_MINS_2 + sample1;
        cont = pm->pointcontents(point, pm->ps->clientNum);
        if (Any(cont & (MASK_WATER | contentsFlags_t::CONTENTS_LADDER))) {
            *waterlevel = 2;
            point[2] = org[2] + DEFAULT_MINS_2 + sample2;
            cont = pm->pointcontents(point, pm->ps->clientNum);
            if (Any(cont & (MASK_WATER | contentsFlags_t::CONTENTS_LADDER))) {
                *waterlevel = 3;
            }
        }
    }
}

void PM_SetWaterHeight(void)
{
    pm->ps->waterHeightLevel = waterHeightLevel_t::WHL_NONE;
    if (pm->waterlevel < 1) {
        pm->ps->waterheight = pm->ps->origin[2] + DEFAULT_MINS_2 - 4;
        return;
    }
    trace_t trace;
    vec3_t top, bottom;

    VectorCopy(pm->ps->origin, top);
    VectorCopy(pm->ps->origin, bottom);
    top[2] += pm->gent->client->standheight;
    bottom[2] += DEFAULT_MINS_2;

    gi.trace(&trace, top, pm->mins, pm->maxs, bottom, pm->ps->clientNum, (contentsFlags_t::CONTENTS_WATER | contentsFlags_t::CONTENTS_SLIME),
             EG2_Collision::G2_NOCOLLIDE, 0);

    if (trace.startsolid) { // under water
        pm->ps->waterheight = top[2] + 4;
    }
    else if (trace.fraction < 1.0f) { // partially in and partially out of water
        pm->ps->waterheight = trace.endpos[2] + pm->mins[2];
    }
    else if (Any(trace.contents & (contentsFlags_t::CONTENTS_WATER | contentsFlags_t::CONTENTS_SLIME))) { // water is above me
        pm->ps->waterheight = top[2] + 4;
    }
    else { // water is below me
        pm->ps->waterheight = bottom[2] - 4;
    }
    float distFromEyes = (pm->ps->origin[2] + pm->gent->client->standheight) - pm->ps->waterheight;

    if (distFromEyes < 0) {
        pm->ps->waterHeightLevel = waterHeightLevel_t::WHL_UNDER;
    }
    else if (distFromEyes < 6) {
        pm->ps->waterHeightLevel = waterHeightLevel_t::WHL_HEAD;
    }
    else if (distFromEyes < 18) {
        pm->ps->waterHeightLevel = waterHeightLevel_t::WHL_SHOULDERS;
    }
    else if (distFromEyes < pm->gent->client->standheight - 8) { // at least 8 above origin
        pm->ps->waterHeightLevel = waterHeightLevel_t::WHL_TORSO;
    }
    else {
        float distFromOrg = pm->ps->origin[2] - pm->ps->waterheight;
        if (distFromOrg < 6) {
            pm->ps->waterHeightLevel = waterHeightLevel_t::WHL_WAIST;
        }
        else if (distFromOrg < 16) {
            pm->ps->waterHeightLevel = waterHeightLevel_t::WHL_KNEES;
        }
        else if (distFromOrg > Q_fabsf(pm->mins[2])) {
            pm->ps->waterHeightLevel = waterHeightLevel_t::WHL_NONE;
        }
        else {
            pm->ps->waterHeightLevel = waterHeightLevel_t::WHL_ANKLES;
        }
    }
}

/*
==============
PM_SetBounds

Sets mins, maxs
==============
*/
static void PM_SetBounds(void)
{
    if (pm->gent && pm->gent->client) {
        if (!pm->gent->mins[0] || !pm->gent->mins[1] || !pm->gent->mins[2] || !pm->gent->maxs[0] || !pm->gent->maxs[1] || !pm->gent->maxs[2]) {
            // assert(0);
        }

        VectorCopy(pm->gent->mins, pm->mins);
        VectorCopy(pm->gent->maxs, pm->maxs);
    }
    else {
        if (!DEFAULT_MINS_0 || !DEFAULT_MINS_1 || !DEFAULT_MAXS_0 || !DEFAULT_MAXS_1 || !DEFAULT_MINS_2 || !DEFAULT_MAXS_2) {
            assert(0);
        }

        pm->mins[0] = DEFAULT_MINS_0;
        pm->mins[1] = DEFAULT_MINS_1;

        pm->maxs[0] = DEFAULT_MAXS_0;
        pm->maxs[1] = DEFAULT_MAXS_1;

        pm->mins[2] = DEFAULT_MINS_2;
        pm->maxs[2] = DEFAULT_MAXS_2;
    }
}

/*
==============
PM_CheckDuck

Sets mins, maxs, and pm->ps->viewheight
==============
*/
static void PM_CheckDuck(void)
{
    trace_t trace;
    int standheight;
    int crouchheight;
    int oldHeight;

    if (pm->gent && pm->gent->client) {
        if (!pm->gent->mins[0] || !pm->gent->mins[1] || !pm->gent->mins[2] || !pm->gent->maxs[0] || !pm->gent->maxs[1] || !pm->gent->maxs[2]) {
            // assert(0);
        }

        if (!pm->ps->clientNum && pm->gent->client->NPC_class == class_t::CLASS_ATST && !cg.renderingThirdPerson) {
            standheight = crouchheight = 128;
        }
        else {
            standheight = pm->gent->client->standheight;
            crouchheight = pm->gent->client->crouchheight;
        }
    }
    else {
        if (!DEFAULT_MINS_0 || !DEFAULT_MINS_1 || !DEFAULT_MAXS_0 || !DEFAULT_MAXS_1 || !DEFAULT_MINS_2 || !DEFAULT_MAXS_2) {
            assert(0);
        }

        standheight = DEFAULT_MAXS_2;
        crouchheight = CROUCH_MAXS_2;
    }

    if (PM_InGetUp(pm->ps)) { // can't do any kind of crouching when getting up
        if (pm->ps->legsAnim == animNumber_t::BOTH_GETUP_CROUCH_B1 || pm->ps->legsAnim == animNumber_t::BOTH_GETUP_CROUCH_F1) { // crouched still
            pm->ps->pm_flags |= pmoveFlags_t::PMF_DUCKED;
            pm->maxs[2] = crouchheight;
        }
        pm->ps->viewheight = crouchheight + STANDARD_VIEWHEIGHT_OFFSET;
        return;
    }

    oldHeight = pm->maxs[2];

    if (PM_InRoll(pm->ps)) {
        /*
        if ( pm->ps->clientNum && pm->gent && pm->gent->client )
        {
            pm->maxs[2] = pm->gent->client->renderInfo.eyePoint[2]-pm->ps->origin[2] + 4;
            if ( crouchheight > pm->maxs[2] )
            {
                pm->maxs[2] = crouchheight;
            }
        }
        else
        */
        {
            pm->maxs[2] = crouchheight;
        }
        pm->ps->viewheight = crouchheight + STANDARD_VIEWHEIGHT_OFFSET;
        pm->ps->pm_flags |= pmoveFlags_t::PMF_DUCKED;
        return;
    }
    if (PM_GettingUpFromKnockDown(standheight, crouchheight)) {
        pm->ps->viewheight = crouchheight + STANDARD_VIEWHEIGHT_OFFSET;
        return;
    }
    if (PM_InKnockDown(pm->ps)) {           // forced crouch
        if (pm->gent && pm->gent->client) { // interrupted any potential delayed weapon fires
            pm->gent->client->fireDelay = 0;
        }
        pm->maxs[2] = crouchheight;
        pm->ps->viewheight = crouchheight + STANDARD_VIEWHEIGHT_OFFSET;
        pm->ps->pm_flags |= pmoveFlags_t::PMF_DUCKED;
        return;
    }
    if (pm->cmd.upmove < 0) { // trying to duck
        pm->maxs[2] = crouchheight;
        pm->ps->viewheight = crouchheight + STANDARD_VIEWHEIGHT_OFFSET;                        // CROUCH_VIEWHEIGHT;
        if (pm->ps->groundEntityNum == ENTITYNUM_NONE && !PM_SwimmingAnim(pm->ps->legsAnim)) { // Not ducked already and trying to duck in mid-air
            // will raise your feet, unducking whilst in air will drop feet
            if (!Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) {
                pm->ps->eFlags ^= entityFlags_t::EF_TELEPORT_BIT;
            }
            if (pm->gent) {
                pm->ps->origin[2] += oldHeight - pm->maxs[2]; // diff will be zero if were already ducking
                // Don't worry, we know we fit in a smaller size
            }
        }
        pm->ps->pm_flags |= pmoveFlags_t::PMF_DUCKED;
        if (d_JediAI->integer) {
            if (pm->ps->clientNum && pm->ps->weapon == weapon_t::WP_SABER) {
                Q_Println("ducking");
            }
        }
    }
    else {                                                      // want to stop ducking, stand up if possible
        if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) { // Was ducking
            if (pm->ps->groundEntityNum == ENTITYNUM_NONE) {    // unducking whilst in air will try to drop feet
                pm->maxs[2] = standheight;
                pm->ps->origin[2] += oldHeight - pm->maxs[2];
                pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin, pm->ps->clientNum, pm->tracemask, EG2_Collision::G2_NOCOLLIDE, 0);
                if (!trace.allsolid) {
                    pm->ps->eFlags ^= entityFlags_t::EF_TELEPORT_BIT;
                    pm->ps->pm_flags &= ~pmoveFlags_t::PMF_DUCKED;
                }
                else { // Put us back
                    pm->ps->origin[2] -= oldHeight - pm->maxs[2];
                }
                // NOTE: this isn't the best way to check this, you may have room to unduck
                // while in air, but your feet are close to landing.  Probably won't be a
                // noticable shortcoming
            }
            else {
                // try to stand up
                pm->maxs[2] = standheight;
                pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin, pm->ps->clientNum, pm->tracemask, EG2_Collision::G2_NOCOLLIDE, 0);
                if (!trace.allsolid) {
                    pm->ps->pm_flags &= ~pmoveFlags_t::PMF_DUCKED;
                }
            }
        }

        if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) { // Still ducking
            pm->maxs[2] = crouchheight;
            pm->ps->viewheight = crouchheight + STANDARD_VIEWHEIGHT_OFFSET; // CROUCH_VIEWHEIGHT;
        }
        else { // standing now
            pm->maxs[2] = standheight;
            // FIXME: have a crouchviewheight and standviewheight on ent?
            pm->ps->viewheight = standheight + STANDARD_VIEWHEIGHT_OFFSET; // DEFAULT_VIEWHEIGHT;
        }
    }
}

//===================================================================
bool PM_SaberLockAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_BF2LOCK:       // #
    case animNumber_t::BOTH_BF1LOCK:       // #
    case animNumber_t::BOTH_CWCIRCLELOCK:  // #
    case animNumber_t::BOTH_CCWCIRCLELOCK: // #
        return true;
    default:
        break;
    }
    return false;
}

bool PM_ForceAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_CHOKE1:         // being choked...???
    case animNumber_t::BOTH_GESTURE1:       // taunting...
    case animNumber_t::BOTH_RESISTPUSH:     // # plant yourself to resist force push/pulls.
    case animNumber_t::BOTH_FORCEPUSH:      // # Use off-hand to do force power.
    case animNumber_t::BOTH_FORCEPULL:      // # Use off-hand to do force power.
    case animNumber_t::BOTH_MINDTRICK1:     // # Use off-hand to do mind trick
    case animNumber_t::BOTH_MINDTRICK2:     // # Use off-hand to do distraction
    case animNumber_t::BOTH_FORCELIGHTNING: // # Use off-hand to do lightning
    case animNumber_t::BOTH_FORCELIGHTNING_START:
    case animNumber_t::BOTH_FORCELIGHTNING_HOLD:    // # Use off-hand to do lightning
    case animNumber_t::BOTH_FORCELIGHTNING_RELEASE: // # Use off-hand to do lightning
    case animNumber_t::BOTH_FORCEHEAL_START:        // # Healing meditation pose start
    case animNumber_t::BOTH_FORCEHEAL_STOP:         // # Healing meditation pose end
    case animNumber_t::BOTH_FORCEHEAL_QUICK:        // # Healing meditation gesture
    case animNumber_t::BOTH_FORCEGRIP1:             // # temp force-grip anim (actually re-using push)
    case animNumber_t::BOTH_FORCEGRIP_HOLD:         // # temp force-grip anim (actually re-using push)
    case animNumber_t::BOTH_FORCEGRIP_RELEASE:      // # temp force-grip anim (actually re-using push)
        // case animNumber_t::BOTH_FORCEGRIP3:		//# force-gripping
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool PM_InSaberAnim(animNumber_t anim)
{
    if (anim >= animNumber_t::BOTH_A1_T__B_ && anim <= animNumber_t::BOTH_H1_S1_BR) {
        return true;
    }
    return false;
}

bool PM_InForceGetUp(playerState_t* ps)
{
    switch (ps->legsAnim) {
    case animNumber_t::BOTH_FORCE_GETUP_F1:
    case animNumber_t::BOTH_FORCE_GETUP_F2:
    case animNumber_t::BOTH_FORCE_GETUP_B1:
    case animNumber_t::BOTH_FORCE_GETUP_B2:
    case animNumber_t::BOTH_FORCE_GETUP_B3:
    case animNumber_t::BOTH_FORCE_GETUP_B4:
    case animNumber_t::BOTH_FORCE_GETUP_B5:
    case animNumber_t::BOTH_FORCE_GETUP_B6:
        if (ps->legsAnimTimer) {
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

bool PM_InGetUp(playerState_t* ps)
{
    switch (ps->legsAnim) {
    case animNumber_t::BOTH_GETUP1:
    case animNumber_t::BOTH_GETUP2:
    case animNumber_t::BOTH_GETUP3:
    case animNumber_t::BOTH_GETUP4:
    case animNumber_t::BOTH_GETUP5:
    case animNumber_t::BOTH_GETUP_CROUCH_F1:
    case animNumber_t::BOTH_GETUP_CROUCH_B1:
        if (ps->legsAnimTimer) {
            return true;
        }
        break;
    default:
        return PM_InForceGetUp(ps);
        break;
    }
    // what the hell, redundant, but...
    return false;
}

bool PM_InKnockDown(playerState_t* ps)
{
    switch (ps->legsAnim) {
    case animNumber_t::BOTH_KNOCKDOWN1:
    case animNumber_t::BOTH_KNOCKDOWN2:
    case animNumber_t::BOTH_KNOCKDOWN3:
    case animNumber_t::BOTH_KNOCKDOWN4:
    case animNumber_t::BOTH_KNOCKDOWN5:
        return true;
        break;
    default:
        return PM_InGetUp(ps);
        break;
    }
}

bool PM_InKnockDownOnGround(playerState_t* ps)
{
    switch (ps->legsAnim) {
    case animNumber_t::BOTH_KNOCKDOWN1:
    case animNumber_t::BOTH_KNOCKDOWN2:
    case animNumber_t::BOTH_KNOCKDOWN3:
    case animNumber_t::BOTH_KNOCKDOWN4:
    case animNumber_t::BOTH_KNOCKDOWN5:
        // if ( PM_AnimLength( g_entities[ps->clientNum].client->clientInfo.animFileIndex, ps->legsAnim ) - ps->legsAnimTimer > 300 )
        { // at end of fall down anim
            return true;
        }
        break;
    case animNumber_t::BOTH_GETUP1:
    case animNumber_t::BOTH_GETUP2:
    case animNumber_t::BOTH_GETUP3:
    case animNumber_t::BOTH_GETUP4:
    case animNumber_t::BOTH_GETUP5:
    case animNumber_t::BOTH_GETUP_CROUCH_F1:
    case animNumber_t::BOTH_GETUP_CROUCH_B1:
    case animNumber_t::BOTH_FORCE_GETUP_F1:
    case animNumber_t::BOTH_FORCE_GETUP_F2:
    case animNumber_t::BOTH_FORCE_GETUP_B1:
    case animNumber_t::BOTH_FORCE_GETUP_B2:
    case animNumber_t::BOTH_FORCE_GETUP_B3:
    case animNumber_t::BOTH_FORCE_GETUP_B4:
    case animNumber_t::BOTH_FORCE_GETUP_B5:
    case animNumber_t::BOTH_FORCE_GETUP_B6:
        if (PM_AnimLength(g_entities[ps->clientNum].client->clientInfo.animFileIndex, ps->legsAnim) - ps->legsAnimTimer < 500) { // at beginning of getup anim
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

bool PM_CrouchGetup(float crouchheight)
{
    pm->maxs[2] = crouchheight;
    pm->ps->viewheight = crouchheight + STANDARD_VIEWHEIGHT_OFFSET;
    std::optional<animNumber_t> anim;
    switch (pm->ps->legsAnim) {
    case animNumber_t::BOTH_KNOCKDOWN1:
    case animNumber_t::BOTH_KNOCKDOWN2:
    case animNumber_t::BOTH_KNOCKDOWN4:
        anim = animNumber_t::BOTH_GETUP_CROUCH_B1;
        break;
    case animNumber_t::BOTH_KNOCKDOWN3:
    case animNumber_t::BOTH_KNOCKDOWN5:
        anim = animNumber_t::BOTH_GETUP_CROUCH_F1;
        break;
    default:
        break;
    }
    if (!anim) {                     // WTF? stay down?
        pm->ps->legsAnimTimer = 100; // hold this anim for another 10th of a second
        return false;
    }
    else { // get up into crouch anim
        PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, *anim,
                   setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_HOLDLESS);
        pm->ps->saberMove = pm->ps->saberBounceMove = saberMoveName_t::LS_READY; // don't finish whatever saber anim you may have been in
        pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_NONE;
        return true;
    }
}

bool PM_GettingUpFromKnockDown(float standheight, float crouchheight)
{
    animNumber_t legsAnim = pm->ps->legsAnim;
    if (legsAnim == animNumber_t::BOTH_KNOCKDOWN1 || legsAnim == animNumber_t::BOTH_KNOCKDOWN2 || legsAnim == animNumber_t::BOTH_KNOCKDOWN3 ||
        legsAnim == animNumber_t::BOTH_KNOCKDOWN4 || legsAnim == animNumber_t::BOTH_KNOCKDOWN5) { // in a knockdown
        if (!pm->ps->legsAnimTimer) { // done with the knockdown - FIXME: somehow this is allowing an *instant* getup...???
            // FIXME: if trying to crouch (holding button?), just get up into a crouch?
            if (pm->cmd.upmove < 0) {
                return PM_CrouchGetup(crouchheight);
            }
            else {
                trace_t trace;
                // try to stand up
                pm->maxs[2] = standheight;
                pm->trace(&trace, pm->ps->origin, pm->mins, pm->maxs, pm->ps->origin, pm->ps->clientNum, pm->tracemask, EG2_Collision::G2_NOCOLLIDE, 0);
                if (!trace.allsolid) { // stand up
                    bool forceGetUp = false;
                    animNumber_t anim = animNumber_t::BOTH_GETUP1;
                    pm->maxs[2] = standheight;
                    pm->ps->viewheight = standheight + STANDARD_VIEWHEIGHT_OFFSET;
                    // NOTE: the force power checks will stop fencers and grunts from getting up using force jump
                    switch (pm->ps->legsAnim) {
                    case animNumber_t::BOTH_KNOCKDOWN1:
                        if ((pm->ps->clientNum && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_0) ||
                            (!pm->ps->clientNum && pm->cmd.upmove > 0 &&
                             pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1)) {
                            anim = Q_irand(animNumber_t::BOTH_FORCE_GETUP_B1,
                                           animNumber_t::BOTH_FORCE_GETUP_B6); // NOTE: BOTH_FORCE_GETUP_B5 takes soe steps forward at end
                            forceGetUp = true;
                        }
                        else {
                            anim = animNumber_t::BOTH_GETUP1;
                        }
                        break;
                    case animNumber_t::BOTH_KNOCKDOWN2:
                        if ((pm->ps->clientNum && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_0) ||
                            (!pm->ps->clientNum && pm->cmd.upmove > 0 &&
                             pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1)) {
                            anim = Q_irand(animNumber_t::BOTH_FORCE_GETUP_B1,
                                           animNumber_t::BOTH_FORCE_GETUP_B6); // NOTE: BOTH_FORCE_GETUP_B5 takes soe steps forward at end
                            forceGetUp = true;
                        }
                        else {
                            anim = animNumber_t::BOTH_GETUP2;
                        }
                        break;
                    case animNumber_t::BOTH_KNOCKDOWN3:
                        if ((pm->ps->clientNum && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_0) ||
                            (!pm->ps->clientNum && pm->cmd.upmove > 0 &&
                             pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1)) {
                            anim = Q_irand(animNumber_t::BOTH_FORCE_GETUP_F1, animNumber_t::BOTH_FORCE_GETUP_F2);
                            forceGetUp = true;
                        }
                        else {
                            anim = animNumber_t::BOTH_GETUP3;
                        }
                        break;
                    case animNumber_t::BOTH_KNOCKDOWN4:
                        if ((pm->ps->clientNum && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_0) ||
                            (!pm->ps->clientNum && pm->cmd.upmove > 0 &&
                             pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1)) {
                            anim = Q_irand(animNumber_t::BOTH_FORCE_GETUP_B1,
                                           animNumber_t::BOTH_FORCE_GETUP_B6); // NOTE: BOTH_FORCE_GETUP_B5 takes soe steps forward at end
                            forceGetUp = true;
                        }
                        else {
                            anim = animNumber_t::BOTH_GETUP4;
                        }
                        break;
                    case animNumber_t::BOTH_KNOCKDOWN5:
                        if ((pm->ps->clientNum && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_0) ||
                            (!pm->ps->clientNum && pm->cmd.upmove > 0 &&
                             pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1)) {
                            anim = Q_irand(animNumber_t::BOTH_FORCE_GETUP_F1, animNumber_t::BOTH_FORCE_GETUP_F2);
                            forceGetUp = true;
                        }
                        else {
                            anim = animNumber_t::BOTH_GETUP5;
                        }
                        break;
                    default:
                        break;
                    }
                    // Q_Println("getupanim = {}", BG_AnimName( anim ));
                    if (forceGetUp) {
                        if (pm->gent && pm->gent->client && pm->gent->client->playerTeam == team_t::TEAM_ENEMY && pm->gent->NPC &&
                            pm->gent->NPC->blockedSpeechDebounceTime < level.time && !Q_irand(0, 1)) {
                            PM_AddEvent(Q_irand(entity_event_t::EV_COMBAT1, entity_event_t::EV_COMBAT3));
                            pm->gent->NPC->blockedSpeechDebounceTime = level.time + 1000;
                        }
                        G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_BODY, "sound/weapons/force/jump.wav");
                        // launch off ground?
                        pm->ps->weaponTime = 300; // just to make sure it's cleared
                    }
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, anim,
                               setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_HOLDLESS);
                    pm->ps->saberMove = pm->ps->saberBounceMove = saberMoveName_t::LS_READY; // don't finish whatever saber anim you may have been in
                    pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_NONE;
                    return true;
                }
                else {
                    return PM_CrouchGetup(crouchheight);
                }
            }
        }
    }
    return false;
}

void PM_CmdForRoll(animNumber_t anim, usercmd_t* pCmd)
{
    switch (anim) {
    case animNumber_t::BOTH_ROLL_F:
        pCmd->forwardmove = 127;
        pCmd->rightmove = 0;
        break;
    case animNumber_t::BOTH_ROLL_B:
        pCmd->forwardmove = -127;
        pCmd->rightmove = 0;
        break;
    case animNumber_t::BOTH_ROLL_R:
        pCmd->forwardmove = 0;
        pCmd->rightmove = 127;
        break;
    case animNumber_t::BOTH_ROLL_L:
        pCmd->forwardmove = 0;
        pCmd->rightmove = -127;
        break;
    default:
        break;
    }
    pCmd->upmove = 0;
}

bool PM_InRoll(playerState_t* ps)
{
    switch (ps->legsAnim) {
    case animNumber_t::BOTH_ROLL_F:
    case animNumber_t::BOTH_ROLL_B:
    case animNumber_t::BOTH_ROLL_R:
    case animNumber_t::BOTH_ROLL_L:
        if (ps->legsAnimTimer) {
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

bool PM_CrouchAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_SIT1:              // # Normal chair sit.
    case animNumber_t::BOTH_SIT2:              // # Lotus position.
    case animNumber_t::BOTH_SIT3:              // # Sitting in tired position: elbows on knees
    case animNumber_t::BOTH_SIT2TO3:           // # Trans from sit2 to sit3?
    case animNumber_t::BOTH_SIT3TO1:           // # Trans from sit3 to sit1?
    case animNumber_t::BOTH_SIT3TO2:           // # Trans from sit3 to sit2?
    case animNumber_t::BOTH_SIT4TO5:           // # Trans from sit4 to sit5
    case animNumber_t::BOTH_SIT4TO6:           // # Trans from sit4 to sit6
    case animNumber_t::BOTH_SIT5TO4:           // # Trans from sit5 to sit4
    case animNumber_t::BOTH_SIT5TO6:           // # Trans from sit5 to sit6
    case animNumber_t::BOTH_SIT6TO4:           // # Trans from sit6 to sit4
    case animNumber_t::BOTH_SIT6TO5:           // # Trans from sit6 to sit5
    case animNumber_t::BOTH_SIT7:              // # sitting with arms over knees: no weapon
    case animNumber_t::BOTH_CROUCH1:           // # Transition from standing to crouch
    case animNumber_t::BOTH_CROUCH1IDLE:       // # Crouching idle
    case animNumber_t::BOTH_CROUCH1WALK:       // # Walking while crouched
    case animNumber_t::BOTH_CROUCH1WALKBACK:   // # Walking while crouched
    case animNumber_t::BOTH_CROUCH2IDLE:       // # crouch and resting on back righ heel: no weapon
    case animNumber_t::BOTH_CROUCH2TOSTAND1:   // # going from crouch2 to stand1
    case animNumber_t::BOTH_CROUCH3:           // # Desann crouching down to Kyle (cin 9)
    case animNumber_t::BOTH_KNEES1:            // # Tavion on her knees
    case animNumber_t::LEGS_CRLEAN_LEFT1:      // # Crouch Lean left
    case animNumber_t::LEGS_CRLEAN_RIGHT1:     // # Crouch Lean Right
    case animNumber_t::BOTH_CROUCHATTACKBACK1: // FIXME: not if in middle of anim?
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool PM_PainAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_PAIN1:  // # First take pain anim
    case animNumber_t::BOTH_PAIN2:  // # Second take pain anim
    case animNumber_t::BOTH_PAIN3:  // # Third take pain anim
    case animNumber_t::BOTH_PAIN4:  // # Fourth take pain anim
    case animNumber_t::BOTH_PAIN5:  // # Fifth take pain anim - from behind
    case animNumber_t::BOTH_PAIN6:  // # Sixth take pain anim - from behind
    case animNumber_t::BOTH_PAIN7:  // # Seventh take pain anim - from behind
    case animNumber_t::BOTH_PAIN8:  // # Eigth take pain anim - from behind
    case animNumber_t::BOTH_PAIN9:  // #
    case animNumber_t::BOTH_PAIN10: // #
    case animNumber_t::BOTH_PAIN11: // #
    case animNumber_t::BOTH_PAIN12: // #
    case animNumber_t::BOTH_PAIN13: // #
    case animNumber_t::BOTH_PAIN14: // #
    case animNumber_t::BOTH_PAIN15: // #
    case animNumber_t::BOTH_PAIN16: // #
    case animNumber_t::BOTH_PAIN17: // #
    case animNumber_t::BOTH_PAIN18: // #
    case animNumber_t::BOTH_PAIN19: // #
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool PM_DodgeAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_DODGE_FL: // # lean-dodge forward left
    case animNumber_t::BOTH_DODGE_FR: // # lean-dodge forward right
    case animNumber_t::BOTH_DODGE_BL: // # lean-dodge backwards left
    case animNumber_t::BOTH_DODGE_BR: // # lean-dodge backwards right
    case animNumber_t::BOTH_DODGE_L:  // # lean-dodge left
    case animNumber_t::BOTH_DODGE_R:  // # lean-dodge right
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool PM_JumpingAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_JUMP1:            // # Jump - wind-up and leave ground
    case animNumber_t::BOTH_INAIR1:           // # In air loop (from jump)
    case animNumber_t::BOTH_LAND1:            // # Landing (from in air loop)
    case animNumber_t::BOTH_LAND2:            // # Landing Hard (from a great height)
    case animNumber_t::BOTH_JUMPBACK1:        // # Jump backwards - wind-up and leave ground
    case animNumber_t::BOTH_INAIRBACK1:       // # In air loop (from jump back)
    case animNumber_t::BOTH_LANDBACK1:        // # Landing backwards(from in air loop)
    case animNumber_t::BOTH_JUMPLEFT1:        // # Jump left - wind-up and leave ground
    case animNumber_t::BOTH_INAIRLEFT1:       // # In air loop (from jump left)
    case animNumber_t::BOTH_LANDLEFT1:        // # Landing left(from in air loop)
    case animNumber_t::BOTH_JUMPRIGHT1:       // # Jump right - wind-up and leave ground
    case animNumber_t::BOTH_INAIRRIGHT1:      // # In air loop (from jump right)
    case animNumber_t::BOTH_LANDRIGHT1:       // # Landing right(from in air loop)
    case animNumber_t::BOTH_FORCEJUMP1:       // # Jump - wind-up and leave ground
    case animNumber_t::BOTH_FORCEINAIR1:      // # In air loop (from jump)
    case animNumber_t::BOTH_FORCELAND1:       // # Landing (from in air loop)
    case animNumber_t::BOTH_FORCEJUMPBACK1:   // # Jump backwards - wind-up and leave ground
    case animNumber_t::BOTH_FORCEINAIRBACK1:  // # In air loop (from jump back)
    case animNumber_t::BOTH_FORCELANDBACK1:   // # Landing backwards(from in air loop)
    case animNumber_t::BOTH_FORCEJUMPLEFT1:   // # Jump left - wind-up and leave ground
    case animNumber_t::BOTH_FORCEINAIRLEFT1:  // # In air loop (from jump left)
    case animNumber_t::BOTH_FORCELANDLEFT1:   // # Landing left(from in air loop)
    case animNumber_t::BOTH_FORCEJUMPRIGHT1:  // # Jump right - wind-up and leave ground
    case animNumber_t::BOTH_FORCEINAIRRIGHT1: // # In air loop (from jump right)
    case animNumber_t::BOTH_FORCELANDRIGHT1:  // # Landing right(from in air loop)
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool PM_LandingAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_LAND1:           // # Landing (from in air loop)
    case animNumber_t::BOTH_LAND2:           // # Landing Hard (from a great height)
    case animNumber_t::BOTH_LANDBACK1:       // # Landing backwards(from in air loop)
    case animNumber_t::BOTH_LANDLEFT1:       // # Landing left(from in air loop)
    case animNumber_t::BOTH_LANDRIGHT1:      // # Landing right(from in air loop)
    case animNumber_t::BOTH_FORCELAND1:      // # Landing (from in air loop)
    case animNumber_t::BOTH_FORCELANDBACK1:  // # Landing backwards(from in air loop)
    case animNumber_t::BOTH_FORCELANDLEFT1:  // # Landing left(from in air loop)
    case animNumber_t::BOTH_FORCELANDRIGHT1: // # Landing right(from in air loop)
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool PM_FlippingAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_FLIP_F: // # Flip forward
    case animNumber_t::BOTH_FLIP_B: // # Flip backwards
    case animNumber_t::BOTH_FLIP_L: // # Flip left
    case animNumber_t::BOTH_FLIP_R: // # Flip right
    case animNumber_t::BOTH_WALL_RUN_RIGHT_FLIP:
    case animNumber_t::BOTH_WALL_RUN_LEFT_FLIP:
    case animNumber_t::BOTH_WALL_FLIP_RIGHT:
    case animNumber_t::BOTH_WALL_FLIP_LEFT:
    case animNumber_t::BOTH_FLIP_BACK1:
    case animNumber_t::BOTH_FLIP_BACK2:
    case animNumber_t::BOTH_FLIP_BACK3:
    case animNumber_t::BOTH_WALL_FLIP_BACK1:
    // Not really flips, but...
    case animNumber_t::BOTH_WALL_RUN_RIGHT:
    case animNumber_t::BOTH_WALL_RUN_LEFT:
    case animNumber_t::BOTH_WALL_RUN_RIGHT_STOP:
    case animNumber_t::BOTH_WALL_RUN_LEFT_STOP:
    case animNumber_t::BOTH_BUTTERFLY_LEFT:
    case animNumber_t::BOTH_BUTTERFLY_RIGHT:
    //
    case animNumber_t::BOTH_ARIAL_LEFT:
    case animNumber_t::BOTH_ARIAL_RIGHT:
    case animNumber_t::BOTH_ARIAL_F1:
    case animNumber_t::BOTH_CARTWHEEL_LEFT:
    case animNumber_t::BOTH_CARTWHEEL_RIGHT:
    case animNumber_t::BOTH_JUMPFLIPSLASHDOWN1:
    case animNumber_t::BOTH_JUMPFLIPSTABDOWN:
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool PM_WalkingAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_WALK1:      // # Normal walk
    case animNumber_t::BOTH_WALK2:      // # Normal walk
    case animNumber_t::BOTH_WALK3:      // # Goes with stand3
    case animNumber_t::BOTH_WALK4:      // # Walk cycle goes to a stand4
    case animNumber_t::BOTH_WALK5:      // # Tavion taunting Kyle (cin 22)
    case animNumber_t::BOTH_WALK6:      // # Slow walk for Luke (cin 12)
    case animNumber_t::BOTH_WALK7:      // # Fast walk
    case animNumber_t::BOTH_WALKTORUN1: // # transition from walk to run
    case animNumber_t::BOTH_WALKBACK1:  // # Walk1 backwards
    case animNumber_t::BOTH_WALKBACK2:  // # Walk2 backwards
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool PM_RunningAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_RUN1:
    case animNumber_t::BOTH_RUN2:
    case animNumber_t::BOTH_RUNBACK1:
    case animNumber_t::BOTH_RUNBACK2:
    case animNumber_t::BOTH_WALKTORUN1:       // # transition from walk to run
    case animNumber_t::BOTH_RUN1START:        // # Start into full run1
    case animNumber_t::BOTH_RUN1STOP:         // # Stop from full run1
    case animNumber_t::BOTH_RUNINJURED1:      // # Run with injured left leg
    case animNumber_t::BOTH_RUNSTRAFE_LEFT1:  // # Sidestep left: should loop
    case animNumber_t::BOTH_RUNSTRAFE_RIGHT1: // # Sidestep right: should loop
    case animNumber_t::BOTH_RUNAWAY1:         // # Running scared
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool PM_RollingAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_ROLL_F:  // # Roll forward
    case animNumber_t::BOTH_ROLL_B:  // # Roll backward
    case animNumber_t::BOTH_ROLL_L:  // # Roll left
    case animNumber_t::BOTH_ROLL_R:  // # Roll right
    case animNumber_t::BOTH_ROLL_FR: // # Roll forward right
    case animNumber_t::BOTH_ROLL_FL: // # Roll forward left
    case animNumber_t::BOTH_ROLL_BR: // # Roll back right
    case animNumber_t::BOTH_ROLL_BL: // # Roll back left
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool PM_SwimmingAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_SWIM1:       // # Swimming
    case animNumber_t::BOTH_SWIM_IDLE1:  // # Swimming Idle 1
    case animNumber_t::BOTH_SWIMFORWARD: // # Swim forward loop
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool PM_SpinningSaberAnim(animNumber_t anim)
{
    switch (anim) {
    // level 1 - FIXME: level 1 will have *no* spins
    case animNumber_t::BOTH_T1_BR_BL:
    case animNumber_t::BOTH_T1__R__L:
    case animNumber_t::BOTH_T1__R_BL:
    case animNumber_t::BOTH_T1_TR_BL:
    case animNumber_t::BOTH_T1_BR_TL:
    case animNumber_t::BOTH_T1_BR__L:
    case animNumber_t::BOTH_T1_TL_BR:
    case animNumber_t::BOTH_T1__L_BR:
    case animNumber_t::BOTH_T1__L__R:
    case animNumber_t::BOTH_T1_BL_BR:
    case animNumber_t::BOTH_T1_BL__R:
    case animNumber_t::BOTH_T1_BL_TR:
    // level 2
    case animNumber_t::BOTH_T2_BR__L:
    case animNumber_t::BOTH_T2_BR_BL:
    case animNumber_t::BOTH_T2__R_BL:
    case animNumber_t::BOTH_T2__L_BR:
    case animNumber_t::BOTH_T2_BL_BR:
    case animNumber_t::BOTH_T2_BL__R:
    // level 3
    case animNumber_t::BOTH_T3_BR__L:
    case animNumber_t::BOTH_T3_BR_BL:
    case animNumber_t::BOTH_T3__R_BL:
    case animNumber_t::BOTH_T3__L_BR:
    case animNumber_t::BOTH_T3_BL_BR:
    case animNumber_t::BOTH_T3_BL__R:
    // level 4
    case animNumber_t::BOTH_T4_BR__L:
    case animNumber_t::BOTH_T4_BR_BL:
    case animNumber_t::BOTH_T4__R_BL:
    case animNumber_t::BOTH_T4__L_BR:
    case animNumber_t::BOTH_T4_BL_BR:
    case animNumber_t::BOTH_T4_BL__R:
    // level 5
    case animNumber_t::BOTH_T5_BR_BL:
    case animNumber_t::BOTH_T5__R__L:
    case animNumber_t::BOTH_T5__R_BL:
    case animNumber_t::BOTH_T5_TR_BL:
    case animNumber_t::BOTH_T5_BR_TL:
    case animNumber_t::BOTH_T5_BR__L:
    case animNumber_t::BOTH_T5_TL_BR:
    case animNumber_t::BOTH_T5__L_BR:
    case animNumber_t::BOTH_T5__L__R:
    case animNumber_t::BOTH_T5_BL_BR:
    case animNumber_t::BOTH_T5_BL__R:
    case animNumber_t::BOTH_T5_BL_TR:
    // special
    // case animNumber_t::BOTH_A2_STABBACK1:
    case animNumber_t::BOTH_ATTACK_BACK:
    case animNumber_t::BOTH_CROUCHATTACKBACK1:
    case animNumber_t::BOTH_BUTTERFLY_LEFT:
    case animNumber_t::BOTH_BUTTERFLY_RIGHT:
    case animNumber_t::BOTH_FJSS_TR_BL:
    case animNumber_t::BOTH_FJSS_TL_BR:
    case animNumber_t::BOTH_JUMPFLIPSLASHDOWN1:
    case animNumber_t::BOTH_JUMPFLIPSTABDOWN:
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool PM_SpinningAnim(animNumber_t anim)
{
    /*
    switch ( anim )
    {
    //FIXME: list any other spinning anims
    default:
        break;
    }
    */
    return PM_SpinningSaberAnim(anim);
}

void PM_ResetAnkleAngles(void)
{
    if (!pm->gent || !pm->gent->client || pm->gent->client->NPC_class != class_t::CLASS_ATST) {
        return;
    }
    if (pm->gent->footLBone != -1) {
        gi.G2API_SetBoneAnglesIndex(&pm->gent->ghoul2[0], pm->gent->footLBone, vec3_origin, boneFlags_t::BONE_ANGLES_POSTMULT, Eorientations::POSITIVE_Z,
                                    Eorientations::NEGATIVE_Y, Eorientations::NEGATIVE_X, nullptr, 0, 0);
    }
    if (pm->gent->footRBone != -1) {
        gi.G2API_SetBoneAnglesIndex(&pm->gent->ghoul2[0], pm->gent->footRBone, vec3_origin, boneFlags_t::BONE_ANGLES_POSTMULT, Eorientations::POSITIVE_Z,
                                    Eorientations::NEGATIVE_Y, Eorientations::NEGATIVE_X, nullptr, 0, 0);
    }
}

void PM_AnglesForSlope(const float yaw, const vec3_t slope, vec3_t angles)
{
    vec3_t nvf, ovf, ovr, new_angles;
    float pitch, mod, dot;

    VectorSet(angles, 0, yaw, 0);
    AngleVectors(angles, ovf, ovr, nullptr);

    vectoangles(slope, new_angles);
    pitch = new_angles[PITCH] + 90;
    new_angles[ROLL] = new_angles[PITCH] = 0;

    AngleVectors(new_angles, nvf, nullptr, nullptr);

    mod = DotProduct(nvf, ovr);

    if (mod < 0)
        mod = -1;
    else
        mod = 1;

    dot = DotProduct(nvf, ovf);

    angles[YAW] = 0;
    angles[PITCH] = dot * pitch;
    angles[ROLL] = ((1 - Q_fabsf(dot)) * pitch * mod);
}

void PM_FootSlopeTrace(float* pDiff, float* pInterval)
{
    vec3_t footLOrg, footROrg, footLBot, footRBot;
    trace_t trace;
    float diff, interval;
    if (pm->gent->client->NPC_class == class_t::CLASS_ATST) {
        interval = 10;
    }
    else {
        interval = 4; //?
    }

    if (pm->gent->footLBolt == -1 || pm->gent->footRBolt == -1) {
        if (pDiff != nullptr) {
            *pDiff = 0;
        }
        if (pInterval != nullptr) {
            *pInterval = interval;
        }
        return;
    }
#if 1
    for (int i = 0; i < 3; i++) {
        if (Q_isnan(pm->gent->client->renderInfo.footLPoint[i]) || Q_isnan(pm->gent->client->renderInfo.footRPoint[i])) {
            if (pDiff != nullptr) {
                *pDiff = 0;
            }
            if (pInterval != nullptr) {
                *pInterval = interval;
            }
            return;
        }
    }
#else

    // FIXME: these really should have been gotten on the cgame, but I guess sometimes they're not and we end up with qnan numbers!
    mdxaBone_t boltMatrix;
    vec3_t G2Angles = {0, pm->gent->client->ps.legsYaw, 0};
    // get the feet
    gi.G2API_GetBoltMatrix(pm->gent->ghoul2, pm->gent->playerModel, pm->gent->footLBolt, &boltMatrix, G2Angles, pm->ps->origin,
                           (cg.time ? cg.time : level.time), nullptr, pm->gent->s.modelScale);
    gi.G2API_GiveMeVectorFromMatrix(boltMatrix, Eorientations::ORIGIN, pm->gent->client->renderInfo.footLPoint);

    gi.G2API_GetBoltMatrix(pm->gent->ghoul2, pm->gent->playerModel, pm->gent->footRBolt, &boltMatrix, G2Angles, pm->ps->origin,
                           (cg.time ? cg.time : level.time), nullptr, pm->gent->s.modelScale);
    gi.G2API_GiveMeVectorFromMatrix(boltMatrix, Eorientations::ORIGIN, pm->gent->client->renderInfo.footRPoint);
#endif
    // NOTE: on AT-STs, rotating the foot moves this point, so it will wiggle...
    //		we have to do this extra work (more G2 transforms) to stop the wiggle... is it worth it?
    /*
    if ( pm->gent->client->NPC_class == class_t::CLASS_ATST )
    {
        mdxaBone_t	boltMatrix;
        vec3_t		G2Angles = {0, pm->gent->client->ps.legsYaw, 0};
        //get the feet
        gi.G2API_SetBoneAnglesIndex( &pm->gent->ghoul2[0], pm->gent->footLBone, vec3_origin, boneFlags_t::BONE_ANGLES_POSTMULT, Eorientations::POSITIVE_Z,
    Eorientations::NEGATIVE_Y, Eorientations::NEGATIVE_X, nullptr ); gi.G2API_GetBoltMatrix( pm->gent->ghoul2, pm->gent->playerModel, pm->gent->footLBolt,
                &boltMatrix, G2Angles, pm->ps->origin, (cg.time?cg.time:level.time),
                        nullptr, pm->gent->s.modelScale );
        gi.G2API_GiveMeVectorFromMatrix( boltMatrix, Eorientations::ORIGIN, pm->gent->client->renderInfo.footLPoint );

        gi.G2API_SetBoneAnglesIndex( &pm->gent->ghoul2[0], pm->gent->footRBone, vec3_origin, boneFlags_t::BONE_ANGLES_POSTMULT, Eorientations::POSITIVE_Z,
    Eorientations::NEGATIVE_Y, Eorientations::NEGATIVE_X, nullptr ); gi.G2API_GetBoltMatrix( pm->gent->ghoul2, pm->gent->playerModel, pm->gent->footRBolt,
                        &boltMatrix, G2Angles, pm->ps->origin, (cg.time?cg.time:level.time),
                        nullptr, pm->gent->s.modelScale );
        gi.G2API_GiveMeVectorFromMatrix( boltMatrix, Eorientations::ORIGIN, pm->gent->client->renderInfo.footRPoint );
    }
    */
    // get these on the cgame and store it, save ourselves a ghoul2 construct skel call
    VectorCopy(pm->gent->client->renderInfo.footLPoint, footLOrg);
    VectorCopy(pm->gent->client->renderInfo.footRPoint, footROrg);

    // step 2: adjust foot tag z height to bottom of bbox+1
    footLOrg[2] = pm->gent->currentOrigin[2] + pm->gent->mins[2] + 1;
    footROrg[2] = pm->gent->currentOrigin[2] + pm->gent->mins[2] + 1;
    VectorSet(footLBot, footLOrg[0], footLOrg[1], footLOrg[2] - interval * 10);
    VectorSet(footRBot, footROrg[0], footROrg[1], footROrg[2] - interval * 10);

    // step 3: trace down from each, find difference
    vec3_t footMins, footMaxs;
    vec3_t footLSlope, footRSlope;
    if (pm->gent->client->NPC_class == class_t::CLASS_ATST) {
        VectorSet(footMins, -16, -16, 0);
        VectorSet(footMaxs, 16, 16, 1);
    }
    else {
        VectorSet(footMins, -3, -3, 0);
        VectorSet(footMaxs, 3, 3, 1);
    }

    pm->trace(&trace, footLOrg, footMins, footMaxs, footLBot, pm->ps->clientNum, pm->tracemask, EG2_Collision::G2_NOCOLLIDE, 0);
    VectorCopy(trace.endpos, footLBot);
    VectorCopy(trace.plane.normal, footLSlope);

    pm->trace(&trace, footROrg, footMins, footMaxs, footRBot, pm->ps->clientNum, pm->tracemask, EG2_Collision::G2_NOCOLLIDE, 0);
    VectorCopy(trace.endpos, footRBot);
    VectorCopy(trace.plane.normal, footRSlope);

    diff = footLBot[2] - footRBot[2];

    // optional step: for atst, tilt the footpads to match the slopes under it...
    if (pm->gent->client->NPC_class == class_t::CLASS_ATST) {
        vec3_t footAngles;
        if (!VectorCompare(footLSlope, vec3_origin)) { // rotate the ATST's left foot pad to match the slope
            PM_AnglesForSlope(pm->gent->client->renderInfo.legsYaw, footLSlope, footAngles);
            // Hmm... lerp this?
            gi.G2API_SetBoneAnglesIndex(&pm->gent->ghoul2[0], pm->gent->footLBone, footAngles, boneFlags_t::BONE_ANGLES_POSTMULT, Eorientations::POSITIVE_Z,
                                        Eorientations::NEGATIVE_Y, Eorientations::NEGATIVE_X, nullptr, 0, 0);
        }
        if (!VectorCompare(footRSlope, vec3_origin)) { // rotate the ATST's right foot pad to match the slope
            PM_AnglesForSlope(pm->gent->client->renderInfo.legsYaw, footRSlope, footAngles);
            // Hmm... lerp this?
            gi.G2API_SetBoneAnglesIndex(&pm->gent->ghoul2[0], pm->gent->footRBone, footAngles, boneFlags_t::BONE_ANGLES_POSTMULT, Eorientations::POSITIVE_Z,
                                        Eorientations::NEGATIVE_Y, Eorientations::NEGATIVE_X, nullptr, 0, 0);
        }
    }

    if (pDiff != nullptr) {
        *pDiff = diff;
    }
    if (pInterval != nullptr) {
        *pInterval = interval;
    }
}

bool PM_InSlopeAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::LEGS_LEFTUP1:  // # On a slope with left foot 4 higher than right
    case animNumber_t::LEGS_LEFTUP2:  // # On a slope with left foot 8 higher than right
    case animNumber_t::LEGS_LEFTUP3:  // # On a slope with left foot 12 higher than right
    case animNumber_t::LEGS_LEFTUP4:  // # On a slope with left foot 16 higher than right
    case animNumber_t::LEGS_LEFTUP5:  // # On a slope with left foot 20 higher than right
    case animNumber_t::LEGS_RIGHTUP1: // # On a slope with RIGHT foot 4 higher than left
    case animNumber_t::LEGS_RIGHTUP2: // # On a slope with RIGHT foot 8 higher than left
    case animNumber_t::LEGS_RIGHTUP3: // # On a slope with RIGHT foot 12 higher than left
    case animNumber_t::LEGS_RIGHTUP4: // # On a slope with RIGHT foot 16 higher than left
    case animNumber_t::LEGS_RIGHTUP5: // # On a slope with RIGHT foot 20 higher than left
    case animNumber_t::LEGS_S1_LUP1:
    case animNumber_t::LEGS_S1_LUP2:
    case animNumber_t::LEGS_S1_LUP3:
    case animNumber_t::LEGS_S1_LUP4:
    case animNumber_t::LEGS_S1_LUP5:
    case animNumber_t::LEGS_S1_RUP1:
    case animNumber_t::LEGS_S1_RUP2:
    case animNumber_t::LEGS_S1_RUP3:
    case animNumber_t::LEGS_S1_RUP4:
    case animNumber_t::LEGS_S1_RUP5:
    case animNumber_t::LEGS_S3_LUP1:
    case animNumber_t::LEGS_S3_LUP2:
    case animNumber_t::LEGS_S3_LUP3:
    case animNumber_t::LEGS_S3_LUP4:
    case animNumber_t::LEGS_S3_LUP5:
    case animNumber_t::LEGS_S3_RUP1:
    case animNumber_t::LEGS_S3_RUP2:
    case animNumber_t::LEGS_S3_RUP3:
    case animNumber_t::LEGS_S3_RUP4:
    case animNumber_t::LEGS_S3_RUP5:
    case animNumber_t::LEGS_S4_LUP1:
    case animNumber_t::LEGS_S4_LUP2:
    case animNumber_t::LEGS_S4_LUP3:
    case animNumber_t::LEGS_S4_LUP4:
    case animNumber_t::LEGS_S4_LUP5:
    case animNumber_t::LEGS_S4_RUP1:
    case animNumber_t::LEGS_S4_RUP2:
    case animNumber_t::LEGS_S4_RUP3:
    case animNumber_t::LEGS_S4_RUP4:
    case animNumber_t::LEGS_S4_RUP5:
    case animNumber_t::LEGS_S5_LUP1:
    case animNumber_t::LEGS_S5_LUP2:
    case animNumber_t::LEGS_S5_LUP3:
    case animNumber_t::LEGS_S5_LUP4:
    case animNumber_t::LEGS_S5_LUP5:
    case animNumber_t::LEGS_S5_RUP1:
    case animNumber_t::LEGS_S5_RUP2:
    case animNumber_t::LEGS_S5_RUP3:
    case animNumber_t::LEGS_S5_RUP4:
    case animNumber_t::LEGS_S5_RUP5:
        return true;
        break;
    default:
        break;
    }
    return false;
}

constexpr int SLOPE_RECALC_INT = 100;
extern bool G_StandardHumanoid(const char* modelName);
bool PM_AdjustStandAnimForSlope(void)
{
    if (!pm->gent || !pm->gent->client) {
        return false;
    }
    if (pm->gent->client->NPC_class != class_t::CLASS_ATST && (!pm->gent || !G_StandardHumanoid(pm->gent->NPC_type))) { // only ATST and player does this
        return false;
    }
    if (!pm->ps->clientNum && (!cg.renderingThirdPerson || cg.zoomMode)) { // first person doesn't do this
        return false;
    }
    if (pm->gent->footLBolt == -1 || pm->gent->footRBolt == -1) { // need these bolts!
        return false;
    }
    // step 1: find the 2 foot tags
    float diff;
    float interval;
    PM_FootSlopeTrace(&diff, &interval);

    // step 4: based on difference, choose one of the left/right slope-match intervals
    animNumber_t destAnim;
    if (diff >= interval * 5) {
        destAnim = animNumber_t::LEGS_LEFTUP5;
    }
    else if (diff >= interval * 4) {
        destAnim = animNumber_t::LEGS_LEFTUP4;
    }
    else if (diff >= interval * 3) {
        destAnim = animNumber_t::LEGS_LEFTUP3;
    }
    else if (diff >= interval * 2) {
        destAnim = animNumber_t::LEGS_LEFTUP2;
    }
    else if (diff >= interval) {
        destAnim = animNumber_t::LEGS_LEFTUP1;
    }
    else if (diff <= interval * -5) {
        destAnim = animNumber_t::LEGS_RIGHTUP5;
    }
    else if (diff <= interval * -4) {
        destAnim = animNumber_t::LEGS_RIGHTUP4;
    }
    else if (diff <= interval * -3) {
        destAnim = animNumber_t::LEGS_RIGHTUP3;
    }
    else if (diff <= interval * -2) {
        destAnim = animNumber_t::LEGS_RIGHTUP2;
    }
    else if (diff <= interval * -1) {
        destAnim = animNumber_t::LEGS_RIGHTUP1;
    }
    else {
        return false;
    }

    animNumber_t legsAnim = pm->ps->legsAnim;
    if (pm->gent->client->NPC_class != class_t::CLASS_ATST) {
        // adjust for current legs anim
        switch (legsAnim) {
        case animNumber_t::BOTH_STAND1:
        case animNumber_t::LEGS_S1_LUP1:
        case animNumber_t::LEGS_S1_LUP2:
        case animNumber_t::LEGS_S1_LUP3:
        case animNumber_t::LEGS_S1_LUP4:
        case animNumber_t::LEGS_S1_LUP5:
        case animNumber_t::LEGS_S1_RUP1:
        case animNumber_t::LEGS_S1_RUP2:
        case animNumber_t::LEGS_S1_RUP3:
        case animNumber_t::LEGS_S1_RUP4:
        case animNumber_t::LEGS_S1_RUP5:
            destAnim = animNumber_t::LEGS_S1_LUP1 + (destAnim - animNumber_t::LEGS_LEFTUP1);
            break;
        case animNumber_t::BOTH_STAND2:
        case animNumber_t::BOTH_SABERFAST_STANCE:
        case animNumber_t::BOTH_SABERSLOW_STANCE:
        case animNumber_t::BOTH_CROUCH1IDLE:
        case animNumber_t::BOTH_CROUCH1:
        case animNumber_t::LEGS_LEFTUP1:  // # On a slope with left foot 4 higher than right
        case animNumber_t::LEGS_LEFTUP2:  // # On a slope with left foot 8 higher than right
        case animNumber_t::LEGS_LEFTUP3:  // # On a slope with left foot 12 higher than right
        case animNumber_t::LEGS_LEFTUP4:  // # On a slope with left foot 16 higher than right
        case animNumber_t::LEGS_LEFTUP5:  // # On a slope with left foot 20 higher than right
        case animNumber_t::LEGS_RIGHTUP1: // # On a slope with RIGHT foot 4 higher than left
        case animNumber_t::LEGS_RIGHTUP2: // # On a slope with RIGHT foot 8 higher than left
        case animNumber_t::LEGS_RIGHTUP3: // # On a slope with RIGHT foot 12 higher than left
        case animNumber_t::LEGS_RIGHTUP4: // # On a slope with RIGHT foot 16 higher than left
        case animNumber_t::LEGS_RIGHTUP5: // # On a slope with RIGHT foot 20 higher than left
            // fine
            break;
        case animNumber_t::BOTH_STAND3:
        case animNumber_t::LEGS_S3_LUP1:
        case animNumber_t::LEGS_S3_LUP2:
        case animNumber_t::LEGS_S3_LUP3:
        case animNumber_t::LEGS_S3_LUP4:
        case animNumber_t::LEGS_S3_LUP5:
        case animNumber_t::LEGS_S3_RUP1:
        case animNumber_t::LEGS_S3_RUP2:
        case animNumber_t::LEGS_S3_RUP3:
        case animNumber_t::LEGS_S3_RUP4:
        case animNumber_t::LEGS_S3_RUP5:
            destAnim = animNumber_t::LEGS_S3_LUP1 + (destAnim - animNumber_t::LEGS_LEFTUP1);
            break;
        case animNumber_t::BOTH_STAND4:
        case animNumber_t::LEGS_S4_LUP1:
        case animNumber_t::LEGS_S4_LUP2:
        case animNumber_t::LEGS_S4_LUP3:
        case animNumber_t::LEGS_S4_LUP4:
        case animNumber_t::LEGS_S4_LUP5:
        case animNumber_t::LEGS_S4_RUP1:
        case animNumber_t::LEGS_S4_RUP2:
        case animNumber_t::LEGS_S4_RUP3:
        case animNumber_t::LEGS_S4_RUP4:
        case animNumber_t::LEGS_S4_RUP5:
            destAnim = animNumber_t::LEGS_S4_LUP1 + (destAnim - animNumber_t::LEGS_LEFTUP1);
            break;
        case animNumber_t::BOTH_STAND5:
        case animNumber_t::LEGS_S5_LUP1:
        case animNumber_t::LEGS_S5_LUP2:
        case animNumber_t::LEGS_S5_LUP3:
        case animNumber_t::LEGS_S5_LUP4:
        case animNumber_t::LEGS_S5_LUP5:
        case animNumber_t::LEGS_S5_RUP1:
        case animNumber_t::LEGS_S5_RUP2:
        case animNumber_t::LEGS_S5_RUP3:
        case animNumber_t::LEGS_S5_RUP4:
        case animNumber_t::LEGS_S5_RUP5:
            destAnim = animNumber_t::LEGS_S5_LUP1 + (destAnim - animNumber_t::LEGS_LEFTUP1);
            break;
        case animNumber_t::BOTH_STAND6:
        default:
            return false;
            break;
        }
    }
    // step 5: based on the chosen interval and the current legsAnim, pick the correct anim
    // step 6: increment/decrement to the dest anim, not instant
    if ((legsAnim >= animNumber_t::LEGS_LEFTUP1 && legsAnim <= animNumber_t::LEGS_LEFTUP5) ||
        (legsAnim >= animNumber_t::LEGS_S1_LUP1 && legsAnim <= animNumber_t::LEGS_S1_LUP5) ||
        (legsAnim >= animNumber_t::LEGS_S3_LUP1 && legsAnim <= animNumber_t::LEGS_S3_LUP5) ||
        (legsAnim >= animNumber_t::LEGS_S4_LUP1 && legsAnim <= animNumber_t::LEGS_S4_LUP5) ||
        (legsAnim >= animNumber_t::LEGS_S5_LUP1 && legsAnim <= animNumber_t::LEGS_S5_LUP5)) { // already in left-side up
        if (destAnim > legsAnim && pm->gent->client->slopeRecalcTime < level.time) {
            legsAnim++;
            pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
        }
        else if (destAnim < legsAnim && pm->gent->client->slopeRecalcTime < level.time) {
            legsAnim--;
            pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
        }
        else {
            destAnim = legsAnim;
        }
    }
    else if ((legsAnim >= animNumber_t::LEGS_RIGHTUP1 && legsAnim <= animNumber_t::LEGS_RIGHTUP5) ||
             (legsAnim >= animNumber_t::LEGS_S1_RUP1 && legsAnim <= animNumber_t::LEGS_S1_RUP5) ||
             (legsAnim >= animNumber_t::LEGS_S3_RUP1 && legsAnim <= animNumber_t::LEGS_S3_RUP5) ||
             (legsAnim >= animNumber_t::LEGS_S4_RUP1 && legsAnim <= animNumber_t::LEGS_S4_RUP5) ||
             (legsAnim >= animNumber_t::LEGS_S5_RUP1 && legsAnim <= animNumber_t::LEGS_S5_RUP5)) { // already in right-side up
        if (destAnim > legsAnim && pm->gent->client->slopeRecalcTime < level.time) {
            legsAnim++;
            pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
        }
        else if (destAnim < legsAnim && pm->gent->client->slopeRecalcTime < level.time) {
            legsAnim--;
            pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
        }
        else {
            destAnim = legsAnim;
        }
    }
    else { // in a stand of some sort?
        if (pm->gent->client->NPC_class == class_t::CLASS_ATST) {
            if (legsAnim == animNumber_t::BOTH_STAND1 || legsAnim == animNumber_t::BOTH_STAND2 || legsAnim == animNumber_t::BOTH_CROUCH1IDLE) {
                if (destAnim >= animNumber_t::LEGS_LEFTUP1 && destAnim <= animNumber_t::LEGS_LEFTUP5) { // going into left side up
                    destAnim = animNumber_t::LEGS_LEFTUP1;
                    pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
                }
                else if (destAnim >= animNumber_t::LEGS_RIGHTUP1 && destAnim <= animNumber_t::LEGS_RIGHTUP5) { // going into right side up
                    destAnim = animNumber_t::LEGS_RIGHTUP1;
                    pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
                }
                else { // will never get here
                    return false;
                }
            }
        }
        else {
            switch (legsAnim) {
            case animNumber_t::BOTH_STAND1:
                if (destAnim >= animNumber_t::LEGS_S1_LUP1 && destAnim <= animNumber_t::LEGS_S1_LUP5) { // going into left side up
                    destAnim = animNumber_t::LEGS_S1_LUP1;
                    pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
                }
                else if (destAnim >= animNumber_t::LEGS_S1_RUP1 && destAnim <= animNumber_t::LEGS_S1_RUP5) { // going into right side up
                    destAnim = animNumber_t::LEGS_S1_RUP1;
                    pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
                }
                else { // will never get here
                    return false;
                }
                break;
            case animNumber_t::BOTH_STAND2:
            case animNumber_t::BOTH_SABERFAST_STANCE:
            case animNumber_t::BOTH_SABERSLOW_STANCE:
            case animNumber_t::BOTH_CROUCH1IDLE:
                if (destAnim >= animNumber_t::LEGS_LEFTUP1 && destAnim <= animNumber_t::LEGS_LEFTUP5) { // going into left side up
                    destAnim = animNumber_t::LEGS_LEFTUP1;
                    pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
                }
                else if (destAnim >= animNumber_t::LEGS_RIGHTUP1 && destAnim <= animNumber_t::LEGS_RIGHTUP5) { // going into right side up
                    destAnim = animNumber_t::LEGS_RIGHTUP1;
                    pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
                }
                else { // will never get here
                    return false;
                }
                break;
            case animNumber_t::BOTH_STAND3:
                if (destAnim >= animNumber_t::LEGS_S3_LUP1 && destAnim <= animNumber_t::LEGS_S3_LUP5) { // going into left side up
                    destAnim = animNumber_t::LEGS_S3_LUP1;
                    pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
                }
                else if (destAnim >= animNumber_t::LEGS_S3_RUP1 && destAnim <= animNumber_t::LEGS_S3_RUP5) { // going into right side up
                    destAnim = animNumber_t::LEGS_S3_RUP1;
                    pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
                }
                else { // will never get here
                    return false;
                }
                break;
            case animNumber_t::BOTH_STAND4:
                if (destAnim >= animNumber_t::LEGS_S4_LUP1 && destAnim <= animNumber_t::LEGS_S4_LUP5) { // going into left side up
                    destAnim = animNumber_t::LEGS_S4_LUP1;
                    pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
                }
                else if (destAnim >= animNumber_t::LEGS_S4_RUP1 && destAnim <= animNumber_t::LEGS_S4_RUP5) { // going into right side up
                    destAnim = animNumber_t::LEGS_S4_RUP1;
                    pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
                }
                else { // will never get here
                    return false;
                }
                break;
            case animNumber_t::BOTH_STAND5:
                if (destAnim >= animNumber_t::LEGS_S5_LUP1 && destAnim <= animNumber_t::LEGS_S5_LUP5) { // going into left side up
                    destAnim = animNumber_t::LEGS_S5_LUP1;
                    pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
                }
                else if (destAnim >= animNumber_t::LEGS_S5_RUP1 && destAnim <= animNumber_t::LEGS_S5_RUP5) { // going into right side up
                    destAnim = animNumber_t::LEGS_S5_RUP1;
                    pm->gent->client->slopeRecalcTime = level.time + SLOPE_RECALC_INT;
                }
                else { // will never get here
                    return false;
                }
                break;
            case animNumber_t::BOTH_STAND6:
            default:
                return false;
                break;
            }
        }
    }
    // step 7: set the anim
    PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, destAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);

    return true;
}

void PM_SwimFloatAnim(void)
{
    animNumber_t legsAnim = pm->ps->legsAnim;
    // FIXME: no start or stop anims
    if (pm->cmd.forwardmove || pm->cmd.rightmove || pm->cmd.upmove) {
        PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_SWIMFORWARD, setAnimFlags_t::SETANIM_FLAG_NORMAL);
    }
    else {                                                // stopping
        if (legsAnim == animNumber_t::BOTH_SWIMFORWARD) { // I was swimming
            if (!pm->ps->legsAnimTimer) {
                PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_SWIM_IDLE1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            }
        }
        else {                                                                              // idle
            if (!Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED) && pm->cmd.upmove >= 0) { // not crouching
                PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_SWIM_IDLE1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            }
        }
    }
}

/*
===============
PM_Footsteps
===============
*/
static void PM_Footsteps(void)
{
    float bobmove;
    int old;
    animNumber_t oldAnim;
    bool footstep = false;
    bool validNPC = false;
    bool flipping = false;
    setAnimFlags_t setAnimFlags = setAnimFlags_t::SETANIM_FLAG_NORMAL;

    if (pm->gent == nullptr || pm->gent->client == nullptr)
        return;

    if (PM_SpinningSaberAnim(pm->ps->legsAnim) && pm->ps->legsAnimTimer) { // spinning
        return;
    }
    if (PM_InKnockDown(pm->ps) || PM_InRoll(pm->ps)) { // in knockdown
        return;
    }

    if (pm->gent->NPC != nullptr) {
        validNPC = true;
    }

    pm->gent->client->renderInfo.legsFpsMod = 1.0f;
    // PM_ResetAnkleAngles();

    //
    // calculate speed and cycle to be used for
    // all cyclic walking effects
    //
    pm->xyspeed = Q_sqrtf(pm->ps->velocity[0] * pm->ps->velocity[0] + pm->ps->velocity[1] * pm->ps->velocity[1]);

    if (pm->ps->legsAnim == animNumber_t::BOTH_FLIP_F || pm->ps->legsAnim == animNumber_t::BOTH_FLIP_B || pm->ps->legsAnim == animNumber_t::BOTH_FLIP_L ||
        pm->ps->legsAnim == animNumber_t::BOTH_FLIP_R) {
        flipping = true;
    }

    if (pm->ps->groundEntityNum == ENTITYNUM_NONE || Any(pm->watertype & contentsFlags_t::CONTENTS_LADDER) ||
        pm->ps->waterHeightLevel >= waterHeightLevel_t::WHL_TORSO) { // in air or submerged in water or in ladder
        // airborne leaves position in cycle intact, but doesn't advance
        if (pm->waterlevel > 0) {
            if (Any(pm->watertype & contentsFlags_t::CONTENTS_LADDER)) { // FIXME: check for watertype, save waterlevel for whether to play
                // the get off ladder transition anim!
                if (pm->ps->velocity[2]) { // going up or down it
                    animNumber_t anim;
                    if (pm->ps->velocity[2] > 0) {
                        anim = animNumber_t::BOTH_LADDER_UP1;
                    }
                    else {
                        anim = animNumber_t::BOTH_LADDER_DWN1;
                    }
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, anim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                    if (pm->waterlevel >= 2) // arms on ladder
                    {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, anim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                    }
                    if (Q_fabsf(pm->ps->velocity[2]) > 5) {
                        bobmove = 0.005 * Q_fabsf(pm->ps->velocity[2]); // climbing bobs slow
                        if (bobmove > 0.3)
                            bobmove = 0.3F;
                        goto DoFootSteps;
                    }
                }
                else {
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_LADDER_IDLE,
                               setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_RESTART);
                    pm->ps->legsAnimTimer += 300;
                    if (pm->waterlevel >= 2) // arms on ladder
                    {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_LADDER_IDLE,
                                   setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_RESTART);
                        pm->ps->torsoAnimTimer += 300;
                    }
                }
                return;
            }
            else if (pm->ps->waterHeightLevel >= waterHeightLevel_t::WHL_TORSO &&
                     (!pm->ps->clientNum || pm->ps->weapon == weapon_t::WP_SABER || pm->ps->weapon == weapon_t::WP_NONE ||
                      pm->ps->weapon == weapon_t::WP_MELEE)) // pm->waterlevel > 1 )	//in deep water
            {
                if (!PM_ForceJumpingUp(pm->gent)) {
                    if (pm->ps->groundEntityNum != ENTITYNUM_NONE && Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) {
                        if (!flipping) { // you can crouch under water if feet are on ground
                            if (pm->cmd.forwardmove || pm->cmd.rightmove) {
                                if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_BACKWARDS_RUN)) {
                                    PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_CROUCH1WALKBACK, setAnimFlags);
                                }
                                else {
                                    PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_CROUCH1WALK, setAnimFlags);
                                }
                            }
                            else {
                                PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_CROUCH1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                            }
                            return;
                        }
                    }
                    PM_SwimFloatAnim();
                    if (pm->ps->legsAnim != animNumber_t::BOTH_SWIM_IDLE1) { // moving
                        old = pm->ps->bobCycle;
                        bobmove = 0.15f; // swim is a slow cycle
                        pm->ps->bobCycle = (int)(old + bobmove * pml.msec) & 255;

                        // if we just crossed a cycle boundary, play an apropriate footstep event
                        if (((old + 64) ^ (pm->ps->bobCycle + 64)) & 128) {
                            PM_AddEvent(entity_event_t::EV_SWIM);
                        }
                    }
                }
                return;
            }
            else { // hmm, in water, but not high enough to swim
                // fall through to walk/run/stand
                if (pm->ps->groundEntityNum == ENTITYNUM_NONE) { // unless in the air
                    // NOTE: this is a dupe of the code just below... for when you are not in the water at all
                    if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) {
                        if (!flipping) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_CROUCH1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    else if (pm->ps->gravity <= 0) // FIXME: or just less than normal?
                    {
                        PM_SwimFloatAnim();
                    }
                    return;
                }
            }
        }
        else {
            if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) {
                if (!flipping) {
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_CROUCH1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                }
            }
            else if (pm->ps->gravity <= 0) // FIXME: or just less than normal?
            {
                PM_SwimFloatAnim();
            }
            return;
        }
    }

    if (PM_SwimmingAnim(pm->ps->legsAnim) && pm->waterlevel < 2) { // legs are in swim anim, and not swimming, be sure to override it
        setAnimFlags |= setAnimFlags_t::SETANIM_FLAG_OVERRIDE;
    }

    // if not trying to move
    if (!pm->cmd.forwardmove && !pm->cmd.rightmove) {
        if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_ATST) {
            if (!PM_AdjustStandAnimForSlope()) {
                PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_STAND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            }
        }
        else if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) {
            if (!PM_InOnGroundAnim(pm->ps)) {
                if (!PM_AdjustStandAnimForSlope()) {
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_CROUCH1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                }
            }
        }
        else {
            if (pm->ps->legsAnimTimer && PM_LandingAnim(pm->ps->legsAnim)) { // still in a landing anim, let it play
                return;
            }
            bool saberInAir = true;
            if (pm->ps->saberInFlight) { // guiding saber
                if (PM_SaberInBrokenParry(pm->ps->saberMove) || pm->ps->saberBlocked == saberBlockedType_t::BLOCKED_PARRY_BROKEN ||
                    PM_DodgeAnim(pm->ps->torsoAnim)) { // we're stuck in a broken parry
                    saberInAir = false;
                }
                if (pm->ps->saberEntityNum < ENTITYNUM_NONE && pm->ps->saberEntityNum > 0)            // player is 0
                {                                                                                     //
                    if (g_entities[pm->ps->saberEntityNum].s.pos.trType == trType_t::TR_STATIONARY) { // fell to the ground and we're not trying to pull it back
                        saberInAir = false;
                    }
                }
            }
            if (pm->gent && pm->gent->client &&
                pm->gent->client->NPC_class ==
                    class_t::CLASS_GALAKMECH) { // NOTE: stand1 is with the helmet retracted, stand1to2 is the helmet going into place
                PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_STAND2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            }
            else if (pm->ps->weapon == weapon_t::WP_SABER && pm->ps->saberInFlight && saberInAir) {
                if (!PM_AdjustStandAnimForSlope()) {
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_SABERPULL, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                }
            }
            else if ((pm->ps->weapon == weapon_t::WP_SABER && pm->ps->saberLength > 0 && !pm->ps->saberInFlight)) {
                if (!PM_AdjustStandAnimForSlope()) {
                    animNumber_t legsAnim;
                    switch (pm->ps->saberAnimLevel) {
                    case forcePowerLevels_t::FORCE_LEVEL_1:
                    case forcePowerLevels_t::FORCE_LEVEL_5:
                        legsAnim = animNumber_t::BOTH_SABERFAST_STANCE;
                        break;
                    case forcePowerLevels_t::FORCE_LEVEL_3:
                        legsAnim = animNumber_t::BOTH_SABERSLOW_STANCE;
                        break;
                    case forcePowerLevels_t::FORCE_LEVEL_0:
                    case forcePowerLevels_t::FORCE_LEVEL_2:
                    case forcePowerLevels_t::FORCE_LEVEL_4:
                    default:
                        legsAnim = animNumber_t::BOTH_STAND2;
                        break;
                    }
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                }
            }
            else if ((validNPC && pm->ps->weapon > weapon_t::WP_SABER && pm->ps->weapon < weapon_t::WP_DET_PACK)) // Being careful or carrying a 2-handed weapon
            {                                                                                                     // Squadmates use BOTH_STAND3
                oldAnim = pm->ps->legsAnim;
                if (oldAnim != animNumber_t::BOTH_GUARD_LOOKAROUND1 && oldAnim != animNumber_t::BOTH_GUARD_IDLE1 && oldAnim != animNumber_t::BOTH_STAND3IDLE1 &&
                    oldAnim != animNumber_t::BOTH_STAND2TO4 && oldAnim != animNumber_t::BOTH_STAND4TO2 &&
                    oldAnim != animNumber_t::BOTH_STAND4) { // Don't auto-override the guard idles
                    if (!PM_AdjustStandAnimForSlope()) {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_STAND3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        // if(oldAnim != animNumber_t::BOTH_STAND2 && pm->ps->legsAnim == animNumber_t::BOTH_STAND2)
                        //{
                        //	pm->ps->legsAnimTimer = 500;
                        // }
                    }
                }
            }
            else {
                if (!PM_AdjustStandAnimForSlope()) {
                    // FIXME: Do we need this here... The imps stand is 4, not 1...
                    if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_IMPERIAL) {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_STAND4, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_STAND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                }
            }
        }
        return;
    }

    // maybe call this every frame, even when moving?
    if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_ATST) {
        PM_FootSlopeTrace(nullptr, nullptr);
    }

    // trying to move laterally
    if ((PM_InSaberAnim(pm->ps->legsAnim) && !PM_SpinningSaberAnim(pm->ps->legsAnim)) || pm->ps->legsAnim == animNumber_t::BOTH_STAND1 ||
        pm->ps->legsAnim == animNumber_t::BOTH_STAND1TO2 || pm->ps->legsAnim == animNumber_t::BOTH_STAND2TO1 || pm->ps->legsAnim == animNumber_t::BOTH_STAND2 ||
        pm->ps->legsAnim == animNumber_t::BOTH_SABERFAST_STANCE || pm->ps->legsAnim == animNumber_t::BOTH_SABERSLOW_STANCE ||
        pm->ps->legsAnim == animNumber_t::BOTH_BUTTON_HOLD || pm->ps->legsAnim == animNumber_t::BOTH_BUTTON_RELEASE || PM_LandingAnim(pm->ps->legsAnim) ||
        PM_PainAnim(pm->ps->legsAnim) || PM_ForceAnim(pm->ps->legsAnim)) { // legs are in a saber anim, and not spinning, be sure to override it
        setAnimFlags |= setAnimFlags_t::SETANIM_FLAG_OVERRIDE;
    }

    if (Any(pm->ps->eFlags & entityFlags_t::EF_IN_ATST))                                                               // does this catch NPCs, too?
    {                                                                                                                  // atst
        if (pm->ps->legsAnim == animNumber_t::BOTH_TURN_LEFT1 || pm->ps->legsAnim == animNumber_t::BOTH_TURN_RIGHT1) { // moving overrides turning
            setAnimFlags |= setAnimFlags_t::SETANIM_FLAG_OVERRIDE;
        }
    }

    if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) {
        bobmove = 0.5; // ducked characters bob much faster
        if (!PM_InOnGroundAnim(pm->ps)) {
            bool rolled = false;
            if (PM_RunningAnim(pm->ps->legsAnim) || pm->ps->legsAnim == animNumber_t::BOTH_FORCEHEAL_START) { // roll!
                rolled = PM_TryRoll();
            }
            if (!rolled) {
                if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_BACKWARDS_RUN)) {
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_CROUCH1WALKBACK, setAnimFlags);
                }
                else {
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_CROUCH1WALK, setAnimFlags);
                }
            }
        }
        // ducked characters never play footsteps
    }
    else if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_BACKWARDS_RUN)) { // Moving backwards
        if (!Any(pm->cmd.buttons & buttonFlags_t::BUTTON_WALKING)) {    // running backwards
            bobmove = 0.4F;                                             // faster speeds bob faster
            PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_RUNBACK1, setAnimFlags);
            footstep = true;
        }
        else {              // walking backwards
            bobmove = 0.3F; // faster speeds bob faster
            PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_WALKBACK1, setAnimFlags);
        }
    }
    else {
        if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_GALAKMECH) {
            bobmove = 0.3F;                            // walking bobs slow
            if (pm->ps->weapon == weapon_t::WP_NONE) { // helmet retracted
                PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_WALK1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            }
            else { // helmet in place
                PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_WALK2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            }
        }
        else if (!Any(pm->cmd.buttons & buttonFlags_t::BUTTON_WALKING)) {
            bobmove = 0.4F; // faster speeds bob faster
            if (pm->ps->weapon == weapon_t::WP_SABER && pm->ps->saberActive) {
                PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_RUN2, setAnimFlags);
            }
            else {
                if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_ATST) {
                    if (pm->ps->legsAnim != animNumber_t::BOTH_RUN1) {
                        if (pm->ps->legsAnim !=
                            animNumber_t::BOTH_RUN1START) { // Hmm, he should really start slow and have to accelerate... also need to do this for stopping
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_RUN1START, setAnimFlags | setAnimFlags_t::SETANIM_FLAG_HOLD);
                        }
                        else if (!pm->ps->legsAnimTimer) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_RUN1, setAnimFlags);
                        }
                    }
                    else {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_RUN1, setAnimFlags);
                    }
                }
                else {
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_RUN1, setAnimFlags);
                }
            }
            footstep = true;
        }
        else {
            bobmove = 0.3F; // walking bobs slow
            if (pm->ps->clientNum && pm->ps->weapon == weapon_t::WP_SABER && pm->ps->saberActive) {
                PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_WALK2, setAnimFlags);
            }
            else {
                PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_WALK1, setAnimFlags);
            }

            // Enemy NPCs always make footsteps for the benefit of the player
            if (pm->gent && pm->gent->NPC && pm->gent->client && pm->gent->client->playerTeam != team_t::TEAM_PLAYER) {
                footstep = true;
            }
        }
    }

    if (pm->gent != nullptr) {
        if (pm->gent->client->renderInfo.legsFpsMod > 2) {
            pm->gent->client->renderInfo.legsFpsMod = 2;
        }
        else if (pm->gent->client->renderInfo.legsFpsMod < 0.5) {
            pm->gent->client->renderInfo.legsFpsMod = 0.5;
        }
    }

DoFootSteps:

    // check for footstep / splash sounds
    old = pm->ps->bobCycle;
    pm->ps->bobCycle = (int)(old + bobmove * pml.msec) & 255;

    // if we just crossed a cycle boundary, play an apropriate footstep event
    if (((old + 64) ^ (pm->ps->bobCycle + 64)) & 128) {
        if (Any(pm->watertype & contentsFlags_t::CONTENTS_LADDER)) {
            if (!pm->noFootsteps) {
                if (pm->ps->groundEntityNum == ENTITYNUM_NONE) { // on ladder
                    PM_AddEvent(entity_event_t::EV_FOOTSTEP_METAL);
                }
                else {
                    PM_AddEvent(PM_FootstepForSurface()); // still on ground
                }
                if (pm->gent && pm->gent->s.number == 0) {
                    //					if ( pm->gent->client && pm->gent->client->playerTeam != TEAM_DISGUISE )
                    {
                        AddSoundEvent(pm->gent, pm->ps->origin, 128, alertEventLevel_e::AEL_MINOR, true);
                    }
                }
            }
        }
        else if (pm->waterlevel == 0) {
            // on ground will only play sounds if running
            if (footstep && !pm->noFootsteps) {
                PM_AddEvent(PM_FootstepForSurface());
                if (pm->gent && pm->gent->s.number == 0) {
                    vec3_t bottom;

                    VectorCopy(pm->ps->origin, bottom);
                    bottom[2] += pm->mins[2];
                    //					if ( pm->gent->client && pm->gent->client->playerTeam != TEAM_DISGUISE )
                    {
                        AddSoundEvent(pm->gent, bottom, 256, alertEventLevel_e::AEL_MINOR, true);
                    }
                }
            }
        }
        else if (pm->waterlevel == 1) {
            // splashing
            if (pm->ps->waterHeightLevel >= waterHeightLevel_t::WHL_KNEES) {
                PM_AddEvent(entity_event_t::EV_FOOTWADE);
            }
            else {
                PM_AddEvent(entity_event_t::EV_FOOTSPLASH);
            }
            if (pm->gent && pm->gent->s.number == 0) {
                vec3_t bottom;

                VectorCopy(pm->ps->origin, bottom);
                bottom[2] += pm->mins[2];
                //				if ( pm->gent->client && pm->gent->client->playerTeam != TEAM_DISGUISE )
                {
                    AddSoundEvent(pm->gent, pm->ps->origin, 384, alertEventLevel_e::AEL_SUSPICIOUS); // was bottom
                    AddSightEvent(pm->gent, pm->ps->origin, 512, alertEventLevel_e::AEL_MINOR);
                }
            }
        }
        else if (pm->waterlevel == 2) {
            // wading / swimming at surface
            /*
            if ( pm->ps->waterHeightLevel >= waterHeightLevel_t::WHL_TORSO )
            {
                PM_AddEvent( entity_event_t::EV_SWIM );
            }
            else
            */
            {
                PM_AddEvent(entity_event_t::EV_FOOTWADE);
            }
            if (pm->gent && pm->gent->s.number == 0) {
                //				if ( pm->gent->client && pm->gent->client->playerTeam != TEAM_DISGUISE )
                {
                    AddSoundEvent(pm->gent, pm->ps->origin, 256, alertEventLevel_e::AEL_MINOR);
                    AddSightEvent(pm->gent, pm->ps->origin, 512, alertEventLevel_e::AEL_SUSPICIOUS);
                }
            }
        }
        else { // or no sound when completely underwater...?
            PM_AddEvent(entity_event_t::EV_SWIM);
        }
    }
}

/*
==============
PM_WaterEvents

Generate sound events for entering and leaving water
==============
*/
static void PM_WaterEvents(void)
{ // FIXME?

    bool impact_splash = false;

    if (Any(pm->watertype & contentsFlags_t::CONTENTS_LADDER)) // fake water for ladder
    {
        return;
    }
    //
    // if just entered a water volume, play a sound
    //
    if (!pml.previous_waterlevel && pm->waterlevel) {
        PM_AddEvent(entity_event_t::EV_WATER_TOUCH);
        if (pm->gent && VectorLengthSquared(pm->ps->velocity) > 40000) {
            impact_splash = true;
        }
        if (pm->gent && !pm->ps->clientNum) {
            AddSoundEvent(pm->gent, pm->ps->origin, 384, alertEventLevel_e::AEL_SUSPICIOUS);
            AddSightEvent(pm->gent, pm->ps->origin, 512, alertEventLevel_e::AEL_SUSPICIOUS);
        }
    }

    //
    // if just completely exited a water volume, play a sound
    //
    if (pml.previous_waterlevel && !pm->waterlevel) {
        PM_AddEvent(entity_event_t::EV_WATER_LEAVE);
        if (pm->gent && VectorLengthSquared(pm->ps->velocity) > 40000) {
            impact_splash = true;
        }
        if (pm->gent && !pm->ps->clientNum) {
            AddSoundEvent(pm->gent, pm->ps->origin, 384, alertEventLevel_e::AEL_SUSPICIOUS);
            AddSightEvent(pm->gent, pm->ps->origin, 512, alertEventLevel_e::AEL_SUSPICIOUS);
        }
    }

    if (impact_splash) {
        // play the splash effect
        trace_t tr;
        vec3_t axis[3], angs, start, end;

        VectorSet(angs, 0, pm->gent->currentAngles[YAW], 0);
        AngleVectors(angs, axis[2], axis[1], axis[0]);

        VectorCopy(pm->ps->origin, start);
        VectorCopy(pm->ps->origin, end);

        // FIXME: set start and end better
        start[2] += 10;
        end[2] -= 40;

        gi.trace(&tr, start, vec3_origin, vec3_origin, end, pm->gent->s.number, contentsFlags_t::CONTENTS_WATER, EG2_Collision::G2_NOCOLLIDE, 0);

        if (tr.fraction < 1.0f) {
            G_PlayEffect("water_impact", tr.endpos, axis);
        }
    }

    //
    // check for head just going under water
    //
    if (pml.previous_waterlevel != 3 && pm->waterlevel == 3) {
        PM_AddEvent(entity_event_t::EV_WATER_UNDER);
        if (pm->gent && !pm->ps->clientNum) {
            AddSoundEvent(pm->gent, pm->ps->origin, 256, alertEventLevel_e::AEL_MINOR);
            AddSightEvent(pm->gent, pm->ps->origin, 384, alertEventLevel_e::AEL_MINOR);
        }
    }

    //
    // check for head just coming out of water
    //
    if (pml.previous_waterlevel == 3 && pm->waterlevel != 3) {
        if (!pm->gent || !pm->gent->client || pm->gent->client->airOutTime < level.time + 2000) { // only do this if we were drowning or about to start drowning
            PM_AddEvent(entity_event_t::EV_WATER_CLEAR);
        }
        else {
            PM_AddEvent(entity_event_t::EV_WATER_LEAVE);
        }
        if (pm->gent && !pm->ps->clientNum) {
            AddSoundEvent(pm->gent, pm->ps->origin, 256, alertEventLevel_e::AEL_MINOR);
            AddSightEvent(pm->gent, pm->ps->origin, 384, alertEventLevel_e::AEL_SUSPICIOUS);
        }
    }
}

/*
===============
PM_BeginWeaponChange
===============
*/
extern void G_CreateG2AttachedWeaponModel(gentity_t* ent, const char* weaponModel);
static void PM_BeginWeaponChange(weapon_t weapon)
{

    if (pm->gent && pm->gent->client && pm->gent->client->pers.enterTime >= level.time - 500) { // just entered map
        if (weapon == weapon_t::WP_NONE && pm->ps->weapon != weapon) {                          // don't switch to weapon none if just entered map
            return;
        }
    }

    if (weapon < weapon_t::WP_NONE || weapon >= weapon_t::WP_NUM_WEAPONS) {
        return;
    }

    if (!(pm->ps->stats[statIndex_t::STAT_WEAPONS] & Bit(weapon))) {
        return;
    }

    if (pm->ps->weaponstate == weaponstate_t::WEAPON_DROPPING) {
        return;
    }

    if (cg.time > 0) { // this way we don't get that annoying change weapon sound every time a map starts
        PM_AddEvent(entity_event_t::EV_CHANGE_WEAPON);
    }

    pm->ps->weaponstate = weaponstate_t::WEAPON_DROPPING;
    pm->ps->weaponTime += 200;
    if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_GALAKMECH) {
        if (pm->gent->alt_fire) { // FIXME: attack delay?
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_DROPWEAP3,
                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
        }
        else { // FIXME: attack delay?
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_DROPWEAP1,
                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
        }
    }
    else {
        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_DROPWEAP1, setAnimFlags_t::SETANIM_FLAG_HOLD);
    }

    // turn of any kind of zooming when weapon switching....except the LA Goggles
    if (pm->ps->clientNum == 0) {
        if (cg.zoomMode > 0 && cg.zoomMode < 3) {
            cg.zoomMode = 0;
            cg.zoomTime = cg.time;
        }
    }

    if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_ATST) {
        if (!pm->ps->clientNum) {
            gi.cvar_set("cg_thirdperson", "1");
        }
    }
    else if (weapon == weapon_t::WP_SABER) { // going to switch to lightsaber
    }
    else {
        if (pm->ps->weapon == weapon_t::WP_SABER) { // going to switch away from saber
            if (pm->gent) {
                G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_WEAPON, "sound/weapons/saber/saberoffquick.wav");
            }
            PM_SetSaberMove(saberMoveName_t::LS_PUTAWAY);
        }
        // put this back in because saberActive isn't being set somewhere else anymore
        pm->ps->saberActive = false;
        pm->ps->saberLength = 0;
    }
}

/*
===============
PM_FinishWeaponChange
===============
*/
static void PM_FinishWeaponChange(void)
{
    weapon_t weapon;
    bool trueSwitch = true;

    if (pm->gent && pm->gent->client && pm->gent->client->pers.enterTime >= level.time - 500) { // just entered map
        if (UC_Weapon(pm->cmd) == weapon_t::WP_NONE && pm->ps->weapon != UC_Weapon(pm->cmd)) {  // don't switch to weapon none if just entered map
            return;
        }
    }
    weapon = UC_Weapon(pm->cmd);
    if (weapon < weapon_t::WP_NONE || weapon >= weapon_t::WP_NUM_WEAPONS) {
        weapon = weapon_t::WP_NONE;
    }

    if (!(pm->ps->stats[statIndex_t::STAT_WEAPONS] & Bit(weapon))) {
        weapon = weapon_t::WP_NONE;
    }

    if (pm->ps->weapon == weapon) {
        trueSwitch = false;
    }
    weapon_t oldWeap = pm->ps->weapon;
    pm->ps->weapon = weapon;
    pm->ps->weaponstate = weaponstate_t::WEAPON_RAISING;
    pm->ps->weaponTime += 250;

    if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_ATST) { // do nothing
    }
    else if (weapon == weapon_t::WP_SABER) { // turn on the lightsaber
        // FIXME: somehow sometimes I still end up with 2 weapons in hand... usually if I
        //		cycle weapons fast enough that I end up in 1st person lightsaber, then
        //		somehow throw the saber and switch to another weapon (all in 1st person),
        //		making my saber drop to the ground... when I switch back to the saber, it
        //		does not remove the current weapon model and then, when I pull the saber
        //		back to my hand, I have 2 weaponModels active...?
        if (pm->gent) { // remove gun if we had it.
            if (pm->gent->weaponModel >= 0) {
                gi.G2API_RemoveGhoul2Model(pm->gent->ghoul2, pm->gent->weaponModel);
            }
        }

        if (!pm->ps->saberInFlight) { // if it's not in flight or lying around, turn it on!
            // FIXME: AddSound/Sight Event
            // FIXME: don't do this if just loaded a game
            if (trueSwitch) { // actually did switch weapons, turn it on
                pm->ps->saberActive = true;
                pm->ps->saberLength = 0;
            }

            if (pm->gent) {
                G_CreateG2AttachedWeaponModel(pm->gent, pm->ps->saberModel);
            }
        }
        else { // FIXME: pull it back to us?
        }

        if (pm->gent) {
            WP_SaberInitBladeData(pm->gent);
            if (!pm->ps->clientNum && cg_saberAutoThird.value) {
                gi.cvar_set("cg_thirdperson", "1");
            }
        }
        if (trueSwitch) { // actually did switch weapons, play anim
            PM_SetSaberMove(saberMoveName_t::LS_DRAW);
        }
    }
    else { // switched away from saber
        if (pm->gent) {
            // remove the sabre if we had it.
            if (pm->gent->weaponModel >= 0) {
                gi.G2API_RemoveGhoul2Model(pm->gent->ghoul2, pm->gent->weaponModel);
                pm->gent->weaponModel = -1;
            }
            if (weaponData[weapon].weaponMdl[0]) { // might be NONE, so check if it has a model
                G_CreateG2AttachedWeaponModel(pm->gent, weaponData[weapon].weaponMdl);
            }
        }

        if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_GALAKMECH) {
            if (pm->gent->alt_fire) { // FIXME: attack delay?
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_RAISEWEAP3,
                           setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
            }
            else { // FIXME: attack delay?
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_RAISEWEAP1,
                           setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
            }
        }
        else {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_RAISEWEAP1, setAnimFlags_t::SETANIM_FLAG_HOLD);
        }
        if (!pm->ps->clientNum && cg_gunAutoFirst.value && oldWeap == weapon_t::WP_SABER && weapon != weapon_t::WP_NONE) {
            gi.cvar_set("cg_thirdperson", "0");
        }
        pm->ps->saberMove = saberMoveName_t::LS_NONE;
        pm->ps->saberBlocking = saberBlockType_t::BLK_NO;
        pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_NONE;
    }
}

void PM_SetSaberMove(saberMoveName_t newMove)
{
    setAnimFlags_t setflags = saberMoveData[newMove].animSetFlags;
    animNumber_t anim = saberMoveData[newMove].animToUse;
    setAnimParts_t parts = setAnimParts_t::SETANIM_TORSO;

    if (cg_debugSaber.integer & 0x01 && (newMove != saberMoveName_t::LS_READY)) {
        Q_Println("SetSaberMove:  From '{}' to '{}'", saberMoveData[pm->ps->saberMove].name, saberMoveData[newMove].name);
    }

    if (newMove == saberMoveName_t::LS_READY || newMove == saberMoveName_t::LS_A_FLIP_STAB ||
        newMove == saberMoveName_t::LS_A_FLIP_SLASH) { // finished with a kata (or in a special move) reset attack counter
        pm->ps->saberAttackChainCount = 0;
    }
    else if (PM_SaberInAttack(newMove)) { // continuing with a kata, increment attack counter
        pm->ps->saberAttackChainCount++;
    }

    if (newMove == saberMoveName_t::LS_READY) {
        switch (pm->ps->saberAnimLevel) {
        case forcePowerLevels_t::FORCE_LEVEL_1:
        case forcePowerLevels_t::FORCE_LEVEL_5:
            anim = animNumber_t::BOTH_SABERFAST_STANCE;
            break;
        case forcePowerLevels_t::FORCE_LEVEL_3:
            anim = animNumber_t::BOTH_SABERSLOW_STANCE;
            break;
        case forcePowerLevels_t::FORCE_LEVEL_0:
        case forcePowerLevels_t::FORCE_LEVEL_2:
        case forcePowerLevels_t::FORCE_LEVEL_4:
        default:
            anim = animNumber_t::BOTH_STAND2;
            break;
        }
    }
    else if (pm->ps->saberAnimLevel > forcePowerLevels_t::FORCE_LEVEL_1 && !PM_SaberInIdle(newMove) && !PM_SaberInParry(newMove) &&
             !PM_SaberInKnockaway(newMove) && !PM_SaberInBrokenParry(newMove) && !PM_SaberInReflect(newMove) &&
             !PM_SaberInSpecial(newMove)) { // readies, parries and reflections have only 1 level
        // increment the anim to the next level of saber anims
        anim += (pm->ps->saberAnimLevel - forcePowerLevels_t::FORCE_LEVEL_1) * SABER_ANIM_GROUP_SIZE;
    }

    // If the move does the same animation as the last one, we need to force a restart...
    if (saberMoveData[pm->ps->saberMove].animToUse == anim && newMove > saberMoveName_t::LS_PUTAWAY) {
        setflags |= setAnimFlags_t::SETANIM_FLAG_RESTART;
    }

    if (anim == animNumber_t::BOTH_STAND2 || anim == animNumber_t::BOTH_SABERFAST_STANCE || anim == animNumber_t::BOTH_SABERSLOW_STANCE) {
        // FIXME: play both_stand2_random1 when you've been idle for a while
        if (pm->ps->legsAnim == animNumber_t::BOTH_WALK1) {
            anim = animNumber_t::BOTH_WALK1;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN2) {
            anim = animNumber_t::BOTH_RUN2;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_WALK2) {
            anim = animNumber_t::BOTH_WALK2;
        }
    }

    if (newMove == saberMoveName_t::LS_A_LUNGE || newMove == saberMoveName_t::LS_A_JUMP_T__B_ || newMove == saberMoveName_t::LS_A_BACKSTAB ||
        newMove == saberMoveName_t::LS_A_BACK || newMove == saberMoveName_t::LS_A_BACK_CR || newMove == saberMoveName_t::LS_A_FLIP_STAB ||
        newMove == saberMoveName_t::LS_A_FLIP_SLASH) {
        parts = setAnimParts_t::SETANIM_BOTH;
    }
    else if (PM_SpinningSaberAnim(anim)) { // spins must be played on entire body
        parts = setAnimParts_t::SETANIM_BOTH;
    }
    else if ((!pm->cmd.forwardmove && !pm->cmd.rightmove && !pm->cmd.upmove)) { // not trying to run, duck or jump
        if (!PM_FlippingAnim(pm->ps->legsAnim) && !PM_InRoll(pm->ps) && !PM_InKnockDown(pm->ps) && !PM_JumpingAnim(pm->ps->legsAnim) &&
            !PM_PainAnim(pm->ps->legsAnim) && !PM_InSpecialJump(pm->ps->legsAnim) && !PM_InSlopeAnim(pm->ps->legsAnim) &&
            //! PM_CrouchAnim( pm->ps->legsAnim ) &&
            // pm->cmd.upmove >= 0 &&
            !Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) {
            parts = setAnimParts_t::SETANIM_BOTH;
        }
    }
    PM_SetAnim(pm, parts, anim, setflags, saberMoveData[newMove].blendTime);

    if (pm->ps->torsoAnim == anim) { // successfully changed anims
        // special check for *starting* a saber swing
        if (pm->gent && pm->ps->saberLength > 1) {
            if (PM_SaberInAttack(newMove) || PM_SaberInSpecialAttack(anim)) { // playing an attack
                if (pm->ps->saberMove != newMove) {                           // wasn't playing that attack before
                    if (PM_SaberInSpecialAttack(anim)) {
                        G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_WEAPON, Q_Format("sound/weapons/saber/saberhup{}.wav", Q_irand(1, 3)).c_str());
                    }
                    else {
                        switch (pm->ps->saberAnimLevel) {
                        case forcePowerLevels_t::FORCE_LEVEL_4:
                        case forcePowerLevels_t::FORCE_LEVEL_3:
                            G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_WEAPON, Q_Format("sound/weapons/saber/saberhup{}.wav", Q_irand(7, 9)).c_str());
                            break;
                        case forcePowerLevels_t::FORCE_LEVEL_2:
                            G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_WEAPON, Q_Format("sound/weapons/saber/saberhup{}.wav", Q_irand(4, 6)).c_str());
                            break;
                        case forcePowerLevels_t::FORCE_LEVEL_5:
                        case forcePowerLevels_t::FORCE_LEVEL_1:
                            G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_WEAPON, Q_Format("sound/weapons/saber/saberhup{}.wav", Q_irand(1, 3)).c_str());
                            break;
                        default:
                            break;
                        }
                    }
                }
            }
            else if (PM_SaberInStart(newMove) && pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_3) {
                G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_WEAPON, Q_Format("sound/weapons/saber/saberhup{}.wav", Q_irand(1, 3)).c_str());
            }
        }

        pm->ps->saberMove = newMove;
        pm->ps->saberBlocking = saberMoveData[newMove].blocking;

        if (pm->ps->clientNum == 0 || PM_ControlledByPlayer()) {
            if (pm->ps->saberBlocked >= saberBlockedType_t::BLOCKED_UPPER_RIGHT_PROJ && pm->ps->saberBlocked <= saberBlockedType_t::BLOCKED_TOP_PROJ &&
                newMove >= saberMoveName_t::LS_REFLECT_UP && newMove <= saberMoveName_t::LS_REFLECT_LL) { // don't clear it when blocking projectiles
            }
            else {
                pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_NONE;
            }
        }
        else if (pm->ps->saberBlocked <= saberBlockedType_t::BLOCKED_ATK_BOUNCE || !pm->ps->saberActive ||
                 (newMove < saberMoveName_t::LS_PARRY_UR || newMove > saberMoveName_t::LS_REFLECT_LL)) { // NPCs only clear blocked if not blocking?
            pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_NONE;
        }

        if (pm->gent && pm->gent->client) {
            if (saberMoveData[newMove].trailLength > 0) {
                pm->gent->client->saberTrail.inAction = true;
                pm->gent->client->saberTrail.duration =
                    saberMoveData[newMove].trailLength; // saber trail lasts for 75ms...feel free to change this if you want it longer or shorter
            }
            else {
                pm->gent->client->saberTrail.inAction = false;
            }
        }
    }
}

/*
==============
PM_Use

Generates a use event
==============
*/
constexpr int USE_DELAY = 250;

void PM_Use(void)
{
    if (pm->ps->useTime > 0) {
        pm->ps->useTime -= pml.msec;
        if (pm->ps->useTime < 0) {
            pm->ps->useTime = 0;
        }
    }

    if (pm->ps->useTime > 0) {
        return;
    }

    if (!Any(pm->cmd.buttons & buttonFlags_t::BUTTON_USE)) {
        pm->useEvent = entity_event_t::EV_NONE;
        pm->ps->useTime = 0;
        return;
    }

    pm->useEvent = entity_event_t::EV_USE;
    pm->ps->useTime = USE_DELAY;
}

extern saberMoveName_t PM_AttackForEnemyPos(bool allowFB);
saberMoveName_t PM_NPCSaberAttackFromQuad(saberQuadrant_t quad)
{
    // FIXME: this should be an AI decision
    //  It should be based on the enemy's current LS_ move, saberAnimLevel,
    //  the jedi's difficulty level, rank and FP_OFFENSE skill...
    saberMoveName_t autoMove = saberMoveName_t::LS_NONE;
    if (pm->gent && ((pm->gent->NPC && pm->gent->NPC->rank != rank_t::RANK_ENSIGN && pm->gent->NPC->rank != rank_t::RANK_CIVILIAN) ||
                     (pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_TAVION))) {
        autoMove = PM_AttackForEnemyPos(true);
    }
    if (autoMove != saberMoveName_t::LS_NONE && PM_SaberInSpecial(autoMove)) { // if have opportunity to do a special attack, do one
        return autoMove;
    }
    else { // pick another one
        saberMoveName_t newmove = saberMoveName_t::LS_NONE;
        switch (quad) {
        case saberQuadrant_t::Q_T: // blocked top
            if (Q_irand(0, 1)) {
                newmove = saberMoveName_t::LS_A_T2B;
            }
            else {
                newmove = saberMoveName_t::LS_A_TR2BL;
            }
            break;
        case saberQuadrant_t::Q_TR:
            if (!Q_irand(0, 2)) {
                newmove = saberMoveName_t::LS_A_R2L;
            }
            else if (!Q_irand(0, 1)) {
                newmove = saberMoveName_t::LS_A_TR2BL;
            }
            else {
                newmove = saberMoveName_t::LS_T1_TR_BR;
            }
            break;
        case saberQuadrant_t::Q_TL:
            if (!Q_irand(0, 2)) {
                newmove = saberMoveName_t::LS_A_L2R;
            }
            else if (!Q_irand(0, 1)) {
                newmove = saberMoveName_t::LS_A_TL2BR;
            }
            else {
                newmove = saberMoveName_t::LS_T1_TL_BL;
            }
            break;
        case saberQuadrant_t::Q_BR:
            if (!Q_irand(0, 2)) {
                newmove = saberMoveName_t::LS_A_BR2TL;
            }
            else if (!Q_irand(0, 1)) {
                newmove = saberMoveName_t::LS_T1_BR_TR;
            }
            else {
                newmove = saberMoveName_t::LS_A_R2L;
            }
            break;
        case saberQuadrant_t::Q_BL:
            if (!Q_irand(0, 2)) {
                newmove = saberMoveName_t::LS_A_BL2TR;
            }
            else if (!Q_irand(0, 1)) {
                newmove = saberMoveName_t::LS_T1_BL_TL;
            }
            else {
                newmove = saberMoveName_t::LS_A_L2R;
            }
            break;
        case saberQuadrant_t::Q_L:
            if (!Q_irand(0, 2)) {
                newmove = saberMoveName_t::LS_A_L2R;
            }
            else if (!Q_irand(0, 1)) {
                newmove = saberMoveName_t::LS_T1__L_T_;
            }
            else {
                newmove = saberMoveName_t::LS_A_R2L;
            }
            break;
        case saberQuadrant_t::Q_R:
            if (!Q_irand(0, 2)) {
                newmove = saberMoveName_t::LS_A_R2L;
            }
            else if (!Q_irand(0, 1)) {
                newmove = saberMoveName_t::LS_T1__R_T_;
            }
            else {
                newmove = saberMoveName_t::LS_A_L2R;
            }
            break;
        case saberQuadrant_t::Q_B:
            if ((pm->gent && pm->gent->NPC && pm->gent->NPC->rank >= rank_t::RANK_LT_JG)) {                // fencers and above can do bottom-up attack
                if (Q_irand(0, to_underlying(pm->gent->NPC->rank)) >= to_underlying(rank_t::RANK_LT_JG)) { // but not overly likely
                    newmove = saberMoveName_t::LS_A_LUNGE;
                }
            }
            break;
        default:
            break;
        }
        return newmove;
    }
}

saberQuadrant_t PM_SaberMoveQuadrantForMovement(usercmd_t* ucmd)
{
    if (ucmd->rightmove > 0) {       // moving right
        if (ucmd->forwardmove > 0) { // forward right = TL2BR slash
            return saberQuadrant_t::Q_TL;
        }
        else if (ucmd->forwardmove < 0) { // backward right = BL2TR uppercut
            return saberQuadrant_t::Q_BL;
        }
        else { // just right is a left slice
            return saberQuadrant_t::Q_L;
        }
    }
    else if (ucmd->rightmove < 0) {  // moving left
        if (ucmd->forwardmove > 0) { // forward left = TR2BL slash
            return saberQuadrant_t::Q_TR;
        }
        else if (ucmd->forwardmove < 0) { // backward left = BR2TL uppercut
            return saberQuadrant_t::Q_BR;
        }
        else { // just left is a right slice
            return saberQuadrant_t::Q_R;
        }
    }
    else {                           // not moving left or right
        if (ucmd->forwardmove > 0) { // forward= T2B slash
            return saberQuadrant_t::Q_T;
        }
        else if (ucmd->forwardmove < 0) { // backward= T2B slash	//or B2T uppercut?
            return saberQuadrant_t::Q_T;
        }
        else // if ( curmove == saberMoveName_t::LS_READY )//???
        {    // Not moving at all
            return saberQuadrant_t::Q_R;
        }
    }
    // return saberQuadrant_t::Q_R;//????
}

void PM_SetAnimFrame(gentity_t* gent, int frame, bool torso, bool legs)
{
    if (!gi.G2API_HaveWeGhoul2Models(gent->ghoul2)) {
        return;
    }
    int actualTime = (cg.time ? cg.time : level.time);
    if (torso && gent->lowerLumbarBone != -1) // gent->upperLumbarBone
    {
        gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->lowerLumbarBone, // gent->upperLumbarBone
                                  frame, frame + 1, boneFlags_t::BONE_ANIM_OVERRIDE_FREEZE | boneFlags_t::BONE_ANIM_BLEND, 1, actualTime, frame, 150);
        if (gent->motionBone != -1) {
            gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->motionBone, // gent->upperLumbarBone
                                      frame, frame + 1, boneFlags_t::BONE_ANIM_OVERRIDE_FREEZE | boneFlags_t::BONE_ANIM_BLEND, 1, actualTime, frame, 150);
        }
    }
    if (legs && gent->rootBone != -1) {
        gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->rootBone, frame, frame + 1,
                                  boneFlags_t::BONE_ANIM_OVERRIDE_FREEZE | boneFlags_t::BONE_ANIM_BLEND, 1, actualTime, frame, 150);
    }
}

animNumber_t PM_SaberLockWinAnim(saberLockResult_t result)
{
    animNumber_t winAnim = animNumber_t::ANIM_NONE;
    switch (pm->ps->torsoAnim) {
        /*
            default:
        #ifndef FINAL_BUILD
                Q_Println(S_COLOR_RED"ERROR-PM_SaberLockBreak: {} not in saberlock anim, anim = ({}){}", pm->gent->NPC_type, to_underlying(
        pm->ps->torsoAnim ), BG_AnimName( pm->ps->torsoAnim )); #endif
        */
    case animNumber_t::BOTH_BF2LOCK:
        if (result == saberLockResult_t::LOCK_DRAW) {
            winAnim = animNumber_t::BOTH_BF1BREAK;
        }
        else {
            pm->ps->saberMove = saberMoveName_t::LS_A_T2B;
            winAnim = animNumber_t::BOTH_A3_T__B_;
        }
        break;
    case animNumber_t::BOTH_BF1LOCK:
        if (result == saberLockResult_t::LOCK_DRAW) {
            winAnim = animNumber_t::BOTH_KNOCKDOWN4;
        }
        else {
            pm->ps->saberMove = saberMoveName_t::LS_K1_T_;
            winAnim = animNumber_t::BOTH_K1_S1_T_;
        }
        break;
    case animNumber_t::BOTH_CWCIRCLELOCK:
        if (result == saberLockResult_t::LOCK_DRAW) {
            pm->ps->saberMove = pm->ps->saberBounceMove = saberMoveName_t::LS_V1_BL;
            pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_PARRY_BROKEN;
            winAnim = animNumber_t::BOTH_V1_BL_S1;
        }
        else {
            winAnim = animNumber_t::BOTH_CWCIRCLEBREAK;
        }
        break;
    case animNumber_t::BOTH_CCWCIRCLELOCK:
        if (result == saberLockResult_t::LOCK_DRAW) {
            pm->ps->saberMove = pm->ps->saberBounceMove = saberMoveName_t::LS_V1_BR;
            pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_PARRY_BROKEN;
            winAnim = animNumber_t::BOTH_V1_BR_S1;
        }
        else {
            winAnim = animNumber_t::BOTH_CCWCIRCLEBREAK;
        }
        break;
    default:
        break;
    }
    if (winAnim != animNumber_t::ANIM_NONE) {
        PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, winAnim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
        pm->ps->weaponTime = pm->ps->torsoAnimTimer;
    }
    return winAnim;
}

animNumber_t PM_SaberLockLoseAnim(gentity_t* genemy, saberLockResult_t result)
{
    animNumber_t loseAnim = animNumber_t::ANIM_NONE;
    switch (genemy->client->ps.torsoAnim) {
        /*
            default:
        #ifndef FINAL_BUILD
                Q_Println(S_COLOR_RED"ERROR-PM_SaberLockBreak: {} not in saberlock anim, anim = ({}){}", genemy->NPC_type, to_underlying(
        genemy->client->ps.torsoAnim ), BG_AnimName( genemy->client->ps.torsoAnim )); #endif
        */
    case animNumber_t::BOTH_BF2LOCK:
        if (result == saberLockResult_t::LOCK_DRAW) {
            loseAnim = animNumber_t::BOTH_BF1BREAK;
        }
        else {
            if (result == saberLockResult_t::LOCK_STALEMATE) { // no-one won
                genemy->client->ps.saberMove = saberMoveName_t::LS_K1_T_;
                loseAnim = animNumber_t::BOTH_K1_S1_T_;
            }
            else { // FIXME: this anim needs to transition back to ready when done
                loseAnim = animNumber_t::BOTH_BF1BREAK;
            }
        }
        break;
    case animNumber_t::BOTH_BF1LOCK:
        if (result == saberLockResult_t::LOCK_DRAW) {
            loseAnim = animNumber_t::BOTH_KNOCKDOWN4;
        }
        else {
            if (result == saberLockResult_t::LOCK_STALEMATE) { // no-one won
                genemy->client->ps.saberMove = saberMoveName_t::LS_A_T2B;
                loseAnim = animNumber_t::BOTH_A3_T__B_;
            }
            else {
                loseAnim = animNumber_t::BOTH_KNOCKDOWN4;
            }
        }
        break;
    case animNumber_t::BOTH_CWCIRCLELOCK:
        if (result == saberLockResult_t::LOCK_DRAW) {
            genemy->client->ps.saberMove = genemy->client->ps.saberBounceMove = saberMoveName_t::LS_V1_BL;
            genemy->client->ps.saberBlocked = saberBlockedType_t::BLOCKED_PARRY_BROKEN;
            loseAnim = animNumber_t::BOTH_V1_BL_S1;
        }
        else {
            if (result == saberLockResult_t::LOCK_STALEMATE) { // no-one won
                loseAnim = animNumber_t::BOTH_CCWCIRCLEBREAK;
            }
            else {
                genemy->client->ps.saberMove = genemy->client->ps.saberBounceMove = saberMoveName_t::LS_V1_BL;
                genemy->client->ps.saberBlocked = saberBlockedType_t::BLOCKED_PARRY_BROKEN;
                loseAnim = animNumber_t::BOTH_V1_BL_S1;
                /*
                genemy->client->ps.saberMove = genemy->client->ps.saberBounceMove = saberMoveName_t::LS_H1_BR;
                genemy->client->ps.saberBlocked = saberBlockedType_t::BLOCKED_PARRY_BROKEN;
                loseAnim = animNumber_t::BOTH_H1_S1_BL;
                */
            }
        }
        break;
    case animNumber_t::BOTH_CCWCIRCLELOCK:
        if (result == saberLockResult_t::LOCK_DRAW) {
            genemy->client->ps.saberMove = genemy->client->ps.saberBounceMove = saberMoveName_t::LS_V1_BR;
            genemy->client->ps.saberBlocked = saberBlockedType_t::BLOCKED_PARRY_BROKEN;
            loseAnim = animNumber_t::BOTH_V1_BR_S1;
        }
        else {
            if (result == saberLockResult_t::LOCK_STALEMATE) { // no-one won
                loseAnim = animNumber_t::BOTH_CWCIRCLEBREAK;
            }
            else {
                genemy->client->ps.saberMove = genemy->client->ps.saberBounceMove = saberMoveName_t::LS_V1_BR;
                genemy->client->ps.saberBlocked = saberBlockedType_t::BLOCKED_PARRY_BROKEN;
                loseAnim = animNumber_t::BOTH_V1_BR_S1;
                /*
                genemy->client->ps.saberMove = genemy->client->ps.saberBounceMove = saberMoveName_t::LS_H1_BL;
                genemy->client->ps.saberBlocked = saberBlockedType_t::BLOCKED_PARRY_BROKEN;
                loseAnim = animNumber_t::BOTH_H1_S1_BR;
                */
            }
        }
        break;
    default:
        break;
    }
    if (loseAnim != animNumber_t::ANIM_NONE) {
        NPC_SetAnim(genemy, setAnimParts_t::SETANIM_BOTH, loseAnim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
        genemy->client->ps.weaponTime = genemy->client->ps.torsoAnimTimer; // + 250;
    }
    return loseAnim;
}

void PM_SaberLockBreak(gentity_t* gent, gentity_t* genemy, saberLockResult_t result, int victoryStrength)
{
    animNumber_t winAnim = animNumber_t::ANIM_NONE, loseAnim = animNumber_t::ANIM_NONE;

    winAnim = PM_SaberLockWinAnim(result);
    if (genemy && genemy->client) {
        loseAnim = PM_SaberLockLoseAnim(genemy, result);
    }

    if (d_saberCombat->integer) {
        Q_Println("{} won saber lock, anim = {}!", gent->NPC_type, winAnim != animNumber_t::ANIM_NONE ? BG_AnimName(winAnim) : "none");
        Q_Println("{} lost saber lock, anim = {}!", genemy->NPC_type, loseAnim != animNumber_t::ANIM_NONE ? BG_AnimName(loseAnim) : "none");
    }

    pm->ps->saberLockTime = genemy->client->ps.saberLockTime = 0;
    pm->ps->saberLockEnemy = genemy->client->ps.saberLockEnemy = ENTITYNUM_NONE;

    PM_AddEvent(entity_event_t::EV_JUMP);
    if (result == saberLockResult_t::LOCK_STALEMATE) { // no-one won
        G_AddEvent(genemy, entity_event_t::EV_JUMP, 0);
    }
    else {
        if (pm->ps->clientNum) {                                        // an NPC
            pm->ps->saberEventFlags |= saberEventFlags_t::SEF_LOCK_WON; // tell the winner to press the advantage
        }
        // painDebounceTime will stop them from doing anything
        genemy->painDebounceTime = level.time + genemy->client->ps.torsoAnimTimer + 500;
        if (Q_irand(0, 1)) {
            G_AddEvent(genemy, entity_event_t::EV_PAIN, Q_irand(0, 75));
        }
        else {
            if (genemy->NPC) {
                genemy->NPC->blockedSpeechDebounceTime = 0;
            }
            G_AddVoiceEvent(genemy, Q_irand(entity_event_t::EV_PUSHED1, entity_event_t::EV_PUSHED3), 500);
        }
        if (result == saberLockResult_t::LOCK_VICTORY && winAnim != animNumber_t::BOTH_CCWCIRCLEBREAK) { // one person won
            if (Q_irand(forcePowerLevels_t::FORCE_LEVEL_1, forcePowerLevels_t::FORCE_LEVEL_2) < pm->ps->forcePowerLevel[forcePowers_t::FP_SABER_OFFENSE]) {
                if ((!genemy->s.number && genemy->health <= 25) // player low on health
                    || (genemy->s.number && genemy->client->NPC_class != class_t::CLASS_LUKE && genemy->client->NPC_class != class_t::CLASS_TAVION &&
                        genemy->client->NPC_class != class_t::CLASS_DESANN) // any NPC that's not a boss character
                    || (genemy->s.number && genemy->health <= 50))          // boss character with less than 50 health left
                {                                                           // possibly knock saber out of hand OR cut hand off!
                    vec3_t throwDir = {0, 0, 350};
                    saberMoveName_t winMove = pm->ps->saberMove;
                    switch (winAnim) {
                    case animNumber_t::BOTH_A3_T__B_:
                        winAnim = animNumber_t::BOTH_D1_TL___;
                        winMove = saberMoveName_t::LS_D1_TL;
                        // FIXME: mod throwDir?
                        break;
                    case animNumber_t::BOTH_K1_S1_T_:
                        // FIXME: mod throwDir?
                        break;
                    case animNumber_t::BOTH_CWCIRCLEBREAK:
                        // FIXME: mod throwDir?
                        break;
                    case animNumber_t::BOTH_CCWCIRCLEBREAK:
                        winAnim = animNumber_t::BOTH_A1_BR_TL;
                        winMove = saberMoveName_t::LS_A_BR2TL;
                        // FIXME: mod throwDir?
                        break;
                    default:
                        break;
                    }
                    if (Q_irand(0, 25) < victoryStrength && ((!genemy->s.number && genemy->health <= 10) || genemy->s.number)) {
                        NPC_SetAnim(genemy, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_RIGHTHANDCHOPPEDOFF,
                                    setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD); // force this
                        genemy->client->dismembered = false;
                        G_DoDismemberment(genemy, genemy->client->renderInfo.handRPoint, meansOfDeath_t::MOD_SABER, 1000, hitLocation_t::HL_HAND_RT, true);
                        G_Damage(genemy, gent, gent, throwDir, genemy->client->renderInfo.handRPoint, genemy->health + 10,
                                 damageFlags_t::DAMAGE_NO_PROTECTION | damageFlags_t::DAMAGE_NO_ARMOR | damageFlags_t::DAMAGE_NO_KNOCKBACK |
                                     damageFlags_t::DAMAGE_NO_HIT_LOC,
                                 meansOfDeath_t::MOD_SABER, hitLocation_t::HL_NONE);

                        PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, winAnim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                        pm->ps->weaponTime = pm->ps->torsoAnimTimer + 500;
                        pm->ps->saberMove = winMove;
                    }
                    else if (Q_irand(0, 10) < victoryStrength) {
                        WP_SaberLose(genemy, throwDir);

                        PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, winAnim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                        pm->ps->weaponTime = pm->ps->torsoAnimTimer;
                        pm->ps->saberMove = winMove;
                    }
                }
            }
        }
    }
}

int G_SaberLockStrength(gentity_t* gent)
{
    if (gent->s.number) {
        if (gent->client->NPC_class == class_t::CLASS_DESANN || gent->client->NPC_class == class_t::CLASS_LUKE) {
            return 5 + Q_irand(0, g_spskill->integer);
        }
        else {
            return to_underlying(gent->client->ps.forcePowerLevel[forcePowers_t::FP_SABER_OFFENSE]) + Q_irand(0, g_spskill->integer);
        }
    }
    else { // player
        return to_underlying(gent->client->ps.forcePowerLevel[forcePowers_t::FP_SABER_OFFENSE]) + Q_irand(0, g_spskill->integer);
    }
}

extern bool ValidAnimFileIndex(int index);
bool PM_SaberLocked(void)
{
    if (pm->ps->saberLockEnemy == ENTITYNUM_NONE) {
        if (pm->ps->torsoAnim == animNumber_t::BOTH_BF2LOCK || pm->ps->torsoAnim == animNumber_t::BOTH_BF1LOCK ||
            pm->ps->torsoAnim == animNumber_t::BOTH_CWCIRCLELOCK || pm->ps->torsoAnim == animNumber_t::BOTH_CCWCIRCLELOCK) { // wtf?  Maybe enemy died?
            PM_SaberLockWinAnim(saberLockResult_t::LOCK_STALEMATE);
        }
        return false;
    }
    gentity_t* gent = pm->gent;
    if (!gent) {
        return false;
    }
    gentity_t* genemy = &g_entities[pm->ps->saberLockEnemy];
    if (!genemy) {
        return false;
    }
    if ((pm->ps->torsoAnim == animNumber_t::BOTH_BF2LOCK || pm->ps->torsoAnim == animNumber_t::BOTH_BF1LOCK ||
         pm->ps->torsoAnim == animNumber_t::BOTH_CWCIRCLELOCK || pm->ps->torsoAnim == animNumber_t::BOTH_CCWCIRCLELOCK) &&
        (genemy->client->ps.torsoAnim == animNumber_t::BOTH_BF2LOCK || genemy->client->ps.torsoAnim == animNumber_t::BOTH_BF1LOCK ||
         genemy->client->ps.torsoAnim == animNumber_t::BOTH_CWCIRCLELOCK || genemy->client->ps.torsoAnim == animNumber_t::BOTH_CCWCIRCLELOCK)) {
        if (pm->ps->saberLockTime <= level.time + 500 && pm->ps->saberLockEnemy != ENTITYNUM_NONE) { // lock just ended
            int strength = G_SaberLockStrength(gent);
            int eStrength = G_SaberLockStrength(genemy);
            if (strength > 1 && eStrength > 1 && !Q_irand(0, Q_fabs((double)strength - eStrength) + 1)) { // both knock each other down!
                PM_SaberLockBreak(gent, genemy, saberLockResult_t::LOCK_DRAW, 0);
            }
            else { // both "win"
                PM_SaberLockBreak(gent, genemy, saberLockResult_t::LOCK_STALEMATE, 0);
            }
            return true;
        }
        else if (pm->ps->saberLockTime < level.time) { // done... tie breaker above should have handled this, but...?
            if (PM_SaberLockAnim(pm->ps->torsoAnim) && pm->ps->torsoAnimTimer > 0) {
                pm->ps->torsoAnimTimer = 0;
            }
            if (PM_SaberLockAnim(pm->ps->legsAnim) && pm->ps->legsAnimTimer > 0) {
                pm->ps->legsAnimTimer = 0;
            }
            return false;
        }
        else if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)) {   // holding attack
            if (!Any(pm->ps->pm_flags & pmoveFlags_t::PMF_ATTACK_HELD)) { // tapping
                int remaining = 0;
                if (ValidAnimFileIndex(gent->client->clientInfo.animFileIndex)) {
                    animation_t* anim;
                    float currentFrame, junk2;
                    int curFrame, junk;
                    boneFlags_t junkFlags;
                    int strength = 1;
                    anim = &level.knownAnimFileSets[gent->client->clientInfo.animFileIndex].animations[pm->ps->torsoAnim];
#ifdef _DEBUG
                    bool ret =
#endif // _DEBUG
                        gi.G2API_GetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->lowerLumbarBone, (cg.time ? cg.time : level.time), &currentFrame,
                                                  &junk, &junk, &junkFlags, &junk2, nullptr);
#ifdef _DEBUG
                    assert(ret); // this would be pretty bad, the below code seems to assume the call succeeds. -gil
#endif
                    strength = G_SaberLockStrength(gent);
                    if (pm->ps->torsoAnim == animNumber_t::BOTH_CCWCIRCLELOCK || pm->ps->torsoAnim == animNumber_t::BOTH_BF2LOCK) {
                        curFrame = Q_floorf(currentFrame) - strength;
                        // drop my frame one
                        if (curFrame <= anim->firstFrame) { // I won!  Break out
                            PM_SaberLockBreak(gent, genemy, saberLockResult_t::LOCK_VICTORY, strength);
                            return true;
                        }
                        else {
                            PM_SetAnimFrame(gent, curFrame, true, true);
                            remaining = curFrame - anim->firstFrame;
                            if (d_saberCombat->integer) {
                                Q_Println("{} pushing in saber lock, {} frames to go!", gent->NPC_type, remaining);
                            }
                        }
                    }
                    else {
                        curFrame = Q_ceilf(currentFrame) + strength;
                        // advance my frame one
                        if (curFrame >= anim->firstFrame + anim->numFrames) { // I won!  Break out
                            PM_SaberLockBreak(gent, genemy, saberLockResult_t::LOCK_VICTORY, strength);
                            return true;
                        }
                        else {
                            PM_SetAnimFrame(gent, curFrame, true, true);
                            remaining = anim->firstFrame + anim->numFrames - curFrame;
                            if (d_saberCombat->integer) {
                                Q_Println("{} pushing in saber lock, {} frames to go!", gent->NPC_type, remaining);
                            }
                        }
                    }
                    if (!Q_irand(0, 2)) {
                        if (!pm->ps->clientNum) {
                            if (!Q_irand(0, 3)) {
                                PM_AddEvent(entity_event_t::EV_JUMP);
                            }
                            else {
                                PM_AddEvent(Q_irand(entity_event_t::EV_PUSHED1, entity_event_t::EV_PUSHED3));
                            }
                        }
                        else {
                            if (gent->NPC && gent->NPC->blockedSpeechDebounceTime < level.time) {
                                switch (Q_irand(0, 3)) {
                                case 0:
                                    PM_AddEvent(entity_event_t::EV_JUMP);
                                    break;
                                case 1:
                                    PM_AddEvent(Q_irand(entity_event_t::EV_ANGER1, entity_event_t::EV_ANGER3));
                                    gent->NPC->blockedSpeechDebounceTime = level.time + 3000;
                                    break;
                                case 2:
                                    PM_AddEvent(Q_irand(entity_event_t::EV_TAUNT1, entity_event_t::EV_TAUNT3));
                                    gent->NPC->blockedSpeechDebounceTime = level.time + 3000;
                                    break;
                                case 3:
                                    PM_AddEvent(Q_irand(entity_event_t::EV_GLOAT1, entity_event_t::EV_GLOAT3));
                                    gent->NPC->blockedSpeechDebounceTime = level.time + 3000;
                                    break;
                                }
                            }
                        }
                    }
                }
                else {
                    return false;
                }
                if (ValidAnimFileIndex(genemy->client->clientInfo.animFileIndex)) {
                    animation_t* anim;
                    anim = &level.knownAnimFileSets[genemy->client->clientInfo.animFileIndex].animations[genemy->client->ps.torsoAnim];
                    /*
                    float		currentFrame, junk2;
                    int			junk;
                    boneFlags_t			junkFlags;

                    gi.G2API_GetBoneAnimIndex( &genemy->ghoul2[genemy->playerModel], genemy->lowerLumbarBone, (cg.time?cg.time:level.time), &currentFrame,
                    &junk, &junk, &junkFlags, &junk2, nullptr );
                    */

                    if (genemy->client->ps.torsoAnim == animNumber_t::BOTH_CWCIRCLELOCK || genemy->client->ps.torsoAnim == animNumber_t::BOTH_BF1LOCK) {
                        if (!Q_irand(0, 2)) {
                            switch (Q_irand(0, 3)) {
                            case 0:
                                G_AddEvent(genemy, entity_event_t::EV_PAIN, Q_floorf((float)genemy->health / genemy->max_health * 100.0f));
                                break;
                            case 1:
                                G_AddVoiceEvent(genemy, Q_irand(entity_event_t::EV_PUSHED1, entity_event_t::EV_PUSHED3), 500);
                                break;
                            case 2:
                                G_AddVoiceEvent(genemy, Q_irand(entity_event_t::EV_CHOKE1, entity_event_t::EV_CHOKE3), 500);
                                break;
                            case 3:
                                G_AddVoiceEvent(genemy, entity_event_t::EV_PUSHFAIL, 2000);
                                break;
                            }
                        }
                        PM_SetAnimFrame(genemy, anim->firstFrame + remaining, true, true);
                        /*
                        curFrame = Q_floorf( currentFrame );

                        //drop his frame one
                        if ( curFrame <= anim->firstFrame )
                        {//He lost!  Break out
                            PM_SaberLockBreak( gent, genemy, saberLockResult_t::LOCK_VICTORY, 0 );
                            return;
                        }
                        else
                        {
                            if ( d_saberCombat->integer )
                            {
                                Q_Println("{} losing in saber lock, {} frames to go!", genemy->NPC_type, (curFrame-1)-anim->firstFrame);
                            }
                            PM_SetAnimFrame( genemy, curFrame-1, true, true );
                        }
                        */
                    }
                    else {
                        PM_SetAnimFrame(genemy, anim->firstFrame + anim->numFrames - remaining, true, true);
                        /*
                        curFrame = Q_ceilf( currentFrame );
                        //advance his frame one
                        if ( curFrame >= anim->firstFrame+anim->numFrames )
                        {//He lost!  Break out
                            PM_SaberLockBreak( gent, genemy, saberLockResult_t::LOCK_VICTORY, 0 );
                            return;
                        }
                        else
                        {
                            if ( d_saberCombat->integer )
                            {
                                Q_Println("{} losing in saber lock, {} frames to go!", genemy->NPC_type, anim->firstFrame+anim->numFrames-(curFrame+1));
                            }
                            PM_SetAnimFrame( genemy, curFrame+1, true, true );
                        }
                        */
                    }
                }
            }
        }
        else { // FIXME: other ways out of a saberlock?
               // force-push?  (requires more force power?)
               // kick?  (requires anim ... hit jump key?)
               // roll?
               // backflip?
        }
    }
    else { // something broke us out of it
        if (gent->painDebounceTime > level.time && genemy->painDebounceTime > level.time) {
            PM_SaberLockBreak(gent, genemy, saberLockResult_t::LOCK_DRAW, 0);
        }
        else if (gent->painDebounceTime > level.time) {
            PM_SaberLockBreak(genemy, gent, saberLockResult_t::LOCK_VICTORY, 0);
        }
        else if (genemy->painDebounceTime > level.time) {
            PM_SaberLockBreak(gent, genemy, saberLockResult_t::LOCK_VICTORY, 0);
        }
        else {
            PM_SaberLockBreak(gent, genemy, saberLockResult_t::LOCK_STALEMATE, 0);
        }
    }
    return true;
}

/*
=================
PM_WeaponLightsaber

Consults a chart to choose what to do with the lightsaber.
While this is a little different than the Quake 3 code, there is no clean way of using the Q3 code for this kind of thing.
=================
*/
// Ultimate goal is to set the sabermove to the proper next location
// Note that if the resultant animation is NONE, then the animation is essentially "idle", and is set in WP_TorsoAnim
void PM_WeaponLightsaber(void)
{
    int addTime;
    bool delayed_fire = false;
    std::optional<animNumber_t> anim;
    saberMoveName_t curmove, newmove = saberMoveName_t::LS_NONE;

    if (!pm->ps->clientNum && cg.saberAnimLevelPending > forcePowerLevels_t::FORCE_LEVEL_0 && cg.saberAnimLevelPending != pm->ps->saberAnimLevel) {
        if (!PM_SaberInStart(pm->ps->saberMove) && !PM_SaberInTransition(pm->ps->saberMove) &&
            !PM_SaberInAttack(pm->ps->saberMove)) { // don't allow changes when in the middle of an attack set...(or delay the change until it's done)
            pm->ps->saberAnimLevel = cg.saberAnimLevelPending;
        }
    }
    /*
    if ( PM_InForceGetUp( pm->ps ) )
    {//if mostly up, can start attack
        if ( pm->ps->torsoAnimTimer > 800 )
        {//not up enough yet
            // make weapon function
            if ( pm->ps->weaponTime > 0 ) {
                pm->ps->weaponTime -= pml.msec;
                if ( pm->ps->weaponTime <= 0 )
                {
                    pm->ps->weaponTime = 0;
                }
            }
            return;
        }
        else
        {
            if ( Any( pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK ) )
            {//let an attack interrupt the torso part of this force getup
                pm->ps->weaponTime = 0;
            }
        }
    }
    else
    */
    if (PM_InKnockDown(pm->ps) || PM_InRoll(pm->ps)) { // in knockdown
        // make weapon function
        if (pm->ps->weaponTime > 0) {
            pm->ps->weaponTime -= pml.msec;
            if (pm->ps->weaponTime <= 0) {
                pm->ps->weaponTime = 0;
            }
        }
        return;
    }

    if (PM_SaberLocked()) {
        pm->ps->saberMove = saberMoveName_t::LS_NONE;
        return;
    }

    if (Any(pm->ps->saberEventFlags & saberEventFlags_t::SEF_INWATER)) // saber in water
    {
        pm->cmd.buttons &= ~(buttonFlags_t::BUTTON_ATTACK | buttonFlags_t::BUTTON_ALT_ATTACK);
    }

    bool saberInAir = true;
    if (!PM_SaberInBrokenParry(pm->ps->saberMove) && pm->ps->saberBlocked != saberBlockedType_t::BLOCKED_PARRY_BROKEN &&
        !PM_DodgeAnim(pm->ps->torsoAnim)) {                                                       // we're not stuck in a broken parry
        if (pm->ps->saberInFlight) {                                                              // guiding saber
            if (pm->ps->saberEntityNum < ENTITYNUM_NONE && pm->ps->saberEntityNum > 0)            // player is 0
            {                                                                                     //
                if (g_entities[pm->ps->saberEntityNum].s.pos.trType == trType_t::TR_STATIONARY) { // fell to the ground and we're not trying to pull it back
                    saberInAir = false;
                }
            }
            if (pm->ps->weaponTime <= 0 || pm->ps->weaponstate != weaponstate_t::WEAPON_FIRING) {
                if (pm->ps->weapon != UC_Weapon(pm->cmd)) {
                    PM_BeginWeaponChange(UC_Weapon(pm->cmd));
                }
            }
            else if (pm->ps->weapon == weapon_t::WP_SABER) { // guiding saber
                if (saberInAir) {
                    if (!PM_ForceAnim(pm->ps->torsoAnim) || pm->ps->torsoAnimTimer < 300) { // don't interrupt a force power anim
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_SABERPULL,
                                   setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                    }
                }
                return;
            }
        }
    }

    if (pm->gent && pm->gent->client && pm->gent->client->fireDelay > 0) { // FIXME: this is going to fire off one frame before you expect, actually
        pm->gent->client->fireDelay -= pml.msec;
        if (pm->gent->client->fireDelay <= 0) { // just finished delay timer
            pm->gent->client->fireDelay = 0;
            delayed_fire = true;
        }
    }

    // don't allow attack until all buttons are up
    if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_RESPAWNED)) {
        return;
    }

    // check for dead player
    if (pm->ps->stats[statIndex_t::STAT_HEALTH] <= 0) {
        if (pm->gent) {
            pm->gent->s.loopSound = 0;
        }
        return;
    }

    // make weapon function
    if (pm->ps->weaponTime > 0) {
        pm->ps->weaponTime -= pml.msec;
        if (pm->ps->weaponTime <= 0) {
            pm->ps->weaponTime = 0;
        }
    }

    if (!pm->ps->clientNum || PM_ControlledByPlayer()) { // player
        if (pm->ps->saberBlocked >= saberBlockedType_t::BLOCKED_UPPER_RIGHT_PROJ &&
            pm->ps->saberBlocked <= saberBlockedType_t::BLOCKED_TOP_PROJ) {                 // blocking a projectile
            if (pm->ps->forcePowerDebounce[forcePowers_t::FP_SABER_DEFENSE] < level.time) { // block is done or breaking out of it with an attack
                pm->ps->weaponTime = 0;
                pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_NONE;
            }
            else if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)) { // block is done or breaking out of it with an attack
                pm->ps->weaponTime = 0;
                pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_NONE;
            }
        }
    }

    // Now we react to a block action by the player's lightsaber.
    if (pm->ps->saberBlocked != saberBlockedType_t::BLOCKED_NONE) {
        if (pm->ps->saberMove > saberMoveName_t::LS_PUTAWAY && pm->ps->saberMove <= saberMoveName_t::LS_A_BL2TR &&
            pm->ps->saberBlocked != saberBlockedType_t::BLOCKED_PARRY_BROKEN &&
            (pm->ps->saberBlocked < saberBlockedType_t::BLOCKED_UPPER_RIGHT_PROJ ||
             pm->ps->saberBlocked > saberBlockedType_t::BLOCKED_TOP_PROJ)) //&& Q_irand( 0, 2 )
        {                                                                  // we parried another lightsaber while attacking, so treat it as a bounce
            pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_ATK_BOUNCE;
        }
        /*
        else if ( Any(pm->cmd.buttons&buttonFlags_t::BUTTON_ATTACK)
            && pm->ps->saberBlocked >= saberBlockedType_t::BLOCKED_UPPER_RIGHT
            && pm->ps->saberBlocked <= saberBlockedType_t::BLOCKED_TOP
            && !PM_SaberInKnockaway( pm->ps->saberBounceMove ) )
        {//if hitting attack during a parry (not already a knockaway)
            if ( pm->ps->forcePowerLevel[forcePowers_t::FP_SABER_DEFENSE] > forcePowerLevels_t::FORCE_LEVEL_2 )
            {//have high saber defense, turn the parry into a knockaway?
                //FIXME: this could actually be bad for us...?  Leaves us open
                pm->ps->saberBounceMove = PM_KnockawayForParry( pm->ps->saberBlocked );
            }
        }
        */

        if (pm->ps->saberBlocked != saberBlockedType_t::BLOCKED_ATK_BOUNCE) { // can't attack for twice whatever your skill level's parry debounce time is
            if (pm->ps->clientNum == 0 || PM_ControlledByPlayer()) {          // player
                if (pm->ps->forcePowerLevel[forcePowers_t::FP_SABER_DEFENSE] <= forcePowerLevels_t::FORCE_LEVEL_1) {
                    pm->ps->weaponTime = parryDebounce[pm->ps->forcePowerLevel[forcePowers_t::FP_SABER_DEFENSE]];
                }
            }
            else { // NPC
                // pm->ps->weaponTime = parryDebounce[pm->ps->forcePowerLevel[forcePowers_t::FP_SABER_DEFENSE]] * 2;
                if (pm->gent) {
                    pm->ps->weaponTime = Jedi_ReCalcParryTime(pm->gent, evasionType_t::EVASION_PARRY);
                }
                else { // WTF???
                    pm->ps->weaponTime = parryDebounce[pm->ps->forcePowerLevel[forcePowers_t::FP_SABER_DEFENSE]] * 2;
                }
            }
        }
        switch (pm->ps->saberBlocked) {
        case saberBlockedType_t::BLOCKED_PARRY_BROKEN:
            // whatever parry we were is in now broken, play the appropriate knocked-away anim
            {
                saberMoveName_t nextMove;
                if (PM_SaberInBrokenParry(pm->ps->saberBounceMove)) { // already have one...?
                    nextMove = pm->ps->saberBounceMove;
                }
                else {
                    nextMove = PM_BrokenParryForParry(pm->ps->saberMove);
                }
                if (nextMove != saberMoveName_t::LS_NONE) {
                    PM_SetSaberMove(nextMove);
                    pm->ps->weaponTime = pm->ps->torsoAnimTimer;
                }
                else { // Maybe in a knockaway?
                }
                // pm->ps->saberBounceMove = saberMoveName_t::LS_NONE;
            }
            break;
        case saberBlockedType_t::BLOCKED_ATK_BOUNCE:
            // If there is absolutely no blocked move in the chart, don't even mess with the animation.
            // OR if we are already in a block or parry.
            if (pm->ps->saberMove >=
                saberMoveName_t::LS_T1_BR__R /*LS_BOUNCE_TOP*/) //|| saberMoveData[pm->ps->saberMove].bounceMove == saberMoveName_t::LS_NONE )
            {                                                   // an actual bounce?  Other bounces before this are actually transitions?
                pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_NONE;
            }
            else if (PM_SaberInBounce(pm->ps->saberMove) ||
                     !PM_SaberInAttack(pm->ps->saberMove)) { // already in the bounce, go into an attack or transition to ready.. should never get here since
                                                             // can't be blocked in a bounce!
                saberMoveName_t nextMove;

                if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)) { // transition to a new attack
                    if (pm->ps->clientNum && !PM_ControlledByPlayer()) {   // NPC
                        nextMove = saberMoveData[pm->ps->saberMove].chain_attack;
                    }
                    else { // player
                        saberQuadrant_t newQuad = PM_SaberMoveQuadrantForMovement(&pm->cmd);
                        while (
                            newQuad ==
                            saberMoveData[pm->ps->saberMove].startQuad) { // player is still in same attack quad, don't repeat that attack because it looks bad,
                            // FIXME: try to pick one that might look cool?
                            newQuad = Q_irand(saberQuadrant_t::Q_BR, saberQuadrant_t::Q_BL);
                            // FIXME: sanity check, just in case?
                        } // else player is switching up anyway, take the new attack dir
                        nextMove = transitionMove[saberMoveData[pm->ps->saberMove].startQuad][newQuad];
                    }
                }
                else {                                                   // return to ready
                    if (pm->ps->clientNum && !PM_ControlledByPlayer()) { // NPC
                        nextMove = saberMoveData[pm->ps->saberMove].chain_idle;
                    }
                    else { // player
                        if (saberMoveData[pm->ps->saberMove].startQuad == saberQuadrant_t::Q_T) {
                            nextMove = saberMoveName_t::LS_R_BL2TR;
                        }
                        else if (saberMoveData[pm->ps->saberMove].startQuad < saberQuadrant_t::Q_T) {
                            nextMove = saberMoveName_t::LS_R_TL2BR + (saberMoveData[pm->ps->saberMove].startQuad - saberQuadrant_t::Q_BR);
                        }
                        else // if ( saberMoveData[pm->ps->saberMove].startQuad > saberQuadrant_t::Q_T )
                        {
                            nextMove = saberMoveName_t::LS_R_BR2TL + (saberMoveData[pm->ps->saberMove].startQuad - saberQuadrant_t::Q_TL);
                        }
                    }
                }
                PM_SetSaberMove(nextMove);
                pm->ps->weaponTime = pm->ps->torsoAnimTimer;
            }
            else { // start the bounce move
                saberMoveName_t bounceMove;
                if (pm->ps->saberBounceMove != saberMoveName_t::LS_NONE) {
                    bounceMove = pm->ps->saberBounceMove;
                }
                else {
                    bounceMove = PM_SaberBounceForAttack(pm->ps->saberMove);
                }
                PM_SetSaberMove(bounceMove);
                pm->ps->weaponTime = pm->ps->torsoAnimTimer;
            }
            // clear the saberBounceMove
            // pm->ps->saberBounceMove = saberMoveName_t::LS_NONE;

            if (cg_debugSaber.integer >= 2) {
                Q_Println("Saber Block: Bounce");
            }
            break;
        case saberBlockedType_t::BLOCKED_UPPER_RIGHT:
            if (pm->ps->saberBounceMove != saberMoveName_t::LS_NONE) {
                PM_SetSaberMove(pm->ps->saberBounceMove);
                // pm->ps->saberBounceMove = saberMoveName_t::LS_NONE;
                pm->ps->weaponTime = pm->ps->torsoAnimTimer;
            }
            else {
                PM_SetSaberMove(saberMoveName_t::LS_PARRY_UR);
            }

            if (cg_debugSaber.integer >= 2) {
                Q_Println("Saber Block: Parry UR");
            }
            break;
        case saberBlockedType_t::BLOCKED_UPPER_RIGHT_PROJ:
            PM_SetSaberMove(saberMoveName_t::LS_REFLECT_UR);

            if (cg_debugSaber.integer >= 2) {
                Q_Println("Saber Block: Deflect UR");
            }
            break;
        case saberBlockedType_t::BLOCKED_UPPER_LEFT:
            if (pm->ps->saberBounceMove != saberMoveName_t::LS_NONE) {
                PM_SetSaberMove(pm->ps->saberBounceMove);
                // pm->ps->saberBounceMove = saberMoveName_t::LS_NONE;
                pm->ps->weaponTime = pm->ps->torsoAnimTimer;
            }
            else {
                PM_SetSaberMove(saberMoveName_t::LS_PARRY_UL);
            }

            if (cg_debugSaber.integer >= 2) {
                Q_Println("Saber Block: Parry UL");
            }
            break;
        case saberBlockedType_t::BLOCKED_UPPER_LEFT_PROJ:
            PM_SetSaberMove(saberMoveName_t::LS_REFLECT_UL);

            if (cg_debugSaber.integer >= 2) {
                Q_Println("Saber Block: Deflect UL");
            }
            break;
        case saberBlockedType_t::BLOCKED_LOWER_RIGHT:
            if (pm->ps->saberBounceMove != saberMoveName_t::LS_NONE) {
                PM_SetSaberMove(pm->ps->saberBounceMove);
                // pm->ps->saberBounceMove = saberMoveName_t::LS_NONE;
                pm->ps->weaponTime = pm->ps->torsoAnimTimer;
            }
            else {
                PM_SetSaberMove(saberMoveName_t::LS_PARRY_LR);
            }

            if (cg_debugSaber.integer >= 2) {
                Q_Println("Saber Block: Parry LR");
            }
            break;
        case saberBlockedType_t::BLOCKED_LOWER_RIGHT_PROJ:
            PM_SetSaberMove(saberMoveName_t::LS_REFLECT_LR);

            if (cg_debugSaber.integer >= 2) {
                Q_Println("Saber Block: Deflect LR");
            }
            break;
        case saberBlockedType_t::BLOCKED_LOWER_LEFT:
            if (pm->ps->saberBounceMove != saberMoveName_t::LS_NONE) {
                PM_SetSaberMove(pm->ps->saberBounceMove);
                // pm->ps->saberBounceMove = saberMoveName_t::LS_NONE;
                pm->ps->weaponTime = pm->ps->torsoAnimTimer;
            }
            else {
                PM_SetSaberMove(saberMoveName_t::LS_PARRY_LL);
            }

            if (cg_debugSaber.integer >= 2) {
                Q_Println("Saber Block: Parry LL");
            }
            break;
        case saberBlockedType_t::BLOCKED_LOWER_LEFT_PROJ:
            PM_SetSaberMove(saberMoveName_t::LS_REFLECT_LL);

            if (cg_debugSaber.integer >= 2) {
                Q_Println("Saber Block: Deflect LL");
            }
            break;
        case saberBlockedType_t::BLOCKED_TOP:
            if (pm->ps->saberBounceMove != saberMoveName_t::LS_NONE) {
                PM_SetSaberMove(pm->ps->saberBounceMove);
                // pm->ps->saberBounceMove = saberMoveName_t::LS_NONE;
                pm->ps->weaponTime = pm->ps->torsoAnimTimer;
            }
            else {
                PM_SetSaberMove(saberMoveName_t::LS_PARRY_UP);
            }

            if (cg_debugSaber.integer >= 2) {
                Q_Println("Saber Block: Parry Top");
            }
            break;
        case saberBlockedType_t::BLOCKED_TOP_PROJ:
            PM_SetSaberMove(saberMoveName_t::LS_REFLECT_UP);

            if (cg_debugSaber.integer >= 2) {
                Q_Println("Saber Block: Deflect Top");
            }
            break;
        default:
            pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_NONE;
            break;
        }

        // Charging is like a lead-up before attacking again.  This is an appropriate use, or we can create a new weaponstate for blocking
        pm->ps->saberBounceMove = saberMoveName_t::LS_NONE;
        pm->ps->weaponstate = weaponstate_t::WEAPON_READY;

        // Done with block, so stop these active weapon branches.
        return;
    }

    // check for weapon change
    // can't change if weapon is firing, but can change again if lowering or raising
    if (pm->ps->weaponTime <= 0 || pm->ps->weaponstate != weaponstate_t::WEAPON_FIRING) {
        if (pm->ps->weapon != UC_Weapon(pm->cmd)) {
            PM_BeginWeaponChange(UC_Weapon(pm->cmd));
        }
    }

    if (pm->ps->weaponTime > 0) {
        // FIXME: allow some window of opportunity to change your attack
        //		if it just started and your directional input is different
        //		than it was before... but only 100 milliseconds at most?
        // OR:	Make it so that attacks don't start until 100ms after you
        //		press the attack button...???
        return;
    }

    // *********************************************************
    // WEAPON_DROPPING
    // *********************************************************

    // change weapon if time
    if (pm->ps->weaponstate == weaponstate_t::WEAPON_DROPPING) {
        PM_FinishWeaponChange();
        return;
    }

    // *********************************************************
    // WEAPON_RAISING
    // *********************************************************

    if (pm->ps->weaponstate == weaponstate_t::WEAPON_RAISING) { // Just selected the weapon
        pm->ps->weaponstate = weaponstate_t::WEAPON_IDLE;
        if (pm->gent && pm->gent->s.number == 0) {
            if (pm->ps->legsAnim == animNumber_t::BOTH_WALK1) {
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WALK1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            }
            else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN1) {
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            }
            else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN2) {
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            }
            else if (pm->ps->legsAnim == animNumber_t::BOTH_WALK2) {
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WALK2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            }
            else {
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            }
        }
        else {
            bool saberInAir = true;
            if (pm->ps->saberInFlight) { // guiding saber
                if (PM_SaberInBrokenParry(pm->ps->saberMove) || pm->ps->saberBlocked == saberBlockedType_t::BLOCKED_PARRY_BROKEN ||
                    PM_DodgeAnim(pm->ps->torsoAnim)) { // we're stuck in a broken parry
                    saberInAir = false;
                }
                if (pm->ps->saberEntityNum < ENTITYNUM_NONE && pm->ps->saberEntityNum > 0)            // player is 0
                {                                                                                     //
                    if (g_entities[pm->ps->saberEntityNum].s.pos.trType == trType_t::TR_STATIONARY) { // fell to the ground and we're not trying to pull it back
                        saberInAir = false;
                    }
                }
            }
            if (pm->ps->weapon == weapon_t::WP_SABER && pm->ps->saberInFlight && saberInAir) { // guiding saber
                if (!PM_ForceAnim(pm->ps->torsoAnim) || pm->ps->torsoAnimTimer < 300) {        // don't interrupt a force power anim
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_SABERPULL,
                               setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                }
            }
            else {
                // PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_ATTACK1, setAnimFlags_t::SETANIM_FLAG_NORMAL);//TORSO_WEAPONIDLE1
                // Select the proper idle Lightsaber attack move from the chart.
                PM_SetSaberMove(saberMoveName_t::LS_READY);
            }
        }
        return;
    }

    // *********************************************************
    // Check for WEAPON ATTACK
    // *********************************************************

    if (!delayed_fire) {
        // Start with the current move, and cross index it with the current control states.
        if (pm->ps->saberMove > saberMoveName_t::LS_NONE && pm->ps->saberMove < saberMoveName_t::LS_MOVE_MAX) {
            curmove = pm->ps->saberMove;
        }
        else {
            curmove = saberMoveName_t::LS_READY;
        }
        if (curmove == saberMoveName_t::LS_A_JUMP_T__B_ ||
            pm->ps->torsoAnim == animNumber_t::BOTH_FORCELEAP2_T__B_) { // must transition back to ready from this anim
            newmove = saberMoveName_t::LS_R_T2B;
        }
        // check for fire
        else if (!Any(pm->cmd.buttons & (buttonFlags_t::BUTTON_ATTACK | buttonFlags_t::BUTTON_ALT_ATTACK))) { // not attacking
            pm->ps->weaponTime = 0;

            if (pm->gent && pm->gent->client && pm->gent->client->fireDelay > 0) { // Still firing
                pm->ps->weaponstate = weaponstate_t::WEAPON_FIRING;
            }
            else if (pm->ps->weaponstate != weaponstate_t::WEAPON_READY) {
                pm->ps->weaponstate = weaponstate_t::WEAPON_IDLE;
            }
            // Check for finishing an anim if necc.
            if (curmove >= saberMoveName_t::LS_S_TL2BR && curmove <= saberMoveName_t::LS_S_T2B) { // started a swing, must continue from here
                newmove = saberMoveName_t::LS_A_TL2BR + (curmove - saberMoveName_t::LS_S_TL2BR);
            }
            else if (curmove >= saberMoveName_t::LS_A_TL2BR && curmove <= saberMoveName_t::LS_A_T2B) { // finished an attack, must continue from here
                newmove = saberMoveName_t::LS_R_TL2BR + (curmove - saberMoveName_t::LS_A_TL2BR);
            }
            else if (PM_SaberInTransition(curmove)) { // in a transition, must play sequential attack
                newmove = saberMoveData[curmove].chain_attack;
            }
            else if (PM_SaberInBounce(curmove)) {                    // in a bounce
                if (pm->ps->clientNum && !PM_ControlledByPlayer()) { // NPCs must play sequential attack
                    // going into another attack...
                    // allow endless chaining in level 1 attacks, several in level 2 and only one or a few in level 3
                    if (PM_SaberKataDone(saberMoveName_t::LS_NONE, saberMoveName_t::LS_NONE)) { // done with this kata, must return to ready before attack again
                        newmove = saberMoveData[curmove].chain_idle;
                    }
                    else {                                             // okay to chain to another attack
                        newmove = saberMoveData[curmove].chain_attack; // we assume they're attacking, even if they're not
                        pm->ps->saberAttackChainCount++;
                    }
                }
                else {                                           // player gets his by directional control
                    newmove = saberMoveData[curmove].chain_idle; // oops, not attacking, so don't chain
                }
            }
            else { // FIXME: what about returning from a parry?
                // PM_SetSaberMove( saberMoveName_t::LS_READY );
                return;
            }
        }

        // ***************************************************
        // Pressing attack, so we must look up the proper attack move.
        bool saberInAir = true;
        if (pm->ps->saberInFlight) { // guiding saber
            if (PM_SaberInBrokenParry(pm->ps->saberMove) || pm->ps->saberBlocked == saberBlockedType_t::BLOCKED_PARRY_BROKEN ||
                PM_DodgeAnim(pm->ps->torsoAnim)) { // we're stuck in a broken parry
                saberInAir = false;
            }
            if (pm->ps->saberEntityNum < ENTITYNUM_NONE && pm->ps->saberEntityNum > 0)            // player is 0
            {                                                                                     //
                if (g_entities[pm->ps->saberEntityNum].s.pos.trType == trType_t::TR_STATIONARY) { // fell to the ground and we're not trying to pull it back
                    saberInAir = false;
                }
            }
        }

        if (pm->ps->weapon == weapon_t::WP_SABER && pm->ps->saberInFlight && saberInAir) { // guiding saber
            if (!PM_ForceAnim(pm->ps->torsoAnim) || pm->ps->torsoAnimTimer < 300) {        // don't interrupt a force power anim
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_SABERPULL,
                           setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
            }
        }
        else if (pm->ps->weaponTime > 0) { // Last attack is not yet complete.
            pm->ps->weaponstate = weaponstate_t::WEAPON_FIRING;
            return;
        }
        else {
            int both = false;

            if (curmove >= saberMoveName_t::LS_PARRY_UP &&
                curmove <= saberMoveName_t::LS_REFLECT_LL) {         // from a parry or reflection, can go directly into an attack
                if (pm->ps->clientNum && !PM_ControlledByPlayer()) { // NPCs
                    newmove = PM_NPCSaberAttackFromQuad(saberMoveData[curmove].endQuad);
                }
                else {
                    switch (saberMoveData[curmove].endQuad) {
                    case saberQuadrant_t::Q_T:
                        newmove = saberMoveName_t::LS_A_T2B;
                        break;
                    case saberQuadrant_t::Q_TR:
                        newmove = saberMoveName_t::LS_A_TR2BL;
                        break;
                    case saberQuadrant_t::Q_TL:
                        newmove = saberMoveName_t::LS_A_TL2BR;
                        break;
                    case saberQuadrant_t::Q_BR:
                        newmove = saberMoveName_t::LS_A_BR2TL;
                        break;
                    case saberQuadrant_t::Q_BL:
                        newmove = saberMoveName_t::LS_A_BL2TR;
                        break;
                    // shouldn't be a parry that ends at L, R or B
                    default:
                        break;
                    }
                }
            }

            if (newmove != saberMoveName_t::LS_NONE) { // have a valid, final LS_ move picked, so skip findingt he transition move and just get the anim
                if (PM_HasAnimation(pm->gent, saberMoveData[newmove].animToUse)) {
                    anim = saberMoveData[newmove].animToUse;
                }
            }

            // FIXME: diagonal dirs use the figure-eight attacks from ready pose?
            if (!anim) {
                // FIXME: take FP_SABER_OFFENSE into account here somehow?
                if (PM_SaberInTransition(curmove)) { // in a transition, must play sequential attack
                    newmove = saberMoveData[curmove].chain_attack;
                }
                else if (curmove >= saberMoveName_t::LS_S_TL2BR && curmove <= saberMoveName_t::LS_S_T2B) { // started a swing, must continue from here
                    newmove = saberMoveName_t::LS_A_TL2BR + (curmove - saberMoveName_t::LS_S_TL2BR);
                }
                else if (PM_SaberInBrokenParry(curmove)) { // broken parries must always return to ready
                    newmove = saberMoveName_t::LS_READY;
                }
                else // if ( Any( pm->cmd.buttons&buttonFlags_t::BUTTON_ATTACK ) && !Any(pm->ps->pm_flags&pmoveFlags_t::PMF_ATTACK_HELD) )//only do this if just
                     // pressed attack button?
                {    // get attack move from movement command
                    /*
                    if ( PM_SaberKataDone() )
                    {//we came from a bounce and cannot chain to another attack because our kata is done
                        newmove = saberMoveData[curmove].chain_idle;
                    }
                    else */
                    if (pm->ps->clientNum && !PM_ControlledByPlayer() &&
                        (Q_irand(0, to_underlying(pm->ps->saberAnimLevel) - 1) ||
                         (pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_1 && pm->gent && pm->gent->NPC &&
                          pm->gent->NPC->rank >= rank_t::RANK_LT_JG && Q_irand(0, 1)))) // minor change to make fast-attack users use the special attacks more
                    {                                                                   // NPCs use more randomized attacks the more skilled they are
                        newmove = PM_NPCSaberAttackFromQuad(saberMoveData[curmove].endQuad);
                    }
                    else {
                        newmove = PM_SaberAttackForMovement(pm->cmd.forwardmove, pm->cmd.rightmove, curmove);
                        if ((PM_SaberInBounce(curmove) || PM_SaberInBrokenParry(curmove)) &&
                            saberMoveData[newmove].startQuad ==
                                saberMoveData[curmove].endQuad) { // this attack would be a repeat of the last (which was blocked), so don't actually use it,
                                                                  // use the default chain attack for this bounce
                            newmove = saberMoveData[curmove].chain_attack;
                        }
                    }
                    if (PM_SaberKataDone(curmove, newmove)) { // cannot chain this time
                        newmove = saberMoveData[curmove].chain_idle;
                    }
                }
                /*
                if ( newmove == saberMoveName_t::LS_NONE )
                {//FIXME: should we allow this?  Are there some anims that you should never be able to chain into an attack?
                    //only curmove that might get in here is LS_NONE, LS_DRAW, LS_PUTAWAY and the LS_R_ returns... all of which are in Q_R
                    newmove = PM_AttackMoveForQuad( saberMoveData[curmove].endQuad );
                }
                */
                if (newmove != saberMoveName_t::LS_NONE) {
                    // Now get the proper transition move
                    newmove = PM_SaberAnimTransitionAnim(curmove, newmove);
                    if (PM_HasAnimation(pm->gent, saberMoveData[newmove].animToUse)) {
                        anim = saberMoveData[newmove].animToUse;
                    }
                }
            }

            if (!anim) { // not side-stepping, pick neutral anim
                // Add randomness for prototype?
                newmove = saberMoveData[curmove].chain_attack;
                if (PM_HasAnimation(pm->gent, saberMoveData[newmove].animToUse)) {
                    anim = saberMoveData[newmove].animToUse;
                }

                if (!pm->cmd.forwardmove && !pm->cmd.rightmove && pm->cmd.upmove >= 0 &&
                    pm->ps->groundEntityNum != ENTITYNUM_NONE) { // not moving at all, so set the anim on entire body
                    both = true;
                }
            }

            if (!anim) {
                if (pm->ps->legsAnim == animNumber_t::BOTH_WALK1) {
                    anim = animNumber_t::BOTH_WALK1;
                }
                else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN2) {
                    anim = animNumber_t::BOTH_RUN2;
                }
                else if (pm->ps->legsAnim == animNumber_t::BOTH_WALK2) {
                    anim = animNumber_t::BOTH_WALK2;
                }
                else {
                    // FIXME: play both_stand2_random1 when you've been idle for a while
                    anim = animNumber_t::BOTH_STAND2;
                }
                newmove = saberMoveName_t::LS_READY;
            }

            if (!pm->ps->saberActive) { // turn on the saber if it's not on
                pm->ps->saberActive = true;
            }

            PM_SetSaberMove(newmove);

            if (both) {
                PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, *anim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
            }
            if (pm->gent && pm->gent->client) {
                //				pm->gent->client->saberTrail.inAction = true;
                //				pm->gent->client->saberTrail.duration = 75; // saber trail lasts for 75ms...feel free to change this if you want it longer or
                // shorter
            }
            // don't fire again until anim is done
            pm->ps->weaponTime = pm->ps->torsoAnimTimer;
            /*
            //FIXME: this may be making it so sometimes you can't swing again right away...
            if ( newmove == saberMoveName_t::LS_READY )
            {
                pm->ps->weaponTime = 500;
            }
            */
        }
    }

    // *********************************************************
    // WEAPON_FIRING
    // *********************************************************

    pm->ps->weaponstate = weaponstate_t::WEAPON_FIRING;

    if (pm->gent && pm->gent->client && pm->gent->client->fireDelay > 0) { // FIXME: this is going to fire off one frame before you expect, actually
        // Clear these out since we're not actually firing yet
        pm->ps->eFlags &= ~entityFlags_t::EF_FIRING;
        pm->ps->eFlags &= ~entityFlags_t::EF_ALT_FIRING;
        return;
    }

    addTime = pm->ps->weaponTime;
    if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {
        PM_AddEvent(entity_event_t::EV_ALT_FIRE);
        if (!addTime) {
            addTime = weaponData[pm->ps->weapon].altFireTime;
            if (g_timescale != nullptr) {
                if (g_timescale->value < 1.0f) {
                    if (!MatrixMode) { // Special test for Matrix Mode (tm)
                        if (pm->ps->clientNum == 0 && !player_locked &&
                            pm->ps->forcePowersActive & Bit(forcePowers_t::FP_SPEED)) { // player always fires at normal speed
                            addTime *= g_timescale->value;
                        }
                        else if (g_entities[pm->ps->clientNum].client && pm->ps->forcePowersActive & Bit(forcePowers_t::FP_SPEED)) {
                            addTime *= g_timescale->value;
                        }
                    }
                }
            }
        }
    }
    else {
        PM_AddEvent(entity_event_t::EV_FIRE_WEAPON);
        if (!addTime) {
            addTime = weaponData[pm->ps->weapon].fireTime;
            if (g_timescale != nullptr) {
                if (g_timescale->value < 1.0f) {
                    if (!MatrixMode) { // Special test for Matrix Mode (tm)
                        if (pm->ps->clientNum == 0 && !player_locked &&
                            pm->ps->forcePowersActive & Bit(forcePowers_t::FP_SPEED)) { // player always fires at normal speed
                            addTime *= g_timescale->value;
                        }
                        else if (g_entities[pm->ps->clientNum].client && pm->ps->forcePowersActive & Bit(forcePowers_t::FP_SPEED)) {
                            addTime *= g_timescale->value;
                        }
                    }
                }
            }
        }
    }

    // If the phaser has been fired, delay the next recharge time
    if (pm->gent && pm->gent->NPC != nullptr) { // NPCs have their own refire logic
        return;
    }

    pm->ps->weaponTime = addTime;
}

//---------------------------------------
static bool PM_DoChargedWeapons(void)
//---------------------------------------
{
    bool charging = false, altFire = false;

    // FIXME: make jedi aware they're being aimed at with a charged-up weapon (strafe and be evasive?)
    //  If you want your weapon to be a charging weapon, just set this bit up
    switch (pm->ps->weapon) {
    //------------------
    case weapon_t::WP_BRYAR_PISTOL:

        // alt-fire charges the weapon
        if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {
            charging = true;
            altFire = true;
        }
        break;

    //------------------
    case weapon_t::WP_DISRUPTOR:

        // alt-fire charges the weapon...but due to zooming being controlled by the alt-button, the main button actually charges...but only when zoomed.
        //	lovely, eh?
        if (!pm->ps->clientNum) {
            if (cg.zoomMode == 2) {
                if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)) {
                    charging = true;
                    altFire = true; // believe it or not, it really is an alt-fire in this case!
                }
            }
        }
        else if (pm->gent && pm->gent->NPC) {
            if (Any(pm->gent->NPC->scriptFlags & scriptFlags_t::SCF_ALT_FIRE)) {
                if (pm->gent->fly_sound_debounce_time > level.time) {
                    charging = true;
                    altFire = true;
                }
            }
        }
        break;

    //------------------
    case weapon_t::WP_BOWCASTER:

        // main-fire charges the weapon
        if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)) {
            charging = true;
        }
        break;

    //------------------
    case weapon_t::WP_DEMP2:

        // alt-fire charges the weapon
        if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {
            charging = true;
            altFire = true;
        }
        break;

    //------------------
    case weapon_t::WP_ROCKET_LAUNCHER:

        // Not really a charge weapon, but we still want to delay fire until the button comes up so that we can
        //	implement our alt-fire locking stuff
        if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {
            charging = true;
            altFire = true;
        }
        break;

    //------------------
    case weapon_t::WP_THERMAL:
        //			FIXME: Really should have a wind-up anim for player
        //			as he holds down the fire button to throw, then play
        //			the actual throw when he lets go...
        if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {
            altFire = true; // override default of not being an alt-fire
            charging = true;
        }
        else if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)) {
            charging = true;
        }
        break;

    default:
        break;
    } // end switch

    // set up the appropriate weapon state based on the button that's down.
    //	Note that we ALWAYS return if charging is set ( meaning the buttons are still down )
    if (charging) {
        if (altFire) {
            if (pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING_ALT && pm->ps->weaponstate != weaponstate_t::WEAPON_DROPPING) {
                if (pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] <= 0) {
                    PM_AddEvent(entity_event_t::EV_NOAMMO);
                    pm->ps->weaponTime += 500;
                    return true;
                }

                // charge isn't started, so do it now
                pm->ps->weaponstate = weaponstate_t::WEAPON_CHARGING_ALT;
                pm->ps->weaponChargeTime = level.time;

                if (cg_weapons[pm->ps->weapon].altChargeSound) {
                    G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_WEAPON, weaponData[pm->ps->weapon].altChargeSnd);
                }
            }
        }
        else {
            if (pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING && pm->ps->weaponstate != weaponstate_t::WEAPON_DROPPING) {
                if (pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] <= 0) {
                    PM_AddEvent(entity_event_t::EV_NOAMMO);
                    pm->ps->weaponTime += 500;
                    return true;
                }

                // charge isn't started, so do it now
                pm->ps->weaponstate = weaponstate_t::WEAPON_CHARGING;
                pm->ps->weaponChargeTime = level.time;

                if (cg_weapons[pm->ps->weapon].chargeSound && pm->gent && !pm->gent->NPC) // HACK: !NPC mostly for bowcaster and weequay
                {
                    G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_WEAPON, weaponData[pm->ps->weapon].chargeSnd);
                }
            }
        }

        return true; // short-circuit rest of weapon code
    }

    // Only charging weapons should be able to set these states...so....
    //	let's see which fire mode we need to set up now that the buttons are up
    if (pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING) {
        // weapon has a charge, so let us do an attack
        // dumb, but since we shoot a charged weapon on button-up, we need to repress this button for now
        pm->cmd.buttons |= buttonFlags_t::BUTTON_ATTACK;
        pm->ps->eFlags |= entityFlags_t::EF_FIRING;
    }
    else if (pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING_ALT) {
        // weapon has a charge, so let us do an alt-attack
        // dumb, but since we shoot a charged weapon on button-up, we need to repress this button for now
        pm->cmd.buttons |= buttonFlags_t::BUTTON_ALT_ATTACK;
        pm->ps->eFlags |= (entityFlags_t::EF_FIRING | entityFlags_t::EF_ALT_FIRING);
    }

    return false; // continue with the rest of the weapon code
}

// Specific weapons can opt to modify the ammo usage based on charges, otherwise if no special case code
//	is handled below, regular ammo usage will happen
//---------------------------------------
static int PM_DoChargingAmmoUsage(int* amount)
//---------------------------------------
{
    int count = 0;

    if (pm->ps->weapon == weapon_t::WP_BOWCASTER && !Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {
        // this code is duplicated ( I know, I know ) in G_weapon.cpp for the bowcaster alt-fire
        count = (level.time - pm->ps->weaponChargeTime) / BOWCASTER_CHARGE_UNIT;

        if (count < 1) {
            count = 1;
        }
        else if (count > 5) {
            count = 5;
        }

        if (!(count & 1)) {
            // if we aren't odd, knock us down a level
            count--;
        }

        // Only bother with these checks if we don't have infinite ammo
        if (pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] != -1) {
            int dif = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] - *amount * count;

            // If we have enough ammo to do the full charged shot, we are ok
            if (dif < 0) {
                // we are not ok, so hack our chargetime and ammo usage, note that DIF is going to be negative
                count += Q_floorf(dif / (float)*amount);

                if (count < 1) {
                    count = 1;
                }

                // now get a real chargeTime so the duplicated code in g_weapon doesn't get freaked
                pm->ps->weaponChargeTime = level.time - (count * BOWCASTER_CHARGE_UNIT);
            }
        }

        // now that count is cool, get the real ammo usage
        *amount *= count;
    }
    else if (pm->ps->weapon == weapon_t::WP_BRYAR_PISTOL && Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {
        // this code is duplicated ( I know, I know ) in G_weapon.cpp for the bryar alt-fire
        count = (level.time - pm->ps->weaponChargeTime) / BRYAR_CHARGE_UNIT;

        if (count < 1) {
            count = 1;
        }
        else if (count > 5) {
            count = 5;
        }

        // Only bother with these checks if we don't have infinite ammo
        if (pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] != -1) {
            int dif = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] - *amount * count;

            // If we have enough ammo to do the full charged shot, we are ok
            if (dif < 0) {
                // we are not ok, so hack our chargetime and ammo usage, note that DIF is going to be negative
                count += Q_floorf(dif / (float)*amount);

                if (count < 1) {
                    count = 1;
                }

                // now get a real chargeTime so the duplicated code in g_weapon doesn't get freaked
                pm->ps->weaponChargeTime = level.time - (count * BRYAR_CHARGE_UNIT);
            }
        }

        // now that count is cool, get the real ammo usage
        *amount *= count;
    }
    else if (pm->ps->weapon == weapon_t::WP_DEMP2 && Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {
        // this code is duplicated ( I know, I know ) in G_weapon.cpp for the demp2 alt-fire
        count = (level.time - pm->ps->weaponChargeTime) / DEMP2_CHARGE_UNIT;

        if (count < 1) {
            count = 1;
        }
        else if (count > 3) {
            count = 3;
        }

        // Only bother with these checks if we don't have infinite ammo
        if (pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] != -1) {
            int dif = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] - *amount * count;

            // If we have enough ammo to do the full charged shot, we are ok
            if (dif < 0) {
                // we are not ok, so hack our chargetime and ammo usage, note that DIF is going to be negative
                count += Q_floorf(dif / (float)*amount);

                if (count < 1) {
                    count = 1;
                }

                // now get a real chargeTime so the duplicated code in g_weapon doesn't get freaked
                pm->ps->weaponChargeTime = level.time - (count * DEMP2_CHARGE_UNIT);
            }
        }

        // now that count is cool, get the real ammo usage
        *amount *= count;

        // this is an after-thought.  should probably re-write the function to do this naturally.
        if (*amount > pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex]) {
            *amount = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex];
        }
    }
    else if (pm->ps->weapon == weapon_t::WP_DISRUPTOR &&
             Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) // BUTTON_ATTACK will have been mapped to BUTTON_ALT_ATTACK if we are zoomed
    {
        // this code is duplicated ( I know, I know ) in G_weapon.cpp for the disruptor alt-fire
        count = (level.time - pm->ps->weaponChargeTime) / DISRUPTOR_CHARGE_UNIT;

        if (count < 1) {
            count = 1;
        }
        else if (count > 10) {
            count = 10;
        }

        // Only bother with these checks if we don't have infinite ammo
        if (pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] != -1) {
            int dif = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] - *amount * count;

            // If we have enough ammo to do the full charged shot, we are ok
            if (dif < 0) {
                // we are not ok, so hack our chargetime and ammo usage, note that DIF is going to be negative
                count += Q_floorf(dif / (float)*amount);

                if (count < 1) {
                    count = 1;
                }

                // now get a real chargeTime so the duplicated code in g_weapon doesn't get freaked
                pm->ps->weaponChargeTime = level.time - (count * DISRUPTOR_CHARGE_UNIT);
            }
        }

        // now that count is cool, get the real ammo usage
        *amount *= count;

        // this is an after-thought.  should probably re-write the function to do this naturally.
        if (*amount > pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex]) {
            *amount = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex];
        }
    }

    return count;
}

bool PM_DroidMelee(class_t npc_class)
{
    if (npc_class == class_t::CLASS_PROBE || npc_class == class_t::CLASS_SEEKER || npc_class == class_t::CLASS_INTERROGATOR ||
        npc_class == class_t::CLASS_SENTRY || npc_class == class_t::CLASS_REMOTE) {
        return true;
    }
    return false;
}

/*
==============
PM_Weapon

Generates weapon events and modifes the weapon counter
==============
*/
static void PM_Weapon(void)
{
    int addTime, amount, trueCount = 1;
    bool delayed_fire = false;

    if (pm->ps->weapon == weapon_t::WP_SABER && (cg.zoomMode == 3 || !cg.zoomMode || pm->ps->clientNum)) // WP_LIGHTSABER
    { // Separate logic for lightsaber, but not for player when zoomed
        PM_WeaponLightsaber();
        if (pm->gent && pm->gent->client && pm->ps->saberActive && pm->ps->saberInFlight) {   // FIXME: put saberTrail in playerState
            if (pm->gent->client->ps.saberEntityState == saberEntityState_t::SES_RETURNING) { // turn off the saber trail
                pm->gent->client->saberTrail.inAction = false;
                pm->gent->client->saberTrail.duration = 75;
            }
            else { // turn on the saber trail
                pm->gent->client->saberTrail.inAction = true;
                pm->gent->client->saberTrail.duration = 150;
            }
        }
        return;
    }

    if (PM_InKnockDown(pm->ps) || PM_InRoll(pm->ps)) { // in knockdown
        if (pm->ps->weaponTime > 0) {
            pm->ps->weaponTime -= pml.msec;
            if (pm->ps->weaponTime <= 0) {
                pm->ps->weaponTime = 0;
            }
        }
        return;
    }

    if (pm->gent && pm->gent->client && pm->gent->client->fireDelay > 0) { // FIXME: this is going to fire off one frame before you expect, actually
        pm->gent->client->fireDelay -= pml.msec;
        if (pm->gent->client->fireDelay <= 0) { // just finished delay timer
            if (pm->ps->clientNum && pm->ps->weapon == weapon_t::WP_ROCKET_LAUNCHER) {
                G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_WEAPON, "sound/weapons/rocket/lock.wav");
                pm->cmd.buttons |= buttonFlags_t::BUTTON_ALT_ATTACK;
            }
            pm->gent->client->fireDelay = 0;
            delayed_fire = true;
        }
        else {
            if (pm->ps->clientNum && pm->ps->weapon == weapon_t::WP_ROCKET_LAUNCHER && Q_irand(0, 1)) {
                G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_WEAPON, "sound/weapons/rocket/tick.wav");
            }
        }
    }

    // don't allow attack until all buttons are up
    if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_RESPAWNED)) {
        return;
    }

    // check for dead player
    if (pm->ps->stats[statIndex_t::STAT_HEALTH] <= 0) {
        if (pm->gent && pm->gent->client) {
            pm->ps->weapon = weapon_t::WP_NONE;
        }

        if (pm->gent) {
            pm->gent->s.loopSound = 0;
        }
        return;
    }

    // make weapon function
    if (pm->ps->weaponTime > 0) {
        pm->ps->weaponTime -= pml.msec;
    }

    // check for weapon change
    // can't change if weapon is firing, but can change again if lowering or raising
    if ((pm->ps->weaponTime <= 0 || pm->ps->weaponstate != weaponstate_t::WEAPON_FIRING) && pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING_ALT &&
        pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING) {
        // eez- don't switch weapons if we're charging our current one up
        if (pm->ps->weapon != UC_Weapon(pm->cmd) && (!pm->ps->viewEntity || pm->ps->viewEntity >= ENTITYNUM_WORLD) && !PM_DoChargedWeapons()) {
            PM_BeginWeaponChange(UC_Weapon(pm->cmd));
        }
    }

    if (pm->ps->weaponTime > 0) {
        return;
    }

    // change weapon if time
    if (pm->ps->weaponstate == weaponstate_t::WEAPON_DROPPING) {
        PM_FinishWeaponChange();
        return;
    }

    if (pm->ps->weapon == weapon_t::WP_NONE) {
        return;
    }

    if (PM_DoChargedWeapons()) {
        return;
    }

    if (pm->ps->weaponstate == weaponstate_t::WEAPON_RAISING) {
        // Just selected the weapon
        pm->ps->weaponstate = weaponstate_t::WEAPON_IDLE;

        if (pm->gent && pm->gent->s.number == 0) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else {
            switch (pm->ps->weapon) {
            case weapon_t::WP_BRYAR_PISTOL:
            case weapon_t::WP_BLASTER_PISTOL:
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONIDLE2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                break;
            default:
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONIDLE3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                break;
            }
        }
        return;
    }

    if (!delayed_fire) {
        // check for fire
        if (!Any(pm->cmd.buttons & (buttonFlags_t::BUTTON_ATTACK | buttonFlags_t::BUTTON_ALT_ATTACK))) {
            pm->ps->weaponTime = 0;

            if (pm->gent && pm->gent->client && pm->gent->client->fireDelay > 0) { // Still firing
                pm->ps->weaponstate = weaponstate_t::WEAPON_FIRING;
            }
            else if (pm->ps->weaponstate != weaponstate_t::WEAPON_READY) {
                if (!pm->gent || !pm->gent->NPC || pm->gent->attackDebounceTime < level.time) {
                    pm->ps->weaponstate = weaponstate_t::WEAPON_IDLE;
                }
            }

            return;
        }

        // start the animation even if out of ammo
        switch (pm->ps->weapon) {
            /*
                    case weapon_t::WP_SABER://1 - handed
                        PM_SetAnim(pm,setAnimParts_t::SETANIM_TORSO,animNumber_t::BOTH_ATTACK1,setAnimFlags_t::SETANIM_FLAG_OVERRIDE|setAnimFlags_t::SETANIM_FLAG_RESTART|setAnimFlags_t::SETANIM_FLAG_HOLD);
                        break;
            */
        case weapon_t::WP_BRYAR_PISTOL:   // 1-handed
        case weapon_t::WP_BLASTER_PISTOL: // 1-handed
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_ATTACK2,
                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_RESTART | setAnimFlags_t::SETANIM_FLAG_HOLD);
            break;

        case weapon_t::WP_MELEE:

            // since there's no RACE_BOTS, I listed all the droids that have might have melee attacks - dmv
            if (pm->gent && pm->gent->client) {
                if (PM_DroidMelee(pm->gent->client->NPC_class)) {
                    if (Q_irand(0, 1))
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_MELEE1,
                                   setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                    else
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_MELEE2,
                                   setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                }
                else {
                    animNumber_t anim;
                    if (!pm->ps->clientNum) {
                        if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {
                            anim = animNumber_t::BOTH_MELEE2;
                        }
                        else {
                            anim = animNumber_t::BOTH_MELEE1;
                        }
                    }
                    else {
                        anim = PM_PickAnim(pm->gent, animNumber_t::BOTH_MELEE1, animNumber_t::BOTH_MELEE2);
                    }
                    if (VectorCompare(pm->ps->velocity, vec3_origin) && pm->cmd.upmove >= 0) {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, anim,
                                   setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_RESTART);
                    }
                    else {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, anim,
                                   setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_RESTART);
                    }
                }
            }
            break;

        case weapon_t::WP_BLASTER:
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_ATTACK3,
                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_RESTART);
            break;

        case weapon_t::WP_DISRUPTOR:
            if ((pm->ps->clientNum && pm->gent && pm->gent->NPC && Any(pm->gent->NPC->scriptFlags & scriptFlags_t::SCF_ALT_FIRE)) ||
                (!pm->ps->clientNum && cg.zoomMode == 2)) { // NPC or player in alt-fire, sniper mode
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_ATTACK4,
                           setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
            }
            else { // in primary fire mode
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_ATTACK3,
                           setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_RESTART);
            }
            break;

        case weapon_t::WP_BOT_LASER:
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_ATTACK1,
                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_RESTART | setAnimFlags_t::SETANIM_FLAG_HOLD);
            break;

        case weapon_t::WP_THERMAL:
            if (pm->ps->clientNum) {
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_ATTACK10,
                           setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_RESTART | setAnimFlags_t::SETANIM_FLAG_HOLD);
            }
            else {
                if (cg.renderingThirdPerson) {
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_THERMAL_THROW,
                               setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_RESTART | setAnimFlags_t::SETANIM_FLAG_HOLD);
                }
                else {
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_ATTACK2,
                               setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_RESTART | setAnimFlags_t::SETANIM_FLAG_HOLD);
                }
            }
            break;

        case weapon_t::WP_EMPLACED_GUN:
            // Guess we don't play an attack animation?  Maybe we should have a custom one??
            break;

        case weapon_t::WP_NONE:
            // no anim
            break;

        case weapon_t::WP_REPEATER:
            if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_GALAKMECH) { //
                if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_ATTACK3,
                               setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_RESTART | setAnimFlags_t::SETANIM_FLAG_HOLD);
                }
                else {
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_ATTACK1,
                               setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_RESTART | setAnimFlags_t::SETANIM_FLAG_HOLD);
                }
            }
            else {
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_ATTACK3,
                           setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_RESTART | setAnimFlags_t::SETANIM_FLAG_HOLD);
            }
            break;

        default: // 2-handed heavy weapon
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_ATTACK3,
                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_RESTART | setAnimFlags_t::SETANIM_FLAG_HOLD);
            break;
        }
    }

    if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {
        amount = weaponData[pm->ps->weapon].altEnergyPerShot;
    }
    else {
        amount = weaponData[pm->ps->weapon].energyPerShot;
    }

    if ((pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING) || (pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING_ALT)) {
        // charging weapons may want to do their own ammo logic.
        trueCount = PM_DoChargingAmmoUsage(&amount);
    }

    pm->ps->weaponstate = weaponstate_t::WEAPON_FIRING;

    // take an ammo away if not infinite
    if (pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] != -1) {
        // enough energy to fire this weapon?
        if ((pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] - amount) >= 0) {
            pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] -= amount;
        }
        else // Not enough energy
        {
            if (!Any(pm->ps->eFlags & entityFlags_t::EF_LOCKED_TO_WEAPON)) {
                // Switch weapons
                PM_AddEvent(entity_event_t::EV_NOAMMO);
                pm->ps->weaponTime += 500;
            }
            return;
        }
    }

    if (pm->gent && pm->gent->client && pm->gent->client->fireDelay > 0) { // FIXME: this is going to fire off one frame before you expect, actually
        // Clear these out since we're not actually firing yet
        pm->ps->eFlags &= ~entityFlags_t::EF_FIRING;
        pm->ps->eFlags &= ~entityFlags_t::EF_ALT_FIRING;
        return;
    }

    if (pm->ps->weapon == weapon_t::WP_MELEE) {
        PM_AddEvent(entity_event_t::EV_FIRE_WEAPON);
        addTime = pm->ps->torsoAnimTimer;
    }
    else if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {
        PM_AddEvent(entity_event_t::EV_ALT_FIRE);
        addTime = weaponData[pm->ps->weapon].altFireTime;
    }
    else {
        if (pm->ps->clientNum                                    // NPC
            && !PM_ControlledByPlayer()                          // not under player control
            && pm->ps->weapon == weapon_t::WP_THERMAL            // using thermals
            && pm->ps->torsoAnim != animNumber_t::BOTH_ATTACK10) // not in the throw anim
        {                                                        // oops, got knocked out of the anim, don't throw the thermal
            return;
        }
        PM_AddEvent(entity_event_t::EV_FIRE_WEAPON);
        addTime = weaponData[pm->ps->weapon].fireTime;

        switch (pm->ps->weapon) {
        case weapon_t::WP_REPEATER:
            // repeater is supposed to do smoke after sustained bursts
            pm->ps->weaponShotCount++;
            break;
        case weapon_t::WP_BOWCASTER:
            addTime *= ((trueCount < 3) ? 0.35f : 1.0f); // if you only did a small charge shot with the bowcaster, use less time between shots
            break;
        default:
            break;
        }
    }

    if (pm->gent && pm->gent->NPC != nullptr) { // NPCs have their own refire logic
        // eez: Unless they're controlled by the player!
        if (!PM_ControlledByPlayer())
            return;
    }

    if (g_timescale != nullptr) {
        if (g_timescale->value < 1.0f) {
            if (!MatrixMode) { // Special test for Matrix Mode (tm)
                if (pm->ps->clientNum == 0 && !player_locked &&
                    pm->ps->forcePowersActive & Bit(forcePowers_t::FP_SPEED)) { // player always fires at normal speed
                    addTime *= g_timescale->value;
                }
                else if (g_entities[pm->ps->clientNum].client && pm->ps->forcePowersActive & Bit(forcePowers_t::FP_SPEED)) {
                    addTime *= g_timescale->value;
                }
            }
        }
    }

    pm->ps->weaponTime += addTime;
    pm->ps->lastShotTime = level.time; // so we know when the last time we fired our gun is

    // HACK!!!!!
    if (pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] <= 0) {
        if (pm->ps->weapon == weapon_t::WP_THERMAL || pm->ps->weapon == weapon_t::WP_TRIP_MINE) {
            // because these weapons have the ammo attached to the hand, we should switch weapons when the last one is thrown, otherwise it will look silly
            //	NOTE: could also switch to an empty had version, but was told we aren't getting any new models at this point
            CG_OutOfAmmoChange();
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_DROPWEAP1 + 2,
                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD); // hack weapon down!
            pm->ps->weaponTime = 50;
        }
    }
}

/*
================
PM_Inventory
================
*/
static void PM_Inventory(void)
{
    // check for item using
    /*
        if ( Any( pm->cmd.buttons & BUTTON_USE_FORCEPOWER ) )
        {
            if ( ! Any( pm->ps->pm_flags & BUTTON_USE_FORCEPOWER ) )
            {
                pm->ps->pm_flags |= BUTTON_USE_FORCEPOWER;
                PM_AddEvent( EV_USE_FORCEPOWER);
                return;
            }
        }
        else
        {
            pm->ps->pm_flags &= ~PMF_USE_ITEM_HELD;
        }
    */
}

extern void ForceThrow(gentity_t* self, bool pull);
extern void ForceHeal(gentity_t* self);
extern void ForceTelepathy(gentity_t* self);
void PM_CheckForceUseButton(gentity_t* ent, usercmd_t* ucmd)
{
    if (!ent) {
        return;
    }
    if (Any(ucmd->buttons & buttonFlags_t::BUTTON_USE_FORCE)) {
        switch (showPowers[cg.forcepowerSelect]) {
        case forcePowers_t::FP_HEAL:
            ForceHeal(ent);
            break;
        case forcePowers_t::FP_LEVITATION:
            ucmd->upmove = 127;
            break;
        case forcePowers_t::FP_SPEED:
            ForceSpeed(ent);
            break;
        case forcePowers_t::FP_PUSH:
            ForceThrow(ent, false);
            break;
        case forcePowers_t::FP_PULL:
            ForceThrow(ent, true);
            break;
        case forcePowers_t::FP_TELEPATHY:
            ForceTelepathy(ent);
            break;
        case forcePowers_t::FP_GRIP:
            ucmd->buttons |= buttonFlags_t::BUTTON_FORCEGRIP;
            break;
        case forcePowers_t::FP_LIGHTNING:
            ucmd->buttons |= buttonFlags_t::BUTTON_FORCE_LIGHTNING;
            break;
        default:
            break;
        }
    }
}

/*
================
PM_ForcePower
================
*/
static void PM_ForcePower(void)
{
    // check for item using
    if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_USE_FORCE)) {
        if (!Any(pm->ps->pm_flags & pmoveFlags_t::PMF_USE_FORCE)) {
            pm->ps->pm_flags |= pmoveFlags_t::PMF_USE_FORCE;
            PM_AddEvent(entity_event_t::EV_USE_FORCE);
            return;
        }
    }
    else {
        pm->ps->pm_flags &= ~pmoveFlags_t::PMF_USE_FORCE;
    }
}

/*
================
PM_DropTimers
================
*/
static void PM_DropTimers(void)
{
    // drop misc timing counter
    if (pm->ps->pm_time) {
        if (pml.msec >= pm->ps->pm_time) {
            pm->ps->pm_flags &= ~pmoveFlags_t::PMF_ALL_TIMES;
            pm->ps->pm_time = 0;
        }
        else {
            pm->ps->pm_time -= pml.msec;
        }
    }

    // drop legs animation counter
    if (pm->ps->legsAnimTimer > 0) {
        int newTime = pm->ps->legsAnimTimer - pml.msec;

        if (newTime < 0) {
            newTime = 0;
        }

        PM_SetLegsAnimTimer(pm->gent, &pm->ps->legsAnimTimer, newTime);
    }

    // drop torso animation counter
    if (pm->ps->torsoAnimTimer > 0) {
        int newTime = pm->ps->torsoAnimTimer - pml.msec;

        if (newTime < 0) {
            newTime = 0;
        }

        PM_SetTorsoAnimTimer(pm->gent, &pm->ps->torsoAnimTimer, newTime);
    }
}

void PM_SetSpecialMoveValues(void)
{
    Flying = flyMode_t::FLY_NONE;
    if (pm->gent) {
        if (pm->gent->NPC) {
            if (pm->gent->NPC->stats.moveType == movetype_t::MT_FLYSWIM) {
                Flying = flyMode_t::FLY_NORMAL;
            }
        }
        else if (pm->ps->vehicleModel != 0) {
            Flying = flyMode_t::FLY_VEHICLE;
        }
    }

    if (g_timescale != nullptr) {
        if (g_timescale->value < 1.0f) {
            if (!MatrixMode) {
                if (pm->ps->clientNum == 0 && !player_locked && pm->ps->forcePowersActive & Bit(forcePowers_t::FP_SPEED)) {
                    pml.frametime *= (1.0f / g_timescale->value);
                }
                else if (g_entities[pm->ps->clientNum].client && pm->ps->forcePowersActive & Bit(forcePowers_t::FP_SPEED)) {
                    pml.frametime *= (1.0f / g_timescale->value);
                }
            }
        }
    }
}

extern float cg_zoomFov; // from cg_view.cpp

//-------------------------------------------
void PM_AdjustAttackStates(pmove_t* pm)
//-------------------------------------------
{
    int amount;

    // get ammo usage
    if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {
        amount = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] - weaponData[pm->ps->weapon].altEnergyPerShot;
    }
    else {
        amount = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] - weaponData[pm->ps->weapon].energyPerShot;
    }

    if (pm->ps->weapon == weapon_t::WP_SABER && (!cg.zoomMode || pm->ps->clientNum)) { // don't let the alt-attack be interpreted as an actual attack command
        // saber alt-attack does a normal swing, too
        pm->cmd.buttons &= ~buttonFlags_t::BUTTON_ALT_ATTACK;
        if (pm->ps->saberInFlight) { // saber not in hand, can't swing it
            pm->cmd.buttons &= ~buttonFlags_t::BUTTON_ATTACK;
        }
    }

    // disruptor alt-fire should toggle the zoom mode, but only bother doing this for the player?
    if (pm->ps->weapon == weapon_t::WP_DISRUPTOR && pm->gent && pm->gent->s.number == 0 && pm->ps->weaponstate != weaponstate_t::WEAPON_DROPPING) {
        // we are not alt-firing yet, but the alt-attack button was just pressed and
        //	we either are ducking ( in which case we don't care if they are moving )...or they are not ducking...and also not moving right/forward.
        if (!Any(pm->ps->eFlags & entityFlags_t::EF_ALT_FIRING) && Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK) &&
            (pm->cmd.upmove < 0 || (!pm->cmd.forwardmove && !pm->cmd.rightmove))) {
            // We just pressed the alt-fire key
            if (cg.zoomMode == 0 || cg.zoomMode == 3) {
                G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_AUTO, "sound/weapons/disruptor/zoomstart.wav");
                // not already zooming, so do it now
                cg.zoomMode = 2;
                cg.zoomLocked = false;
                cg_zoomFov = 80.0f; // Any(cg.overrides.active&cgOverrideFlags_t::CG_OVERRIDE_FOV) ? cg.overrides.fov : cg_fov.value;
            }
            else if (cg.zoomMode == 2) {
                G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_AUTO, "sound/weapons/disruptor/zoomend.wav");
                // already zooming, so must be wanting to turn it off
                cg.zoomMode = 0;
                cg.zoomTime = cg.time;
                cg.zoomLocked = false;
            }
        }
        else if (!Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {
            // Not pressing zoom any more
            if (cg.zoomMode == 2) {
                // were zooming in, so now lock the zoom
                cg.zoomLocked = true;
            }
        }

        if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)) {
            // If we are zoomed, we should switch the ammo usage to the alt-fire, otherwise, we'll
            //	just use whatever ammo was selected from above
            if (cg.zoomMode == 2) {
                amount = pm->ps->ammo[weaponData[pm->ps->weapon].ammoIndex] - weaponData[pm->ps->weapon].altEnergyPerShot;
            }
        }
        else {
            // alt-fire button pressing doesn't use any ammo
            amount = 0;
        }
    }

    // Check for binocular specific mode
    if (cg.zoomMode == 1 && pm->gent && pm->gent->s.number == 0) //
    {
        if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK) && pm->ps->batteryCharge) {
            // zooming out
            cg.zoomLocked = false;
            cg.zoomDir = 1;
        }
        else if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK) && pm->ps->batteryCharge) {
            // zooming in
            cg.zoomLocked = false;
            cg.zoomDir = -1;
        }
        else {
            // if no buttons are down, we should be in a locked state
            cg.zoomLocked = true;
        }

        // kill buttons and associated firing flags so we can't fire
        pm->ps->eFlags &= ~entityFlags_t::EF_FIRING;
        pm->ps->eFlags &= ~entityFlags_t::EF_ALT_FIRING;
        pm->cmd.buttons &= ~(buttonFlags_t::BUTTON_ALT_ATTACK | buttonFlags_t::BUTTON_ATTACK);
    }

    // set the firing flag for continuous beam weapons, phaser will fire even if out of ammo
    if (((Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK) || Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) &&
         (amount >= 0 || pm->ps->weapon == weapon_t::WP_SABER))) {
        if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {
            pm->ps->eFlags |= entityFlags_t::EF_ALT_FIRING;
            if (!pm->ps->clientNum && pm->gent && Any(pm->ps->eFlags & entityFlags_t::EF_IN_ATST)) { // switch ATST barrels
                pm->gent->alt_fire = true;
            }
        }
        else {
            pm->ps->eFlags &= ~entityFlags_t::EF_ALT_FIRING;
            if (!pm->ps->clientNum && pm->gent && Any(pm->ps->eFlags & entityFlags_t::EF_IN_ATST)) { // switch ATST barrels
                pm->gent->alt_fire = false;
            }
        }

        // This flag should always get set, even when alt-firing
        pm->ps->eFlags |= entityFlags_t::EF_FIRING;
    }
    else {
        //		entityFlags_t iFlags = pm->ps->eFlags;

        // Clear 'em out
        pm->ps->eFlags &= ~entityFlags_t::EF_FIRING;
        pm->ps->eFlags &= ~entityFlags_t::EF_ALT_FIRING;

        // if I don't check the flags before stopping FX then it switches them off too often, which tones down
        //	the stronger FFFX so you can hardly feel them. However, if you only do iton these flags then the
        //	repeat-fire weapons like tetrion and dreadnought don't switch off quick enough. So...
        //
        /* // Might need this for beam type weapons
                if ( pm->ps->weapon == WP_DREADNOUGHT || Any(iFlags & (entityFlags_t::EF_FIRING|entityFlags_t::EF_ALT_FIRING) )
                {
                    cgi_FF_StopAllFX();
                }
                */
    }

    // disruptor should convert a main fire to an alt-fire if the gun is currently zoomed
    if (pm->ps->weapon == weapon_t::WP_DISRUPTOR && pm->gent && pm->gent->s.number == 0) {
        if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK) && cg.zoomMode == 2) {
            // converting the main fire to an alt-fire
            pm->cmd.buttons |= buttonFlags_t::BUTTON_ALT_ATTACK;
            pm->ps->eFlags |= entityFlags_t::EF_ALT_FIRING;
        }
        else {
            // don't let an alt-fire through
            pm->cmd.buttons &= ~buttonFlags_t::BUTTON_ALT_ATTACK;
        }
    }
}

/*
================
Pmove

Can be called by either the server or the client
================
*/
void Pmove(pmove_t* pmove)
{
    pm = pmove;

    // this counter lets us debug movement problems with a journal by setting a conditional breakpoint fot the previous frame
    c_pmove++;

    // clear results
    pm->numtouch = 0;
    pm->watertype = contentsFlags_t::CONTENTS_NONE;
    pm->waterlevel = 0;

    // Clear the blocked flag
    // pm->ps->pm_flags &= ~PMF_BLOCKED;
    // pm->ps->pm_flags &= ~pmoveFlags_t::PMF_BUMPED;

    // In certain situations, we may want to control which attack buttons are pressed and what kind of functionality
    //	is attached to them
    PM_AdjustAttackStates(pm);

    // clear the respawned flag if attack and use are cleared
    if (pm->ps->stats[statIndex_t::STAT_HEALTH] > 0 && !Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)) {
        pm->ps->pm_flags &= ~pmoveFlags_t::PMF_RESPAWNED;
    }

    // clear all pmove local vars
    // all zero, groundTrace's collision records included: only their default constructor makes pml_t non-trivial
    memset((void*)&pml, 0, sizeof(pml));

    // determine the time
    pml.msec = pmove->cmd.serverTime - pm->ps->commandTime;
    if (pml.msec < 1) {
        pml.msec = 1;
    }
    else if (pml.msec > 200) {
        pml.msec = 200;
    }

    pm->ps->commandTime = pmove->cmd.serverTime;

    // save old org in case we get stuck
    VectorCopy(pm->ps->origin, pml.previous_origin);

    // save old velocity for crashlanding
    VectorCopy(pm->ps->velocity, pml.previous_velocity);

    pml.frametime = pml.msec * 0.001;

    PM_SetSpecialMoveValues();

    // update the viewangles
    PM_UpdateViewAngles(pm->ps, &pm->cmd, pm->gent);

    AngleVectors(pm->ps->viewangles, pml.forward, pml.right, pml.up);

    if (pm->cmd.upmove < 10) {
        // not holding jump
        pm->ps->pm_flags &= ~pmoveFlags_t::PMF_JUMP_HELD;
    }

    // decide if backpedaling animations should be used
    if (pm->cmd.forwardmove < 0) {
        pm->ps->pm_flags |= pmoveFlags_t::PMF_BACKWARDS_RUN;
    }
    else if (pm->cmd.forwardmove > 0 || (pm->cmd.forwardmove == 0 && pm->cmd.rightmove)) {
        pm->ps->pm_flags &= ~pmoveFlags_t::PMF_BACKWARDS_RUN;
    }

    if (pm->ps->pm_type >= pmtype_t::PM_DEAD) {
        pm->cmd.forwardmove = 0;
        pm->cmd.rightmove = 0;
        pm->cmd.upmove = 0;
        if (pm->ps->viewheight > -12) { // slowly sink view to ground
            pm->ps->viewheight -= 1;
        }
    }

    if (pm->ps->pm_type == pmtype_t::PM_SPECTATOR) {
        PM_CheckDuck();
        PM_FlyMove();
        PM_DropTimers();
        return;
    }

    if (pm->ps->pm_type == pmtype_t::PM_NOCLIP) {
        PM_NoclipMove();
        PM_DropTimers();
        return;
    }

    if (pm->ps->pm_type == pmtype_t::PM_FREEZE) {
        return; // no movement at all
    }

    if (pm->ps->pm_type == pmtype_t::PM_INTERMISSION) {
        return; // no movement at all
    }

    if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_SLOW_MO_FALL)) { // half grav
        pm->ps->gravity *= 0.5;
    }

    // set watertype, and waterlevel
    PM_SetWaterLevelAtPoint(pm->ps->origin, &pm->waterlevel, &pm->watertype);
    PM_SetWaterHeight();
    if (!Any(pm->watertype & contentsFlags_t::CONTENTS_LADDER)) { // Don't want to remember this for ladders, is only for waterlevel change events (sounds)
        pml.previous_waterlevel = pmove->waterlevel;
    }
    waterForceJump = false;
    if (pmove->waterlevel && pm->ps->clientNum) {
        if (pm->ps->forceJumpZStart                                     // force jumping
            || (pm->gent && !TIMER_Done(pm->gent, "forceJumpChasing"))) // force-jumping
        {
            waterForceJump = true;
        }
    }

    // set mins, maxs, and viewheight
    PM_SetBounds();

    if (Flying == flyMode_t::FLY_NONE && !Any(pm->watertype & contentsFlags_t::CONTENTS_LADDER) &&
        pm->ps->pm_type != pmtype_t::PM_DEAD) { // NOTE: noclippers shouldn't jump or duck either, no?
        PM_CheckDuck();
    }

    // set groundentity
    PM_GroundTrace();

    if (pm->ps->pm_type == pmtype_t::PM_DEAD) {
        PM_DeadMove();
    }

    PM_DropTimers();

    if (pm->ps && Any(pm->ps->eFlags & entityFlags_t::EF_LOCKED_TO_WEAPON)) { // in an emplaced gun
        PM_NoclipMove();
    }
    else if (Flying == flyMode_t::FLY_NORMAL) //|| pm->ps->gravity <= 0 )
    {
        // flight powerup doesn't allow jump and has different friction
        PM_FlyMove();
    }
    else if (Flying == flyMode_t::FLY_VEHICLE) {
        PM_FlyVehicleMove();
    }
    else if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_TIME_WATERJUMP)) {
        PM_WaterJumpMove();
    }
    else if (pm->waterlevel > 1                          // in water
             && (!pm->ps->clientNum || !waterForceJump)) // player or NPC not force jumping
    {                                                    // force-jumping NPCs should
        // swimming or in ladder
        PM_WaterMove();
    }
    else if (pml.walking) { // walking on ground
        vec3_t oldOrg;

        VectorCopy(pm->ps->origin, oldOrg);

        PM_WalkMove();

        float threshHold = 0.001f, movedDist = DistanceSquared(oldOrg, pm->ps->origin);
        if (PM_StandingAnim(pm->ps->legsAnim) || pm->ps->legsAnim == animNumber_t::BOTH_CROUCH1) {
            threshHold = 0.005f;
        }

        if (movedDist < threshHold) { // didn't move, play no legs anim
            pm->cmd.forwardmove = pm->cmd.rightmove = 0;
        }
    }
    else {
        if (pm->ps->gravity <= 0) {
            PM_FlyMove();
        }
        else {
            // airborne
            PM_AirMove();
        }
    }

    // PM_Animate();

    // If we didn't move at all, then why bother doing this again -MW.
    if (!(VectorCompare(pm->ps->origin, pml.previous_origin))) {
        PM_GroundTrace();
    }

    if (pm->ps->groundEntityNum != ENTITYNUM_NONE) { // on ground
        pm->ps->forceJumpZStart = 0;
        pm->ps->jumpZStart = 0;
        pm->ps->pm_flags &= ~pmoveFlags_t::PMF_JUMPING;
        pm->ps->pm_flags &= ~pmoveFlags_t::PMF_TRIGGER_PUSHED;
        pm->ps->pm_flags &= ~pmoveFlags_t::PMF_SLOW_MO_FALL;
    }

    // If we didn't move at all, then why bother doing this again -MW.
    // Note: ok, so long as we don't have water levels that change.
    if (!(VectorCompare(pm->ps->origin, pml.previous_origin))) {
        PM_SetWaterLevelAtPoint(pm->ps->origin, &pm->waterlevel, &pm->watertype);
        PM_SetWaterHeight();
    }

    PM_Inventory();

    PM_ForcePower();

    // weapons
    PM_Weapon();
    if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)) {
        pm->ps->pm_flags |= pmoveFlags_t::PMF_ATTACK_HELD;
    }
    else {
        pm->ps->pm_flags &= ~pmoveFlags_t::PMF_ATTACK_HELD;
    }
    if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {
        pm->ps->pm_flags |= pmoveFlags_t::PMF_ALT_ATTACK_HELD;
    }
    else {
        pm->ps->pm_flags &= ~pmoveFlags_t::PMF_ALT_ATTACK_HELD;
    }

    if (pm->gent) //&& pm->gent->s.number == 0 )//player only?
    {
        // Use
        PM_Use();
    }

    // TEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMP
    if (pm->gent && pm->ps && Any(pm->ps->eFlags & entityFlags_t::EF_LOCKED_TO_WEAPON)) {
        PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_GUNSIT1,
                   setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD); // SETANIM_FLAG_NORMAL
    }
    else // TEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMPTEMP
    {
        // footstep events / legs animations
        PM_Footsteps();
    }
    // torso animation
    PM_TorsoAnimation();

    // entering / leaving water splashes
    PM_WaterEvents();

    // snap some parts of playerstate to save network bandwidth
    // SnapVector( pm->ps->velocity );

    if (!pm->cmd.rightmove && !pm->cmd.forwardmove && pm->cmd.upmove <= 0) {
        if (VectorCompare(pm->ps->velocity, vec3_origin)) {
            pm->ps->lastStationary = level.time;
        }
    }

    if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_SLOW_MO_FALL)) { // half grav
        pm->ps->gravity *= 2;
    }
}
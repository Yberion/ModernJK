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

// BG_PAnimate.c
// game and cgame, NOT ui

#include <iterator>

#include "qcommon/q_shared.h"
#include "bg_public.h"
#include "bg_local.h"
#include "anims.h"
#include "cgame/animtable.h"

#ifdef _GAME
#include "g_local.h"
#elif _CGAME
#include "cgame/cg_local.h"
#elif UI_BUILD
#include "ui/ui_local.h"
#endif

extern saberInfo_t* BG_MySaber(int clientNum, int saberNum);
/*
==============================================================================
BEGIN: Animation utility functions (sequence checking)
==============================================================================
*/
// Called regardless of pm validity:

// VVFIXME - Most of these functions are totally stateless and stupid. Don't
// need multiple copies of this, but it's much easier (and less likely to
// break in the future) if I keep separate namespace versions now.

bool BG_SaberStanceAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_STAND1:            // not really a saberstance anim, actually... "saber off" stance
    case animNumber_t::BOTH_STAND2:            // single-saber, medium style
    case animNumber_t::BOTH_SABERFAST_STANCE:  // single-saber, fast style
    case animNumber_t::BOTH_SABERSLOW_STANCE:  // single-saber, strong style
    case animNumber_t::BOTH_SABERSTAFF_STANCE: // saber staff style
    case animNumber_t::BOTH_SABERDUAL_STANCE:  // dual saber style
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool BG_CrouchAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_SIT1:              // # Normal chair sit.
    case animNumber_t::BOTH_SIT2:              // # Lotus position.
    case animNumber_t::BOTH_SIT3:              // # Sitting in tired position: elbows on knees
    case animNumber_t::BOTH_CROUCH1:           // # Transition from standing to crouch
    case animNumber_t::BOTH_CROUCH1IDLE:       // # Crouching idle
    case animNumber_t::BOTH_CROUCH1WALK:       // # Walking while crouched
    case animNumber_t::BOTH_CROUCH1WALKBACK:   // # Walking while crouched
    case animNumber_t::BOTH_CROUCH2TOSTAND1:   // # going from crouch2 to stand1
    case animNumber_t::BOTH_CROUCH3:           // # Desann crouching down to Kyle (cin 9)
    case animNumber_t::BOTH_KNEES1:            // # Tavion on her knees
    case animNumber_t::BOTH_CROUCHATTACKBACK1: // FIXME: not if in middle of anim?
    case animNumber_t::BOTH_ROLL_STAB:
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool BG_InSpecialJump(animNumber_t anim)
{
    switch ((anim)) {
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
    case animNumber_t::BOTH_BUTTERFLY_FL1:
    case animNumber_t::BOTH_BUTTERFLY_FR1:
    case animNumber_t::BOTH_FJSS_TR_BL:
    case animNumber_t::BOTH_FJSS_TL_BR:
    case animNumber_t::BOTH_FORCELEAP2_T__B_:
    case animNumber_t::BOTH_JUMPFLIPSLASHDOWN1: // #
    case animNumber_t::BOTH_JUMPFLIPSTABDOWN:   // #
    case animNumber_t::BOTH_JUMPATTACK6:
    case animNumber_t::BOTH_JUMPATTACK7:
    case animNumber_t::BOTH_ARIAL_LEFT:
    case animNumber_t::BOTH_ARIAL_RIGHT:
    case animNumber_t::BOTH_ARIAL_F1:
    case animNumber_t::BOTH_CARTWHEEL_LEFT:
    case animNumber_t::BOTH_CARTWHEEL_RIGHT:

    case animNumber_t::BOTH_FORCELONGLEAP_START:
    case animNumber_t::BOTH_FORCELONGLEAP_ATTACK:
    case animNumber_t::BOTH_FORCEWALLRUNFLIP_START:
    case animNumber_t::BOTH_FORCEWALLRUNFLIP_END:
    case animNumber_t::BOTH_FORCEWALLRUNFLIP_ALT:
    case animNumber_t::BOTH_FLIP_ATTACK7:
    case animNumber_t::BOTH_FLIP_HOLD7:
    case animNumber_t::BOTH_FLIP_LAND:
    case animNumber_t::BOTH_A7_SOULCAL:
        return true;
    default:
        break;
    }
    if (BG_InReboundJump(anim)) {
        return true;
    }
    if (BG_InReboundHold(anim)) {
        return true;
    }
    if (BG_InReboundRelease(anim)) {
        return true;
    }
    if (BG_InBackFlip(anim)) {
        return true;
    }
    return false;
}

bool BG_InSaberStandAnim(animNumber_t anim)
{
    switch ((anim)) {
    case animNumber_t::BOTH_SABERFAST_STANCE:
    case animNumber_t::BOTH_STAND2:
    case animNumber_t::BOTH_SABERSLOW_STANCE:
    case animNumber_t::BOTH_SABERDUAL_STANCE:
    case animNumber_t::BOTH_SABERSTAFF_STANCE:
        return true;
    default:
        return false;
    }
}

bool BG_InReboundJump(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_FORCEWALLREBOUND_FORWARD:
    case animNumber_t::BOTH_FORCEWALLREBOUND_LEFT:
    case animNumber_t::BOTH_FORCEWALLREBOUND_BACK:
    case animNumber_t::BOTH_FORCEWALLREBOUND_RIGHT:
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool BG_InReboundHold(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_FORCEWALLHOLD_FORWARD:
    case animNumber_t::BOTH_FORCEWALLHOLD_LEFT:
    case animNumber_t::BOTH_FORCEWALLHOLD_BACK:
    case animNumber_t::BOTH_FORCEWALLHOLD_RIGHT:
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool BG_InReboundRelease(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_FORCEWALLRELEASE_FORWARD:
    case animNumber_t::BOTH_FORCEWALLRELEASE_LEFT:
    case animNumber_t::BOTH_FORCEWALLRELEASE_BACK:
    case animNumber_t::BOTH_FORCEWALLRELEASE_RIGHT:
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool BG_InBackFlip(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_FLIP_BACK1:
    case animNumber_t::BOTH_FLIP_BACK2:
    case animNumber_t::BOTH_FLIP_BACK3:
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool BG_DirectFlippingAnim(animNumber_t anim)
{
    switch ((anim)) {
    case animNumber_t::BOTH_FLIP_F: // # Flip forward
    case animNumber_t::BOTH_FLIP_B: // # Flip backwards
    case animNumber_t::BOTH_FLIP_L: // # Flip left
    case animNumber_t::BOTH_FLIP_R: // # Flip right
        return true;
        break;
    default:
        break;
    }

    return false;
}

bool BG_SaberInAttackPure(saberMoveName_t move)
{
    if (move >= saberMoveName_t::LS_A_TL2BR && move <= saberMoveName_t::LS_A_T2B) {
        return true;
    }
    return false;
}
bool BG_SaberInAttack(saberMoveName_t move)
{
    if (move >= saberMoveName_t::LS_A_TL2BR && move <= saberMoveName_t::LS_A_T2B) {
        return true;
    }
    switch (move) {
    case saberMoveName_t::LS_A_BACK:
    case saberMoveName_t::LS_A_BACK_CR:
    case saberMoveName_t::LS_A_BACKSTAB:
    case saberMoveName_t::LS_ROLL_STAB:
    case saberMoveName_t::LS_A_LUNGE:
    case saberMoveName_t::LS_A_JUMP_T__B_:
    case saberMoveName_t::LS_A_FLIP_STAB:
    case saberMoveName_t::LS_A_FLIP_SLASH:
    case saberMoveName_t::LS_JUMPATTACK_DUAL:
    case saberMoveName_t::LS_JUMPATTACK_ARIAL_LEFT:
    case saberMoveName_t::LS_JUMPATTACK_ARIAL_RIGHT:
    case saberMoveName_t::LS_JUMPATTACK_CART_LEFT:
    case saberMoveName_t::LS_JUMPATTACK_CART_RIGHT:
    case saberMoveName_t::LS_JUMPATTACK_STAFF_LEFT:
    case saberMoveName_t::LS_JUMPATTACK_STAFF_RIGHT:
    case saberMoveName_t::LS_BUTTERFLY_LEFT:
    case saberMoveName_t::LS_BUTTERFLY_RIGHT:
    case saberMoveName_t::LS_A_BACKFLIP_ATK:
    case saberMoveName_t::LS_SPINATTACK_DUAL:
    case saberMoveName_t::LS_SPINATTACK:
    case saberMoveName_t::LS_LEAP_ATTACK:
    case saberMoveName_t::LS_SWOOP_ATTACK_RIGHT:
    case saberMoveName_t::LS_SWOOP_ATTACK_LEFT:
    case saberMoveName_t::LS_TAUNTAUN_ATTACK_RIGHT:
    case saberMoveName_t::LS_TAUNTAUN_ATTACK_LEFT:
    case saberMoveName_t::LS_KICK_F:
    case saberMoveName_t::LS_KICK_B:
    case saberMoveName_t::LS_KICK_R:
    case saberMoveName_t::LS_KICK_L:
    case saberMoveName_t::LS_KICK_S:
    case saberMoveName_t::LS_KICK_BF:
    case saberMoveName_t::LS_KICK_RL:
    case saberMoveName_t::LS_KICK_F_AIR:
    case saberMoveName_t::LS_KICK_B_AIR:
    case saberMoveName_t::LS_KICK_R_AIR:
    case saberMoveName_t::LS_KICK_L_AIR:
    case saberMoveName_t::LS_STABDOWN:
    case saberMoveName_t::LS_STABDOWN_STAFF:
    case saberMoveName_t::LS_STABDOWN_DUAL:
    case saberMoveName_t::LS_DUAL_SPIN_PROTECT:
    case saberMoveName_t::LS_STAFF_SOULCAL:
    case saberMoveName_t::LS_A1_SPECIAL:
    case saberMoveName_t::LS_A2_SPECIAL:
    case saberMoveName_t::LS_A3_SPECIAL:
    case saberMoveName_t::LS_UPSIDE_DOWN_ATTACK:
    case saberMoveName_t::LS_PULL_ATTACK_STAB:
    case saberMoveName_t::LS_PULL_ATTACK_SWING:
    case saberMoveName_t::LS_SPINATTACK_ALORA:
    case saberMoveName_t::LS_DUAL_FB:
    case saberMoveName_t::LS_DUAL_LR:
    case saberMoveName_t::LS_HILT_BASH:
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool BG_SaberInKata(saberMoveName_t saberMove)
{
    switch (saberMove) {
    case saberMoveName_t::LS_A1_SPECIAL:
    case saberMoveName_t::LS_A2_SPECIAL:
    case saberMoveName_t::LS_A3_SPECIAL:
    case saberMoveName_t::LS_DUAL_SPIN_PROTECT:
    case saberMoveName_t::LS_STAFF_SOULCAL:
        return true;
    default:
        break;
    }
    return false;
}

bool BG_InKataAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_A6_SABERPROTECT:
    case animNumber_t::BOTH_A7_SOULCAL:
    case animNumber_t::BOTH_A1_SPECIAL:
    case animNumber_t::BOTH_A2_SPECIAL:
    case animNumber_t::BOTH_A3_SPECIAL:
        return true;
    default:
        break;
    }
    return false;
}

bool BG_SaberInSpecial(saberMoveName_t move)
{
    switch (move) {
    case saberMoveName_t::LS_A_BACK:
    case saberMoveName_t::LS_A_BACK_CR:
    case saberMoveName_t::LS_A_BACKSTAB:
    case saberMoveName_t::LS_ROLL_STAB:
    case saberMoveName_t::LS_A_LUNGE:
    case saberMoveName_t::LS_A_JUMP_T__B_:
    case saberMoveName_t::LS_A_FLIP_STAB:
    case saberMoveName_t::LS_A_FLIP_SLASH:
    case saberMoveName_t::LS_JUMPATTACK_DUAL:
    case saberMoveName_t::LS_JUMPATTACK_ARIAL_LEFT:
    case saberMoveName_t::LS_JUMPATTACK_ARIAL_RIGHT:
    case saberMoveName_t::LS_JUMPATTACK_CART_LEFT:
    case saberMoveName_t::LS_JUMPATTACK_CART_RIGHT:
    case saberMoveName_t::LS_JUMPATTACK_STAFF_LEFT:
    case saberMoveName_t::LS_JUMPATTACK_STAFF_RIGHT:
    case saberMoveName_t::LS_BUTTERFLY_LEFT:
    case saberMoveName_t::LS_BUTTERFLY_RIGHT:
    case saberMoveName_t::LS_A_BACKFLIP_ATK:
    case saberMoveName_t::LS_SPINATTACK_DUAL:
    case saberMoveName_t::LS_SPINATTACK:
    case saberMoveName_t::LS_LEAP_ATTACK:
    case saberMoveName_t::LS_SWOOP_ATTACK_RIGHT:
    case saberMoveName_t::LS_SWOOP_ATTACK_LEFT:
    case saberMoveName_t::LS_TAUNTAUN_ATTACK_RIGHT:
    case saberMoveName_t::LS_TAUNTAUN_ATTACK_LEFT:
    case saberMoveName_t::LS_KICK_F:
    case saberMoveName_t::LS_KICK_B:
    case saberMoveName_t::LS_KICK_R:
    case saberMoveName_t::LS_KICK_L:
    case saberMoveName_t::LS_KICK_S:
    case saberMoveName_t::LS_KICK_BF:
    case saberMoveName_t::LS_KICK_RL:
    case saberMoveName_t::LS_KICK_F_AIR:
    case saberMoveName_t::LS_KICK_B_AIR:
    case saberMoveName_t::LS_KICK_R_AIR:
    case saberMoveName_t::LS_KICK_L_AIR:
    case saberMoveName_t::LS_STABDOWN:
    case saberMoveName_t::LS_STABDOWN_STAFF:
    case saberMoveName_t::LS_STABDOWN_DUAL:
    case saberMoveName_t::LS_DUAL_SPIN_PROTECT:
    case saberMoveName_t::LS_STAFF_SOULCAL:
    case saberMoveName_t::LS_A1_SPECIAL:
    case saberMoveName_t::LS_A2_SPECIAL:
    case saberMoveName_t::LS_A3_SPECIAL:
    case saberMoveName_t::LS_UPSIDE_DOWN_ATTACK:
    case saberMoveName_t::LS_PULL_ATTACK_STAB:
    case saberMoveName_t::LS_PULL_ATTACK_SWING:
    case saberMoveName_t::LS_SPINATTACK_ALORA:
    case saberMoveName_t::LS_DUAL_FB:
    case saberMoveName_t::LS_DUAL_LR:
    case saberMoveName_t::LS_HILT_BASH:
        return true;
    default:
        break;
    }
    return false;
}

bool BG_KickMove(saberMoveName_t move)
{
    switch (move) {
    case saberMoveName_t::LS_KICK_F:
    case saberMoveName_t::LS_KICK_B:
    case saberMoveName_t::LS_KICK_R:
    case saberMoveName_t::LS_KICK_L:
    case saberMoveName_t::LS_KICK_S:
    case saberMoveName_t::LS_KICK_BF:
    case saberMoveName_t::LS_KICK_RL:
    case saberMoveName_t::LS_KICK_F_AIR:
    case saberMoveName_t::LS_KICK_B_AIR:
    case saberMoveName_t::LS_KICK_R_AIR:
    case saberMoveName_t::LS_KICK_L_AIR:
    case saberMoveName_t::LS_HILT_BASH:
        return true;
    default:
        break;
    }
    return false;
}

bool BG_SaberInIdle(saberMoveName_t move)
{
    switch (move) {
    case saberMoveName_t::LS_NONE:
    case saberMoveName_t::LS_READY:
    case saberMoveName_t::LS_DRAW:
    case saberMoveName_t::LS_PUTAWAY:
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool BG_InExtraDefenseSaberMove(saberMoveName_t move)
{
    switch (move) {
    case saberMoveName_t::LS_SPINATTACK_DUAL:
    case saberMoveName_t::LS_SPINATTACK:
    case saberMoveName_t::LS_DUAL_SPIN_PROTECT:
    case saberMoveName_t::LS_STAFF_SOULCAL:
    case saberMoveName_t::LS_A1_SPECIAL:
    case saberMoveName_t::LS_A2_SPECIAL:
    case saberMoveName_t::LS_A3_SPECIAL:
    case saberMoveName_t::LS_JUMPATTACK_DUAL:
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool BG_FlippingAnim(animNumber_t anim)
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
    case animNumber_t::BOTH_BUTTERFLY_FL1:
    case animNumber_t::BOTH_BUTTERFLY_FR1:
    //
    case animNumber_t::BOTH_ARIAL_LEFT:
    case animNumber_t::BOTH_ARIAL_RIGHT:
    case animNumber_t::BOTH_ARIAL_F1:
    case animNumber_t::BOTH_CARTWHEEL_LEFT:
    case animNumber_t::BOTH_CARTWHEEL_RIGHT:
    case animNumber_t::BOTH_JUMPFLIPSLASHDOWN1:
    case animNumber_t::BOTH_JUMPFLIPSTABDOWN:
    case animNumber_t::BOTH_JUMPATTACK6:
    case animNumber_t::BOTH_JUMPATTACK7:
    // JKA
    case animNumber_t::BOTH_FORCEWALLRUNFLIP_END:
    case animNumber_t::BOTH_FORCEWALLRUNFLIP_ALT:
    case animNumber_t::BOTH_FLIP_ATTACK7:
    case animNumber_t::BOTH_A7_SOULCAL:
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool BG_SpinningSaberAnim(animNumber_t anim)
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
    // level 6
    case animNumber_t::BOTH_T6_BR_TL:
    case animNumber_t::BOTH_T6__R_TL:
    case animNumber_t::BOTH_T6__R__L:
    case animNumber_t::BOTH_T6__R_BL:
    case animNumber_t::BOTH_T6_TR_TL:
    case animNumber_t::BOTH_T6_TR__L:
    case animNumber_t::BOTH_T6_TR_BL:
    case animNumber_t::BOTH_T6_T__TL:
    case animNumber_t::BOTH_T6_T__BL:
    case animNumber_t::BOTH_T6_TL_BR:
    case animNumber_t::BOTH_T6__L_BR:
    case animNumber_t::BOTH_T6__L__R:
    case animNumber_t::BOTH_T6_TL__R:
    case animNumber_t::BOTH_T6_TL_TR:
    case animNumber_t::BOTH_T6__L_TR:
    case animNumber_t::BOTH_T6__L_T_:
    case animNumber_t::BOTH_T6_BL_T_:
    case animNumber_t::BOTH_T6_BR__L:
    case animNumber_t::BOTH_T6_BR_BL:
    case animNumber_t::BOTH_T6_BL_BR:
    case animNumber_t::BOTH_T6_BL__R:
    case animNumber_t::BOTH_T6_BL_TR:
    // level 7
    case animNumber_t::BOTH_T7_BR_TL:
    case animNumber_t::BOTH_T7_BR__L:
    case animNumber_t::BOTH_T7_BR_BL:
    case animNumber_t::BOTH_T7__R__L:
    case animNumber_t::BOTH_T7__R_BL:
    case animNumber_t::BOTH_T7_TR__L:
    case animNumber_t::BOTH_T7_T___R:
    case animNumber_t::BOTH_T7_TL_BR:
    case animNumber_t::BOTH_T7__L_BR:
    case animNumber_t::BOTH_T7__L__R:
    case animNumber_t::BOTH_T7_BL_BR:
    case animNumber_t::BOTH_T7_BL__R:
    case animNumber_t::BOTH_T7_BL_TR:
    case animNumber_t::BOTH_T7_TL_TR:
    case animNumber_t::BOTH_T7_T__BR:
    case animNumber_t::BOTH_T7__L_TR:
    case animNumber_t::BOTH_V7_BL_S7:
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

bool BG_SaberInSpecialAttack(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_A2_STABBACK1:
    case animNumber_t::BOTH_ATTACK_BACK:
    case animNumber_t::BOTH_CROUCHATTACKBACK1:
    case animNumber_t::BOTH_ROLL_STAB:
    case animNumber_t::BOTH_BUTTERFLY_LEFT:
    case animNumber_t::BOTH_BUTTERFLY_RIGHT:
    case animNumber_t::BOTH_BUTTERFLY_FL1:
    case animNumber_t::BOTH_BUTTERFLY_FR1:
    case animNumber_t::BOTH_FJSS_TR_BL:
    case animNumber_t::BOTH_FJSS_TL_BR:
    case animNumber_t::BOTH_LUNGE2_B__T_:
    case animNumber_t::BOTH_FORCELEAP2_T__B_:
    case animNumber_t::BOTH_JUMPFLIPSLASHDOWN1: // #
    case animNumber_t::BOTH_JUMPFLIPSTABDOWN:   // #
    case animNumber_t::BOTH_JUMPATTACK6:
    case animNumber_t::BOTH_JUMPATTACK7:
    case animNumber_t::BOTH_SPINATTACK6:
    case animNumber_t::BOTH_SPINATTACK7:
    case animNumber_t::BOTH_FORCELONGLEAP_ATTACK:
    case animNumber_t::BOTH_VS_ATR_S:
    case animNumber_t::BOTH_VS_ATL_S:
    case animNumber_t::BOTH_VT_ATR_S:
    case animNumber_t::BOTH_VT_ATL_S:
    case animNumber_t::BOTH_A7_KICK_F:
    case animNumber_t::BOTH_A7_KICK_B:
    case animNumber_t::BOTH_A7_KICK_R:
    case animNumber_t::BOTH_A7_KICK_L:
    case animNumber_t::BOTH_A7_KICK_S:
    case animNumber_t::BOTH_A7_KICK_BF:
    case animNumber_t::BOTH_A7_KICK_RL:
    case animNumber_t::BOTH_A7_KICK_F_AIR:
    case animNumber_t::BOTH_A7_KICK_B_AIR:
    case animNumber_t::BOTH_A7_KICK_R_AIR:
    case animNumber_t::BOTH_A7_KICK_L_AIR:
    case animNumber_t::BOTH_STABDOWN:
    case animNumber_t::BOTH_STABDOWN_STAFF:
    case animNumber_t::BOTH_STABDOWN_DUAL:
    case animNumber_t::BOTH_A6_SABERPROTECT:
    case animNumber_t::BOTH_A7_SOULCAL:
    case animNumber_t::BOTH_A1_SPECIAL:
    case animNumber_t::BOTH_A2_SPECIAL:
    case animNumber_t::BOTH_A3_SPECIAL:
    case animNumber_t::BOTH_FLIP_ATTACK7:
    case animNumber_t::BOTH_PULL_IMPALE_STAB:
    case animNumber_t::BOTH_PULL_IMPALE_SWING:
    case animNumber_t::BOTH_ALORA_SPIN_SLASH:
    case animNumber_t::BOTH_A6_FB:
    case animNumber_t::BOTH_A6_LR:
    case animNumber_t::BOTH_A7_HILT:
        return true;
    default:
        break;
    }
    return false;
}

bool BG_KickingAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_A7_KICK_F:
    case animNumber_t::BOTH_A7_KICK_B:
    case animNumber_t::BOTH_A7_KICK_R:
    case animNumber_t::BOTH_A7_KICK_L:
    case animNumber_t::BOTH_A7_KICK_S:
    case animNumber_t::BOTH_A7_KICK_BF:
    case animNumber_t::BOTH_A7_KICK_RL:
    case animNumber_t::BOTH_A7_KICK_F_AIR:
    case animNumber_t::BOTH_A7_KICK_B_AIR:
    case animNumber_t::BOTH_A7_KICK_R_AIR:
    case animNumber_t::BOTH_A7_KICK_L_AIR:
    case animNumber_t::BOTH_A7_HILT:
    // NOT kicks, but do kick traces anyway
    case animNumber_t::BOTH_GETUP_BROLL_B:
    case animNumber_t::BOTH_GETUP_BROLL_F:
    case animNumber_t::BOTH_GETUP_FROLL_B:
    case animNumber_t::BOTH_GETUP_FROLL_F:
        return true;
        break;
    default:
        break;
    }
    return false;
}

int BG_InGrappleMove(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_KYLE_GRAB:
    case animNumber_t::BOTH_KYLE_MISS:
        return 1; // grabbing at someone
    case animNumber_t::BOTH_KYLE_PA_1:
    case animNumber_t::BOTH_KYLE_PA_2:
        return 2; // beating the shit out of someone
    case animNumber_t::BOTH_PLAYER_PA_1:
    case animNumber_t::BOTH_PLAYER_PA_2:
    case animNumber_t::BOTH_PLAYER_PA_FLY:
        return 3; // getting the shit beaten out of you
        break;
    default:
        break;
    }

    return 0;
}

saberMoveName_t BG_BrokenParryForAttack(saberMoveName_t move)
{
    // Our attack was knocked away by a knockaway parry
    // FIXME: need actual anims for this
    // FIXME: need to know which side of the saber was hit!  For now, we presume the saber gets knocked away from the center
    switch (saberMoveData[move].startQuad) {
    case saberQuadrant_t::Q_B:
        return saberMoveName_t::LS_V1_B_;
        break;
    case saberQuadrant_t::Q_BR:
        return saberMoveName_t::LS_V1_BR;
        break;
    case saberQuadrant_t::Q_R:
        return saberMoveName_t::LS_V1__R;
        break;
    case saberQuadrant_t::Q_TR:
        return saberMoveName_t::LS_V1_TR;
        break;
    case saberQuadrant_t::Q_T:
        return saberMoveName_t::LS_V1_T_;
        break;
    case saberQuadrant_t::Q_TL:
        return saberMoveName_t::LS_V1_TL;
        break;
    case saberQuadrant_t::Q_L:
        return saberMoveName_t::LS_V1__L;
        break;
    case saberQuadrant_t::Q_BL:
        return saberMoveName_t::LS_V1_BL;
        break;
    default:
        break;
    }
    return saberMoveName_t::LS_NONE;
}

saberMoveName_t BG_BrokenParryForParry(saberMoveName_t move)
{
    // FIXME: need actual anims for this
    // FIXME: need to know which side of the saber was hit!  For now, we presume the saber gets knocked away from the center
    switch (move) {
    case saberMoveName_t::LS_PARRY_UP:
        // Hmm... since we don't know what dir the hit came from, randomly pick knock down or knock back
        if (Q_irand(0, 1)) {
            return saberMoveName_t::LS_H1_B_;
        }
        else {
            return saberMoveName_t::LS_H1_T_;
        }
        break;
    case saberMoveName_t::LS_PARRY_UR:
        return saberMoveName_t::LS_H1_TR;
        break;
    case saberMoveName_t::LS_PARRY_UL:
        return saberMoveName_t::LS_H1_TL;
        break;
    case saberMoveName_t::LS_PARRY_LR:
        return saberMoveName_t::LS_H1_BR;
        break;
    case saberMoveName_t::LS_PARRY_LL:
        return saberMoveName_t::LS_H1_BL;
        break;
    case saberMoveName_t::LS_READY:
        return saberMoveName_t::LS_H1_B_; //???
        break;
    default:
        break;
    }
    return saberMoveName_t::LS_NONE;
}

saberMoveName_t BG_KnockawayForParry(saberBlockedType_t move)
{
    // FIXME: need actual anims for this
    // FIXME: need to know which side of the saber was hit!  For now, we presume the saber gets knocked away from the center
    switch (move) {
    case saberBlockedType_t::BLOCKED_TOP: // LS_PARRY_UP:
        return saberMoveName_t::LS_K1_T_; // push up
        break;
    case saberBlockedType_t::BLOCKED_UPPER_RIGHT: // LS_PARRY_UR:
    default:                                      // case saberMoveName_t::LS_READY:
        return saberMoveName_t::LS_K1_TR;         // push up, slightly to right
        break;
    case saberBlockedType_t::BLOCKED_UPPER_LEFT: // LS_PARRY_UL:
        return saberMoveName_t::LS_K1_TL;        // push up and to left
        break;
    case saberBlockedType_t::BLOCKED_LOWER_RIGHT: // LS_PARRY_LR:
        return saberMoveName_t::LS_K1_BR;         // push down and to left
        break;
    case saberBlockedType_t::BLOCKED_LOWER_LEFT: // LS_PARRY_LL:
        return saberMoveName_t::LS_K1_BL;        // push down and to right
        break;
    }
    // return saberMoveName_t::LS_NONE;
}

bool BG_InRoll(playerState_t* ps, animNumber_t anim)
{
    switch ((anim)) {
    case animNumber_t::BOTH_GETUP_BROLL_B:
    case animNumber_t::BOTH_GETUP_BROLL_F:
    case animNumber_t::BOTH_GETUP_BROLL_L:
    case animNumber_t::BOTH_GETUP_BROLL_R:
    case animNumber_t::BOTH_GETUP_FROLL_B:
    case animNumber_t::BOTH_GETUP_FROLL_F:
    case animNumber_t::BOTH_GETUP_FROLL_L:
    case animNumber_t::BOTH_GETUP_FROLL_R:
    case animNumber_t::BOTH_ROLL_F:
    case animNumber_t::BOTH_ROLL_B:
    case animNumber_t::BOTH_ROLL_R:
    case animNumber_t::BOTH_ROLL_L:
        if (ps->legsTimer > 0) {
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

bool BG_InSpecialDeathAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_DEATH_ROLL:       // # Death anim from a roll
    case animNumber_t::BOTH_DEATH_FLIP:       // # Death anim from a flip
    case animNumber_t::BOTH_DEATH_SPIN_90_R:  // # Death anim when facing 90 degrees right
    case animNumber_t::BOTH_DEATH_SPIN_90_L:  // # Death anim when facing 90 degrees left
    case animNumber_t::BOTH_DEATH_SPIN_180:   // # Death anim when facing backwards
    case animNumber_t::BOTH_DEATH_LYING_UP:   // # Death anim when lying on back
    case animNumber_t::BOTH_DEATH_LYING_DN:   // # Death anim when lying on front
    case animNumber_t::BOTH_DEATH_FALLING_DN: // # Death anim when falling on face
    case animNumber_t::BOTH_DEATH_FALLING_UP: // # Death anim when falling on back
    case animNumber_t::BOTH_DEATH_CROUCHED:   // # Death anim when crouched
        return true;
        break;
    default:
        return false;
        break;
    }
}

bool BG_InDeathAnim(animNumber_t anim)
{ // Purposely does not cover stumbledeath and falldeath...
    switch (anim) {
    case animNumber_t::BOTH_DEATH1:         // # First Death anim
    case animNumber_t::BOTH_DEATH2:         // # Second Death anim
    case animNumber_t::BOTH_DEATH3:         // # Third Death anim
    case animNumber_t::BOTH_DEATH4:         // # Fourth Death anim
    case animNumber_t::BOTH_DEATH5:         // # Fifth Death anim
    case animNumber_t::BOTH_DEATH6:         // # Sixth Death anim
    case animNumber_t::BOTH_DEATH7:         // # Seventh Death anim
    case animNumber_t::BOTH_DEATH8:         // #
    case animNumber_t::BOTH_DEATH9:         // #
    case animNumber_t::BOTH_DEATH10:        // #
    case animNumber_t::BOTH_DEATH11:        // #
    case animNumber_t::BOTH_DEATH12:        // #
    case animNumber_t::BOTH_DEATH13:        // #
    case animNumber_t::BOTH_DEATH14:        // #
    case animNumber_t::BOTH_DEATH14_UNGRIP: // # Desann's end death (cin #35)
    case animNumber_t::BOTH_DEATH14_SITUP:  // # Tavion sitting up after having been thrown (cin #23)
    case animNumber_t::BOTH_DEATH15:        // #
    case animNumber_t::BOTH_DEATH16:        // #
    case animNumber_t::BOTH_DEATH17:        // #
    case animNumber_t::BOTH_DEATH18:        // #
    case animNumber_t::BOTH_DEATH19:        // #
    case animNumber_t::BOTH_DEATH20:        // #
    case animNumber_t::BOTH_DEATH21:        // #
    case animNumber_t::BOTH_DEATH22:        // #
    case animNumber_t::BOTH_DEATH23:        // #
    case animNumber_t::BOTH_DEATH24:        // #
    case animNumber_t::BOTH_DEATH25:        // #

    case animNumber_t::BOTH_DEATHFORWARD1:  // # First Death in which they get thrown forward
    case animNumber_t::BOTH_DEATHFORWARD2:  // # Second Death in which they get thrown forward
    case animNumber_t::BOTH_DEATHFORWARD3:  // # Tavion's falling in cin# 23
    case animNumber_t::BOTH_DEATHBACKWARD1: // # First Death in which they get thrown backward
    case animNumber_t::BOTH_DEATHBACKWARD2: // # Second Death in which they get thrown backward

    case animNumber_t::BOTH_DEATH1IDLE:      // # Idle while close to death
    case animNumber_t::BOTH_LYINGDEATH1:     // # Death to play when killed lying down
    case animNumber_t::BOTH_STUMBLEDEATH1:   // # Stumble forward and fall face first death
    case animNumber_t::BOTH_FALLDEATH1:      // # Fall forward off a high cliff and splat death - start
    case animNumber_t::BOTH_FALLDEATH1INAIR: // # Fall forward off a high cliff and splat death - loop
    case animNumber_t::BOTH_FALLDEATH1LAND:  // # Fall forward off a high cliff and splat death - hit bottom
    // # #sep case BOTH_ DEAD POSES # Should be last frame of corresponding previous anims
    case animNumber_t::BOTH_DEAD1:         // # First Death finished pose
    case animNumber_t::BOTH_DEAD2:         // # Second Death finished pose
    case animNumber_t::BOTH_DEAD3:         // # Third Death finished pose
    case animNumber_t::BOTH_DEAD4:         // # Fourth Death finished pose
    case animNumber_t::BOTH_DEAD5:         // # Fifth Death finished pose
    case animNumber_t::BOTH_DEAD6:         // # Sixth Death finished pose
    case animNumber_t::BOTH_DEAD7:         // # Seventh Death finished pose
    case animNumber_t::BOTH_DEAD8:         // #
    case animNumber_t::BOTH_DEAD9:         // #
    case animNumber_t::BOTH_DEAD10:        // #
    case animNumber_t::BOTH_DEAD11:        // #
    case animNumber_t::BOTH_DEAD12:        // #
    case animNumber_t::BOTH_DEAD13:        // #
    case animNumber_t::BOTH_DEAD14:        // #
    case animNumber_t::BOTH_DEAD15:        // #
    case animNumber_t::BOTH_DEAD16:        // #
    case animNumber_t::BOTH_DEAD17:        // #
    case animNumber_t::BOTH_DEAD18:        // #
    case animNumber_t::BOTH_DEAD19:        // #
    case animNumber_t::BOTH_DEAD20:        // #
    case animNumber_t::BOTH_DEAD21:        // #
    case animNumber_t::BOTH_DEAD22:        // #
    case animNumber_t::BOTH_DEAD23:        // #
    case animNumber_t::BOTH_DEAD24:        // #
    case animNumber_t::BOTH_DEAD25:        // #
    case animNumber_t::BOTH_DEADFORWARD1:  // # First thrown forward death finished pose
    case animNumber_t::BOTH_DEADFORWARD2:  // # Second thrown forward death finished pose
    case animNumber_t::BOTH_DEADBACKWARD1: // # First thrown backward death finished pose
    case animNumber_t::BOTH_DEADBACKWARD2: // # Second thrown backward death finished pose
    case animNumber_t::BOTH_LYINGDEAD1:    // # Killed lying down death finished pose
    case animNumber_t::BOTH_STUMBLEDEAD1:  // # Stumble forward death finished pose
    case animNumber_t::BOTH_FALLDEAD1LAND: // # Fall forward and splat death finished pose
    // # #sep case BOTH_ DEAD TWITCH/FLOP # React to being shot from death poses
    case animNumber_t::BOTH_DEADFLOP1:        // # React to being shot from First Death finished pose
    case animNumber_t::BOTH_DEADFLOP2:        // # React to being shot from Second Death finished pose
    case animNumber_t::BOTH_DISMEMBER_HEAD1:  // #
    case animNumber_t::BOTH_DISMEMBER_TORSO1: // #
    case animNumber_t::BOTH_DISMEMBER_LLEG:   // #
    case animNumber_t::BOTH_DISMEMBER_RLEG:   // #
    case animNumber_t::BOTH_DISMEMBER_RARM:   // #
    case animNumber_t::BOTH_DISMEMBER_LARM:   // #
        return true;
        break;
    default:
        return BG_InSpecialDeathAnim(anim);
        break;
    }
}

bool BG_InKnockDownOnly(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_KNOCKDOWN1:
    case animNumber_t::BOTH_KNOCKDOWN2:
    case animNumber_t::BOTH_KNOCKDOWN3:
    case animNumber_t::BOTH_KNOCKDOWN4:
    case animNumber_t::BOTH_KNOCKDOWN5:
        return true;
    default:
        break;
    }
    return false;
}

bool BG_InSaberLockOld(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_BF2LOCK:
    case animNumber_t::BOTH_BF1LOCK:
    case animNumber_t::BOTH_CWCIRCLELOCK:
    case animNumber_t::BOTH_CCWCIRCLELOCK:
        return true;
    default:
        break;
    }
    return false;
}

bool BG_InSaberLock(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_LK_S_DL_S_L_1:  // lock if I'm using single vs. a dual
    case animNumber_t::BOTH_LK_S_DL_T_L_1:  // lock if I'm using single vs. a dual
    case animNumber_t::BOTH_LK_S_ST_S_L_1:  // lock if I'm using single vs. a staff
    case animNumber_t::BOTH_LK_S_ST_T_L_1:  // lock if I'm using single vs. a staff
    case animNumber_t::BOTH_LK_S_S_S_L_1:   // lock if I'm using single vs. a single and I initiated
    case animNumber_t::BOTH_LK_S_S_T_L_1:   // lock if I'm using single vs. a single and I initiated
    case animNumber_t::BOTH_LK_DL_DL_S_L_1: // lock if I'm using dual vs. dual and I initiated
    case animNumber_t::BOTH_LK_DL_DL_T_L_1: // lock if I'm using dual vs. dual and I initiated
    case animNumber_t::BOTH_LK_DL_ST_S_L_1: // lock if I'm using dual vs. a staff
    case animNumber_t::BOTH_LK_DL_ST_T_L_1: // lock if I'm using dual vs. a staff
    case animNumber_t::BOTH_LK_DL_S_S_L_1:  // lock if I'm using dual vs. a single
    case animNumber_t::BOTH_LK_DL_S_T_L_1:  // lock if I'm using dual vs. a single
    case animNumber_t::BOTH_LK_ST_DL_S_L_1: // lock if I'm using staff vs. dual
    case animNumber_t::BOTH_LK_ST_DL_T_L_1: // lock if I'm using staff vs. dual
    case animNumber_t::BOTH_LK_ST_ST_S_L_1: // lock if I'm using staff vs. a staff and I initiated
    case animNumber_t::BOTH_LK_ST_ST_T_L_1: // lock if I'm using staff vs. a staff and I initiated
    case animNumber_t::BOTH_LK_ST_S_S_L_1:  // lock if I'm using staff vs. a single
    case animNumber_t::BOTH_LK_ST_S_T_L_1:  // lock if I'm using staff vs. a single
    case animNumber_t::BOTH_LK_S_S_S_L_2:
    case animNumber_t::BOTH_LK_S_S_T_L_2:
    case animNumber_t::BOTH_LK_DL_DL_S_L_2:
    case animNumber_t::BOTH_LK_DL_DL_T_L_2:
    case animNumber_t::BOTH_LK_ST_ST_S_L_2:
    case animNumber_t::BOTH_LK_ST_ST_T_L_2:
        return true;
        break;
    default:
        return BG_InSaberLockOld(anim);
        break;
    }
    // return false;
}

// Called only where pm is valid (not all require pm, but some do):
bool PM_InCartwheel(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_ARIAL_LEFT:
    case animNumber_t::BOTH_ARIAL_RIGHT:
    case animNumber_t::BOTH_ARIAL_F1:
    case animNumber_t::BOTH_CARTWHEEL_LEFT:
    case animNumber_t::BOTH_CARTWHEEL_RIGHT:
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool BG_InKnockDownOnGround(playerState_t* ps)
{
    switch (ps->legsAnim) {
    case animNumber_t::BOTH_KNOCKDOWN1:
    case animNumber_t::BOTH_KNOCKDOWN2:
    case animNumber_t::BOTH_KNOCKDOWN3:
    case animNumber_t::BOTH_KNOCKDOWN4:
    case animNumber_t::BOTH_KNOCKDOWN5:
    case animNumber_t::BOTH_RELEASED:
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
        if (BG_AnimLength(0, ps->legsAnim) - ps->legsTimer < 500) { // at beginning of getup anim
            return true;
        }
        break;
    case animNumber_t::BOTH_GETUP_BROLL_B:
    case animNumber_t::BOTH_GETUP_BROLL_F:
    case animNumber_t::BOTH_GETUP_BROLL_L:
    case animNumber_t::BOTH_GETUP_BROLL_R:
    case animNumber_t::BOTH_GETUP_FROLL_B:
    case animNumber_t::BOTH_GETUP_FROLL_F:
    case animNumber_t::BOTH_GETUP_FROLL_L:
    case animNumber_t::BOTH_GETUP_FROLL_R:
        if (BG_AnimLength(0, ps->legsAnim) - ps->legsTimer < 500) { // at beginning of getup anim
            return true;
        }
        break;
    case animNumber_t::BOTH_LK_DL_ST_T_SB_1_L:
        if (ps->legsTimer < 1000) {
            return true;
        }
        break;
    case animNumber_t::BOTH_PLAYER_PA_3_FLY:
        if (ps->legsTimer < 300) {
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

bool BG_StabDownAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_STABDOWN:
    case animNumber_t::BOTH_STABDOWN_STAFF:
    case animNumber_t::BOTH_STABDOWN_DUAL:
        return true;
    default:
        break;
    }
    return false;
}

saberMoveName_t PM_SaberBounceForAttack(saberMoveName_t move)
{
    switch (saberMoveData[move].startQuad) {
    case saberQuadrant_t::Q_B:
    case saberQuadrant_t::Q_BR:
        return saberMoveName_t::LS_B1_BR;
        break;
    case saberQuadrant_t::Q_R:
        return saberMoveName_t::LS_B1__R;
        break;
    case saberQuadrant_t::Q_TR:
        return saberMoveName_t::LS_B1_TR;
        break;
    case saberQuadrant_t::Q_T:
        return saberMoveName_t::LS_B1_T_;
        break;
    case saberQuadrant_t::Q_TL:
        return saberMoveName_t::LS_B1_TL;
        break;
    case saberQuadrant_t::Q_L:
        return saberMoveName_t::LS_B1__L;
        break;
    case saberQuadrant_t::Q_BL:
        return saberMoveName_t::LS_B1_BL;
        break;
    default:
        break;
    }
    return saberMoveName_t::LS_NONE;
}

saberMoveName_t PM_SaberDeflectionForQuad(saberQuadrant_t quad)
{
    switch (quad) {
    case saberQuadrant_t::Q_B:
        return saberMoveName_t::LS_D1_B_;
        break;
    case saberQuadrant_t::Q_BR:
        return saberMoveName_t::LS_D1_BR;
        break;
    case saberQuadrant_t::Q_R:
        return saberMoveName_t::LS_D1__R;
        break;
    case saberQuadrant_t::Q_TR:
        return saberMoveName_t::LS_D1_TR;
        break;
    case saberQuadrant_t::Q_T:
        return saberMoveName_t::LS_D1_T_;
        break;
    case saberQuadrant_t::Q_TL:
        return saberMoveName_t::LS_D1_TL;
        break;
    case saberQuadrant_t::Q_L:
        return saberMoveName_t::LS_D1__L;
        break;
    case saberQuadrant_t::Q_BL:
        return saberMoveName_t::LS_D1_BL;
        break;
    default:
        break;
    }
    return saberMoveName_t::LS_NONE;
}

bool PM_SaberInDeflect(saberMoveName_t move)
{
    if (move >= saberMoveName_t::LS_D1_BR && move <= saberMoveName_t::LS_D1_B_) {
        return true;
    }
    return false;
}

bool PM_SaberInParry(saberMoveName_t move)
{
    if (move >= saberMoveName_t::LS_PARRY_UP && move <= saberMoveName_t::LS_PARRY_LL) {
        return true;
    }
    return false;
}

bool PM_SaberInKnockaway(saberMoveName_t move)
{
    if (move >= saberMoveName_t::LS_K1_T_ && move <= saberMoveName_t::LS_K1_BL) {
        return true;
    }
    return false;
}

bool PM_SaberInReflect(saberMoveName_t move)
{
    if (move >= saberMoveName_t::LS_REFLECT_UP && move <= saberMoveName_t::LS_REFLECT_LL) {
        return true;
    }
    return false;
}

bool PM_SaberInStart(saberMoveName_t move)
{
    if (move >= saberMoveName_t::LS_S_TL2BR && move <= saberMoveName_t::LS_S_T2B) {
        return true;
    }
    return false;
}

bool PM_SaberInReturn(saberMoveName_t move)
{
    if (move >= saberMoveName_t::LS_R_TL2BR && move <= saberMoveName_t::LS_R_T2B) {
        return true;
    }
    return false;
}

bool BG_SaberInReturn(saberMoveName_t move)
{
    return PM_SaberInReturn(move);
}

bool PM_InSaberAnim(animNumber_t anim)
{
    if ((anim) >= animNumber_t::BOTH_A1_T__B_ && (anim) <= animNumber_t::BOTH_H1_S1_BR) {
        return true;
    }
    return false;
}

bool PM_InKnockDown(playerState_t* ps)
{
    switch ((ps->legsAnim)) {
    case animNumber_t::BOTH_KNOCKDOWN1:
    case animNumber_t::BOTH_KNOCKDOWN2:
    case animNumber_t::BOTH_KNOCKDOWN3:
    case animNumber_t::BOTH_KNOCKDOWN4:
    case animNumber_t::BOTH_KNOCKDOWN5:
        return true;
        break;
    case animNumber_t::BOTH_GETUP1:
    case animNumber_t::BOTH_GETUP2:
    case animNumber_t::BOTH_GETUP3:
    case animNumber_t::BOTH_GETUP4:
    case animNumber_t::BOTH_GETUP5:
    case animNumber_t::BOTH_FORCE_GETUP_F1:
    case animNumber_t::BOTH_FORCE_GETUP_F2:
    case animNumber_t::BOTH_FORCE_GETUP_B1:
    case animNumber_t::BOTH_FORCE_GETUP_B2:
    case animNumber_t::BOTH_FORCE_GETUP_B3:
    case animNumber_t::BOTH_FORCE_GETUP_B4:
    case animNumber_t::BOTH_FORCE_GETUP_B5:
    case animNumber_t::BOTH_GETUP_BROLL_B:
    case animNumber_t::BOTH_GETUP_BROLL_F:
    case animNumber_t::BOTH_GETUP_BROLL_L:
    case animNumber_t::BOTH_GETUP_BROLL_R:
    case animNumber_t::BOTH_GETUP_FROLL_B:
    case animNumber_t::BOTH_GETUP_FROLL_F:
    case animNumber_t::BOTH_GETUP_FROLL_L:
    case animNumber_t::BOTH_GETUP_FROLL_R:
        if (ps->legsTimer) {
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

bool PM_PainAnim(animNumber_t anim)
{
    switch ((anim)) {
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
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool PM_JumpingAnim(animNumber_t anim)
{
    switch ((anim)) {
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
    switch ((anim)) {
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
    return BG_SpinningSaberAnim(anim);
}

bool PM_InOnGroundAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_DEAD1:
    case animNumber_t::BOTH_DEAD2:
    case animNumber_t::BOTH_DEAD3:
    case animNumber_t::BOTH_DEAD4:
    case animNumber_t::BOTH_DEAD5:
    case animNumber_t::BOTH_DEADFORWARD1:
    case animNumber_t::BOTH_DEADBACKWARD1:
    case animNumber_t::BOTH_DEADFORWARD2:
    case animNumber_t::BOTH_DEADBACKWARD2:
    case animNumber_t::BOTH_LYINGDEATH1:
    case animNumber_t::BOTH_LYINGDEAD1:
    case animNumber_t::BOTH_SLEEP1:     // # laying on back-rknee up-rhand on torso
    case animNumber_t::BOTH_KNOCKDOWN1: // #
    case animNumber_t::BOTH_KNOCKDOWN2: // #
    case animNumber_t::BOTH_KNOCKDOWN3: // #
    case animNumber_t::BOTH_KNOCKDOWN4: // #
    case animNumber_t::BOTH_KNOCKDOWN5: // #
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
    case animNumber_t::BOTH_GETUP_BROLL_B:
    case animNumber_t::BOTH_GETUP_BROLL_F:
    case animNumber_t::BOTH_GETUP_BROLL_L:
    case animNumber_t::BOTH_GETUP_BROLL_R:
    case animNumber_t::BOTH_GETUP_FROLL_B:
    case animNumber_t::BOTH_GETUP_FROLL_F:
    case animNumber_t::BOTH_GETUP_FROLL_L:
    case animNumber_t::BOTH_GETUP_FROLL_R:
        return true;
        break;
    default:
        break;
    }

    return false;
}

bool PM_InRollComplete(playerState_t* ps, animNumber_t anim)
{
    switch ((anim)) {
    case animNumber_t::BOTH_ROLL_F:
    case animNumber_t::BOTH_ROLL_B:
    case animNumber_t::BOTH_ROLL_R:
    case animNumber_t::BOTH_ROLL_L:
        if (ps->legsTimer < 1) {
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

bool PM_CanRollFromSoulCal(playerState_t* ps)
{
    if (ps->legsAnim == animNumber_t::BOTH_A7_SOULCAL && ps->legsTimer < 700 && ps->legsTimer > 250) {
        return true;
    }
    return false;
}

bool BG_SuperBreakLoseAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_LK_S_DL_S_SB_1_L:  // super break I lost
    case animNumber_t::BOTH_LK_S_DL_T_SB_1_L:  // super break I lost
    case animNumber_t::BOTH_LK_S_ST_S_SB_1_L:  // super break I lost
    case animNumber_t::BOTH_LK_S_ST_T_SB_1_L:  // super break I lost
    case animNumber_t::BOTH_LK_S_S_S_SB_1_L:   // super break I lost
    case animNumber_t::BOTH_LK_S_S_T_SB_1_L:   // super break I lost
    case animNumber_t::BOTH_LK_DL_DL_S_SB_1_L: // super break I lost
    case animNumber_t::BOTH_LK_DL_DL_T_SB_1_L: // super break I lost
    case animNumber_t::BOTH_LK_DL_ST_S_SB_1_L: // super break I lost
    case animNumber_t::BOTH_LK_DL_ST_T_SB_1_L: // super break I lost
    case animNumber_t::BOTH_LK_DL_S_S_SB_1_L:  // super break I lost
    case animNumber_t::BOTH_LK_DL_S_T_SB_1_L:  // super break I lost
    case animNumber_t::BOTH_LK_ST_DL_S_SB_1_L: // super break I lost
    case animNumber_t::BOTH_LK_ST_DL_T_SB_1_L: // super break I lost
    case animNumber_t::BOTH_LK_ST_ST_S_SB_1_L: // super break I lost
    case animNumber_t::BOTH_LK_ST_ST_T_SB_1_L: // super break I lost
    case animNumber_t::BOTH_LK_ST_S_S_SB_1_L:  // super break I lost
    case animNumber_t::BOTH_LK_ST_S_T_SB_1_L:  // super break I lost
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool BG_SuperBreakWinAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_LK_S_DL_S_SB_1_W:  // super break I won
    case animNumber_t::BOTH_LK_S_DL_T_SB_1_W:  // super break I won
    case animNumber_t::BOTH_LK_S_ST_S_SB_1_W:  // super break I won
    case animNumber_t::BOTH_LK_S_ST_T_SB_1_W:  // super break I won
    case animNumber_t::BOTH_LK_S_S_S_SB_1_W:   // super break I won
    case animNumber_t::BOTH_LK_S_S_T_SB_1_W:   // super break I won
    case animNumber_t::BOTH_LK_DL_DL_S_SB_1_W: // super break I won
    case animNumber_t::BOTH_LK_DL_DL_T_SB_1_W: // super break I won
    case animNumber_t::BOTH_LK_DL_ST_S_SB_1_W: // super break I won
    case animNumber_t::BOTH_LK_DL_ST_T_SB_1_W: // super break I won
    case animNumber_t::BOTH_LK_DL_S_S_SB_1_W:  // super break I won
    case animNumber_t::BOTH_LK_DL_S_T_SB_1_W:  // super break I won
    case animNumber_t::BOTH_LK_ST_DL_S_SB_1_W: // super break I won
    case animNumber_t::BOTH_LK_ST_DL_T_SB_1_W: // super break I won
    case animNumber_t::BOTH_LK_ST_ST_S_SB_1_W: // super break I won
    case animNumber_t::BOTH_LK_ST_ST_T_SB_1_W: // super break I won
    case animNumber_t::BOTH_LK_ST_S_S_SB_1_W:  // super break I won
    case animNumber_t::BOTH_LK_ST_S_T_SB_1_W:  // super break I won
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool BG_SaberLockBreakAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_BF1BREAK:
    case animNumber_t::BOTH_BF2BREAK:
    case animNumber_t::BOTH_CWCIRCLEBREAK:
    case animNumber_t::BOTH_CCWCIRCLEBREAK:
    case animNumber_t::BOTH_LK_S_DL_S_B_1_L:  // normal break I lost
    case animNumber_t::BOTH_LK_S_DL_S_B_1_W:  // normal break I won
    case animNumber_t::BOTH_LK_S_DL_T_B_1_L:  // normal break I lost
    case animNumber_t::BOTH_LK_S_DL_T_B_1_W:  // normal break I won
    case animNumber_t::BOTH_LK_S_ST_S_B_1_L:  // normal break I lost
    case animNumber_t::BOTH_LK_S_ST_S_B_1_W:  // normal break I won
    case animNumber_t::BOTH_LK_S_ST_T_B_1_L:  // normal break I lost
    case animNumber_t::BOTH_LK_S_ST_T_B_1_W:  // normal break I won
    case animNumber_t::BOTH_LK_S_S_S_B_1_L:   // normal break I lost
    case animNumber_t::BOTH_LK_S_S_S_B_1_W:   // normal break I won
    case animNumber_t::BOTH_LK_S_S_T_B_1_L:   // normal break I lost
    case animNumber_t::BOTH_LK_S_S_T_B_1_W:   // normal break I won
    case animNumber_t::BOTH_LK_DL_DL_S_B_1_L: // normal break I lost
    case animNumber_t::BOTH_LK_DL_DL_S_B_1_W: // normal break I won
    case animNumber_t::BOTH_LK_DL_DL_T_B_1_L: // normal break I lost
    case animNumber_t::BOTH_LK_DL_DL_T_B_1_W: // normal break I won
    case animNumber_t::BOTH_LK_DL_ST_S_B_1_L: // normal break I lost
    case animNumber_t::BOTH_LK_DL_ST_S_B_1_W: // normal break I won
    case animNumber_t::BOTH_LK_DL_ST_T_B_1_L: // normal break I lost
    case animNumber_t::BOTH_LK_DL_ST_T_B_1_W: // normal break I won
    case animNumber_t::BOTH_LK_DL_S_S_B_1_L:  // normal break I lost
    case animNumber_t::BOTH_LK_DL_S_S_B_1_W:  // normal break I won
    case animNumber_t::BOTH_LK_DL_S_T_B_1_L:  // normal break I lost
    case animNumber_t::BOTH_LK_DL_S_T_B_1_W:  // normal break I won
    case animNumber_t::BOTH_LK_ST_DL_S_B_1_L: // normal break I lost
    case animNumber_t::BOTH_LK_ST_DL_S_B_1_W: // normal break I won
    case animNumber_t::BOTH_LK_ST_DL_T_B_1_L: // normal break I lost
    case animNumber_t::BOTH_LK_ST_DL_T_B_1_W: // normal break I won
    case animNumber_t::BOTH_LK_ST_ST_S_B_1_L: // normal break I lost
    case animNumber_t::BOTH_LK_ST_ST_S_B_1_W: // normal break I won
    case animNumber_t::BOTH_LK_ST_ST_T_B_1_L: // normal break I lost
    case animNumber_t::BOTH_LK_ST_ST_T_B_1_W: // normal break I won
    case animNumber_t::BOTH_LK_ST_S_S_B_1_L:  // normal break I lost
    case animNumber_t::BOTH_LK_ST_S_S_B_1_W:  // normal break I won
    case animNumber_t::BOTH_LK_ST_S_T_B_1_L:  // normal break I lost
    case animNumber_t::BOTH_LK_ST_S_T_B_1_W:  // normal break I won
        return true;
        break;
    default:
        break;
    }
    return (BG_SuperBreakLoseAnim(anim) || BG_SuperBreakWinAnim(anim));
}

bool BG_FullBodyTauntAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_GESTURE1:
    case animNumber_t::BOTH_DUAL_TAUNT:
    case animNumber_t::BOTH_STAFF_TAUNT:
    case animNumber_t::BOTH_BOW:
    case animNumber_t::BOTH_MEDITATE:
    case animNumber_t::BOTH_SHOWOFF_FAST:
    case animNumber_t::BOTH_SHOWOFF_MEDIUM:
    case animNumber_t::BOTH_SHOWOFF_STRONG:
    case animNumber_t::BOTH_SHOWOFF_DUAL:
    case animNumber_t::BOTH_SHOWOFF_STAFF:
    case animNumber_t::BOTH_VICTORY_FAST:
    case animNumber_t::BOTH_VICTORY_MEDIUM:
    case animNumber_t::BOTH_VICTORY_STRONG:
    case animNumber_t::BOTH_VICTORY_DUAL:
    case animNumber_t::BOTH_VICTORY_STAFF:
        return true;
        break;
    default:
        break;
    }
    return false;
}

/*
=============
BG_AnimLength

Get the "length" of an anim given the local anim index (which skeleton)
and anim number. Obviously does not take things like the length of the
anim while force speeding (as an example) and whatnot into account.
=============
*/
int BG_AnimLength(int index, animNumber_t anim)
{
    if (anim < animNumber_t::FACE_TALK0 || anim >= animNumber_t::MAX_ANIMATIONS) {
        return 0;
    }

    return (*bgAllAnims[index].anims)[anim].numFrames * Q_fabs((float)((*bgAllAnims[index].anims)[anim].frameLerp));
}

// just use whatever pm->animations is
int PM_AnimLength(int index, animNumber_t anim)
{
    if (!pm->animations || anim < animNumber_t::FACE_TALK0 || anim >= animNumber_t::MAX_ANIMATIONS) {
        return 0;
    }

    return (*pm->animations)[anim].numFrames * Q_fabs((float)((*pm->animations)[anim].frameLerp));
}

void PM_DebugLegsAnim(animNumber_t anim)
{
    animNumber_t oldAnim = (pm->ps->legsAnim);
    animNumber_t newAnim = (anim);

    if (oldAnim < animNumber_t::MAX_TOTALANIMATIONS && oldAnim >= animNumber_t::BOTH_DEATH1 && newAnim < animNumber_t::MAX_TOTALANIMATIONS &&
        newAnim >= animNumber_t::BOTH_DEATH1) {
        Com_Printf("OLD: %s\n", BG_AnimName(oldAnim));
        Com_Printf("NEW: %s\n", BG_AnimName(newAnim));
    }
}

bool PM_SaberInTransition(saberMoveName_t move)
{
    if (move >= saberMoveName_t::LS_T1_BR__R && move <= saberMoveName_t::LS_T1_BL__L) {
        return true;
    }
    return false;
}

bool BG_SaberInTransitionAny(saberMoveName_t move)
{
    if (PM_SaberInStart(move)) {
        return true;
    }
    else if (PM_SaberInTransition(move)) {
        return true;
    }
    else if (PM_SaberInReturn(move)) {
        return true;
    }
    return false;
}

/*
==============================================================================
END: Animation utility functions (sequence checking)
==============================================================================
*/

void BG_FlipPart(playerState_t* ps, setAnimParts_t part)
{
    if (part == setAnimParts_t::SETANIM_TORSO) {
        if (ps->torsoFlip) {
            ps->torsoFlip = false;
        }
        else {
            ps->torsoFlip = true;
        }
    }
    else if (part == setAnimParts_t::SETANIM_LEGS) {
        if (ps->legsFlip) {
            ps->legsFlip = false;
        }
        else {
            ps->legsFlip = true;
        }
    }
}

bool BGPAFtextLoaded = false;
animSet_t bgHumanoidAnimations; // humanoid animations are the only ones that are statically allocated.

bgLoadedAnim_t bgAllAnims[MAX_ANIM_FILES];
int bgNumAllAnims = 2; // start off at 2, because 0 will always be assigned to humanoid, and 1 will always be rockettrooper

// ALWAYS call on game/cgame init
void BG_InitAnimsets(void)
{
    memset(&bgAllAnims, 0, sizeof(bgAllAnims));
    BGPAFtextLoaded = false; // VVFIXME - The PC doesn't seem to need this, but why?
}

// ALWAYS call on game/cgame shutdown
void BG_ClearAnimsets(void)
{
    /*
    int i = 1;

    while (i < bgNumAllAnims)
    {
        if (bgAllAnims[i].anims)
        {
            strap_TrueFree((void **)&bgAllAnims[i].anims);
        }
        i++;
    }
    */
}

animSet_t* BG_AnimsetAlloc(void)
{
    assert(bgNumAllAnims < MAX_ANIM_FILES);
    bgAllAnims[bgNumAllAnims].anims = (animSet_t*)BG_Alloc(sizeof(animSet_t));

    return bgAllAnims[bgNumAllAnims].anims;
}

void BG_AnimsetFree(animSet_t* animset)
{
    /*
    if (!animset)
    {
        return;
    }

    strap_TrueFree((void **)&animset);

#ifdef _DEBUG
    if (animset)
    {
        assert(!"Failed to free anim set");
    }
#endif
    */
}

#ifdef _CGAME // none of this is actually needed serverside. Could just be moved to cgame code but it's here since it used to tie in a lot with the anim loading
              // stuff.
stringID_table_t animEventTypeTable[MAX_ANIM_EVENTS + 1] = {
    ENUM2STRING(animEventType_t, AEV_SOUND),       // # animID AEV_SOUND framenum soundpath randomlow randomhi chancetoplay
    ENUM2STRING(animEventType_t, AEV_FOOTSTEP),    // # animID AEV_FOOTSTEP framenum footstepType
    ENUM2STRING(animEventType_t, AEV_EFFECT),      // # animID AEV_EFFECT framenum effectpath boltName
    ENUM2STRING(animEventType_t, AEV_FIRE),        // # animID AEV_FIRE framenum altfire chancetofire
    ENUM2STRING(animEventType_t, AEV_MOVE),        // # animID AEV_MOVE framenum forwardpush rightpush uppush
    ENUM2STRING(animEventType_t, AEV_SOUNDCHAN),   // # animID AEV_SOUNDCHAN framenum CHANNEL soundpath randomlow randomhi chancetoplay
    ENUM2STRING(animEventType_t, AEV_SABER_SWING), // # animID AEV_SABER_SWING framenum CHANNEL randomlow randomhi chancetoplay
    ENUM2STRING(animEventType_t, AEV_SABER_SPIN),  // # animID AEV_SABER_SPIN framenum CHANNEL chancetoplay
    // must be terminated
    {nullptr, -1}};

stringID_table_t footstepTypeTable[to_underlying(footstepType_t::NUM_FOOTSTEP_TYPES) + 1] = {ENUM2STRING(footstepType_t, FOOTSTEP_R),
                                                                                             ENUM2STRING(footstepType_t, FOOTSTEP_L),
                                                                                             ENUM2STRING(footstepType_t, FOOTSTEP_HEAVY_R),
                                                                                             ENUM2STRING(footstepType_t, FOOTSTEP_HEAVY_L),
                                                                                             // must be terminated
                                                                                             {nullptr, -1}};

int CheckAnimFrameForEventType(animevent_t* animEvents, int keyFrame, animEventType_t eventType)
{
    int i;

    for (i = 0; i < MAX_ANIM_EVENTS; i++) {
        if (animEvents[i].keyFrame == keyFrame) {       // there is an animevent on this frame already
            if (animEvents[i].eventType == eventType) { // and it is of the same type
                return i;
            }
        }
    }
    // nope
    return -1;
}

void ParseAnimationEvtBlock(const char* aeb_filename, animevent_t* animEvents, animSet_t* animations, int* i, const char** text_p)
{
    const char* token;
    int num, n, keyFrame, lowestVal, highestVal, curAnimEvent, lastAnimEvent = 0;
    animEventType_t eventType;
    char stringData[MAX_QPATH];

    // get past starting bracket
    while (1) {
        token = COM_Parse(text_p);
        if (!Q_stricmp(token, "{")) {
            break;
        }
    }

    // NOTE: instead of a blind increment, increase the index
    //			this way if we have an event on an anim that already
    //			has an event of that type, it stomps it

    // read information for each frame
    while (1) {
        if (lastAnimEvent >= MAX_ANIM_EVENTS) {
            Com_Error(errorParm_t::ERR_DROP, "ParseAnimationEvtBlock: number events in animEvent file %s > MAX_ANIM_EVENTS(%i)", aeb_filename, MAX_ANIM_EVENTS);
            return;
        }
        // Get base frame of sequence
        token = COM_Parse(text_p);
        if (!token || !token[0]) {
            break;
        }

        if (!Q_stricmp(token, "}")) // At end of block
        {
            break;
        }

        // Compare to same table as animations used
        //	so we don't have to use actual numbers for animation first frames,
        //	just need offsets.
        // This way when animation numbers change, this table won't have to be updated,
        //	at least not much.
        const std::optional<animNumber_t> animNum = BG_AnimFromName(token);
        if (!animNum) { // Unrecognized ANIM ENUM name, or we're skipping this line, keep going till you get a good one
            Com_Printf(S_COLOR_YELLOW "WARNING: Unknown token %s in animEvent file %s\n", token, aeb_filename);
            while (token[0]) {
                token = COM_ParseExt(text_p, false); // returns empty string when next token is EOL
            }
            continue;
        }

        if ((*animations)[*animNum].numFrames == 0) { // we don't use this anim
            Com_Printf(S_COLOR_YELLOW "WARNING: %s animevents.cfg: anim %s not used by this model\n", aeb_filename, token);
            // skip this entry
            SkipRestOfLine(text_p);
            continue;
        }

        token = COM_Parse(text_p);
        const int eventId = GetIDForString(animEventTypeTable, token);
        eventType = static_cast<animEventType_t>(eventId);
        if (eventId == -1 ||
            eventType == animEventType_t::AEV_NONE) { // Unrecognized ANIM EVENT TYOE, or we're skipping this line, keep going till you get a good one
            // Com_Printf(S_COLOR_YELLOW"WARNING: Unknown token %s in animEvent file %s\n", token, aeb_filename );
            continue;
        }

        // set our start frame
        keyFrame = (*animations)[*animNum].firstFrame;
        // Get offset to frame within sequence
        token = COM_Parse(text_p);
        if (!token) {
            break;
        }
        keyFrame += atoi(token);

        // see if this frame already has an event of this type on it, if so, overwrite it
        curAnimEvent = CheckAnimFrameForEventType(animEvents, keyFrame, eventType);
        if (curAnimEvent == -1) { // this anim frame doesn't already have an event of this type on it
            curAnimEvent = lastAnimEvent;
        }

        // now that we know which event index we're going to plug the data into, start doing it
        animEvents[curAnimEvent].eventType = eventType;
        animEvents[curAnimEvent].keyFrame = keyFrame;

        // now read out the proper data based on the type
        switch (animEvents[curAnimEvent].eventType) {
        case animEventType_t::AEV_SOUNDCHAN: // # animID AEV_SOUNDCHAN framenum CHANNEL soundpath randomlow randomhi chancetoplay
            token = COM_Parse(text_p);
            if (!token)
                break;

            if (!Q_stricmp(token, "CHAN_VOICE_ATTEN"))
                animEvents[curAnimEvent].eventData[AED_SOUNDCHANNEL] = to_underlying(soundChannel_t::CHAN_VOICE_ATTEN);
            else if (!Q_stricmp(token, "CHAN_VOICE_GLOBAL"))
                animEvents[curAnimEvent].eventData[AED_SOUNDCHANNEL] = to_underlying(soundChannel_t::CHAN_VOICE_GLOBAL);
            else if (!Q_stricmp(token, "CHAN_ANNOUNCER"))
                animEvents[curAnimEvent].eventData[AED_SOUNDCHANNEL] = to_underlying(soundChannel_t::CHAN_ANNOUNCER);
            else if (!Q_stricmp(token, "CHAN_BODY"))
                animEvents[curAnimEvent].eventData[AED_SOUNDCHANNEL] = to_underlying(soundChannel_t::CHAN_BODY);
            else if (!Q_stricmp(token, "CHAN_WEAPON"))
                animEvents[curAnimEvent].eventData[AED_SOUNDCHANNEL] = to_underlying(soundChannel_t::CHAN_WEAPON);
            else if (!Q_stricmp(token, "CHAN_VOICE"))
                animEvents[curAnimEvent].eventData[AED_SOUNDCHANNEL] = to_underlying(soundChannel_t::CHAN_VOICE);
            else
                animEvents[curAnimEvent].eventData[AED_SOUNDCHANNEL] = to_underlying(soundChannel_t::CHAN_AUTO);

            // fall through to normal sound
        case animEventType_t::AEV_SOUND: // # animID AEV_SOUND framenum soundpath randomlow randomhi chancetoplay
            // get soundstring
            token = COM_Parse(text_p);
            if (!token) {
                break;
            }
            strcpy(stringData, token);
            // get lowest value
            token = COM_Parse(text_p);
            if (!token) { // WARNING!  BAD TABLE!
                break;
            }
            lowestVal = atoi(token);
            // get highest value
            token = COM_Parse(text_p);
            if (!token) { // WARNING!  BAD TABLE!
                break;
            }
            highestVal = atoi(token);
            // Now precache all the sounds
            // NOTE: If we can be assured sequential handles, we can store sound indices
            //		instead of strings, unfortunately, if these sounds were previously
            //		registered, we cannot be guaranteed sequential indices.  Thus an array
            if (lowestVal && highestVal) {
                // assert(highestVal - lowestVal < MAX_RANDOM_ANIM_SOUNDS);
                if ((highestVal - lowestVal) >= MAX_RANDOM_ANIM_SOUNDS) {
                    highestVal = lowestVal + (MAX_RANDOM_ANIM_SOUNDS - 1);
                }
                for (n = lowestVal, num = AED_SOUNDINDEX_START; n <= highestVal && num <= AED_SOUNDINDEX_END; n++, num++) {
                    if (stringData[0] == '*') { // FIXME? Would be nice to make custom sounds work with animEvents.
                        animEvents[curAnimEvent].eventData[num] = 0;
                    }
                    else {
                        animEvents[curAnimEvent].eventData[num] = trap->S_RegisterSound(va(stringData, n));
                    }
                }
                animEvents[curAnimEvent].eventData[AED_SOUND_NUMRANDOMSNDS] = num - 1;
            }
            else {
                if (stringData[0] == '*') { // FIXME? Would be nice to make custom sounds work with animEvents.
                    animEvents[curAnimEvent].eventData[AED_SOUNDINDEX_START] = 0;
                }
                else {
                    animEvents[curAnimEvent].eventData[AED_SOUNDINDEX_START] = trap->S_RegisterSound(stringData);
                }
#ifndef FINAL_BUILD
                if (!animEvents[curAnimEvent].eventData[AED_SOUNDINDEX_START] && stringData[0] != '*') { // couldn't register it - file not found
                    Com_Printf(S_COLOR_RED "ParseAnimationSndBlock: sound %s does not exist (animevents.cfg %s)!\n", stringData, aeb_filename);
                }
#endif
                animEvents[curAnimEvent].eventData[AED_SOUND_NUMRANDOMSNDS] = 0;
            }
            // get probability
            token = COM_Parse(text_p);
            if (!token) { // WARNING!  BAD TABLE!
                break;
            }
            animEvents[curAnimEvent].eventData[AED_SOUND_PROBABILITY] = atoi(token);

            // last part - cheat and check and see if it's a special overridable saber sound we know of...
            if (!Q_stricmpn("sound/weapons/saber/saberhup", stringData, 28)) { // a saber swing
                animEvents[curAnimEvent].eventType = animEventType_t::AEV_SABER_SWING;
                animEvents[curAnimEvent].eventData[AED_SABER_SWING_SABERNUM] =
                    0; // since we don't know which one they meant if we're hacking this, always use first saber
                animEvents[curAnimEvent].eventData[AED_SABER_SWING_PROBABILITY] = animEvents[curAnimEvent].eventData[AED_SOUND_PROBABILITY];
                if (lowestVal < 4) {                                              // fast swing
                    animEvents[curAnimEvent].eventData[AED_SABER_SWING_TYPE] = 0; // SWING_FAST;
                }
                else if (lowestVal < 7) {                                         // medium swing
                    animEvents[curAnimEvent].eventData[AED_SABER_SWING_TYPE] = 1; // SWING_MEDIUM;
                }
                else {                                                            // strong swing
                    animEvents[curAnimEvent].eventData[AED_SABER_SWING_TYPE] = 2; // SWING_STRONG;
                }
            }
            else if (!Q_stricmpn("sound/weapons/saber/saberspin", stringData, 29)) { // a saber spin
                animEvents[curAnimEvent].eventType = animEventType_t::AEV_SABER_SPIN;
                animEvents[curAnimEvent].eventData[AED_SABER_SPIN_SABERNUM] =
                    0; // since we don't know which one they meant if we're hacking this, always use first saber
                animEvents[curAnimEvent].eventData[AED_SABER_SPIN_PROBABILITY] = animEvents[curAnimEvent].eventData[AED_SOUND_PROBABILITY];
                if (stringData[29] == 'o') { // saberspinoff
                    animEvents[curAnimEvent].eventData[AED_SABER_SPIN_TYPE] = 0;
                }
                else if (stringData[29] == '1') { // saberspin1
                    animEvents[curAnimEvent].eventData[AED_SABER_SPIN_TYPE] = 2;
                }
                else if (stringData[29] == '2') { // saberspin2
                    animEvents[curAnimEvent].eventData[AED_SABER_SPIN_TYPE] = 3;
                }
                else if (stringData[29] == '3') { // saberspin3
                    animEvents[curAnimEvent].eventData[AED_SABER_SPIN_TYPE] = 4;
                }
                else if (stringData[29] == '%') { // saberspin%d
                    animEvents[curAnimEvent].eventData[AED_SABER_SPIN_TYPE] = 5;
                }
                else { // just plain saberspin
                    animEvents[curAnimEvent].eventData[AED_SABER_SPIN_TYPE] = 1;
                }
            }
            break;
        case animEventType_t::AEV_FOOTSTEP: // # animID AEV_FOOTSTEP framenum footstepType
            // get footstep type
            token = COM_Parse(text_p);
            if (!token) {
                break;
            }
            animEvents[curAnimEvent].eventData[AED_FOOTSTEP_TYPE] = GetIDForString(footstepTypeTable, token);
            // get probability
            token = COM_Parse(text_p);
            if (!token) { // WARNING!  BAD TABLE!
                break;
            }
            animEvents[curAnimEvent].eventData[AED_FOOTSTEP_PROBABILITY] = atoi(token);
            break;
        case animEventType_t::AEV_EFFECT: // # animID AEV_EFFECT framenum effectpath boltName
            // get effect index
            token = COM_Parse(text_p);
            if (!token) {
                break;
            }
            animEvents[curAnimEvent].eventData[AED_EFFECTINDEX] = trap->FX_RegisterEffect(token);
            // get bolt index
            token = COM_Parse(text_p);
            if (!token) {
                break;
            }
            if (Q_stricmp("none", token) != 0 && Q_stricmp("NULL", token) != 0) { // actually are specifying a bolt to use
                if (!animEvents[curAnimEvent].stringData) {                       // eh, whatever. no dynamic stuff, so this will do.
                    animEvents[curAnimEvent].stringData = (char*)BG_Alloc(2048);
                }
                strcpy(animEvents[curAnimEvent].stringData, token);
            }
            // NOTE: this string will later be used to add a bolt and store the index, as below:
            // animEvent->eventData[AED_BOLTINDEX] = trap->G2API_AddBolt( &cent->gent->ghoul2[cent->gent->playerModel], animEvent->stringData );
            // get probability
            token = COM_Parse(text_p);
            if (!token) { // WARNING!  BAD TABLE!
                break;
            }
            animEvents[curAnimEvent].eventData[AED_EFFECT_PROBABILITY] = atoi(token);
            break;
        case animEventType_t::AEV_FIRE: // # animID AEV_FIRE framenum altfire chancetofire
            // get altfire
            token = COM_Parse(text_p);
            if (!token) { // WARNING!  BAD TABLE!
                break;
            }
            animEvents[curAnimEvent].eventData[AED_FIRE_ALT] = atoi(token);
            // get probability
            token = COM_Parse(text_p);
            if (!token) { // WARNING!  BAD TABLE!
                break;
            }
            animEvents[curAnimEvent].eventData[AED_FIRE_PROBABILITY] = atoi(token);
            break;
        case animEventType_t::AEV_MOVE: // # animID AEV_MOVE framenum forwardpush rightpush uppush
            // get forward push
            token = COM_Parse(text_p);
            if (!token) { // WARNING!  BAD TABLE!
                break;
            }
            animEvents[curAnimEvent].eventData[AED_MOVE_FWD] = atoi(token);
            // get right push
            token = COM_Parse(text_p);
            if (!token) { // WARNING!  BAD TABLE!
                break;
            }
            animEvents[curAnimEvent].eventData[AED_MOVE_RT] = atoi(token);
            // get upwards push
            token = COM_Parse(text_p);
            if (!token) { // WARNING!  BAD TABLE!
                break;
            }
            animEvents[curAnimEvent].eventData[AED_MOVE_UP] = atoi(token);
            break;
        default: // unknown?
            SkipRestOfLine(text_p);
            continue;
            break;
        }

        if (curAnimEvent == lastAnimEvent) {
            lastAnimEvent++;
        }
    }
}

/*
======================
BG_ParseAnimationEvtFile

Read a configuration file containing animation events
models/players/kyle/animevents.cfg, etc

This file's presence is not required

======================
*/
bgLoadedEvents_t bgAllEvents[MAX_ANIM_FILES];
int bgNumAnimEvents = 1;
static int bg_animParseIncluding = 0;
int BG_ParseAnimationEvtFile(const char* as_filename, int animFileIndex, int eventFileIndex)
{
    const char* text_p;
    int len;
    const char* token;
    char text[80000];
    char sfilename[MAX_QPATH];
    fileHandle_t f;
    int i, j, upper_i, lower_i;
    int usedIndex = -1;
    animevent_t* legsAnimEvents;
    animevent_t* torsoAnimEvents;
    animSet_t* animations;
    int forcedIndex;

    assert(animFileIndex < MAX_ANIM_FILES);
    assert(eventFileIndex < MAX_ANIM_FILES);

    if (eventFileIndex == -1) {
        forcedIndex = 0;
    }
    else {
        forcedIndex = eventFileIndex;
    }

    if (bg_animParseIncluding <= 0) {                // if we should be parsing an included file, skip this part
        if (bgAllEvents[forcedIndex].eventsParsed) { // already cached this one
            return forcedIndex;
        }
    }

    legsAnimEvents = bgAllEvents[forcedIndex].legsAnimEvents;
    torsoAnimEvents = bgAllEvents[forcedIndex].torsoAnimEvents;
    animations = bgAllAnims[animFileIndex].anims;

    if (bg_animParseIncluding <= 0) { // if we should be parsing an included file, skip this part
        // Go through and see if this filename is already in the table.
        i = 0;
        while (i < bgNumAnimEvents && forcedIndex != 0) {
            if (!Q_stricmp(as_filename, bgAllEvents[i].filename)) { // looks like we have it already.
                return i;
            }
            i++;
        }
    }

    // Load and parse animevents.cfg file
    Com_sprintf(sfilename, sizeof(sfilename), "%sanimevents.cfg", as_filename);

    if (bg_animParseIncluding <= 0) { // should already be done if we're including
        // initialize anim event array
        for (i = 0; i < MAX_ANIM_EVENTS; i++) {
            // Type of event
            torsoAnimEvents[i].eventType = animEventType_t::AEV_NONE;
            legsAnimEvents[i].eventType = animEventType_t::AEV_NONE;
            // Frame to play event on
            torsoAnimEvents[i].keyFrame = -1;
            legsAnimEvents[i].keyFrame = -1;
            // we allow storage of one string, temporarily (in case we have to look up an index later, then make sure to set stringData to nullptr so we only do
            // the look-up once)
            torsoAnimEvents[i].stringData = nullptr;
            legsAnimEvents[i].stringData = nullptr;
            // Unique IDs, can be soundIndex of sound file to play OR effect index or footstep type, etc.
            for (j = 0; j < AED_ARRAY_SIZE; j++) {
                torsoAnimEvents[i].eventData[j] = -1;
                legsAnimEvents[i].eventData[j] = -1;
            }
        }
    }

    // load the file
    len = trap->FS_Open(sfilename, &f, fsMode_t::FS_READ);
    if (len <= 0) { // no file
        goto fin;
    }
    if (len >= std::ssize(text) - 1) {
        trap->FS_Close(f);
#ifndef FINAL_BUILD
        Com_Error(errorParm_t::ERR_DROP, "File %s too long\n", sfilename);
#else
        Com_Printf("File %s too long\n", sfilename);
#endif
        goto fin;
    }

    trap->FS_Read(text, len, f);
    text[len] = 0;
    trap->FS_Close(f);

    // parse the text
    text_p = text;
    upper_i = 0;
    lower_i = 0;

    COM_BeginParseSession("BG_ParseAnimationEvtFile");

    // read information for batches of sounds (UPPER or LOWER)
    while (1) {
        // Get base frame of sequence
        token = COM_Parse(&text_p);
        if (!token || !token[0]) {
            break;
        }

        if (!Q_stricmp(token, "include")) // grab from another animevents.cfg
        {                                 // NOTE: you REALLY should NOT do this after the main block of UPPERSOUNDS and LOWERSOUNDS
            const char* include_filename = COM_Parse(&text_p);
            if (include_filename != nullptr) {
                char fullIPath[MAX_QPATH];
                strcpy(fullIPath, va("models/players/%s/", include_filename));
                bg_animParseIncluding++;
                BG_ParseAnimationEvtFile(fullIPath, animFileIndex, forcedIndex);
                bg_animParseIncluding--;
            }
        }

        if (!Q_stricmp(token, "UPPEREVENTS")) // A batch of upper sounds
        {
            ParseAnimationEvtBlock(as_filename, torsoAnimEvents, animations, &upper_i, &text_p);
        }

        else if (!Q_stricmp(token, "LOWEREVENTS")) // A batch of lower sounds
        {
            ParseAnimationEvtBlock(as_filename, legsAnimEvents, animations, &lower_i, &text_p);
        }
    }

    usedIndex = forcedIndex;
fin:
    // Mark this anim set so that we know we tried to load he sounds, don't care if the load failed
    if (bg_animParseIncluding <= 0) { // if we should be parsing an included file, skip this part
        bgAllEvents[forcedIndex].eventsParsed = true;
        strcpy(bgAllEvents[forcedIndex].filename, as_filename);
        if (forcedIndex) {
            bgNumAnimEvents++;
        }
    }

    return usedIndex;
}
#endif

/*
======================
BG_ParseAnimationFile

Read a configuration file containing animation counts and rates
models/players/visor/animation.cfg, etc

======================
*/
int BG_ParseAnimationFile(const char* filename, animSet_t* animset, bool isHumanoid)
{
    char* text_p;
    int len;
    int i;
    char* token;
    float fps;
    int usedIndex = -1;
    int nextIndex = bgNumAllAnims;
    bool dynAlloc = false;
    /// bool	wasLoaded = false;
    static char BGPAFtext[60000];
    fileHandle_t f;

    BGPAFtext[0] = '\0';

    if (!isHumanoid) {
        i = 0;
        while (i < bgNumAllAnims) { // see if it's been loaded already
            if (!Q_stricmp(bgAllAnims[i].filename, filename)) {
                animset = bgAllAnims[i].anims;
                return i; // alright, we already have it.
            }
            i++;
        }

        // Looks like it has not yet been loaded. Allocate space for the anim set if we need to, and continue along.
        if (!animset) {
            if (strstr(filename, "players/_humanoid/")) { // then use the static humanoid set.
                animset = &bgHumanoidAnimations;
                nextIndex = 0;
            }
            else if (strstr(filename, "players/rockettrooper/")) { // rockettrooper always index 1
                nextIndex = 1;
                animset = BG_AnimsetAlloc();
                dynAlloc = true; // so we know to free this memory in case we have to return early. Don't want any leaks.

                if (!animset) {
                    assert(!"Anim set alloc failed!");
                    return -1;
                }
            }
            else {
                animset = BG_AnimsetAlloc();
                dynAlloc = true; // so we know to free this memory in case we have to return early. Don't want any leaks.

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
    if (!BGPAFtextLoaded || !isHumanoid) { // rww - We are always using the same animation config now. So only load it once.
        len = trap->FS_Open(filename, &f, fsMode_t::FS_READ);
        if ((len <= 0) || (len >= std::ssize(BGPAFtext) - 1)) {
            trap->FS_Close(f);
            if (dynAlloc) {
                BG_AnimsetFree(animset);
            }
            if (len > 0) {
                Com_Error(errorParm_t::ERR_DROP, "%s exceeds the allowed game-side animation buffer!", filename);
            }
            return -1;
        }

        trap->FS_Read(BGPAFtext, len, f);

        BGPAFtext[len] = 0;
        trap->FS_Close(f);
    }
    else {
        if (dynAlloc) {
            assert(!"Should not have allocated dynamically for humanoid");
            BG_AnimsetFree(animset);
        }
        return 0; // humanoid index
    }

    // parse the text
    text_p = BGPAFtext;

    // FIXME: have some way of playing anims backwards... negative numFrames?

    // initialize anim array so that from 0 to MAX_ANIMATIONS, set default values of 0 1 0 100
    for (animNumber_t anim = animNumber_t::FACE_TALK0; anim < animNumber_t::MAX_ANIMATIONS; anim++) {
        (*animset)[anim].firstFrame = 0;
        (*animset)[anim].numFrames = 0;
        (*animset)[anim].loopFrames = -1;
        (*animset)[anim].frameLerp = 100;
    }

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
            if (strcmp(token, "ROOT")) {
                Com_Printf(S_COLOR_RED "WARNING: Unknown token %s in %s\n", token, filename);
            }
            while (token[0]) {
                token = COM_ParseExt((const char**)&text_p, false); // returns empty string when next token is EOL
            }
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
    }
/*
#ifdef _DEBUG
    //Check the array, and print the ones that have nothing in them.
    for(animNumber_t anim = animNumber_t::FACE_TALK0; anim < animNumber_t::MAX_ANIMATIONS; anim++)
    {
        if (BG_AnimName( anim ) != nullptr)		// This animation reference exists.
        {
            if ((*animset)[anim].firstFrame <= 0 && (*animset)[anim].numFrames <=0)
            {	// This is an empty animation reference.
                Com_Printf("***ANIMTABLE reference #%d (%s) is empty!\n", to_underlying( anim ), BG_AnimName( anim ));
            }
        }
    }
#endif // _DEBUG
*/
#ifdef CONVENIENT_ANIMATION_FILE_DEBUG_THING
    SpewDebugStuffToFile();
#endif

    //	wasLoaded = BGPAFtextLoaded;

    if (isHumanoid) {
        bgAllAnims[0].anims = animset;
        strcpy(bgAllAnims[0].filename, filename);
        BGPAFtextLoaded = true;

        usedIndex = 0;
    }
    else {
        bgAllAnims[nextIndex].anims = animset;
        strcpy(bgAllAnims[nextIndex].filename, filename);

        usedIndex = bgNumAllAnims;

        if (nextIndex > 1) { // don't bother increasing the number if this ended up as a humanoid/rockettrooper load.
            bgNumAllAnims++;
        }
        else {
            BGPAFtextLoaded = true;
            usedIndex = nextIndex;
        }
    }

    /*
    if (!wasLoaded && BGPAFtextLoaded)
    { //just loaded humanoid skel - we always want the rockettrooper to be after it, in slot 1
#ifdef _DEBUG
        assert(BG_ParseAnimationFile("models/players/rockettrooper/animation.cfg", nullptr, false) == 1);
#else
        BG_ParseAnimationFile("models/players/rockettrooper/animation.cfg", nullptr, false);
#endif
    }
    */

    return usedIndex;
}

/*
===================
LEGS Animations
Base animation for overall body
===================
*/
static void BG_StartLegsAnim(playerState_t* ps, animNumber_t anim)
{
    if (ps->pm_type >= pmtype_t::PM_DEAD) {
        // vehicles are allowed to do this.. IF it's a vehicle death anim
        if (ps->clientNum < MAX_CLIENTS || anim != animNumber_t::BOTH_VT_DEATH1) {
            return;
        }
    }
    if (ps->legsTimer > 0) {
        return; // a high priority animation is running
    }

    if (ps->legsAnim == anim) {
        BG_FlipPart(ps, setAnimParts_t::SETANIM_LEGS);
    }
#ifdef _GAME
    else if (ES_LegsAnim(g_entities[ps->clientNum].s) == anim) { // toggled anim to one anim then back to the one we were at previously in
        // one frame, indicating that anim should be restarted.
        BG_FlipPart(ps, setAnimParts_t::SETANIM_LEGS);
    }
#elif defined(_CGAME)
    else if (ps == pm_startPs &&
             pm_startLegsAnim == anim) { // the server's rule above, in the prediction: its entity state holds the animation its last frame ended with, which
        // is the one the predicted command starts from. Without it the client restarted the animation once less than the
        // server within a command that leaves it and takes it again (zoomed with the disruptor: at every tick)
        BG_FlipPart(ps, setAnimParts_t::SETANIM_LEGS);
    }
#endif
    ps->legsAnim = anim;

    /*
    if ( pm->debugLevel ) {
        Com_Printf("%d:  StartLegsAnim %d, on client#%d\n", pm->cmd.serverTime, anim, pm->ps->clientNum);
    }
    */
}

void PM_ContinueLegsAnim(animNumber_t anim)
{
    if ((pm->ps->legsAnim) == anim) {
        return;
    }
    if (pm->ps->legsTimer > 0) {
        return; // a high priority animation is running
    }

    BG_StartLegsAnim(pm->ps, anim);
}

void PM_ForceLegsAnim(animNumber_t anim)
{
    if (BG_InSpecialJump(pm->ps->legsAnim) && pm->ps->legsTimer > 0 && !BG_InSpecialJump(anim)) {
        return;
    }

    if (BG_InRoll(pm->ps, pm->ps->legsAnim) && pm->ps->legsTimer > 0 && !BG_InRoll(pm->ps, anim)) {
        return;
    }

    pm->ps->legsTimer = 0;
    BG_StartLegsAnim(pm->ps, anim);
}

/*
===================
TORSO Animations
Override animations for upper body
===================
*/
void BG_StartTorsoAnim(playerState_t* ps, animNumber_t anim)
{
    if (ps->pm_type >= pmtype_t::PM_DEAD) {
        return;
    }

    if (ps->torsoAnim == anim) {
        BG_FlipPart(ps, setAnimParts_t::SETANIM_TORSO);
    }
#ifdef _GAME
    else if (ES_TorsoAnim(g_entities[ps->clientNum].s) == anim) { // toggled anim to one anim then back to the one we were at previously in
        // one frame, indicating that anim should be restarted.
        BG_FlipPart(ps, setAnimParts_t::SETANIM_TORSO);
    }
#elif defined(_CGAME)
    else if (ps == pm_startPs && pm_startTorsoAnim == anim) { // as for the legs (BG_StartLegsAnim)
        BG_FlipPart(ps, setAnimParts_t::SETANIM_TORSO);
    }
#endif
    ps->torsoAnim = anim;
}

void PM_StartTorsoAnim(animNumber_t anim)
{
    BG_StartTorsoAnim(pm->ps, anim);
}

/*
-------------------------
PM_SetLegsAnimTimer
-------------------------
*/
void BG_SetLegsAnimTimer(playerState_t* ps, int time)
{
    ps->legsTimer = time;

    if (ps->legsTimer < 0 &&
        time != -1) { // Cap timer to 0 if was counting down, but let it be -1 if that was intentional.  NOTENOTE Yeah this seems dumb, but it mirrors SP.
        ps->legsTimer = 0;
    }
}

void PM_SetLegsAnimTimer(int time)
{
    BG_SetLegsAnimTimer(pm->ps, time);
}

/*
-------------------------
PM_SetTorsoAnimTimer
-------------------------
*/
void BG_SetTorsoAnimTimer(playerState_t* ps, int time)
{
    ps->torsoTimer = time;

    if (ps->torsoTimer < 0 &&
        time != -1) { // Cap timer to 0 if was counting down, but let it be -1 if that was intentional.  NOTENOTE Yeah this seems dumb, but it mirrors SP.
        ps->torsoTimer = 0;
    }
}

void PM_SetTorsoAnimTimer(int time)
{
    BG_SetTorsoAnimTimer(pm->ps, time);
}

void BG_SaberStartTransAnim(int clientNum, saber_styles_t saberAnimLevel, weapon_t weapon, animNumber_t anim, float* animSpeed, int broken)
{
    if (anim >= animNumber_t::BOTH_A1_T__B_ && anim <= animNumber_t::BOTH_ROLL_STAB) {
        if (weapon == weapon_t::WP_SABER) {
            saberInfo_t* saber = BG_MySaber(clientNum, 0);
            if (saber && saber->animSpeedScale != 1.0f) {
                *animSpeed *= saber->animSpeedScale;
            }
            saber = BG_MySaber(clientNum, 1);
            if (saber && saber->animSpeedScale != 1.0f) {
                *animSpeed *= saber->animSpeedScale;
            }
        }
    }

    if (((anim) >= animNumber_t::BOTH_T1_BR__R && (anim) <= animNumber_t::BOTH_T1_BL_TL) ||
        ((anim) >= animNumber_t::BOTH_T2_BR__R && (anim) <= animNumber_t::BOTH_T2_BL_TL) ||
        ((anim) >= animNumber_t::BOTH_T3_BR__R && (anim) <= animNumber_t::BOTH_T3_BL_TL)) {
        if (saberAnimLevel == saber_styles_t::SS_FAST) {
            *animSpeed *= 1.5f;
        }
        else if (saberAnimLevel == saber_styles_t::SS_STRONG) {
            *animSpeed *= 0.75f;
        }

        if (broken & Bit(brokenLimb_t::BROKENLIMB_RARM)) {
            *animSpeed *= 0.5f;
        }
        else if (broken & Bit(brokenLimb_t::BROKENLIMB_LARM)) {
            *animSpeed *= 0.65f;
        }
    }
    else if (broken && PM_InSaberAnim(anim)) {
        if (broken & Bit(brokenLimb_t::BROKENLIMB_RARM)) {
            *animSpeed *= 0.5f;
        }
        else if (broken & Bit(brokenLimb_t::BROKENLIMB_LARM)) {
            *animSpeed *= 0.65f;
        }
    }
}

/*
-------------------------
PM_SetAnimFinal
-------------------------
*/
bool PM_RunningAnim(animNumber_t anim);
bool PM_WalkingAnim(animNumber_t anim);

void BG_SetAnimFinal(playerState_t* ps, animSet_t* animations, setAnimParts_t setAnimParts, animNumber_t anim, setAnimFlags_t setAnimFlags)
{
    float editAnimSpeed = 1;

    if (!animations) {
        return;
    }

    assert(anim > animNumber_t::ANIM_NONE);
    assert((*animations)[anim].firstFrame > 0 || (*animations)[anim].numFrames > 0);

    BG_SaberStartTransAnim(ps->clientNum, ps->fd.saberAnimLevel, ps->weapon, anim, &editAnimSpeed, ps->brokenLimbs);

    // Set torso anim
    if (Any(setAnimParts & setAnimParts_t::SETANIM_TORSO)) {
        // Don't reset if it's already running the anim
        if (!Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_RESTART) && (ps->torsoAnim) == anim) {
            goto setAnimLegs;
        }
        // or if a more important anim is running
        if (!Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_OVERRIDE) && ((ps->torsoTimer > 0) || (ps->torsoTimer == -1))) {
            goto setAnimLegs;
        }

        BG_StartTorsoAnim(ps, anim);

        if (Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_HOLD)) {
            if (Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_HOLDLESS)) { // Make sure to only wait in full 1/20 sec server frame intervals.
                int dur;
                int speedDif;

                dur = ((*animations)[anim].numFrames - 1) * Q_fabs((float)((*animations)[anim].frameLerp));
                speedDif = dur - (dur * editAnimSpeed);
                dur += speedDif;
                if (dur > 1) {
                    ps->torsoTimer = dur - 1;
                }
                else {
                    ps->torsoTimer = Q_fabs((float)((*animations)[anim].frameLerp));
                }
            }
            else {
                ps->torsoTimer = (((*animations)[anim].numFrames) * Q_fabs((float)((*animations)[anim].frameLerp)));
            }

            if (ps->fd.forcePowersActive & Bit(forcePowers_t::FP_RAGE)) {
                ps->torsoTimer /= 1.7;
            }
        }
    }

setAnimLegs:
    // Set legs anim
    if (Any(setAnimParts & setAnimParts_t::SETANIM_LEGS)) {
        // Don't reset if it's already running the anim
        if (!Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_RESTART) && (ps->legsAnim) == anim) {
            goto setAnimDone;
        }
        // or if a more important anim is running
        if (!Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_OVERRIDE) && ((ps->legsTimer > 0) || (ps->legsTimer == -1))) {
            goto setAnimDone;
        }

        BG_StartLegsAnim(ps, anim);

        if (Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_HOLD)) {
            if (Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_HOLDLESS)) { // Make sure to only wait in full 1/20 sec server frame intervals.
                int dur;
                int speedDif;

                dur = ((*animations)[anim].numFrames - 1) * Q_fabs((float)((*animations)[anim].frameLerp));
                speedDif = dur - (dur * editAnimSpeed);
                dur += speedDif;
                if (dur > 1) {
                    ps->legsTimer = dur - 1;
                }
                else {
                    ps->legsTimer = Q_fabs((float)((*animations)[anim].frameLerp));
                }
            }
            else {
                ps->legsTimer = (((*animations)[anim].numFrames) * Q_fabs((float)((*animations)[anim].frameLerp)));
            }

            if (PM_RunningAnim(anim) || PM_WalkingAnim(anim)) // these guys are ok, they don't actually reference pm
            {
                if (ps->fd.forcePowersActive & Bit(forcePowers_t::FP_RAGE)) {
                    ps->legsTimer /= 1.3;
                }
                else if (ps->fd.forcePowersActive & Bit(forcePowers_t::FP_SPEED)) {
                    ps->legsTimer /= 1.7;
                }
            }
        }
    }

setAnimDone:
    return;
}

void PM_SetAnimFinal(setAnimParts_t setAnimParts, animNumber_t anim, setAnimFlags_t setAnimFlags)
{
    BG_SetAnimFinal(pm->ps, pm->animations, setAnimParts, anim, setAnimFlags);
}

bool BG_HasAnimation(int animIndex, animNumber_t animation)
{
    animSet_t* animations;

    // must be a valid anim number
    if (animation < animNumber_t::FACE_TALK0 || animation >= animNumber_t::MAX_ANIMATIONS) {
        return false;
    }

    // Must have a file index entry
    if (animIndex < 0 || animIndex > bgNumAllAnims)
        return false;

    animations = bgAllAnims[animIndex].anims;

    // No frames, no anim
    if ((*animations)[animation].numFrames == 0)
        return false;

    // Has the sequence
    return true;
}

std::optional<animNumber_t> BG_PickAnim(int animIndex, animNumber_t minAnim, animNumber_t maxAnim)
{
    animNumber_t anim;
    int count = 0;

    do {
        anim = Q_irand(minAnim, maxAnim);
        count++;
    } while (!BG_HasAnimation(animIndex, anim) && count < 1000);

    if (count == 1000) { // guess we just don't have a death anim then.
        return std::nullopt;
    }

    return anim;
}

// I want to be able to use this on a playerstate even when we are not the focus
// of a pmove too so I have ported it to true BGishness.
// Please do not reference pm in this function or any functions that it calls,
// or I will cry. -rww
void BG_SetAnim(playerState_t* ps, animSet_t* animations, setAnimParts_t setAnimParts, animNumber_t anim, setAnimFlags_t setAnimFlags)
{
    if (!animations) {
        animations = bgAllAnims[0].anims;
    }

    if ((*animations)[anim].firstFrame == 0 && (*animations)[anim].numFrames == 0) {
        if (anim == animNumber_t::BOTH_RUNBACK1 || anim == animNumber_t::BOTH_WALKBACK1 || anim == animNumber_t::BOTH_RUN1) { // hack for droids
            anim = animNumber_t::BOTH_WALK2;
        }

        if ((*animations)[anim].firstFrame == 0 && (*animations)[anim].numFrames == 0) { // still? Just return then I guess.
            return;
        }
    }

    /*
    if (BG_InSpecialJump(anim))
    {
        setAnimFlags |= setAnimFlags_t::SETANIM_FLAG_RESTART;
    }
    */
    // Don't know why I put this here originally but it's messing stuff up now and it isn't needed.

    //	if (BG_InRoll(ps, ps->legsAnim))
    //	{ //never interrupt a roll
    //		return;
    //	}

    if (Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_OVERRIDE)) {
        if (Any(setAnimParts & setAnimParts_t::SETANIM_TORSO)) {
            if (Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_RESTART) || (ps->torsoAnim) != anim) {
                BG_SetTorsoAnimTimer(ps, 0);
            }
        }
        if (Any(setAnimParts & setAnimParts_t::SETANIM_LEGS)) {
            if (Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_RESTART) || (ps->legsAnim) != anim) {
                BG_SetLegsAnimTimer(ps, 0);
            }
        }
    }

    BG_SetAnimFinal(ps, animations, setAnimParts, anim, setAnimFlags);
}

void PM_SetAnim(setAnimParts_t setAnimParts, animNumber_t anim, setAnimFlags_t setAnimFlags)
{
    BG_SetAnim(pm->ps, pm->animations, setAnimParts, anim, setAnimFlags);
}

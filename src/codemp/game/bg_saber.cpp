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

#include <iterator>

#include <qcommon/q_shared.h>
#include "bg_public.h"
#include "bg_local.h"
#include "w_saber.h"

#if defined(_GAME)
#include "g_local.h" // for cvars
#elif defined(_CGAME)
#include <cgame/cg_local.h> // for configstrings
#endif

extern bool BG_SabersOff(playerState_t* ps);
saberInfo_t* BG_MySaber(int clientNum, int saberNum);

// A number from val1 to val2 - 1 (val2 itself never came out of 1.01's formula, val1 + random * (val2 - val1)
// truncated, and the saber's chances were tuned with it), or val1 when val2 <= val1. Drawn from the command's own
// state, seeded with its time, so that the server and the client's prediction draw the same numbers.
int PM_irand_timesync(int val1, int val2)
{
    if (val2 <= val1) {
        return val1;
    }
    return Q_RandInt(&pml.rand, val1, val2 - 1);
}

void BG_ForcePowerDrain(playerState_t* ps, forcePowers_t forcePower, int overrideAmt)
{
    // take away the power
    int drain = overrideAmt;

    /*
    if (ps->powerups[powerup_t::PW_FORCE_BOON])
    {
        return;
    }
    */
    // No longer grant infinite force with boon.

    if (!drain) {
        drain = forcePowerNeeded[ps->fd.forcePowerLevel[forcePower]][forcePower];
    }
    if (!drain) {
        return;
    }

    if (forcePower == forcePowers_t::FP_LEVITATION) { // special case
        int jumpDrain = 0;

        if (ps->velocity[2] > 250) {
            jumpDrain = 20;
        }
        else if (ps->velocity[2] > 200) {
            jumpDrain = 16;
        }
        else if (ps->velocity[2] > 150) {
            jumpDrain = 12;
        }
        else if (ps->velocity[2] > 100) {
            jumpDrain = 8;
        }
        else if (ps->velocity[2] > 50) {
            jumpDrain = 6;
        }
        else if (ps->velocity[2] > 0) {
            jumpDrain = 4;
        }

        if (jumpDrain) {
            if (ps->fd.forcePowerLevel[forcePowers_t::FP_LEVITATION] != forcePowerLevels_t::FORCE_LEVEL_0) { // don't divide by 0!
                jumpDrain /= to_underlying(ps->fd.forcePowerLevel[forcePowers_t::FP_LEVITATION]);
            }
        }

        ps->fd.forcePower -= jumpDrain;
        if (ps->fd.forcePower < 0) {
            ps->fd.forcePower = 0;
        }

        return;
    }

    ps->fd.forcePower -= drain;
    if (ps->fd.forcePower < 0) {
        ps->fd.forcePower = 0;
    }
}

bool BG_EnoughForcePowerForMove(int cost)
{
    if (pm->ps->fd.forcePower < cost) {
        PM_AddEvent(entity_event_t::EV_NOAMMO);
        return false;
    }

    return true;
}

// Silly, but I'm replacing these macros so they are shorter!
inline constexpr setAnimFlags_t AFLAG_IDLE = setAnimFlags_t::SETANIM_FLAG_NORMAL;
inline constexpr setAnimFlags_t AFLAG_ACTIVE =
    setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_HOLDLESS;
inline constexpr setAnimFlags_t AFLAG_WAIT = setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_HOLDLESS;
inline constexpr setAnimFlags_t AFLAG_FINISH = setAnimFlags_t::SETANIM_FLAG_HOLD;

// FIXME: add the alternate anims for each style?
EnumArray<saberMoveData_t, saberMoveName_t, saberMoveName_t::LS_MOVE_MAX> saberMoveData = {{
    //							NB:randomized
    // name			anim(do all styles?)startQ	endQ	setanimflag		blend,	blocking	chain_idle		chain_attack	trailLen
    {"None", animNumber_t::BOTH_STAND1, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_IDLE, 350, saberBlockType_t::BLK_NO, saberMoveName_t::LS_NONE,
     saberMoveName_t::LS_NONE, 0}, // saberMoveName_t::LS_NONE		= 0,

    // General movements with saber
    {"Ready", animNumber_t::BOTH_STAND2, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_IDLE, 350, saberBlockType_t::BLK_WIDE, saberMoveName_t::LS_READY,
     saberMoveName_t::LS_S_R2L, 0}, // LS_READY,
    {"Draw", animNumber_t::BOTH_STAND1TO2, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_FINISH, 350, saberBlockType_t::BLK_NO, saberMoveName_t::LS_READY,
     saberMoveName_t::LS_S_R2L, 0}, // LS_DRAW,
    {"Putaway", animNumber_t::BOTH_STAND2TO1, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_FINISH, 350, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_S_R2L, 0}, // LS_PUTAWAY,

    // Attacks
    // UL2LR
    {"TL2BR Att", animNumber_t::BOTH_A1_TL_BR, saberQuadrant_t::Q_TL, saberQuadrant_t::Q_BR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_R_TL2BR, saberMoveName_t::LS_R_TL2BR, 200}, // LS_A_TL2BR
    // SLASH LEFT
    {"L2R Att", animNumber_t::BOTH_A1__L__R, saberQuadrant_t::Q_L, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_R_L2R, saberMoveName_t::LS_R_L2R, 200}, // LS_A_L2R
    // LL2UR
    {"BL2TR Att", animNumber_t::BOTH_A1_BL_TR, saberQuadrant_t::Q_BL, saberQuadrant_t::Q_TR, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_R_BL2TR, 200}, // LS_A_BL2TR
    // LR2UL
    {"BR2TL Att", animNumber_t::BOTH_A1_BR_TL, saberQuadrant_t::Q_BR, saberQuadrant_t::Q_TL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_R_BR2TL, saberMoveName_t::LS_R_BR2TL, 200}, // LS_A_BR2TL
    // SLASH RIGHT
    {"R2L Att", animNumber_t::BOTH_A1__R__L, saberQuadrant_t::Q_R, saberQuadrant_t::Q_L, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_R_R2L, saberMoveName_t::LS_R_R2L, 200}, // LS_A_R2L
    // UR2LL
    {"TR2BL Att", animNumber_t::BOTH_A1_TR_BL, saberQuadrant_t::Q_TR, saberQuadrant_t::Q_BL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_R_TR2BL, saberMoveName_t::LS_R_TR2BL, 200}, // LS_A_TR2BL
    // SLASH DOWN
    {"T2B Att", animNumber_t::BOTH_A1_T__B_, saberQuadrant_t::Q_T, saberQuadrant_t::Q_B, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_R_T2B, saberMoveName_t::LS_R_T2B, 200}, // LS_A_T2B
    // special attacks
    {"Back Stab", animNumber_t::BOTH_A2_STABBACK1, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_A_BACKSTAB
    {"Back Att", animNumber_t::BOTH_ATTACK_BACK, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_A_BACK
    {"CR Back Att", animNumber_t::BOTH_CROUCHATTACKBACK1, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_A_BACK_CR
    {"RollStab", animNumber_t::BOTH_ROLL_STAB, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_ROLL_STAB
    {"Lunge Att", animNumber_t::BOTH_LUNGE2_B__T_, saberQuadrant_t::Q_B, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_A_LUNGE
    {"Jump Att", animNumber_t::BOTH_FORCELEAP2_T__B_, saberQuadrant_t::Q_T, saberQuadrant_t::Q_B, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_A_JUMP_T__B_
    {"Flip Stab", animNumber_t::BOTH_JUMPFLIPSTABDOWN, saberQuadrant_t::Q_R, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_T1_T___R, 200}, // LS_A_FLIP_STAB
    {"Flip Slash", animNumber_t::BOTH_JUMPFLIPSLASHDOWN1, saberQuadrant_t::Q_L, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_T1__R_T_, 200}, // LS_A_FLIP_SLASH
    {"DualJump Atk", animNumber_t::BOTH_JUMPATTACK6, saberQuadrant_t::Q_R, saberQuadrant_t::Q_BL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_T1_BL_TR, 200}, // LS_JUMPATTACK_DUAL

    {"DualJumpAtkL_A", animNumber_t::BOTH_ARIAL_LEFT, saberQuadrant_t::Q_R, saberQuadrant_t::Q_TL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_A_TL2BR, 200}, // LS_JUMPATTACK_ARIAL_LEFT
    {"DualJumpAtkR_A", animNumber_t::BOTH_ARIAL_RIGHT, saberQuadrant_t::Q_R, saberQuadrant_t::Q_TR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_A_TR2BL, 200}, // LS_JUMPATTACK_ARIAL_RIGHT

    {"DualJumpAtkL_A", animNumber_t::BOTH_CARTWHEEL_LEFT, saberQuadrant_t::Q_R, saberQuadrant_t::Q_TL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_T1_TL_BR, 200}, // LS_JUMPATTACK_CART_LEFT
    {"DualJumpAtkR_A", animNumber_t::BOTH_CARTWHEEL_RIGHT, saberQuadrant_t::Q_R, saberQuadrant_t::Q_TR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_T1_TR_BL, 200}, // LS_JUMPATTACK_CART_RIGHT

    {"DualJumpAtkLStaff", animNumber_t::BOTH_BUTTERFLY_FL1, saberQuadrant_t::Q_R, saberQuadrant_t::Q_L, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_T1__L__R, 200}, // LS_JUMPATTACK_STAFF_LEFT
    {"DualJumpAtkRStaff", animNumber_t::BOTH_BUTTERFLY_FR1, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_T1__R__L, 200}, // LS_JUMPATTACK_STAFF_RIGHT

    {"ButterflyLeft", animNumber_t::BOTH_BUTTERFLY_LEFT, saberQuadrant_t::Q_R, saberQuadrant_t::Q_L, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_T1__L__R, 200}, // LS_BUTTERFLY_LEFT
    {"ButterflyRight", animNumber_t::BOTH_BUTTERFLY_RIGHT, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_T1__R__L, 200}, // LS_BUTTERFLY_RIGHT

    {"BkFlip Atk", animNumber_t::BOTH_JUMPATTACK7, saberQuadrant_t::Q_B, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_T1_T___R, 200}, // LS_A_BACKFLIP_ATK
    {"DualSpinAtk", animNumber_t::BOTH_SPINATTACK6, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_SPINATTACK_DUAL
    {"StfSpinAtk", animNumber_t::BOTH_SPINATTACK7, saberQuadrant_t::Q_L, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_SPINATTACK
    {"LngLeapAtk", animNumber_t::BOTH_FORCELONGLEAP_ATTACK, saberQuadrant_t::Q_R, saberQuadrant_t::Q_L, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_LEAP_ATTACK
    {"SwoopAtkR", animNumber_t::BOTH_VS_ATR_S, saberQuadrant_t::Q_R, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_SWOOP_ATTACK_RIGHT
    {"SwoopAtkL", animNumber_t::BOTH_VS_ATL_S, saberQuadrant_t::Q_L, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_SWOOP_ATTACK_LEFT
    {"TauntaunAtkR", animNumber_t::BOTH_VT_ATR_S, saberQuadrant_t::Q_R, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_TAUNTAUN_ATTACK_RIGHT
    {"TauntaunAtkL", animNumber_t::BOTH_VT_ATL_S, saberQuadrant_t::Q_L, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_TAUNTAUN_ATTACK_LEFT
    {"StfKickFwd", animNumber_t::BOTH_A7_KICK_F, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_S_R2L, 200}, // LS_KICK_F
    {"StfKickBack", animNumber_t::BOTH_A7_KICK_B, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_S_R2L, 200}, // LS_KICK_B
    {"StfKickRight", animNumber_t::BOTH_A7_KICK_R, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_S_R2L, 200}, // LS_KICK_R
    {"StfKickLeft", animNumber_t::BOTH_A7_KICK_L, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_S_R2L, 200}, // LS_KICK_L
    {"StfKickSpin", animNumber_t::BOTH_A7_KICK_S, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_S_R2L, 200}, // LS_KICK_S
    {"StfKickBkFwd", animNumber_t::BOTH_A7_KICK_BF, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_S_R2L, 200}, // LS_KICK_BF
    {"StfKickSplit", animNumber_t::BOTH_A7_KICK_RL, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_S_R2L, 200}, // LS_KICK_RL
    {"StfKickFwdAir", animNumber_t::BOTH_A7_KICK_F_AIR, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_S_R2L, 200}, // LS_KICK_F_AIR
    {"StfKickBackAir", animNumber_t::BOTH_A7_KICK_B_AIR, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_S_R2L, 200}, // LS_KICK_B_AIR
    {"StfKickRightAir", animNumber_t::BOTH_A7_KICK_R_AIR, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_S_R2L, 200}, // LS_KICK_R_AIR
    {"StfKickLeftAir", animNumber_t::BOTH_A7_KICK_L_AIR, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_S_R2L, 200}, // LS_KICK_L_AIR
    {"StabDown", animNumber_t::BOTH_STABDOWN, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_S_R2L, 200}, // LS_STABDOWN
    {"StabDownStf", animNumber_t::BOTH_STABDOWN_STAFF, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_S_R2L, 200}, // LS_STABDOWN_STAFF
    {"StabDownDual", animNumber_t::BOTH_STABDOWN_DUAL, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_S_R2L, 200}, // LS_STABDOWN_DUAL
    {"dualspinprot", animNumber_t::BOTH_A6_SABERPROTECT, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 500}, // LS_DUAL_SPIN_PROTECT
    {"StfSoulCal", animNumber_t::BOTH_A7_SOULCAL, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 500}, // LS_STAFF_SOULCAL
    {"specialfast", animNumber_t::BOTH_A1_SPECIAL, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 2000}, // LS_A1_SPECIAL
    {"specialmed", animNumber_t::BOTH_A2_SPECIAL, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 2000}, // LS_A2_SPECIAL
    {"specialstr", animNumber_t::BOTH_A3_SPECIAL, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 2000}, // LS_A3_SPECIAL
    {"upsidedwnatk", animNumber_t::BOTH_FLIP_ATTACK7, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_UPSIDE_DOWN_ATTACK
    {"pullatkstab", animNumber_t::BOTH_PULL_IMPALE_STAB, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_PULL_ATTACK_STAB
    {"pullatkswing", animNumber_t::BOTH_PULL_IMPALE_SWING, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_PULL_ATTACK_SWING
    {"AloraSpinAtk", animNumber_t::BOTH_ALORA_SPIN_SLASH, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_SPINATTACK_ALORA
    {"Dual FB Atk", animNumber_t::BOTH_A6_FB, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_DUAL_FB
    {"Dual LR Atk", animNumber_t::BOTH_A6_LR, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_DUAL_LR
    {"StfHiltBash", animNumber_t::BOTH_A7_HILT, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_HILT_BASH

    // starts
    {"TL2BR St", animNumber_t::BOTH_S1_S1_TL, saberQuadrant_t::Q_R, saberQuadrant_t::Q_TL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_A_TL2BR, saberMoveName_t::LS_A_TL2BR, 200}, // LS_S_TL2BR
    {"L2R St", animNumber_t::BOTH_S1_S1__L, saberQuadrant_t::Q_R, saberQuadrant_t::Q_L, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_A_L2R, saberMoveName_t::LS_A_L2R, 200}, // LS_S_L2R
    {"BL2TR St", animNumber_t::BOTH_S1_S1_BL, saberQuadrant_t::Q_R, saberQuadrant_t::Q_BL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_A_BL2TR, saberMoveName_t::LS_A_BL2TR, 200}, // LS_S_BL2TR
    {"BR2TL St", animNumber_t::BOTH_S1_S1_BR, saberQuadrant_t::Q_R, saberQuadrant_t::Q_BR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_A_BR2TL, saberMoveName_t::LS_A_BR2TL, 200}, // LS_S_BR2TL
    {"R2L St", animNumber_t::BOTH_S1_S1__R, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_A_R2L, saberMoveName_t::LS_A_R2L, 200}, // LS_S_R2L
    {"TR2BL St", animNumber_t::BOTH_S1_S1_TR, saberQuadrant_t::Q_R, saberQuadrant_t::Q_TR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_A_TR2BL, saberMoveName_t::LS_A_TR2BL, 200}, // LS_S_TR2BL
    {"T2B St", animNumber_t::BOTH_S1_S1_T_, saberQuadrant_t::Q_R, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_A_T2B, saberMoveName_t::LS_A_T2B, 200}, // LS_S_T2B

    // returns
    {"TL2BR Ret", animNumber_t::BOTH_R1_BR_S1, saberQuadrant_t::Q_BR, saberQuadrant_t::Q_R, AFLAG_FINISH, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_R_TL2BR
    {"L2R Ret", animNumber_t::BOTH_R1__R_S1, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_FINISH, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_R_L2R
    {"BL2TR Ret", animNumber_t::BOTH_R1_TR_S1, saberQuadrant_t::Q_TR, saberQuadrant_t::Q_R, AFLAG_FINISH, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_R_BL2TR
    {"BR2TL Ret", animNumber_t::BOTH_R1_TL_S1, saberQuadrant_t::Q_TL, saberQuadrant_t::Q_R, AFLAG_FINISH, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_R_BR2TL
    {"R2L Ret", animNumber_t::BOTH_R1__L_S1, saberQuadrant_t::Q_L, saberQuadrant_t::Q_R, AFLAG_FINISH, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_R_R2L
    {"TR2BL Ret", animNumber_t::BOTH_R1_BL_S1, saberQuadrant_t::Q_BL, saberQuadrant_t::Q_R, AFLAG_FINISH, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_R_TR2BL
    {"T2B Ret", animNumber_t::BOTH_R1_B__S1, saberQuadrant_t::Q_B, saberQuadrant_t::Q_R, AFLAG_FINISH, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_R_T2B

    // Transitions
    {"BR2R Trans", animNumber_t::BOTH_T1_BR__R, saberQuadrant_t::Q_BR, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_L2R, saberMoveName_t::LS_A_R2L, 150}, // # Fast arc bottom right to right
    {"BR2TR Trans", animNumber_t::BOTH_T1_BR_TR, saberQuadrant_t::Q_BR, saberQuadrant_t::Q_TR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_A_TR2BL, 150}, // # Fast arc bottom right to top right		(use: BOTH_T1_TR_BR)
    {"BR2T Trans", animNumber_t::BOTH_T1_BR_T_, saberQuadrant_t::Q_BR, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_A_T2B, 150}, // # Fast arc bottom right to top			(use: BOTH_T1_T__BR)
    {"BR2TL Trans", animNumber_t::BOTH_T1_BR_TL, saberQuadrant_t::Q_BR, saberQuadrant_t::Q_TL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BR2TL, saberMoveName_t::LS_A_TL2BR, 150}, // # Fast weak spin bottom right to top left
    {"BR2L Trans", animNumber_t::BOTH_T1_BR__L, saberQuadrant_t::Q_BR, saberQuadrant_t::Q_L, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_R2L, saberMoveName_t::LS_A_L2R, 150}, // # Fast weak spin bottom right to left
    {"BR2BL Trans", animNumber_t::BOTH_T1_BR_BL, saberQuadrant_t::Q_BR, saberQuadrant_t::Q_BL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_TR2BL, saberMoveName_t::LS_A_BL2TR, 150}, // # Fast weak spin bottom right to bottom left
    {"R2BR Trans", animNumber_t::BOTH_T1__R_BR, saberQuadrant_t::Q_R, saberQuadrant_t::Q_BR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_TL2BR, saberMoveName_t::LS_A_BR2TL, 150}, // # Fast arc right to bottom right			(use: BOTH_T1_BR__R)
    {"R2TR Trans", animNumber_t::BOTH_T1__R_TR, saberQuadrant_t::Q_R, saberQuadrant_t::Q_TR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_A_TR2BL, 150}, // # Fast arc right to top right
    {"R2T Trans", animNumber_t::BOTH_T1__R_T_, saberQuadrant_t::Q_R, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_A_T2B, 150}, // # Fast ar right to top				(use: BOTH_T1_T___R)
    {"R2TL Trans", animNumber_t::BOTH_T1__R_TL, saberQuadrant_t::Q_R, saberQuadrant_t::Q_TL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BR2TL, saberMoveName_t::LS_A_TL2BR, 150}, // # Fast arc right to top left
    {"R2L Trans", animNumber_t::BOTH_T1__R__L, saberQuadrant_t::Q_R, saberQuadrant_t::Q_L, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_R2L, saberMoveName_t::LS_A_L2R, 150}, // # Fast weak spin right to left
    {"R2BL Trans", animNumber_t::BOTH_T1__R_BL, saberQuadrant_t::Q_R, saberQuadrant_t::Q_BL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_TR2BL, saberMoveName_t::LS_A_BL2TR, 150}, // # Fast weak spin right to bottom left
    {"TR2BR Trans", animNumber_t::BOTH_T1_TR_BR, saberQuadrant_t::Q_TR, saberQuadrant_t::Q_BR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_TL2BR, saberMoveName_t::LS_A_BR2TL, 150}, // # Fast arc top right to bottom right
    {"TR2R Trans", animNumber_t::BOTH_T1_TR__R, saberQuadrant_t::Q_TR, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_L2R, saberMoveName_t::LS_A_R2L, 150}, // # Fast arc top right to right			(use: BOTH_T1__R_TR)
    {"TR2T Trans", animNumber_t::BOTH_T1_TR_T_, saberQuadrant_t::Q_TR, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_A_T2B, 150}, // # Fast arc top right to top				(use: BOTH_T1_T__TR)
    {"TR2TL Trans", animNumber_t::BOTH_T1_TR_TL, saberQuadrant_t::Q_TR, saberQuadrant_t::Q_TL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BR2TL, saberMoveName_t::LS_A_TL2BR, 150}, // # Fast arc top right to top left
    {"TR2L Trans", animNumber_t::BOTH_T1_TR__L, saberQuadrant_t::Q_TR, saberQuadrant_t::Q_L, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_R2L, saberMoveName_t::LS_A_L2R, 150}, // # Fast arc top right to left
    {"TR2BL Trans", animNumber_t::BOTH_T1_TR_BL, saberQuadrant_t::Q_TR, saberQuadrant_t::Q_BL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_TR2BL, saberMoveName_t::LS_A_BL2TR, 150}, // # Fast weak spin top right to bottom left
    {"T2BR Trans", animNumber_t::BOTH_T1_T__BR, saberQuadrant_t::Q_T, saberQuadrant_t::Q_BR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_TL2BR, saberMoveName_t::LS_A_BR2TL, 150}, // # Fast arc top to bottom right
    {"T2R Trans", animNumber_t::BOTH_T1_T___R, saberQuadrant_t::Q_T, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_L2R, saberMoveName_t::LS_A_R2L, 150}, // # Fast arc top to right
    {"T2TR Trans", animNumber_t::BOTH_T1_T__TR, saberQuadrant_t::Q_T, saberQuadrant_t::Q_TR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_A_TR2BL, 150}, // # Fast arc top to top right
    {"T2TL Trans", animNumber_t::BOTH_T1_T__TL, saberQuadrant_t::Q_T, saberQuadrant_t::Q_TL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BR2TL, saberMoveName_t::LS_A_TL2BR, 150}, // # Fast arc top to top left
    {"T2L Trans", animNumber_t::BOTH_T1_T___L, saberQuadrant_t::Q_T, saberQuadrant_t::Q_L, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_R2L, saberMoveName_t::LS_A_L2R, 150}, // # Fast arc top to left
    {"T2BL Trans", animNumber_t::BOTH_T1_T__BL, saberQuadrant_t::Q_T, saberQuadrant_t::Q_BL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_TR2BL, saberMoveName_t::LS_A_BL2TR, 150}, // # Fast arc top to bottom left
    {"TL2BR Trans", animNumber_t::BOTH_T1_TL_BR, saberQuadrant_t::Q_TL, saberQuadrant_t::Q_BR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_TL2BR, saberMoveName_t::LS_A_BR2TL, 150}, // # Fast weak spin top left to bottom right
    {"TL2R Trans", animNumber_t::BOTH_T1_TL__R, saberQuadrant_t::Q_TL, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_L2R, saberMoveName_t::LS_A_R2L, 150}, // # Fast arc top left to right			(use: BOTH_T1__R_TL)
    {"TL2TR Trans", animNumber_t::BOTH_T1_TL_TR, saberQuadrant_t::Q_TL, saberQuadrant_t::Q_TR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_A_TR2BL, 150}, // # Fast arc top left to top right			(use: BOTH_T1_TR_TL)
    {"TL2T Trans", animNumber_t::BOTH_T1_TL_T_, saberQuadrant_t::Q_TL, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_A_T2B, 150}, // # Fast arc top left to top				(use: BOTH_T1_T__TL)
    {"TL2L Trans", animNumber_t::BOTH_T1_TL__L, saberQuadrant_t::Q_TL, saberQuadrant_t::Q_L, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_R2L, saberMoveName_t::LS_A_L2R, 150}, // # Fast arc top left to left				(use: BOTH_T1__L_TL)
    {"TL2BL Trans", animNumber_t::BOTH_T1_TL_BL, saberQuadrant_t::Q_TL, saberQuadrant_t::Q_BL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_TR2BL, saberMoveName_t::LS_A_BL2TR, 150}, // # Fast arc top left to bottom left
    {"L2BR Trans", animNumber_t::BOTH_T1__L_BR, saberQuadrant_t::Q_L, saberQuadrant_t::Q_BR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_TL2BR, saberMoveName_t::LS_A_BR2TL, 150}, // # Fast weak spin left to bottom right
    {"L2R Trans", animNumber_t::BOTH_T1__L__R, saberQuadrant_t::Q_L, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_L2R, saberMoveName_t::LS_A_R2L, 150}, // # Fast weak spin left to right
    {"L2TR Trans", animNumber_t::BOTH_T1__L_TR, saberQuadrant_t::Q_L, saberQuadrant_t::Q_TR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_A_TR2BL, 150}, // # Fast arc left to top right			(use: BOTH_T1_TR__L)
    {"L2T Trans", animNumber_t::BOTH_T1__L_T_, saberQuadrant_t::Q_L, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_A_T2B, 150}, // # Fast arc left to top				(use: BOTH_T1_T___L)
    {"L2TL Trans", animNumber_t::BOTH_T1__L_TL, saberQuadrant_t::Q_L, saberQuadrant_t::Q_TL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BR2TL, saberMoveName_t::LS_A_TL2BR, 150}, // # Fast arc left to top left
    {"L2BL Trans", animNumber_t::BOTH_T1__L_BL, saberQuadrant_t::Q_L, saberQuadrant_t::Q_BL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_TR2BL, saberMoveName_t::LS_A_BL2TR, 150}, // # Fast arc left to bottom left			(use: BOTH_T1_BL__L)
    {"BL2BR Trans", animNumber_t::BOTH_T1_BL_BR, saberQuadrant_t::Q_BL, saberQuadrant_t::Q_BR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_TL2BR, saberMoveName_t::LS_A_BR2TL, 150}, // # Fast weak spin bottom left to bottom right
    {"BL2R Trans", animNumber_t::BOTH_T1_BL__R, saberQuadrant_t::Q_BL, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_L2R, saberMoveName_t::LS_A_R2L, 150}, // # Fast weak spin bottom left to right
    {"BL2TR Trans", animNumber_t::BOTH_T1_BL_TR, saberQuadrant_t::Q_BL, saberQuadrant_t::Q_TR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_A_TR2BL, 150}, // # Fast weak spin bottom left to top right
    {"BL2T Trans", animNumber_t::BOTH_T1_BL_T_, saberQuadrant_t::Q_BL, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_A_T2B, 150}, // # Fast arc bottom left to top			(use: BOTH_T1_T__BL)
    {"BL2TL Trans", animNumber_t::BOTH_T1_BL_TL, saberQuadrant_t::Q_BL, saberQuadrant_t::Q_TL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BR2TL, saberMoveName_t::LS_A_TL2BR, 150}, // # Fast arc bottom left to top left		(use: BOTH_T1_TL_BL)
    {"BL2L Trans", animNumber_t::BOTH_T1_BL__L, saberQuadrant_t::Q_BL, saberQuadrant_t::Q_L, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_R2L, saberMoveName_t::LS_A_L2R, 150}, // # Fast arc bottom left to left

    // Bounces
    {"Bounce BR", animNumber_t::BOTH_B1_BR___, saberQuadrant_t::Q_BR, saberQuadrant_t::Q_BR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_TL2BR, saberMoveName_t::LS_T1_BR_TR, 150},
    {"Bounce R", animNumber_t::BOTH_B1__R___, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_L2R, saberMoveName_t::LS_T1__R__L, 150},
    {"Bounce TR", animNumber_t::BOTH_B1_TR___, saberQuadrant_t::Q_TR, saberQuadrant_t::Q_TR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_T1_TR_TL, 150},
    {"Bounce T", animNumber_t::BOTH_B1_T____, saberQuadrant_t::Q_T, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_T1_T__BL, 150},
    {"Bounce TL", animNumber_t::BOTH_B1_TL___, saberQuadrant_t::Q_TL, saberQuadrant_t::Q_TL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BR2TL, saberMoveName_t::LS_T1_TL_TR, 150},
    {"Bounce L", animNumber_t::BOTH_B1__L___, saberQuadrant_t::Q_L, saberQuadrant_t::Q_L, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_R2L, saberMoveName_t::LS_T1__L__R, 150},
    {"Bounce BL", animNumber_t::BOTH_B1_BL___, saberQuadrant_t::Q_BL, saberQuadrant_t::Q_BL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_TR2BL, saberMoveName_t::LS_T1_BL_TR, 150},

    // Deflected attacks (like bounces, but slide off enemy saber, not straight back)
    {"Deflect BR", animNumber_t::BOTH_D1_BR___, saberQuadrant_t::Q_BR, saberQuadrant_t::Q_BR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_TL2BR, saberMoveName_t::LS_T1_BR_TR, 150},
    {"Deflect R", animNumber_t::BOTH_D1__R___, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_L2R, saberMoveName_t::LS_T1__R__L, 150},
    {"Deflect TR", animNumber_t::BOTH_D1_TR___, saberQuadrant_t::Q_TR, saberQuadrant_t::Q_TR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_T1_TR_TL, 150},
    {"Deflect T", animNumber_t::BOTH_B1_T____, saberQuadrant_t::Q_T, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_T1_T__BL, 150},
    {"Deflect TL", animNumber_t::BOTH_D1_TL___, saberQuadrant_t::Q_TL, saberQuadrant_t::Q_TL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BR2TL, saberMoveName_t::LS_T1_TL_TR, 150},
    {"Deflect L", animNumber_t::BOTH_D1__L___, saberQuadrant_t::Q_L, saberQuadrant_t::Q_L, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_R2L, saberMoveName_t::LS_T1__L__R, 150},
    {"Deflect BL", animNumber_t::BOTH_D1_BL___, saberQuadrant_t::Q_BL, saberQuadrant_t::Q_BL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_TR2BL, saberMoveName_t::LS_T1_BL_TR, 150},
    {"Deflect B", animNumber_t::BOTH_D1_B____, saberQuadrant_t::Q_B, saberQuadrant_t::Q_B, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_T1_T__BL, 150},

    // Reflected attacks
    {"Reflected BR", animNumber_t::BOTH_V1_BR_S1, saberQuadrant_t::Q_BR, saberQuadrant_t::Q_BR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 150}, //	LS_V1_BR
    {"Reflected R", animNumber_t::BOTH_V1__R_S1, saberQuadrant_t::Q_R, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 150}, //	LS_V1__R
    {"Reflected TR", animNumber_t::BOTH_V1_TR_S1, saberQuadrant_t::Q_TR, saberQuadrant_t::Q_TR, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 150}, //	LS_V1_TR
    {"Reflected T", animNumber_t::BOTH_V1_T__S1, saberQuadrant_t::Q_T, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 150}, //	LS_V1_T_
    {"Reflected TL", animNumber_t::BOTH_V1_TL_S1, saberQuadrant_t::Q_TL, saberQuadrant_t::Q_TL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 150}, //	LS_V1_TL
    {"Reflected L", animNumber_t::BOTH_V1__L_S1, saberQuadrant_t::Q_L, saberQuadrant_t::Q_L, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 150}, //	LS_V1__L
    {"Reflected BL", animNumber_t::BOTH_V1_BL_S1, saberQuadrant_t::Q_BL, saberQuadrant_t::Q_BL, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 150}, //	LS_V1_BL
    {"Reflected B", animNumber_t::BOTH_V1_B__S1, saberQuadrant_t::Q_B, saberQuadrant_t::Q_B, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 150}, //	LS_V1_B_

    // Broken parries
    {"BParry Top", animNumber_t::BOTH_H1_S1_T_, saberQuadrant_t::Q_T, saberQuadrant_t::Q_B, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 150}, // LS_PARRY_UP,
    {"BParry UR", animNumber_t::BOTH_H1_S1_TR, saberQuadrant_t::Q_TR, saberQuadrant_t::Q_BL, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 150}, // LS_PARRY_UR,
    {"BParry UL", animNumber_t::BOTH_H1_S1_TL, saberQuadrant_t::Q_TL, saberQuadrant_t::Q_BR, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 150}, // LS_PARRY_UL,
    {"BParry LR", animNumber_t::BOTH_H1_S1_BL, saberQuadrant_t::Q_BL, saberQuadrant_t::Q_TR, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 150}, // LS_PARRY_LR,
    {"BParry Bot", animNumber_t::BOTH_H1_S1_B_, saberQuadrant_t::Q_B, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 150}, // LS_PARRY_LR
    {"BParry LL", animNumber_t::BOTH_H1_S1_BR, saberQuadrant_t::Q_BR, saberQuadrant_t::Q_TL, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 150}, // LS_PARRY_LL

    // Knockaways
    {"Knock Top", animNumber_t::BOTH_K1_S1_T_, saberQuadrant_t::Q_R, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_WIDE,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_T1_T__BR, 150}, // LS_PARRY_UP,
    {"Knock UR", animNumber_t::BOTH_K1_S1_TR, saberQuadrant_t::Q_R, saberQuadrant_t::Q_TR, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_WIDE,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_T1_TR__R, 150}, // LS_PARRY_UR,
    {"Knock UL", animNumber_t::BOTH_K1_S1_TL, saberQuadrant_t::Q_R, saberQuadrant_t::Q_TL, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_WIDE,
     saberMoveName_t::LS_R_BR2TL, saberMoveName_t::LS_T1_TL__L, 150}, // LS_PARRY_UL,
    {"Knock LR", animNumber_t::BOTH_K1_S1_BL, saberQuadrant_t::Q_R, saberQuadrant_t::Q_BL, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_WIDE,
     saberMoveName_t::LS_R_TL2BR, saberMoveName_t::LS_T1_BL_TL, 150}, // LS_PARRY_LR,
    {"Knock LL", animNumber_t::BOTH_K1_S1_BR, saberQuadrant_t::Q_R, saberQuadrant_t::Q_BR, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_WIDE,
     saberMoveName_t::LS_R_TR2BL, saberMoveName_t::LS_T1_BR_TR, 150}, // LS_PARRY_LL

    // Parry
    {"Parry Top", animNumber_t::BOTH_P1_S1_T_, saberQuadrant_t::Q_R, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_WIDE,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_A_T2B, 150}, // LS_PARRY_UP,
    {"Parry UR", animNumber_t::BOTH_P1_S1_TR, saberQuadrant_t::Q_R, saberQuadrant_t::Q_TL, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_WIDE,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_A_TR2BL, 150}, // LS_PARRY_UR,
    {"Parry UL", animNumber_t::BOTH_P1_S1_TL, saberQuadrant_t::Q_R, saberQuadrant_t::Q_TR, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_WIDE,
     saberMoveName_t::LS_R_BR2TL, saberMoveName_t::LS_A_TL2BR, 150}, // LS_PARRY_UL,
    {"Parry LR", animNumber_t::BOTH_P1_S1_BL, saberQuadrant_t::Q_R, saberQuadrant_t::Q_BR, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_WIDE,
     saberMoveName_t::LS_R_TL2BR, saberMoveName_t::LS_A_BR2TL, 150}, // LS_PARRY_LR,
    {"Parry LL", animNumber_t::BOTH_P1_S1_BR, saberQuadrant_t::Q_R, saberQuadrant_t::Q_BL, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_WIDE,
     saberMoveName_t::LS_R_TR2BL, saberMoveName_t::LS_A_BL2TR, 150}, // LS_PARRY_LL

    // Reflecting a missile
    {"Reflect Top", animNumber_t::BOTH_P1_S1_T_, saberQuadrant_t::Q_R, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_WIDE,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_A_T2B, 300}, // LS_PARRY_UP,
    {"Reflect UR", animNumber_t::BOTH_P1_S1_TL, saberQuadrant_t::Q_R, saberQuadrant_t::Q_TR, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_WIDE,
     saberMoveName_t::LS_R_BR2TL, saberMoveName_t::LS_A_TL2BR, 300}, // LS_PARRY_UR,
    {"Reflect UL", animNumber_t::BOTH_P1_S1_TR, saberQuadrant_t::Q_R, saberQuadrant_t::Q_TL, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_WIDE,
     saberMoveName_t::LS_R_BL2TR, saberMoveName_t::LS_A_TR2BL, 300}, // LS_PARRY_UL,
    {"Reflect LR", animNumber_t::BOTH_P1_S1_BR, saberQuadrant_t::Q_R, saberQuadrant_t::Q_BL, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_WIDE,
     saberMoveName_t::LS_R_TR2BL, saberMoveName_t::LS_A_BL2TR, 300}, // LS_PARRY_LR
    {"Reflect LL", animNumber_t::BOTH_P1_S1_BL, saberQuadrant_t::Q_R, saberQuadrant_t::Q_BR, AFLAG_ACTIVE, 50, saberBlockType_t::BLK_WIDE,
     saberMoveName_t::LS_R_TL2BR, saberMoveName_t::LS_A_BR2TL, 300}, // LS_PARRY_LL,
}};

void BG_FixSaberMoveData(void)
{
#if defined(_GAME)
    const bool doFix = !!g_fixSaberMoveData.integer;
#elif defined(_CGAME)
    const char* cs = CG_ConfigString(CS_LEGACY_FIXES);
    const uint32_t legacyFixes = strtoul(cs, nullptr, 0);
    const bool doFix = !!(legacyFixes & Bit(legacyFixes_t::LEGACYFIX_SABERMOVEDATA));
#endif
    saberMoveData_t* move;

    for (move = saberMoveData.begin(); move != saberMoveData.end(); move++) {
        if (!strcmp(move->name, "BParry LR")) {
            move->animToUse = doFix ? animNumber_t::BOTH_H1_S1_BR : animNumber_t::BOTH_H1_S1_BL;
        }
        else if (!strcmp(move->name, "BParry LL")) {
            move->animToUse = doFix ? animNumber_t::BOTH_H1_S1_BL : animNumber_t::BOTH_H1_S1_BR;
        }
        else if (!strcmp(move->name, "Knock LR")) {
            move->animToUse = doFix ? animNumber_t::BOTH_K1_S1_BR : animNumber_t::BOTH_K1_S1_BL;
        }
        else if (!strcmp(move->name, "Knock LL")) {
            move->animToUse = doFix ? animNumber_t::BOTH_K1_S1_BL : animNumber_t::BOTH_K1_S1_BR;
        }
        else if (!strcmp(move->name, "Parry LR")) {
            move->animToUse = doFix ? animNumber_t::BOTH_P1_S1_BR : animNumber_t::BOTH_P1_S1_BL;
        }
        else if (!strcmp(move->name, "Parry LL")) {
            move->animToUse = doFix ? animNumber_t::BOTH_P1_S1_BL : animNumber_t::BOTH_P1_S1_BR;
        }
    }
}

EnumArray<EnumArray<saberMoveName_t, saberQuadrant_t, saberQuadrant_t::Q_NUM_QUADS>, saberQuadrant_t, saberQuadrant_t::Q_NUM_QUADS> transitionMove = {{
    {saberMoveName_t::LS_NONE, saberMoveName_t::LS_T1_BR__R, saberMoveName_t::LS_T1_BR_TR, saberMoveName_t::LS_T1_BR_T_, saberMoveName_t::LS_T1_BR_TL,
     saberMoveName_t::LS_T1_BR__L, saberMoveName_t::LS_T1_BR_BL, saberMoveName_t::LS_NONE},
    {saberMoveName_t::LS_T1__R_BR, saberMoveName_t::LS_NONE, saberMoveName_t::LS_T1__R_TR, saberMoveName_t::LS_T1__R_T_, saberMoveName_t::LS_T1__R_TL,
     saberMoveName_t::LS_T1__R__L, saberMoveName_t::LS_T1__R_BL, saberMoveName_t::LS_NONE},
    {saberMoveName_t::LS_T1_TR_BR, saberMoveName_t::LS_T1_TR__R, saberMoveName_t::LS_NONE, saberMoveName_t::LS_T1_TR_T_, saberMoveName_t::LS_T1_TR_TL,
     saberMoveName_t::LS_T1_TR__L, saberMoveName_t::LS_T1_TR_BL, saberMoveName_t::LS_NONE},
    {saberMoveName_t::LS_T1_T__BR, saberMoveName_t::LS_T1_T___R, saberMoveName_t::LS_T1_T__TR, saberMoveName_t::LS_NONE, saberMoveName_t::LS_T1_T__TL,
     saberMoveName_t::LS_T1_T___L, saberMoveName_t::LS_T1_T__BL, saberMoveName_t::LS_NONE},
    {saberMoveName_t::LS_T1_TL_BR, saberMoveName_t::LS_T1_TL__R, saberMoveName_t::LS_T1_TL_TR, saberMoveName_t::LS_T1_TL_T_, saberMoveName_t::LS_NONE,
     saberMoveName_t::LS_T1_TL__L, saberMoveName_t::LS_T1_TL_BL, saberMoveName_t::LS_NONE},
    {saberMoveName_t::LS_T1__L_BR, saberMoveName_t::LS_T1__L__R, saberMoveName_t::LS_T1__L_TR, saberMoveName_t::LS_T1__L_T_, saberMoveName_t::LS_T1__L_TL,
     saberMoveName_t::LS_NONE, saberMoveName_t::LS_T1__L_BL, saberMoveName_t::LS_NONE},
    {saberMoveName_t::LS_T1_BL_BR, saberMoveName_t::LS_T1_BL__R, saberMoveName_t::LS_T1_BL_TR, saberMoveName_t::LS_T1_BL_T_, saberMoveName_t::LS_T1_BL_TL,
     saberMoveName_t::LS_T1_BL__L, saberMoveName_t::LS_NONE, saberMoveName_t::LS_NONE},
    {saberMoveName_t::LS_T1_BL_BR, saberMoveName_t::LS_T1_BR__R, saberMoveName_t::LS_T1_BR_TR, saberMoveName_t::LS_T1_BR_T_, saberMoveName_t::LS_T1_BR_TL,
     saberMoveName_t::LS_T1_BR__L, saberMoveName_t::LS_T1_BR_BL, saberMoveName_t::LS_NONE},
}};

saberMoveName_t PM_AttackMoveForQuad(saberQuadrant_t quad)
{
    switch (quad) {
    case saberQuadrant_t::Q_B:
    case saberQuadrant_t::Q_BR:
        return saberMoveName_t::LS_A_BR2TL;
        break;
    case saberQuadrant_t::Q_R:
        return saberMoveName_t::LS_A_R2L;
        break;
    case saberQuadrant_t::Q_TR:
        return saberMoveName_t::LS_A_TR2BL;
        break;
    case saberQuadrant_t::Q_T:
        return saberMoveName_t::LS_A_T2B;
        break;
    case saberQuadrant_t::Q_TL:
        return saberMoveName_t::LS_A_TL2BR;
        break;
    case saberQuadrant_t::Q_L:
        return saberMoveName_t::LS_A_L2R;
        break;
    case saberQuadrant_t::Q_BL:
        return saberMoveName_t::LS_A_BL2TR;
        break;
    default:
        break;
    }
    return saberMoveName_t::LS_NONE;
}

bool PM_SaberKataDone(saberMoveName_t curmove, saberMoveName_t newmove);

saberMoveName_t PM_SaberAnimTransitionAnim(saberMoveName_t curmove, saberMoveName_t newmove)
{
    saberMoveName_t retmove = newmove;
    if (curmove == saberMoveName_t::LS_READY) { // just standing there
        switch (newmove) {
        case saberMoveName_t::LS_A_TL2BR:
        case saberMoveName_t::LS_A_L2R:
        case saberMoveName_t::LS_A_BL2TR:
        case saberMoveName_t::LS_A_BR2TL:
        case saberMoveName_t::LS_A_R2L:
        case saberMoveName_t::LS_A_TR2BL:
        case saberMoveName_t::LS_A_T2B:
            // transition is the start
            retmove = saberMoveName_t::LS_S_TL2BR + (newmove - saberMoveName_t::LS_A_TL2BR);
            break;
        default:
            break;
        }
    }
    else {
        switch (newmove) {
        // transitioning to ready pose
        case saberMoveName_t::LS_READY:
            switch (curmove) {
            // transitioning from an attack
            case saberMoveName_t::LS_A_TL2BR:
            case saberMoveName_t::LS_A_L2R:
            case saberMoveName_t::LS_A_BL2TR:
            case saberMoveName_t::LS_A_BR2TL:
            case saberMoveName_t::LS_A_R2L:
            case saberMoveName_t::LS_A_TR2BL:
            case saberMoveName_t::LS_A_T2B:
                // transition is the return
                retmove = saberMoveName_t::LS_R_TL2BR + (curmove - saberMoveName_t::LS_A_TL2BR);
                break;
            default:
                break;
            }
            break;
        // transitioning to an attack
        case saberMoveName_t::LS_A_TL2BR:
        case saberMoveName_t::LS_A_L2R:
        case saberMoveName_t::LS_A_BL2TR:
        case saberMoveName_t::LS_A_BR2TL:
        case saberMoveName_t::LS_A_R2L:
        case saberMoveName_t::LS_A_TR2BL:
        case saberMoveName_t::LS_A_T2B:
            if (newmove == curmove) {
                // going into an attack
                if (PM_SaberKataDone(curmove, newmove)) { // done with this kata, must return to ready before attack again
                    retmove = saberMoveName_t::LS_R_TL2BR + (newmove - saberMoveName_t::LS_A_TL2BR);
                }
                else { // okay to chain to another attack
                    retmove = transitionMove[saberMoveData[curmove].endQuad][saberMoveData[newmove].startQuad];
                }
            }
            else if (saberMoveData[curmove].endQuad == saberMoveData[newmove].startQuad) { // new move starts from same quadrant
                retmove = newmove;
            }
            else {
                switch (curmove) {
                // transitioning from an attack
                case saberMoveName_t::LS_A_TL2BR:
                case saberMoveName_t::LS_A_L2R:
                case saberMoveName_t::LS_A_BL2TR:
                case saberMoveName_t::LS_A_BR2TL:
                case saberMoveName_t::LS_A_R2L:
                case saberMoveName_t::LS_A_TR2BL:
                case saberMoveName_t::LS_A_T2B:
                case saberMoveName_t::LS_D1_BR:
                case saberMoveName_t::LS_D1__R:
                case saberMoveName_t::LS_D1_TR:
                case saberMoveName_t::LS_D1_T_:
                case saberMoveName_t::LS_D1_TL:
                case saberMoveName_t::LS_D1__L:
                case saberMoveName_t::LS_D1_BL:
                case saberMoveName_t::LS_D1_B_:
                    retmove = transitionMove[saberMoveData[curmove].endQuad][saberMoveData[newmove].startQuad];
                    break;
                // transitioning from a return
                case saberMoveName_t::LS_R_TL2BR:
                case saberMoveName_t::LS_R_L2R:
                case saberMoveName_t::LS_R_BL2TR:
                case saberMoveName_t::LS_R_BR2TL:
                case saberMoveName_t::LS_R_R2L:
                case saberMoveName_t::LS_R_TR2BL:
                case saberMoveName_t::LS_R_T2B:
                // transitioning from a bounce
                /*
                case LS_BOUNCE_UL2LL:
                case LS_BOUNCE_LL2UL:
                case LS_BOUNCE_L2LL:
                case LS_BOUNCE_L2UL:
                case LS_BOUNCE_UR2LR:
                case LS_BOUNCE_LR2UR:
                case LS_BOUNCE_R2LR:
                case LS_BOUNCE_R2UR:
                case LS_BOUNCE_TOP:
                case LS_OVER_UR2UL:
                case LS_OVER_UL2UR:
                case LS_BOUNCE_UR:
                case LS_BOUNCE_UL:
                case LS_BOUNCE_LR:
                case LS_BOUNCE_LL:
                */
                // transitioning from a parry/reflection/knockaway/broken parry
                case saberMoveName_t::LS_PARRY_UP:
                case saberMoveName_t::LS_PARRY_UR:
                case saberMoveName_t::LS_PARRY_UL:
                case saberMoveName_t::LS_PARRY_LR:
                case saberMoveName_t::LS_PARRY_LL:
                case saberMoveName_t::LS_REFLECT_UP:
                case saberMoveName_t::LS_REFLECT_UR:
                case saberMoveName_t::LS_REFLECT_UL:
                case saberMoveName_t::LS_REFLECT_LR:
                case saberMoveName_t::LS_REFLECT_LL:
                case saberMoveName_t::LS_K1_T_:
                case saberMoveName_t::LS_K1_TR:
                case saberMoveName_t::LS_K1_TL:
                case saberMoveName_t::LS_K1_BR:
                case saberMoveName_t::LS_K1_BL:
                case saberMoveName_t::LS_V1_BR:
                case saberMoveName_t::LS_V1__R:
                case saberMoveName_t::LS_V1_TR:
                case saberMoveName_t::LS_V1_T_:
                case saberMoveName_t::LS_V1_TL:
                case saberMoveName_t::LS_V1__L:
                case saberMoveName_t::LS_V1_BL:
                case saberMoveName_t::LS_V1_B_:
                case saberMoveName_t::LS_H1_T_:
                case saberMoveName_t::LS_H1_TR:
                case saberMoveName_t::LS_H1_TL:
                case saberMoveName_t::LS_H1_BR:
                case saberMoveName_t::LS_H1_BL:
                    retmove = transitionMove[saberMoveData[curmove].endQuad][saberMoveData[newmove].startQuad];
                    break;
                // NB: transitioning from transitions is fine
                default:
                    break;
                }
            }
            break;
        // transitioning to any other anim is not supported
        default:
            break;
        }
    }

    if (retmove == saberMoveName_t::LS_NONE) {
        return newmove;
    }

    return retmove;
}

extern bool BG_InKnockDown(animNumber_t anim);
saberMoveName_t PM_CheckStabDown(void)
{
    vec3_t faceFwd, facingAngles;
    vec3_t fwd;
    bgEntity_t* ent = nullptr;
    trace_t tr;
    // yeah, vm's may complain, but.. who cares!
    vec3_t trmins = {-15, -15, -15};
    vec3_t trmaxs = {15, 15, 15};

    saberInfo_t* saber1 = BG_MySaber(pm->ps->clientNum, 0);
    saberInfo_t* saber2 = BG_MySaber(pm->ps->clientNum, 1);
    if (saber1 && Any(saber1->saberFlags & saberFlags_t::SFL_NO_STABDOWN)) {
        return saberMoveName_t::LS_NONE;
    }
    if (saber2 && Any(saber2->saberFlags & saberFlags_t::SFL_NO_STABDOWN)) {
        return saberMoveName_t::LS_NONE;
    }

    if (pm->ps->groundEntityNum == ENTITYNUM_NONE) { // sorry must be on ground!
        return saberMoveName_t::LS_NONE;
    }
    if (pm->ps->clientNum < MAX_CLIENTS) { // player
        pm->ps->velocity[2] = 0;
        pm->cmd.upmove = 0;
    }

    VectorSet(facingAngles, 0, pm->ps->viewangles[YAW], 0);
    AngleVectors(facingAngles, faceFwd, nullptr, nullptr);

    // FIXME: need to only move forward until we bump into our target...?
    VectorMA(pm->ps->origin, 164.0f, faceFwd, fwd);

    pm->trace(&tr, pm->ps->origin, trmins, trmaxs, fwd, pm->ps->clientNum, MASK_PLAYERSOLID);

    if (tr.entityNum < ENTITYNUM_WORLD) {
        ent = PM_BGEntForNum(tr.entityNum);
    }

    if (ent && (ent->s.eType == entityType_t::ET_PLAYER || ent->s.eType == entityType_t::ET_NPC) &&
        BG_InKnockDown(ES_LegsAnim(ent->s))) { // guy is on the ground below me, do a top-down attack
        if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_DUAL) {
            return saberMoveName_t::LS_STABDOWN_DUAL;
        }
        else if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_STAFF) {
            return saberMoveName_t::LS_STABDOWN_STAFF;
        }
        else {
            return saberMoveName_t::LS_STABDOWN;
        }
    }
    return saberMoveName_t::LS_NONE;
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
        else { // Not moving at all
            return saberQuadrant_t::Q_R;
        }
    }
}

//===================================================================
bool PM_SaberInBounce(saberMoveName_t move)
{
    if (move >= saberMoveName_t::LS_B1_BR && move <= saberMoveName_t::LS_B1_BL) {
        return true;
    }
    if (move >= saberMoveName_t::LS_D1_BR && move <= saberMoveName_t::LS_D1_BL) {
        return true;
    }
    return false;
}

bool PM_SaberInTransition(saberMoveName_t move);

EnumArray<EnumArray<int, saberQuadrant_t, saberQuadrant_t::Q_NUM_QUADS>, saberQuadrant_t, saberQuadrant_t::Q_NUM_QUADS> saberMoveTransitionAngle = {{
    //		Q_BR,Q_BR,	Q_BR,Q_R,	Q_BR,Q_TR,	Q_BR,Q_T,	Q_BR,Q_TL,	Q_BR,Q_L,	Q_BR,Q_BL,	Q_BR,Q_B,
    {0, 45, 90, 135, 180, 215, 270, 45},
    //		Q_R,Q_BR,	Q_R,Q_R,	Q_R,Q_TR,	Q_R,Q_T,	Q_R,Q_TL,	Q_R,Q_L,	Q_R,Q_BL,	Q_R,Q_B,
    {45, 0, 45, 90, 135, 180, 215, 90},
    //		Q_TR,Q_BR,	Q_TR,Q_R,	Q_TR,Q_TR,	Q_TR,Q_T,	Q_TR,Q_TL,	Q_TR,Q_L,	Q_TR,Q_BL,	Q_TR,Q_B,
    {90, 45, 0, 45, 90, 135, 180, 135},
    //		Q_T,Q_BR,	Q_T,Q_R,	Q_T,Q_TR,	Q_T,Q_T,	Q_T,Q_TL,	Q_T,Q_L,	Q_T,Q_BL,	Q_T,Q_B,
    {135, 90, 45, 0, 45, 90, 135, 180},
    //		Q_TL,Q_BR,	Q_TL,Q_R,	Q_TL,Q_TR,	Q_TL,Q_T,	Q_TL,Q_TL,	Q_TL,Q_L,	Q_TL,Q_BL,	Q_TL,Q_B,
    {180, 135, 90, 45, 0, 45, 90, 135},
    //		Q_L,Q_BR,	Q_L,Q_R,	Q_L,Q_TR,	Q_L,Q_T,	Q_L,Q_TL,	Q_L,Q_L,	Q_L,Q_BL,	Q_L,Q_B,
    {215, 180, 135, 90, 45, 0, 45, 90},
    //		Q_BL,Q_BR,	Q_BL,Q_R,	Q_BL,Q_TR,	Q_BL,Q_T,	Q_BL,Q_TL,	Q_BL,Q_L,	Q_BL,Q_BL,	Q_BL,Q_B,
    {270, 215, 180, 135, 90, 45, 0, 45},
    //		Q_B,Q_BR,	Q_B,Q_R,	Q_B,Q_TR,	Q_B,Q_T,	Q_B,Q_TL,	Q_B,Q_L,	Q_B,Q_BL,	Q_B,Q_B,
    {45, 90, 135, 180, 135, 90, 45, 0},
}};

int PM_SaberAttackChainAngle(saberMoveName_t move1, saberMoveName_t move2)
{
    if (move1 == saberMoveName_t::LS_INVALID || move2 == saberMoveName_t::LS_INVALID) {
        return -1;
    }
    return saberMoveTransitionAngle[saberMoveData[move1].endQuad][saberMoveData[move2].startQuad];
}

bool PM_SaberKataDone(saberMoveName_t curmove, saberMoveName_t newmove)
{
    if (pm->ps->m_iVehicleNum) { // never continue kata on vehicle
        if (pm->ps->saberAttackChainCount > 0) {
            return true;
        }
    }

    if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_DESANN ||
        pm->ps->fd.saberAnimLevel == saber_styles_t::SS_TAVION) { // desann and tavion can link up as many attacks as they want
        return false;
    }

    if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_STAFF) {
        // TEMP: for now, let staff attacks infinitely chain
        return false;
    }
    else if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_DUAL) {
        // TEMP: for now, let staff attacks infinitely chain
        return false;
    }
    else if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_STRONG) {
        if (curmove == saberMoveName_t::LS_NONE || newmove == saberMoveName_t::LS_NONE) {
            if (pm->ps->fd.saberAnimLevel >= saber_styles_t::SS_STRONG && pm->ps->saberAttackChainCount > PM_irand_timesync(0, 1)) {
                return true;
            }
        }
        else if (pm->ps->saberAttackChainCount > PM_irand_timesync(2, 3)) {
            return true;
        }
        else if (pm->ps->saberAttackChainCount > 0) {
            int chainAngle = PM_SaberAttackChainAngle(curmove, newmove);
            if (chainAngle < 135 || chainAngle > 215) { // if trying to chain to a move that doesn't continue the momentum
                return true;
            }
            else if (chainAngle == 180) { // continues the momentum perfectly, allow it to chain 66% of the time
                if (pm->ps->saberAttackChainCount > 1) {
                    return true;
                }
            }
            else { // would continue the movement somewhat, 50% chance of continuing
                if (pm->ps->saberAttackChainCount > 2) {
                    return true;
                }
            }
        }
    }
    else { // Perhaps have chainAngle influence fast and medium chains as well? For now, just do level 3.
        if (newmove == saberMoveName_t::LS_A_TL2BR || newmove == saberMoveName_t::LS_A_L2R || newmove == saberMoveName_t::LS_A_BL2TR ||
            newmove == saberMoveName_t::LS_A_BR2TL || newmove == saberMoveName_t::LS_A_R2L ||
            newmove == saberMoveName_t::LS_A_TR2BL) { // lower chaining tolerance for spinning saber anims
            int chainTolerance;

            if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_FAST) {
                chainTolerance = 5;
            }
            else {
                chainTolerance = 3;
            }

            if (pm->ps->saberAttackChainCount >= chainTolerance && PM_irand_timesync(1, pm->ps->saberAttackChainCount) > chainTolerance) {
                return true;
            }
        }
        if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_MEDIUM && pm->ps->saberAttackChainCount > PM_irand_timesync(2, 5)) {
            return true;
        }
    }
    return false;
}

void PM_SetAnimFrame(playerState_t* gent, int frame, bool torso, bool legs)
{
    gent->saberLockFrame = frame;
}

std::optional<animNumber_t> PM_SaberLockWinAnim(bool victory, bool superBreak)
{
    std::optional<animNumber_t> winAnim;
    switch (pm->ps->torsoAnim) {
        /*
            default:
        #ifndef FINAL_BUILD
                Q_Println(S_COLOR_RED"ERROR-PM_SaberLockBreak: {} not in saberlock anim, anim = ({}){}", pm->gent->NPC_type, to_underlying(
        pm->ps->torsoAnim ), BG_AnimName( pm->ps->torsoAnim )); #endif
        */
    case animNumber_t::BOTH_BF2LOCK:
        if (superBreak) {
            winAnim = animNumber_t::BOTH_LK_S_S_T_SB_1_W;
        }
        else if (!victory) {
            winAnim = animNumber_t::BOTH_BF1BREAK;
        }
        else {
            pm->ps->saberMove = saberMoveName_t::LS_A_T2B;
            winAnim = animNumber_t::BOTH_A3_T__B_;
        }
        break;
    case animNumber_t::BOTH_BF1LOCK:
        if (superBreak) {
            winAnim = animNumber_t::BOTH_LK_S_S_T_SB_1_W;
        }
        else if (!victory) {
            winAnim = animNumber_t::BOTH_KNOCKDOWN4;
        }
        else {
            pm->ps->saberMove = saberMoveName_t::LS_K1_T_;
            winAnim = animNumber_t::BOTH_K1_S1_T_;
        }
        break;
    case animNumber_t::BOTH_CWCIRCLELOCK:
        if (superBreak) {
            winAnim = animNumber_t::BOTH_LK_S_S_S_SB_1_W;
        }
        else if (!victory) {
            pm->ps->saberMove = saberMoveName_t::LS_V1_BL; // pm->ps->saberBounceMove =
            pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_PARRY_BROKEN;
            winAnim = animNumber_t::BOTH_V1_BL_S1;
        }
        else {
            winAnim = animNumber_t::BOTH_CWCIRCLEBREAK;
        }
        break;
    case animNumber_t::BOTH_CCWCIRCLELOCK:
        if (superBreak) {
            winAnim = animNumber_t::BOTH_LK_S_S_S_SB_1_W;
        }
        else if (!victory) {
            pm->ps->saberMove = saberMoveName_t::LS_V1_BR; // pm->ps->saberBounceMove =
            pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_PARRY_BROKEN;
            winAnim = animNumber_t::BOTH_V1_BR_S1;
        }
        else {
            winAnim = animNumber_t::BOTH_CCWCIRCLEBREAK;
        }
        break;
    default:
        // must be using new system:
        break;
    }
    if (winAnim) {
        PM_SetAnim(setAnimParts_t::SETANIM_BOTH, *winAnim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
        pm->ps->weaponTime = pm->ps->torsoTimer;
        pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_NONE;
        pm->ps->weaponstate = weaponstate_t::WEAPON_FIRING;
        /*
        if ( superBreak
            && winAnim != animNumber_t::BOTH_LK_ST_DL_T_SB_1_W )
        {//going to attack with saber, do a saber trail
            pm->ps->SaberActivateTrail( 200 );
        }
        */
    }
    return winAnim;
}

// Need to avoid nesting namespaces!
#ifdef _GAME // including game headers on cgame is FORBIDDEN ^_^
#include "g_local.h"
extern void NPC_SetAnim(gentity_t* ent, setAnimParts_t setAnimParts, animNumber_t anim, setAnimFlags_t setAnimFlags);
extern gentity_t g_entities[];
#elif defined(_CGAME)
#include <cgame/cg_local.h> //ahahahahhahahaha@$!$!
#endif

std::optional<animNumber_t> PM_SaberLockLoseAnim(playerState_t* genemy, bool victory, bool superBreak)
{
    std::optional<animNumber_t> loseAnim;
    switch (genemy->torsoAnim) {
        /*
            default:
        #ifndef FINAL_BUILD
                Q_Println(S_COLOR_RED"ERROR-PM_SaberLockBreak: {} not in saberlock anim, anim = ({}){}", genemy->NPC_type, to_underlying(
        genemy->client->ps.torsoAnim ), BG_AnimName( genemy->client->ps.torsoAnim )); #endif
        */
    case animNumber_t::BOTH_BF2LOCK:
        if (superBreak) {
            loseAnim = animNumber_t::BOTH_LK_S_S_T_SB_1_L;
        }
        else if (!victory) {
            loseAnim = animNumber_t::BOTH_BF1BREAK;
        }
        else {
            if (!victory) { // no-one won
                genemy->saberMove = saberMoveName_t::LS_K1_T_;
                loseAnim = animNumber_t::BOTH_K1_S1_T_;
            }
            else { // FIXME: this anim needs to transition back to ready when done
                loseAnim = animNumber_t::BOTH_BF1BREAK;
            }
        }
        break;
    case animNumber_t::BOTH_BF1LOCK:
        if (superBreak) {
            loseAnim = animNumber_t::BOTH_LK_S_S_T_SB_1_L;
        }
        else if (!victory) {
            loseAnim = animNumber_t::BOTH_KNOCKDOWN4;
        }
        else {
            if (!victory) { // no-one won
                genemy->saberMove = saberMoveName_t::LS_A_T2B;
                loseAnim = animNumber_t::BOTH_A3_T__B_;
            }
            else {
                loseAnim = animNumber_t::BOTH_KNOCKDOWN4;
            }
        }
        break;
    case animNumber_t::BOTH_CWCIRCLELOCK:
        if (superBreak) {
            loseAnim = animNumber_t::BOTH_LK_S_S_S_SB_1_L;
        }
        else if (!victory) {
            genemy->saberMove = saberMoveName_t::LS_V1_BL; // genemy->saberBounceMove =
            genemy->saberBlocked = saberBlockedType_t::BLOCKED_PARRY_BROKEN;
            loseAnim = animNumber_t::BOTH_V1_BL_S1;
        }
        else {
            if (!victory) { // no-one won
                loseAnim = animNumber_t::BOTH_CCWCIRCLEBREAK;
            }
            else {
                genemy->saberMove = saberMoveName_t::LS_V1_BL; // genemy->saberBounceMove =
                genemy->saberBlocked = saberBlockedType_t::BLOCKED_PARRY_BROKEN;
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
        if (superBreak) {
            loseAnim = animNumber_t::BOTH_LK_S_S_S_SB_1_L;
        }
        else if (!victory) {
            genemy->saberMove = saberMoveName_t::LS_V1_BR; // genemy->saberBounceMove =
            genemy->saberBlocked = saberBlockedType_t::BLOCKED_PARRY_BROKEN;
            loseAnim = animNumber_t::BOTH_V1_BR_S1;
        }
        else {
            if (!victory) { // no-one won
                loseAnim = animNumber_t::BOTH_CWCIRCLEBREAK;
            }
            else {
                genemy->saberMove = saberMoveName_t::LS_V1_BR; // genemy->saberBounceMove =
                genemy->saberBlocked = saberBlockedType_t::BLOCKED_PARRY_BROKEN;
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
    if (loseAnim) {
#ifdef _GAME
        NPC_SetAnim(&g_entities[genemy->clientNum], setAnimParts_t::SETANIM_BOTH, *loseAnim,
                    setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
        genemy->weaponTime = genemy->torsoTimer; // + 250;
#endif
        genemy->saberBlocked = saberBlockedType_t::BLOCKED_NONE;
        genemy->weaponstate = weaponstate_t::WEAPON_READY;
    }
    return loseAnim;
}

std::optional<animNumber_t> PM_SaberLockResultAnim(playerState_t* duelist, bool superBreak, bool won)
{
    animNumber_t baseAnim = duelist->torsoAnim;
    switch (baseAnim) {
    case animNumber_t::BOTH_LK_S_S_S_L_2: // lock if I'm using single vs. a single and other intitiated
        baseAnim = animNumber_t::BOTH_LK_S_S_S_L_1;
        break;
    case animNumber_t::BOTH_LK_S_S_T_L_2: // lock if I'm using single vs. a single and other initiated
        baseAnim = animNumber_t::BOTH_LK_S_S_T_L_1;
        break;
    case animNumber_t::BOTH_LK_DL_DL_S_L_2: // lock if I'm using dual vs. dual and other initiated
        baseAnim = animNumber_t::BOTH_LK_DL_DL_S_L_1;
        break;
    case animNumber_t::BOTH_LK_DL_DL_T_L_2: // lock if I'm using dual vs. dual and other initiated
        baseAnim = animNumber_t::BOTH_LK_DL_DL_T_L_1;
        break;
    case animNumber_t::BOTH_LK_ST_ST_S_L_2: // lock if I'm using staff vs. a staff and other initiated
        baseAnim = animNumber_t::BOTH_LK_ST_ST_S_L_1;
        break;
    case animNumber_t::BOTH_LK_ST_ST_T_L_2: // lock if I'm using staff vs. a staff and other initiated
        baseAnim = animNumber_t::BOTH_LK_ST_ST_T_L_1;
        break;
    default:
        break;
    }
    // what kind of break?
    if (!superBreak) {
        baseAnim -= 2;
    }
    else if (superBreak) {
        baseAnim += 1;
    }
    else { // WTF?  Not a valid result
        return std::nullopt;
    }
    // win or lose?
    if (won) {
        baseAnim += 1;
    }

    // play the anim and hold it
#ifdef _GAME
    // server-side: set it on the other guy, too
    if (duelist->clientNum == pm->ps->clientNum) { // me
        PM_SetAnim(setAnimParts_t::SETANIM_BOTH, baseAnim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
    }
    else { // other guy
        NPC_SetAnim(&g_entities[duelist->clientNum], setAnimParts_t::SETANIM_BOTH, baseAnim,
                    setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
    }
#else
    PM_SetAnim(setAnimParts_t::SETANIM_BOTH, baseAnim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
#endif

    if (superBreak && !won) { // if you lose a superbreak, you're defenseless
                              /*
                              //Taken care of in SetSaberBoxSize()
                              //make saberent not block
                              gentity_t *saberent = &g_entities[duelist->client->ps.saberEntityNum];
                              if ( saberent )
                              {
                                  VectorClear(saberent->mins);
                                  VectorClear(saberent->maxs);
                                  G_SetOrigin(saberent, duelist->currentOrigin);
                              }
                              */
#ifdef _GAME
        if (1)
#else
        if (duelist->clientNum == pm->ps->clientNum)
#endif
        {
            // set sabermove to none
            duelist->saberMove = saberMoveName_t::LS_NONE;
            // Hold the anim a little longer than it is
            duelist->torsoTimer += 250;
        }
    }

#ifdef _GAME
    if (1)
#else
    if (duelist->clientNum == pm->ps->clientNum)
#endif
    {
        // no attacking during this anim
        duelist->weaponTime = duelist->torsoTimer;
        duelist->saberBlocked = saberBlockedType_t::BLOCKED_NONE;
        /*
        if ( superBreak
            && won
            && baseAnim != animNumber_t::BOTH_LK_ST_DL_T_SB_1_W )
        {//going to attack with saber, do a saber trail
            duelist->client->ps.SaberActivateTrail( 200 );
        }
        */
    }
    return baseAnim;
}

void PM_SaberLockBreak(playerState_t* genemy, bool victory, int strength)
{
    // bool punishLoser = false;
    bool noKnockdown = false;
    bool superBreak = (strength + pm->ps->saberLockHits > Q_irand(2, 4));

    // a single vs. single break
    if (PM_SaberLockWinAnim(victory, superBreak))
        PM_SaberLockLoseAnim(genemy, victory, superBreak);

    // must be a saberlock that's not between single and single...
    else {
        PM_SaberLockResultAnim(pm->ps, superBreak, true);
        pm->ps->weaponstate = weaponstate_t::WEAPON_FIRING;
        PM_SaberLockResultAnim(genemy, superBreak, false);
        genemy->weaponstate = weaponstate_t::WEAPON_READY;
    }

    if (victory) {                                  // someone lost the lock, so punish them by knocking them down
        if (pm->ps->saberLockHits && !superBreak) { // there was some over-power in the win, but not enough to superbreak
            vec3_t oppDir;

            int newstrength = 8;

            VectorSubtract(genemy->origin, pm->ps->origin, oppDir);
            VectorNormalize(oppDir);

            if (noKnockdown) {
                if (!genemy->saberEntityNum) { // if he has already lost his saber then just knock him down
                    noKnockdown = false;
                }
            }

            if (!noKnockdown && BG_KnockDownable(genemy)) {
                genemy->forceHandExtend = forceHandAnims_t::HANDEXTEND_KNOCKDOWN;
                genemy->forceHandExtendTime = pm->cmd.serverTime + 1100;
                genemy->forceDodgeAnim = 0; // this toggles between 1 and 0, when it's 1 we should play the get up anim

                genemy->otherKiller = pm->ps->clientNum;
                genemy->otherKillerTime = pm->cmd.serverTime + 5000;
                genemy->otherKillerDebounceTime = pm->cmd.serverTime + 100;

                genemy->velocity[0] = oppDir[0] * (newstrength * 40);
                genemy->velocity[1] = oppDir[1] * (newstrength * 40);
                genemy->velocity[2] = 100;
            }

            pm->checkDuelLoss = genemy->clientNum + 1;

            pm->ps->saberEventFlags |= saberEventFlags_t::SEF_LOCK_WON;
        }
    }
    else { // If no one lost, then shove each player away from the other
        vec3_t oppDir;

        int newstrength = 4;

        VectorSubtract(genemy->origin, pm->ps->origin, oppDir);
        VectorNormalize(oppDir);
        genemy->velocity[0] = oppDir[0] * (newstrength * 40);
        genemy->velocity[1] = oppDir[1] * (newstrength * 40);
        genemy->velocity[2] = 150;

        VectorSubtract(pm->ps->origin, genemy->origin, oppDir);
        VectorNormalize(oppDir);
        pm->ps->velocity[0] = oppDir[0] * (newstrength * 40);
        pm->ps->velocity[1] = oppDir[1] * (newstrength * 40);
        pm->ps->velocity[2] = 150;

        genemy->forceHandExtend = forceHandAnims_t::HANDEXTEND_WEAPONREADY;
    }

    pm->ps->weaponTime = 0;
    genemy->weaponTime = 0;

    pm->ps->saberLockTime = genemy->saberLockTime = 0;
    pm->ps->saberLockFrame = genemy->saberLockFrame = 0;
    pm->ps->saberLockEnemy = genemy->saberLockEnemy = 0;

    pm->ps->forceHandExtend = forceHandAnims_t::HANDEXTEND_WEAPONREADY;

    PM_AddEvent(entity_event_t::EV_JUMP);
    if (!victory) { // no-one won
        BG_AddPredictableEventToPlayerstate(entity_event_t::EV_JUMP, 0, genemy);
    }
    else {
        if (PM_irand_timesync(0, 1)) {
            BG_AddPredictableEventToPlayerstate(entity_event_t::EV_JUMP, PM_irand_timesync(0, 75), genemy);
        }
    }
}

bool BG_CheckIncrementLockAnim(animNumber_t anim, saberLockType_t winOrLose)
{
    bool increment = false; //???
    // RULE: if you are the first style in the lock anim, you advance from LOSING position to WINNING position
    //		if you are the second style in the lock anim, you advance from WINNING position to LOSING position
    switch (anim) {
    // increment to win:
    case animNumber_t::BOTH_LK_DL_DL_S_L_1: // lock if I'm using dual vs. dual and I initiated
    case animNumber_t::BOTH_LK_DL_DL_S_L_2: // lock if I'm using dual vs. dual and other initiated
    case animNumber_t::BOTH_LK_DL_DL_T_L_1: // lock if I'm using dual vs. dual and I initiated
    case animNumber_t::BOTH_LK_DL_DL_T_L_2: // lock if I'm using dual vs. dual and other initiated
    case animNumber_t::BOTH_LK_DL_S_S_L_1:  // lock if I'm using dual vs. a single
    case animNumber_t::BOTH_LK_DL_S_T_L_1:  // lock if I'm using dual vs. a single
    case animNumber_t::BOTH_LK_DL_ST_S_L_1: // lock if I'm using dual vs. a staff
    case animNumber_t::BOTH_LK_DL_ST_T_L_1: // lock if I'm using dual vs. a staff
    case animNumber_t::BOTH_LK_S_S_S_L_1:   // lock if I'm using single vs. a single and I initiated
    case animNumber_t::BOTH_LK_S_S_T_L_2:   // lock if I'm using single vs. a single and other initiated
    case animNumber_t::BOTH_LK_ST_S_S_L_1:  // lock if I'm using staff vs. a single
    case animNumber_t::BOTH_LK_ST_S_T_L_1:  // lock if I'm using staff vs. a single
    case animNumber_t::BOTH_LK_ST_ST_T_L_1: // lock if I'm using staff vs. a staff and I initiated
    case animNumber_t::BOTH_LK_ST_ST_T_L_2: // lock if I'm using staff vs. a staff and other initiated
        if (winOrLose == saberLockType_t::SABERLOCK_WIN) {
            increment = true;
        }
        else {
            increment = false;
        }
        break;

    // decrement to win:
    case animNumber_t::BOTH_LK_S_DL_S_L_1:  // lock if I'm using single vs. a dual
    case animNumber_t::BOTH_LK_S_DL_T_L_1:  // lock if I'm using single vs. a dual
    case animNumber_t::BOTH_LK_S_S_S_L_2:   // lock if I'm using single vs. a single and other intitiated
    case animNumber_t::BOTH_LK_S_S_T_L_1:   // lock if I'm using single vs. a single and I initiated
    case animNumber_t::BOTH_LK_S_ST_S_L_1:  // lock if I'm using single vs. a staff
    case animNumber_t::BOTH_LK_S_ST_T_L_1:  // lock if I'm using single vs. a staff
    case animNumber_t::BOTH_LK_ST_DL_S_L_1: // lock if I'm using staff vs. dual
    case animNumber_t::BOTH_LK_ST_DL_T_L_1: // lock if I'm using staff vs. dual
    case animNumber_t::BOTH_LK_ST_ST_S_L_1: // lock if I'm using staff vs. a staff and I initiated
    case animNumber_t::BOTH_LK_ST_ST_S_L_2: // lock if I'm using staff vs. a staff and other initiated
        if (winOrLose == saberLockType_t::SABERLOCK_WIN) {
            increment = false;
        }
        else {
            increment = true;
        }
        break;
    default:
        break;
    }
    return increment;
}

extern bool ValidAnimFileIndex(int index);
void PM_SaberLocked(void)
{
    int remaining = 0;
    playerState_t* genemy;
    bgEntity_t* eGenemy = PM_BGEntForNum(pm->ps->saberLockEnemy);

    if (!eGenemy) {
        return;
    }

    genemy = eGenemy->playerState;

    if (!genemy) {
        return;
    }
    /*if ( ( (pm->ps->torsoAnim) == animNumber_t::BOTH_BF2LOCK ||
            (pm->ps->torsoAnim) == animNumber_t::BOTH_BF1LOCK ||
            (pm->ps->torsoAnim) == animNumber_t::BOTH_CWCIRCLELOCK ||
            (pm->ps->torsoAnim) == animNumber_t::BOTH_CCWCIRCLELOCK )
        && ( (genemy->torsoAnim) == animNumber_t::BOTH_BF2LOCK ||
            (genemy->torsoAnim) == animNumber_t::BOTH_BF1LOCK ||
            (genemy->torsoAnim) == animNumber_t::BOTH_CWCIRCLELOCK ||
            (genemy->torsoAnim) == animNumber_t::BOTH_CCWCIRCLELOCK )
        )
        */ //yeah..
    if (pm->ps->saberLockFrame && genemy->saberLockFrame && BG_InSaberLock(pm->ps->torsoAnim) && BG_InSaberLock(genemy->torsoAnim)) {
        float dist = 0;

        pm->ps->torsoTimer = 0;
        pm->ps->weaponTime = 0;
        genemy->torsoTimer = 0;
        genemy->weaponTime = 0;

        dist = DistanceSquared(pm->ps->origin, genemy->origin);
        if (dist < 64 || dist > 6400) { // between 8 and 80 from each other
            PM_SaberLockBreak(genemy, false, 0);
            return;
        }
        /*
        //NOTE: time-out is handled around where PM_SaberLocked is called
        if ( pm->ps->saberLockTime <= pm->cmd.serverTime + 500 )
        {//lock just ended
            PM_SaberLockBreak( genemy, false, 0 );
            return;
        }
        */
        if (pm->ps->saberLockAdvance) { // holding attack
            animation_t* anim;
            float currentFrame;
            int curFrame;
            int strength = to_underlying(pm->ps->fd.forcePowerLevel[forcePowers_t::FP_SABER_OFFENSE]) + 1;

            pm->ps->saberLockAdvance = false;

            anim = &(*pm->animations)[pm->ps->torsoAnim];

            currentFrame = pm->ps->saberLockFrame;

            // advance/decrement my frame number
            if (BG_InSaberLockOld(pm->ps->torsoAnim)) { // old locks
                if ((pm->ps->torsoAnim) == animNumber_t::BOTH_CCWCIRCLELOCK || (pm->ps->torsoAnim) == animNumber_t::BOTH_BF2LOCK) {
                    curFrame = Q_floor(currentFrame) - strength;
                    // drop my frame one
                    if (curFrame <= anim->firstFrame) { // I won!  Break out
                        PM_SaberLockBreak(genemy, true, strength);
                        return;
                    }
                    else {
                        PM_SetAnimFrame(pm->ps, curFrame, true, true);
                        remaining = curFrame - anim->firstFrame;
                    }
                }
                else {
                    curFrame = Q_ceil(currentFrame) + strength;
                    // advance my frame one
                    if (curFrame >= anim->firstFrame + anim->numFrames) { // I won!  Break out
                        PM_SaberLockBreak(genemy, true, strength);
                        return;
                    }
                    else {
                        PM_SetAnimFrame(pm->ps, curFrame, true, true);
                        remaining = anim->firstFrame + anim->numFrames - curFrame;
                    }
                }
            }
            else { // new locks
                if (BG_CheckIncrementLockAnim(pm->ps->torsoAnim, saberLockType_t::SABERLOCK_WIN)) {
                    curFrame = Q_ceil(currentFrame) + strength;
                    // advance my frame one
                    if (curFrame >= anim->firstFrame + anim->numFrames) { // I won!  Break out
                        PM_SaberLockBreak(genemy, true, strength);
                        return;
                    }
                    else {
                        PM_SetAnimFrame(pm->ps, curFrame, true, true);
                        remaining = anim->firstFrame + anim->numFrames - curFrame;
                    }
                }
                else {
                    curFrame = Q_floor(currentFrame) - strength;
                    // drop my frame one
                    if (curFrame <= anim->firstFrame) { // I won!  Break out
                        PM_SaberLockBreak(genemy, true, strength);
                        return;
                    }
                    else {
                        PM_SetAnimFrame(pm->ps, curFrame, true, true);
                        remaining = curFrame - anim->firstFrame;
                    }
                }
            }
            if (!PM_irand_timesync(0, 2)) {
                PM_AddEvent(entity_event_t::EV_JUMP);
            }
            // advance/decrement enemy frame number
            anim = &(*pm->animations)[(genemy->torsoAnim)];

            if (BG_InSaberLockOld(genemy->torsoAnim)) {
                if ((genemy->torsoAnim) == animNumber_t::BOTH_CWCIRCLELOCK || (genemy->torsoAnim) == animNumber_t::BOTH_BF1LOCK) {
                    if (!PM_irand_timesync(0, 2)) {
                        BG_AddPredictableEventToPlayerstate(entity_event_t::EV_PAIN, Q_floor((float)80 / 100 * 100.0f), genemy);
                    }
                    PM_SetAnimFrame(genemy, anim->firstFrame + remaining, true, true);
                }
                else {
                    PM_SetAnimFrame(genemy, anim->firstFrame + anim->numFrames - remaining, true, true);
                }
            }
            else { // new locks
                if (BG_CheckIncrementLockAnim(genemy->torsoAnim, saberLockType_t::SABERLOCK_LOSE)) {
                    if (!PM_irand_timesync(0, 2)) {
                        BG_AddPredictableEventToPlayerstate(entity_event_t::EV_PAIN, Q_floor((float)80 / 100 * 100.0f), genemy);
                    }
                    PM_SetAnimFrame(genemy, anim->firstFrame + anim->numFrames - remaining, true, true);
                }
                else {
                    PM_SetAnimFrame(genemy, anim->firstFrame + remaining, true, true);
                }
            }
        }
    }
    else { // something broke us out of it
        PM_SaberLockBreak(genemy, false, 0);
    }
}

bool PM_SaberInBrokenParry(saberMoveName_t move)
{
    if (move >= saberMoveName_t::LS_V1_BR && move <= saberMoveName_t::LS_V1_B_) {
        return true;
    }
    if (move >= saberMoveName_t::LS_H1_T_ && move <= saberMoveName_t::LS_H1_BL) {
        return true;
    }
    return false;
}

saberMoveName_t PM_BrokenParryForParry(saberMoveName_t move)
{
    switch (move) {
    case saberMoveName_t::LS_PARRY_UP:
        return saberMoveName_t::LS_H1_T_;
        break;
    case saberMoveName_t::LS_PARRY_UR:
        return saberMoveName_t::LS_H1_TR;
        break;
    case saberMoveName_t::LS_PARRY_UL:
        return saberMoveName_t::LS_H1_TL;
        break;
    case saberMoveName_t::LS_PARRY_LR:
        return saberMoveName_t::LS_H1_BL;
        break;
    case saberMoveName_t::LS_PARRY_LL:
        return saberMoveName_t::LS_H1_BR;
        break;
    case saberMoveName_t::LS_READY:
        return saberMoveName_t::LS_H1_B_;
        break;
    default:
        break;
    }
    return saberMoveName_t::LS_NONE;
}

constexpr int BACK_STAB_DISTANCE = 128;

bool PM_CanBackstab(void)
{
    trace_t tr;
    vec3_t flatAng;
    vec3_t fwd, back;
    vec3_t trmins = {-15, -15, -8};
    vec3_t trmaxs = {15, 15, 8};

    VectorCopy(pm->ps->viewangles, flatAng);
    flatAng[PITCH] = 0;

    AngleVectors(flatAng, fwd, nullptr, nullptr);

    back[0] = pm->ps->origin[0] - fwd[0] * BACK_STAB_DISTANCE;
    back[1] = pm->ps->origin[1] - fwd[1] * BACK_STAB_DISTANCE;
    back[2] = pm->ps->origin[2] - fwd[2] * BACK_STAB_DISTANCE;

    pm->trace(&tr, pm->ps->origin, trmins, trmaxs, back, pm->ps->clientNum, MASK_PLAYERSOLID);

    if (tr.fraction != 1.0 && tr.entityNum >= 0 && tr.entityNum < ENTITYNUM_NONE) {
        bgEntity_t* bgEnt = PM_BGEntForNum(tr.entityNum);

        if (bgEnt && (bgEnt->s.eType == entityType_t::ET_PLAYER || bgEnt->s.eType == entityType_t::ET_NPC)) {
            return true;
        }
    }

    return false;
}

saberMoveName_t PM_SaberFlipOverAttackMove(void)
{
    vec3_t fwdAngles, jumpFwd;
    //	float zDiff = 0;
    //	playerState_t *psData;
    //	bgEntity_t *bgEnt;

    saberInfo_t* saber1 = BG_MySaber(pm->ps->clientNum, 0);
    saberInfo_t* saber2 = BG_MySaber(pm->ps->clientNum, 1);
    // see if we have an overridden (or cancelled) lunge move
    if (saber1 && saber1->jumpAtkFwdMove != saberMoveName_t::LS_INVALID) {
        if (saber1->jumpAtkFwdMove != saberMoveName_t::LS_NONE) {
            return saber1->jumpAtkFwdMove;
        }
    }
    if (saber2 && saber2->jumpAtkFwdMove != saberMoveName_t::LS_INVALID) {
        if (saber2->jumpAtkFwdMove != saberMoveName_t::LS_NONE) {
            return saber2->jumpAtkFwdMove;
        }
    }
    // no overrides, cancelled?
    if (saber1 && saber1->jumpAtkFwdMove == saberMoveName_t::LS_NONE) {
        return saberMoveName_t::LS_A_T2B; // saberMoveName_t::LS_NONE;
    }
    if (saber2 && saber2->jumpAtkFwdMove == saberMoveName_t::LS_NONE) {
        return saberMoveName_t::LS_A_T2B; // saberMoveName_t::LS_NONE;
    }
    // just do it
    VectorCopy(pm->ps->viewangles, fwdAngles);
    fwdAngles[PITCH] = fwdAngles[ROLL] = 0;
    AngleVectors(fwdAngles, jumpFwd, nullptr, nullptr);
    VectorScale(jumpFwd, 150, pm->ps->velocity); // was 50
    pm->ps->velocity[2] = 400;

    /*
    bgEnt = PM_BGEntForNum(tr->entityNum);

    if (!bgEnt)
    {
        return saberMoveName_t::LS_A_FLIP_STAB;
    }

    psData = bgEnt->playerState;

    //go higher for enemies higher than you, lower for those lower than you
    if (psData)
    {
        zDiff = psData->origin[2] - pm->ps->origin[2];

    }
    else
    {
        zDiff = 0;
    }
    pm->ps->velocity[2] += (zDiff)*1.5f;

    //clamp to decent-looking values
    if ( zDiff <= 0 && pm->ps->velocity[2] < 200 )
    {//if we're on same level, don't let me jump so low, I clip into the ground
        pm->ps->velocity[2] = 200;
    }
    else if ( pm->ps->velocity[2] < 100 )
    {
        pm->ps->velocity[2] = 100;
    }
    else if ( pm->ps->velocity[2] > 400 )
    {
        pm->ps->velocity[2] = 400;
    }
    */

    PM_SetForceJumpZStart(pm->ps->origin[2]); // so we don't take damage if we land at same height

    PM_AddEvent(entity_event_t::EV_JUMP);
    pm->ps->fd.forceJumpSound = 1;
    pm->cmd.upmove = 0;

    /*
    if ( PM_irand_timesync( 0, 1 ) )
    {
        return saberMoveName_t::LS_A_FLIP_STAB;
    }
    else
    */
    {
        return saberMoveName_t::LS_A_FLIP_SLASH;
    }
}

saberMoveName_t PM_SaberBackflipAttackMove(void)
{
    saberInfo_t* saber1 = BG_MySaber(pm->ps->clientNum, 0);
    saberInfo_t* saber2 = BG_MySaber(pm->ps->clientNum, 1);
    // see if we have an overridden (or cancelled) lunge move
    if (saber1 && saber1->jumpAtkBackMove != saberMoveName_t::LS_INVALID) {
        if (saber1->jumpAtkBackMove != saberMoveName_t::LS_NONE) {
            return saber1->jumpAtkBackMove;
        }
    }
    if (saber2 && saber2->jumpAtkBackMove != saberMoveName_t::LS_INVALID) {
        if (saber2->jumpAtkBackMove != saberMoveName_t::LS_NONE) {
            return saber2->jumpAtkBackMove;
        }
    }
    // no overrides, cancelled?
    if (saber1 && saber1->jumpAtkBackMove == saberMoveName_t::LS_NONE) {
        return saberMoveName_t::LS_A_T2B; // saberMoveName_t::LS_NONE;
    }
    if (saber2 && saber2->jumpAtkBackMove == saberMoveName_t::LS_NONE) {
        return saberMoveName_t::LS_A_T2B; // saberMoveName_t::LS_NONE;
    }
    // just do it
    pm->cmd.upmove = 127;
    pm->ps->velocity[2] = 500;
    return saberMoveName_t::LS_A_BACKFLIP_ATK;
}

saberMoveName_t PM_SaberDualJumpAttackMove(void)
{
    // FIXME: to make this move easier to execute, should be allowed to do it
    //		after you've already started your jump... but jump is delayed in
    //		this anim, so how do we undo the jump?
    pm->cmd.upmove = 0; // no jump just yet
    return saberMoveName_t::LS_JUMPATTACK_DUAL;
}

constexpr int FLIPHACK_DISTANCE = 200;

bool PM_SomeoneInFront(trace_t* tr)
{ // Also a very simplified version of the sp counterpart
    vec3_t flatAng;
    vec3_t fwd, back;
    vec3_t trmins = {-15, -15, -8};
    vec3_t trmaxs = {15, 15, 8};

    VectorCopy(pm->ps->viewangles, flatAng);
    flatAng[PITCH] = 0;

    AngleVectors(flatAng, fwd, nullptr, nullptr);

    back[0] = pm->ps->origin[0] + fwd[0] * FLIPHACK_DISTANCE;
    back[1] = pm->ps->origin[1] + fwd[1] * FLIPHACK_DISTANCE;
    back[2] = pm->ps->origin[2] + fwd[2] * FLIPHACK_DISTANCE;

    pm->trace(tr, pm->ps->origin, trmins, trmaxs, back, pm->ps->clientNum, MASK_PLAYERSOLID);

    if (tr->fraction != 1.0 && tr->entityNum >= 0 && tr->entityNum < ENTITYNUM_NONE) {
        bgEntity_t* bgEnt = PM_BGEntForNum(tr->entityNum);

        if (bgEnt && (bgEnt->s.eType == entityType_t::ET_PLAYER || bgEnt->s.eType == entityType_t::ET_NPC)) {
            return true;
        }
    }

    return false;
}

saberMoveName_t PM_SaberLungeAttackMove(bool noSpecials)
{
    vec3_t fwdAngles, jumpFwd;
    saberInfo_t* saber1 = BG_MySaber(pm->ps->clientNum, 0);
    saberInfo_t* saber2 = BG_MySaber(pm->ps->clientNum, 1);
    // see if we have an overridden (or cancelled) lunge move
    if (saber1 && saber1->lungeAtkMove != saberMoveName_t::LS_INVALID) {
        if (saber1->lungeAtkMove != saberMoveName_t::LS_NONE) {
            return saber1->lungeAtkMove;
        }
    }
    if (saber2 && saber2->lungeAtkMove != saberMoveName_t::LS_INVALID) {
        if (saber2->lungeAtkMove != saberMoveName_t::LS_NONE) {
            return saber2->lungeAtkMove;
        }
    }
    // no overrides, cancelled?
    if (saber1 && saber1->lungeAtkMove == saberMoveName_t::LS_NONE) {
        return saberMoveName_t::LS_A_T2B; // saberMoveName_t::LS_NONE;
    }
    if (saber2 && saber2->lungeAtkMove == saberMoveName_t::LS_NONE) {
        return saberMoveName_t::LS_A_T2B; // saberMoveName_t::LS_NONE;
    }
    // just do it
    if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_FAST) {
        VectorCopy(pm->ps->viewangles, fwdAngles);
        fwdAngles[PITCH] = fwdAngles[ROLL] = 0;
        // do the lunge
        AngleVectors(fwdAngles, jumpFwd, nullptr, nullptr);
        VectorScale(jumpFwd, 150, pm->ps->velocity);
        PM_AddEvent(entity_event_t::EV_JUMP);

        return saberMoveName_t::LS_A_LUNGE;
    }
    else if (!noSpecials && pm->ps->fd.saberAnimLevel == saber_styles_t::SS_STAFF) {
        return saberMoveName_t::LS_SPINATTACK;
    }
    else if (!noSpecials) {
        return saberMoveName_t::LS_SPINATTACK_DUAL;
    }
    return saberMoveName_t::LS_A_T2B;
}

saberMoveName_t PM_SaberJumpAttackMove2(void)
{
    saberInfo_t* saber1 = BG_MySaber(pm->ps->clientNum, 0);
    saberInfo_t* saber2 = BG_MySaber(pm->ps->clientNum, 1);
    // see if we have an overridden (or cancelled) lunge move
    if (saber1 && saber1->jumpAtkFwdMove != saberMoveName_t::LS_INVALID) {
        if (saber1->jumpAtkFwdMove != saberMoveName_t::LS_NONE) {
            return saber1->jumpAtkFwdMove;
        }
    }
    if (saber2 && saber2->jumpAtkFwdMove != saberMoveName_t::LS_INVALID) {
        if (saber2->jumpAtkFwdMove != saberMoveName_t::LS_NONE) {
            return saber2->jumpAtkFwdMove;
        }
    }
    // no overrides, cancelled?
    if (saber1 && saber1->jumpAtkFwdMove == saberMoveName_t::LS_NONE) {
        return saberMoveName_t::LS_A_T2B; // saberMoveName_t::LS_NONE;
    }
    if (saber2 && saber2->jumpAtkFwdMove == saberMoveName_t::LS_NONE) {
        return saberMoveName_t::LS_A_T2B; // saberMoveName_t::LS_NONE;
    }
    // just do it
    if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_DUAL) {
        return PM_SaberDualJumpAttackMove();
    }
    else {
        // rwwFIXMEFIXME I don't like randomness for this sort of thing, gives people reason to
        // complain combat is unpredictable. Maybe do something more clever to determine
        // if we should do a left or right?
        /*
        if (PM_irand_timesync(0, 1))
        {
            newmove = saberMoveName_t::LS_JUMPATTACK_STAFF_LEFT;
        }
        else
        */
        {
            return saberMoveName_t::LS_JUMPATTACK_STAFF_RIGHT;
        }
    }
    //	return saberMoveName_t::LS_A_T2B;
}

saberMoveName_t PM_SaberJumpAttackMove(void)
{
    vec3_t fwdAngles, jumpFwd;
    saberInfo_t* saber1 = BG_MySaber(pm->ps->clientNum, 0);
    saberInfo_t* saber2 = BG_MySaber(pm->ps->clientNum, 1);
    // see if we have an overridden (or cancelled) lunge move
    if (saber1 && saber1->jumpAtkFwdMove != saberMoveName_t::LS_INVALID) {
        if (saber1->jumpAtkFwdMove != saberMoveName_t::LS_NONE) {
            return saber1->jumpAtkFwdMove;
        }
    }
    if (saber2 && saber2->jumpAtkFwdMove != saberMoveName_t::LS_INVALID) {
        if (saber2->jumpAtkFwdMove != saberMoveName_t::LS_NONE) {
            return saber2->jumpAtkFwdMove;
        }
    }
    // no overrides, cancelled?
    if (saber1 && saber1->jumpAtkFwdMove == saberMoveName_t::LS_NONE) {
        return saberMoveName_t::LS_A_T2B; // saberMoveName_t::LS_NONE;
    }
    if (saber2 && saber2->jumpAtkFwdMove == saberMoveName_t::LS_NONE) {
        return saberMoveName_t::LS_A_T2B; // saberMoveName_t::LS_NONE;
    }
    // just do it
    VectorCopy(pm->ps->viewangles, fwdAngles);
    fwdAngles[PITCH] = fwdAngles[ROLL] = 0;
    AngleVectors(fwdAngles, jumpFwd, nullptr, nullptr);
    VectorScale(jumpFwd, 300, pm->ps->velocity);
    pm->ps->velocity[2] = 280;
    PM_SetForceJumpZStart(pm->ps->origin[2]); // so we don't take damage if we land at same height

    PM_AddEvent(entity_event_t::EV_JUMP);
    pm->ps->fd.forceJumpSound = 1;
    pm->cmd.upmove = 0;

    return saberMoveName_t::LS_A_JUMP_T__B_;
}

float PM_GroundDistance(void)
{
    trace_t tr;
    vec3_t down;

    VectorCopy(pm->ps->origin, down);

    down[2] -= 4096;

    pm->trace(&tr, pm->ps->origin, pm->mins, pm->maxs, down, pm->ps->clientNum, MASK_SOLID);

    VectorSubtract(pm->ps->origin, tr.endpos, down);

    return VectorLength(down);
}

float PM_WalkableGroundDistance(void)
{
    trace_t tr;
    vec3_t down;

    VectorCopy(pm->ps->origin, down);

    down[2] -= 4096;

    pm->trace(&tr, pm->ps->origin, pm->mins, pm->maxs, down, pm->ps->clientNum, MASK_SOLID);

    if (tr.plane.normal[2] < MIN_WALK_NORMAL) { // can't stand on this plane
        return 4096;
    }

    VectorSubtract(pm->ps->origin, tr.endpos, down);

    return VectorLength(down);
}

bool BG_SaberInTransitionAny(saberMoveName_t move);
static bool PM_CanDoDualDoubleAttacks(void)
{
    if (pm->ps->weapon == weapon_t::WP_SABER) {
        saberInfo_t* saber = BG_MySaber(pm->ps->clientNum, 0);
        if (saber && Any(saber->saberFlags & saberFlags_t::SFL_NO_MIRROR_ATTACKS)) {
            return false;
        }
        saber = BG_MySaber(pm->ps->clientNum, 1);
        if (saber && Any(saber->saberFlags & saberFlags_t::SFL_NO_MIRROR_ATTACKS)) {
            return false;
        }
    }
    if (BG_SaberInSpecialAttack(pm->ps->torsoAnim) || BG_SaberInSpecialAttack(pm->ps->legsAnim)) {
        return false;
    }
    return true;
}

static bool PM_CheckEnemyPresence(direction_t dir, float radius)
{ // anyone in this dir?
    vec3_t angles;
    vec3_t checkDir = {0.0f};
    vec3_t tTo;
    vec3_t tMins, tMaxs;
    trace_t tr;
    const float tSize = 12.0f;
    // sp uses a bbox ent list check, but.. that's not so easy/fast to
    // do in predicted code. So I'll just do a single box trace in the proper direction,
    // and take whatever is first hit.

    VectorSet(tMins, -tSize, -tSize, -tSize);
    VectorSet(tMaxs, tSize, tSize, tSize);

    VectorCopy(pm->ps->viewangles, angles);
    angles[PITCH] = 0.0f;

    switch (dir) {
    case direction_t::DIR_RIGHT:
        AngleVectors(angles, nullptr, checkDir, nullptr);
        break;
    case direction_t::DIR_LEFT:
        AngleVectors(angles, nullptr, checkDir, nullptr);
        VectorScale(checkDir, -1, checkDir);
        break;
    case direction_t::DIR_FRONT:
        AngleVectors(angles, checkDir, nullptr, nullptr);
        break;
    case direction_t::DIR_BACK:
        AngleVectors(angles, checkDir, nullptr, nullptr);
        VectorScale(checkDir, -1, checkDir);
        break;
    }

    VectorMA(pm->ps->origin, radius, checkDir, tTo);
    pm->trace(&tr, pm->ps->origin, tMins, tMaxs, tTo, pm->ps->clientNum, MASK_PLAYERSOLID);

    if (tr.fraction != 1.0f && tr.entityNum < ENTITYNUM_WORLD) { // let's see who we hit
        bgEntity_t* bgEnt = PM_BGEntForNum(tr.entityNum);

        if (bgEnt && (bgEnt->s.eType == entityType_t::ET_PLAYER ||
                      bgEnt->s.eType == entityType_t::ET_NPC)) { // this guy can be considered an "enemy"... if he is on the same team, oh well. can't bg-check
                                                                 // that (without a whole lot of hassle).
            return true;
        }
    }

    // no one in the trace
    return false;
}

constexpr int SABER_ALT_ATTACK_POWER = 50;    // 75?
constexpr int SABER_ALT_ATTACK_POWER_LR = 10; // 30?
constexpr int SABER_ALT_ATTACK_POWER_FB = 25; // 30/50?

extern bool PM_SaberInReturn(saberMoveName_t move); // bg_panimate.cpp
saberMoveName_t PM_CheckPullAttack(void)
{
#if 0 // disabling these for MP, they aren't useful
	if (!Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK))
	{
		return saberMoveName_t::LS_NONE;
	}

	if ( (pm->ps->saberMove == saberMoveName_t::LS_READY||PM_SaberInReturn(pm->ps->saberMove)||PM_SaberInReflect(pm->ps->saberMove))//ready
		//&& (pm->ps->clientNum < MAX_CLIENTS||PM_ControlledByPlayer())//PLAYER ONLY
		&& pm->ps->fd.saberAnimLevel >= saber_styles_t::SS_FAST//single saber styles - FIXME: Tavion?
		&& pm->ps->fd.saberAnimLevel <= saber_styles_t::SS_STRONG//single saber styles - FIXME: Tavion?
		//&& G_TryingPullAttack( pm->gent, &pm->cmd, false )
		//&& pm->ps->fd.forcePowerLevel[forcePowers_t::FP_PULL]
		//rwwFIXMEFIXME: rick has the damn msg.cpp file checked out exclusively so I can't update the bloody psf to send this for prediction
		&& pm->ps->powerups[powerup_t::PW_DISINT_4] > pm->cmd.serverTime
		&& !(pm->ps->fd.forcePowersActive & Bit( forcePowers_t::FP_GRIP ))
		&& pm->ps->powerups[powerup_t::PW_PULL] > pm->cmd.serverTime
		//&& pm->cmd.forwardmove<0//pulling back
		&& (pm->cmd.buttons&buttonFlags_t::BUTTON_ATTACK)//attacking
		&& BG_EnoughForcePowerForMove( SABER_ALT_ATTACK_POWER_FB ) )//pm->ps->forcePower >= SABER_ALT_ATTACK_POWER_FB//have enough power
	{//FIXME: some NPC logic to do this?
		bool doMove = true;
//		if ( g_saberNewControlScheme->integer
//			|| g_crosshairEntNum < ENTITYNUM_WORLD )//in old control scheme, there has to be someone there
		{
			saberMoveName_t pullAttackMove = saberMoveName_t::LS_NONE;
			if ( pm->ps->fd.saberAnimLevel == saber_styles_t::SS_FAST )
			{
				pullAttackMove = saberMoveName_t::LS_PULL_ATTACK_STAB;
			}
			else
			{
				pullAttackMove = saberMoveName_t::LS_PULL_ATTACK_SWING;
			}

			/*
			if ( g_crosshairEntNum < ENTITYNUM_WORLD
				&& pm->gent && pm->gent->client )
			{
				gentity_t *targEnt = &g_entities[g_crosshairEntNum];
				if ( targEnt->client
					&& targEnt->health > 0
					//FIXME: check other things like in knockdown, saberlock, uninterruptable anims, etc.
					&& !PM_InOnGroundAnim( &targEnt->client->ps )
					&& !PM_LockedAnim( targEnt->client->ps.legsAnim )
					&& !PM_SuperBreakLoseAnim( targEnt->client->ps.legsAnim )
					&& !PM_SuperBreakWinAnim( targEnt->client->ps.legsAnim )
					&& targEnt->client->ps.saberLockTime <= 0
					&& WP_ForceThrowable( targEnt, targEnt, pm->gent, true, 1.0f, 0.0f, nullptr ) )
				{
					if ( !g_saberNewControlScheme->integer )
					{//in old control scheme, make sure they're close or far enough away for the move we'll be doing
						float targDist = Distance( targEnt->currentOrigin, pm->ps->origin );
						if ( pullAttackMove == saberMoveName_t::LS_PULL_ATTACK_STAB )
						{//must be closer than 512
							if ( targDist > 384.0f )
							{
								return saberMoveName_t::LS_NONE;
							}
						}
						else//if ( pullAttackMove == LS_PULL_ATTACK_SWING )
						{//must be farther than 256
							if ( targDist > 512.0f )
							{
								return saberMoveName_t::LS_NONE;
							}
							if ( targDist < 192.0f )
							{
								return saberMoveName_t::LS_NONE;
							}
						}
					}

					vec3_t targAngles = {0,targEnt->client->ps.viewangles[YAW],0};
					if ( InFront( pm->ps->origin, targEnt->currentOrigin, targAngles ) )
					{
						NPC_SetAnim( targEnt, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_PULLED_INAIR_F, setAnimFlags_t::SETANIM_FLAG_OVERRIDE, setAnimFlags_t::SETANIM_FLAG_HOLD );
					}
					else
					{
						NPC_SetAnim( targEnt, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_PULLED_INAIR_B, setAnimFlags_t::SETANIM_FLAG_OVERRIDE, setAnimFlags_t::SETANIM_FLAG_HOLD );
					}
					//hold the anim until I'm with done pull anim
					targEnt->client->ps.legsAnimTimer = targEnt->client->ps.torsoAnimTimer = PM_AnimLength( pm->gent->client->clientInfo.animFileIndex, saberMoveData[pullAttackMove].animToUse );
					//set pullAttackTime
					pm->gent->client->ps.pullAttackTime = targEnt->client->ps.pullAttackTime = level.time+targEnt->client->ps.legsAnimTimer;
					//make us know about each other
					pm->gent->client->ps.pullAttackEntNum = g_crosshairEntNum;
					targEnt->client->ps.pullAttackEntNum = pm->ps->clientNum;
					//do effect and sound on me
					pm->ps->powerups[PW_FORCE_PUSH] = level.time + 1000;
					if ( pm->gent )
					{
						G_Sound( pm->gent, G_SoundIndex( "sound/weapons/force/pull.wav" ) );
					}
					doMove = true;
				}
			}
			*/
			if ( doMove )
			{
				BG_ForcePowerDrain(pm->ps, forcePowers_t::FP_GRIP, SABER_ALT_ATTACK_POWER_FB );
				return pullAttackMove;
			}
		}
	}
#endif
    return saberMoveName_t::LS_NONE;
}

bool PM_InSecondaryStyle(void)
{
    if (pm->ps->fd.saberAnimLevelBase == saber_styles_t::SS_STAFF || pm->ps->fd.saberAnimLevelBase == saber_styles_t::SS_DUAL) {
        if (pm->ps->fd.saberAnimLevel != pm->ps->fd.saberAnimLevelBase) {
            return true;
        }
    }
    return false;
}

saberMoveName_t PM_SaberAttackForMovement(saberMoveName_t curmove)
{
    saberMoveName_t newmove = saberMoveName_t::LS_NONE;
    bool noSpecials = PM_InSecondaryStyle();
    bool allowCartwheels = true;
    saberMoveName_t overrideJumpRightAttackMove = saberMoveName_t::LS_INVALID;
    saberMoveName_t overrideJumpLeftAttackMove = saberMoveName_t::LS_INVALID;

    if (pm->ps->weapon == weapon_t::WP_SABER) {
        saberInfo_t* saber1 = BG_MySaber(pm->ps->clientNum, 0);
        saberInfo_t* saber2 = BG_MySaber(pm->ps->clientNum, 1);

        if (saber1 && saber1->jumpAtkRightMove != saberMoveName_t::LS_INVALID) {
            if (saber1->jumpAtkRightMove != saberMoveName_t::LS_NONE) { // actually overriding
                overrideJumpRightAttackMove = saber1->jumpAtkRightMove;
            }
            else if (saber2 && saber2->jumpAtkRightMove > saberMoveName_t::LS_NONE) { // would be cancelling it, but check the second saber, too
                overrideJumpRightAttackMove = saber2->jumpAtkRightMove;
            }
            else { // nope, just cancel it
                overrideJumpRightAttackMove = saberMoveName_t::LS_NONE;
            }
        }
        else if (saber2 && saber2->jumpAtkRightMove != saberMoveName_t::LS_INVALID) { // first saber not overridden, check second
            overrideJumpRightAttackMove = saber2->jumpAtkRightMove;
        }

        if (saber1 && saber1->jumpAtkLeftMove != saberMoveName_t::LS_INVALID) {
            if (saber1->jumpAtkLeftMove != saberMoveName_t::LS_NONE) { // actually overriding
                overrideJumpLeftAttackMove = saber1->jumpAtkLeftMove;
            }
            else if (saber2 && saber2->jumpAtkLeftMove > saberMoveName_t::LS_NONE) { // would be cancelling it, but check the second saber, too
                overrideJumpLeftAttackMove = saber2->jumpAtkLeftMove;
            }
            else { // nope, just cancel it
                overrideJumpLeftAttackMove = saberMoveName_t::LS_NONE;
            }
        }
        else if (saber2 && saber2->jumpAtkLeftMove != saberMoveName_t::LS_INVALID) { // first saber not overridden, check second
            overrideJumpLeftAttackMove = saber1->jumpAtkLeftMove;
        }

        if (saber1 && Any(saber1->saberFlags & saberFlags_t::SFL_NO_CARTWHEELS)) {
            allowCartwheels = false;
        }
        if (saber2 && Any(saber2->saberFlags & saberFlags_t::SFL_NO_CARTWHEELS)) {
            allowCartwheels = false;
        }
    }

    if (pm->cmd.rightmove > 0) { // moving right
        if (!noSpecials && overrideJumpRightAttackMove != saberMoveName_t::LS_NONE &&
            pm->ps->velocity[2] > 20.0f                                                    // pm->ps->groundEntityNum != ENTITYNUM_NONE//on ground
            && Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)                         // hitting attack
            && PM_GroundDistance() < 70.0f                                                 // not too high above ground
            && (pm->cmd.upmove > 0 || Any(pm->ps->pm_flags & pmoveFlags_t::PMF_JUMP_HELD)) // focus-holding player
            && BG_EnoughForcePowerForMove(SABER_ALT_ATTACK_POWER_LR))                      // have enough power
        {                                                                                  // cartwheel right
            BG_ForcePowerDrain(pm->ps, forcePowers_t::FP_GRIP, SABER_ALT_ATTACK_POWER_LR);
            if (overrideJumpRightAttackMove != saberMoveName_t::LS_INVALID) { // overridden with another move
                return overrideJumpRightAttackMove;
            }
            else {
                vec3_t right, fwdAngles;

                VectorSet(fwdAngles, 0.0f, pm->ps->viewangles[YAW], 0.0f);

                AngleVectors(fwdAngles, nullptr, right, nullptr);
                pm->ps->velocity[0] = pm->ps->velocity[1] = 0.0f;
                VectorMA(pm->ps->velocity, 190.0f, right, pm->ps->velocity);
                if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_STAFF) {
                    newmove = saberMoveName_t::LS_BUTTERFLY_RIGHT;
                    pm->ps->velocity[2] = 350.0f;
                }
                else if (allowCartwheels) {
                    // PM_SetJumped( JUMP_VELOCITY, true );
                    PM_AddEvent(entity_event_t::EV_JUMP);
                    pm->ps->velocity[2] = 300.0f;

                    // if ( !Q_irand( 0, 1 ) )
                    // if (PM_GroundDistance() >= 25.0f)
                    if (1) {
                        newmove = saberMoveName_t::LS_JUMPATTACK_ARIAL_RIGHT;
                    }
                    else {
                        newmove = saberMoveName_t::LS_JUMPATTACK_CART_RIGHT;
                    }
                }
            }
        }
        else if (pm->cmd.forwardmove > 0) { // forward right = TL2BR slash
            newmove = saberMoveName_t::LS_A_TL2BR;
        }
        else if (pm->cmd.forwardmove < 0) { // backward right = BL2TR uppercut
            newmove = saberMoveName_t::LS_A_BL2TR;
        }
        else { // just right is a left slice
            newmove = saberMoveName_t::LS_A_L2R;
        }
    }
    else if (pm->cmd.rightmove < 0) { // moving left
        if (!noSpecials && overrideJumpLeftAttackMove != saberMoveName_t::LS_NONE &&
            pm->ps->velocity[2] > 20.0f                                                    // pm->ps->groundEntityNum != ENTITYNUM_NONE//on ground
            && Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)                         // hitting attack
            && PM_GroundDistance() < 70.0f                                                 // not too high above ground
            && (pm->cmd.upmove > 0 || Any(pm->ps->pm_flags & pmoveFlags_t::PMF_JUMP_HELD)) // focus-holding player
            && BG_EnoughForcePowerForMove(SABER_ALT_ATTACK_POWER_LR))                      // have enough power
        {                                                                                  // cartwheel left
            BG_ForcePowerDrain(pm->ps, forcePowers_t::FP_GRIP, SABER_ALT_ATTACK_POWER_LR);

            if (overrideJumpLeftAttackMove != saberMoveName_t::LS_INVALID) { // overridden with another move
                return overrideJumpLeftAttackMove;
            }
            else {
                vec3_t right, fwdAngles;

                VectorSet(fwdAngles, 0.0f, pm->ps->viewangles[YAW], 0.0f);
                AngleVectors(fwdAngles, nullptr, right, nullptr);
                pm->ps->velocity[0] = pm->ps->velocity[1] = 0.0f;
                VectorMA(pm->ps->velocity, -190.0f, right, pm->ps->velocity);
                if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_STAFF) {
                    newmove = saberMoveName_t::LS_BUTTERFLY_LEFT;
                    pm->ps->velocity[2] = 250.0f;
                }
                else if (allowCartwheels) {
                    // PM_SetJumped( JUMP_VELOCITY, true );
                    PM_AddEvent(entity_event_t::EV_JUMP);
                    pm->ps->velocity[2] = 350.0f;

                    // if ( !Q_irand( 0, 1 ) )
                    // if (PM_GroundDistance() >= 25.0f)
                    if (1) {
                        newmove = saberMoveName_t::LS_JUMPATTACK_ARIAL_LEFT;
                    }
                    else {
                        newmove = saberMoveName_t::LS_JUMPATTACK_CART_LEFT;
                    }
                }
            }
        }
        else if (pm->cmd.forwardmove > 0) { // forward left = TR2BL slash
            newmove = saberMoveName_t::LS_A_TR2BL;
        }
        else if (pm->cmd.forwardmove < 0) { // backward left = BR2TL uppercut
            newmove = saberMoveName_t::LS_A_BR2TL;
        }
        else { // just left is a right slice
            newmove = saberMoveName_t::LS_A_R2L;
        }
    }
    else {                             // not moving left or right
        if (pm->cmd.forwardmove > 0) { // forward= T2B slash
            if (!noSpecials && (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_DUAL || pm->ps->fd.saberAnimLevel == saber_styles_t::SS_STAFF) &&
                pm->ps->fd.forceRageRecoveryTime < pm->cmd.serverTime &&
                // pm->ps->fd.forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1 &&
                (pm->ps->groundEntityNum != ENTITYNUM_NONE || PM_GroundDistance() <= 40) && pm->ps->velocity[2] >= 0 &&
                (pm->cmd.upmove > 0 || Any(pm->ps->pm_flags & pmoveFlags_t::PMF_JUMP_HELD)) && !BG_SaberInTransitionAny(pm->ps->saberMove) &&
                !BG_SaberInAttack(pm->ps->saberMove) && pm->ps->weaponTime <= 0 && pm->ps->forceHandExtend == forceHandAnims_t::HANDEXTEND_NONE &&
                Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK) && BG_EnoughForcePowerForMove(SABER_ALT_ATTACK_POWER_FB)) { // DUAL/STAFF JUMP ATTACK
                newmove = PM_SaberJumpAttackMove2();
                if (newmove != saberMoveName_t::LS_A_T2B && newmove != saberMoveName_t::LS_NONE) {
                    BG_ForcePowerDrain(pm->ps, forcePowers_t::FP_GRIP, SABER_ALT_ATTACK_POWER_FB);
                }
            }
            else if (!noSpecials && pm->ps->fd.saberAnimLevel == saber_styles_t::SS_MEDIUM && pm->ps->velocity[2] > 100 && PM_GroundDistance() < 32 &&
                     !BG_InSpecialJump(pm->ps->legsAnim) && !BG_SaberInSpecialAttack(pm->ps->torsoAnim) &&
                     BG_EnoughForcePowerForMove(SABER_ALT_ATTACK_POWER_FB)) { // FLIP AND DOWNWARD ATTACK
                // trace_t tr;

                // if (PM_SomeoneInFront(&tr))
                {
                    newmove = PM_SaberFlipOverAttackMove();
                    if (newmove != saberMoveName_t::LS_A_T2B && newmove != saberMoveName_t::LS_NONE) {
                        BG_ForcePowerDrain(pm->ps, forcePowers_t::FP_GRIP, SABER_ALT_ATTACK_POWER_FB);
                    }
                }
            }
            else if (!noSpecials && pm->ps->fd.saberAnimLevel == saber_styles_t::SS_STRONG && pm->ps->velocity[2] > 100 && PM_GroundDistance() < 32 &&
                     !BG_InSpecialJump(pm->ps->legsAnim) && !BG_SaberInSpecialAttack(pm->ps->torsoAnim) &&
                     BG_EnoughForcePowerForMove(SABER_ALT_ATTACK_POWER_FB)) { // DFA
                // trace_t tr;

                // if (PM_SomeoneInFront(&tr))
                {
                    newmove = PM_SaberJumpAttackMove();
                    if (newmove != saberMoveName_t::LS_A_T2B && newmove != saberMoveName_t::LS_NONE) {
                        BG_ForcePowerDrain(pm->ps, forcePowers_t::FP_GRIP, SABER_ALT_ATTACK_POWER_FB);
                    }
                }
            }
            else if ((pm->ps->fd.saberAnimLevel == saber_styles_t::SS_FAST || pm->ps->fd.saberAnimLevel == saber_styles_t::SS_DUAL ||
                      pm->ps->fd.saberAnimLevel == saber_styles_t::SS_STAFF) &&
                     pm->ps->groundEntityNum != ENTITYNUM_NONE && Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED) && pm->ps->weaponTime <= 0 &&
                     !BG_SaberInSpecialAttack(pm->ps->torsoAnim) && BG_EnoughForcePowerForMove(SABER_ALT_ATTACK_POWER_FB)) { // LUNGE (weak)
                newmove = PM_SaberLungeAttackMove(noSpecials);
                if (newmove != saberMoveName_t::LS_A_T2B && newmove != saberMoveName_t::LS_NONE) {
                    BG_ForcePowerDrain(pm->ps, forcePowers_t::FP_GRIP, SABER_ALT_ATTACK_POWER_FB);
                }
            }
            else if (!noSpecials) {
                saberMoveName_t stabDownMove = PM_CheckStabDown();
                if (stabDownMove != saberMoveName_t::LS_NONE && BG_EnoughForcePowerForMove(SABER_ALT_ATTACK_POWER_FB)) {
                    newmove = stabDownMove;
                    BG_ForcePowerDrain(pm->ps, forcePowers_t::FP_GRIP, SABER_ALT_ATTACK_POWER_FB);
                }
                else {
                    newmove = saberMoveName_t::LS_A_T2B;
                }
            }
        }
        else if (pm->cmd.forwardmove < 0) { // backward= T2B slash//B2T uppercut?
            if (!noSpecials && pm->ps->fd.saberAnimLevel == saber_styles_t::SS_STAFF && pm->ps->fd.forceRageRecoveryTime < pm->cmd.serverTime &&
                pm->ps->fd.forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1 &&
                (pm->ps->groundEntityNum != ENTITYNUM_NONE || PM_GroundDistance() <= 40) && pm->ps->velocity[2] >= 0 &&
                (pm->cmd.upmove > 0 || Any(pm->ps->pm_flags & pmoveFlags_t::PMF_JUMP_HELD)) && !BG_SaberInTransitionAny(pm->ps->saberMove) &&
                !BG_SaberInAttack(pm->ps->saberMove) && pm->ps->weaponTime <= 0 && pm->ps->forceHandExtend == forceHandAnims_t::HANDEXTEND_NONE &&
                Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)) { // BACKFLIP ATTACK
                newmove = PM_SaberBackflipAttackMove();
            }
            else if (PM_CanBackstab() && !BG_SaberInSpecialAttack(pm->ps->torsoAnim)) { // BACKSTAB (attack varies by level)
                if (pm->ps->fd.saberAnimLevel >= saber_styles_t::SS_MEDIUM &&
                    pm->ps->fd.saberAnimLevel != saber_styles_t::SS_STAFF) { // medium and higher attacks
                    if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED) || pm->cmd.upmove < 0) {
                        newmove = saberMoveName_t::LS_A_BACK_CR;
                    }
                    else {
                        newmove = saberMoveName_t::LS_A_BACK;
                    }
                }
                else { // weak attack
                    newmove = saberMoveName_t::LS_A_BACKSTAB;
                }
            }
            else {
                newmove = saberMoveName_t::LS_A_T2B;
            }
        }
        else if (PM_SaberInBounce(curmove)) { // bounces should go to their default attack if you don't specify a direction but are attacking
            newmove = saberMoveData[curmove].chain_attack;

            if (PM_SaberKataDone(curmove, newmove)) {
                newmove = saberMoveData[curmove].chain_idle;
            }
            else {
                newmove = saberMoveData[curmove].chain_attack;
            }
        }
        else if (curmove == saberMoveName_t::LS_READY) { // Not moving at all, shouldn't have gotten here...?
            // for now, just pick a random attack
            // newmove = Q_irand( saberMoveName_t::LS_A_TL2BR, saberMoveName_t::LS_A_T2B );
            // rww - If we don't seed with a "common" value, the client and server will get mismatched
            // prediction values. Under laggy conditions this will cause the appearance of rapid swing
            // sequence changes.

            newmove = saberMoveName_t::LS_A_T2B; // decided we don't like random attacks when idle, use an overhead instead.
        }
    }

    if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_DUAL) {
        if ((newmove == saberMoveName_t::LS_A_R2L || newmove == saberMoveName_t::LS_S_R2L || newmove == saberMoveName_t::LS_A_L2R ||
             newmove == saberMoveName_t::LS_S_L2R) &&
            PM_CanDoDualDoubleAttacks() && PM_CheckEnemyPresence(direction_t::DIR_RIGHT, 100.0f) &&
            PM_CheckEnemyPresence(direction_t::DIR_LEFT, 100.0f)) { // enemy both on left and right
            newmove = saberMoveName_t::LS_DUAL_LR;
            // probably already moved, but...
            pm->cmd.rightmove = 0;
        }
        else if ((newmove == saberMoveName_t::LS_A_T2B || newmove == saberMoveName_t::LS_S_T2B || newmove == saberMoveName_t::LS_A_BACK ||
                  newmove == saberMoveName_t::LS_A_BACK_CR) &&
                 PM_CanDoDualDoubleAttacks() && PM_CheckEnemyPresence(direction_t::DIR_FRONT, 100.0f) &&
                 PM_CheckEnemyPresence(direction_t::DIR_BACK, 100.0f)) { // enemy both in front and back
            newmove = saberMoveName_t::LS_DUAL_FB;
            // probably already moved, but...
            pm->cmd.forwardmove = 0;
        }
    }

    return newmove;
}

saberMoveName_t PM_KickMoveForConditions(void)
{
    saberMoveName_t kickMove = saberMoveName_t::LS_INVALID;

    // FIXME: only if forcePowers_t::FP_SABER_OFFENSE >= 3
    if (pm->cmd.rightmove) {         // kick to side
        if (pm->cmd.rightmove > 0) { // kick right
            kickMove = saberMoveName_t::LS_KICK_R;
        }
        else { // kick left
            kickMove = saberMoveName_t::LS_KICK_L;
        }
        pm->cmd.rightmove = 0;
    }
    else if (pm->cmd.forwardmove) {    // kick front/back
        if (pm->cmd.forwardmove > 0) { // kick fwd
            /*
            if (pm->ps->groundEntityNum != ENTITYNUM_NONE &&
                PM_CheckEnemyPresence( direction_t::DIR_FRONT, 64.0f ))
            {
                kickMove = saberMoveName_t::LS_HILT_BASH;
            }
            else
            */
            {
                kickMove = saberMoveName_t::LS_KICK_F;
            }
        }
        else { // kick back
            kickMove = saberMoveName_t::LS_KICK_B;
        }
        pm->cmd.forwardmove = 0;
    }
    else {
        // if (Any( pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK ))
        // if (Any( pm->ps->pm_flags & pmoveFlags_t::PMF_JUMP_HELD ))
        if (0) { // ok, let's try some fancy kicks
            // bool is actually of type int anyway, but just for safeness.
            int front = (int)PM_CheckEnemyPresence(direction_t::DIR_FRONT, 100.0f);
            int back = (int)PM_CheckEnemyPresence(direction_t::DIR_BACK, 100.0f);
            int right = (int)PM_CheckEnemyPresence(direction_t::DIR_RIGHT, 100.0f);
            int left = (int)PM_CheckEnemyPresence(direction_t::DIR_LEFT, 100.0f);
            int numEnemy = front + back + right + left;

            if (numEnemy >= 3 || ((!right || !left) && numEnemy >= 2)) { //> 2 enemies near, or, >= 2 enemies near and they are not to the right and left.
                kickMove = saberMoveName_t::LS_KICK_S;
            }
            else if (right && left) { // enemies on both sides
                kickMove = saberMoveName_t::LS_KICK_RL;
            }
            else { // oh well, just do a forward kick
                kickMove = saberMoveName_t::LS_KICK_F;
            }

            pm->cmd.upmove = 0;
        }
    }

    return kickMove;
}

bool BG_InSlopeAnim(animNumber_t anim);
bool PM_RunningAnim(animNumber_t anim);

bool PM_SaberMoveOkayForKata(void)
{
    if (pm->ps->saberMove == saberMoveName_t::LS_READY || PM_SaberInStart(pm->ps->saberMove)) {
        return true;
    }
    else {
        return false;
    }
}

bool PM_CanDoKata(void)
{
    if (PM_InSecondaryStyle()) {
        return false;
    }

    if (!pm->ps->saberInFlight // not throwing saber
        && PM_SaberMoveOkayForKata() && !BG_SaberInKata(pm->ps->saberMove) && !BG_InKataAnim(pm->ps->legsAnim) &&
        !BG_InKataAnim(pm->ps->torsoAnim)
        /*
        && pm->ps->saberAnimLevel >= saber_styles_t::SS_FAST//fast, med or strong style
        && pm->ps->saberAnimLevel <= saber_styles_t::SS_STRONG//FIXME: Tavion, too?
        */
        && pm->ps->groundEntityNum != ENTITYNUM_NONE               // not in the air
        && Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)     // pressing attack
        && Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK) // pressing alt attack
        && !pm->cmd.forwardmove                                    // not moving f/b
        && !pm->cmd.rightmove                                      // not moving r/l
        && pm->cmd.upmove <= 0                                     // not jumping...?
        && BG_EnoughForcePowerForMove(SABER_ALT_ATTACK_POWER))     // have enough power
    {                                                              // FIXME: check rage, etc...
        saberInfo_t* saber = BG_MySaber(pm->ps->clientNum, 0);
        if (saber && saber->kataMove == saberMoveName_t::LS_NONE) { // kata move has been overridden in a way that should stop you from doing it at all
            return false;
        }
        saber = BG_MySaber(pm->ps->clientNum, 1);
        if (saber && saber->kataMove == saberMoveName_t::LS_NONE) { // kata move has been overridden in a way that should stop you from doing it at all
            return false;
        }
        return true;
    }
    return false;
}

bool PM_CheckAltKickAttack(void)
{
    if (pm->ps->weapon == weapon_t::WP_SABER) {
        saberInfo_t* saber = BG_MySaber(pm->ps->clientNum, 0);
        if (saber && Any(saber->saberFlags & saberFlags_t::SFL_NO_KICKS)) {
            return false;
        }
        saber = BG_MySaber(pm->ps->clientNum, 1);
        if (saber && Any(saber->saberFlags & saberFlags_t::SFL_NO_KICKS)) {
            return false;
        }
    }
    if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)
        //&& (!Any(pm->ps->pm_flags&PMF_ALT_ATTACK_HELD)||PM_SaberInReturn(pm->ps->saberMove))
        && (!BG_FlippingAnim(pm->ps->legsAnim) || pm->ps->legsTimer <= 250) &&
        (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_STAFF /*||!pm->ps->saber[0].throwable*/) && !pm->ps->saberHolstered) {
        return true;
    }
    return false;
}

EnumArray<int, forcePowerLevels_t, forcePowerLevels_t::NUM_FORCE_POWER_LEVELS> bg_parryDebounce = {500, // if don't even have defense, can't use defense!
                                                                                                   300, 150, 50};

bool PM_SaberPowerCheck(void)
{
    if (pm->ps->saberInFlight) { // so we don't keep doing stupid force out thing while guiding saber.
        if (pm->ps->fd.forcePower > forcePowerNeeded[pm->ps->fd.forcePowerLevel[forcePowers_t::FP_SABERTHROW]][forcePowers_t::FP_SABERTHROW]) {
            return true;
        }
    }
    else {
        return BG_EnoughForcePowerForMove(forcePowerNeeded[pm->ps->fd.forcePowerLevel[forcePowers_t::FP_SABERTHROW]][forcePowers_t::FP_SABERTHROW]);
    }

    return false;
}

bool PM_CanDoRollStab(void)
{
    if (pm->ps->weapon == weapon_t::WP_SABER) {
        saberInfo_t* saber = BG_MySaber(pm->ps->clientNum, 0);
        if (saber && Any(saber->saberFlags & saberFlags_t::SFL_NO_ROLL_STAB)) {
            return false;
        }
        saber = BG_MySaber(pm->ps->clientNum, 1);
        if (saber && Any(saber->saberFlags & saberFlags_t::SFL_NO_ROLL_STAB)) {
            return false;
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
bool PM_WalkingAnim(animNumber_t anim);
bool PM_SwimmingAnim(animNumber_t anim);
saberMoveName_t PM_SaberBounceForAttack(saberMoveName_t move);
bool BG_SuperBreakLoseAnim(animNumber_t anim);
bool BG_SuperBreakWinAnim(animNumber_t anim);
void PM_WeaponLightsaber(void)
{
    int addTime;
    bool delayed_fire = false;
    std::optional<animNumber_t> anim;
    saberMoveName_t curmove, newmove = saberMoveName_t::LS_NONE;

    bool checkOnlyWeap = false;

    if (PM_InKnockDown(pm->ps) || BG_InRoll(pm->ps, pm->ps->legsAnim)) { // in knockdown
        // make weapon function
        if (pm->ps->weaponTime > 0) {
            pm->ps->weaponTime -= pml.msec;
            if (pm->ps->weaponTime <= 0) {
                pm->ps->weaponTime = 0;
            }
        }
        if (pm->ps->legsAnim == animNumber_t::BOTH_ROLL_F && pm->ps->legsTimer <= 250) {
            if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)) {
                if (BG_EnoughForcePowerForMove(SABER_ALT_ATTACK_POWER_FB) && !pm->ps->saberInFlight) {
                    if (PM_CanDoRollStab()) {
                        // make sure the saber is on for this move!
                        if (pm->ps->saberHolstered == 2) { // all the way off
                            pm->ps->saberHolstered = 0;
                            PM_AddEvent(entity_event_t::EV_SABER_UNHOLSTER);
                        }
                        PM_SetSaberMove(saberMoveName_t::LS_ROLL_STAB);
                        BG_ForcePowerDrain(pm->ps, forcePowers_t::FP_GRIP, SABER_ALT_ATTACK_POWER_FB);
                    }
                }
            }
        }
        return;
    }

    if (pm->ps->saberLockTime > pm->cmd.serverTime) {
        pm->ps->saberMove = saberMoveName_t::LS_NONE;
        PM_SaberLocked();
        return;
    }
    else {
        if (/*( (pm->ps->torsoAnim) == animNumber_t::BOTH_BF2LOCK ||
               (pm->ps->torsoAnim) == animNumber_t::BOTH_BF1LOCK ||
               (pm->ps->torsoAnim) == animNumber_t::BOTH_CWCIRCLELOCK ||
               (pm->ps->torsoAnim) == animNumber_t::BOTH_CCWCIRCLELOCK ||*/
            pm->ps->saberLockFrame) {
            if (pm->ps->saberLockEnemy < ENTITYNUM_NONE && pm->ps->saberLockEnemy >= 0) {
                bgEntity_t* bgEnt;
                playerState_t* en;

                bgEnt = PM_BGEntForNum(pm->ps->saberLockEnemy);

                if (bgEnt) {
                    en = bgEnt->playerState;

                    if (en) {
                        PM_SaberLockBreak(en, false, 0);
                        return;
                    }
                }
            }

            if (/* ( (pm->ps->torsoAnim) == animNumber_t::BOTH_BF2LOCK ||
                    (pm->ps->torsoAnim) == animNumber_t::BOTH_BF1LOCK ||
                    (pm->ps->torsoAnim) == animNumber_t::BOTH_CWCIRCLELOCK ||
                    (pm->ps->torsoAnim) == animNumber_t::BOTH_CCWCIRCLELOCK ||*/
                pm->ps->saberLockFrame) {
                pm->ps->torsoTimer = 0;
                PM_SetAnim(setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND1, setAnimFlags_t::SETANIM_FLAG_OVERRIDE);
                pm->ps->saberLockFrame = 0;
            }
        }
    }

    if (BG_KickingAnim(pm->ps->legsAnim) || BG_KickingAnim(pm->ps->torsoAnim)) {
        if (pm->ps->legsTimer > 0) { // you're kicking, no interruptions
            return;
        }
        // done?  be immeditately ready to do an attack
        pm->ps->saberMove = saberMoveName_t::LS_READY;
        pm->ps->weaponTime = 0;
    }

    if (BG_SuperBreakLoseAnim(pm->ps->torsoAnim) || BG_SuperBreakWinAnim(pm->ps->torsoAnim)) {
        if (pm->ps->torsoTimer > 0) { // never interrupt these
            return;
        }
    }

    if (BG_SabersOff(pm->ps)) {
        if (pm->ps->saberMove != saberMoveName_t::LS_READY) {
            PM_SetSaberMove(saberMoveName_t::LS_READY);
        }

        if ((pm->ps->legsAnim) != (pm->ps->torsoAnim) && !BG_InSlopeAnim(pm->ps->legsAnim) && pm->ps->torsoTimer <= 0) {
            PM_SetAnim(setAnimParts_t::SETANIM_TORSO, (pm->ps->legsAnim), setAnimFlags_t::SETANIM_FLAG_OVERRIDE);
        }
        else if (BG_InSlopeAnim(pm->ps->legsAnim) && pm->ps->torsoTimer <= 0) {
            PM_SetAnim(setAnimParts_t::SETANIM_TORSO, PM_GetSaberStance(), setAnimFlags_t::SETANIM_FLAG_OVERRIDE);
        }

        if (pm->ps->weaponTime < 1 && (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK) || Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK))) {
            if (pm->ps->duelTime < pm->cmd.serverTime) {
                if (!pm->ps->m_iVehicleNum) { // don't let em unholster the saber by attacking while on vehicle
                    pm->ps->saberHolstered = 0;
                    PM_AddEvent(entity_event_t::EV_SABER_UNHOLSTER);
                }
                else {
                    pm->cmd.buttons &= ~buttonFlags_t::BUTTON_ALT_ATTACK;
                    pm->cmd.buttons &= ~buttonFlags_t::BUTTON_ATTACK;
                }
            }
        }

        if (pm->ps->weaponTime > 0) {
            pm->ps->weaponTime -= pml.msec;
        }

        checkOnlyWeap = true;
        goto weapChecks;
    }

    if (!pm->ps->saberEntityNum && pm->ps->saberInFlight) { // this means our saber has been knocked away
        /*
        if (pm->ps->saberMove != saberMoveName_t::LS_READY)
        {
            PM_SetSaberMove( saberMoveName_t::LS_READY );
        }

        if ((pm->ps->legsAnim) != (pm->ps->torsoAnim) && !BG_InSlopeAnim(pm->ps->legsAnim))
        {
            PM_SetAnim(setAnimParts_t::SETANIM_TORSO,(pm->ps->legsAnim),setAnimFlags_t::SETANIM_FLAG_OVERRIDE);
        }

        if (BG_InSaberStandAnim(pm->ps->torsoAnim) || pm->ps->torsoAnim == animNumber_t::BOTH_SABERPULL)
        {
            PM_SetAnim(setAnimParts_t::SETANIM_TORSO,animNumber_t::BOTH_STAND1,setAnimFlags_t::SETANIM_FLAG_OVERRIDE);
        }

        return;
        */
        // Old method, don't want to do this now because we want to finish up reflected attacks and things
        // if our saber is pried out of our hands from one.
        if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_DUAL) {
            if (pm->ps->saberHolstered > 1) {
                pm->ps->saberHolstered = 1;
            }
        }
        else {
            pm->cmd.buttons &= ~buttonFlags_t::BUTTON_ATTACK;
        }
        pm->cmd.buttons &= ~buttonFlags_t::BUTTON_ALT_ATTACK;
    }

    if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) {   // might as well just check for a saber throw right here
        if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_STAFF) { // kick instead of doing a throw
            // if in a saber attack return anim, can interrupt it with a kick
            if (pm->ps->weaponTime > 0                 // can't fire yet
                && PM_SaberInReturn(pm->ps->saberMove) // in a saber return move - FIXME: what about transitions?
                //&& pm->ps->weaponTime <= 250//should be able to fire soon
                //&& pm->ps->torsoTimer <= 250//torso almost done
                && pm->ps->saberBlocked == saberBlockedType_t::BLOCKED_NONE // not interacting with any other saber
                && !Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK))    // not trying to swing the saber
            {
                if ((pm->cmd.forwardmove || pm->cmd.rightmove) // trying to kick in a specific direction
                    && PM_CheckAltKickAttack())                // trying to do a kick
                {                                              // allow them to do the kick now!
                    saberMoveName_t kickMove = PM_KickMoveForConditions();
                    if (kickMove != saberMoveName_t::LS_INVALID) {
                        pm->ps->weaponTime = 0;
                        PM_SetSaberMove(kickMove);
                        return;
                    }
                }
            }
        }
        else if (pm->ps->weaponTime < 1 && pm->ps->saberCanThrow &&
                 // pm->ps->fd.forcePower >= forcePowerNeeded[pm->ps->fd.forcePowerLevel[forcePowers_t::FP_SABERTHROW]][forcePowers_t::FP_SABERTHROW] &&
                 !BG_HasYsalamiri(pm->gametype, pm->ps) && BG_CanUseFPNow(pm->gametype, pm->ps, pm->cmd.serverTime, forcePowers_t::FP_SABERTHROW) &&
                 pm->ps->fd.forcePowerLevel[forcePowers_t::FP_SABERTHROW] > forcePowerLevels_t::FORCE_LEVEL_0 && PM_SaberPowerCheck()) {
            trace_t sabTr;
            vec3_t fwd, minFwd, sabMins, sabMaxs;

            VectorSet(sabMins, SABERMINS_X, SABERMINS_Y, SABERMINS_Z);
            VectorSet(sabMaxs, SABERMAXS_X, SABERMAXS_Y, SABERMAXS_Z);

            AngleVectors(pm->ps->viewangles, fwd, nullptr, nullptr);
            VectorMA(pm->ps->origin, SABER_MIN_THROW_DIST, fwd, minFwd);

            pm->trace(&sabTr, pm->ps->origin, sabMins, sabMaxs, minFwd, pm->ps->clientNum, MASK_PLAYERSOLID);

            if (sabTr.allsolid || sabTr.startsolid || sabTr.fraction < 1.0f) { // not enough room to throw
            }
            else { // throw it
                // This will get set to false again once the saber makes it back to its owner game-side
                if (!pm->ps->saberInFlight) {
                    pm->ps->fd.forcePower -= forcePowerNeeded[pm->ps->fd.forcePowerLevel[forcePowers_t::FP_SABERTHROW]][forcePowers_t::FP_SABERTHROW];
                }

                pm->ps->saberInFlight = true;
            }
        }
    }

    if (pm->ps->saberInFlight && pm->ps->saberEntityNum) {        // guiding saber
        if ((pm->ps->fd.saberAnimLevel != saber_styles_t::SS_DUAL // not using 2 sabers
             || pm->ps->saberHolstered // left one off - FIXME: saberHolstered 1 should be left one off, 0 should be both on, 2 should be both off
             || (!Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)         // not trying to start an attack AND...
                 && (pm->ps->torsoAnim == animNumber_t::BOTH_SABERDUAL_STANCE // not already attacking
                     || pm->ps->torsoAnim == animNumber_t::BOTH_SABERPULL     // not already attacking
                     || pm->ps->torsoAnim == animNumber_t::BOTH_STAND1        // not already attacking
                     || PM_RunningAnim(pm->ps->torsoAnim)                     // not already attacking
                     || PM_WalkingAnim(pm->ps->torsoAnim)                     // not already attacking
                     || PM_JumpingAnim(pm->ps->torsoAnim)                     // not already attacking
                     || PM_SwimmingAnim(pm->ps->torsoAnim))                   // not already attacking
                 ))) {
            PM_SetAnim(setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_SABERPULL, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
            pm->ps->torsoTimer = 1;
            return;
        }
    }

    // don't allow attack until all buttons are up
    // This is bad. It freezes the attack state and the animations if you hold the button after respawning, and it looks strange.
    /*
    if ( Any( pm->ps->pm_flags & pmoveFlags_t::PMF_RESPAWNED ) ) {
        return;
    }
    */

    // check for dead player
    if (pm->ps->stats[statIndex_t::STAT_HEALTH] <= 0) {
        return;
    }

    /*

    if (pm->ps->weaponstate == weaponstate_t::WEAPON_READY ||
        pm->ps->weaponstate == weaponstate_t::WEAPON_IDLE)
    {
        if (pm->ps->saberMove != saberMoveName_t::LS_READY && pm->ps->weaponTime <= 0 && !pm->ps->saberBlocked)
        {
            PM_SetSaberMove( saberMoveName_t::LS_READY );
        }
    }

    if(PM_RunningAnim(pm->ps->torsoAnim))
    {
        if ((pm->ps->torsoAnim) != (pm->ps->legsAnim))
        {
            PM_SetAnim(setAnimParts_t::SETANIM_TORSO,(pm->ps->legsAnim),setAnimFlags_t::SETANIM_FLAG_OVERRIDE);
        }
    }
    */

    // make weapon function
    if (pm->ps->weaponTime > 0) {
        // check for special pull move while busy
        saberMoveName_t pullmove = PM_CheckPullAttack();
        if (pullmove != saberMoveName_t::LS_NONE) {
            pm->ps->weaponTime = 0;
            pm->ps->torsoTimer = 0;
            pm->ps->legsTimer = 0;
            pm->ps->forceHandExtend = forceHandAnims_t::HANDEXTEND_NONE;
            pm->ps->weaponstate = weaponstate_t::WEAPON_READY;
            PM_SetSaberMove(pullmove);
            return;
        }

        pm->ps->weaponTime -= pml.msec;

        // This was stupid and didn't work right. Looks like things are fine without it.
        //	if (pm->ps->saberBlocked && pm->ps->torsoAnim != saberMoveData[pm->ps->saberMove].animToUse)
        //	{ //rww - keep him in the blocking pose until he can attack again
        //		PM_SetAnim(setAnimParts_t::SETANIM_TORSO,saberMoveData[pm->ps->saberMove].animToUse,saberMoveData[pm->ps->saberMove].animSetFlags|setAnimFlags_t::SETANIM_FLAG_HOLD);
        //		return;
        //	}
    }
    else {
        pm->ps->weaponstate = weaponstate_t::WEAPON_READY;
    }

    // Now we react to a block action by the player's lightsaber.
    if (pm->ps->saberBlocked != saberBlockedType_t::BLOCKED_NONE) {
        if (pm->ps->saberBlocked >= saberBlockedType_t::BLOCKED_UPPER_RIGHT &&
            pm->ps->saberBlocked < saberBlockedType_t::BLOCKED_UPPER_RIGHT_PROJ) { // hold the parry for a bit
            pm->ps->weaponTime = bg_parryDebounce[pm->ps->fd.forcePowerLevel[forcePowers_t::FP_SABER_DEFENSE]] + 200;
        }
        switch (pm->ps->saberBlocked) {
        case saberBlockedType_t::BLOCKED_BOUNCE_MOVE: { // act as a bounceMove and reset the saberMove instead of using a seperate value for it
            pm->ps->torsoTimer = 0;
            PM_SetSaberMove(pm->ps->saberMove);
            pm->ps->weaponTime = pm->ps->torsoTimer;
            pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_NONE;
        } break;
        case saberBlockedType_t::BLOCKED_PARRY_BROKEN:
            // whatever parry we were is in now broken, play the appropriate knocked-away anim
            {
                saberMoveName_t nextMove;

                if (PM_SaberInBrokenParry(pm->ps->saberMove)) { // already have one...?
                    nextMove = pm->ps->saberMove;
                }
                else {
                    nextMove = PM_BrokenParryForParry(pm->ps->saberMove);
                }
                if (nextMove != saberMoveName_t::LS_NONE) {
                    PM_SetSaberMove(nextMove);
                    pm->ps->weaponTime = pm->ps->torsoTimer;
                }
                else { // Maybe in a knockaway?
                }
            }
            break;
        case saberBlockedType_t::BLOCKED_ATK_BOUNCE:
            // If there is absolutely no blocked move in the chart, don't even mess with the animation.
            // OR if we are already in a block or parry.
            if (pm->ps->saberMove >= saberMoveName_t::LS_T1_BR__R) { // an actual bounce?  Other bounces before this are actually transitions?
                pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_NONE;
            }
            else {
                saberMoveName_t bounceMove;

                if (PM_SaberInBounce(pm->ps->saberMove) || !BG_SaberInAttack(pm->ps->saberMove)) {
                    if (Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)) { // transition to a new attack
                        saberQuadrant_t newQuad = PM_SaberMoveQuadrantForMovement(&pm->cmd);
                        while (
                            newQuad ==
                            saberMoveData[pm->ps->saberMove].startQuad) { // player is still in same attack quad, don't repeat that attack because it looks bad,
                            // FIXME: try to pick one that might look cool?
                            // newQuad = Q_irand( saberQuadrant_t::Q_BR, saberQuadrant_t::Q_BL );
                            newQuad = saberQuadrant_t::Q_BR + PM_irand_timesync(0, saberQuadrant_t::Q_BL - saberQuadrant_t::Q_BR);
                            // FIXME: sanity check, just in case?
                        } // else player is switching up anyway, take the new attack dir
                        bounceMove = transitionMove[saberMoveData[pm->ps->saberMove].startQuad][newQuad];
                    }
                    else { // return to ready
                        if (saberMoveData[pm->ps->saberMove].startQuad == saberQuadrant_t::Q_T) {
                            bounceMove = saberMoveName_t::LS_R_BL2TR;
                        }
                        else if (saberMoveData[pm->ps->saberMove].startQuad < saberQuadrant_t::Q_T) {
                            bounceMove = saberMoveName_t::LS_R_TL2BR + (saberMoveData[pm->ps->saberMove].startQuad - saberQuadrant_t::Q_BR);
                        }
                        else // if ( saberMoveData[pm->ps->saberMove].startQuad > saberQuadrant_t::Q_T )
                        {
                            bounceMove = saberMoveName_t::LS_R_BR2TL + (saberMoveData[pm->ps->saberMove].startQuad - saberQuadrant_t::Q_TL);
                        }
                    }
                }
                else { // start the bounce
                    bounceMove = PM_SaberBounceForAttack(pm->ps->saberMove);
                }

                PM_SetSaberMove(bounceMove);

                pm->ps->weaponTime = pm->ps->torsoTimer; //+saberMoveData[bounceMove].blendTime+SABER_BLOCK_DUR;
            }
            break;
        case saberBlockedType_t::BLOCKED_UPPER_RIGHT:
            PM_SetSaberMove(saberMoveName_t::LS_PARRY_UR);
            break;
        case saberBlockedType_t::BLOCKED_UPPER_RIGHT_PROJ:
            PM_SetSaberMove(saberMoveName_t::LS_REFLECT_UR);
            break;
        case saberBlockedType_t::BLOCKED_UPPER_LEFT:
            PM_SetSaberMove(saberMoveName_t::LS_PARRY_UL);
            break;
        case saberBlockedType_t::BLOCKED_UPPER_LEFT_PROJ:
            PM_SetSaberMove(saberMoveName_t::LS_REFLECT_UL);
            break;
        case saberBlockedType_t::BLOCKED_LOWER_RIGHT:
            PM_SetSaberMove(saberMoveName_t::LS_PARRY_LR);
            break;
        case saberBlockedType_t::BLOCKED_LOWER_RIGHT_PROJ:
            PM_SetSaberMove(saberMoveName_t::LS_REFLECT_LR);
            break;
        case saberBlockedType_t::BLOCKED_LOWER_LEFT:
            PM_SetSaberMove(saberMoveName_t::LS_PARRY_LL);
            break;
        case saberBlockedType_t::BLOCKED_LOWER_LEFT_PROJ:
            PM_SetSaberMove(saberMoveName_t::LS_REFLECT_LL);
            break;
        case saberBlockedType_t::BLOCKED_TOP:
            PM_SetSaberMove(saberMoveName_t::LS_PARRY_UP);
            break;
        case saberBlockedType_t::BLOCKED_TOP_PROJ:
            PM_SetSaberMove(saberMoveName_t::LS_REFLECT_UP);
            break;
        default:
            pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_NONE;
            break;
        }
        if (pm->ps->saberBlocked >= saberBlockedType_t::BLOCKED_UPPER_RIGHT &&
            pm->ps->saberBlocked < saberBlockedType_t::BLOCKED_UPPER_RIGHT_PROJ) { // hold the parry for a bit
            if (pm->ps->torsoTimer < pm->ps->weaponTime) {
                pm->ps->torsoTimer = pm->ps->weaponTime;
            }
        }

        // what the? I don't know why I was doing this.
        /*
        if (pm->ps->saberBlocked != saberBlockedType_t::BLOCKED_ATK_BOUNCE && pm->ps->saberBlocked != saberBlockedType_t::BLOCKED_PARRY_BROKEN &&
        pm->ps->weaponTime < 1)
        {
            pm->ps->torsoTimer = SABER_BLOCK_DUR;
            pm->ps->weaponTime = pm->ps->torsoTimer;
        }
        */

        // clear block
        pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_NONE;

        // Charging is like a lead-up before attacking again.  This is an appropriate use, or we can create a new weaponstate for blocking
        pm->ps->weaponstate = weaponstate_t::WEAPON_READY;

        // Done with block, so stop these active weapon branches.
        return;
    }

weapChecks:
    if (pm->ps->saberEntityNum) { // only check if we have our saber with us
        // check for weapon change
        // can't change if weapon is firing, but can change again if lowering or raising
        // if ( pm->ps->weaponTime <= 0 || pm->ps->weaponstate != weaponstate_t::WEAPON_FIRING ) {
        if (pm->ps->weaponTime <= 0 && pm->ps->torsoTimer <= 0) {
            if (pm->ps->weapon != UC_Weapon(pm->cmd)) {
                PM_BeginWeaponChange(UC_Weapon(pm->cmd));
            }
        }
    }

    if (PM_CanDoKata()) {
        saberMoveName_t overrideMove = saberMoveName_t::LS_INVALID;
        saberInfo_t* saber1 = BG_MySaber(pm->ps->clientNum, 0);
        saberInfo_t* saber2 = BG_MySaber(pm->ps->clientNum, 1);
        // see if we have an overridden (or cancelled) kata move
        if (saber1 && saber1->kataMove != saberMoveName_t::LS_INVALID) {
            if (saber1->kataMove != saberMoveName_t::LS_NONE) {
                overrideMove = saber1->kataMove;
            }
        }
        if (overrideMove == saberMoveName_t::LS_INVALID) { // not overridden by first saber, check second
            if (saber2 && saber2->kataMove != saberMoveName_t::LS_INVALID) {
                if (saber2->kataMove != saberMoveName_t::LS_NONE) {
                    overrideMove = saber2->kataMove;
                }
            }
        }
        // no overrides, cancelled?
        if (overrideMove == saberMoveName_t::LS_INVALID) {
            if (saber2 && saber2->kataMove == saberMoveName_t::LS_NONE) {
                overrideMove = saberMoveName_t::LS_NONE;
            }
            else if (saber2 && saber2->kataMove == saberMoveName_t::LS_NONE) {
                overrideMove = saberMoveName_t::LS_NONE;
            }
        }
        if (overrideMove == saberMoveName_t::LS_INVALID) { // not overridden
            // FIXME: make sure to turn on saber(s)!
            switch (pm->ps->fd.saberAnimLevel) {
            case saber_styles_t::SS_FAST:
            case saber_styles_t::SS_TAVION:
                PM_SetSaberMove(saberMoveName_t::LS_A1_SPECIAL);
                break;
            case saber_styles_t::SS_MEDIUM:
                PM_SetSaberMove(saberMoveName_t::LS_A2_SPECIAL);
                break;
            case saber_styles_t::SS_STRONG:
            case saber_styles_t::SS_DESANN:
                PM_SetSaberMove(saberMoveName_t::LS_A3_SPECIAL);
                break;
            case saber_styles_t::SS_DUAL:
                PM_SetSaberMove(saberMoveName_t::LS_DUAL_SPIN_PROTECT); // PM_CheckDualSpinProtect();
                break;
            case saber_styles_t::SS_STAFF:
                PM_SetSaberMove(saberMoveName_t::LS_STAFF_SOULCAL);
                break;
            default:
                break;
            }
            pm->ps->weaponstate = weaponstate_t::WEAPON_FIRING;
            // G_DrainPowerForSpecialMove( pm->gent, forcePowers_t::FP_SABER_OFFENSE, SABER_ALT_ATTACK_POWER );//FP_SPEED, SINGLE_SPECIAL_POWER );
            BG_ForcePowerDrain(pm->ps, forcePowers_t::FP_GRIP, SABER_ALT_ATTACK_POWER);
        }
        else if (overrideMove != saberMoveName_t::LS_NONE) {
            PM_SetSaberMove(overrideMove);
            pm->ps->weaponstate = weaponstate_t::WEAPON_FIRING;
            BG_ForcePowerDrain(pm->ps, forcePowers_t::FP_GRIP, SABER_ALT_ATTACK_POWER);
        }
        if (overrideMove != saberMoveName_t::LS_NONE) { // not cancelled
            return;
        }
    }

    if (pm->ps->weaponTime > 0) {
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
        if ((pm->ps->legsAnim) == animNumber_t::BOTH_WALK1) {
            PM_SetAnim(setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WALK1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if ((pm->ps->legsAnim) == animNumber_t::BOTH_RUN1) {
            PM_SetAnim(setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if ((pm->ps->legsAnim) == animNumber_t::BOTH_RUN2) {
            PM_SetAnim(setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN_STAFF) {
            PM_SetAnim(setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN_STAFF, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN_DUAL) {
            PM_SetAnim(setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN_DUAL, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if ((pm->ps->legsAnim) == animNumber_t::BOTH_WALK1) {
            PM_SetAnim(setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WALK1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_WALK2) {
            PM_SetAnim(setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WALK2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_WALK_STAFF) {
            PM_SetAnim(setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WALK_STAFF, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_WALK_DUAL) {
            PM_SetAnim(setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WALK_DUAL, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else {
            PM_SetAnim(setAnimParts_t::SETANIM_TORSO, PM_GetSaberStance(), setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }

        if (pm->ps->weaponstate == weaponstate_t::WEAPON_RAISING) {
            return;
        }
    }

    if (checkOnlyWeap) {
        return;
    }

    // *********************************************************
    // Check for WEAPON ATTACK
    // *********************************************************
    if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_STAFF && Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ALT_ATTACK)) { // ok, try a kick I guess.
        saberMoveName_t kickMove = saberMoveName_t::LS_INVALID;

        if (!BG_KickingAnim(pm->ps->torsoAnim) && !BG_KickingAnim(pm->ps->legsAnim) && !BG_InRoll(pm->ps, pm->ps->legsAnim) &&
            //			!BG_KickMove( pm->ps->saberMove )//not already in a kick
            pm->ps->saberMove == saberMoveName_t::LS_READY && !Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED) // not ducked
            && (pm->cmd.upmove >= 0)                                                                            // not trying to duck
            )                                                                                                   //&& pm->ps->groundEntityNum != ENTITYNUM_NONE)
        {                                                                                                       // player kicks
            kickMove = PM_KickMoveForConditions();
        }

        if (kickMove != saberMoveName_t::LS_INVALID) {
            if (pm->ps->groundEntityNum == ENTITYNUM_NONE) { // if in air, convert kick to an in-air kick
                float gDist = PM_GroundDistance();
                // let's only allow air kicks if a certain distance from the ground
                // it's silly to be able to do them right as you land.
                // also looks wrong to transition from a non-complete flip anim...
                if ((!BG_FlippingAnim(pm->ps->legsAnim) || pm->ps->legsTimer <= 0) && gDist > 64.0f && // strict minimum
                    gDist > (-pm->ps->velocity[2]) - 64.0f // make sure we are high to ground relative to downward velocity as well
                ) {
                    switch (kickMove) {
                    case saberMoveName_t::LS_KICK_F:
                        kickMove = saberMoveName_t::LS_KICK_F_AIR;
                        break;
                    case saberMoveName_t::LS_KICK_B:
                        kickMove = saberMoveName_t::LS_KICK_B_AIR;
                        break;
                    case saberMoveName_t::LS_KICK_R:
                        kickMove = saberMoveName_t::LS_KICK_R_AIR;
                        break;
                    case saberMoveName_t::LS_KICK_L:
                        kickMove = saberMoveName_t::LS_KICK_L_AIR;
                        break;
                    default: // oh well, can't do any other kick move while in-air
                        kickMove = saberMoveName_t::LS_INVALID;
                        break;
                    }
                }
                else {                                                // leave it as a normal kick unless we're too high up
                    if (gDist > 128.0f || pm->ps->velocity[2] >= 0) { // off ground, but too close to ground
                        kickMove = saberMoveName_t::LS_INVALID;
                    }
                }
            }

            if (kickMove != saberMoveName_t::LS_INVALID) {
                PM_SetSaberMove(kickMove);
                return;
            }
        }
    }

    // this is never a valid regular saber attack button
    pm->cmd.buttons &= ~buttonFlags_t::BUTTON_ALT_ATTACK;

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

            if (pm->ps->weaponTime > 0) { // Still firing
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
            else if (PM_SaberInBounce(curmove)) {            // in a bounce
                newmove = saberMoveData[curmove].chain_idle; // oops, not attacking, so don't chain
            }
            else { // FIXME: what about returning from a parry?
                // PM_SetSaberMove( saberMoveName_t::LS_READY );
                // if ( pm->ps->saberBlockingTime > pm->cmd.serverTime )
                {
                    PM_SetSaberMove(saberMoveName_t::LS_READY);
                }
                return;
            }
        }

        // ***************************************************
        // Pressing attack, so we must look up the proper attack move.

        if (pm->ps->weaponTime > 0) { // Last attack is not yet complete.
            pm->ps->weaponstate = weaponstate_t::WEAPON_FIRING;
            return;
        }
        else {
            int both = false;
            if (pm->ps->torsoAnim == animNumber_t::BOTH_FORCELONGLEAP_ATTACK ||
                pm->ps->torsoAnim == animNumber_t::BOTH_FORCELONGLEAP_LAND) { // can't attack in these anims
                return;
            }
            else if (pm->ps->torsoAnim == animNumber_t::BOTH_FORCELONGLEAP_START) { // only 1 attack you can do from this anim
                if (pm->ps->torsoTimer >= 200) {                                    // hit it early enough to do the attack
                    PM_SetSaberMove(saberMoveName_t::LS_LEAP_ATTACK);
                }
                return;
            }
            if (curmove >= saberMoveName_t::LS_PARRY_UP &&
                curmove <= saberMoveName_t::LS_REFLECT_LL) { // from a parry or reflection, can go directly into an attack
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

            if (newmove != saberMoveName_t::LS_NONE) { // have a valid, final LS_ move picked, so skip findingt he transition move and just get the anim
                anim = saberMoveData[newmove].animToUse;
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
                else // if ( Any( pm->cmd.buttons&buttonFlags_t::BUTTON_ATTACK ) && !Any(pm->ps->pm_flags&PMF_ATTACK_HELD) )//only do this if just pressed
                     // attack button?
                {    // get attack move from movement command
                    /*
                    if ( PM_SaberKataDone() )
                    {//we came from a bounce and cannot chain to another attack because our kata is done
                        newmove = saberMoveData[curmove].chain_idle;
                    }
                    else */
                    newmove = PM_SaberAttackForMovement(curmove);
                    if ((PM_SaberInBounce(curmove) || PM_SaberInBrokenParry(curmove)) &&
                        saberMoveData[newmove].startQuad ==
                            saberMoveData[curmove].endQuad) { // this attack would be a repeat of the last (which was blocked), so don't actually use it, use
                                                              // the default chain attack for this bounce
                        newmove = saberMoveData[curmove].chain_attack;
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
                    anim = saberMoveData[newmove].animToUse;
                }
            }

            if (!anim) { // not side-stepping, pick neutral anim
                // Add randomness for prototype?
                newmove = saberMoveData[curmove].chain_attack;

                anim = saberMoveData[newmove].animToUse;

                if (!pm->cmd.forwardmove && !pm->cmd.rightmove && pm->cmd.upmove >= 0 &&
                    pm->ps->groundEntityNum != ENTITYNUM_NONE) { // not moving at all, so set the anim on entire body
                    both = true;
                }
            }

            if (!anim) {
                switch (pm->ps->legsAnim) {
                case animNumber_t::BOTH_WALK1:
                case animNumber_t::BOTH_WALK2:
                case animNumber_t::BOTH_WALK_STAFF:
                case animNumber_t::BOTH_WALK_DUAL:
                case animNumber_t::BOTH_WALKBACK1:
                case animNumber_t::BOTH_WALKBACK2:
                case animNumber_t::BOTH_WALKBACK_STAFF:
                case animNumber_t::BOTH_WALKBACK_DUAL:
                case animNumber_t::BOTH_RUN1:
                case animNumber_t::BOTH_RUN2:
                case animNumber_t::BOTH_RUN_STAFF:
                case animNumber_t::BOTH_RUN_DUAL:
                case animNumber_t::BOTH_RUNBACK1:
                case animNumber_t::BOTH_RUNBACK2:
                case animNumber_t::BOTH_RUNBACK_STAFF:
                    anim = pm->ps->legsAnim;
                    break;
                default:
                    anim = PM_GetSaberStance();
                    break;
                }

                //				if (PM_RunningAnim(*anim) && !pm->cmd.forwardmove && !pm->cmd.rightmove)
                //				{ //semi-hacky (if not moving on x-y and still playing the running anim, force the player out of it)
                //					anim = PM_GetSaberStance();
                //				}
                newmove = saberMoveName_t::LS_READY;
            }

            PM_SetSaberMove(newmove);

            if (both && pm->ps->torsoAnim == anim) {
                PM_SetAnim(setAnimParts_t::SETANIM_LEGS, *anim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
            }

            // don't fire again until anim is done
            pm->ps->weaponTime = pm->ps->torsoTimer;
        }
    }

    // *********************************************************
    // WEAPON_FIRING
    // *********************************************************

    pm->ps->weaponstate = weaponstate_t::WEAPON_FIRING;

    addTime = pm->ps->weaponTime;

    pm->ps->saberAttackSequence = pm->ps->torsoAnim;
    if (!addTime) {
        addTime = weaponData[pm->ps->weapon].fireTime;
    }
    pm->ps->weaponTime = addTime;
}

void PM_SetSaberMove(saberMoveName_t newMove)
{
    setAnimFlags_t setflags = saberMoveData[newMove].animSetFlags;
    animNumber_t anim = saberMoveData[newMove].animToUse;
    setAnimParts_t parts = setAnimParts_t::SETANIM_TORSO;

    if (newMove == saberMoveName_t::LS_READY || newMove == saberMoveName_t::LS_A_FLIP_STAB ||
        newMove == saberMoveName_t::LS_A_FLIP_SLASH) { // finished with a kata (or in a special move) reset attack counter
        pm->ps->saberAttackChainCount = 0;
    }
    else if (BG_SaberInAttack(newMove)) { // continuing with a kata, increment attack counter
        pm->ps->saberAttackChainCount++;
    }

    if (pm->ps->saberAttackChainCount > 16) { // for the sake of being able to send the value over the net within a reasonable bit count
        pm->ps->saberAttackChainCount = 16;
    }

    if (newMove == saberMoveName_t::LS_DRAW) {
        saberInfo_t* saber1 = BG_MySaber(pm->ps->clientNum, 0);
        saberInfo_t* saber2 = BG_MySaber(pm->ps->clientNum, 1);
        if (saber1 && saber1->drawAnim != animNumber_t::ANIM_NONE) {
            anim = saber1->drawAnim;
        }
        else if (saber2 && saber2->drawAnim != animNumber_t::ANIM_NONE) {
            anim = saber2->drawAnim;
        }
        else if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_STAFF) {
            anim = animNumber_t::BOTH_S1_S7;
        }
        else if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_DUAL) {
            anim = animNumber_t::BOTH_S1_S6;
        }
    }
    else if (newMove == saberMoveName_t::LS_PUTAWAY) {
        saberInfo_t* saber1 = BG_MySaber(pm->ps->clientNum, 0);
        saberInfo_t* saber2 = BG_MySaber(pm->ps->clientNum, 1);
        if (saber1 && saber1->putawayAnim != animNumber_t::ANIM_NONE) {
            anim = saber1->putawayAnim;
        }
        else if (saber2 && saber2->putawayAnim != animNumber_t::ANIM_NONE) {
            anim = saber2->putawayAnim;
        }
        else if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_STAFF) {
            anim = animNumber_t::BOTH_S7_S1;
        }
        else if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_DUAL) {
            anim = animNumber_t::BOTH_S6_S1;
        }
    }
    else if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_STAFF && newMove >= saberMoveName_t::LS_S_TL2BR &&
             newMove < saberMoveName_t::LS_REFLECT_LL) { // staff has an entirely new set of anims, besides special attacks
        // FIXME: include ready and draw/putaway?
        // FIXME: get hand-made bounces and deflections?
        if (newMove >= saberMoveName_t::LS_V1_BR && newMove <= saberMoveName_t::LS_REFLECT_LL) { // there aren't 1-7, just 1, 6 and 7, so just set it
            anim = animNumber_t::BOTH_P7_S7_T_ + (anim - animNumber_t::BOTH_P1_S1_T_);           // shift it up to the proper set
        }
        else { // add the appropriate animLevel
            anim += (pm->ps->fd.saberAnimLevel - saber_styles_t::SS_FAST) * SABER_ANIM_GROUP_SIZE;
        }
    }
    else if (pm->ps->fd.saberAnimLevel == saber_styles_t::SS_DUAL && newMove >= saberMoveName_t::LS_S_TL2BR &&
             newMove < saberMoveName_t::LS_REFLECT_LL) { // akimbo has an entirely new set of anims, besides special attacks
        // FIXME: include ready and draw/putaway?
        // FIXME: get hand-made bounces and deflections?
        if (newMove >= saberMoveName_t::LS_V1_BR && newMove <= saberMoveName_t::LS_REFLECT_LL) { // there aren't 1-7, just 1, 6 and 7, so just set it
            anim = animNumber_t::BOTH_P6_S6_T_ + (anim - animNumber_t::BOTH_P1_S1_T_);           // shift it up to the proper set
        }
        else { // add the appropriate animLevel
            anim += (pm->ps->fd.saberAnimLevel - saber_styles_t::SS_FAST) * SABER_ANIM_GROUP_SIZE;
        }
    }
    /*
    else if ( newMove == saberMoveName_t::LS_DRAW && pm->ps->SaberStaff() )
    {//hold saber out front as we turn it on
        //FIXME: need a real "draw" anim for this (and put-away)
        anim = animNumber_t::BOTH_SABERSTAFF_STANCE;
    }
    */
    else if (pm->ps->fd.saberAnimLevel > saber_styles_t::SS_FAST && !BG_SaberInIdle(newMove) && !PM_SaberInParry(newMove) && !PM_SaberInKnockaway(newMove) &&
             !PM_SaberInBrokenParry(newMove) && !PM_SaberInReflect(newMove) &&
             !BG_SaberInSpecial(newMove)) { // readies, parries and reflections have only 1 level
        anim += (pm->ps->fd.saberAnimLevel - saber_styles_t::SS_FAST) * SABER_ANIM_GROUP_SIZE;
    }

    // If the move does the same animation as the last one, we need to force a restart...
    if (saberMoveData[pm->ps->saberMove].animToUse == anim && newMove > saberMoveName_t::LS_PUTAWAY) {
        setflags |= setAnimFlags_t::SETANIM_FLAG_RESTART;
    }

    // saber torso anims should always be highest priority (4/12/02 - for special anims only)
    if (!pm->ps->m_iVehicleNum) { // if not riding a vehicle
        if (BG_SaberInSpecial(newMove)) {
            setflags |= setAnimFlags_t::SETANIM_FLAG_OVERRIDE;
        }
        /*
        if ( newMove == saberMoveName_t::LS_A_LUNGE
            || newMove == saberMoveName_t::LS_A_JUMP_T__B_
            || newMove == saberMoveName_t::LS_A_BACKSTAB
            || newMove == saberMoveName_t::LS_A_BACK
            || newMove == saberMoveName_t::LS_A_BACK_CR
            || newMove == saberMoveName_t::LS_A_FLIP_STAB
            || newMove == saberMoveName_t::LS_A_FLIP_SLASH
            || newMove == saberMoveName_t::LS_JUMPATTACK_DUAL
            || newMove == saberMoveName_t::LS_A_BACKFLIP_ATK)
        {
            setflags |= setAnimFlags_t::SETANIM_FLAG_OVERRIDE;
        }
        */
    }
    if (BG_InSaberStandAnim(anim) || anim == animNumber_t::BOTH_STAND1) {
        anim = (pm->ps->legsAnim);

        if ((anim >= animNumber_t::BOTH_STAND1 && anim <= animNumber_t::BOTH_STAND4TOATTACK2) ||
            (anim >= animNumber_t::TORSO_DROPWEAP1 && anim <= animNumber_t::TORSO_WEAPONIDLE10)) { // If standing then use the special saber stand anim
            anim = PM_GetSaberStance();
        }

        if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) { // Playing torso walk anims while crouched makes you look like a monkey
            anim = PM_GetSaberStance();
        }

        if (anim == animNumber_t::BOTH_WALKBACK1 || anim == animNumber_t::BOTH_WALKBACK2 ||
            anim == animNumber_t::BOTH_WALK1) { // normal stance when walking backward so saber doesn't look like it's cutting through leg
            anim = PM_GetSaberStance();
        }

        if (BG_InSlopeAnim(anim)) {
            anim = PM_GetSaberStance();
        }

        parts = setAnimParts_t::SETANIM_TORSO;
    }

    if (!pm->ps->m_iVehicleNum) {                                                                                            // if not riding a vehicle
        if (newMove == saberMoveName_t::LS_JUMPATTACK_ARIAL_RIGHT || newMove == saberMoveName_t::LS_JUMPATTACK_ARIAL_LEFT) { // force only on legs
            parts = setAnimParts_t::SETANIM_LEGS;
        }
        else if (newMove == saberMoveName_t::LS_A_LUNGE || newMove == saberMoveName_t::LS_A_JUMP_T__B_ || newMove == saberMoveName_t::LS_A_BACKSTAB ||
                 newMove == saberMoveName_t::LS_A_BACK || newMove == saberMoveName_t::LS_A_BACK_CR || newMove == saberMoveName_t::LS_ROLL_STAB ||
                 newMove == saberMoveName_t::LS_A_FLIP_STAB || newMove == saberMoveName_t::LS_A_FLIP_SLASH || newMove == saberMoveName_t::LS_JUMPATTACK_DUAL ||
                 newMove == saberMoveName_t::LS_JUMPATTACK_ARIAL_LEFT || newMove == saberMoveName_t::LS_JUMPATTACK_ARIAL_RIGHT ||
                 newMove == saberMoveName_t::LS_JUMPATTACK_CART_LEFT || newMove == saberMoveName_t::LS_JUMPATTACK_CART_RIGHT ||
                 newMove == saberMoveName_t::LS_JUMPATTACK_STAFF_LEFT || newMove == saberMoveName_t::LS_JUMPATTACK_STAFF_RIGHT ||
                 newMove == saberMoveName_t::LS_A_BACKFLIP_ATK || newMove == saberMoveName_t::LS_STABDOWN || newMove == saberMoveName_t::LS_STABDOWN_STAFF ||
                 newMove == saberMoveName_t::LS_STABDOWN_DUAL || newMove == saberMoveName_t::LS_DUAL_SPIN_PROTECT ||
                 newMove == saberMoveName_t::LS_STAFF_SOULCAL || newMove == saberMoveName_t::LS_A1_SPECIAL || newMove == saberMoveName_t::LS_A2_SPECIAL ||
                 newMove == saberMoveName_t::LS_A3_SPECIAL || newMove == saberMoveName_t::LS_UPSIDE_DOWN_ATTACK ||
                 newMove == saberMoveName_t::LS_PULL_ATTACK_STAB || newMove == saberMoveName_t::LS_PULL_ATTACK_SWING || BG_KickMove(newMove)) {
            parts = setAnimParts_t::SETANIM_BOTH;
        }
        else if (BG_SpinningSaberAnim(anim)) { // spins must be played on entire body
            parts = setAnimParts_t::SETANIM_BOTH;
        }
        else if ((!pm->cmd.forwardmove && !pm->cmd.rightmove && !pm->cmd.upmove)) { // not trying to run, duck or jump
            if (!BG_FlippingAnim(pm->ps->legsAnim) && !BG_InRoll(pm->ps, pm->ps->legsAnim) && !PM_InKnockDown(pm->ps) && !PM_JumpingAnim(pm->ps->legsAnim) &&
                !BG_InSpecialJump(pm->ps->legsAnim) && anim != PM_GetSaberStance() && pm->ps->groundEntityNum != ENTITYNUM_NONE &&
                !Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) {
                parts = setAnimParts_t::SETANIM_BOTH;
            }
            else if (!Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED) &&
                     (newMove == saberMoveName_t::LS_SPINATTACK_DUAL || newMove == saberMoveName_t::LS_SPINATTACK)) {
                parts = setAnimParts_t::SETANIM_BOTH;
            }
        }

        PM_SetAnim(parts, anim, setflags);
        if (parts != setAnimParts_t::SETANIM_LEGS &&
            (pm->ps->legsAnim == animNumber_t::BOTH_ARIAL_LEFT || pm->ps->legsAnim == animNumber_t::BOTH_ARIAL_RIGHT)) {
            if (pm->ps->legsTimer > pm->ps->torsoTimer) {
                pm->ps->legsTimer = pm->ps->torsoTimer;
            }
        }
    }

    if ((pm->ps->torsoAnim) == anim) { // successfully changed anims
        // special check for *starting* a saber swing
        // playing at attack
        if (BG_SaberInAttack(newMove) || BG_SaberInSpecialAttack(anim)) {
            if (pm->ps->saberMove != newMove) { // wasn't playing that attack before
                if (newMove != saberMoveName_t::LS_KICK_F && newMove != saberMoveName_t::LS_KICK_B && newMove != saberMoveName_t::LS_KICK_R &&
                    newMove != saberMoveName_t::LS_KICK_L && newMove != saberMoveName_t::LS_KICK_F_AIR && newMove != saberMoveName_t::LS_KICK_B_AIR &&
                    newMove != saberMoveName_t::LS_KICK_R_AIR && newMove != saberMoveName_t::LS_KICK_L_AIR) {
                    PM_AddEvent(entity_event_t::EV_SABER_ATTACK);
                }

                if (pm->ps->brokenLimbs) { // randomly make pain sounds with a broken arm because we are suffering.
                    int iFactor = -1;

                    if (pm->ps->brokenLimbs & Bit(brokenLimb_t::BROKENLIMB_RARM)) { // You're using it more. So it hurts more.
                        iFactor = 5;
                    }
                    else if (pm->ps->brokenLimbs & Bit(brokenLimb_t::BROKENLIMB_LARM)) {
                        iFactor = 10;
                    }

                    if (iFactor != -1) {
                        if (!PM_irand_timesync(0, iFactor)) {
                            BG_AddPredictableEventToPlayerstate(entity_event_t::EV_PAIN, PM_irand_timesync(1, 100), pm->ps);
                        }
                    }
                }
            }
        }

        if (BG_SaberInSpecial(newMove) &&
            pm->ps->weaponTime <
                pm->ps->torsoTimer) { // rww 01-02-03 - I think this will solve the issue of special attacks being interruptable, hopefully without side effects
            pm->ps->weaponTime = pm->ps->torsoTimer;
        }

        pm->ps->saberMove = newMove;
        pm->ps->saberBlocking = saberMoveData[newMove].blocking;

        pm->ps->torsoAnim = anim;

        if (pm->ps->weaponTime <= 0) {
            pm->ps->saberBlocked = saberBlockedType_t::BLOCKED_NONE;
        }
    }
}

saberInfo_t* BG_MySaber(int clientNum, int saberNum)
{
    // returns a pointer to the requested saberNum
#ifdef _GAME
    gentity_t* ent = &g_entities[clientNum];
    if (ent->inuse && ent->client) {
        if (!ent->client->saber[saberNum].model[0]) { // don't have saber anymore!
            return nullptr;
        }
        return &ent->client->saber[saberNum];
    }
#elif defined(_CGAME)
    clientInfo_t* ci = nullptr;
    if (clientNum < MAX_CLIENTS) {
        ci = &cgs.clientinfo[clientNum];
    }
    else {
        centity_t* cent = &cg_entities[clientNum];
        if (cent->npcClient) {
            ci = cent->npcClient;
        }
    }
    if (ci && ci->infoValid) {
        if (!ci->saber[saberNum].model[0]) { // don't have sabers anymore!
            return nullptr;
        }
        return &ci->saber[saberNum];
    }
#endif

    return nullptr;
}

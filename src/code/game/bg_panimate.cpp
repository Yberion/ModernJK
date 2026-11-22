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

#include "common_headers.h"

// define GAME_INCLUDE so that g_public.h does not define the
// short, server-visible gclient_t and gentity_t structures,
// because we define the full size ones in this file
#define GAME_INCLUDE

#include <qcommon/q_shared.h>
#include "g_shared.h"
#include "bg_local.h"
#include <cgame/cg_local.h>
#include "anims.h"
#include "Q3_Interface.h"
#include "g_local.h"
#include "wp_saber.h"
#include "g_vehicles.h"

extern pmove_t* pm;
extern pml_t pml;
extern cvar_t* g_ICARUSDebug;
extern cvar_t* g_timescale;
extern cvar_t* g_synchSplitAnims;
extern cvar_t* g_AnimWarning;
extern cvar_t* g_noFootSlide;
extern cvar_t* g_noFootSlideRunScale;
extern cvar_t* g_noFootSlideWalkScale;
extern cvar_t* g_saberAnimSpeed;
extern cvar_t* g_saberAutoAim;
extern cvar_t* g_speederControlScheme;
extern cvar_t* g_saberNewControlScheme;

extern bool InFront(vec3_t spot, vec3_t from, vec3_t fromAngles, float threshHold = 0.0f);
extern void WP_ForcePowerDrain(gentity_t* self, forcePowers_t forcePower, int overrideAmt);
extern bool ValidAnimFileIndex(int index);
extern bool PM_ControlledByPlayer(void);
extern bool PM_DroidMelee(class_t npc_class);
extern bool PM_PainAnim(animNumber_t anim);
extern bool PM_JumpingAnim(animNumber_t anim);
extern bool PM_FlippingAnim(animNumber_t anim);
extern bool PM_RollingAnim(animNumber_t anim);
extern bool PM_SwimmingAnim(animNumber_t anim);
extern bool PM_InKnockDown(playerState_t* ps);
extern bool PM_InRoll(playerState_t* ps);
extern bool PM_DodgeAnim(animNumber_t anim);
extern bool PM_InSlopeAnim(animNumber_t anim);
extern bool PM_ForceAnim(animNumber_t anim);
extern bool PM_InKnockDownOnGround(playerState_t* ps);
extern bool PM_InSpecialJump(animNumber_t anim);
extern bool PM_RunningAnim(animNumber_t anim);
extern bool PM_WalkingAnim(animNumber_t anim);
extern bool PM_SwimmingAnim(animNumber_t anim);
extern bool PM_JumpingAnim(animNumber_t anim);
extern bool PM_SaberStanceAnim(animNumber_t anim);
extern bool PM_SaberDrawPutawayAnim(animNumber_t anim);
extern void PM_SetJumped(float height, bool force);
extern bool PM_InGetUpNoRoll(playerState_t* ps);
extern bool PM_CrouchAnim(animNumber_t anim);
extern bool G_TryingKataAttack(gentity_t* self, usercmd_t* cmd);
extern bool G_TryingCartwheel(gentity_t* self, usercmd_t* cmd);
extern bool G_TryingSpecial(gentity_t* self, usercmd_t* cmd);
extern bool G_TryingJumpAttack(gentity_t* self, usercmd_t* cmd);
extern bool G_TryingJumpForwardAttack(gentity_t* self, usercmd_t* cmd);
extern bool G_TryingLungeAttack(gentity_t* self, usercmd_t* cmd);
extern bool G_TryingPullAttack(gentity_t* self, usercmd_t* cmd, bool amPulling);
extern bool G_InCinematicSaberAnim(gentity_t* self);
extern bool G_ControlledByPlayer(gentity_t* self);

extern int g_crosshairEntNum;

int PM_AnimLength(int index, animNumber_t anim);
bool PM_LockedAnim(animNumber_t anim);
bool PM_StandingAnim(animNumber_t anim);
bool PM_InOnGroundAnim(playerState_t* ps);
bool PM_SuperBreakWinAnim(animNumber_t anim);
bool PM_SuperBreakLoseAnim(animNumber_t anim);
bool PM_LockedAnim(animNumber_t anim);
saberMoveName_t PM_SaberFlipOverAttackMove(void);
bool PM_CheckFlipOverAttackMove(bool checkEnemy);
saberMoveName_t PM_SaberJumpForwardAttackMove(void);
bool PM_CheckJumpForwardAttackMove(void);
saberMoveName_t PM_SaberBackflipAttackMove(void);
bool PM_CheckBackflipAttackMove(void);
saberMoveName_t PM_SaberDualJumpAttackMove(void);
bool PM_CheckDualJumpAttackMove(void);
saberMoveName_t PM_SaberLungeAttackMove(bool fallbackToNormalLunge);
bool PM_CheckLungeAttackMove(void);
// Okay, here lies the much-dreaded Pat-created FSM movement chart...  Heretic II strikes again!
// Why am I inflicting this on you?  Well, it's better than hardcoded states.
// Ideally this will be replaced with an external file or more sophisticated move-picker
// once the game gets out of prototype stage.

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
    {"SwoopAtkR", animNumber_t::BOTH_VS_ATR_S, saberQuadrant_t::Q_R, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_SWOOP_ATTACK_RIGHT
    {"SwoopAtkL", animNumber_t::BOTH_VS_ATL_S, saberQuadrant_t::Q_L, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_NO,
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
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 150}, // LS_PARRY_LL
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

EnumArray<EnumArray<saberMoveName_t, saberQuadrant_t, saberQuadrant_t::Q_NUM_QUADS>, saberQuadrant_t, saberQuadrant_t::Q_NUM_QUADS> transitionMove = {
    {{
         saberMoveName_t::LS_NONE,     // Can't transition to same pos!
         saberMoveName_t::LS_T1_BR__R, // 40
         saberMoveName_t::LS_T1_BR_TR, saberMoveName_t::LS_T1_BR_T_, saberMoveName_t::LS_T1_BR_TL, saberMoveName_t::LS_T1_BR__L, saberMoveName_t::LS_T1_BR_BL,
         saberMoveName_t::LS_NONE // No transitions to bottom, and no anims start there, so shouldn't need any
     },
     {
         saberMoveName_t::LS_T1__R_BR, // 46
         saberMoveName_t::LS_NONE,     // Can't transition to same pos!
         saberMoveName_t::LS_T1__R_TR, saberMoveName_t::LS_T1__R_T_, saberMoveName_t::LS_T1__R_TL, saberMoveName_t::LS_T1__R__L, saberMoveName_t::LS_T1__R_BL,
         saberMoveName_t::LS_NONE // No transitions to bottom, and no anims start there, so shouldn't need any
     },
     {
         saberMoveName_t::LS_T1_TR_BR, // 52
         saberMoveName_t::LS_T1_TR__R,
         saberMoveName_t::LS_NONE, // Can't transition to same pos!
         saberMoveName_t::LS_T1_TR_T_, saberMoveName_t::LS_T1_TR_TL, saberMoveName_t::LS_T1_TR__L, saberMoveName_t::LS_T1_TR_BL,
         saberMoveName_t::LS_NONE // No transitions to bottom, and no anims start there, so shouldn't need any
     },
     {
         saberMoveName_t::LS_T1_T__BR, // 58
         saberMoveName_t::LS_T1_T___R, saberMoveName_t::LS_T1_T__TR,
         saberMoveName_t::LS_NONE, // Can't transition to same pos!
         saberMoveName_t::LS_T1_T__TL, saberMoveName_t::LS_T1_T___L, saberMoveName_t::LS_T1_T__BL,
         saberMoveName_t::LS_NONE // No transitions to bottom, and no anims start there, so shouldn't need any
     },
     {
         saberMoveName_t::LS_T1_TL_BR, // 64
         saberMoveName_t::LS_T1_TL__R, saberMoveName_t::LS_T1_TL_TR, saberMoveName_t::LS_T1_TL_T_,
         saberMoveName_t::LS_NONE, // Can't transition to same pos!
         saberMoveName_t::LS_T1_TL__L, saberMoveName_t::LS_T1_TL_BL,
         saberMoveName_t::LS_NONE // No transitions to bottom, and no anims start there, so shouldn't need any
     },
     {
         saberMoveName_t::LS_T1__L_BR, // 70
         saberMoveName_t::LS_T1__L__R, saberMoveName_t::LS_T1__L_TR, saberMoveName_t::LS_T1__L_T_, saberMoveName_t::LS_T1__L_TL,
         saberMoveName_t::LS_NONE, // Can't transition to same pos!
         saberMoveName_t::LS_T1__L_BL,
         saberMoveName_t::LS_NONE // No transitions to bottom, and no anims start there, so shouldn't need any
     },
     {
         saberMoveName_t::LS_T1_BL_BR, // 76
         saberMoveName_t::LS_T1_BL__R, saberMoveName_t::LS_T1_BL_TR, saberMoveName_t::LS_T1_BL_T_, saberMoveName_t::LS_T1_BL_TL, saberMoveName_t::LS_T1_BL__L,
         saberMoveName_t::LS_NONE, // Can't transition to same pos!
         saberMoveName_t::LS_NONE  // No transitions to bottom, and no anims start there, so shouldn't need any
     },
     {
         saberMoveName_t::LS_T1_BL_BR, // NOTE: there are no transitions from bottom, so re-use the bottom right transitions
         saberMoveName_t::LS_T1_BR__R, saberMoveName_t::LS_T1_BR_TR, saberMoveName_t::LS_T1_BR_T_, saberMoveName_t::LS_T1_BR_TL, saberMoveName_t::LS_T1_BR__L,
         saberMoveName_t::LS_T1_BR_BL,
         saberMoveName_t::LS_NONE // No transitions to bottom, and no anims start there, so shouldn't need any
     }}};

void PM_VelocityForSaberMove(playerState_t* ps, vec3_t throwDir)
{
    vec3_t vForward = {0.0f}, vRight = {0.0f}, vUp = {0.0f}, startQ = {0.0f}, endQ = {0.0f};

    AngleVectors(ps->viewangles, vForward, vRight, vUp);

    switch (saberMoveData[ps->saberMove].startQuad) {
    case saberQuadrant_t::Q_BR:
        VectorScale(vRight, 1, startQ);
        VectorMA(startQ, -1, vUp, startQ);
        break;
    case saberQuadrant_t::Q_R:
        VectorScale(vRight, 2, startQ);
        break;
    case saberQuadrant_t::Q_TR:
        VectorScale(vRight, 1, startQ);
        VectorMA(startQ, 1, vUp, startQ);
        break;
    case saberQuadrant_t::Q_T:
        VectorScale(vUp, 2, startQ);
        break;
    case saberQuadrant_t::Q_TL:
        VectorScale(vRight, -1, startQ);
        VectorMA(startQ, 1, vUp, startQ);
        break;
    case saberQuadrant_t::Q_L:
        VectorScale(vRight, -2, startQ);
        break;
    case saberQuadrant_t::Q_BL:
        VectorScale(vRight, -1, startQ);
        VectorMA(startQ, -1, vUp, startQ);
        break;
    case saberQuadrant_t::Q_B:
        VectorScale(vUp, -2, startQ);
        break;
    default:
        break;
    }
    switch (saberMoveData[ps->saberMove].endQuad) {
    case saberQuadrant_t::Q_BR:
        VectorScale(vRight, 1, endQ);
        VectorMA(endQ, -1, vUp, endQ);
        break;
    case saberQuadrant_t::Q_R:
        VectorScale(vRight, 2, endQ);
        break;
    case saberQuadrant_t::Q_TR:
        VectorScale(vRight, 1, endQ);
        VectorMA(endQ, 1, vUp, endQ);
        break;
    case saberQuadrant_t::Q_T:
        VectorScale(vUp, 2, endQ);
        break;
    case saberQuadrant_t::Q_TL:
        VectorScale(vRight, -1, endQ);
        VectorMA(endQ, 1, vUp, endQ);
        break;
    case saberQuadrant_t::Q_L:
        VectorScale(vRight, -2, endQ);
        break;
    case saberQuadrant_t::Q_BL:
        VectorScale(vRight, -1, endQ);
        VectorMA(endQ, -1, vUp, endQ);
        break;
    case saberQuadrant_t::Q_B:
        VectorScale(vUp, -2, endQ);
        break;
    default:
        break;
    }
    VectorMA(endQ, 2, vForward, endQ);
    VectorScale(throwDir, 125, throwDir); // FIXME: pass in the throw strength?
    VectorSubtract(endQ, startQ, throwDir);
}

bool PM_VelocityForBlockedMove(playerState_t* ps, vec3_t throwDir)
{
    vec3_t vForward, vRight, vUp;
    AngleVectors(ps->viewangles, vForward, vRight, vUp);
    switch (ps->saberBlocked) {
    case saberBlockedType_t::BLOCKED_UPPER_RIGHT:
        VectorScale(vRight, 1, throwDir);
        VectorMA(throwDir, 1, vUp, throwDir);
        break;
    case saberBlockedType_t::BLOCKED_UPPER_LEFT:
        VectorScale(vRight, -1, throwDir);
        VectorMA(throwDir, 1, vUp, throwDir);
        break;
    case saberBlockedType_t::BLOCKED_LOWER_RIGHT:
        VectorScale(vRight, 1, throwDir);
        VectorMA(throwDir, -1, vUp, throwDir);
        break;
    case saberBlockedType_t::BLOCKED_LOWER_LEFT:
        VectorScale(vRight, -1, throwDir);
        VectorMA(throwDir, -1, vUp, throwDir);
        break;
    case saberBlockedType_t::BLOCKED_TOP:
        VectorScale(vUp, 2, throwDir);
        break;
    default:
        return false;
        break;
    }
    VectorMA(throwDir, 2, vForward, throwDir);
    VectorScale(throwDir, 250, throwDir); // FIXME: pass in the throw strength?
    return true;
}

saber_styles_t PM_AnimLevelForSaberAnim(animNumber_t anim)
{
    if (anim >= animNumber_t::BOTH_A1_T__B_ && anim <= animNumber_t::BOTH_D1_B____) {
        return saber_styles_t::SS_FAST;
    }
    if (anim >= animNumber_t::BOTH_A2_T__B_ && anim <= animNumber_t::BOTH_D2_B____) {
        return saber_styles_t::SS_MEDIUM;
    }
    if (anim >= animNumber_t::BOTH_A3_T__B_ && anim <= animNumber_t::BOTH_D3_B____) {
        return saber_styles_t::SS_STRONG;
    }
    if (anim >= animNumber_t::BOTH_A4_T__B_ && anim <= animNumber_t::BOTH_D4_B____) { // desann
        return saber_styles_t::SS_DESANN;
    }
    if (anim >= animNumber_t::BOTH_A5_T__B_ && anim <= animNumber_t::BOTH_D5_B____) { // tavion
        return saber_styles_t::SS_TAVION;
    }
    if (anim >= animNumber_t::BOTH_A6_T__B_ && anim <= animNumber_t::BOTH_D6_B____) { // dual
        return saber_styles_t::SS_DUAL;
    }
    if (anim >= animNumber_t::BOTH_A7_T__B_ && anim <= animNumber_t::BOTH_D7_B____) { // staff
        return saber_styles_t::SS_STAFF;
    }
    return saber_styles_t::SS_NONE;
}

forcePowerLevels_t PM_PowerLevelForSaberAnim(playerState_t* ps, int saberNum)
{
    animNumber_t anim = ps->torsoAnim;
    int animTimeElapsed = PM_AnimLength(g_entities[ps->clientNum].client->clientInfo.animFileIndex, anim) - ps->torsoAnimTimer;
    if (anim >= animNumber_t::BOTH_A1_T__B_ && anim <= animNumber_t::BOTH_D1_B____) {
        // FIXME: these two need their own style
        if (ps->saber[0].type == saberType_t::SABER_LANCE) {
            return forcePowerLevels_t::FORCE_LEVEL_4;
        }
        else if (ps->saber[0].type == saberType_t::SABER_TRIDENT) {
            return forcePowerLevels_t::FORCE_LEVEL_3;
        }
        return forcePowerLevels_t::FORCE_LEVEL_1;
    }
    if (anim >= animNumber_t::BOTH_A2_T__B_ && anim <= animNumber_t::BOTH_D2_B____) {
        return forcePowerLevels_t::FORCE_LEVEL_2;
    }
    if (anim >= animNumber_t::BOTH_A3_T__B_ && anim <= animNumber_t::BOTH_D3_B____) {
        return forcePowerLevels_t::FORCE_LEVEL_3;
    }
    if (anim >= animNumber_t::BOTH_A4_T__B_ && anim <= animNumber_t::BOTH_D4_B____) { // desann
        return forcePowerLevels_t::FORCE_LEVEL_4;
    }
    if (anim >= animNumber_t::BOTH_A5_T__B_ && anim <= animNumber_t::BOTH_D5_B____) { // tavion
        return forcePowerLevels_t::FORCE_LEVEL_2;
    }
    if (anim >= animNumber_t::BOTH_A6_T__B_ && anim <= animNumber_t::BOTH_D6_B____) { // dual
        return forcePowerLevels_t::FORCE_LEVEL_2;
    }
    if (anim >= animNumber_t::BOTH_A7_T__B_ && anim <= animNumber_t::BOTH_D7_B____) { // staff
        return forcePowerLevels_t::FORCE_LEVEL_2;
    }
    if ((anim >= animNumber_t::BOTH_P1_S1_T_ && anim <= animNumber_t::BOTH_P1_S1_BR) ||
        (anim >= animNumber_t::BOTH_P6_S6_T_ && anim <= animNumber_t::BOTH_P6_S6_BR) ||
        (anim >= animNumber_t::BOTH_P7_S7_T_ && anim <= animNumber_t::BOTH_P7_S7_BR)) { // parries
        switch (ps->saberAnimLevel) {
        case saber_styles_t::SS_STRONG:
        case saber_styles_t::SS_DESANN:
            return forcePowerLevels_t::FORCE_LEVEL_3;
            break;
        case saber_styles_t::SS_TAVION:
        case saber_styles_t::SS_STAFF:
        case saber_styles_t::SS_DUAL:
        case saber_styles_t::SS_MEDIUM:
            return forcePowerLevels_t::FORCE_LEVEL_2;
            break;
        case saber_styles_t::SS_FAST:
            return forcePowerLevels_t::FORCE_LEVEL_1;
            break;
        default:
            return forcePowerLevels_t::FORCE_LEVEL_0;
            break;
        }
    }
    if ((anim >= animNumber_t::BOTH_K1_S1_T_ && anim <= animNumber_t::BOTH_K1_S1_BR) ||
        (anim >= animNumber_t::BOTH_K6_S6_T_ && anim <= animNumber_t::BOTH_K6_S6_BR) ||
        (anim >= animNumber_t::BOTH_K7_S7_T_ && anim <= animNumber_t::BOTH_K7_S7_BR)) { // knockaways
        return forcePowerLevels_t::FORCE_LEVEL_3;
    }
    if ((anim >= animNumber_t::BOTH_V1_BR_S1 && anim <= animNumber_t::BOTH_V1_B__S1) ||
        (anim >= animNumber_t::BOTH_V6_BR_S6 && anim <= animNumber_t::BOTH_V6_B__S6) ||
        (anim >= animNumber_t::BOTH_V7_BR_S7 && anim <= animNumber_t::BOTH_V7_B__S7)) { // knocked-away attacks
        return forcePowerLevels_t::FORCE_LEVEL_1;
    }
    if ((anim >= animNumber_t::BOTH_H1_S1_T_ && anim <= animNumber_t::BOTH_H1_S1_BR) ||
        (anim >= animNumber_t::BOTH_H6_S6_T_ && anim <= animNumber_t::BOTH_H6_S6_BR) ||
        (anim >= animNumber_t::BOTH_H7_S7_T_ && anim <= animNumber_t::BOTH_H7_S7_BR)) { // broken parries
        return forcePowerLevels_t::FORCE_LEVEL_0;
    }
    switch (anim) {
    case animNumber_t::BOTH_A2_STABBACK1:
        if (ps->torsoAnimTimer < 450) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 400) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_ATTACK_BACK:
        if (ps->torsoAnimTimer < 500) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_CROUCHATTACKBACK1:
        if (ps->torsoAnimTimer < 800) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_BUTTERFLY_LEFT:
    case animNumber_t::BOTH_BUTTERFLY_RIGHT:
    case animNumber_t::BOTH_BUTTERFLY_FL1:
    case animNumber_t::BOTH_BUTTERFLY_FR1:
        // FIXME: break up?
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_FJSS_TR_BL:
    case animNumber_t::BOTH_FJSS_TL_BR:
        // FIXME: break up?
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_K1_S1_T_: // # knockaway saber top
    case animNumber_t::BOTH_K1_S1_TR: // # knockaway saber top right
    case animNumber_t::BOTH_K1_S1_TL: // # knockaway saber top left
    case animNumber_t::BOTH_K1_S1_BL: // # knockaway saber bottom left
    case animNumber_t::BOTH_K1_S1_B_: // # knockaway saber bottom
    case animNumber_t::BOTH_K1_S1_BR: // # knockaway saber bottom right
        // FIXME: break up?
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_LUNGE2_B__T_:
        if (ps->torsoAnimTimer < 400) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 150) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_FORCELEAP2_T__B_:
        if (ps->torsoAnimTimer < 400) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 550) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_VS_ATR_S:
    case animNumber_t::BOTH_VS_ATL_S:
    case animNumber_t::BOTH_VT_ATR_S:
    case animNumber_t::BOTH_VT_ATL_S:
        return forcePowerLevels_t::FORCE_LEVEL_3; //???
        break;
    case animNumber_t::BOTH_JUMPFLIPSLASHDOWN1:
        if (ps->torsoAnimTimer <= 900) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 550) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_JUMPFLIPSTABDOWN:
        if (ps->torsoAnimTimer <= 1200) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed <= 250) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_JUMPATTACK6:
        /*
        if (pm->ps)
        {
            if ( ( pm->ps->legsAnimTimer >= 1450
                    && PM_AnimLength( g_entities[ps->clientNum].client->clientInfo.animFileIndex, animNumber_t::BOTH_JUMPATTACK6 ) - pm->ps->legsAnimTimer >=
        400 )
                ||(pm->ps->legsAnimTimer >= 400
                    && PM_AnimLength( g_entities[ps->clientNum].client->clientInfo.animFileIndex, animNumber_t::BOTH_JUMPATTACK6 ) - pm->ps->legsAnimTimer >=
        1100 ) )
            {//pretty much sideways
                return forcePowerLevels_t::FORCE_LEVEL_3;
            }
        }
        */
        if ((ps->torsoAnimTimer >= 1450 && animTimeElapsed >= 400) || (ps->torsoAnimTimer >= 400 && animTimeElapsed >= 1100)) { // pretty much sideways
            return forcePowerLevels_t::FORCE_LEVEL_3;
        }
        return forcePowerLevels_t::FORCE_LEVEL_0;
        break;
    case animNumber_t::BOTH_JUMPATTACK7:
        if (ps->torsoAnimTimer <= 1200) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 200) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_SPINATTACK6:
        if (animTimeElapsed <= 200) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_SPINATTACK7:
        if (ps->torsoAnimTimer <= 500) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 500) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_FORCELONGLEAP_ATTACK:
        if (animTimeElapsed <= 200) { // 1st four frames of anim
            return forcePowerLevels_t::FORCE_LEVEL_3;
        }
        break;
    /*
    case animNumber_t::BOTH_A7_KICK_F://these kicks attack, too
    case animNumber_t::BOTH_A7_KICK_B:
    case animNumber_t::BOTH_A7_KICK_R:
    case animNumber_t::BOTH_A7_KICK_L:
        //FIXME: break up
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    */
    case animNumber_t::BOTH_STABDOWN:
        if (ps->torsoAnimTimer <= 900) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_3;
        }
        break;
    case animNumber_t::BOTH_STABDOWN_STAFF:
        if (ps->torsoAnimTimer <= 850) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_3;
        }
        break;
    case animNumber_t::BOTH_STABDOWN_DUAL:
        if (ps->torsoAnimTimer <= 900) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_3;
        }
        break;
    case animNumber_t::BOTH_A6_SABERPROTECT:
        if (ps->torsoAnimTimer < 650) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_A7_SOULCAL:
        if (ps->torsoAnimTimer < 650) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 600) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_A1_SPECIAL:
        if (ps->torsoAnimTimer < 600) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 200) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_A2_SPECIAL:
        if (ps->torsoAnimTimer < 300) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 200) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_A3_SPECIAL:
        if (ps->torsoAnimTimer < 700) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 200) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_FLIP_ATTACK7:
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_PULL_IMPALE_STAB:
        if (ps->torsoAnimTimer < 1000) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_PULL_IMPALE_SWING:
        if (ps->torsoAnimTimer < 500) // 750 )
        {                             // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 650) // 600 )
        {                               // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_ALORA_SPIN_SLASH:
        if (ps->torsoAnimTimer < 900) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 250) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_A6_FB:
        if (ps->torsoAnimTimer < 250) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 250) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_A6_LR:
        if (ps->torsoAnimTimer < 250) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 250) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_A7_HILT:
        return forcePowerLevels_t::FORCE_LEVEL_0;
        break;
        //===SABERLOCK SUPERBREAKS START===========================================================================
    case animNumber_t::BOTH_LK_S_DL_T_SB_1_W:
        if (ps->torsoAnimTimer < 700) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_5;
        break;
    case animNumber_t::BOTH_LK_S_ST_S_SB_1_W:
        if (ps->torsoAnimTimer < 300) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_5;
        break;
    case animNumber_t::BOTH_LK_S_DL_S_SB_1_W:
    case animNumber_t::BOTH_LK_S_S_S_SB_1_W:
        if (ps->torsoAnimTimer < 700) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 400) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_5;
        break;
    case animNumber_t::BOTH_LK_S_ST_T_SB_1_W:
    case animNumber_t::BOTH_LK_S_S_T_SB_1_W:
        if (ps->torsoAnimTimer < 150) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 400) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_5;
        break;
    case animNumber_t::BOTH_LK_DL_DL_T_SB_1_W:
        return forcePowerLevels_t::FORCE_LEVEL_5;
        break;
    case animNumber_t::BOTH_LK_DL_DL_S_SB_1_W:
    case animNumber_t::BOTH_LK_DL_ST_S_SB_1_W:
        if (animTimeElapsed < 1000) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_5;
        break;
    case animNumber_t::BOTH_LK_DL_ST_T_SB_1_W:
        if (ps->torsoAnimTimer < 950) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 650) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_5;
        break;
    case animNumber_t::BOTH_LK_DL_S_S_SB_1_W:
        if (saberNum != 0) { // only right hand saber does damage in this suberbreak
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        if (ps->torsoAnimTimer < 900) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 450) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_5;
        break;
    case animNumber_t::BOTH_LK_DL_S_T_SB_1_W:
        if (saberNum != 0) { // only right hand saber does damage in this suberbreak
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        if (ps->torsoAnimTimer < 250) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 150) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_5;
        break;
    case animNumber_t::BOTH_LK_ST_DL_S_SB_1_W:
        return forcePowerLevels_t::FORCE_LEVEL_5;
        break;
    case animNumber_t::BOTH_LK_ST_DL_T_SB_1_W:
        // special suberbreak - doesn't kill, just kicks them backwards
        return forcePowerLevels_t::FORCE_LEVEL_0;
        break;
    case animNumber_t::BOTH_LK_ST_ST_S_SB_1_W:
    case animNumber_t::BOTH_LK_ST_S_S_SB_1_W:
        if (ps->torsoAnimTimer < 800) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 350) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_5;
        break;
    case animNumber_t::BOTH_LK_ST_ST_T_SB_1_W:
    case animNumber_t::BOTH_LK_ST_S_T_SB_1_W:
        return forcePowerLevels_t::FORCE_LEVEL_5;
        break;
        //===SABERLOCK SUPERBREAKS START===========================================================================
    case animNumber_t::BOTH_HANG_ATTACK:
        // FIME: break up
        if (ps->torsoAnimTimer < 1000) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (animTimeElapsed < 250) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else { // sweet spot
            return forcePowerLevels_t::FORCE_LEVEL_5;
        }
        break;
    case animNumber_t::BOTH_ROLL_STAB:
        if (animTimeElapsed > 400) { // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else {
            return forcePowerLevels_t::FORCE_LEVEL_3;
        }
        break;
    default:
        break;
    }
    return forcePowerLevels_t::FORCE_LEVEL_0;
}

bool PM_InAnimForSaberMove(animNumber_t anim, saberMoveName_t saberMove)
{
    switch (anim) { // special case anims
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
    case animNumber_t::BOTH_VS_ATR_S:
    case animNumber_t::BOTH_VS_ATL_S:
    case animNumber_t::BOTH_VT_ATR_S:
    case animNumber_t::BOTH_VT_ATL_S:
    case animNumber_t::BOTH_FORCELONGLEAP_ATTACK:
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
    case animNumber_t::BOTH_LK_S_DL_S_SB_1_W:
    case animNumber_t::BOTH_LK_S_DL_T_SB_1_W:
    case animNumber_t::BOTH_LK_S_ST_S_SB_1_W:
    case animNumber_t::BOTH_LK_S_ST_T_SB_1_W:
    case animNumber_t::BOTH_LK_S_S_S_SB_1_W:
    case animNumber_t::BOTH_LK_S_S_T_SB_1_W:
    case animNumber_t::BOTH_LK_DL_DL_S_SB_1_W:
    case animNumber_t::BOTH_LK_DL_DL_T_SB_1_W:
    case animNumber_t::BOTH_LK_DL_ST_S_SB_1_W:
    case animNumber_t::BOTH_LK_DL_ST_T_SB_1_W:
    case animNumber_t::BOTH_LK_DL_S_S_SB_1_W:
    case animNumber_t::BOTH_LK_DL_S_T_SB_1_W:
    case animNumber_t::BOTH_LK_ST_DL_S_SB_1_W:
    case animNumber_t::BOTH_LK_ST_DL_T_SB_1_W:
    case animNumber_t::BOTH_LK_ST_ST_S_SB_1_W:
    case animNumber_t::BOTH_LK_ST_ST_T_SB_1_W:
    case animNumber_t::BOTH_LK_ST_S_S_SB_1_W:
    case animNumber_t::BOTH_LK_ST_S_T_SB_1_W:
    case animNumber_t::BOTH_HANG_ATTACK:
        return true;
    default:
        break;
    }
    if (PM_SaberDrawPutawayAnim(anim)) {
        if (saberMove == saberMoveName_t::LS_DRAW || saberMove == saberMoveName_t::LS_PUTAWAY) {
            return true;
        }
        return false;
    }
    else if (PM_SaberStanceAnim(anim)) {
        if (saberMove == saberMoveName_t::LS_READY) {
            return true;
        }
        return false;
    }
    const saber_styles_t animLevel = PM_AnimLevelForSaberAnim(anim);
    if (animLevel <= saber_styles_t::SS_NONE) { // NOTE: this will always return false for the ready poses and putaway/draw...
        return false;
    }
    // drop the anim to the first level and start the checks there
    anim -= (animLevel - saber_styles_t::SS_FAST) * SABER_ANIM_GROUP_SIZE;
    // check level 1
    if (anim == saberMoveData[saberMove].animToUse) {
        return true;
    }
    // check level 2
    anim += SABER_ANIM_GROUP_SIZE;
    if (anim == saberMoveData[saberMove].animToUse) {
        return true;
    }
    // check level 3
    anim += SABER_ANIM_GROUP_SIZE;
    if (anim == saberMoveData[saberMove].animToUse) {
        return true;
    }
    // check level 4
    anim += SABER_ANIM_GROUP_SIZE;
    if (anim == saberMoveData[saberMove].animToUse) {
        return true;
    }
    // check level 5
    anim += SABER_ANIM_GROUP_SIZE;
    if (anim == saberMoveData[saberMove].animToUse) {
        return true;
    }
    if (anim >= animNumber_t::BOTH_P1_S1_T_ && anim <= animNumber_t::BOTH_H1_S1_BR) { // parries, knockaways and broken parries
        return (anim == saberMoveData[saberMove].animToUse);
    }
    return false;
}

bool PM_SaberInIdle(saberMoveName_t move)
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
bool PM_SaberInSpecialAttack(animNumber_t anim)
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
    case animNumber_t::BOTH_LK_S_DL_S_SB_1_W:
    case animNumber_t::BOTH_LK_S_DL_T_SB_1_W:
    case animNumber_t::BOTH_LK_S_ST_S_SB_1_W:
    case animNumber_t::BOTH_LK_S_ST_T_SB_1_W:
    case animNumber_t::BOTH_LK_S_S_S_SB_1_W:
    case animNumber_t::BOTH_LK_S_S_T_SB_1_W:
    case animNumber_t::BOTH_LK_DL_DL_S_SB_1_W:
    case animNumber_t::BOTH_LK_DL_DL_T_SB_1_W:
    case animNumber_t::BOTH_LK_DL_ST_S_SB_1_W:
    case animNumber_t::BOTH_LK_DL_ST_T_SB_1_W:
    case animNumber_t::BOTH_LK_DL_S_S_SB_1_W:
    case animNumber_t::BOTH_LK_DL_S_T_SB_1_W:
    case animNumber_t::BOTH_LK_ST_DL_S_SB_1_W:
    case animNumber_t::BOTH_LK_ST_DL_T_SB_1_W:
    case animNumber_t::BOTH_LK_ST_ST_S_SB_1_W:
    case animNumber_t::BOTH_LK_ST_ST_T_SB_1_W:
    case animNumber_t::BOTH_LK_ST_S_S_SB_1_W:
    case animNumber_t::BOTH_LK_ST_S_T_SB_1_W:
    case animNumber_t::BOTH_HANG_ATTACK:
        return true;
    default:
        break;
    }
    return false;
}

bool PM_SaberInAttackPure(saberMoveName_t move)
{
    if (move >= saberMoveName_t::LS_A_TL2BR && move <= saberMoveName_t::LS_A_T2B) {
        return true;
    }
    return false;
}
bool PM_SaberInAttack(saberMoveName_t move)
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
bool PM_SaberInTransition(saberMoveName_t move)
{
    if (move >= saberMoveName_t::LS_T1_BR__R && move <= saberMoveName_t::LS_T1_BL__L) {
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
bool PM_SaberInTransitionAny(saberMoveName_t move)
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

bool PM_SaberInSpecial(saberMoveName_t move)
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

bool PM_KickMove(saberMoveName_t move)
{
    switch (move) {
    case saberMoveName_t::LS_KICK_F:
    case saberMoveName_t::LS_KICK_B:
    case saberMoveName_t::LS_KICK_R:
    case saberMoveName_t::LS_KICK_L:
    case saberMoveName_t::LS_KICK_S:
    case saberMoveName_t::LS_KICK_BF:
    case saberMoveName_t::LS_KICK_RL:
    case saberMoveName_t::LS_HILT_BASH:
    case saberMoveName_t::LS_KICK_F_AIR:
    case saberMoveName_t::LS_KICK_B_AIR:
    case saberMoveName_t::LS_KICK_R_AIR:
    case saberMoveName_t::LS_KICK_L_AIR:
        return true;
    default:
        break;
    }
    return false;
}

bool PM_SaberCanInterruptMove(saberMoveName_t move, animNumber_t anim)
{
    if (PM_InAnimForSaberMove(anim, move)) {
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
        case saberMoveName_t::LS_KICK_S:
        case saberMoveName_t::LS_KICK_BF:
        case saberMoveName_t::LS_KICK_RL:
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
            return false;
        default:
            break;
        }

        if (PM_SaberInAttackPure(move)) {
            return false;
        }
        if (PM_SaberInStart(move)) {
            return false;
        }
        if (PM_SaberInTransition(move)) {
            return false;
        }
        if (PM_SaberInBounce(move)) {
            return false;
        }
        if (PM_SaberInBrokenParry(move)) {
            return false;
        }
        if (PM_SaberInDeflect(move)) {
            return false;
        }
        if (PM_SaberInParry(move)) {
            return false;
        }
        if (PM_SaberInKnockaway(move)) {
            return false;
        }
        if (PM_SaberInReflect(move)) {
            return false;
        }
    }
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
    case animNumber_t::BOTH_A7_KICK_S:
    case animNumber_t::BOTH_A7_KICK_BF:
    case animNumber_t::BOTH_A7_KICK_RL:
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
    case animNumber_t::BOTH_LK_S_DL_S_SB_1_W:
    case animNumber_t::BOTH_LK_S_DL_T_SB_1_W:
    case animNumber_t::BOTH_LK_S_ST_S_SB_1_W:
    case animNumber_t::BOTH_LK_S_ST_T_SB_1_W:
    case animNumber_t::BOTH_LK_S_S_S_SB_1_W:
    case animNumber_t::BOTH_LK_S_S_T_SB_1_W:
    case animNumber_t::BOTH_LK_DL_DL_S_SB_1_W:
    case animNumber_t::BOTH_LK_DL_DL_T_SB_1_W:
    case animNumber_t::BOTH_LK_DL_ST_S_SB_1_W:
    case animNumber_t::BOTH_LK_DL_ST_T_SB_1_W:
    case animNumber_t::BOTH_LK_DL_S_S_SB_1_W:
    case animNumber_t::BOTH_LK_DL_S_T_SB_1_W:
    case animNumber_t::BOTH_LK_ST_DL_S_SB_1_W:
    case animNumber_t::BOTH_LK_ST_DL_T_SB_1_W:
    case animNumber_t::BOTH_LK_ST_ST_S_SB_1_W:
    case animNumber_t::BOTH_LK_ST_ST_T_SB_1_W:
    case animNumber_t::BOTH_LK_ST_S_S_SB_1_W:
    case animNumber_t::BOTH_LK_ST_S_T_SB_1_W:
    case animNumber_t::BOTH_HANG_ATTACK:
        return false;
    default:
        break;
    }
    return true;
}

saberMoveName_t PM_BrokenParryForAttack(saberMoveName_t move)
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

saberMoveName_t PM_BrokenParryForParry(saberMoveName_t move)
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

saberMoveName_t PM_KnockawayForParry(saberBlockedType_t move)
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

EnumArray<EnumArray<int, saberQuadrant_t, saberQuadrant_t::Q_NUM_QUADS>, saberQuadrant_t, saberQuadrant_t::Q_NUM_QUADS> saberMoveTransitionAngle = {
    {{
         0,   // Q_BR,Q_BR,
         45,  // Q_BR,Q_R,
         90,  // Q_BR,Q_TR,
         135, // Q_BR,Q_T,
         180, // Q_BR,Q_TL,
         215, // Q_BR,Q_L,
         270, // Q_BR,Q_BL,
         45,  // Q_BR,Q_B,
     },
     {
         45,  // Q_R,Q_BR,
         0,   // Q_R,Q_R,
         45,  // Q_R,Q_TR,
         90,  // Q_R,Q_T,
         135, // Q_R,Q_TL,
         180, // Q_R,Q_L,
         215, // Q_R,Q_BL,
         90,  // Q_R,Q_B,
     },
     {
         90,  // Q_TR,Q_BR,
         45,  // Q_TR,Q_R,
         0,   // Q_TR,Q_TR,
         45,  // Q_TR,Q_T,
         90,  // Q_TR,Q_TL,
         135, // Q_TR,Q_L,
         180, // Q_TR,Q_BL,
         135, // Q_TR,Q_B,
     },
     {
         135, // Q_T,Q_BR,
         90,  // Q_T,Q_R,
         45,  // Q_T,Q_TR,
         0,   // Q_T,Q_T,
         45,  // Q_T,Q_TL,
         90,  // Q_T,Q_L,
         135, // Q_T,Q_BL,
         180, // Q_T,Q_B,
     },
     {
         180, // Q_TL,Q_BR,
         135, // Q_TL,Q_R,
         90,  // Q_TL,Q_TR,
         45,  // Q_TL,Q_T,
         0,   // Q_TL,Q_TL,
         45,  // Q_TL,Q_L,
         90,  // Q_TL,Q_BL,
         135, // Q_TL,Q_B,
     },
     {
         215, // Q_L,Q_BR,
         180, // Q_L,Q_R,
         135, // Q_L,Q_TR,
         90,  // Q_L,Q_T,
         45,  // Q_L,Q_TL,
         0,   // Q_L,Q_L,
         45,  // Q_L,Q_BL,
         90,  // Q_L,Q_B,
     },
     {
         270, // Q_BL,Q_BR,
         215, // Q_BL,Q_R,
         180, // Q_BL,Q_TR,
         135, // Q_BL,Q_T,
         90,  // Q_BL,Q_TL,
         45,  // Q_BL,Q_L,
         0,   // Q_BL,Q_BL,
         45,  // Q_BL,Q_B,
     },
     {
         45,  // Q_B,Q_BR,
         90,  // Q_B,Q_R,
         135, // Q_B,Q_TR,
         180, // Q_B,Q_T,
         135, // Q_B,Q_TL,
         90,  // Q_B,Q_L,
         45,  // Q_B,Q_BL,
         0    // Q_B,Q_B,
     }}};

int PM_SaberAttackChainAngle(saberMoveName_t move1, saberMoveName_t move2)
{
    if (move1 == saberMoveName_t::LS_INVALID || move2 == saberMoveName_t::LS_INVALID) {
        return -1;
    }
    return saberMoveTransitionAngle[saberMoveData[move1].endQuad][saberMoveData[move2].startQuad];
}

bool PM_SaberKataDone(saberMoveName_t curmove = saberMoveName_t::LS_NONE, saberMoveName_t newmove = saberMoveName_t::LS_NONE)
{
    if (pm->ps->forceRageRecoveryTime > level.time) { // rage recovery, only 1 swing at a time (tired)
        if (pm->ps->saberAttackChainCount > 0) {      // swung once
            return true;
        }
        else { // allow one attack
            return false;
        }
    }
    else if ((pm->ps->forcePowersActive & Bit(forcePowers_t::FP_RAGE))) { // infinite chaining when raged
        return false;
    }
    else if (pm->ps->saber[0].maxChain == -1) {
        return false;
    }
    else if (pm->ps->saber[0].maxChain != 0) {
        if (pm->ps->saberAttackChainCount >= pm->ps->saber[0].maxChain) {
            return true;
        }
        else {
            return false;
        }
    }

    if (pm->ps->saberAnimLevel == saber_styles_t::SS_DESANN ||
        pm->ps->saberAnimLevel == saber_styles_t::SS_TAVION) { // desann and tavion can link up as many attacks as they want
        return false;
    }
    // FIXME: instead of random, apply some sort of logical conditions to whether or
    //		not you can chain?  Like if you were completely missed, you can't chain as much, or...?
    //		And/Or based on FP_SABER_OFFENSE level?  So number of attacks you can chain
    //		increases with your FP_SABER_OFFENSE skill?
    if (pm->ps->saberAnimLevel == saber_styles_t::SS_STAFF) {
        // TEMP: for now, let staff attacks infinitely chain
        return false;
        /*
        if ( pm->ps->saberAttackChainCount > Q_irand( 3, 4 ) )
        {
            return true;
        }
        else if ( pm->ps->saberAttackChainCount > 0 )
        {
            int chainAngle = PM_SaberAttackChainAngle( curmove, newmove );
            if ( chainAngle < 135 || chainAngle > 215 )
            {//if trying to chain to a move that doesn't continue the momentum
                if ( pm->ps->saberAttackChainCount > 1 )
                {
                    return true;
                }
            }
            else if ( chainAngle == 180 )
            {//continues the momentum perfectly, allow it to chain 66% of the time
                if ( pm->ps->saberAttackChainCount > 2 )
                {
                    return true;
                }
            }
            else
            {//would continue the movement somewhat, 50% chance of continuing
                if ( pm->ps->saberAttackChainCount > 3 )
                {
                    return true;
                }
            }
        }
        */
    }
    else if (pm->ps->saberAnimLevel == saber_styles_t::SS_DUAL) {
        // TEMP: for now, let staff attacks infinitely chain
        return false;
    }
    else if (pm->ps->saberAnimLevel == saber_styles_t::SS_STRONG) {
        if (curmove == saberMoveName_t::LS_NONE || newmove == saberMoveName_t::LS_NONE) {
            if (pm->ps->saberAnimLevel >= saber_styles_t::SS_STRONG && pm->ps->saberAttackChainCount > Q_irand(0, 1)) {
                return true;
            }
        }
        else if (pm->ps->saberAttackChainCount > Q_irand(2, 3)) {
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
    else { // FIXME: have chainAngle influence fast and medium chains as well?
        if ((pm->ps->saberAnimLevel == saber_styles_t::SS_MEDIUM || pm->ps->saberAnimLevel == saber_styles_t::SS_DUAL) &&
            pm->ps->saberAttackChainCount > Q_irand(2, 5)) {
            return true;
        }
    }
    return false;
}

bool PM_CheckEnemyInBack(float backCheckDist)
{
    if (!pm->gent || !pm->gent->client) {
        return false;
    }
    if ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) && !g_saberAutoAim->integer && pm->cmd.forwardmove >= 0) { // don't auto-backstab
        return false;
    }
    if (pm->ps->groundEntityNum == ENTITYNUM_NONE) { // only when on ground
        return false;
    }
    trace_t trace;
    vec3_t end, fwd, fwdAngles = {0, pm->ps->viewangles[YAW], 0};

    AngleVectors(fwdAngles, fwd, nullptr, nullptr);
    VectorMA(pm->ps->origin, -backCheckDist, fwd, end);

    pm->trace(&trace, pm->ps->origin, vec3_origin, vec3_origin, end, pm->ps->clientNum, contentsFlags_t::CONTENTS_SOLID | contentsFlags_t::CONTENTS_BODY,
              EG2_Collision::G2_NOCOLLIDE, 0);
    if (trace.fraction < 1.0f && trace.entityNum < ENTITYNUM_WORLD) {
        gentity_t* traceEnt = &g_entities[trace.entityNum];
        if (traceEnt && traceEnt->health > 0 && traceEnt->client && traceEnt->client->playerTeam == pm->gent->client->enemyTeam &&
            traceEnt->client->ps.groundEntityNum != ENTITYNUM_NONE) {
            if ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer())) { // player
                if (pm->gent) {                                                 // set player enemy to traceEnt so he auto-aims at him
                    pm->gent->enemy = traceEnt;
                }
            }
            return true;
        }
    }
    return false;
}

saberMoveName_t PM_PickBackStab(void)
{
    if (!pm->gent || !pm->gent->client) {
        return saberMoveName_t::LS_READY;
    }
    if (pm->ps->dualSabers && pm->ps->saber[1].Active()) {
        if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) {
            return saberMoveName_t::LS_A_BACK_CR;
        }
        else {
            return saberMoveName_t::LS_A_BACK;
        }
    }
    if (pm->gent->client->ps.saberAnimLevel == saber_styles_t::SS_TAVION) {
        return saberMoveName_t::LS_A_BACKSTAB;
    }
    else if (pm->gent->client->ps.saberAnimLevel == saber_styles_t::SS_DESANN) {
        if (pm->ps->saberMove == saberMoveName_t::LS_READY || !Q_irand(0, 3)) {
            return saberMoveName_t::LS_A_BACKSTAB;
        }
        else if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) {
            return saberMoveName_t::LS_A_BACK_CR;
        }
        else {
            return saberMoveName_t::LS_A_BACK;
        }
    }
    else if (pm->ps->saberAnimLevel == saber_styles_t::SS_MEDIUM || pm->ps->saberAnimLevel == saber_styles_t::SS_DUAL) { // using medium attacks or dual sabers
        if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) {
            return saberMoveName_t::LS_A_BACK_CR;
        }
        else {
            return saberMoveName_t::LS_A_BACK;
        }
    }
    else {
        return saberMoveName_t::LS_A_BACKSTAB;
    }
}

saberMoveName_t PM_CheckStabDown(void)
{
    if (!pm->gent || !pm->gent->enemy || !pm->gent->enemy->client) {
        return saberMoveName_t::LS_NONE;
    }
    if (Any(pm->ps->saber[0].saberFlags & saberFlags_t::SFL_NO_STABDOWN)) {
        return saberMoveName_t::LS_NONE;
    }
    if (pm->ps->dualSabers && Any(pm->ps->saber[1].saberFlags & saberFlags_t::SFL_NO_STABDOWN)) {
        return saberMoveName_t::LS_NONE;
    }
    if (pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) // PLAYER ONLY
    {                                                               // player
        if (G_TryingKataAttack(pm->gent, &pm->cmd))                 // Any(pm->cmd.buttons&buttonFlags_t::BUTTON_FORCE_FOCUS) )//Holding focus
        {                                                           // want to try a special
            return saberMoveName_t::LS_NONE;
        }
    }
    if ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer())) {                                         // player
        if (pm->ps->groundEntityNum == ENTITYNUM_NONE)                                                          // in air
        {                                                                                                       // sorry must be on ground (or have just jumped)
            if (level.time - pm->ps->lastOnGround <= 50 && Any(pm->ps->pm_flags & pmoveFlags_t::PMF_JUMPING)) { // just jumped, it's okay
            }
            else {
                return saberMoveName_t::LS_NONE;
            }
        }
        /*
        if ( pm->cmd.upmove > 0 )
        {//trying to jump
        }
        else if ( pm->ps->groundEntityNum == ENTITYNUM_NONE //in air
            && level.time-pm->ps->lastOnGround <= 250 //just left ground
            && Any(pm->ps->pm_flags&pmoveFlags_t::PMF_JUMPING) )//jumped
        {//just jumped
        }
        else
        {
            return saberMoveName_t::LS_NONE;
        }
        */
        pm->ps->velocity[2] = 0;
        pm->cmd.upmove = 0;
    }
    else if ((pm->ps->clientNum >= MAX_CLIENTS && !PM_ControlledByPlayer())) { // NPC
        if (pm->ps->groundEntityNum == ENTITYNUM_NONE)                         // in air
        {                                                                      // sorry must be on ground (or have just jumped)
            if (level.time - pm->ps->lastOnGround <= 250 && Any(pm->ps->pm_flags & pmoveFlags_t::PMF_JUMPING)) { // just jumped, it's okay
            }
            else {
                return saberMoveName_t::LS_NONE;
            }
        }
        if (!pm->gent->NPC) { // wtf???
            return saberMoveName_t::LS_NONE;
        }
        if (Q_irand(0, to_underlying(rank_t::RANK_CAPTAIN)) > to_underlying(pm->gent->NPC->rank)) { // lower ranks do this less often
            return saberMoveName_t::LS_NONE;
        }
    }
    vec3_t enemyDir, faceFwd, facingAngles = {0, pm->ps->viewangles[YAW], 0};
    AngleVectors(facingAngles, faceFwd, nullptr, nullptr);
    VectorSubtract(pm->gent->enemy->currentOrigin, pm->ps->origin, enemyDir);
    float enemyZDiff = enemyDir[2];
    enemyDir[2] = 0;
    float enemyHDist = VectorNormalize(enemyDir) - (pm->gent->maxs[0] + pm->gent->enemy->maxs[0]);
    float dot = DotProduct(enemyDir, faceFwd);

    if ( //(pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer())
        dot > 0.65f
        //&& enemyHDist >= 32 //was 48
        && enemyHDist <= 164                                                                      // was 112
        && PM_InKnockDownOnGround(&pm->gent->enemy->client->ps)                                   // still on ground
        && !PM_InGetUpNoRoll(&pm->gent->enemy->client->ps)                                        // not getting up yet
        && enemyZDiff <= 20) {                                                                    // guy is on the ground below me, do a top-down attack
        if (pm->gent->enemy->s.number >= MAX_CLIENTS || !G_ControlledByPlayer(pm->gent->enemy)) { // don't get up while I'm doing this
            // stop them from trying to get up for at least another 3 seconds
            TIMER_Set(pm->gent->enemy, "noGetUpStraight", 3000);
        }
        // pick the right anim
        if (pm->ps->saberAnimLevel == saber_styles_t::SS_DUAL || (pm->ps->dualSabers && pm->ps->saber[1].Active())) {
            return saberMoveName_t::LS_STABDOWN_DUAL;
        }
        else if (pm->ps->saberAnimLevel == saber_styles_t::SS_STAFF) {
            return saberMoveName_t::LS_STABDOWN_STAFF;
        }
        else {
            return saberMoveName_t::LS_STABDOWN;
        }
    }
    return saberMoveName_t::LS_NONE;
}

extern saberMoveName_t PM_NPCSaberAttackFromQuad(saberQuadrant_t quad);
saberMoveName_t PM_SaberFlipOverAttackMove(void);
saberMoveName_t PM_AttackForEnemyPos(bool allowFB, bool allowStabDown)
{
    saberMoveName_t autoMove = saberMoveName_t::LS_INVALID;

    if (!pm->gent->enemy) {
        return saberMoveName_t::LS_NONE;
    }

    vec3_t enemy_org, enemyDir, faceFwd, faceRight, faceUp, facingAngles = {0, pm->ps->viewangles[YAW], 0};
    AngleVectors(facingAngles, faceFwd, faceRight, faceUp);
    // FIXME: predict enemy position?
    if (pm->gent->enemy->client) {
        // VectorCopy( pm->gent->enemy->currentOrigin, enemy_org );
        // HMM... using this will adjust for bbox size, so let's do that...
        vec3_t size;
        VectorSubtract(pm->gent->enemy->absmax, pm->gent->enemy->absmin, size);
        VectorMA(pm->gent->enemy->absmin, 0.5, size, enemy_org);

        VectorSubtract(pm->gent->enemy->client->renderInfo.eyePoint, pm->ps->origin, enemyDir);
    }
    else {
        if (pm->gent->enemy->bmodel && VectorCompare(vec3_origin, pm->gent->enemy->currentOrigin)) { // a brush model without an origin brush
            vec3_t size;
            VectorSubtract(pm->gent->enemy->absmax, pm->gent->enemy->absmin, size);
            VectorMA(pm->gent->enemy->absmin, 0.5, size, enemy_org);
        }
        else {
            VectorCopy(pm->gent->enemy->currentOrigin, enemy_org);
        }
        VectorSubtract(enemy_org, pm->ps->origin, enemyDir);
    }
    float enemyZDiff = enemyDir[2];
    float enemyDist = VectorNormalize(enemyDir);
    float dot = DotProduct(enemyDir, faceFwd);
    if (dot > 0) {           // enemy is in front
        if (allowStabDown) { // okay to try this
            saberMoveName_t stabDownMove = PM_CheckStabDown();
            if (stabDownMove != saberMoveName_t::LS_NONE) {
                return stabDownMove;
            }
        }
        if ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) && dot > 0.65f && enemyDist <= 64 && pm->gent->enemy->client &&
            (enemyZDiff <= 20 || PM_InKnockDownOnGround(&pm->gent->enemy->client->ps) ||
             PM_CrouchAnim(pm->gent->enemy->client->ps.legsAnim))) { // swing down at them
            return saberMoveName_t::LS_A_T2B;
        }
        if (allowFB) { // directly in front anim allowed
            if (!Any(pm->ps->saber[0].saberFlags & saberFlags_t::SFL_NO_BACK_ATTACK) &&
                (!pm->ps->dualSabers || !Any(pm->ps->saber[1].saberFlags & saberFlags_t::SFL_NO_BACK_ATTACK))) { // okay to do backstabs with this saber
                if (enemyDist > 200 || pm->gent->enemy->health <= 0) {                                           // hmm, look in back for an enemy
                    if (pm->ps->clientNum && !PM_ControlledByPlayer()) {                                         // player should never do this automatically
                        if (pm->ps->groundEntityNum != ENTITYNUM_NONE) {                                         // only when on ground
                            if (pm->gent && pm->gent->client && pm->gent->NPC && pm->gent->NPC->rank >= rank_t::RANK_LT_JG &&
                                Q_irand(0, to_underlying(pm->gent->NPC->rank)) >
                                    to_underlying(rank_t::RANK_ENSIGN)) { // only fencers and higher can do this, higher rank does it more
                                if (PM_CheckEnemyInBack(100)) {
                                    return PM_PickBackStab();
                                }
                            }
                        }
                    }
                }
            }
            // this is the default only if they're *right* in front...
            if ((pm->ps->clientNum && !PM_ControlledByPlayer()) ||
                ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) && cg.renderingThirdPerson && !cg.zoomMode)) { // NPC or player not in 1st person
                if (PM_CheckFlipOverAttackMove(true)) {                                                                      // enemy must be close and in front
                    return PM_SaberFlipOverAttackMove();
                }
            }
            if (PM_CheckLungeAttackMove()) { // NPC
                autoMove = PM_SaberLungeAttackMove(true);
            }
            else {
                autoMove = saberMoveName_t::LS_A_T2B;
            }
        }
        else { // pick a random one
            if (Q_irand(0, 1)) {
                autoMove = saberMoveName_t::LS_A_TR2BL;
            }
            else {
                autoMove = saberMoveName_t::LS_A_TL2BR;
            }
        }
        float dotR = DotProduct(enemyDir, faceRight);
        if (dotR > 0.35) { // enemy is to far right
            autoMove = saberMoveName_t::LS_A_L2R;
        }
        else if (dotR < -0.35) { // far left
            autoMove = saberMoveName_t::LS_A_R2L;
        }
        else if (dotR > 0.15) { // enemy is to near right
            autoMove = saberMoveName_t::LS_A_TR2BL;
        }
        else if (dotR < -0.15) { // near left
            autoMove = saberMoveName_t::LS_A_TL2BR;
        }
        if (DotProduct(enemyDir, faceUp) > 0.5) { // enemy is above me
            if (autoMove == saberMoveName_t::LS_A_TR2BL) {
                autoMove = saberMoveName_t::LS_A_BL2TR;
            }
            else if (autoMove == saberMoveName_t::LS_A_TL2BR) {
                autoMove = saberMoveName_t::LS_A_BR2TL;
            }
        }
    }
    else if (allowFB) { // back attack allowed
        // if ( !PM_InKnockDown( pm->ps ) )
        if (!Any(pm->ps->saber[0].saberFlags & saberFlags_t::SFL_NO_BACK_ATTACK) &&
            (!pm->ps->dualSabers || !Any(pm->ps->saber[1].saberFlags & saberFlags_t::SFL_NO_BACK_ATTACK))) { // okay to do backstabs with this saber
            if (pm->ps->groundEntityNum != ENTITYNUM_NONE) {                                                 // only when on ground
                if (!pm->gent->enemy->client ||
                    pm->gent->enemy->client->ps.groundEntityNum != ENTITYNUM_NONE) { // enemy not a client or is a client and on ground
                    if (dot < -0.75f && enemyDist < 128 &&
                        (pm->ps->saberAnimLevel == saber_styles_t::SS_FAST || pm->ps->saberAnimLevel == saber_styles_t::SS_STAFF ||
                         (pm->gent->client && (pm->gent->client->NPC_class == class_t::CLASS_TAVION || pm->gent->client->NPC_class == class_t::CLASS_ALORA) &&
                          Q_irand(0, 2)))) {                                                            // fast back-stab
                        if (!Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED) && pm->cmd.upmove >= 0) { // can't do it while ducked?
                            if ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) ||
                                (pm->gent->NPC && pm->gent->NPC->rank >= rank_t::RANK_LT_JG)) { // only fencers and above can do this
                                autoMove = saberMoveName_t::LS_A_BACKSTAB;
                            }
                        }
                    }
                    else if (pm->ps->saberAnimLevel != saber_styles_t::SS_FAST &&
                             pm->ps->saberAnimLevel != saber_styles_t::SS_STAFF) { // higher level back spin-attacks
                        if ((pm->ps->clientNum && !PM_ControlledByPlayer()) ||
                            ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) && cg.renderingThirdPerson && !cg.zoomMode)) {
                            if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED) || pm->cmd.upmove < 0) {
                                autoMove = saberMoveName_t::LS_A_BACK_CR;
                            }
                            else {
                                autoMove = saberMoveName_t::LS_A_BACK;
                            }
                        }
                    }
                }
            }
        }
    }
    return autoMove;
}

bool PM_InSecondaryStyle(void)
{
    if (pm->ps->saber[0].numBlades > 1 && pm->ps->saber[0].singleBladeStyle != saber_styles_t::SS_NONE &&
        (pm->ps->saber[0].stylesForbidden & Bit(pm->ps->saber[0].singleBladeStyle)) && pm->ps->saberAnimLevel == pm->ps->saber[0].singleBladeStyle) {
        return true;
    }

    if (pm->ps->dualSabers && !pm->ps->saber[1].Active()) // pm->ps->saberAnimLevel != saber_styles_t::SS_DUAL )
    {
        return true;
    }
    return false;
}

saberMoveName_t PM_SaberLungeAttackMove(bool fallbackToNormalLunge)
{
    G_DrainPowerForSpecialMove(pm->gent, forcePowers_t::FP_SABER_OFFENSE, SABER_ALT_ATTACK_POWER_FB);

    // see if we have an overridden (or cancelled) lunge move
    if (pm->ps->saber[0].lungeAtkMove != saberMoveName_t::LS_INVALID) {
        if (pm->ps->saber[0].lungeAtkMove != saberMoveName_t::LS_NONE) {
            return pm->ps->saber[0].lungeAtkMove;
        }
    }
    if (pm->ps->dualSabers) {
        if (pm->ps->saber[1].lungeAtkMove != saberMoveName_t::LS_INVALID) {
            if (pm->ps->saber[1].lungeAtkMove != saberMoveName_t::LS_NONE) {
                return pm->ps->saber[1].lungeAtkMove;
            }
        }
    }
    // no overrides, cancelled?
    if (pm->ps->saber[0].lungeAtkMove == saberMoveName_t::LS_NONE) {
        return saberMoveName_t::LS_NONE;
    }
    if (pm->ps->dualSabers) {
        if (pm->ps->saber[1].lungeAtkMove == saberMoveName_t::LS_NONE) {
            return saberMoveName_t::LS_NONE;
        }
    }
    // do normal checks
    if (pm->gent->client->NPC_class == class_t::CLASS_ALORA && !Q_irand(0, 3)) { // alora NPC
        return saberMoveName_t::LS_SPINATTACK_ALORA;
    }
    else {
        if (pm->ps->dualSabers) {
            return saberMoveName_t::LS_SPINATTACK_DUAL;
        }
        switch (pm->ps->saberAnimLevel) {
        case saber_styles_t::SS_DUAL:
            return saberMoveName_t::LS_SPINATTACK_DUAL;
            break;
        case saber_styles_t::SS_STAFF:
            return saberMoveName_t::LS_SPINATTACK;
            break;
        default: // normal lunge
            if (fallbackToNormalLunge) {
                vec3_t fwdAngles, jumpFwd;

                VectorCopy(pm->ps->viewangles, fwdAngles);
                fwdAngles[PITCH] = fwdAngles[ROLL] = 0;
                // do the lunge
                AngleVectors(fwdAngles, jumpFwd, nullptr, nullptr);
                VectorScale(jumpFwd, 150, pm->ps->velocity);
                pm->ps->velocity[2] = 50;
                PM_AddEvent(entity_event_t::EV_JUMP);

                return saberMoveName_t::LS_A_LUNGE;
            }
            break;
        }
    }
    return saberMoveName_t::LS_NONE;
}

bool PM_CheckLungeAttackMove(void)
{
    // check to see if it's cancelled?
    if (pm->ps->saber[0].lungeAtkMove == saberMoveName_t::LS_NONE) {
        if (pm->ps->dualSabers) {
            if (pm->ps->saber[1].lungeAtkMove == saberMoveName_t::LS_NONE || pm->ps->saber[1].lungeAtkMove == saberMoveName_t::LS_INVALID) {
                return false;
            }
        }
        else {
            return false;
        }
    }
    if (pm->ps->dualSabers) {
        if (pm->ps->saber[1].lungeAtkMove == saberMoveName_t::LS_NONE) {
            if (pm->ps->saber[0].lungeAtkMove == saberMoveName_t::LS_NONE || pm->ps->saber[0].lungeAtkMove == saberMoveName_t::LS_INVALID) {
                return false;
            }
        }
    }
    // do normal checks
    if (pm->ps->saberAnimLevel == saber_styles_t::SS_FAST                                 // fast
        || pm->ps->saberAnimLevel == saber_styles_t::SS_DUAL                              // dual
        || pm->ps->saberAnimLevel == saber_styles_t::SS_STAFF                             // staff
        || pm->ps->saberAnimLevel == saber_styles_t::SS_DESANN || pm->ps->dualSabers) {   // alt+back+attack using fast, dual or staff attacks
        if (pm->ps->clientNum >= MAX_CLIENTS && !PM_ControlledByPlayer()) {               // NPC
            if (pm->cmd.upmove < 0 || Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) { // ducking
                if (pm->ps->legsAnim == animNumber_t::BOTH_STAND2 || pm->ps->legsAnim == animNumber_t::BOTH_SABERFAST_STANCE ||
                    pm->ps->legsAnim == animNumber_t::BOTH_SABERSLOW_STANCE || pm->ps->legsAnim == animNumber_t::BOTH_SABERSTAFF_STANCE ||
                    pm->ps->legsAnim == animNumber_t::BOTH_SABERDUAL_STANCE ||
                    (level.time - pm->ps->lastStationary) <= 500) {  // standing or just stopped standing
                    if (pm->gent && pm->gent->NPC                    // NPC
                        && pm->gent->NPC->rank >= rank_t::RANK_LT_JG // high rank
                        &&
                        (pm->gent->NPC->rank == rank_t::RANK_LT_JG ||
                         Q_irand(-3, to_underlying(pm->gent->NPC->rank)) >=
                             to_underlying(rank_t::RANK_LT_JG))   // Q_irand( 0, to_underlying( pm->gent->NPC->rank ) ) >= to_underlying( rank_t::RANK_LT_JG ) )
                        && !Q_irand(0, 3 - g_spskill->integer)) { // only fencer and higher can do this
                        if (pm->ps->saberAnimLevel == saber_styles_t::SS_DESANN) {
                            if (!Q_irand(0, 4)) {
                                return true;
                            }
                        }
                        else {
                            return true;
                        }
                    }
                }
            }
        }
        else { // player
            if (G_TryingLungeAttack(pm->gent, &pm->cmd) &&
                G_EnoughPowerForSpecialMove(
                    pm->ps->forcePower,
                    SABER_ALT_ATTACK_POWER_FB) /*pm->ps->forcePower >= SABER_ALT_ATTACK_POWER_FB*/) // have enough force power to pull it off
            {
                return true;
            }
        }
    }

    return false;
}

saberMoveName_t PM_SaberJumpForwardAttackMove(void)
{
    G_DrainPowerForSpecialMove(pm->gent, forcePowers_t::FP_LEVITATION, SABER_ALT_ATTACK_POWER_FB);

    // see if we have an overridden (or cancelled) kata move
    if (pm->ps->saber[0].jumpAtkFwdMove != saberMoveName_t::LS_INVALID) {
        if (pm->ps->saber[0].jumpAtkFwdMove != saberMoveName_t::LS_NONE) {
            return pm->ps->saber[0].jumpAtkFwdMove;
        }
    }
    if (pm->ps->dualSabers) {
        if (pm->ps->saber[1].jumpAtkFwdMove != saberMoveName_t::LS_INVALID) {
            if (pm->ps->saber[1].jumpAtkFwdMove != saberMoveName_t::LS_NONE) {
                return pm->ps->saber[1].jumpAtkFwdMove;
            }
        }
    }
    // no overrides, cancelled?
    if (pm->ps->saber[0].jumpAtkFwdMove == saberMoveName_t::LS_NONE) {
        return saberMoveName_t::LS_NONE;
    }
    if (pm->ps->dualSabers) {
        if (pm->ps->saber[1].jumpAtkFwdMove == saberMoveName_t::LS_NONE) {
            return saberMoveName_t::LS_NONE;
        }
    }
    if (pm->ps->saberAnimLevel == saber_styles_t::SS_DUAL || pm->ps->saberAnimLevel == saber_styles_t::SS_STAFF) {
        pm->cmd.upmove = 0; // no jump just yet

        if (pm->ps->saberAnimLevel == saber_styles_t::SS_STAFF) {
            if (Q_irand(0, 1)) {
                return saberMoveName_t::LS_JUMPATTACK_STAFF_LEFT;
            }
            else {
                return saberMoveName_t::LS_JUMPATTACK_STAFF_RIGHT;
            }
        }

        return saberMoveName_t::LS_JUMPATTACK_DUAL;
    }
    else {
        vec3_t fwdAngles, jumpFwd;

        VectorCopy(pm->ps->viewangles, fwdAngles);
        fwdAngles[PITCH] = fwdAngles[ROLL] = 0;
        AngleVectors(fwdAngles, jumpFwd, nullptr, nullptr);
        VectorScale(jumpFwd, 200, pm->ps->velocity);
        pm->ps->velocity[2] = 180;
        pm->ps->forceJumpZStart = pm->ps->origin[2]; // so we don't take damage if we land at same height
        pm->ps->pm_flags |= pmoveFlags_t::PMF_JUMPING | pmoveFlags_t::PMF_SLOW_MO_FALL;

        // FIXME: NPCs yell?
        PM_AddEvent(entity_event_t::EV_JUMP);
        G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_BODY, "sound/weapons/force/jump.wav");
        pm->cmd.upmove = 0;

        return saberMoveName_t::LS_A_JUMP_T__B_;
    }
}

bool PM_CheckJumpForwardAttackMove(void)
{
    if (pm->ps->clientNum < MAX_CLIENTS && PM_InSecondaryStyle()) {
        return false;
    }

    // check to see if it's cancelled?
    if (pm->ps->saber[0].jumpAtkFwdMove == saberMoveName_t::LS_NONE) {
        if (pm->ps->dualSabers) {
            if (pm->ps->saber[1].jumpAtkFwdMove == saberMoveName_t::LS_NONE || pm->ps->saber[1].jumpAtkFwdMove == saberMoveName_t::LS_INVALID) {
                return false;
            }
        }
        else {
            return false;
        }
    }
    if (pm->ps->dualSabers) {
        if (pm->ps->saber[1].jumpAtkFwdMove == saberMoveName_t::LS_NONE) {
            if (pm->ps->saber[0].jumpAtkFwdMove == saberMoveName_t::LS_NONE || pm->ps->saber[0].jumpAtkFwdMove == saberMoveName_t::LS_INVALID) {
                return false;
            }
        }
    }
    // do normal checks

    if (pm->cmd.forwardmove > 0                                                                      // going forward
        && pm->ps->forceRageRecoveryTime < pm->cmd.serverTime                                        // not in a force Rage recovery period
        && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1 // can force jump
        && pm->gent &&
        !Any(
            pm->gent->flags &
            gentityFlags_t::FL_LOCK_PLAYER_WEAPONS) // yes this locked weapons check also includes force powers, if we need a separate check later I'll make one
        && (pm->ps->groundEntityNum != ENTITYNUM_NONE || level.time - pm->ps->lastOnGround <= 250) // on ground or just jumped (if not player)
    ) {
        if (pm->ps->saberAnimLevel == saber_styles_t::SS_DUAL || pm->ps->saberAnimLevel == saber_styles_t::SS_STAFF) { // dual and staff
            if (!PM_SaberInTransitionAny(pm->ps->saberMove)             // not going to/from/between an attack anim
                && !PM_SaberInAttack(pm->ps->saberMove)                 // not in attack anim
                && pm->ps->weaponTime <= 0                              // not busy
                && Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)) // want to attack
            {
                if (pm->ps->clientNum >= MAX_CLIENTS && !PM_ControlledByPlayer()) {              // NPC
                    if (pm->cmd.upmove > 0 || Any(pm->ps->pm_flags & pmoveFlags_t::PMF_JUMPING)) // jumping NPC
                    {
                        if (pm->gent && pm->gent->NPC && (pm->gent->NPC->rank == rank_t::RANK_CREWMAN || pm->gent->NPC->rank >= rank_t::RANK_LT)) {
                            return true;
                        }
                    }
                }
                else { // PLAYER
                    if (G_TryingJumpForwardAttack(pm->gent, &pm->cmd) &&
                        G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER_FB)) // have enough power to attack
                    {
                        return true;
                    }
                }
            }
        }
        // check strong
        else if (pm->ps->saberAnimLevel == saber_styles_t::SS_STRONG     // strong style
                 || pm->ps->saberAnimLevel == saber_styles_t::SS_DESANN) // desann
        {
            if ( //&& !PM_InKnockDown( pm->ps )
                !pm->ps->dualSabers
                //&& (pm->ps->legsAnim == animNumber_t::BOTH_STAND2||pm->ps->legsAnim == animNumber_t::BOTH_SABERFAST_STANCE||pm->ps->legsAnim ==
                // animNumber_t::BOTH_SABERSLOW_STANCE||level.time-pm->ps->lastStationary<=500)//standing or just started moving
            ) { // strong attack: jump-hack
                /*
                if ( pm->ps->legsAnim == animNumber_t::BOTH_STAND2
                    || pm->ps->legsAnim == animNumber_t::BOTH_SABERFAST_STANCE
                    || pm->ps->legsAnim == animNumber_t::BOTH_SABERSLOW_STANCE
                    || level.time-pm->ps->lastStationary <= 250 )//standing or just started moving
                */
                if (pm->ps->clientNum >= MAX_CLIENTS && !PM_ControlledByPlayer()) {              // NPC
                    if (pm->cmd.upmove > 0 || Any(pm->ps->pm_flags & pmoveFlags_t::PMF_JUMPING)) // NPC jumping
                    {
                        if (pm->gent && pm->gent->NPC &&
                            (pm->gent->NPC->rank == rank_t::RANK_CREWMAN ||
                             pm->gent->NPC->rank >= rank_t::RANK_LT)) { // only acrobat or boss and higher can do this
                            if (pm->ps->legsAnim == animNumber_t::BOTH_STAND2 || pm->ps->legsAnim == animNumber_t::BOTH_SABERFAST_STANCE ||
                                pm->ps->legsAnim == animNumber_t::BOTH_SABERSLOW_STANCE ||
                                level.time - pm->ps->lastStationary <= 250) { // standing or just started moving
                                if (pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_DESANN) {
                                    if (!Q_irand(0, 1)) {
                                        return true;
                                    }
                                }
                                else {
                                    return true;
                                }
                            }
                        }
                    }
                }
                else { // player
                    if (G_TryingJumpForwardAttack(pm->gent, &pm->cmd) && G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER_FB)) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

saberMoveName_t PM_SaberFlipOverAttackMove(void)
{
    // see if we have an overridden (or cancelled) kata move
    if (pm->ps->saber[0].jumpAtkFwdMove != saberMoveName_t::LS_INVALID) {
        if (pm->ps->saber[0].jumpAtkFwdMove != saberMoveName_t::LS_NONE) {
            return pm->ps->saber[0].jumpAtkFwdMove;
        }
    }
    if (pm->ps->dualSabers) {
        if (pm->ps->saber[1].jumpAtkFwdMove != saberMoveName_t::LS_INVALID) {
            if (pm->ps->saber[1].jumpAtkFwdMove != saberMoveName_t::LS_NONE) {
                return pm->ps->saber[1].jumpAtkFwdMove;
            }
        }
    }
    // no overrides, cancelled?
    if (pm->ps->saber[0].jumpAtkFwdMove == saberMoveName_t::LS_NONE) {
        return saberMoveName_t::LS_NONE;
    }
    if (pm->ps->dualSabers) {
        if (pm->ps->saber[1].jumpAtkFwdMove == saberMoveName_t::LS_NONE) {
            return saberMoveName_t::LS_NONE;
        }
    }
    // FIXME: check above for room enough to jump!
    // FIXME: while in this jump, keep velocity[2] at a minimum until the end of the anim
    vec3_t fwdAngles, jumpFwd;

    VectorCopy(pm->ps->viewangles, fwdAngles);
    fwdAngles[PITCH] = fwdAngles[ROLL] = 0;
    AngleVectors(fwdAngles, jumpFwd, nullptr, nullptr);
    VectorScale(jumpFwd, 150, pm->ps->velocity);
    pm->ps->velocity[2] = 250;
    // 250 is normalized for a standing enemy at your z level, about 64 tall... adjust for actual maxs[2]-mins[2] of enemy and for zdiff in origins
    if (pm->gent && pm->gent->enemy) { // go higher for taller enemies
        pm->ps->velocity[2] *= (pm->gent->enemy->maxs[2] - pm->gent->enemy->mins[2]) / 64.0f;
        // go higher for enemies higher than you, lower for those lower than you
        float zDiff = pm->gent->enemy->currentOrigin[2] - pm->ps->origin[2];
        pm->ps->velocity[2] += (zDiff) * 1.5f;
        // clamp to decent-looking values
        // FIXME: still jump too low sometimes
        if (zDiff <= 0 && pm->ps->velocity[2] < 200) { // if we're on same level, don't let me jump so low, I clip into the ground
            pm->ps->velocity[2] = 200;
        }
        else if (pm->ps->velocity[2] < 50) {
            pm->ps->velocity[2] = 50;
        }
        else if (pm->ps->velocity[2] > 400) {
            pm->ps->velocity[2] = 400;
        }
    }
    pm->ps->forceJumpZStart = pm->ps->origin[2]; // so we don't take damage if we land at same height
    pm->ps->pm_flags |= pmoveFlags_t::PMF_JUMPING | pmoveFlags_t::PMF_SLOW_MO_FALL;

    // FIXME: NPCs yell?
    PM_AddEvent(entity_event_t::EV_JUMP);
    G_SoundOnEnt(pm->gent, soundChannel_t::CHAN_BODY, "sound/weapons/force/jump.wav");
    pm->cmd.upmove = 0;
    // FIXME: don't allow this to land on other people

    pm->gent->angle = pm->ps->viewangles[YAW]; // so we know what yaw we started this at

    G_DrainPowerForSpecialMove(pm->gent, forcePowers_t::FP_LEVITATION, SABER_ALT_ATTACK_POWER_FB);

    if (Q_irand(0, 1)) {
        return saberMoveName_t::LS_A_FLIP_STAB;
    }
    else {
        return saberMoveName_t::LS_A_FLIP_SLASH;
    }
}

bool PM_CheckFlipOverAttackMove(bool checkEnemy)
{
    if (pm->ps->clientNum < MAX_CLIENTS && PM_InSecondaryStyle()) {
        return false;
    }
    // check to see if it's cancelled?
    if (pm->ps->saber[0].jumpAtkFwdMove == saberMoveName_t::LS_NONE) {
        if (pm->ps->dualSabers) {
            if (pm->ps->saber[1].jumpAtkFwdMove == saberMoveName_t::LS_NONE || pm->ps->saber[1].jumpAtkFwdMove == saberMoveName_t::LS_INVALID) {
                return false;
            }
        }
        else {
            return false;
        }
    }
    if (pm->ps->dualSabers) {
        if (pm->ps->saber[1].jumpAtkFwdMove == saberMoveName_t::LS_NONE) {
            if (pm->ps->saber[0].jumpAtkFwdMove == saberMoveName_t::LS_NONE || pm->ps->saber[0].jumpAtkFwdMove == saberMoveName_t::LS_INVALID) {
                return false;
            }
        }
    }
    // do normal checks

    if ((pm->ps->saberAnimLevel == saber_styles_t::SS_MEDIUM                                         // medium
         || pm->ps->saberAnimLevel == saber_styles_t::SS_TAVION)                                     // tavion
        && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1 // can force jump
        &&
        !Any(
            pm->gent->flags &
            gentityFlags_t::FL_LOCK_PLAYER_WEAPONS) // yes this locked weapons check also includes force powers, if we need a separate check later I'll make one
        && (pm->ps->groundEntityNum != ENTITYNUM_NONE || level.time - pm->ps->lastOnGround <= 250) // on ground or just jumped
    ) {
        bool tryMove = false;
        if (pm->ps->clientNum >= MAX_CLIENTS && !PM_ControlledByPlayer()) { // NPC
            if (pm->cmd.upmove > 0                                          // want to jump
                || Any(pm->ps->pm_flags & pmoveFlags_t::PMF_JUMPING))       // jumping
            {                                                               // flip over-forward down-attack
                if ((pm->gent->NPC && (pm->gent->NPC->rank == rank_t::RANK_CREWMAN || pm->gent->NPC->rank >= rank_t::RANK_LT) &&
                     !Q_irand(0, 2))) // NPC who can do this, 33% chance
                {                     // only player or acrobat or boss and higher can do this
                    tryMove = true;
                }
            }
        }
        else { // player
            if (G_TryingJumpForwardAttack(pm->gent, &pm->cmd) &&
                G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER_FB)) // have enough power
            {
                if (!pm->cmd.rightmove) {
                    if (pm->ps->legsAnim == animNumber_t::BOTH_JUMP1 || pm->ps->legsAnim == animNumber_t::BOTH_FORCEJUMP1 ||
                        pm->ps->legsAnim == animNumber_t::BOTH_INAIR1 || pm->ps->legsAnim == animNumber_t::BOTH_FORCEINAIR1) { // in a non-flip forward jump
                        tryMove = true;
                    }
                }
            }
        }

        if (tryMove) {
            if (!checkEnemy) { // based just on command input
                return true;
            }
            else {                   // based on presence of enemy
                if (pm->gent->enemy) // have an enemy
                {
                    vec3_t fwdAngles = {0, pm->ps->viewangles[YAW], 0};
                    if (pm->gent->enemy->health > 0 && pm->ps->forceRageRecoveryTime < pm->cmd.serverTime // not in a force Rage recovery period
                        && pm->gent->enemy->maxs[2] > 12 && (!pm->gent->enemy->client || !PM_InKnockDownOnGround(&pm->gent->enemy->client->ps)) &&
                        DistanceSquared(pm->gent->currentOrigin, pm->gent->enemy->currentOrigin) < 10000 &&
                        InFront(pm->gent->enemy->currentOrigin, pm->gent->currentOrigin, fwdAngles,
                                0.3f)) { // enemy must be alive, not low to ground, close and in front
                        return true;
                    }
                }
                return false;
            }
        }
    }
    return false;
}

saberMoveName_t PM_SaberBackflipAttackMove(void)
{
    // see if we have an overridden (or cancelled) kata move
    if (pm->ps->saber[0].jumpAtkBackMove != saberMoveName_t::LS_INVALID) {
        if (pm->ps->saber[0].jumpAtkBackMove != saberMoveName_t::LS_NONE) {
            return pm->ps->saber[0].jumpAtkBackMove;
        }
    }
    if (pm->ps->dualSabers) {
        if (pm->ps->saber[1].jumpAtkBackMove != saberMoveName_t::LS_INVALID) {
            if (pm->ps->saber[1].jumpAtkBackMove != saberMoveName_t::LS_NONE) {
                return pm->ps->saber[1].jumpAtkBackMove;
            }
        }
    }
    // no overrides, cancelled?
    if (pm->ps->saber[0].jumpAtkBackMove == saberMoveName_t::LS_NONE) {
        return saberMoveName_t::LS_NONE;
    }
    if (pm->ps->dualSabers) {
        if (pm->ps->saber[1].jumpAtkBackMove == saberMoveName_t::LS_NONE) {
            return saberMoveName_t::LS_NONE;
        }
    }
    pm->cmd.upmove = 0; // no jump just yet
    return saberMoveName_t::LS_A_BACKFLIP_ATK;
}

bool PM_CheckBackflipAttackMove(void)
{
    if (pm->ps->clientNum < MAX_CLIENTS && PM_InSecondaryStyle()) {
        return false;
    }

    // check to see if it's cancelled?
    if (pm->ps->saber[0].jumpAtkBackMove == saberMoveName_t::LS_NONE) {
        if (pm->ps->dualSabers) {
            if (pm->ps->saber[1].jumpAtkBackMove == saberMoveName_t::LS_NONE || pm->ps->saber[1].jumpAtkBackMove == saberMoveName_t::LS_INVALID) {
                return false;
            }
        }
        else {
            return false;
        }
    }
    if (pm->ps->dualSabers) {
        if (pm->ps->saber[1].jumpAtkBackMove == saberMoveName_t::LS_NONE) {
            if (pm->ps->saber[0].jumpAtkBackMove == saberMoveName_t::LS_NONE || pm->ps->saber[0].jumpAtkBackMove == saberMoveName_t::LS_INVALID) {
                return false;
            }
        }
    }
    // do normal checks

    if (pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1 // can force jump
        && pm->ps->forceRageRecoveryTime < pm->cmd.serverTime                                     // not in a force Rage recovery period
        && pm->gent &&
        !Any(
            pm->gent->flags &
            gentityFlags_t::FL_LOCK_PLAYER_WEAPONS) // yes this locked weapons check also includes force powers, if we need a separate check later I'll make one
        //&& (pm->ps->legsAnim == animNumber_t::BOTH_SABERSTAFF_STANCE || level.time-pm->ps->lastStationary<=250)//standing or just started moving
        && (pm->ps->groundEntityNum != ENTITYNUM_NONE || level.time - pm->ps->lastOnGround <= 250)) // on ground or just jumped (if not player)
    {
        if (pm->cmd.forwardmove < 0                                                       // moving backwards
            && pm->ps->saberAnimLevel == saber_styles_t::SS_STAFF                         // using staff
            && (pm->cmd.upmove > 0 || Any(pm->ps->pm_flags & pmoveFlags_t::PMF_JUMPING))) // jumping
        {                                                                                 // jumping backwards and using staff
            if (!PM_SaberInTransitionAny(pm->ps->saberMove)                               // not going to/from/between an attack anim
                && !PM_SaberInAttack(pm->ps->saberMove)                                   // not in attack anim
                && pm->ps->weaponTime <= 0                                                // not busy
                && Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK))                   // want to attack
            {                                                                             // not already attacking
                if (pm->ps->clientNum >= MAX_CLIENTS && !PM_ControlledByPlayer()) {       // NPC
                    if (pm->gent && pm->gent->NPC &&
                        (pm->gent->NPC->rank == rank_t::RANK_CREWMAN || pm->gent->NPC->rank >= rank_t::RANK_LT)) { // acrobat or boss and higher can do this
                        return true;
                    }
                }
                else { // player
                    return true;
                }
            }
        }
    }
    return false;
}

saberMoveName_t PM_CheckDualSpinProtect(void)
{
    if (pm->ps->clientNum < MAX_CLIENTS && PM_InSecondaryStyle()) {
        return saberMoveName_t::LS_NONE;
    }

    // see if we have an overridden (or cancelled) kata move
    if (pm->ps->saber[0].kataMove != saberMoveName_t::LS_INVALID) {
        if (pm->ps->saber[0].kataMove != saberMoveName_t::LS_NONE) {
            return pm->ps->saber[0].kataMove;
        }
    }
    if (pm->ps->dualSabers) {
        if (pm->ps->saber[1].kataMove != saberMoveName_t::LS_INVALID) {
            if (pm->ps->saber[1].kataMove != saberMoveName_t::LS_NONE) {
                return pm->ps->saber[1].kataMove;
            }
        }
    }
    // no overrides, cancelled?
    if (pm->ps->saber[0].kataMove == saberMoveName_t::LS_NONE) {
        return saberMoveName_t::LS_NONE;
    }
    if (pm->ps->dualSabers) {
        if (pm->ps->saber[1].kataMove == saberMoveName_t::LS_NONE) {
            return saberMoveName_t::LS_NONE;
        }
    }
    // do normal checks
    if (pm->ps->saberMove == saberMoveName_t::LS_READY       // ready
                                                             //&& (pm->ps->clientNum < MAX_CLIENTS||PM_ControlledByPlayer())//PLAYER ONLY...?
                                                             //&& pm->ps->viewangles[0] > 30 //looking down
        && pm->ps->saberAnimLevel == saber_styles_t::SS_DUAL // using dual saber style
        && pm->ps->saber[0].Active() &&
        pm->ps->saber[1].Active() // both sabers on
        //&& pm->ps->forcePowerLevel[forcePowers_t::FP_PUSH]>=forcePowerLevels_t::FORCE_LEVEL_3//force push 3
        //&& ((pm->ps->forcePowersActive&Bit( forcePowers_t::FP_PUSH ))||pm->ps->forcePowerDebounce[forcePowers_t::FP_PUSH]>level.time)//force-pushing
        && G_TryingKataAttack(pm->gent, &pm->cmd) // Any(pm->cmd.buttons&buttonFlags_t::BUTTON_FORCE_FOCUS)//holding focus
        && G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER,
                                       true)                   // pm->ps->forcePower >= SABER_ALT_ATTACK_POWER//DUAL_SPIN_PROTECT_POWER//force push 3
        && Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK) // pressing attack
    ) {                                                        // FIXME: some NPC logic to do this?
        /*
        if ( (Any( pm->ps->pm_flags&pmoveFlags_t::PMF_DUCKED )||pm->cmd.upmove<0)//crouching
            && g_crosshairEntNum >= ENTITYNUM_WORLD )
        */
        {
            if (pm->gent) {
                G_DrainPowerForSpecialMove(pm->gent, forcePowers_t::FP_PUSH, SABER_ALT_ATTACK_POWER, true); // drain the required force power
            }
            return saberMoveName_t::LS_DUAL_SPIN_PROTECT;
        }
    }
    return saberMoveName_t::LS_NONE;
}

saberMoveName_t PM_CheckStaffKata(void)
{
    if (pm->ps->clientNum < MAX_CLIENTS && PM_InSecondaryStyle()) {
        return saberMoveName_t::LS_NONE;
    }

    // see if we have an overridden (or cancelled) kata move
    if (pm->ps->saber[0].kataMove != saberMoveName_t::LS_INVALID) {
        if (pm->ps->saber[0].kataMove != saberMoveName_t::LS_NONE) {
            return pm->ps->saber[0].kataMove;
        }
    }
    if (pm->ps->dualSabers) {
        if (pm->ps->saber[1].kataMove != saberMoveName_t::LS_INVALID) {
            if (pm->ps->saber[1].kataMove != saberMoveName_t::LS_NONE) {
                return pm->ps->saber[1].kataMove;
            }
        }
    }
    // no overrides, cancelled?
    if (pm->ps->saber[0].kataMove == saberMoveName_t::LS_NONE) {
        return saberMoveName_t::LS_NONE;
    }
    if (pm->ps->dualSabers) {
        if (pm->ps->saber[1].kataMove == saberMoveName_t::LS_NONE) {
            return saberMoveName_t::LS_NONE;
        }
    }
    // do normal checks
    if (pm->ps->saberMove == saberMoveName_t::LS_READY        // ready
                                                              //&& (pm->ps->clientNum < MAX_CLIENTS||PM_ControlledByPlayer())//PLAYER ONLY...?
                                                              //&& pm->ps->viewangles[0] > 30 //looking down
        && pm->ps->saberAnimLevel == saber_styles_t::SS_STAFF // using dual saber style
        && pm->ps->saber[0].Active()                          // saber on
        //&& pm->ps->forcePowerLevel[forcePowers_t::FP_PUSH]>=forcePowerLevels_t::FORCE_LEVEL_3//force push 3
        //&& ((pm->ps->forcePowersActive&Bit( forcePowers_t::FP_PUSH ))||pm->ps->forcePowerDebounce[forcePowers_t::FP_PUSH]>level.time)//force-pushing
        && G_TryingKataAttack(pm->gent, &pm->cmd) // Any(pm->cmd.buttons&buttonFlags_t::BUTTON_FORCE_FOCUS)//holding focus
        && G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER,
                                       true)                   // pm->ps->forcePower >= SABER_ALT_ATTACK_POWER//DUAL_SPIN_PROTECT_POWER//force push 3
        && Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK) // pressing attack
    ) {                                                        // FIXME: some NPC logic to do this?
        /*
        if ( (Any( pm->ps->pm_flags&pmoveFlags_t::PMF_DUCKED )||pm->cmd.upmove<0)//crouching
            && g_crosshairEntNum >= ENTITYNUM_WORLD )
        */
        {
            if (pm->gent) {
                G_DrainPowerForSpecialMove(pm->gent, forcePowers_t::FP_LEVITATION, SABER_ALT_ATTACK_POWER, true); // drain the required force power
            }
            return saberMoveName_t::LS_STAFF_SOULCAL;
        }
    }
    return saberMoveName_t::LS_NONE;
}

extern bool WP_ForceThrowable(gentity_t* ent, gentity_t* forwardEnt, gentity_t* self, bool pull, float cone, float radius, vec3_t forward);
saberMoveName_t PM_CheckPullAttack(void)
{
    if (pm->ps->clientNum < MAX_CLIENTS && PM_InSecondaryStyle()) {
        return saberMoveName_t::LS_NONE;
    }

    if (Any(pm->ps->saber[0].saberFlags & saberFlags_t::SFL_NO_PULL_ATTACK)) {
        return saberMoveName_t::LS_NONE;
    }
    if (pm->ps->dualSabers && Any(pm->ps->saber[1].saberFlags & saberFlags_t::SFL_NO_PULL_ATTACK)) {
        return saberMoveName_t::LS_NONE;
    }

    if ((pm->ps->saberMove == saberMoveName_t::LS_READY || PM_SaberInReturn(pm->ps->saberMove) ||
         PM_SaberInReflect(pm->ps->saberMove))                 // ready
                                                               //&& (pm->ps->clientNum < MAX_CLIENTS||PM_ControlledByPlayer())//PLAYER ONLY
        && pm->ps->saberAnimLevel >= saber_styles_t::SS_FAST   // single saber styles - FIXME: Tavion?
        && pm->ps->saberAnimLevel <= saber_styles_t::SS_STRONG // single saber styles - FIXME: Tavion?
        && G_TryingPullAttack(pm->gent, &pm->cmd, false)       // Any(pm->cmd.buttons&buttonFlags_t::BUTTON_FORCE_FOCUS)//holding focus
        //&& pm->cmd.forwardmove<0//pulling back
        && Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)                        // attacking
        && G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER_FB) // pm->ps->forcePower >= SABER_ALT_ATTACK_POWER_FB//have enough power
    ) {                                                                               // FIXME: some NPC logic to do this?
        bool doMove = g_saberNewControlScheme->integer ? true : false; // in new control scheme, can always do this, even if there's no-one to do it to
        if (g_saberNewControlScheme->integer || g_crosshairEntNum < ENTITYNUM_WORLD) // in old control scheme, there has to be someone there
        {
            saberMoveName_t pullAttackMove = saberMoveName_t::LS_NONE;
            if (pm->ps->saberAnimLevel == saber_styles_t::SS_FAST) {
                pullAttackMove = saberMoveName_t::LS_PULL_ATTACK_STAB;
            }
            else {
                pullAttackMove = saberMoveName_t::LS_PULL_ATTACK_SWING;
            }

            if (g_crosshairEntNum < ENTITYNUM_WORLD && pm->gent && pm->gent->client) {
                gentity_t* targEnt = &g_entities[g_crosshairEntNum];
                if (targEnt->client &&
                    targEnt->health > 0
                    // FIXME: check other things like in knockdown, saberlock, uninterruptable anims, etc.
                    && !PM_InOnGroundAnim(&targEnt->client->ps) && !PM_LockedAnim(targEnt->client->ps.legsAnim) &&
                    !PM_SuperBreakLoseAnim(targEnt->client->ps.legsAnim) && !PM_SuperBreakWinAnim(targEnt->client->ps.legsAnim) &&
                    targEnt->client->ps.saberLockTime <= 0 && WP_ForceThrowable(targEnt, targEnt, pm->gent, true, 1.0f, 0.0f, nullptr)) {
                    if (!g_saberNewControlScheme->integer) { // in old control scheme, make sure they're close or far enough away for the move we'll be doing
                        float targDist = Distance(targEnt->currentOrigin, pm->ps->origin);
                        if (pullAttackMove == saberMoveName_t::LS_PULL_ATTACK_STAB) { // must be closer than 512
                            if (targDist > 384.0f) {
                                return saberMoveName_t::LS_NONE;
                            }
                        }
                        else // if ( pullAttackMove == saberMoveName_t::LS_PULL_ATTACK_SWING )
                        {    // must be farther than 256
                            if (targDist > 512.0f) {
                                return saberMoveName_t::LS_NONE;
                            }
                            if (targDist < 192.0f) {
                                return saberMoveName_t::LS_NONE;
                            }
                        }
                    }

                    vec3_t targAngles = {0, targEnt->client->ps.viewangles[YAW], 0};
                    if (InFront(pm->ps->origin, targEnt->currentOrigin, targAngles)) {
                        NPC_SetAnim(targEnt, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_PULLED_INAIR_F,
                                    setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                    }
                    else {
                        NPC_SetAnim(targEnt, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_PULLED_INAIR_B,
                                    setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                    }
                    // hold the anim until I'm with done pull anim
                    targEnt->client->ps.legsAnimTimer = targEnt->client->ps.torsoAnimTimer =
                        PM_AnimLength(pm->gent->client->clientInfo.animFileIndex, saberMoveData[pullAttackMove].animToUse);
                    // set pullAttackTime
                    pm->gent->client->ps.pullAttackTime = targEnt->client->ps.pullAttackTime = level.time + targEnt->client->ps.legsAnimTimer;
                    // make us know about each other
                    pm->gent->client->ps.pullAttackEntNum = g_crosshairEntNum;
                    targEnt->client->ps.pullAttackEntNum = pm->ps->clientNum;
                    // do effect and sound on me
                    pm->ps->powerups[powerup_t::PW_FORCE_PUSH] = level.time + 1000;
                    if (pm->gent) {
                        G_Sound(pm->gent, G_SoundIndex("sound/weapons/force/pull.wav"));
                    }
                    doMove = true;
                }
            }
            if (doMove) {
                if (pm->gent) {
                    G_DrainPowerForSpecialMove(pm->gent, forcePowers_t::FP_PULL, SABER_ALT_ATTACK_POWER_FB);
                }
                return pullAttackMove;
            }
        }
    }
    return saberMoveName_t::LS_NONE;
}

saberMoveName_t PM_CheckPlayerAttackFromParry(saberMoveName_t curmove)
{
    if (pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) {
        if (curmove >= saberMoveName_t::LS_PARRY_UP && curmove <= saberMoveName_t::LS_REFLECT_LL) { // in a parry
            switch (saberMoveData[curmove].endQuad) {
            case saberQuadrant_t::Q_T:
                return saberMoveName_t::LS_A_T2B;
                break;
            case saberQuadrant_t::Q_TR:
                return saberMoveName_t::LS_A_TR2BL;
                break;
            case saberQuadrant_t::Q_TL:
                return saberMoveName_t::LS_A_TL2BR;
                break;
            case saberQuadrant_t::Q_BR:
                return saberMoveName_t::LS_A_BR2TL;
                break;
            case saberQuadrant_t::Q_BL:
                return saberMoveName_t::LS_A_BL2TR;
                break;
            // shouldn't be a parry that ends at L, R or B
            default:
                break;
            }
        }
    }
    return saberMoveName_t::LS_NONE;
}

saberMoveName_t PM_SaberAttackForMovement(int forwardmove, int rightmove, saberMoveName_t curmove)
{
    bool noSpecials = false;

    if (pm->ps->clientNum < MAX_CLIENTS && PM_InSecondaryStyle()) {
        noSpecials = true;
    }

    saberMoveName_t overrideJumpRightAttackMove = saberMoveName_t::LS_INVALID;
    if (pm->ps->saber[0].jumpAtkRightMove != saberMoveName_t::LS_INVALID) {
        if (pm->ps->saber[0].jumpAtkRightMove != saberMoveName_t::LS_NONE) { // actually overriding
            overrideJumpRightAttackMove = pm->ps->saber[0].jumpAtkRightMove;
        }
        else if (pm->ps->dualSabers && pm->ps->saber[1].jumpAtkRightMove > saberMoveName_t::LS_NONE) { // would be cancelling it, but check the second saber,
                                                                                                       // too
            overrideJumpRightAttackMove = pm->ps->saber[1].jumpAtkRightMove;
        }
        else { // nope, just cancel it
            overrideJumpRightAttackMove = saberMoveName_t::LS_NONE;
        }
    }
    else if (pm->ps->dualSabers && pm->ps->saber[1].jumpAtkRightMove != saberMoveName_t::LS_INVALID) { // first saber not overridden, check second
        overrideJumpRightAttackMove = pm->ps->saber[1].jumpAtkRightMove;
    }

    saberMoveName_t overrideJumpLeftAttackMove = saberMoveName_t::LS_INVALID;
    if (pm->ps->saber[0].jumpAtkLeftMove != saberMoveName_t::LS_INVALID) {
        if (pm->ps->saber[0].jumpAtkLeftMove != saberMoveName_t::LS_NONE) { // actually overriding
            overrideJumpLeftAttackMove = pm->ps->saber[0].jumpAtkLeftMove;
        }
        else if (pm->ps->dualSabers && pm->ps->saber[1].jumpAtkLeftMove > saberMoveName_t::LS_NONE) { // would be cancelling it, but check the second saber, too
            overrideJumpLeftAttackMove = pm->ps->saber[1].jumpAtkLeftMove;
        }
        else { // nope, just cancel it
            overrideJumpLeftAttackMove = saberMoveName_t::LS_NONE;
        }
    }
    else if (pm->ps->dualSabers && pm->ps->saber[1].jumpAtkLeftMove != saberMoveName_t::LS_INVALID) { // first saber not overridden, check second
        overrideJumpLeftAttackMove = pm->ps->saber[1].jumpAtkLeftMove;
    }
    if (rightmove > 0) { // moving right
        if (!noSpecials && overrideJumpRightAttackMove != saberMoveName_t::LS_NONE &&
            (pm->ps->groundEntityNum != ENTITYNUM_NONE || level.time - pm->ps->lastOnGround <= 250)      // on ground or just jumped
            && Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)                                       // hitting attack
            && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_0 // have force jump 1 at least
            && G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER_LR) // pm->ps->forcePower >= SABER_ALT_ATTACK_POWER_LR//have enough power
            && (((pm->ps->clientNum >= MAX_CLIENTS && !PM_ControlledByPlayer()) && pm->cmd.upmove > 0) // jumping NPC
                || ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) &&
                    G_TryingCartwheel(pm->gent, &pm->cmd) /*Any(pm->cmd.buttons&buttonFlags_t::BUTTON_FORCE_FOCUS)*/))) // focus-holding player
        {                                                                                                               // cartwheel right
            vec3_t right, fwdAngles = {0, pm->ps->viewangles[YAW], 0};
            if (pm->gent) {
                G_DrainPowerForSpecialMove(pm->gent, forcePowers_t::FP_LEVITATION, SABER_ALT_ATTACK_POWER_LR);
            }
            pm->cmd.upmove = 0;
            if (overrideJumpRightAttackMove != saberMoveName_t::LS_INVALID) { // overridden with another move
                return overrideJumpRightAttackMove;
            }
            else if (pm->ps->saberAnimLevel == saber_styles_t::SS_STAFF) {
                AngleVectors(fwdAngles, nullptr, right, nullptr);
                pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
                VectorMA(pm->ps->velocity, 190, right, pm->ps->velocity);
                return saberMoveName_t::LS_BUTTERFLY_RIGHT;
            }
            else {
                if (!Any(pm->ps->saber[0].saberFlags & saberFlags_t::SFL_NO_CARTWHEELS) &&
                    (!pm->ps->dualSabers || !Any(pm->ps->saber[1].saberFlags & saberFlags_t::SFL_NO_CARTWHEELS))) { // okay to do cartwheels with this saber
                    /*
                    if ( pm->ps->groundEntityNum != ENTITYNUM_NONE )
                    {//still on ground
                        VectorClear( pm->ps->velocity );
                        return saberMoveName_t::LS_JUMPATTACK_CART_RIGHT;
                    }
                    else
                    */
                    { // in air
                        AngleVectors(fwdAngles, nullptr, right, nullptr);
                        pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
                        VectorMA(pm->ps->velocity, 190, right, pm->ps->velocity);
                        PM_SetJumped(JUMP_VELOCITY, true);
                        return saberMoveName_t::LS_JUMPATTACK_ARIAL_RIGHT;
                    }
                }
            }
        }
        else if (pm->ps->legsAnim != animNumber_t::BOTH_CARTWHEEL_RIGHT && pm->ps->legsAnim != animNumber_t::BOTH_ARIAL_RIGHT) { // not in a cartwheel/arial
            if (pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer())                                                      // PLAYER ONLY
            {                                                                                                                    // player
                if (G_TryingSpecial(pm->gent, &pm->cmd) /*Any(pm->cmd.buttons&buttonFlags_t::BUTTON_FORCE_FOCUS)*/)              // Holding focus
                { // if no special worked, do nothing
                    return saberMoveName_t::LS_NONE;
                }
            }
            // checked all special attacks, if we're in a parry, attack from that move
            saberMoveName_t parryAttackMove = PM_CheckPlayerAttackFromParry(curmove);
            if (parryAttackMove != saberMoveName_t::LS_NONE) {
                return parryAttackMove;
            }
            // check regular attacks
            if (forwardmove > 0) { // forward right = TL2BR slash
                return saberMoveName_t::LS_A_TL2BR;
            }
            else if (forwardmove < 0) { // backward right = BL2TR uppercut
                return saberMoveName_t::LS_A_BL2TR;
            }
            else { // just right is a left slice
                return saberMoveName_t::LS_A_L2R;
            }
        }
    }
    else if (rightmove < 0) { // moving left
        if (!noSpecials && overrideJumpLeftAttackMove != saberMoveName_t::LS_NONE &&
            (pm->ps->groundEntityNum != ENTITYNUM_NONE || level.time - pm->ps->lastOnGround <= 250)      // on ground or just jumped
            && Any(pm->cmd.buttons & buttonFlags_t::BUTTON_ATTACK)                                       // hitting attack
            && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_0 // have force jump 1 at least
            && G_EnoughPowerForSpecialMove(pm->ps->forcePower, SABER_ALT_ATTACK_POWER_LR) // pm->ps->forcePower >= SABER_ALT_ATTACK_POWER_LR//have enough power
            && (((pm->ps->clientNum >= MAX_CLIENTS && !PM_ControlledByPlayer()) && pm->cmd.upmove > 0) // jumping NPC
                || ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) &&
                    G_TryingCartwheel(pm->gent, &pm->cmd) /*Any(pm->cmd.buttons&buttonFlags_t::BUTTON_FORCE_FOCUS)*/))) // focus-holding player
        {                                                                                                               // cartwheel left
            vec3_t right, fwdAngles = {0, pm->ps->viewangles[YAW], 0};
            if (pm->gent) {
                G_DrainPowerForSpecialMove(pm->gent, forcePowers_t::FP_LEVITATION, SABER_ALT_ATTACK_POWER_LR);
            }
            pm->cmd.upmove = 0;
            if (overrideJumpRightAttackMove != saberMoveName_t::LS_INVALID) { // overridden with another move
                return overrideJumpRightAttackMove;
            }
            else if (pm->ps->saberAnimLevel == saber_styles_t::SS_STAFF) {
                AngleVectors(fwdAngles, nullptr, right, nullptr);
                pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
                VectorMA(pm->ps->velocity, -190, right, pm->ps->velocity);
                return saberMoveName_t::LS_BUTTERFLY_LEFT;
            }
            else {
                if (!Any(pm->ps->saber[0].saberFlags & saberFlags_t::SFL_NO_CARTWHEELS) &&
                    (!pm->ps->dualSabers || !Any(pm->ps->saber[1].saberFlags & saberFlags_t::SFL_NO_CARTWHEELS))) { // okay to do cartwheels with this saber
                    /*
                    if ( pm->ps->groundEntityNum != ENTITYNUM_NONE )
                    {//still on ground
                        VectorClear( pm->ps->velocity );
                        return saberMoveName_t::LS_JUMPATTACK_ARIAL_LEFT;
                    }
                    else
                    */
                    {
                        AngleVectors(fwdAngles, nullptr, right, nullptr);
                        pm->ps->velocity[0] = pm->ps->velocity[1] = 0;
                        VectorMA(pm->ps->velocity, -190, right, pm->ps->velocity);
                        PM_SetJumped(JUMP_VELOCITY, true);
                        return saberMoveName_t::LS_JUMPATTACK_CART_LEFT;
                    }
                }
            }
        }
        else if (pm->ps->legsAnim != animNumber_t::BOTH_CARTWHEEL_LEFT && pm->ps->legsAnim != animNumber_t::BOTH_ARIAL_LEFT) { // not in a left cartwheel/arial
            if (pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer())                                                    // PLAYER ONLY
            {                                                                                                                  // player
                if (G_TryingSpecial(pm->gent, &pm->cmd) /*Any(pm->cmd.buttons&buttonFlags_t::BUTTON_FORCE_FOCUS)*/)            // Holding focus
                { // if no special worked, do nothing
                    return saberMoveName_t::LS_NONE;
                }
            }
            // checked all special attacks, if we're in a parry, attack from that move
            saberMoveName_t parryAttackMove = PM_CheckPlayerAttackFromParry(curmove);
            if (parryAttackMove != saberMoveName_t::LS_NONE) {
                return parryAttackMove;
            }
            // check regular attacks
            if (forwardmove > 0) { // forward left = TR2BL slash
                return saberMoveName_t::LS_A_TR2BL;
            }
            else if (forwardmove < 0) { // backward left = BR2TL uppercut
                return saberMoveName_t::LS_A_BR2TL;
            }
            else { // just left is a right slice
                return saberMoveName_t::LS_A_R2L;
            }
        }
    }
    else {                     // not moving left or right
        if (forwardmove > 0) { // forward= T2B slash
            saberMoveName_t stabDownMove = noSpecials ? saberMoveName_t::LS_NONE : PM_CheckStabDown();
            if (stabDownMove != saberMoveName_t::LS_NONE) {
                return stabDownMove;
            }
            if (((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) && cg.renderingThirdPerson &&
                 !cg.zoomMode)) // player in third person, not zoomed in
            {                   // player in thirdperson, not zoomed in
                // flip-over attack logic
                if (!noSpecials && PM_CheckFlipOverAttackMove(false)) { // flip over-forward down-attack
                    return PM_SaberFlipOverAttackMove();
                }
                // lunge attack logic
                else if (PM_CheckLungeAttackMove()) {
                    return PM_SaberLungeAttackMove(true);
                }
                // jump forward attack logic
                else if (!noSpecials && PM_CheckJumpForwardAttackMove()) {
                    return PM_SaberJumpForwardAttackMove();
                }
            }

            // player NPC with enemy: autoMove logic
            if (pm->gent && pm->gent->enemy && pm->gent->enemy->client) {         // I have an active enemy
                if (pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) { // a player who is running at an enemy
                    // if the enemy is not a jedi, don't use top-down, pick a diagonal or side attack
                    if (pm->gent->enemy->s.weapon != weapon_t::WP_SABER &&
                        pm->gent->enemy->client->NPC_class != class_t::CLASS_REMOTE    // too small to do auto-aiming accurately
                        && pm->gent->enemy->client->NPC_class != class_t::CLASS_SEEKER // too small to do auto-aiming accurately
                        && pm->gent->enemy->client->NPC_class != class_t::CLASS_GONK   // too short to do auto-aiming accurately
                        && pm->gent->enemy->client->NPC_class != class_t::CLASS_HOWLER // too short to do auto-aiming accurately
                        && g_saberAutoAim->integer) {
                        saberMoveName_t autoMove = PM_AttackForEnemyPos(false, (pm->ps->clientNum >= MAX_CLIENTS && !PM_ControlledByPlayer()));
                        if (autoMove != saberMoveName_t::LS_INVALID) {
                            return autoMove;
                        }
                    }
                }

                if (pm->ps->clientNum >= MAX_CLIENTS && !PM_ControlledByPlayer()) // NPC ONLY
                {                                                                 // NPC
                    if (PM_CheckFlipOverAttackMove(true)) {
                        return PM_SaberFlipOverAttackMove();
                    }
                }
            }

            // Regular NPCs
            if (pm->ps->clientNum >= MAX_CLIENTS && !PM_ControlledByPlayer()) // NPC ONLY
            {                                                                 // NPC or player in third person, not zoomed in
                // fwd jump attack logic
                if (PM_CheckJumpForwardAttackMove()) {
                    return PM_SaberJumpForwardAttackMove();
                }
                // lunge attack logic
                else if (PM_CheckLungeAttackMove()) {
                    return PM_SaberLungeAttackMove(true);
                }
            }

            if (pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) // PLAYER ONLY
            {                                                               // player
                if (G_TryingSpecial(pm->gent, &pm->cmd))                    // Any(pm->cmd.buttons&buttonFlags_t::BUTTON_FORCE_FOCUS) )//Holding focus
                {                                                           // if no special worked, do nothing
                    return saberMoveName_t::LS_NONE;
                }
            }

            // checked all special attacks, if we're in a parry, attack from that move
            saberMoveName_t parryAttackMove = PM_CheckPlayerAttackFromParry(curmove);
            if (parryAttackMove != saberMoveName_t::LS_NONE) {
                return parryAttackMove;
            }
            // check regular attacks
            return saberMoveName_t::LS_A_T2B;
        }
        else if (forwardmove < 0) { // backward= T2B slash//B2T uppercut?
            if (g_saberNewControlScheme->integer) {
                saberMoveName_t pullAtk = PM_CheckPullAttack();
                if (pullAtk != saberMoveName_t::LS_NONE) {
                    return pullAtk;
                }
            }

            if (g_saberNewControlScheme->integer && (pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) // PLAYER ONLY
                && Any(pm->cmd.buttons & buttonFlags_t::BUTTON_FORCE_FOCUS)) // Holding focus, trying special backwards attacks
            {                                                                // player lunge attack logic
                if ((pm->ps->dualSabers                                      // or dual
                     || pm->ps->saberAnimLevel == saber_styles_t::SS_STAFF)  // pm->ps->SaberStaff() )//or staff
                    && G_EnoughPowerForSpecialMove(
                           pm->ps->forcePower,
                           SABER_ALT_ATTACK_POWER_FB) /*pm->ps->forcePower >= SABER_ALT_ATTACK_POWER_FB*/) // have enough force power to pull it off
                {                                                                                          // alt+back+attack using fast, dual or staff attacks
                    PM_SaberLungeAttackMove(false);
                }
            }
            else if ((pm->ps->clientNum && !PM_ControlledByPlayer()) // NPC
                     || ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) && cg.renderingThirdPerson &&
                         !cg.zoomMode)) // player in third person, not zooomed
            {                           // NPC or player in third person, not zoomed
                if (PM_CheckBackflipAttackMove()) {
                    return PM_SaberBackflipAttackMove(); // backflip attack
                }
                if (pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) // PLAYER ONLY
                {                                                               // player
                    if (G_TryingSpecial(pm->gent, &pm->cmd))                    // Any(pm->cmd.buttons&buttonFlags_t::BUTTON_FORCE_FOCUS) )//Holding focus
                    {                                                           // if no special worked, do nothing
                        return saberMoveName_t::LS_NONE;
                    }
                }
                // if ( !PM_InKnockDown( pm->ps ) )
                // check backstabs
                if (!Any(pm->ps->saber[0].saberFlags & saberFlags_t::SFL_NO_BACK_ATTACK) &&
                    (!pm->ps->dualSabers || !Any(pm->ps->saber[1].saberFlags & saberFlags_t::SFL_NO_BACK_ATTACK))) { // okay to do backstabs with this saber
                    if (pm->ps->groundEntityNum != ENTITYNUM_NONE) {                                                 // only when on ground
                        if (pm->gent && pm->gent->enemy) { // FIXME: or just trace for a valid enemy standing behind me?  And no enemy in front?
                            vec3_t enemyDir, faceFwd, facingAngles = {0, pm->ps->viewangles[YAW], 0};
                            AngleVectors(facingAngles, faceFwd, nullptr, nullptr);
                            VectorSubtract(pm->gent->enemy->currentOrigin, pm->ps->origin, enemyDir);
                            float dot = DotProduct(enemyDir, faceFwd);
                            if (dot < 0) {                                                                                           // enemy is behind me
                                if (dot < -0.75f && DistanceSquared(pm->gent->currentOrigin, pm->gent->enemy->currentOrigin) < 16384 // 128 squared
                                    && (pm->ps->saberAnimLevel == saber_styles_t::SS_FAST || pm->ps->saberAnimLevel == saber_styles_t::SS_STAFF ||
                                        (pm->gent->client &&
                                         (pm->gent->client->NPC_class == class_t::CLASS_TAVION || pm->gent->client->NPC_class == class_t::CLASS_ALORA) &&
                                         Q_irand(0, 1)))) {                                                         // fast attacks and Tavion
                                    if (!Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED) && pm->cmd.upmove >= 0) { // can't do it while ducked?
                                        if ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) ||
                                            (pm->gent->NPC && pm->gent->NPC->rank >= rank_t::RANK_LT_JG)) { // only fencers and above can do this
                                            return saberMoveName_t::LS_A_BACKSTAB;
                                        }
                                    }
                                }
                                else if (pm->ps->saberAnimLevel != saber_styles_t::SS_FAST &&
                                         pm->ps->saberAnimLevel != saber_styles_t::SS_STAFF) { // medium and higher attacks
                                    if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED) || pm->cmd.upmove < 0) {
                                        return saberMoveName_t::LS_A_BACK_CR;
                                    }
                                    else {
                                        return saberMoveName_t::LS_A_BACK;
                                    }
                                }
                            }
                            else { // enemy in front
                                float enemyDistSq = DistanceSquared(pm->gent->currentOrigin, pm->gent->enemy->currentOrigin);
                                if (((pm->ps->saberAnimLevel == saber_styles_t::SS_FAST || pm->ps->saberAnimLevel == saber_styles_t::SS_STAFF ||
                                      pm->gent->client->NPC_class == class_t::CLASS_TAVION || pm->gent->client->NPC_class == class_t::CLASS_ALORA ||
                                      (pm->gent->client->NPC_class == class_t::CLASS_DESANN && !Q_irand(0, 3))) &&
                                     enemyDistSq > 16384) ||
                                    pm->gent->enemy->health <= 0) // 128 squared
                                {                                 // my enemy is pretty far in front of me and I'm using fast attacks
                                    if ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) ||
                                        (pm->gent && pm->gent->client && pm->gent->NPC && pm->gent->NPC->rank >= rank_t::RANK_LT_JG &&
                                         Q_irand(0, to_underlying(pm->gent->NPC->rank)) >
                                             to_underlying(rank_t::RANK_ENSIGN))) { // only fencers and higher can do this, higher rank does it more
                                        if (PM_CheckEnemyInBack(128)) {
                                            return PM_PickBackStab();
                                        }
                                    }
                                }
                                else if (((pm->ps->saberAnimLevel >= saber_styles_t::SS_MEDIUM || pm->gent->client->NPC_class == class_t::CLASS_DESANN) &&
                                          enemyDistSq > 40000) ||
                                         pm->gent->enemy->health <= 0) // 200 squared
                                {                                      // enemy is very faw away and I'm using medium/strong attacks
                                    if ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) ||
                                        (pm->gent && pm->gent->client && pm->gent->NPC && pm->gent->NPC->rank >= rank_t::RANK_LT_JG &&
                                         Q_irand(0, to_underlying(pm->gent->NPC->rank)) >
                                             to_underlying(rank_t::RANK_ENSIGN))) { // only fencers and higher can do this, higher rank does it more
                                        if (PM_CheckEnemyInBack(164)) {
                                            return PM_PickBackStab();
                                        }
                                    }
                                }
                            }
                        }
                        else {                                                                                                  // no current enemy
                            if ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) && pm->gent && pm->gent->client) { // only player
                                if (PM_CheckEnemyInBack(128)) {
                                    return PM_PickBackStab();
                                }
                            }
                        }
                    }
                }
            }

            if (pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) // PLAYER ONLY
            {                                                               // player
                if (G_TryingSpecial(pm->gent, &pm->cmd))                    // Any(pm->cmd.buttons&buttonFlags_t::BUTTON_FORCE_FOCUS) )//Holding focus
                {                                                           // if no special worked, do nothing
                    return saberMoveName_t::LS_NONE;
                }
            }

            // checked all special attacks, if we're in a parry, attack from that move
            saberMoveName_t parryAttackMove = PM_CheckPlayerAttackFromParry(curmove);
            if (parryAttackMove != saberMoveName_t::LS_NONE) {
                return parryAttackMove;
            }
            // check regular attacks
            // else just swing down
            return saberMoveName_t::LS_A_T2B;
        }
        else {                               // not moving in any direction
            if (PM_SaberInBounce(curmove)) { // bounces should go to their default attack if you don't specify a direction but are attacking
                if (pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) // PLAYER ONLY
                {                                                               // player
                    if (G_TryingSpecial(pm->gent, &pm->cmd))                    // Any(pm->cmd.buttons&buttonFlags_t::BUTTON_FORCE_FOCUS) )//Holding focus
                    {                                                           // if no special worked, do nothing
                        return saberMoveName_t::LS_NONE;
                    }
                }
                saberMoveName_t newmove;
                if (pm->ps->clientNum && !PM_ControlledByPlayer() && Q_irand(0, 3)) { // use NPC random
                    newmove = PM_NPCSaberAttackFromQuad(saberMoveData[curmove].endQuad);
                }
                else { // player uses chain-attack
                    newmove = saberMoveData[curmove].chain_attack;
                }
                if (PM_SaberKataDone(curmove, newmove)) {
                    return saberMoveData[curmove].chain_idle;
                }
                else {
                    return newmove;
                }
            }
            else if (PM_SaberInKnockaway(curmove)) { // bounces should go to their default attack if you don't specify a direction but are attacking
                if (pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) // PLAYER ONLY
                {                                                               // player
                    if (G_TryingSpecial(pm->gent, &pm->cmd))                    // Any(pm->cmd.buttons&buttonFlags_t::BUTTON_FORCE_FOCUS) )//Holding focus
                    {                                                           // if no special worked, do nothing
                        return saberMoveName_t::LS_NONE;
                    }
                }
                saberMoveName_t newmove;
                if (pm->ps->clientNum && !PM_ControlledByPlayer() && Q_irand(0, 3)) { // use NPC random
                    newmove = PM_NPCSaberAttackFromQuad(saberMoveData[curmove].endQuad);
                }
                else {
                    if (pm->ps->saberAnimLevel == saber_styles_t::SS_FAST ||
                        pm->ps->saberAnimLevel == saber_styles_t::SS_TAVION) { // player is in fast attacks, so come right back down from the same spot
                        newmove = PM_AttackMoveForQuad(saberMoveData[curmove].endQuad);
                    }
                    else { // use a transition to wrap to another attack from a different dir
                        newmove = saberMoveData[curmove].chain_attack;
                    }
                }
                if (PM_SaberKataDone(curmove, newmove)) {
                    return saberMoveData[curmove].chain_idle;
                }
                else {
                    return newmove;
                }
            }
            else if (curmove == saberMoveName_t::LS_READY || curmove == saberMoveName_t::LS_A_FLIP_STAB || curmove == saberMoveName_t::LS_A_FLIP_SLASH ||
                     (curmove >= saberMoveName_t::LS_PARRY_UP && curmove <= saberMoveName_t::LS_REFLECT_LL)) { // Not moving at all, not too busy to attack
                // push + lookdown + attack + dual sabers = LS_DUAL_SPIN_PROTECT
                if (g_saberNewControlScheme->integer) {
                    if (PM_CheckDualSpinProtect() != saberMoveName_t::LS_NONE) {
                        return saberMoveName_t::LS_DUAL_SPIN_PROTECT;
                    }
                    if (PM_CheckStaffKata() != saberMoveName_t::LS_NONE) {
                        return saberMoveName_t::LS_STAFF_SOULCAL;
                    }
                }
                if (pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) // PLAYER ONLY
                {                                                               // player
                    if (G_TryingSpecial(pm->gent, &pm->cmd))                    // Any(pm->cmd.buttons&buttonFlags_t::BUTTON_FORCE_FOCUS) )//Holding focus
                    {                                                           // if no special worked, do nothing
                        return saberMoveName_t::LS_NONE;
                    }
                }
                // checked all special attacks, if we're in a parry, attack from that move
                saberMoveName_t parryAttackMove = PM_CheckPlayerAttackFromParry(curmove);
                if (parryAttackMove != saberMoveName_t::LS_NONE) {
                    return parryAttackMove;
                }
                // check regular attacks
                if (pm->ps->clientNum || g_saberAutoAim->integer) { // auto-aim
                    if (pm->gent && pm->gent->enemy) {              // based on enemy position, pick a proper attack
                        saberMoveName_t autoMove = PM_AttackForEnemyPos(true, (pm->ps->clientNum >= MAX_CLIENTS));
                        if (autoMove != saberMoveName_t::LS_INVALID) {
                            return autoMove;
                        }
                    }
                    else if (Q_fabsf(pm->ps->viewangles[0]) >
                             30) { // looking far up or far down uses the top to bottom attack, presuming you want a vertical attack
                        return saberMoveName_t::LS_A_T2B;
                    }
                }
                else { // for now, just pick a random attack
                    return Q_irand(saberMoveName_t::LS_A_TL2BR, saberMoveName_t::LS_A_T2B);
                }
            }
        }
    }
    // FIXME: pick a return?
    return saberMoveName_t::LS_NONE;
}

saberMoveName_t PM_SaberAnimTransitionMove(saberMoveName_t curmove, saberMoveName_t newmove)
{
    // FIXME: take FP_SABER_OFFENSE into account here somehow?
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
            if (newmove == curmove) { // FIXME: need a spin or something or go to next level, but for now, just play the return
                // going into another attack...
                // allow endless chaining in level 1 attacks, several in level 2 and only one or a few in level 3
                // FIXME: don't let strong attacks chain to an attack in the opposite direction ( > 45 degrees?)
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

/*
-------------------------
PM_LegsAnimForFrame
Returns animNumber for current frame
-------------------------
*/
std::optional<animNumber_t> PM_LegsAnimForFrame(gentity_t* ent, int legsFrame)
{
    // Must be a valid client
    if (ent->client == nullptr)
        return std::nullopt;

    // Must have a file index entry
    if (ValidAnimFileIndex(ent->client->clientInfo.animFileIndex) == false)
        return std::nullopt;

    auto& animations = level.knownAnimFileSets[ent->client->clientInfo.animFileIndex].animations;
    int glaIndex = gi.G2API_GetAnimIndex(&(ent->ghoul2[0]));

    for (animNumber_t animation = animNumber_t::FACE_TALK0; animation < animNumber_t::BOTH_CIN_1; animation++) // first anim after last legs
    {
        if (animation >= animNumber_t::TORSO_DROPWEAP1 && animation < animNumber_t::LEGS_TURN1) // first legs only anim
        {                                                                                       // not a possible legs anim
            continue;
        }

        if (animations[animation].glaIndex != glaIndex) {
            continue;
        }

        if (animations[animation].firstFrame > legsFrame) { // This anim starts after this frame
            continue;
        }

        if (animations[animation].firstFrame + animations[animation].numFrames < legsFrame) { // This anim ends before this frame
            continue;
        }
        // else, must be in this anim!
        return animation;
    }

    // Not in ANY torsoAnim?  SHOULD NEVER HAPPEN
    //	assert(0);
    return std::nullopt;
}

std::optional<animNumber_t> PM_ValidateAnimRange(const int startFrame, const int endFrame, const float animSpeed)
{ // given a startframe and endframe, see if that lines up with any known animation
    auto& animations = level.knownAnimFileSets[0].animations;

    for (animNumber_t anim = animNumber_t::FACE_TALK0; anim < animNumber_t::MAX_ANIMATIONS; anim++) {
        if (animSpeed < 0) { // playing backwards
            if (animations[anim].firstFrame == endFrame) {
                if (animations[anim].numFrames + animations[anim].firstFrame == startFrame) {
                    // Com_Printf( "valid reverse anim: %s\n", BG_AnimName( anim ) );
                    return anim;
                }
            }
        }
        else {                                                                              // playing forwards
            if (animations[anim].firstFrame == startFrame) {                                // This anim starts on this frame
                if (animations[anim].firstFrame + animations[anim].numFrames == endFrame) { // This anim ends on this frame
                    // Com_Printf( "valid forward anim: %s\n", BG_AnimName( anim ) );
                    return anim;
                }
            }
        }
        // else, must not be this anim!
    }

    // Not in ANY anim?  SHOULD NEVER HAPPEN
    Com_Printf("invalid anim range %d to %d, speed %4.2f\n", startFrame, endFrame, animSpeed);
    return std::nullopt;
}
/*
-------------------------
PM_TorsoAnimForFrame
Returns animNumber for current frame
-------------------------
*/
std::optional<animNumber_t> PM_TorsoAnimForFrame(gentity_t* ent, int torsoFrame)
{
    // Must be a valid client
    if (ent->client == nullptr)
        return std::nullopt;

    // Must have a file index entry
    if (ValidAnimFileIndex(ent->client->clientInfo.animFileIndex) == false)
        return std::nullopt;

    auto& animations = level.knownAnimFileSets[ent->client->clientInfo.animFileIndex].animations;
    int glaIndex = gi.G2API_GetAnimIndex(&(ent->ghoul2[0]));

    for (animNumber_t animation = animNumber_t::FACE_TALK0; animation < animNumber_t::LEGS_TURN1; animation++) // first legs only anim
    {
        if (animations[animation].glaIndex != glaIndex) {
            continue;
        }

        if (animations[animation].firstFrame > torsoFrame) { // This anim starts after this frame
            continue;
        }

        if (animations[animation].firstFrame + animations[animation].numFrames < torsoFrame) { // This anim ends before this frame
            continue;
        }
        // else, must be in this anim!
        return animation;
    }

    // Not in ANY torsoAnim?  SHOULD NEVER HAPPEN
    //	assert(0);
    return std::nullopt;
}

bool PM_FinishedCurrentLegsAnim(gentity_t* self)
{
    int junk, curFrame;
    boneFlags_t junkFlags;
    float currentFrame, animSpeed;

    if (!self->client) {
        return true;
    }

    gi.G2API_GetBoneAnimIndex(&self->ghoul2[self->playerModel], self->rootBone, (cg.time ? cg.time : level.time), &currentFrame, &junk, &junk, &junkFlags,
                              &animSpeed, nullptr);
    curFrame = Q_floorf(currentFrame);

    animNumber_t legsAnim = self->client->ps.legsAnim;
    auto& animations = level.knownAnimFileSets[self->client->clientInfo.animFileIndex].animations;

    if (curFrame >= animations[legsAnim].firstFrame + (animations[legsAnim].numFrames - 2)) {
        return true;
    }

    return false;
}

/*
-------------------------
PM_HasAnimation
-------------------------
*/

bool PM_HasAnimation(gentity_t* ent, animNumber_t animation)
{
    // Must be a valid client
    if (!ent || ent->client == nullptr)
        return false;

    // must be a valid anim number
    if (animation < animNumber_t::FACE_TALK0 || animation >= animNumber_t::MAX_ANIMATIONS) {
        return false;
    }
    // Must have a file index entry
    if (ValidAnimFileIndex(ent->client->clientInfo.animFileIndex) == false)
        return false;

    auto& animations = level.knownAnimFileSets[ent->client->clientInfo.animFileIndex].animations;

    // No frames, no anim
    if (animations[animation].numFrames == 0)
        return false;

    // Has the sequence
    return true;
}

animNumber_t PM_PickAnim(gentity_t* self, animNumber_t minAnim, animNumber_t maxAnim)
{
    animNumber_t anim;
    int count = 0;

    if (!self) {
        return Q_irand(minAnim, maxAnim);
    }

    do {
        anim = Q_irand(minAnim, maxAnim);
        count++;
    } while (!PM_HasAnimation(self, anim) && count < 1000);

    return anim;
}

/*
-------------------------
PM_AnimLength
-------------------------
*/

int PM_AnimLength(int index, animNumber_t anim)
{
    if (!ValidAnimFileIndex(index) || anim < animNumber_t::FACE_TALK0 || anim >= animNumber_t::MAX_ANIMATIONS) {
        return 0;
    }

    return level.knownAnimFileSets[index].animations[anim].numFrames * abs(level.knownAnimFileSets[index].animations[anim].frameLerp);
}

/*
-------------------------
PM_SetLegsAnimTimer
-------------------------
*/

void PM_SetLegsAnimTimer(gentity_t* ent, int* legsAnimTimer, int time)
{
    *legsAnimTimer = time;

    if (*legsAnimTimer < 0 && time != -1) { // Cap timer to 0 if was counting down, but let it be -1 if that was intentional
        *legsAnimTimer = 0;
    }

    if (!*legsAnimTimer && ent && Q3_TaskIDPending(ent, taskID_t::TID_ANIM_LOWER)) { // Waiting for legsAnimTimer to complete, and it just got set to zero
        if (!Q3_TaskIDPending(ent, taskID_t::TID_ANIM_BOTH)) {                       // Not waiting for top
            Q3_TaskIDComplete(ent, taskID_t::TID_ANIM_LOWER);
        }
        else {                                                      // Waiting for both to finish before complete
            Q3_TaskIDClear(&ent->taskID[taskID_t::TID_ANIM_LOWER]); // Bottom is done, regardless
            if (!Q3_TaskIDPending(ent, taskID_t::TID_ANIM_UPPER)) { // top is done and we're done
                Q3_TaskIDComplete(ent, taskID_t::TID_ANIM_BOTH);
            }
        }
    }
}

/*
-------------------------
PM_SetTorsoAnimTimer
-------------------------
*/

void PM_SetTorsoAnimTimer(gentity_t* ent, int* torsoAnimTimer, int time)
{
    *torsoAnimTimer = time;

    if (*torsoAnimTimer < 0 && time != -1) { // Cap timer to 0 if was counting down, but let it be -1 if that was intentional
        *torsoAnimTimer = 0;
    }

    if (!*torsoAnimTimer && ent && Q3_TaskIDPending(ent, taskID_t::TID_ANIM_UPPER)) { // Waiting for torsoAnimTimer to complete, and it just got set to zero
        if (!Q3_TaskIDPending(ent, taskID_t::TID_ANIM_BOTH)) {                        // Not waiting for bottom
            Q3_TaskIDComplete(ent, taskID_t::TID_ANIM_UPPER);
        }
        else {                                                      // Waiting for both to finish before complete
            Q3_TaskIDClear(&ent->taskID[taskID_t::TID_ANIM_UPPER]); // Top is done, regardless
            if (!Q3_TaskIDPending(ent, taskID_t::TID_ANIM_LOWER)) { // lower is done and we're done
                Q3_TaskIDComplete(ent, taskID_t::TID_ANIM_BOTH);
            }
        }
    }
}

extern bool PM_SpinningSaberAnim(animNumber_t anim);
extern EnumArray<float, forcePowerLevels_t, forcePowerLevels_t::NUM_FORCE_POWER_LEVELS> saberAnimSpeedMod;
void PM_SaberStartTransAnim(saber_styles_t saberAnimLevel, animNumber_t anim, float* animSpeed, gentity_t* gent)
{
    if (anim >= animNumber_t::BOTH_A1_T__B_ && anim <= animNumber_t::BOTH_ROLL_STAB) {
        if (g_saberAnimSpeed->value != 1.0f) {
            *animSpeed *= g_saberAnimSpeed->value;
        }
        else if (gent && gent->client && gent->client->ps.weapon == weapon_t::WP_SABER) {
            if (gent->client->ps.saber[0].animSpeedScale != 1.0f) {
                *animSpeed *= gent->client->ps.saber[0].animSpeedScale;
            }
            if (gent->client->ps.dualSabers && gent->client->ps.saber[1].animSpeedScale != 1.0f) {
                *animSpeed *= gent->client->ps.saber[1].animSpeedScale;
            }
        }
    }
    if (gent && gent->client && gent->client->ps.stats[statIndex_t::STAT_WEAPONS] & Bit(weapon_t::WP_SCEPTER) && gent->client->ps.dualSabers &&
        saberAnimLevel == saber_styles_t::SS_DUAL && gent->weaponModel[1]) { // using a scepter and dual style, slow down anims
        if (anim >= animNumber_t::BOTH_A1_T__B_ && anim <= animNumber_t::BOTH_H7_S7_BR) {
            *animSpeed *= 0.75;
        }
    }
    if (gent && gent->client && gent->client->ps.forceRageRecoveryTime > level.time) {    // rage recovery
        if (anim >= animNumber_t::BOTH_A1_T__B_ && anim <= animNumber_t::BOTH_H1_S1_BR) { // animate slower
            *animSpeed *= 0.75;
        }
    }
    else if (gent && gent->NPC && gent->NPC->rank == rank_t::RANK_CIVILIAN) {             // grunt reborn
        if (anim >= animNumber_t::BOTH_A1_T__B_ && anim <= animNumber_t::BOTH_R1_TR_S1) { // his fast attacks are slower
            if (!PM_SpinningSaberAnim(anim)) {
                *animSpeed *= 0.75;
            }
            return;
        }
    }
    else if (gent && gent->client) {
        if (gent->client->ps.saber[0].type == saberType_t::SABER_LANCE ||
            gent->client->ps.saber[0].type ==
                saberType_t::SABER_TRIDENT) { // FIXME: hack for now - these use the fast anims, but slowed down.  Should have own style
            if (anim >= animNumber_t::BOTH_A1_T__B_ && anim <= animNumber_t::BOTH_R1_TR_S1) { // his fast attacks are slower
                if (!PM_SpinningSaberAnim(anim)) {
                    *animSpeed *= 0.75;
                }
                return;
            }
        }
    }

    if ((anim >= animNumber_t::BOTH_T1_BR__R && anim <= animNumber_t::BOTH_T1_BL_TL) ||
        (anim >= animNumber_t::BOTH_T3_BR__R && anim <= animNumber_t::BOTH_T3_BL_TL) ||
        (anim >= animNumber_t::BOTH_T5_BR__R && anim <= animNumber_t::BOTH_T5_BL_TL)) {
        if (saberAnimLevel == saber_styles_t::SS_FAST || saberAnimLevel == saber_styles_t::SS_TAVION) { // FIXME: should not be necc for FORCE_LEVEL_1's
            *animSpeed *= 1.5;
        }
        else if (saberAnimLevel == saber_styles_t::SS_STRONG) {
            *animSpeed *= 0.75;
        }
    }
}
/*
void PM_SaberStartTransAnim( animNumber_t anim, int entNum, forcePowerLevels_t saberOffenseLevel, float *animSpeed )
{
    //check starts
    if ( ( anim >= animNumber_t::BOTH_S1_S1_T_ &&
        anim <= animNumber_t::BOTH_S1_S1_TR ) ||
        ( anim >= animNumber_t::BOTH_S1_S1_T_ &&
            anim <= animNumber_t::BOTH_S1_S1_TR ) ||
        ( anim >= animNumber_t::BOTH_S3_S1_T_ &&
            anim <= animNumber_t::BOTH_S3_S1_TR ) )
    {
        if ( entNum == 0 )
        {
            *animSpeed *= saberAnimSpeedMod[forcePowerLevels_t::FORCE_LEVEL_3];
        }
        else
        {
            *animSpeed *= saberAnimSpeedMod[saberOffenseLevel];
        }
    }
    //Check transitions
    else if ( PM_SpinningSaberAnim( anim ) )
    {//spins stay normal speed
        return;
    }
    else if ( ( anim >= animNumber_t::BOTH_T1_BR__R &&
            anim <= animNumber_t::BOTH_T1_BL_TL ) ||
            ( anim >= animNumber_t::BOTH_T2_BR__R &&
            anim <= animNumber_t::BOTH_T2_BL_TL ) ||
            ( anim >= animNumber_t::BOTH_T3_BR__R &&
            anim <= animNumber_t::BOTH_T3_BL_TL ) )
    {//slow down the transitions
        if ( entNum == 0 && saberOffenseLevel <= forcePowerLevels_t::FORCE_LEVEL_2 )
        {
            *animSpeed *= saberAnimSpeedMod[saberOffenseLevel];
        }
        else
        {
            *animSpeed *= saberAnimSpeedMod[saberOffenseLevel]/2.0f;
        }
    }

    return;
}
*/
extern bool player_locked;
extern bool MatrixMode;
float PM_GetTimeScaleMod(gentity_t* gent)
{
    if (g_timescale->value) {
        if (!MatrixMode && gent->client->ps.legsAnim != animNumber_t::BOTH_FORCELONGLEAP_START &&
            gent->client->ps.legsAnim != animNumber_t::BOTH_FORCELONGLEAP_ATTACK && gent->client->ps.legsAnim != animNumber_t::BOTH_FORCELONGLEAP_LAND) {
            if (gent && gent->s.clientNum == 0 && !player_locked && gent->client->ps.forcePowersActive & Bit(forcePowers_t::FP_SPEED)) {
                return (1.0 / g_timescale->value);
            }
            else if (gent && gent->client && gent->client->ps.forcePowersActive & Bit(forcePowers_t::FP_SPEED)) {
                return (1.0 / g_timescale->value);
            }
        }
    }
    return 1.0f;
}

/*
-------------------------
PM_SetAnimFinal
-------------------------
*/
void PM_SetAnimFinal(animNumber_t* torsoAnim, animNumber_t* legsAnim, setAnimParts_t setAnimParts, animNumber_t anim, setAnimFlags_t setAnimFlags,
                     int* torsoAnimTimer, int* legsAnimTimer, gentity_t* gent, int blendTime) // default blendTime=350
{

    // BASIC SETUP AND SAFETY CHECKING
    //=================================

    // If It Is A Busted Entity, Don't Do Anything Here.
    //---------------------------------------------------
    if (!gent || !gent->client) {
        return;
    }

    // Make Sure This Character Has Such An Anim And A Model
    //-------------------------------------------------------
    if (anim < animNumber_t::FACE_TALK0 || anim >= animNumber_t::MAX_ANIMATIONS || !ValidAnimFileIndex(gent->client->clientInfo.animFileIndex)) {
#ifndef FINAL_BUILD
        if (g_AnimWarning->integer) {
            if (anim < animNumber_t::FACE_TALK0 || anim >= animNumber_t::MAX_ANIMATIONS) {
                Com_Printf(S_COLOR_RED "PM_SetAnimFinal: Invalid Anim Index (%d)!\n", to_underlying(anim));
            }
            else {
                Com_Printf(S_COLOR_RED "PM_SetAnimFinal: Invalid Anim File Index (%d)!\n", gent->client->clientInfo.animFileIndex);
            }
        }
#endif
        return;
    }

    // Get Global Time Properties
    //----------------------------
    float timeScaleMod = PM_GetTimeScaleMod(gent);
    const int actualTime = (cg.time ? cg.time : level.time);
    const auto& animations = level.knownAnimFileSets[gent->client->clientInfo.animFileIndex].animations;
    const animation_t& curAnim = animations[anim];

    // Make Sure This Character Has Such An Anim And A Model
    //-------------------------------------------------------
    if (animations[anim].numFrames == 0) {
#ifndef FINAL_BUILD
        static animNumber_t LastAnimWarningNum = animNumber_t::FACE_TALK0;
        if (LastAnimWarningNum != anim) {
            if ((cg_debugAnim.integer == 3) ||                                              // 3 = do everyone
                (cg_debugAnim.integer == 1 && gent->s.number == 0) ||                       // 1 = only the player
                (cg_debugAnim.integer == 2 && gent->s.number != 0) ||                       // 2 = only everyone else
                (cg_debugAnim.integer == 4 && gent->s.number != cg_debugAnimTarget.integer) // 4 = specific entnum
            ) {
                Com_Printf(S_COLOR_RED "PM_SetAnimFinal: Anim %s does not exist in this model (%s)!\n", BG_AnimName(anim), gent->NPC_type);
            }
        }
        LastAnimWarningNum = anim;
#endif
        return;
    }

    // If It's Not A Ghoul 2 Model, Just Remember The Anims And Stop, Because Everything Beyond This Is Ghoul2
    //---------------------------------------------------------------------------------------------------------
    if (!gi.G2API_HaveWeGhoul2Models(gent->ghoul2)) {
        if (Any(setAnimParts & setAnimParts_t::SETANIM_TORSO)) {
            (*torsoAnim) = anim;
        }
        if (Any(setAnimParts & setAnimParts_t::SETANIM_LEGS)) {
            (*legsAnim) = anim;
        }
        return;
    }

    // Lower Offensive Skill Slows Down The Saber Start Attack Animations
    //--------------------------------------------------------------------
    PM_SaberStartTransAnim(gent->client->ps.saberAnimLevel, anim, &timeScaleMod, gent);

    // SETUP VALUES FOR INCOMMING ANIMATION
    //======================================
    const bool animFootMove =
        (PM_WalkingAnim(anim) || PM_RunningAnim(anim) || anim == animNumber_t::BOTH_CROUCH1WALK || anim == animNumber_t::BOTH_CROUCH1WALKBACK);
    const bool animHoldless = Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_HOLDLESS);
    const bool animHold = Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_HOLD);
    const bool animRestart = Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_RESTART);
    const bool animOverride = Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_OVERRIDE);
    const bool animSync = (g_synchSplitAnims->integer != 0 && !animRestart);
    float animCurrent = (-1.0f);
    float animSpeed = (50.0f / curAnim.frameLerp * timeScaleMod); // animSpeed is 1.0 if the frameLerp (ms/frame) is 50 (20 fps).
    const float animFPS = (::abs(curAnim.frameLerp));
    const int animDurMSec = (int)(((curAnim.numFrames - 1) * animFPS) / timeScaleMod);
    const int animHoldMSec = ((animHoldless && timeScaleMod == 1.0f) ? ((animDurMSec > 1) ? (animDurMSec - 1) : (animFPS)) : (animDurMSec));
    boneFlags_t animFlags = (curAnim.loopFrames != -1) ? (boneFlags_t::BONE_ANIM_OVERRIDE_LOOP) : (boneFlags_t::BONE_ANIM_OVERRIDE_FREEZE);
    int animStart = (curAnim.firstFrame);
    int animEnd = (curAnim.firstFrame) + (animations[anim].numFrames);

    // If We Have A Blend Timer, Add The Blend Flag
    //----------------------------------------------
    if (blendTime > 0) {
        animFlags |= boneFlags_t::BONE_ANIM_BLEND;
    }

    // If Animation Is Going Backwards, Swap Last And First Frames
    //-------------------------------------------------------------
    if (animSpeed < 0.0f) {
        //	#ifndef FINAL_BUILD
#if 0
		if (g_AnimWarning->integer==1)
		{
			if (animFlags&boneFlags_t::BONE_ANIM_OVERRIDE_LOOP)
			{
				Com_Printf(S_COLOR_YELLOW"PM_SetAnimFinal: WARNING: Anim (%s) looping backwards!\n", BG_AnimName( anim ));
			}
		}
#endif

        int temp = animEnd;
        animEnd = animStart;
        animStart = temp;
        blendTime = 0;
    }

    // If The Animation Is Walking Or Running, Attempt To Scale The Playback Speed To Match
    //--------------------------------------------------------------------------------------
    if (g_noFootSlide->integer && animFootMove &&
        !(animSpeed < 0.0f)
        // FIXME: either read speed from animation.cfg or only do this for NPCs
        //			for whom we've specifically determined the proper numbers!
        && gent->client->NPC_class != class_t::CLASS_HOWLER && gent->client->NPC_class != class_t::CLASS_WAMPA &&
        gent->client->NPC_class != class_t::CLASS_GONK && gent->client->NPC_class != class_t::CLASS_HOWLER && gent->client->NPC_class != class_t::CLASS_MOUSE &&
        gent->client->NPC_class != class_t::CLASS_PROBE && gent->client->NPC_class != class_t::CLASS_PROTOCOL &&
        gent->client->NPC_class != class_t::CLASS_R2D2 && gent->client->NPC_class != class_t::CLASS_R5D2 && gent->client->NPC_class != class_t::CLASS_SEEKER) {
        bool Walking = !!PM_WalkingAnim(anim);
        bool HasDual = (gent->client->ps.saberAnimLevel == saber_styles_t::SS_DUAL);
        bool HasStaff = (gent->client->ps.saberAnimLevel == saber_styles_t::SS_STAFF);
        float moveSpeedOfAnim = 150.0f; // g_noFootSlideRunScale->value;

        if (anim == animNumber_t::BOTH_CROUCH1WALK || anim == animNumber_t::BOTH_CROUCH1WALKBACK) {
            moveSpeedOfAnim = 75.0f;
        }
        else {
            if (gent->client->NPC_class == class_t::CLASS_HAZARD_TROOPER) {
                moveSpeedOfAnim = 50.0f;
            }
            else if (gent->client->NPC_class == class_t::CLASS_RANCOR) {
                moveSpeedOfAnim = 173.0f;
            }
            else {
                if (Walking) {
                    if (HasDual || HasStaff) {
                        moveSpeedOfAnim = 100.0f;
                    }
                    else {
                        moveSpeedOfAnim = 50.0f; // g_noFootSlideWalkScale->value;
                    }
                }
                else {
                    if (HasStaff) {
                        moveSpeedOfAnim = 250.0f;
                    }
                    else {
                        moveSpeedOfAnim = 150.0f;
                    }
                }
            }
        }

        animSpeed *= (gent->resultspeed / moveSpeedOfAnim);
        if (animSpeed < 0.01f) {
            animSpeed = 0.01f;
        }

        // Make Sure Not To Play Too Fast An Anim
        //----------------------------------------
        float maxPlaybackSpeed = (1.5f * timeScaleMod);
        if (animSpeed > maxPlaybackSpeed) {
            animSpeed = maxPlaybackSpeed;
        }
    }

    // GET VALUES FOR EXISTING BODY ANIMATION
    //==========================================
    float bodySpeed = 0.0f;
    float bodyCurrent = 0.0f;
    int bodyStart = 0;
    int bodyEnd = 0;
    boneFlags_t bodyFlags = boneFlags_t::BONE_NONE;
    animNumber_t bodyAnim = (*legsAnim);
    int bodyBone = (gent->rootBone);
    bool bodyTimerOn = ((*legsAnimTimer > 0) || (*legsAnimTimer) == -1);
    bool bodyPlay = (Any(setAnimParts & setAnimParts_t::SETANIM_LEGS) && (bodyBone != -1) && (animOverride || !bodyTimerOn));
    bool bodyAnimating = !!gi.G2API_GetBoneAnimIndex(&gent->ghoul2[gent->playerModel], bodyBone, actualTime, &bodyCurrent, &bodyStart, &bodyEnd, &bodyFlags,
                                                     &bodySpeed, nullptr);
    bool bodyOnAnimNow = (bodyAnimating && bodyAnim == anim && bodyStart == animStart && bodyEnd == animEnd);
    bool bodyMatchTorsFrame = false;

    // GET VALUES FOR EXISTING TORSO ANIMATION
    //===========================================
    float torsSpeed = 0.0f;
    float torsCurrent = 0.0f;
    int torsStart = 0;
    int torsEnd = 0;
    boneFlags_t torsFlags = boneFlags_t::BONE_NONE;
    animNumber_t torsAnim = (*torsoAnim);
    int torsBone = (gent->lowerLumbarBone);
    bool torsTimerOn = ((*torsoAnimTimer) > 0 || (*torsoAnimTimer) == -1);
    bool torsPlay = (gent->client->NPC_class != class_t::CLASS_RANCOR && Any(setAnimParts & setAnimParts_t::SETANIM_TORSO) && (torsBone != -1) &&
                     (animOverride || !torsTimerOn));
    bool torsAnimating = !!gi.G2API_GetBoneAnimIndex(&gent->ghoul2[gent->playerModel], torsBone, actualTime, &torsCurrent, &torsStart, &torsEnd, &torsFlags,
                                                     &torsSpeed, nullptr);
    bool torsOnAnimNow = (torsAnimating && torsAnim == anim && torsStart == animStart && torsEnd == animEnd);
    bool torsMatchBodyFrame = false;

    // APPLY SYNC TO TORSO
    //=====================
    if (animSync && torsPlay && !bodyPlay && bodyOnAnimNow && (!torsOnAnimNow || torsCurrent != bodyCurrent)) {
        torsMatchBodyFrame = true;
        animCurrent = bodyCurrent;
    }
    if (animSync && bodyPlay && !torsPlay && torsOnAnimNow && (!bodyOnAnimNow || bodyCurrent != torsCurrent)) {
        bodyMatchTorsFrame = true;
        animCurrent = torsCurrent;
    }

    // If Already Doing These Exact Parameters, Then Don't Play
    //----------------------------------------------------------
    if (!animRestart) {
        torsPlay &= !(torsOnAnimNow && torsSpeed == animSpeed && !torsMatchBodyFrame);
        bodyPlay &= !(bodyOnAnimNow && bodySpeed == animSpeed && !bodyMatchTorsFrame);
    }

#ifndef FINAL_BUILD
    if ((cg_debugAnim.integer == 3) ||                                              // 3 = do everyone
        (cg_debugAnim.integer == 1 && gent->s.number == 0) ||                       // 1 = only the player
        (cg_debugAnim.integer == 2 && gent->s.number != 0) ||                       // 2 = only everyone else
        (cg_debugAnim.integer == 4 && gent->s.number != cg_debugAnimTarget.integer) // 4 = specific entnum
    ) {
        if (bodyPlay || torsPlay) {
            const char* entName = gent->targetname;
            const char* location;

            // Select Entity Name
            //--------------------
            if (!entName || !entName[0]) {
                entName = gent->NPC_targetname;
            }
            if (!entName || !entName[0]) {
                entName = gent->NPC_type;
            }
            if (!entName || !entName[0]) {
                entName = gent->classname;
            }
            if (!entName || !entName[0]) {
                entName = "UNKNOWN";
            }

            // Select Play Location
            //----------------------
            if (bodyPlay && torsPlay) {
                location = "BOTH ";
            }
            else if (bodyPlay) {
                location = "LEGS ";
            }
            else {
                location = "TORSO";
            }

            // Print It!
            //-----------
            Com_Printf("[%10d] ent[%3d-%18s] %s anim[%3d] - %s\n", actualTime, gent->s.number, entName, location, to_underlying(anim), BG_AnimName(anim));
        }
    }
#endif

    // PLAY ON THE TORSO
    //========================
    if (torsPlay) {
        *torsoAnim = anim;
        float oldAnimCurrent = animCurrent;
        if (animCurrent != bodyCurrent && torsOnAnimNow && !animRestart && !torsMatchBodyFrame) {
            animCurrent = torsCurrent;
        }

        gi.G2API_SetAnimIndex(&gent->ghoul2[gent->playerModel], curAnim.glaIndex);
        gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], torsBone, animStart, animEnd,
                                  (torsOnAnimNow && !animRestart) ? (animFlags & ~boneFlags_t::BONE_ANIM_BLEND) : (animFlags), animSpeed, actualTime,
                                  animCurrent, blendTime);

        if (gent->motionBone != -1) {
            gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->motionBone, animStart, animEnd,
                                      (torsOnAnimNow && !animRestart) ? (animFlags & ~boneFlags_t::BONE_ANIM_BLEND) : (animFlags), animSpeed, actualTime,
                                      animCurrent, blendTime);
        }

        animCurrent = oldAnimCurrent;

        // If This Animation Is To Be Locked And Held, Calculate The Duration And Set The Timer
        //--------------------------------------------------------------------------------------
        if (animHold || animHoldless) {
            PM_SetTorsoAnimTimer(gent, torsoAnimTimer, animHoldMSec);
        }
    }

    // PLAY ON THE WHOLE BODY
    //========================
    if (bodyPlay) {
        *legsAnim = anim;

        if (bodyOnAnimNow && !animRestart && !bodyMatchTorsFrame) {
            animCurrent = bodyCurrent;
        }

        gi.G2API_SetAnimIndex(&gent->ghoul2[gent->playerModel], curAnim.glaIndex);
        gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], bodyBone, animStart, animEnd,
                                  (bodyOnAnimNow && !animRestart) ? (animFlags & ~boneFlags_t::BONE_ANIM_BLEND) : (animFlags), animSpeed, actualTime,
                                  animCurrent, blendTime);

        // If This Animation Is To Be Locked And Held, Calculate The Duration And Set The Timer
        //--------------------------------------------------------------------------------------
        if (animHold || animHoldless) {
            PM_SetLegsAnimTimer(gent, legsAnimTimer, animHoldMSec);
        }
    }

    // PRINT SOME DEBUG TEXT OF EXISTING VALUES
    //==========================================
    if (false) {
        Com_Printf("PLAYANIM: (%3d) Speed(%4.2f) ", to_underlying(anim), animSpeed);
        if (bodyAnimating) {
            Com_Printf("BODY: (%4.2f) (%4.2f) ", bodyCurrent, bodySpeed);
        }
        else {
            Com_Printf("                      ");
        }
        if (torsAnimating) {
            Com_Printf("TORS: (%4.2f) (%4.2f)\n", torsCurrent, torsSpeed);
        }
        else {
            Com_Printf("\n");
        }
    }
}

void PM_SetAnim(pmove_t* pm, setAnimParts_t setAnimParts, animNumber_t anim, setAnimFlags_t setAnimFlags, int blendTime)
{ // FIXME : once torsoAnim and legsAnim are in the same structure for NPC and Players
    // rename PM_SetAnimFinal to PM_SetAnim and have both NPC and Players call PM_SetAnim

    if (pm->ps->pm_type >= pmtype_t::PM_DEAD) { // FIXME: sometimes we'll want to set anims when your dead... twitches, impacts, etc.
        return;
    }

    if (pm->gent == nullptr) {
        return;
    }

    if (!pm->gent || pm->gent->health > 0) {                                                      // don't lock anims if the guy is dead
        if (pm->ps->torsoAnimTimer && PM_LockedAnim(pm->ps->torsoAnim) && !PM_LockedAnim(anim)) { // nothing can override these special anims
            setAnimParts &= ~setAnimParts_t::SETANIM_TORSO;
        }

        if (pm->ps->legsAnimTimer && PM_LockedAnim(pm->ps->legsAnim) && !PM_LockedAnim(anim)) { // nothing can override these special anims
            setAnimParts &= ~setAnimParts_t::SETANIM_LEGS;
        }
    }

    if (!Any(setAnimParts)) {
        return;
    }

    if (Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_OVERRIDE)) {
        //		pm->ps->animationTimer = 0;

        if (Any(setAnimParts & setAnimParts_t::SETANIM_TORSO)) {
            if (Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_RESTART) || pm->ps->torsoAnim != anim) {
                PM_SetTorsoAnimTimer(pm->gent, &pm->ps->torsoAnimTimer, 0);
            }
        }
        if (Any(setAnimParts & setAnimParts_t::SETANIM_LEGS)) {
            if (Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_RESTART) || pm->ps->legsAnim != anim) {
                PM_SetLegsAnimTimer(pm->gent, &pm->ps->legsAnimTimer, 0);
            }
        }
    }

    PM_SetAnimFinal(&pm->ps->torsoAnim, &pm->ps->legsAnim, setAnimParts, anim, setAnimFlags, &pm->ps->torsoAnimTimer, &pm->ps->legsAnimTimer,
                    &g_entities[pm->ps->clientNum], blendTime); // was pm->gent
}

bool TorsoAgainstWindTest(gentity_t* ent)
{
    if (ent &&         // valid ent
        ent->client && // a client
        (ent->client->ps.weapon != weapon_t::WP_SABER ||
         ent->client->ps.saberMove == saberMoveName_t::LS_READY) && // either not holding a saber or the saber is in the ready pose
        (ent->s.number < MAX_CLIENTS || G_ControlledByPlayer(ent)) &&
        gi.WE_GetWindGusting(ent->currentOrigin) && gi.WE_IsOutside(ent->currentOrigin)) {
        if (Q_stricmp(level.mapname, "t2_wedge") != 0) {
            vec3_t fwd;
            vec3_t windDir;
            if (gi.WE_GetWindVector(windDir, ent->currentOrigin)) {
                VectorScale(windDir, -1.0f, windDir);
                AngleVectors(pm->gent->currentAngles, fwd, nullptr, nullptr);
                if (DotProduct(fwd, windDir) > 0.65f) {
                    if (ent->client && ent->client->ps.torsoAnim != animNumber_t::BOTH_WIND) {
                        NPC_SetAnim(ent, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WIND, setAnimFlags_t::SETANIM_FLAG_NORMAL, 400);
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

/*
-------------------------
PM_TorsoAnimLightsaber
-------------------------
*/

// Note that this function is intended to set the animation for the player, but
// only does idle-ish anims.  Anything that has a timer associated, such as attacks and blocks,
// are set by PM_WeaponLightsaber()

extern Vehicle_t* G_IsRidingVehicle(gentity_t* pEnt);
extern bool PM_LandingAnim(animNumber_t anim);
extern bool PM_JumpingAnim(animNumber_t anim);
bool PM_InCartwheel(animNumber_t anim);
void PM_TorsoAnimLightsaber()
{
    // *********************************************************
    // WEAPON_READY
    // *********************************************************
    if (pm->ps->forcePowersActive & Bit(forcePowers_t::FP_GRIP) &&
        pm->ps->forcePowerLevel[forcePowers_t::FP_GRIP] > forcePowerLevels_t::FORCE_LEVEL_1) { // holding an enemy aloft with force-grip
        return;
    }

    if (pm->ps->forcePowersActive & Bit(forcePowers_t::FP_LIGHTNING) &&
        pm->ps->forcePowerLevel[forcePowers_t::FP_LIGHTNING] > forcePowerLevels_t::FORCE_LEVEL_1) { // lightning
        return;
    }

    if (pm->ps->forcePowersActive & Bit(forcePowers_t::FP_DRAIN)) { // drain
        return;
    }

    if (pm->ps->saber[0].blade[0].active && pm->ps->saber[0].blade[0].length < 3 && !Any(pm->ps->saberEventFlags & saberEventFlags_t::SEF_HITWALL) &&
        pm->ps->weaponstate == weaponstate_t::WEAPON_RAISING) {
        if (!G_IsRidingVehicle(pm->gent)) {
            PM_SetSaberMove(saberMoveName_t::LS_DRAW);
        }
        return;
    }
    else if (!pm->ps->SaberActive() && pm->ps->SaberLength()) {
        if (!G_IsRidingVehicle(pm->gent)) {
            PM_SetSaberMove(saberMoveName_t::LS_PUTAWAY);
        }
        return;
    }

    if (pm->ps->weaponTime > 0) {                                                                             // weapon is already busy.
        if (pm->ps->torsoAnim == animNumber_t::BOTH_TOSS1 || pm->ps->torsoAnim == animNumber_t::BOTH_TOSS2) { // in toss
            if (!pm->ps->torsoAnimTimer) {                                                                    // weird, get out of it, I guess
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            }
        }
        return;
    }

    if (pm->ps->weaponstate == weaponstate_t::WEAPON_READY || pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING ||
        pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING_ALT) {           // ready
        if (pm->ps->weapon == weapon_t::WP_SABER && (pm->ps->SaberLength())) { // saber is on
            // Select the proper idle Lightsaber attack move from the chart.
            if (pm->ps->saberMove > saberMoveName_t::LS_READY && pm->ps->saberMove < saberMoveName_t::LS_MOVE_MAX) {
                PM_SetSaberMove(saberMoveData[pm->ps->saberMove].chain_idle);
            }
            else {
                if (PM_JumpingAnim(pm->ps->legsAnim) || PM_LandingAnim(pm->ps->legsAnim) || PM_InCartwheel(pm->ps->legsAnim) ||
                    PM_FlippingAnim(pm->ps->legsAnim)) {
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                }
                else {
                    if ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) && pm->ps->torsoAnim == animNumber_t::BOTH_BUTTON_HOLD) { // using
                                                                                                                                               // something
                        if (!pm->ps->useTime) { // stopped holding it, release
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_BUTTON_RELEASE,
                                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                        } // else still holding, leave it as it is
                    }
                    else {
                        if ((PM_RunningAnim(pm->ps->legsAnim) || pm->ps->legsAnim == animNumber_t::BOTH_WALK_STAFF ||
                             pm->ps->legsAnim == animNumber_t::BOTH_WALK_DUAL || pm->ps->legsAnim == animNumber_t::BOTH_WALKBACK_STAFF ||
                             pm->ps->legsAnim == animNumber_t::BOTH_WALKBACK_DUAL) &&
                            pm->ps->saberBlockingTime < cg.time) { // running w/1-handed weapon uses full-body anim
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetSaberMove(saberMoveName_t::LS_READY);
                        }
                    }
                }
            }
            /*
            if ( PM_JumpingAnim( pm->ps->legsAnim )
                || PM_LandingAnim( pm->ps->legsAnim )
                || PM_InCartwheel( pm->ps->legsAnim )
                || PM_FlippingAnim( pm->ps->legsAnim ))
            {//jumping, landing cartwheel, flipping
                PM_SetAnim(pm,setAnimParts_t::SETANIM_TORSO,pm->ps->legsAnim,setAnimFlags_t::SETANIM_FLAG_NORMAL);
            }
            else
            {
                PM_SetSaberMove( saberMoveName_t::LS_READY );
            }
            */
        }
        else if (TorsoAgainstWindTest(pm->gent)) {
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN1) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN2) //&& pm->ps->saberAnimLevel != saber_styles_t::SS_STAFF )
        {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN_STAFF) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN_STAFF, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN_DUAL) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN_DUAL, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_WALK1) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WALK1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_WALK2) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WALK2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_WALK_STAFF) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WALK_STAFF, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_WALK_DUAL) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WALK_DUAL, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_CROUCH1IDLE && pm->ps->clientNum != 0) // player falls through
        {
            //??? Why nothing?  What if you were running???
            // PM_SetAnim(pm,setAnimParts_t::SETANIM_TORSO,animNumber_t::BOTH_CROUCH1IDLE,setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_JUMP1) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_JUMP1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else { // Used to default to both_stand1 which is an arms-down anim
            //				PM_SetAnim(pm,setAnimParts_t::SETANIM_TORSO,animNumber_t::BOTH_ATTACK1,setAnimFlags_t::SETANIM_FLAG_NORMAL);//TORSO_WEAPONREADY1
            // Select the next proper pose for the lightsaber assuming that there are no attacks.
            if (pm->ps->saberMove > saberMoveName_t::LS_READY && pm->ps->saberMove < saberMoveName_t::LS_MOVE_MAX) {
                PM_SetSaberMove(saberMoveData[pm->ps->saberMove].chain_idle);
            }
            else {
                if (PM_JumpingAnim(pm->ps->legsAnim) || PM_LandingAnim(pm->ps->legsAnim) || PM_InCartwheel(pm->ps->legsAnim) ||
                    PM_FlippingAnim(pm->ps->legsAnim)) {
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                }
                else {
                    if ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) && pm->ps->torsoAnim == animNumber_t::BOTH_BUTTON_HOLD) { // using
                                                                                                                                               // something
                        if (!pm->ps->useTime) { // stopped holding it, release
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_BUTTON_RELEASE,
                                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                        } // else still holding, leave it as it is
                    }
                    else {
                        PM_SetSaberMove(saberMoveName_t::LS_READY);
                    }
                }
            }
        }
    }

    // *********************************************************
    // WEAPON_IDLE
    // *********************************************************

    else if (pm->ps->weaponstate == weaponstate_t::WEAPON_IDLE) {
        if (TorsoAgainstWindTest(pm->gent)) {
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_GUARD_LOOKAROUND1) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_GUARD_LOOKAROUND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_GUARD_IDLE1) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_GUARD_IDLE1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_STAND1IDLE1 || pm->ps->legsAnim == animNumber_t::BOTH_STAND2IDLE1 ||
                 pm->ps->legsAnim == animNumber_t::BOTH_STAND2IDLE2 || pm->ps->legsAnim == animNumber_t::BOTH_STAND3IDLE1 ||
                 pm->ps->legsAnim == animNumber_t::BOTH_STAND5IDLE1) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_STAND2TO4) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND2TO4, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_STAND4TO2) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND4TO2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_STAND4) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND4, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else {
            // This is now set in SetSaberMove.
            // Idle for Lightsaber
            if (pm->gent && pm->gent->client) {
                //				pm->gent->client->saberTrail.inAction = false;
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
            if (pm->ps->saberInFlight && saberInAir && (!pm->ps->dualSabers || !pm->ps->saber[1].Active())) {
                if (!PM_ForceAnim(pm->ps->torsoAnim) || pm->ps->torsoAnimTimer < 300) { // don't interrupt a force power anim
                    if (pm->ps->torsoAnim != animNumber_t::BOTH_LOSE_SABER || !pm->ps->torsoAnimTimer) {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_SABERPULL,
                                   setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                    }
                }
            }
            else { // saber is on
                // Idle for Lightsaber
                if (pm->gent && pm->gent->client) {
                    if (!G_InCinematicSaberAnim(pm->gent)) {
                        pm->gent->client->ps.SaberDeactivateTrail(0);
                    }
                }
                // Idle for idle/ready Lightsaber
                //				PM_SetAnim(pm,setAnimParts_t::SETANIM_TORSO,animNumber_t::BOTH_ATTACK1,setAnimFlags_t::SETANIM_FLAG_NORMAL);//TORSO_WEAPONIDLE1
                // Select the proper idle Lightsaber attack move from the chart.
                if (pm->ps->saberMove > saberMoveName_t::LS_READY && pm->ps->saberMove < saberMoveName_t::LS_MOVE_MAX) {
                    PM_SetSaberMove(saberMoveData[pm->ps->saberMove].chain_idle);
                }
                else {
                    if (PM_JumpingAnim(pm->ps->legsAnim) || PM_LandingAnim(pm->ps->legsAnim) || PM_InCartwheel(pm->ps->legsAnim) ||
                        PM_FlippingAnim(pm->ps->legsAnim)) {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        if ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) &&
                            pm->ps->torsoAnim == animNumber_t::BOTH_BUTTON_HOLD) { // using something
                            if (!pm->ps->useTime) {                                // stopped holding it, release
                                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_BUTTON_RELEASE,
                                           setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                            } // else still holding, leave it as it is
                        }
                        else {
                            if ((PM_RunningAnim(pm->ps->legsAnim) || pm->ps->legsAnim == animNumber_t::BOTH_WALK_STAFF ||
                                 pm->ps->legsAnim == animNumber_t::BOTH_WALK_DUAL || pm->ps->legsAnim == animNumber_t::BOTH_WALKBACK_STAFF ||
                                 pm->ps->legsAnim == animNumber_t::BOTH_WALKBACK_DUAL) &&
                                pm->ps->saberBlockingTime < cg.time) { // running w/1-handed weapon uses full-body anim
                                setAnimFlags_t setFlags = setAnimFlags_t::SETANIM_FLAG_NORMAL;
                                if (PM_LandingAnim(pm->ps->torsoAnim)) {
                                    setFlags = setAnimFlags_t::SETANIM_FLAG_OVERRIDE;
                                }
                                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setFlags);
                            }
                            else {
                                PM_SetSaberMove(saberMoveName_t::LS_READY);
                            }
                        }
                    }
                }
            }
        }
    }
}

/*
-------------------------
PM_TorsoAnimation
-------------------------
*/

void PM_TorsoAnimation(void)
{ // FIXME: Write a much smarter and more appropriate anim picking routine logic...
    //	animNumber_t	oldAnim;
    if (PM_InKnockDown(pm->ps) || PM_InRoll(pm->ps)) { // in knockdown
        return;
    }

    if (Any(pm->ps->eFlags & entityFlags_t::EF_HELD_BY_WAMPA)) {
        return;
    }

    if (Any(pm->ps->eFlags & entityFlags_t::EF_FORCE_DRAINED)) { // being drained
        // PM_SetAnim( pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_HUGGEE1, setAnimFlags_t::SETANIM_FLAG_OVERRIDE|setAnimFlags_t::SETANIM_FLAG_HOLD );
        return;
    }
    if ((pm->ps->forcePowersActive & Bit(forcePowers_t::FP_DRAIN)) && pm->ps->forceDrainEntityNum < ENTITYNUM_WORLD) { // draining
        // PM_SetAnim( pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_HUGGER1, setAnimFlags_t::SETANIM_FLAG_OVERRIDE|setAnimFlags_t::SETANIM_FLAG_HOLD );
        return;
    }

    if (pm->gent && pm->gent->NPC && Any(pm->gent->NPC->scriptFlags & scriptFlags_t::SCF_FORCED_MARCH)) {
        return;
    }

    if (pm->gent != nullptr && pm->gent->client) {
        pm->gent->client->renderInfo.torsoFpsMod = 1.0f;
    }

    if (pm->gent && pm->ps && Any(pm->ps->eFlags & entityFlags_t::EF_LOCKED_TO_WEAPON)) {
        if (pm->gent->owner && pm->gent->owner->e_UseFunc == useFunc_t::useF_emplaced_gun_use) // ugly way to tell, but...
        {                                                                                      // full body
            PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_GUNSIT1,
                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD); // SETANIM_FLAG_NORMAL
        }
        else { // torso
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_GUNSIT1,
                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD); // SETANIM_FLAG_NORMAL
        }
        return;
    }
    /*	else if ( pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_VEHICLE && pm->ps->clientNum < MAX_CLIENTS &&
       (m_pVehicleInfo[((CVehicleNPC *)pm->gent->NPC)->m_iVehicleTypeID].numHands == 2 || g_speederControlScheme->value == 2) )
        {//can't look around
            PM_SetAnim(pm,setAnimParts_t::SETANIM_TORSO,m_pVehicleInfo[((CVehicleNPC
       *)pm->gent->NPC)->m_iVehicleTypeID].riderAnim,setAnimFlags_t::SETANIM_FLAG_OVERRIDE|setAnimFlags_t::SETANIM_FLAG_HOLD); return;
        }*/

    if (pm->ps->taunting > level.time) {
        if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_ALORA) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_ALORA_TAUNT,
                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD); // SETANIM_FLAG_NORMAL
        }
        else if (pm->ps->weapon == weapon_t::WP_SABER && pm->ps->saberAnimLevel == saber_styles_t::SS_DUAL &&
                 PM_HasAnimation(pm->gent, animNumber_t::BOTH_DUAL_TAUNT)) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_DUAL_TAUNT,
                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD); // SETANIM_FLAG_NORMAL
        }
        else if (pm->ps->weapon == weapon_t::WP_SABER &&
                 pm->ps->saberAnimLevel == saber_styles_t::SS_STAFF) // pm->ps->saber[0].type == saberType_t::SABER_STAFF )
        {                                                            // turn on the blades
            if (PM_HasAnimation(pm->gent, animNumber_t::BOTH_STAFF_TAUNT)) {
                PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_STAFF_TAUNT,
                           setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD); // SETANIM_FLAG_NORMAL
            }
            /*
            else
            {
                if ( !pm->ps->saber[0].blade[0].active )
                {//first blade is off
                    //turn it on
                    pm->ps->SaberBladeActivate( 0, 0, true );
                    if ( !pm->ps->saber[0].blade[1].active )
                    {//second blade is also off, extend time of this taunt so we have enough time to turn them both on
                        pm->ps->taunting = level.time + 3000;
                    }
                }
                else if ( (pm->ps->taunting - level.time) < 1500 )
                {//only 1500ms left in taunt
                    if ( !pm->ps->saber[0].blade[1].active )
                    {//second blade is off
                        //turn it on
                        pm->ps->SaberBladeActivate( 0, 1, true );
                    }
                }
                //pose
                PM_SetAnim(pm,setAnimParts_t::SETANIM_BOTH,animNumber_t::BOTH_SABERSTAFF_STANCE,setAnimFlags_t::SETANIM_FLAG_OVERRIDE|setAnimFlags_t::SETANIM_FLAG_HOLD);
                pm->ps->torsoAnimTimer = pm->ps->legsAnimTimer = (pm->ps->taunting - level.time);
            }
            */
        }
        else if (PM_HasAnimation(pm->gent, animNumber_t::BOTH_GESTURE1)) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_GESTURE1,
                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD); // SETANIM_FLAG_NORMAL
            pm->gent->client->ps.SaberActivateTrail(100);
            // FIXME: will this reset?
            // FIXME: force-control (yellow glow) effect on hand and saber?
        }
        else {
            // PM_SetAnim(pm,setAnimParts_t::SETANIM_TORSO,TORSO_WEAPONIDLE1,setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        return;
    }

    if (pm->ps->weapon == weapon_t::WP_SABER) // WP_LIGHTSABER
    {
        bool saberInAir = false;
        if (pm->ps->SaberLength() && !pm->ps->saberInFlight) {
            PM_TorsoAnimLightsaber();
        }
        else {
            if (pm->ps->forcePowersActive & Bit(forcePowers_t::FP_GRIP) &&
                pm->ps->forcePowerLevel[forcePowers_t::FP_GRIP] > forcePowerLevels_t::FORCE_LEVEL_1) { // holding an enemy aloft with force-grip
                return;
            }
            if (pm->ps->forcePowersActive & Bit(forcePowers_t::FP_LIGHTNING) &&
                pm->ps->forcePowerLevel[forcePowers_t::FP_LIGHTNING] > forcePowerLevels_t::FORCE_LEVEL_1) { // lightning
                return;
            }
            if (pm->ps->forcePowersActive & Bit(forcePowers_t::FP_DRAIN)) { // drain
                return;
            }

            saberInAir = true;

            if (PM_SaberInBrokenParry(pm->ps->saberMove) || pm->ps->saberBlocked == saberBlockedType_t::BLOCKED_PARRY_BROKEN ||
                PM_DodgeAnim(pm->ps->torsoAnim)) { // we're stuck in a broken parry
                PM_TorsoAnimLightsaber();
            }
            else {
                if (pm->ps->saberEntityNum < ENTITYNUM_NONE && pm->ps->saberEntityNum > 0)            // player is 0
                {                                                                                     //
                    if (g_entities[pm->ps->saberEntityNum].s.pos.trType == trType_t::TR_STATIONARY) { // fell to the ground and we're not trying to pull it back
                        saberInAir = false;
                    }
                }

                if (pm->ps->saberInFlight && saberInAir &&
                    (!pm->ps->dualSabers                                         // not using 2 sabers
                     || !pm->ps->saber[1].Active()                               // left one off
                     || pm->ps->torsoAnim == animNumber_t::BOTH_SABERDUAL_STANCE // not attacking
                     || pm->ps->torsoAnim == animNumber_t::BOTH_SABERPULL        // not attacking
                     || pm->ps->torsoAnim == animNumber_t::BOTH_STAND1           // not attacking
                     || PM_RunningAnim(pm->ps->torsoAnim)                        // not attacking
                     || PM_WalkingAnim(pm->ps->torsoAnim)                        // not attacking
                     || PM_JumpingAnim(pm->ps->torsoAnim)                        // not attacking
                     || PM_SwimmingAnim(pm->ps->torsoAnim))                      // not attacking
                ) {
                    if (!PM_ForceAnim(pm->ps->torsoAnim) || pm->ps->torsoAnimTimer < 300) { // don't interrupt a force power anim
                        if (pm->ps->torsoAnim != animNumber_t::BOTH_LOSE_SABER || !pm->ps->torsoAnimTimer) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_SABERPULL,
                                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                        }
                    }
                }
                else {
                    if (PM_InSlopeAnim(pm->ps->legsAnim)) { // HMM... this probably breaks the saber putaway and select anims
                        if (pm->ps->SaberLength() > 0) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    else {
                        if ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) &&
                            pm->ps->torsoAnim == animNumber_t::BOTH_BUTTON_HOLD) { // using something
                            if (!pm->ps->useTime) {                                // stopped holding it, release
                                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_BUTTON_RELEASE,
                                           setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                            } // else still holding, leave it as it is
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                }
            }
        }

        if (pm->ps->weaponTime <= 0 && (pm->ps->saberMove == saberMoveName_t::LS_READY || pm->ps->SaberLength() == 0) && !saberInAir) {
            TorsoAgainstWindTest(pm->gent);
        }
        return;
    }

    if (PM_ForceAnim(pm->ps->torsoAnim) && pm->ps->torsoAnimTimer > 0) { // in a force anim, don't do a stand anim
        return;
    }

    bool weaponBusy = false;

    if (pm->ps->weapon == weapon_t::WP_NONE) {
        weaponBusy = false;
    }
    else if (pm->ps->weaponstate == weaponstate_t::WEAPON_FIRING || pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING ||
             pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING_ALT) {
        weaponBusy = true;
    }
    else if (pm->ps->lastShotTime > level.time - 3000) {
        weaponBusy = true;
    }
    else if (pm->ps->weaponTime > 0) {
        weaponBusy = true;
    }
    else if (pm->gent && pm->gent->client->fireDelay > 0) {
        weaponBusy = true;
    }
    else if (TorsoAgainstWindTest(pm->gent)) {
        return;
    }
    else if ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) && cg.zoomTime > cg.time - 5000) { // if we used binoculars recently, aim weapon
        weaponBusy = true;
        pm->ps->weaponstate = weaponstate_t::WEAPON_IDLE;
    }
    else if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) { // ducking is considered on alert... plus looks stupid to have arms hanging down when crouched
        weaponBusy = true;
    }

    if (pm->ps->weapon == weapon_t::WP_NONE || pm->ps->weaponstate == weaponstate_t::WEAPON_READY || pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING ||
        pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING_ALT) {
        if (pm->ps->weapon == weapon_t::WP_SABER && pm->ps->SaberLength()) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_ATTACK1, setAnimFlags_t::SETANIM_FLAG_NORMAL); // TORSO_WEAPONREADY1
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN1 && !weaponBusy) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN2 && !weaponBusy) //&& pm->ps->saberAnimLevel != saber_styles_t::SS_STAFF )
        {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN4 && !weaponBusy) //&& pm->ps->saberAnimLevel != saber_styles_t::SS_STAFF )
        {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN4, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN_STAFF && !weaponBusy) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN_STAFF, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN_DUAL && !weaponBusy) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN_DUAL, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_WALK1 && !weaponBusy) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WALK1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_WALK2 && !weaponBusy) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WALK2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_WALK_STAFF && !weaponBusy) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WALK_STAFF, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_WALK_DUAL && !weaponBusy) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WALK_DUAL, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_CROUCH1IDLE && pm->ps->clientNum != 0) // player falls through
        {
            //??? Why nothing?  What if you were running???
            // PM_SetAnim(pm,setAnimParts_t::SETANIM_TORSO,animNumber_t::BOTH_CROUCH1IDLE,setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_JUMP1 && !weaponBusy) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_JUMP1, setAnimFlags_t::SETANIM_FLAG_NORMAL, 100); // Only blend over 100ms
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_SWIM_IDLE1 && !weaponBusy) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_SWIM_IDLE1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_SWIMFORWARD && !weaponBusy) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_SWIMFORWARD, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->weapon == weapon_t::WP_NONE) {
            animNumber_t legsAnim = pm->ps->legsAnim;
            /*
            if ( PM_RollingAnim( legsAnim ) ||
                PM_FlippingAnim( legsAnim ) ||
                PM_JumpingAnim( legsAnim ) ||
                PM_PainAnim( legsAnim ) ||
                PM_SwimmingAnim( legsAnim ) )
            */
            {
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            }
        }
        else { // Used to default to both_stand1 which is an arms-down anim
            if ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) && pm->ps->torsoAnim == animNumber_t::BOTH_BUTTON_HOLD) { // using something
                if (!pm->ps->useTime) { // stopped holding it, release
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_BUTTON_RELEASE,
                               setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                } // else still holding, leave it as it is
            }
            else if (pm->gent != nullptr && (pm->gent->s.number < MAX_CLIENTS || G_ControlledByPlayer(pm->gent)) &&
                     pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING &&
                     pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING_ALT) {                           // PLayer- temp hack for weapon frame
                if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_RANCOR) { // ignore
                }
                else if (pm->ps->weapon == weapon_t::WP_MELEE) { // hehe
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND6, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                }
                else {
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                }
            }
            else if (PM_InSpecialJump(
                         pm->ps->legsAnim)) { // use legs anim
                                              // FIXME: or just use whatever's currently playing?
                                              // PM_SetAnim( pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL );
            }
            else {
                switch (pm->ps->weapon) {
                // ********************************************************
                case weapon_t::WP_SABER: // WP_LIGHTSABER
                    // Ready pose for Lightsaber
                    //					PM_SetAnim(pm,setAnimParts_t::SETANIM_TORSO,animNumber_t::BOTH_ATTACK1,setAnimFlags_t::SETANIM_FLAG_NORMAL);//TORSO_WEAPONREADY1
                    // Select the next proper pose for the lightsaber assuming that there are no attacks.
                    if (pm->ps->saberMove > saberMoveName_t::LS_NONE && pm->ps->saberMove < saberMoveName_t::LS_MOVE_MAX) {
                        PM_SetSaberMove(saberMoveData[pm->ps->saberMove].chain_idle);
                    }
                    break;
                    // ********************************************************

                case weapon_t::WP_BRYAR_PISTOL:
                    // FIXME: if recently fired, hold the ready!
                    if (pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING_ALT || weaponBusy) {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else if (PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                             PM_SwimmingAnim(pm->ps->legsAnim)) { // running w/1-handed weapon uses full-body anim
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    break;
                case weapon_t::WP_BLASTER_PISTOL:
                    if (pm->gent && pm->gent->weaponModel[1] > 0) { // dual pistols
                        if (weaponBusy) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_GUNSIT1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else if (PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                                 PM_SwimmingAnim(pm->ps->legsAnim)) { // running w/1-handed weapon uses full-body anim
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND6, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    else { // single pistols
                        if (pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING_ALT || weaponBusy) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else if (PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                                 PM_SwimmingAnim(pm->ps->legsAnim)) { // running w/1-handed weapon uses full-body anim
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    break;
                case weapon_t::WP_NONE:
                    // NOTE: should never get here
                    break;
                case weapon_t::WP_MELEE:
                    if (PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                        PM_SwimmingAnim(pm->ps->legsAnim)) { // running w/1-handed weapon uses full-body anim
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_RANCOR) { // ignore
                        }
                        else if (pm->gent && pm->gent->client && !PM_DroidMelee(pm->gent->client->NPC_class)) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND6, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    break;
                case weapon_t::WP_TUSKEN_STAFF:
                    if (PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                        PM_SwimmingAnim(pm->ps->legsAnim)) { // running w/1-handed weapon uses full-body anim
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    break;

                case weapon_t::WP_NOGHRI_STICK:
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    // PM_SetAnim(pm,setAnimParts_t::SETANIM_LEGS,animNumber_t::BOTH_ATTACK2,setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    break;

                case weapon_t::WP_BLASTER:
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    // PM_SetAnim(pm,setAnimParts_t::SETANIM_LEGS,animNumber_t::BOTH_ATTACK2,setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    break;
                case weapon_t::WP_DISRUPTOR:
                case weapon_t::WP_TUSKEN_RIFLE:
                    if ((pm->ps->weaponstate != weaponstate_t::WEAPON_FIRING && pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING &&
                         pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING_ALT) ||
                        PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                        PM_SwimmingAnim(pm->ps->legsAnim)) { // running sniper weapon uses normal ready
                        if (pm->ps->clientNum) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    else {
                        if (pm->ps->clientNum) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY4,
                                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD); // TORSO_WEAPONREADY4//SETANIM_FLAG_RESTART|
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY4, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    break;
                case weapon_t::WP_BOT_LASER:
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONIDLE2,
                               setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_RESTART | setAnimFlags_t::SETANIM_FLAG_HOLD);
                    break;
                case weapon_t::WP_THERMAL:
                    if (pm->ps->weaponstate != weaponstate_t::WEAPON_FIRING && pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING &&
                        pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING_ALT &&
                        (PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                         PM_SwimmingAnim(pm->ps->legsAnim))) { // running w/1-handed weapon uses full-body anim
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        if ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) &&
                            (pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING ||
                             pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING_ALT)) { // player pulling back to throw
                            if (PM_StandingAnim(pm->ps->legsAnim)) {
                                PM_SetAnim(pm, setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_THERMAL_READY,
                                           setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                            }
                            else if (pm->ps->legsAnim == animNumber_t::BOTH_THERMAL_READY) { // sigh... hold it so pm_footsteps doesn't override
                                if (pm->ps->legsAnimTimer < 100) {
                                    pm->ps->legsAnimTimer = 100;
                                }
                            }
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_THERMAL_READY,
                                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                        }
                        else {
                            if (weaponBusy) {
                                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY10, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                            }
                            else {
                                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                            }
                        }
                    }
                    break;
                case weapon_t::WP_REPEATER:
                    if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_GALAKMECH) { //
                        if (pm->gent->alt_fire) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    else {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    break;
                case weapon_t::WP_TRIP_MINE:
                case weapon_t::WP_DET_PACK:
                    if (PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                        PM_SwimmingAnim(pm->ps->legsAnim)) { // running w/1-handed weapon uses full-body anim
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        if (weaponBusy) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    break;
                default:
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    break;
                }
            }
        }
    }
    else if (pm->ps->weaponstate == weaponstate_t::WEAPON_IDLE) {
        if (pm->ps->legsAnim == animNumber_t::BOTH_GUARD_LOOKAROUND1) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_GUARD_LOOKAROUND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_GUARD_IDLE1) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_GUARD_IDLE1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_STAND1IDLE1 || pm->ps->legsAnim == animNumber_t::BOTH_STAND2IDLE1 ||
                 pm->ps->legsAnim == animNumber_t::BOTH_STAND2IDLE2 || pm->ps->legsAnim == animNumber_t::BOTH_STAND3IDLE1 ||
                 pm->ps->legsAnim == animNumber_t::BOTH_STAND5IDLE1) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_STAND2TO4) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND2TO4, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_STAND4TO2) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND4TO2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_STAND4) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND4, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_SWIM_IDLE1) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_SWIM_IDLE1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_SWIMFORWARD) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_SWIMFORWARD, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (PM_InSpecialJump(
                     pm->ps->legsAnim)) { // use legs anim
                                          // FIXME: or just use whatever's currently playing?
                                          // PM_SetAnim( pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL );
        }
        else if ((pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer()) && pm->ps->torsoAnim == animNumber_t::BOTH_BUTTON_HOLD) { // using something
            if (!pm->ps->useTime) { // stopped holding it, release
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_BUTTON_RELEASE,
                           setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
            } // else still holding, leave it as it is
        }
        else {
            if (!weaponBusy && pm->ps->weapon != weapon_t::WP_BOWCASTER && pm->ps->weapon != weapon_t::WP_REPEATER &&
                pm->ps->weapon != weapon_t::WP_FLECHETTE && pm->ps->weapon != weapon_t::WP_ROCKET_LAUNCHER && pm->ps->weapon != weapon_t::WP_CONCUSSION &&
                (PM_RunningAnim(pm->ps->legsAnim) || (PM_WalkingAnim(pm->ps->legsAnim) && (pm->ps->clientNum < MAX_CLIENTS || PM_ControlledByPlayer())) ||
                 PM_JumpingAnim(pm->ps->legsAnim) ||
                 PM_SwimmingAnim(
                     pm->ps->legsAnim))) { // running w/1-handed or light 2-handed weapon uses full-body anim if you're not using the weapon right now
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            }
            else {
                switch (pm->ps->weapon) {
                // ********************************************************
                case weapon_t::WP_SABER: // WP_LIGHTSABER
                    // Shouldn't get here, should go to TorsoAnimLightsaber
                    break;
                    // ********************************************************

                case weapon_t::WP_BRYAR_PISTOL:
                    if (pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING_ALT || weaponBusy) {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else if (PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                             PM_SwimmingAnim(pm->ps->legsAnim)) { // running w/1-handed weapon uses full-body anim
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONIDLE2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    break;
                case weapon_t::WP_BLASTER_PISTOL:
                    if (pm->gent && pm->gent->weaponModel[1] > 0) { // dual pistols
                        if (weaponBusy) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_GUNSIT1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else if (PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                                 PM_SwimmingAnim(pm->ps->legsAnim)) { // running w/1-handed weapon uses full-body anim
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    else { // single pistols
                        if (pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING_ALT || weaponBusy) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else if (PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                                 PM_SwimmingAnim(pm->ps->legsAnim)) { // running w/1-handed weapon uses full-body anim
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONIDLE2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    break;

                case weapon_t::WP_NONE:
                    // NOTE: should never get here
                    break;

                case weapon_t::WP_MELEE:
                    if (PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                        PM_SwimmingAnim(pm->ps->legsAnim)) { // running w/1-handed weapon uses full-body anim
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_RANCOR) { // ignore
                        }
                        else if (pm->gent && pm->gent->client && !PM_DroidMelee(pm->gent->client->NPC_class)) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND6, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    break;

                case weapon_t::WP_TUSKEN_STAFF:
                    if (PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                        PM_SwimmingAnim(pm->ps->legsAnim)) { // running w/1-handed weapon uses full-body anim
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    break;

                case weapon_t::WP_NOGHRI_STICK:
                    if (weaponBusy) {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONIDLE3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    break;

                case weapon_t::WP_BLASTER:
                    if (weaponBusy) {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONIDLE3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    break;

                case weapon_t::WP_DISRUPTOR:
                case weapon_t::WP_TUSKEN_RIFLE:
                    if ((pm->ps->weaponstate != weaponstate_t::WEAPON_FIRING && pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING &&
                         pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING_ALT) ||
                        PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                        PM_SwimmingAnim(pm->ps->legsAnim)) { // running sniper weapon uses normal ready
                        if (pm->ps->clientNum) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    else {
                        if (pm->ps->clientNum) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY4, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY4, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    break;

                case weapon_t::WP_BOT_LASER:
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONIDLE2,
                               setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_RESTART | setAnimFlags_t::SETANIM_FLAG_HOLD);
                    break;

                case weapon_t::WP_THERMAL:
                    if (PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                        PM_SwimmingAnim(pm->ps->legsAnim)) { // running w/1-handed weapon uses full-body anim
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        if (weaponBusy) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONIDLE10, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    break;

                case weapon_t::WP_REPEATER:
                    if (weaponBusy) {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONIDLE3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    break;
                case weapon_t::WP_TRIP_MINE:
                case weapon_t::WP_DET_PACK:
                    if (PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                        PM_SwimmingAnim(pm->ps->legsAnim)) { // running w/1-handed weapon uses full-body anim
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        if (weaponBusy) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONIDLE3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    break;

                default:
                    if (weaponBusy) {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONIDLE3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    break;
                }
            }
        }
    }
}

//=========================================================================
// Anim checking utils
//=========================================================================

std::optional<animNumber_t> PM_GetTurnAnim(gentity_t* gent, animNumber_t anim)
{
    if (!gent) {
        return std::nullopt;
    }

    switch (anim) {
    case animNumber_t::BOTH_STAND1:      // # Standing idle: no weapon: hands down
    case animNumber_t::BOTH_STAND1IDLE1: // # Random standing idle
    case animNumber_t::BOTH_STAND2:      // # Standing idle with a weapon
    case animNumber_t::BOTH_SABERFAST_STANCE:
    case animNumber_t::BOTH_SABERSLOW_STANCE:
    case animNumber_t::BOTH_STAND2IDLE1: // # Random standing idle
    case animNumber_t::BOTH_STAND2IDLE2: // # Random standing idle
    case animNumber_t::BOTH_STAND3:      // # Standing hands behind back: at ease: etc.
    case animNumber_t::BOTH_STAND3IDLE1: // # Random standing idle
    case animNumber_t::BOTH_STAND4:      // # two handed: gun down: relaxed stand
    case animNumber_t::BOTH_STAND5:      // # standing idle, no weapon, hand down, back straight
    case animNumber_t::BOTH_STAND5IDLE1: // # Random standing idle
    case animNumber_t::BOTH_STAND6:      // # one handed: gun at side: relaxed stand
    case animNumber_t::BOTH_STAND2TO4:   // # Transition from stand2 to stand4
    case animNumber_t::BOTH_STAND4TO2:   // # Transition from stand4 to stand2
    case animNumber_t::BOTH_GESTURE1:    // # Generic gesture: non-specific
    case animNumber_t::BOTH_GESTURE2:    // # Generic gesture: non-specific
    case animNumber_t::BOTH_TALK1:       // # Generic talk anim
    case animNumber_t::BOTH_TALK2:       // # Generic talk anim
        if (PM_HasAnimation(gent, animNumber_t::LEGS_TURN1)) {
            return animNumber_t::LEGS_TURN1;
        }
        else {
            return std::nullopt;
        }
        break;
    case animNumber_t::BOTH_ATTACK1:           // # Attack with generic 1-handed weapon
    case animNumber_t::BOTH_ATTACK2:           // # Attack with generic 2-handed weapon
    case animNumber_t::BOTH_ATTACK3:           // # Attack with heavy 2-handed weapon
    case animNumber_t::BOTH_ATTACK4:           // # Attack with ???
    case animNumber_t::BOTH_MELEE1:            // # First melee attack
    case animNumber_t::BOTH_MELEE2:            // # Second melee attack
    case animNumber_t::BOTH_GUARD_LOOKAROUND1: // # Cradling weapon and looking around
    case animNumber_t::BOTH_GUARD_IDLE1:       // # Cradling weapon and standing
        if (PM_HasAnimation(gent, animNumber_t::LEGS_TURN2)) {
            return animNumber_t::LEGS_TURN2;
        }
        else {
            return std::nullopt;
        }
        break;
    default:
        return std::nullopt;
        break;
    }
}

std::optional<animNumber_t> PM_TurnAnimForLegsAnim(gentity_t* gent, animNumber_t anim)
{
    if (!gent) {
        return std::nullopt;
    }

    switch (anim) {
    case animNumber_t::BOTH_STAND1:      // # Standing idle: no weapon: hands down
    case animNumber_t::BOTH_STAND1IDLE1: // # Random standing idle
        if (PM_HasAnimation(gent, animNumber_t::BOTH_TURNSTAND1)) {
            return animNumber_t::BOTH_TURNSTAND1;
        }
        else {
            return std::nullopt;
        }
        break;
    case animNumber_t::BOTH_STAND2: // # Standing idle with a weapon
    case animNumber_t::BOTH_SABERFAST_STANCE:
    case animNumber_t::BOTH_SABERSLOW_STANCE:
    case animNumber_t::BOTH_STAND2IDLE1: // # Random standing idle
    case animNumber_t::BOTH_STAND2IDLE2: // # Random standing idle
        if (PM_HasAnimation(gent, animNumber_t::BOTH_TURNSTAND2)) {
            return animNumber_t::BOTH_TURNSTAND2;
        }
        else {
            return std::nullopt;
        }
        break;
    case animNumber_t::BOTH_STAND3:      // # Standing hands behind back: at ease: etc.
    case animNumber_t::BOTH_STAND3IDLE1: // # Random standing idle
        if (PM_HasAnimation(gent, animNumber_t::BOTH_TURNSTAND3)) {
            return animNumber_t::BOTH_TURNSTAND3;
        }
        else {
            return std::nullopt;
        }
        break;
    case animNumber_t::BOTH_STAND4: // # two handed: gun down: relaxed stand
        if (PM_HasAnimation(gent, animNumber_t::BOTH_TURNSTAND4)) {
            return animNumber_t::BOTH_TURNSTAND4;
        }
        else {
            return std::nullopt;
        }
        break;
    case animNumber_t::BOTH_STAND5:      // # standing idle, no weapon, hand down, back straight
    case animNumber_t::BOTH_STAND5IDLE1: // # Random standing idle
        if (PM_HasAnimation(gent, animNumber_t::BOTH_TURNSTAND5)) {
            return animNumber_t::BOTH_TURNSTAND5;
        }
        else {
            return std::nullopt;
        }
        break;
    case animNumber_t::BOTH_CROUCH1:     // # Transition from standing to crouch
    case animNumber_t::BOTH_CROUCH1IDLE: // # Crouching idle
        /*
        case animNumber_t::BOTH_UNCROUCH1:			//# Transition from crouch to standing
        case animNumber_t::BOTH_CROUCH2TOSTAND1:	//# going from crouch2 to stand1
        case animNumber_t::BOTH_CROUCH3:			//# Desann crouching down to Kyle (cin 9)
        case animNumber_t::BOTH_UNCROUCH3:			//# Desann uncrouching down to Kyle (cin 9)
        case animNumber_t::BOTH_CROUCH4:			//# Slower version of crouch1 for cinematics
        case animNumber_t::BOTH_UNCROUCH4:			//# Slower version of uncrouch1 for cinematics
        */
        if (PM_HasAnimation(gent, animNumber_t::BOTH_TURNCROUCH1)) {
            return animNumber_t::BOTH_TURNCROUCH1;
        }
        else {
            return std::nullopt;
        }
        break;
    default:
        return std::nullopt;
        break;
    }
}

bool PM_InOnGroundAnim(playerState_t* ps)
{
    switch (ps->legsAnim) {
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
    case animNumber_t::BOTH_SLEEP1: // # laying on back-rknee up-rhand on torso
        return true;
        break;
    case animNumber_t::BOTH_KNOCKDOWN1: // #
    case animNumber_t::BOTH_KNOCKDOWN2: // #
    case animNumber_t::BOTH_KNOCKDOWN3: // #
    case animNumber_t::BOTH_KNOCKDOWN4: // #
    case animNumber_t::BOTH_KNOCKDOWN5: // #
    case animNumber_t::BOTH_LK_DL_ST_T_SB_1_L:
    case animNumber_t::BOTH_RELEASED:
        if (ps->legsAnimTimer < 500) { // pretty much horizontal by this point
            return true;
        }
        break;
    case animNumber_t::BOTH_PLAYER_PA_3_FLY:
        if (ps->legsAnimTimer < 300) { // pretty much horizontal by this point
            return true;
        }
        /*
        else if ( ps->clientNum < MAX_CLIENTS
            && ps->legsAnimTimer < 300 + PLAYER_KNOCKDOWN_HOLD_EXTRA_TIME )
        {
            return true;
        }
        */
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
        if (ps->legsAnimTimer >
            PM_AnimLength(g_entities[ps->clientNum].client->clientInfo.animFileIndex, ps->legsAnim) - 400) { // still pretty much horizontal at this point
            return true;
        }
        break;
    default:
        break;
    }

    return false;
}

bool PM_InSpecialDeathAnim(animNumber_t anim)
{
    switch (pm->ps->legsAnim) {
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

bool PM_InDeathAnim(void)
{ // Purposely does not cover stumbledeath and falldeath...
    switch (pm->ps->legsAnim) {
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
        return PM_InSpecialDeathAnim(pm->ps->legsAnim);
        break;
    }
}

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

bool PM_InButterfly(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_BUTTERFLY_LEFT:
    case animNumber_t::BOTH_BUTTERFLY_RIGHT:
    case animNumber_t::BOTH_BUTTERFLY_FL1:
    case animNumber_t::BOTH_BUTTERFLY_FR1:
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool PM_StandingAnim(animNumber_t anim)
{ // NOTE: does not check idles or special (cinematic) stands
    switch (anim) {
    case animNumber_t::BOTH_STAND1:
    case animNumber_t::BOTH_STAND2:
    case animNumber_t::BOTH_STAND3:
    case animNumber_t::BOTH_STAND4:
    case animNumber_t::BOTH_ATTACK3:
        return true;
        break;
    default:
        break;
    }
    return false;
}

bool PM_InAirKickingAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_A7_KICK_F_AIR:
    case animNumber_t::BOTH_A7_KICK_B_AIR:
    case animNumber_t::BOTH_A7_KICK_R_AIR:
    case animNumber_t::BOTH_A7_KICK_L_AIR:
        return true;
    default:
        break;
    }
    return false;
}

bool PM_KickingAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_A7_KICK_F:
    case animNumber_t::BOTH_A7_KICK_B:
    case animNumber_t::BOTH_A7_KICK_R:
    case animNumber_t::BOTH_A7_KICK_L:
    case animNumber_t::BOTH_A7_KICK_S:
    case animNumber_t::BOTH_A7_KICK_BF:
    case animNumber_t::BOTH_A7_KICK_RL:
    // NOT a kick, but acts like one:
    case animNumber_t::BOTH_A7_HILT:
    // NOT kicks, but do kick traces anyway
    case animNumber_t::BOTH_GETUP_BROLL_B:
    case animNumber_t::BOTH_GETUP_BROLL_F:
    case animNumber_t::BOTH_GETUP_FROLL_B:
    case animNumber_t::BOTH_GETUP_FROLL_F:
        return true;
        break;
    default:
        return PM_InAirKickingAnim(anim);
        break;
    }
    // return false;
}

bool PM_StabDownAnim(animNumber_t anim)
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

bool PM_GoingToAttackDown(playerState_t* ps)
{
    if (PM_StabDownAnim(ps->torsoAnim)                                                                            // stabbing downward
        || ps->saberMove == saberMoveName_t::LS_A_LUNGE                                                           // lunge
        || ps->saberMove == saberMoveName_t::LS_A_JUMP_T__B_                                                      // death from above
        || ps->saberMove == saberMoveName_t::LS_A_T2B                                                             // attacking top to bottom
        || ps->saberMove == saberMoveName_t::LS_S_T2B                                                             // starting at attack downward
        || (PM_SaberInTransition(ps->saberMove) && saberMoveData[ps->saberMove].endQuad == saberQuadrant_t::Q_T)) // transitioning to a top to bottom attack
    {
        return true;
    }
    return false;
}

bool PM_ForceUsingSaberAnim(animNumber_t anim)
{ // saber/acrobatic anims that should prevent you from recharging force power while you're in them...
    switch (anim) {
    case animNumber_t::BOTH_JUMPFLIPSLASHDOWN1:
    case animNumber_t::BOTH_JUMPFLIPSTABDOWN:
    case animNumber_t::BOTH_FORCELEAP2_T__B_:
    case animNumber_t::BOTH_JUMPATTACK6:
    case animNumber_t::BOTH_JUMPATTACK7:
    case animNumber_t::BOTH_FORCELONGLEAP_START:
    case animNumber_t::BOTH_FORCELONGLEAP_ATTACK:
    case animNumber_t::BOTH_FORCEWALLRUNFLIP_START:
    case animNumber_t::BOTH_FORCEWALLRUNFLIP_END:
    case animNumber_t::BOTH_FORCEWALLRUNFLIP_ALT:
    case animNumber_t::BOTH_FORCEWALLREBOUND_FORWARD:
    case animNumber_t::BOTH_FORCEWALLREBOUND_LEFT:
    case animNumber_t::BOTH_FORCEWALLREBOUND_BACK:
    case animNumber_t::BOTH_FORCEWALLREBOUND_RIGHT:
    case animNumber_t::BOTH_FLIP_ATTACK7:
    case animNumber_t::BOTH_FLIP_HOLD7:
    case animNumber_t::BOTH_FLIP_LAND:
    case animNumber_t::BOTH_PULL_IMPALE_STAB:
    case animNumber_t::BOTH_PULL_IMPALE_SWING:
    case animNumber_t::BOTH_A6_SABERPROTECT:
    case animNumber_t::BOTH_A7_SOULCAL:
    case animNumber_t::BOTH_A1_SPECIAL:
    case animNumber_t::BOTH_A2_SPECIAL:
    case animNumber_t::BOTH_A3_SPECIAL:
    case animNumber_t::BOTH_ARIAL_LEFT:
    case animNumber_t::BOTH_ARIAL_RIGHT:
    case animNumber_t::BOTH_CARTWHEEL_LEFT:
    case animNumber_t::BOTH_CARTWHEEL_RIGHT:
    case animNumber_t::BOTH_FLIP_LEFT:
    case animNumber_t::BOTH_FLIP_BACK1:
    case animNumber_t::BOTH_FLIP_BACK2:
    case animNumber_t::BOTH_FLIP_BACK3:
    case animNumber_t::BOTH_ALORA_FLIP_B:
    case animNumber_t::BOTH_BUTTERFLY_LEFT:
    case animNumber_t::BOTH_BUTTERFLY_RIGHT:
    case animNumber_t::BOTH_BUTTERFLY_FL1:
    case animNumber_t::BOTH_BUTTERFLY_FR1:
    case animNumber_t::BOTH_WALL_RUN_RIGHT:
    case animNumber_t::BOTH_WALL_RUN_RIGHT_FLIP:
    case animNumber_t::BOTH_WALL_RUN_RIGHT_STOP:
    case animNumber_t::BOTH_WALL_RUN_LEFT:
    case animNumber_t::BOTH_WALL_RUN_LEFT_FLIP:
    case animNumber_t::BOTH_WALL_RUN_LEFT_STOP:
    case animNumber_t::BOTH_WALL_FLIP_RIGHT:
    case animNumber_t::BOTH_WALL_FLIP_LEFT:
    case animNumber_t::BOTH_FORCEJUMP1:
    case animNumber_t::BOTH_FORCEINAIR1:
    case animNumber_t::BOTH_FORCELAND1:
    case animNumber_t::BOTH_FORCEJUMPBACK1:
    case animNumber_t::BOTH_FORCEINAIRBACK1:
    case animNumber_t::BOTH_FORCELANDBACK1:
    case animNumber_t::BOTH_FORCEJUMPLEFT1:
    case animNumber_t::BOTH_FORCEINAIRLEFT1:
    case animNumber_t::BOTH_FORCELANDLEFT1:
    case animNumber_t::BOTH_FORCEJUMPRIGHT1:
    case animNumber_t::BOTH_FORCEINAIRRIGHT1:
    case animNumber_t::BOTH_FORCELANDRIGHT1:
    case animNumber_t::BOTH_FLIP_F:
    case animNumber_t::BOTH_FLIP_B:
    case animNumber_t::BOTH_FLIP_L:
    case animNumber_t::BOTH_FLIP_R:
    case animNumber_t::BOTH_ALORA_FLIP_1:
    case animNumber_t::BOTH_ALORA_FLIP_2:
    case animNumber_t::BOTH_ALORA_FLIP_3:
    case animNumber_t::BOTH_DODGE_FL:
    case animNumber_t::BOTH_DODGE_FR:
    case animNumber_t::BOTH_DODGE_BL:
    case animNumber_t::BOTH_DODGE_BR:
    case animNumber_t::BOTH_DODGE_L:
    case animNumber_t::BOTH_DODGE_R:
    case animNumber_t::BOTH_DODGE_HOLD_FL:
    case animNumber_t::BOTH_DODGE_HOLD_FR:
    case animNumber_t::BOTH_DODGE_HOLD_BL:
    case animNumber_t::BOTH_DODGE_HOLD_BR:
    case animNumber_t::BOTH_DODGE_HOLD_L:
    case animNumber_t::BOTH_DODGE_HOLD_R:
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
    case animNumber_t::BOTH_WALL_FLIP_BACK1:
    case animNumber_t::BOTH_WALL_FLIP_BACK2:
    case animNumber_t::BOTH_SPIN1:
    case animNumber_t::BOTH_FJSS_TR_BL:
    case animNumber_t::BOTH_FJSS_TL_BR:
    case animNumber_t::BOTH_DEFLECTSLASH__R__L_FIN:
    case animNumber_t::BOTH_ARIAL_F1:
        return true;
    default:
        break;
    }
    return false;
}

bool G_HasKnockdownAnims(gentity_t* ent)
{
    if (PM_HasAnimation(ent, animNumber_t::BOTH_KNOCKDOWN1) && PM_HasAnimation(ent, animNumber_t::BOTH_KNOCKDOWN2) &&
        PM_HasAnimation(ent, animNumber_t::BOTH_KNOCKDOWN3) && PM_HasAnimation(ent, animNumber_t::BOTH_KNOCKDOWN4) &&
        PM_HasAnimation(ent, animNumber_t::BOTH_KNOCKDOWN5)) {
        return true;
    }
    return false;
}

bool PM_InAttackRoll(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_GETUP_BROLL_B:
    case animNumber_t::BOTH_GETUP_BROLL_F:
    case animNumber_t::BOTH_GETUP_FROLL_B:
    case animNumber_t::BOTH_GETUP_FROLL_F:
        return true;
    default:
        break;
    }
    return false;
}

bool PM_LockedAnim(animNumber_t anim)
{ // anims that can *NEVER* be overridden, regardless
    switch (anim) {
    case animNumber_t::BOTH_KYLE_PA_1:
    case animNumber_t::BOTH_KYLE_PA_2:
    case animNumber_t::BOTH_KYLE_PA_3:
    case animNumber_t::BOTH_PLAYER_PA_1:
    case animNumber_t::BOTH_PLAYER_PA_2:
    case animNumber_t::BOTH_PLAYER_PA_3:
    case animNumber_t::BOTH_PLAYER_PA_3_FLY:
    case animNumber_t::BOTH_TAVION_SCEPTERGROUND:
    case animNumber_t::BOTH_TAVION_SWORDPOWER:
    case animNumber_t::BOTH_SCEPTER_START:
    case animNumber_t::BOTH_SCEPTER_HOLD:
    case animNumber_t::BOTH_SCEPTER_STOP:
    // grabbed by wampa
    case animNumber_t::BOTH_GRABBED:     // #
    case animNumber_t::BOTH_RELEASED:    // #	when Wampa drops player, transitions into fall on back
    case animNumber_t::BOTH_HANG_IDLE:   // #
    case animNumber_t::BOTH_HANG_ATTACK: // #
    case animNumber_t::BOTH_HANG_PAIN:   // #
        return true;
    default:
        break;
    }
    return false;
}

bool PM_SuperBreakLoseAnim(animNumber_t anim)
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

bool PM_SuperBreakWinAnim(animNumber_t anim)
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

bool PM_SaberLockBreakAnim(animNumber_t anim)
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
        return (PM_SuperBreakLoseAnim(anim) || PM_SuperBreakWinAnim(anim));
        break;
    default:
        break;
    }
    return false;
}

bool PM_GetupAnimNoMove(animNumber_t legsAnim)
{
    switch (legsAnim) {
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
        return true;
    default:
        break;
    }
    return false;
}

bool PM_KnockDownAnim(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_KNOCKDOWN1:
    case animNumber_t::BOTH_KNOCKDOWN2:
    case animNumber_t::BOTH_KNOCKDOWN3:
    case animNumber_t::BOTH_KNOCKDOWN4:
    case animNumber_t::BOTH_KNOCKDOWN5:
        /*
        //special anims:
        case animNumber_t::BOTH_RELEASED:
        case animNumber_t::BOTH_LK_DL_ST_T_SB_1_L:
        case animNumber_t::BOTH_PLAYER_PA_3_FLY:
        */
        return true;
        break;
    /*
    default:
        return PM_InGetUp( ps );
        break;
    */
    default:
        break;
    }
    return false;
}

bool PM_KnockDownAnimExtended(animNumber_t anim)
{
    switch (anim) {
    case animNumber_t::BOTH_KNOCKDOWN1:
    case animNumber_t::BOTH_KNOCKDOWN2:
    case animNumber_t::BOTH_KNOCKDOWN3:
    case animNumber_t::BOTH_KNOCKDOWN4:
    case animNumber_t::BOTH_KNOCKDOWN5:
    // special anims:
    case animNumber_t::BOTH_RELEASED:
    case animNumber_t::BOTH_LK_DL_ST_T_SB_1_L:
    case animNumber_t::BOTH_PLAYER_PA_3_FLY:
        return true;
        break;
    /*
    default:
        return PM_InGetUp( ps );
        break;
    */
    default:
        break;
    }
    return false;
}

bool PM_SaberInKata(saberMoveName_t saberMove)
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

bool PM_CanRollFromSoulCal(playerState_t* ps)
{
    if (ps->legsAnim == animNumber_t::BOTH_A7_SOULCAL && ps->legsAnimTimer < 700 && ps->legsAnimTimer > 250) {
        return true;
    }
    return false;
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

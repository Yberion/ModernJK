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

// define GAME_INCLUDE so that g_public.h does not define the
// short, server-visible gclient_t and gentity_t structures,
// because we define the full size ones in this file
#define GAME_INCLUDE
#include <code/qcommon/q_shared.h>
#include "b_local.h"
#include "g_shared.h"
#include "bg_local.h"
#include <cgame/cg_local.h>
#include "anims.h"
#include "Q3_Interface.h"
#include "g_local.h"
#include "wp_saber.h"

extern pmove_t* pm;
extern pml_t pml;
extern cvar_t* g_timescale;
extern cvar_t* g_synchSplitAnims;
extern cvar_t* g_saberAnimSpeed;
extern cvar_t* g_saberAutoAim;

extern bool InFront(vec3_t spot, vec3_t from, vec3_t fromAngles, float threshHold = 0.0f);
extern void G_SoundOnEnt(gentity_t* ent, sound::SoundClass soundClass, sound::Slot slot, const char* soundPath);

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

int PM_AnimLength(int index, animNumber_t anim);
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

EnumArray<saberMoveData_t, saberMoveName_t, saberMoveName_t::LS_MOVE_MAX> saberMoveData = {{
    //							NB:randomized
    // name			anim				startQ	endQ	setanimflag		blend,	blocking	chain_idle		chain_attack	trailLen
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
    {"Lunge Att", animNumber_t::BOTH_LUNGE2_B__T_, saberQuadrant_t::Q_B, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_A_LUNGE
    {"Jump Att", animNumber_t::BOTH_FORCELEAP2_T__B_, saberQuadrant_t::Q_T, saberQuadrant_t::Q_B, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_READY, 200}, // LS_A_JUMP_T__B_
    {"Flip Stab", animNumber_t::BOTH_JUMPFLIPSTABDOWN, saberQuadrant_t::Q_R, saberQuadrant_t::Q_T, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_T1_T___R, 200}, // LS_A_FLIP_STAB
    {"Flip Slash", animNumber_t::BOTH_JUMPFLIPSLASHDOWN1, saberQuadrant_t::Q_L, saberQuadrant_t::Q_R, AFLAG_ACTIVE, 100, saberBlockType_t::BLK_TIGHT,
     saberMoveName_t::LS_READY, saberMoveName_t::LS_T1__R_T_, 200}, // LS_A_FLIP_SLASH

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
         saberMoveName_t::LS_T1_BR_TR,
         saberMoveName_t::LS_T1_BR_T_,
         saberMoveName_t::LS_T1_BR_TL,
         saberMoveName_t::LS_T1_BR__L,
         saberMoveName_t::LS_T1_BR_BL,
         saberMoveName_t::LS_NONE, // No transitions to bottom, and no anims start there, so shouldn't need any
     },
     {
         saberMoveName_t::LS_T1__R_BR, // 46
         saberMoveName_t::LS_NONE,     // Can't transition to same pos!
         saberMoveName_t::LS_T1__R_TR,
         saberMoveName_t::LS_T1__R_T_,
         saberMoveName_t::LS_T1__R_TL,
         saberMoveName_t::LS_T1__R__L,
         saberMoveName_t::LS_T1__R_BL,
         saberMoveName_t::LS_NONE, // No transitions to bottom, and no anims start there, so shouldn't need any
     },
     {
         saberMoveName_t::LS_T1_TR_BR, // 52
         saberMoveName_t::LS_T1_TR__R,
         saberMoveName_t::LS_NONE, // Can't transition to same pos!
         saberMoveName_t::LS_T1_TR_T_,
         saberMoveName_t::LS_T1_TR_TL,
         saberMoveName_t::LS_T1_TR__L,
         saberMoveName_t::LS_T1_TR_BL,
         saberMoveName_t::LS_NONE, // No transitions to bottom, and no anims start there, so shouldn't need any
     },
     {
         saberMoveName_t::LS_T1_T__BR, // 58
         saberMoveName_t::LS_T1_T___R,
         saberMoveName_t::LS_T1_T__TR,
         saberMoveName_t::LS_NONE, // Can't transition to same pos!
         saberMoveName_t::LS_T1_T__TL,
         saberMoveName_t::LS_T1_T___L,
         saberMoveName_t::LS_T1_T__BL,
         saberMoveName_t::LS_NONE, // No transitions to bottom, and no anims start there, so shouldn't need any
     },
     {
         saberMoveName_t::LS_T1_TL_BR, // 64
         saberMoveName_t::LS_T1_TL__R,
         saberMoveName_t::LS_T1_TL_TR,
         saberMoveName_t::LS_T1_TL_T_,
         saberMoveName_t::LS_NONE, // Can't transition to same pos!
         saberMoveName_t::LS_T1_TL__L,
         saberMoveName_t::LS_T1_TL_BL,
         saberMoveName_t::LS_NONE, // No transitions to bottom, and no anims start there, so shouldn't need any
     },
     {
         saberMoveName_t::LS_T1__L_BR, // 70
         saberMoveName_t::LS_T1__L__R,
         saberMoveName_t::LS_T1__L_TR,
         saberMoveName_t::LS_T1__L_T_,
         saberMoveName_t::LS_T1__L_TL,
         saberMoveName_t::LS_NONE, // Can't transition to same pos!
         saberMoveName_t::LS_T1__L_BL,
         saberMoveName_t::LS_NONE, // No transitions to bottom, and no anims start there, so shouldn't need any
     },
     {
         saberMoveName_t::LS_T1_BL_BR, // 76
         saberMoveName_t::LS_T1_BL__R,
         saberMoveName_t::LS_T1_BL_TR,
         saberMoveName_t::LS_T1_BL_T_,
         saberMoveName_t::LS_T1_BL_TL,
         saberMoveName_t::LS_T1_BL__L,
         saberMoveName_t::LS_NONE, // Can't transition to same pos!
         saberMoveName_t::LS_NONE, // No transitions to bottom, and no anims start there, so shouldn't need any
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

forcePowerLevels_t PM_AnimLevelForSaberAnim(animNumber_t anim)
{
    if (anim >= animNumber_t::BOTH_A1_T__B_ && anim <= animNumber_t::BOTH_D1_B____) {
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
        return forcePowerLevels_t::FORCE_LEVEL_5;
    }
    return forcePowerLevels_t::FORCE_LEVEL_0;
}

forcePowerLevels_t PM_PowerLevelForSaberAnim(playerState_t* ps)
{
    animNumber_t anim = ps->torsoAnim;
    if (anim >= animNumber_t::BOTH_A1_T__B_ && anim <= animNumber_t::BOTH_D1_B____) {
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
    if (anim >= animNumber_t::BOTH_P1_S1_T_ && anim <= animNumber_t::BOTH_H1_S1_BR) { // parries, knockaways and broken parries
        return forcePowerLevels_t::FORCE_LEVEL_1;                                     // FIXME: saberAnimLevel?
    }
    switch (anim) {
    case animNumber_t::BOTH_A2_STABBACK1:
    case animNumber_t::BOTH_ATTACK_BACK:
    case animNumber_t::BOTH_CROUCHATTACKBACK1:
    case animNumber_t::BOTH_BUTTERFLY_LEFT:
    case animNumber_t::BOTH_BUTTERFLY_RIGHT:
    case animNumber_t::BOTH_FJSS_TR_BL:
    case animNumber_t::BOTH_FJSS_TL_BR:
    case animNumber_t::BOTH_K1_S1_T_: // # knockaway saber top
    case animNumber_t::BOTH_K1_S1_TR: // # knockaway saber top right
    case animNumber_t::BOTH_K1_S1_TL: // # knockaway saber top left
    case animNumber_t::BOTH_K1_S1_BL: // # knockaway saber bottom left
    case animNumber_t::BOTH_K1_S1_B_: // # knockaway saber bottom
    case animNumber_t::BOTH_K1_S1_BR: // # knockaway saber bottom right
    case animNumber_t::BOTH_LUNGE2_B__T_:
    case animNumber_t::BOTH_FORCELEAP2_T__B_:
        return forcePowerLevels_t::FORCE_LEVEL_3;
        break;
    case animNumber_t::BOTH_JUMPFLIPSLASHDOWN1: // FIXME: only middle of anim should have any power
    case animNumber_t::BOTH_JUMPFLIPSTABDOWN:   // FIXME: only middle of anim should have any power
        if (ps->torsoAnimTimer <= 300) {        // end of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        else if (PM_AnimLength(g_entities[ps->clientNum].client->clientInfo.animFileIndex, anim) - ps->torsoAnimTimer < 300) { // beginning of anim
            return forcePowerLevels_t::FORCE_LEVEL_0;
        }
        return forcePowerLevels_t::FORCE_LEVEL_3;
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
    case animNumber_t::BOTH_BUTTERFLY_LEFT:
    case animNumber_t::BOTH_BUTTERFLY_RIGHT:
    case animNumber_t::BOTH_FJSS_TR_BL:
    case animNumber_t::BOTH_FJSS_TL_BR:
    case animNumber_t::BOTH_LUNGE2_B__T_:
    case animNumber_t::BOTH_FORCELEAP2_T__B_:
    case animNumber_t::BOTH_JUMPFLIPSLASHDOWN1: // #
    case animNumber_t::BOTH_JUMPFLIPSTABDOWN:   // #
        return true;
    default:
        break;
    }
    forcePowerLevels_t animLevel = PM_AnimLevelForSaberAnim(anim);
    if (animLevel <= forcePowerLevels_t::FORCE_LEVEL_0) { // NOTE: this will always return false for the ready poses and putaway/draw...
        return false;
    }
    // drop the anim to the first level and start the checks there
    anim -= (animLevel - forcePowerLevels_t::FORCE_LEVEL_1) * SABER_ANIM_GROUP_SIZE;
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
    case animNumber_t::BOTH_BUTTERFLY_LEFT:
    case animNumber_t::BOTH_BUTTERFLY_RIGHT:
    case animNumber_t::BOTH_FJSS_TR_BL:
    case animNumber_t::BOTH_FJSS_TL_BR:
    case animNumber_t::BOTH_LUNGE2_B__T_:
    case animNumber_t::BOTH_FORCELEAP2_T__B_:
    case animNumber_t::BOTH_JUMPFLIPSLASHDOWN1: // #
    case animNumber_t::BOTH_JUMPFLIPSTABDOWN:   // #
        return true;
    default:
        break;
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
    case saberMoveName_t::LS_A_LUNGE:
    case saberMoveName_t::LS_A_JUMP_T__B_:
    case saberMoveName_t::LS_A_FLIP_STAB:
    case saberMoveName_t::LS_A_FLIP_SLASH:
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
    if (move >= saberMoveName_t::LS_R_TL2BR && move <= saberMoveName_t::LS_R_TL2BR) {
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
    case saberMoveName_t::LS_A_LUNGE:
    case saberMoveName_t::LS_A_JUMP_T__B_:
    case saberMoveName_t::LS_A_FLIP_STAB:
    case saberMoveName_t::LS_A_FLIP_SLASH:
        return true;
    default:
        break;
    }
    return false;
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
    /*
    if ( pm->gent && pm->gent->client )
    {
        if ( pm->gent->client->NPC_class == class_t::CLASS_DESANN ||
            pm->gent->client->NPC_class == class_t::CLASS_TAVION )
        {//desann and tavion can link up as many attacks as they want
            return false;
        }
    }
    */
    if (pm->ps->saberAnimLevel > forcePowerLevels_t::FORCE_LEVEL_3) { // desann and tavion can link up as many attacks as they want
        return false;
    }
    // FIXME: instead of random, apply some sort of logical conditions to whether or
    //		not you can chain?  Like if you were completely missed, you can't chain as much, or...?
    //		And/Or based on FP_SABER_OFFENSE level?  So number of attacks you can chain
    //		increases with your FP_SABER_OFFENSE skill?
    if (pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_3) {
        if (curmove == saberMoveName_t::LS_NONE || newmove == saberMoveName_t::LS_NONE) {
            if (pm->ps->saberAnimLevel >= forcePowerLevels_t::FORCE_LEVEL_3 && pm->ps->saberAttackChainCount > Q_irand(0, 1)) {
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
        if (pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_2 && pm->ps->saberAttackChainCount > Q_irand(2, 5)) {
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
    if (!pm->ps->clientNum && !g_saberAutoAim->integer && pm->cmd.forwardmove >= 0) { // don't auto-backstab
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
            if (!pm->ps->clientNum) { // player
                if (pm->gent) {       // set player enemy to traceEnt so he auto-aims at him
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
    if (pm->gent->client->NPC_class == class_t::CLASS_TAVION) {
        return saberMoveName_t::LS_A_BACKSTAB;
    }
    else if (pm->gent->client->NPC_class == class_t::CLASS_DESANN) {
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
    else if (pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_2) { // using medium attacks
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
extern saberMoveName_t PM_NPCSaberAttackFromQuad(saberQuadrant_t quad);
saberMoveName_t PM_SaberFlipOverAttackMove(void);
saberMoveName_t PM_AttackForEnemyPos(bool allowFB)
{
    saberMoveName_t autoMove = saberMoveName_t::LS_INVALID;

    vec3_t enemy_org, enemyDir, faceFwd, faceRight, faceUp, facingAngles = {0, pm->ps->viewangles[YAW], 0};
    AngleVectors(facingAngles, faceFwd, faceRight, faceUp);
    // FIXME: predict enemy position?
    if (pm->gent->enemy->client) {
        VectorCopy(pm->gent->enemy->currentOrigin, enemy_org);
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
    float enemyDist = VectorNormalize(enemyDir);
    float dot = DotProduct(enemyDir, faceFwd);
    if (dot > 0) { // enemy is in front
        if ((!pm->ps->clientNum || PM_ControlledByPlayer()) && dot > 0.65f && pm->gent->enemy->client && PM_InKnockDownOnGround(&pm->gent->enemy->client->ps) &&
            enemyDir[2] <= 20) { // guy is on the ground below me, do a top-down attack
            return saberMoveName_t::LS_A_T2B;
        }
        if (allowFB) {                                               // directly in front anim allowed
            if (enemyDist > 200 || pm->gent->enemy->health <= 0) {   // hmm, look in back for an enemy
                if (pm->ps->clientNum && !PM_ControlledByPlayer()) { // player should never do this automatically
                    if (pm->gent && pm->gent->client && pm->gent->NPC && pm->gent->NPC->rank >= rank_t::RANK_LT_JG &&
                        Q_irand(0, std::to_underlying(pm->gent->NPC->rank)) >
                            std::to_underlying(rank_t::RANK_ENSIGN)) { // only fencers and higher can do this, higher rank does it more
                        if (PM_CheckEnemyInBack(100)) {
                            return PM_PickBackStab();
                        }
                    }
                }
            }
            // this is the default only if they're *right* in front...
            if ((pm->ps->clientNum && !PM_ControlledByPlayer()) ||
                ((!pm->ps->clientNum || PM_ControlledByPlayer()) && cg.renderingThirdPerson && !cg.zoomMode)) {
                if ((pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_2 ||
                     pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_5)                                // using medium attacks or Tavion
                                                                                                                 //&& !PM_InKnockDown( pm->ps )
                    && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1 // can force jump
                    && !Any(pm->gent->flags & gentityFlags_t::FL_LOCK_PLAYER_WEAPONS) // yes this locked weapons check also includes force powers, if we need a
                                                                                      // separate check later I'll make one
                    && (pm->ps->groundEntityNum != ENTITYNUM_NONE || level.time - pm->ps->lastOnGround <= 500) // on ground or just jumped
                    && (pm->ps->clientNum || pm->ps->legsAnim == animNumber_t::BOTH_JUMP1 || pm->ps->legsAnim == animNumber_t::BOTH_FORCEJUMP1 ||
                        pm->ps->legsAnim == animNumber_t::BOTH_INAIR1 ||
                        pm->ps->legsAnim == animNumber_t::BOTH_FORCEINAIR1) // either an NPC or in a non-flip forward jump
                    && ((pm->ps->clientNum && !PM_ControlledByPlayer() && !Q_irand(0, 2)) || pm->cmd.upmove ||
                        Any(pm->ps->pm_flags & pmoveFlags_t::PMF_JUMPING))) // jumping
                {                                                           // flip over-forward down-attack
                    if ((!pm->ps->clientNum || PM_ControlledByPlayer()) ||
                        (pm->gent->NPC && (pm->gent->NPC->rank == rank_t::RANK_CREWMAN || pm->gent->NPC->rank >= rank_t::RANK_LT) &&
                         !Q_irand(0, 2))) { // only player or acrobat or boss and higher can do this
                        if (pm->gent->enemy->health > 0 && pm->gent->enemy->maxs[2] > 12 &&
                            (!pm->gent->enemy->client || !PM_InKnockDownOnGround(&pm->gent->enemy->client->ps)) &&
                            DistanceSquared(pm->gent->currentOrigin, enemy_org) < 10000 &&
                            InFront(enemy_org, pm->gent->currentOrigin, facingAngles, 0.3f)) { // enemy must be close and in front
                            return PM_SaberFlipOverAttackMove();
                        }
                    }
                }
            }
            if (pm->ps->clientNum && !PM_ControlledByPlayer()) { // NPC
                if (pm->gent->NPC && pm->gent->NPC->rank >= rank_t::RANK_LT_JG &&
                    (pm->gent->NPC->rank == rank_t::RANK_LT_JG ||
                     Q_irand(0, std::to_underlying(pm->gent->NPC->rank)) >= std::to_underlying(rank_t::RANK_ENSIGN))
                    //&& !PM_InKnockDown( pm->ps )
                    && ((pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_1 && !Q_irand(0, 2)) ||
                        (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_DESANN &&
                         !Q_irand(0, 4)))) { // fencers and higher only - 33% chance of using lunge (desann 20% chance)
                    autoMove = saberMoveName_t::LS_A_LUNGE;
                }
                else {
                    autoMove = saberMoveName_t::LS_A_T2B;
                }
            }
            else { // player
                if (pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_1
                    //&& !PM_InKnockDown( pm->ps )
                    && (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED) || pm->cmd.upmove < 0)) { // ducking player
                    autoMove = saberMoveName_t::LS_A_LUNGE;
                }
                else {
                    autoMove = saberMoveName_t::LS_A_T2B;
                }
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
        if (!pm->gent->enemy->client || pm->gent->enemy->client->ps.groundEntityNum != ENTITYNUM_NONE) {
            if (dot < -0.75f && enemyDist < 128 &&
                (pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_1 ||
                 (pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_TAVION && Q_irand(0, 2)))) { // fast back-stab
                if (!Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED) && pm->cmd.upmove >= 0) {                  // can't do it while ducked?
                    if ((!pm->ps->clientNum || PM_ControlledByPlayer()) ||
                        (pm->gent->NPC && pm->gent->NPC->rank >= rank_t::RANK_LT_JG)) { // only fencers and above can do this
                        autoMove = saberMoveName_t::LS_A_BACKSTAB;
                    }
                }
            }
            else if (pm->ps->saberAnimLevel > forcePowerLevels_t::FORCE_LEVEL_1) { // higher level back spin-attacks
                if ((pm->ps->clientNum && !PM_ControlledByPlayer()) ||
                    ((!pm->ps->clientNum || PM_ControlledByPlayer()) && cg.renderingThirdPerson && !cg.zoomMode)) {
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
    return autoMove;
}

saberMoveName_t PM_SaberLungeAttackMove(void)
{
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

saberMoveName_t PM_SaberJumpAttackMove(void)
{
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
    G_SoundOnEnt(pm->gent, sound::SoundClass::Force, sound::Slot::None, "sound/weapons/force/jump.wav");
    pm->cmd.upmove = 0;

    return saberMoveName_t::LS_A_JUMP_T__B_;
}

saberMoveName_t PM_SaberFlipOverAttackMove(void)
{
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
    G_SoundOnEnt(pm->gent, sound::SoundClass::Force, sound::Slot::None, "sound/weapons/force/jump.wav");
    pm->cmd.upmove = 0;
    // FIXME: don't allow this to land on other people

    pm->gent->angle = pm->ps->viewangles[YAW]; // so we know what yaw we started this at
    if (Q_irand(0, 1)) {
        return saberMoveName_t::LS_A_FLIP_STAB;
    }
    else {
        return saberMoveName_t::LS_A_FLIP_SLASH;
    }
}

saberMoveName_t PM_SaberAttackForMovement(int forwardmove, int rightmove, saberMoveName_t move)
{
    if (rightmove > 0) {       // moving right
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
    else if (rightmove < 0) {  // moving left
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
    else {                                                                // not moving left or right
        if (forwardmove > 0) {                                            // forward= T2B slash
            if (pm->gent && pm->gent->enemy && pm->gent->enemy->client) { // I have an active enemy
                if (!pm->ps->clientNum || PM_ControlledByPlayer()) {      // a player who is running at an enemy
                    // if the enemy is not a jedi, don't use top-down, pick a diagonal or side attack
                    if (pm->gent->enemy->s.weapon != weapon_t::WP_SABER && g_saberAutoAim->integer) {
                        saberMoveName_t autoMove = PM_AttackForEnemyPos(false);
                        if (autoMove != saberMoveName_t::LS_INVALID) {
                            return autoMove;
                        }
                    }
                }
                if ((pm->ps->clientNum && !PM_ControlledByPlayer()) ||
                    ((!pm->ps->clientNum || PM_ControlledByPlayer()) && cg.renderingThirdPerson && !cg.zoomMode)) {
                    if ((pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_2 ||
                         pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_5)                                // using medium attacks or Tavion
                                                                                                                     //&& !PM_InKnockDown( pm->ps )
                        && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1 // can force jump
                        && !Any(pm->gent->flags & gentityFlags_t::FL_LOCK_PLAYER_WEAPONS) // yes this locked weapons check also includes force powers, if we
                                                                                          // need a separate check later I'll make one
                        && (pm->ps->groundEntityNum != ENTITYNUM_NONE || level.time - pm->ps->lastOnGround <= 500) // on ground or just jumped
                        && (pm->ps->clientNum || pm->ps->legsAnim == animNumber_t::BOTH_JUMP1 || pm->ps->legsAnim == animNumber_t::BOTH_FORCEJUMP1 ||
                            pm->ps->legsAnim == animNumber_t::BOTH_INAIR1 ||
                            pm->ps->legsAnim == animNumber_t::BOTH_FORCEINAIR1) // either an NPC or in a non-flip forward jump
                        && ((pm->ps->clientNum && !PM_ControlledByPlayer() && !Q_irand(0, 2)) || pm->cmd.upmove > 0 ||
                            Any(pm->ps->pm_flags & pmoveFlags_t::PMF_JUMPING))) // jumping
                    {                                                           // flip over-forward down-attack
                        if ((!pm->ps->clientNum || PM_ControlledByPlayer()) ||
                            (pm->gent->NPC && (pm->gent->NPC->rank == rank_t::RANK_CREWMAN || pm->gent->NPC->rank >= rank_t::RANK_LT) &&
                             !Q_irand(0, 2))) { // only player or acrobat or boss and higher can do this
                            vec3_t fwdAngles = {0, pm->ps->viewangles[YAW], 0};
                            if (pm->gent->enemy->health > 0 && pm->gent->enemy->maxs[2] > 12 &&
                                (!pm->gent->enemy->client || !PM_InKnockDownOnGround(&pm->gent->enemy->client->ps)) &&
                                DistanceSquared(pm->gent->currentOrigin, pm->gent->enemy->currentOrigin) < 10000 &&
                                InFront(pm->gent->enemy->currentOrigin, pm->gent->currentOrigin, fwdAngles,
                                        0.3f)) { // enemy must be alive, not low to ground, close and in front
                                return PM_SaberFlipOverAttackMove();
                            }
                        }
                    }
                }
            }
            if ((pm->ps->clientNum && !PM_ControlledByPlayer()) ||
                ((!pm->ps->clientNum || PM_ControlledByPlayer()) && cg.renderingThirdPerson && !cg.zoomMode)) {
                if ((pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_1 ||
                     (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_DESANN && !Q_irand(0, 2)))
                    //&& !PM_InKnockDown( pm->ps )
                    && (pm->cmd.upmove < 0 || Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) &&
                    (pm->ps->legsAnim == animNumber_t::BOTH_STAND2 || pm->ps->legsAnim == animNumber_t::BOTH_SABERFAST_STANCE ||
                     pm->ps->legsAnim == animNumber_t::BOTH_SABERSLOW_STANCE ||
                     level.time - pm->ps->lastStationary <= 500)) { // not moving (or just started), ducked and using fast attacks
                    if ((!pm->ps->clientNum || PM_ControlledByPlayer()) ||
                        (pm->gent && pm->gent->NPC && pm->gent->NPC->rank >= rank_t::RANK_LT_JG &&
                         (pm->gent->NPC->rank == rank_t::RANK_LT_JG ||
                          Q_irand(0, std::to_underlying(pm->gent->NPC->rank)) >= std::to_underlying(rank_t::RANK_ENSIGN)) &&
                         !Q_irand(0, 3 - g_spskill->integer))) { // only player or fencer and higher can do this
                        return PM_SaberLungeAttackMove();
                    }
                }
            }
            if (pm->ps->clientNum && !PM_ControlledByPlayer()) {
                if ((pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_3 ||
                     (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_DESANN && !Q_irand(0, 1))) // using strong attacks
                                                                                                                               //&& !PM_InKnockDown( pm->ps )
                    && pm->ps->forcePowerLevel[forcePowers_t::FP_LEVITATION] > forcePowerLevels_t::FORCE_LEVEL_1               // can force jump
                    && pm->gent && !Any(pm->gent->flags & gentityFlags_t::FL_LOCK_PLAYER_WEAPONS) // yes this locked weapons check also includes force powers,
                                                                                                  // if we need a separate check later I'll make one
                    && (pm->ps->groundEntityNum != ENTITYNUM_NONE || level.time - pm->ps->lastOnGround <= 500) // on ground or just jumped (if not player)
                    //&& (pm->ps->legsAnim == animNumber_t::BOTH_STAND2||pm->ps->legsAnim == animNumber_t::BOTH_SABERFAST_STANCE||pm->ps->legsAnim ==
                    // animNumber_t::BOTH_SABERSLOW_STANCE||level.time-pm->ps->lastStationary<=500)//standing or just started moving
                    && (pm->cmd.upmove || Any(pm->ps->pm_flags & pmoveFlags_t::PMF_JUMPING))) // jumping
                {                                                                             // strong attack: jump-hack
                    if (                                                                      //! pm->ps->clientNum ||
                        (pm->gent && pm->gent->NPC && !PM_ControlledByPlayer() &&
                         (pm->gent->NPC->rank == rank_t::RANK_CREWMAN ||
                          pm->gent->NPC->rank >= rank_t::RANK_LT))) { // only player or acrobat or boss and higher can do this
                        return PM_SaberJumpAttackMove();
                    }
                }
            }

            return saberMoveName_t::LS_A_T2B;
        }
        else if (forwardmove < 0) { // backward= T2B slash//B2T uppercut?
            if ((pm->ps->clientNum && !PM_ControlledByPlayer()) ||
                ((!pm->ps->clientNum || PM_ControlledByPlayer()) && cg.renderingThirdPerson && !cg.zoomMode)) {
                if (pm->gent && pm->gent->enemy) { // FIXME: or just trace for a valid enemy standing behind me?  And no enemy in front?
                    vec3_t enemyDir, faceFwd, facingAngles = {0, pm->ps->viewangles[YAW], 0};
                    AngleVectors(facingAngles, faceFwd, nullptr, nullptr);
                    VectorSubtract(pm->gent->enemy->currentOrigin, pm->ps->origin, enemyDir);
                    float dot = DotProduct(enemyDir, faceFwd);
                    if (dot < 0) {                                                                                           // enemy is behind me
                        if (dot < -0.75f && DistanceSquared(pm->gent->currentOrigin, pm->gent->enemy->currentOrigin) < 16384 // 128 squared
                            && (pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_1 ||
                                (pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_TAVION && Q_irand(0, 1)))) { // fast attacks and Tavion
                            if (!Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED) && pm->cmd.upmove >= 0) {                     // can't do it while ducked?
                                if ((!pm->ps->clientNum || PM_ControlledByPlayer()) ||
                                    (pm->gent->NPC && pm->gent->NPC->rank >= rank_t::RANK_LT_JG)) { // only fencers and above can do this
                                    return saberMoveName_t::LS_A_BACKSTAB;
                                }
                            }
                        }
                        else if (pm->ps->saberAnimLevel > forcePowerLevels_t::FORCE_LEVEL_1) { // medium and higher attacks
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
                        if ((pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_1 || pm->gent->client->NPC_class == class_t::CLASS_TAVION ||
                             (pm->gent->client->NPC_class == class_t::CLASS_DESANN && !Q_irand(0, 3))) &&
                            (enemyDistSq > 16384 || pm->gent->enemy->health <= 0)) // 128 squared
                        {                                                          // my enemy is pretty far in front of me and I'm using fast attacks
                            if ((!pm->ps->clientNum || PM_ControlledByPlayer()) ||
                                (pm->gent && pm->gent->client && pm->gent->NPC && pm->gent->NPC->rank >= rank_t::RANK_LT_JG &&
                                 Q_irand(0, std::to_underlying(pm->gent->NPC->rank)) >
                                     std::to_underlying(rank_t::RANK_ENSIGN))) { // only fencers and higher can do this, higher rank does it more
                                if (PM_CheckEnemyInBack(128)) {
                                    return PM_PickBackStab();
                                }
                            }
                        }
                        else if ((pm->ps->saberAnimLevel >= forcePowerLevels_t::FORCE_LEVEL_2 || pm->gent->client->NPC_class == class_t::CLASS_DESANN) &&
                                 (enemyDistSq > 40000 || pm->gent->enemy->health <= 0)) // 200 squared
                        {                                                               // enemy is very faw away and I'm using medium/strong attacks
                            if ((!pm->ps->clientNum || PM_ControlledByPlayer()) ||
                                (pm->gent && pm->gent->client && pm->gent->NPC && pm->gent->NPC->rank >= rank_t::RANK_LT_JG &&
                                 Q_irand(0, std::to_underlying(pm->gent->NPC->rank)) >
                                     std::to_underlying(rank_t::RANK_ENSIGN))) { // only fencers and higher can do this, higher rank does it more
                                if (PM_CheckEnemyInBack(164)) {
                                    return PM_PickBackStab();
                                }
                            }
                        }
                    }
                }
                else {                                                                                     // no current enemy
                    if ((!pm->ps->clientNum || PM_ControlledByPlayer()) && pm->gent && pm->gent->client) { // only player
                        if (PM_CheckEnemyInBack(128)) {
                            return PM_PickBackStab();
                        }
                    }
                }
            }
            // else just swing down
            return saberMoveName_t::LS_A_T2B;
        }
        else if (PM_SaberInBounce(move)) { // bounces should go to their default attack if you don't specify a direction but are attacking
            saberMoveName_t newmove;
            if (pm->ps->clientNum && !PM_ControlledByPlayer() && Q_irand(0, 3)) { // use NPC random
                newmove = PM_NPCSaberAttackFromQuad(saberMoveData[move].endQuad);
            }
            else { // player uses chain-attack
                newmove = saberMoveData[move].chain_attack;
            }
            if (PM_SaberKataDone(move, newmove)) {
                return saberMoveData[move].chain_idle;
            }
            else {
                return newmove;
            }
        }
        else if (PM_SaberInKnockaway(move)) { // bounces should go to their default attack if you don't specify a direction but are attacking
            saberMoveName_t newmove;
            if (pm->ps->clientNum && !PM_ControlledByPlayer() && Q_irand(0, 3)) { // use NPC random
                newmove = PM_NPCSaberAttackFromQuad(saberMoveData[move].endQuad);
            }
            else {
                if (pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_1 ||
                    pm->ps->saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_5) { // player is in fast attacks, so come right back down from the same spot
                    newmove = PM_AttackMoveForQuad(saberMoveData[move].endQuad);
                }
                else { // use a transition to wrap to another attack from a different dir
                    newmove = saberMoveData[move].chain_attack;
                }
            }
            if (PM_SaberKataDone(move, newmove)) {
                return saberMoveData[move].chain_idle;
            }
            else {
                return newmove;
            }
        }
        else if (move == saberMoveName_t::LS_READY || move == saberMoveName_t::LS_A_FLIP_STAB ||
                 move == saberMoveName_t::LS_A_FLIP_SLASH) {    // Not moving at all, shouldn't have gotten here...?
            if (pm->ps->clientNum || g_saberAutoAim->integer) { // auto-aim
                if (pm->gent && pm->gent->enemy) {              // based on enemy position, pick a proper attack
                    saberMoveName_t autoMove = PM_AttackForEnemyPos(true);
                    if (autoMove != saberMoveName_t::LS_INVALID) {
                        return autoMove;
                    }
                }
                else if (Q_fabsf(pm->ps->viewangles[0]) > 30) { // looking far up or far down uses the top to bottom attack, presuming you want a vertical
                                                                // attack
                    return saberMoveName_t::LS_A_T2B;
                }
            }
            else { // for now, just pick a random attack
                return Q_irand(saberMoveName_t::LS_A_TL2BR, saberMoveName_t::LS_A_T2B);
            }
        }
    }
    // FIXME: pick a return?
    return saberMoveName_t::LS_NONE;
}

saberMoveName_t PM_SaberAnimTransitionAnim(saberMoveName_t curmove, saberMoveName_t newmove)
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

    for (animNumber_t animation = animNumber_t::BOTH_1CRUFTFORGIL; animation < animNumber_t::FACE_TALK1; animation++) {
        if (animation >= animNumber_t::TORSO_DROPWEAP1 && animation < animNumber_t::LEGS_WALKBACK1) { // not a possible legs anim
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

std::optional<animNumber_t> PM_ValidateAnimRange(int startFrame, int endFrame, float animSpeed)
{ // given a startframe and endframe, see if that lines up with any known animation
    auto& animations = level.knownAnimFileSets[0].animations;

    for (animNumber_t anim = animNumber_t::BOTH_1CRUFTFORGIL; anim < animNumber_t::MAX_ANIMATIONS; anim++) {
        if (animSpeed < 0) { // playing backwards
            if (animations[anim].firstFrame == endFrame) {
                if (animations[anim].numFrames + animations[anim].firstFrame == startFrame) {
                    // Q_Println("valid reverse anim: {}", BG_AnimName( anim ));
                    return anim;
                }
            }
        }
        else {                                                                              // playing forwards
            if (animations[anim].firstFrame == startFrame) {                                // This anim starts on this frame
                if (animations[anim].firstFrame + animations[anim].numFrames == endFrame) { // This anim ends on this frame
                    // Q_Println("valid forward anim: {}", BG_AnimName( anim ));
                    return anim;
                }
            }
        }
        // else, must not be this anim!
    }

    // Not in ANY anim?  SHOULD NEVER HAPPEN
    Q_Println("invalid anim range {} to {}, speed {:4.2f}", startFrame, endFrame, animSpeed);
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

    for (animNumber_t animation = animNumber_t::BOTH_1CRUFTFORGIL; animation < animNumber_t::LEGS_WALKBACK1; animation++) {
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

std::optional<animNumber_t> BG_AnimFromName(const char* name)
{
    const int anim = GetIDForString(animTable, name);
    if (anim == -1) {
        return std::nullopt;
    }
    return static_cast<animNumber_t>(anim);
}

const char* BG_AnimName(animNumber_t anim)
{
    return animTable[std::to_underlying(anim)].name;
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
    if (animation < animNumber_t::BOTH_1CRUFTFORGIL || animation >= animNumber_t::MAX_ANIMATIONS) {
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
    if (!ValidAnimFileIndex(index) || anim < animNumber_t::BOTH_1CRUFTFORGIL || anim >= animNumber_t::MAX_ANIMATIONS) {
        return 0;
    }

    return level.knownAnimFileSets[index].animations[anim].numFrames * Q_fabs((double)(level.knownAnimFileSets[index].animations[anim].frameLerp));
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
void PM_SaberStartTransAnim(forcePowerLevels_t saberAnimLevel, animNumber_t anim, float* animSpeed, gentity_t* gent)
{
    if (g_saberAnimSpeed->value != 1.0f) {
        if (anim >= animNumber_t::BOTH_A1_T__B_ && anim <= animNumber_t::BOTH_CROUCHATTACKBACK1) {
            *animSpeed *= g_saberAnimSpeed->value;
        }
    }
    if (gent && gent->NPC && gent->NPC->rank == rank_t::RANK_CIVILIAN) {                  // grunt reborn
        if (anim >= animNumber_t::BOTH_A1_T__B_ && anim <= animNumber_t::BOTH_R1_TR_S1) { // his fast attacks are slower
            if (!PM_SpinningSaberAnim(anim)) {
                *animSpeed *= 0.75;
            }
            return;
        }
    }
    if ((anim >= animNumber_t::BOTH_T1_BR__R && anim <= animNumber_t::BOTH_T1_BL_TL) ||
        (anim >= animNumber_t::BOTH_T3_BR__R && anim <= animNumber_t::BOTH_T3_BL_TL) ||
        (anim >= animNumber_t::BOTH_T5_BR__R && anim <= animNumber_t::BOTH_T5_BL_TL)) {
        if (saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_1 ||
            saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_5) { // FIXME: should not be necc for FORCE_LEVEL_1's
            *animSpeed *= 1.5;
        }
        else if (saberAnimLevel == forcePowerLevels_t::FORCE_LEVEL_3) {
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
        if (!MatrixMode) {
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
#define G2_DEBUG_TIMING (0)
void PM_SetAnimFinal(animNumber_t* torsoAnim, animNumber_t* legsAnim, setAnimParts_t setAnimParts, animNumber_t anim, setAnimFlags_t setAnimFlags,
                     int* torsoAnimTimer, int* legsAnimTimer, gentity_t* gent, int blendTime) // default blendTime=350
{
    if (!ValidAnimFileIndex(gent->client->clientInfo.animFileIndex)) {
        return;
    }
    if (anim < animNumber_t::BOTH_1CRUFTFORGIL || anim >= animNumber_t::MAX_ANIMATIONS) {
        assert(0 && "anim out of range!!!");
#ifndef FINAL_BUILD
        Q_Error(errorParm_t::ERR_DROP, "{} tried to play invalid anim {}", gent->NPC_type, anim);
#endif
        return;
    }
    auto& animations = level.knownAnimFileSets[gent->client->clientInfo.animFileIndex].animations;
    float timeScaleMod = PM_GetTimeScaleMod(gent);
    float animSpeed, oldAnimSpeed;
    int actualTime = (cg.time ? cg.time : level.time);

    if (gent && gent->client) { // lower offensive skill slows down the saber start attack animations
        PM_SaberStartTransAnim(gent->client->ps.saberAnimLevel, anim, &timeScaleMod, gent);
        //	PM_SaberStartTransAnim( anim, gent->s.number, gent->client->ps.forcePowerLevel[forcePowers_t::FP_SABER_OFFENSE], &timeScaleMod );
    }

    // Set torso anim
    if (Any(setAnimParts & setAnimParts_t::SETANIM_TORSO)) {
        // or if a more important anim is running
        if (!Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_OVERRIDE) && ((*torsoAnimTimer > 0) || (*torsoAnimTimer == -1))) {
            goto setAnimLegs;
        }

        if (!PM_HasAnimation(gent, anim)) {
            if (Q3_DebugLevelShown(WL_e::WL_VERBOSE)) {
                // Q_Println(S_COLOR_RED"SET_ANIM_UPPER ERROR: anim {} does not exist in this model ({})!", BG_AnimName( anim ),
                // ((gent!=nullptr&&gent->client!=nullptr) ? gent->client->renderInfo.torsoModelName : "unknown"));
            }
            goto setAnimLegs;
        }

        // animSpeed is 1.0 if the frameLerp (ms/frame) is 50 (20 fps).
        animSpeed = oldAnimSpeed = 50.0f / animations[anim].frameLerp * timeScaleMod;

        if (gi.G2API_HaveWeGhoul2Models(gent->ghoul2) && gent->lowerLumbarBone != -1) // gent->upperLumbarBone
        { // see if we need to tell ghoul2 to play it again because of a animSpeed change
            int blah;
            boneFlags_t blahFlags;
            float junk;
            if (!gi.G2API_GetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->lowerLumbarBone, actualTime, &junk, &blah, &blah, &blahFlags, &oldAnimSpeed,
                                           nullptr)) {
                animSpeed = oldAnimSpeed;
            }
        }

        if (oldAnimSpeed == animSpeed) { // animSpeed has not changed
            // Don't reset if it's already running the anim
            if (!Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_RESTART) && *torsoAnim == anim) {
                goto setAnimLegs;
            }
        }

        *torsoAnim = anim;

        // lets try and run a ghoul2 animation if we have a ghoul2 model on this guy
        if (gi.G2API_HaveWeGhoul2Models(gent->ghoul2)) {
            if (gent->lowerLumbarBone != -1) // gent->upperLumbarBone
            {
                int startFrame, endFrame;

                if (cg_debugAnim.integer == 3 || (!gent->s.number && cg_debugAnim.integer == 1) || (gent->s.number && cg_debugAnim.integer == 2)) {
                    Q_Println("Time={}: {} TORSO anim {} {}", actualTime, gent->targetname, anim, BG_AnimName(anim));
                }

                // we have a ghoul2 model - animate it?
                // don't bother if the animation is missing
                if (!animations[anim].numFrames) {
                    // remove it if we already have an animation on this bone
                    if (gi.G2API_GetAnimRange(&gent->ghoul2[gent->playerModel], "lower_lumbar", &startFrame, &endFrame)) //"upper_lumbar"
                    {
#if G2_DEBUG_TIMING
                        Q_Println("tstop {}", cg.time);
#endif
                        gi.G2API_StopBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->lowerLumbarBone); // gent->upperLumbarBone
                        if (gent->motionBone != -1) {
                            gi.G2API_StopBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->motionBone);
                        }
                    }
                }
                else {

                    // before we do this, lets see if this animation is one the legs are already playing?

                    boneFlags_t animFlags = boneFlags_t::BONE_ANIM_OVERRIDE_FREEZE;

                    if (animations[anim].loopFrames != -1) {
                        animFlags = boneFlags_t::BONE_ANIM_OVERRIDE_LOOP;
                    }
                    // HACKHACKHACK - need some better way of not lerping between something like a spin/flip and a death, etc.
                    if (blendTime > 0) {
                        animFlags |= boneFlags_t::BONE_ANIM_BLEND;
                    }
                    // HACKHACKHACK
                    // bool animatingLegs =  gi.G2API_GetAnimRange(&gent->ghoul2[gent->playerModel], "model_root", &startFrame, &endFrame);
                    float currentFrame, legAnimSpeed, firstFrame, lastFrame;
                    boneFlags_t flags;
                    bool animatingLegs = gi.G2API_GetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->rootBone, actualTime, &currentFrame, &startFrame,
                                                                   &endFrame, &flags, &legAnimSpeed, nullptr);
                    if (g_synchSplitAnims->integer && !Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_RESTART) && animatingLegs &&
                        (animations[anim].firstFrame == startFrame) && (legAnimSpeed == animSpeed) //|| oldAnimSpeed != animSpeed)
                        && (((animations[anim].numFrames) + animations[anim].firstFrame) ==
                            endFrame)) { // if we're playing this *exact* anim (speed check should fix problems with anims that play other anims backwards) on
                                         // the legs already andwe're not restarting the anim, then match the legs' frame
                        if (0) {         // just stop it
                            gi.G2API_StopBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->lowerLumbarBone); // gent->upperLumbarBone
                            gi.G2API_StopBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->motionBone);      // gent->upperLumbarBone
                        }
                        else { // try to synch it
//							assert((currentFrame <=endFrame) && (currentFrame>=startFrame));
// yes, its the same animation, so work out where we are in the leg anim, and blend us
#if G2_DEBUG_TIMING
                            Q_Println("tlegb {}     {} {} {:4.2f} {:4.2f} {}", actualTime, animations[anim].firstFrame,
                                      (animations[anim].numFrames) + animations[anim].firstFrame, legAnimSpeed, currentFrame, blendTime);
#endif
                            if (oldAnimSpeed != animSpeed &&
                                ((oldAnimSpeed > 0 && animSpeed > 0) || (oldAnimSpeed < 0 && animSpeed < 0))) { // match the new speed, actually
                                legAnimSpeed = animSpeed;
                            }
                            if (legAnimSpeed < 0) {                                                      // play anim backwards
                                lastFrame = animations[anim].firstFrame;                                 // -1;
                                firstFrame = (animations[anim].numFrames) + animations[anim].firstFrame; // -1) + animations[anim].firstFrame;
                            }
                            else {
                                firstFrame = animations[anim].firstFrame;
                                lastFrame = (animations[anim].numFrames) + animations[anim].firstFrame;
                            }
                            gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->lowerLumbarBone, // gent->upperLumbarBone
                                                      firstFrame, lastFrame, animFlags, legAnimSpeed, actualTime, currentFrame, blendTime);
                            if (gent->motionBone != -1) {
                                gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->motionBone, firstFrame, lastFrame, animFlags, legAnimSpeed,
                                                          actualTime, currentFrame, blendTime);
                            }
                        }
                    }
                    else { // no, we aren't the same anim as the legs are running, so just run it.
                        // animSpeed is 1.0 if the frameLerp (ms/frame) is 50 (20 fps).
                        int firstFrame;
                        int lastFrame;
                        if (animSpeed < 0) {                         // play anim backwards
                            lastFrame = animations[anim].firstFrame; // -1;
                            firstFrame = (animations[anim].numFrames) + animations[anim].firstFrame;
                            ; // -1) + animations[anim].firstFrame;
                        }
                        else {
                            firstFrame = animations[anim].firstFrame;
                            lastFrame = (animations[anim].numFrames) + animations[anim].firstFrame;
                        }
                        // first decide if we are doing an animation on the torso already
                        bool animatingTorso = gi.G2API_GetAnimRange(&gent->ghoul2[gent->playerModel], "lower_lumbar", &startFrame, &endFrame); //"upper_lumbar"

                        // lets see if a) we are already animating and b) we aren't going to do the same animation again
                        if (animatingTorso && ((firstFrame != startFrame) || (lastFrame != endFrame))) {
#if G2_DEBUG_TIMING
                            Q_Println("trsob {}     {} {} {:4.2f} {:4.2f} {}", actualTime, firstFrame, lastFrame, animSpeed, -1, blendTime);
#endif
                            gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->lowerLumbarBone, // gent->upperLumbarBone
                                                      firstFrame, lastFrame, animFlags, animSpeed, actualTime, -1, blendTime);

                            if (gent->motionBone != -1) {
                                gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->motionBone, firstFrame, lastFrame, animFlags, animSpeed,
                                                          actualTime, -1, blendTime);
                            }
                        }
                        else {
                            // no, ok, no blend then because we are either looping on the same anim, or starting from no anim
#if G2_DEBUG_TIMING
                            Q_Println("trson {}     {} {} {:4.2f} {:4.2f} {}", actualTime, firstFrame, lastFrame, animSpeed, -1, -1);
#endif
                            gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->lowerLumbarBone, // gent->upperLumbarBone
                                                      firstFrame, lastFrame, animFlags & ~boneFlags_t::BONE_ANIM_BLEND, animSpeed, cg.time, -1, -1);
                            if (gent->motionBone != -1) {
                                gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->motionBone, firstFrame, lastFrame,
                                                          animFlags & ~boneFlags_t::BONE_ANIM_BLEND, animSpeed, cg.time, -1, -1);
                            }
                        }
                    }
                }
            }
        }
        /*
        #ifdef _DEBUG
                Q_Println(S_COLOR_GREEN"SET_ANIM_UPPER: {} ({})", BG_AnimName( anim ), gent->targetname);
        #endif
        */
        if ((gent->client) && Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_HOLD)) { // FIXME: allow to set a specific time?
            if (timeScaleMod != 1.0) {
                PM_SetTorsoAnimTimer(gent, torsoAnimTimer, (animations[anim].numFrames - 1) * Q_fabs((double)animations[anim].frameLerp) / timeScaleMod);
            }
            else if (Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_HOLDLESS)) { // Make sure to only wait in full 1/20 sec server frame intervals.
                int dur;

                dur = (animations[anim].numFrames - 1) * Q_fabs((double)animations[anim].frameLerp);
                // dur = ((int)(dur/50.0)) * 50;
                // dur -= blendTime;
                if (dur > 1) {
                    PM_SetTorsoAnimTimer(gent, torsoAnimTimer, dur - 1);
                }
                else {
                    PM_SetTorsoAnimTimer(gent, torsoAnimTimer, Q_fabs((double)animations[anim].frameLerp));
                }
            }
            else {
                PM_SetTorsoAnimTimer(gent, torsoAnimTimer, (animations[anim].numFrames) * Q_fabs((double)animations[anim].frameLerp));
            }
        }
    }

setAnimLegs:
    // Set legs anim
    if (Any(setAnimParts & setAnimParts_t::SETANIM_LEGS)) {
        // or if a more important anim is running
        if (!Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_OVERRIDE) && ((*legsAnimTimer > 0) || (*legsAnimTimer == -1))) {
            goto setAnimDone;
        }

        if (!PM_HasAnimation(gent, anim)) {
            if (Q3_DebugLevelShown(WL_e::WL_VERBOSE)) {
                // Q_Println(S_COLOR_RED"SET_ANIM_LOWER ERROR: anim {} does not exist in this model ({})!", BG_AnimName( anim ),
                // ((gent!=nullptr&&gent->client!=nullptr) ? gent->client->renderInfo.legsModelName : "unknown"));
            }
            goto setAnimDone;
        }

        // animSpeed is 1.0 if the frameLerp (ms/frame) is 50 (20 fps).
        animSpeed = oldAnimSpeed = 50.0f / animations[anim].frameLerp * timeScaleMod;

        if (gi.G2API_HaveWeGhoul2Models(gent->ghoul2) && gent->rootBone != -1) { // see if we need to tell ghoul2 to play it again because of a animSpeed change
            int blah;
            boneFlags_t blahFlags;
            float junk;
            if (!gi.G2API_GetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->rootBone, actualTime, &junk, &blah, &blah, &blahFlags, &oldAnimSpeed,
                                           nullptr)) {
                animSpeed = oldAnimSpeed;
            }
        }

        if (oldAnimSpeed == animSpeed) { // animSpeed has not changed
            // Don't reset if it's already running the anim
            if (!Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_RESTART) && *legsAnim == anim) {
                goto setAnimDone;
            }
        }

        *legsAnim = anim;

        // lets try and run a ghoul2 animation if we have a ghoul2 model on this guy
        if (gi.G2API_HaveWeGhoul2Models(gent->ghoul2)) {
            int startFrame, endFrame;

            if (cg_debugAnim.integer == 3 || (!gent->s.number && cg_debugAnim.integer == 1) || (gent->s.number && cg_debugAnim.integer == 2)) {
                Q_Println("Time={}: {} LEGS anim {} {}", actualTime, gent->targetname, anim, BG_AnimName(anim));
            }

            boneFlags_t animFlags = boneFlags_t::BONE_ANIM_OVERRIDE_FREEZE;

            if (animations[anim].loopFrames != -1) {
                animFlags = boneFlags_t::BONE_ANIM_OVERRIDE_LOOP;
            }
            // HACKHACKHACK - need some better way of not lerping between something like a spin/flip and a death, etc.
            if (blendTime > 0) {
                animFlags |= boneFlags_t::BONE_ANIM_BLEND;
            }
            // HACKHACKHACK

            // don't bother if the animation is missing
            if (!animations[anim].numFrames) {
                // remove it if we already have an animation on this bone
                if (gi.G2API_GetAnimRange(&gent->ghoul2[gent->playerModel], "model_root", &startFrame, &endFrame)) {
#if G2_DEBUG_TIMING
                    Q_Println("lstop {}", cg.time);
#endif
                    gi.G2API_StopBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->rootBone);
                }
            }
            else {
                int firstFrame;
                int lastFrame;
                if (animSpeed < 0) {                                                         // play anim backwards
                    lastFrame = animations[anim].firstFrame;                                 // -1;
                    firstFrame = (animations[anim].numFrames) + animations[anim].firstFrame; // -1) + animations[anim].firstFrame;
                }
                else {
                    firstFrame = animations[anim].firstFrame;
                    lastFrame = (animations[anim].numFrames) + animations[anim].firstFrame;
                }
                // HACKHACKHACK
                // bool animatingTorso =  gi.G2API_GetAnimRangeIndex(&gent->ghoul2[gent->playerModel], gent->lowerLumbarBone, &startFrame, &endFrame);
                float currentFrame, torsoAnimSpeed;
                boneFlags_t flags;
                bool animatingTorso = gi.G2API_GetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->lowerLumbarBone, actualTime, &currentFrame, &startFrame,
                                                                &endFrame, &flags, &torsoAnimSpeed, nullptr);
                if (g_synchSplitAnims->integer && !Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_RESTART) && animatingTorso &&
                    (torsoAnimSpeed == animSpeed) //|| oldAnimSpeed != animSpeed)
                    && (animations[anim].firstFrame == startFrame) &&
                    (((animations[anim].numFrames) + animations[anim].firstFrame) ==
                     endFrame)) { // if we're playing this *exact* anim on the torso already and we're not restarting the anim, then match the torso's frame
                                  // try to synch it
//					assert((currentFrame <=endFrame) && (currentFrame>=startFrame));
// yes, its the same animation, so work out where we are in the torso anim, and blend us
#if G2_DEBUG_TIMING
                    Q_Println("ltrsb {}     {} {} {:4.2f} {:4.2f} {}", actualTime, animations[anim].firstFrame,
                              (animations[anim].numFrames) + animations[anim].firstFrame, legAnimSpeed, currentFrame, blendTime);
#endif
                    if (oldAnimSpeed != animSpeed &&
                        ((oldAnimSpeed > 0 && animSpeed > 0) || (oldAnimSpeed < 0 && animSpeed < 0))) { // match the new speed, actually
                        torsoAnimSpeed = animSpeed;
                    }
                    if (torsoAnimSpeed < 0) {                                                    // play anim backwards
                        lastFrame = animations[anim].firstFrame;                                 // -1;
                        firstFrame = (animations[anim].numFrames) + animations[anim].firstFrame; // -1) + animations[anim].firstFrame;
                    }
                    else {
                        firstFrame = animations[anim].firstFrame;
                        lastFrame = (animations[anim].numFrames) + animations[anim].firstFrame;
                    }

                    gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->rootBone, firstFrame, lastFrame, animFlags, torsoAnimSpeed, actualTime,
                                              currentFrame, blendTime);
                }
                else { // no, we aren't the same anim as the torso is running, so just run it.
                    // before we do this, lets see if this animation is one the legs are already playing?
                    // bool animatingLegs =  gi.G2API_GetAnimRange(&gent->ghoul2[gent->playerModel], "model_root", &startFrame, &endFrame);
                    float currentFrame, legAnimSpeed;
                    boneFlags_t flags;
                    bool animatingLegs = gi.G2API_GetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->rootBone, actualTime, &currentFrame, &startFrame,
                                                                   &endFrame, &flags, &legAnimSpeed, nullptr);
                    // lets see if a) we are already animating and b) we aren't going to do the same animation again
                    if (animatingLegs && ((legAnimSpeed != animSpeed) || (firstFrame != startFrame) || (lastFrame != endFrame))) {
#if G2_DEBUG_TIMING
                        Q_Println("legsb {}     {} {} {:4.2f} {:4.2f} {}", actualTime, firstFrame, lastFrame, animSpeed, -1, blendTime);
#endif
                        gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->rootBone, firstFrame, lastFrame, animFlags, animSpeed, actualTime, -1,
                                                  blendTime);
                    }
                    else {
                        // no, ok, no blend then because we are either looping on the same anim, or starting from no anim
#if G2_DEBUG_TIMING
                        Q_Println("legsn {}     {} {} {:4.2f} {:4.2f} {}", actualTime, firstFrame, lastFrame, animSpeed, -1, -1);
#endif
                        gi.G2API_SetBoneAnimIndex(&gent->ghoul2[gent->playerModel], gent->rootBone, firstFrame, lastFrame,
                                                  animFlags & ~boneFlags_t::BONE_ANIM_BLEND, animSpeed, cg.time, -1, -1);
                    }
                }
            }
        }

        /*
        #ifdef _DEBUG
                Q_Println(S_COLOR_GREEN"SET_ANIM_LOWER: {} ({})", BG_AnimName( anim ), gent->targetname);
        #endif
        */
        if ((gent->client) && Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_HOLD)) { // FIXME: allow to set a specific time?
            if (timeScaleMod != 1.0) {
                PM_SetLegsAnimTimer(gent, legsAnimTimer, (animations[anim].numFrames - 1) * Q_fabs((double)animations[anim].frameLerp) / timeScaleMod);
            }
            else if (Any(setAnimFlags & setAnimFlags_t::SETANIM_FLAG_HOLDLESS)) { // Make sure to only wait in full 1/20 sec server frame intervals.
                int dur;

                dur = (animations[anim].numFrames - 1) * Q_fabs((double)animations[anim].frameLerp);
                // dur = ((int)(dur/50.0)) * 50;
                // dur -= blendTime;
                if (dur > 1) {
                    PM_SetLegsAnimTimer(gent, legsAnimTimer, dur - 1);
                }
                else {
                    PM_SetLegsAnimTimer(gent, legsAnimTimer, Q_fabs((double)animations[anim].frameLerp));
                }
            }
            else {
                PM_SetLegsAnimTimer(gent, legsAnimTimer, (animations[anim].numFrames) * Q_fabs((double)animations[anim].frameLerp));
            }
        }
    }

setAnimDone:
    return;
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

/*
-------------------------
PM_TorsoAnimLightsaber
-------------------------
*/

// Note that this function is intended to set the animation for the player, but
// only does idle-ish anims.  Anything that has a timer associated, such as attacks and blocks,
// are set by PM_WeaponLightsaber()

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
        // PM_SetAnim( pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_FORCEGRIP_HOLD,
        // setAnimFlags_t::SETANIM_FLAG_OVERRIDE|setAnimFlags_t::SETANIM_FLAG_HOLD );
        return;
    }
    if (pm->ps->forcePowersActive & Bit(forcePowers_t::FP_LIGHTNING) &&
        pm->ps->forcePowerLevel[forcePowers_t::FP_LIGHTNING] > forcePowerLevels_t::FORCE_LEVEL_1) { // holding an enemy aloft with force-grip
        // PM_SetAnim( pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_FORCELIGHTNING_HOLD,
        // setAnimFlags_t::SETANIM_FLAG_OVERRIDE|setAnimFlags_t::SETANIM_FLAG_HOLD );
        return;
    }

    if (pm->ps->saberActive && pm->ps->saberLength < 3 && !Any(pm->ps->saberEventFlags & saberEventFlags_t::SEF_HITWALL) &&
        pm->ps->weaponstate == weaponstate_t::WEAPON_RAISING) {
        PM_SetSaberMove(saberMoveName_t::LS_DRAW);
        return;
    }
    else if (!pm->ps->saberActive && pm->ps->saberLength) {
        PM_SetSaberMove(saberMoveName_t::LS_PUTAWAY);
        return;
    }

    if (pm->ps->weaponTime > 0) { // weapon is already busy.
        return;
    }

    if (pm->ps->weaponstate == weaponstate_t::WEAPON_READY || pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING ||
        pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING_ALT) {
        if (pm->ps->weapon == weapon_t::WP_SABER && pm->ps->saberLength) {
            if (PM_JumpingAnim(pm->ps->legsAnim) || PM_LandingAnim(pm->ps->legsAnim) || PM_InCartwheel(pm->ps->legsAnim) || PM_FlippingAnim(pm->ps->legsAnim)) {
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            }
            else {
                PM_SetSaberMove(saberMoveName_t::LS_READY);
            }
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN1) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN2) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
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
                    if (!pm->ps->clientNum && pm->ps->torsoAnim == animNumber_t::BOTH_BUTTON_HOLD) { // using something
                        if (!pm->ps->useTime) {                                                      // stopped holding it, release
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
        if (pm->ps->legsAnim == animNumber_t::BOTH_GUARD_LOOKAROUND1) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_GUARD_LOOKAROUND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_GUARD_IDLE1) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_GUARD_IDLE1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
            pm->ps->saberMove = saberMoveName_t::LS_READY;
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_STAND1IDLE1 || pm->ps->legsAnim == animNumber_t::BOTH_STAND2IDLE1 ||
                 pm->ps->legsAnim == animNumber_t::BOTH_STAND2IDLE2 || pm->ps->legsAnim == animNumber_t::BOTH_STAND3IDLE1 ||
                 pm->ps->legsAnim == animNumber_t::BOTH_STAND4IDLE1 || pm->ps->legsAnim == animNumber_t::BOTH_STAND5IDLE1) {
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
            if (pm->ps->saberInFlight && saberInAir) {
                if (!PM_ForceAnim(pm->ps->torsoAnim) || pm->ps->torsoAnimTimer < 300) { // don't interrupt a force power anim
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_SABERPULL,
                               setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                }
            }
            else { // saber is on
                // Idle for Lightsaber
                if (pm->gent && pm->gent->client) {
                    pm->gent->client->saberTrail.inAction = false;
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
                        if (!pm->ps->clientNum && pm->ps->torsoAnim == animNumber_t::BOTH_BUTTON_HOLD) { // using something
                            if (!pm->ps->useTime) {                                                      // stopped holding it, release
                                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_BUTTON_RELEASE,
                                           setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                            } // else still holding, leave it as it is
                        }
                        else {
                            if (PM_RunningAnim(pm->ps->legsAnim)) { // running w/1-handed weapon uses full-body anim
                                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
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

    if (pm->gent && pm->gent->NPC && Any(pm->gent->NPC->scriptFlags & scriptFlags_t::SCF_FORCED_MARCH)) {
        return;
    }

    if (pm->gent != nullptr && pm->gent->client) {
        pm->gent->client->renderInfo.torsoFpsMod = 1.0f;
    }

    if (pm->gent && pm->ps && Any(pm->ps->eFlags & entityFlags_t::EF_LOCKED_TO_WEAPON)) {
        PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_GUNSIT1,
                   setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD); // SETANIM_FLAG_NORMAL
        return;
    }

    if (pm->ps->taunting > level.time) {
        if (PM_HasAnimation(pm->gent, animNumber_t::BOTH_GESTURE1)) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_GESTURE1,
                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD); // SETANIM_FLAG_NORMAL
            pm->gent->client->saberTrail.inAction = true;
            pm->gent->client->saberTrail.duration = 100;
            // FIXME: will this reset?
            // FIXME: force-control (yellow glow) effect on hand and saber?
        }
        else {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONIDLE1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        return;
    }

    if (pm->ps->weapon == weapon_t::WP_SABER) // WP_LIGHTSABER
    {
        if (pm->ps->saberLength > 0 && !pm->ps->saberInFlight) {
            PM_TorsoAnimLightsaber();
        }
        else {
            if (pm->ps->forcePowersActive & Bit(forcePowers_t::FP_GRIP) &&
                pm->ps->forcePowerLevel[forcePowers_t::FP_GRIP] > forcePowerLevels_t::FORCE_LEVEL_1) { // holding an enemy aloft with force-grip
                // PM_SetAnim( pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_FORCEGRIP_HOLD,
                // setAnimFlags_t::SETANIM_FLAG_OVERRIDE|setAnimFlags_t::SETANIM_FLAG_HOLD );
                return;
            }
            if (pm->ps->forcePowersActive & Bit(forcePowers_t::FP_LIGHTNING) &&
                pm->ps->forcePowerLevel[forcePowers_t::FP_LIGHTNING] > forcePowerLevels_t::FORCE_LEVEL_1) { // holding an enemy aloft with force-grip
                // PM_SetAnim( pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_FORCELIGHTNING_HOLD,
                // setAnimFlags_t::SETANIM_FLAG_OVERRIDE|setAnimFlags_t::SETANIM_FLAG_HOLD );
                return;
            }
            bool saberInAir = true;

            if (PM_SaberInBrokenParry(pm->ps->saberMove) || pm->ps->saberBlocked == saberBlockedType_t::BLOCKED_PARRY_BROKEN ||
                PM_DodgeAnim(pm->ps->torsoAnim)) { // we're stuck in a broken parry
                PM_TorsoAnimLightsaber();
                return;
            }
            if (pm->ps->saberEntityNum < ENTITYNUM_NONE && pm->ps->saberEntityNum > 0)            // player is 0
            {                                                                                     //
                if (g_entities[pm->ps->saberEntityNum].s.pos.trType == trType_t::TR_STATIONARY) { // fell to the ground and we're not trying to pull it back
                    saberInAir = false;
                }
            }

            if (pm->ps->saberInFlight && saberInAir) {
                if (!PM_ForceAnim(pm->ps->torsoAnim) || pm->ps->torsoAnimTimer < 300) { // don't interrupt a force power anim
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_SABERPULL,
                               setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                }
            }
            else {
                if (PM_InSlopeAnim(pm->ps->legsAnim)) { // HMM... this probably breaks the saber putaway and select anims
                    if (pm->ps->saberLength > 0) {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                }
                else {
                    if (!pm->ps->clientNum && pm->ps->torsoAnim == animNumber_t::BOTH_BUTTON_HOLD) { // using something
                        if (!pm->ps->useTime) {                                                      // stopped holding it, release
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
    else if (pm->ps->weaponTime) {
        weaponBusy = true;
    }
    else if (pm->gent && pm->gent->client->fireDelay > 0) {
        weaponBusy = true;
    }
    else if (!pm->ps->clientNum && cg.zoomTime > cg.time - 5000) { // if we used binoculars recently, aim weapon
        weaponBusy = true;
        pm->ps->weaponstate = weaponstate_t::WEAPON_IDLE;
    }
    else if (Any(pm->ps->pm_flags & pmoveFlags_t::PMF_DUCKED)) { // ducking is considered on alert... plus looks stupid to have arms hanging down when crouched
        weaponBusy = true;
    }

    if (pm->ps->weapon == weapon_t::WP_NONE || pm->ps->weaponstate == weaponstate_t::WEAPON_READY || pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING ||
        pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING_ALT) {
        if (pm->ps->weapon == weapon_t::WP_SABER && pm->ps->saberLength) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_ATTACK1, setAnimFlags_t::SETANIM_FLAG_NORMAL); // TORSO_WEAPONREADY1
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN1 && !weaponBusy) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_RUN2 && !weaponBusy) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_RUN2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_WALK1 && !weaponBusy) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WALK1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
        }
        else if (pm->ps->legsAnim == animNumber_t::BOTH_WALK2 && !weaponBusy) {
            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_WALK2, setAnimFlags_t::SETANIM_FLAG_NORMAL);
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
        else {                                                                               // Used to default to both_stand1 which is an arms-down anim
            if (!pm->ps->clientNum && pm->ps->torsoAnim == animNumber_t::BOTH_BUTTON_HOLD) { // using something
                if (!pm->ps->useTime) {                                                      // stopped holding it, release
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_BUTTON_RELEASE,
                               setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                } // else still holding, leave it as it is
            }
            else if (pm->gent != nullptr && pm->gent->s.number == 0 && pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING &&
                     pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING_ALT) { // PLayer- temp hack for weapon frame
                if (pm->ps->weapon == weapon_t::WP_MELEE) {                       // hehe
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
                    if (weaponBusy) {
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
                case weapon_t::WP_NONE:
                    // NOTE: should never get here
                    break;
                case weapon_t::WP_MELEE:
                    if (PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                        PM_SwimmingAnim(pm->ps->legsAnim)) { // running w/1-handed weapon uses full-body anim
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        if (pm->gent && pm->gent->client && !PM_DroidMelee(pm->gent->client->NPC_class)) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND6, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    break;
                case weapon_t::WP_BLASTER:
                    PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    // PM_SetAnim(pm,setAnimParts_t::SETANIM_LEGS,animNumber_t::BOTH_ATTACK2,setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    break;
                case weapon_t::WP_DISRUPTOR:
                    if ((pm->ps->weaponstate != weaponstate_t::WEAPON_FIRING && pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING &&
                         pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING_ALT) ||
                        PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                        PM_SwimmingAnim(pm->ps->legsAnim)) { // running sniper weapon uses normal ready
                        if (pm->ps->clientNum) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3,
                                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD); // TORSO_WEAPONREADY4//SETANIM_FLAG_RESTART|
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
                        if (!pm->ps->clientNum && (pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING ||
                                                   pm->ps->weaponstate == weaponstate_t::WEAPON_CHARGING_ALT)) { // player pulling back to throw
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
                 pm->ps->legsAnim == animNumber_t::BOTH_STAND4IDLE1 || pm->ps->legsAnim == animNumber_t::BOTH_STAND5IDLE1) {
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
        else if (!pm->ps->clientNum && pm->ps->torsoAnim == animNumber_t::BOTH_BUTTON_HOLD) { // using something
            if (!pm->ps->useTime) {                                                           // stopped holding it, release
                PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_BUTTON_RELEASE,
                           setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
            } // else still holding, leave it as it is
        }
        else {
            if (!weaponBusy && pm->ps->weapon != weapon_t::WP_BOWCASTER && pm->ps->weapon != weapon_t::WP_REPEATER &&
                pm->ps->weapon != weapon_t::WP_FLECHETTE && pm->ps->weapon != weapon_t::WP_ROCKET_LAUNCHER &&
                (PM_RunningAnim(pm->ps->legsAnim) || (PM_WalkingAnim(pm->ps->legsAnim) && !pm->ps->clientNum) || PM_JumpingAnim(pm->ps->legsAnim) ||
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
                    if (weaponBusy) {
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

                case weapon_t::WP_NONE:
                    // NOTE: should never get here
                    break;

                case weapon_t::WP_MELEE:
                    if (PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                        PM_SwimmingAnim(pm->ps->legsAnim)) { // running w/1-handed weapon uses full-body anim
                        PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, pm->ps->legsAnim, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                    }
                    else {
                        if (pm->gent && pm->gent->client && !PM_DroidMelee(pm->gent->client->NPC_class)) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND6, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_STAND1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
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
                    if ((pm->ps->weaponstate != weaponstate_t::WEAPON_FIRING && pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING &&
                         pm->ps->weaponstate != weaponstate_t::WEAPON_CHARGING_ALT) ||
                        PM_RunningAnim(pm->ps->legsAnim) || PM_WalkingAnim(pm->ps->legsAnim) || PM_JumpingAnim(pm->ps->legsAnim) ||
                        PM_SwimmingAnim(pm->ps->legsAnim)) { // running sniper weapon uses normal ready
                        if (pm->ps->clientNum) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3,
                                       setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD); // TORSO_WEAPONREADY4//SETANIM_FLAG_RESTART|
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
                    if (pm->gent && pm->gent->client && pm->gent->client->NPC_class == class_t::CLASS_GALAKMECH) { //
                        if (pm->gent->alt_fire) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONIDLE3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONIDLE1, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                    }
                    else {
                        if (weaponBusy) {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONREADY3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
                        else {
                            PM_SetAnim(pm, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_WEAPONIDLE3, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        }
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
    case animNumber_t::BOTH_STAND4IDLE1: // # Random standing idle
    case animNumber_t::BOTH_STAND5:      // # standing idle, no weapon, hand down, back straight
    case animNumber_t::BOTH_STAND5IDLE1: // # Random standing idle
    case animNumber_t::BOTH_STAND6:      // # one handed: gun at side: relaxed stand
    case animNumber_t::BOTH_STAND1TO3:   // # Transition from stand1 to stand3
    case animNumber_t::BOTH_STAND3TO1:   // # Transition from stand3 to stand1
    case animNumber_t::BOTH_STAND2TO4:   // # Transition from stand2 to stand4
    case animNumber_t::BOTH_STAND4TO2:   // # Transition from stand4 to stand2
    case animNumber_t::BOTH_STANDUP1:    // # standing up and stumbling
    case animNumber_t::BOTH_GESTURE1:    // # Generic gesture: non-specific
    case animNumber_t::BOTH_GESTURE2:    // # Generic gesture: non-specific
    case animNumber_t::BOTH_GESTURE3:    // # Generic gesture: non-specific
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
    case animNumber_t::BOTH_ATTACK5:           // # Attack with rocket launcher
    case animNumber_t::BOTH_MELEE1:            // # First melee attack
    case animNumber_t::BOTH_MELEE2:            // # Second melee attack
    case animNumber_t::BOTH_MELEE3:            // # Third melee attack
    case animNumber_t::BOTH_MELEE4:            // # Fourth melee attack
    case animNumber_t::BOTH_MELEE5:            // # Fifth melee attack
    case animNumber_t::BOTH_MELEE6:            // # Sixth melee attack
    case animNumber_t::BOTH_COVERUP1_LOOP:     // # animation of getting in line of friendly fire
    case animNumber_t::BOTH_GUARD_LOOKAROUND1: // # Cradling weapon and looking around
    case animNumber_t::BOTH_GUARD_IDLE1:       // # Cradling weapon and standing
    case animNumber_t::BOTH_ALERT1:            // # Startled by something while on guard
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
    case animNumber_t::BOTH_STAND4:      // # two handed: gun down: relaxed stand
    case animNumber_t::BOTH_STAND4IDLE1: // # Random standing idle
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
        case animNumber_t::BOTH_CROUCH2IDLE:		//# crouch and resting on back righ heel: no weapon
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
    case animNumber_t::BOTH_PAIN2WRITHE1:        // # Transition from upright position to writhing on ground anim
    case animNumber_t::BOTH_WRITHING1:           // # Lying on ground writhing in pain
    case animNumber_t::BOTH_WRITHING1RLEG:       // # Lying on ground writhing in pain: holding right leg
    case animNumber_t::BOTH_WRITHING1LLEG:       // # Lying on ground writhing in pain: holding left leg
    case animNumber_t::BOTH_WRITHING2:           // # Lying on stomache writhing in pain
    case animNumber_t::BOTH_INJURED1:            // # Lying down: against wall - can also be sleeping
    case animNumber_t::BOTH_CRAWLBACK1:          // # Lying on back: crawling backwards with elbows
    case animNumber_t::BOTH_INJURED2:            // # Injured pose 2
    case animNumber_t::BOTH_INJURED3:            // # Injured pose 3
    case animNumber_t::BOTH_INJURED6:            // # Injured pose 6
    case animNumber_t::BOTH_INJURED6ATTACKSTART: // # Start attack while in injured 6 pose
    case animNumber_t::BOTH_INJURED6ATTACKSTOP:  // # End attack while in injured 6 pose
    case animNumber_t::BOTH_INJURED6COMBADGE:    // # Hit combadge while in injured 6 pose
    case animNumber_t::BOTH_INJURED6POINT:       // # Chang points to door while in injured state
    case animNumber_t::BOTH_SLEEP1:              // # laying on back-rknee up-rhand on torso
    case animNumber_t::BOTH_SLEEP2:              // # on floor-back against wall-arms crossed
    case animNumber_t::BOTH_SLEEP5:              // # Laying on side sleeping on flat sufrace
    case animNumber_t::BOTH_SLEEP_IDLE1:         // # rub face and nose while asleep from sleep pose 1
    case animNumber_t::BOTH_SLEEP_IDLE2:         // # shift position while asleep - stays in sleep2
    case animNumber_t::BOTH_SLEEP1_NOSE:         // # Scratch nose from SLEEP1 pose
    case animNumber_t::BOTH_SLEEP2_SHIFT:        // # Shift in sleep from SLEEP2 pose
        return true;
        break;
    case animNumber_t::BOTH_KNOCKDOWN1: // #
    case animNumber_t::BOTH_KNOCKDOWN2: // #
    case animNumber_t::BOTH_KNOCKDOWN3: // #
    case animNumber_t::BOTH_KNOCKDOWN4: // #
    case animNumber_t::BOTH_KNOCKDOWN5: // #
        if (ps->legsAnimTimer < 500) {  // pretty much horizontal by this point
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
    case animNumber_t::BOTH_DEADFLOP1:          // # React to being shot from First Death finished pose
    case animNumber_t::BOTH_DEADFLOP2:          // # React to being shot from Second Death finished pose
    case animNumber_t::BOTH_DEADFLOP3:          // # React to being shot from Third Death finished pose
    case animNumber_t::BOTH_DEADFLOP4:          // # React to being shot from Fourth Death finished pose
    case animNumber_t::BOTH_DEADFLOP5:          // # React to being shot from Fifth Death finished pose
    case animNumber_t::BOTH_DEADFORWARD1_FLOP:  // # React to being shot First thrown forward death finished pose
    case animNumber_t::BOTH_DEADFORWARD2_FLOP:  // # React to being shot Second thrown forward death finished pose
    case animNumber_t::BOTH_DEADBACKWARD1_FLOP: // # React to being shot First thrown backward death finished pose
    case animNumber_t::BOTH_DEADBACKWARD2_FLOP: // # React to being shot Second thrown backward death finished pose
    case animNumber_t::BOTH_LYINGDEAD1_FLOP:    // # React to being shot Killed lying down death finished pose
    case animNumber_t::BOTH_STUMBLEDEAD1_FLOP:  // # React to being shot Stumble forward death finished pose
    case animNumber_t::BOTH_FALLDEAD1_FLOP:     // # React to being shot Fall forward and splat death finished pose
    case animNumber_t::BOTH_DISMEMBER_HEAD1:    // #
    case animNumber_t::BOTH_DISMEMBER_TORSO1:   // #
    case animNumber_t::BOTH_DISMEMBER_LLEG:     // #
    case animNumber_t::BOTH_DISMEMBER_RLEG:     // #
    case animNumber_t::BOTH_DISMEMBER_RARM:     // #
    case animNumber_t::BOTH_DISMEMBER_LARM:     // #
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

bool PM_StandingAnim(animNumber_t anim)
{ // NOTE: does not check idles or special (cinematic) stands
    switch (anim) {
    case animNumber_t::BOTH_STAND1:
    case animNumber_t::BOTH_STAND2:
    case animNumber_t::BOTH_STAND3:
    case animNumber_t::BOTH_STAND4:
        return true;
        break;
    default:
        break;
    }
    return false;
}

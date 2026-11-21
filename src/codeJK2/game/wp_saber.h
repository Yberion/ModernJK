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

#ifndef __WP_SABER_H
#define __WP_SABER_H

inline constexpr int ARMOR_EFFECT_TIME = 500;

inline constexpr int JSF_AMBUSH = 16; // ambusher Jedi

#include <shared/qcommon/q_saberflags.h>
#include <shared/qcommon/q_spsets.h>

inline constexpr float SABER_EXTRAPOLATE_DIST = 16.0f;

inline constexpr float SABER_MAX_DIST = 400.0f;

inline constexpr int FORCE_POWER_MAX = 100;

inline constexpr float SABER_REFLECT_MISSILE_CONE = 0.2f;

inline constexpr int SABER_LOCK_TIME = 10000;
inline constexpr int SABER_LOCK_DELAYED_TIME = 9500;
enum class saberLockResult_t {
    LOCK_VICTORY = 0, // one side won
    LOCK_STALEMATE,   // neither side won
    LOCK_DRAW         // both people fall back
};

inline constexpr int MAX_FORCE_HEAL = 25;
inline constexpr int FORCE_HEAL_INTERVAL = 200; // FIXME: maybe level 1 is slower or level 2 is faster?

inline constexpr float FORCE_GRIP_DIST = 512.0f; // FIXME: vary by power level?
inline constexpr float FORCE_GRIP_DIST_SQUARED = FORCE_GRIP_DIST * FORCE_GRIP_DIST;

extern EnumArray<float, forcePowerLevels_t, forcePowerLevels_t::NUM_FORCE_POWER_LEVELS> forceJumpStrength;
extern EnumArray<float, forcePowerLevels_t, forcePowerLevels_t::NUM_FORCE_POWER_LEVELS> forceJumpHeight;

extern EnumArray<float, forcePowerLevels_t, forcePowerLevels_t::NUM_FORCE_POWER_LEVELS> forcePushPullRadius;

extern void ForceSpeed(gentity_t* self, int duration = 0);
extern EnumArray<float, forcePowerLevels_t, forcePowerLevels_t::NUM_FORCE_POWER_LEVELS> forceSpeedValue;
extern EnumArray<float, forcePowerLevels_t, forcePowerLevels_t::NUM_FORCE_POWER_LEVELS> forceSpeedRangeMod;
extern EnumArray<float, forcePowerLevels_t, forcePowerLevels_t::NUM_FORCE_POWER_LEVELS> forceSpeedFOVMod;
inline constexpr float FORCE_SPEED_DURATION = 10000.0f;

enum class forceJump_t { FJ_FORWARD, FJ_BACKWARD, FJ_RIGHT, FJ_LEFT, FJ_UP };

inline constexpr float FORCE_JUMP_CHARGE_TIME = 1000.0f; // Force jump reaches maximum power in one second

extern void WP_InitForcePowers(gentity_t* ent);
extern forceJump_t WP_GetVelocityForForceJump(gentity_t* self, vec3_t jumpVel, usercmd_t* ucmd);

enum class evasionType_t {
    EVASION_NONE = 0,
    EVASION_PARRY,
    EVASION_DUCK_PARRY,
    EVASION_JUMP_PARRY,
    EVASION_DODGE,
    EVASION_JUMP,
    EVASION_DUCK,
    EVASION_FJUMP,
    EVASION_CARTWHEEL,
    EVASION_OTHER,
    NUM_EVASION_TYPES
};

// Okay, here lies the much-dreaded Pat-created FSM movement chart...  Heretic II strikes again!
// Why am I inflicting this on you?  Well, it's better than hardcoded states.
// Ideally this will be replaced with an external file or more sophisticated move-picker
// once the game gets out of prototype stage.

enum class saberMoveName_t {
    // no move found: the -1 the code returned and tested for, named as in Jedi Academy
    LS_INVALID = -1,
    // Invalid, or saber not armed
    LS_NONE = 0,

    // General movements with saber
    LS_READY,
    LS_DRAW,
    LS_PUTAWAY,

    // Attacks
    LS_A_TL2BR, // 4
    LS_A_L2R,
    LS_A_BL2TR,
    LS_A_BR2TL,
    LS_A_R2L,
    LS_A_TR2BL,
    LS_A_T2B,
    LS_A_BACKSTAB,
    LS_A_BACK,
    LS_A_BACK_CR,
    LS_A_LUNGE,
    LS_A_JUMP_T__B_,
    LS_A_FLIP_STAB,
    LS_A_FLIP_SLASH,

    // starts
    LS_S_TL2BR, // 26
    LS_S_L2R,
    LS_S_BL2TR, // # Start of attack chaining to SLASH LR2UL
    LS_S_BR2TL, // # Start of attack chaining to SLASH LR2UL
    LS_S_R2L,
    LS_S_TR2BL,
    LS_S_T2B,

    // returns
    LS_R_TL2BR, // 33
    LS_R_L2R,
    LS_R_BL2TR,
    LS_R_BR2TL,
    LS_R_R2L,
    LS_R_TR2BL,
    LS_R_T2B,

    // transitions
    LS_T1_BR__R, // 40
    LS_T1_BR_TR,
    LS_T1_BR_T_,
    LS_T1_BR_TL,
    LS_T1_BR__L,
    LS_T1_BR_BL,
    LS_T1__R_BR, // 46
    LS_T1__R_TR,
    LS_T1__R_T_,
    LS_T1__R_TL,
    LS_T1__R__L,
    LS_T1__R_BL,
    LS_T1_TR_BR, // 52
    LS_T1_TR__R,
    LS_T1_TR_T_,
    LS_T1_TR_TL,
    LS_T1_TR__L,
    LS_T1_TR_BL,
    LS_T1_T__BR, // 58
    LS_T1_T___R,
    LS_T1_T__TR,
    LS_T1_T__TL,
    LS_T1_T___L,
    LS_T1_T__BL,
    LS_T1_TL_BR, // 64
    LS_T1_TL__R,
    LS_T1_TL_TR,
    LS_T1_TL_T_,
    LS_T1_TL__L,
    LS_T1_TL_BL,
    LS_T1__L_BR, // 70
    LS_T1__L__R,
    LS_T1__L_TR,
    LS_T1__L_T_,
    LS_T1__L_TL,
    LS_T1__L_BL,
    LS_T1_BL_BR, // 76
    LS_T1_BL__R,
    LS_T1_BL_TR,
    LS_T1_BL_T_,
    LS_T1_BL_TL,
    LS_T1_BL__L,

    // Bounces
    LS_B1_BR,
    LS_B1__R,
    LS_B1_TR,
    LS_B1_T_,
    LS_B1_TL,
    LS_B1__L,
    LS_B1_BL,

    // Deflected attacks
    LS_D1_BR,
    LS_D1__R,
    LS_D1_TR,
    LS_D1_T_,
    LS_D1_TL,
    LS_D1__L,
    LS_D1_BL,
    LS_D1_B_,

    // Reflected attacks
    LS_V1_BR,
    LS_V1__R,
    LS_V1_TR,
    LS_V1_T_,
    LS_V1_TL,
    LS_V1__L,
    LS_V1_BL,
    LS_V1_B_,

    // Broken parries
    LS_H1_T_, //
    LS_H1_TR,
    LS_H1_TL,
    LS_H1_BR,
    LS_H1_B_,
    LS_H1_BL,

    // Knockaways
    LS_K1_T_, //
    LS_K1_TR,
    LS_K1_TL,
    LS_K1_BR,
    LS_K1_BL,

    // Parries
    LS_PARRY_UP, //
    LS_PARRY_UR,
    LS_PARRY_UL,
    LS_PARRY_LR,
    LS_PARRY_LL,

    // Projectile Reflections
    LS_REFLECT_UP, //
    LS_REFLECT_UR,
    LS_REFLECT_UL,
    LS_REFLECT_LR,
    LS_REFLECT_LL,

    LS_MOVE_MAX //
};
template <> inline constexpr bool enableEnumArithmetic<saberMoveName_t> = true;

enum class saberQuadrant_t { Q_BR, Q_R, Q_TR, Q_T, Q_TL, Q_L, Q_BL, Q_B, Q_NUM_QUADS };
template <> inline constexpr bool enableEnumArithmetic<saberQuadrant_t> = true;

typedef struct {
    const char* name;
    animNumber_t animToUse;
    saberQuadrant_t startQuad;
    saberQuadrant_t endQuad;
    setAnimFlags_t animSetFlags;
    int blendTime;
    saberBlockType_t blocking;
    saberMoveName_t chain_idle;   // What move to call if the attack button is not pressed at the end of this anim
    saberMoveName_t chain_attack; // What move to call if the attack button (and nothing else) is pressed
    int trailLength;
} saberMoveData_t;

extern EnumArray<saberMoveData_t, saberMoveName_t, saberMoveName_t::LS_MOVE_MAX> saberMoveData;

#endif // __WP_SABER_H

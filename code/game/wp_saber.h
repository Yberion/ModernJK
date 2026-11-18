/*
===========================================================================
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

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

#include "b_public.h"

#define ARMOR_EFFECT_TIME	500

#define JSF_AMBUSH		16	//ambusher Jedi

#include "qcommon/q_saberflags.h"
//saberEntityState
#define SES_LEAVING		1
#define SES_HOVERING	2
#define SES_RETURNING	3

#define SABER_EXTRAPOLATE_DIST 16.0f

#define	SABER_MAX_DIST	400.0f
#define	SABER_MAX_DIST_SQUARED	(SABER_MAX_DIST*SABER_MAX_DIST)

#define	FORCE_POWER_MAX	100

#define	SABER_REFLECT_MISSILE_CONE	0.2f

#define SABER_RADIUS_STANDARD	3.0f

#define	SABER_LOCK_TIME			10000
#define	SABER_LOCK_DELAYED_TIME 9500
enum class saberLockResult_t
{
	LOCK_VICTORY = 0,//one side won
	LOCK_STALEMATE,//neither side won
	LOCK_DRAW//both people fall back
};
using enum saberLockResult_t;

enum class sabersLockMode_t
{
	LOCK_FIRST = 0,
	LOCK_TOP = LOCK_FIRST,
	LOCK_DIAG_TR,
	LOCK_DIAG_TL,
	LOCK_DIAG_BR,
	LOCK_DIAG_BL,
	LOCK_R,
	LOCK_L,
	LOCK_RANDOM,
	LOCK_KYLE_GRAB1,
	LOCK_KYLE_GRAB2,
	LOCK_KYLE_GRAB3,
	LOCK_FORCE_DRAIN
};
using enum sabersLockMode_t;
template <> inline constexpr bool enableEnumArithmetic<sabersLockMode_t> = true;

enum class saberLockType_t
{
	SABERLOCK_TOP,
	SABERLOCK_SIDE,
	SABERLOCK_LOCK,
	SABERLOCK_BREAK,
	SABERLOCK_SUPERBREAK,
	SABERLOCK_WIN,
	SABERLOCK_LOSE
};
using enum saberLockType_t;

enum class direction_t
{
	DIR_RIGHT,
	DIR_LEFT,
	DIR_FRONT,
	DIR_BACK
};
using enum direction_t;

#define FORCE_LIGHTSIDE			1
#define FORCE_DARKSIDE			2

#define	MAX_FORCE_HEAL_HARD		25
#define	MAX_FORCE_HEAL_MEDIUM	50
#define	MAX_FORCE_HEAL_EASY		75
#define	FORCE_HEAL_INTERVAL	200//FIXME: maybe level 1 is slower or level 2 is faster?

#define	FORCE_GRIP_3_MIN_DIST	128.0f
#define	FORCE_GRIP_3_MAX_DIST	256.0f
#define	FORCE_GRIP_DIST			512.0f//FIXME: vary by power level?
#define	FORCE_GRIP_DIST_SQUARED	(FORCE_GRIP_DIST*FORCE_GRIP_DIST)

#define	FORCE_DRAIN_DIST	64.0f//FIXME: vary by power level?
#define	FORCE_DRAIN_DIST_SQUARED	(FORCE_DRAIN_DIST*FORCE_DRAIN_DIST)

#define MAX_DRAIN_DISTANCE 512

#define MIN_SABERBLADE_DRAW_LENGTH 0.5f

#define STAFF_KICK_RANGE 16

#define	JUMP_OFF_WALL_SPEED	200.0f

#define	FORCE_LONG_LEAP_SPEED	475.0f//300


//#define DUAL_SPIN_PROTECT_POWER	50	//power required to do the dual spin attack
//#define SINGLE_SPECIAL_POWER	20	//power required to do the single saber special attacks

#define SABER_ALT_ATTACK_POWER		50//75?
#define SABER_ALT_ATTACK_POWER_LR	10//30?
#define SABER_ALT_ATTACK_POWER_FB	25//30/50?

#define FORCE_LONGJUMP_POWER	20

#define WALL_RUN_UP_BACKFLIP_SPEED	-150.0f//was -300.0f
#define	MAX_WALL_RUN_Z_NORMAL	0.4f//was 0.0f

#define	PLAYER_KNOCKDOWN_HOLD_EXTRA_TIME	4000	//player stays down after a knockdown for 4 whole seconds before automatically doing one of the slow get-ups

#define MAX_WALL_GRAB_SLOPE	0.2f

//"Matrix" effect flags
enum class matrixEffectFlags_t
{
	MEF_NONE				= 0,
	MEF_NO_TIMESCALE		= 0x000001,	//no timescale
	MEF_NO_VERTBOB			= 0x000002,	//no vert bob
	MEF_NO_SPIN				= 0x000004,	//no spin
	MEF_NO_RANGEVAR			= 0x000008,	//no camera range variation
	MEF_HIT_GROUND_STOP		= 0x000010,	//stop effect when subject hits the ground
	MEF_REVERSE_SPIN		= 0x000020,	//spin counter-clockwise instead of clockwise
	MEF_MULTI_SPIN			= 0x000040,	//spin once every second, until the effect stops
	MEF_LOOK_AT_ENEMY		= 0x000200,
};
using enum matrixEffectFlags_t;
template <> inline constexpr bool enableEnumFlags<matrixEffectFlags_t> = true;

#define SABER_PITCH_HACK 90

extern EnumArray<float, forcePowerLevels_t, NUM_FORCE_POWER_LEVELS> forceJumpStrength;
extern EnumArray<float, forcePowerLevels_t, NUM_FORCE_POWER_LEVELS> forceJumpHeight;
extern EnumArray<float, forcePowerLevels_t, NUM_FORCE_POWER_LEVELS> forceJumpHeightMax;

extern EnumArray<float, forcePowerLevels_t, NUM_FORCE_POWER_LEVELS> forcePushPullRadius;

extern void ForceSpeed( gentity_t *self, int duration = 0 );
extern EnumArray<float, forcePowerLevels_t, NUM_FORCE_POWER_LEVELS> forceSpeedValue;
extern EnumArray<float, forcePowerLevels_t, NUM_FORCE_POWER_LEVELS> forceSpeedRangeMod;
extern EnumArray<float, forcePowerLevels_t, NUM_FORCE_POWER_LEVELS> forceSpeedFOVMod;
extern const EnumArray<const char *, saber_colors_t, to_underlying( SABER_PURPLE ) + 1> saberColorStringForColor;
#define FORCE_SPEED_DURATION 10000.0f
#define FORCE_RAGE_DURATION 10000.0f

inline constexpr contentsFlags_t MASK_FORCE_PUSH = MASK_OPAQUE | CONTENTS_SOLID;

enum class forceJump_t
{
	FJ_FORWARD,
	FJ_BACKWARD,
	FJ_RIGHT,
	FJ_LEFT,
	FJ_UP
};
using enum forceJump_t;

#define FORCE_JUMP_CHARGE_TIME 1000.0f	//Force jump reaches maximum power in one second

#define FORCE_POWERS_ROSH_FROM_TWINS (Bit( FP_SPEED )|Bit( FP_GRIP )|Bit( FP_RAGE )|Bit( FP_SABERTHROW ))

extern void WP_InitForcePowers( gentity_t *ent );
extern forceJump_t WP_GetVelocityForForceJump( gentity_t *self, vec3_t jumpVel, usercmd_t *ucmd );
extern int WP_SaberInitBladeData( gentity_t *ent );
extern void G_CreateG2AttachedWeaponModel( gentity_t *ent, const char *weaponModel, int boltNum, int weaponNum );
extern void WP_SaberAddG2SaberModels( gentity_t *ent, int specificSaberNum = -1 );
extern bool WP_SaberParseParms( const char *SaberName, saberInfo_t *saber, bool setColors = true );
extern bool WP_BreakSaber( gentity_t *ent, const char *surfName, saberType_t saberType = SABER_NONE );
extern void ForceThrow( gentity_t *self, bool pull, bool fake = false );
extern bool G_GetHitLocFromSurfName( gentity_t *ent, const char *surfName, hitLocation_t *hitLoc, vec3_t point, vec3_t dir, vec3_t bladeDir, meansOfDeath_t mod, saberType_t saberType = SABER_NONE );
extern bool G_CheckEnemyPresence( gentity_t *ent, direction_t dir, float radius, float tolerance = 0.75f );
extern void WP_SaberFreeStrings( saberInfo_t &saber );
extern bool G_EnoughPowerForSpecialMove( int forcePower, int cost, bool kataMove = false );
extern void G_DrainPowerForSpecialMove( gentity_t *self, forcePowers_t fp, int cost, bool kataMove = false );
extern int G_CostForSpecialMove( int cost, bool kataMove = false );
extern gentity_t *G_DropSaberItem( const char *saberType, saber_colors_t saberColor, vec3_t saberPos, vec3_t saberVel, vec3_t saberAngles, gentity_t *copySaber = nullptr );

enum class evasionType_t
{
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
using enum evasionType_t;

enum class swingType_t
{
	SWING_FAST,
	SWING_MEDIUM,
	SWING_STRONG
};
using enum swingType_t;
// Okay, here lies the much-dreaded Pat-created FSM movement chart...  Heretic II strikes again!
// Why am I inflicting this on you?  Well, it's better than hardcoded states.
// Ideally this will be replaced with an external file or more sophisticated move-picker
// once the game gets out of prototype stage. <- HAHA!

#ifdef LS_NONE
#undef LS_NONE
#endif

enum class saberMoveName_t {
	// Invalid, or saber not armed
	LS_INVALID	= -1,
	LS_NONE		= 0,

	// General movements with saber
	LS_READY,
	LS_DRAW,
	LS_PUTAWAY,

	// Attacks
	LS_A_TL2BR,//4
	LS_A_L2R,
	LS_A_BL2TR,
	LS_A_BR2TL,
	LS_A_R2L,
	LS_A_TR2BL,
	LS_A_T2B,
	LS_A_BACKSTAB,
	LS_A_BACK,
	LS_A_BACK_CR,
	LS_ROLL_STAB,
	LS_A_LUNGE,
	LS_A_JUMP_T__B_,
	LS_A_FLIP_STAB,
	LS_A_FLIP_SLASH,
	LS_JUMPATTACK_DUAL,
	LS_JUMPATTACK_ARIAL_LEFT,
	LS_JUMPATTACK_ARIAL_RIGHT,
	LS_JUMPATTACK_CART_LEFT,
	LS_JUMPATTACK_CART_RIGHT,
	LS_JUMPATTACK_STAFF_LEFT,
	LS_JUMPATTACK_STAFF_RIGHT,
	LS_BUTTERFLY_LEFT,
	LS_BUTTERFLY_RIGHT,
	LS_A_BACKFLIP_ATK,
	LS_SPINATTACK_DUAL,
	LS_SPINATTACK,
	LS_LEAP_ATTACK,
	LS_SWOOP_ATTACK_RIGHT,
	LS_SWOOP_ATTACK_LEFT,
	LS_TAUNTAUN_ATTACK_RIGHT,
	LS_TAUNTAUN_ATTACK_LEFT,
	LS_KICK_F,
	LS_KICK_B,
	LS_KICK_R,
	LS_KICK_L,
	LS_KICK_S,
	LS_KICK_BF,
	LS_KICK_RL,
	LS_KICK_F_AIR,
	LS_KICK_B_AIR,
	LS_KICK_R_AIR,
	LS_KICK_L_AIR,
	LS_STABDOWN,
	LS_STABDOWN_STAFF,
	LS_STABDOWN_DUAL,
	LS_DUAL_SPIN_PROTECT,
	LS_STAFF_SOULCAL,
	LS_A1_SPECIAL,
	LS_A2_SPECIAL,
	LS_A3_SPECIAL,
	LS_UPSIDE_DOWN_ATTACK,
	LS_PULL_ATTACK_STAB,
	LS_PULL_ATTACK_SWING,
	LS_SPINATTACK_ALORA,
	LS_DUAL_FB,
	LS_DUAL_LR,
	LS_HILT_BASH,

	//starts
	LS_S_TL2BR,//26
	LS_S_L2R,
	LS_S_BL2TR,//# Start of attack chaining to SLASH LR2UL
	LS_S_BR2TL,//# Start of attack chaining to SLASH LR2UL
	LS_S_R2L,
	LS_S_TR2BL,
	LS_S_T2B,

	//returns
	LS_R_TL2BR,//33
	LS_R_L2R,
	LS_R_BL2TR,
	LS_R_BR2TL,
	LS_R_R2L,
	LS_R_TR2BL,
	LS_R_T2B,

	//transitions
	LS_T1_BR__R,//40
	LS_T1_BR_TR,
	LS_T1_BR_T_,
	LS_T1_BR_TL,
	LS_T1_BR__L,
	LS_T1_BR_BL,
	LS_T1__R_BR,//46
	LS_T1__R_TR,
	LS_T1__R_T_,
	LS_T1__R_TL,
	LS_T1__R__L,
	LS_T1__R_BL,
	LS_T1_TR_BR,//52
	LS_T1_TR__R,
	LS_T1_TR_T_,
	LS_T1_TR_TL,
	LS_T1_TR__L,
	LS_T1_TR_BL,
	LS_T1_T__BR,//58
	LS_T1_T___R,
	LS_T1_T__TR,
	LS_T1_T__TL,
	LS_T1_T___L,
	LS_T1_T__BL,
	LS_T1_TL_BR,//64
	LS_T1_TL__R,
	LS_T1_TL_TR,
	LS_T1_TL_T_,
	LS_T1_TL__L,
	LS_T1_TL_BL,
	LS_T1__L_BR,//70
	LS_T1__L__R,
	LS_T1__L_TR,
	LS_T1__L_T_,
	LS_T1__L_TL,
	LS_T1__L_BL,
	LS_T1_BL_BR,//76
	LS_T1_BL__R,
	LS_T1_BL_TR,
	LS_T1_BL_T_,
	LS_T1_BL_TL,
	LS_T1_BL__L,

	//Bounces
	LS_B1_BR,
	LS_B1__R,
	LS_B1_TR,
	LS_B1_T_,
	LS_B1_TL,
	LS_B1__L,
	LS_B1_BL,

	//Deflected attacks
	LS_D1_BR,
	LS_D1__R,
	LS_D1_TR,
	LS_D1_T_,
	LS_D1_TL,
	LS_D1__L,
	LS_D1_BL,
	LS_D1_B_,

	//Reflected attacks
	LS_V1_BR,
	LS_V1__R,
	LS_V1_TR,
	LS_V1_T_,
	LS_V1_TL,
	LS_V1__L,
	LS_V1_BL,
	LS_V1_B_,

	// Broken parries
	LS_H1_T_,//
	LS_H1_TR,
	LS_H1_TL,
	LS_H1_BR,
	LS_H1_B_,
	LS_H1_BL,

	// Knockaways
	LS_K1_T_,//
	LS_K1_TR,
	LS_K1_TL,
	LS_K1_BR,
	LS_K1_BL,

	// Parries
	LS_PARRY_UP,//
	LS_PARRY_UR,
	LS_PARRY_UL,
	LS_PARRY_LR,
	LS_PARRY_LL,

	// Projectile Reflections
	LS_REFLECT_UP,//
	LS_REFLECT_UR,
	LS_REFLECT_UL,
	LS_REFLECT_LR,
	LS_REFLECT_LL,

	LS_MOVE_MAX//
};
using enum saberMoveName_t;
template <> inline constexpr bool enableEnumArithmetic<saberMoveName_t> = true;

void PM_SetSaberMove(saberMoveName_t newMove);

enum class saberQuadrant_t {
	Q_BR,
	Q_R,
	Q_TR,
	Q_T,
	Q_TL,
	Q_L,
	Q_BL,
	Q_B,
	Q_NUM_QUADS
};
using enum saberQuadrant_t;
template <> inline constexpr bool enableEnumArithmetic<saberQuadrant_t> = true;

typedef struct
{
	const char *name;
	animNumber_t animToUse;
	saberQuadrant_t	startQuad;
	saberQuadrant_t	endQuad;
	setAnimFlags_t animSetFlags;
	int blendTime;
	saberBlockType_t blocking;
	saberMoveName_t chain_idle;			// What move to call if the attack button is not pressed at the end of this anim
	saberMoveName_t chain_attack;		// What move to call if the attack button (and nothing else) is pressed
	int trailLength;
} saberMoveData_t;

extern EnumArray<saberMoveData_t, saberMoveName_t, LS_MOVE_MAX> saberMoveData;

#endif	// __WP_SABER_H

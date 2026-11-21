/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
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

// Filename:-	weapons.h
//
// Note that this is now included from both server and game modules, so don't include any other header files
//	within this one that might break stuff...

#ifndef __WEAPONS_H__
#define __WEAPONS_H__

#include "../../code/qcommon/q_shared.h"

enum class weapon_t //# weapon_e
{
	WP_NONE,

	// Player weapons
	WP_SABER,				 // NOTE: lots of code assumes this is the first weapon (... which is crap) so be careful -Ste.
	WP_BRYAR_PISTOL,
	WP_BLASTER,
	WP_DISRUPTOR,
	WP_BOWCASTER,
	WP_REPEATER,
	WP_DEMP2,
	WP_FLECHETTE,
	WP_ROCKET_LAUNCHER,
	WP_THERMAL,
	WP_TRIP_MINE,
	WP_DET_PACK,
	WP_STUN_BATON,

	//NOTE: player can only have up to 16 weapons, anything after that is enemy only

	WP_MELEE,			// Any ol' melee attack

	// NPC enemy weapons

	WP_EMPLACED_GUN,

	WP_BOT_LASER,		// Probe droid	- Laser blast

	WP_TURRET,			// turret guns

	WP_ATST_MAIN,
	WP_ATST_SIDE,

	WP_TIE_FIGHTER,

	WP_RAPID_FIRE_CONC,

	WP_BLASTER_PISTOL,	// apparently some enemy only version of the blaster

//	WP_CHAOTICA_GUARD_GUN,	//			- B/W version of scav rifle for Chaotica's guards.
//	WP_BOT_ROCKET,		// Hunter Seeker - Rocket projectile
//	WP_KLINGON_BLADE,
//	WP_IMPERIAL_BLADE,

//	WP_DESPERADO,			// special holo-weapon
//	WP_PALADIN,			// special holo-weapon

	//# #eol
	WP_NUM_WEAPONS
};
template <> inline constexpr bool enableEnumArithmetic<weapon_t> = true;

inline constexpr weapon_t FIRST_WEAPON = weapon_t::WP_SABER;		// this is the first weapon for next and prev weapon switching
inline constexpr weapon_t MAX_PLAYER_WEAPONS = weapon_t::WP_STUN_BATON;	// this is the max you can switch to and get with the give all.

// AMMO_NONE must be first and AMMO_MAX must be last, cause weapon load validates based off of these vals
enum class ammo_t //# ammo_e
{
	AMMO_NONE,
	AMMO_FORCE,		// AMMO_PHASER
	AMMO_BLASTER,	// AMMO_STARFLEET,
	AMMO_POWERCELL,	// AMMO_ALIEN,
	AMMO_METAL_BOLTS,
	AMMO_ROCKETS,
	AMMO_EMPLACED,
	AMMO_THERMAL,
	AMMO_TRIPMINE,
	AMMO_DETPACK,
	AMMO_MAX
};
template <> inline constexpr bool enableEnumArithmetic<ammo_t> = true;


typedef struct weaponData_s
{
	char	classname[32];		// Spawning name
	char	weaponMdl[64];		// Weapon Model
	char	firingSnd[64];		// Sound made when fired
	char	altFiringSnd[64];	// Sound made when alt-fired
//	char	flashSnd[64];		// Sound made by flash
//	char	altFlashSnd[64];	// Sound made by an alt-flash
	char	stopSnd[64];		// Sound made when weapon stops firing
	char	chargeSnd[64];		// sound to start when the weapon initiates the charging sequence
	char	altChargeSnd[64];	// alt sound to start when the weapon initiates the charging sequence
	char	selectSnd[64];		// the sound to play when this weapon gets selected

	ammo_t	ammoIndex;			// Index to proper ammo slot
	int		ammoLow;			// Count when ammo is low

	int		energyPerShot;		// Amount of energy used per shot
	int		fireTime;			// Amount of time between firings
	int		range;				// Range of weapon

	int		altEnergyPerShot;	// Amount of energy used for alt-fire
	int		altFireTime;		// Amount of time between alt-firings
	int		altRange;			// Range of alt-fire

	char	weaponIcon[64];		// Name of weapon icon file
	int		numBarrels;			// how many barrels should we expect for this weapon?

	char	missileMdl[64];		// Missile Model
	char	missileSound[64];	// Missile flight sound
	float  	missileDlight;		// what is says
	vec3_t 	missileDlightColor;	// ditto

	char	alt_missileMdl[64];		// Missile Model
	char	alt_missileSound[64];	// Missile sound
	float  	alt_missileDlight;		// what is says
	vec3_t 	alt_missileDlightColor;	// ditto

	char	missileHitSound[64];	// Missile impact sound
	char	altmissileHitSound[64];	// alt Missile impact sound
	void	*func;
	void	*altfunc;

	char	mMuzzleEffect[64];
	int		mMuzzleEffectID;
	char	mAltMuzzleEffect[64];
	int		mAltMuzzleEffectID;

	// OPENJK ADD
	int		damage;
	int		altDamage;
	int		splashDamage;
	int		altSplashDamage;
	float	splashRadius;
	float	altSplashRadius;

} weaponData_t;


typedef struct ammoData_s
{
	char	icon[64];	// Name of ammo icon file
	int		max;		// Max amount player can hold of ammo
} ammoData_t;

// Bryar Pistol
//--------
inline constexpr int BRYAR_PISTOL_VEL = 1800;
inline constexpr int BRYAR_PISTOL_DAMAGE = 14;
inline constexpr float BRYAR_CHARGE_UNIT = 200.0f;	// bryar charging gives us one more unit every 200ms--if you change this, you'll have to do the same in bg_pmove

// E11 Blaster
//---------
inline constexpr float BLASTER_MAIN_SPREAD = 0.5f;
inline constexpr float BLASTER_ALT_SPREAD = 1.5f;
inline constexpr float BLASTER_NPC_SPREAD = 0.5f;
inline constexpr int BLASTER_VELOCITY = 2300;
inline constexpr float BLASTER_NPC_VEL_CUT = 0.5f;
inline constexpr float BLASTER_NPC_HARD_VEL_CUT = 0.7f;
inline constexpr int BLASTER_DAMAGE = 20;
inline constexpr int BLASTER_NPC_DAMAGE_EASY = 6;
inline constexpr int BLASTER_NPC_DAMAGE_NORMAL = 12; // 14
inline constexpr int BLASTER_NPC_DAMAGE_HARD = 16; // 18

// Tenloss Disruptor
//----------
inline constexpr int DISRUPTOR_MAIN_DAMAGE = 14;
inline constexpr int DISRUPTOR_NPC_MAIN_DAMAGE_EASY = 5;
inline constexpr int DISRUPTOR_NPC_MAIN_DAMAGE_MEDIUM = 10;
inline constexpr int DISRUPTOR_NPC_MAIN_DAMAGE_HARD = 15;

inline constexpr int DISRUPTOR_ALT_DAMAGE = 12;
inline constexpr int DISRUPTOR_NPC_ALT_DAMAGE_EASY = 15;
inline constexpr int DISRUPTOR_NPC_ALT_DAMAGE_MEDIUM = 25;
inline constexpr int DISRUPTOR_NPC_ALT_DAMAGE_HARD = 30;
inline constexpr int DISRUPTOR_ALT_TRACES = 3;		// can go through a max of 3 entities
inline constexpr float DISRUPTOR_CHARGE_UNIT = 150.0f;	// distruptor charging gives us one more unit every 150ms--if you change this, you'll have to do the same in bg_pmove

// Wookie Bowcaster
//----------
inline constexpr int BOWCASTER_DAMAGE = 45;
inline constexpr int BOWCASTER_VELOCITY = 1300;
inline constexpr int BOWCASTER_NPC_DAMAGE_EASY = 12;
inline constexpr int BOWCASTER_NPC_DAMAGE_NORMAL = 24;
inline constexpr int BOWCASTER_NPC_DAMAGE_HARD = 36;
inline constexpr int BOWCASTER_SPLASH_DAMAGE = 0;
inline constexpr int BOWCASTER_SPLASH_RADIUS = 0;
inline constexpr int BOWCASTER_SIZE = 2;

inline constexpr float BOWCASTER_ALT_SPREAD = 5.0f;
inline constexpr float BOWCASTER_VEL_RANGE = 0.3f;
inline constexpr float BOWCASTER_CHARGE_UNIT = 200.0f;	// bowcaster charging gives us one more unit every 200ms--if you change this, you'll have to do the same in bg_pmove

// Heavy Repeater
//----------
inline constexpr float REPEATER_SPREAD = 1.4f;
inline constexpr float REPEATER_NPC_SPREAD = 0.7f;
inline constexpr int REPEATER_DAMAGE = 8;
inline constexpr int REPEATER_VELOCITY = 1600;
inline constexpr int REPEATER_NPC_DAMAGE_EASY = 2;
inline constexpr int REPEATER_NPC_DAMAGE_NORMAL = 4;
inline constexpr int REPEATER_NPC_DAMAGE_HARD = 6;

inline constexpr int REPEATER_ALT_SIZE = 3;	// half of bbox size
inline constexpr int REPEATER_ALT_DAMAGE = 60;
inline constexpr int REPEATER_ALT_SPLASH_DAMAGE = 60;
inline constexpr int REPEATER_ALT_SPLASH_RADIUS = 128;
inline constexpr int REPEATER_ALT_VELOCITY = 1100;
inline constexpr int REPEATER_ALT_NPC_DAMAGE_EASY = 15;
inline constexpr int REPEATER_ALT_NPC_DAMAGE_NORMAL = 30;
inline constexpr int REPEATER_ALT_NPC_DAMAGE_HARD = 45;

// DEMP2
//----------
inline constexpr int DEMP2_DAMAGE = 15;
inline constexpr int DEMP2_VELOCITY = 1800;
inline constexpr int DEMP2_NPC_DAMAGE_EASY = 6;
inline constexpr int DEMP2_NPC_DAMAGE_NORMAL = 12;
inline constexpr int DEMP2_NPC_DAMAGE_HARD = 18;
inline constexpr int DEMP2_SIZE = 2;		// half of bbox size

inline constexpr int DEMP2_ALT_DAMAGE = 15;
inline constexpr float DEMP2_CHARGE_UNIT = 500.0f;	// demp2 charging gives us one more unit every 500ms--if you change this, you'll have to do the same in bg_pmove
inline constexpr int DEMP2_ALT_RANGE = 4096;
inline constexpr int DEMP2_ALT_SPLASHRADIUS = 256;

// Golan Arms Flechette
//---------
inline constexpr int FLECHETTE_SHOTS = 6;
inline constexpr float FLECHETTE_SPREAD = 4.0f;
inline constexpr int FLECHETTE_DAMAGE = 15;
inline constexpr int FLECHETTE_VEL = 3500;
inline constexpr int FLECHETTE_SIZE = 1;

inline constexpr int FLECHETTE_ALT_DAMAGE = 20;
inline constexpr int FLECHETTE_ALT_SPLASH_DAM = 20;
inline constexpr int FLECHETTE_ALT_SPLASH_RAD = 128;

// NOT CURRENTLY USED
inline constexpr int FLECHETTE_MINE_RADIUS_CHECK = 200;
inline constexpr int FLECHETTE_MINE_VEL = 1000;
inline constexpr int FLECHETTE_MINE_DAMAGE = 100;
inline constexpr int FLECHETTE_MINE_SPLASH_DAMAGE = 200;
inline constexpr int FLECHETTE_MINE_SPLASH_RADIUS = 200;

// Personal Rocket Launcher
//---------
inline constexpr int ROCKET_VELOCITY = 900;
inline constexpr int ROCKET_DAMAGE = 100;
inline constexpr int ROCKET_SPLASH_DAMAGE = 100;
inline constexpr int ROCKET_SPLASH_RADIUS = 160;
inline constexpr int ROCKET_NPC_DAMAGE_EASY = 20;
inline constexpr int ROCKET_NPC_DAMAGE_NORMAL = 40;
inline constexpr int ROCKET_NPC_DAMAGE_HARD = 60;
inline constexpr int ROCKET_SIZE = 3;

inline constexpr int ROCKET_ALT_THINK_TIME = 100;

// Emplaced Gun
//--------------
inline constexpr int EMPLACED_VEL = 6000;	// very fast
inline constexpr int EMPLACED_DAMAGE = 150;		// and very damaging
inline constexpr int EMPLACED_SIZE = 5;		// make it easier to hit things

// ATST Main Gun
//--------------
inline constexpr int ATST_MAIN_VEL = 4000;	//
inline constexpr int ATST_MAIN_DAMAGE = 25;		//
inline constexpr int ATST_MAIN_SIZE = 3;		// make it easier to hit things

// ATST Side Gun
//---------------
inline constexpr int ATST_SIDE_MAIN_DAMAGE = 75;
inline constexpr int ATST_SIDE_MAIN_VELOCITY = 1300;
inline constexpr int ATST_SIDE_MAIN_NPC_DAMAGE_EASY = 30;
inline constexpr int ATST_SIDE_MAIN_NPC_DAMAGE_NORMAL = 40;
inline constexpr int ATST_SIDE_MAIN_NPC_DAMAGE_HARD = 50;
inline constexpr int ATST_SIDE_MAIN_SIZE = 4;
inline constexpr int ATST_SIDE_MAIN_SPLASH_DAMAGE = 10;	// yeah, pretty small, either zero out or make it worth having?
inline constexpr int ATST_SIDE_MAIN_SPLASH_RADIUS = 16;	// yeah, pretty small, either zero out or make it worth having?

inline constexpr int ATST_SIDE_ALT_VELOCITY = 1100;
inline constexpr int ATST_SIDE_ALT_NPC_VELOCITY = 600;
inline constexpr int ATST_SIDE_ALT_DAMAGE = 130;

inline constexpr int ATST_SIDE_ROCKET_NPC_DAMAGE_EASY = 30;
inline constexpr int ATST_SIDE_ROCKET_NPC_DAMAGE_NORMAL = 50;
inline constexpr int ATST_SIDE_ROCKET_NPC_DAMAGE_HARD = 90;

inline constexpr int ATST_SIDE_ALT_SPLASH_DAMAGE = 130;
inline constexpr int ATST_SIDE_ALT_SPLASH_RADIUS = 200;
inline constexpr int ATST_SIDE_ALT_ROCKET_SIZE = 5;
inline constexpr float ATST_SIDE_ALT_ROCKET_SPLASH_SCALE = 0.5f;	// scales splash for NPC's

// Stun Baton
//--------------
inline constexpr int STUN_BATON_DAMAGE = 22;
inline constexpr int STUN_BATON_ALT_DAMAGE = 22;
inline constexpr int STUN_BATON_RANGE = 25;

// Laser Trip Mine
//--------------
inline constexpr int LT_DAMAGE = 150;
inline constexpr float LT_SPLASH_RAD = 256.0f;
inline constexpr int LT_SPLASH_DAM = 90;

inline constexpr float LT_VELOCITY = 250.0f;

inline constexpr int PROX_MINE_RADIUS_CHECK = 190;

inline constexpr float LT_SIZE = 3.0f;
inline constexpr int LT_ACTIVATION_DELAY = 1000;

// Thermal Detonator
//--------------
inline constexpr int TD_DAMAGE = 100;
inline constexpr float TD_NPC_DAMAGE_CUT = 0.6f;	// NPC thrown dets deliver only 60% of the damage that a player thrown one does
inline constexpr int TD_SPLASH_RAD = 128;
inline constexpr int TD_SPLASH_DAM = 90;
inline constexpr int TD_VELOCITY = 900;
inline constexpr float TD_MIN_CHARGE = 0.15f;
inline constexpr int TD_TIME = 4000;
inline constexpr int TD_THINK_TIME = 300;		// don't think too often?
#define TD_TEST_RAD			(weaponData[weapon_t::WP_THERMAL].splashRadius * 0.8f) // no sense in auto-blowing up if exactly on the radius edge--it would hardly do any damage

inline constexpr int TD_ALT_DAMAGE = 100;
inline constexpr int TD_ALT_SPLASH_RAD = 128;
inline constexpr int TD_ALT_SPLASH_DAM = 90;


#endif//#ifndef __WEAPONS_H__

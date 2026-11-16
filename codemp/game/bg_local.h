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

#pragma once

// bg_local.h -- local definitions for the bg (both games) files

#define	MIN_WALK_NORMAL	0.7f		// can't walk on very steep slopes

#define	TIMER_LAND		130
#define	TIMER_GESTURE	(34*66+50)

#define	OVERCLIP		1.001f

// all of the locals will be zeroed before each
// pmove, just to make damn sure we don't have
// any differences when running on client or server
typedef struct
{
	vec3_t		forward, right, up;
	float		frametime;

	int			msec;

	bool	walking;
	bool	groundPlane;
	trace_t		groundTrace;

	float		impactSpeed;

	vec3_t		previous_origin;
	vec3_t		previous_velocity;
	int			previous_waterlevel;

	qrand_t		rand;	// PM_irand_timesync's numbers: seeded with the command's time, the same on the server and in
						// the client's prediction (plan step 14)
} pml_t;


extern	pml_t		pml;
#ifdef _CGAME
extern	const playerState_t	*pm_startPs;
extern	int			pm_startLegsAnim, pm_startTorsoAnim;
#endif

// movement parameters
extern	float	pm_stopspeed;
extern	float	pm_duckScale;
extern	float	pm_swimScale;
extern	float	pm_wadeScale;

extern	float	pm_accelerate;
extern	float	pm_airaccelerate;
extern	float	pm_wateraccelerate;
extern	float	pm_flyaccelerate;

extern	float	pm_friction;
extern	float	pm_waterfriction;
extern	float	pm_flightfriction;

extern	int		c_pmove;

extern EnumArray<int[NUM_FORCE_POWERS], forcePowerLevels_t, NUM_FORCE_POWER_LEVELS> forcePowerNeeded;

//PM anim utility functions:
bool PM_SaberInParry( int move );
bool PM_SaberInKnockaway( int move );
bool PM_SaberInReflect( int move );
bool PM_SaberInStart( int move );
bool PM_InSaberAnim( int anim );
bool PM_InKnockDown( playerState_t *ps );
bool PM_PainAnim( int anim );
bool PM_JumpingAnim( int anim );
bool PM_LandingAnim( int anim );
bool PM_SpinningAnim( int anim );
bool PM_InOnGroundAnim ( int anim );
bool PM_InRollComplete( playerState_t *ps, int anim );

int PM_AnimLength( int index, animNumber_t anim );

int PM_GetSaberStance(void);
float PM_GroundDistance(void);
bool PM_SomeoneInFront(trace_t *tr);
saberMoveName_t PM_SaberFlipOverAttackMove(void);
saberMoveName_t PM_SaberJumpAttackMove( void );

void PM_ClipVelocity( vec3_t in, vec3_t normal, vec3_t out, float overbounce );
void PM_AddTouchEnt( int entityNum );
void PM_AddEvent( int newEvent );

bool	PM_SlideMove( bool gravity );
void		PM_StepSlideMove( bool gravity );

void PM_StartTorsoAnim( int anim );
void PM_ContinueLegsAnim( int anim );
void PM_ForceLegsAnim( int anim );

void PM_BeginWeaponChange( int weapon );
void PM_FinishWeaponChange( void );

void PM_SetAnim(int setAnimParts,int anim,int setAnimFlags);

void PM_WeaponLightsaber(void);
void PM_SetSaberMove(short newMove);

void PM_SetForceJumpZStart(float value);

void BG_CycleInven(playerState_t *ps, int direction);


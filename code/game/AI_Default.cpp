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

#include "../cgame/cg_local.h"
#include "b_local.h"
#include "g_nav.h"
#include "Q3_Interface.h"

extern int g_crosshairEntNum;

/*
void NPC_LostEnemyDecideChase(void)

  We lost our enemy and want to drop him but see if we should chase him if we are in the proper bState
*/

void NPC_LostEnemyDecideChase(void)
{
	switch( NPCInfo->behaviorState )
	{
	case bState_t::BS_HUNT_AND_KILL:
		//We were chasing him and lost him, so try to find him
		if ( NPC->enemy == NPCInfo->goalEntity && NPC->enemy->lastWaypoint != WAYPOINT_NONE )
		{//Remember his last valid Wp, then check it out
			//FIXME: Should we only do this if there's no other enemies or we've got LOCKED_ENEMY on?
			NPC_BSSearchStart( NPC->enemy->lastWaypoint, bState_t::BS_SEARCH );
		}
		//If he's not our goalEntity, we're running somewhere else, so lose him
		break;
	default:
		break;
	}
	G_ClearEnemy( NPC );
}
/*
-------------------------
NPC_StandIdle
-------------------------
*/

void NPC_StandIdle( void )
{
/*
	//Must be done with any other animations
	if ( NPC->client->ps.legsAnimTimer != 0 )
		return;

	//Not ready to do another one
	if ( TIMER_Done( NPC, "idleAnim" ) == false )
		return;

	animNumber_t anim = NPC->client->ps.legsAnim;

	if ( anim != animNumber_t::BOTH_STAND1 && anim != animNumber_t::BOTH_STAND2 )
		return;

	//FIXME: Account for STAND1 or STAND2 here and set the base anim accordingly
	animNumber_t	baseSeq = ( anim == animNumber_t::BOTH_STAND1 ) ? BOTH_STAND1_RANDOM1 : BOTH_STAND2_RANDOM1;

	//Must have at least one random idle animation
	//NOTENOTE: This relies on proper ordering of animations, which SHOULD be okay
	if ( PM_HasAnimation( NPC, baseSeq ) == false )
		return;

	int	newIdle = Q_irand( 0, MAX_IDLE_ANIMS-1 );

	//FIXME: Technically this could never complete.. but that's not really too likely
	while( 1 )
	{
		if ( PM_HasAnimation( NPC, baseSeq + newIdle ) )
			break;

		newIdle = Q_irand( 0, MAX_IDLE_ANIMS );
	}

	//Start that animation going
	NPC_SetAnim( NPC, setAnimParts_t::SETANIM_BOTH, baseSeq + newIdle, setAnimFlags_t::SETANIM_FLAG_OVERRIDE|setAnimFlags_t::SETANIM_FLAG_HOLD );

	int newTime = PM_AnimLength( NPC->client->clientInfo.animFileIndex, baseSeq + newIdle );

	//Don't do this again for a random amount of time
	TIMER_Set( NPC, "idleAnim", newTime + Q_irand( 2000, 10000 ) );
*/
}

bool NPC_StandTrackAndShoot (gentity_t *NPC, bool canDuck)
{
	bool	attack_ok = false;
	bool	duck_ok = false;
	bool	faced = false;
	float		attack_scale = 1.0;

	//First see if we're hurt bad- if so, duck
	//FIXME: if even when ducked, we can shoot someone, we should.
	//Maybe is can be shot even when ducked, we should run away to the nearest cover?
	if ( canDuck )
	{
		if ( NPC->health < 20 )
		{
		//	if( NPC->svFlags&SVF_HEALING || Q_flrand(0.0f, 1.0f) )
			if( Q_flrand(0.0f, 1.0f) )
			{
				duck_ok = true;
			}
		}
		else if ( NPC->health < 40 )
		{
//			if ( NPC->svFlags&SVF_HEALING )
//			{//Medic is on the way, get down!
//				duck_ok = true;
//			}
		}
	}

	//NPC_CheckEnemy( true, false );

	if ( !duck_ok )
	{//made this whole part a function call
		attack_ok = NPC_CheckCanAttack( attack_scale, true );
		faced = true;
	}

	if ( canDuck && (duck_ok || (!attack_ok && client->fireDelay == 0)) && ucmd.upmove != -127 )
	{//if we didn't attack check to duck if we're not already
		if( !duck_ok )
		{
			if ( NPC->enemy->client )
			{
				if ( NPC->enemy->enemy == NPC )
				{
					if ( Any( NPC->enemy->client->buttons & buttonFlags_t::BUTTON_ATTACK ) )
					{//FIXME: determine if enemy fire angles would hit me or get close
						if ( NPC_CheckDefend( 1.0 ) )//FIXME: Check self-preservation?  Health?
						{
							duck_ok = true;
						}
					}
				}
			}
		}

		if ( duck_ok )
		{//duck and don't shoot
			attack_ok = false;
			ucmd.upmove = -127;
			NPCInfo->duckDebounceTime = level.time + 1000;//duck for a full second
		}
	}

	return faced;
}


void NPC_BSIdle( void )
{
	//FIXME if there is no nav data, we need to do something else
	// if we're stuck, try to move around it
	if ( UpdateGoal() )
	{
		NPC_MoveToGoal( true );
	}

	if ( ( ucmd.forwardmove == 0 ) && ( ucmd.rightmove == 0 ) && ( ucmd.upmove == 0 ) )
	{
//		NPC_StandIdle();
	}

	NPC_UpdateAngles( true, true );
	ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
}

void NPC_BSRun (void)
{
	//FIXME if there is no nav data, we need to do something else
	// if we're stuck, try to move around it
	if ( UpdateGoal() )
	{
		NPC_MoveToGoal( true );
	}

	NPC_UpdateAngles( true, true );
}

void NPC_BSStandGuard (void)
{
	//FIXME: Use Snapshot info
	if ( NPC->enemy == nullptr )
	{//Possible to pick one up by being shot
		if( Q_flrand(0.0f, 1.0f) < 0.5 )
		{
			if(NPC->client->enemyTeam != team_t::TEAM_FREE)
			{
				gentity_t *newenemy = NPC_PickEnemy(
					NPC, NPC->client->enemyTeam,
					(NPC->cantHitEnemyCounter < 10),
					(NPC->client->enemyTeam == team_t::TEAM_PLAYER),
					true);

				//only checks for vis if couldn't hit last enemy
				if(newenemy)
				{
					G_SetEnemy( NPC, newenemy );
				}
			}
		}
	}

	if ( NPC->enemy != nullptr )
	{
		if( NPCInfo->tempBehavior == bState_t::BS_STAND_GUARD )
		{
			NPCInfo->tempBehavior = bState_t::BS_DEFAULT;
		}

		if( NPCInfo->behaviorState == bState_t::BS_STAND_GUARD )
		{
			NPCInfo->behaviorState = bState_t::BS_STAND_AND_SHOOT;
		}
	}

	NPC_UpdateAngles( true, true );
}

/*
-------------------------
NPC_BSHuntAndKill
-------------------------
*/

void NPC_BSHuntAndKill( void )
{
	bool	turned = false;
	vec3_t		vec;
	float		enemyDist;
	visibility_t	oEVis;
	animNumber_t			curAnim;

	NPC_CheckEnemy( (NPCInfo->tempBehavior != bState_t::BS_HUNT_AND_KILL), false );//don't find new enemy if this is tempbehav

	if ( NPC->enemy )
	{
		oEVis = enemyVisibility = NPC_CheckVisibility ( NPC->enemy, visCheckFlags_t::CHECK_FOV|visCheckFlags_t::CHECK_SHOOT );//CHECK_360|//CHECK_PVS|
		if(enemyVisibility > visibility_t::VIS_PVS)
		{
			if ( !NPC_EnemyTooFar( NPC->enemy, 0, true ) )
			{//Enemy is close enough to shoot - FIXME: this next func does this also, but need to know here for info on whether ot not to turn later
				NPC_CheckCanAttack( 1.0, false );
				turned = true;
			}
		}

		curAnim = NPC->client->ps.legsAnim;
		if(curAnim != animNumber_t::BOTH_ATTACK1 && curAnim != animNumber_t::BOTH_ATTACK2 && curAnim != animNumber_t::BOTH_ATTACK3 && curAnim != animNumber_t::BOTH_MELEE1 && curAnim != animNumber_t::BOTH_MELEE2 )
		{//Don't move toward enemy if we're in a full-body attack anim
			//FIXME, use IdealDistance to determin if we need to close distance
			VectorSubtract(NPC->enemy->currentOrigin, NPC->currentOrigin, vec);
			enemyDist = VectorLength(vec);
			if( enemyDist > 48 && ((enemyDist*1.5)*(enemyDist*1.5) >= NPC_MaxDistSquaredForWeapon() ||
				oEVis != visibility_t::VIS_SHOOT ||
				//!Any(ucmd.buttons & buttonFlags_t::BUTTON_ATTACK) ||
				enemyDist > IdealDistance(NPC)*3 ) )
			{//We should close in?
				NPCInfo->goalEntity = NPC->enemy;

				NPC_MoveToGoal( true );
			}
			else if(enemyDist < IdealDistance(NPC))
			{//We should back off?
				//if(Any( ucmd.buttons & buttonFlags_t::BUTTON_ATTACK ))
				{
					NPCInfo->goalEntity = NPC->enemy;
					NPCInfo->goalRadius = 12;
					NPC_MoveToGoal( true );

					ucmd.forwardmove *= -1;
					ucmd.rightmove *= -1;
					VectorScale( NPC->client->ps.moveDir, -1, NPC->client->ps.moveDir );

					ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
				}
			}//otherwise, stay where we are
		}
	}
	else
	{//ok, stand guard until we find an enemy
		if( NPCInfo->tempBehavior == bState_t::BS_HUNT_AND_KILL )
		{
			NPCInfo->tempBehavior = bState_t::BS_DEFAULT;
		}
		else
		{
			NPCInfo->tempBehavior = bState_t::BS_STAND_GUARD;
			NPC_BSStandGuard();
		}
		return;
	}

	if(!turned)
	{
		NPC_UpdateAngles(true, true);
	}
}

void NPC_BSStandAndShoot (void)
{
	//FIXME:
	//When our numbers outnumber enemies 3 to 1, or only one of them,
	//go into hunt and kill mode

	//FIXME:
	//When they're all dead, go to some script or wander off to sickbay?

	if(NPC->client->playerTeam != team_t::TEAM_FREE && NPC->client->enemyTeam != team_t::TEAM_FREE)
	{
		//FIXME: don't realize this right away- or else enemies show up and we're standing around
		/*
		if( teamNumbers[NPC->enemyTeam] == 0 )
		{//ok, stand guard until we find another enemy
			//reset our rush counter
			teamCounter[NPC->playerTeam] = 0;
			NPCInfo->tempBehavior = bState_t::BS_STAND_GUARD;
			NPC_BSStandGuard();
			return;
		}*/
		/*
		//FIXME: whether to do this or not should be settable
		else if( NPC->playerTeam != TEAM_BORG )//Borg don't rush
		{
		//FIXME: In case reinforcements show up, we should wait a few seconds
		//and keep checking before rushing!
		//Also: what if not everyone on our team is going after playerTeam?
		//Also: our team count includes medics!
			if(NPC->health > 25)
			{//Can we rush the enemy?
				if(teamNumbers[NPC->enemyTeam] == 1 ||
					teamNumbers[NPC->playerTeam] >= teamNumbers[NPC->enemyTeam]*3)
				{//Only one of them or we outnumber 3 to 1
					if(teamStrength[NPC->playerTeam] >= 75 ||
						(teamStrength[NPC->playerTeam] >= 50 && teamStrength[NPC->playerTeam] > teamStrength[NPC->enemyTeam]))
					{//Our team is strong enough to rush
						teamCounter[NPC->playerTeam]++;
						if(teamNumbers[NPC->playerTeam] * 17 <= teamCounter[NPC->playerTeam])
						{//ok, we waited 1.7 think cycles on average and everyone is go, let's do it!
							//FIXME: Should we do this to everyone on our team?
							NPCInfo->behaviorState = bState_t::BS_HUNT_AND_KILL;
							//FIXME: if the tide changes, we should retreat!
							//FIXME: when do we reset the counter?
							NPC_BSHuntAndKill ();
							return;
						}
					}
					else//Oops!  Something's wrong, reset the counter to rush
						teamCounter[NPC->playerTeam] = 0;
				}
				else//Oops!  Something's wrong, reset the counter to rush
					teamCounter[NPC->playerTeam] = 0;
			}
		}
		*/
	}

	NPC_CheckEnemy(true, false);

	if(NPCInfo->duckDebounceTime > level.time && NPC->client->ps.weapon != weapon_t::WP_SABER )
	{
		ucmd.upmove = -127;
		if(NPC->enemy)
		{
			NPC_CheckCanAttack(1.0, true);
		}
		return;
	}

	if(NPC->enemy)
	{
		if(!NPC_StandTrackAndShoot( NPC, true ))
		{//That func didn't update our angles
			NPCInfo->desiredYaw = NPC->client->ps.viewangles[YAW];
			NPCInfo->desiredPitch = NPC->client->ps.viewangles[PITCH];
			NPC_UpdateAngles(true, true);
		}
	}
	else
	{
		NPCInfo->desiredYaw = NPC->client->ps.viewangles[YAW];
		NPCInfo->desiredPitch = NPC->client->ps.viewangles[PITCH];
		NPC_UpdateAngles(true, true);
//		NPC_BSIdle();//only moves if we have a goal
	}
}

void NPC_BSRunAndShoot (void)
{
	/*if(NPC->playerTeam && NPC->enemyTeam)
	{
		//FIXME: don't realize this right away- or else enemies show up and we're standing around
		if( teamNumbers[NPC->enemyTeam] == 0 )
		{//ok, stand guard until we find another enemy
			//reset our rush counter
			teamCounter[NPC->playerTeam] = 0;
			NPCInfo->tempBehavior = bState_t::BS_STAND_GUARD;
			NPC_BSStandGuard();
			return;
		}
	}*/

	//NOTE: are we sure we want ALL run and shoot people to move this way?
	//Shouldn't it check to see if we have an enemy and our enemy is our goal?!
	//Moved that check into NPC_MoveToGoal
	//NPCInfo->combatMove = true;

	NPC_CheckEnemy( true, false );

	if ( NPCInfo->duckDebounceTime > level.time ) // && NPCInfo->hidingGoal )
	{
		ucmd.upmove = -127;
		if ( NPC->enemy )
		{
			NPC_CheckCanAttack( 1.0, false );
		}
		return;
	}

	if ( NPC->enemy )
	{
		int monitor = NPC->cantHitEnemyCounter;
		NPC_StandTrackAndShoot( NPC, false );//(NPCInfo->hidingGoal != nullptr) );

		if ( !Any(ucmd.buttons & buttonFlags_t::BUTTON_ATTACK) && ucmd.upmove >= 0 && NPC->cantHitEnemyCounter > monitor )
		{//not crouching and not firing
			vec3_t	vec;

			VectorSubtract( NPC->enemy->currentOrigin, NPC->currentOrigin, vec );
			vec[2] = 0;
			if ( VectorLength( vec ) > 128 || NPC->cantHitEnemyCounter >= 10 )
			{//run at enemy if too far away
				//The cantHitEnemyCounter getting high has other repercussions
				//100 (10 seconds) will make you try to pick a new enemy...
				//But we're chasing, so we clamp it at 50 here
				if ( NPC->cantHitEnemyCounter > 60 )
				{
					NPC->cantHitEnemyCounter = 60;
				}

				if ( NPC->cantHitEnemyCounter >= (NPCInfo->stats.aggression+1) * 10 )
				{
					NPC_LostEnemyDecideChase();
				}

				//chase and face
				ucmd.angles[YAW] = 0;
				ucmd.angles[PITCH] = 0;
				NPCInfo->goalEntity = NPC->enemy;
				NPCInfo->goalRadius = 12;
				NPC_MoveToGoal( true );
				NPC_UpdateAngles(true, true);
			}
			else
			{
				//FIXME: this could happen if they're just on the other side
				//of a thin wall or something else blocking out shot.  That
				//would make us just stand there and not go around it...
				//but maybe it's okay- might look like we're waiting for
				//him to come out...?
				//Current solution: runs around if cantHitEnemyCounter gets
				//to 10 (1 second).
			}
		}
		else
		{//Clear the can't hit enemy counter here
			NPC->cantHitEnemyCounter = 0;
		}
	}
	else
	{
		if ( NPCInfo->tempBehavior == bState_t::BS_HUNT_AND_KILL )
		{//lost him, go back to what we were doing before
			NPCInfo->tempBehavior = bState_t::BS_DEFAULT;
			return;
		}

//		NPC_BSRun();//only moves if we have a goal
	}
}

//Simply turn until facing desired angles
void NPC_BSFace (void)
{
	//FIXME: once you stop sending turning info, they reset to whatever their delta_angles was last????
	//Once this is over, it snaps back to what it was facing before- WHY???
	if( NPC_UpdateAngles ( true, true ) )
	{
		Q3_TaskIDComplete( NPC, taskID_t::TID_BSTATE );

		NPCInfo->desiredYaw = client->ps.viewangles[YAW];
		NPCInfo->desiredPitch = client->ps.viewangles[PITCH];

		NPCInfo->aimTime = 0;//ok to turn normally now
	}
}

void NPC_BSPointShoot (bool shoot)
{//FIXME: doesn't check for clear shot...
	vec3_t	muzzle, dir, angles, org;

	if ( !NPC->enemy || !NPC->enemy->inuse || (NPC->enemy->NPC && NPC->enemy->health <= 0) )
	{//FIXME: should still keep shooting for a second or two after they actually die...
		Q3_TaskIDComplete( NPC, taskID_t::TID_BSTATE );
		goto finished;
		return;
	}

	CalcEntitySpot(NPC, spot_t::SPOT_WEAPON, muzzle);
	CalcEntitySpot(NPC->enemy, spot_t::SPOT_HEAD, org);//Was spot_org
	//Head is a little high, so let's aim for the chest:
	if ( NPC->enemy->client )
	{
		org[2] -= 12;//NOTE: is this enough?
	}

	VectorSubtract(org, muzzle, dir);
	vectoangles(dir, angles);

	switch( NPC->client->ps.weapon )
	{
	case weapon_t::WP_NONE:
	case weapon_t::WP_MELEE:
	case weapon_t::WP_TUSKEN_STAFF:
	case weapon_t::WP_SABER:
		//don't do any pitch change if not holding a firing weapon
		break;
	default:
		NPCInfo->desiredPitch = NPCInfo->lockedDesiredPitch = AngleNormalize360(angles[PITCH]);
		break;
	}

	NPCInfo->desiredYaw = NPCInfo->lockedDesiredYaw = AngleNormalize360(angles[YAW]);

	if ( NPC_UpdateAngles ( true, true ) )
	{//FIXME: if angles clamped, this may never work!
		//NPCInfo->shotTime = NPC->attackDebounceTime = 0;

		if ( shoot )
		{//FIXME: needs to hold this down if using a weapon that requires it, like phaser...
			ucmd.buttons |= buttonFlags_t::BUTTON_ATTACK;
		}

		if ( !shoot || !Any( NPC->svFlags & svFlags_t::SVF_LOCKEDENEMY ) )
		{//If locked_enemy is on, dont complete until it is destroyed...
			Q3_TaskIDComplete( NPC, taskID_t::TID_BSTATE );
			goto finished;
		}
	}
	else if ( shoot && Any( NPC->svFlags & svFlags_t::SVF_LOCKEDENEMY ) )
	{//shooting them till their dead, not aiming right at them yet...
		/*
		bool movingTarget = false;

		if ( NPC->enemy->client )
		{
			if ( VectorLengthSquared( NPC->enemy->client->ps.velocity ) )
			{
				movingTarget = true;
			}
		}
		else if ( VectorLengthSquared( NPC->enemy->s.pos.trDelta ) )
		{
			movingTarget = true;
		}

		if (movingTarget )
		*/
		{
			float	dist = VectorLength( dir );
			float	yawMiss, yawMissAllow = NPC->enemy->maxs[0];
			float	pitchMiss, pitchMissAllow = (NPC->enemy->maxs[2] - NPC->enemy->mins[2])/2;

			if ( yawMissAllow < 8.0f )
			{
				yawMissAllow = 8.0f;
			}

			if ( pitchMissAllow < 8.0f )
			{
				pitchMissAllow = 8.0f;
			}

			yawMiss = Q_tanf(DEG2RAD(AngleDelta ( NPC->client->ps.viewangles[YAW], NPCInfo->desiredYaw ))) * dist;
			pitchMiss = Q_tanf(DEG2RAD(AngleDelta ( NPC->client->ps.viewangles[PITCH], NPCInfo->desiredPitch))) * dist;

			if ( yawMissAllow >= yawMiss && pitchMissAllow > pitchMiss )
			{
				ucmd.buttons |= buttonFlags_t::BUTTON_ATTACK;
			}
		}
	}

	return;

finished:
	NPCInfo->desiredYaw = client->ps.viewangles[YAW];
	NPCInfo->desiredPitch = client->ps.viewangles[PITCH];

	NPCInfo->aimTime = 0;//ok to turn normally now
}

/*
void NPC_BSMove(void)
Move in a direction, face another
*/
void NPC_BSMove(void)
{
	gentity_t	*goal = nullptr;

	NPC_CheckEnemy(true, false);
	if(NPC->enemy)
	{
		NPC_CheckCanAttack(1.0, false);
	}
	else
	{
		NPC_UpdateAngles(true, true);
	}

	goal = UpdateGoal();
	if(goal)
	{
//		NPCInfo->moveToGoalMod = 1.0;

		NPC_SlideMoveToGoal();
	}
}

/*
void NPC_BSShoot(void)
Move in a direction, face another
*/

void NPC_BSShoot(void)
{
//	NPC_BSMove();

	enemyVisibility = visibility_t::VIS_SHOOT;

	if ( client->ps.weaponstate != weaponstate_t::WEAPON_READY && client->ps.weaponstate != weaponstate_t::WEAPON_FIRING )
	{
		client->ps.weaponstate = weaponstate_t::WEAPON_READY;
	}

	WeaponThink(true);
}

/*
void NPC_BSPatrol( void )

  Same as idle, but you look for enemies every "vigilance"
  using your angles, HFOV, VFOV and visrange, and listen for sounds within earshot...
*/
void NPC_BSPatrol( void )
{
	//int	alertEventNum;

	if(level.time > NPCInfo->enemyCheckDebounceTime)
	{
		NPCInfo->enemyCheckDebounceTime = level.time + (NPCInfo->stats.vigilance * 1000);
		NPC_CheckEnemy(true, false);
		if(NPC->enemy)
		{//FIXME: do anger script
			NPCInfo->behaviorState = bState_t::BS_HUNT_AND_KILL;
			//NPC_AngerSound();
			return;
		}
	}

	//FIXME: Implement generic sound alerts
	/*
	alertEventNum = NPC_CheckAlertEvents( true, true );
	if( alertEventNum != -1 )
	{//If we heard something, see if we should check it out
		if ( NPC_CheckInvestigate( alertEventNum ) )
		{
			return;
		}
	}
	*/

	NPCInfo->investigateSoundDebounceTime = 0;
	//FIXME if there is no nav data, we need to do something else
	// if we're stuck, try to move around it
	if ( UpdateGoal() )
	{
		NPC_MoveToGoal( true );
	}

	NPC_UpdateAngles( true, true );

	ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
}

/*
void NPC_BSDefault(void)
	uses various scriptflags to determine how an npc should behave
*/
extern void NPC_CheckGetNewWeapon( void );
extern void NPC_BSST_Attack( void );

void NPC_BSDefault( void )
{
//	vec3_t		enemyDir;
//	float		enemyDist;
//	float		shootDist;
//	bool	enemyFOV = false;
//	bool	enemyShotFOV = false;
//	bool	enemyPVS = false;
//	vec3_t		enemyHead;
//	vec3_t		muzzle;
//	bool	enemyLOS = false;
//	bool	enemyCS = false;
	bool	move = true;
//	bool	shoot = false;


	if( Any( NPCInfo->scriptFlags & scriptFlags_t::SCF_FIRE_WEAPON ) )
	{
		WeaponThink( true );
	}

	if ( Any( NPCInfo->scriptFlags & scriptFlags_t::SCF_FORCED_MARCH ) )
	{//being forced to walk
		if( NPC->client->ps.torsoAnim != animNumber_t::TORSO_SURRENDER_START )
		{
			NPC_SetAnim( NPC, setAnimParts_t::SETANIM_TORSO, animNumber_t::TORSO_SURRENDER_START, setAnimFlags_t::SETANIM_FLAG_HOLD );
		}
	}
	//look for a new enemy if don't have one and are allowed to look, validate current enemy if have one
	NPC_CheckEnemy( Any(NPCInfo->scriptFlags&scriptFlags_t::SCF_LOOK_FOR_ENEMIES), false );
	if ( !NPC->enemy )
	{//still don't have an enemy
		if ( !Any(NPCInfo->scriptFlags&scriptFlags_t::SCF_IGNORE_ALERTS) )
		{//check for alert events
			//FIXME: Check Alert events, see if we should investigate or just look at it
			int alertEvent = NPC_CheckAlertEvents( true, true, -1, true, alertEventLevel_e::AEL_DISCOVERED );

			//There is an event to look at
			if ( alertEvent >= 0 )//&& level.alertEvents[alertEvent].ID != NPCInfo->lastAlertID )
			{//heard/saw something
				if ( level.alertEvents[alertEvent].level >= alertEventLevel_e::AEL_DISCOVERED && Any(NPCInfo->scriptFlags&scriptFlags_t::SCF_LOOK_FOR_ENEMIES) )
				{//was a big event
					if ( level.alertEvents[alertEvent].owner
						&& level.alertEvents[alertEvent].owner != NPC
						&& level.alertEvents[alertEvent].owner->client
						&& level.alertEvents[alertEvent].owner->health >= 0
						&& level.alertEvents[alertEvent].owner->client->playerTeam == NPC->client->enemyTeam )
					{//an enemy
						G_SetEnemy( NPC, level.alertEvents[alertEvent].owner );
					}
				}
				else
				{//FIXME: investigate lesser events
				}
			}
			//FIXME: also check our allies' condition?
		}
	}

	if ( NPC->enemy && !Any(NPCInfo->scriptFlags&scriptFlags_t::SCF_FORCED_MARCH) )
	{
		// just use the stormtrooper attack AI...
		NPC_CheckGetNewWeapon();
		if ( NPC->client->leader
			&& NPCInfo->goalEntity == NPC->client->leader
			&& !Q3_TaskIDPending( NPC, taskID_t::TID_MOVE_NAV ) )
		{
			NPC_ClearGoal();
		}
		NPC_BSST_Attack();
		return;
/*
		//have an enemy
		//FIXME: if one of these fails, meaning we can't shoot, do we really need to do the rest?
		VectorSubtract( NPC->enemy->currentOrigin, NPC->currentOrigin, enemyDir );
		enemyDist = VectorNormalize( enemyDir );
		enemyDist *= enemyDist;
		shootDist = NPC_MaxDistSquaredForWeapon();

		enemyFOV = InFOV( NPC->enemy, NPC, NPCInfo->stats.hfov, NPCInfo->stats.vfov );
		enemyShotFOV = InFOV( NPC->enemy, NPC, 20, 20 );
		enemyPVS = gi.inPVS( NPC->enemy->currentOrigin, NPC->currentOrigin );

		if ( enemyPVS )
		{//in the pvs
			trace_t	tr;

			CalcEntitySpot( NPC->enemy, spot_t::SPOT_HEAD, enemyHead );
			enemyHead[2] -= Q_flrand( 0.0f, NPC->enemy->maxs[2]*0.5f );
			CalcEntitySpot( NPC, spot_t::SPOT_WEAPON, muzzle );
			enemyLOS = NPC_ClearLOS( muzzle, enemyHead );

			gi.trace ( &tr, muzzle, vec3_origin, vec3_origin, enemyHead, NPC->s.number, MASK_SHOT );
			enemyCS = NPC_EvaluateShot( tr.entityNum, true );
		}
		else
		{//skip thr 2 traces since they would have to fail
			enemyLOS = false;
			enemyCS = false;
		}

		if ( enemyCS && enemyShotFOV )
		{//can hit enemy if we want
			NPC->cantHitEnemyCounter = 0;
		}
		else
		{//can't hit
			NPC->cantHitEnemyCounter++;
		}

		if ( enemyCS && enemyShotFOV && enemyDist < shootDist )
		{//can shoot
			shoot = true;
			if ( NPCInfo->goalEntity == NPC->enemy )
			{//my goal is my enemy and I have a clear shot, no need to chase right now
				move = false;
			}
		}
		else
		{//don't shoot yet, keep chasing
			shoot = false;
			move = true;
		}

		//shoot decision
		if ( !Any(NPCInfo->scriptFlags&scriptFlags_t::SCF_DONT_FIRE) )
		{//try to shoot
			if ( NPC->enemy )
			{
				if ( shoot )
				{
					if( !Any(NPCInfo->scriptFlags & scriptFlags_t::SCF_FIRE_WEAPON) ) // we've already fired, no need to do it again here
					{
						WeaponThink( true );
					}
				}
			}
		}

		//chase decision
		if ( Any( NPCInfo->scriptFlags & scriptFlags_t::SCF_CHASE_ENEMIES ) )
		{//go after him
			NPCInfo->goalEntity = NPC->enemy;
			//FIXME: don't need to chase when have a clear shot and in range?
			if ( !enemyCS && NPC->cantHitEnemyCounter > 60 )
			{//haven't been able to shoot enemy for about 6 seconds, need to do something
				//FIXME: combat points?  Just chase?
				if ( enemyPVS )
				{//in my PVS, just pick a combat point
					//FIXME: implement
				}
				else
				{//just chase him
				}
			}
			//FIXME: in normal behavior, should we use combat Points?  Do we care?  Is anyone actually going to ever use this AI?
		}
		else if ( NPC->cantHitEnemyCounter > 60 )
		{//pick a new one
			NPC_CheckEnemy( true, false );
		}

		if ( enemyPVS && enemyLOS )//&& !enemyShotFOV )
		{//have a clear LOS to him//, but not looking at him
			//Find the desired angles
			vec3_t	angles;

			GetAnglesForDirection( muzzle, enemyHead, angles );

			NPCInfo->desiredYaw		= AngleNormalize180( angles[YAW] );
			NPCInfo->desiredPitch	= AngleNormalize180( angles[PITCH] );
		}
		*/
	}

	if ( UpdateGoal() )
	{//have a goal
		if ( !NPC->enemy
			&& NPC->client->leader
			&& NPCInfo->goalEntity == NPC->client->leader
			&& !Q3_TaskIDPending( NPC, taskID_t::TID_MOVE_NAV ) )
		{
			NPC_BSFollowLeader();
		}
		else
		{
			//set angles
			if ( Any(NPCInfo->scriptFlags & scriptFlags_t::SCF_FACE_MOVE_DIR) || NPCInfo->goalEntity != NPC->enemy )
			{//face direction of movement, NOTE: default behavior when not chasing enemy
				NPCInfo->combatMove = false;
			}
			else
			{//face goal.. FIXME: what if have a navgoal but want to face enemy while moving?  Will this do that?
				vec3_t	dir, angles;

				NPCInfo->combatMove = false;

				VectorSubtract( NPCInfo->goalEntity->currentOrigin, NPC->currentOrigin, dir );
				vectoangles( dir, angles );
				NPCInfo->desiredYaw = angles[YAW];
				if ( NPCInfo->goalEntity == NPC->enemy )
				{
					NPCInfo->desiredPitch = angles[PITCH];
				}
			}

			//set movement
			//override default walk/run behavior
			//NOTE: redundant, done in NPC_ApplyScriptFlags
			if ( Any( NPCInfo->scriptFlags & scriptFlags_t::SCF_RUNNING ) )
			{
				ucmd.buttons &= ~buttonFlags_t::BUTTON_WALKING;
			}
			else if ( Any( NPCInfo->scriptFlags & scriptFlags_t::SCF_WALKING ) )
			{
				ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
			}
			else if ( NPCInfo->goalEntity == NPC->enemy )
			{
				ucmd.buttons &= ~buttonFlags_t::BUTTON_WALKING;
			}
			else
			{
				ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
			}

			if ( Any( NPCInfo->scriptFlags & scriptFlags_t::SCF_FORCED_MARCH ) )
			{//being forced to walk
				if ( g_crosshairEntNum != NPC->s.number )
				{//don't walk if player isn't aiming at me
					move = false;
				}
			}

			if ( move )
			{
				//move toward goal
				NPC_MoveToGoal( true );
			}
		}
	}
	else if ( !NPC->enemy && NPC->client->leader )
	{
		NPC_BSFollowLeader();
	}

	//update angles
	NPC_UpdateAngles( true, true );
}
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

#include "b_local.h"

// These define the working combat range for these suckers
constexpr int MIN_DISTANCE = 54;
constexpr int MIN_DISTANCE_SQR = MIN_DISTANCE * MIN_DISTANCE;

constexpr int MAX_DISTANCE = 128;


/*
-------------------------
NPC_MineMonster_Precache
-------------------------
*/
void NPC_MineMonster_Precache( void )
{
	int i;

	for ( i = 0; i < 4; i++ )
	{
		G_SoundIndex( va("sound/chars/mine/misc/bite%i.wav", i+1 ));
		G_SoundIndex( va("sound/chars/mine/misc/miss%i.wav", i+1 ));
	}
}


/*
-------------------------
MineMonster_Idle
-------------------------
*/
void MineMonster_Idle( void )
{
	if ( UpdateGoal() )
	{
		NPCS.ucmd.buttons &= ~buttonFlags_t::BUTTON_WALKING;
		NPC_MoveToGoal( true );
	}
}


/*
-------------------------
MineMonster_Patrol
-------------------------
*/
void MineMonster_Patrol( void )
{
	vec3_t dif;

	NPC_SetLocalState( *NPCS.NPCInfo, monsterLocalState_t::LSTATE_CLEAR );

	//If we have somewhere to go, then do that
	if ( UpdateGoal() )
	{
		NPCS.ucmd.buttons &= ~buttonFlags_t::BUTTON_WALKING;
		NPC_MoveToGoal( true );
	}
	else
	{
		if ( TIMER_Done( NPCS.NPC, "patrolTime" ))
		{
			TIMER_Set( NPCS.NPC, "patrolTime", Q_flrand(-1.0f, 1.0f) * 5000 + 5000 );
		}
	}

	//rwwFIXMEFIXME: Care about all clients, not just client 0
	//OJKFIXME: clientnum 0
	VectorSubtract( g_entities[0].r.currentOrigin, NPCS.NPC->r.currentOrigin, dif );

	if ( VectorLengthSquared( dif ) < 256 * 256 )
	{
		G_SetEnemy( NPCS.NPC, &g_entities[0] );
	}

	if ( NPC_CheckEnemyExt( true ) == false )
	{
		MineMonster_Idle();
		return;
	}
}

/*
-------------------------
MineMonster_Move
-------------------------
*/
void MineMonster_Move( bool visible )
{
	if ( NPC_LocalState<monsterLocalState_t>( *NPCS.NPCInfo ) != monsterLocalState_t::LSTATE_WAITING )
	{
		NPCS.NPCInfo->goalEntity = NPCS.NPC->enemy;
		NPC_MoveToGoal( true );
		NPCS.NPCInfo->goalRadius = MAX_DISTANCE;	// just get us within combat range
	}
}

//---------------------------------------------------------
void MineMonster_TryDamage( gentity_t *enemy, int damage )
{
	vec3_t	end, dir;
	trace_t	tr;

	if ( !enemy )
	{
		return;
	}

	AngleVectors( NPCS.NPC->client->ps.viewangles, dir, nullptr, nullptr );
	VectorMA( NPCS.NPC->r.currentOrigin, MIN_DISTANCE, dir, end );

	// Should probably trace from the mouth, but, ah well.
	trap->Trace( &tr, NPCS.NPC->r.currentOrigin, vec3_origin, vec3_origin, end, NPCS.NPC->s.number, MASK_SHOT, false, g2TraceFlags_t::G2TRFLAG_NONE, 0 );

	if ( tr.entityNum >= 0 && tr.entityNum < ENTITYNUM_NONE )
	{
		G_Damage( &g_entities[tr.entityNum], NPCS.NPC, NPCS.NPC, dir, tr.endpos, damage, damageFlags_t::DAMAGE_NO_KNOCKBACK, meansOfDeath_t::MOD_MELEE );
		G_Sound( NPCS.NPC, soundChannel_t::CHAN_AUTO, G_EffectIndex(va("sound/chars/mine/misc/bite%i.wav", Q_irand(1,4))));
	}
	else
	{
		G_Sound( NPCS.NPC, soundChannel_t::CHAN_AUTO, G_EffectIndex(va("sound/chars/mine/misc/miss%i.wav", Q_irand(1,4))));
	}
}

//------------------------------
void MineMonster_Attack( void )
{
	if ( !TIMER_Exists( NPCS.NPC, "attacking" ))
	{
		// usually try and play a jump attack if the player somehow got above them....or just really rarely
		if ( NPCS.NPC->enemy && ((NPCS.NPC->enemy->r.currentOrigin[2] - NPCS.NPC->r.currentOrigin[2] > 10 && Q_flrand(0.0f, 1.0f) > 0.1f )
						|| Q_flrand(0.0f, 1.0f) > 0.8f ))
		{
			// Going to do ATTACK4
			TIMER_Set( NPCS.NPC, "attacking", 1750 + Q_flrand(0.0f, 1.0f) * 200 );
			NPC_SetAnim( NPCS.NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_ATTACK4, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD );

			TIMER_Set( NPCS.NPC, "attack2_dmg", 950 ); // level two damage
		}
		else if ( Q_flrand(0.0f, 1.0f) > 0.5f )
		{
			if ( Q_flrand(0.0f, 1.0f) > 0.8f )
			{
				// Going to do ATTACK3, (rare)
				TIMER_Set( NPCS.NPC, "attacking", 850 );
				NPC_SetAnim( NPCS.NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_ATTACK3, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD );

				TIMER_Set( NPCS.NPC, "attack2_dmg", 400 ); // level two damage
			}
			else
			{
				// Going to do ATTACK1
				TIMER_Set( NPCS.NPC, "attacking", 850 );
				NPC_SetAnim( NPCS.NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_ATTACK1, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD );

				TIMER_Set( NPCS.NPC, "attack1_dmg", 450 ); // level one damage
			}
		}
		else
		{
			// Going to do ATTACK2
			TIMER_Set( NPCS.NPC, "attacking", 1250 );
			NPC_SetAnim( NPCS.NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_ATTACK2, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD );

			TIMER_Set( NPCS.NPC, "attack1_dmg", 700 ); // level one damage
		}
	}
	else
	{
		// Need to do delayed damage since the attack animations encapsulate multiple mini-attacks
		if ( TIMER_Done2( NPCS.NPC, "attack1_dmg", true ))
		{
			MineMonster_TryDamage( NPCS.NPC->enemy, 5 );
		}
		else if ( TIMER_Done2( NPCS.NPC, "attack2_dmg", true ))
		{
			MineMonster_TryDamage( NPCS.NPC->enemy, 10 );
		}
	}

	// Just using this to remove the attacking flag at the right time
	TIMER_Done2( NPCS.NPC, "attacking", true );
}

//----------------------------------
void MineMonster_Combat( void )
{
	float distance;
	bool advance;

	// If we cannot see our target or we have somewhere to go, then do that
	if ( !NPC_ClearLOS4( NPCS.NPC->enemy ) || UpdateGoal( ))
	{
		NPCS.NPCInfo->combatMove = true;
		NPCS.NPCInfo->goalEntity = NPCS.NPC->enemy;
		NPCS.NPCInfo->goalRadius = MAX_DISTANCE;	// just get us within combat range

		NPC_MoveToGoal( true );
		return;
	}

	// Sometimes I have problems with facing the enemy I'm attacking, so force the issue so I don't look dumb
	NPC_FaceEnemy( true );

	distance	= DistanceHorizontalSquared( NPCS.NPC->r.currentOrigin, NPCS.NPC->enemy->r.currentOrigin );

	advance = ( distance > MIN_DISTANCE_SQR ? true : false  );

	if (( advance || NPC_LocalState<monsterLocalState_t>( *NPCS.NPCInfo ) == monsterLocalState_t::LSTATE_WAITING ) && TIMER_Done( NPCS.NPC, "attacking" )) // waiting monsters can't attack
	{
		if ( TIMER_Done2( NPCS.NPC, "takingPain", true ))
		{
			NPC_SetLocalState( *NPCS.NPCInfo, monsterLocalState_t::LSTATE_CLEAR );
		}
		else
		{
			MineMonster_Move( true );
		}
	}
	else
	{
		MineMonster_Attack();
	}
}

/*
-------------------------
NPC_MineMonster_Pain
-------------------------
*/
void NPC_MineMonster_Pain(gentity_t *self, gentity_t *attacker, int damage)
{
	G_AddEvent( self, entity_event_t::EV_PAIN, Q_floor((float)self->health/self->client->pers.maxHealth*100.0f) );

	if ( damage >= 10 )
	{
		TIMER_Remove( self, "attacking" );
		TIMER_Remove( self, "attacking1_dmg" );
		TIMER_Remove( self, "attacking2_dmg" );
		TIMER_Set( self, "takingPain", 1350 );

		VectorCopy( self->NPC->lastPathAngles, self->s.angles );

		NPC_SetAnim( self, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_PAIN1, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD );

		if ( self->NPC )
		{
			NPC_SetLocalState( *self->NPC, monsterLocalState_t::LSTATE_WAITING );
		}
	}
}


/*
-------------------------
NPC_BSMineMonster_Default
-------------------------
*/
void NPC_BSMineMonster_Default( void )
{
	if ( NPCS.NPC->enemy )
	{
		MineMonster_Combat();
	}
	else if ( Any( NPCS.NPCInfo->scriptFlags & scriptFlags_t::SCF_LOOK_FOR_ENEMIES ) )
	{
		MineMonster_Patrol();
	}
	else
	{
		MineMonster_Idle();
	}

	NPC_UpdateAngles( true, true );
}

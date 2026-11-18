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

/*****************************************************************************
 * name:		be_ai_move.h
 *
 * desc:		movement AI
 *
 * $Archive: /source/code/botlib/be_ai_move.h $
 * $Author: osman $
 * $Revision: 1.4 $
 * $Modtime: 10/05/99 3:32p $
 * $Date: 2003/03/15 23:44:00 $
 *
 *****************************************************************************/

#pragma once

//movement types
enum class moveTypeFlags_t
{
	MOVE_NONE = 0,
	MOVE_WALK						= 1,
	MOVE_CROUCH						= 2,
	MOVE_JUMP						= 4,
	MOVE_GRAPPLE					= 8,
	MOVE_ROCKETJUMP					= 16,
	MOVE_BFGJUMP					= 32,
};
using enum moveTypeFlags_t;
template <> inline constexpr bool enableEnumFlags<moveTypeFlags_t> = true;
//move flags
enum class moveFlags_t
{
	MFL_NONE = 0,
	MFL_BARRIERJUMP					= 1,		//bot is performing a barrier jump
	MFL_ONGROUND					= 2,		//bot is in the ground
	MFL_SWIMMING					= 4,		//bot is swimming
	MFL_AGAINSTLADDER				= 8,		//bot is against a ladder
	MFL_WATERJUMP					= 16,		//bot is waterjumping
	MFL_TELEPORTED					= 32,		//bot is being teleported
	MFL_GRAPPLEPULL					= 64,		//bot is being pulled by the grapple
	MFL_ACTIVEGRAPPLE				= 128,		//bot is using the grapple hook
	MFL_GRAPPLERESET				= 256,		//bot has reset the grapple
	MFL_WALK						= 512,		//bot should walk slowly
};
using enum moveFlags_t;
template <> inline constexpr bool enableEnumFlags<moveFlags_t> = true;
// move result flags
enum class moveResultFlags_t
{
	MOVERESULT_NONE = 0,
	MOVERESULT_MOVEMENTVIEW			= 1,		//bot uses view for movement
	MOVERESULT_SWIMVIEW				= 2,		//bot uses view for swimming
	MOVERESULT_WAITING				= 4,		//bot is waiting for something
	MOVERESULT_MOVEMENTVIEWSET		= 8,		//bot has set the view in movement code
	MOVERESULT_MOVEMENTWEAPON		= 16,		//bot uses weapon for movement
	MOVERESULT_ONTOPOFOBSTACLE		= 32,		//bot is ontop of obstacle
	MOVERESULT_ONTOPOF_FUNCBOB		= 64,		//bot is ontop of a func_bobbing
	MOVERESULT_ONTOPOF_ELEVATOR		= 128,		//bot is ontop of an elevator (func_plat)
	MOVERESULT_BLOCKEDBYAVOIDSPOT	= 256,		//bot is blocked by an avoid spot
};
using enum moveResultFlags_t;
template <> inline constexpr bool enableEnumFlags<moveResultFlags_t> = true;
//
#define MAX_AVOIDREACH					1
#define MAX_AVOIDSPOTS					32
// avoid spot types
#define AVOID_CLEAR						0		//clear all avoid spots
#define AVOID_ALWAYS					1		//avoid always
#define AVOID_DONTBLOCK					2		//never totally block
// restult types
enum class moveResultTypeFlags_t
{
	RESULTTYPE_NONE = 0,
	RESULTTYPE_ELEVATORUP			= 1,		//elevator is up
	RESULTTYPE_WAITFORFUNCBOBBING	= 2,		//waiting for func bobbing to arrive
	RESULTTYPE_BADGRAPPLEPATH		= 4,		//grapple path is obstructed
	RESULTTYPE_INSOLIDAREA			= 8,		//stuck in solid area, this is bad
};
using enum moveResultTypeFlags_t;
template <> inline constexpr bool enableEnumFlags<moveResultTypeFlags_t> = true;

//structure used to initialize the movement state
//the or_moveflags MFL_ONGROUND, MFL_TELEPORTED and MFL_WATERJUMP come from the playerstate
typedef struct bot_initmove_s
{
	vec3_t origin;				//origin of the bot
	vec3_t velocity;			//velocity of the bot
	vec3_t viewoffset;			//view offset
	int entitynum;				//entity number of the bot
	int client;					//client number of the bot
	float thinktime;			//time the bot thinks
	presenceFlags_t presencetype;			//presencetype of the bot
	vec3_t viewangles;			//view angles of the bot
	moveFlags_t or_moveflags;			//values ored to the movement flags
} bot_initmove_t;

//NOTE: the ideal_viewangles are only valid if MFL_MOVEMENTVIEW is set
typedef struct bot_moveresult_s
{
	int failure;				//true if movement failed all together
	moveResultTypeFlags_t type;					//failure or blocked type
	int blocked;				//true if blocked by an entity
	int blockentity;			//entity blocking the bot
	travelTypeFlags_t traveltype;				//last executed travel type
	moveResultFlags_t flags;					//result flags
	int weapon;					//weapon used for movement
	vec3_t movedir;				//movement direction
	vec3_t ideal_viewangles;	//ideal viewangles for the movement
} bot_moveresult_t;

#define bot_moveresult_t_cleared(x) bot_moveresult_t x = {0, RESULTTYPE_NONE, 0, 0, TRAVEL_NONE, MOVERESULT_NONE, 0, {0, 0, 0}, {0, 0, 0}}

typedef struct bot_avoidspot_s
{
	vec3_t origin;
	float radius;
	int type;
} bot_avoidspot_t;

//resets the whole move state
void BotResetMoveState(int movestate);
//moves the bot to the given goal
void BotMoveToGoal(bot_moveresult_t *result, int movestate, bot_goal_t *goal, travelFlags_t travelflags);
//moves the bot in the specified direction using the specified type of movement
int BotMoveInDirection(int movestate, vec3_t dir, float speed, moveTypeFlags_t type);
//reset avoid reachability
void BotResetAvoidReach(int movestate);
//resets the last avoid reachability
void BotResetLastAvoidReach(int movestate);
//returns a reachability area if the origin is in one
int BotReachabilityArea(vec3_t origin, int client);
//view target based on movement
int BotMovementViewTarget(int movestate, bot_goal_t *goal, travelFlags_t travelflags, float lookahead, vec3_t target);
//predict the position of a player based on movement towards a goal
int BotPredictVisiblePosition(vec3_t origin, int areanum, bot_goal_t *goal, travelFlags_t travelflags, vec3_t target);
//returns the handle of a newly allocated movestate
int BotAllocMoveState(void);
//frees the movestate with the given handle
void BotFreeMoveState(int handle);
//initialize movement state before performing any movement
void BotInitMoveState(int handle, bot_initmove_t *initmove);
//add a spot to avoid (if type == AVOID_CLEAR all spots are removed)
void BotAddAvoidSpot(int movestate, vec3_t origin, float radius, int type);
//must be called every map change
void BotSetBrushModelTypes(void);
//setup movement AI
int BotSetupMoveAI(void);
//shutdown movement AI
void BotShutdownMoveAI(void);

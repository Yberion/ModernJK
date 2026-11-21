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

#pragma once

#include "bg_saga.h"

//#define FORCEJUMP_INSTANTMETHOD 1

inline constexpr int MAX_CHAT_BUFFER_SIZE = 8192;
inline constexpr int MAX_CHAT_LINE_SIZE = 128;

inline constexpr int TABLE_BRANCH_DISTANCE = 32;
inline constexpr int MAX_NODETABLE_SIZE = 16384;

inline constexpr int MAX_LOVED_ONES = 4;
inline constexpr int MAX_ATTACHMENT_NAME = 64;

inline constexpr int MAX_FORCE_INFO_SIZE = 2048;

enum class waypointFlags_t
{
	WPFLAG_NONE = 0,
	WPFLAG_NODE_USED		= 0x00000001, //nodetable only: the node is already in the trail being connected (RMG)
	WPFLAG_JUMP				= 0x00000010, //jump when we hit this
	WPFLAG_DUCK				= 0x00000020, //duck while moving around here
	WPFLAG_NOVIS			= 0x00000400, //go here for a bit even with no visibility
	WPFLAG_SNIPEORCAMPSTAND	= 0x00000800, //a good position to snipe or camp - stand
	WPFLAG_WAITFORFUNC		= 0x00001000, //wait for a func brushent under this point before moving here
	WPFLAG_SNIPEORCAMP		= 0x00002000, //a good position to snipe or camp - crouch
	WPFLAG_ONEWAY_FWD		= 0x00004000, //can only go forward on the trial from here (e.g. went over a ledge)
	WPFLAG_ONEWAY_BACK		= 0x00008000, //can only go backward on the trail from here
	WPFLAG_GOALPOINT		= 0x00010000, //make it a goal to get here.. goal points will be decided by setting "weight" values
	WPFLAG_RED_FLAG			= 0x00020000, //red flag
	WPFLAG_BLUE_FLAG		= 0x00040000, //blue flag
	WPFLAG_SIEGE_REBELOBJ	= 0x00080000, //rebel siege objective
	WPFLAG_SIEGE_IMPERIALOBJ	= 0x00100000, //imperial siege objective
	WPFLAG_NOMOVEFUNC		= 0x00200000, //don't move over if a func is under

	WPFLAG_CALCULATED		= 0x00400000, //don't calculate it again
	WPFLAG_NEVERONEWAY		= 0x00800000, //never flag it as one-way
};
template <> inline constexpr bool enableEnumFlags<waypointFlags_t> = true;

enum class botLevelFlags_t
{
	LEVELFLAG_NONE = 0,
	LEVELFLAG_NOPOINTPREDICTION			= 1, //don't take waypoint beyond current into account when adjusting path view angles
	LEVELFLAG_IGNOREINFALLBACK			= 2, //ignore enemies when in a fallback navigation routine
	LEVELFLAG_IMUSTNTRUNAWAY			= 4, //don't be scared
};
template <> inline constexpr bool enableEnumFlags<botLevelFlags_t> = true;

inline constexpr int WP_KEEP_FLAG_DIST = 128;

enum class botWeaponRange_t
{
	BWEAPONRANGE_MELEE			= 1,
	BWEAPONRANGE_MID,
	BWEAPONRANGE_LONG,
	BWEAPONRANGE_SABER,
};

inline constexpr int MELEE_ATTACK_RANGE = 256;
inline constexpr int SABER_ATTACK_RANGE = 128;
inline constexpr int MAX_CHICKENWUSS_TIME = 10000; //wait 10 secs between checking which run-away path to take

inline constexpr int BOT_RUN_HEALTH = 40;
inline constexpr int BOT_WPTOUCH_DISTANCE = 32;
inline constexpr int ENEMY_FORGET_MS = 10000;
//if our enemy isn't visible within 10000ms (aprx 10sec) then "forget" about him and treat him like every other threat, but still look for
//more immediate threats while main enemy is not visible

inline constexpr int BOT_PLANT_DISTANCE = 256; //plant if within this radius from the last spotted enemy position
inline constexpr int BOT_PLANT_INTERVAL = 15000; //only plant once per 15 seconds at max
inline constexpr int BOT_PLANT_BLOW_DISTANCE = 256; //blow det packs if enemy is within this radius and I am further away than the enemy

inline constexpr int BOT_MAX_WEAPON_GATHER_TIME = 1000; //spend a max of 1 second after spawn issuing orders to gather weapons before attacking enemy base
inline constexpr int BOT_MAX_WEAPON_CHASE_TIME = 15000; //time to spend gathering the weapon before persuing the enemy base (in case it takes longer than expected)

inline constexpr int BOT_MAX_WEAPON_CHASE_CTF = 5000; //time to spend gathering the weapon before persuing the enemy base (in case it takes longer than expected) [ctf-only]

inline constexpr int BOT_MIN_SIEGE_GOAL_SHOOT = 1024;
inline constexpr int BOT_MIN_SIEGE_GOAL_TRAVEL = 128;

inline constexpr int BASE_GUARD_DISTANCE = 256; //guarding the flag
inline constexpr int BASE_FLAGWAIT_DISTANCE = 256; //has the enemy flag and is waiting in his own base for his flag to be returned
inline constexpr int BASE_GETENEMYFLAG_DISTANCE = 256; //waiting around to get the enemy's flag

inline constexpr int BOT_FLAG_GET_DISTANCE = 256;

inline constexpr int BOT_SABER_THROW_RANGE = 800;

enum class bot_ctf_state_t
{
	CTFSTATE_NONE,
	CTFSTATE_ATTACKER,
	CTFSTATE_DEFENDER,
	CTFSTATE_RETRIEVAL,
	CTFSTATE_GUARDCARRIER,
	CTFSTATE_GETFLAGHOME,
	CTFSTATE_MAXCTFSTATES
};

enum class bot_siege_state_t
{
	SIEGESTATE_NONE,
	SIEGESTATE_ATTACKER,
	SIEGESTATE_DEFENDER,
	SIEGESTATE_MAXSIEGESTATES
};

enum class bot_teamplay_state_t
{
	TEAMPLAYSTATE_NONE,
	TEAMPLAYSTATE_FOLLOWING,
	TEAMPLAYSTATE_ASSISTING,
	TEAMPLAYSTATE_REGROUP,
	TEAMPLAYSTATE_MAXTPSTATES
};

typedef struct botattachment_s
{
	int level;
	char name[MAX_ATTACHMENT_NAME];
} botattachment_t;

typedef struct nodeobject_s
{
	vec3_t origin;
//	int index;
	float weight;
	waypointFlags_t flags;
	int neighbornum;
	int inuse;
} nodeobject_t;

typedef struct boteventtracker_s
{
	int			eventSequence;
	entity_event_t	events[MAX_PS_EVENTS];
	float		eventTime;
} boteventtracker_t;

typedef struct botskills_s
{
	int					reflex;
	float				accuracy;
	float				turnspeed;
	float				turnspeed_combat;
	float				maxturn;
	int					perfectaim;
} botskills_t;

//bot state
typedef struct bot_state_s
{
	int inuse;										//true if this state is used by a bot client
	int botthink_residual;							//residual for the bot thinks
	int client;										//client number of the bot
	int entitynum;									//entity number of the bot
	playerState_t cur_ps;							//current player state
	usercmd_t lastucmd;								//usercmd from last frame
	bot_settings_t settings;						//several bot settings
	float thinktime;								//time the bot thinks this frame
	vec3_t origin;									//origin of the bot
	vec3_t velocity;								//velocity of the bot
	vec3_t eye;										//eye coordinates of the bot
	int setupcount;									//true when the bot has just been setup
	float ltime;									//local bot time
	float entergame_time;							//time the bot entered the game
	int ms;											//move state of the bot
	int gs;											//goal state of the bot
	int ws;											//weapon state of the bot
	vec3_t viewangles;								//current view angles
	vec3_t ideal_viewangles;						//ideal view angles
	vec3_t viewanglespeed;

	//rww - new AI values
	gentity_t			*currentEnemy;
	gentity_t			*revengeEnemy;

	gentity_t			*squadLeader;

	gentity_t			*lastHurt;
	gentity_t			*lastAttacked;

	gentity_t			*wantFlag;

	gentity_t			*touchGoal;
	gentity_t			*shootGoal;

	gentity_t			*dangerousObject;

	vec3_t				staticFlagSpot;

	int					revengeHateLevel;
	int					isSquadLeader;

	int					squadRegroupInterval;
	int					squadCannotLead;

	int					lastDeadTime;

	wpobject_t			*wpCurrent;
	wpobject_t			*wpDestination;
	wpobject_t			*wpStoreDest;
	vec3_t				goalAngles;
	vec3_t				goalMovedir;
	vec3_t				goalPosition;

	vec3_t				lastEnemySpotted;
	vec3_t				hereWhenSpotted;
	int					lastVisibleEnemyIndex;
	int					hitSpotted;

	int					wpDirection;

	float				destinationGrabTime;
	float				wpSeenTime;
	float				wpTravelTime;
	float				wpDestSwitchTime;
	float				wpSwitchTime;
	float				wpDestIgnoreTime;

	float				timeToReact;

	float				enemySeenTime;

	float				chickenWussCalculationTime;

	float				beStill;
	float				duckTime;
	float				jumpTime;
	float				jumpHoldTime;
	float				jumpPrep;
	float				forceJumping;
	float				jDelay;

	float				aimOffsetTime;
	float				aimOffsetAmtYaw;
	float				aimOffsetAmtPitch;

	float				frame_Waypoint_Len;
	int					frame_Waypoint_Vis;
	float				frame_Enemy_Len;
	int					frame_Enemy_Vis;

	int					isCamper;
	float				isCamping;
	wpobject_t			*wpCamping;
	wpobject_t			*wpCampingTo;
	bool			campStanding;

	int					randomNavTime;
	int					randomNav;

	int					saberSpecialist;

	int					canChat;
	int					chatFrequency;
	char				currentChat[MAX_CHAT_LINE_SIZE];
	float				chatTime;
	float				chatTime_stored;
	int					doChat;
	int					chatTeam;
	gentity_t			*chatObject;
	gentity_t			*chatAltObject;

	float				meleeStrafeTime;
	int					meleeStrafeDir;
	float				meleeStrafeDisable;

	int					altChargeTime;

	float				escapeDirTime;

	float				dontGoBack;

	int					doAttack;
	int					doAltAttack;

	weapon_t			forceWeaponSelect;
	weapon_t			virtualWeapon;

	int					plantTime;
	int					plantDecided;
	int					plantContinue;
	int					plantKillEmAll;

	int					runningLikeASissy;
	int					runningToEscapeThreat;

	//char				chatBuffer[MAX_CHAT_BUFFER_SIZE];
	//Since we're once again not allocating bot structs dynamically,
	//shoving a 64k chat buffer into one is a bad thing.

	botskills_t			skills;

	botattachment_t		loved[MAX_LOVED_ONES];
	int					lovednum;

	int					loved_death_thresh;

	int					deathActivitiesDone;

	EnumArray<float, weapon_t, weapon_t::WP_NUM_WEAPONS>	botWeaponWeights;

	bot_ctf_state_t		ctfState;

	bot_siege_state_t	siegeState;

	bot_teamplay_state_t	teamplayState;

	int					jmState;

	int					state_Forced; //set by player ordering menu

	int					saberDefending;
	int					saberDefendDecideTime;
	int					saberBFTime;
	int					saberBTime;
	int					saberSTime;
	int					saberThrowTime;

	bool			saberPower;
	int					saberPowerTime;

	int					botChallengingTime;

	char				forceinfo[MAX_FORCE_INFO_SIZE];

#ifndef FORCEJUMP_INSTANTMETHOD
	int					forceJumpChargeTime;
#endif

	int					doForcePush;

	int					noUseTime;
	bool			doingFallback;

	int					iHaveNoIdeaWhereIAmGoing;
	vec3_t				lastSignificantAreaChange;
	int					lastSignificantChangeTime;

	int					forceMove_Forward;
	int					forceMove_Right;
	int					forceMove_Up;
	//end rww
} bot_state_t;

void *B_TempAlloc(int size);
void B_TempFree(int size);

void *B_Alloc(int size);
void B_Free(void *ptr);

//resets the whole bot state
void BotResetState(bot_state_t *bs);
//returns the number of bots in the game
int NumBots(void);

void BotUtilizePersonality(bot_state_t *bs);
int BotDoChat(bot_state_t *bs, const char *section, int always);
void StandardBotAI(bot_state_t *bs, float thinktime);
void BotWaypointRender(void);
int OrgVisibleBox(vec3_t org1, vec3_t mins, vec3_t maxs, vec3_t org2, int ignore);
int BotIsAChickenWuss(bot_state_t *bs);
int GetNearestVisibleWP(vec3_t org, int ignore);
int GetBestIdleGoal(bot_state_t *bs);

char *ConcatArgs( int start );

extern vmCvar_t bot_forcepowers;
extern vmCvar_t bot_forgimmick;
extern vmCvar_t bot_honorableduelacceptance;
#ifdef _DEBUG
extern vmCvar_t bot_nogoals;
extern vmCvar_t bot_debugmessages;
#endif

extern vmCvar_t bot_attachments;
extern vmCvar_t bot_camp;

extern vmCvar_t bot_wp_info;
extern vmCvar_t bot_wp_edit;
extern vmCvar_t bot_wp_clearweight;
extern vmCvar_t bot_wp_distconnect;
extern vmCvar_t bot_wp_visconnect;

extern wpobject_t *flagRed;
extern wpobject_t *oFlagRed;
extern wpobject_t *flagBlue;
extern wpobject_t *oFlagBlue;

extern gentity_t *eFlagRed;
extern gentity_t *eFlagBlue;

extern char gBotChatBuffer[MAX_CLIENTS][MAX_CHAT_BUFFER_SIZE];
extern float gWPRenderTime;
extern float gDeactivated;
extern float gBotEdit;
extern int gWPRenderedFrame;

extern wpobject_t *gWPArray[MAX_WPARRAY_SIZE];
extern int gWPNum;

extern int gLastPrintedIndex;
extern nodeobject_t nodetable[MAX_NODETABLE_SIZE];
extern int nodenum;

extern botLevelFlags_t gLevelFlags;

extern float floattime;
#define FloatTime() floattime

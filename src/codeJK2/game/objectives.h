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

#ifndef __OBJECTIVES_H__
#define __OBJECTIVES_H__

// mission Objectives


// DO NOT CHANGE MAX_MISSION_OBJ. IT AFFECTS THE SAVEGAME STRUCTURE

enum class objectiveNumber_t //# Objective_e
{
	//=================================================
	//
	//=================================================
	KEJIM_POST_OBJ1 = 0,		//# KEJIM POST
	KEJIM_POST_OBJ2,			//# KEJIM POST
	KEJIM_BASE_OBJ1,			//# KEJIM BASE
	KEJIM_BASE_OBJ2,			//# KEJIM BASE
	KEJIM_BASE_OBJ3,			//# KEJIM BASE
	ARTUS_MINE_OBJ1,			//# ARTUS MINE
	ARTUS_MINE_OBJ2,			//# ARTUS MINE
	ARTUS_MINE_OBJ3,			//# ARTUS MINE
	ARTUS_DETENTION_OBJ1,		//# ARTUS DETENTION
	ARTUS_DETENTION_OBJ2,		//# ARTUS DETENTION
	ARTUS_TOPSIDE_OBJ1,			//# ARTUS TOPSIDE
	ARTUS_TOPSIDE_OBJ2,			//# ARTUS TOPSIDE
	YAVIN_TEMPLE_OBJ1,			//# YAVIN TEMPLE
	YAVIN_TRIAL_OBJ1,			//# YAVIN TRIAL
	YAVIN_TRIAL_OBJ2,			//# YAVIN TRIAL
	NS_STREETS_OBJ1,			//# NS STREETS
	NS_STREETS_OBJ2,			//# NS STREETS
	NS_STREETS_OBJ3,			//# NS STREETS
	NS_HIDEOUT_OBJ1,			//# NS HIDEOUT
	NS_HIDEOUT_OBJ2,			//# NS HIDEOUT
	NS_STARPAD_OBJ1,			//# NS STARPAD
	NS_STARPAD_OBJ2,			//# NS STARPAD
	NS_STARPAD_OBJ3,			//# NS STARPAD
	NS_STARPAD_OBJ4,			//# NS STARPAD
	NS_STARPAD_OBJ5,			//# NS STARPAD
	BESPIN_UNDERCITY_OBJ1,		//# BESPIN UNDERCITY
	BESPIN_UNDERCITY_OBJ2,		//# BESPIN UNDERCITY
	BESPIN_STREETS_OBJ1,		//# BESPIN STREETS
	BESPIN_STREETS_OBJ2,		//# BESPIN STREETS
	BESPIN_PLATFORM_OBJ1,		//# BESPIN PLATFORM
	BESPIN_PLATFORM_OBJ2,		//# BESPIN PLATFORM
	CAIRN_BAY_OBJ1,				//# CAIRN BAY
	CAIRN_BAY_OBJ2,				//# CAIRN BAY
	CAIRN_ASSEMBLY_OBJ1,		//# CAIRN ASSEMBLY
	CAIRN_REACTOR_OBJ1,			//# CAIRN REACTOR
	CAIRN_REACTOR_OBJ2,			//# CAIRN REACTOR
	CAIRN_DOCK1_OBJ1,			//# CAIRN DOCK1
	CAIRN_DOCK1_OBJ2,			//# CAIRN DOCK1
	DOOM_COMM_OBJ1,				//# DOOM COMM
	DOOM_COMM_OBJ2,				//# DOOM COMM
	DOOM_COMM_OBJ3,				//# DOOM COMM
	DOOM_DETENTION_OBJ1,		//# DOOM DETENTION
	DOOM_DETENTION_OBJ2,		//# DOOM DETENTION
	DOOM_SHIELDS_OBJ1,			//# DOOM SHIELDS
	DOOM_SHIELDS_OBJ2,			//# DOOM SHIELDS
	YAVIN_SWAMP_OBJ1,			//# YAVIN SWAMP
	YAVIN_SWAMP_OBJ2,			//# YAVIN SWAMP
	YAVIN_CANYON_OBJ1,			//# YAVIN CANYON
	YAVIN_CANYON_OBJ2,			//# YAVIN CANYON
	YAVIN_COURTYARD_OBJ1,		//# YAVIN COURTYARD
	YAVIN_COURTYARD_OBJ2,		//# YAVIN COURTYARD
	YAVIN_FINAL_OBJ1,			//# YAVIN FINAL
	KEJIM_POST_OBJ3,			//# KEJIM POST - GRAPHICS IN IT.
	KEJIM_POST_OBJ4,			//# KEJIM POST - GRAPHICS IN IT.
	KEJIM_POST_OBJ5,			//# KEJIM POST - GRAPHICS IN IT.
	ARTUS_DETENTION_OBJ3,		//# ARTUS DETENTION
	DOOM_COMM_OBJ4,				//# DOOM COMM - GRAPHICS IN IT.
	DOOM_SHIELDS_OBJ3,			//# DOOM SHIELDS
	DEMO_OBJ1,					//# DEMO
	DEMO_OBJ2,					//# DEMO
	DEMO_OBJ3,					//# DEMO
	DEMO_OBJ4,					//# DEMO

	//# #eol
	MAX_OBJECTIVES,
};
template <> inline constexpr bool enableEnumArithmetic<objectiveNumber_t> = true;


enum class missionFailed_t //# MissionFailed_e
{
	MISSIONFAILED_JAN=0,		//#
	MISSIONFAILED_LUKE,			//#
	MISSIONFAILED_LANDO,		//#
	MISSIONFAILED_R5D2,			//#
	MISSIONFAILED_WARDEN,		//#
	MISSIONFAILED_PRISONERS,	//#
	MISSIONFAILED_EMPLACEDGUNS,	//#
	MISSIONFAILED_LADYLUCK,		//#
	MISSIONFAILED_KYLECAPTURE,	//#
	MISSIONFAILED_TOOMANYALLIESDIED,	//#
	//# #eol
	MAX_MISSIONFAILED,
};


enum class statusText_t //# StatusText_e
{
	//=================================================
	//
	//=================================================
	STAT_INSUBORDINATION = 0,		//# Starfleet will not tolerate such insubordination
	STAT_YOUCAUSEDDEATHOFTEAMMATE,	//# You caused the death of a teammate.
	STAT_DIDNTPROTECTTECH,			//# You failed to protect Chell, your technician.
	STAT_DIDNTPROTECT7OF9,			//# You failed to protect 7 of 9
	STAT_NOTSTEALTHYENOUGH,			//# You weren't quite stealthy enough
	STAT_STEALTHTACTICSNECESSARY,	//# Starfleet will not tolerate such insubordination
	STAT_WATCHYOURSTEP,				//# Watch your step
	STAT_JUDGEMENTMUCHDESIRED,		//# Your judgement leaves much to be desired

	//# #eol
	MAX_STATUSTEXT,
};

extern bool	missionInfo_Updated;


inline constexpr int SET_OBJ_HIDE = 0;
inline constexpr int SET_OBJ_SHOW = 1;
inline constexpr int SET_OBJ_PENDING = 2;
inline constexpr int SET_OBJ_SUCCEEDED = 3;
inline constexpr int SET_OBJ_FAILED = 4;

inline constexpr int OBJECTIVE_HIDE = 0;
inline constexpr int OBJECTIVE_SHOW = 1;

inline constexpr int OBJECTIVE_STAT_PENDING = 0;
inline constexpr int OBJECTIVE_STAT_SUCCEEDED = 1;
inline constexpr int OBJECTIVE_STAT_FAILED = 2;

extern int statusTextIndex;

void OBJ_SaveObjectiveData(void);
void OBJ_LoadObjectiveData(void);
extern void OBJ_SetPendingObjectives(gentity_t *ent);

#ifndef G_OBJECTIVES_CPP

extern stringID_table_t objectiveTable [];
extern stringID_table_t statusTextTable [];
extern stringID_table_t missionFailedTable [];

#else

stringID_table_t objectiveTable [] =
{
	//=================================================
	//
	//=================================================
	ENUM2STRING(objectiveNumber_t, KEJIM_POST_OBJ1),		//# KEJIM POST
	ENUM2STRING(objectiveNumber_t, KEJIM_POST_OBJ2),			//# KEJIM POST
	ENUM2STRING(objectiveNumber_t, KEJIM_BASE_OBJ1),			//# KEJIM BASE
	ENUM2STRING(objectiveNumber_t, KEJIM_BASE_OBJ2),			//# KEJIM BASE
	ENUM2STRING(objectiveNumber_t, KEJIM_BASE_OBJ3),			//# KEJIM BASE
	ENUM2STRING(objectiveNumber_t, ARTUS_MINE_OBJ1),			//# ARTUS MINE
	ENUM2STRING(objectiveNumber_t, ARTUS_MINE_OBJ2),			//# ARTUS MINE
	ENUM2STRING(objectiveNumber_t, ARTUS_MINE_OBJ3),			//# ARTUS MINE
	ENUM2STRING(objectiveNumber_t, ARTUS_DETENTION_OBJ1),		//# ARTUS DETENTION
	ENUM2STRING(objectiveNumber_t, ARTUS_DETENTION_OBJ2),		//# ARTUS DETENTION
	ENUM2STRING(objectiveNumber_t, ARTUS_TOPSIDE_OBJ1),			//# ARTUS TOPSIDE
	ENUM2STRING(objectiveNumber_t, ARTUS_TOPSIDE_OBJ2),			//# ARTUS TOPSIDE
	ENUM2STRING(objectiveNumber_t, YAVIN_TEMPLE_OBJ1),			//# YAVIN TEMPLE
	ENUM2STRING(objectiveNumber_t, YAVIN_TRIAL_OBJ1),			//# YAVIN TRIAL
	ENUM2STRING(objectiveNumber_t, YAVIN_TRIAL_OBJ2),			//# YAVIN TRIAL
	ENUM2STRING(objectiveNumber_t, NS_STREETS_OBJ1),			//# NS STREETS
	ENUM2STRING(objectiveNumber_t, NS_STREETS_OBJ2),			//# NS STREETS
	ENUM2STRING(objectiveNumber_t, NS_STREETS_OBJ3),			//# NS STREETS
	ENUM2STRING(objectiveNumber_t, NS_HIDEOUT_OBJ1),			//# NS HIDEOUT
	ENUM2STRING(objectiveNumber_t, NS_HIDEOUT_OBJ2),			//# NS HIDEOUT
	ENUM2STRING(objectiveNumber_t, NS_STARPAD_OBJ1),			//# NS STARPAD
	ENUM2STRING(objectiveNumber_t, NS_STARPAD_OBJ2),			//# NS STARPAD
	ENUM2STRING(objectiveNumber_t, NS_STARPAD_OBJ3),			//# NS STARPAD
	ENUM2STRING(objectiveNumber_t, NS_STARPAD_OBJ4),			//# NS STARPAD
	ENUM2STRING(objectiveNumber_t, NS_STARPAD_OBJ5),			//# NS STARPAD
	ENUM2STRING(objectiveNumber_t, BESPIN_UNDERCITY_OBJ1),		//# BESPIN UNDERCITY
	ENUM2STRING(objectiveNumber_t, BESPIN_UNDERCITY_OBJ2),		//# BESPIN UNDERCITY
	ENUM2STRING(objectiveNumber_t, BESPIN_STREETS_OBJ1),		//# BESPIN STREETS
	ENUM2STRING(objectiveNumber_t, BESPIN_STREETS_OBJ2),		//# BESPIN STREETS
	ENUM2STRING(objectiveNumber_t, BESPIN_PLATFORM_OBJ1),		//# BESPIN PLATFORM
	ENUM2STRING(objectiveNumber_t, BESPIN_PLATFORM_OBJ2),		//# BESPIN PLATFORM
	ENUM2STRING(objectiveNumber_t, CAIRN_BAY_OBJ1),				//# CAIRN BAY
	ENUM2STRING(objectiveNumber_t, CAIRN_BAY_OBJ2),				//# CAIRN BAY
	ENUM2STRING(objectiveNumber_t, CAIRN_ASSEMBLY_OBJ1),		//# CAIRN ASSEMBLY
	ENUM2STRING(objectiveNumber_t, CAIRN_REACTOR_OBJ1),			//# CAIRN REACTOR
	ENUM2STRING(objectiveNumber_t, CAIRN_REACTOR_OBJ2),			//# CAIRN REACTOR
	ENUM2STRING(objectiveNumber_t, CAIRN_DOCK1_OBJ1),			//# CAIRN DOCK1
	ENUM2STRING(objectiveNumber_t, CAIRN_DOCK1_OBJ2),			//# CAIRN DOCK1
	ENUM2STRING(objectiveNumber_t, DOOM_COMM_OBJ1),				//# DOOM COMM
	ENUM2STRING(objectiveNumber_t, DOOM_COMM_OBJ2),				//# DOOM COMM
	ENUM2STRING(objectiveNumber_t, DOOM_COMM_OBJ3),				//# DOOM COMM
	ENUM2STRING(objectiveNumber_t, DOOM_DETENTION_OBJ1),		//# DOOM DETENTION
	ENUM2STRING(objectiveNumber_t, DOOM_DETENTION_OBJ2),		//# DOOM DETENTION
	ENUM2STRING(objectiveNumber_t, DOOM_SHIELDS_OBJ1),			//# DOOM SHIELDS
	ENUM2STRING(objectiveNumber_t, DOOM_SHIELDS_OBJ2),			//# DOOM SHIELDS
	ENUM2STRING(objectiveNumber_t, YAVIN_SWAMP_OBJ1),			//# YAVIN SWAMP
	ENUM2STRING(objectiveNumber_t, YAVIN_SWAMP_OBJ2),			//# YAVIN SWAMP
	ENUM2STRING(objectiveNumber_t, YAVIN_CANYON_OBJ1),			//# YAVIN CANYON
	ENUM2STRING(objectiveNumber_t, YAVIN_CANYON_OBJ2),			//# YAVIN CANYON
	ENUM2STRING(objectiveNumber_t, YAVIN_COURTYARD_OBJ1),		//# YAVIN COURTYARD
	ENUM2STRING(objectiveNumber_t, YAVIN_COURTYARD_OBJ2),		//# YAVIN COURTYARD
	ENUM2STRING(objectiveNumber_t, YAVIN_FINAL_OBJ1),			//# YAVIN FINAL
	ENUM2STRING(objectiveNumber_t, KEJIM_POST_OBJ3),			//# KEJIM POST - GRAPHICS IN IT.
	ENUM2STRING(objectiveNumber_t, KEJIM_POST_OBJ4),			//# KEJIM POST - GRAPHICS IN IT.
	ENUM2STRING(objectiveNumber_t, KEJIM_POST_OBJ5),			//# KEJIM POST - GRAPHICS IN IT.
	ENUM2STRING(objectiveNumber_t, ARTUS_DETENTION_OBJ3),		//# ARTUS DETENTION
	ENUM2STRING(objectiveNumber_t, DOOM_COMM_OBJ4),			//# DOOM COMM - GRAPHICS IN IT. IT MUST BE LAST IN THE LIST
	ENUM2STRING(objectiveNumber_t, DOOM_SHIELDS_OBJ3),			//# DOOM SHIELDS
	ENUM2STRING(objectiveNumber_t, DEMO_OBJ1),					//# DEMO
	ENUM2STRING(objectiveNumber_t, DEMO_OBJ2),					//# DEMO
	ENUM2STRING(objectiveNumber_t, DEMO_OBJ3),					//# DEMO
	ENUM2STRING(objectiveNumber_t, DEMO_OBJ4),					//# DEMO

	//stringID_table_t Must end with a null entry
	{"", 0}
};

stringID_table_t missionFailedTable [] =
{
	ENUM2STRING(missionFailed_t, MISSIONFAILED_JAN),			//# JAN DIED
	ENUM2STRING(missionFailed_t, MISSIONFAILED_LUKE),		//# LUKE DIED
	ENUM2STRING(missionFailed_t, MISSIONFAILED_LANDO),		//# LANDO DIED
	ENUM2STRING(missionFailed_t, MISSIONFAILED_R5D2),		//# R5D2 DIED
	ENUM2STRING(missionFailed_t, MISSIONFAILED_WARDEN),		//# THE WARDEN DIED
	ENUM2STRING(missionFailed_t, MISSIONFAILED_PRISONERS),	//#	TOO MANY PRISONERS DIED
	ENUM2STRING(missionFailed_t, MISSIONFAILED_EMPLACEDGUNS),//#	ALL EMPLACED GUNS GONE
	ENUM2STRING(missionFailed_t, MISSIONFAILED_LADYLUCK),	//#	LADY LUCK DISTROYED
	ENUM2STRING(missionFailed_t, MISSIONFAILED_KYLECAPTURE),	//# KYLE HAS BEEN CAPTURED
	ENUM2STRING(missionFailed_t, MISSIONFAILED_TOOMANYALLIESDIED),	//# TOO MANY ALLIES DIED

	//stringID_table_t Must end with a null entry
	{"", 0}
};

stringID_table_t statusTextTable [] =
{
	//=================================================
	//
	//=================================================
	ENUM2STRING(statusText_t, STAT_INSUBORDINATION),				//# Starfleet will not tolerate such insubordination
	ENUM2STRING(statusText_t, STAT_YOUCAUSEDDEATHOFTEAMMATE),		//# You caused the death of a teammate.
	ENUM2STRING(statusText_t, STAT_DIDNTPROTECTTECH),				//# You failed to protect Chell, your technician.
	ENUM2STRING(statusText_t, STAT_DIDNTPROTECT7OF9),				//# You failed to protect 7 of 9
	ENUM2STRING(statusText_t, STAT_NOTSTEALTHYENOUGH),			//# You weren't quite stealthy enough
	ENUM2STRING(statusText_t, STAT_STEALTHTACTICSNECESSARY),		//# Starfleet will not tolerate such insubordination
	ENUM2STRING(statusText_t, STAT_WATCHYOURSTEP),				//# Watch your step
	ENUM2STRING(statusText_t, STAT_JUDGEMENTMUCHDESIRED),			//# Your judgement leaves much to be desired
	//stringID_table_t Must end with a null entry
	{"", 0}
};

#endif// #ifndef G_OBJECTIVES_CPP


#endif// #ifndef __OBJECTIVES_H__


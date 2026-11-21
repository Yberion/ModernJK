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

enum class objectiveNumber_t // # Objective_e
{
    //=================================================
    //
    //=================================================

    LIGHTSIDE_OBJ = 0,
    HOTH2_OBJ1,
    HOTH2_OBJ2,
    HOTH2_OBJ3,
    HOTH3_OBJ1,
    HOTH3_OBJ2,
    HOTH3_OBJ3,
    T2_DPREDICAMENT_OBJ1,
    T2_DPREDICAMENT_OBJ2,
    T2_DPREDICAMENT_OBJ3,
    T2_DPREDICAMENT_OBJ4,
    T2_RANCOR_OBJ1,
    T2_RANCOR_OBJ2,
    T2_RANCOR_OBJ3,
    T2_RANCOR_OBJ4,
    T2_RANCOR_OBJ5,
    T2_RANCOR_OBJ5_2,
    T2_RANCOR_OBJ6,
    T2_WEDGE_OBJ1,
    T2_WEDGE_OBJ2,
    T2_WEDGE_OBJ3,
    T2_WEDGE_OBJ4,
    T2_WEDGE_OBJ5,
    T2_WEDGE_OBJ6,
    T2_WEDGE_OBJ7,
    T2_WEDGE_OBJ8,
    T2_WEDGE_OBJ9,
    T2_WEDGE_OBJ10,
    T2_WEDGE_OBJ11,
    T2_WEDGE_OBJ12,
    T3_RIFT_OBJ1,
    T3_RIFT_OBJ2,
    T3_RIFT_OBJ3,
    T1_DANGER_OBJ1,
    T1_DANGER_OBJ2,
    T1_DANGER_OBJ3,
    T1_DANGER_OBJ4,
    T1_DANGER_OBJ5,
    T3_BOUNTY_OBJ1,
    T3_BOUNTY_OBJ2,
    T3_BOUNTY_OBJ3,
    T3_BOUNTY_OBJ4,
    T3_BOUNTY_OBJ5,
    T3_BOUNTY_OBJ6,
    T3_BOUNTY_OBJ7,
    T3_BOUNTY_OBJ8,
    T3_BOUNTY_OBJ9,
    T2_ROGUE_OBJ1,
    T2_ROGUE_OBJ2,
    T2_TRIP_OBJ1,
    T2_TRIP_OBJ2,
    T3_BYSS_OBJ1,
    T3_BYSS_OBJ2,
    T3_BYSS_OBJ3,
    T3_HEVIL_OBJ1,
    T3_HEVIL_OBJ2,
    T3_HEVIL_OBJ3,
    T3_STAMP_OBJ1,
    T3_STAMP_OBJ2,
    T3_STAMP_OBJ3,
    T3_STAMP_OBJ4,
    TASPIR1_OBJ1,
    TASPIR1_OBJ2,
    TASPIR1_OBJ3,
    TASPIR1_OBJ4,
    TASPIR2_OBJ1,
    TASPIR2_OBJ2,
    VJUN1_OBJ1,
    VJUN1_OBJ2,
    VJUN2_OBJ1,
    VJUN3_OBJ1,
    YAVIN1_OBJ1,
    YAVIN1_OBJ2,
    YAVIN2_OBJ1,
    T1_FATAL_OBJ1,
    T1_FATAL_OBJ2,
    T1_FATAL_OBJ3,
    T1_FATAL_OBJ4,
    T1_FATAL_OBJ5,
    T1_FATAL_OBJ6,
    KOR1_OBJ1,
    KOR1_OBJ2,
    KOR2_OBJ1,
    KOR2_OBJ2,
    KOR2_OBJ3,
    KOR2_OBJ4,
    T1_RAIL_OBJ1,
    T1_RAIL_OBJ2,
    T1_RAIL_OBJ3,
    T1_SOUR_OBJ1,
    T1_SOUR_OBJ2,
    T1_SOUR_OBJ3,
    T1_SOUR_OBJ4,
    T1_SURPRISE_OBJ1,
    T1_SURPRISE_OBJ2,
    T1_SURPRISE_OBJ3,
    T1_SURPRISE_OBJ4,

    // # #eol
    MAX_OBJECTIVES,
};
template <> inline constexpr bool enableEnumArithmetic<objectiveNumber_t> = true;

enum class missionFailed_t // # MissionFailed_e
{
    MISSIONFAILED_JAN = 0,           // #
    MISSIONFAILED_LUKE,              // #
    MISSIONFAILED_LANDO,             // #
    MISSIONFAILED_R5D2,              // #
    MISSIONFAILED_WARDEN,            // #
    MISSIONFAILED_PRISONERS,         // #
    MISSIONFAILED_EMPLACEDGUNS,      // #
    MISSIONFAILED_LADYLUCK,          // #
    MISSIONFAILED_KYLECAPTURE,       // #
    MISSIONFAILED_TOOMANYALLIESDIED, // #
    MISSIONFAILED_CHEWIE,            // #
    MISSIONFAILED_KYLE,              // #
    MISSIONFAILED_ROSH,              // #
    MISSIONFAILED_WEDGE,             // #
    MISSIONFAILED_TURNED,            // # Turned on your friends.

    // # #eol
    MAX_MISSIONFAILED,
};

enum class statusText_t // # StatusText_e
{
    //=================================================
    //
    //=================================================
    STAT_INSUBORDINATION = 0,      // # Starfleet will not tolerate such insubordination
    STAT_YOUCAUSEDDEATHOFTEAMMATE, // # You caused the death of a teammate.
    STAT_DIDNTPROTECTTECH,         // # You failed to protect Chell, your technician.
    STAT_DIDNTPROTECT7OF9,         // # You failed to protect 7 of 9
    STAT_NOTSTEALTHYENOUGH,        // # You weren't quite stealthy enough
    STAT_STEALTHTACTICSNECESSARY,  // # Starfleet will not tolerate such insubordination
    STAT_WATCHYOURSTEP,            // # Watch your step
    STAT_JUDGEMENTMUCHDESIRED,     // # Your judgement leaves much to be desired

    // # #eol
    MAX_STATUSTEXT,
};

extern bool missionInfo_Updated;

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
extern void OBJ_SetPendingObjectives(gentity_t* ent);

#ifndef G_OBJECTIVES_CPP

extern stringID_table_t objectiveTable[];
extern stringID_table_t statusTextTable[];
extern stringID_table_t missionFailedTable[];

#else

stringID_table_t objectiveTable[] = {//=================================================
                                     //
                                     //=================================================
                                     ENUM2STRING(objectiveNumber_t, LIGHTSIDE_OBJ),
                                     ENUM2STRING(objectiveNumber_t, HOTH2_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, HOTH2_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, HOTH2_OBJ3),
                                     ENUM2STRING(objectiveNumber_t, HOTH3_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, HOTH3_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, HOTH3_OBJ3),
                                     ENUM2STRING(objectiveNumber_t, T2_DPREDICAMENT_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, T2_DPREDICAMENT_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, T2_DPREDICAMENT_OBJ3),
                                     ENUM2STRING(objectiveNumber_t, T2_DPREDICAMENT_OBJ4),
                                     ENUM2STRING(objectiveNumber_t, T2_RANCOR_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, T2_RANCOR_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, T2_RANCOR_OBJ3),
                                     ENUM2STRING(objectiveNumber_t, T2_RANCOR_OBJ4),
                                     ENUM2STRING(objectiveNumber_t, T2_RANCOR_OBJ5),
                                     ENUM2STRING(objectiveNumber_t, T2_RANCOR_OBJ5_2),
                                     ENUM2STRING(objectiveNumber_t, T2_RANCOR_OBJ6),
                                     ENUM2STRING(objectiveNumber_t, T2_WEDGE_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, T2_WEDGE_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, T2_WEDGE_OBJ3),
                                     ENUM2STRING(objectiveNumber_t, T2_WEDGE_OBJ4),
                                     ENUM2STRING(objectiveNumber_t, T2_WEDGE_OBJ5),
                                     ENUM2STRING(objectiveNumber_t, T2_WEDGE_OBJ6),
                                     ENUM2STRING(objectiveNumber_t, T2_WEDGE_OBJ7),
                                     ENUM2STRING(objectiveNumber_t, T2_WEDGE_OBJ8),
                                     ENUM2STRING(objectiveNumber_t, T2_WEDGE_OBJ9),
                                     ENUM2STRING(objectiveNumber_t, T2_WEDGE_OBJ10),
                                     ENUM2STRING(objectiveNumber_t, T2_WEDGE_OBJ11),
                                     ENUM2STRING(objectiveNumber_t, T2_WEDGE_OBJ12),
                                     ENUM2STRING(objectiveNumber_t, T3_RIFT_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, T3_RIFT_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, T3_RIFT_OBJ3),
                                     ENUM2STRING(objectiveNumber_t, T1_DANGER_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, T1_DANGER_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, T1_DANGER_OBJ3),
                                     ENUM2STRING(objectiveNumber_t, T1_DANGER_OBJ4),
                                     ENUM2STRING(objectiveNumber_t, T1_DANGER_OBJ5),
                                     ENUM2STRING(objectiveNumber_t, T3_BOUNTY_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, T3_BOUNTY_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, T3_BOUNTY_OBJ3),
                                     ENUM2STRING(objectiveNumber_t, T3_BOUNTY_OBJ4),
                                     ENUM2STRING(objectiveNumber_t, T3_BOUNTY_OBJ5),
                                     ENUM2STRING(objectiveNumber_t, T3_BOUNTY_OBJ6),
                                     ENUM2STRING(objectiveNumber_t, T3_BOUNTY_OBJ7),
                                     ENUM2STRING(objectiveNumber_t, T3_BOUNTY_OBJ8),
                                     ENUM2STRING(objectiveNumber_t, T3_BOUNTY_OBJ9),
                                     ENUM2STRING(objectiveNumber_t, T2_ROGUE_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, T2_ROGUE_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, T2_TRIP_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, T2_TRIP_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, T3_BYSS_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, T3_BYSS_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, T3_BYSS_OBJ3),
                                     ENUM2STRING(objectiveNumber_t, T3_HEVIL_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, T3_HEVIL_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, T3_HEVIL_OBJ3),
                                     ENUM2STRING(objectiveNumber_t, T3_STAMP_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, T3_STAMP_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, T3_STAMP_OBJ3),
                                     ENUM2STRING(objectiveNumber_t, T3_STAMP_OBJ4),
                                     ENUM2STRING(objectiveNumber_t, TASPIR1_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, TASPIR1_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, TASPIR1_OBJ3),
                                     ENUM2STRING(objectiveNumber_t, TASPIR1_OBJ4),
                                     ENUM2STRING(objectiveNumber_t, TASPIR2_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, TASPIR2_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, VJUN1_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, VJUN1_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, VJUN2_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, VJUN3_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, YAVIN1_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, YAVIN1_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, YAVIN2_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, T1_FATAL_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, T1_FATAL_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, T1_FATAL_OBJ3),
                                     ENUM2STRING(objectiveNumber_t, T1_FATAL_OBJ4),
                                     ENUM2STRING(objectiveNumber_t, T1_FATAL_OBJ5),
                                     ENUM2STRING(objectiveNumber_t, T1_FATAL_OBJ6),
                                     ENUM2STRING(objectiveNumber_t, KOR1_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, KOR1_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, KOR2_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, KOR2_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, KOR2_OBJ3),
                                     ENUM2STRING(objectiveNumber_t, KOR2_OBJ4),
                                     ENUM2STRING(objectiveNumber_t, T1_RAIL_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, T1_RAIL_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, T1_RAIL_OBJ3),
                                     ENUM2STRING(objectiveNumber_t, T1_SOUR_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, T1_SOUR_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, T1_SOUR_OBJ3),
                                     ENUM2STRING(objectiveNumber_t, T1_SOUR_OBJ4),
                                     ENUM2STRING(objectiveNumber_t, T1_SURPRISE_OBJ1),
                                     ENUM2STRING(objectiveNumber_t, T1_SURPRISE_OBJ2),
                                     ENUM2STRING(objectiveNumber_t, T1_SURPRISE_OBJ3),
                                     ENUM2STRING(objectiveNumber_t, T1_SURPRISE_OBJ4),

                                     // stringID_table_t Must end with a null entry
                                     {"", 0}};

stringID_table_t missionFailedTable[] = {ENUM2STRING(missionFailed_t, MISSIONFAILED_JAN),               // # JAN DIED
                                         ENUM2STRING(missionFailed_t, MISSIONFAILED_LUKE),              // # LUKE DIED
                                         ENUM2STRING(missionFailed_t, MISSIONFAILED_LANDO),             // # LANDO DIED
                                         ENUM2STRING(missionFailed_t, MISSIONFAILED_R5D2),              // # R5D2 DIED
                                         ENUM2STRING(missionFailed_t, MISSIONFAILED_WARDEN),            // # THE WARDEN DIED
                                         ENUM2STRING(missionFailed_t, MISSIONFAILED_PRISONERS),         // #	TOO MANY PRISONERS DIED
                                         ENUM2STRING(missionFailed_t, MISSIONFAILED_EMPLACEDGUNS),      // #	ALL EMPLACED GUNS GONE
                                         ENUM2STRING(missionFailed_t, MISSIONFAILED_LADYLUCK),          // #	LADY LUCK DISTROYED
                                         ENUM2STRING(missionFailed_t, MISSIONFAILED_KYLECAPTURE),       // # KYLE HAS BEEN CAPTURED
                                         ENUM2STRING(missionFailed_t, MISSIONFAILED_TOOMANYALLIESDIED), // # TOO MANY ALLIES DIED
                                         ENUM2STRING(missionFailed_t, MISSIONFAILED_CHEWIE),
                                         ENUM2STRING(missionFailed_t, MISSIONFAILED_KYLE),
                                         ENUM2STRING(missionFailed_t, MISSIONFAILED_ROSH),
                                         ENUM2STRING(missionFailed_t, MISSIONFAILED_WEDGE),
                                         ENUM2STRING(missionFailed_t, MISSIONFAILED_TURNED), // # Turned on your friends.

                                         // stringID_table_t Must end with a null entry
                                         {"", 0}};

stringID_table_t statusTextTable[] = {//=================================================
                                      //
                                      //=================================================
                                      ENUM2STRING(statusText_t, STAT_INSUBORDINATION),          // # Starfleet will not tolerate such insubordination
                                      ENUM2STRING(statusText_t, STAT_YOUCAUSEDDEATHOFTEAMMATE), // # You caused the death of a teammate.
                                      ENUM2STRING(statusText_t, STAT_DIDNTPROTECTTECH),         // # You failed to protect Chell, your technician.
                                      ENUM2STRING(statusText_t, STAT_DIDNTPROTECT7OF9),         // # You failed to protect 7 of 9
                                      ENUM2STRING(statusText_t, STAT_NOTSTEALTHYENOUGH),        // # You weren't quite stealthy enough
                                      ENUM2STRING(statusText_t, STAT_STEALTHTACTICSNECESSARY),  // # Starfleet will not tolerate such insubordination
                                      ENUM2STRING(statusText_t, STAT_WATCHYOURSTEP),            // # Watch your step
                                      ENUM2STRING(statusText_t, STAT_JUDGEMENTMUCHDESIRED),     // # Your judgement leaves much to be desired
                                                                                                // stringID_table_t Must end with a null entry
                                      {"", 0}};

#endif // #ifndef G_OBJECTIVES_CPP

#endif // #ifndef __OBJECTIVES_H__

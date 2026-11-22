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

//
// NPC_misc.cpp
//
#include "b_local.h"
#include <qcommon/q_shared.h>

/*
Debug_Printf
*/
void Debug_VPrintln(vmCvar_t* cv, aiDebugLevel_t debugLevel, fmt::string_view fmt, fmt::format_args args)
{
    const char* color;
    char msg[1024];

    if (cv->value < to_underlying(debugLevel))
        return;

    if (debugLevel == aiDebugLevel_t::DEBUG_LEVEL_DETAIL)
        color = S_COLOR_WHITE;
    else if (debugLevel == aiDebugLevel_t::DEBUG_LEVEL_INFO)
        color = S_COLOR_GREEN;
    else if (debugLevel == aiDebugLevel_t::DEBUG_LEVEL_WARNING)
        color = S_COLOR_YELLOW;
    else // if (debugLevel == aiDebugLevel_t::DEBUG_LEVEL_ERROR)
        color = S_COLOR_RED;

    Q_VFormatToCut(msg, fmt, args);

    Q_Println("{}{:5}:{}", color, level.time, msg);
}

/*
Debug_NPCPrintf
*/
void Debug_VNPCPrintf(gentity_t* printNPC, vmCvar_t* cv, aiDebugLevel_t debugLevel, fmt::string_view fmt, fmt::format_args args)
{
    int color;
    char msg[1024];

    if (cv->value < to_underlying(debugLevel)) {
        return;
    }

    //	if ( debugNPCName.string[0] && Q_stricmp( debugNPCName.string, printNPC->targetname) != 0 )
    //	{
    //		return;
    //	}

    if (debugLevel == aiDebugLevel_t::DEBUG_LEVEL_DETAIL)
        color = COLOR_WHITE;
    else if (debugLevel == aiDebugLevel_t::DEBUG_LEVEL_INFO)
        color = COLOR_GREEN;
    else if (debugLevel == aiDebugLevel_t::DEBUG_LEVEL_WARNING)
        color = COLOR_YELLOW;
    else // if (debugLevel == aiDebugLevel_t::DEBUG_LEVEL_ERROR)
        color = COLOR_RED;

    Q_VFormatToCut(msg, fmt, args);

    Q_Print("{}{:c}{:5} ({}) {}", Q_COLOR_ESCAPE, color, level.time, printNPC->targetname, msg);
}

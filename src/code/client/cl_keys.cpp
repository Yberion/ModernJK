/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
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

// cl_keys.cpp -- the single player's part of the keys (src/shared/client/engine/keys.h): where a key goes beside the
// console, the UI compiled into the engine; a key during a video.
#include <server/exe_headers.h>

#include "client.h"
#include "client_ui.h"

// a key during a video stops it: a level's video refuses its first 1.2 seconds so that firing does not skip it, the
// others (the opening logos, the cinematic command's) stop at the first key
bool CL_KeyStopsVideo(void)
{
    if (cls.state != connstate_t::CA_CINEMATIC && !CL_IsRunningInGameCinematic())
        return false;

    SCR_StopCinematic(CL_IsRunningInGameCinematic());
    return true;
}

// nothing but the console and the UI takes the keys
bool CL_EscapeClosesCatcher(void)
{
    return false;
}

// the menu over the game while playing, else the main menu, disconnected
void CL_EscapeOpensMenu(void)
{
    if (cls.state == connstate_t::CA_ACTIVE)
        UI_SetActiveMenu("ingame", nullptr);
    else {
        CL_Disconnect_f();
        UI_SetActiveMenu("mainMenu", nullptr);
    }
}

// the client has no menu of its own
bool CL_ClientMenuTakesKeys(void)
{
    return false;
}

// a key to the UI while it takes the keys
bool CL_ModuleKeyEvent(fakeAscii_t key, bool down)
{
    if (!Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_UI))
        return false;

    _UI_KeyEvent(key, down);
    return true;
}

// a character to the UI while it takes the keys
bool CL_ModuleCharEvent(int character)
{
    if (!Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_UI))
        return false;

    _UI_CharEvent(character);
    return true;
}

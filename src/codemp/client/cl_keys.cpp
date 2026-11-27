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

// cl_keys.cpp -- the multiplayer's part of the keys (src/shared/client/engine/keys.h): the chat's line, and where a
// key goes beside the console: the client's own menu (the download question), the UI module, the cgame module, the
// chat; a key during a video.
#include "client.h"
#include "cl_cgameapi.h"
#include "cl_uiapi.h"

// chat
field_t chatField;
bool chat_team;
int chat_playerNum;

/*
================
Message_Key

In game talk message
================
*/
static void Message_Key(fakeAscii_t key)
{
    char buffer[MAX_STRING_CHARS] = {0};

    if (key == fakeAscii_t::A_ESCAPE) {
        Key_SetCatcher(Key_GetCatcher() & ~keyCatchFlags_t::KEYCATCH_MESSAGE);
        Field_Clear(&chatField);
        return;
    }

    if (key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        if (chatField.buffer[0] && cls.state == connstate_t::CA_ACTIVE) {
            if (chat_playerNum != -1)
                Q_FormatTo(buffer, "tell {} \"{}\"\n", chat_playerNum, chatField.buffer);
            else if (chat_team)
                Q_FormatTo(buffer, "say_team \"{}\"\n", chatField.buffer);
            else
                Q_FormatTo(buffer, "say \"{}\"\n", chatField.buffer);

            CL_AddReliableCommand(buffer, false);
        }
        Key_SetCatcher(Key_GetCatcher() & ~keyCatchFlags_t::KEYCATCH_MESSAGE);
        Field_Clear(&chatField);
        return;
    }

    Field_KeyDownEvent(&chatField, key);
}

// a key during a video played outside a level (the cinematic command) leaves it for the main menu, as Escape; in camera
// mode it goes on to the binds
bool CL_KeyStopsVideo(void)
{
    if (cls.state != connstate_t::CA_CINEMATIC || com_cameraMode->integer)
        return false;

    Cvar_Set("nextdemo", "");
    CL_EscapeOpensMenu();
    return true;
}

// Escape closes the chat, or what the cgame shows
bool CL_EscapeClosesCatcher(void)
{
    if (Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_MESSAGE)) {
        Message_Key(fakeAscii_t::A_ESCAPE);
        return true;
    }

    if (Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_CGAME)) {
        Key_SetCatcher(Key_GetCatcher() & ~keyCatchFlags_t::KEYCATCH_CGAME);
        CGVM_EventHandling(cgameEvent_t::CGAME_EVENT_NONE);
        return true;
    }

    return false;
}

// the menu over the game while playing, else the main menu, disconnected (a demo played back is left)
void CL_EscapeOpensMenu(void)
{
    if (cls.state == connstate_t::CA_ACTIVE && !clc.demoplaying)
        UIVM_SetActiveMenu(uiMenuCommand_t::UIMENU_INGAME);
    else {
        CL_Disconnect_f();
        SND_StopAll();
        UIVM_SetActiveMenu(uiMenuCommand_t::UIMENU_MAIN);
    }
}

// the download question takes the keys while it is shown
bool CL_ClientMenuTakesKeys(void)
{
    return cls.cursorActive;
}

// a press to the download question first; then to the UI, the cgame or the chat, whichever takes the keys
bool CL_ModuleKeyEvent(fakeAscii_t key, bool down)
{
    if (down && cls.cursorActive) {
        CL_CursorButton(key);
        return true;
    }

    if (Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_UI)) {
        if (cls.uiStarted)
            UIVM_KeyEvent(key, down);
        return true;
    }

    if (Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_CGAME)) {
        if (cls.cgameStarted)
            CGVM_KeyEvent(key, down);
        return true;
    }

    if (down && Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_MESSAGE)) {
        Message_Key(key);
        return true;
    }

    return false;
}

// a character to the UI, the cgame or the chat, whichever takes the keys
bool CL_ModuleCharEvent(int character)
{
    if (Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_UI))
        UIVM_CharEvent(character);
    else if (Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_CGAME))
        CGVM_CharEvent(character);
    else if (Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_MESSAGE))
        Field_CharEvent(&chatField, character);
    else
        return false;

    return true;
}

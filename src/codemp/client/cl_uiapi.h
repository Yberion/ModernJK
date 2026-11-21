/*
===========================================================================
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

void UIVM_Init(bool inGameLoad);
void UIVM_Shutdown(void);
void UIVM_KeyEvent(int key, bool down);
enum class fakeAscii_t; // ui/keycodes.h
void UIVM_KeyEvent(fakeAscii_t key, bool down);
void UIVM_MouseEvent(int dx, int dy);
void UIVM_Refresh(int realtime);
bool UIVM_IsFullscreen(void);
void UIVM_SetActiveMenu(uiMenuCommand_t menu);
bool UIVM_ConsoleCommand(int realTime);
void UIVM_DrawConnectScreen(bool overlay);

void CL_BindUI(void);
void CL_UnbindUI(void);

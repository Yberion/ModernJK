/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
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

// unix_local.h: Linux-specific Quake3 header file

void Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr );
bool Sys_GetPacket ( netadr_t *net_from, msg_t *net_message );

// Input subsystem

void IN_Init (void);
void IN_Frame (void);
void IN_Shutdown (void);


void IN_JoyMove( void );
void IN_StartupJoystick( void );

// GL subsystem
bool QGL_Init( const char *dllname );
void QGL_EnableLogging( bool enable );
void QGL_Shutdown( void );





// bk001130 - win32
// void IN_JoystickCommands (void);

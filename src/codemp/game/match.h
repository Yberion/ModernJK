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

//===========================================================================
//
// Name:			match.h
// Function:		match template defines
// Programmer:		Mr Elusive
// Last update:
// Tab Size:		4 (real tabs)
//
//===========================================================================

// make sure this is the same character as we use in chats in g_cmd.c
#define EC "\x19"

// message types
//
//
//

// word replacement variables
// team message variables
inline constexpr int PLACE = 1;
inline constexpr int MESSAGE = 2;
inline constexpr int ITEM = 3;
inline constexpr int ENEMY = 4;
inline constexpr int POSITION = 5;
inline constexpr int NUMBER = 5;
inline constexpr int TIME = 6;
inline constexpr int NAME = 6;

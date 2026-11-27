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

// ui_shared.h -- the multiplayer's values of the menus' shared code: the sliders' size its menus are drawn for, an item's
// Ghoul2 instance as the handle the module is given; then the menus' types (src/shared/ui/ui_shared.h).

#pragma once

#include <qcommon/q_shared.h>
#include <rd-common/tr_types.h>
#include <shared/qcommon/q_keycodes.h>

#include "menudef.h"

inline constexpr double SLIDER_WIDTH = 96.0;
inline constexpr double SLIDER_THUMB_HEIGHT = 20.0;

// an item's Ghoul2 instance: the handle the engine gives the module
using uiGhoul2_t = void*;

// the display context's functions of this game alone: none
struct displayContextGame_t {};

#include <shared/ui/ui_shared.h>

extern EnumArray<const char*, forcePowers_t, forcePowers_t::NUM_FORCE_POWERS> HolocronIcons;

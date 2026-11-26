/*
===========================================================================
Copyright (C) 2026, ModernJK contributors

This file is part of the ModernJK source code.

ModernJK is free software; you can redistribute it and/or modify it
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

// The parts of the platform's tests, each in its own file; main.cpp runs them in this order. Each part starts the
// layer on SDL's dummy video driver (a screen and windows in memory) and stops it.
#pragma once

// SDL 3 as the platform layer uses it (sdl.cpp)
void RunSdlTests();
// src/shared/platform/window (window.cpp)
void RunWindowTests();
// src/shared/platform/input (input.cpp)
void RunInputTests();
// the window on the system's own driver and screen, asked by hand (real.cpp)
void RunRealWindowTests();

// Starts the layer on SDL's dummy video driver; false, with a failed check, when it does not start.
bool StartOnDummyDriver();

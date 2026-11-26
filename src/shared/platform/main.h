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

// main.h -- the program's entry point as the system wants it, for the one file of a program that defines main: on
// Windows a program with a window starts at WinMain, which SDL's header defines here and which calls that file's
// main with the command line's arguments; elsewhere it changes nothing.
// Example: #include <shared/platform/main.h> above int main(int argc, char* argv[]), in that file only.
#pragma once

#include <SDL3/SDL_main.h>

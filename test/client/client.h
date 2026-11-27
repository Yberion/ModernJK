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

// The parts of the clients' tests (src/shared/client), each in its own file; main.cpp runs them in this order. Each
// part calls functions of their arguments only, on fields and tables written in the test.
#pragma once

// a button's presses and releases and the share of a span it was held (button.cpp)
void RunButtonTests();
// a text field's editing and what it shows (field.cpp)
void RunFieldTests();
// the keys' names and the free codes (keynames.cpp)
void RunKeyNamesTests();

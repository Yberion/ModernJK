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

// The tests of src/shared/qcommon/safe (the checked strings and the vector of fixed capacity the single player uses).
// Usage: modernjk_test_safe; prints each failed check, exits with their count.
#include "check.h"

#include <cstdio>

void RunStringTests();
void RunLimitedVectorTests();

int main()
{
	RunStringTests();
	RunLimitedVectorTests();
	std::printf( "%d failed check(s)\n", ::test::failures );
	return ::test::failures;
}

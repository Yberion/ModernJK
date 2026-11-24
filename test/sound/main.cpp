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

// The tests of src/shared/sound and of OpenAL Soft as the sound uses it.
// Usage: modernjk_test_sound <test/sound/data>; prints each failed check, exits with their count.
#include "sound.h"

#include <check.h>

#include <fmt/base.h>

// runs every part on the data folder given as the argument, and exits with the count of failed checks
int main(int argc, char** argv)
{
    if (argc != 2) {
        fmt::println("usage: modernjk_test_sound <test/sound/data>");
        return 1;
    }

    const std::filesystem::path data = argv[1];
    RunDecoderTests(data);
    RunDeviceTests(data);
    RunBankTests(data);
    RunVoicesTests(data);
    RunStreamTests(data);
    RunAmbientTests(data);
    RunContactTests(data);
    RunZonesTests(data);
    fmt::println("{} failed check(s)", ::test::failures);
    return ::test::failures;
}

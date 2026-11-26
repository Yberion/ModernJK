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

// The tests of src/shared/platform and of SDL 3 as it is built for the client. They need no screen (SDL's dummy
// video driver).
// Usage: modernjk_test_platform; prints each failed check, exits with their count. With the argument "real", by
// hand, it runs instead the window's checks on the system's own video driver and screen, which opens a window.
#include "platform.h"

#include <check.h>

#include <shared/platform/platform.h>

#include <SDL3/SDL.h>

#include <fmt/base.h>

#include <string_view>

// starts the layer on the dummy driver, chosen by SDL's hint before the start
bool StartOnDummyDriver()
{
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    const auto started = platform::Start("modernjk_test_platform", "1");
    CHECK(started.has_value());
    return started.has_value();
}

// runs every part, or the real window's checks when asked, and exits with the count of failed checks
int main(int argc, char** argv)
{
    if (argc == 2 && std::string_view(argv[1]) == "real") {
        RunRealWindowTests();
    }
    else {
        RunSdlTests();
        RunWindowTests();
        RunInputTests();
        RunMenusTests();
    }

    fmt::println("{} failed check(s)", ::test::failures);
    return ::test::failures;
}

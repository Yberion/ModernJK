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

// The window's checks on the system's own video driver and screen, which ctest does not run (it has no screen):
// asked by hand with "modernjk_test_platform real", on a machine whose driver is to be checked (Windows, X11,
// Wayland). A window with an OpenGL context is opened in borderless fullscreen, taken to a window and to exclusive
// fullscreen in the screen's present mode, an image shown in each state; the layer's description, the screens and
// what the context got are printed.
#include "platform.h"

#include <check.h>

#include <shared/platform/input/input.h>
#include <shared/platform/platform.h>
#include <shared/platform/window/window.h>

#include <fmt/base.h>

#include <cstdint>
#include <vector>

namespace {

// shows a few images in the window's present state, reading the input as an engine does between them
void ShowImages(platform::Window& window, platform::Input& input)
{
    for (int i = 0; i < 30; ++i) {
        static_cast<void>(input.Poll());
        CHECK(window.Present());
    }
}

} // namespace

// runs the window through its three states on the system's driver, with an OpenGL context
void RunRealWindowTests()
{
    const auto started = platform::Start("modernjk_test_platform", "1");
    CHECK(started.has_value());

    if (!started) {
        fmt::println("{}", started.error());
        return;
    }

    fmt::println("{}", platform::Description());
    const std::vector<platform::Screen> screens = platform::Screens();
    CHECK(!screens.empty());

    for (const platform::Screen& screen : screens) {
        fmt::println("screen \"{}\": desktop {}x{} at {} Hz, {} modes", screen.name, screen.desktop.width, screen.desktop.height, screen.desktop.refresh,
                     screen.modes.size());
    }

    if (screens.empty()) {
        platform::Stop();
        return;
    }

    {
        const platform::PixelSize desktop{.width = screens.front().desktop.width, .height = screens.front().desktop.height};
        platform::WindowSettings settings;
        auto window = platform::Window::Open("modernjk_test_platform", settings, platform::GlRequest{.samples = 4});
        CHECK(window.has_value());

        if (!window) {
            fmt::println("{}", window.error());
        }
        else {
            platform::Input input(*window);
            fmt::println("borderless: {}x{}, {} samples", window->Size().width, window->Size().height, window->Samples());
            CHECK_EQUAL(window->Size(), desktop);
            CHECK(window->GlProcAddress("glClear") != nullptr);
            // a driver may have no vertical sync to set (a virtual machine's): told, not a failure
            fmt::println("vertical sync: {}", window->SetSwapInterval(platform::SwapInterval::VerticalSync) ? "set" : "refused");
            ShowImages(*window, input);

            settings.state = platform::DisplayState::Window;
            settings.width = 800;
            settings.height = 600;
            CHECK(window->Apply(settings));
            fmt::println("window: {}x{}", window->Size().width, window->Size().height);
            CHECK_EQUAL(window->Size(), (platform::PixelSize{.width = 800, .height = 600}));
            ShowImages(*window, input);

            settings.state = platform::DisplayState::Exclusive;
            CHECK(window->Apply(settings));
            fmt::println("exclusive: {}x{}", window->Size().width, window->Size().height);
            CHECK_EQUAL(window->Size(), desktop);
            ShowImages(*window, input);

            // what SDL's dummy driver lacks: an icon, and a window to minimize
            const std::vector<std::uint8_t> pixels(2 * 2 * 4, 255);
            CHECK(window->SetIcon({.rgba = pixels, .size = {.width = 2, .height = 2}}));
            CHECK(window->Minimize());
        }
    }

    platform::Stop();
}

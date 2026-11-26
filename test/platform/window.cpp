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

// The tests of src/shared/platform (platform.h) and of its window (window/), on SDL's dummy video driver: the layer's
// description and clock; the screens listed with their desktop's mode and usable area, and without their modes; a window opened at the size asked, in pixels;
// the same window taken through the three display states and back, never created again; a screen that does not
// exist taken as the primary one; a window without a context refusing what needs one; an icon and a pointer's image
// taken, pixels of a wrong count left out; an OpenGL context, which the dummy driver lacks, refused with a text
// that says so.
#include "platform.h"

#include <check.h>

#include <shared/platform/platform.h>
#include <shared/platform/window/window.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

// the layer's description names the library and the driver, and its clock does not go back
void CheckLayer()
{
    const std::string description = platform::Description();
    CHECK(description.starts_with("SDL 3.4.20 ("));
    CHECK(description.ends_with("video driver dummy"));
    const platform::Time before = platform::Now();
    CHECK(platform::Now() >= before);
    // the dummy driver has no clipboard: no text, no fault
    CHECK(platform::ClipboardText().empty());
}

// the window through its states: the size asked in a window, the screen's own in both fullscreen states
void CheckStates(const platform::Screen& screen)
{
    platform::WindowSettings settings{.state = platform::DisplayState::Window, .width = 800, .height = 600};
    auto window = platform::Window::Open("modernjk_test_platform", settings, platform::Context::None);
    CHECK(window.has_value());

    if (!window) {
        return;
    }

    const SDL_Window* const handle = window->Handle();
    CHECK(handle != nullptr);
    CHECK_EQUAL(window->Size(), (platform::PixelSize{.width = 800, .height = 600}));

    settings.width = 1024;
    settings.height = 576;
    CHECK(window->Apply(settings));
    CHECK_EQUAL(window->Size(), (platform::PixelSize{.width = 1024, .height = 576}));

    const platform::PixelSize desktop{.width = screen.desktop.width, .height = screen.desktop.height};
    settings.state = platform::DisplayState::Borderless;
    CHECK(window->Apply(settings));
    CHECK_EQUAL(window->Size(), desktop);

    // exclusive fullscreen in the screen's present mode; a screen without a mode of its own (the dummy driver's)
    // refuses it and the window stays as it was
    settings.state = platform::DisplayState::Exclusive;
    CHECK_EQUAL(window->Apply(settings), !screen.modes.empty());
    CHECK_EQUAL(window->Size(), desktop);

    if (!screen.modes.empty()) {
        settings.exclusive = screen.modes.back();
        CHECK(window->Apply(settings));
        CHECK_EQUAL(window->Size(), (platform::PixelSize{.width = settings.exclusive.width, .height = settings.exclusive.height}));
    }

    // a screen the system does not have is its primary one
    settings.state = platform::DisplayState::Window;
    settings.screen = 99;
    CHECK(window->Apply(settings));
    CHECK_EQUAL(window->Size(), (platform::PixelSize{.width = 1024, .height = 576}));

    // the same window all along
    CHECK(window->Handle() == handle);

    // without a context
    CHECK(!window->Present());
    CHECK(!window->SetSwapInterval(platform::SwapInterval::VerticalSync));
    CHECK(window->GlProcAddress("glClear") == nullptr);
    CHECK(!window->GlExtensionSupported("GL_ARB_multisample"));

    // images: 2 by 2 pixels taken as the pointer's, bytes that are not the size's refused
    const std::vector<std::uint8_t> pixels(2 * 2 * 4, 255);
    const platform::Image image{.rgba = pixels, .size = {.width = 2, .height = 2}};
    const platform::Image wrong{.rgba = pixels, .size = {.width = 3, .height = 2}};
    CHECK(window->SetPointerImage(image, {.width = 8, .height = 6}, {.x = 1, .y = 1}));
    CHECK(!window->SetPointerImage(image, {}, {}));
    CHECK(!window->SetIcon(wrong));
    CHECK(!window->SetPointerImage(wrong, {.width = 8, .height = 6}, {}));
    // the dummy driver has no icon to set and no window to minimize: it says so, and nothing faults
    CHECK(!window->SetIcon(image));
    CHECK(!window->Minimize());
}

// the dummy driver has no OpenGL: the request fails, and the text says so
void CheckContextRefused()
{
    const auto window = platform::Window::Open("modernjk_test_platform", platform::WindowSettings{}, platform::Context::OpenGl);
    CHECK(!window.has_value());

    if (!window) {
        CHECK(window.error().contains("OpenGL"));
    }
}

} // namespace

// runs the window's checks on the dummy driver's one screen
void RunWindowTests()
{
    if (!StartOnDummyDriver()) {
        return;
    }

    CheckLayer();
    const std::vector<platform::Screen> screens = platform::Screens();
    CHECK(!screens.empty());

    if (!screens.empty()) {
        CHECK(screens.front().desktop.width > 0 && screens.front().desktop.height > 0);
        // without the modes, the same screens
        const std::vector<platform::Screen> quick = platform::Screens(platform::ScreenModes::Skipped);
        CHECK(quick.size() == screens.size() && quick.front().modes.empty());
        CHECK(!quick.empty() && quick.front().name == screens.front().name && quick.front().desktop == screens.front().desktop);
        // the dummy driver has no taskbar: the usable area is the whole screen
        CHECK(screens.front().usable == (platform::PixelSize{.width = screens.front().desktop.width, .height = screens.front().desktop.height}));
        CheckStates(screens.front());
    }

    CheckContextRefused();
    platform::Stop();
}

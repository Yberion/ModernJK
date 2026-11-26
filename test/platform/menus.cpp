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

// The tests of the display's rows of the menus (src/shared/platform/menus), on two screens written here, as SDL
// lists them: the screen a number names; the display's state from its setting; a window's sizes, which fit in the usable area, and exclusive fullscreen's;
// a size's rates, the highest first; a mode read from its text and written back; each row's text; a click on each
// row, forward and back, wrapping, with exclusive fullscreen's rate kept, taken highest or nearest.
#include "platform.h"

#include <check.h>

#include <shared/platform/menus/display.h>

#include <span>
#include <vector>

namespace {

using platform::DisplayMode;
using platform::DisplayState;
using platform::PixelSize;
using platform::Screen;
using platform::WindowSettings;

// two screens 1920 by 1080, the first with a taskbar of 48 pixels, each mode largest first and each size's highest
// rate first, as SDL lists them
std::vector<Screen> TwoScreens()
{
    Screen first{.name = "AW2724HF", .desktop = {.width = 1920, .height = 1080, .refresh = 360.0F}, .usable = {.width = 1920, .height = 1032}};
    first.modes = {{1920, 1080, 360.0F}, {1920, 1080, 240.0F}, {1920, 1080, 60.0F}, {1680, 1050, 360.0F}, {1680, 1050, 60.0F},
                   {1600, 1024, 360.0F}, {1600, 900, 360.0F},  {1600, 900, 60.0F},  {1280, 720, 360.0F},  {1280, 720, 144.0F}};
    Screen second{.name = "AW2525HM", .desktop = {.width = 1920, .height = 1080, .refresh = 319.98F}, .usable = {.width = 1920, .height = 1080}};
    second.modes = {{1920, 1080, 319.98F}, {1920, 1080, 240.0F}, {1920, 1080, 144.23F}, {1280, 720, 144.23F}};
    return {first, second};
}

// settings in a state, on a screen, of a window's size and an exclusive mode
WindowSettings Settings(DisplayState state, int screen, PixelSize window, DisplayMode exclusive)
{
    return {.state = state, .screen = screen, .width = window.width, .height = window.height, .exclusive = exclusive};
}

// the screen a number names, and the lists of a screen's modes
void CheckLists(std::span<const Screen> screens)
{
    CHECK(platform::ScreenOf({}, 0) == nullptr);
    CHECK(platform::ScreenOf(screens, 1) == &screens[1]);
    CHECK(platform::ScreenOf(screens, 5) == &screens[0]);
    CHECK(platform::ScreenOf(screens, -1) == &screens[0]);

    // 1920 X 1080 and 1680 X 1050 are taller than the 1032 pixels the taskbar leaves
    const std::vector<PixelSize> windowed{{1600, 1024}, {1600, 900}, {1280, 720}};
    CHECK(platform::WindowSizes(screens[0]) == windowed);
    const std::vector<PixelSize> wholeScreen{{1920, 1080}, {1280, 720}};
    CHECK(platform::WindowSizes(screens[1]) == wholeScreen);
    const std::vector<PixelSize> exclusive{{1920, 1080}, {1680, 1050}, {1600, 1024}, {1600, 900}, {1280, 720}};
    CHECK(platform::ExclusiveSizes(screens[0]) == exclusive);

    const std::vector<float> rates{360.0F, 240.0F, 60.0F};
    CHECK(platform::RefreshRates(screens[0], {1920, 1080}) == rates);
    CHECK(platform::RefreshRates(screens[0], {800, 600}).empty());
}

// the display's state from its setting and back, and a mode from its text, back to its text, and what is no mode
void CheckSettingTexts()
{
    for (const DisplayState state : {DisplayState::Window, DisplayState::Borderless, DisplayState::Exclusive}) {
        CHECK(platform::DisplayStateOf(platform::DisplayStateSetting(state)) == state);
    }

    CHECK_EQUAL(platform::DisplayStateSetting(DisplayState::Exclusive), 2);
    CHECK(platform::DisplayStateOf(5) == DisplayState::Borderless && platform::DisplayStateOf(-1) == DisplayState::Borderless);

    CHECK(platform::ParseMode("1920x1080@119.88") == (DisplayMode{1920, 1080, 119.88F}));
    CHECK(platform::ParseMode("1920x1080") == (DisplayMode{1920, 1080, 0.0F}));
    CHECK(platform::ParseMode("1920x1080@-5") == (DisplayMode{1920, 1080, 0.0F}));
    CHECK(platform::ParseMode("") == DisplayMode{});
    CHECK(platform::ParseMode("borderless") == DisplayMode{});
    CHECK(platform::ParseMode("0x1080@60") == DisplayMode{});
    CHECK(platform::ParseMode("1920x") == DisplayMode{});

    CHECK(platform::ModeText({}).empty());
    CHECK_EQUAL(platform::ModeText({1920, 1080, 119.88F}), "1920x1080@119.88");
    CHECK_EQUAL(platform::ModeText({1280, 720, 0.0F}), "1280x720");

    // the rates SDL gives come back as they were
    for (const float rate : {319.98F, 144.23F, 254.87F, 59.94F, 23.976F}) {
        const DisplayMode mode{1920, 1080, rate};
        CHECK(platform::ParseMode(platform::ModeText(mode)) == mode);
    }
}

// each row's text, and whether it offers a choice
void CheckTexts(std::span<const Screen> screens)
{
    CHECK_EQUAL(platform::ScreenText(screens, 0), "1: AW2724HF");
    CHECK_EQUAL(platform::ScreenText(screens, 1), "2: AW2525HM");
    CHECK_EQUAL(platform::ScreenText(screens, 7), "1: AW2724HF");
    CHECK(platform::ScreenText({}, 0).empty());
    std::vector<Screen> unnamed(screens.begin(), screens.end());
    unnamed[1].name.clear();
    CHECK_EQUAL(platform::ScreenText(unnamed, 1), "2");

    const WindowSettings window = Settings(DisplayState::Window, 0, {1280, 720}, {});
    const WindowSettings borderless = Settings(DisplayState::Borderless, 1, {1280, 720}, {});
    const WindowSettings desktop = Settings(DisplayState::Exclusive, 0, {1280, 720}, {});
    const WindowSettings mode = Settings(DisplayState::Exclusive, 0, {1280, 720}, {1600, 900, 60.0F});
    CHECK_EQUAL(platform::ResolutionText(screens, window, "Desktop"), "1280 X 720");
    CHECK_EQUAL(platform::ResolutionText(screens, borderless, "Desktop"), "1920 X 1080");
    CHECK_EQUAL(platform::ResolutionText(screens, desktop, "Desktop"), "Desktop");
    CHECK_EQUAL(platform::ResolutionText(screens, mode, "Desktop"), "1600 X 900");

    CHECK_EQUAL(platform::RefreshText(screens, window), "360 Hz");
    CHECK_EQUAL(platform::RefreshText(screens, borderless), "319.98 Hz");
    CHECK_EQUAL(platform::RefreshText(screens, desktop), "360 Hz");
    CHECK_EQUAL(platform::RefreshText(screens, mode), "60 Hz");
    // no rate asked: the highest; a rate the screen lacks: the nearest
    CHECK_EQUAL(platform::RefreshText(screens, Settings(DisplayState::Exclusive, 0, {}, {1600, 900, 0.0F})), "360 Hz");
    CHECK_EQUAL(platform::RefreshText(screens, Settings(DisplayState::Exclusive, 0, {}, {1600, 900, 61.0F})), "60 Hz");
    CHECK(platform::RefreshText({}, window).empty());

    CHECK(platform::ResolutionHasChoice(window) && platform::ResolutionHasChoice(desktop) && !platform::ResolutionHasChoice(borderless));
    CHECK(platform::RefreshHasChoice(mode) && !platform::RefreshHasChoice(desktop) && !platform::RefreshHasChoice(window));
}

// clicks on the resolution row: a window's sizes, then exclusive fullscreen's with the desktop's mode first
void CheckResolutionClicks(std::span<const Screen> screens)
{
    const auto next = [screens](const WindowSettings& settings, bool back) { return platform::NextResolution(screens, settings, back); };
    const auto windowSize = [](const WindowSettings& settings) { return PixelSize{settings.width, settings.height}; };

    const WindowSettings window = Settings(DisplayState::Window, 0, {1280, 720}, {});
    CHECK(windowSize(next(window, false)) == (PixelSize{1600, 1024}));
    CHECK(windowSize(next(window, true)) == (PixelSize{1600, 900}));
    // a size typed in the console, not in the list
    const WindowSettings typed = Settings(DisplayState::Window, 0, {1000, 700}, {});
    CHECK(windowSize(next(typed, false)) == (PixelSize{1600, 1024}));
    CHECK(windowSize(next(typed, true)) == (PixelSize{1280, 720}));
    const WindowSettings borderless = Settings(DisplayState::Borderless, 0, {1280, 720}, {});
    CHECK(next(borderless, false) == borderless);

    const WindowSettings desktop = Settings(DisplayState::Exclusive, 0, {1280, 720}, {});
    CHECK(next(desktop, false).exclusive == (DisplayMode{1920, 1080, 360.0F}));
    CHECK(next(desktop, true).exclusive == (DisplayMode{1280, 720, 360.0F}));
    // the rate kept when the next size has it, else the next size's highest
    CHECK(next(Settings(DisplayState::Exclusive, 0, {}, {1920, 1080, 60.0F}), false).exclusive == (DisplayMode{1680, 1050, 60.0F}));
    CHECK(next(Settings(DisplayState::Exclusive, 0, {}, {1680, 1050, 60.0F}), false).exclusive == (DisplayMode{1600, 1024, 360.0F}));
    // past the last size, the desktop's mode
    CHECK(next(Settings(DisplayState::Exclusive, 0, {}, {1280, 720, 144.0F}), false).exclusive == DisplayMode{});
    // a size the screen does not have: the first size, its rate kept since that size has it
    CHECK(next(Settings(DisplayState::Exclusive, 0, {}, {800, 600, 60.0F}), false).exclusive == (DisplayMode{1920, 1080, 60.0F}));
    CHECK(next(Settings(DisplayState::Exclusive, 0, {}, {800, 600, 75.0F}), true).exclusive == (DisplayMode{1280, 720, 360.0F}));
}

// clicks on the refresh rate and screen rows
void CheckRateAndScreenClicks(std::span<const Screen> screens)
{
    const WindowSettings fastest = Settings(DisplayState::Exclusive, 0, {}, {1920, 1080, 360.0F});
    CHECK(platform::NextRefreshRate(screens, fastest, false).exclusive.refresh == 240.0F);
    CHECK(platform::NextRefreshRate(screens, fastest, true).exclusive.refresh == 60.0F);
    const WindowSettings desktop = Settings(DisplayState::Exclusive, 0, {}, {});
    CHECK(platform::NextRefreshRate(screens, desktop, false) == desktop);
    const WindowSettings window = Settings(DisplayState::Window, 0, {1280, 720}, {1920, 1080, 360.0F});
    CHECK(platform::NextRefreshRate(screens, window, false) == window);

    CHECK_EQUAL(platform::NextScreen(screens, window, false).screen, 1);
    CHECK_EQUAL(platform::NextScreen(screens, window, true).screen, 1);
    CHECK_EQUAL(platform::NextScreen(screens.first(1), window, false).screen, 0);
    // exclusive fullscreen's mode on the new screen: none of that size, the same rate, the nearest rate
    CHECK(platform::NextScreen(screens, Settings(DisplayState::Exclusive, 0, {}, {1600, 900, 60.0F}), false).exclusive == DisplayMode{});
    CHECK(platform::NextScreen(screens, Settings(DisplayState::Exclusive, 0, {}, {1920, 1080, 240.0F}), false).exclusive == (DisplayMode{1920, 1080, 240.0F}));
    CHECK(platform::NextScreen(screens, fastest, false).exclusive == (DisplayMode{1920, 1080, 319.98F}));
}

} // namespace

void RunMenusTests()
{
    const std::vector<Screen> screens = TwoScreens();
    CheckLists(screens);
    CheckSettingTexts();
    CheckTexts(screens);
    CheckResolutionClicks(screens);
    CheckRateAndScreenClicks(screens);
}

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

// display.h -- the display's rows of the setup menus, the same in the three games (display.cpp): what the screen,
// resolution and refresh rate rows show and the value a click on each gives, from the screens the layer lists and the
// display's settings the menus edit before they are applied. Functions of their arguments only: the menus read and
// write their own settings, the engine lists the screens (platform::Screens).
// Example: in a window, the resolution row steps through the screen's sizes that fit in its usable area:
//     settings = platform::NextResolution(screens, settings, false); // 1600 X 1024, then 1600 X 900...
//     const std::string shown = platform::ResolutionText(screens, settings, "Desktop");
#pragma once

#include <shared/platform/window/window.h>

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace platform {

// The screen a number names: the one of that number, the system's primary one when there is none of it; null when
// no screen is listed.
[[nodiscard]] const Screen* ScreenOf(std::span<const Screen> screens, int number) noexcept;

// The sizes a window can take on a screen, the largest first, each once: the screen's own sizes that fit in its
// usable area.
[[nodiscard]] std::vector<PixelSize> WindowSizes(const Screen& screen);
// The sizes exclusive fullscreen can set on a screen, the largest first, each once.
[[nodiscard]] std::vector<PixelSize> ExclusiveSizes(const Screen& screen);
// The refresh rates of a screen's modes of one size, the highest first.
[[nodiscard]] std::vector<float> RefreshRates(const Screen& screen, PixelSize size);

// The display's state r_displayState names: 0 a window, 1 borderless fullscreen, 2 exclusive fullscreen; any other
// value borderless fullscreen, the default.
[[nodiscard]] DisplayState DisplayStateOf(int setting) noexcept;
// The value of r_displayState that names a display's state.
[[nodiscard]] int DisplayStateSetting(DisplayState state) noexcept;

// The mode a text names, "1920x1080@144" or "1920x1080" (no rate asked); all zero, the screen's present mode, for an
// empty text or one of another form.
[[nodiscard]] DisplayMode ParseMode(std::string_view text) noexcept;
// A mode's text as ParseMode reads it: "1920x1080@119.88"; empty for all zero.
[[nodiscard]] std::string ModeText(const DisplayMode& mode);

// Whether the resolution row offers a choice: in a window and in exclusive fullscreen, not in borderless
// fullscreen, which takes the desktop's mode.
[[nodiscard]] bool ResolutionHasChoice(const WindowSettings& settings) noexcept;
// Whether the refresh rate row offers a choice: in exclusive fullscreen with a mode of its own.
[[nodiscard]] bool RefreshHasChoice(const WindowSettings& settings) noexcept;

// The screen row's text: the screen's number from 1, which tells two screens of one name apart, and its name.
[[nodiscard]] std::string ScreenText(std::span<const Screen> screens, int number);
// The resolution row's text: the window's size, the size of exclusive fullscreen's mode, or the desktop's in
// borderless fullscreen ("1920 X 1080", as the games wrote their modes); the word given for exclusive fullscreen at
// the desktop's mode.
[[nodiscard]] std::string ResolutionText(std::span<const Screen> screens, const WindowSettings& settings, std::string_view desktop);
// The refresh rate row's text: exclusive fullscreen's mode's rate, else the desktop's ("144 Hz", "119.88 Hz"); empty
// when the system does not tell.
[[nodiscard]] std::string RefreshText(std::span<const Screen> screens, const WindowSettings& settings);

// A click on the screen row: the next screen, or the one before when back. Exclusive fullscreen's mode follows: the
// new screen's mode of that size at the nearest rate, its desktop's when it has no mode of that size.
[[nodiscard]] WindowSettings NextScreen(std::span<const Screen> screens, WindowSettings settings, bool back);
// A click on the resolution row: the next size, or the one before when back. In a window, the sizes of WindowSizes;
// in exclusive fullscreen, the desktop's mode then the sizes of ExclusiveSizes, a size keeping the rate it had when
// it has it, else taking its highest. Nothing changes in borderless fullscreen.
[[nodiscard]] WindowSettings NextResolution(std::span<const Screen> screens, WindowSettings settings, bool back);
// A click on the refresh rate row: the next rate of the mode's size, or the one before when back; nothing changes
// without a choice (RefreshHasChoice).
[[nodiscard]] WindowSettings NextRefreshRate(std::span<const Screen> screens, WindowSettings settings, bool back);

} // namespace platform

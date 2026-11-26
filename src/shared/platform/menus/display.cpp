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

// display.cpp -- the display's rows of the setup menus (display.h): each row's list built from a screen's modes, its
// text, and one step through it. A list wraps around: past its last value comes its first, as the games' other rows.
// Example: on a screen 1920 by 1080 whose taskbar leaves 1920 by 1032, a window's sizes start at 1600 X 1024; 1680 X
// 1050 does not fit.
#include "display.h"

#include <fmt/format.h>

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstddef>
#include <functional>
#include <iterator>
#include <system_error>

namespace platform {

namespace {

// The size of a mode.
constexpr PixelSize SizeOf(const DisplayMode& mode) noexcept
{
    return {.width = mode.width, .height = mode.height};
}

// The place one step from another in a list of count places, wrapping around: the next one, or the one before when
// back.
std::size_t Step(std::size_t index, std::size_t count, bool back) noexcept
{
    assert(count > 0 && index < count);
    return back ? (index + count - 1) % count : (index + 1) % count;
}

// The place of the screen a number names, as ScreenOf chooses it; the list not empty.
std::size_t ScreenIndex(std::span<const Screen> screens, int number) noexcept
{
    assert(!screens.empty());
    const auto index = static_cast<std::size_t>(std::max(number, 0));
    return index < screens.size() ? index : 0;
}

// The place of the rate nearest to one asked, the list not empty; the first, the highest, when none is asked.
std::size_t NearestRate(std::span<const float> rates, float wanted) noexcept
{
    assert(!rates.empty());

    if (wanted <= 0.0F) {
        return 0;
    }

    std::size_t nearest = 0;
    float nearestGap = -1.0F;

    for (std::size_t index = 0; index < rates.size(); ++index) {
        const float gap = rates[index] > wanted ? rates[index] - wanted : wanted - rates[index];

        if (nearestGap < 0.0F || gap < nearestGap) {
            nearest = index;
            nearestGap = gap;
        }
    }

    return nearest;
}

// The sizes of a screen's modes that a test keeps, in the modes' order, each once.
template <class Keep> std::vector<PixelSize> SizesOf(const Screen& screen, Keep keep)
{
    std::vector<PixelSize> sizes;

    for (const DisplayMode& mode : screen.modes) {
        const PixelSize size = SizeOf(mode);

        if (keep(size) && std::ranges::find(sizes, size) == sizes.end()) {
            sizes.push_back(size);
        }
    }

    return sizes;
}

// A size as the games wrote their modes in the menus.
std::string SizeText(int width, int height)
{
    return fmt::format("{} X {}", width, height);
}

} // namespace

const Screen* ScreenOf(std::span<const Screen> screens, int number) noexcept
{
    return screens.empty() ? nullptr : &screens[ScreenIndex(screens, number)];
}

std::vector<PixelSize> WindowSizes(const Screen& screen)
{
    // the window's title bar and borders are not counted: the system tells them only for a window that has them, and
    // the menus list the sizes while the game may be in fullscreen
    return SizesOf(screen, [&screen](PixelSize size) { return size.width <= screen.usable.width && size.height <= screen.usable.height; });
}

std::vector<PixelSize> ExclusiveSizes(const Screen& screen)
{
    return SizesOf(screen, [](PixelSize /*size*/) { return true; });
}

std::vector<float> RefreshRates(const Screen& screen, PixelSize size)
{
    std::vector<float> rates;

    for (const DisplayMode& mode : screen.modes) {
        if (SizeOf(mode) == size) {
            rates.push_back(mode.refresh);
        }
    }

    std::ranges::sort(rates, std::greater{});
    return rates;
}

DisplayState DisplayStateOf(int setting) noexcept
{
    switch (setting) {
    case 0:
        return DisplayState::Window;
    case 2:
        return DisplayState::Exclusive;
    default:
        return DisplayState::Borderless;
    }
}

int DisplayStateSetting(DisplayState state) noexcept
{
    switch (state) {
    case DisplayState::Window:
        return 0;
    case DisplayState::Exclusive:
        return 2;
    case DisplayState::Borderless:
        break;
    }

    return 1;
}

DisplayMode ParseMode(std::string_view text) noexcept
{
    DisplayMode mode;
    // the text's bounds: from_chars reads between them, never up to a terminating null
    const char* const first = text.data();
    const char* const end = first + text.size();
    const std::from_chars_result width = std::from_chars(first, end, mode.width);

    if (width.ec != std::errc{} || width.ptr == end || *width.ptr != 'x') {
        return {};
    }

    const std::from_chars_result height = std::from_chars(std::next(width.ptr), end, mode.height);

    if (height.ec != std::errc{} || mode.width <= 0 || mode.height <= 0) {
        return {};
    }

    // a rate that cannot be read, or is not positive, is no rate asked
    if (height.ptr != end && *height.ptr == '@') {
        float refresh = 0.0F;
        const std::from_chars_result rate = std::from_chars(std::next(height.ptr), end, refresh);
        mode.refresh = rate.ec == std::errc{} && refresh > 0.0F ? refresh : 0.0F;
    }

    return mode;
}

std::string ModeText(const DisplayMode& mode)
{
    if (mode == DisplayMode{}) {
        return {};
    }

    return mode.refresh > 0.0F ? fmt::format("{}x{}@{}", mode.width, mode.height, mode.refresh) : fmt::format("{}x{}", mode.width, mode.height);
}

bool ResolutionHasChoice(const WindowSettings& settings) noexcept
{
    return settings.state != DisplayState::Borderless;
}

bool RefreshHasChoice(const WindowSettings& settings) noexcept
{
    return settings.state == DisplayState::Exclusive && settings.exclusive != DisplayMode{};
}

std::string ScreenText(std::span<const Screen> screens, int number)
{
    if (screens.empty()) {
        return {};
    }

    const std::size_t index = ScreenIndex(screens, number);
    const std::string& name = screens[index].name;
    return name.empty() ? fmt::format("{}", index + 1) : fmt::format("{}: {}", index + 1, name);
}

std::string ResolutionText(std::span<const Screen> screens, const WindowSettings& settings, std::string_view desktop)
{
    switch (settings.state) {
    case DisplayState::Window:
        return SizeText(settings.width, settings.height);
    case DisplayState::Exclusive:
        return settings.exclusive == DisplayMode{} ? std::string(desktop) : SizeText(settings.exclusive.width, settings.exclusive.height);
    case DisplayState::Borderless:
        break;
    }

    const Screen* const screen = ScreenOf(screens, settings.screen);
    return screen != nullptr ? SizeText(screen->desktop.width, screen->desktop.height) : std::string();
}

std::string RefreshText(std::span<const Screen> screens, const WindowSettings& settings)
{
    const Screen* const screen = ScreenOf(screens, settings.screen);

    if (screen == nullptr) {
        return {};
    }

    float rate = screen->desktop.refresh;

    // the rate the window will take: of the screen's modes of that size, the nearest, or the highest when none is asked
    if (RefreshHasChoice(settings)) {
        const std::vector<float> rates = RefreshRates(*screen, SizeOf(settings.exclusive));
        rate = rates.empty() ? settings.exclusive.refresh : rates[NearestRate(rates, settings.exclusive.refresh)];
    }

    return rate > 0.0F ? fmt::format("{} Hz", rate) : std::string();
}

WindowSettings NextScreen(std::span<const Screen> screens, WindowSettings settings, bool back)
{
    if (screens.empty()) {
        return settings;
    }

    const std::size_t next = Step(ScreenIndex(screens, settings.screen), screens.size(), back);
    settings.screen = static_cast<int>(next);

    if (settings.exclusive != DisplayMode{}) {
        const PixelSize size = SizeOf(settings.exclusive);
        const std::vector<float> rates = RefreshRates(screens[next], size);
        settings.exclusive = rates.empty()
                                 ? DisplayMode{}
                                 : DisplayMode{.width = size.width, .height = size.height, .refresh = rates[NearestRate(rates, settings.exclusive.refresh)]};
    }

    return settings;
}

WindowSettings NextResolution(std::span<const Screen> screens, WindowSettings settings, bool back)
{
    const Screen* const screen = ScreenOf(screens, settings.screen);

    if (screen == nullptr || !ResolutionHasChoice(settings)) {
        return settings;
    }

    if (settings.state == DisplayState::Window) {
        const std::vector<PixelSize> sizes = WindowSizes(*screen);

        if (sizes.empty()) {
            return settings;
        }

        // a size typed in the console, not in the list, goes to the list's first, or its last when back
        const auto found = std::ranges::find(sizes, PixelSize{.width = settings.width, .height = settings.height});
        std::size_t next = back ? sizes.size() - 1 : 0;

        if (found != sizes.end()) {
            next = Step(static_cast<std::size_t>(found - sizes.begin()), sizes.size(), back);
        }

        settings.width = sizes[next].width;
        settings.height = sizes[next].height;
        return settings;
    }

    // exclusive fullscreen: the desktop's mode at place 0, then each size
    const std::vector<PixelSize> sizes = ExclusiveSizes(*screen);
    const std::size_t count = sizes.size() + 1;
    std::size_t next = 0;

    if (settings.exclusive == DisplayMode{}) {
        next = Step(0, count, back);
    }
    else if (const auto found = std::ranges::find(sizes, SizeOf(settings.exclusive)); found != sizes.end()) {
        next = Step(static_cast<std::size_t>(found - sizes.begin()) + 1, count, back);
    }
    else {
        // a size the screen does not have: the first size, or the last when back
        next = back ? count - 1 : std::min<std::size_t>(1, count - 1);
    }

    if (next == 0) {
        settings.exclusive = {};
        return settings;
    }

    const PixelSize size = sizes[next - 1];
    const std::vector<float> rates = RefreshRates(*screen, size);
    const bool keep = std::ranges::find(rates, settings.exclusive.refresh) != rates.end();
    settings.exclusive = {.width = size.width, .height = size.height, .refresh = keep ? settings.exclusive.refresh : rates.front()};
    return settings;
}

WindowSettings NextRefreshRate(std::span<const Screen> screens, WindowSettings settings, bool back)
{
    const Screen* const screen = ScreenOf(screens, settings.screen);

    if (screen == nullptr || !RefreshHasChoice(settings)) {
        return settings;
    }

    const std::vector<float> rates = RefreshRates(*screen, SizeOf(settings.exclusive));

    if (rates.empty()) {
        return settings;
    }

    settings.exclusive.refresh = rates[Step(NearestRate(rates, settings.exclusive.refresh), rates.size(), back)];
    return settings;
}

} // namespace platform

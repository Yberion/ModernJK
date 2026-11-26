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

// samples.h -- the samples a pixel of the scene's multisampling can have (r_ext_multisample): none, or 2, 4 or 8 at
// most what the driver allows. One definition for the stage, which makes its images with them, and for the video
// menus' row of the three games, which offers them; it needs nothing but the standard library.
// Example: the menu's row goes to the next value at a click, the renderer having told the driver's limit:
//     const int next = render_output::NextSamples(r_ext_multisample, r_ext_multisample_avail, false);
#pragma once

#include <array>
#include <cstddef>

namespace render_output {

// the samples a pixel the stage offers, the largest first
inline constexpr std::array<int, 3> kSampleCounts{8, 4, 2};

// The samples the stage uses for those asked, with the driver's maximum: 0 for less than 2, else the largest of 2, 4
// and 8 not above either.
[[nodiscard]] constexpr int AllowedSamples(int asked, int driverMaximum) noexcept
{
    const int limit = asked < driverMaximum ? asked : driverMaximum;

    for (const int count : kSampleCounts) {
        if (count <= limit) {
            return count;
        }
    }

    return 0;
}

// The value a menu's row goes to from current: the next of 0 and the counts the driver allows, in rising order, or
// the one before it (back), after the last the first again.
[[nodiscard]] constexpr int NextSamples(int current, int driverMaximum, bool back) noexcept
{
    // the values offered, rising: 0, then the allowed counts
    std::array<int, kSampleCounts.size() + 1> values{};
    std::size_t count = 1;

    for (std::size_t index = kSampleCounts.size(); index > 0; --index) {
        if (kSampleCounts[index - 1] <= driverMaximum) {
            values[count] = kSampleCounts[index - 1];
            ++count;
        }
    }

    const int now = AllowedSamples(current, driverMaximum);
    std::size_t position = 0;

    for (std::size_t index = 0; index < count; ++index) {
        if (values[index] == now) {
            position = index;
        }
    }

    return values[back ? (position + count - 1) % count : (position + 1) % count];
}

} // namespace render_output

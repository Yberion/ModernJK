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

// button.h -- a button of the game's commands (+forward, +attack...), the same in the three games (button.cpp): the
// keys holding it, the time it went down, and the share of a span it was held, from the keys' own times in
// nanoseconds on the client's clock. Two keys can hold one button, which is up once both are released.
// Example: W pressed 3.2 ms into a tick of 10 ms and still held at its end counts 0.68 of the tick:
//     client_input::PressButton(forward, key, tickStart + 3'200'000);
//     const float share = client_input::HeldShare(forward, tickStart + 10'000'000, 10'000'000); // 0.68
#pragma once

#include <array>
#include <cstdint>
#include <optional>

// A button: the keys holding it (their numbers, 0 for none, -1 for the console's command typed without a key), the
// time it went down (0 when not known), the nanoseconds it was held since its share was last asked, from the times it
// went down and up in between; whether it is down, and whether it went down since the commands last read it. The
// engines' type, in their shape.
struct kbutton_t {
    std::array<int, 2> down;
    std::int64_t downtime;
    std::int64_t nsec;
    bool active;
    bool wasPressed;
};

namespace client_input {

// A key pressed on a button at a time (0 when not known): the button goes down with its first key, a key already
// holding it changes nothing (a repeat). False when two other keys hold it already: a third is not counted.
[[nodiscard]] bool PressButton(kbutton_t& button, int key, std::int64_t time) noexcept;

// A key released from a button at a time (0 when not known): the button goes up when no other key holds it, its time
// down counted until then, half of frameTime when the release's time is not known. A key that does not hold it changes
// nothing; no key at all (the console's command typed without one) releases the button whatever holds it.
void ReleaseButton(kbutton_t& button, std::optional<int> key, std::int64_t time, std::int64_t frameTime) noexcept;

// The share of a span, up to now, a button was held since its share was last asked, between 0 and 1: its times down
// counted, and for a button still down the time from when it went down (or from the last ask) to now, the whole span
// when that time is not known. The count starts again from now.
[[nodiscard]] float HeldShare(kbutton_t& button, std::int64_t now, std::int64_t span) noexcept;

} // namespace client_input

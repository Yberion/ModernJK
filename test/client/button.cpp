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

// The tests of a button (src/shared/client/button.h): a press, a repeat, two keys holding it and a third refused; a
// release of a key that does not hold it, of one key of two, of the console's command without a key; the share of a
// span: held through it, pressed in it, pressed and released in it, released without a time, down from an unknown
// time, then counted again from the last ask; between 0 and 1 whatever the times.
#include "client.h"

#include <check.h>

#include <shared/client/button.h>

#include <cstdint>

namespace {

using client_input::HeldShare;
using client_input::PressButton;
using client_input::ReleaseButton;

constexpr std::int64_t kMs = 1'000'000;

// a button's presses by keys: down with its first, a repeat changing nothing, two keys holding it, a third refused;
// its releases: a key that does not hold it, one of two, the last, the console's command without a key
void TestKeys()
{
    kbutton_t button{};

    CHECK(PressButton(button, 17, 100 * kMs));
    CHECK(button.active);
    CHECK(button.wasPressed);
    CHECK_EQUAL(button.downtime, 100 * kMs);

    CHECK(PressButton(button, 17, 150 * kMs));
    CHECK_EQUAL(button.downtime, 100 * kMs);

    CHECK(PressButton(button, 42, 120 * kMs));
    CHECK_EQUAL(button.downtime, 100 * kMs);
    CHECK(!PressButton(button, 99, 130 * kMs));

    ReleaseButton(button, 7, 140 * kMs, 10 * kMs);
    CHECK(button.active);
    ReleaseButton(button, 17, 140 * kMs, 10 * kMs);
    CHECK(button.active);
    ReleaseButton(button, 42, 160 * kMs, 10 * kMs);
    CHECK(!button.active);
    CHECK_EQUAL(button.nsec, 60 * kMs);

    kbutton_t typed{};
    CHECK(PressButton(typed, -1, 0));
    CHECK(PressButton(typed, 5, 10 * kMs));
    ReleaseButton(typed, std::nullopt, 0, 10 * kMs);
    CHECK(!typed.active);
    CHECK_EQUAL(typed.down[0], 0);
    CHECK_EQUAL(typed.down[1], 0);
}

// the share of a span a button was held
void TestShares()
{
    // pressed 5 ms before the end of a span of 10 ms: half of it, then the whole next span
    kbutton_t held{};
    CHECK(PressButton(held, 1, 95 * kMs));
    CHECK_EQUAL(HeldShare(held, 100 * kMs, 10 * kMs), 0.5F);
    CHECK_EQUAL(HeldShare(held, 110 * kMs, 10 * kMs), 1.0F);

    // pressed 3.2 ms into a tick of 10 ms, still held at its end
    kbutton_t pressed{};
    CHECK(PressButton(pressed, 1, 103'200'000));
    CHECK_CLOSE_FRACTION(HeldShare(pressed, 110 * kMs, 10 * kMs), 0.68F, 1e-6F);

    // pressed and released within the tick: its time down only, then nothing
    kbutton_t tap{};
    CHECK(PressButton(tap, 1, 102 * kMs));
    ReleaseButton(tap, 1, 105'200'000, 10 * kMs);
    CHECK_CLOSE_FRACTION(HeldShare(tap, 110 * kMs, 10 * kMs), 0.32F, 1e-6F);
    CHECK_EQUAL(HeldShare(tap, 120 * kMs, 10 * kMs), 0.0F);

    // released without a time: half a frame counted
    kbutton_t untimed{};
    CHECK(PressButton(untimed, 1, 102 * kMs));
    ReleaseButton(untimed, 1, 0, 8 * kMs);
    CHECK_EQUAL(untimed.nsec, 4 * kMs);

    // down from an unknown time (the console's command): the whole span
    kbutton_t console{};
    CHECK(PressButton(console, -1, 0));
    CHECK_EQUAL(HeldShare(console, 110 * kMs, 10 * kMs), 1.0F);

    // between 0 and 1 whatever the times: a release dated before the press, a span shorter than the time down
    kbutton_t early{};
    CHECK(PressButton(early, 1, 105 * kMs));
    ReleaseButton(early, 1, 100 * kMs, 10 * kMs);
    CHECK_EQUAL(HeldShare(early, 110 * kMs, 10 * kMs), 0.0F);
    kbutton_t longer{};
    CHECK(PressButton(longer, 1, 50 * kMs));
    CHECK_EQUAL(HeldShare(longer, 110 * kMs, 10 * kMs), 1.0F);
}

} // namespace

void RunButtonTests()
{
    TestKeys();
    TestShares();
}

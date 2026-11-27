/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
Copyright (C) 2013 - 2015, OpenJK contributors
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

// button.cpp -- a button of the game's commands (button.h), from the engines' IN_KeyDown, IN_KeyUp and CL_KeyState.
// The share is computed in double precision from the nanoseconds, then given as a float, which the commands' movement
// multiplies.
#include "button.h"

#include <algorithm>

namespace client_input {

bool PressButton(kbutton_t& button, int key, std::int64_t time) noexcept
{
    if (key == button.down[0] || key == button.down[1]) {
        return true; // a repeat
    }

    if (button.down[0] == 0) {
        button.down[0] = key;
    }
    else if (button.down[1] == 0) {
        button.down[1] = key;
    }
    else {
        return false;
    }

    if (button.active) {
        return true; // still down
    }

    button.downtime = time;
    button.active = true;
    button.wasPressed = true;
    return true;
}

void ReleaseButton(kbutton_t& button, std::optional<int> key, std::int64_t time, std::int64_t frameTime) noexcept
{
    if (!key) {
        button.down = {0, 0};
        button.active = false;
        return;
    }

    if (button.down[0] == *key) {
        button.down[0] = 0;
    }
    else if (button.down[1] == *key) {
        button.down[1] = 0;
    }
    else {
        return; // a release without its press: the press went to a menu
    }

    if (button.down[0] != 0 || button.down[1] != 0) {
        return; // another key still holds it
    }

    button.active = false;
    button.nsec += time != 0 ? time - button.downtime : frameTime / 2;
}

float HeldShare(kbutton_t& button, std::int64_t now, std::int64_t span) noexcept
{
    std::int64_t held = button.nsec;
    button.nsec = 0;

    if (button.active) {
        held = button.downtime == 0 ? span : held + (now - button.downtime);
        button.downtime = now;
    }

    return static_cast<float>(std::clamp(static_cast<double>(held) / static_cast<double>(span), 0.0, 1.0));
}

} // namespace client_input

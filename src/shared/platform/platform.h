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

// platform.h -- the client's door to the system, on SDL 3: the one clock, the start and the stop of the system's
// video and events, the clipboard and the error's message box. The window is in window/window.h, the keyboard and the
// mouse in input/input.h. Nothing here knows the game, and no SDL type appears in these headers.
// Example: an engine starts the layer, then opens its window and reads its input until it quits:
//     if (auto started = platform::Start("ModernJK", "1.0"); !started) { platform::ShowError("ModernJK", started.error()); }
//     const platform::Time frameStart = platform::Now();
//     ...
//     platform::Stop();
#pragma once

#include <chrono>
#include <expected>
#include <string>

namespace platform {

// A time on the client's one clock, in nanoseconds since the layer's library was loaded; it never goes back. Every
// event carries the time it happened at on this clock.
using Time = std::chrono::nanoseconds;

// The time now.
[[nodiscard]] Time Now() noexcept;

// Starts the system's video and events, once, before a window opens; name and version are the program's, which the
// system shows where it names a program (the sound mixer, the task switcher). The error's text when the system has
// no display to open a window on.
[[nodiscard]] std::expected<void, std::string> Start(const std::string& name, const std::string& version);

// Closes what Start opened; the windows are closed before.
void Stop() noexcept;

// One line about the layer for the program's log: the library's version and revision, and the video driver in use
// once started ("SDL 3.4.20 (SDL-release-3.4.20-0-gf00dc919c), video driver windows").
[[nodiscard]] std::string Description();

// The text the system's clipboard holds, in UTF-8; empty when it holds none.
[[nodiscard]] std::string ClipboardText();

// Shows a message box with an error's text and waits for the player to close it. It works before Start and after
// Stop, for the errors that stop the program.
void ShowError(const std::string& title, const std::string& text) noexcept;

} // namespace platform

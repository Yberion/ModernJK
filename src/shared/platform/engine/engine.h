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

// engine.h -- the platform layer as an engine runs it, compiled by each engine against its own headers (engine.cpp):
// the display's settings (r_displayState, r_screen, r_windowWidth, r_windowHeight, r_exclusiveMode, r_swapInterval,
// r_ext_multisample), its commands (modelist, minimize), the one window the client opens and keeps, what the renderer asks of it, and the input read
// and given to the client as its keys, its characters and its mouse.
// Example: the client opens the window before it starts its renderer (Platform_OpenWindow), gives the renderer
// Platform_Display, Platform_Present and the two OpenGL lookups, and calls Platform_Frame where it reads its input.
#pragma once

#include <shared/platform/window/window.h>

#include <cstdint>
#include <span>

struct glconfig_s;

// A function of the OpenGL library, which the renderer casts to its own type.
using Platform_GlFunction = void (*)();

// Starts the layer and opens the client's window from the display's settings, once: a call with the window open
// does nothing. A fatal error, with what failed, when no window or no OpenGL context can be made.
void Platform_OpenWindow();

// Closes the window and the layer, at the program's end.
void Platform_Shutdown();

// Reads the input: gives the mouse to the view when the client says it turns the view (CL_MouseTurnsView) and the
// window has the focus, and to the system's pointer everywhere else (a menu, the console, a level loading, no
// focus), turns the text input on while something takes characters, then hands every event since the call before to
// the client (CL_KeyEvent, CL_CharEvent, CL_MouseEvent, CL_PointerEvent), each key with the time it moved at and the
// mouse's motion with its fractions, and the window's own (the focus, minimized, quit). Nothing before the window is
// open.
void Platform_Frame();

// Whether a text is the name of a key the engine has no code of its own for, as the engine writes such a key in a
// bind: the system's name of the key in capitals, its spaces as underscores ("F13", "LEFT_GUI"), or the character
// the player's layout gives the key as "U+" and its Unicode number in hexadecimal ("U+00B4").
[[nodiscard]] bool Platform_IsKeyName(const char* name);

// The image of the menus' pointer, the system's own over the window: four bytes per pixel (red, green, blue, alpha),
// width times height of them, shown at shownWidth by shownHeight pixels, its top left corner the pixel that points.
// False when the system refused or before the window is open: the pointer stays the system's arrow.
[[nodiscard]] bool Platform_SetPointerImage(std::span<const std::uint8_t> rgba, int width, int height, int shownWidth, int shownHeight);

// The screens, the system's primary one first, with their modes, for the menus' display rows
// (src/shared/platform/menus/display.h): the modes are read at the first call, which waits for the system to enumerate
// them (half a second for two screens of some 440 modes each), the screens read again at each start of the renderer
// and when a screen is plugged or unplugged or its desktop's mode or usable area changes; empty before the window
// opens. The list stays valid until one of those.
std::span<const platform::Screen> Platform_Screens();

// The client's clock, the layer's: in milliseconds; in nanoseconds; and the time, in nanoseconds, at which
// Platform_Frame last read the input: no event it handed over is later.
[[nodiscard]] int Platform_Milliseconds();
[[nodiscard]] std::int64_t Platform_Nanoseconds();
[[nodiscard]] std::int64_t Platform_InputNanoseconds();

// For the renderer, each doing nothing before the window is open. The display as it is: the size in pixels, the
// bits, whether it fills the screen; the display's settings changed since the renderer last started are applied to
// the window first.
void Platform_Display(glconfig_s* glConfig);
// The image drawn is shown, r_swapInterval applied when it changed.
void Platform_Present();
// A function of the OpenGL library by its name, null when the library has none; under X11 a pointer comes back for
// any name, so a function of an extension is asked only once Platform_GlExtensionSupported said yes.
[[nodiscard]] Platform_GlFunction Platform_GlProcAddress(const char* name);
// Whether the window's context has an OpenGL extension.
[[nodiscard]] bool Platform_GlExtensionSupported(const char* name);

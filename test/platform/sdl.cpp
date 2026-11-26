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

// The tests of SDL 3 as the platform layer uses it, on its dummy video driver (a window in memory, no screen needed):
// the library linked is the one its headers describe, built with the video and the gamepads and without the sound; a
// window created by properties has the size asked, in pixels; the text input is off until started; the relative mouse
// mode is the window's; an event pushed with a time in nanoseconds comes back with that time; the clock does not go
// back.
#include "platform.h"

#include <check.h>

#include <SDL3/SDL.h>

#include <string_view>

namespace {

// the library's version is the headers', and the build has what the client asks of it and not what it left out
void CheckBuild()
{
    CHECK_EQUAL(SDL_GetVersion(), SDL_VERSION);
    CHECK_EQUAL(SDL_VERSION, SDL_VERSIONNUM(3, 4, 20));
    CHECK(SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy"));
    CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD));
    const char* const driver = SDL_GetCurrentVideoDriver();
    CHECK(driver != nullptr && std::string_view(driver) == "dummy");
    // OpenAL Soft plays the sound: SDL's own sound is not built
    CHECK(!SDL_InitSubSystem(SDL_INIT_AUDIO));
}

// a window created by properties, as the layer creates its own: its size in pixels, the text input started and
// stopped, the relative mouse mode turned on and off
void CheckWindow()
{
    const SDL_PropertiesID properties = SDL_CreateProperties();
    CHECK(properties != 0);
    CHECK(SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "modernjk_test_platform"));
    CHECK(SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, 1280));
    CHECK(SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, 720));
    CHECK(SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true));
    SDL_Window* const window = SDL_CreateWindowWithProperties(properties);
    SDL_DestroyProperties(properties);
    CHECK(window != nullptr);

    if (window == nullptr) {
        return;
    }

    int width = 0;
    int height = 0;
    CHECK(SDL_GetWindowSizeInPixels(window, &width, &height));
    CHECK_EQUAL(width, 1280);
    CHECK_EQUAL(height, 720);

    CHECK(!SDL_TextInputActive(window));
    CHECK(SDL_StartTextInput(window));
    CHECK(SDL_TextInputActive(window));
    CHECK(SDL_StopTextInput(window));
    CHECK(!SDL_TextInputActive(window));

    CHECK(!SDL_GetWindowRelativeMouseMode(window));
    CHECK(SDL_SetWindowRelativeMouseMode(window, true));
    CHECK(SDL_GetWindowRelativeMouseMode(window));
    CHECK(SDL_SetWindowRelativeMouseMode(window, false));

    SDL_DestroyWindow(window);
}

// a key event pushed with its own time comes back with that time, in nanoseconds, and the clock does not go back
void CheckEvents()
{
    SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
    const Uint64 before = SDL_GetTicksNS();
    SDL_Event pushed{};
    pushed.key.type = SDL_EVENT_KEY_DOWN;
    pushed.key.timestamp = 123'456'789;
    pushed.key.scancode = SDL_SCANCODE_W;
    pushed.key.key = SDLK_W;
    pushed.key.down = true;
    CHECK(SDL_PushEvent(&pushed));

    bool found = false;
    SDL_Event event{};

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_KEY_DOWN) {
            found = true;
            CHECK_EQUAL(event.key.timestamp, Uint64{123'456'789});
            CHECK_EQUAL(event.key.scancode, SDL_SCANCODE_W);
            CHECK_EQUAL(event.key.key, SDLK_W);
            CHECK(event.key.down);
        }
    }

    CHECK(found);
    CHECK(SDL_GetTicksNS() >= before);
}

} // namespace

// runs the checks of SDL 3 on its dummy video driver, then closes the library
void RunSdlTests()
{
    CheckBuild();
    CheckWindow();
    CheckEvents();
    SDL_Quit();
}

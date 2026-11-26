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

// platform.cpp -- the layer's start and stop, its clock, the clipboard and the error's message box, over SDL 3. The
// clock is SDL's (SDL_GetTicksNS), the one its events are dated on, so that an event's time and the program's time
// compare without a conversion.
#include "platform.h"

#include <SDL3/SDL.h>

#include <fmt/format.h>

#include <memory>

namespace platform {

// The time now, on SDL's clock.
Time Now() noexcept
{
    return Time{static_cast<Time::rep>(SDL_GetTicksNS())};
}

// Starts SDL's video (its events with it). On Windows the keyboard is read raw, on SDL's own thread, which dates each
// key when it arrives instead of when the program reads its messages.
std::expected<void, std::string> Start(const std::string& name, const std::string& version)
{
    SDL_SetAppMetadata(name.c_str(), version.c_str(), nullptr);
    SDL_SetHint(SDL_HINT_WINDOWS_RAW_KEYBOARD, "1");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        return std::unexpected(fmt::format("the system's video could not be started: {}", SDL_GetError()));
    }

    return {};
}

// Closes SDL.
void Stop() noexcept
{
    SDL_Quit();
}

// The library's version and revision, and the video driver once started.
std::string Description()
{
    const int version = SDL_GetVersion();
    const char* const driver = SDL_GetCurrentVideoDriver();
    return fmt::format("SDL {}.{}.{} ({}), video driver {}", SDL_VERSIONNUM_MAJOR(version), SDL_VERSIONNUM_MINOR(version), SDL_VERSIONNUM_MICRO(version),
                       SDL_GetRevision(), driver != nullptr ? driver : "none");
}

// The clipboard's text; SDL returns a copy to free, empty when there is none.
std::string ClipboardText()
{
    const std::unique_ptr<char, void (*)(void*)> text(SDL_GetClipboardText(), SDL_free);
    return text ? std::string(text.get()) : std::string();
}

// The system's message box; without a display SDL fails, and the text is already in the program's log.
void ShowError(const std::string& title, const std::string& text) noexcept
{
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), text.c_str(), nullptr);
}

} // namespace platform

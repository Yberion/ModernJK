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

// window.h -- the client's window and its OpenGL context. The program opens one window and keeps it until it quits:
// a change of the display's state (a window, borderless fullscreen, exclusive fullscreen), of the screen or of the
// size is applied to that window, which is never created again. Every size is in pixels. What can fail says so by
// its result, which the caller must read: the layer throws no exception of its own.
// Example: an engine opens its window, then follows a setting changed in the menus:
//     auto window = platform::Window::Open("ModernJK", settings, platform::GlRequest{.samples = 4});
//     if (!window) { /* window.error() says what failed */ }
//     settings.state = platform::DisplayState::Window;
//     if (!window->Apply(settings)) { /* the system refused: the window is as it was */ }
//     if (!window->Present()) { /* the image could not be shown */ }
#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

struct SDL_Window;
struct SDL_GLContextState;
struct SDL_Cursor;

namespace platform {

// How the window sits on its screen.
enum class DisplayState {
    Window,     // a window with its title bar, centred on its screen
    Borderless, // the whole screen, in the screen's present mode: alt-tab changes no mode
    Exclusive,  // the whole screen, in a mode of the screen's own list
};

// A mode of a screen: its size in pixels and its refresh rate in images per second.
struct DisplayMode {
    int width = 0;
    int height = 0;
    float refresh = 0.0F;

    [[nodiscard]] bool operator==(const DisplayMode&) const = default;
};

// What the player chose for the display.
struct WindowSettings {
    DisplayState state = DisplayState::Borderless;
    // the screen: 0 the system's primary one, then the others; one that does not exist is the primary
    int screen = 0;
    // the window's size, for DisplayState::Window
    int width = 1280;
    int height = 720;
    // the mode for DisplayState::Exclusive: of the screen's modes of that size, the one of the nearest refresh rate
    // (the highest when none is asked); all zero for the screen's present mode
    DisplayMode exclusive;

    [[nodiscard]] bool operator==(const WindowSettings&) const = default;
};

// What the renderer asks of the OpenGL context: 24 bits of colour, 24 of depth and 8 of stencil always, and this.
struct GlRequest {
    // the samples per pixel of the multisampling, 0 for none
    int samples = 0;
};

// A size in pixels.
struct PixelSize {
    int width = 0;
    int height = 0;

    [[nodiscard]] bool operator==(const PixelSize&) const = default;
};

// A pixel of an image, from its top left corner.
struct PixelPoint {
    int x = 0;
    int y = 0;
};

// An image the caller keeps: four bytes per pixel (red, green, blue, alpha), size.width times size.height of them,
// row after row from the top.
struct Image {
    std::span<const std::uint8_t> rgba;
    PixelSize size;
};

// A screen as the system reports it.
struct Screen {
    std::string name;
    // the mode of the desktop, which a window and borderless fullscreen keep
    DisplayMode desktop;
    // the part of the screen a window can take, without the taskbar or the dock: the desktop's size when the system
    // does not tell
    PixelSize usable;
    // the modes exclusive fullscreen can set, the largest first, each size and rate once
    std::vector<DisplayMode> modes;
};

// When an image drawn is shown.
enum class SwapInterval {
    Immediate,    // at once, without waiting for the screen
    VerticalSync, // at the screen's next refresh
    AdaptiveSync, // at the screen's next refresh, but at once when the image is late; VerticalSync where the system lacks it
};

// A function of the OpenGL library, to cast to its own type.
using GlFunction = void (*)();

// Whether platform::Screens lists each screen's modes. The system enumerates them slowly the first time it is asked
// (half a second for two screens of some 440 modes each, then a few microseconds), so a program that needs only the
// screens' names, desktops and usable areas leaves them out.
enum class ScreenModes {
    Skipped, // Screen::modes left empty
    Listed,  // Screen::modes filled
};

// The screens, the system's primary one first: the order of WindowSettings::screen; with their modes or without.
[[nodiscard]] std::vector<Screen> Screens(ScreenModes modes = ScreenModes::Listed);

class Window {
public:
    // Opens the window in the state asked, with an OpenGL context when gl is given: one request, one more without
    // multisampling when the first fails, then the error's text, which gives both failures. Without gl the window
    // has no context (the tests' windows; another drawing API's).
    [[nodiscard]] static std::expected<Window, std::string> Open(const std::string& title, const WindowSettings& settings, const std::optional<GlRequest>& gl);

    // Applies the display's state, the screen, the size and the mode to the window, and waits for the system to have
    // done it. False when the system refused (a screen without a mode that fits); the window stays usable.
    [[nodiscard]] bool Apply(const WindowSettings& settings);

    // The size of what is drawn, in pixels.
    [[nodiscard]] PixelSize Size() const;

    // Shows the image drawn (swaps the context's buffers). False without a context, or when the system failed.
    [[nodiscard]] bool Present();

    // False without a context, or when the system refused (a driver with no vertical sync to set).
    [[nodiscard]] bool SetSwapInterval(SwapInterval interval);

    // The samples per pixel the context has; 0 without multisampling or without a context.
    [[nodiscard]] int Samples() const;

    // A function of the OpenGL library by its name. Under X11 a pointer comes back for any name, also one the driver
    // lacks, which must not be called: a function of an extension is asked only once GlExtensionSupported said yes.
    // nullptr without a context, and where the library says it has none of that name.
    [[nodiscard]] GlFunction GlProcAddress(const std::string& name) const;

    // Whether the context has an OpenGL extension; false without a context.
    [[nodiscard]] bool GlExtensionSupported(const std::string& name) const;

    // The window's icon. False when the image's bytes are not its size's, or when the system refused.
    [[nodiscard]] bool SetIcon(const Image& image);

    // The image of the system's pointer over the window, drawn at the size shown, hotSpot the pixel of the image as
    // shown that points. The system draws and moves it, at its own pace. False as SetIcon, or when the size shown is
    // not at least a pixel each way; the pointer then stays as it was.
    [[nodiscard]] bool SetPointerImage(const Image& image, PixelSize shown, PixelPoint hotSpot);

    // Minimizes the window. False when the system refused.
    [[nodiscard]] bool Minimize();

    // SDL's window, never null, for the layer's own parts (the input) and for a drawing API that makes its surface
    // from it. The window still owns it.
    [[nodiscard]] SDL_Window* Handle() noexcept;

private:
    using WindowPtr = std::unique_ptr<SDL_Window, void (*)(SDL_Window*)>;
    using ContextPtr = std::unique_ptr<SDL_GLContextState, void (*)(SDL_GLContextState*)>;
    using CursorPtr = std::unique_ptr<SDL_Cursor, void (*)(SDL_Cursor*)>;

    // takes a window that exists and its context, null for a window without one
    Window(WindowPtr window, ContextPtr context);

    // one attempt of Open, with the samples given
    [[nodiscard]] static std::expected<Window, std::string> Create(const std::string& title, const WindowSettings& settings, bool gl, int samples);

    // destroyed in the reverse order: the pointer, the context, then the window
    WindowPtr window_;
    ContextPtr context_;
    CursorPtr cursor_;
};

} // namespace platform

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

// window.cpp -- the window and its OpenGL context over SDL 3. The window is created hidden, by properties, at the
// pixel density of its screen; its context is made, the display's state applied, and only then is it shown, so that
// a player never sees it at another place or size first. SDL's changes of a window are asynchronous: Apply waits
// for them (SDL_SyncWindow), so that the size read after it is the new one.
#include "window.h"

#include <SDL3/SDL.h>

#include <fmt/format.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <utility>

namespace platform {

namespace {

// what the context is asked for, beside its samples
constexpr int kColourChannelBits = 8;
constexpr int kDepthBits = 24;
constexpr int kStencilBits = 8;
// the bytes of a pixel of an Image
constexpr std::size_t kPixelBytes = 4;

// SDL's deleters under the types the smart pointers ask for (SDL_GL_DestroyContext returns a result).
void DestroyContext(SDL_GLContextState* context)
{
    SDL_GL_DestroyContext(context);
}

using SurfacePtr = std::unique_ptr<SDL_Surface, void (*)(SDL_Surface*)>;
using ModeListPtr = std::unique_ptr<SDL_DisplayMode*, void (*)(void*)>;

// The screens' identifiers, the system's primary one first: the order of WindowSettings::screen.
std::vector<SDL_DisplayID> DisplayIds()
{
    std::vector<SDL_DisplayID> ids;
    int count = 0;
    const std::unique_ptr<SDL_DisplayID, void (*)(void*)> list(SDL_GetDisplays(&count), SDL_free);
    const SDL_DisplayID primary = SDL_GetPrimaryDisplay();

    if (primary != 0) {
        ids.push_back(primary);
    }

    if (list) {
        for (const SDL_DisplayID id : std::span(list.get(), static_cast<std::size_t>(count))) {
            if (id != primary) {
                ids.push_back(id);
            }
        }
    }

    return ids;
}

// The screen of a setting: the one of that number, the primary one when there is none of it; nothing when the
// system reports no screen.
std::optional<SDL_DisplayID> DisplayOf(int screen)
{
    const std::vector<SDL_DisplayID> ids = DisplayIds();

    if (ids.empty()) {
        return std::nullopt;
    }

    const auto index = static_cast<std::size_t>(std::max(screen, 0));
    return index < ids.size() ? ids[index] : ids.front();
}

// A size, never negative, to the nearest whole number.
int RoundSize(float size) noexcept
{
    const int whole = static_cast<int>(size);
    return size - static_cast<float>(whole) >= 0.5F ? whole + 1 : whole;
}

// A mode of SDL in pixels: SDL's sizes are in the screen's own units, pixels once multiplied by its density.
DisplayMode ModeInPixels(const SDL_DisplayMode& mode)
{
    const float density = mode.pixel_density > 0.0F ? mode.pixel_density : 1.0F;
    return {.width = RoundSize(static_cast<float>(mode.w) * density), .height = RoundSize(static_cast<float>(mode.h) * density), .refresh = mode.refresh_rate};
}

// Whether a mode's refresh rate serves the one wanted better than another's: nearer to it, or higher when none is
// wanted.
bool BetterRefresh(float candidate, float best, float wanted) noexcept
{
    if (wanted <= 0.0F) {
        return candidate > best;
    }

    const float candidateGap = candidate > wanted ? candidate - wanted : wanted - candidate;
    const float bestGap = best > wanted ? best - wanted : wanted - best;
    return candidateGap < bestGap;
}

// The mode of a screen for the one asked, in pixels: of the modes of that size, the one whose refresh rate is the
// nearest (the highest when none is asked); when the screen has no mode of that size, the closest SDL finds.
// Nothing when the screen has no mode that fits.
std::optional<SDL_DisplayMode> FindMode(SDL_DisplayID display, const DisplayMode& wanted)
{
    int count = 0;
    const ModeListPtr modes(SDL_GetFullscreenDisplayModes(display, &count), SDL_free);
    const SDL_DisplayMode* best = nullptr;

    if (modes) {
        for (const SDL_DisplayMode* const mode : std::span(modes.get(), static_cast<std::size_t>(count))) {
            const DisplayMode pixels = ModeInPixels(*mode);
            const bool sameSize = pixels.width == wanted.width && pixels.height == wanted.height;

            if (sameSize && (best == nullptr || BetterRefresh(mode->refresh_rate, best->refresh_rate, wanted.refresh))) {
                best = mode;
            }
        }
    }

    if (best != nullptr) {
        return *best;
    }

    SDL_DisplayMode closest{};

    if (SDL_GetClosestFullscreenDisplayMode(display, wanted.width, wanted.height, wanted.refresh, true, &closest)) {
        return closest;
    }

    return std::nullopt;
}

// A surface of SDL holding a copy of an image; null when the image's bytes are not its size's, or when SDL has no
// memory for it.
SurfacePtr SurfaceFrom(const Image& image)
{
    SurfacePtr surface(nullptr, SDL_DestroySurface);
    const PixelSize size = image.size;

    if (size.width <= 0 || size.height <= 0 ||
        image.rgba.size() != static_cast<std::size_t>(size.width) * static_cast<std::size_t>(size.height) * kPixelBytes) {
        return surface;
    }

    surface.reset(SDL_CreateSurface(size.width, size.height, SDL_PIXELFORMAT_RGBA32));

    if (!surface || !SDL_LockSurface(surface.get())) {
        surface.reset();
        return surface;
    }

    // row by row: a surface's rows can be longer than their pixels
    const std::size_t row = static_cast<std::size_t>(size.width) * kPixelBytes;
    const auto pitch = static_cast<std::size_t>(surface->pitch);
    const std::span<std::uint8_t> pixels(static_cast<std::uint8_t*>(surface->pixels), pitch * static_cast<std::size_t>(size.height));

    for (int y = 0; y < size.height; ++y) {
        const auto line = static_cast<std::size_t>(y);
        std::ranges::copy(image.rgba.subspan(line * row, row), pixels.subspan(line * pitch, row).begin());
    }

    SDL_UnlockSurface(surface.get());
    return surface;
}

// Asks SDL for the context every window of the layer gets, with the samples given.
void RequestContext(int samples)
{
    SDL_GL_ResetAttributes();
    // an attribute SDL knows is never refused here: the request as a whole is, when the context is made
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, kColourChannelBits);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, kColourChannelBits);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, kColourChannelBits);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, kDepthBits);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, kStencilBits);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, samples > 0 ? 1 : 0);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, samples);
}

// A hidden window of SDL centred on a screen, at the pixel density of that screen, for OpenGL or not; null, with
// SDL's error set, when it cannot be created.
SDL_Window* CreateHidden(const std::string& title, SDL_DisplayID display, PixelSize size, bool gl)
{
    const auto centred = static_cast<Sint64>(SDL_WINDOWPOS_CENTERED_DISPLAY(display));
    const SDL_PropertiesID properties = SDL_CreateProperties();
    // a property that could not be set leaves SDL's default, and no properties at all fail the creation below
    SDL_SetStringProperty(properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_X_NUMBER, centred);
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_Y_NUMBER, centred);
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, std::max(size.width, 1));
    SDL_SetNumberProperty(properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, std::max(size.height, 1));
    SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
    SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
    SDL_SetBooleanProperty(properties, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, gl);
    SDL_Window* const window = SDL_CreateWindowWithProperties(properties);
    SDL_DestroyProperties(properties);
    return window;
}

// A window: out of fullscreen, at its size in pixels (SDL's sizes are in the screen's units), centred on its screen.
bool ApplyWindowed(SDL_Window* window, SDL_DisplayID display, PixelSize size)
{
    const float density = std::max(SDL_GetWindowPixelDensity(window), 1.0F);
    const int width = std::max(RoundSize(static_cast<float>(size.width) / density), 1);
    const int height = std::max(RoundSize(static_cast<float>(size.height) / density), 1);
    const auto centred = static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(display));
    return SDL_SetWindowFullscreen(window, false) && SDL_SetWindowSize(window, width, height) && SDL_SetWindowPosition(window, centred, centred);
}

// Borderless fullscreen: no fullscreen mode, the window moved onto its screen, then fullscreen.
bool ApplyBorderless(SDL_Window* window, SDL_DisplayID display)
{
    const auto centred = static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(display));
    return SDL_SetWindowFullscreenMode(window, nullptr) && SDL_SetWindowPosition(window, centred, centred) && SDL_SetWindowFullscreen(window, true);
}

// Exclusive fullscreen: the screen's mode for the one asked, or its desktop's when none is; the mode carries its
// screen.
bool ApplyExclusive(SDL_Window* window, SDL_DisplayID display, const DisplayMode& wanted)
{
    std::optional<SDL_DisplayMode> mode;

    if (wanted == DisplayMode{}) {
        if (const SDL_DisplayMode* const desktop = SDL_GetDesktopDisplayMode(display); desktop != nullptr) {
            mode = *desktop;
        }
    }
    else {
        mode = FindMode(display, wanted);
    }

    return mode && SDL_SetWindowFullscreenMode(window, &*mode) && SDL_SetWindowFullscreen(window, true);
}

} // namespace

// Each screen with its desktop's mode and the modes exclusive fullscreen can set; SDL lists a mode once per pixel
// format, kept once here.
std::vector<Screen> Screens()
{
    std::vector<Screen> screens;

    for (const SDL_DisplayID id : DisplayIds()) {
        Screen screen;
        const char* const name = SDL_GetDisplayName(id);
        screen.name = name != nullptr ? name : "";

        float density = 1.0F;

        if (const SDL_DisplayMode* const desktop = SDL_GetDesktopDisplayMode(id); desktop != nullptr) {
            screen.desktop = ModeInPixels(*desktop);
            density = desktop->pixel_density > 0.0F ? desktop->pixel_density : 1.0F;
        }

        // in the screen's units, as the desktop's mode, so in pixels once multiplied by its density
        SDL_Rect usable{};
        const bool told = SDL_GetDisplayUsableBounds(id, &usable) && usable.w > 0 && usable.h > 0;
        screen.usable = told
                            ? PixelSize{.width = RoundSize(static_cast<float>(usable.w) * density), .height = RoundSize(static_cast<float>(usable.h) * density)}
                            : PixelSize{.width = screen.desktop.width, .height = screen.desktop.height};

        int count = 0;
        const ModeListPtr modes(SDL_GetFullscreenDisplayModes(id, &count), SDL_free);

        if (modes) {
            for (const SDL_DisplayMode* const mode : std::span(modes.get(), static_cast<std::size_t>(count))) {
                const DisplayMode pixels = ModeInPixels(*mode);

                if (std::ranges::find(screen.modes, pixels) == screen.modes.end()) {
                    screen.modes.push_back(pixels);
                }
            }
        }

        screens.push_back(std::move(screen));
    }

    return screens;
}

// Takes the window and its context; the pointer's image comes later.
Window::Window(WindowPtr window, ContextPtr context) : window_(std::move(window)), context_(std::move(context)), cursor_(nullptr, SDL_DestroyCursor)
{
    assert(window_);
}

// One request, and one more without multisampling: a driver that lacks the samples asked refuses the whole context.
std::expected<Window, std::string> Window::Open(const std::string& title, const WindowSettings& settings, const std::optional<GlRequest>& gl)
{
    const int samples = gl ? std::max(gl->samples, 0) : 0;
    std::expected<Window, std::string> window = Create(title, settings, gl.has_value(), samples);

    if (!window && samples > 0) {
        std::expected<Window, std::string> plain = Create(title, settings, true, 0);

        if (!plain) {
            return std::unexpected(fmt::format("{}; without multisampling: {}", window.error(), plain.error()));
        }

        return plain;
    }

    return window;
}

// Creates the window hidden on its screen, makes its context, applies the display's state, then shows it.
std::expected<Window, std::string> Window::Create(const std::string& title, const WindowSettings& settings, bool gl, int samples)
{
    const std::optional<SDL_DisplayID> display = DisplayOf(settings.screen);

    if (!display) {
        return std::unexpected(fmt::format("the system reports no screen: {}", SDL_GetError()));
    }

    if (gl) {
        RequestContext(samples);
    }

    WindowPtr window(CreateHidden(title, *display, {.width = settings.width, .height = settings.height}, gl), SDL_DestroyWindow);

    if (!window) {
        return std::unexpected(fmt::format("the window could not be created: {}", SDL_GetError()));
    }

    ContextPtr context(nullptr, DestroyContext);

    if (gl) {
        context.reset(SDL_GL_CreateContext(window.get()));

        if (!context) {
            return std::unexpected(fmt::format("the OpenGL context ({}-bit colour, {}-bit depth, {}-bit stencil, {} samples) could not be created: {}",
                                               3 * kColourChannelBits, kDepthBits, kStencilBits, samples, SDL_GetError()));
        }
    }

    Window result(std::move(window), std::move(context));

    if (!result.Apply(settings)) {
        return std::unexpected(fmt::format("the display's state could not be set: {}", SDL_GetError()));
    }

    if (!SDL_ShowWindow(result.Handle())) {
        return std::unexpected(fmt::format("the window could not be shown: {}", SDL_GetError()));
    }

    // false only when the system took too long to answer: the window is shown all the same
    SDL_SyncWindow(result.Handle());
    return result;
}

// Sets the state on SDL's window, then waits for the system to have done it.
bool Window::Apply(const WindowSettings& settings)
{
    const std::optional<SDL_DisplayID> display = DisplayOf(settings.screen);

    if (!display) {
        return false;
    }

    bool applied = false;

    switch (settings.state) {
    case DisplayState::Window:
        applied = ApplyWindowed(window_.get(), *display, {.width = settings.width, .height = settings.height});
        break;
    case DisplayState::Borderless:
        applied = ApplyBorderless(window_.get(), *display);
        break;
    case DisplayState::Exclusive:
        applied = ApplyExclusive(window_.get(), *display, settings.exclusive);
        break;
    }

    // a hidden window's changes are applied when it is shown: nothing to wait for before; and a wait that ends
    // because the system took too long is not a refusal
    if ((SDL_GetWindowFlags(window_.get()) & SDL_WINDOW_HIDDEN) == 0) {
        SDL_SyncWindow(window_.get());
    }

    return applied;
}

// The size in pixels, which differs from the window's own size on a screen of high density; zero when SDL cannot
// tell.
PixelSize Window::Size() const
{
    PixelSize size;

    if (!SDL_GetWindowSizeInPixels(window_.get(), &size.width, &size.height)) {
        return {};
    }

    return size;
}

// Swaps the buffers.
bool Window::Present()
{
    return context_ && SDL_GL_SwapWindow(window_.get());
}

// SDL counts the screen's refreshes to wait for: 0, 1, and -1 for the adaptive sync, which is not everywhere: the
// plain vertical sync in its place, as SDL's documentation says.
bool Window::SetSwapInterval(SwapInterval interval)
{
    if (!context_) {
        return false;
    }

    switch (interval) {
    case SwapInterval::Immediate:
        return SDL_GL_SetSwapInterval(0);
    case SwapInterval::VerticalSync:
        return SDL_GL_SetSwapInterval(1);
    case SwapInterval::AdaptiveSync:
        return SDL_GL_SetSwapInterval(-1) || SDL_GL_SetSwapInterval(1);
    }

    return false;
}

// What the driver gave, which can be less than what was asked.
int Window::Samples() const
{
    int samples = 0;

    if (!context_ || !SDL_GL_GetAttribute(SDL_GL_MULTISAMPLESAMPLES, &samples)) {
        return 0;
    }

    return samples;
}

// The library's function, for the renderer's own table.
GlFunction Window::GlProcAddress(const std::string& name) const
{
    return context_ ? SDL_GL_GetProcAddress(name.c_str()) : nullptr;
}

// SDL reads the context's extension list.
bool Window::GlExtensionSupported(const std::string& name) const
{
    return context_ && SDL_GL_ExtensionSupported(name.c_str());
}

// SDL copies the icon: the surface lives only for the call.
bool Window::SetIcon(const Image& image)
{
    const SurfacePtr surface = SurfaceFrom(image);
    return surface && SDL_SetWindowIcon(window_.get(), surface.get());
}

// SDL scales the image to the size shown, then copies it into a pointer of the system's, which stays the window's
// until another replaces it.
bool Window::SetPointerImage(const Image& image, PixelSize shown, PixelPoint hotSpot)
{
    const SurfacePtr surface = SurfaceFrom(image);

    if (!surface || shown.width <= 0 || shown.height <= 0) {
        return false;
    }

    const SurfacePtr scaled(SDL_ScaleSurface(surface.get(), shown.width, shown.height, SDL_SCALEMODE_LINEAR), SDL_DestroySurface);

    if (!scaled) {
        return false;
    }

    CursorPtr cursor(SDL_CreateColorCursor(scaled.get(), hotSpot.x, hotSpot.y), SDL_DestroyCursor);

    if (!cursor || !SDL_SetCursor(cursor.get())) {
        return false;
    }

    cursor_ = std::move(cursor);
    return true;
}

// Asks the system to minimize the window.
bool Window::Minimize()
{
    return SDL_MinimizeWindow(window_.get());
}

// SDL's window.
SDL_Window* Window::Handle() noexcept
{
    return window_.get();
}

} // namespace platform

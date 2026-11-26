# The window

The client's window and its OpenGL context (`window.h`, `window.cpp`). The program opens one window and keeps it until it quits: a change of the display is applied to that window, which is never created again.

## In the game

The "display" rows of the setup menus: a player picks borderless fullscreen (the default), a window or exclusive fullscreen, the screen the game is on, and, for exclusive fullscreen, one of that screen's own modes; "Apply" changes the window where it stands. Alt-tab out of borderless fullscreen changes no screen mode, so the desktop is there at once.

## The three states

| state | what the window is | what sets its size |
|---|---|---|
| `DisplayState::Window` | a window with its title bar, centred on its screen | `WindowSettings::width` and `height`, in pixels |
| `DisplayState::Borderless` | the whole screen, in the screen's present mode | the screen's desktop mode |
| `DisplayState::Exclusive` | the whole screen, in a mode of the screen's own list | `WindowSettings::exclusive`: of the screen's modes of that size, the one of the nearest refresh rate; all zero for the screen's present mode |

`WindowSettings::screen` is 0 for the system's primary screen, then the others, in the order of `platform::Screens()`; a screen that does not exist is the primary one. Each `Screen` gives its name, its desktop's mode, the part a window can take without the taskbar or the dock (`usable`, the desktop's size when the system does not tell) and the modes exclusive fullscreen can set.

## Rules

- Every size is in pixels. SDL counts in the screen's own units on a screen of high density (macOS, Wayland with a scale): the layer converts with the window's or the mode's pixel density, both ways.
- `Open` creates the window hidden, makes the context, applies the state and only then shows it: a player never sees it somewhere else first.
- The context is asked once, with 24 bits of colour, 24 of depth, 8 of stencil and the samples wanted; when that fails with samples, once more without them; then the error's text gives both failures. `Samples()` says what the driver gave.
- SDL applies a window's changes asynchronously: `Apply` waits for them (`SDL_SyncWindow`), so that `Size()` after it is the new size.
- What can fail says so by its result, which the caller must read (`[[nodiscard]]`): `Open` gives the window or the error's text, the others `bool`. `Apply` returns false when the system refuses (a screen without a mode that fits); the window stays as it was and stays usable.
- A window opened without a `GlRequest` has no context: `Present`, `SetSwapInterval` and `GlExtensionSupported` return false, `GlProcAddress` nullptr. The tests' windows are such; so will a window drawn by another API be, which takes `Handle()`.
- `GlProcAddress` gives a pointer for any name under X11, also for a function the driver lacks (SDL's documentation of `SDL_GL_GetProcAddress`): a function of an extension is asked only once `GlExtensionSupported` said yes.
- `SetSwapInterval` takes `SwapInterval::Immediate`, `VerticalSync` or `AdaptiveSync` (the vertical sync where the system lacks the adaptive one) and returns false where the driver has no vertical sync to set (seen on a virtual machine's): the images are then shown at once.
- An image is given as an `Image` (its bytes and its size, four bytes per pixel) that the caller keeps; one whose bytes are not its size's is refused.
- The pointer's image (`SetPointerImage`), scaled to the size the caller wants it shown at, becomes the system's pointer over the window: the system draws and moves it, at its own pace, also while the game loads.

## Example

```cpp
platform::WindowSettings settings{.state = platform::DisplayState::Borderless, .screen = 1};
auto window = platform::Window::Open("ModernJK", settings, platform::GlRequest{.samples = 4});

if (!window) {
    return std::unexpected(window.error()); // "the OpenGL context (...) could not be created: ..."
}

if (!window->SetSwapInterval(platform::SwapInterval::VerticalSync)) {
    // a driver without it: the images are shown at once
}

const platform::PixelSize size = window->Size(); // the second screen's size

settings.state = platform::DisplayState::Window; // from the menus: the same window, now 1280 by 720

if (!window->Apply(settings)) {
    // the system refused: the window is as it was
}
```

## Tests

`test/platform/window.cpp`, on SDL's dummy video driver: the screens listed; a window at the size asked; the same window through the three states and back; a screen that does not exist; exclusive fullscreen refused by a screen without modes; what needs a context refused without one; an icon and a pointer's image; a context, which that driver lacks, refused with both errors in the text. `test/platform/real.cpp`, by hand (`modernjk_test_platform real`): the same states on the system's own driver, with a context.

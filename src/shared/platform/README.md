# The platform layer

The client's door to the system, for every engine of the project that opens a window (the multiplayer, the single player of Jedi Academy and Jedi Outcast), on SDL 3: one static library, `modernjk_platform`, independent of the engines. An engine starts it, opens one `Window` and keeps it until it quits, and reads one `Input` at its frames and ticks. No SDL type appears in its headers but three pointers declared by name; the dedicated server does not link it.

## In the game

What a player touches before anything is drawn: the window that fills the screen without changing its mode, so that alt-tab is instant; the switch to a window or to another screen from the menus, without the picture going away; the key pressed between two frames, dated when it was pressed and not when the frame read it; the mouse turning the view by the device's own counts, without the system's acceleration; the pointer of the menus, the system's own with the game's image.

## Folders

| folder | what it does | README |
|---|---|---|
| `window/` | the window and its OpenGL context: the three display states (a window, borderless fullscreen, exclusive fullscreen), the screens and their modes, the swap, the icon and the pointer's image | [window/README.md](window/README.md) |
| `input/` | the keyboard, the mouse and the window's own events as a list of typed events, each with its time in nanoseconds | [input/README.md](input/README.md) |

The files at the root: `platform.h` and `platform.cpp` (the clock, the start and the stop, the layer's description, the clipboard, the error's message box).

## The clock

`platform::Now()` is the client's one clock, SDL's (`SDL_GetTicksNS`): nanoseconds that never go back. SDL dates its events on it, so an event's time and the frame's time compare as they are. On Windows the layer asks SDL to read the keyboard raw (`SDL_HINT_WINDOWS_RAW_KEYBOARD`): SDL's own thread dates each key when it arrives, where the window's messages only give the time they are read at.

## Example

```cpp
if (auto started = platform::Start("ModernJK", version); !started) {
    platform::ShowError("ModernJK", started.error());
    return 1;
}

auto window = platform::Window::Open("ModernJK", settings, platform::GlRequest{.samples = 4});
platform::Input input(*window);
const bool view = input.SetMouseMode(platform::MouseMode::Game);

for (;;) {
    for (const platform::Event& event : input.Poll()) {
        // each event with its own time: std::get_if<platform::KeyEvent>(&event) ...
    }

    // ... the frame
    const bool shown = window->Present();
}
```

## Tests

`modernjk.platform` in `ctest` (`test/platform`): SDL 3 as it is built for the client (`sdl.cpp`), the window (`window.cpp`) and the input (`input.cpp`), on SDL's dummy video driver, which keeps a screen and windows in memory: no screen is needed, and the events are pushed into SDL's queue at chosen times. The dummy driver has no OpenGL: `modernjk_test_platform real`, run by hand, opens a window with a context on the system's own driver and screen, takes it through the three display states and prints what it got (`real.cpp`).

## Rules

The code follows the C++ Core Guidelines entirely, clang-tidy at zero under `.clang-tidy`, the strict warnings of netcodec, a comment on every function, a description at the top of every file, a blank line around each block (`GUIDELINES.md` sections 3, 5 and 10). It throws no exception of its own: what can fail returns `std::expected` or `bool`, marked `[[nodiscard]]`, as E.27 of the C++ Core Guidelines asks of code that cannot throw. It calls no math function of a library. The guidelines' text was read for it, section by section, and clang-tidy is one check among them: `engine/`, which each engine compiles, follows them too, its state one object reached through one function (I.2 and I.3 of the C++ Core Guidelines).

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

// input.h -- the keyboard, the mouse and the window's own events, as a list of typed events, each with the time it
// happened at on the layer's clock (platform::Now). Poll hands over everything since the call before, in order.
// A key is the key of the player's layout: the key marked Z is Z on every layout. The console key alone is found by
// its place, the key under Escape. Text comes apart from the keys, and only while it is asked for.
// Example: an engine reads its input at a frame's start:
//     platform::Input input(window);
//     input.SetMouseMode(platform::MouseMode::Game);
//     for (const platform::Event& event : input.Poll()) {
//         if (const auto* key = std::get_if<platform::KeyEvent>(&event)) { /* key->time, key->symbol, key->down */ }
//     }
#pragma once

#include <shared/platform/platform.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

struct SDL_Window;

namespace platform {

class Window;

// The keys that type no character and that the layer names. A key that types a character is known by that
// character (KeyEvent::symbol); any other key by its identifier and the name the system gives it (KeyName).
enum class NamedKey {
    None,
    Escape,
    Enter,
    Tab,
    Backspace,
    Delete,
    Insert,
    Home,
    End,
    PageUp,
    PageDown,
    Up,
    Down,
    Left,
    Right,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    LeftShift,
    RightShift,
    LeftCtrl,
    RightCtrl,
    LeftAlt,
    RightAlt,
    CapsLock,
    NumLock,
    ScrollLock,
    Pause,
    PrintScreen,
    Keypad0,
    Keypad1,
    Keypad2,
    Keypad3,
    Keypad4,
    Keypad5,
    Keypad6,
    Keypad7,
    Keypad8,
    Keypad9,
    KeypadPeriod,
    KeypadEnter,
    KeypadPlus,
    KeypadMinus,
    KeypadMultiply,
    KeypadDivide,
};

// A key pressed or released.
struct KeyEvent {
    Time time{};
    // the key when the layer names it, None otherwise
    NamedKey named = NamedKey::None;
    // the character the player's layout gives the key, in lower case; 0 for a key that types none
    char32_t symbol = 0;
    // the key of the layout, whatever it is: the same number for the same key, and what KeyName takes
    std::uint32_t id = 0;
    // the key under Escape, by its place on the keyboard, whatever its layout marks on it
    bool console = false;
    // whether the keypad types digits (num lock on)
    bool numLock = false;
    bool down = false;
    // a press the system repeats while the key is held
    bool repeat = false;
};

// A character typed, while the text input is on: what the layout, its dead keys and the system's input method made
// of the keys.
struct TextEvent {
    Time time{};
    // a Unicode code point
    char32_t character = 0;
};

// The mouse moved, in MouseMode::Game: the device's own counts, without the system's acceleration, to the right and
// down.
struct MouseMotion {
    Time time{};
    float dx = 0.0F;
    float dy = 0.0F;
};

// The system's pointer moved, in MouseMode::Pointer: its place in the window, in pixels from the top left corner.
// One comes too when the mouse is given to the pointer, with the place the pointer is at, before any motion.
struct PointerMotion {
    Time time{};
    float x = 0.0F;
    float y = 0.0F;
};

// The mouse's buttons by their number; a mouse can have more, numbered on from kForwardButton.
inline constexpr int kLeftButton = 1;
inline constexpr int kMiddleButton = 2;
inline constexpr int kRightButton = 3;
inline constexpr int kBackButton = 4;
inline constexpr int kForwardButton = 5;

// A mouse button pressed or released.
struct MouseButton {
    Time time{};
    // kLeftButton to kForwardButton, then the mouse's other buttons
    int button = 0;
    bool down = false;
};

// The wheel turned by whole notches.
struct MouseWheel {
    Time time{};
    // positive away from the player
    int notches = 0;
};

// What happened to the window.
enum class WindowChange {
    FocusGained,
    FocusLost,
    Minimized,
    Restored,
    // the size of what is drawn changed: WindowEvent::width and height, in pixels
    PixelSizeChanged,
    // a screen was plugged or unplugged, or a screen's desktop mode or usable area changed: what platform::Screens
    // lists changed
    ScreensChanged,
    // the player or the system asks the program to quit
    Quit,
};

struct WindowEvent {
    Time time{};
    WindowChange change{};
    int width = 0;
    int height = 0;
};

using Event = std::variant<KeyEvent, TextEvent, MouseMotion, PointerMotion, MouseButton, MouseWheel, WindowEvent>;

// What the mouse drives.
enum class MouseMode {
    // the system's pointer, visible, free to leave the window: the menus
    Pointer,
    // the view: the pointer hidden and kept in the window, the motion read raw
    Game,
};

// The name the system gives a key of the player's layout, by KeyEvent::id ("F13", "Left Windows"); empty when it has
// none.
[[nodiscard]] std::string KeyName(std::uint32_t id);

// The key the system gives that name to, whatever its letters' case: KeyName's way back. Nothing for a text that
// names no key.
[[nodiscard]] std::optional<std::uint32_t> KeyFromName(const std::string& name);

class Input {
public:
    // The input of a window, which must outlive this object; the mouse starts as the pointer, the text input off.
    explicit Input(Window& window);

    // Everything that happened since the call before, in order; valid until the next call.
    [[nodiscard]] std::span<const Event> Poll();

    // Gives the mouse to the view or to the pointer; asked again for the mode it has, it does nothing. False when
    // the system refused: the mode stays as it was.
    [[nodiscard]] bool SetMouseMode(MouseMode mode);

    // Keeps the system's pointer inside the window, or lets it leave; asked again for the state it has, it does
    // nothing. It matters in MouseMode::Pointer only: in MouseMode::Game the mouse is always kept. False when the
    // system refused.
    [[nodiscard]] bool SetPointerConfined(bool confined);

    // Turns the text input on while something takes characters (the console, the chat, a menu's field) and off in
    // game, where the system's text services have nothing to do; asked again for the state it has, it does nothing.
    // False when the system refused.
    [[nodiscard]] bool SetTextInput(bool on);

private:
    // the window's, which owns it
    SDL_Window* window_;
    MouseMode mode_ = MouseMode::Pointer;
    bool textInput_ = false;
    bool confined_ = false;
    // the pointer's place is still to hand over: the mouse was just given to the pointer
    bool pointerPending_ = true;
    std::vector<Event> events_;
};

} // namespace platform

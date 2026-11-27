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

// input.cpp -- SDL 3's events turned into the layer's own. Each keeps the time SDL dated it at, in nanoseconds on
// the clock of platform::Now. A key is SDL's key code, the symbol of the player's layout: a Unicode character for a
// key that types one, a number of SDL's own for the others, of which the layer names those every keyboard has.
#include "input.h"

#include <shared/platform/window/window.h>

#include <SDL3/SDL.h>

#include <cassert>
#include <optional>

namespace platform {

namespace {

// An event's time on the layer's clock.
Time TimeOf(Uint64 timestamp) noexcept
{
    return Time{static_cast<Time::rep>(timestamp)};
}

// The layer's name of a key of SDL, None for a key that types a character or that the layer does not name.
NamedKey NamedOf(SDL_Keycode key) noexcept
{
    switch (key) {
    case SDLK_ESCAPE:
        return NamedKey::Escape;
    case SDLK_RETURN:
        return NamedKey::Enter;
    case SDLK_TAB:
        return NamedKey::Tab;
    case SDLK_BACKSPACE:
        return NamedKey::Backspace;
    case SDLK_DELETE:
        return NamedKey::Delete;
    case SDLK_INSERT:
        return NamedKey::Insert;
    case SDLK_HOME:
        return NamedKey::Home;
    case SDLK_END:
        return NamedKey::End;
    case SDLK_PAGEUP:
        return NamedKey::PageUp;
    case SDLK_PAGEDOWN:
        return NamedKey::PageDown;
    case SDLK_UP:
        return NamedKey::Up;
    case SDLK_DOWN:
        return NamedKey::Down;
    case SDLK_LEFT:
        return NamedKey::Left;
    case SDLK_RIGHT:
        return NamedKey::Right;
    case SDLK_F1:
        return NamedKey::F1;
    case SDLK_F2:
        return NamedKey::F2;
    case SDLK_F3:
        return NamedKey::F3;
    case SDLK_F4:
        return NamedKey::F4;
    case SDLK_F5:
        return NamedKey::F5;
    case SDLK_F6:
        return NamedKey::F6;
    case SDLK_F7:
        return NamedKey::F7;
    case SDLK_F8:
        return NamedKey::F8;
    case SDLK_F9:
        return NamedKey::F9;
    case SDLK_F10:
        return NamedKey::F10;
    case SDLK_F11:
        return NamedKey::F11;
    case SDLK_F12:
        return NamedKey::F12;
    case SDLK_LSHIFT:
        return NamedKey::LeftShift;
    case SDLK_RSHIFT:
        return NamedKey::RightShift;
    case SDLK_LCTRL:
        return NamedKey::LeftCtrl;
    case SDLK_RCTRL:
        return NamedKey::RightCtrl;
    case SDLK_LALT:
        return NamedKey::LeftAlt;
    case SDLK_RALT:
        return NamedKey::RightAlt;
    case SDLK_CAPSLOCK:
        return NamedKey::CapsLock;
    case SDLK_NUMLOCKCLEAR:
        return NamedKey::NumLock;
    case SDLK_SCROLLLOCK:
        return NamedKey::ScrollLock;
    case SDLK_PAUSE:
        return NamedKey::Pause;
    case SDLK_PRINTSCREEN:
        return NamedKey::PrintScreen;
    case SDLK_KP_0:
        return NamedKey::Keypad0;
    case SDLK_KP_1:
        return NamedKey::Keypad1;
    case SDLK_KP_2:
        return NamedKey::Keypad2;
    case SDLK_KP_3:
        return NamedKey::Keypad3;
    case SDLK_KP_4:
        return NamedKey::Keypad4;
    case SDLK_KP_5:
        return NamedKey::Keypad5;
    case SDLK_KP_6:
        return NamedKey::Keypad6;
    case SDLK_KP_7:
        return NamedKey::Keypad7;
    case SDLK_KP_8:
        return NamedKey::Keypad8;
    case SDLK_KP_9:
        return NamedKey::Keypad9;
    case SDLK_KP_PERIOD:
        return NamedKey::KeypadPeriod;
    case SDLK_KP_ENTER:
        return NamedKey::KeypadEnter;
    case SDLK_KP_PLUS:
        return NamedKey::KeypadPlus;
    case SDLK_KP_MINUS:
        return NamedKey::KeypadMinus;
    case SDLK_KP_MULTIPLY:
        return NamedKey::KeypadMultiply;
    case SDLK_KP_DIVIDE:
        return NamedKey::KeypadDivide;
    default:
        return NamedKey::None;
    }
}

// The character of a key of SDL: its key code when that is a Unicode character (SDL marks the other keys with two
// high bits), from the space on, the delete key excepted; 0 otherwise.
char32_t SymbolOf(SDL_Keycode key) noexcept
{
    const bool character = (key & (SDLK_SCANCODE_MASK | SDLK_EXTENDED_MASK)) == 0 && key >= SDLK_SPACE && key != SDLK_DELETE;
    return character ? static_cast<char32_t>(key) : U'\0';
}

// The key of SDL a key event stands for. X11's and Wayland's layouts give some keys that type no character a symbol the
// layer does not name (Alt Gr, the level 3 shift, on the right Alt of most European layouts; the level 5 shift, Compose,
// Meta, Hyper): such a key takes the code of its place, as SDL gives it on Windows and macOS, where a layout does not
// rename those keys. A key made into one the layer names (Caps Lock made a Ctrl by the player's layout) keeps it.
SDL_Keycode KeyCodeOf(const SDL_KeyboardEvent& event) noexcept
{
    const bool unnamed = NamedOf(event.key) == NamedKey::None && SymbolOf(event.key) == U'\0';
    // SDL_SCANCODE_TO_KEYCODE, the scancode converted explicitly (the macro's implicit one fails GCC's -Wsign-conversion)
    return unnamed && event.scancode != SDL_SCANCODE_UNKNOWN ? static_cast<SDL_Keycode>(event.scancode) | SDLK_SCANCODE_MASK : event.key;
}

// A key pressed or released, as the layer hands it over.
KeyEvent KeyOf(const SDL_KeyboardEvent& event) noexcept
{
    const SDL_Keycode key = KeyCodeOf(event);
    return {.time = TimeOf(event.timestamp),
            .named = NamedOf(key),
            .symbol = SymbolOf(key),
            .id = key,
            .console = event.scancode == SDL_SCANCODE_GRAVE,
            .numLock = (event.mod & SDL_KMOD_NUM) != 0,
            .down = event.down,
            .repeat = event.repeat};
}

// A text SDL hands over as one event per character: it holds several when an input method commits a word.
void AppendText(std::vector<Event>& events, const SDL_TextInputEvent& event)
{
    const char* text = event.text;

    while (text != nullptr && *text != '\0') {
        events.emplace_back(TextEvent{.time = TimeOf(event.timestamp), .character = static_cast<char32_t>(SDL_StepUTF8(&text, nullptr))});
    }
}

// The mouse's motion: a motion for the view, a place for the pointer. SDL's place is in the window's own units,
// pixels once multiplied by the window's density.
Event MotionOf(const SDL_MouseMotionEvent& event, MouseMode mode, SDL_Window* window) noexcept
{
    if (mode == MouseMode::Game) {
        return MouseMotion{.time = TimeOf(event.timestamp), .dx = event.xrel, .dy = event.yrel};
    }

    const float density = SDL_GetWindowPixelDensity(window);
    return PointerMotion{.time = TimeOf(event.timestamp), .x = event.x * density, .y = event.y * density};
}

// The wheel's whole notches, by the wheel's own direction: the system's "natural scrolling" turned back. Nothing
// while the wheel has turned by less than a notch.
std::optional<MouseWheel> WheelOf(const SDL_MouseWheelEvent& event) noexcept
{
    const int notches = event.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.integer_y : event.integer_y;

    if (notches == 0) {
        return std::nullopt;
    }

    return MouseWheel{.time = TimeOf(event.timestamp), .notches = notches};
}

// What an event of SDL's window says happened to it; nothing for the events the layer does not hand over.
std::optional<WindowEvent> WindowEventOf(const SDL_WindowEvent& event) noexcept
{
    const Time time = TimeOf(event.timestamp);

    switch (event.type) {
    case SDL_EVENT_WINDOW_FOCUS_GAINED:
        return WindowEvent{.time = time, .change = WindowChange::FocusGained};
    case SDL_EVENT_WINDOW_FOCUS_LOST:
        return WindowEvent{.time = time, .change = WindowChange::FocusLost};
    case SDL_EVENT_WINDOW_MINIMIZED:
        return WindowEvent{.time = time, .change = WindowChange::Minimized};
    case SDL_EVENT_WINDOW_RESTORED:
        return WindowEvent{.time = time, .change = WindowChange::Restored};
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        return WindowEvent{.time = time, .change = WindowChange::PixelSizeChanged, .width = event.data1, .height = event.data2};
    default:
        return std::nullopt;
    }
}

} // namespace

// SDL's own name of the key.
std::string KeyName(std::uint32_t id)
{
    return SDL_GetKeyName(id);
}

// SDL's key of a name; SDLK_UNKNOWN is its answer for a text that names none.
std::optional<std::uint32_t> KeyFromName(const std::string& name)
{
    const SDL_Keycode key = SDL_GetKeyFromName(name.c_str());
    return key != SDLK_UNKNOWN ? std::optional<std::uint32_t>(key) : std::nullopt;
}

// Keeps SDL's window, which the relative mouse mode and the text input belong to.
Input::Input(Window& window) : window_(window.Handle())
{
    assert(window_ != nullptr);
}

// Reads SDL's queue to its end and turns each event the layer knows into its own; the others are dropped.
std::span<const Event> Input::Poll()
{
    events_.clear();
    const SDL_WindowID id = SDL_GetWindowID(window_);

    // the pointer's place when the mouse has just been given to it: nothing else tells where it is before it moves
    if (pointerPending_ && mode_ == MouseMode::Pointer) {
        float x = 0.0F;
        float y = 0.0F;
        SDL_GetMouseState(&x, &y);
        const float density = SDL_GetWindowPixelDensity(window_);
        events_.emplace_back(PointerMotion{.time = Now(), .x = x * density, .y = y * density});
    }

    pointerPending_ = false;
    // SDL's mouse focus as the window's events tell it: given up last, or taken back since
    bool mouseLeft = false;

    for (SDL_Event event{}; SDL_PollEvent(&event);) {
        switch (event.type) {
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP:
            events_.emplace_back(KeyOf(event.key));
            break;
        case SDL_EVENT_TEXT_INPUT:
            AppendText(events_, event.text);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            events_.push_back(MotionOf(event.motion, mode_, window_));
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP:
            events_.emplace_back(MouseButton{.time = TimeOf(event.button.timestamp), .button = event.button.button, .down = event.button.down});
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            if (const std::optional<MouseWheel> wheel = WheelOf(event.wheel)) {
                events_.emplace_back(*wheel);
            }

            break;
        case SDL_EVENT_DISPLAY_ADDED:
        case SDL_EVENT_DISPLAY_REMOVED:
        case SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED:
        case SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED:
            events_.emplace_back(WindowEvent{.time = TimeOf(event.display.timestamp), .change = WindowChange::ScreensChanged});
            break;
        case SDL_EVENT_QUIT:
            // SDL sends it after the window's close button, and for the system's own requests to end the program
            events_.emplace_back(WindowEvent{.time = TimeOf(event.quit.timestamp), .change = WindowChange::Quit});
            break;
        default:
            // the window's own events, another window's left out
            if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST && event.window.windowID == id) {
                if (event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE || event.type == SDL_EVENT_WINDOW_MOUSE_ENTER) {
                    mouseLeft = event.type == SDL_EVENT_WINDOW_MOUSE_LEAVE;
                }
                else if (const std::optional<WindowEvent> change = WindowEventOf(event.window)) {
                    events_.emplace_back(*change);
                }
            }

            break;
        }
    }

    // In relative mode SDL gives the mouse's focus up when the window is deactivated, and takes it back only on a
    // motion of the pointer itself, which never comes while the mode holds the pointer in the window: the system's
    // arrow then stays drawn over the view, after a change of the display that deactivated the window for an instant.
    // Turned off and on again, the mode takes the focus from the keyboard's (SDL_SetRelativeMouseMode).
    const bool focusLost = mouseLeft && mode_ == MouseMode::Game && SDL_GetKeyboardFocus() == window_;

    // a refusal leaves the mode as SDL has it, which SetMouseMode asks for again
    if (focusLost && !(SDL_SetWindowRelativeMouseMode(window_, false) && SDL_SetWindowRelativeMouseMode(window_, true))) {
        mode_ = SDL_GetWindowRelativeMouseMode(window_) ? MouseMode::Game : MouseMode::Pointer;
    }

    return events_;
}

// SDL's relative mode hides the pointer, keeps the mouse in the window and reads its motion raw.
bool Input::SetMouseMode(MouseMode mode)
{
    if (mode == mode_) {
        return true;
    }

    if (!SDL_SetWindowRelativeMouseMode(window_, mode == MouseMode::Game)) {
        return false;
    }

    mode_ = mode;
    pointerPending_ = mode == MouseMode::Pointer;
    return true;
}

// SDL's mouse grab of the window: the pointer stays visible and cannot leave it.
bool Input::SetPointerConfined(bool confined)
{
    if (confined == confined_) {
        return true;
    }

    if (!SDL_SetWindowMouseGrab(window_, confined)) {
        return false;
    }

    confined_ = confined;
    return true;
}

// SDL's text input belongs to a window.
bool Input::SetTextInput(bool on)
{
    if (on == textInput_) {
        return true;
    }

    if (!(on ? SDL_StartTextInput(window_) : SDL_StopTextInput(window_))) {
        return false;
    }

    textInput_ = on;
    return true;
}

} // namespace platform

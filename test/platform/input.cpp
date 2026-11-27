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

// The tests of src/shared/platform/input, with events pushed into SDL's queue at chosen times: the keys (a letter, a
// named key, the keypad with num lock, the key under Escape by its place, a key the layer does not name with the
// system's name, an accented letter of a layout), each with its own time; a text of two characters as two events;
// the mouse as the pointer (a place) and as the view (a motion in floats), the view keeping it when SDL gives up its
// focus, its buttons, its wheel by whole notches
// and by its own direction; the window's events, another window's left out; the screens' and the quit's; the order
// kept; the text input turned on and off.
#include "platform.h"

#include <check.h>

#include <shared/platform/input/input.h>
#include <shared/platform/window/window.h>

#include <SDL3/SDL.h>

#include <chrono>
#include <optional>
#include <span>
#include <variant>

namespace {

using namespace std::chrono_literals;

// pushes a key event dated at a number of nanoseconds
void PushKey(SDL_Scancode scancode, SDL_Keycode key, SDL_Keymod mod, bool down, Uint64 timestamp)
{
    SDL_Event event{};
    event.key.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.timestamp = timestamp;
    event.key.scancode = scancode;
    event.key.key = key;
    event.key.mod = mod;
    event.key.down = down;
    CHECK(SDL_PushEvent(&event));
}

// pushes one of the window's own events
void PushWindow(SDL_EventType type, SDL_WindowID window, Sint32 data1, Sint32 data2)
{
    SDL_Event event{};
    event.window.type = type;
    event.window.timestamp = 77;
    event.window.windowID = window;
    event.window.data1 = data1;
    event.window.data2 = data2;
    CHECK(SDL_PushEvent(&event));
}

// the one event of a list as the type expected; nullptr, with a failed check, when it is not that
template <typename T> const T* Only(std::span<const platform::Event> events)
{
    CHECK_EQUAL(events.size(), 1U);

    if (events.size() != 1) {
        return nullptr;
    }

    const T* const event = std::get_if<T>(&events.front());
    CHECK(event != nullptr);
    return event;
}

// a key pushed and read back
std::optional<platform::KeyEvent> Key(platform::Input& input, SDL_Scancode scancode, SDL_Keycode key, SDL_Keymod mod, bool down, Uint64 timestamp)
{
    PushKey(scancode, key, mod, down, timestamp);
    const auto* const event = Only<platform::KeyEvent>(input.Poll());
    return event != nullptr ? std::optional(*event) : std::nullopt;
}

// the keys: by the layout's symbol, the named ones, the console key by its place, the system's name of the others
void CheckKeys(platform::Input& input)
{
    if (const auto key = Key(input, SDL_SCANCODE_W, SDLK_W, SDL_KMOD_NONE, true, 1000)) {
        CHECK(key->time == 1000ns);
        CHECK(key->named == platform::NamedKey::None);
        CHECK(key->symbol == U'w');
        CHECK_EQUAL(key->id, SDLK_W);
        CHECK(key->down && !key->repeat && !key->console && !key->numLock);
    }

    if (const auto key = Key(input, SDL_SCANCODE_W, SDLK_W, SDL_KMOD_NONE, false, 2'500'000)) {
        CHECK(key->time == 2'500'000ns);
        CHECK(!key->down);
    }

    // AZERTY: the key at the place of 2 is marked with an e acute (U+00E9), the key under Escape is marked with a superscript two
    if (const auto key = Key(input, SDL_SCANCODE_2, 0xE9, SDL_KMOD_NONE, true, 3)) {
        CHECK(key->symbol == char32_t{0xE9});
        CHECK(!key->console);
    }

    if (const auto key = Key(input, SDL_SCANCODE_GRAVE, 0xB2, SDL_KMOD_NONE, true, 4)) {
        CHECK(key->console);
        CHECK(key->symbol == char32_t{0xB2});
    }

    if (const auto key = Key(input, SDL_SCANCODE_SPACE, SDLK_SPACE, SDL_KMOD_NONE, true, 5)) {
        CHECK(key->symbol == U' ');
        CHECK(key->named == platform::NamedKey::None);
    }

    if (const auto key = Key(input, SDL_SCANCODE_RETURN, SDLK_RETURN, SDL_KMOD_NONE, true, 6)) {
        CHECK(key->named == platform::NamedKey::Enter);
        CHECK(key->symbol == U'\0');
    }

    if (const auto key = Key(input, SDL_SCANCODE_DELETE, SDLK_DELETE, SDL_KMOD_NONE, true, 7)) {
        CHECK(key->named == platform::NamedKey::Delete);
        CHECK(key->symbol == U'\0');
    }

    if (const auto key = Key(input, SDL_SCANCODE_F5, SDLK_F5, SDL_KMOD_NONE, true, 8)) {
        CHECK(key->named == platform::NamedKey::F5);
    }

    if (const auto key = Key(input, SDL_SCANCODE_RSHIFT, SDLK_RSHIFT, SDL_KMOD_RSHIFT, true, 9)) {
        CHECK(key->named == platform::NamedKey::RightShift);
    }

    if (const auto key = Key(input, SDL_SCANCODE_KP_5, SDLK_KP_5, SDL_KMOD_NUM, true, 10)) {
        CHECK(key->named == platform::NamedKey::Keypad5);
        CHECK(key->numLock);
    }

    // a key the layer does not name: its identifier, and the system's name for it
    if (const auto key = Key(input, SDL_SCANCODE_F13, SDLK_F13, SDL_KMOD_NONE, true, 11)) {
        CHECK(key->named == platform::NamedKey::None);
        CHECK(key->symbol == U'\0');
        CHECK_EQUAL(key->id, SDLK_F13);
        CHECK_EQUAL(platform::KeyName(key->id), "F13");
        // and back from the name, whatever its case; a text that names no key gives none
        CHECK(platform::KeyFromName("f13") == key->id);
        CHECK(!platform::KeyFromName("no such key"));
    }

    // X11's and Wayland's layouts: Alt Gr (the level 3 shift, SDLK_MODE) at the right Alt's place is the right Alt, as on
    // Windows; the level 5 shift at Caps Lock's place is Caps Lock; Compose at the menu key's place takes that place's code
    if (const auto key = Key(input, SDL_SCANCODE_RALT, SDLK_MODE, SDL_KMOD_MODE, true, 12)) {
        CHECK(key->named == platform::NamedKey::RightAlt);
        CHECK_EQUAL(key->id, SDLK_RALT);
    }

    if (const auto key = Key(input, SDL_SCANCODE_CAPSLOCK, SDLK_LEVEL5_SHIFT, SDL_KMOD_NONE, true, 13)) {
        CHECK(key->named == platform::NamedKey::CapsLock);
    }

    if (const auto key = Key(input, SDL_SCANCODE_APPLICATION, SDLK_MULTI_KEY_COMPOSE, SDL_KMOD_NONE, true, 14)) {
        CHECK(key->named == platform::NamedKey::None);
        CHECK_EQUAL(key->id, SDLK_APPLICATION);
    }

    // a key the player's layout made into one the layer names keeps it: Caps Lock made a Ctrl
    if (const auto key = Key(input, SDL_SCANCODE_CAPSLOCK, SDLK_LCTRL, SDL_KMOD_LCTRL, true, 15)) {
        CHECK(key->named == platform::NamedKey::LeftCtrl);
    }
}

// a text of two characters, of two and three bytes in UTF-8, as two events of the text's time
void CheckText(platform::Input& input, SDL_Window* window)
{
    CHECK(!SDL_TextInputActive(window));
    CHECK(input.SetTextInput(true));
    CHECK(SDL_TextInputActive(window));
    // asked again for the state it has
    CHECK(input.SetTextInput(true));

    SDL_Event event{};
    event.text.type = SDL_EVENT_TEXT_INPUT;
    event.text.timestamp = 42;
    event.text.text = "\xC3\xA9\xE2\x82\xAC";
    CHECK(SDL_PushEvent(&event));
    const std::span<const platform::Event> events = input.Poll();
    CHECK_EQUAL(events.size(), 2U);

    if (events.size() == 2) {
        const auto* const first = std::get_if<platform::TextEvent>(&events[0]);
        const auto* const second = std::get_if<platform::TextEvent>(&events[1]);
        CHECK(first != nullptr && first->character == char32_t{0xE9} && first->time == 42ns);
        CHECK(second != nullptr && second->character == char32_t{0x20AC} && second->time == 42ns);
    }

    CHECK(input.SetTextInput(false));
    CHECK(!SDL_TextInputActive(window));
}

// the mouse: a place as the pointer, a motion in floats as the view, the buttons, the wheel
void CheckMouse(platform::Input& input, SDL_Window* window)
{
    SDL_Event motion{};
    motion.motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.timestamp = 50;
    motion.motion.x = 10.0F;
    motion.motion.y = 20.0F;
    motion.motion.xrel = 1.5F;
    motion.motion.yrel = -2.25F;
    CHECK(SDL_PushEvent(&motion));

    if (const auto* const pointer = Only<platform::PointerMotion>(input.Poll())) {
        CHECK(pointer->x == 10.0F && pointer->y == 20.0F);
        CHECK(pointer->time == 50ns);
    }

    CHECK(input.SetMouseMode(platform::MouseMode::Game));
    CHECK(SDL_GetWindowRelativeMouseMode(window));
    // what the change of mode itself put in the queue is not this test's
    static_cast<void>(input.Poll());
    CHECK(SDL_PushEvent(&motion));

    if (const auto* const view = Only<platform::MouseMotion>(input.Poll())) {
        CHECK(view->dx == 1.5F && view->dy == -2.25F);
        CHECK(view->time == 50ns);
    }

    // SDL's mouse focus given up while the mouse turns the view and the window has the keyboard (the dummy driver
    // gives both to a window shown): the view keeps the mouse, SDL its focus, and nothing is handed over
    PushWindow(SDL_EVENT_WINDOW_MOUSE_LEAVE, SDL_GetWindowID(window), 0, 0);
    CHECK(input.Poll().empty());
    CHECK(SDL_GetWindowRelativeMouseMode(window));
    CHECK(SDL_GetMouseFocus() == window);
    CHECK(input.Poll().empty());

    // given back to the pointer, the mouse tells where the pointer is before it moves, once
    CHECK(input.SetMouseMode(platform::MouseMode::Pointer));
    CHECK(!SDL_GetWindowRelativeMouseMode(window));
    static_cast<void>(Only<platform::PointerMotion>(input.Poll()));
    CHECK(input.Poll().empty());

    // the pointer leaving the window as the pointer is no change of the mode
    PushWindow(SDL_EVENT_WINDOW_MOUSE_LEAVE, SDL_GetWindowID(window), 0, 0);
    CHECK(input.Poll().empty());
    CHECK(!SDL_GetWindowRelativeMouseMode(window));

    // the pointer kept in the window and freed
    CHECK(!SDL_GetWindowMouseGrab(window));
    CHECK(input.SetPointerConfined(true));
    CHECK(SDL_GetWindowMouseGrab(window));
    CHECK(input.SetPointerConfined(false));
    CHECK(!SDL_GetWindowMouseGrab(window));
    static_cast<void>(input.Poll());

    SDL_Event button{};
    button.button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    button.button.timestamp = 60;
    button.button.button = SDL_BUTTON_RIGHT;
    button.button.down = true;
    CHECK(SDL_PushEvent(&button));

    if (const auto* const pressed = Only<platform::MouseButton>(input.Poll())) {
        CHECK_EQUAL(pressed->button, platform::kRightButton);
        CHECK(pressed->down && pressed->time == 60ns);
    }

    // two notches away from the player; the same on a system that flips the wheel; less than a notch is no event
    SDL_Event wheel{};
    wheel.wheel.type = SDL_EVENT_MOUSE_WHEEL;
    wheel.wheel.timestamp = 70;
    wheel.wheel.integer_y = 2;
    wheel.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    CHECK(SDL_PushEvent(&wheel));

    if (const auto* const turned = Only<platform::MouseWheel>(input.Poll())) {
        CHECK_EQUAL(turned->notches, 2);
    }

    wheel.wheel.integer_y = -2;
    wheel.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    CHECK(SDL_PushEvent(&wheel));

    if (const auto* const turned = Only<platform::MouseWheel>(input.Poll())) {
        CHECK_EQUAL(turned->notches, 2);
    }

    wheel.wheel.integer_y = 0;
    CHECK(SDL_PushEvent(&wheel));
    CHECK(input.Poll().empty());
}

// the window's events in the order pushed, another window's left out, then the screens' and the quit's
void CheckWindowEvents(platform::Input& input, SDL_Window* window)
{
    const SDL_WindowID id = SDL_GetWindowID(window);
    PushWindow(SDL_EVENT_WINDOW_FOCUS_LOST, id, 0, 0);
    PushWindow(SDL_EVENT_WINDOW_MINIMIZED, id, 0, 0);
    PushWindow(SDL_EVENT_WINDOW_FOCUS_GAINED, id + 1, 0, 0);
    PushWindow(SDL_EVENT_WINDOW_RESTORED, id, 0, 0);
    PushWindow(SDL_EVENT_WINDOW_FOCUS_GAINED, id, 0, 0);
    PushWindow(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED, id, 640, 480);

    SDL_Event display{};
    display.display.type = SDL_EVENT_DISPLAY_ADDED;
    display.display.timestamp = 78;
    CHECK(SDL_PushEvent(&display));
    display.display.type = SDL_EVENT_DISPLAY_DESKTOP_MODE_CHANGED;
    CHECK(SDL_PushEvent(&display));
    display.display.type = SDL_EVENT_DISPLAY_USABLE_BOUNDS_CHANGED;
    CHECK(SDL_PushEvent(&display));
    // the screen's present mode changes with exclusive fullscreen: not the list's
    display.display.type = SDL_EVENT_DISPLAY_CURRENT_MODE_CHANGED;
    CHECK(SDL_PushEvent(&display));
    SDL_Event quit{};
    quit.quit.type = SDL_EVENT_QUIT;
    quit.quit.timestamp = 79;
    CHECK(SDL_PushEvent(&quit));

    const std::span<const platform::Event> events = input.Poll();
    constexpr platform::WindowChange kExpected[] = {
        platform::WindowChange::FocusLost,      platform::WindowChange::Minimized,        platform::WindowChange::Restored,
        platform::WindowChange::FocusGained,    platform::WindowChange::PixelSizeChanged, platform::WindowChange::ScreensChanged,
        platform::WindowChange::ScreensChanged, platform::WindowChange::ScreensChanged,   platform::WindowChange::Quit};
    CHECK_EQUAL(events.size(), std::size(kExpected));

    if (events.size() != std::size(kExpected)) {
        return;
    }

    for (std::size_t i = 0; i < events.size(); ++i) {
        const auto* const change = std::get_if<platform::WindowEvent>(&events[i]);
        CHECK(change != nullptr && change->change == kExpected[i]);

        if (change != nullptr && change->change == platform::WindowChange::PixelSizeChanged) {
            CHECK(change->width == 640 && change->height == 480);
            CHECK(change->time == 77ns);
        }
    }

    CHECK(input.Poll().empty());
}

} // namespace

// runs the input's checks on a window of the dummy driver
void RunInputTests()
{
    if (!StartOnDummyDriver()) {
        return;
    }

    {
        auto window =
            platform::Window::Open("modernjk_test_platform", platform::WindowSettings{.state = platform::DisplayState::Window}, platform::Context::None);
        CHECK(window.has_value());

        if (window) {
            platform::Input input(*window);
            // what the window's opening put in the queue is not this test's
            static_cast<void>(input.Poll());
            CheckKeys(input);
            CheckText(input, window->Handle());
            CheckMouse(input, window->Handle());
            CheckWindowEvents(input, window->Handle());
        }
    }

    platform::Stop();
}

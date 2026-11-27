# The input

The keyboard, the mouse and the window's own events (`input.h`, `input.cpp`): `Input::Poll()` hands over everything that happened since the call before, as a list of typed events in order, each with the time it happened at on the layer's clock (`platform::Now`), in nanoseconds.

## In the game

A player who taps a strafe key for a few milliseconds between two frames moves for those milliseconds: the press and the release each carry their own time, so the share of a tick the key was held is exact, whatever the frame rate. The mouse turns the view by the device's own counts, in fractions when the system gives them. In the menus the pointer is the system's, and the game gets its place. The key under Escape opens the console on every keyboard, whatever its layout prints on it.

## The events

| event | when | what it holds |
|---|---|---|
| `KeyEvent` | a key pressed, released or repeated | the key (below), `down`, `repeat`, `console`, `numLock` |
| `TextEvent` | a character typed, while the text input is on | a Unicode code point |
| `MouseMotion` | the mouse moved, in `MouseMode::Game` | the motion in floats, without the system's acceleration |
| `PointerMotion` | the pointer moved, in `MouseMode::Pointer`; and once when the mouse is given to the pointer, before it moves | its place in the window, in pixels |
| `MouseButton` | a button pressed or released | its number: `kLeftButton`, `kMiddleButton`, `kRightButton`, `kBackButton`, `kForwardButton`, then the mouse's others |
| `MouseWheel` | the wheel turned by at least a notch | whole notches, positive away from the player |
| `WindowEvent` | the focus, minimized, restored, a new size in pixels, a screen plugged or unplugged or its desktop's mode or usable area changed, quit | the change, and the size for a new size |

## A key

A key is the key of the player's layout, not its place: the key marked Z is Z on a QWERTY and on an AZERTY keyboard. A `KeyEvent` says which in three ways:

- `symbol`: the character the layout gives the key, in lower case, for a key that types one (`w`, `é`, `2`, the space);
- `named`: one of the keys every keyboard has and that type nothing (Escape, the arrows, F1 to F12, the modifiers of each side, the keypad's);
- `id`: a number for the key whatever it is, the same for the same key, which `KeyName` turns into the system's name for it ("F13", "Left Windows"). A program can bind a key the layer does not name by that name, and `KeyFromName` tells whether a text read from a configuration is such a name.

On X11 and Wayland a layout can give a key that types nothing a symbol the layer does not name: Alt Gr on the right Alt of most European layouts (the level 3 shift), the level 5 shift, Compose, Meta, Hyper. Such a key is named by its place, as SDL names it on Windows and macOS, where a layout does not rename those keys: Alt Gr is `RightAlt`. A key the player's layout made into one the layer names (Caps Lock made a Ctrl) keeps that name.

`console` is apart: true for the key under Escape, found by its place, so that one key opens the console on every layout. `numLock` tells whether the keypad types digits, for a program that takes them as digits in a text field and as keys in game.

## Rules

- A character comes from a `TextEvent` only, never from a key: the layout, its dead keys and the system's input method make the text. The text input is on while something takes characters (`SetTextInput(true)`: the console, the chat, a menu's field) and off in game.
- `MouseMode::Game` is SDL's relative mode of the window: the pointer hidden and kept in the window, the motion read raw. Nothing is thrown away at a change of mode, and the pointer is never moved by the program.
- In `MouseMode::Game`, when SDL gives up the mouse's focus while the window keeps the keyboard's (`SDL_EVENT_WINDOW_MOUSE_LEAVE`, which Windows causes when it deactivates the window for an instant, during a change of the display), `Poll` turns the relative mode off and on again, which takes the focus back from the keyboard's: SDL otherwise takes it back only on a motion of the pointer itself, which the relative mode never lets happen, and the system's arrow stays drawn over the view. SDL drops the motion it has not handed over yet at that instant and puts the pointer back at its place.
- In `MouseMode::Pointer` the pointer is free to leave the window, unless the program keeps it in (`SetPointerConfined(true)`): a program in exclusive fullscreen does, since a click on another screen would take its focus and minimize it.
- The wheel's direction is the wheel's own: a system that flips it ("natural scrolling") is turned back. Less than a notch makes no event.
- Another window's events are dropped. The quit comes once, for the window's close button and for the system's own requests.
- The list `Poll` returns is valid until the next call.
- `SetMouseMode` and `SetTextInput` return false when the system refuses, the state staying as it was; asked for the state they have, they do nothing, so a program can call them at every frame.
- The `Input` is made from the window and must not outlive it.

## Example

```cpp
platform::Input input(window);

if (!input.SetMouseMode(platform::MouseMode::Game)) {
    // the system refused: the pointer stays
}

for (const platform::Event& event : input.Poll()) {
    if (const auto* key = std::get_if<platform::KeyEvent>(&event)) {
        if (key->console && key->down) {
            ToggleConsole();
        }
        else if (key->symbol == U'w') {
            Forward(key->down, key->time); // the time the key moved at, to the nanosecond
        }
    }
    else if (const auto* motion = std::get_if<platform::MouseMotion>(&event)) {
        Turn(motion->dx, motion->dy);
    }
}
```

## Tests

`test/platform/input.cpp`, on SDL's dummy video driver, with events pushed into SDL's queue at chosen times: a letter, an accented letter of a layout, the space, named keys, the keypad with num lock, the key under Escape by its place, a key the layer does not name with the system's name; a key of a symbol the layer does not name taken by its place (Alt Gr as the right Alt, the level 5 shift on Caps Lock, Compose on the menu key) and one the layout made a Ctrl kept; a text of two characters as two events; the mouse as the pointer and as the view, a button, the wheel by notches and flipped; the window's events in order, another window's dropped; the screens' and the quit's; the text input on and off.

# The menus' display rows

The display's rows of the setup menus, the same in the three games (`display.h`, `display.cpp`): what the screen, resolution and refresh rate rows show, and the value a click on each gives. Functions of their arguments only, without SDL: the multiplayer's UI module compiles the file as the engines do (`modernjk_platform_menus`), reads and writes its own copies of the settings, and gets the screens from the engine (`Platform_Screens`).

## In the game

The video page of the setup menus shows four rows in place of the old video mode, colour depth and full screen rows:

| row | setting | what it offers |
|---|---|---|
| Display | `r_displayState` | Window, Borderless, Exclusive (a list written in the menu) |
| Screen | `r_screen` | each screen by its number from 1 and its name: "1: AW2724HF" |
| Resolution | `r_windowWidth` and `r_windowHeight` in a window, `r_exclusiveMode` in exclusive fullscreen | in a window, the screen's own sizes that fit in its usable area (without the taskbar); in exclusive fullscreen, "Desktop" (the screen's present mode, `r_exclusiveMode` empty) then each size of the screen's modes; greyed in borderless fullscreen, showing the desktop's size |
| Refresh rate | `r_exclusiveMode` | in exclusive fullscreen with a mode of its own, the rates the screen has at that size, the highest first; greyed otherwise, showing the desktop's rate |

The screens' modes are read the first time the rows ask for them (`Platform_Screens`), not when the game starts: the system enumerates them slowly that first time (half a second for two screens of some 440 modes each), once, then again only after a change of the screens.

A click, Enter or the right arrow gives the next value, the right button or the left arrow the one before; a list wraps around. The changes wait for "Apply", which restarts the video, as the page's other rows.

## Rules

- A size that changes keeps the refresh rate it had when the new size has it, else takes its highest.
- A screen that changes keeps exclusive fullscreen's size when it has a mode of that size, at the nearest rate; else exclusive fullscreen goes to its desktop's mode.
- A window's size typed in the console that is not in the list stays shown; the first click goes to the list's first size, or its last when back.
- A window's title bar and borders are not counted against the usable area: the system tells them only for a window that has them, and the menus list the sizes while the game may be in fullscreen.
- The sizes are written as the games wrote their modes, "1920 X 1080"; a rate as SDL gives it, "119.88 Hz"; `r_exclusiveMode` as `1920x1080@119.88`, which reads back to the same rate.

## Example

```cpp
const std::span<const platform::Screen> screens = trap->ext.Screens();
platform::WindowSettings choice = /* the menu's copies of the five settings */;

choice = platform::NextResolution(screens, choice, false);               // a click on the row
const std::string shown = platform::ResolutionText(screens, choice, desktopWord);
const bool greyed = !platform::RefreshHasChoice(choice);                  // the refresh rate row
```

## Tests

`test/platform/menus.cpp`, on two screens written in the test as SDL lists them (1920 by 1080, one with a taskbar of 48 pixels): the screen a number names; the display's state from its setting; a window's sizes, which fit in the usable area, and exclusive fullscreen's; a size's rates; a mode read from its text and written back, SDL's fractional rates included; each row's text; a click on each row, forward and back, wrapping, a rate kept, taken highest or nearest.

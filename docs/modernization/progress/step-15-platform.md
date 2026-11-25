# Step 15: a new platform layer on SDL 3

Plan step 15 ([../PLAN.md](../PLAN.md)). The survey and the decisions P1 to P9 of 15 November 2026 are in [../surveys/platform-layer.md](../surveys/platform-layer.md). This page holds what was read and decided when the step started, on 25 November 2026, and where to resume. Nothing is written in the code yet.

The step is not a migration: the layer is designed from SDL 3's own documentation, as a new SDL 3 program is written today, and the present code (`src/shared/sdl/`) only tells which features a player has. Three criteria hold for every choice (25 November 2026): the most responsive possible (the picture, the keys, the feedback), one system for the multiplayer and both single-player games, and the same on every platform.

## 1. Decisions (25 November 2026)

Put all at once, each with its options; none changes the CPU, the memory or a delay in a way that can be measured, except where said.

| # | question | decided | not taken |
|---|---|---|---|
| 1 | The program's structure | a classic `main` and the engine's own loop: SDL's documentation calls it "the primary way", and the loop keeps the tick's wait in its hands (P8) | SDL's main callbacks (`SDL_AppIterate`, `SDL_AppEvent`): SDL calls the engine at its own pace; meant for iOS, the web and Wayland |
| 2 | The boundary between the engines and the layer | a new interface: the client creates the window once and keeps it, the renderer receives the context and never destroys the window; the input is a list of typed events, each with its own time in nanoseconds; in place of `WIN_*`, `IN_*`, `Sys_QueEvent` and the queue of 256 events dated in milliseconds; `vid_restart` no longer creates the window again | the new layer behind today's function names: fewer changes in the engines, the shape of ioquake3 kept |
| 3 | The present layer | `src/shared/sdl/` and SDL 2 removed in the part that replaces them; a build of the commit before is the reference for the comparisons. With decision 2, keeping it selectable by a CMake option (the plan until then) would keep two interfaces in the engines | kept selectable until the new layer is accepted |
| 4 | The order | nothing is done on SDL 2: it goes at the start. Parts 1 and 2 (the window, the keyboard, the mouse) are one piece of work, since SDL 2's input cannot read an SDL 3 window; then part 4 (the picture's output stage), part 5 (the gamepad), part 6 (the wait). `r_gamma` has no effect between parts 1 and 2 and part 4 | the brightness pass written first on SDL 2 and compared with the hardware ramp on one build; the plan's order with a throwaway bridge for the input |
| 5 | The mouse's motion | kept in floating point up to the view angles: SDL 3 gives floats, without the system's acceleration by default (`SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE` is "0"); nothing is rounded | rounded to integers when read, as today |
| 6 | The single player's view that does not turn | not diagnosed apart: looked at when the single player runs on the new input, which is the multiplayer's (section 8) | a short instrumented session before part 1 |
| 7 | The brightness | the output stage of a recent engine, in both OpenGL renderers: the scene drawn into an offscreen image, one final shader pass that writes it to the window and applies `r_gamma`, the curve computed in floating point in the shader (this revises P2's table of 256 values), the offscreen image's precision (8, 10 or 16 bits) chosen by a measurement of the banding in the dark tones. The lighting in linear light and the HDR output go to plan step 25 (section 4). Cost: one full-screen pass per frame, to measure at 100, 125, 144 (vsync) and 333 frames per second; no frame of delay | the whole model now in rd-vanilla (the work of step 25 done twice); nothing before step 25 (`r_gamma` is 1 by default, the menus' brightness removed until then) |
| 8 | How SDL 3 is linked | its source in `libs/SDL3`, built with the project as a static library, as OpenAL Soft is: no DLL to copy or install, never an executable beside a DLL of another version; a player can still put another SDL in its place with the environment variable `SDL3_DYNAMIC_API` (section 6) | a DLL built by the project and copied by CMake after each link, which SDL's authors prefer ("We'd still rather you ship a shared library") |

The download of SDL 3.4.20 was approved the same day.

## 2. What a player has today (the features to keep)

Read in `src/shared/sdl/sdl_window.cpp` (822 lines), `src/shared/sdl/sdl_input.cpp` (1,507 lines) and their callers on 25 November 2026. The removals of P3 stand.

| what | today | where |
|---|---|---|
| the window | a window, a borderless window, exclusive fullscreen, by five settings (`r_mode`, `r_customwidth`, `r_customheight`, `r_fullscreen`, `r_noborder`), `r_centerWindow`; created by the renderer at each of its starts | `GLimp_SetMode`, `sdl_window.cpp:303`; `WIN_Init` `:669` |
| the picture | vertical sync changed while running (`r_swapInterval`), multisampling (`r_ext_multisample`, needing a restart of the video), the icon, the brightness by the screen's hardware ramp (`r_gamma`) | `WIN_Present` `:142`, `WIN_SetGamma` `:767` |
| commands | `minimize`, `modelist`, `in_restart` | `sdl_window.cpp:671`, `src/shared/sys/sys_main.cpp:164` |
| the keys | by the layout's symbol, with the keypad's digits while a text field is open, the console key under Escape | `IN_TranslateSDLToJKKey`, `sdl_input.cpp:279` |
| text | UTF-8 turned into the engine's one-byte set (the Windows-1252 letters, and tables for Windows-1251 and Windows-1250), always on | `ConvertUTF32ToExpectedCharset` `:807`, `SDL_StartTextInput` `:786` |
| the mouse | relative motion in integers, five buttons and up to 32 more, the wheel as one key press per event, taken or given back by the console, the loading and the focus | `IN_ProcessEvents` `:1117`, `IN_Frame` `:1441` |
| the joystick | one device chosen by number, polled; axes, hats, buttons and trackballs as keys, or the axes as analog values | `IN_JoyMove` `:1253` |
| the focus | `com_unfocused`, `com_minimized`, the sound muted (`SND_SetFocus`) | `sdl_input.cpp:1219-1240` |
| the rest | the clipboard's text, the error's message box, SDL's version printed, the modules' libraries loaded through `SDL_LoadObject` | `src/shared/sys/sys_main.cpp:110`, `:274`, `:742`; `src/shared/sys/sys_loadlib.h:40` |

How the engines reach it: the renderers through their import table (`WIN_Init`, `WIN_SetGamma`, `WIN_Present`, `WIN_Shutdown`, `GL_GetProcAddress`, `GL_ExtensionSupported`: `src/codemp/rd-common/tr_public.h:342`, `src/code/rd-common/tr_public.h:94`, filled in `src/codemp/client/cl_main.cpp:2351` and `src/code/client/cl_main.cpp:1170`); the main loops through `IN_Frame` (`src/codemp/qcommon/common.cpp:1545`, `:1567`; `src/code/qcommon/common.cpp:1427`); the events through `Sys_QueEvent` and `Com_EventLoop` (`src/shared/sys/sys_event.cpp`, 256 events, the time an `int` of milliseconds taken when the event is read).

The settings of today, each to keep, replace or remove when its part is designed: `r_fullscreen`, `r_noborder`, `r_centerWindow`, `r_customwidth`, `r_customheight`, `r_mode`, `r_displayRefresh`, `r_swapInterval`, `r_stereo`, `r_stencilbits`, `r_depthbits`, `r_colorbits`, `r_ignorehwgamma`, `r_ext_multisample`, `r_allowSoftwareGL`, `r_availableModes` and `r_sdlDriver` (read only); `in_mouse`, `in_nograb`, `in_keyboardDebug`, `in_joystick`, `in_joystickNo`, `in_joystickUseAnalog`, `joy_threshold`, `in_availableJoysticks` (read only); `cl_consoleKeys`, `cl_consoleUseScanCode`, `cl_consoleShiftRequirement`; `com_unfocused`, `com_minimized`, `com_maxfpsUnfocused`, `com_maxfpsMinimized`, `com_busyWait`.

## 3. Why the picture changes with the focus

Seen in game: the picture changes a little when the window loses or gains the focus. It is the brightness. SDL 2 applies the hardware ramp to the whole screen, puts the screen's own ramp back when the window loses the focus and applies the game's again when it gains it (`SDL_OnWindowFocusLost` and `SDL_OnWindowFocusGained`, SDL 2.0.12's `src/video/SDL_video.c`, read on 25 November 2026); it does not even send a new ramp to the driver while the window has no focus (`SDL_SetWindowGammaRamp`). Microsoft's documentation says to leave that table alone, since it may hold the monitor's calibration, and that recent games do such things "as part of the post-processing of each frame".

In the same code, overbright works only in exclusive fullscreen and only with the hardware ramp (`src/codemp/rd-vanilla/tr_image.cpp:1293-1300`, `src/code/rd-vanilla/tr_image.cpp:1299-1306`); `r_overBrightBits` is 0 by default.

## 4. The brightness as recent engines do it

Read on 25 November 2026 in the engines' and Microsoft's documentation (primary); no primary source was found on one shipped game's brightness setting.

- The screen's ramp is not touched (Microsoft, "Using gamma correction").
- The scene is computed in linear light into an offscreen image of high precision. Windows composes in 16-bit floating point itself (Microsoft, "Use DirectX with Advanced Color"). Godot: "After all the rendering is done, the scene linear render requires transforming to a suitable output such as an sRGB display".
- A transform at the end of the frame converts to what the screen expects: Unreal Engine chooses it with `r.HDR.Display.OutputDevice` (sRGB, Rec709 or an explicit gamma for a standard screen; PQ or scRGB for an HDR screen).
- The player's setting is a parameter of that transform: Unreal Engine's `r.Gamma` ("gamma on output"); on an HDR screen, the level of the reference white.

What this step takes of it (decision 7), in part 4: the output stage. The scene is drawn into an offscreen image (a framebuffer object), and one final pass writes it to the window, applying `r_gamma` computed in floating point; the same formula as today's ramp, so the same brightness at the same setting; the same picture in a window, in borderless and in exclusive fullscreen, with or without the focus, and in the captures. One code for both renderers, in `src/shared/rd-common`. The multisampling becomes a property of the offscreen image, so it changes while running and the window is never created again. It needs OpenGL 3.0.

What goes to plan step 25 (the Vulkan renderer), written there: the lighting computed in linear light (it changes every blend, so the whole picture, and needs shaders everywhere, while rd-vanilla draws with the fixed functions), the tone mapping, the HDR output with the reference white's level, and the same output stage with the player's brightness as its parameter.

Not looked at for this step, read on the way: Quake3e does the same pass behind `r_fbo` (its documentation), and ioquake3's issue 632 lists a multiplying full-screen rectangle and the brightness baked into the textures at load; neither is a model here.

## 5. SDL 3 as read (version 3.4.20)

SDL 3.4.20, of 3 November 2026, is still the newest stable release on 25 November 2026 (the project's releases page). No agent skill about SDL with any standing was found (two web searches; the one found calls itself an "empty shell"), so the sources are SDL's documentation and the headers of the release.

- The program: `SDL_main.h` is included by the one file that holds `main`; there is no `SDL2main` library any more. The main callbacks are optional ("The primary way we expect people to write SDL apps is still with SDL_main").
- The functions return `bool`; the events carry a time in nanoseconds (`SDL_GetTicksNS`'s clock).
- The keyboard (the wiki's "Best keyboard practices"): scancodes for the keys used by their place, keycodes for the keys used by their meaning, `SDL_GetKeyName` to show a key, text only from `SDL_EVENT_TEXT_INPUT` between `SDL_StartTextInput` and `SDL_StopTextInput` (per window, off by default), `SDL_SetTextInputArea` to place the input method. `SDL_HINT_KEYCODE_OPTIONS` is "french_numbers,latin_letters" by default.
- The raw keyboard on Windows (P7): `SDL_HINT_WINDOWS_RAW_KEYBOARD` ("0" by default, since 3.2.0, set at any time); new since the decisions P1 to P9: `SDL_HINT_WINDOWS_RAW_KEYBOARD_EXCLUDE_HOTKEYS` (3.4.0) and `SDL_HINT_WINDOWS_RAW_KEYBOARD_INPUTSINK` (3.4.4), both off by default; `SDL_HINT_WINDOWS_GAMEINPUT` is off by default in 3.4.
- The mouse: the relative mode is the window's (`SDL_SetWindowRelativeMouseMode`), its motion in floats, not scaled by the system (`SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE` "0"); a warp makes no motion event in relative mode (`SDL_HINT_MOUSE_RELATIVE_WARP_MOTION`); `SDL_SetWindowKeyboardGrab` is not for games ("Normal games should not use keyboard grab").
- The window: created by properties (`SDL_CreateWindowWithProperties`, the `SDL_PROP_WINDOW_CREATE_*` names); borderless fullscreen when the window has no fullscreen mode, exclusive when it has one of `SDL_GetFullscreenDisplayModes` (`SDL_SetWindowFullscreenMode`, then `SDL_SetWindowFullscreen`); these changes are asynchronous and answered by events (`SDL_EVENT_WINDOW_ENTER_FULLSCREEN`, `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`), `SDL_SyncWindow` waits for them; the size in pixels by `SDL_GetWindowSizeInPixels`; the screens by `SDL_DisplayID`; `SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS` is "auto" (only exclusive fullscreen minimizes).
- OpenGL: `SDL_GL_SetSwapInterval` takes 0, 1 or -1 (adaptive; when it fails, try 1); `SDL_GL_GetProcAddress` returns a function pointer; `SDL_GL_ExtensionSupported` is to be cached.
- Wayland: `SDL_HINT_VIDEO_DOUBLE_BUFFER` asks for two buffers instead of three "for lower latency".
- The gamepad: `SDL_INIT_GAMEPAD`; devices come and go by events ("Your application should always support gamepad hotplugging"); the face buttons are named by their place (south, east, west, north), their printed label by `SDL_GetGamepadButtonLabel`; the sticks from -32768 to 32767, the triggers from 0 to 32767.
- The hardware ramp and the window's brightness are gone ("use a shader or in-game effect").

## 6. SDL 3's build, measured (25 November 2026)

The release's source archive, from the project's own releases page: `SDL3-3.4.20.tar.gz`, 15,656,494 bytes, SHA-256 `f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f`; its GPG signature was downloaded and not checked. Unpacked: 53 MB, of which `src` 28 MB, `include` 3.8 MB, `test` 8.9 MB and `examples` 7.7 MB. It is in `build/sdl3/` (outside git), with the two trial builds.

Built here with llvm-mingw's clang (Release), through `add_subdirectory( ... EXCLUDE_FROM_ALL)` as SDL's `README-cmake` describes, with the subsystems the client does not use turned off (`SDL_AUDIO`, `SDL_GPU`, `SDL_RENDER`, `SDL_CAMERA`, `SDL_SENSOR`, `SDL_HAPTIC`, `SDL_DIALOG`, `SDL_TRAY`, `SDL_POWER`) and without its tests, examples and install rules. A small program asks what the client will ask (a window with an OpenGL context, the events, the relative mouse, the text, the gamepads, the clipboard, a message box, `SDL_DelayPrecise`) and runs in both forms:

| form | what ships | size |
|---|---|---|
| static | nothing beside the executable | 2,049,024 bytes for the whole program, stripped: about 2.0 MB more per executable |
| shared | `SDL3.dll` beside each executable | 3,126,272 bytes, 2,166,784 stripped; the program 11,776 bytes |

The speed is the same in both: SDL's calls go through its jump table either way ("one extra function call", SDL's `README-dynapi`), and a statically linked program can still be given another SDL ("Developers can statically link SDL, and users can still replace it", by `SDL3_DYNAMIC_API`).

SDL's own code gives one warning with this clang (`-Wunused-but-set-global`, `src/events/SDL_events.c:119`): it is a library's code, built outside the project's warnings.

## 7. Linux: the packages to build with

SDL talks to X11 and to Wayland through their own client libraries, and each piece is a library with its package of headers; both are needed since a player has one or the other. They are headers only: at run time SDL loads what the player's system has ("the rest of the features will be enabled dynamically at runtime depending on the available features on the target system", SDL's `README-linux`), as OpenAL Soft does with the sound servers.

| package | for |
|---|---|
| `libx11-dev`, `libxext-dev` | an X11 window |
| `libxrandr-dev` | the screens and their modes |
| `libxi-dev` | the raw mouse motion |
| `libxfixes-dev` | the mouse kept in the window |
| `libxcursor-dev` | the cursor |
| `libxkbcommon-dev` | the keyboard layouts and the dead keys |
| `libwayland-dev` | a Wayland window |
| `libdecor-0-dev` | the title bar under GNOME |
| `libegl1-mesa-dev` | the OpenGL context under Wayland |
| `libudev-dev` | a gamepad plugged while playing |

Beside those the project already asks (`libgl1-mesa-dev`, `libdbus-1-dev`, `pkg-config`); `libsdl2-dev` leaves the list. Turned off, having no use here: IBus (the input methods of the Asian languages, which the game's font cannot draw before plan step 26), XTest and the X screen saver extension. Each option kept is to be required by CMake, as OpenAL Soft's backends are, so that a binary never lacks Wayland silently. The final list comes from SDL's configuration report on the Linux VM, in part 1; the root README is updated then.

## 8. The single player's view, as read

Known issue of the progress page: in single player neither the mouse nor the keys (`+left`, `+right`) turn the view. Read on 25 November 2026: both write `cl.viewangles` in `CL_CreateCmd` (`CL_AdjustAngles` and `CL_MouseMove`, `src/code/client/cl_input.cpp:853-863`), which then replaces them with the angles the cgame gave when `cl_overrideAngles` is set (`:876`). The cgame sets it through `cgi_SetUserCmdAngles`, called by `CG_SetClientViewAngles` from `CG_CheckModifyUCmd` when one of three tests of the game says so (`PM_AdjustAnglesToGripper`, `PM_AdjustAnglesForSpinningFlip`, `G_CheckClampUcmd`: `src/code/cgame/cg_predict.cpp:285-308`). So the cause is not in the platform code; one of these tests always true is suspected, not proven. The floats cross the cgame's interface correctly as read (`PASSFLOAT`, `_vmf`).

The multiplayer's view on the Linux VM (the same known issue): a guess, not verified, is VirtualBox's absolute pointer, which gives no relative motion.

## 9. The rest of the inventory (25 November 2026)

The menus, the keys' way to the modules and the main loops, read the same day after the decisions. P1 stands as decided on 15 November 2026 (the keys bound by the layout's symbol, every key of every layout bindable, the console key by its place), and so does the key event and the character event in place of one `int` (17 November 2026).

### 9.1 The menus

The display's settings are in the six setup menus, whose changed copies are already in `assets/` since plan step 21 (`assets/jka/ui/jamp/setup.menu` and `ingame_setup.menu` for the multiplayer, `assets/jka/ui/setup.menu` and `ingamesetup.menu` for Jedi Academy's single player, `assets/jk2/ui/setup.menu` and `ingamesetup.menu` for Jedi Outcast). Each shows the same five rows:

| row | setting | today | with this step |
|---|---|---|---|
| video mode | `ui_r_mode` | a fixed list of `r_mode` numbers: 640x480 (3), 800x600 (4), 1024x768 (6), 1152x864 (7), 1280x1024 (8), 1600x1200 (9), 2048x1536 (10), 2400x600 (12) (`assets/jka/ui/jamp/setup.menu:1065`) | the display's state, the screen and, for exclusive fullscreen, a mode of that screen (P4): rows fed at run time, not a list written in the menu |
| colour depth | `ui_r_colorbits` | default, 16 or 32 bits | removed (P3) |
| full screen | `ui_r_fullscreen` | off or on | part of the display's state (P4) |
| vertical sync | `r_swapInterval` | off or on | kept |
| brightness | `r_gamma` | a slider from 0.5 to 3, 1 by default | kept; it drives the output stage (part 4) |

No menu reads `r_availableModes`.

The input's settings are in the six controls menus, which are still the games' own (`assets1.pk3` of each game: `ui/jamp/controls.menu` and `ui/jamp/ingame_controls.menu`, `ui/controls.menu` and `ui/ingamecontrols.menu` of Jedi Academy, the same two names in Jedi Outcast, whose `assets2.pk3` replaces them); a changed copy goes into `assets/` with the part that changes a row:

| row | setting | note |
|---|---|---|
| mouse sensitivity | `sensitivity`, a slider from 2 to 30, 5 by default | the engine's, kept |
| smooth mouse, invert mouse, free look, always run | `m_filter`, `ui_mousePitch` (the sign of `m_pitch`), `cl_freelook` (not in Jedi Academy's single player), `cl_run` | the engine's, kept |
| joystick | `in_joystick`, and `joy_threshold`, a slider from 0.05 to 0.75 | part 5 |
| three more joystick rows | `joy_xbutton`, `joy_ybutton`, `use_ff` | no code reads these three settings: rows that do nothing, to remove in part 5 (`use_ff` is not in the multiplayer's `controls.menu`) |
| the binds | one row per command | `ITEM_TYPE_BIND`, below |

### 9.2 The UI's code behind them

- "Apply changes" (`uiScript updatevideosetup`) copies each `ui_r_*` setting into its `r_*` one and runs `vid_restart` (`src/codemp/ui/ui_main.cpp:4793-4808`, `src/code/ui/ui_main.cpp:3775-3790`); the menu's opening copies them the other way (`:4844-4859`, `:3826-3841`).
- The quality presets (`ui_r_glCustom`) also set `ui_r_fullScreen` to 1, `ui_r_mode` to 3 or 4, `ui_r_colorbits` and `ui_r_depthbits` (`src/codemp/ui/ui_main.cpp:4651-4713`, `src/code/ui/ui_main.cpp:3075-3137`), and a change of `ui_r_colorbits` sets `ui_r_depthbits` (`:4622`, `:3046`): a preset of the picture's quality should no longer change the display's state.
- A row fed by the engine exists since plan step 21 part 6, the sound's output: an owner-drawn item (`UI_SOUND_DEVICE`, `UI_DrawSoundDevice`, `UI_SoundDevice_HandleKey`, `src/codemp/ui/ui_main.cpp:1478` and `:3303`, `src/code/ui/ui_main.cpp:1812`). It is the model for the display's rows.
- The binds: `Item_Bind_HandleKey` takes the key pressed and `Controls_SetConfig` writes the bind (`src/codemp/ui/ui_shared.cpp:4931`, `:4755`; `src/code/ui/ui_shared.cpp:8785`, `:8740`); a key's name comes from the engine (`Key_KeynumToStringBuf`).

### 9.3 The keys' way to the modules

- The key codes are `fakeAscii_t`, an `enum class` from `A_NULL` to `A_AUX31` with `MAX_KEYS` at its end, defined twice with the same enumerators (`src/codemp/ui/keycodes.h` and `src/code/client/keycodes.h` differ by an include and a blank line): one definition in `src/shared/qcommon` is due (GUIDELINES section 6). The engine's tables are indexed by it (`kg.keys`, `keynames`, `src/codemp/client/keys.h:40-51`), so P1's codes given at run time to the keys the table lacks need a place in that range and a name kept beside it.
- The engines' entries, called by `Com_EventLoop`: `CL_KeyEvent(fakeAscii_t key, bool down, unsigned time)`, `CL_CharEvent(int key)`, `CL_MouseEvent(int dx, int dy, int time)`, `CL_JoystickEvent(int axis, int value, int time)`.
- A character reaches a module through its key entry, as `key | K_CHAR_FLAG` (1024, `src/shared/qcommon/q_limits.h:34`): the multiplayer's UI and cgame (`src/codemp/client/cl_keys.cpp:1500-1502`), the single player's UI (`src/code/client/cl_keys.cpp:1366`); read back in `ui_shared.cpp` (`src/codemp/ui/ui_shared.cpp:3310`, `src/code/ui/ui_shared.cpp:8999`). It is one byte of the engine's character set.
- Who takes the characters tells when text is typed: the console, the UI, the cgame and the chat in the multiplayer (`KEYCATCH_CONSOLE`, `KEYCATCH_UI`, `KEYCATCH_CGAME`, `KEYCATCH_MESSAGE`), the console and the UI in the single player, and the console while disconnected (`CL_CharEvent`). The layer's text input follows these.
- A key's time is used: the share of a tick a movement key was held (`CL_KeyState`, `src/codemp/client/cl_input.cpp:498-520`), in milliseconds.

### 9.4 The main loops and the clock

- One `main` serves the clients and the dedicated server (`src/shared/sys/sys_main.cpp:705`); on Windows the client's comes through SDL 2's `SDL2main` library. It prints SDL's versions (`:742`) and calls `Com_Frame` for ever (`:773`).
- The multiplayer's `Com_Frame` (`src/codemp/qcommon/common.cpp:1476`): until the frame is due it sleeps on the socket (`NET_Sleep`) to one millisecond before, then spins (`:1554-1564`); when a command is due between two frames it reads the input and makes the command (`IN_Frame`, `CL_InputTick`, `:1544-1549`); then `IN_Frame`, and the frame's time is the time `Com_EventLoop` returns (`:1567-1570`).
- The single player's `Com_Frame` (`src/code/qcommon/common.cpp:1381`): `Sys_Sleep` to one millisecond before, then a spin (`:1420-1426`), `IN_Frame`, `Com_EventLoop`. It has no input between the frames: the single player has no fixed tick.
- Both cap the frames of a window without focus or minimized (`com_maxfpsUnfocused`, `com_maxfpsMinimized`).
- The engine's clock is `Sys_Milliseconds`, an `int` of milliseconds: `timeGetTime` on Windows (`src/shared/sys/sys_win32.cpp:95`), `clock_gettime(CLOCK_MONOTONIC)` on Linux (`src/shared/sys/sys_unix.cpp:105`). The frame's time, the keys' times and the frame's length are all on it, while SDL 3 dates its events in nanoseconds on its own clock.

### 9.5 What the interface has to settle

Read from the above, for the interface's proposal (section 10, point 1); none is decided:

1. the clock: the events' times in nanoseconds against the engine's `int` of milliseconds, and how one is brought onto the other;
2. the display's rows in the menus fed at run time (the state, the screen, a mode for exclusive fullscreen) and the presets no longer touching them;
3. the key codes in one shared definition, the keys the table lacks (P1), the key event and the character event apart, and what a character is until plan step 26 (the engine's one-byte set);
4. the text input started and stopped by who takes the characters;
5. the same input for the single player, read at its frames only;
6. the `main`: `SDL_main.h` for the clients only, the dedicated server linking no SDL;
7. the three dead joystick rows, with part 5.

## 10. Where to resume

Parts 1 and 2 (the window, the keyboard and the mouse, with SDL 2's removal) start, once approved, with:

1. the interface of `src/shared/platform/`, proposed with its open choices (section 9.5) before any code (the model: [step-21-sound.md](step-21-sound.md) section 12);
2. SDL 3.4.20's source into `libs/SDL3` from `build/sdl3/` (the files left out and the options in its README, as GUIDELINES section 9 asks), linked statically; `libs/SDL2`, `src/shared/sdl/` and the exception for SDL 2 in GUIDELINES section 9 removed.

## 11. Not read or verified

- The menus' five other copies were read by their settings only (the rows' lists as in the multiplayer's `setup.menu`); Jedi Outcast's multiplayer menus (`ui/jk2mp/`) are not this project's.
- In SDL 3's documentation: `README-highdpi`, `SDL_events.h`, `SDL_timer.h` (`SDL_DelayPrecise`), and which video driver SDL chooses first on Linux (Wayland or X11); the truncated parts of `SDL_video.h` and `SDL_hints.h` were read in two passes, not line by line.
- Whether Ubuntu 22.04's versions of Wayland and libdecor are enough for SDL 3.4.20.
- SDL 3 built with GCC 15 and with MSVC: only llvm-mingw's clang was tried.
- The cost of the output stage's pass and the banding of an 8-bit offscreen image: to measure in part 4.
- Whether Windows presents a borderless OpenGL window directly or through the compositor (P4): to measure in part 1.

## 12. Sources

Primary:
- SDL releases: https://github.com/libsdl-org/SDL/releases
- SDL 3 main functions: https://wiki.libsdl.org/SDL3/README-main-functions
- SDL 3 best keyboard practices: https://wiki.libsdl.org/SDL3/BestKeyboardPractices
- SDL 3 hints: https://wiki.libsdl.org/SDL3/CategoryHints and `include/SDL3/SDL_hints.h` of the release 3.4.20
- SDL 3 `SDL_video.h` and `SDL_gamepad.h` of the release 3.4.20: https://github.com/libsdl-org/SDL/tree/release-3.4.20/include/SDL3
- SDL 3 `README-cmake`, `README-linux`, `README-dynapi`, `WhatsNew.txt` and `CMakeLists.txt` of the release 3.4.20: https://github.com/libsdl-org/SDL/tree/release-3.4.20
- SDL 3 migration guide (for what was removed only): https://wiki.libsdl.org/SDL3/README-migration
- SDL 2.0.12 `src/video/SDL_video.c`: https://github.com/libsdl-org/SDL/blob/release-2.0.12/src/video/SDL_video.c
- Microsoft, "Using gamma correction": https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/using-gamma-correction
- Microsoft, "Use DirectX with Advanced Color on high/standard dynamic range displays": https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range
- Unreal Engine, "High dynamic range display output": https://dev.epicgames.com/documentation/en-us/unreal-engine/high-dynamic-range-display-output-in-unreal-engine
- Unreal Engine, console variables reference: https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-console-variables-reference
- Godot, "High dynamic range lighting": https://docs.godotengine.org/en/stable/tutorials/3d/high_dynamic_range.html
- Quake3e's documentation: https://github.com/ec-/Quake3e/blob/main/docs/quake3e.htm
- ioquake3 issue 632: https://github.com/ioquake/ioq3/issues/632

Community:
- The one agent skill found about SDL 3, empty: https://skills.lc/ln167/odin_master/ln167-odin-master-claude-skills-sdl3-skill-md

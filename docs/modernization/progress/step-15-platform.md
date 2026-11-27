# Step 15: a new platform layer on SDL 3

Plan step 15 ([../PLAN.md](../PLAN.md)). The survey and the decisions P1 to P9 of 15 November 2026 are in [../surveys/platform-layer.md](../surveys/platform-layer.md). This page holds what was read and decided when the step started, on 25 November 2026, the code of parts 1 and 2 as it was written from that day on (section 14), and where to resume (section 15, 26 November 2026: the multiplayer's client is on the layer, committed the same day; the single player's engines on it and SDL 2 removed the same day, sections 14.14 and 14.15, committed the same day; the menus' display rows, section 14.16, and the checks and measurements before and after, section 14.17, committed the same day), and part 4, the picture's output stage (section 16, from 26 November 2026: sub-steps 1 and 2 done and validated the same day; where to resume, section 16.6).

The step is not a migration: the layer is designed from SDL 3's own documentation, as a new SDL 3 program is written today, and the present code (`src/shared/sdl/`) only tells which features a player has. Three criteria hold for every choice (25 November 2026): the most responsive possible (the picture, the keys, the feedback), one system for the multiplayer and both single-player games, and the same on every platform.

## 1. Decisions (25 November 2026)

Put all at once, each with its options; none changes the CPU, the memory or a delay in a way that can be measured, except where said.

| # | question | decided | not taken |
|---|---|---|---|
| 1 | The program's structure | a classic `main` and the engine's own loop: SDL's documentation calls it "the primary way", and the loop keeps the tick's wait in its hands (P8) | SDL's main callbacks (`SDL_AppIterate`, `SDL_AppEvent`): SDL calls the engine at its own pace; meant for iOS, the web and Wayland |
| 2 | The boundary between the engines and the layer | a new interface: the client creates the window once and keeps it, the renderer receives the context and never destroys the window; the input is a list of typed events, each with its own time in nanoseconds; in place of `WIN_*`, `IN_*`, `Sys_QueEvent` and the queue of 256 events dated in milliseconds; `vid_restart` no longer creates the window again | the new layer behind today's function names: fewer changes in the engines, the shape of ioquake3 kept |
| 3 | The present layer | `src/shared/sdl/` and SDL 2 removed in the part that replaces them; a build of the commit before is the reference for the comparisons. With decision 2, keeping it selectable by a CMake option (the plan until then) would keep two interfaces in the engines | kept selectable until the new layer is accepted |
| 4 | The order | nothing is done on SDL 2: it goes at the start. Parts 1 and 2 (the window, the keyboard, the mouse) are one piece of work, since SDL 2's input cannot read an SDL 3 window; then part 4 (the picture's output stage), part 5 (the gamepad), part 6 (the wait). Between parts 1 and 2 and part 4, `r_gamma` acts through the renderer's existing path for a device without a ramp, applied to the textures when they are loaded, so after a `vid_restart` (section 10.3) | the brightness pass written first on SDL 2 and compared with the hardware ramp on one build; the plan's order with a throwaway bridge for the input |
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

The release's source archive, from the project's own releases page: `SDL3-3.4.20.tar.gz`, 15,656,494 bytes, SHA-256 `f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f`; its GPG signature was downloaded and not checked (decided 25 November 2026: the two signing keys are not downloaded, the checksum alone stands for the archive). Unpacked: 53 MB, of which `src` 28 MB, `include` 3.8 MB, `test` 8.9 MB and `examples` 7.7 MB. It is in `build/sdl3/` (outside git), with the two trial builds.

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

Read from the above, for the interface's proposal; settled in section 10:

1. the clock: the events' times in nanoseconds against the engine's `int` of milliseconds, and how one is brought onto the other;
2. the display's rows in the menus fed at run time (the state, the screen, a mode for exclusive fullscreen) and the presets no longer touching them;
3. the key codes in one shared definition, the keys the table lacks (P1), the key event and the character event apart, and what a character is until plan step 26 (the engine's one-byte set);
4. the text input started and stopped by who takes the characters;
5. the same input for the single player, read at its frames only;
6. the `main`: `SDL_main.h` for the clients only, the dedicated server linking no SDL;
7. the three dead joystick rows, with part 5.

## 10. The interface, and its decisions (25 November 2026)

### 10.1 The interface proposed

A module independent of the engines, as the sound is: the namespace `platform`, one folder per subject (`window/`, `input/`, later `gamepad/`), a README in each, and tests (`modernjk.platform`) that run without a screen, on SDL's dummy video driver with events pushed into SDL's queue.

```cpp
namespace platform {

using Time = std::chrono::nanoseconds;      // the client's one clock
Time Now() noexcept;

enum class DisplayState : std::uint8_t { Window, Borderless, Exclusive };
struct DisplayMode { int width, height; float refresh; };
struct WindowSettings { DisplayState state; int screen; int width, height; DisplayMode exclusive; int swapInterval; };

class Window {                               // created once by the client, kept until quit
    static std::expected<Window, std::string> Open(const WindowSettings&, const GlRequest&);
    bool Apply(const WindowSettings&);       // state, screen, mode: never a new window
    void Present();
    PixelSize Size() const;
    // the screens and their modes, the OpenGL functions, the icon, the pointer's image
};

struct KeyEvent    { Time time; Key key; bool down; bool repeat; };
struct TextEvent   { Time time; char32_t character; };
struct MouseMotion { Time time; float dx, dy; };
struct MouseButton { Time time; int button; bool down; };
struct MouseWheel  { Time time; int notches; };
struct WindowEvent { Time time; WindowChange change; };   // focus, minimized, size, close
using Event = std::variant<KeyEvent, TextEvent, MouseMotion, MouseButton, MouseWheel, WindowEvent>;

std::span<const Event> Poll();               // everything since the last call
void SetMouseMode(MouseMode);                // game (raw, relative) or pointer
void SetTextInput(bool);                     // on while the console, the chat or a menu takes characters
}
```

The names and the signatures are a sketch; the code settles them.

### 10.2 What it settles without a choice

- The key codes: `fakeAscii_t` in one definition shared by the trees. A key its table lacks (P1) takes a free code when it is first met, with the name SDL gives it (`SDL_GetKeyName`); a bind of such a key is written by that name.
- The characters: the modules get a character event of their own, and `K_CHAR_FLAG` goes (decided 17 November 2026). The layer hands a Unicode code point; the engine turns it into its one-byte set, with today's tables, until plan step 26.
- The text input is on while something takes characters (the console, the chat, a menu, the cgame's own catcher), off in game.
- The wheel: one key press per whole notch (`integer_y`, SDL 3.2.12), not one per event.
- Linux: SDL tries Wayland first, then X11 (`Wayland_preferred_bootstrap` before `X11_bootstrap`, SDL 3.4.20's `src/video/SDL_video.c`); no setting.
- The single player reads the same input, at its frames only.
- The Windows manifest already declares the program aware of each screen's pixel density (`src/shared/sys/win_manifest.manifest`), and on Windows SDL's sizes are in pixels (`README-highdpi`).

### 10.3 Facts found while designing it

- A switch of the display's state without a restart of the video is whole only at the same size in pixels: the cgame and the UI keep the picture's size they read at their start (`cgs.glconfig`, `uiInfo.uiDC.glconfig`: `src/codemp/cgame/cg_main.cpp:2490`, `src/codemp/ui/ui_main.cpp:9352`, `src/code/cgame/cg_main.cpp:1670`, `src/code/ui/ui_main.cpp:2349`). In parts 1 and 2 a change of the size restarts the renderer and the modules, as a change of mode does today, without creating the window again. Whether the offscreen image of part 4 lets the size change without it is decided with part 4.
- Between parts 1 and 2 and part 4, `r_gamma` is not without effect: for a device without a ramp the renderer already applies it to the textures when they are loaded (`R_LightScaleTexture`, `src/codemp/rd-vanilla/tr_image.cpp:314-330`), so after a `vid_restart`. Part 4 removes that path.
- The raw keyboard's times are real (P7): SDL reads the raw input on a thread of time-critical priority that sleeps until input arrives, and dates each key when it is read (`WIN_RawInputThread`, `WIN_PollRawInput`: SDL 3.4.20's `src/video/windows/SDL_windowsrawinput.c:102-122`, `SDL_windowsevents.c:805-871`); the mouse's packets of one read are spread over the time since the read before.
- The clocks differ: the engine's is `timeGetTime` on Windows and `CLOCK_MONOTONIC` on Linux, SDL's is `QueryPerformanceCounter` and `CLOCK_MONOTONIC_RAW` (`src/timer/windows/SDL_systimer.c`, `src/timer/unix/SDL_systimer.c`). Two clocks drift apart, so one cannot be brought onto the other by a constant.

### 10.4 The decisions (25 November 2026)

| # | question | decided | not taken |
|---|---|---|---|
| 1 | The clock | one clock for the client, SDL's in nanoseconds (`SDL_GetTicksNS`, given by `platform::Now`); the client's `Sys_Milliseconds` reads it; the events' times stay in nanoseconds up to the computation of the share of a tick a key was held, which becomes exact instead of steps of 1 ms (a tenth of a 10 ms tick at each press and release). The dedicated server keeps its own clock | one clock with the times rounded to milliseconds on entering the engine; two clocks and a conversion measured again at each read |
| 2 | The display's settings | four new ones, each with its row in the six setup menus and its texts in the games' languages: `r_displayState` (0 a window, 1 borderless fullscreen, the default, 2 exclusive fullscreen; in place of `r_fullscreen` and `r_noborder`), `r_screen` (0 the system's primary screen, then the others; new), `r_windowWidth` and `r_windowHeight` (the window's size, 1280x720 by default; in place of `r_mode`, `r_customwidth`, `r_customheight`), `r_exclusiveMode` (empty for the screen's present mode, else a mode such as `1920x1080@144`; in place of `r_mode` and `r_displayRefresh`). Kept: `r_swapInterval`, `r_ext_multisample`, `r_gamma`. Removed without a replacement: `r_centerWindow` (the window is centred on its screen), `r_availableModes`, `r_sdlDriver` (printed by `gfxinfo`), `in_mouse`, `in_nograb`, and P3's list | other names or defaults |
| 3 | The menus' pointer | the system's pointer with the game's image (`SDL_CreateColorCursor`), the raw relative mode in game only: the pointer moves at the system's pace, also while a level loads or at a low frame rate, at the speed of the player's desktop, and leaves a window. It is what recent engines do: Godot's documentation says a pointer drawn by the game "will add at least one frame of latency compared to a "hardware" mouse cursor" and recommends the system's "whenever possible"; Unity's default mode is "Use hardware cursors on supported platforms"; Unreal Engine has its "Hardware Cursors" setting. The UI then receives a position in place of a displacement, in the three games, and no longer draws its pointer | the pointer drawn by the game at the system's position (the desktop's speed, still a frame late); as today (drawn by the game, moved by the raw motion) |
| 2b | The refresh rate, which `r_displayRefresh` held as a free number from 0 to 240 | part of `r_exclusiveMode`, taken from the modes the screen itself reports; in a window and in borderless fullscreen the rate is the desktop's, which a program changes only by changing the screen's mode. In the menus, two rows write that one setting: the resolution, then the refresh rate among those the screen has at that resolution, the highest first; one row listing every mode would be some hundred values to step through on a recent screen. Unity does the same (the refresh rate of `Screen.SetResolution` applies in exclusive fullscreen only); Unreal Engine's stock settings have a window mode, a resolution of two integers and a frame rate limit, and no refresh rate | a setting of its own for the rate; one row for the whole mode |
| 4 | A tool to measure the presentation (P4: whether Windows shows a borderless OpenGL window directly or through the compositor) | its download approved; the tool chosen by a search when the measurement is made | |

## 11. Where to resume

The code of parts 1 and 2 (the window, the keyboard and the mouse, with SDL 2's removal), once approved, in this order, each point built and checked before the next:

1. ✅ SDL 3.4.20's source into `libs/SDL3` from `build/sdl3/` (the files left out and the options in its README, as GUIDELINES section 9 asks), built as a static library by the three compilers (section 14.1);
2. ✅ the module `src/shared/platform` (the window, the input) with its tests `modernjk.platform`, under netcodec's strict warnings and clang-tidy at zero, as the sound (section 14.2);
3. ✅ (sections 14.3 to 14.13) the multiplayer's engine on it: the client owning the window, the renderer's import table, the events and the clock, the key and character events to the modules, the pointer;
4. ✅ (sections 14.14 and 14.15) the single player's engine (both games) on it, with Escape while a key is bound or a field edited tried in its menus (section 14.12); then `libs/SDL2`, `src/shared/sdl/` and the exception for SDL 2 in GUIDELINES section 9 removed, the root README's packages updated;
5. ✅ (section 14.16) the menus: the four settings' rows and their texts, the rows removed, the presets no longer touching the display;
6. ✅ (section 14.17) the checks in game (the plan's list), the measurements before (SDL 2) and after (asked on 25 November 2026: as many as can be made, list below), `CHANGES.md`, the VM validations.

The measurements of point 6, each before and after, at 100, 125, 144 (vsync) and 333 frames per second where a frame rate matters. The reference is a build of the commit before this work, kept in a worktree made before `src/shared/sdl/` is removed (point 4), so that a measure can be made again with the same instrumentation:

- the frame's time and the process's CPU on a demo played back;
- `netstats`' input margin (100, 125 and 333 frames per second);
- the share of a tick a movement key was held, against the real one, with events injected at known times (steps of 1 ms before, exact after);
- the time spent reading the input per frame, with a mouse at 1,000 and at 8,000 reports per second;
- the presentation path (direct or through the compositor) and its latency, with the tool chosen by a search (section 10.4);
- how long a `vid_restart`, a switch of the display's state and an alt-tab take;
- the time from the start to the main menu;
- the process's memory and the executables' size;
- the CPU of a window without the focus and of a minimized one.

Not measurable here: the real delay from a key to the screen, which needs a hardware sensor.

At the end of the step, asked on 26 November 2026: see whether the code this step changes three times over can be one, as the sound's is (`src/shared/cgame/cg_sound.cpp`, compiled by each cgame): the clients' keys and input (`cl_keys.cpp`, `cl_input.cpp`) and the menus' shared code (`ui_shared.cpp`) exist once for the multiplayer and once for the single player, which also builds Jedi Outcast, and each change of this step is made in both.

At the end of the step, asked on 25 November 2026: the sound's engine part (`src/shared/sound/engine/engine.cpp`) read against the C++ Core Guidelines' text and clang-tidy, as the layer's was (section 14.5).

## 12. Not read or verified

- The menus' five other copies were read by their settings only (the rows' lists as in the multiplayer's `setup.menu`); Jedi Outcast's multiplayer menus (`ui/jk2mp/`) are not this project's.
- In SDL 3's documentation: `README-highdpi`, `SDL_events.h`, `SDL_timer.h` (`SDL_DelayPrecise`), and which video driver SDL chooses first on Linux (Wayland or X11); the truncated parts of `SDL_video.h` and `SDL_hints.h` were read in two passes, not line by line.
- Whether Ubuntu 22.04's versions of Wayland and libdecor are enough for SDL 3.4.20: they are (section 14.1).
- SDL 3 built with GCC 15 and with MSVC: done (section 14.1).
- The cost of the output stage's pass and the banding of an 8-bit offscreen image: to measure in part 4.
- Whether Windows presents a borderless OpenGL window directly or through the compositor (P4): directly, measured on 26 November 2026 (section 14.17, measure A).

## 13. Sources

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
- Godot, "Customizing the mouse cursor": https://docs.godotengine.org/en/stable/tutorials/inputs/custom_mouse_cursor.html
- Unity, `CursorMode`: https://docs.unity.com/en-us/engine/current/script-reference/unityengine/cursormode
- Unity, `Screen.SetResolution` and `RefreshRate`: https://docs.unity3d.com/ScriptReference/Screen.SetResolution.html, https://docs.unity.com/en-us/engine/6000.3/script-reference/unityengine/refreshrate
- Unreal Engine, `GameUserSettings` (Python API page, "Set Screen Resolution", "Set Frame Rate Limit"): https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/GameUserSettings?application_version=5.3, https://dev.epicgames.com/documentation/unreal-engine/BlueprintAPI/Settings/SetScreenResolution, https://dev.epicgames.com/documentation/unreal-engine/BlueprintAPI/Settings/SetFrameRateLimit
- Unreal Engine, "User Interface Settings" (hardware cursors): https://dev.epicgames.com/documentation/en-us/unreal-engine/user-interface-settings-in-the-unreal-engine-project-settings
- SDL 3.4.20's source, read in the release's archive: `src/video/SDL_video.c`, `src/video/windows/SDL_windowsrawinput.c`, `src/video/windows/SDL_windowsevents.c`, `src/timer/windows/SDL_systimer.c`, `src/timer/unix/SDL_systimer.c`, `include/SDL3/SDL_events.h`, `docs/README-highdpi.md`
- Quake3e's documentation: https://github.com/ec-/Quake3e/blob/main/docs/quake3e.htm
- ioquake3 issue 632: https://github.com/ioquake/ioq3/issues/632

Community:
- The one agent skill found about SDL 3, empty: https://skills.lc/ln167/odin_master/ln167-odin-master-claude-skills-sdl3-skill-md

## 14. The code of parts 1 and 2

Started on 25 November 2026, the document approved as it was and the menus' pointer confirmed as the system's (section 10.4, decision 3). In the order of section 11.

### 14.1 SDL 3 in `libs/SDL3` (25 November 2026)

- The source: the release's archive of section 6, 1,494 files and 33 MB in `libs/SDL3`: `src/`, `include/`, `cmake/`, `wayland-protocols/`, `CMakeLists.txt`, the licence, `CREDITS.md`, `WhatsNew.txt` and `REVISION.txt`, unchanged. Left out: its tests, examples, documents, build scripts and the projects of other build systems and platforms (the list in `libs/SDL3/README.md`). `src/` is kept whole, the platforms the project does not build included: SDL's CMake file lists its sources by folder, and an update stays a replacement of the files. Two of its folders have a name the repository's `.gitignore` ignores (`include/build_config/`, `src/hidapi/dist/`): taken back in there.
- The build (`Dependencies.cmake`, target `vendored::sdl`): a static library by SDL's own CMake file, for the engines that open a window. Off: the trial build's list (the audio, the GPU and render APIs, the camera, the sensors, the haptic devices, the dialogs, the tray, the power state, the test library, the tests, the examples, the install) and OpenGL ES. On Linux, off as well: IBus, XTest, the X screen saver and shape extensions, FriBidi and libthai (new in 3.4: right-to-left and Thai text in X11's own dialogs), the KMS/DRM video driver, liburing, libusb (the gamepads go through hidraw), PipeWire, and the Raspberry Pi's and Rockchip's video drivers: each either without a use here or found only when the build machine has its package. Kept and required: X11 with Xcursor, Xdbe, Xfixes, XInput 2, XRandR and Xsync, Wayland with libdecor, EGL, udev and D-Bus (with D-Bus, SDL also talks to the Fcitx input method, which needs no package). SDL leaves out without an error what the build machine lacks, so `Dependencies.cmake` reads SDL's results after its configuration (`HAVE_X11`, `HAVE_WAYLAND_SHARED`...) and stops when one is missing, naming the package; the names ending in `_SHARED` check that the libraries are opened at run time, not linked.
- The warning of section 6 (`-Wunused-but-set-global`, clang) is switched off on SDL's target with its reason.
- The test `modernjk.platform` (`test/platform`, 16 tests in `ctest` now): SDL as the layer will use it, on its dummy video driver, without a screen: the library's version is its headers', 3.4.20; the video and the gamepads start, the audio does not (not built); a window created by properties has the size asked, in pixels; the text input is off until started; the relative mouse mode is the window's; a key event pushed with a time in nanoseconds comes back with that time; the clock does not go back. The module's own tests join it with the module.
- The root README: the Linux packages of section 7 added to the prerequisites; SDL2's stay until `src/shared/sdl/` goes.
- Checked on the development machine (llvm-mingw's clang): the presets Release, Debug and AddressSanitizer build without a warning, `ctest` 16 of 16 in each.
- The VMs' Release builds, the same day (configuration, build, `ctest`; logs `build/sdl3-linux-gcc-release.log`, `build/sdl3-linux-clang-release.log`, `build/build-msvc-release.log`, `build/ctest-msvc-release.log`): GCC 15, Clang 23 and MSVC build SDL 3 and pass `ctest` 16 of 16, `modernjk.platform` included. On Linux every required piece was found with the packages of section 7 on Ubuntu 22.04 (Wayland 1.20, libdecor 0.1), and SDL's report gives the final list: the video drivers `dummy offscreen wayland(dynamic) x11(dynamic)`, the X11 libraries `xcursor xdbe xfixes xinput2 xrandr xsync`, the joystick drivers `hidapi linux virtual`; no compiler warning. Ubuntu 22.04's `wayland-scanner` (1.20) prints "XML failed validation against built-in DTD" for four protocols whose attribute `deprecated-since` is newer than its DTD, and generates their code all the same: the system's tool, not the compiler; left as it is. With MSVC, two warnings on one file of SDL: D9025 and C4652 on `src/stdlib/SDL_mslibc.c`, which SDL compiles with `/GL-` (its `memcpy` and `memset` must not be replaced at the link) against the Release presets' `/GL`; fixed the same day by building SDL without link-time optimization with MSVC (its functions are called through its jump table, so nothing was inlined across it); confirmed by the next MSVC build (section 14.2). MSVC's build has one joystick driver more than llvm-mingw's, `gameinput` (the Windows SDK's header; `SDL_HINT_WINDOWS_GAMEINPUT` is off by default): to look at with part 5.

### 14.2 The module `src/shared/platform` (25 November 2026)

The static library `modernjk_platform`, independent of the engines, under netcodec's strict warnings and its own `.clang-tidy` (the sound's checks), its findings errors in the clang-tidy preset; a README at its root and in each folder. No SDL type appears in its headers but three pointers declared by name (`SDL_Window`, `SDL_GLContextState`, `SDL_Cursor`). Nothing in the game uses it yet.

The interface as the code settled it, against section 10.1's sketch:

- `platform.h`: `Time` and `Now()` as sketched; `Start(name, version)` and `Stop()` (SDL's video and events; the raw keyboard asked on Windows, decision P7; the program's name given to the system with `SDL_SetAppMetadata`, which SDL's documentation asks for before the start); `Description()` (the library's version, its revision and the video driver, for the log, in place of `r_sdlDriver`); `ClipboardText()`; `ShowError()` (the message box, usable before the start).
- `window/`: `Window::Open(title, settings, gl)` returns the window or the error's text (`std::expected`); `gl` is optional, a window without it has no context (the tests' windows, since SDL's dummy driver has no OpenGL; a window drawn by another API, plan step 25). `Apply(settings)` for the state, the screen, the size and the mode, the window never created again; `Size()` in pixels; `Screens()` (each with its name, its desktop's mode and its exclusive modes, each size and rate once: 244 and 195 on the development machine's two screens); `Present()`; `SetSwapInterval()` (a method of its own, not a field of the settings as sketched: it changes at any frame, apart from the display's state, and needs the context); `Samples()`; `GlProcAddress()`, `GlExtensionSupported()`; `SetIcon()`, `SetPointerImage()` (decision 3 of section 10.4: the system's pointer with the game's image); `Minimize()`; `Handle()`.
- `input/`: a class `Input` made from the window (the sketch had free functions: the events' list would have been a global), `Poll()`, `SetMouseMode()`, `SetTextInput()`. A `KeyEvent` gives the key three ways: `symbol` (the character of the player's layout, for a key that types one), `named` (`NamedKey`: the 53 keys every keyboard has and that type nothing), `id` (a number for any key, which `KeyName` turns into the system's name: what P1's keys the engine's table lacks are bound by); with `console` (the key under Escape, by its place) and `numLock`. `TextEvent` holds one Unicode code point. The mouse's motion is two events: `MouseMotion` in the game's mode (floats, raw), `PointerMotion` in the pointer's mode (a place in pixels, for decision 3). `MouseButton` numbers the buttons from 1; `MouseWheel` counts whole notches by the wheel's own direction. `WindowEvent`: the focus gained and lost, minimized, restored, a new size in pixels, the screens changed, quit.

Rules written in the code and its READMEs: every size in pixels, converted from SDL's units by the pixel density; the window created hidden and shown once its context and state are set; one request for the context and one more without multisampling, the error's text giving both failures; `Apply` waits for SDL's asynchronous changes (`SDL_SyncWindow`); an exclusive mode is the screen's mode of the size asked with the nearest refresh rate; no exception, no math function of a library.

The tests, in `modernjk.platform` (`test/platform`: `sdl.cpp`, `window.cpp`, `input.cpp`): on SDL's dummy video driver, the events pushed into SDL's queue at chosen times (the list in each README). Found by them: the dummy driver's screen has no exclusive mode at all, so exclusive fullscreen is refused there and the window stays as it was, which the test checks. Since that driver has no OpenGL, `modernjk_test_platform real`, run by hand and not by `ctest`, opens a window with a context on the system's own driver and takes it through the three states (`real.cpp`): on the development machine (Windows 11, two screens at 1920x1080), borderless 1920x1080 with the 4 samples asked, a window of 800x600, exclusive fullscreen in the present mode, every check passed.

Checked on the development machine (llvm-mingw's clang): the presets Release, Debug and AddressSanitizer build without a warning, `ctest`'s five `modernjk` tests pass in each, clang-tidy at zero on the module's three files, clang-format clean.

The VMs' Release builds, the same day (logs `build/platform-linux-gcc-release.log`, `build/platform-linux-clang-release.log`, `build/build-msvc-release.log`, `build/ctest-msvc-release.log`): the module builds under the strict warnings with GCC 15, Clang 23 and MSVC without a warning, `ctest` 16 of 16 on each; SDL's two MSVC warnings of section 14.1 are gone with its link-time optimization off.

`modernjk_test_platform real` on the Linux VM (Ubuntu 22.04, a Wayland session of GNOME under VirtualBox; `build/platform-linux-real.log`): a window with a context and the 4 samples asked, the three states applied (borderless 1920x955, a window of 800x600, exclusive in the present mode). Learnt there:

- SDL chose its X11 driver, through XWayland (the screen is named `XWAYLAND0`), not its Wayland one. Read in SDL 3.4.20 (`Wayland_IsPreferred`, `src/video/wayland/SDL_waylandvideo.c`): Wayland is tried first only when the compositor has the protocol `fifo-v1`; without it SDL falls back to XWayland "for GPU performance reasons", and the Wayland driver comes only after X11's. Ubuntu 22.04's GNOME lacks that protocol. This refines section 10.2 ("SDL tries Wayland first, then X11"): no setting still; the Wayland driver is tried on that VM with the environment variable `SDL_VIDEO_DRIVER=wayland`, in the checks of point 6.
- Under X11 the OpenGL library gives a pointer for a function it does not have (a check of the test expected none): as SDL's documentation of `SDL_GL_GetProcAddress` says; the check removed, the rule written in `window.h` and its README (an extension's function is asked only once the extension is found).
- The vertical sync was refused by that VM's driver; the test now prints it instead of failing.

### 14.3 The multiplayer's engine on the module: the plan (25 November 2026)

Read for it the same day: `Com_Frame` and `Com_EventLoop` (`src/codemp/qcommon/common.cpp:1476`, `:895`), the system's event queue (`src/shared/sys/sys_event.cpp`), `IN_ProcessEvents` and `IN_Frame` (`src/shared/sdl/sdl_input.cpp:1117`, `:1441`), the renderer's six imports (`src/codemp/rd-common/tr_public.h:342-349`, filled in `src/codemp/client/cl_main.cpp:2357-2362`; the renderer links OpenGL itself and asks the layer only for its extensions' functions), the UI's pointer (`src/codemp/ui/ui_main.cpp:9534`: the image of the shader `cursor`, `gfx/menus/cursor1` (corrected in section 14.9; `menu/art/3_cursor2` was written here first), drawn 40 by 40 of the menus' 640 by 480, so 90 pixels high on a screen of 1080; moved by the mouse's motion, `:9667`).

Facts that shape the work:

- `src/shared/sys/` is compiled by the multiplayer's and the single player's engines, and the single player's stays on SDL 2 until point 4: between the two points its files choose by a definition of the target which layer they call; the switch goes at point 4.
- Today's input also makes characters of its own, which the console and the fields read: Backspace as Ctrl-H, and Ctrl with a letter as that control character (`sdl_input.cpp:1133-1136`): kept, in the engine's part.
- The joystick's code is in `sdl_input.cpp` and leaves with it; SDL 3's gamepads are part 5.
- The system's queue (`Sys_QueEvent`) also carries the console's typed lines, for the dedicated server too: that part stays. The journal (`com_journal`, the engine's record and replay of the queue's events) no longer sees the keys and the mouse once they come from the layer.

Decided the same day, each with its options:

| # | question | decided | not taken |
|---|---|---|---|
| 1 | The journal, which no longer sees the keys and the mouse and never recorded the network's packets | `com_journal` and its two files removed, in the three games: the demos, `com_debugSeed` with `fixedtime` and the scripted scenarios replay a session | kept for the console's lines only; a new journal of the layer's events |
| 2 | The joystick between this work's commit and part 5 | the plan's order (parts 1 and 2, then 4, 5, 6): a joystick does nothing in game between the two commits | part 5 right after parts 1 and 2 |
| 3 | The size of the menus' pointer, now the system's | the game's image scaled to the size it has today on screen (40 of the 480 of the window's height), made again when the size changes | the size of the desktop's pointers |

Sub-steps, each built and tried in game before the next:

1. the window and the input, the modules' interfaces untouched: the client starts the layer, opens the window from the display's settings and keeps it; the renderer's imports become "the display as it is" (the size, the samples), the swap and the two OpenGL lookups, without `WIN_Init`, `WIN_SetGamma` and `WIN_Shutdown`; `Com_Frame` reads the layer's events where it called `IN_Frame`, the engine's part turning them into its keys (the table of `sdl_input.cpp`, kept for its knowledge, over `NamedKey` and the symbols), its characters (the one-byte set's tables) and the mouse; the client's clock reads the layer's; the focus, the minimized state and the quit; the mouse's mode and the text input following who takes the keys; the modules' libraries loaded by the system's own call;
2. the keys' times in nanoseconds up to the share of a tick a key was held (`CL_KeyState`), and the mouse's motion in floats up to the view angles;
3. the display's four settings in place of the old ones, their commands (`modelist` listing the screens and their modes, `minimize`), `vid_restart` without a new window, a change of size restarting the renderer and the modules, the removed settings (section 10.4, decision 2, and P3);
4. the modules' character event in place of `K_CHAR_FLAG` (the multiplayer's UI and cgame);
5. the system's pointer with the game's image, the UI and the cgame given a position;
6. the keys the engine's table lacks, bound by the system's name (P1), the console key as P3 decided it, the key codes in one shared definition.

### 14.4 Sub-step 1: the multiplayer's client on the module (25 November 2026)

Written:

- `src/shared/platform/engine/` (`engine.h`, `engine.cpp`), the layer as an engine runs it, compiled by the engine against its own headers as the sound's `engine/` is: `Platform_OpenWindow` (the layer started, the display's settings registered, the window opened once, its icon), `Platform_Shutdown`, `Platform_Frame` (the mouse's mode, the text input, then every event given to the client), `Platform_Milliseconds`, and the renderer's four imports (`Platform_Display`, `Platform_Present`, `Platform_GlProcAddress`, `Platform_GlExtensionSupported`). The keys: `NamedKey` and the layout's symbols turned into the engine's key codes with the knowledge of the old table (a letter is its capital's code, the accented letters of the one-byte set, the printable ASCII characters), the keypad's digits with num lock while something takes the keys, the console key by its place and, with Shift, the key its layout marks (P3); Backspace as Ctrl-H and Ctrl with a letter as its control character, as before; a typed character through the one-byte set's tables; the mouse's motion in whole counts with the fraction carried (floats up to the view angles come in sub-step 2); the wheel one press and release per notch; the focus, the minimized state and the quit.
- The display's settings, registered by the engine's part: `r_displayState` (1 by default, borderless fullscreen), `r_screen`, `r_windowWidth`, `r_windowHeight` (1280 by 720), `r_exclusiveMode` (`1920x1080@144`, empty for the screen's present mode), all latched, with `r_swapInterval` and `r_ext_multisample`. A change is applied to the same window when the renderer starts again (`vid_restart`).
- The renderer's imports (`src/codemp/rd-common/tr_public.h`): `Platform_Display` (the display as it is) and `Platform_Present` in place of `WIN_Init`, `WIN_SetGamma`, `WIN_Present` and `WIN_Shutdown`; rd-vanilla no longer creates, destroys nor holds a window, and no longer sends a gamma ramp (`r_gamma` acts on the textures, section 10.3).
- `Com_Frame` calls `Platform_Frame` where it called `IN_Frame`: the events go straight to the client, no longer through the system's queue.
- The shared system files (`src/shared/sys/`): the definition `MODERNJK_PLATFORM`, set by the new target `modernjk_sys_platform` on the engines on the layer, chooses between the layer and SDL 2 (the clipboard, the error's message box, the program's end, the clock, the modules' libraries loaded by the system's own call, `in_restart` gone); `src/shared/platform/main.h` gives the program's entry point (SDL's `WinMain` on Windows). `src/shared/sys/sys_charset.cpp`: the one-byte set's tables, taken out of `sdl_input.cpp` for the engines that no longer compile it.
- The multiplayer's client links `modernjk_sys_platform` and no SDL 2: no `SDL2.dll` copied nor installed beside it.

Checked on the development machine (llvm-mingw's clang): the presets Release, Debug and AddressSanitizer build every target without a warning (the single player's engines, still on SDL 2, included), the five `modernjk` tests pass. A scripted silent session of the Release client (`r_displayState 0`, a window of 1280 by 720): the layer's line in the log (`SDL 3.4.20 (...), video driver windows`), the context on the machine's graphics card, `mp/duel1` loaded, a bot joined, a screenshot written showing the map and a menu, the program quit by its command without an error.

The VMs' Release builds, the same day: MSVC without an error nor a warning; GCC 15 and Clang 23 stopped on one error, a Linux clock function left unused in the client (`Sys_MonotonicMilliseconds`, `src/shared/sys/sys_unix.cpp`), now compiled only for the engines that call it; the second round built and passed `ctest` 16 of 16 with both. Then the engine part's functions renamed from `PLAT_` to `Platform_`, and the renderer's two imports with them (asked the same day); the clang presets built again without a warning, the VMs' builds of the renamed code to come.

Not checked: the keys, the mouse, the text and the focus, which need a player at the keyboard.

Known limits of this sub-step, each closed by a later one: the old display's settings (`r_mode`, `r_fullscreen`, `r_customwidth`...) are no longer read, and the menus' video rows still write them (sub-step 3 and point 5); `gfxinfo` still prints the old mode's line; `r_ext_multisample` is read when the window opens, so a change needs the program started again until part 4; the menus' pointer is still the game's own, moved by the mouse's motion (sub-step 5); a key the engine's table lacks does nothing (sub-step 6); no joystick (part 5); the journal was still there, no longer seeing the keys and the mouse (removed in section 14.6).

### 14.5 The layer against the C++ Core Guidelines' text (25 November 2026)

Asked on 25 November 2026: the layer follows the C++ Core Guidelines, its engine part included, and clang-tidy at zero does not show it: the guidelines themselves are read and the code checked against them. Until then clang-tidy had been run on the library's three files only; on `engine/engine.cpp`, which the engines compile, it gave 34 findings (22 for its variables at file scope), and `engine/engine.h` had no `[[nodiscard]]`.

The text read: the guidelines of 14 June 2026 (https://raw.githubusercontent.com/isocpp/CppCoreGuidelines/master/CppCoreGuidelines.md), the rule lists of the sections the layer touches (I, F, C, Enum, R, ES, E, Con, SF, SL) and the bodies of the rules below. What changed, each with its rule of the C++ Core Guidelines:

| rule | what it asks | what changed |
|---|---|---|
| I.2, I.3 | no non-`const` global variable; a singleton only in its simplest form, a function returning a local `static` | the engine part's 22 variables are one object (`Layer`: the window, the input, the settings, the keys' and the mouse's state), reached through one function (`Running`); every other function is given what it works on |
| I.4 | interfaces precisely and strongly typed | `SwapInterval` (at once, vertical sync, adaptive sync) in place of the numbers 0, 1 and -1; `Image` and `PixelPoint` in place of a span and four integers; the mouse's buttons named (`kLeftButton`...); the engine part turns `r_displayState` and `r_swapInterval` into the layer's enumerations in one function each |
| I.23, I.24 | few arguments, no neighbours of one type that can be swapped | `SetIcon(image)` and `SetPointerImage(image, hotSpot)`, which took a width, a height and two more integers in a row |
| I.5 | preconditions stated | the window a `Window` is made from and the one an `Input` is made from are asserted; the comments say what must outlive what |
| E.27 | code that cannot throw uses error codes systematically | everything that can fail returns `bool` or `std::expected`, marked `[[nodiscard]]`: `Present`, `SetIcon`, `SetPointerImage`, `Minimize`, `SetMouseMode` and `SetTextInput` returned nothing; the engine part reads each result and tells a refusal once, when it starts |
| F.20, F.21 | results returned, not written through parameters | `FindMode` returns an optional mode in place of a `bool` and an out parameter; the screen of a setting is an optional, where 0 meant "no screen" (also GUIDELINES section 6, explicit state over sentinel values) |
| F.2, F.3 | one operation per function, short functions | `Input::Poll` and `Window::Create`, `Window::Apply` split into one function per event kind and per display state |
| F.25, SL.str | a C string's type says what it is | the layer's own functions take `std::string`; `const char*` stays at the engine part's functions, which the renderer calls |
| C.4 | a member only when it needs the representation | `Screens()` is a free function, no longer a static member of `Window` |
| Con.2, Con.3 | `const` by default, never a way around it | `Window::Handle()` is not `const` and `Input` takes a `Window&`: a `const` window no longer hands out what changes it |
| Enum.7 | an underlying type only when necessary | the four enumerations lose their `std::uint8_t` (it saved no byte in the events, measured by their layout); clang-tidy's `performance-enum-size`, which asks the opposite, is off in the layer's `.clang-tidy` with that reason |
| ES.45 | no magic constant | the context's bits, the bytes of a pixel, the code points of the one-byte set and the count of auxiliary keys are named constants |
| ES.48, ES.49 | casts avoided | the renderer's import returns a function pointer (`Platform_GlFunction`), so the engine part no longer casts a function pointer to `void*` |
| ES.1 | the standard library before handwritten code | an image's rows copied with `std::ranges::copy` over spans, in place of `memcpy` and a moving pointer; the events dispatched by `std::visit` on a class with one call operator per kind, so that a kind added does not compile until it is handled |

The engine's boundary keeps the engine's shape: the `Platform_*` functions are free functions taking the engine's types, and the settings are `cvar_t*`. The joystick's setting, which the engine part defined as a global for the client to read, is the client's own again (`src/codemp/client/cl_input.cpp`).

Found by the results now read: SDL's dummy video driver refuses the icon and the minimizing, which the tests had been calling without knowing; the test says so, and `modernjk_test_platform real` checks both on a real driver. Found by the scripted session: a latched setting takes its new value only when it is registered again, so the engine part registers the display's settings again when the renderer starts, as the renderer does with its own; without it a `vid_restart` kept the old size.

So that it stays so: the layer's `.clang-tidy` makes every finding an error for every file under `src/shared/platform/`, the engine part included (`WarningsAsErrors: '*'`), in the clang-tidy preset.

Checked on the development machine (llvm-mingw's clang): clang-tidy at zero on the four source files, `engine/engine.cpp` included; the presets Release, Debug and AddressSanitizer without a warning, the five `modernjk` tests passed; `modernjk_test_platform real` passed (the icon and the minimizing with it); the scripted session again, with `r_windowWidth 1024`, `r_windowHeight 576` and `vid_restart` in it: the same window at the new size, the screenshot 1024 by 576. Not checked: the keys and the mouse by hand; GCC 15, Clang 23 on Linux and MSVC on this code.

To do at the end of the step (asked the same day): the same pass on the sound's engine part, `src/shared/sound/engine/engine.cpp`, written before this was asked and never run through clang-tidy either.

### 14.6 The journal removed (25 November 2026)

The VMs' Release builds of section 14.5's code first: GCC 15, Clang 23 and MSVC without an error nor a warning, `ctest` 16 of 16 on Linux.

Then decision 1 of section 14.3: the journal is gone from the three games.

- The multiplayer's engine (`src/codemp/qcommon/common.cpp`, `qcommon.h`): the setting `journal` (0, 1 to record, 2 to replay), `Com_InitJournaling`, the two files `journal.dat` and `journaldata.dat`, and the record and the replay of the system's events in `Com_GetRealEvent`, which now only reads the system's queue.
- The file system (`src/shared/qcommon/files.cpp`): `FS_ReadFile` no longer writes each configuration file read into the journal nor reads it back from there; the copy of a file no longer skips the journal's two files.
- The single player's engine (`src/code/qcommon/`), which had no such setting: its two unused file handles and the stand-in the file system needed for it.

Kept as it was that day: on a server that checks its clients' archives, a `.dat` file was still read from a folder (it was allowed "for journal files"). Decided on 26 November 2026: the exception goes with the journal (`FS_FOpenFileRead`, `src/shared/qcommon/files.cpp`). On such a server a `.dat` file outside the archives (`ext_data/dms.dat`, the list of the dynamic music; the Thai font's two tables) is no longer read in place of the archives' own, and reading one elsewhere marks the client as the other loose data do; the client's own `servercache.dat` is read by another function and is not concerned.

Checked on the development machine (llvm-mingw's clang): the presets Release, Debug and AddressSanitizer build every target without a warning, the five `modernjk` tests pass, the scripted session of section 14.5 again (the configuration files read, a map, a bot, the size changed by `vid_restart`, a clean quit). Not checked: GCC 15, Clang 23 on Linux and MSVC; the scripted scenarios, run with the part's validations.

### 14.7 Sub-step 1 tried in game (26 November 2026)

The MSVC build in the game's folder, tried by hand in multiplayer: the binds, the mouse and its wheel, the key under Escape alone and with Shift, the chat with accents, the keypad, alt-tab, and a size changed by `vid_restart`: everything works.

Seen there, with borderless fullscreen (`r_displayState 1`): at each alt-tab the screen goes black for a moment, and after several in a row the lagometer shows hitches when the game comes back. Not diagnosed. What is known: no frame cap applies without the focus (`com_maxfpsUnfocused` is 0), and SDL does not minimize a borderless window that loses the focus (`SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS` is "auto": exclusive fullscreen only). The suspect is the one decision P4 names: how Windows presents a borderless OpenGL window, directly or through the compositor, and its switch between the two when the window loses and gains the foreground. To measure with the presentation tool of point 6, against the build of the commit before this work (whether the old layer did the same in the same state is not known), and to fix there.

### 14.8 Sub-step 2: the keys' times in nanoseconds, the mouse in floats (26 November 2026)

Read first: a bind's command carries its key and its time as text (`+forward 87 <time>`, `CL_ParseBinding`, `src/codemp/client/cl_keys.cpp`), which `IN_KeyDown` and `IN_KeyUp` read back; `CL_KeyState` gives the share of a span a key was held: the time between two reads of the input for the keys that turn the view, the time since the last command for the keys that move (`src/codemp/client/cl_input.cpp`). All of it was in milliseconds, on the frame's clock.

Written:

- A key's time is the layer's, in nanoseconds, from the event to the share: `CL_KeyEvent(key, down, int64_t time)`, the bind's command carrying the 64-bit number, `kbutton_t` keeping when the key went down and how long it was held in nanoseconds, `CL_KeyState` dividing by a span in nanoseconds. The spans are measured on the same clock: `Platform_Frame` notes the time right after it read the events (`Platform_InputNanoseconds`: no event is later), and the client takes it where it took the frame's time. The spans keep their bounds, 1 ms and 200 ms. The frame's time in milliseconds stays for everything else.
- The mouse's motion reaches the view as the device gives it: `CL_MouseEvent(float dx, float dy)`, the client's two sums of motion in floats; nothing is rounded before the view angles. What takes whole counts (the UI's and the cgame's pointer, the cinematics' cursor, the map view) gets them from the client, the fraction carried to the next motion; the carry left the engine part.
- `Com_EventLoop` no longer has cases for the keys, the characters, the mouse and the joystick's axes, which the system's queue no longer carries in the multiplayer; the engine part gained `Platform_Nanoseconds` (the console's own Escape) and `Platform_InputNanoseconds`.

What a player gets: a movement key tapped for 3.2 ms within a tick of 10 ms moves for 0.32 of the tick; before, its press and its release each fell on a whole millisecond, so the same tap counted for 0.3 or 0.4 depending on where it fell. A mouse that reports fractions of a count (a system that scales the motion, a high-resolution device) turns the view by those fractions.

Checked on the development machine (llvm-mingw's clang): the presets Release, Debug and AddressSanitizer build every target without a warning, the five `modernjk` tests pass, clang-tidy at zero on the engine part, a scripted session started, joined the game and quit cleanly. Not checked: the feel by hand (a scripted `+forward` does not move the player in that session, with this build nor with the one before it, so the script proves nothing there); the share itself against events injected at known times, which is one of point 6's measures; GCC 15, Clang 23 on Linux and MSVC.

### 14.9 Sub-step 5, taken before sub-step 3: the system's pointer in the menus (26 November 2026)

The VMs' Release builds of sections 14.6 and 14.8 first, on 26 November 2026: GCC 15, Clang 23 and MSVC without an error nor a warning, `ctest` 16 of 16 on Linux; sub-step 2 tried by hand in multiplayer, everything works.

Asked the same day: in a window the mouse is held by the game from its start, and only alt-tab or the Windows key gives it back; so this sub-step comes before the display's settings (sub-steps 3 and 4 after it; they do not depend on it).

A fact corrected: the menus' pointer is the shader `cursor` their files name, whose image is `gfx/menus/cursor1` (`assets1.pk3`); `menu/art/3_cursor2`, which sections 14.3 and its decision 3 name after the UI's own registration, is in no archive of the game. The size decided stands: 40 of the menus' 640 by 480.

Written:

- The mouse turns the view only in game: with the focus, connected and active, and while nothing takes the pointer (a menu of the UI or of the cgame, the console, the client's own menu). Everywhere else it is the system's pointer, free to leave the window: the main menu, a level loading, the console in every display state. While the chat's line is open the mouse still turns the view, as before.
- The layer: `Window::SetPointerImage(image, shown, hotSpot)` scales the image to the size it is shown at (`SDL_ScaleSurface`, linear); `Input` hands over the pointer's place once when the mouse is given to it, before it moves, so that a menu that opens knows where the pointer is.
- The engine part: `Platform_SetPointerImage`; the pointer's events go to the client's new `CL_PointerEvent(x, y)`, in pixels.
- The client: the pointer's image set when the renderer starts, so at the program's start and at each `vid_restart` (a new size gives a new image), from the renderer's new export `ext.LoadImagePixels` (the pixels of an image by its name); `CL_PointerEvent` gives the place to the client's own menu, the UI or the cgame; `CL_MouseEvent` keeps the view and the map view only; the client's own cursor and its image are no longer drawn.
- The modules' interface: `MouseEvent(x, y)` of the UI and of the cgame takes the pointer's place in the window's pixels, where the UI's took a motion; each turns it into its 640 by 480. The UI no longer draws its pointer.

What a player gets: in the menus the pointer is the system's, with the game's image at the size it had; it moves at the desktop's speed, also while a level loads or at a low frame rate, and leaves a window without alt-tab. At the game's start in a window the mouse is no longer taken nor put at the window's centre.

Known limits: the menus still place the pointer in whole units of their 640 by 480 (3 pixels on a screen of 1,920), as before, so an item's edge can answer up to a pixel and a half from where the pointer's image is; in floats it would be exact, at the cost of the menus' shared code in the three games (79 places in the multiplayer's). The hot spot is the image's top left corner, where the menus drew it from.

Checked on the development machine (llvm-mingw's clang): the presets Release, Debug and AddressSanitizer build every target without a warning, the five `modernjk` tests pass (the pointer's image scaled, its place handed over once when the mouse is given back), `modernjk_test_platform real` passes, clang-tidy at zero on the layer and its engine part; a scripted session: the image found and taken by the system without a warning, the main menu's screenshot without a drawn pointer, a map loaded, a clean quit. Not checked: the pointer by hand (a screenshot does not show the system's pointer); GCC 15, Clang 23 on Linux and MSVC.

Decided on 26 November 2026, after the pointer was tried by hand in multiplayer (everything works; the VMs' Release builds of this sub-step: GCC 15, Clang 23 and MSVC without a warning, `ctest` 16 of 16): where the menus' pointer may go on a machine with several screens.

| # | question | decided | not taken |
|---|---|---|---|
| 1 | Borderless fullscreen | free: the pointer goes to another screen without alt-tab, as in most recent games; a click there takes the game's focus, and the game stays shown | kept on the game's screen |
| 2 | Exclusive fullscreen | kept on the game's screen (`Input::SetPointerConfined`, SDL's mouse grab of the window): a click on another screen would take the focus, and SDL then minimizes the game and puts the screen's mode back | free |

In a window the pointer is free, which is what this sub-step was asked for.

### 14.10 Sub-step 3: the display's commands, the old settings gone (26 November 2026)

Written, in the multiplayer's client and renderer:

- `modelist` lists each screen by the number `r_screen` takes, with its desktop's mode and the modes exclusive fullscreen can set, each written as `r_exclusiveMode` takes it (`1920x1080@240`); `minimize` minimizes the window. Both are the engine part's.
- `gfxinfo` prints the display as it is (`MODE: 1920 x 1080 fullscreen hz:240`), without the old mode's number; the refresh rate is the exclusive mode's when the setting names one, the desktop's otherwise.
- Alt+Enter goes between a window and borderless fullscreen (`r_displayState` 0 and 1) and restarts the video: the same window, the renderer started again at the new size. It set `r_fullscreen`, which nothing reads any more.
- Gone: the console key's three settings (`cl_consoleKeys`, `cl_consoleUseScanCode`, `cl_consoleShiftRequirement`), the key being the one under Escape since sub-step 1; in the renderer, the declarations of ten settings it never defined (`r_mode`, `r_fullscreen`, `r_noborder`, `r_centerWindow`, `r_displayRefresh`, `r_ignorehwgamma`, `r_colorbits`, `r_depthbits`, `r_stencilbits`, `r_stereo`) and of `R_GetModeInfo`. The settings themselves (with `r_customwidth`, `r_customheight`, `r_allowSoftwareGL`, `r_availableModes`, `r_sdlDriver`, `in_mouse`, `in_nograb`, `in_keyboardDebug`, and the commands `in_restart` and the old `modelist`) lived in `src/shared/sdl/`, which the multiplayer's client no longer compiles.

Left for later, each in its place: the dedicated server's stand-in renderer (`src/codemp/rd-dedicated/tr_init.cpp`) still registers the old names and its own `modelist`, a copy of the renderer's settings that the server never reads (with the dead code of plan step 16); the UI's video rows still write `ui_r_mode`, `ui_r_fullscreen` and `ui_r_colorbits` (point 5, the menus).

Checked on the development machine (llvm-mingw's clang): the presets Release, Debug and AddressSanitizer build every target without a warning, the five `modernjk` tests pass, clang-tidy at zero on the engine part; a scripted session: `modelist` lists the machine's two screens and their modes (439 lines), `gfxinfo` prints the window's line, then `r_displayState 2` with `r_exclusiveMode 1920x1080@240` and `vid_restart`: the same window in exclusive fullscreen, `MODE: 1920 x 1080 fullscreen hz:240`. Not checked: Alt+Enter and `minimize` by hand; GCC 15, Clang 23 on Linux and MSVC.

### 14.11 Sub-step 4: a character event for the multiplayer's modules (26 November 2026)

The VMs' Release builds of sections 14.9 and 14.10 first: GCC 15, Clang 23 and MSVC without a warning; Alt+Enter, the pointer kept on the game's screen in exclusive fullscreen and free in borderless, `modelist` and `minimize` tried by hand: everything works.

Written (decided 17 November 2026, plan step 18):

- The modules' interface: the UI's and the cgame's `KeyEvent(fakeAscii_t key, bool down)` takes a key only, and a new `CharEvent(int character)` takes a typed character in the engine's one-byte set; `UI_API_VERSION` 7 and `CGAME_API_VERSION` 6, so that a module of before is refused. The client's `CL_CharEvent` calls `UIVM_CharEvent` or `CGVM_CharEvent`; nothing in the multiplayer adds `K_CHAR_FLAG` to a key any more (the constant stays in `src/shared/qcommon/q_limits.h` for the single player's engine until it is on the layer).
- The menus' shared code (`src/codemp/ui/ui_shared.cpp`, compiled by the UI and the cgame): the field being edited has `Item_TextField_HandleChar` for the characters (the paste with Ctrl-V, Backspace as Ctrl-H, the digits of a numeric field, insert and overstrike) and `Item_TextField_HandleKey(item, fakeAscii_t key)` for the keys, cut out of the one function that told them apart by the flag; `Menu_HandleChar` gives a character to the field being edited; `Menu_HandleKey` reads a key, no longer an optional one; `KeyEvent_Key` is gone from `keycodes.h`.

Checked on the development machine (llvm-mingw's clang): the presets Release, Debug and AddressSanitizer build every target without a warning, the five `modernjk` tests pass. Not checked: a field typed in by hand (a name, a server's address, a number, a paste, Backspace); GCC 15, Clang 23 on Linux and MSVC.

### 14.12 Escape while a key is bound or a field edited (26 November 2026)

The VMs' Release builds of section 14.11 first: GCC 15, Clang 23 and MSVC without a warning; the menus' fields, the paste and the binds tried by hand: everything works (a numeric field not tried, none being known).

Seen while trying it, the original game's: in a menu opened over the game, Escape pressed while a row waits for the key to bind closes the menu, and the bind is lost; from the main menu the same key cancels the bind and the menu stays. Read: `UI_KeyEvent` (`src/codemp/ui/ui_main.cpp`) closes every menu on Escape when no full-screen menu shows, before the row sees the key; the row's own code for Escape (`Item_Bind_HandleKey`) is reached from the main menu only. A field being edited is in the same case.

Fixed the same day (asked): `Menu_TakesEscape` (`src/codemp/ui/ui_shared.cpp`) tells whether a row waits for a key or a field is being edited, and `UI_KeyEvent` then gives Escape to the menu's own handling.

| | before | now |
|---|---|---|
| Escape in a menu over the game, while a row waits for a key | the menu closes | the bind is cancelled, the menu stays; Escape again closes it |
| Escape in a menu over the game, while a field is edited | the menu closes | the editing ends, the menu stays; Escape again closes it |

Checked: the clang presets Release, Debug and AddressSanitizer without a warning. Not checked: by hand; the VMs.

To do with the single player's engines (point 4): its menus have the same code (`src/code/ui/ui_main.cpp`: every menu closed on Escape unless one is full screen or ignores Escape), so the same case is to try there and to fix the same way if it shows.

### 14.13 Sub-step 6: the keys the engine's table has no code for (26 November 2026)

Read first: the engine's key codes follow its one-byte character set only in part. From 0x80 to 0xBF the table holds the engine's own keys (the second Shift, Ctrl and Alt, F5 to F12, the mouse's buttons and wheel, the arrows) between a few characters, and sixteen codes are free (`A_UNDEFINED_7` to `A_UNDEFINED_22`, 0xAF to 0xBE). So a key whose character is `§`, `°`, `²`, `´` or `º`, common on European layouts, had no code, nor has a function key beyond F12.

Written (P1: every key of every layout can be bound):

- Eight more characters of the one-byte set go to the codes the table already has for them: `¡ ¢ £ ¥ © ® ¿` and `ß`.
- Every other key takes one of the sixteen free codes when it is first pressed or first named in a bind (`Key_DynamicKey`, `src/codemp/client/cl_keys.cpp`), with a name: the character the layout gives the key as `U+` and its Unicode number (`U+00B4`), the same on every system and drawn by the game's font; for a key without a character, the system's name in capitals with underscores (`F13`). A bind is written and read back by that name (`bind "F13" "+attack"`); the controls menu shows it. A name read in a bind is taken only when it is one (`Platform_IsKeyName`, on the layer's new `KeyFromName`): `bind FOOBAR x` is still refused.
- A code goes back to the next key that needs one when its key has no bind and is not held; with sixteen such keys bound, a seventeenth is refused with a message.

Limits: sixteen such keys bound at once; the name of a key without a character is the system's, so a bind of the Windows key written on Windows (`LEFT_WINDOWS`) is not the name another system gives it.

Left with the single player's engines (point 4): the key codes in one definition shared by the trees (section 9.3), which touches both.

Checked on the development machine (llvm-mingw's clang): the presets Release, Debug and AddressSanitizer build every target without a warning, the five `modernjk` tests pass (the layer's `KeyFromName` among them), clang-tidy at zero on the layer and its engine part; a scripted session: `bind F13` and `bind U+00B4` taken and written by name in the configuration, `bind FOOBAR` and `bind U+0041` (a plain letter) refused. The VMs' Release builds, the same day: GCC 15, Clang 23 and MSVC without a warning, `ctest` 16 of 16 on Linux; Escape while a key is bound or a field edited (section 14.12) and the usual binds tried by hand in multiplayer: everything works.

Not checked: such a key pressed by hand and bound from the controls menu (left out on 26 November 2026: the keyboard at hand has none worth the try; the scripted session covers the bind's writing and reading).

Not checked yet: the Wayland driver itself (`SDL_VIDEO_DRIVER=wayland`). Left open, for the checks under Wayland: `SDL_HINT_VIDEO_DOUBLE_BUFFER` (two buffers instead of three under Wayland, "a whole frame of latency" less by SDL's documentation, at the cost of waiting for the vertical sync after each swap): not set, to measure before it is.

### 14.14 The single player's engines on the layer (26 November 2026)

Point 4 of section 11. The reference for the measures first: the worktree `E:\Git\jka\ModernJK_sdl2ref`, at the commit before the multiplayer's part (`fix(sound): step 21, part 7`), built with the clang Release preset.

Not in sub-steps as the multiplayer was: the engine part (`src/shared/platform/engine/engine.cpp`) is compiled against each engine's headers and asks for the whole of the client's side at once (a key's time in nanoseconds, the mouse in floats, the pointer's place, the keys without a code), so the single player's engine, which builds Jedi Academy and Jedi Outcast, was moved in one piece.

Written, in `src/code/` as sections 14.4 to 14.13 did in `src/codemp/`:

- The build: `sp_engine_common` links `modernjk_sys_platform` in place of `modernjk_sys_client`; no `SDL2.dll` is copied nor installed beside the two executables.
- The engine: `Com_Frame` calls `Platform_Frame` where it called `IN_Frame`, and `Com_EventLoop` no longer has cases for the keys, the characters, the mouse and the joystick's axes.
- The client: `CL_KeyEvent(key, down, int64_t)` with the time carried by the bind's command, `kbutton_t` in nanoseconds and `CL_KeyState` dividing by the frame's time in nanoseconds (the single player makes one command per frame, so one span serves the view and the movement); `CL_MouseEvent(float, float)` and the sums of motion in floats; `CL_PointerEvent`; `Key_DynamicKey` and the names of the keys without a code in `Key_StringToKeynum`; Alt+Enter between a window and borderless fullscreen; the console key's three settings gone; the joystick's setting the client's own; the window opened in `CL_InitRef` and the renderer's four imports; the pointer's image set when the renderer starts.
- The pointer: both games' menus name the shader `cursor`, whose image is `gfx/menus/cursor1` in each game's archives (read there, as section 14.9 did); the single player's UI drew it 48 by 48 of its 640 by 480 (the multiplayer's 40), which is the size given to the system. The UI no longer draws it, and `_UI_MouseEvent` takes the pointer's place in pixels.
- The UI, which is in the engine: `_UI_KeyEvent(fakeAscii_t, bool)` and a new `_UI_CharEvent(int)`; the field's handler cut into `Item_TextField_HandleChar` and `Item_TextField_HandleKey`, `Menu_HandleChar`, as section 14.11; `K_CHAR_FLAG` and `KeyEvent_Key` removed, no game using them any more.
- Escape while a row waits for a key or a field is edited (section 14.12): the same code was there, so the same fix, `Menu_TakesEscape`.
- The renderer (`src/code/rd-vanilla`, `src/code/rd-common/tr_public.h`): `Platform_Display` and `Platform_Present` in place of `WIN_Init`, `WIN_SetGamma`, `WIN_Present` and `WIN_Shutdown`, `gfxinfo`'s line, the stale declarations gone, as sections 14.4 and 14.10.
- Who has the mouse, a change of the engine part: its own test read the multiplayer's key catchers (`KEYCATCH_CGAME`, which the single player has not), so each client now answers `CL_MouseTurnsView`, and the engine part adds the focus. The multiplayer's answer is the test as it was. The single player's: in game while neither the menus nor the console take the keys, and while a video plays (`CA_CINEMATIC` or an in-game one), whatever the menus' state: a video started from the menus leaves the UI's key catcher set and hides the UI's pointer (`UI_Cursor_Show(false)`), so no pointer is shown over it; the mouse is held meanwhile, as it was.

The view that did not turn (section 8; "Known issues" of the progress page): found and fixed. A scripted `+left` reproduces it (`viewpos` before and after: the same yaw), and a trace showed the view angles not being numbers from the level's first frame, without `cl_overrideAngles` being set: section 8's suspicion was wrong. The cause: `CL_MouseMove` computes the mouse's rate as `SQRTFAST(mx * mx + my * my)`, which is `x * Q_rsqrtf(x)`; with no motion that is 0 times the inverse root of 0. Quake's approximation of the inverse root gave a large number and the product was 0; since the inverse root is exact (12 November 2026, plan step 14) it is infinite and the product is not a number, which the test for "no motion" placed after it (`!mx && !my`) lets through into `cl.viewangles`, where every later addition stays not a number. The multiplayer's `CL_MouseMove` returns before the rate when the mouse did not move; the single player's now does the same. The reference build (SDL 2) has the defect, so it was not the platform code's.

| | before | now |
|---|---|---|
| In a level of either game, the mouse moved or the key bound to `+left` held | the view stays where the level started it (`viewpos`: yaw 0, then 0) | the view turns (`viewpos`: yaw 165, then 218 after 60 frames of `+left`) |

Checked on the development machine (llvm-mingw's clang): the presets Release, Debug and AddressSanitizer build every target without a warning, the five `modernjk` tests pass in each, clang-tidy at zero on the layer's four files (the engine part once per engine: three times), clang-format clean. Scripted silent sessions of both games on the Release build: the layer's line in the log, a level loaded (`mp/duel1` and `yavin1b` in Jedi Academy, `ffa_bespin` and `artus_mine` in Jedi Outcast), the view turned by `+left` and the player moved by `+forward`, `bind F13` and `bind U+00B4` taken and `bind FOOBAR` refused, a size changed by `vid_restart` on the same window (`MODE: 1024 x 576 windowed`), a screenshot of the level at that size, a clean quit. The mouse tried by hand on these sessions, in both games: the view turns.

Decided on 26 November 2026: while a video plays the mouse is held and no pointer is shown, and the single player's pointer keeps its 48 units.

Seen on the way, the reference build's too, and fixed the same day (asked): at a `vid_restart` the single player's renderer printed five warnings, `cvar 'r_picmip' must be numeric, setting to 0` and the same for `r_subdivisions`, `r_znear`, `r_primitives` and `r_screenshotJpegQuality`, each setting put back to its default. The cause is `Q_isanumber` (`src/shared/qcommon/q_string.cpp`), which the settings with a range are checked by: it read `errno` after the conversion without clearing it before, and the conversion sets `errno` only when it fails; an error left by any earlier call made every number "not numeric". OpenJK's function does the same with the C library's `strtod`. Reproduced without the view's defect, in the console: `r_primitives 1e999` (refused, rightly), then `r_primitives 1`. The warnings at `vid_restart` were seen only in the sessions that had the view's defect (this build before its fix, the reference build) and in none since, with the same commands; which call left the error there was not looked for.

| | before | now |
|---|---|---|
| In the console, `r_primitives 1e999`, then `r_primitives 1` | the second is refused too: `cvar 'r_primitives' must be numeric, setting to 0`; so is `r_subdivisions 8` after it, and every setting with a range, each put back to its default | the first is refused, the second taken (`r_primitives = "1"`) |

Removed with it: a declaration of `in_joystick` that nothing used, in the single player's game (`src/code/game/g_vehicles.cpp`).

Checked after these two fixes and the `.dat` exception's removal (section 14.6): the clang presets Release, Debug and AddressSanitizer without a warning, the five `modernjk` tests passed in each, the scripted scenarios `golden: SAME` (`scenarios.sh step15sp`, 2 min 43 s).

The VMs' Release builds and the two games by hand, the same day: GCC 15, Clang 23 and MSVC built; in both games the pointer in the menus, no pointer over the intro's video, the view and the keys in game, a bind and Escape while a row waits for it, a field and Escape while it is edited, the console key, a saved game loaded: all passed. Two things came out of it:

- Alt+Enter answered the left Alt only. The right Shift, Ctrl and Alt are keys of their own on this branch (8 November 2026), and the single player's shortcuts still read the left ones alone (sixteen tests in `src/code/client/cl_keys.cpp`: Alt+Enter, Shift+Escape for the console, Shift+Insert, the console's history with Shift and the wheel, its scrolling and its ends with Ctrl, the fields' Ctrl shortcuts). Fixed the same day as the multiplayer has it: `Key_ShiftDown`, `Key_CtrlDown` and `Key_AltDown` take either side.
- Shift with the key under Escape typed nothing, in the multiplayer either. It is the layout's: on the French AZERTY layout that key types `²` alone and nothing with Shift (asked of Windows, `ToUnicodeEx` on scancode 0x29 of layout 040C: one character without Shift, none with it). The test asked for was wrong; on a layout whose key has a shifted character (`~` on the US one) that character is typed.

| | before | now |
|---|---|---|
| In either single-player game, the right Alt held and Enter pressed | nothing | the display goes between a window and borderless fullscreen, as with the left Alt |
| The console open, the right Ctrl held and Page Up pressed | one line up | five lines up, as with the left Ctrl |

Seen there too, in Jedi Outcast, not this step's (the reference build does it too), and fixed the same day (asked): the picture stored in a saved game was upside down. It is the picture the loading screen shows when a saved game is loaded, and again at each restart of the video after it, which is how Alt+Enter showed it, and the one the load menu shows beside a save. The screen's copy has its rows from the top since upstream OpenJK's fix of the level transition screen (`RE_GetScreenShot`, March 2025), as the level's picture of an autosave has, and `SG_WriteScreenshot` (`src/code/server/sv_savegame.cpp`) still asked the JPEG writer for rows from the bottom: one argument. Jedi Academy stores no picture.

| | before | now |
|---|---|---|
| Jedi Outcast, a game saved then loaded | the loading screen's picture is upside down, and the save's picture in the load menu | upright, for a save made from now on (a save made before keeps its picture) |

Checked by reading the picture out of the saves' files, a save made by hand and an autosave, before and after: upside down, then upright.

Checked after the modifiers' and the picture's fixes: the clang presets Release, Debug and AddressSanitizer without a warning, the five `modernjk` tests passed in each. Not checked: the right Alt, Ctrl and Shift by hand; GCC 15, Clang 23 on Linux and MSVC on these last two changes; the loading screen and the load menu by hand.

### 14.15 SDL 2 and the old layer removed, the key codes in one definition (26 November 2026)

The end of point 4 of section 11, once both single-player games were tried on the layer.

Removed:

- `libs/SDL2` (the prebuilt DLL, its import library and its headers, 4.4 MB) and its targets in `Dependencies.cmake` (`SDL2::SDL2`, `SDL2::SDL2main`, the system's SDL2 on Linux); `.gitignore`'s line for its `bin` folder.
- `src/shared/sdl/` (`sdl_window.cpp`, `sdl_input.cpp`); its icon, which the engine part includes, is `src/shared/platform/engine/window_icon.h`.
- The target `modernjk_sys_client` and the function `modernjk_copy_runtime_dlls` (`src/shared/CMakeLists.txt`); every engine that opens a window links `modernjk_sys_platform`.
- The definition `MODERNJK_PLATFORM` and the branches it chose in `src/shared/sys/` (`sys_main.cpp`, `sys_loadlib.h`, `sys_win32.cpp`, `sys_unix.cpp`): what differs now is the dedicated server (`DEDICATED`), which keeps its own clock and links no SDL; the modules' libraries are loaded by the system's own call everywhere.
- What only the old layer used: the declarations of `IN_Init`, `IN_Frame`, `IN_Shutdown` and `IN_Restart` and the dedicated server's empty ones (`src/codemp/null/null_input.cpp`), `WIN_Init` and its kin with their types (`window_t`, `windowDesc_t`, `graphicsApi_t`, `glProfile_t`, `glContextFlags_t`), the system queue's events for the keys, the characters, the mouse and the joystick's axes (`SE_KEY`, `SE_CHAR`, `SE_MOUSE`, `SE_JOYSTICK_AXIS`: the queue carries the console's lines only), the multiplayer renderer's unused `window_t` field.
- clang-tidy's `-DSDL_DISABLE_ANALYZE_MACROS` (`cmake/StaticAnalyzers.cmake`), the SDL 2 exception of GUIDELINES section 9, `libsdl2-dev` and SDL 2's row in the root README.

The key codes: `fakeAscii_t` is defined once, in `src/shared/qcommon/q_keycodes.h`, in place of `src/codemp/ui/keycodes.h` and `src/code/client/keycodes.h`, which held the same enumerators (GUIDELINES section 6; section 9.3).

Checked on the development machine (llvm-mingw's clang): the presets Release, Debug and AddressSanitizer configure again and build every target without a warning, the five `modernjk` tests pass in each, clang-tidy at zero on the layer's four files, clang-format clean; a scripted session of each of the three clients (the multiplayer, Jedi Academy, Jedi Outcast) loads a level and quits cleanly; the scripted scenarios `golden: SAME`.

The full VM validations, the same day (logs `build/validate-linux.log`, `build/validate-windows.log`): `validate.sh` ALL OK (GCC 15 and Clang 23: Release, Debug, AddressSanitizer, the server's preset, the Debug build with link-time optimization, the formatting) and `validate.bat` ALL OK (MSVC: Release, Debug, AddressSanitizer), `ctest` 16 of 16 in every preset that runs it, no compiler warning.

Outside the repository: Jedi Outcast's `update_from_build.bat`, in its game folder, no longer copies `SDL2.dll`; the game folders still hold the `SDL2.dll` of earlier builds, which nothing loads.

### 14.16 Point 5: the display's rows in the setup menus (26 November 2026)

Point 5 of section 11. What the six setup menus showed is in section 9.1, the UI's code behind them in section 9.2; since sections 14.10 and 14.15 their video mode, colour depth and full screen rows wrote settings no engine reads.

Read before the decisions, on the development machine (the `modelist` command, a scripted session): two screens 1920 by 1080, at 360 Hz and 319.98 Hz, each with nineteen sizes from 1920 by 1080 to 640 by 480, and eighteen refresh rates at 1920 by 1080, from 360 down to 23.98 (119.88 and 120 apart).

The decisions, put all at once on 26 November 2026:

| # | question | decided | not taken |
|---|---|---|---|
| 1 | The rows that do not apply to the state chosen (a window's size in fullscreen, a rate in a window) | one resolution row and one refresh rate row, always there, their content following the state: in a window the window's size, the rate greyed with the desktop's; in borderless fullscreen both greyed with the desktop's mode; in exclusive fullscreen the mode's size and rate. Four rows always in the same place | rows hidden when they do not apply (the rows below moving, or a gap); five rows, a window's size and exclusive fullscreen's resolution apart (one more row, and the multiplayer's in-game menu's "Apply" moved) |
| 2 | The sizes offered for a window | the screen's own sizes that fit in its usable area, without the taskbar (`SDL_GetDisplayUsableBounds`), every shape: in a window nothing is stretched, the picture is drawn at the window's size | the same of the desktop's shape only, as Unreal Engine's `GetConvenientWindowedResolutions` ("smaller than the desktop, with a matching aspect ratio", the comment of its `GameEngine.cpp`); a fixed list of common sizes; free entry |
| 3 | Exclusive fullscreen with `r_exclusiveMode` empty (the screen's present mode, the default) | the resolution list starts with "Desktop", which writes it empty and greys the rate: the menu can go back to the default, which follows a change of the desktop's mode | no such entry, any change writing a mode, the default back from the console only |
| 4 | The vertical sync's row, Off or On, while `r_swapInterval` takes -1, the adaptive sync, since part 1 | a third value, Adaptive: an image late for the screen's refresh is shown at once, with a tear, instead of a whole refresh later; on a driver without it, the plain vertical sync while the menu says Adaptive | Off and On only |
| 5 | The multisampling, `r_ext_multisample`, kept on 25 November 2026 but in no menu, read when the program starts until part 4 | its row with part 4, where it becomes the offscreen image's property, changed while running | a row now, applied at the next start |

Asked the same day, for part 4: the depth format of the offscreen image. The window's own request (24 bits of colour, 24 of depth, 8 of stencil) is what a window of OpenGL offers; the picture's precision comes from the offscreen image of decision 7 (section 1), and a depth in 32-bit floating point with a reversed range (`GL_DEPTH32F_STENCIL8`, OpenGL 3.0; 64 bits a pixel against 32) exists only for such an image. To put among part 4's decisions.

Written:

- `src/shared/platform/menus/` (new; README): what the screen, resolution and refresh rate rows show and the value a click on each gives, functions of their arguments only, without SDL; compiled by both engines and by the multiplayer's UI module (`modernjk_platform_menus` in `src/shared/CMakeLists.txt`). The reading of `r_exclusiveMode` and of `r_displayState`, which the engine part held, moved there, so that the engines and the menus read them one way.
- The layer: each `Screen` gives its usable area (`usable`, in pixels); the screens' event (`WindowChange::ScreensChanged`) comes too when a screen's desktop mode or usable area changes.
- The engine part: `Platform_Screens`, the list read when the window opens, at each start of the renderer and at the screens' event.
- The two UIs (`src/codemp/ui/ui_main.cpp`, `src/code/ui/ui_main.cpp`): the rows owner-drawn (`UI_DISPLAY_SCREEN`, `UI_DISPLAY_RESOLUTION`, `UI_DISPLAY_REFRESH`), as the sound device's row is, and their keys; `getvideosetup` and `updatevideosetup` copying the five display settings in place of `r_mode`, `r_fullscreen`, `r_colorbits`, `r_depthbits`; the quality presets without the display; `ui_r_refreshChoice`, which the menus read to grey the refresh rate row. The multiplayer's UI gets the screens through `uiImport_t::ext.Screens` (`UI_API_VERSION` 8).
- The six menus: the three old rows replaced by the four (Display a list of `ui_r_displayState`; Screen, Resolution, Refresh rate owner-drawn), the rows below moved down one place where the page has room (the multiplayer's and Jedi Academy's main setup menus and Jedi Academy's in-game one, by 14 units; Jedi Outcast's two, by 20), the empty place under the quality preset taken in the multiplayer's in-game menu, whose "Apply" sits right under its last row; a `disableColor` for the greyed rows; the vertical sync's Adaptive.
- The texts, in English, French, German and Spanish: `assets/jka/strings/<Language>/MODERNJK.str` (both Jedi Academy games), `assets/jk2/strip/MENUS4.sp` (Jedi Outcast, without the space before a colon in French, as its other texts). The state's values are short (Window, Borderless, Exclusive), to fit in the place a value has; the row's description gives their meaning.
- The tests: `test/platform/menus.cpp` (two screens written as SDL lists them, every list, text and click, forward and back), the usable area on the dummy driver, the screens' event for the two new causes and not for the present mode's change.

Seen on the way, not changed: the dedicated server's renderer (`src/codemp/rd-dedicated/tr_init.cpp`) still registers `r_mode`, `r_fullscreen`, `r_colorbits` and `r_depthbits`, which nothing reads. Seen while trying the menus by hand, the same day: the single player's text is wider than the multiplayer's on a wide screen (a menu's title about 430 pixels wide against 310 at 1920 by 1080), because only the multiplayer's renderer has `r_aspectCorrectFonts`, set to 1 in the configuration it was tried with (0 by default); placed in plan step 25, whose one renderer makes the text's drawing one code for the three games, and step 26, the fonts, with one text system for the three games (decided 26 November 2026).

Checked on the development machine (llvm-mingw's clang): the presets Release, Debug and AddressSanitizer build every target without a warning, the five `modernjk` tests pass in each, clang-tidy at zero on the layer's files (`display.cpp` with them), clang-format clean; scripted sessions of the three clients load their menus without a warning.

The full VM validations, the same day (logs `build/validate-linux.log`, `build/validate-windows.log`): `validate.sh` ALL OK (GCC 15 and Clang 23: Release, Debug, AddressSanitizer, the server's preset, the Debug build with link-time optimization, the formatting) and `validate.bat` ALL OK (MSVC: Release, Debug, AddressSanitizer; its Release build failed once on the shared folder, `C1001` "In-page error", and passed when built again from clean), `ctest` 16 of 16 in every preset that runs it, no compiler warning. The rows tried by hand in the three games, from the main menu and in game, the same day: the display's state, a window's size, an exclusive mode and its rate, the second screen, the vertical sync's Adaptive, a quality preset leaving the display alone, the multiplayer's in-game menu: all passed.

### 14.17 Point 6: the checks and the measurements before and after (26 November 2026)

Point 6 of section 11, its list of measures. The reference is the worktree `E:\Git\jka\ModernJK_sdl2ref` (section 14.14), its clang Release build; HEAD's is the same preset here. Every client silent (`s_initsound 0`), the two builds alternated session by session.

The decisions, put all at once on 26 November 2026:

| # | question | decided | not taken |
|---|---|---|---|
| 1 | The tool for the presentation (section 10.4, decision 4) | PresentMon 2.6.0 (Intel, MIT licence, 21 September 2026; https://github.com/GameTechDev/PresentMon), its console program, outside the repository (its SHA-256 the release's, its signature Intel's): per image, the present mode and the time until it is shown; for OpenGL its README says the runtime is "Other", the mode exact and the times counted from the kernel's present, about 0.5 ms after the program's | the same downloaded by hand; the Windows Performance Toolkit already installed (GPUView), read by hand |
| 2 | The case "144 Hz with the vertical sync", on screens of 360 and 320 Hz where only the second has 144.23 Hz, in exclusive fullscreen, and the reference cannot choose its screen | 165 Hz in exclusive fullscreen on the primary screen, in both builds: the rate nearest to 144 both can set | 120 Hz; 144.23 Hz on the second screen with HEAD only |
| 3 | The share of a tick a key was held, against the real one | a temporary harness in both builds (lines marked `TMPDBG`, removed afterwards): the key bound to `+forward` pressed and released through SDL's queue at known times, the share each command gave it printed | computed from the code; left to the tests of the end of the step |
| 4 | The input's reading per frame, with a mouse at 1,000 and 8,000 reports a second | a temporary timer around the frame's reading in both builds, the mouse moved by hand | a samply profile |

The machine was quiet for measure A only; the others ran while the machine was in use (decided the same day: measured now, the times measured again on a quiet machine later). Their times are marked so.

**A. A demo played in real time** (a 30 s demo of two bots dueling on `mp/duel1`, recorded by HEAD; each session settled 6 s, measured 20 s; the process's CPU in % of one core; the images' times and present modes by PresentMon; two rounds, quiet machine):

| case | CPU, HEAD | CPU, reference | time between images, 99th percentile, HEAD / reference | present mode, both builds |
|---|---|---|---|---|
| borderless fullscreen, 100 images a second | 25.1, 25.6 % | 31.0, 32.6 % | 10.60 / 10.86 ms | Hardware: Legacy Flip |
| borderless fullscreen, 125 | 31.5, 33.8 % | 34.0, 34.9 % | 8.78 / 8.95 ms | Hardware: Legacy Flip |
| borderless fullscreen, 333 | 42.4, 41.9 % | 54.7, 55.6 % | 3.50 / 3.72 ms | Hardware: Legacy Flip |
| exclusive fullscreen, vertical sync at 165 Hz | 68.6, 57.9 % | 74.7, 74.3 % | 6.41 / 6.31 ms | Hardware: Legacy Flip |
| a window of 1280 by 720, 125 | 26.7, 29.7 % | 35.6, 33.5 % | 8.87 / 8.97 ms | Composed: Copy with GPU GDI |

No image left unshown in any session; the median time between images at 333 images a second 3.00 ms with HEAD, 3.06 with the reference, whose clock counts whole milliseconds; the memory the same (about 186 MB). Without a cap (`timedemo 1`, a window of 640 by 480): 982 and 989 images a second. What P4 asked (section 1): Windows shows the borderless OpenGL window directly, the program owning the screen ("Hardware: Legacy Flip", PresentMon's mode of exclusive fullscreen), 0.28 ms from the present to the screen, in both builds; a window goes through the compositor (2.3 ms). With the vertical sync and no cap on the frame rate, 17.5 ms from the present to the screen in both builds, about three images queued: the same before and after.

**B. The input margin** (`netstats` over 20 s, each client in a window on a dedicated server of HEAD on this machine with two bots on `mp/ffa3`; machine in use):

| images a second | HEAD: mean, lowest, after their tick | reference |
|---|---|---|
| 100 | 3.5 ms, 3 ms, 0 of 999 | 3.6 ms, 1 ms, 0 of 1,000 |
| 125 | 3.7 ms, 2 ms, 0 of 999 | 3.9 ms, 2 ms, 0 of 1,000 |
| 333 | 3.6 ms, 2 ms, 0 of 999 | 3.6 ms, 3 ms, 0 of 999 |

Seen there: at 100 images a second some sessions send every client packet without a base, 26.0 bytes per packet against 10.0 in others, with either build (measured again twice per build: HEAD twice, the reference once of two). The netcode's, not this step's: placed in plan step 13 (decided 26 November 2026).

**C. The share of a tick a key was held** (the harness of decision 3; the length counted is the sum of each command's share times its span; a level on a local server, a window; machine in use):

| real press | HEAD, 125 images a second | reference, 125 | HEAD, 333 | reference, 333 |
|---|---|---|---|---|
| 3.2 ms (three presses) | 3.200, 3.200, 3.200 ms | 6.0, 6.0, 6.0 ms | 3.198, 3.200, 3.200 ms | 4.0, 6.0, 5.0 ms |
| 5 ms | 5.000 | 6.0 | 5.000 | 7.0 |
| 7.5 ms | 7.499 | 10.0 | 7.499 | 9.0 |
| 12.5 ms | 12.499 | 14.0 | 12.499 | 14.0 |
| 25 ms | 25.000 | 30.0 | 24.999 | 25.0 |

HEAD counts the time the key was held, split between the ticks it spans; the reference counts the time between the two frames that read the press and the release, in whole milliseconds. Found while writing the harness: a bind's `+forward` goes through the command buffer, behind any `wait` there, so the presses are driven by the harness itself (`tmpseries`), and a level started by `devmap` from a configuration file at the program's start leaves the UI's key catcher set (the harness clears it).

**F. A restart of the video** (`CL_Vid_Restart_f`, a window of 1280 by 720, three restarts and two changes of size per session; machine in use): 340 to 410 ms with HEAD, 520 to 660 ms with the reference, which creates the window again.

**G. The start to the main menu** (from the process's start to "--- Common Initialization Complete ---" in its log, a window, three times each, median; machine in use): first 0.78 to 0.95 s with HEAD against 0.48 to 0.66 s with the reference, in the three games. Found with temporary timings: the first listing of the screens' modes (`SDL_GetFullscreenDisplayModes`) took 0.49 s, SDL enumerating the modes of both screens (some 440 each), then 35 us once SDL keeps them; it ran when the window opened, since part 1 (the refresh rate's computation for the renderer) and since point 5 (the menus' list). Decided on 26 November 2026: the modes read only when something needs them, the menus' display rows (`Platform_Screens`), `modelist` and an exclusive mode applied; the window's opening and the renderer read the screens without them (`platform::ScreenModes::Skipped`). The first opening of the video menu, or the first switch to exclusive fullscreen, then waits for them once. After the change:

| game | HEAD | reference |
|---|---|---|
| the multiplayer | 413 ms | 616 ms |
| Jedi Academy | 465 ms | 658 ms |
| Jedi Outcast | 321 ms | 525 ms |

**H. The memory and the sizes**: the process's memory at the main menu the same in both builds (the multiplayer 165 MB, Jedi Academy 130 MB, Jedi Outcast 97 MB). On disk: the multiplayer's client 8.61 MB against 5.79 MB and `SDL2.dll`'s 1.47 MB, 7.26 MB together; Jedi Academy's 7.15 MB against 5.93 MB; Jedi Outcast's 7.13 MB against 5.91 MB; the dedicated server 2.298 MB against 2.302 MB.

**I. A minimized window** (the demo played at 125 images a second, minimized by `minimize`; machine in use): 17.0 and 17.3 % of a core with HEAD, 18.4 and 17.6 % with the reference, both capped at 50 images a second (`com_maxfpsMinimized`). A first session gave HEAD 27.9 % at 125 images a second; not reproduced in two more nor at the main menu (12.1 and 9.1 % against 11.4 and 7.3 %): the window was restored meanwhile, not a defect.

**A window without the focus** (the demo at 125 images a second in a window; another window clicked by hand at once, Windows asked at the measure's start and end which window was in front): 34.2 % of a core with HEAD, 36.1 % with the reference, neither in front. Without a cap out of the focus (`com_maxfpsUnfocused` is 0 in both builds), about what a window in front takes.

**D. The input's reading per frame** (the harness of decision 4: the time from the frame's reading of the input to the end of `Com_EventLoop`, `Platform_Frame` or `IN_Frame` included, mean and maximum over 1,000 frames; a level on a local server, a window of 1280 by 720 at 125 images a second, the mouse still then moved by hand without a pause; the mouse at 1,000 then 8,000 reports a second):

| mouse | HEAD: still, moving | reference: still, moving |
|---|---|---|
| 1,000 reports a second | about 4 us; 3.7 to 4.6 us (max 75 us) | 7.4 us; 35 to 37 us (max 504 us) |
| 8,000 reports a second | 4.1 to 4.3 us; 5.3 to 5.4 us (max 20 us) | 5.1 to 5.3 us; 162 to 176 us (max 1.3 ms) |

The work is not gone: SDL 3 reads the raw mouse on a thread of its own (`WIN_RawInputThread`, section 10.3), which hands the frame the motion already gathered, where SDL 2 handled each of the mouse's messages in the frame's own loop; at 8,000 reports a second the reference's frame waited up to 1.3 ms for it.

**The black moment at alt-tab, measured** (PresentMon over 60 s, the client in borderless fullscreen on the primary screen at its main menu, alt-tabs by hand): in both builds the window is "Hardware: Legacy Flip" with the focus and "Composed: Copy with GPU GDI" without it, and the game's images stop at each switch for 52 to 67 ms with HEAD, 60 to 82 ms with the reference, the composed desktop showing meanwhile; on the screen, a black moment of about 10 ms at each alt-tab, the same in both builds. Not this step's: the reference does it the same.

**Read for the black moment at alt-tab** (section 14.7; 26 November 2026, the GPU of the development machine an NVIDIA GeForce RTX 5070 Ti): SDL's issue 12791 (https://github.com/libsdl-org/SDL/issues/12791, open, milestone 3.x) reports the same: on Windows 11 an OpenGL window without borders that covers its screen is turned by NVIDIA's driver into exclusive fullscreen at its first swap, with a flicker and a slow alt-tab (an engine reports 3 to 5 s of black screen per alt-tab). SDL's maintainer, primary: "This is intentional OpenGL driver behavior to increase performance and not something SDL can affect", it "reduces latency and improves performance because it bypasses the DWM compositor pipeline", and a hint building a workaround into SDL would be accepted. The ways reported there, community, none verified here: the driver's setting "Vulkan/OpenGL present method: Prefer layered on DXGI Swapchain" (NVIDIA's control panel), a player's setting and not the game's; a window one line shorter than the screen (the taskbar then drawn over it); the window without the style `WS_POPUP` (some drivers only); OpenGL presented through a DXGI swap chain with `WGL_NV_DX_interop2`, said to exist on the three vendors' drivers. Measure A's "Hardware: Legacy Flip" in borderless fullscreen is that conversion. To decide after PresentMon during alt-tabs, with each way's latency against today's.

**On a quiet machine** (the same series in one batch, the virtual machines off, nothing else running; HEAD with the change of measure G; the builds alternated):

| measure | HEAD | reference |
|---|---|---|
| A, CPU at 100 / 125 / 333 images a second, borderless | 29.6 / 33.7 / 39.3 % | 30.7 / 38.7 / 51.8 % |
| A, CPU with the vertical sync at 165 Hz | 56.1 % | 75.1 % |
| A, CPU in a window at 125 | 33.5 % | 35.2 % |
| A, 99th percentile between images at 100 / 125 / 333 | 10.68 / 8.83 / 3.59 ms | 10.95 / 8.92 / 3.81 ms |
| A, the same with the vertical sync at 165 Hz | 7.64 ms (max 8.49) | 6.46 ms (max 6.99) |
| B, input margin at 100 / 125 / 333 (mean) | 7.0 / 7.9 / 8.3 ms, none after its tick | 8.1 / 8.0 / 8.1 ms, none after its tick |
| C, a press of 3.2 ms (three) at 125 / 333 | 3.199-3.200 / 3.198-3.201 ms | 8.0, 8.0, 8.0 / 6.0, 5.0, 4.0 ms |
| C, a press of 25 ms at 125 / 333 | 25.000 / 24.998 ms | 32.0 / 27.0 ms |
| F, a restart of the video | 328 to 418 ms | 493 to 732 ms |
| G, the start to the main menu: multiplayer / Jedi Academy / Jedi Outcast | 406 / 448 / 308 ms | 567 / 627 / 497 ms |
| I, minimized, capped at 50 images a second in both | 6.6 then 37.7 % | 10.5 then 26.6 % |

The input margin's level moved between the two batches for both builds alike (3.5 to 3.9 ms the first time): the 20 s measured start 10 s after the connection, while the command clock may still be settling; the builds do not differ. The CPU of a minimized window varies between sessions as much in both builds, the images capped at 50 a second in each: no difference shown. The vertical sync's 99th percentile, higher with HEAD this time (7.64 against 6.46 ms) and the reverse the first time (6.41 against 6.31 ms), shows no image left unshown in either.

The temporary lines removed afterwards from both trees, the reference's diff empty again; HEAD built again with the three clang presets, the tests passed, clang-tidy at zero on the layer's files.

Decided on 26 November 2026, the black moment at alt-tab: the present way kept, the most direct, and the question left to plan step 25, whose Vulkan renderer lets the program choose whether the driver takes the screen (`VK_EXT_full_screen_exclusive`), on every system (Windows, Linux with X11 and Wayland, macOS when it is built); not taken: OpenGL presented through a DXGI swap chain (`WGL_NV_DX_interop2`) in part 4, work step 25 may replace; only telling NVIDIA's players the driver's setting.

The VM validations of the change of measure G, the same day: `validate.sh` ALL OK without a warning; `validate.bat`'s Debug and AddressSanitizer passed, its Release build failed twice on the shared folder (`C1001`, exception `C0000006`, "In-page error", while both VMs ran) and passed built again alone.

**Wayland** (the Linux VM, `SDL_VIDEO_DRIVER=wayland`, 26 November 2026): the client did not start, "the display's state could not be set: wayland cannot position non-popup windows". The layer centred the window with `SDL_SetWindowPosition` and took its refusal for a failure of the whole state, while Wayland refuses every program's own window a position ("Wayland does not allow toplevel windows to position themselves programmatically", SDL's README-wayland; `Wayland_SetWindowPosition`), its compositor placing them; SDL keeps the screen such a call names for the fullscreen that follows (`SDL_GetVideoDisplayForFullscreenWindow`: "the very common pattern of SDL_SetWindowPosition() followed immediately by SDL_SetWindowFullscreen()"). Fixed the same day (decided the same day): the position asked, its refusal failing nothing (`RequestCentred`, `src/shared/platform/window/window.cpp`); leaving fullscreen, the size and entering fullscreen still fail the state when refused.

| | before | now |
|---|---|---|
| The client started under Wayland | an error box, "the display's state could not be set: wayland cannot position non-popup windows", and no game | the game opens; in a window the compositor places it, in fullscreen it goes on the screen chosen |
| Under X11 and Windows | the window centred on its screen | the same |

Tried again under Wayland the same day: the keys, alt-tab, borderless and exclusive fullscreen work. The mouse did not turn the view, as under X11 on this VM since 25 November 2026 ("Known issues"); with VirtualBox's mouse integration turned off (Input menu, the host key and I) it does: VirtualBox gives the guest an absolute pointer, which has no motion to give a locked pointer. The VM's, not the game's.

**A restart of the video that changed nothing on Linux** (seen there the same day): with the Debug build, a change of the display's state applied from the menus left the display as it was, the renderer's `MODE` line repeating the old state, while the settings had their new values. `InitOpenGL` asks the client for the display only when `glConfig.vidWidth` is 0, which it is when the renderer's library is loaded afresh; glibc never unloads a library that holds GCC's unique symbols (`STB_GNU_UNIQUE`), and the Debug renderer for Linux holds 32 ({fmt}'s and the standard library's static objects; the Release one none), so the library stayed loaded with its `glConfig`, and the display was never asked again. Windows and Linux's Release worked because their renderer is unloaded. Fixed the same day (decided the same day): `RE_Shutdown` resets `glConfig` (and the multiplayer's `glConfigExt`) when the renderer restarts, in both renderers; checked on Windows, the multiplayer and Jedi Academy going between a window, another size and borderless fullscreen by `vid_restart`.

| | before | now |
|---|---|---|
| Linux, a GCC Debug build: the display's state, size or screen changed and applied | nothing changes, `MODE` repeats the old state | applied, as on Windows |
| Linux Release, Windows | applied, the renderer being loaded afresh | applied, whether it is or not |

**A crash under Wayland at a change of size** (the Linux VM, Mesa 23.2.1 of Ubuntu 22.04, its software drivers llvmpipe and softpipe; 26 November 2026): the client crashed at the first image shown after any change of the window's size, from fullscreen to a window or between two sizes of a window, in Mesa (`swrast_dri.so`, then `memmove` in `libEGL_mesa` once Ubuntu's symbols were fetched through debuginfod: the copy of the image into the window's shared-memory buffer), under `eglSwapBuffers`. Found by steps, each a session or a small program:

| step | result |
|---|---|
| X11 (XWayland), from fullscreen to a window | works |
| Wayland: a wait for leaving fullscreen before the size; without libdecor; `SDL_VIDEO_DOUBLE_BUFFER`; softpipe in place of llvmpipe | crashes, at the same address with both drivers: code they share |
| Wayland, `vid_restart` without a change | works |
| a small SDL 3 program (`build/linux-wayland/sdlresize/resize.cpp`, outside the repository): the context asked as the layer asks it, a resize between two images, out of fullscreen, the swap interval 0, the splash's drawing, the layer's three calls, a pause of 400 ms without events | works, every variant |
| the game, an empty image shown right after the change, before the renderer draws anything (a temporary line) | crashes on that image |

The cause, as these steps show it: Mesa's software path under Wayland takes the window's new size into its buffer only at the first drawing after a swap or when the context is bound again; the renderer that stops at `vid_restart` still runs its pending commands (`R_IssuePendingRenderCommands`) after the last swap, so the buffer is checked at the old size, then the size changes without a swap between, and the next swap copies a buffer of the old size into the window of the new one. The small program never draws between a swap and its resize. Mesa's resize callback should invalidate the buffer; the layer does not count on it: fixed the same day (decided the same day), `Window::Apply` binds the context to the window again after a change (released first, since SDL leaves a context already current as it is). Checked under Wayland, scripted: a window, borderless fullscreen, a window, exclusive fullscreen, a window, another size, each `MODE` line right, no crash; on Windows the same changes by `vid_restart`.

| | before | now |
|---|---|---|
| Linux under Wayland with Mesa's software drivers: the window's size changed, or from fullscreen to a window | the client crashes in Mesa at the first image after the change | the change applied |
| Windows, X11, hardware drivers | works | the same, the context bound again once at such a change |

`SDL_HINT_VIDEO_DOUBLE_BUFFER` (`SDL_VIDEO_DOUBLE_BUFFER=1`), tried under Wayland on the VM the same day: the game runs as without it; whether it saves a frame of delay, as SDL's documentation says, needs a measure of the delay this VM has no tool for. Left at SDL's default (off), to measure with plan step 25's presentation on every system.

The full VM validations of these fixes, the same day (logs `build/validate-linux.log`, `build/validate-windows.log`): `validate.sh` ALL OK (GCC 15 and Clang 23: Release, Debug, AddressSanitizer, the server's preset, the Debug build with link-time optimization, the formatting) and `validate.bat` ALL OK (MSVC: Release, Debug, AddressSanitizer), `ctest` 16 of 16 in every preset that runs it, no compiler warning. Tried by hand on Windows the same day, in the multiplayer and Jedi Outcast: borderless fullscreen, a window, exclusive fullscreen and borderless fullscreen again from the video menu, each applied: passed.

## 15. Where to resume (26 November 2026)

### 15.1 The state

- The multiplayer's part is committed (26 November 2026: `feat(platform): step 15, parts 1 and 2, the multiplayer's client on a new platform layer`), after the validations and the scenarios below. What that commit holds: `libs/SDL3/` (1,494 files, new), `src/shared/platform/` (new), `src/shared/sys/sys_charset.cpp` (new), `test/platform/` (new), and about 46 files changed (`git status`): the build (`Dependencies.cmake`, `.gitignore`, `src/shared/CMakeLists.txt`, `src/codemp/CMakeLists.txt`, `test/CMakeLists.txt`), the multiplayer's client, renderer, UI and cgame, the shared system files, the journal's removal in both engines and the file system, the documents, `GUIDELINES.md`, the root `README.md`.
- Points 1, 2 and 3 of section 11 are written; point 3's six sub-steps are sections 14.4 (1), 14.8 (2), 14.10 (3), 14.11 (4), 14.9 (5), 14.13 (6), with the journal's removal (14.6), the pass against the C++ Core Guidelines (14.5) and the Escape fix (14.12). The multiplayer's client runs on the layer; so do the single player's two games since section 14.14 (26 November 2026). SDL 2, `src/shared/sdl/` and `libs/SDL2` are gone (section 14.15, the same day; the VM validations ALL OK without a warning). Both committed together on 26 November 2026 (`feat(platform): step 15, parts 1 and 2, the single player's two games on the platform layer and SDL 2 removed`).
- Checked: every sub-step by the clang presets Release, Debug and AddressSanitizer on the development machine and by the Release presets of GCC 15, Clang 23 and MSVC on the VMs, without a warning; each tried by hand in multiplayer on the MSVC build ("everything works"), but a key without a code of its own pressed by hand (left out).
- The scripted scenarios on the development machine, 26 November 2026 (`scenarios.sh step15mp`, the clang Release build; the journal's removal touched the file system the server uses): `golden: SAME`, in 2 min 42 s.
- The full VM validations, 26 November 2026 (logs `build/validate-linux.log`, `build/validate-windows.log`): `validate.sh` ALL OK (GCC 15 and Clang 23: Release, Debug, AddressSanitizer, the server's preset, the Debug build with link-time optimization, the formatting) and `validate.bat` ALL OK (MSVC: Release, Debug, AddressSanitizer), `ctest` 16 of 16 in every preset that runs it, no compiler warning; the only messages are the `wayland-scanner`'s of section 14.1, once per protocol and configuration.
- `CHANGES.md` section 39 is written for the multiplayer's part, section 40 for the single player's.
- Point 5, the menus, decided and written on 26 November 2026 (section 14.16; `CHANGES.md` section 41): the VM validations ALL OK without a warning, the rows tried by hand in the three games; committed the same day (`feat(platform): step 15, parts 1 and 2, the display's rows in the setup menus`).
- Point 6, the checks and the measurements before and after, done on 26 November 2026 (section 14.17), with four fixes found on the way; the VM validations ALL OK, the display's changes tried by hand on Windows; committed the same day (`fix(platform): step 15, parts 1 and 2, point 6, the checks and the measurements before and after`). Points 1 to 6 of section 11 are done: the code of parts 1 and 2 is complete.
- Part 4, the picture's output stage, taken on 26 November 2026: its decisions (section 16.2), its sub-steps 1 and 2 done, tried by hand in the three games and validated the same day (sections 16.4, 16.5; `CHANGES.md` section 42), committed the same day; sub-step 3 written and checked on the development machine the same day (section 16.6), sub-step 4 remains; where to resume in section 16.7.

### 15.2 Next, in this order

1. Done on 26 November 2026: the validations ALL OK, the scenarios `golden: SAME`; the multiplayer's part is ready to commit.
2. Done on 26 November 2026: the multiplayer's part committed.
3. Point 4, the single player's engines (Jedi Academy and Jedi Outcast, one engine in `src/code`) on the layer. Written on 26 November 2026 (section 14.14): the worktree, the engine, the client, the UI, the renderer, Escape and the view are done; the VMs' builds and both games by hand passed the same day; the removals and the key codes' one definition are done (section 14.15), the full VM validations ALL OK the same day; committed the same day. The list as it was:
   - a worktree at the commit before this work first, kept for the measurements of point 6 (section 11);
   - the engine part `src/shared/platform/engine/engine.cpp` is compiled against each engine's headers: it uses the client's `cls`, `kg`, `Key_GetCatcher`, `CL_KeyEvent(key, down, int64_t)`, `CL_CharEvent`, `CL_MouseEvent(float, float)`, `CL_PointerEvent`, `Key_DynamicKey`, `SND_SetFocus`, `com_unfocused`; the single player's client (`src/code/client/`) has to give the same;
   - what was done to the multiplayer, to do again there: `Com_Frame` calling `Platform_Frame` (`src/code/qcommon/common.cpp:1427`), the renderer's imports (`src/code/rd-common/tr_public.h:94`, filled in `src/code/client/cl_main.cpp:1170`) and `src/code/rd-vanilla`, the keys' times and the mouse in floats (`src/code/client/cl_input.cpp`, `cl_keys.cpp`), the pointer (its image is the shader the single player's menus name as `cursor`: to read in each game's archives, as section 14.9 did; each UI's `MouseEvent` and drawn pointer), the character event in place of `K_CHAR_FLAG` (`src/code/client/cl_keys.cpp:1366`, `src/code/ui/ui_shared.cpp:8999`), the keys without a code, the console key's settings, Alt+Enter;
   - Escape while a key is bound or a field edited, to try in its menus and to fix as section 14.12 did (`src/code/ui/ui_main.cpp:3633`);
   - the single player's view that does not turn (section 8), looked at once it runs on the new input;
   - then the removals: `libs/SDL2`, `src/shared/sdl/` (its icon `sdl_icon.h`, which the engine part includes, moves into `src/shared/platform/engine/`), the target `modernjk_sys_client` and `modernjk_copy_runtime_dlls`, the definition `MODERNJK_PLATFORM` and every branch it chooses (`src/shared/sys/sys_main.cpp`, `sys_loadlib.h`, `sys_win32.cpp`, `sys_unix.cpp`), `K_CHAR_FLAG` (`src/shared/qcommon/q_limits.h`), the SDL 2 exception of GUIDELINES section 9, `libsdl2-dev` and SDL's row of the root README, clang-tidy's `-DSDL_DISABLE_ANALYZE_MACROS` (`cmake/StaticAnalyzers.cmake`);
   - the key codes in one definition for the trees (`src/codemp/ui/keycodes.h` and `src/code/client/keycodes.h` into `src/shared/qcommon`).
4. Point 5, the menus: the four settings' rows in the six setup menus and their texts in the games' languages (the sound device's row, `UI_SOUND_DEVICE`, is the model), the old rows removed, the quality presets no longer touching the display (section 9.2). Decided and written on 26 November 2026 (section 14.16); the VM validations ALL OK and the rows tried by hand in the three games the same day; committed the same day.
5. Point 6: the checks in game, the measurements before and after (section 11's list; the black moment at alt-tab in borderless fullscreen, section 14.7, with the presentation's tool), the Wayland driver on the Linux VM (`SDL_VIDEO_DRIVER=wayland`), `SDL_HINT_VIDEO_DOUBLE_BUFFER`, then the validations. Done on 26 November 2026 (section 14.17), with four fixes found on the way (the screens' modes read when needed, the position under Wayland, a restart of the video on Linux that changed nothing, the crash under Wayland at a change of size); the VM validations ALL OK and the changes tried by hand on Windows the same day; committed the same day.
6. At the end of the step: one code for the clients' keys and input and the menus' shared code, if it can be (section 11); the sound's engine part against the C++ Core Guidelines (section 14.5); the menus' pointer placed exactly, in floats, where it is placed in whole units of the menus' 640 by 480 today (3 pixels on a screen 1,920 wide, 6 on one of 3,840: section 14.9's known limit), taken with the menus' one code (decided 26 November 2026: left as it is until then).
   - Asked on 26 November 2026, where it is needed: the layer's tests made stronger, fuzzers included. What has no test today: the engine part (`src/shared/platform/engine/engine.cpp`: the keys' codes from the layer's events, the name of a key without a code, `r_exclusiveMode`'s reading, `Platform_IsKeyName`), the client's `Key_DynamicKey` (its sixteen codes, the one taken back) and `CL_KeyState` (the share of a span a key was held), all compiled into the engine only: to test them they have to be reachable from a test, as the sound's are. What reads text or data a player's files or the system give, so what a fuzzer is for: a bind's key name and `r_exclusiveMode` (configuration files), a typed text (UTF-8 from the system, turned into the one-byte set), an image for the pointer or the icon (its size against its bytes), and any sequence of SDL's events through `Input::Poll` and of display states through `Window::Apply` on the dummy driver. The fuzzers' preset is `windows-clang-fuzz` (netcodec's and the sound's, `src/shared/sound/fuzz/` the model).
7. Then, each on a word: part 4 (the renderers' output stage; among its decisions, the offscreen image's depth format and the multisampling's row, section 14.16), part 5 (the gamepad), part 6 (`SDL_DelayPrecise`). Part 4 taken on 26 November 2026, its decisions taken the same day, its sub-steps 1 and 2 done and validated the same day, its sub-step 3 written the same day and tried by hand in the three games and on Linux on 27 November 2026: section 16, where to resume in section 16.7.

### 15.3 What a next session needs to know

- The rules that came with this work: the layer and its engine part follow the C++ Core Guidelines, read in their text and not only through clang-tidy (GUIDELINES section 5; section 14.5 lists the rules applied); the engine part's functions are named `Platform_`; a refusal of the system is a result the caller reads (`[[nodiscard]]`).
- A change is built on the three clang presets here, then by the Release presets on both VMs, then tried by hand, before the next one; nothing is edited while the VMs build.
- The VMs' quick round: on Linux `cmake -S /media/sf_jka/ModernJK --preset linux-gcc-release`, `cmake --build /media/sf_jka/ModernJK/build/linux-gcc-release`, `ctest --test-dir ...` (and `linux-clang-release`), on Windows the MSVC Release build in Visual Studio's x64 environment, then the game folders' `update_from_build.bat`; the full round is `scripts/validate.sh` and `scripts/validate.bat`.
- clang-tidy on the layer here: `clang-tidy -p build/windows-clang-release -extra-arg=-Wno-unknown-warning-option -extra-arg=-Wno-ignored-optimization-argument -extra-arg=-Wno-unused-command-line-argument src/shared/platform/platform.cpp src/shared/platform/window/window.cpp src/shared/platform/input/input.cpp src/shared/platform/engine/engine.cpp` (no output, exit 0).
- A scripted silent session of the client, which shows the window, the renderer and the commands but neither the keys nor the mouse: the Release client started from `build/windows-clang-release/bin` with `+set fs_basepath "E:/Jeux/Jedi Academy default - ModernJK" +set fs_homepath <a scratch folder> +set s_initsound 0 +set logfile 2 +set r_displayState 0 +exec session.cfg`, the modules copied into `<scratch>/base`, the result read in `<scratch>/base/qconsole.log`. A scripted `+forward` does not move the player there: the keys and the mouse are tried by hand.
- Non-ASCII characters and backslash escapes in a source written from a session's tools: MSVC reads the sources without `/utf-8`, so the layer's sources and tests are plain ASCII, a character being written by its number (`char32_t{0xE9}`).
- A Windows header turns `LoadImage` into `LoadImageA`: the renderer's export is `LoadImagePixels`.
- A latched setting takes a new value only when it is registered again: the engine part registers the display's settings again at each start of the renderer.
- SDL's dummy video driver, which the tests run on, has no OpenGL, no icon, no minimizing and no exclusive mode: `modernjk_test_platform real` checks those on a real driver, by hand.
- The guidelines' text is at https://raw.githubusercontent.com/isocpp/CppCoreGuidelines/master/CppCoreGuidelines.md (the edition of 14 June 2026 was read).
- Measuring against the build before this work (point 6, section 14.17): the worktree `E:\Git\jka\ModernJK_sdl2ref` (clang Release, SDL 2, diff empty); the clients started from PowerShell need llvm-mingw's `bin` folder in their `PATH` (its `libc++.dll`, `libunwind.dll`), and a number PowerShell formats takes the system's decimal comma unless the invariant culture is asked; PresentMon (Intel, MIT, 2.6.0) runs elevated through `cmd /c` (started directly it stopped without a word) and records every present of a process, its present mode and the time to the screen; a client connects to a server of the same machine by `127.0.0.1` (`localhost` is the engine's own loopback).
- Scripted sessions in a level: a configuration file's `wait` counts every pass of the command buffer, a tick's included (about 225 a second at 125 frames a second in a game), and a bind's `+command` goes into that buffer behind any `wait` there, so a test that presses keys drives itself from code (section 14.17's harness, `tmpseries`); a level started by `devmap` from a configuration at the program's start leaves the UI's key catcher set.
- The Linux VM: VirtualBox's mouse integration gives the guest an absolute pointer, which turns no view: turn it off (Input menu, the host key and I) to try the mouse in game; under Wayland its OpenGL is Mesa's software driver (llvmpipe, Mesa 23.2.1); `gdb` with Ubuntu's debuginfod (`DEBUGINFOD_URLS=https://debuginfod.ubuntu.com`, `-iex "set debuginfod enabled on"`) names Mesa's functions in a trace; a GCC Debug library with unique symbols is never unloaded by glibc (`readelf --dyn-syms`, `UNIQUE`).

## 16. Part 4: the picture's output stage (26 November 2026)

Part 4 of the plan, on the word given on 26 November 2026, after section 1's decision 7 and section 4.

### 16.1 Read before the decisions

- The window's OpenGL context is asked without a version nor a profile: SDL asks for 2.1 by default (`SDL_GL_ResetAttributes`, SDL 3.4.20's `src/video/SDL_video.c`), and the drivers give their highest compatibility profile (4.5 with Mesa 23.2.1 on the Linux VM, its log's `GL_VERSION`); with 8 bits of alpha (SDL 3's default), 24 of depth, 8 of stencil and the samples of `r_ext_multisample`.
- Since part 1 `r_gamma` acts on the textures when they are loaded (`R_LightScaleTexture`, section 10.3). The multiplayer's renderer also has OpenJK's `r_gammaShaders` (0 by default, latched): at each image the back buffer copied into a rectangle texture and drawn again through ARB assembly programs and a 3D table of 64 levels a channel, 6 bits.
- Overbright (`r_overBrightBits`) needs the screen's ramp and fullscreen (`R_SetColorMappings`), so it has done nothing since part 1; 0 by default here and in Raven's code as released (the commit that added the source dump), as `r_gamma` 1.
- What reads the picture back: the dynamic glow (`r_DynamicGlow`, 0 by default) uses the back buffer as its scratch, four copies an image (`glCopyTexSubImage2D`: the scene, the glowing objects, the blur); the flares read one pixel of depth (`glReadPixels`); the captures read the back buffer (`RB_ReadPixels`, Jedi Outcast's saved picture), from the console's command between two images; the overdraw count reads the stencil. OpenGL refuses to read a multisampled framebuffer object ("GL_INVALID_OPERATION is generated if GL_READ_FRAMEBUFFER_BINDING is non-zero, the read framebuffer is complete, and the value of GL_SAMPLE_BUFFERS for the read framebuffer is greater than zero", `glReadPixels`' reference page), so each such read resolves the image first.
- The game's shaders that blend with the destination's alpha, read in the archives: Jedi Academy's main menu (`gfx/menus/menu4`, `shaders/ui.shader` of `assets1.pk3`), a cloud layer (`textures/skies/cloudlayer2`, `shaders/skies.shader`), Jedi Outcast's menu (`shaders/ui.shader` of `assets0.pk3`). An offscreen image with 2 bits of alpha (`GL_RGB10_A2`, the 10-bit candidate of section 1's decision 7) would change them: it is not a candidate.
- A floating-point image does not clamp the blends: "If the color buffer is floating-point, no clamping occurs", while a fixed-point one clamps the inputs and the result (ARB_color_buffer_float, core in OpenGL 3.0). An additive stage above 1 followed by a multiplying stage would give another picture than the game's; that is the lighting model of plan step 25.
- OpenGL 3.0 requires `RGBA8`, `RGBA16`, `RGBA16F`, `DEPTH24_STENCIL8` and `DEPTH32F_STENCIL8` for the textures and the renderbuffers (its section 3.9.1, "Required Texture Formats", and section 4.4, "Required Renderbuffer Formats"), with `BlitFramebuffer` and `RenderbufferStorageMultisample`.
- A depth in floating point gains only with OpenGL's [0, 1] range and a reversed depth (`glClipControl`, ARB_clip_control, core in OpenGL 4.5): the default [-1, 1] range "destroyed all the precision in the far half of the range" (NVIDIA, "Visualizing Depth Precision").
- Dithering: Unreal Engine adds a noise in its tone mapper "to avoid color banding because of 8bit quantization" (`r.Tonemapper.GrainQuantization`, its documentation); Godot offers a debanding pass, "very cheap", off by default.
- A change of resolution in Unity "is applied at the end of the current frame and not immediately" (`Screen.SetResolution`).
- The modules read the picture's size at their start: each cgame 6 or 7 times, each UI 3 or 4 (`glconfig.vidWidth`, `glconfig.vidHeight`).

### 16.2 The decisions (26 November 2026)

| # | question | decided | not taken |
|---|---|---|---|
| 1 | The offscreen image's precision (section 1, decision 7) | measured between `RGBA8` and `RGBA16` (16 bits unsigned normalized), each with and without a fixed dither: both clamp the blends as the window does; `RGBA16` takes 8.3 MB more at 1920 by 1080 and the pass reads 16.6 MB an image instead of 8.3; a fixed pattern keeps the captures reproducible | `RGBA16F` among them (blends not clamped: another picture); `RGBA8` without a measure |
| 2 | Its depth | `DEPTH24_STENCIL8`, as the window's today: 4 bytes a pixel, 8.3 MB at 1920 by 1080; a reversed depth in floating point with plan step 25, whose depth range is [0, 1] | `DEPTH32F_STENCIL8` with a reversed depth and `glClipControl`: 8 bytes a pixel, the 82 depth calls of both renderers, the projections, the weapon's and the sky's depth ranges and the flares' read reversed, no z-fighting reported; the same without the reversal (no gain) |
| 3 | The multisampling's row (section 14.16, decision 5) | Off, 2x, 4x, 8x, limited to what the driver offers, Off by default as before and in the original game; changed while running; at 4x and 1920 by 1080, 66 MB more, a resolve at each image and before each read | 4x by default |
| 4 | A change of the window's size or of the display's state (section 10.3) | applied at once, without starting the renderer and the modules again: the offscreen images made again at the new size, the textures kept, the cgame and the UI given the new size by a new call (the API versions of the multiplayer's cgame and UI, of the single player's UI, of Jedi Academy's and Jedi Outcast's cgames); Alt+Enter and "Apply" one image instead of 330 to 420 ms; the display's settings no longer latched | a restart of the video at each change, as in parts 1 and 2 |
| 5 | `r_overBrightBits` | removed, its default kept (none); `r_mapOverBrightBits` stays (the lightmaps scaled when loaded) | applied by the pass in every display state |
| 6 | The output stage's code | after the C++ Core Guidelines read in their text, clang-tidy at zero, a README, the tests of what runs without OpenGL, as the platform layer (GUIDELINES section 5) | the engine's style |

Settled without a choice, by section 1's decision 7 and the project's rules: the pass in GLSL 1.30, `r_gamma`'s curve computed in floating point (`pow(c, 1 / r_gamma)`, the formula of today's table, so the same brightness at the same setting), applied to the whole picture, the menus and the HUD included, as the screen's ramp was; `r_gammaShaders` and the gamma applied to the textures removed, `r_intensity` kept; OpenGL 3.0 checked at the start, an error below it (no fallback); the functions of OpenGL loaded through the renderer's import `GL_GetProcAddress`, as its extensions are; the window without depth, stencil nor samples, since it only receives the pass; the captures read after the pass; the image resolved before each read.

Decided the same day: when the renderer moves to Vulkan (plan step 25), these decisions are discussed again for it, not taken over as they are (written in the plan's step 25).

### 16.3 The order

Each sub-step built and checked before the next, as the parts 1 and 2 were:

1. ✅ (written, built on the VMs and tried by hand on 26 November 2026, section 16.4) the output stage (`src/shared/rd-common/output/`) and both renderers drawing through it: the offscreen image (`RGBA8` and `RGBA16`, and the dither, chosen by a temporary setting until the measure), its depth, its multisampling read when the renderer starts as before, with the resolves before each read (the glow, the flares, the captures), the pass with `r_gamma` applied at once; OpenGL 3.0 checked; the captures after the pass; `r_gammaShaders`, the gamma on the textures and `r_overBrightBits` removed; the window without depth, stencil nor samples;
2. ✅ (written, built on the VMs and tried by hand in the three games on 26 November 2026, section 16.5) the multisampling's row: `r_ext_multisample` changed while running, its row in the six setup menus with its texts;
3. ✅ (written on 26 November 2026, built on the VMs and tried by hand in the three games and on Linux under X11 and Wayland on 27 November 2026, with four fixes found on the way, section 16.6; the VM validations ALL OK the same day) a change of size or of the display's state at once: the renderer's size-dependent images made again, the modules' new call, the display's settings no longer latched, Alt+Enter and "Apply" without a restart of the video;
4. ⏳ the measures: the banding (the two formats, with and without the dither, at several values of `r_gamma`), the pass's cost at 100, 125, 144 (vsync) and 333 images a second; the format and the dither chosen and the rest removed; `CHANGES.md`; the VM validations.

### 16.4 Sub-step 1: the stage in both renderers (26 November 2026)

Written:

- `src/shared/rd-common/output/` (its README), the static library `modernjk_render_output`, independent of the renderers, under netcodec's strict warnings and its own `.clang-tidy` (the platform layer's checks), position independent since the renderers are shared libraries. `gl.h` and `gl.cpp`: the part of OpenGL the stage uses, under names of its own so that the system's headers can be included beside it, every function asked of the driver by name through the renderer's import, OpenGL 1.1's included; the version read first and refused below 3.0. `stage.h` and `stage.cpp`: the scene (a framebuffer object of one colour image, `RGBA8` or `RGBA16`, and one `DEPTH24_STENCIL8` image; multisampled, renderbuffers and a single-sampled copy for the resolve), the pass in GLSL 1.30 (`texelFetch`, `pow(c, 1 / gamma)`, the ordered dither), `MakeReadable`, `Present` keeping the renderer's state of OpenGL (`glPushAttrib`).
- `src/shared/rd-common/output/engine/` (its README): the stage as a renderer runs it, compiled by each renderer against its tree's `tr_local.h`: `Output_Init`, `Output_Shutdown`, `Output_DrawScene`, `Output_MakeReadable`, `Output_Present`, `Output_PrintInfo`.
- Both renderers: `Output_Init` where the renderer starts on the window (`InitOpenGL`); each image drawn into the scene from `RB_DrawBuffer` (the draw buffer command's buffer and its stereo branch gone, the window having no stereo buffers); the pass before the swap, and for the splash; every read of the picture made readable first: the dynamic glow's copies (three, and two in its blur), the distortion's capture (`RB_CaptureScreenImage`), the post-render entities' copy, a flare's pixel of depth, the overdraw's stencil, the single player's dissolve (`RE_InitDissolve`, which read the back buffer after the swap, so whatever the driver had left there, and now reads the scene's last image). The captures (`RB_ReadPixels`: the screenshots, the level shots, Jedi Outcast's saved picture) are read in the window after the pass, so with the brightness, which the screen's ramp left out of them (`R_GammaCorrect` gone). Removed: `r_gammaShaders` with its ARB programs and its 3D table (the multiplayer's), the gamma applied to the textures (`R_LightScaleTexture` keeps `r_intensity`), `r_overBrightBits` and `tr.overbrightBits` (`tr.identityLight` is 1, the lightmaps scaled by `r_mapOverBrightBits` alone, as with the default before), the multiplayer's 3D texture functions, which only that table used.
- The settings, registered by each renderer: `r_ext_multisample` (moved from the platform layer's engine part, latched as before until sub-step 2), and the temporary `r_outputPrecision` (0 for `RGBA8`, 1 for `RGBA16`) and `r_outputDither` (0, 1), applied at once, until the measure of sub-step 4.
- The platform layer: `Window::Open` takes `Context::OpenGl` or `Context::None` in place of a `GlRequest`; the context's window is asked 24 bits of colour and nothing more (no alpha, which a compositor would use to blend the window with what is behind it; no depth, stencil nor samples), and `Window::Samples()` is gone; `Platform_Display` no longer gives the bits of colour, depth and stencil, which are the scene's (`Output_Init`).
- The tests, `test/rd-common` (`modernjk.rd-common`): the version read in the driver's string, the samples allowed, the table loaded through a fake driver; by hand, `modernjk_test_rd_common real`, the stage on the system's own driver (the stage's README).

Found on the way: NVIDIA's conversion of the pass's colour to the window's 8 bits is not always the exact rounding. With the dither's thresholds at ±15/32 of a level, a level drawn exactly came back one level up (`modernjk_test_rd_common real`); the thresholds are scaled by 0.8, within 0.375 of a level, and every level comes back as drawn.

Checked on the development machine (llvm-mingw's clang): the presets Release, Debug and AddressSanitizer build every target without a warning, the six `modernjk` tests pass in each, clang-tidy at zero on the stage's files (its engine part once per renderer, three times) and on the layer's changed files, clang-format clean. `modernjk_test_rd_common real` on the development machine's GPU (OpenGL 4.6.0 NVIDIA 617.14), in Release and under AddressSanitizer: at `r_gamma` 1 every level back as drawn, with and without the dither, at both precisions, with and without 4 samples; at 1.5 and 2.5 within one level of the curve; a multisampled scene made readable gives its levels back. Scripted silent sessions: the multiplayer on `mp/duel1` (`r_gamma` 2 applied at once to the whole picture, its menus included; `RGBA16` and the dither switched while running; 4 samples after a restart of the video; the dynamic glow with and without samples), Jedi Academy on `yavin1b` and Jedi Outcast on `artus_mine` (`r_gamma` 2, 4 samples): the screenshots as expected, no warning of the stage.

The VMs' Release builds, the same day (logs `build/p4-linux-gcc-release.log`, `build/p4-linux-clang-release.log`, `build/build-msvc-release.log`): GCC 15, Clang 23 and MSVC without a warning, `ctest` 17 of 17 with both Linux compilers; the MSVC build's `modernjk_test_rd_common`, its checks and `real`, passed on the development machine. `modernjk_test_rd_common real` on the Linux VM, Mesa 23.2.1's software driver in its compatibility profile 4.5 (`build/p4-linux-real-x11.log`, `build/p4-linux-real-wayland.log`): under X11 (XWayland) and under Wayland, every level of every case back exactly as the curve gives it, no difference of a level even at `r_gamma` 1.5 and 2.5.

Tried by hand on Windows the same day, the MSVC build in the multiplayer: the brightness slider of the video menu changing the whole picture at once, the menus included, without a restart; after an alt-tab the desktop at its own brightness and the game as before it; a screenshot as bright as the screen: passed.

The clang-tidy command of section 15.3, with the stage's files: `clang-tidy -p build/windows-clang-release -extra-arg=-Wno-unknown-warning-option -extra-arg=-Wno-ignored-optimization-argument -extra-arg=-Wno-unused-command-line-argument src/shared/rd-common/output/gl.cpp src/shared/rd-common/output/stage.cpp src/shared/rd-common/output/engine/engine.cpp`.

### 16.5 Sub-step 2: the multisampling's row (26 November 2026)

Written (section 16.2, decision 3):

- `src/shared/rd-common/output/samples.h`: the samples the stage offers (`AllowedSamples`, moved there from the stage) and the value the menus' row goes to at a click (`NextSamples`: none, then 2, 4 and 8 as far as the driver allows, rising, round), one definition for the stage and for the menus of the three games, the standard library only (GUIDELINES section 6).
- `r_ext_multisample` no longer latched: the stage makes its images again at the next image (`Output_DrawScene`); the renderer sets `r_ext_multisample_avail` to the most samples the driver allows, which the row offers up to (the game's own `r_ext_texture_filter_anisotropic_avail` is the model).
- The row, owner-drawn (`UI_MULTISAMPLE`: 302 in the multiplayer's UI, 261 in the single player's), Off or "4x" as the renderer applies it, each click, Enter or right arrow the next value, the right button or the left arrow the one before; the value set at once, without "Apply". In the six setup menus on the advanced video page, right under the anisotropic filtering, where each page has room (the multiplayer's main menu at 370 and its in-game one at 243, Jedi Academy's two at 373, Jedi Outcast's two at 371, the place of the original game's hidden flares row there), each with the highlight its neighbours use.
- The texts: "Anti-aliasing:" and its description in English, French, German and Spanish, in `assets/jka/strings/<Language>/MODERNJK.str` and `assets/jk2/strip/MENUS4.sp` (without the space before a colon in French there).

Checked on the development machine (llvm-mingw's clang): the presets Release, Debug and AddressSanitizer build every target without a warning, the six `modernjk` tests pass in each (the row's next value among them), clang-tidy at zero on the stage's files, clang-format clean; a scripted session of the multiplayer: `r_ext_multisample_avail` 8 on the development machine's GPU, `r_ext_multisample` 8, 3 and 0 set during a level giving 8, 2 and no samples at the next image, without a restart; the three games load their menus without an error of the menus' parser, and the single player's two take 4 samples during a level. The VMs' Release builds, the same day (logs `build/p4s2-linux-gcc-release.log`, `build/p4s2-linux-clang-release.log`, `build/build-msvc-release.log`): GCC 15, Clang 23 and MSVC without a warning, `ctest` 17 of 17 with both Linux compilers.

Tried by hand in the multiplayer the same day, its main and in-game menus: the row in its place with its highlight and its description, Off, 2x, 4x, 8x and Off again at each click and back with the right button, the edges smoothed at once without a restart, 4x kept into a level: passed. The same in Jedi Academy's single player and in Jedi Outcast, their main and in-game menus, the same day: passed.

The full VM validations of sub-steps 1 and 2 together, the same day (logs `build/validate-linux.log`, `build/validate-windows.log`): `validate.sh` ALL OK (GCC 15 and Clang 23: Release, Debug, AddressSanitizer, the server's preset, the Debug build with link-time optimization, the formatting) and `validate.bat` ALL OK (MSVC: Release, Debug, AddressSanitizer), `ctest` passed in every preset that runs it, no compiler warning. `CHANGES.md` section 42 written the same day (its cost an estimate until sub-step 4's measure). The scripted scenarios were not run again: they play the server and the bots, which these sub-steps do not touch.

### 16.6 Sub-step 3: a change of size or of the display's state at once (26 November 2026)

Decided on 26 November 2026, the open points of section 16.2's decision 4:

| # | question | decided | not taken |
|---|---|---|---|
| 1 | Who applies a changed display setting, and when | the platform layer's engine part, at each `Platform_Frame` after the input's events: a change of one of the five settings (its flag `modified`; the settings are no longer latched) is applied to the window, then the client is told (`CL_DisplayChanged`), which tells the renderer, the UI and the cgame; the same path when the system changes the window's size (`PixelSizeChanged`, ignored before: a window moved to a screen of another scale, a Wayland compositor resizing it). Alt+Enter and "Apply" take effect at their own frame, a setting typed in the console at the next one (two settings typed one after the other are two changes) | a command `vid_apply`, which the menus and Alt+Enter would call |
| 2 | The modules' new call | one function without argument, "the display changed": the module reads the configuration again through the call it has (`GetGlconfig`) and computes its scales again, the code of its start moved into one function both call. `CGAME_API_VERSION` 7 and `UI_API_VERSION` 9 in the multiplayer; the command `CG_DISPLAY_CHANGED` for the single player's cgames, which live in the game's library (`GAME_API_VERSION` 13 for both games); the single player's UI, compiled into the engine, called directly (its `UI_API_VERSION` 4 unchanged) | the size given as arguments; the modules reading the configuration at each frame |
| 3 | The renderer's new call | an export `DisplayChanged(glconfig_t*)` (`REF_API_VERSION` 12 in the multiplayer, 21 in the single player), called once per change: the display asked again (`Platform_Display`), the scene and the dynamic glow's three images made again when the size changed, the textures and the rest kept, the configuration returned to the client | the renderer checking the window's size at each frame |
| 4 | What "Apply" restarts | the menus decide: the video restarts only when one of the six renderer settings that need it has a new value (`r_picmip`, `r_texturebits`, `r_detailtextures`, `r_ext_compress_textures`, `r_subdivisions`, `r_allowExtensions`); the display's settings take effect at the next frame, the others at once as before; the player stays on the video page | `vid_restart` deciding by itself, which needs the engine to know the renderer's latched settings and no longer forces a restart from the console |

Written:

- The platform layer's engine part (`src/shared/platform/engine/engine.cpp`): the display's five settings registered once, when the window opens, and no longer latched; `Platform_Frame` applies them after the events (`TakeModified`, `ApplySettings`) and calls `CL_DisplayChanged` when it did or when the system changed the window's size; `Platform_Display` applies a setting changed since the last frame (one set together with `vid_restart`) and no longer registers the settings again.
- Both clients: `CL_DisplayChanged`, declared in each tree's `qcommon.h` beside the other functions the engine part calls: the renderer's `DisplayChanged` fills `cls.glconfig`; when the size changed, the menus' pointer image is made again at its size in pixels; then the UI and the cgame are told, at each change, the same size in another state included. Alt+Enter sets `r_displayState` only.
- Both renderers: `RE_DisplayChanged`; `R_SizeGlowImages`, the dynamic glow's images at the window's size, called by `R_CreateBuiltinImages` and when the size changed (bound to their own target only, without the enabling and disabling of the textures around them, which changed nothing); the stage's `Output_Resize` (`Stage::Apply` at the new size). A size of zero, which the window gives when the system cannot tell, keeps the images as they are.
- The modules: the multiplayer's cgame (`CG_DisplayChanged`: the configuration, `screenXScale`, `screenYScale`) and UI (`UI_DisplayChanged`: the configuration, `xscale`, `yscale`, `bias`, then the video page's display rows); the single player's UI (`UI_DisplayChanged` in `ui_atoms.cpp`: the old menus' `uis`, then the new menus' `_UI_DisplayChanged`, the same as the multiplayer's); Jedi Academy's and Jedi Outcast's cgames (the command reads `cgs.glconfig` again: nothing else is derived from it at their start, the view, the HUD and the letterbox reading it at each frame).
- The menus' "Apply" in both UIs (`UI_UpdateVideoSetup`, its restart decided by `UI_VideoNeedsRestart`).
- What needs nothing: the console (`Con_CheckResize` at each drawing), the cinematics, the fonts, the single player's dissolve (drawn in the menus' 640 by 480), which read the size at each use.

Measured on the development machine, the same day (a temporary trace, the clang Release build, a demo played back in the multiplayer with the dynamic glow on): a change takes the system 3 to 13 ms to apply to the window (`SDL_SyncWindow` waits for it), then 1.5 to 8.8 ms for the renderer, the pointer's image and the modules, 5 to 18 ms in all; the second notice that follows (the system's own size event, the size unchanged by then) 6 to 10 us. A restart of the video took 330 to 420 ms (section 14.17), and the cgame's start alone 0.5 to 0.8 s with a level loaded (the same logs). Memory: the scene's and the glow's images replaced, none added once done; for an instant the old and the new scene both exist (`Stage::Apply` makes the new images before it deletes the old). At each frame: five flags read.

Checked on the development machine, the same day (llvm-mingw's clang): the presets Release, Debug and AddressSanitizer build every target without a warning, the six `modernjk` tests pass, clang-tidy at zero on both engine parts, clang-format clean. Scripted silent sessions of the clang Release build: the multiplayer's main menu at 1280 by 720 then 1600 by 900; in a level and in a demo, the dynamic glow on, 1600 by 900, 1024 by 900, 900 by 900, 1024 by 576 with 4 samples, borderless fullscreen at 1920 by 1080 and back to a window: the renderer started once, each screenshot at its size, the HUD, the menus, the console and the glow placed for it; `r_windowWidth` set with `vid_restart` in one go, then another size: right. Jedi Academy on `yavin1b` and Jedi Outcast on `artus_mine`: the main menus at 1600 by 900, the level's opening cinematic and its bars at 900 by 900, borderless fullscreen.

Found while writing it, and fixed on 27 November 2026 (decided that day: the UI copies the display into its rows): Alt+Enter while the video page was open, then another row of that page changed and "Apply", set the display back to the state the page showed when it opened (in a window, Alt+Enter, then the texture quality and "Apply": back to a window). Before this sub-step, Alt+Enter restarted the UI with the video, so the page opened again. Now both UIs read the display's settings into the page's display rows when the display changed (`UI_GetDisplaySetup`, also called by `UI_GetVideoSetup` when the page opens), unless the page has a change of its own to apply (`ui_r_modified`), which "Apply" then sets as the page shows it. A scripted session of the multiplayer: the setup menu open, `r_displayState` 1 then 0 and a new size, the page's copies following each.

Tried by hand in the multiplayer on 27 November 2026 (the MSVC build), after the VMs' Release builds (GCC 15, Clang 23, MSVC without a warning, their tests passed): Alt+Enter in the main menu and in a level, the display row following Alt+Enter on the video page, a change of the texture quality restarting the renderer in borderless fullscreen: passed. Found: a new resolution applied at once, but the menus back to their main page. The cause was the six menus, not the engine: their "Apply" opens a question ("This will apply video settings and return to the Main Menu. Continue?") whose "Yes" closes every menu before it applies, written for a restart. Fixed the same day (decided that day): without a setting that needs the renderer to start again, "Apply" applies at once, hides itself and the page stays; the question stays, as it was, only when one of the six texture settings changed, its text then true. The button of the six menus runs `defer VideoApply` before the question, which the UIs' `UI_DeferMenuScript` answers by applying and leaving the rest of the script out when no restart is needed (the menus' own mechanism, which the page's warning on leaving with changes uses). The three games load the six menus without an error of their parser.

Seen once by hand, not reproduced (27 November 2026): after changes of the display in the menus (the three states, exclusive fullscreen, resolutions), the first Alt+Enter in a level, from borderless fullscreen to a window, showed the system's arrow at the centre of the screen, no menu open; it stayed there when the mouse moved, and the next Alt+Enter took it away. Four scripted series in a demo (about 45 changes: the three states, exclusive fullscreen at 1600 by 900, four sizes, `vid_restart`, the console open), the system's pointer read every 5 ms (`GetCursorInfo`) and SDL's focus traced after each change: the pointer hidden and SDL's focus kept every time. The cause, as SDL's source gives it: when Windows deactivates the window in relative mode, SDL gives up the mouse's focus and draws the system's arrow (`WIN_UpdateFocus` in `SDL_windowsevents.c`, `SDL_RedrawCursor` in `SDL_mouse.c`), and takes the focus back only on a motion of the pointer itself (`SDL_SendMouseMotion` updates the focus only for a motion that is not relative), which the relative mode, holding the pointer at the window's centre, never lets happen; the layer, whose mode had not changed (the focus lost and regained between two frames), did not set it again. Fixed the same day (decided that day): `Input::Poll` turns the relative mode off and on again when SDL gave up the mouse's focus (`SDL_EVENT_WINDOW_MOUSE_LEAVE`) while the mouse turns the view and the window has the keyboard, which takes the focus back from the keyboard's (`SDL_SetRelativeMouseMode`); a check of `modernjk.platform` (the mode and SDL's focus kept, nothing handed over; as the pointer, no change); a scripted series of 14 changes in a demo: the pointer hidden every time. SDL gives up the mouse's focus in relative mode on X11 and Wayland only when the pointer really leaves (a crossing that is not the grab's, the compositor's leave), so not at every frame.

Both fixes tried by hand in the multiplayer the same day (the MSVC build): a new resolution and "Apply" without a question, the page kept and the button hidden, in the main and the in-game menus; a change of the texture quality and "Apply" with the question, "No" back to the page, "Yes" restarting and back to the main menu; the changes of the menus played again (the three states, exclusive fullscreen, resolutions), then Alt+Enter several times in a level: the system's arrow not seen again. Passed.

Tried by hand in both single-player games on 27 November 2026 (the MSVC build): in Jedi Academy and in Jedi Outcast, Alt+Enter in the main menu; a new resolution and "Apply" without a question, the page kept; Alt+Enter on the video page, its display row following; a change of the texture quality and "Apply" with the question, "Yes" restarting the renderer in borderless fullscreen; Alt+Enter twice in a level (the HUD, the crosshair and the console in place, no system arrow); a new resolution from the in-game menu, the page kept: passed.

Tried by hand on the Linux VM the same day (GCC 15's and Clang 23's builds; X11 through XWayland, and Wayland; VirtualBox's mouse integration off in game): Alt+Enter in the main menu, a new resolution and "Apply" without a question, Alt+Enter twice in a level, the mouse turning the view: passed, but for the scene and the right Alt below.

Asked the same day: the screens' modes, which the system enumerates slowly the first time (about 0.4 s for two screens), read at the first click on the screen, resolution or refresh rate row rather than when the video page is first drawn. Kept as it is (decided 27 November 2026): until that click, the refresh rate row in exclusive fullscreen with a mode of its own would show the rate `r_exclusiveMode` names rather than the nearest one the screen has, which differ for a rate typed in the console or a configuration from another screen.

Found during the tries and fixed on 27 November 2026, each decided that day:

- **The single player's opening logos.** A key in a video's first 1.2 s was refused (`SCR_StopCinematic(true)`, Raven's guard against skipping a level's video by firing), for every video: the first click on the LucasArts logo was lost. The guard now applies to a level's videos only (`CL_IsRunningInGameCinematic()`); the logos and Jedi Outcast's last video, played outside a level by the `cinematic` command, stop at the first key, as the multiplayer's. Tried by hand in Jedi Academy: the logo skipped by a click in its first second; a level's video (`ingamecinematic ja02` in `yavin1b`) kept by a click at once, stopped by one after 2 s.
- **The scene overwritten on Linux.** On the Linux VM, in a window: a new resolution and "Apply" twice, Alt+Enter twice, then `devmap mp/ffa3`: the screen black but for a piece of the picture in its bottom left corner (256 by 256 pixels once, a strip of about 512 by 32 another time), Alt+Enter bringing the picture back; reproduced every time. The same steps in a scripted session on Windows: the picture right. The cause: both renderers choose their textures' names themselves (`1024 + giTextureBindNum++`, from 2048 up, Raven's), while the stage asks the driver for the scene's (`glGenTextures`), which guarantees only that "none of the returned names was in use immediately before the call" (its reference page). Mesa 23.2.1 gives the name after the highest in use (`_mesa_HashFindFreeKeyBlock`, `src/mesa/main/hash.c` lines 418 to 426, "the quick solution", unless the driver option `force_gl_names_reuse` is set, `src/mesa/state_tracker/st_context.c` line 751; a name bound without `glGenTextures` counts, `hash.c` lines 191 and 192; read in the release's archive from archive.mesa3d.org): the scene made again during a session took the names right above the renderers' last, which their next textures then took, a level's texture loaded into the scene. NVIDIA's driver gives low names, far from 2048. OpenJK's renderers never call `glGenTextures`: the defect came with the stage (sub-step 1), seen once the scene is made again during a session (the multisampling's row of sub-step 2, a change of size here). Fixed: every texture name of both renderers given by the driver (`qglGenTextures` for the images and for the dynamic glow's three), `giTextureBindNum` removed. Tried on the VM under X11 and Wayland: the reproduction, then Alt+Enter in a level and `devmap mp/duel1`: the picture whole.
- **The dynamic glow's images kept with the glow off** (Raven's, OpenJK's). Each start of the renderer, so each level loaded, makes the glow's three images at the window's size (`R_CreateBuiltinImages`); the renderer's shutdown deleted them only when `r_DynamicGlow` was on. With the glow off (the default), each level left the last ones in the graphics card's memory: about 17 MB at 1152 by 864 and 34 MB at 1920 by 1080 (two `GL_RGBA16` images of 8 bytes a pixel and the blur's; computed from the code, not measured). With the names from the driver it would have happened at each restart of the video too, where the counter used to give the same names again. Fixed: the three deleted at each shutdown of the renderer, whatever the glow, and their names cleared. Scripted sessions in the three games: levels loaded with the glow on and off, a restart of the video with the glow on: the pictures right.
- **The right Alt under X11 and Wayland.** On the VM, right Alt and Enter did nothing. X11 and Wayland give a key the symbol of the player's layout at its first level; most European layouts, the French one among them, make the right Alt Alt Gr (`ISO_Level3_Shift`, which SDL turns into `SDLK_MODE`, `src/events/SDL_keysym_to_keycode.c`), a symbol the layer does not name, so the key came as one without a code (`MODESWITCH` in the controls menu). On Windows and macOS, SDL names a key that types no character by its place whatever the layout (`src/video/windows/SDL_windowskeyboard.c`, `src/video/cocoa/SDL_cocoakeyboard.m`). Fixed: a key that types no character and whose symbol the layer does not name takes its place's code (`KeyCodeOf`, `src/shared/platform/input/input.cpp`), which covers the level 3 and level 5 shifts, Compose, Meta and Hyper wherever a layout puts them; a key the player's layout made into one the layer names (Caps Lock made a Ctrl) keeps it, as on the player's desktop. Nothing changes on Windows, where SDL already gives those keys their place's code. Checks of `modernjk.platform`: Alt Gr at the right Alt's place is `RightAlt`, the level 5 shift at Caps Lock's is Caps Lock, Compose at the menu key's takes that place's code, Caps Lock made a Ctrl stays a Ctrl. Tried on the VM under X11 and Wayland: `RALT` in the controls menu, right Alt and Enter switching the display, Alt Gr still typing `@` in the console. GCC 15 refused SDL's macro `SDL_SCANCODE_TO_KEYCODE` there (`-Wsign-conversion`, on the scancode's implicit conversion; the VM's build failed, which a reading of its log for warnings and tests alone missed): the conversion written explicitly.

Looked at, not changed: the dead keys. Read in SDL 3.4.20, a dead key under X11 (the circumflex of the French layout, `dead_circumflex`) has no symbol SDL knows and should take its place's code on a US layout (`[`), where Windows gives `^`; SDL's `main` branch maps the dead keys to their accents since 6 October 2026 (its commit "Add keycode mappings for keysyms corresponding to dead keys", which the 3.4 branch does not have). On the VM, before any change, the key showed `^` in the controls menu, as on Windows; the cause of the difference with the reading was not found. SDL's commit not taken (decided 27 November 2026): no defect seen.

The VMs' Release builds of each round the same day (logs `build/p4s3-linux-gcc-release.log`, `build/p4s3-linux-clang-release.log`, `build/build-msvc-release.log`): GCC 15, Clang 23 and MSVC without a warning, `ctest` 17 of 17 with both Linux compilers, the development machine's three clang presets without a warning and the six `modernjk` tests passed in each.

Tried again with the final sources the same day: on the Linux VM, GCC 15's and Clang 23's builds under X11 and under Wayland, the scene's reproduction, Alt+Enter with either Alt in a level with the mouse turning the view, Alt Gr typing `@`, `RALT` and `^` in the controls menu, `devmap mp/duel1`: passed; on Windows (the MSVC build), right Alt and Enter, Alt Gr typing `@`: passed, as before.

The full VM validations of sub-step 3 with its fixes, the same day (logs `build/validate-linux.log`, `build/validate-windows.log`): `validate.sh` ALL OK (GCC 15 and Clang 23: Release, Debug, AddressSanitizer, the server's preset, the Debug build with link-time optimization, the formatting) and `validate.bat` ALL OK (MSVC: Release, Debug, AddressSanitizer), `ctest` passed in every preset that runs it, no compiler warning. The scripted scenarios were not run again: they play the server and the bots, which this sub-step does not touch.

### 16.7 Where to resume (26 November 2026)

**The state.** Sub-steps 1 and 2 of part 4 are written, tried by hand in the three games, validated and committed (sections 16.4 and 16.5). Sub-step 3 is written, built on the VMs and tried by hand in the three games and on Linux under X11 and Wayland, with four fixes found on the way, and validated (section 16.6); sub-step 4 comes next, each built and checked before the next as the others were. The temporary settings `r_outputPrecision` and `r_outputDither` stay until sub-step 4 chooses.

**Sub-step 4: the measures, then the choice.** The banding in the dark tones of `RGBA8` against `RGBA16`, each with and without the dither (`r_outputPrecision`, `r_outputDither`), at `r_gamma` 1, 1.5, 2 and 3, on screenshots of dark places (the levels a dark gradient keeps, the gaps between them); the pass's cost at 100, 125, 144 (vsync) and 333 images a second, against the build of the commit before part 4, with the method of section 14.17 (PresentMon, the CPU of a demo played back, the machine quiet); then the precision and the dither chosen, the two temporary settings removed (the chosen kept as constants), `CHANGES.md` section 42's cost made a measure, the VM validations.

**What a next session needs to know**, beside section 15.3:

- A change of the display goes one way, whatever made it: the engine part's `Platform_Frame` applies the settings to the window, then `CL_DisplayChanged` tells the renderer (`DisplayChanged`), the pointer's image, the UI and the cgame (section 16.6). Something new that keeps the picture's size, in the client, a renderer or a module, is read again there.
- The stage's rules and files: `src/shared/rd-common/output/README.md` and `engine/README.md`. clang-tidy on its files: the command at the end of section 16.4.
- `modernjk_test_rd_common real` checks the pass on a real driver level by level; on the Linux VM it runs under X11 (XWayland, the default there) and under Wayland (`SDL_VIDEO_DRIVER=wayland`), Mesa's software driver giving every level exactly.
- Every read of the picture goes through `Output_MakeReadable` then `Output_DrawScene` (section 16.4's list): a new read of the screen added to a renderer must too, or it fails on a multisampled scene.
- A driver's conversion to 8 bits is not always the exact rounding (NVIDIA's, section 16.4): the dither stays within 0.375 of a level.
- No console command opens a setup menu's advanced video page (the multiplayer's `ui_openmenu` opens a menu, not its sub-pages): the menus' rows are checked by hand, in the six menus.
- The multisampling's row sits under the anisotropic filtering in each menu (section 16.5); a row added to those pages has little room left (the in-game multiplayer menu's "Apply" and the single player's driver information button sit right under it).
- Every texture name of both renderers and of the stage comes from the driver (`glGenTextures`), never chosen by the code: a name chosen by hand can be one Mesa has given another texture (section 16.6).
- A key that types nothing and whose symbol the layer does not name is named by its place (section 16.6): a layout's Alt Gr on the right Alt is `RightAlt` on every system.
- A session on the Linux VM by hand: the client of a Linux build, the game's archives of the shared folder, and a home of its own, `build/linux-p4s3`, whose `base/` holds the build's modules (`cgamex86_64.so`, `uix86_64.so`, `jampgamex86_64.so`) and its `z_ModernJK.pk3`, copied again from `build/linux-gcc-release/bin` when they change (the renderer is read beside the executable). VirtualBox's mouse integration off (its Input menu, the host key and I) to turn the view. The builds, on the VM:

  ```
  cmake -S /media/sf_jka/ModernJK --preset linux-gcc-release 2>&1 | tee /media/sf_jka/ModernJK/build/p4s3-linux-gcc-release.log
  cmake --build /media/sf_jka/ModernJK/build/linux-gcc-release 2>&1 | tee -a /media/sf_jka/ModernJK/build/p4s3-linux-gcc-release.log
  ctest --test-dir /media/sf_jka/ModernJK/build/linux-gcc-release 2>&1 | tee -a /media/sf_jka/ModernJK/build/p4s3-linux-gcc-release.log
  cmake -S /media/sf_jka/ModernJK --preset linux-clang-release 2>&1 | tee /media/sf_jka/ModernJK/build/p4s3-linux-clang-release.log
  cmake --build /media/sf_jka/ModernJK/build/linux-clang-release 2>&1 | tee -a /media/sf_jka/ModernJK/build/p4s3-linux-clang-release.log
  ctest --test-dir /media/sf_jka/ModernJK/build/linux-clang-release 2>&1 | tee -a /media/sf_jka/ModernJK/build/p4s3-linux-clang-release.log
  ```

  The client of each build under X11 (XWayland, the default there) and under Wayland:

  ```
  /media/sf_jka/ModernJK/build/linux-gcc-release/bin/modernjk.x86_64 +set fs_basepath /media/sf_jka/server +set fs_homepath /media/sf_jka/ModernJK/build/linux-p4s3 +set r_displayState 0
  SDL_VIDEO_DRIVER=wayland /media/sf_jka/ModernJK/build/linux-gcc-release/bin/modernjk.x86_64 +set fs_basepath /media/sf_jka/server +set fs_homepath /media/sf_jka/ModernJK/build/linux-p4s3 +set r_displayState 0
  /media/sf_jka/ModernJK/build/linux-clang-release/bin/modernjk.x86_64 +set fs_basepath /media/sf_jka/server +set fs_homepath /media/sf_jka/ModernJK/build/linux-p4s3 +set r_displayState 0
  SDL_VIDEO_DRIVER=wayland /media/sf_jka/ModernJK/build/linux-clang-release/bin/modernjk.x86_64 +set fs_basepath /media/sf_jka/server +set fs_homepath /media/sf_jka/ModernJK/build/linux-p4s3 +set r_displayState 0
  ```

  What X11 sends for a key, and the layout's symbols of a key, in a terminal of the VM: `xev -event keyboard` (the key pressed in its window), `setxkbmap -query; xmodmap -pke | grep -E "keycode 108 "`.
- The MSVC build on the Windows VM (PowerShell; `vcvars64.bat` through `cmd /c`, script execution being disabled there), then its install into the game folders, on the development machine:

  ```
  $vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  cmd /c "`"$vs\VC\Auxiliary\Build\vcvars64.bat`" && cd /d Z:\ModernJK && cmake --preset windows-msvc-release && cmake --build --preset windows-msvc-release" | Tee-Object -FilePath Z:\ModernJK\build\build-msvc-release.log
  ```

  ```
  & "E:\Jeux\Jedi Academy default - ModernJK\update_from_build.bat"
  & "E:\Jeux\Star Wars Jedi Knight II - Jedi Outcast\update_from_build.bat"
  ```

  The full validations, both VMs at once: `bash /media/sf_jka/ModernJK/scripts/validate.sh` on Linux, `Z:\ModernJK\scripts\validate.bat` on Windows (logs `build/validate-linux.log` and `build/validate-windows.log`; the second has come in ASCII and in UTF-16, `file` tells which). A log is read for `error` and `FAILED` first.
- A VM build's log is read for its errors too (`error`, `FAILED`), not only for its warnings and its tests: a failed build leaves the previous binaries, whose tests pass.

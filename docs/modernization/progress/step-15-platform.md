# Step 15: a new platform layer on SDL 3

Plan step 15 ([../PLAN.md](../PLAN.md)). The survey and the decisions P1 to P9 of 15 November 2026 are in [../surveys/platform-layer.md](../surveys/platform-layer.md). This page holds what was read and decided when the step started, on 25 November 2026, the code of parts 1 and 2 as it was written from that day on (section 14), and where to resume (section 15, 26 November 2026: the multiplayer's client is on the layer, committed the same day; the single player's engines written the same day, section 14.14, not yet built on the VMs nor committed).

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
4. the single player's engine (both games) on it, with Escape while a key is bound or a field edited tried in its menus (section 14.12); then `libs/SDL2`, `src/shared/sdl/` and the exception for SDL 2 in GUIDELINES section 9 removed, the root README's packages updated;
5. the menus: the four settings' rows and their texts, the rows removed, the presets no longer touching the display;
6. the checks in game (the plan's list), the measurements before (SDL 2) and after (asked on 25 November 2026: as many as can be made, list below), `CHANGES.md`, the VM validations.

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
- Whether Windows presents a borderless OpenGL window directly or through the compositor (P4): to measure in part 1.

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

## 15. Where to resume (26 November 2026)

### 15.1 The state

- The multiplayer's part is committed (26 November 2026: `feat(platform): step 15, parts 1 and 2, the multiplayer's client on a new platform layer`), after the validations and the scenarios below. What that commit holds: `libs/SDL3/` (1,494 files, new), `src/shared/platform/` (new), `src/shared/sys/sys_charset.cpp` (new), `test/platform/` (new), and about 46 files changed (`git status`): the build (`Dependencies.cmake`, `.gitignore`, `src/shared/CMakeLists.txt`, `src/codemp/CMakeLists.txt`, `test/CMakeLists.txt`), the multiplayer's client, renderer, UI and cgame, the shared system files, the journal's removal in both engines and the file system, the documents, `GUIDELINES.md`, the root `README.md`.
- Points 1, 2 and 3 of section 11 are written; point 3's six sub-steps are sections 14.4 (1), 14.8 (2), 14.10 (3), 14.11 (4), 14.9 (5), 14.13 (6), with the journal's removal (14.6), the pass against the C++ Core Guidelines (14.5) and the Escape fix (14.12). The multiplayer's client runs on the layer; so do the single player's two games since section 14.14 (26 November 2026, not committed: built and checked with clang on the development machine only). SDL 2, `src/shared/sdl/` and `libs/SDL2` are gone (section 14.15, the same day; the VM validations ALL OK without a warning; not committed).
- Checked: every sub-step by the clang presets Release, Debug and AddressSanitizer on the development machine and by the Release presets of GCC 15, Clang 23 and MSVC on the VMs, without a warning; each tried by hand in multiplayer on the MSVC build ("everything works"), but a key without a code of its own pressed by hand (left out).
- The scripted scenarios on the development machine, 26 November 2026 (`scenarios.sh step15mp`, the clang Release build; the journal's removal touched the file system the server uses): `golden: SAME`, in 2 min 42 s.
- The full VM validations, 26 November 2026 (logs `build/validate-linux.log`, `build/validate-windows.log`): `validate.sh` ALL OK (GCC 15 and Clang 23: Release, Debug, AddressSanitizer, the server's preset, the Debug build with link-time optimization, the formatting) and `validate.bat` ALL OK (MSVC: Release, Debug, AddressSanitizer), `ctest` 16 of 16 in every preset that runs it, no compiler warning; the only messages are the `wayland-scanner`'s of section 14.1, once per protocol and configuration.
- `CHANGES.md` section 39 is written for the multiplayer's part.

### 15.2 Next, in this order

1. Done on 26 November 2026: the validations ALL OK, the scenarios `golden: SAME`; the multiplayer's part is ready to commit.
2. Done on 26 November 2026: the multiplayer's part committed.
3. Point 4, the single player's engines (Jedi Academy and Jedi Outcast, one engine in `src/code`) on the layer. Written on 26 November 2026 (section 14.14): the worktree, the engine, the client, the UI, the renderer, Escape and the view are done; the VMs' builds and both games by hand passed the same day; the removals and the key codes' one definition are done (section 14.15), the full VM validations ALL OK the same day. Left of this point: the commit. The list as it was:
   - a worktree at the commit before this work first, kept for the measurements of point 6 (section 11);
   - the engine part `src/shared/platform/engine/engine.cpp` is compiled against each engine's headers: it uses the client's `cls`, `kg`, `Key_GetCatcher`, `CL_KeyEvent(key, down, int64_t)`, `CL_CharEvent`, `CL_MouseEvent(float, float)`, `CL_PointerEvent`, `Key_DynamicKey`, `SND_SetFocus`, `com_unfocused`; the single player's client (`src/code/client/`) has to give the same;
   - what was done to the multiplayer, to do again there: `Com_Frame` calling `Platform_Frame` (`src/code/qcommon/common.cpp:1427`), the renderer's imports (`src/code/rd-common/tr_public.h:94`, filled in `src/code/client/cl_main.cpp:1170`) and `src/code/rd-vanilla`, the keys' times and the mouse in floats (`src/code/client/cl_input.cpp`, `cl_keys.cpp`), the pointer (its image is the shader the single player's menus name as `cursor`: to read in each game's archives, as section 14.9 did; each UI's `MouseEvent` and drawn pointer), the character event in place of `K_CHAR_FLAG` (`src/code/client/cl_keys.cpp:1366`, `src/code/ui/ui_shared.cpp:8999`), the keys without a code, the console key's settings, Alt+Enter;
   - Escape while a key is bound or a field edited, to try in its menus and to fix as section 14.12 did (`src/code/ui/ui_main.cpp:3633`);
   - the single player's view that does not turn (section 8), looked at once it runs on the new input;
   - then the removals: `libs/SDL2`, `src/shared/sdl/` (its icon `sdl_icon.h`, which the engine part includes, moves into `src/shared/platform/engine/`), the target `modernjk_sys_client` and `modernjk_copy_runtime_dlls`, the definition `MODERNJK_PLATFORM` and every branch it chooses (`src/shared/sys/sys_main.cpp`, `sys_loadlib.h`, `sys_win32.cpp`, `sys_unix.cpp`), `K_CHAR_FLAG` (`src/shared/qcommon/q_limits.h`), the SDL 2 exception of GUIDELINES section 9, `libsdl2-dev` and SDL's row of the root README, clang-tidy's `-DSDL_DISABLE_ANALYZE_MACROS` (`cmake/StaticAnalyzers.cmake`);
   - the key codes in one definition for the trees (`src/codemp/ui/keycodes.h` and `src/code/client/keycodes.h` into `src/shared/qcommon`).
4. Point 5, the menus: the four settings' rows in the six setup menus and their texts in the games' languages (the sound device's row, `UI_SOUND_DEVICE`, is the model), the old rows removed, the quality presets no longer touching the display (section 9.2).
5. Point 6: the checks in game, the measurements before and after (section 11's list; the black moment at alt-tab in borderless fullscreen, section 14.7, with the presentation's tool), the Wayland driver on the Linux VM (`SDL_VIDEO_DRIVER=wayland`), `SDL_HINT_VIDEO_DOUBLE_BUFFER`, then the validations.
6. At the end of the step: one code for the clients' keys and input and the menus' shared code, if it can be (section 11); the sound's engine part against the C++ Core Guidelines (section 14.5); the menus' pointer placed exactly, in floats, where it is placed in whole units of the menus' 640 by 480 today (3 pixels on a screen 1,920 wide, 6 on one of 3,840: section 14.9's known limit), taken with the menus' one code (decided 26 November 2026: left as it is until then).
   - Asked on 26 November 2026, where it is needed: the layer's tests made stronger, fuzzers included. What has no test today: the engine part (`src/shared/platform/engine/engine.cpp`: the keys' codes from the layer's events, the name of a key without a code, `r_exclusiveMode`'s reading, `Platform_IsKeyName`), the client's `Key_DynamicKey` (its sixteen codes, the one taken back) and `CL_KeyState` (the share of a span a key was held), all compiled into the engine only: to test them they have to be reachable from a test, as the sound's are. What reads text or data a player's files or the system give, so what a fuzzer is for: a bind's key name and `r_exclusiveMode` (configuration files), a typed text (UTF-8 from the system, turned into the one-byte set), an image for the pointer or the icon (its size against its bytes), and any sequence of SDL's events through `Input::Poll` and of display states through `Window::Apply` on the dummy driver. The fuzzers' preset is `windows-clang-fuzz` (netcodec's and the sound's, `src/shared/sound/fuzz/` the model).
7. Then, each on a word: part 4 (the renderers' output stage), part 5 (the gamepad), part 6 (`SDL_DelayPrecise`).

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

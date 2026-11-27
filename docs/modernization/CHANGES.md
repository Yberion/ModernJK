# What this branch changes compared with OpenJK

Last update: 26 November 2026. ModernJK compared with OpenJK `master` at `260c59c29`. Multiplayer only (`src/codemp/` and the shared code it uses), but for the sound (sections 33 to 36 and 38), the seed of the random numbers (section 37) and the platform layer (section 40), which concern the single-player games of Jedi Academy and Jedi Outcast too.

This page lists every change of the branch, with its reason and what it gives. The work follows the goals stated in the [README](README.md): no loss of precision, no compatibility constraint, as few network settings as possible, a netcode other JKA forks can reuse, Windows and Linux. The measurements in full, and the history of each change, are in the step documents of [progress/](progress/README.md).

## How to read this page

- The **summary** is written for players: what changes in plain words, then the delays before and now, in milliseconds.
- Then **one section per change**, always in the same order: **Before** (OpenJK as it is), **Now** (this branch), **Why**, **Gain** (with its cost when there is one). Each section ends with **Technical details**, folded: the same before and now for readers who know the engine, with the file names. There is no code on this page.
- A number is **measured** when it comes from a statistics command in game (`netstats`, `svnetstats`, `predictmiss`, section 27) or from a benchmark. It is **computed** when it is derived from the rates and not measured; the page says which. The "test server" is a Linux dedicated server with one virtual core, played over the Internet at a ping of about 20 ms; the "local server" runs on the player's machine.
- "Before" is OpenJK. Where OpenJK's default differs from the base game's (the server rate: 40 frames per second in OpenJK, 20 in the base game 1.01), both are given: 20 is the rate the game was tuned on, and the reference of this work.

## Summary

### In short

1. **The server runs and sends 100 times a second**, where the base game ran 20 times and OpenJK runs 40 by default. The client sends 100 times a second too.
2. **The rules of the game still run 20 times a second**: saber, force powers, weapons, damage, doors and lifts behave exactly as on a server at `sv_fps 20`. The game is the same; it is shown and controlled with less delay.
3. **The other players are drawn 7 to 11 ms behind the newest news from the server**, where a 20 Hz server gave about 55 ms and OpenJK's default about 30 ms.
4. **Every player is equal**: the same 10 ms movement step and the same delay from a key to the server at 100, 125 or 333 frames per second. No frame rate jumps higher or acts sooner, and no setting gives an edge.
5. **Exact positions, nothing guessed**: the positions and velocities of the players and the missiles reach the client exactly as the server holds them, and the other players are never drawn where they might be.
6. **Shots land where you aim**: every projectile turns towards the point the eyes look at, so it lands at the centre of the screen; the NPCs and the bots aim at the chest of the model they see.
7. **Windows and Linux compute the same game, bit for bit**: the same math functions, random numbers and roundings on every system and compiler.
8. **Messages are about 2.5 times smaller**, which pays for most of the higher rate.
9. **No network setting is left**: `rate`, `snaps`, `cl_maxpackets`, `cl_packetdup` and `cl_timeNudge` are gone; the game computes or fixes their values.
10. **The sound is played by OpenAL Soft**, in the multiplayer and both single-player games: up to 64 sounds heard at once, chosen by how loud they are where you stand, instead of 32 channels where a new sound cut the oldest; every saber, shot and footstep from its own place, real Doppler, nothing played twice, `s_volume` the volume of everything (section [33](#33-the-sound-on-openal-soft)).
11. **Each map's reverb by zones**, as the original game played it on sound cards with EAX: the room you stand in, the rooms around through their doors, the walls and pillars muffling what is behind them; `s_reverb` switches it off (section [34](#34-the-reverb-by-zones)).
12. **The sound comes sooner**: about 50 ms from a sound asked to the system's mix at every frame rate, against 110 to 116 ms with the old mixer, a shot's sound sent before the screen waits for vsync, the sound's work on the main thread about 4 times smaller (section [35](#35-the-sound-measured-and-what-the-measures-changed)).
13. **The sound device in the menus**: the system's default output or the one you choose, followed when it is unplugged and plugged back (section [36](#36-the-sound-device-chosen-in-the-menus)).
14. **A run replayed in the three games**: with one seed and a fixed frame time, a single-player session of Jedi Academy or Jedi Outcast plays the same game and the same sounds again, and a multiplayer demo the same effects and sounds; a tool for testing, not a setting for play (section [37](#37-one-seed-for-every-random-number-stream)).
15. **The sound audited**: the walls heard once behind loops of one sound, the same walls for a player with the reverb off, the fans and beams of three single-player levels sounding again, the mouths following the lines in Force Speed, the sound back when a headset is plugged in after the game started (section [38](#38-the-sound-audited)).
16. **The price**: a branch client and an OpenJK server (or the reverse) do not understand each other, and demos of one do not play on the other.

### Delays before and now

#### The words

| word | meaning |
|---|---|
| server frame, tick | one step of the server: it moves the players, runs the game and sends the result. "Tick" is the name of the fixed step of this branch, 10 ms |
| snapshot | the message the server sends to a client: where everything is at that step |
| command | what the client sends to the server: the keys held, the buttons, the view direction, for one step of movement |
| ping, round trip | the time for a message to go to the server and for its answer to come back |
| prediction | the client moves your own player at once, with the same code as the server, without waiting for the server's answer |
| interpolation | the client draws the other players between two snapshots it has already received: a real position, slightly in the past |
| extrapolation | the client draws a player beyond the newest snapshot, going on by its last known speed: closer to the present, but a guess, wrong whenever the player turns |
| display delay | how far in the past the other players are drawn, counted behind the newest snapshot when it arrives |

#### What runs 100 times a second and what stays at 20

In OpenJK one cvar, `sv_fps`, sets everything at once: how often the players move, how often the game's rules run and how often snapshots leave. Raising it lowers the delay but changes the game, since the saber, the force powers and the timers count in server frames. This branch separates the two.

| 100 times a second (every tick, 10 ms) | 20 times a second (every fifth tick, 50 ms) |
|---|---|
| the movement of every player, one command per player | the saber: contacts, damage, blocks, bounces |
| the movement of the bots | the force powers and their timers |
| a snapshot to every client | the missiles, the items, the doors and lifts |
| a packet from every client | the bots' decisions, the level's rules (votes, end of a round, siege) |
| | pain, falling and drowning damage, the end of powerups |

In OpenJK's terms: the movement and the network behave as a server at `sv_fps 100` with `snaps 100`, and the rules of the game as a server at `sv_fps 20`. This is proven, not assumed: 40 scripted fights (saber, force powers, weapons) give the same counts of hits, damage, blocks and deaths at 20 and at 100 (section 1).

#### Delay by delay

The client side is OpenJK with its defaults (`cl_maxpackets 63`, `cl_timeNudge 0`, `cg_smoothClients 1`). Averages; the network trips are the same before and now and are left out.

| delay | OpenJK, server at 20 Hz (the base game's rate) | OpenJK, server at 40 Hz (its default) | this branch | section |
|---|---|---|---|---|
| Between two snapshots | 50 ms | 25 ms | 10 ms | [1](#1-server-tick-of-100-hz-with-the-rules-of-the-game-at-20-hz) |
| Other players drawn behind the newest snapshot (display delay) | about 55 ms (computed from the code; 53.0 ms measured with this branch's first rule, the same within 3 ms) | about 30 ms (computed: one interval plus 5 ms) | 7.0 to 7.8 ms at 125 frames per second, 10.5 to 11.4 ms at 333 (measured) | [4](#4-display-delay-of-the-other-players-computed) |
| Smoothing of the other players, added to it | a frame of their movement: 8 ms at 125 frames per second, 17 ms at 60 | the same | none | [5](#5-other-players-never-guessed) |
| When a snapshot is late or lost | the other players go on by their speed for up to 50 ms (a guess) | the same | they wait at their last real position; frames drawn past the newest snapshot: none without loss, 0.2 to 0.5 % with 2 to 9 % of loss (measured) | [5](#5-other-players-never-guessed) |
| From a key to its command | the next drawn frame: 4 ms at 125 frames per second, 1.5 ms at 333, 8 ms at 60 | the same | the next tick: 5 ms at every frame rate | [2](#2-one-command-per-tick) |
| From the command to its packet | at most 63 packets per second, so every second frame at 125 frames per second: 4 ms more (computed) | the same | none: a packet leaves with each command, 100 per second | [8](#8-network-settings-removed) |
| The command at the server | run when it arrives, but dated in the player's own past | the same | run at its tick, 3.6 to 3.8 ms after it arrives, the same at 100, 125 and 333 frames per second (measured) | [3](#3-commands-made-at-the-start-of-the-tick) |
| From your movement on the server to the snapshot that shows it | the next server frame: 25 ms | 12.5 ms | none: the snapshot of the same tick | [1](#1-server-tick-of-100-hz-with-the-rules-of-the-game-at-20-hz) |
| Length of a movement step | a drawn frame: 8 ms at 125 frames per second, 3 ms at 333 | the same | 10 ms for everyone | [2](#2-one-command-per-tick) |
| A decision of the game (a saber hit, damage, a block) | at the next server frame: 25 ms | 12.5 ms, at a rate the game was not tuned on | at the next 20 Hz instant: 25 ms, as the base game | [1](#1-server-tick-of-100-hz-with-the-rules-of-the-game-at-20-hz) |
| The ping shown | counted in server frames: steps of 50 ms | steps of 25 ms | the real round trip, to the millisecond | [7](#7-ping-measured-in-real-time) |
| A player's model loaded during the game (at the first scoreboard, or when you die: the client stops and sends nothing meanwhile) | more than 71 ms per model (measured with the textures compressed by the driver, as OpenJK does, and this branch's image decoder, which is faster than OpenJK's) | the same | 20 ms per model (measured: ten models in 202 ms) | [25](#25-client-fixes-outside-the-netcode) |
| A change of the display applied (Alt+Enter, a resolution, a screen) | a restart of the video that makes the window again: 493 to 732 ms (measured with this branch's build on SDL 2, the same code) | the same | 5 to 18 ms, nothing restarted (328 to 418 ms with step 15's first restart of the video in the same window) | [42](#42-the-pictures-output-stage) |

The whole chain, from your key to another player's screen, both clients at 125 frames per second, in milliseconds (sums of the averages above, computed, not measured end to end):

```
OpenJK, server at 20 Hz
  key -4-> command -4-> packet ==trip==> server -25-> snapshot ==trip==> client -55+8-> screen      96 ms + the trips

OpenJK, server at 40 Hz
  key -4-> command -4-> packet ==trip==> server -12-> snapshot ==trip==> client -30+8-> screen      58 ms + the trips

This branch
  key -5-> command and packet  ==trip==> server  -4-> snapshot ==trip==> client   -7-> screen       16 ms + the trips
```

What it means in a fight:

- **You see the others sooner.** On your screen your own player is at the present and the others are in the past. That gap was the network round trip plus about 55 ms on a 20 Hz server; it is now the round trip plus about 11 ms: 20 ms measured at 125 frames per second on the test server (its shown ping of 20 ms includes the wait for the client's next packet). The server judges saber hits with the positions it holds now, so the smaller the gap, the closer what you aim at is to what the server judges.
- **You learn sooner what happened.** A hit, a block or a death decided by the server is shown about 45 ms sooner than on a 20 Hz server.
- **The others see you sooner**, by about 80 ms against a 20 Hz server and 40 ms against OpenJK's default (the chain above).

What did not get faster, and what it costs:

- The game decides 20 times a second, as the base game does. A saber contact is judged at the next 20 Hz instant, 25 ms later on average: the same as a 20 Hz server, 12 ms more than OpenJK's default of 40, which changes the game's balance.
- A key acts at the next tick, not at the next drawn frame: 5 ms on average instead of 4 ms at 125 frames per second and 1.5 ms at 333. A player at 333 frames per second loses 3.5 ms on its own screen; in exchange every player has the same delay and the same movement.
- There is no lag compensation: a hitscan shot (the disruptor) is judged with the positions the server holds, as before. It was planned for the hitscan shots only (plan step 14, part 8) and dropped on 14 November 2026.
- Bandwidth, measured with 17 or 18 bots: 90 to 115 bytes per snapshot, 100 times a second, about 12 to 15 KB per second down for each client with the packet headers, and about 4.5 KB per second up. OpenJK's format on the same messages would be 2.4 times larger.
- Server CPU, measured on the test server (1 virtual core) with 16 bots and no player: 4.1 % of the core at 100 Hz (5.3 % before the optimisation of plan step 14, part 7, which takes about a third off the time of a tick; section [23](#23-server-fixes)).

#### Prediction, interpolation, extrapolation

| | before | now |
|---|---|---|
| Your own player (prediction) | predicted at every drawn frame, one movement step per frame | predicted tick by tick and drawn between the last two ticks at the frame's exact time: as smooth, the same steps as the server's. Measured against the server: every field right in more than 97 % of the ticks; the position differs in 0.4 % of the ticks, all of them in contact with another player (section [6](#6-prediction-of-your-own-player)) |
| The other players (interpolation) | between two snapshots when the next one is there, then smoothed | always between two snapshots, 100 real positions a second, no smoothing |
| Extrapolation | when the next snapshot is late; and by choice, with a negative `cl_timeNudge` | never. The delay grows by itself on a link with jitter or loss, so that the next snapshot is nearly always there |

### List of changes

Each line links to its section.

**Responsiveness and equality**

| change | before | now |
|---|---|---|
| [1. Server tick](#1-server-tick-of-100-hz-with-the-rules-of-the-game-at-20-hz) | one rate for everything, a setting: 20 or 40 frames per second | 100 ticks per second for the movement and the network, the game's rules at 20, no setting |
| [2. One command per tick](#2-one-command-per-tick) | a command per drawn frame: the movement depends on the frame rate | a command per tick: the same movement for everyone |
| [3. Commands at the start of the tick](#3-commands-made-at-the-start-of-the-tick) | a key waits for the next drawn frame | the same delay at every frame rate |
| [4. Display delay computed](#4-display-delay-of-the-other-players-computed) | about one snapshot interval; `cl_timeNudge` to shift it | as short as the link allows, computed; no setting |
| [5. Other players never guessed](#5-other-players-never-guessed) | smoothed, and carried on by their speed when a snapshot is late | drawn between two real positions only |
| [6. Prediction of your own player](#6-prediction-of-your-own-player) | wrong on lifts at a high server rate, and on a few animations | measured field by field and corrected |
| [7. Ping](#7-ping-measured-in-real-time) | counted in server frames | the real round trip |
| [8. Network settings](#8-network-settings-removed) | `rate`, `snaps`, `cl_maxpackets`, `cl_packetdup`, timeouts | fixed values, nothing to set |

**Precision: the same game on every machine**

| change | before | now |
|---|---|---|
| [9. Player velocity](#9-player-velocity-never-rounded) | rounded at every step: jumps depend on the frame rate | exact |
| [10. Missiles and roundings](#10-missiles-exact-and-one-rounding-rule) | missiles rounded; the rounding differs between compilers | missiles exact; one rule everywhere |
| [11. Math functions](#11-same-math-functions-on-every-platform) | the system's: other results on Windows and Linux | one library: the same bits everywhere |
| [12. Random numbers](#12-same-random-numbers-on-every-platform) | another sequence on each system | one generator: the same numbers everywhere |

**Network format and memory**

| change | before | now |
|---|---|---|
| [13. Messages from the server](#13-messages-from-the-server-coded-by-a-new-codec) | Huffman coding | a new codec: about 0.4 of the bytes, values exact |
| [14. Packets from the client](#14-packets-from-the-client-coded-by-the-new-codec) | Huffman coding | the same codec: about 0.25 to 0.4 of the bytes |
| [15. Server commands](#15-server-commands-sent-once) | repeated in every message until acknowledged | sent once, sent again only when lost |
| [16. Lost gamestate](#16-lost-gamestate-sent-again-quickly) | sent again after a second | sent again after about a round trip |
| [17. Snapshot history](#17-snapshot-history-of-128-messages) | 32 messages | 128 messages: a long loss or a long ping keeps small snapshots |
| [18. Entity store of the server](#18-entity-store-of-the-server-shared) | a copy per client and per snapshot; a forced map restart after hours | one copy while a state does not change; less memory, no restart |
| [19. Messages too large](#19-messages-too-large) | sent empty | sent with the nearest entities |
| [20. Obfuscation](#20-obfuscation-removed) | messages scrambled with a guessable key | not scrambled |
| [21. Demos](#21-demos) | a copy of the network messages | coded for the demo, one snapshot in two, no event lost; old demos do not play |
| [22. Downloads](#22-downloads-over-udp) | several blocks per message, by the rate | one block per message |

**Fixes found on the way**

| change | before | now |
|---|---|---|
| [23. Server](#23-server-fixes) | an idle Linux server used a tenth of a core; a clock change could freeze it; work repeated for nothing at every tick | it sleeps; a clock that only moves forward; about a third less time per tick |
| [24. Game](#24-game-fixes) | three random choices differed between compilers; a zoomed animation restarted at every step; the rocket launcher's homing lock took cloaked players and refused pulled ones; an idle animation went on through a deflected shot; the selected force power lost after a map change; the single player's absorb gave back the wrong amount; a character typed in a menu's field also moved the focus; a menu's command without its argument read a pointer never set; logs with "(null)"; a weapon picked up gave the wrong amount of ammo; an unknown weapon name in a script or a map; an NPC's pain delay while gripped, and a creature's twice its animation; the single player's vehicle flashes and datapad weapons; a weapon's charge counted with the ping's gap, then up to 50 ms short | fixed; a charge is exactly the time held |
| [25. Client](#25-client-fixes-outside-the-netcode) | shots louder at a high frame rate, menu cursor too fast, keypad binds, servers hidden from the browser; images decoded by another library on Windows and on Linux; the pk3 archives read through libraries of 2010 and 2013, the system's on Linux; every texture compressed by the graphics driver as it loads | fixed; one image decoder everywhere, more than twice as fast; one reader of the archives everywhere, 1.8 times as fast; a player's model loaded in 20 ms instead of 71 |
| [26. Shots](#26-the-shots-towards-the-point-aimed-at) | from a fixed point beside the eyes, parallel to the view; the NPCs and the bots fire beside what they aim at, the emplaced gun beside its crosshair | from the same point towards the point the eyes aim at, for every shooter; the crosshair where the shot lands |

**The sound**

| change | before | now |
|---|---|---|
| [34. The reverb by zones](#34-the-reverb-by-zones) | no reverb (the original's, through EAX, removed with its 32-bit OpenAL path) | each map's reverb by zones, the neighbouring rooms through their doors, the walls and pillars muffling; `s_reverb` |
| [33. The sound on OpenAL Soft](#33-the-sound-on-openal-soft) | a software mixer with 32 channels shared by every sound, a 1995 MP3 decoder, SDL 2's device | OpenAL Soft: 64 voices mixed, chosen by loudness, the others followed silently; real Doppler; the reference decoders; the system's device followed; the loudness after the standard for games, ASWG-R001 (-24 LUFS at the default settings) |

**The three games alike**

| change | before | now |
|---|---|---|
| [37. One seed for every random number stream](#37-one-seed-for-every-random-number-stream) | a fixed seed for the multiplayer's game only (`g_debugSeed`); none in the single player | `com_debugSeed`: every stream of the three games, each its own sequence; a run replays its game and its sounds |

**The sound audited**

| change | before | now |
|---|---|---|
| [38. The sound audited](#38-the-sound-audited) | a wall heard twice behind loops of one sound, a fixed zone with the reverb off, three levels' fans and beams silent or wrong, the lips behind the voices in slow motion, no sound for a game started without a device | each of 40 defects found by an audit and checked in the code, fixed and tested |

**The platform layer**

| change | before | now |
|---|---|---|
| [39. The multiplayer's window and input on a new platform layer](#39-the-multiplayers-window-and-input-on-a-new-platform-layer) | SDL 2 behind a layer of 2005: keys dated by the frame and rounded to the millisecond, the mouse rounded to whole counts and held in the menus, a window created again at each restart of the video, thirteen fixed video modes | a layer written from zero on SDL 3.4.20, linked into the client: keys dated in nanoseconds when they move, the mouse in floats, the system's pointer in the menus, one window kept, borderless fullscreen by default and the screen's own modes |
| [40. The single player's two games on the platform layer](#40-the-single-players-two-games-on-the-platform-layer) | Jedi Academy's and Jedi Outcast's single player still on SDL 2, with `SDL2.dll` beside them; on this branch their view turned neither with the mouse nor with the keys | both on the same layer as the multiplayer, with what section 39 lists; the view turns again |
| [41. The display's rows in the setup menus](#41-the-displays-rows-in-the-setup-menus) | a video mode among eight fixed sizes, a colour depth and full screen on or off, which the quality presets also changed | the display (window, borderless, exclusive), the screen, the resolution and the refresh rate from the screen's own modes; the presets leave the display alone |
| [42. The picture's output stage](#42-the-pictures-output-stage) | the brightness by the screen's ramp, for the whole desktop, given back at each alt-tab and missing from the screenshots (on this branch since section 39, baked into the textures after a restart of the video), with bands in the dark tones once raised; the multisampling in the console only, after a restart; Alt+Enter and a new resolution restarting the renderer and the modules | the game draws into an image of its own and one pass draws it into the window with the brightness, at once, on the whole picture and in the screenshots, the dark tones kept (16 bits a channel, a faint fixed dither); the multisampling an Anti-aliasing row of the menus, applied at once; a change of the display applied at the next image without a restart |

**The three games written once**

| change | before | now |
|---|---|---|
| [43. The keys and the input written once for the three games](#43-the-keys-and-the-input-written-once-for-the-three-games) | the keys, the console's line, the binds and the turn of the view written twice, once for the multiplayer and once for the single player, which differed: the single player's right Shift, Ctrl and Alt named as the left ones, so their binds came back on the left keys; Home in a long console line leaving the cursor out of sight in the single player, Ctrl-E hiding the line's last character in the three games; one mouse acceleration in the single player | one code for the three games: the right keys named apart everywhere, the cursor kept in view after every key, the console's line the same in the three games, both mouse accelerations everywhere |

**Reference**: [27. Settings and commands](#27-settings-and-commands), [28. Build, tests and repository](#28-build-tests-and-repository), [29. Name: ModernJK](#29-name-modernjk), [30. Compatibility](#30-compatibility), [31. C++20, 64-bit Windows, Linux and macOS](#31-c20-64-bit-windows-linux-and-macos), [32. Not changed yet](#32-not-changed-yet).

## A. Responsiveness and equality

### 1. Server tick of 100 Hz with the rules of the game at 20 Hz

**Before.** The cvar `sv_fps` sets how many frames per second the server runs: 20 in the base game, 40 by default in OpenJK. A frame does everything: it runs the game's rules once and sends a snapshot. A higher value shows the game sooner but changes it, because the game was tuned on frames of 50 ms: the saber is traced once per frame, and the force powers, the timers and the bots count in frames.

**Now.** The server runs 100 ticks per second, a constant with no setting. At every tick it moves each player with that player's command and sends a snapshot to every client. Every fifth tick is a "20 Hz instant": the rules of the game run there, with OpenJK's code.

**Why.** To cut the delay without changing the game. Running OpenJK's game code unchanged at 100 Hz breaks it (saber contacts 5 times shorter, damage nearly always 1 point, timers up to 44 % more frequent), and tuning that code tick by tick failed: the saber's reactions need a 20 Hz frame's worth of blade movement.

**Gain.**

- A snapshot every 10 ms instead of every 50 ms (20 Hz) or 25 ms (40 Hz).
- The same game as at `sv_fps 20`. Proof: 40 scripted fights between two bots (saber: 5 styles, defender idle or attacking; 6 force powers; 12 weapons with both fire modes) give the same wounds, damage, deaths, blocks, bounces, shots and hits at 20 and at 100, on Windows (clang, MSVC) and on Linux (GCC). A free-for-all of 16 bots shows no difference beyond the noise (damage -2.8 % with a margin of 1.8 %).
- Cost: server CPU (4.1 % of the test server's core with 16 bots and no player, measured; an empty server 1.1 %), bandwidth (summary), and demos, which hold 100 messages per second instead of 20 or 40 (their size is not measured yet: plan step 14, part 11).

<details>
<summary>Technical details</summary>

| | OpenJK | this branch |
|---|---|---|
| Server rate | `sv_fps`, default 40 | `TICK_MSEC` 10 (`qcommon/q_shared.h`); the cvar is removed |
| Game clock | `level.time`, one step per frame | two clocks (`game/g_local.h`, `g_main.c`): `level.time`, read by all game code, is the logic clock, the last multiple of 50 ms; `level.tickTime` is the tick's time, used only for the bots' command times and the sanity clamp of a command's time |
| On the 20 Hz instants | everything, every frame | thinks, the saber, the force powers and missile blocks, space, hacking, jetpack and cloak fuel, the end of powerups, world damage (lava, drowning) and the pain feedback, the level's rules (votes, exit rules, tournament, siege waves and timers), the bots' AI, all missiles, items and movers (doors, lifts, trains: movement, pushes, crush damage) |
| Every tick | | the players' commands, the bots' commands, the snapshots |
| Clock of the server's models (ghoul2: bolts and model traces) | `sv.time`, set after the frame | the logic instant of the frame just run (`server/sv_main.cpp`), so an instant evaluates the models 50 ms after the previous one, as every frame did at 20 Hz. With `sv.time` at 100 Hz the blades ran 40 ms ahead of their 20 Hz path (5 times less saber damage) |
| Missiles | moved every frame | moved on the 20 Hz instants only: the engine's model trace depends on the step, so per-tick steps changed which shots hit, where, and the damage (the bryar 38 % less, the homing rocket lethal every time) |
| Bots | their AI and one movement per server frame, inside the frame | one command per tick, run as a player's; their AI every 50 ms on the instants. The debug cvar `bot_debugClientRate` is removed |
| After a stall | catches up to 5 s of frames at once | at most 25 ticks (250 ms); the rest is dropped with a "Hitch" message and the clients follow the server's clock as after a `map_restart` |
| `map_restart`, map load | three frames of 100 ms | the same |
| Client | takes the snapshot interval from the server info | from the constant; `timedemo` advances one tick per frame (50 ms before) |

What OpenJK's code does when run unchanged at 100 Hz, and what this branch does:

| what | OpenJK's code at 100 Hz | this branch at 100 Hz |
|---|---|---|
| Saber traces, damage, bounces | every frame; contacts 5 times shorter, damage nearly always 1 point per contact, attacks rarely bounced | on the 20 Hz instants: the 20 Hz damage, bounces and blocks; a contact is found at the instant after it, up to 40 ms later than a per-tick trace would |
| Force power timers (grip, rage, protect, absorb drains, grip damage, force jump charge, seeker drone) | re-armed with "now + n", so every n + 10 ms instead of n + 50 | the 20 Hz lengths |
| Mind trick broken by an attack in view | within 4 frames (40 ms) | within 200 ms (written as a time: the frame-length variable is removed) |
| Pain sound and view kick | 10 points within one frame, which a blow spread over 5 frames rarely reaches | 10 points within a 50 ms window |
| Movers (doors, lifts) | move and crush every frame | on the 20 Hz instants; the client places them accordingly (section 6) |

The scenarios: the server command `scenario` and the script `tools/gamestats/scenarios.sh` (`scenarios40.sh` until plan step 20 widened it), which compares the cases with recorded counts (section 27). Details: [progress/step-14-fixed-tick.md](progress/step-14-fixed-tick.md) sections 9 and 10, [progress/step-14-audit-100hz.md](progress/step-14-audit-100hz.md) sections 9 to 11.

</details>

### 2. One command per tick

**Before.** The client makes one command per drawn frame, and a command moves the player for the length of that frame: 8 ms at 125 frames per second, 3 ms at 333. The movement code does not give the same result for different step lengths, so the frame rate changes the jumps and the air control. The server runs each command when its packet arrives, in the order of arrival. Each command is dated with the time the client draws the world at, which is in the past: on the server every player lives in its own past, behind by its ping and its display delay. A player can hold its commands back and send up to a second of them at once. When a packet is lost, the player stops on the server until the next one.

**Now.** The client makes one command per tick, 100 per second at any frame rate; the view still turns at every drawn frame. Every command moves the player for 10 ms. The server runs exactly one command per player at each tick, the one made for that tick. To make that possible the client runs a second clock, the command clock, slightly ahead of the server: far enough for a command to arrive just before its tick. The server tells the client in every snapshot how early its command arrived (the input margin), and the client speeds its clock up or slows it down by a few percent to keep that margin small.

- Order within a tick: the players run from the lowest client number to the highest at one tick and in the reverse order at the next, so of any two players each is first every other tick.
- A command that arrives after its tick gets no movement, and what it newly pressed (a button, a jump, a force power) goes into the next one: a short press acts a tick late instead of being lost.
- A tick without a command runs the previous one again, for 250 ms at most, then an empty one: a lost packet no longer stops the player, a lost connection does.

**Why.** Equality: the same movement for everyone, every player at the same time on the server, no way to bank commands. It is also what lag compensation by tick would need (planned as step 14 part 8, dropped).

**Gain.**

- The same movement step for every player, by construction (not re-measured as jump heights in game).
- Measured on the test server (ping 20 ms): every tick ran with the player's own command, except 1 or 2 in 100 s. With 5 % of the client's packets lost: 0.34 % of the ticks without it.
- A charged shot is fair: the charge's start and the server's clock are now the same clock. In OpenJK a charge was longer by the player's latency.
- A third of the commands to predict and to run at 333 frames per second.
- Cost: a key acts at the next tick, 5 ms on average, against 4 ms at 125 frames per second and 1.5 ms at 333 with a command per frame.

<details>
<summary>Technical details</summary>

| | OpenJK | this branch |
|---|---|---|
| Commands made | one per frame (`CL_CreateNewCommands`), dated `cl.serverTime`, the drawn time | one per tick of the command clock, made as the clock enters the tick with the input gathered over the tick before, dated with the time that tick ends; below 100 frames per second a frame makes several, and its generic command (a style change, a force power) goes in the first only |
| Command clock | none | `cl.cmdTime = cls.realtime + cl.cmdTimeDelta` (`client/cl_cgame.cpp`). The client keeps the margins of the last 2 s (64 slots of 32 ms) and moves the clock to where the second smallest is 3 ms (`CMD_MARGIN_MSEC`), by at most about 6 % faster or 7 % slower (Counter-Strike 2's limits), or at once at a connection or when the target is more than 100 ms away |
| Input margin | none | measured by the server in real time between a command's arrival and its tick, sent once in the next snapshot (one byte until the tables are retrained; to the millisecond up to 64 ms either way, steps of 8 ms beyond; `INPUT_MARGIN_NONE` when nothing was measured) |
| Commands run | on arrival, in `SV_UserMove` | kept in a buffer per client (`SV_INPUT_BUFFER` 256 commands), run by `SV_RunClientCommands` in the tick loop after the bots' commands and before the game frame (`server/sv_client.cpp`) |
| Order of the players | the packets' order of arrival | rising client numbers at one tick, falling at the next; the bots before the players, in the order of their numbers; the saber and the force powers from client 0 to 31 on the 20 Hz instants, as OpenJK |
| A late command | cannot exist | no movement time; its new presses (buttons, jump, generic command) go into the next command run, as Counter-Strike 2 does |
| No command | the player does not move; with `g_forceClientUpdateRate` 250 an empty command after 250 ms | the last command again (without its generic command) for 250 ms, then an empty one every tick |
| Commands held back | up to a second of them can be sent at once, and all run on arrival | one command runs per tick; a command dated more than 1 s ahead is not kept |
| `g_synchronousClients`, `g_forceClientUpdateRate` | cvars | removed (`G_RunClient` too): every player runs one command per tick and the client always predicts |
| Commands the client keeps | 64 (`CMD_BACKUP`); "Connection Interrupted" past them | 256, that is 2.56 s |
| Times the movement code writes (weapon charge, rocket lock, zoom) | on the client's past clock | on the game's clock; cgame compares them with the command clock (`cg.cmdTime`) instead of the drawn time |
| Random numbers of the movement code | seeded with the command's time | seeded with the command's tick (`PmoveSingle`, `game/bg_pmove.c`) |
| A move of -128 (the keys give -127 to 127; mouse or joystick sums, or a modified client, can give -128) | read back as -127 by the server | never made: the client's bound (`ClampChar`) is -127 to 127, and the server turns a -128 it receives into -127 |

Measured in game (13 November 2026, `svnetstats` and `netstats`):

| | ticks run with the player's command | commands after their tick | client packets lost |
|---|---|---|---|
| local server, 125 frames per second | 10,383 of 10,383 | 0 | 0 |
| local server, 333 frames per second | 10,418 of 10,418 | 0 | 0 |
| test server, 125 frames per second | 9,384 of 9,385 | 1 | 0 |
| test server, 333 frames per second | 10,104 of 10,106 | 2 | 0 |
| test server, 125 frames per second, 5 % of the client's packets dropped | 13,218 of 13,263 | 43 | 659 |

`cg_showmiss` also prints "shown flicker" when an animation or the crouch shown goes back and forth over three frames, and "anim miss" when a snapshot's animations differ from those the client predicted for the same command time. Details: [progress/step-14-fixed-tick.md](progress/step-14-fixed-tick.md) section 11.

</details>

### 3. Commands made at the start of the tick

**Before.** A command is made when a frame is drawn. A key pressed just after a frame waits for the next one, so the wait depends on the frame rate: on average 5 ms at 100 frames per second, 4 ms at 125, 1.5 ms at 333.

**Now.** The command of a tick is made and sent at the moment the tick begins, between two drawn frames if need be: the game then reads the keys and the mouse and sends the packet, without drawing.

**Why.** The client is for every player. A delay that depends on the frame rate favours the players with the fastest machines.

**Gain.** Measured on the test server (ping 20 ms):

| frames per second | input margin on the server | lowest | commands after their tick |
|---|---|---|---|
| 100 | 3.6 ms | 1 ms | 0 |
| 125 | 3.7 ms | 2 ms | 0 |
| 333 | 3.8 ms | -1 ms | 2 of 9,017 |

- The same delay from a key to the server at every frame rate.
- Against one command per tick made at the drawn frames (this branch before the change, 125 frames per second): the margin was 7.3 ms, so a key reaches the server 3.6 ms sooner on average, and your own player is drawn 8 ms less ahead of the others (20 ms instead of 28).
- The packets leave at the ticks, so the ping is steady and the server codes its snapshots against a base one message newer.
- Cost: the wait before a tick ends with a busy loop of up to a millisecond, up to a tenth of a core more on the player's machine. The mouse filter (`m_filter`) and the mouse acceleration read the mouse at the ticks too, so on shorter spans at a low frame rate.

<details>
<summary>Technical details</summary>

- `Com_Frame` (`qcommon/common.cpp`): the wait for the next frame also ends when the command clock enters a tick (`CL_NextCommandMsec`). The input alone then runs, `CL_InputTick` (`client/cl_input.cpp`): the system's events, the key binds (skipped while a `wait` command counts frames), the view's turn, the tick's command and its packet.
- The frames still make the commands that are due when they run (a tick that begins while a frame is drawn, a frame rate with no wait).
- Not with a scaled or a fixed time (`timescale`, `fixedtime`), in a demo or in a pause.
- The input code measures on a clock of its own (`s_inputTime`) instead of the frame's time: the keys' held time, the time since the input was last read, the time since the last command.
- The dedicated server has the two functions as stubs (`null/null_client.cpp`).
- Details: [progress/step-14-fixed-tick.md](progress/step-14-fixed-tick.md) section 14.

</details>

### 4. Display delay of the other players computed

**Before.** The client draws the world slightly in the past, so that it nearly always holds a snapshot before and after the drawn time. OpenJK's rule moves the client's time forward a little at each snapshot and back when a frame comes too close to the newest one: it settles about 5 ms further back than needed, and with jitter it often passes the newest snapshot and guesses. The player can shift the result by up to 30 ms with `cl_timeNudge`; a negative value means guessed positions, and an edge over a player who does not use it.

**Now.** The client measures, for the last 2 s, how close to the newest snapshot it could have drawn without passing it, and stays 2 ms behind that. On a link with jitter or loss the delay grows by itself, by about the jitter. `cl_timeNudge` is removed.

**Why.** The least delay that still shows real positions, the same rule for every player, no setting.

**Gain.** Measured with `netstats` (delay behind the newest snapshot when it arrives; a frame "past the newest snapshot" is a guessed one):

| | delay | jitter | frames past the newest snapshot |
|---|---|---|---|
| local server, 125 frames per second | 7.8 ms | 2 ms | 0 of 7,320 |
| local server, 333 frames per second | 10.5 ms | 1 ms | 0 of 7,948 |
| test server, 125 frames per second | 7.3 ms (7.0 ms in a later session) | 0 ms | 0 of 5,457 |
| test server, 333 frames per second | 11.4 ms (10.6 ms in a later session) | 1 ms | 0 of 8,110 |
| test server, 125 frames per second, 2 % of the snapshots dropped | 14.5 ms | 8 ms | 28 of 6,943 (0.4 %) |
| test server, 125 frames per second, 9 % dropped | 19.5 ms | 16 ms | 38 of 7,321 (0.5 %) |
| the same server at 20 Hz, this branch's first rule, no loss | 53.0 ms | 2 to 4 ms | 0 of 2,734 |
| the same at 20 Hz, 9 % dropped | 102.8 ms | 4 ms | 7 of 1,153 (0.6 %) |

- The delay depends on the frame rate: no frame is drawn past the newest snapshot, so the last frame before the next snapshot arrives sets it. At 100 frames per second a frame lasts exactly one snapshot interval, and the delay moves between about 2 and 10 ms over a game (2.1 ms measured in one session).
- OpenJK's own rule was not measured: about 55 to 56 ms at 20 Hz, computed from its code.

<details>
<summary>Technical details</summary>

| | OpenJK | this branch |
|---|---|---|
| Rule (`CL_AdjustTimeDelta`, `client/cl_cgame.cpp`) | 1 ms forward at each snapshot, 2 ms back after a frame within 5 ms of the newest snapshot: it settles where that happens in one interval out of three | at every frame the client's time must not pass the newest snapshot's. Each interval between two snapshot arrivals gives a sample: how far ahead the last frame before the next snapshot could have been. The client keeps the smallest sample of each of the last 64 slots of 32 ms and aims 2 ms under the second smallest |
| Speed of the correction | 1 ms forward or 2 back per frame that brought snapshots | at most about 6 % faster or 7 % slower than real time |
| A late snapshot | pushes the client past the newest snapshot | one in 2 s does not raise the delay, two do |
| Loss | no rule | a lost snapshot gives a lower sample, so loss that comes more than once in 2 s raises the delay by itself |
| An interruption of more than 100 ms | | gives no sample |
| A jump of the server's clock (`map_restart` runs three frames of 100 ms) | the client drifts back in a few snapshots | followed at once, measured on the second snapshot that shows it |
| The player's choice | `cl_timeNudge`, -30 to 30 ms | removed. Debug cvars `cl_debugInterpForce` and `cl_debugInterpDelay` force a delay; they are cheat-protected (reset unless the server sets `sv_cheats 1`) |
| The client's own ping (`snap.ping`, the lagometer) | over its last 32 packets, with the last frame's time: 999 above 320 ms of round trip at 100 Hz, up to a frame short | over its last 256 packets, in real time |

The first rule of this branch (plan step 11) took the second latest arrival of the last 64 snapshots minus a full interval, plus one interval above 5 % of loss. The present one (plan step 14, part 4e) gives 5 to 8 ms less delay at 60 and 125 frames per second and about 1 ms less at 333, simulated on the same arrivals (`tools/clock/sim_clock.py`). Survey of what other games do: [surveys/network-settings.md](surveys/network-settings.md), point 5. Details: [progress/step-14-fixed-tick.md](progress/step-14-fixed-tick.md) section 11, "4e".

</details>

### 5. Other players never guessed

**Before.** With the defaults (`cg_smoothClients 1`, `g_smoothClients 1`) another player is drawn between two snapshots when the next one has arrived, and carried on by its speed for up to 50 ms when it has not: a guess, wrong if the player turned. Its drawn position is also smoothed: at each frame it moves half of the remaining distance, so a moving player is drawn behind by a frame of its movement, and a sudden change of direction is spread over 3 to 4 frames.

**Now.** The other players and the NPCs are always drawn between two snapshots, without smoothing. While a snapshot is missing a player stays at its last real position. Both cvars are removed.

**Why.** The least delay comes first, and nothing is guessed. With 100 snapshots per second the smoothing has nothing left to hide.

**Gain.**

- The other players are drawn a frame sooner: 8 ms at 125 frames per second, 17 ms at 60.
- A change of direction shows at once.
- Cost: with loss, a player stops for the length of the missing snapshot (10 ms each) instead of sliding on. Tested in game with 30 % of the snapshots dropped: playable, nothing wrong reported.

<details>
<summary>Technical details</summary>

- cgame: `CG_CalcEntityLerpPositions` (`cgame/cg_ents.c`) always interpolates, which is what `cg_smoothClients 0` did. The filter on the drawn origin (`cgame/cg_players.c`) is gone; it stays for a player held or thrown by another.
- Game module: `g_smoothClients` is removed and the player's entity state keeps the form of its default (the velocity and the command's time, `BG_PlayerStateToEntityStateExtraPolate`). So the messages do not change: the same bytes, and the form the codec's position prediction and tables were made for.
- This removal takes the other value than the default of `cg_smoothClients`,'s decision, against the usual rule (a removed cvar keeps its default's behaviour).
- Details: [progress/step-14-fixed-tick.md](progress/step-14-fixed-tick.md) section 13.

</details>

### 6. Prediction of your own player

**Before.** The client predicts your own player with the same movement code as the server, and corrects it when the server's answer differs. Nothing measures how often it differs beyond the position. Several things make it differ:

- on a lift at a server rate above 20 Hz, the client and the server do not place the lift at the same height;
- an animation left and taken again within one step is restarted by the server only;
- some values the movement code keeps from one step to the next are not sent, and start from zero at every snapshot;
- under loss, the client predicts from an older snapshot than the newest it holds.

**Now.**

- A measuring tool, the console command `predictmiss`, compares the whole predicted state of the player with the server's, field by field, at every tick.
- The lifts and doors are placed where the server has them, for your own player, for the other players and for the items standing on them.
- The server's animation rule is applied in the prediction, and the disruptor's zoomed animation is no longer restarted at every step.
- The unsent values are carried over from the client's own prediction.
- Under loss, the prediction starts from the newest snapshot received.
- Your own player is drawn between its last two predicted ticks, at the frame's exact time.
- Your own sounds and effects (a jump, a saber ignited, a hit taken) are played even in a frame where the server has already run every command the client made, which is common on a local server or at a very low ping below 333 frames per second.

**Why.** Every prediction error that can be removed is removed; what is left is what only the server can know.

**Gain.** Measured with `predictmiss` on the test server (ping 20 ms):

| | before the fixes (22,938 ticks) | after (9,696 ticks) |
|---|---|---|
| Ticks where the position differs | 0.6 %, two thirds of them standing on a lift (up to 25 units) | 0.4 %, all in contact with another player (1.0 unit on average, 2.5 at most) |
| On a lift | a fall predicted at every tick | none |
| Zoomed with the disruptor | an animation difference at every tick | none |

- Every field is predicted right in more than 97 % of the ticks. What differs most is what the game decides on the server: the force's regeneration, the jetpack's fuel, the idle animation, hits, blocks, deaths.
- With 20 % of the snapshots dropped: 17,634 frames predicted from a newer snapshot, and one position difference in 13,484 ticks (a respawn).
- Not removed: the contact with another player. The client collides with the others where it draws them (the past), the server where they are.

<details>
<summary>Technical details</summary>

| | OpenJK | this branch |
|---|---|---|
| Movers for the client's collisions | placed at the snapshot's time, the same as the server's while a snapshot comes per server frame | placed at the server's last 20 Hz instant (`CG_MoverTime`, `cgame/cg_predict.c`): the server moves its movers on the instants only (section 1). Also for the ride of the local player, for the other players and entities standing on a mover (`CG_CarryWithMover`, `cgame/cg_ents.c`), for a spectator, a followed player and a demo |
| An animation left and taken again within one step | restarted by the server only (`BG_StartLegsAnim`, `BG_StartTorsoAnim`, `game/bg_panimate.c`) | the prediction applies the same rule, with the animations the command starts from |
| Zoomed with the disruptor | the torso animation is left and taken again at every movement step, on both sides, so its restart bit changes at every tick and is sent at every tick | the animation is kept (`PM_TorsoAnimation`, `game/bg_pmove.c`; shared code, so the server too) |
| Fields of the player state that are not sent (187 of its 388 words) | 0 at every snapshot | those the movement code keeps between steps (force jump drain delay, zoom lock time, use delay, rocket lock time, jump pad frame) are taken from the client's own prediction of the snapshot's tick (`CG_CarryUnsentFields`), unless a respawn or a teleport lies between |
| Snapshot the prediction starts from | cgame's next snapshot, the first one after the drawn time | the newest one received, when it is ahead (`CG_SnapshotAhead`); not across a teleport, a respawn, a change of followed player, a vehicle boarded or left, or a map restart |
| Between two movement steps | the player is predicted up to each frame's time | the predicted states of the last 64 command times are kept; the player is drawn between the newest tick and the one before at the frame's time (`CG_InterpolateTicks`): position and velocity interpolated, the rest the newest tick's, the view angles of the frame, except on an axis the movement holds (wall run, lunge, jump attack). A pilot is drawn at the newest tick |
| Prediction error check | before each replayed command | one more turn after the newest command: with a command per tick, a frame that brings a snapshot and no new command had no check |
| Held view compared | | on 16 bits, the width the field is sent with |
| Measuring tools | `cg_showmiss`: position and animations | also `predictmiss`: writes `predictmiss.txt` with every word that differs, the position differences sorted by cause, and the first cases in full; `tools/predict/ps_fields.sh` puts the field names |

Decided, not done: the force power levels the movement code reads (saber defence, throw, offence) are not sent, so the client predicts with level 0 (the saber's delay after a block: 700 ms predicted for 250); a block came 5 times in 224 s of fights. They are added to the format with plan step 13. A cache of predicted states was measured and not kept (the replay costs 0.27 % of a core at 125 frames per second and 0.51 % at 333 at a ping of 21 ms). Details: [progress/step-14-fixed-tick.md](progress/step-14-fixed-tick.md) sections 11 and 14.

</details>

### 7. Ping measured in real time

**Before.** The server counts a message's round trip on its frame clock, which moves once per frame: steps of 50 ms at 20 Hz. Every packet that acknowledges the message overwrites the time, so the last one counts, not the first. The history holds 32 messages: above a round trip of 1.6 s at 20 Hz (800 ms at 40 Hz) no message is acknowledged within it and the ping shows 999.

**Now.** The real time when the message leaves and when the first packet acknowledging it arrives, over the last 256 messages.

**Why.** The ping is the measure the players read and the server uses (to send a lost command again); it should be the real round trip.

**Gain.** The ping is exact and steady, and measured up to a round trip of 2.56 s. On the test server it shows 19 to 21 ms.

<details>
<summary>Technical details</summary>

| | OpenJK | this branch |
|---|---|---|
| When a message was sent | `svs.time`, the server's frame clock | the real time (`Sys_Milliseconds`) |
| When it was acknowledged | `svs.time` again, written by every packet acknowledging it | the real time of the first packet acknowledging it |
| Messages measured | the last 32 (`PACKET_BACKUP`), in the frame history | the last 256 (`SV_PING_WINDOW`, the window of the client's 8-bit acknowledge), in a history of their own |
| An acknowledge older than the history | could write into the frame of a newer message | ignored |
| "Not acknowledged yet" | a time of -1 (any time at or below 0 counted as none) | an explicit flag |

The ping is the average over the acknowledged messages among the last 256 (`SV_CalcPings`). It holds the network round trip, the wait for the client's next packet (up to a tick) and the processing.

</details>

### 8. Network settings removed

**Before.** The player sets `rate` (bytes per second), `snaps` (snapshots per second), `cl_maxpackets` (packets per second) and `cl_packetdup` (commands repeated against loss), and the server has policies to bound them. A wrong value costs delay or lost commands, and the right one depends on the server.

**Now.** None of them exists. Every client gets a snapshot per tick and sends a packet per tick; each packet repeats the commands of the 2 packets before it; the timeouts are fixed.

**Why.** One of the goals: a value that needs no measuring is a constant, a value that depends on the link is computed (section 4), and what must not be bypassed is enforced by the server. Recent competitive games do the same ([surveys/network-settings.md](surveys/network-settings.md)).

**Gain.**

- Nothing to set, on any link, and no setting that gives an edge.
- Two client packets lost in a row cost no command (one in OpenJK).
- A dead client frees its slot after 30 s instead of 200.

<details>
<summary>Technical details</summary>

| cvars | OpenJK | this branch |
|---|---|---|
| `rate`, `snaps` (client); `sv_clientRate`, `sv_ratePolicy`, `sv_minRate`, `sv_maxRate`, `sv_lanForceRate`, `sv_snapsMin`, `sv_snapsMax`, `sv_snapsPolicy` (server) | the client asks, the server's policies decide; LAN clients unlimited | removed. A snapshot per tick for every client. A fixed rate of 200,000 bytes per second (`SV_CLIENT_RATE`) only paces what is sent in several fragments (a gamestate, a download, a very large snapshot): a single message counts at most 1,500 bytes, which the rate clears within a tick. The server's `status` loses its `rate` column |
| `cl_maxpackets`, `cl_lanForcePackets` | at most 63 packets per second, as a minimum gap checked once per frame; every frame on a LAN | removed. One packet per tick, 100 per second (`MAX_PACKET_RATE`), and one per frame until the first snapshot. The server decodes at most 110 per second on average (`SV_PACKET_RATE`, 10 % above the client's rate, whose clock may run slightly faster), with a burst of 100 (`SV_PACKET_BURST`), and drops the rest unread (`svnetstats` counts them) |
| `cl_packetdup` | 1: each packet repeats the commands of the previous packet | removed: always 2 (`CL_PACKET_DUP`). A repeated command costs 1.5 to 1.9 bytes (measured) |
| `cl_timeout`, `sv_timeout` (200 s), `sv_zombietime` (2 s) | cvars | removed: 60 s while connecting or loading, 30 s in the game (`TIMEOUT_CONNECTING`, `TIMEOUT_ACTIVE`, both sides), 2 s of zombie state (`SV_ZOMBIE_TIME`) |
| `cl_timeNudge` | -30 to 30 ms | removed (section 4) |
| `sv_fps` | 40 | removed (section 1) |

- Configuration files that still set a removed cvar are harmless: the value is stored and never read.
- The UI's `ui_setRate`, which set `cl_maxpackets` and `cl_packetdup` from `rate`, is removed; the game's menus have no network setting.
- The game module no longer advises `/snaps`; its userinfo validation no longer knows `rate` and `snaps`; bots no longer send them; cgame no longer sets `snaps`.
- `g_userinfoValidate` is a bit mask indexed by the position of each checked field, so removing the two fields moves the bits after them. Its default becomes 6291455 (the same checks as OpenJK's 25165823). A server configuration that still stores the old value turns on the extended ASCII check (names with accents refused) until the cvar is reset (`reset g_userinfoValidate`).
- Details: [progress/step-11-settings.md](progress/step-11-settings.md).

</details>

## B. Precision: the same game on every machine

These changes are in the game module and cgame (`src/codemp/game/`, `src/codemp/cgame/`) and in the shared math code. The interface between the engine and the modules is unchanged, but the server's game module and the client's cgame must come from the same source, since cgame predicts the movement with the same code.

### 9. Player velocity never rounded

**Before.** After every movement step the player's velocity is rounded to whole units per second (`pmove_float 0`, the default). The rounding adds up differently with the length of the step, which is the length of a drawn frame: hence the higher jumps at 125 and 333 frames per second. A Windows client and a Linux server did not round a half the same way, which gave prediction errors of up to 2.7 units. Two more cvars, `pmove_fixed` and `pmove_msec`, could force a fixed step for everyone.

**Now.** The velocity is never rounded and the three cvars are removed. With section 2, every player moves in steps of 10 ms.

**Why.** No loss of precision: the client gets the velocity the server holds. And equality: the movement must not depend on the frame rate or on the system.

**Gain.**

- The same jump for everyone.
- A Windows client and a Linux server agree: at most 0.0006 to 0.0009 unit of difference in the movement test, 100 times under the threshold of a prediction error (measured before the math functions were made identical, section 11).
- Cost: 6.5 bytes more per message with bots (6.1 estimated on recorded games), because the tables were trained on rounded velocities; retraining on this build's games (plan step 13) should lower it.
- What it implies: the jumps of 125 and 333 frames per second are gone, and a jump designed for them on a map may no longer pass.

<details>
<summary>Technical details</summary>

| cvar | what it did | OpenJK default | this branch (no cvar) |
|---|---|---|---|
| `pmove_float` | 0: the velocity is rounded to whole units per second at the end of every step. 1: it is kept exact | 0 | always exact |
| `pmove_fixed` | 0: one step per command, as long as the time since the previous command (cut into pieces of at most 66 ms). 1: steps of exactly `pmove_msec` for everyone | 0 | one step per command; every command lasts one tick, 10 ms (section 2) |
| `pmove_msec` | the length of the fixed step of `pmove_fixed 1`, 8 to 33 ms | 8 | not used |

| | velocity rounded (OpenJK default) | velocity exact (this branch) |
|---|---|---|
| Jump height | depends on the frame time | the same for everyone |
| Windows client (MSVC) against Linux server (GCC) | up to 2.7 units of prediction error; with the rounding rule fixed (section 10), 0.0001 to 0.0002 unit in the test, but a rounding can still flip by 1 unit per second | at most 0.0006 to 0.0009 unit, no flip possible |
| Legs of a player pushing against a wall or another player | the velocity the push leaves (under 0.5 unit per second) rounds to 0: the legs stay straight | the legs turn to the velocity's direction only from 1 unit per second on the ground (`BG_G2PlayerAngles`, `game/bg_pmove.c`: the speed under which the movement code takes a player as stopped). Without that threshold the few hundredths of a unit per second the push leaves sideways turned the legs up to 60 degrees, on the client and in the server's model for saber hits |

A mod that takes this game code loses the three `pmove_*` cvars; a mod with its own game code keeps its own physics, as long as its game and cgame match each other. The trap and `Sys_SnapVector` stay in the module interface.

</details>

### 10. Missiles exact and one rounding rule

**Before.** The game rounds a missile's launch point, its velocity and its impact and explosion points to whole units. The rounding function itself differs between compilers: it truncates on most, rounds to the nearest on 32-bit Windows, and the engine's own version rounds a half away from zero with MSVC and to the even number elsewhere.

**Now.** Missiles are not rounded. What is still rounded (events, dropped items, the spawn point) is rounded to the nearest, a half to the even number, on every compiler.

**Why.** The client sees the trajectory the server computes, and a Linux server and a Windows server place things the same way.

**Gain.** Exact trajectories for 0.5 byte more per message on recorded games.

<details>
<summary>Technical details</summary>

| what | OpenJK | this branch |
|---|---|---|
| Missile launch point, velocities, impact and explosion points | rounded to integers (`SnapVector`, `SnapVectorTowards`) | exact |
| `SnapVector` (events, dropped items, spawn point) | truncation, except MSVC 32-bit: nearest | to nearest, ties to even, on every compiler, the rounding mode forced as in `Sys_SnapVector` (the base game's Linux build truncated: its values move by at most 1 unit) |
| `Sys_SnapVector` (the engine's) | ties away from zero with MSVC, to even elsewhere | to even everywhere; no movement call uses it now, kept consistent for any module that does |

Files: `src/shared/qcommon/q_math.c`, `src/shared/sys/snapvector.cpp`. The findings: [progress/snapvector-rounding-report.md](progress/snapvector-rounding-report.md). The base game's code, OpenJK's and the fix side by side, for a merge request upstream: [progress/step-06-cpp-codec.md](progress/step-06-cpp-codec.md), part F.

</details>

### 11. Same math functions on every platform

**Before.** OpenJK calls the system's sine, cosine, arc tangent, power and the like. No standard fixes their last bit: it differs between Linux versions, between Windows versions, between 32 and 64-bit programs and between processors. A Windows client and a Linux server therefore do not compute the same movement, and two Linux servers running the same program do not compute the same game.

**Now.** All the code, the single player and Jedi Outcast included, calls one set of functions, built on a library whose results are the correctly rounded ones (LLVM libc). Reading a number from text (a cvar, a map entity) goes through it too, and pi is the same number on every system.

**Why.** Identical results on Linux and Windows, which the prediction, the scripted tests and the comparison of two servers all need.

**Gain.** The same bits on every compiler, system and processor measured (10 builds and the test server). With section 12, the 40 scripted fights give the same counts with clang, MSVC and GCC, on Windows and on Linux.

<details>
<summary>Technical details</summary>

| what | OpenJK | this branch |
|---|---|---|
| `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`, `log`, `log2`, `pow` and their float versions, everywhere in the multiplayer code and `shared` (game, cgame, UI, client, server, botlib, renderers, sound; not the two third-party libraries inside rend2) | the C library's | one entry point, `src/shared/qcommon/q_libm.h` (`Q_sinf`, `Q_sin`, `Q_atan2f`, ...), implemented in `q_libm.cpp` on LLVM libc's correctly rounded functions (`libs/llvm-libc`); each call keeps its precision |
| `Q_rsqrt` (inverse square root) | Quake's approximation, about 0.2 % off | `Q_rsqrtf`, correctly rounded |
| `Q_acos`, `Q_asin` | the C library's with a clamp that never applied | the entry point's |
| `Q_powf(float, int)` | an integer power by repeated multiplication, used by the third-person camera | removed; the name is the entry point's real power, called with the same whole exponent |
| `Q_log2(int)` | integer logarithm | renamed `Q_ilog2` |
| `sqrt`, `floor`, `ceil`, `fabs`, `fmod`, `round`, `lrintf`, `isnan`, `isfinite` | the C library's, called directly | through the same entry point, as inline calls to the C library (IEEE 754 fixes their results) |
| Trigonometry of angles in degrees (`AngleVectors`, `RotatePointAroundVector`, the sine trajectories, field of view, effects) | degrees converted to radians in float, then `sin`, `cos`, `tan` | `Q_sinpif`, `Q_cospif`, `Q_tanpif` of the angle in half turns: exact at multiples of 90 degrees (the radians path differed from the exact result on 1.5 million of 2.1 million angles) |
| Angles from vectors (`vectoangles`, `vectoyaw`, the movers' arc) | `atan2`, `atan` or `acos` in float, times 180 over a float pi | `Q_atan2degf`, `Q_atandegf`, `Q_acosdegf`: computed in double, rounded to float once |
| Vector lengths and normalisation | in float, each operation rounded | in double, each result rounded to float once |
| Text to numbers: `atof` (318 calls, the cvars included), `strtod`, the `sscanf` of floats in map entities, vehicle, effect, ICARUS and BSP strings (28) | the C library's: Windows rounds a decimal exactly halfway between two floats away from even, Linux to even | `Q_atof`, `Q_strtod`, `Q_strtof`, `Q_ScanFloats` on LLVM libc's correctly rounded parser |
| `a * b + c` fused into one instruction | allowed (GCC and Clang do it when the target has the instruction) | `-ffp-contract=off`; no effect on today's builds |
| `cg_smoothCamera` | 0 (default): the third-person camera's damping uses an integer power; 1: the real power | removed: always the damping of 0, computed by the entry point's power function |
| The single-player games (Jedi Academy and Jedi Outcast: game, cgame, UI, engine, renderer, MP3 decoder), plan step 18 | the C library's (about 1,180 calls) | the same entry point, each call at the precision the compiler chose (`sin` of a float calls the float function); the `sscanf` of floats (43) through the same parser |
| Pi | `M_PI`: a float on Windows (`q_math.h`), a double on Linux (glibc), so the seeker drone's orbit, a pendulum's period, the fields of view and effects of cgame and the renderer were computed differently on the two systems | `Q_PI`, a double everywhere, as Linux computed; the calls whose arguments use pi take the double function |

Survey, measurements and choice: [surveys/deterministic-math.md](surveys/deterministic-math.md).

</details>

### 12. Same random numbers on every platform

**Before.** Three generators: Raven's (15 bits, biased ranges), the system's `rand()`, which gives another sequence on Linux and on Windows, and a special one for the saber's choices in the movement code. A Linux server and a Windows server therefore play differently. The connection challenges come from `rand()` and the clock.

**Now.** One generator (xoshiro128++), the same numbers on every platform, without bias, in the multiplayer and the single player alike. The connection challenges come from the system's secure random bytes.

**Why.** Identical results everywhere; and a challenge an attacker cannot guess.

**Gain.** The same game on Windows and Linux for the same seed: a run can be replayed exactly (`com_debugSeed`, section 37).

<details>
<summary>Technical details</summary>

| what | OpenJK | this branch |
|---|---|---|
| Generators | `holdrand` (MSVC's LCG, behind `Q_irand`, `Q_flrand`, `irand`, `flrand`), the C library's `rand()`, `PM_irand_timesync` | one entry point, `src/shared/qcommon/q_rand.h`: xoshiro128++ seeded through SplitMix64, unbiased integer ranges (Lemire), 24-bit floats |
| The saber's choices in the movement code (`PM_irand_timesync`) | drawn by overwriting the command's time (section 24) | from a state seeded with the command's tick; the base game's effective range kept |
| `g_randFix` | how `RandFloat` scaled `rand()`: 0 as the base game, 1 (default) the correct range; its Linux branch never compiled, so Linux servers got values far outside the range | removed: the default's correct range, from the entry point |
| Connection challenges, pure checksum feed | `rand()` and the clock | the system's random bytes (`Com_RandomBytes`) |
| Two draws in one expression | three places (section 24) | none; `tools/rng/unsequenced.py` lists such statements |
| The single-player games, plan step 18 | `rand()` (62 calls) seeded by the game, the engine's and the renderer's never or by the clock; `Q_irand` never seeded, the same draws at every launch | the entry point, its streams seeded as in multiplayer (the game by its seed, the engine from the system's random bytes, the renderer by the clock): the draws change from one launch to the next; the renderer's noise table from a fixed seed, one file for both games |

Survey and measurements: [surveys/random-numbers.md](surveys/random-numbers.md).

</details>

## C. Network format and memory

### 13. Messages from the server coded by a new codec

**Before.** The server writes each message (snapshot, server commands, gamestate) field by field into a bit stream compressed with one fixed Huffman table. A changed value is written in full.

**Now.** A new library, `netcodec`, builds every message. It knows each kind of field: it codes a value against its previous value or against a prediction (a position from the velocity, a time from the snapshot's time, an angle from its last two values), with statistics learnt on real games.

**Why.** Fewer bytes for the same values, so that 100 snapshots per second stay cheap; and exact values, where the old format cut some to their field width.

**Gain.**

| measurement | OpenJK's format | netcodec | ratio |
|---|---|---|---|
| 20 recorded games kept out of the training (maps not used for it), all messages | about 24 MB | 9.24 MB | 0.385 |
| the same, snapshots only | 155.1 bytes per snapshot | 66.9 | 0.432 |
| in game at 100 Hz, test server with 18 bots, the same messages in both formats | 264.6 to 419.2 bytes per message | 112.7 to 171.0 | 0.417 |

- Coding takes about 0.38 and decoding about 0.56 of OpenJK's time on the same messages (clang; the encoder is another 8 to 14 % faster since section 18).
- Every compiler (MSVC, GCC, Clang, 32 and 64 bits) produces the same bytes, checked by tests on each build.
- Tested in game with 5 % and 20 % of the server's messages dropped, and with a Linux server played from a Windows client over the Internet: no message the client could not decode.

<details>
<summary>Technical details</summary>

| | OpenJK (`msg.cpp`, `huffman.cpp`) | this branch (`src/codemp/netcodec/`; `server/sv_netcodec.cpp`, `client/cl_netcodec.cpp`) |
|---|---|---|
| Entropy coder | one static Huffman table for every byte | rANS with a separate probability table for each kind of field, trained on real games |
| Unchanged fields | one bit per field up to the last changed one, plus a count byte | only the changed fields are listed |
| Changed values | written in full, or as a 13-bit integer when integral | coded against the previous value or a prediction |
| Floats | full 32 bits or 13-bit integers | by kind: zero, integral, exact angle step, or the exact 32 bits; `-0.0` kept |
| Booleans | a "changed" bit, then the value | the "changed" flag only |
| `user*` fields of mods | 1 bit wide unless a mod widens them with `netf_overrides.txt` or `psf_overrides.txt` | coded by their C type, so the full value arrives; the overrides files are no longer read |
| Server commands | text through Huffman | each one coded against an earlier command of the same kind, token by token (and sent once: section 15) |
| Game directory (`svc_setgame`) | a separate message | carried by the next message |
| Map instance (`serverId`) | only in the system-info config string | also in every message, coded as "unchanged" (a fraction of a bit), so that the client's packets can code theirs against it |
| Input margin | none | one byte per snapshot until the tables are retrained (section 2) |
| End of message | an `svc_EOF` byte | none |
| Precision | what the field widths allow | exactly what the server holds |

- The engine hands its own data to the codec in place: the snapshots, the entity store, the command buffer, the baselines.
- Not changed: connectionless messages (`getinfo`, `getstatus`, `connect`, the master server); the choice of the base snapshot (the newest one the client acknowledged); the netchan and its fragmentation.
- OpenJK's writer is kept in the server, only to compare sizes (`sv_netcodecCompare`, section 27).
- `cl_shownet 1` still prints the packet sizes and `cl_shownet 3` the snapshot line; `cl_shownet 2` prints one line per decoded operation. The per-entity and per-field output (levels 4, -1, -2) is gone.
- A message the client cannot decode (only possible if it no longer holds the base the server used) is dropped whole; the client then asks for a full snapshot, as before.
- Details: [progress/step-06-cpp-codec.md](progress/step-06-cpp-codec.md), [progress/step-07-engine-integration.md](progress/step-07-engine-integration.md).

</details>

### 14. Packets from the client coded by the new codec

**Before.** Each client packet carries the acknowledges and the commands through the same Huffman coding, the commands scrambled with a key.

**Now.** The same codec as the server's messages. The first command of a packet is coded against the snapshot the client acknowledges (its time, its view angles, its weapon), the others against the command before.

**Why.** Fewer bytes, so that 100 packets per second cost less than OpenJK's 63.

**Gain.**

| measurement | OpenJK's format | netcodec | ratio |
|---|---|---|---|
| six recorded games over the Internet, 81,789 packets | 40.68 bytes per packet | 10.07 | 0.25 |
| in game at 100 Hz, three commands per packet | | 12.6 to 13.6 | 0.38 to 0.41 |

- 100 packets of 13 bytes per second are 1.3 KB per second of payload; OpenJK's 63 packets of 27 to 28 bytes were 1.75 KB (measured in plan step 8).
- Coding takes 0.34 to 0.40 and decoding 0.74 to 0.86 of OpenJK's time.
- The server decodes packets from anyone: the decoder bounds everything it builds, and is tested with 400,000 malformed packets on every build and with a fuzzer.
- In game: no packet the server could not decode, also with 10 % of the server's messages or 20 % of the client's packets dropped, after `map_restart` and after a server demo started in the middle of a game.

<details>
<summary>Technical details</summary>

| | OpenJK | this branch |
|---|---|---|
| `serverId`, last server message received, last server command received | three 32-bit values through Huffman | the message as its low 8 bits (the server takes the nearest number at or below its last message sent); `serverId` as "the same as in that message"; the command acknowledge against the commands that message carried |
| Client commands | text through Huffman, each with its sequence number | their count, the first sequence number against that message, the texts coded as the server's command texts |
| User commands | the first against an empty command, the others against the previous one, XORed with a key, through Huffman | the first against the snapshot of the acknowledged message, the others against the previous one, each field with its own statistics |
| User-command key | XOR with the checksum feed, the acknowledge and a hash of the last command | removed |
| End of packet | a `clc_EOF` byte | none |
| A client command longer than 1,023 characters (no client can send one) | the first 1,024 characters read, the rest read as the next fields of the packet | the packet is refused, as malformed |

- What the server does with the decoded values is unchanged: the acknowledge checks, the `serverId` checks, the client commands with the flood protection, the `sv_legacyFixes` clamps. When the commands run changes (section 2).
- The server keeps, for each client, the few values a packet is coded against for each of its last 256 messages (32 bytes each: 8 KB per client).
- The client codes against the acknowledged message only while it is sure the server has sent fewer than 256 messages since: the time since that message arrived plus the round-trip time it measures must stay under 128 message intervals (1.28 s). Otherwise, or right after a gamestate, the packet is coded without it: about twice larger (estimate), never wrong.
- A packet the server cannot decode is dropped and counted by `svnetstats`.
- `cl_nodelta` is unchanged: the client still asks for a full snapshot when it misses the last one.
- Details: [progress/step-08-client-to-server.md](progress/step-08-client-to-server.md).

</details>

### 15. Server commands sent once

**Before.** A server command (a chat line, a score update, a config string) is repeated in every message until the client acknowledges it: 2.6 times each on real games, about 60 % of the bytes of the commands.

**Now.** A command is sent once. It is sent again only when the message that carried it is lost, or together with a newer command so that they arrive in order.

**Why.** At 100 messages per second, repeating until the acknowledge would send each command many times.

**Gain.**

- Up to 4 % fewer bytes on real games.
- Measured in game: no command repeated without loss, on a LAN (ping 4 ms) and on the test server (19 ms). With 4 to 17 % of the messages dropped, 52 of 498 commands were sent again.
- Cost: a command in a lost message arrives about one ping later than with the repeats. The command buffers are twice larger (256 KB per client on the server, 512 KB on the client).

<details>
<summary>Technical details</summary>

| | OpenJK | this branch |
|---|---|---|
| A command not yet acknowledged | in every message | in one message; again when nothing is heard of that message after twice the ping plus a client packet interval (10 ms), or when a newer command is sent |
| `MAX_RELIABLE_COMMANDS` (`qcommon/qcommon.h`) | 128 | 256: a command is coded against an earlier one up to 127 commands back, so both sides must hold at least 256 to share the reference. The client-to-server buffer uses the same constant |
| Client dropped when this many server commands are unacknowledged | 129 | 257 |

`svnetstats` counts the commands sent and the repeats, by cause. Details: [progress/step-10-acks-reliable.md](progress/step-10-acks-reliable.md).

</details>

### 16. Lost gamestate sent again quickly

**Before.** While a client waits for the gamestate (the first large message of a connection), it sends one packet a second. The server learns that a fragment was lost only from those packets, so a loss costs a second or more.

**Now.** The client sends at its normal rate while it waits.

**Why.** A connection should not hang on one lost fragment.

**Gain.** A lost fragment costs about a round trip. Tested: the connection is immediate with 30 % of the messages dropped.

<details>
<summary>Technical details</summary>

`client/cl_input.cpp`: the packet rate while awaiting the gamestate. The connection's own fixed delays stay (`RETRANSMIT_TIMEOUT`, `sv_reconnectlimit`). Details: [progress/step-10-acks-reliable.md](progress/step-10-acks-reliable.md).

</details>

### 17. Snapshot history of 128 messages

**Before.** A snapshot is coded as the differences from an older one the client has confirmed (its base). Both sides keep the last 32 snapshots, so the base is at most 29 messages back. When the client's confirmation is older than that, the server sends a whole snapshot, in several fragments.

**Now.** Both sides keep 128 snapshots: a base up to 125 messages back.

**Why.** At 100 messages per second, 29 messages are 290 ms. A player with a long ping, or in a long burst of loss, would get whole snapshots at every tick.

**Gain.**

- A base is kept up to 1.25 s of round trip or of loss.
- Tested with 95 % of the server's messages dropped on the client: no whole snapshot needed, 2,171 snapshots coded against a base 9 messages back or more.
- A client that stands still for a moment (a load, a hitch of the system) reads the snapshots that waited all at once: up to 1.28 s none is lost, events included (tested frozen 0.5, 1.0 and 1.2 s). With 32 snapshots, a freeze of a third of a second already lost some at 100 per second; the base game kept 1.6 s at 20.
- Cost: the client keeps 17.4 MB of entity states instead of 4.4 MB. The server's memory is in section 18.

<details>
<summary>Technical details</summary>

- `PACKET_BACKUP` (`qcommon/qcommon.h`) and `eng::kPacketBackup` (`netcodec/engine.h`): 32 to 128. The rest follows: the server's snapshots per client, the codec's frames, the versions kept of each entity (`SV_SNAPSHOT_TICKS`), the client's snapshots, its entity ring (`MAX_PARSE_ENTITIES`, 8,192 to 32,768 states) and its demo frames.
- The format does not change: the base's distance is one symbol that goes up to 255. Its tables were trained on bases at most 29 messages back, so a base further back costs more bits until the retraining (plan step 13); not measured.
- A new client is built in static storage (`SV_DirectConnect`, `server/sv_client.cpp`): with 128 snapshots the structure is 896 KB, too large for the 1 MB stack of a Windows program.
- Details: [progress/step-14-fixed-tick.md](progress/step-14-fixed-tick.md) section 12.

</details>

### 18. Entity store of the server shared

**Before.** For every client the server keeps its own copy of every entity of each of its last 32 snapshots: 139.5 MB for 32 players. The copies are counted on 32 bits, and the server restarts the map before the counter wraps: after about 11.6 hours on one map with 32 players at 20 Hz, which would be 2.3 hours at 100 Hz (computed).

**Now.** The server keeps one copy of an entity's state for as long as it does not change, shared by every snapshot of every client that holds it. The counter is on 64 bits.

**Why.** A window of 128 snapshots (section 17) with a copy per client would take 558 MB. It was also to be the store lag compensation reads (plan step 14, part 8, dropped).

**Gain.**

- Memory: 69.7 MB reserved for 128 snapshots, in use only where a state was written. Measured, whole server process with 16 bots: 42 to 48 MB on Windows (57 MB at the window of 32, before the array was zeroed by the system), 82.6 MB on the Linux test server.
- No forced map restart.
- The encoder skips an entity that did not change since the base without comparing the two states (86.7 % of the entities on recorded games): encoding is 14.3 % faster with clang, 12.1 % with GCC, 8.4 % with MSVC. The bytes sent are the same.

<details>
<summary>Technical details</summary>

- `svs.snapshotEntities` holds `SV_SNAPSHOT_TICKS` (128) versions of each of the 1,024 entities; version v of entity n is at place `n * SV_SNAPSHOT_TICKS + v % SV_SNAPSHOT_TICKS` (`SV_SnapshotPlace`, `server/sv_snapshot.cpp`). A tick compares an entity's state with its last copy once, for the first client that sees it, and writes a new version only if it differs.
- A client's snapshot is a list of places. An unchanged entity is at the same place in a snapshot and in its base, so the encoder skips it on the places alone (library: `EntityRing::index`, `Frame::index`).
- The array is allocated zeroed by the system instead of cleared by hand at every map, and the versions of an entity are neighbours: the memory in use is that of the entity numbers in use. On Linux with transparent huge pages a write makes 2 MB resident, which is why the test server uses more than Windows.
- A snapshot too large is cut by rewriting its list, not the shared states. The bots and `sv_netcodecCompare` read their snapshot's entities through the list.
- Lag compensation would have needed the version each entity had at each tick (a byte per entity and tick); not written, the part is dropped.
- Details: [progress/step-14-fixed-tick.md](progress/step-14-fixed-tick.md) section 12.

</details>

### 19. Messages too large

**Before.** When a snapshot does not fit in a message, the server clears it and sends it empty, with a warning in its console.

**Now.**

- A message that codes larger than 1 KB is also coded in a raw form, and the smaller of the two is sent: a worst case stays close to the raw size.
- A snapshot still larger than the limit (49,152 bytes) is sent with the entities nearest to the player; the others come back in the next message.
- A gamestate that large drops the client, which returns to the menu with the reason.

**Why.** Never drop a snapshot silently.

**Gain.** Tested in game with a lowered limit: 296 snapshots cut, no message the client could not decode.

<details>
<summary>Technical details</summary>

`server/sv_netcodec.cpp` (`SV_NetcodecFitSnapshot`: a binary search over the number of entities). The debug cvar `sv_netcodecMaxBytes` lowers the limit to test both cases; `svnetstats` counts the cut snapshots and the messages sent raw. Details: [progress/step-09-safety-limits.md](progress/step-09-safety-limits.md).

</details>

### 20. Obfuscation removed

**Before.** OpenJK scrambles each message, in both directions, with a key made of the connection's challenge, the message numbers and the last command texts. It is not security: anyone who sees the connection can undo it.

**Now.** Removed in both directions, with the key of the client's commands.

**Why.** The receiver found the key from numbers read in clear at the start of the message; the codec now codes those numbers, so the scheme could not stay as it was.

**Gain.** None for the player. Whether to replace it with real encryption is decided in plan step 12.

<details>
<summary>Technical details</summary>

`server/sv_net_chan.cpp`, `client/cl_net_chan.cpp`: `SV_Netchan_Encode`, `CL_Netchan_Decode` and their counterparts no longer XOR the payload.

</details>

### 21. Demos

**Before.** A demo is a copy of the network messages. A client demo asks the server for a full snapshot to start.

**Now.** A client demo and a server demo are both coded again for the demo, so that the file refers only to what it contains. A demo keeps one snapshot in two (50 per second) and the player draws between them, as recent engines do; a snapshot in between is kept as well when it holds an event the next one no longer shows, so that no sound or effect is lost.

**Why.** A demo is played back from an empty state, so each message may only refer to snapshots and commands that are in the demo. The client cannot know which earlier messages the server's next ones will refer to, and lost messages would leave holes.

**Gain.**

- A client demo is complete by construction, also after losses, and starts without asking the server for anything.
- Size, measured with ten bots: about 12 MB per hour, 1.2 to 1.35 times a demo of the base game at 20 Hz (9.1 MB per hour); keeping every message, it was 2.2 times. Every event of the game is in it.
- Cost: one coding per message kept, about 1.5 microseconds, on the player's machine for a client demo and on the server for each player recorded; the server codes its demos once every message of the tick has left, so no player's message waits.
- What it implies: demos recorded with OpenJK or the base game do not play on this build, and the reverse.

<details>
<summary>Technical details</summary>

| | OpenJK | this branch |
|---|---|---|
| Server demo (`svrecord`, `sv_autoDemo`) content | copy of the messages sent | coded again, as a client demo, from the messages sent, after the tick's sends |
| Server demo start | waits for a full snapshot (`demowaiting`), then forbids older bases (`minDeltaFrame`) | a coded gamestate; the player's own messages are not touched |
| Client demo (`record`) content | copy of the messages received | each decoded message coded again against the demo's last snapshot, with the commands not written yet and without its download block |
| Snapshots kept | every one | one every 20 ms, and any other whose entity event, external event or predictable events the next one would lose (`qcommon/demo_record.cpp`, shared by both demos); a message with a gamestate, a game dir or a map change always |
| Client demo start | asks the server for a full snapshot and waits for it | nothing asked of the server |
| Gamestate at the start | OpenJK's format | coded, with the last 127 server commands |
| Cost | none | one coding per message kept (about 1.5 microseconds), after the tick's sends on the server |

The files keep the `.dm_26` extension for now. As in OpenJK, server demos still open when the server quits have no end marker; the player stops cleanly at the end of such a file. A reader or converter for old demos is decided with the protocol number, at the end of the plan. Details: [progress/step-07-engine-integration.md](progress/step-07-engine-integration.md), part 3.

</details>

### 22. Downloads over UDP

**Before.** The server sends as many 2,048-byte blocks of a file as the client's rate allows in one snapshot interval, up to 8.

**Now.** One block per message.

**Why.** The codec carries one download operation per message. A better path (several blocks per message, or a channel of its own) is decided with the transport, in plan step 12.

**Gain.** None intended. Computed from the code, not measured: a block is sent in two fragments, one per tick, so about 100 KB per second on every link. OpenJK is bounded by the rate: 50 KB per second with its default `sv_clientRate`, 100 KB per second on a LAN. HTTP downloads are not affected.

<details>
<summary>Technical details</summary>

`SV_WriteDownloadToClient` (`server/sv_client.cpp`) gives one block to each coded message; a message above 1,300 bytes leaves in fragments, paced at `SV_CLIENT_RATE`. The window of 8 blocks and the one-second resend of a lost block are OpenJK's.

</details>

## D. Fixes found on the way

### 23. Server fixes

**Before and now.**

| fix | before | now |
|---|---|---|
| The wait before a frame | A dedicated server spends the last millisecond before each frame in a busy loop: up to a tenth of a core at 100 Hz, even when empty. | Outside Windows the server sleeps that millisecond, and a packet that arrives still wakes it at once. |
| Linux's clock | On Linux the engine reads the wall clock. When the system's time is set back (a time synchronisation, a date set by hand), the server runs no frame and sends nothing until the clock catches up; beyond 30 s every client times out. When it is set forward, up to 5 s of frames run at once. | On Linux the engine reads a clock that only moves forward. |
| Small vector operations | Small vector operations (a dot product, a copy, a subtraction) are functions called from every loop of the collision and model code: the dot product alone is 11.5 % of a server tick with 16 bots. | The small vector operations are inline, for the server and the client. |
| The nearest navigation node | At every server frame, for every client, the game searches the nearest navigation node, which only NPCs use. | The navigation node is searched 20 times a second, as on a 20 Hz server. |
| Distances between clients | At every command of every client, the game computes a distance and two arc tangents to every other client, used only in the Jedi Master game type or against a client with force sight on. | The distance and the angles are computed only when a test needs them. |
| A player's model | Every call about a player's model (a bone, a bolt, a collision) looks the model up again by its file name, and reads a setting by its name. | The model lookup is cheaper, on the server and on the client; the setting is found once. |
| Bots' snapshots | The server builds a snapshot for every bot at every tick, and sends it to no one. | A bot's snapshot is built only when the bot is recorded in a demo. |

**Why.** Found while measuring and profiling the server at 100 Hz.

**Gain.** An empty server at 100 Hz on the test server: 9.7 % of a core before, 1.1 % after (measured over 120 s). A change of the system's time no longer freezes the server. The speed changes, with 16 bots and the same game replayed tick for tick (same statistics, the 40 scripted fights unchanged): 35 % less time per tick on the test server with GCC (311 to 201 microseconds), 30 % with MSVC on Windows (360 to 253), about 36 % with clang on Windows (four steps, each timed against the one before: 368 to 309 microseconds, 324 to 274, 279 to 267, 264 to 250). In real time: 5.3 to 4.1 % of the test server's core, 4.8 to 2.6 % of one core of the Windows machine. Nothing changes in the simulation and no delay is added.

<details>
<summary>Technical details</summary>

- `Com_Frame` (`qcommon/common.cpp`): outside Windows the last millisecond is slept in steps of 100 microseconds (`NET_Sleep` takes microseconds, `qcommon/net_ip.cpp`). Windows keeps the busy loop (`select` waits whole timer periods there); OpenJK's cvar `com_busyWait 1` keeps it everywhere.
- `Sys_Milliseconds` (`src/shared/sys/sys_unix.cpp`): `clock_gettime( CLOCK_MONOTONIC )` instead of `gettimeofday`, at the same cost; time spent suspended is no longer counted. The unused `baseTime` parameter is gone.
- `src/shared/qcommon/q_math.h`: `VectorAdd`, `VectorSubtract`, `VectorScale`, `VectorMA`, `VectorSet`, `VectorClear`, `VectorCopy`, `VectorLength`, `VectorLengthSquared`, `Distance`, `DistanceSquared`, `VectorInc`, `VectorDec`, `VectorInverse`, `CrossProduct`, `DotProduct`, `VectorCompare` are `static` inline functions with the bodies they had in `q_math.c` (macros in Quake 3, functions in OpenJK); same semantics, an aliased output included.
- `NAV_FindPlayerWaypoint` on the 20 Hz instants (`game/g_main.c`); `G_UpdateClientBroadcasts` skips a pair of clients when neither of its tests applies (`game/g_active.c`).
- `G2_SetupModelPointers` (`rd-dedicated/G2_API.cpp`, `rd-vanilla/G2_API.cpp`): the cvar `dedicated` is found once (`G2_Dedicated`); the model hash (`generateHashValue`, `tr_model.cpp`) folds the case without `tolower`. The model is still looked up at every call, as in OpenJK.
- `SV_SendClientSnapshot` (`server/sv_snapshot.cpp`): no snapshot for a bot unless it is recorded (`sv_autoDemoBots`, a demo started on it); `BotGetSnapshotEntity`, which the base game's bots never call, then finds no entity.
- The profile and the timings: [progress/step-14-fixed-tick.md](progress/step-14-fixed-tick.md) section 15.
- Windows keeps `timeGetTime`, already monotonic. Its value changes every 0.5 ms with the timer period the engine asks for, so a frame starts 0 to 0.5 ms after its millisecond (measured), without accumulating; `QueryPerformanceCounter` would cost 14.7 ns per call instead of 2.8 for no visible gain.

</details>

### 24. Game fixes

**Before and now.**

| fix | before | now |
|---|---|---|
| The saber's random choices in the movement code (chain ends, lock breaks, flip and jump attacks) | They draw their numbers by overwriting the command's time, so the rest of that movement step reads a random time: footstep and slope timers, weapon charge, saber lock and hand extension times, zoom. | They draw from a seed of their own; the command's time stays intact. The numbers are the same, and the client and the server still agree. |
| Two random draws in one expression | Three places do it, in an order the language leaves to the compiler: the bouncing flechettes of the alternate fire (their speed and their lifetime), an NPC's victory voice and the Galak mech's aim. A server built with MSVC gives other flechettes than one built with clang or GCC. | The draws are made one after the other, in the left-to-right order. |
| The disruptor's zoom | The torso animation is left and taken again at every movement step. | The zoomed animation is kept. |
| The rocket launcher's homing lock (alternate fire, vehicles' missiles) | Meant to refuse a cloaked target, it tests the wrong bits: a cloaked player can be locked, while a player who makes or undergoes a force pull cannot be for about 0.6 s. In single player, a target whose armour just took a hit cannot be locked for 0.5 s, a cloaked one can. | A cloaked target is never locked; a force pull or a hit on the armour no longer matters. |
| A shot deflected during an idle animation | A player in an idle animation (after 5 s without action) whose saber deflects a shot without a block (fired from too close to be seen coming) keeps playing the idle animation while the shot bounces off. | The deflection cuts the idle animation, as any other action does, and the 5 s count starts again. |
| The selected force power after a map change | After a map change or a `map_restart`, a player whose selected power was Heal finds another one selected (the last power known): the server tests the saved power's number as a mask of powers, so Heal never passes and Pull passes only for a player who knows Speed. | The saved power is selected again whenever the player knows it. |
| Single player: Absorb | Absorbing a force power gives back a share of the cost of the power whose number is the attack's level. With Absorb 3, a level 2 push gives back 48 points though it cost the attacker 15, a level 1 push 9, a level 3 push 15, a level 2 grip 48. | A share of the attacking power's own cost: with Absorb 3, 15 points for a push or a pull at any level, 1 for a grip or a drain. |
| Single player: the datapad | When a script gives a force power, the datapad opens on the slot numbered as the power (Push opens on Mind Trick). | It opens on the power given. |
| A menu's text field | A character whose code is also a key's (« ª » on a Spanish keyboard, « « ») is inserted, then moves the focus to the previous or the next field as that key would; in multiplayer a « Ť » typed with the cursor outside the field ends the editing. | The character is inserted and the focus stays in the field. |
| Single player: a script's forced march | A script reading whether an NPC is in a forced march (`get` of `SET_FORCED_MARCH`) reads other flags (crouched, no response) instead. | It reads the forced march. |
| Single player: the renderer's errors | An error (an unknown `rgbGen` in a shader) never reaches the console. | It is printed in red, as a warning is printed in yellow. |
| Multiplayer logs | A siege team change is logged "killed … by (null)"; with `g_statLog`, the concussion rifle's kills and damage are counted under "No Weapon"; `gamestats` prints "(null)" for the NPCs' voice events. | The team change is logged by its name, the concussion rifle counts for itself, every event has its name. |
| A script's `affect()` on an entity that does not exist | The warning prints a name read from random memory (three trees). | It names the entity the script asked for. |
| The weapon statistics of `g_statLog` | At the end of a map, "Combat Data by Weapon" names its lines one weapon off from the second (the melee's numbers labelled "Saber", the saber's "Bryar Pistol", ...), and the last three lines print "(null)". | Each line has its weapon's name. |
| Picking up a weapon | The rule "only half the ammo for a player who has enough" reads the ammo cell numbered as the weapon: with 300 blaster cells and no rocket, an E-11 picked up compares the rockets and gives the full amount; a weapon numbered 10 or more compares an unused cell and always gives the full amount. | The rule reads the weapon's own ammo type. |
| A script or a `misc_weapon_shooter` naming a weapon the game does not have | The weapon becomes `-1`: in single player the game goes back to the menu ("Couldn't find item for weapon -1"); in multiplayer the shooter's map does not load, and the script gives a broken weapon mask. | The name is ignored, with a warning for the script; the shooter keeps its blaster. |
| Single player: the vehicles' muzzle flashes | The flash is taken from the hand weapon numbered as the vehicle's weapon. | The vehicle weapon's own flash, as in multiplayer. |
| Single player: the datapad's weapons | The icons right of the selected weapon test the ammo of a stale weapon number, so a full weapon can show the "no ammo" icon. | Each icon tests its own weapon. |
| An NPC hit while gripped or drained | No pain animation is chosen then, and the pain delay is read from memory before the animations' table: a random delay, the NPC flinching again at once or not for a long time. | No delay in that case, as in single player. |
| The pain delay of a creature or a droid | A non-humanoid NPC's pain delay multiplies its own animation's frames by the humanoid's frame time; for the first pain animation of the 8 that reach it in multiplayer (the AT-ST, the probe, the protocol droid, R2-D2, the hazard trooper, the assassin and saber droids, the rocket trooper), whose frame rate is higher than the humanoid's, the delay is two or three times the animation: an AT-ST's 0.7 s pain gives 1.4 s without another pain reaction, a rocket trooper's 0.57 s gives 1.7 s. | The delay is the animation's own length. |
| A `misc_weapon_shooter` holding the disruptor | Its beam starts at the map's origin (0, 0, 0), not at the shooter: the client the game lends it for the weapon code never gets a position, and the disruptor reads that position, where the other weapons read the entity's (read from the code, not run; the same in 1.01). The beam hits what lies on that line, nothing when the origin is inside a wall, and is drawn from the shooter to where it stops. Only single-player maps of the game have such a shooter. | The beam starts at the shooter and hits what it aims at. Players, bots and NPCs fire from the same point as before. |
| A stormtrooper sent to scout | When a squad has lost its enemy and sends one trooper ahead, he leaves in silence: his chase line is stored as the number 0, which the code reads as "nothing to say" (multiplayer and Jedi Outcast). | He says his chase line as he leaves, unless his group spoke too recently. |
| Single player: breaking free of a drain | A Jedi NPC drained by the player can break free and push him back from the first instant: the test of the drain's length always passes. | Not before 0.5, 1 or 1.5 s of drain (drain level 1, 2 or 3), as the code means. |
| Single player: gripping an object | An object gripped at level 2 or 3 takes two thirds of the grip's damage, as if it absorbed the power. | The full damage; only a character's Absorb reduces it. |
| Jedi Outcast: the items carried to the next level | The item precached at the level's start is the one of the slot before the carried one: carrying a seeker precaches the bacta, and the seeker loads during the game when first used. | The carried items are precached. |
| A script moving an entity to a tag that does not exist | The error is printed, then the entity is moved to a position read from uninitialised memory (three trees). | The script's task fails and the entity stays where it is. |
| Votes in Capture the Ysalamiri | `callvote map`, `kick`, `clientkick`, `g_doWarmup`, `g_gametype`, `map_restart`, `nextmap` and `timelimit` are refused ("… is not applicable in this gametype."). | They are accepted, as in Capture the Flag. |
| Jedi Outcast: the turrets | A `misc_turret` whose muzzle is inside any volume (a trigger, water, fog) never fires. | It fires unless its muzzle is in a solid, a body, a corpse or a shot clip, as in Jedi Academy. |
| Multiplayer: a player strafing on an emplaced gun | For one second, a ghoul2 trace of the client on that player writes through a stale pointer: a wrong hit, or memory overwritten. With `com_optvehtrace`, a traced vehicle is drawn with the strongest animation smoothing. | The smoothing has a bit of its own; the traces are unaffected. |
| Single player: the saber's pull attack | The pulled victim jumps into the "pulled in the air" pose in 2 ms. | It blends into it in 100 ms, the default. |
| A script reading a flag | A `get` of one of 35 flags (`SET_INVISIBLE`, `SET_NOTARGET`, `SET_LOOK_FOR_ENEMIES`, ...) gives the flag's bit, 128 or 2048, where the other booleans give 1; no script of the game reads them. | 1 or 0. |
| Combat points (multiplayer, Jedi Outcast) | An NPC that flees or wants to duck skips the points the map marks FLEE or DUCK and takes ordinary ones (no multiplayer map of the game has combat points). | It takes them, as in Jedi Academy's single player. |
| Landing on a surface without footsteps (`surfaceparm nosteps`, multiplayer) | A small landing plays a stone footstep, whatever the surface (no map of the game has such a surface). | No footstep, as the shader asks. |
| The bots' debug lines (multiplayer, a Debug build with `bot_debug`) | Blue lines are drawn yellow, yellow ones blue, orange ones magenta: the renderer draws a debug colour with three bits. | Each in its colour; the orange is named magenta, the colour three bits draw. |
| A menu or a bot file using `__DATE__` or `__TIME__` (multiplayer) | The botlib's preprocessor frees the C library's static buffer: a corrupted memory or a crash (no file of the game uses them). | The date or the time, nothing freed. |
| A menu's script command without its argument (28 commands of the single player's menus, `startmap`, `giveweapon` and `giveinventory` among them, and the multiplayer's `clearmouseover`; found on 21 November 2026 by MSVC's link-time optimization) | The command reads a pointer that was never set (`String_Parse` sets nothing when the argument is missing): a crash or an action on whatever the memory held (no menu of the game omits an argument). | The command does nothing, as the commands of the same menus that already tested their argument. |
| The weapons' charge (the bryars' and the DEMP2's alternate fire, the bowcaster, the disruptor's sniper shot, the thermal detonator's throw, the rocket launcher's lock, the vehicles' homing missiles) | The charge starts at the time of the command that presses the button and ends at the time of the server frame that fires: the time held plus the gap between the server's clock and the time of the command that releases the button, none for a bot, but for a player a gap that varies with the ping and the packets' timing (read from the code, not measured). On this branch's 100 Hz tick it ended at the last 20 Hz instant, up to 50 ms before the releasing command: the bryar's alternate held 400 ms fired a shot of 1 unit (19 damage) instead of 2 (44), held 1 s 4 units instead of 5; an NPC's charge, its commands dated 50 ms before the instant, lasted 50 ms more than held. | The charge ends at the time of the command that releases the button: exactly the time held, for every player whatever the ping, for the bots and for the NPCs. The charge the client draws was counted that way already, so it agrees with the shot. |

**Why.** Each of the first three made two machines, or the client and the server, compute something different. The lock did the opposite of what its code says, and the idle test compared a block type with the time, so it never held (both found on 16 November 2026, plan step 18). The others were found on 17 November 2026 while typing the enumerations (plan step 18): a number used as a mask or as the index of the wrong table, a character read as a key, tables shorter than their enumeration, a format without its argument. The last five were found the same night while naming the unnamed enumerations: a value of 0 that also meant "none", a misplaced parenthesis, a starting value meant for characters only, a loop one slot off, a failure returned as a success. The weapons' charge was found on 19 November 2026 by the scripted scenarios of plan step 20 (part 2): the 100 Hz tick of plan step 14 had left its end on the logic clock.

**Gain.** The flechette's alternate fire was the one scripted fight of 40 that differed between a clang and an MSVC server; all 40 now agree. No animation difference while zoomed (section 6). A weapon's charge depends only on how long the button is held: in the scripted scenarios, 18 cases of the charged weapons change, a unit more where the time held reaches a multiple of the unit, and a rocket held 750 ms locks on its target.

<details>
<summary>Technical details</summary>

- `PM_irand_timesync` (`game/bg_saber.c`).
- `WP_CreateFlechetteBouncyThing` (`game/g_weapon.c`), `NPC.c`, `NPC_AI_GalakMech.c`.
- `PM_TorsoAnimation` (`game/bg_pmove.c`): the test that sets the weapon's ready animation is skipped while zoomed with the disruptor.
- `PM_RocketLock` (`game/bg_pmove.cpp`) and the single player's `WP_RocketLock` (`src/code/game/g_weapon.cpp`) tested `s.powerups & PW_CLOAKED`, the powerup's number used as a mask (11: the bits of `PW_PULL` and of two powerups never set; 4 in single player: `PW_BATTLESUIT`), now `Bit( PW_CLOAKED )`.
- Single player: taking a saber staff added its one-blade style (medium for the game's staffs) to the styles known as a value, not as a bit, so it taught the fast style; it teaches the medium one now (`g_client.cpp`, `NPC_stats.cpp`, `g_items.cpp`, `wp_saberLoad.cpp` of `src/code/game`).
- The weapon log (`g_log.cpp`) kept its two tables of pickups with their sizes swapped: picking up a force enlightenment, the force boon or the ysalamiri was counted in the next player's row, and for the last player slot written past the end of the table, into another global variable. Nothing visible (the only reader serves awards that nothing computes); each pickup counts for its player now and nothing is written out of the table.
- The scripts' `SetForcePowerLevel` (`game/g_ICARUScb.cpp`, and `Q3_Interface.cpp` of the single-player games) tested the level where it meant the power's index, so a map's script with a power index past the last wrote past the player state's force levels: the index is tested now.
- `G_CheckClientIdle` (`game/g_active.cpp`): `ps.saberBlocking >= level.time` (a block type, 0 to 2, against the time) replaced by `ps.saberBlockTime >= level.time`, true for 150 to 250 ms after a deflection (defence 2 and 1; never in defence 3).
- The calls of printf-like functions are checked by the compiler (17 November 2026, plan step 18): the multiplayer UI's warnings about a menu's `cursor` keyword and about the full string pool printed only their colour code, the effects' "no free slots" message printed the effect's name as a number, the NPC scale warning lost its `%`; a message whose argument held a `%` (a file name, an error, a command) was formatted twice. Each prints its text now.
- The Interrogator droid's patrol talk named a sound file the game does not have, so the droid was silent; it plays the probe droid's three talk sounds now (`AI_Interrogator.cpp`, `NPC_AI_Interrogator.cpp`; 17 November 2026).
- Found while typing the enumerations and fixed after a decision (17 November 2026, plan step 18):
  - `WP_InitForcePowers` (`game/w_force.cpp`): `forcePowersKnown & sess.selectedFP` is `forcePowersKnown & Bit( sess.selectedFP )`, the saved power tested in range first;
  - the single player's absorb (`src/code/game/wp_saber.cpp`, six calls of `WP_AbsorbConversion`) took `forcePowerNeeded[level of the attack]`, a line from multiplayer, whose table is indexed by level and power, while the single player's is indexed by power only: `forcePowerNeeded[attacking power]` now;
  - the single player's datapad (`src/code/cgame/cg_draw.cpp`) put the power's number in the slot index: `CG_DataPadUpdatedPowerSlot` (`cg_main.cpp`) finds the slot that shows the power (Jedi Outcast's slots are in the powers' order, so it had no such problem);
  - `Item_TextField_HandleKey` (`ui/ui_shared.cpp`, multiplayer and single player) tested a character, its flag removed, against the keys after inserting it: the key of the event is an empty optional for a character now (`KeyEvent_Key`);
  - ICARUS's `GetFloat` of `SET_FORCED_MARCH` (`Q3_Interface.cpp` of the single-player games) tested `scriptFlags & SET_FORCED_MARCH`, the field's number as a mask: `SCF_FORCED_MARCH` now;
  - `CL_RefPrintf` (both clients) and `SV_RefPrintf` (multiplayer) printed `PRINT_ALL`, `PRINT_WARNING` and `PRINT_DEVELOPER` only; `PRINT_ERROR`, which the single-player renderer uses for shaders, is printed in red;
  - `modNames` (`game/g_combat.cpp`) had no name for `MOD_TEAM_CHANGE`; `weaponFromMOD` (`game/g_log.cpp`) lacked the cells of `MOD_VEHICLE`, `MOD_CONC` and `MOD_CONC_ALT`, so every cell after them was three off; `eventnames` (`game/bg_misc.cpp`) stopped at `EV_TAUNT`, 76 events before the end;
  - the `affect()` warning of ICARUS's sequencer (`Sequencer.cpp` of the three trees) had a `%s` without its argument (found when the format attribute was given to `IGameInterface::DebugPrint` and `I_DPrintf`); two script texts were given as formats (`CenterPrint`), so a `%` in them was read as a conversion;
  - without a visible effect: a negative `g_maxHolocronCarry` made picking up a holocron write before `holocronsCarried` (over `hasDetPackPlanted`), and is a limit no more; the saber's return to ready from an attack (`PM_SaberAnimTransitionAnim`, three trees) was computed from `newmove`, there `LS_READY`, where the attack left (`curmove`) is meant, on a path no caller takes; `debugSetSaberMove` refuses a negative move (debug builds); the botlib's entity types (`be_aas_entity.cpp`) had Quake 3's `ET_MOVER` (4, the game's force field), now the game's 6; the scripts' "failed sscanf on `SET_PARM%d`" message named the parm one below.
- Found while typing the weapons and the animations and fixed after a decision (17 November 2026, plan step 18):
  - `weaponNameFromIndex` (`game/g_log.cpp`) lacked `WP_MELEE`, `WP_CONCUSSION` and `WP_BRYAR_OLD`;
  - `Pickup_Weapon` (`game/g_items.cpp`) read `ps.ammo[giTag]`, the weapon's number as an ammo type: `ps.ammo[weaponData[weapon].ammoIndex]` now;
  - `Q3_SetWeapon` (the three trees) and `misc_weapon_shooter` (multiplayer and single player) converted `GetIDForString`'s `-1` into a weapon: the name goes through `G_WeaponFromName` (Jedi Outcast: the `-1` tested), an unknown one ignored;
  - the single player's vehicle muzzle (`src/code/cgame/cg_players.cpp`) read `weaponData[weapMuzzle[i]]`, while `weapMuzzle` indexes `g_vehWeaponInfo`: its `iMuzzleFX` through `CG_PlayEffectID`;
  - the single player's datapad (`CG_DrawDataPadWeaponSelect`, `src/code/cgame/cg_weapons.cpp`) tested `CG_WeaponCheck( i )`, a counter left at the last weapon, for the icons on the right: `weaponSelectI`;
  - `NPC_ChoosePainAnimation` (`game/NPC_reactions.cpp`) read the length of animation `-1` when none was chosen: 0 then; and it took the frame count from the NPC's animation set and the frame time from the humanoid's (`bgHumanoidAnimations`), as did the knockdown's length in `G_CheckSpecialDeathAnim` (`game/g_combat.cpp`): both go through `BG_AnimLength( localAnimIndex, anim )`, one set for both, as the single player does; checked in the game's files: `BOTH_PAIN1` is at 20 frames per second (30 for the rocket trooper) in the sets of the rancor, the wampa, the AT-ST, the howler, the probe, the protocol droid, R2-D2, the hazard trooper, the assassin and saber droids, 10 for the humanoid; in multiplayer the rancor, the wampa and the howler never reach `NPC_ChoosePainAnimation`: their pain functions of their own (`NPC_Rancor_Pain`, `NPC_Wampa_Pain`, `NPC_Howler_Pain`, as in 1.01) take the delay from the animation's own timer (the summary's line cited the rancor and 14 sets until 20 November 2026, corrected in plan step 20 part 9: 11 sets have `BOTH_PAIN1` faster than the humanoid's);
  - without a visible effect: the portable E-Web wrote its weapon's number into the weapon mask (`Bit( WP_EMPLACED_GUN )` now); cgame registered the ammo model of the ammo type numbered as the weapon (`CG_RegisterWeapon`); the session cvars of the single-player games (`sessionpowers`, `sessionweapons`) with more numbers than powers or weapons wrote past their tables; Jedi Outcast's anim sounds with `cg_reliableAnimSounds 2` and its saber lock print with `d_saberCombat 1` read out of their tables; the tactician award's loop (`g_log.cpp`, never called) read one cell past its table.
- Found while naming the unnamed enumerations and fixed after a decision (17 November 2026, plan step 18):
  - the troopers' movement speech (`NPC_ST_StoreMovementSpeech`, `NPC_ST_SayMovementSpeech`, the three trees): `speechType_t` starts with `SPEECH_NONE`, so that a cleared NPC has nothing to say, and `gNPC_t::movementSpeech` is a `speechType_t`; the Galak mech counted its anger taunts in the same field, they have their own, `angerTaunts` (`AI_GalakMech.cpp`, Jedi Outcast and multiplayer);
  - the drain's minimum length (`WP_CheckForceDraineeStopMe` and `WP_ForcePowersUpdate`, `src/code/game/wp_saber.cpp`) was written `level.time-(debounce > level*500)`, always true; the drain's start is kept in `gclient_t::forceDrainStartTime` (set by `WP_ForcePowerStart`), since the debounce it read also paces the drain's damage;
  - the grip (`WP_ForcePowersUpdate`, `src/code/game/wp_saber.cpp`) started from its own level for an entity that is not a character, where the absorb's result is meant: an empty optional for an object;
  - `Player_CacheFromPrevLevel` (`src/codeJK2/game/g_client.cpp`) registered the item of slot `i-1` for bit `i`: slot `i`, as in Jedi Academy;
  - ICARUS's `GetVector` and `Get` (`TaskManager.cpp`, the three trees) returned `TASK_FAILED`, 1, read as true by their callers, when a tag or a value was not found: `false`; the single player's `CIcarus::Run` returns whether the script started, so that `g_ICARUSDebug 3` prints "Script … executed" for the scripts that started, not for those that failed; `SetFloatVariable` returned `VTYPE_FLOAT`, 1, for a variable that does not exist: `false` (no caller reads it);
  - without a visible effect: the single player's `UI_GiveInventory` (`src/code/ui/ui_main.cpp`) accepted a negative slot from a menu script, which wrote before `inventory`; `CG_UseItem` (`cg_event.cpp`, the single-player games) let the slot `INV_MAX` through, one past the last item;
  - the single player's and Jedi Outcast's saved games change format (the drain's start, Galak's taunts): `iSAVEGAME_VERSION` is 2, and a save of an earlier build is refused with a message.
- The Debug builds (`_DEBUG`, without `FINAL_BUILD`) compile again, with no warning (17 November 2026): the code only they compile had `char *` given `const char *` values, a saber colour given to `G_TestLine`'s integer, `%d` for sizes, strings given as formats, a format without the `%s` of its argument, a `switch` without its default, two variables never read.
- Found while typing the families of flags and fixed after a decision (18 November 2026, plan step 18):
  - `GTB_ALL` (`game/bg_public.h`, the votes' game types in `g_cmds.cpp`) was 0x1FF, without `Bit( GT_CTY )`;
  - `turret_fire` (`src/codeJK2/game/g_turret.cpp`) called `gi.pointcontents( start, MASK_SHOT )`, the mask as the entity to skip, any contents refusing the shot: `pointcontents( start, ent->s.number ) & MASK_SHOT`, as Jedi Academy;
  - `GHOUL2_CRAZY_SMOOTH` (`G2API_AbsurdSmoothing`, set by cgame for 1 s on a player strafing on an emplaced gun, `bg_pmove.cpp`) had the bit of `GHOUL2_ZONETRANSALLOC`, which tells the trace code that the transformed vertices are in the zone (`G2_misc.cpp`): 0x4000 now;
  - the single player's pull attack (`src/code/game/bg_panimate.cpp`) passed `SETANIM_FLAG_HOLD` as `NPC_SetAnim`'s blend time: `SETANIM_FLAG_OVERRIDE|SETANIM_FLAG_HOLD`, the timers set after it as before;
  - the gets of ICARUS (`Q3_Interface.cpp` of the single-player games, `g_ICARUScb.cpp`) return a flag through `Q3_FlagValue`, 1 or 0;
  - `NPC_CollectCombatPoints` (`NPC_combat.cpp`, multiplayer and Jedi Outcast) skipped the DUCK and FLEE points when one was asked for: the reverse, as the single player; the INVESTIGATE test, inverted in the three games, kept with a comment;
  - without a visible effect: `FL_BOUNCE` (multiplayer `g_local.h`) shared `FL_UNDYING`'s bit, a free one now; the single player's `CP_SHORTEST_PATH` was `CP_SAFE` (`0x0004000`, a zero missing), so its retry test was never reached; the simplified `FX_AddSprite` and `FX_AddLine` (`FxUtil.cpp` of the single-player games) and the navigation's debug lines (`cg_main.cpp`) gave `FX_ALPHA_LINEAR` as the alpha's parameter and not as a flag, two of them dropping their `flags`; the botlib's trace (`sv_bot.cpp`) wrote the surface flags into `value` where the grapple test reads `flags`, and `AAS_TravelFlagForType_inline` tested `TRAVELFLAG_NOTTEAM1/2` on a zero; the renderers' `contents != CONTENTS_SOLID` on a node (always true) and `G2_bones.cpp`'s `flags < 0` (never true) removed.
- Found while turning the numeric `#define` constants into constants and fixed after a decision (19 November 2026, plan step 18):
  - the UI's `globalservers` (`ui/ui_main.cpp`) sent the source minus one, while `CL_GlobalServers_f` reads 0 as every master server and n as `sv_master<n>`: the source itself (section 25);
  - `PM_FootstepForSurface` (`game/bg_pmove.cpp`) returned material 0 for a `nosteps` surface, sent with `EV_FOOTSTEP` and played by cgame as stone (in Quake 3, 0 meant no event): an empty optional, and no event;
  - `PC_ExpandBuiltinDefine` (`botlib/l_precomp.cpp`) freed `ctime`'s static buffer for `__DATE__` and `__TIME__`: not freed;
  - the botlib's debug line colours (`botlib/botlib.h`) follow the renderer's three bits (`R_DebugPolygon`): `LINECOLOR_BLUE` 4, `LINECOLOR_YELLOW` 3, `LINECOLOR_ORANGE` renamed `LINECOLOR_MAGENTA` (5);
  - without a visible effect: `BotAISetup` (`game/ai_main.cpp`) returned false when the botlib's setup succeeded (`!trap->BotLibSetup()`, ignored by `G_InitGame`), false on a failure now; `PS_ExpectTokenType` (`botlib/l_script.cpp`) named the expected punctuation by indexing the table with its number (the next entry; past the table for the last), on a path no caller takes: `PunctuationFromNum`.
- The weapons' charge (19 November 2026, plan step 20): `WP_FireBryarPistol`, `WP_DisruptorAltFire`, `WP_BowcasterMainFire`, `WP_DEMP2_AltFire`, `WP_FireRocket`, `WP_FireThermalDetonator` and `WP_FireVehicleWeapon` (`game/g_weapon.cpp`) count from `ps.weaponChargeTime` or `ps.rocketLockTime` (set from `cmd.serverTime`) to `ps.commandTime`, the time of the command that fires (Pmove sets it before `PM_Weapon`, and `ClientEvents` runs right after Pmove), instead of `level.time`; cgame counted to `cg.cmdTime` already. A `misc_weapon_shooter` (`game/g_misc.cpp`) sends no command: its `commandTime` is set to `level.time` before it fires, so that its charge stays full, as before (only single-player maps of the game have one). The bots' AI still decides when to release with `level.time` (`ai_main.cpp`).
- The legs of a player pushing against a wall: section 9. `g_randFix`: section 12.

</details>

### 25. Client fixes outside the netcode

Made on the way while testing, mostly as TaystJK does.

| fix | before | now | files |
|---|---|---|---|
| Menu cursor speed | moves by raw screen pixels: 3 times faster than the desktop at 1920 x 1080 | moves by mouse deltas scaled to the 640 x 480 menu space | `ui_main.c` |
| `r_aspectCorrectFonts` | narrows the spacing but not the glyphs: letters overlap | narrows both | `tr_font.cpp` |
| Keypad with num lock on | keypad keys become digits everywhere, so `KP_*` binds never fire in game | digits only while typing (and not while binding a key) | `sdl_input.cpp`, `ui_shared.c` |
| Right shift, ctrl, alt | the same keys as the left ones | keys of their own (`RSHIFT`, `RCTRL`, `RALT`), bindable apart; shortcuts accept either side | `cl_keys.cpp`, `sdl_input.cpp`, `ui_shared.c` |
| Dropped while connecting | a client dropped before its game screen runs (kicked while loading, for example) keeps waiting | back to the menu with the reason | `cl_parse.cpp` |
| Image files | JPEG decoded by libjpeg 9a on Windows and by the system's library on Linux, which give other pixels (up to 71 levels of 255 on a channel of a player's skin); PNG refused unless 8-bit RGB or RGBA with sides that are powers of two; a malformed TGA stops the game | one decoder for JPEG, PNG and TGA on every platform, the same pixels everywhere (those the Linux build drew), every PNG accepted, a malformed image refused with a warning. The game's JPEG files decode 2.4 times as fast and its PNG files 2.0 times: a map loads in 0.89 s instead of 1.16 s and a player's model in 19 ms instead of 27 (measured with the textures' compression off) | `src/shared/rd-common/tr_image_codec.cpp`, `tr_image_load.cpp` |
| Archive files (pk3) | read through minizip (2010) on zlib 1.2.8 (2013) on Windows and on the system's zlib on Linux; an archive's directory read entry by entry, twice; the CRC-32 of a file computed and never looked at; a file that could not be inflated handed to the game as it was | one reader on every platform, on zlib-ng 2.3.3 built with the project: all of the game's files read in 2.08 s instead of 3.70 (1.8 times as fast), the directories of its four archives in 0.5 ms instead of 81; a file whose CRC-32 is wrong, or that cannot be inflated, is refused with a warning, as a file that is not there; a file read in pieces (a video, music) is still inflated as it is read | `src/shared/qcommon/q_pk3.cpp`, `q_pk3.h`, `src/shared/qcommon/files.cpp` |
| Textures handed to the graphics card | compressed by the graphics driver as each one is loaded (`r_ext_compress_textures` 1), in DXT5 even when opaque: the client waits for the driver, 43 ms of the 71 a player's model takes, and a map loads in 2.06 s; the textures lose detail | handed as they were decoded (`r_ext_compress_textures` 0; 1 brings the compression back, for a graphics card short of memory): a player's model loads in 28 ms instead of 71 and a map in 0.89 s; the textures keep the files' pixels; they take more video memory, 436 MB instead of 130 on `mp/ffa3` with ten players' models | `rd-vanilla/tr_init.cpp` |
| Shader scripts | a texture with no script is looked up in the scripts' table, then searched again by reading the whole text of every script: 7.6 ms of each player's model, for nothing | looked up in the table only, which holds every script (checked on the game's scripts): a player's model loads in 20 ms instead of 28 | `rd-vanilla/tr_shader.cpp` |
| Server browser | a ping of 0 means "request sent", "no answer" and "answered in 0 ms" at once, so a server answering in under 1 ms (same machine or LAN) is hidden | each server has a state of its own (not asked, waiting, answered, no answer); the ping is only a measure | `client.h`, `cl_main.cpp`, `cl_lan.cpp`, `cl_uiapi.cpp`, `ui_public.h`, `ui_main.c`, `ui_syscalls.c` |
| Shots' sound and muzzle flash | the muzzle effect, which carries the shot's own sound, started again at every frame drawn during the 20 ms of the flash: 3 copies of the sound at 125 frames per second, 7 at 333, so a shot was louder the higher the frame rate (rocket launcher: -10.9 and -6.5 dBFS while firing), with more of the flash's particles | once per shot at every frame rate, as the single-player game does (rocket launcher: -12.2 dBFS at 125 and at 333) | `cg_weapons.c`, `cg_local.h` |
| A looping sound set again on an entity | added in a new slot each time (up to 8), the copies summed by the mixer | updated in place | `cg_ents.c` |
| Server browser, the sources "Internet 1" to "Internet 5" | "Internet 1" asks every master server, "Internet 2" Raven's (`sv_master1`) instead of master.jkhub.org (`sv_master2`): an empty list while Raven's does not answer; `sv_master5` is never asked alone | "Internet n" asks `sv_master<n>` | `ui_main.cpp` |
| The console's messages (22 November 2026) | each part writes its warnings and errors its own way: `WARNING:` with or without yellow, the bot library's `Warning:`, `Error:` and `Fatal:` (an error that stops nothing), the renderers' warnings yellow without a word and their developer's messages red; the effects system's messages printed always in the single player outside the final build (never in a release), only with `developer` in the multiplayer; some debug messages written only to the Windows debugger's window; two debug messages print no number (`%4.2 secs`) | one look per level in every part: `^3WARNING: ` before a warning, `^1ERROR: ` before an error that stops nothing, a developer's message without a colour; the effects system's and the debugger's messages are developer's messages, in the console; the two debug messages print their number | `src/shared/qcommon/q_fmt.h`, every printing call |
| A message written in several calls (22 November 2026) | a cvar out of range warns in up to four calls, yellow up to the first; a shader without its opening brace in three; about 25 warnings, errors and developer's messages forget their line's end and run into the next message | one line per message, in its colour: `cvar 'x' must be integral and is out of range (min 0), setting to 0` | `q_fmt.h`, `cvar.cpp`, `tr_shader.cpp` |
| A script's entity named by a script runner (22 November 2026) | a `target_scriptrunner` run on an activator without a script name names it `newICARUSEnt<n>` in a buffer that later formatting overwrites, so that its name can change and the scripts no longer find it | the name copied | `g_target.cpp` |
| The single player's videos played outside a level: the opening logos, Jedi Outcast's last video (27 November 2026) | a key in a video's first 1.2 s refused, a guard against skipping a level's video by firing: the first click on the LucasArts logo was lost | stopped by the first key; a level's video keeps its 1.2 s | `cl_keys.cpp` |

The server browser's change adds an import for the UI module, `trap->ext.LAN_GetServerState`, and makes `serverInfo_t` larger, so an existing `servercache.dat` is discarded once, favourites included.

### 26. The shots: towards the point aimed at

**Before.** Every shot of a hand weapon started at a fixed point beside and below the eyes (12 units forward, 0 to 8 right, 4 to 10 down) and flew parallel to the view, never towards the point the eyes look at. The default crosshair (`cg_dynamicCrosshair 1`) was drawn where such a straight shot lands, so it moved on the screen with the distance of what is aimed at; with `cg_dynamicCrosshair 0` it stayed at the centre while the shots landed 2 to 8 units right of it and 4 to 6 below. The NPCs and the bots aimed from their eyes and fired from that point: a stormtrooper's bolts passed 6 units right of the eye level it aimed at and 6 below, and never touched a player standing with a saber (225 bolts in 48 tries). The emplaced gun's two barrels fired 10 units on each side of the point its crosshair shows, beside a thin model. A swoop's rider saw the rider's own crosshair while the swoop's lasers fired level from 43 units lower. All of it is the base game's (the same code in 1.01).

**Now.** A ray from the eyes along the view finds the point aimed at (against the world and the players' models, as far as the map's view distance), and the projectile leaves the same fixed point as before, turned towards that point: it lands at the centre of the screen at any distance, unless something stands between that point and the point aimed at, which it then hits. The beams (the disruptor, the concussion rifle's alternate fire, the DEMP2's charged blast) are traced from the eyes. The emplaced gun's two barrels fire towards the point its crosshair shows. The NPCs and the bots that shoot aim at the chest of the model they see, from their eyes, as a player aims; holding a saber, the melee or nothing, they aim as before, since their view moves their blade or their blows. The crosshair (`cg_dynamicCrosshair 1`, the default) is drawn where the shot lands: at the centre in first person, and in third person below the centre by the camera's height; `cg_dynamicCrosshair 0` keeps it at the centre of the screen, in both views. A swoop's rider whose swoop fires (holding the melee or the saber off) sees the swoop's crosshair. Unchanged: the thrown items (the thermal detonator, the trip mine, the det pack), the stun baton, the melee, the saber, the E-Web, the vehicles, a rider's blaster, and in third person the bolt's start against the drawn barrel (5 to 24 units behind its flash).

**Why.** What is seen should be what happens, for every shooter alike: a crosshair that moved, NPCs and bots that missed what they aimed at exactly ([surveys/shot-offsets.md](surveys/shot-offsets.md) section 11). The start stays at the fixed point, inside the shooter's box (decided on 20 November 2026, after the game was tried): started at the muzzle of the weapon's model, as Unreal Engine 5's tutorial does, a shot depended on the animation the server last posed (the first shot after an idle animation left the lowered weapon) and, for a player against a wall, started inside the wall and ran along its face.

**Gain.** Measured in the scripted fights, 48 tries each:

| case | before | now |
|---|---|---|
| A blaster bolt aimed level at a player 256 units away | the right arm in one shot in two, the head otherwise | the head every time |
| A rocket aimed from the eyes at a player's head | passes beside the head and hits the right arm: 85, the player survives | hits the head: 130, the player dies |
| A stormtrooper against a player standing with a saber | no bolt touches the player | the bolts reach the player: 218 blocked by the saber, sent back into the trooper (2 deaths) |
| A stormtrooper against a player holding the bryar | hits the left arm (1,656 damage) | hits the chest (2,070) |
| The emplaced gun aimed 2 degrees up at a player 204 units away | 144 of 432 bolts hit | 192 |
| A blaster bolt against a wall 16 to 48 units away, the view 75 degrees up to 75 down | lands beside the centre of the screen | lands on the point the eyes aim at, to 1 unit (480 shots) |

The bots with guns in free play (siege, 6 servers of 3 minutes before and after, measured with the weapon model's muzzle as the start, since dropped): 17.0 % of their missiles hit a player before, 16.4 % then, within the spread of the servers (13.8 to 19.0 %); their own aim error is larger than the offset was.

The cost: per shot, two traces (the ray from the eyes, the line from the eyes to the muzzle point); per aim of an NPC or a bot that shoots, the bolt of its target's chest (1.2 to 2.5 microseconds), at the 20 Hz instants; no byte on the network; 12 bytes per client, the angles its model was posed with. cgame: two more traces per frame for the crosshair while a projectile weapon is held.

<details>
<summary>Technical details</summary>

- `game/g_weapon.cpp`: `WP_AimPoint`, the ray against `CONTENTS_SOLID|CONTENTS_BODY` with the ghoul2 models, as the crosshair's, as far as `g_cullDistance` (the map's `distanceCull`). `WP_AimFromMuzzle`, in `FireWeapon` for the weapons of `BG_FiresFromMuzzle` (the bryars, the blaster, the bowcaster, the repeater, the flechette, the rocket launcher, the DEMP2's and the concussion rifle's primary fire; `game/bg_misc.cpp`, shared with cgame): from `CalcMuzzlePoint`'s point, unchanged, the direction towards the point aimed at; a point at or behind the muzzle point keeps the view's direction. That point is inside a player's box (at most 14.4 units from its axis, the box 15 wide on each side), so no wall stands between it and the eyes; for a shooter whose box is smaller, a wall in between is hit there, the shot going on along the line from the eyes to the muzzle point (towards the point aimed at, on the same wall, it would run along the wall's face). The weapons' spread is applied after, unchanged. The eyes are the entity's origin raised by its view height (`WP_EyePoint`), as `CalcMuzzlePoint` takes them.
- `WP_FireConcussionAlt` and `WP_DEMP2_AltFire` trace from the eyes; the concussion's beam is still drawn from the muzzle point (`EV_CONC_ALT_IMPACT`'s `origin2`). `WP_FireEmplaced`: the point from between the barrels (46 above the gun), each barrel's bolt towards it.
- The NPCs and the bots: `G_AimSpot` (`game/g_utils.cpp`) gives the target's `thoracic` bolt with the angles the server last posed its model with (`renderInfo.modelAngles`, stored by `WP_SaberPositionUpdate` after `G_G2PlayerAngles`), for a shooter holding a weapon that is not the saber, the melee or nothing; `NPC_FaceEnemy`, `NPC_CheckCanAttack`, `NPC_BSPointShoot`, `NPC_BSAdvanceFight` and `NPC_BSFollowLeader` aim at it from the eyes (`SPOT_HEAD_LEAN`), the sniper at the last place it saw its enemy from the eyes (`Sniper_CheckFireState`); the line-of-fire tests and the grenadier's and the Galak mech's lobs are unchanged. The bots: `headlevel` in `ai_main.cpp`.
- `cgame/cg_draw.cpp`, `CG_ScanForCrosshairEntity`: the ray from the predicted eyes along the predicted view; for those weapons, the same muzzle point computed from the predicted origin and view (`WP_MuzzlePoint`, as `CalcMuzzlePoint`), the same wall test, then the line to the ray's end, the crosshair where it first meets something. A speeder's rider holding the melee or the saber off: from the speeder (`CG_CalcVehicleMuzzlePoint`); a rider's own weapon: as before. `cg_dynamicCrosshairPrecision` removed, its default kept (the models traced); `cg_dynamicCrosshair` kept (decided on 20 November 2026), 0 drawing the crosshair at the centre as before.
- The scripted fights and their goldens: [progress/step-20-scenarios.md](progress/step-20-scenarios.md) section 18, [tools/gamestats/GOLDEN.md](tools/gamestats/GOLDEN.md) section 3.

</details>

## E. Reference

### 27. Settings and commands

Removed cvars. A removed cvar keeps the behaviour of its default, except `cg_smoothClients`.

| cvar | what stays | section |
|---|---|---|
| `sv_fps`, `bot_debugClientRate` | a tick of 10 ms; the bots send a command per tick | 1 |
| `g_synchronousClients`, `g_forceClientUpdateRate` | one command per player and tick; the last command repeated 250 ms | 2 |
| `cl_timeNudge` | the display delay is computed | 4 |
| `cg_smoothClients`, `g_smoothClients` | the other players between two snapshots, not smoothed; the messages as with `g_smoothClients 1` | 5 |
| `rate`, `snaps`, `sv_clientRate`, `sv_ratePolicy`, `sv_minRate`, `sv_maxRate`, `sv_lanForceRate`, `sv_snapsMin`, `sv_snapsMax`, `sv_snapsPolicy` | a snapshot per tick | 8 |
| `cl_maxpackets`, `cl_lanForcePackets`, `cl_packetdup` | a packet per tick, the commands of 2 packets repeated | 8 |
| `cl_timeout`, `sv_timeout`, `sv_zombietime` | 60 s while connecting, 30 s in the game, 2 s of zombie state | 8 |
| `pmove_float`, `pmove_fixed`, `pmove_msec` | exact velocity, steps of one tick | 9 |
| `cg_smoothCamera` | the default's camera damping | 11 |
| `g_randFix` | the default's correct range | 12 |
| `g_debugSeed` | the engine's `com_debugSeed`, which seeds every stream of the three games | 37 |
| `sv_padPackets` | nothing: it padded only OpenJK's format | 13 |
| `cg_dynamicCrosshairPrecision` | the crosshair's ray traced against the players' models | 26 |
| `s_khz`, `s_mixahead`, `s_mixPreStep`, `s_separation`, `s_testsound`, `s_soundpoolmegs`, `s_sdlBits`, `s_sdlChannels`, `s_sdlDevSamps`, `s_sdlMixSamps` (and the menus' `s_UseOpenAL`) | OpenAL Soft mixes at the device's rate and pans in 3D; every sound decoded when registered, outside the zone | 33 |
| `s_doppler` | Doppler on, for every moving sound | 33 |
| `s_threshold1` to `s_threshold4` | the lip sync's levels, Jedi Academy's (0.3, 4, 6, 8) for every game | 33 |
| `cl_aviFrameRate`, `cl_aviMotionJpeg`, `cl_avi2GBLimit`, `cl_forceavidemo`, `r_aviMotionJpegQuality`, and the single player's `cl_avidemo` | no video recorder: a player records with a program made for it | 33 |
| `r_gammaShaders` (the multiplayer's) | the brightness applied by the output stage's pass, to every picture | 42 |
| `r_overBrightBits` | no overbright, as with the default: the lights at their own scale | 42 |

Changed: `cl_shownet` has levels 1, 2 and 3 (section 13). `g_userinfoValidate` has a new default (section 8). `r_ext_compress_textures` defaults to 0: the graphics driver no longer compresses the textures (section 25); a configuration file holds it only where a player had set 0, so nothing is to reset.

Changed: the lagometer (`cg_lagometer 1`) counts in time and shows one more measure. A column was one drawn frame (upper graph) or one snapshot (lower graph): the graph held 0.48 s at 100 frames per second and 0.14 s at 333, so a loss left it before it could be read, sooner for a player at a high frame rate. A column is now 50 ms of real time and shows the worst of what happened in it: 2.4 s for everyone. Every bar is a delay, so a thin graph is a good connection: the usual values are about one pixel high.

| where, colour | what it measures | what it means for you | usual | when it grows |
|---|---|---|---|---|
| top, cyan (new) | how long your command waited on the server before the tick that ran it (the input margin; the highest of the 50 ms) | a delay added to your keys and your mouse; the client keeps it as short as it safely can | about 4 ms: 1 pixel (12 pixels for 50 ms) | your packets reach the server irregularly: the client then sends them earlier so that none is late |
| top, magenta (new), the whole band | a command reached the server after its tick | the server ran that tick with your previous command; what you had just pressed took effect a tick later (10 ms) | none | packets of yours late or lost |
| middle, blue, below the line | how far behind the newest snapshot the world is drawn (the farthest of the 50 ms) | the other players are shown that much in the past, on top of the network's own delay | 7 to 11 ms: 1 pixel (16 pixels for 150 ms) | the snapshots arrive irregularly or get lost: the client waits longer so that the next one is there in time |
| middle, yellow, above the line | a frame was drawn after the newest snapshot ran out: the next one was late | the other players stand still for that time, then catch up at once | none | a snapshot later than the blue delay covers |
| bottom, green | your ping (the highest of the 50 ms) | the round trip to the server: your own movement is predicted, but hits, damage and the other players reach you after it | 20 ms is half a pixel, 100 ms about 3 pixels (24 pixels for 900 ms, as before) | the route to the server |
| bottom, yellow in place of green | the server held a snapshot back to respect the rate | a snapshot sent late | none: the rate is 200,000 bytes per second | a message far larger than usual |
| bottom, red, the whole band | a snapshot of the server never arrived | the other players' positions of that tick are missing: if the next snapshot is not there in time, they stand still until it comes (the yellow above the line) | none | loss on the way from the server |
| an empty column | nothing was read in those 50 ms | the connection was interrupted, or the client itself was stopped (a load, for example) | none | |

The client gives cgame the input margin with each snapshot (`snapshot_t`, `cgame/cg_public.h`): the client and cgame must be of the same build. Details: [progress/step-14-fixed-tick.md](progress/step-14-fixed-tick.md) section 16.

Added cvars, all for debugging or measuring:

| cvar | side | what it does |
|---|---|---|
| `sv_netcodecCompare` | server | also codes each message and each client packet in OpenJK's format and prints the byte ratios, every 10 s (1) or every message (2) |
| `sv_netcodecMaxBytes` | server | the largest message the server codes (0: 49,152 bytes), to test what happens to larger ones (section 19) |
| `cl_debugInterpForce`, `cl_debugInterpDelay` | client, cheat-protected | force a display delay (section 4) |
| `s_muteWhenUnfocused` | client, archived, default 1 | the sound muted while the window has not the focus (section 33) |
| `s_reverb` | client, archived, default 1 | the reverb of the zones; 0 switches it off, the walls and the water still muffling (section 34) |
| `s_musicMult` | client (multiplayer cgame) | the music's level, which the cgame lowers to 0.3 under a short tune (becoming the Jedi Master, dying as the Jedi Master) and raises back over a second; at most 1, back to 1 at a disconnection (section 38) |
| `s_device` | client, archived, default empty | the output the sound plays on, by its name ("Sound device" in the sound menus); empty, the system's default, followed when it changes (section 36) |
| `com_debugSeed` | engine (server and client, the three games), cheat-protected | a non-zero value seeds every random number stream at each level's start (the game, the cgame, the UI, the engine, the renderer's weather), each with its own sequence: with `fixedtime` set before the level loads, a run replays the same game and the same sounds (section 37) |

Added commands. The statistics commands print their counters since the previous call.

| command | side | what it prints |
|---|---|---|
| `svnetstats` | server | the server's CPU time: the server frames and, within them, the game frames, the bots, the players' commands and the snapshots sent, each with its count, mean and largest time, the frames longer than a tick and the share of one core. Then for each client: messages, bytes per message, messages sent raw, snapshots without a base, snapshots cut to fit, the base's age; the ping; server commands sent and repeated, by cause; client packets, bytes per packet, packets not decoded or dropped above the packet rate, commands per packet, packets lost on the way and the length of each run of losses; how much the time a snapshot leaves changes against the game clock; the ticks run with and without the player's command, the late commands, the mean and lowest input margin |
| `netstats` | client | messages lost on the way and not decoded, bytes per message, snapshots without a base, the base's age; the display delay, the jitter of the arrivals, the frames drawn past the newest snapshot; the command clock (how far ahead of the drawn time, the mean and lowest input margin the server reported); client packets sent, bytes per packet, packets coded without a base |
| `predictmiss` | client (cgame) | writes `predictmiss.txt`: every difference between the predicted player state and the server's (section 6) |
| `gamestats` | server (game module) | combat statistics: saber swings, wounds, damage, blocks, parries, locks; projectiles blocked, force damage, deaths; for each force power the starts, the periodic events and the time held; the health taken and the deaths by cause, the shots by weapon and fire mode, the saber attacks started, bounced or deflected by style, the special saber moves started by move; during a scenario, what each bot did by role (health and force given and taken, the instants off the ground, knocked down, mind tricked and in a saber lock, the distance moved, the end place) and what they added to the scores (each role's score and captures, the red and blue team scores) |
| `scenario` | server (game module), cheat | `scenario saber <repetitions> <fast\|medium\|strong\|dual\|staff> <mode> [distance]` (the modes `idle`, `attack` and the special moves: `throw`, `lock`, `kata`, `back`, `backcrouch`, `jump`, `lunge`, `cartwheel`, `backflip`, `kick`, `airkick`, `rollstab`, `stabdown`), `scenario force <repetitions> <power> [level] [distance] [the defender's level]`, `scenario weapon <repetitions> <weapon number> <0\|1\|2> [distance]`, `scenario turret <repetitions> <g2\|hoth> [distance]`, `scenario off`, with the options `hold=`, `rep=`, `pickup=` (a holocron or the Jedi Master's saber in front of the attacker, which walks onto it first) and `target=ally` (the attacker's teammate as the target) (the syntax in full at the top of `game/g_scenario.cpp`): two or three bots that are not spectators, placed face to face and reset every 2 s, play a fixed script of commands timed in milliseconds; `gamestats` is printed at the end. With `scenario_trace <n>` the two bots' state at every 20 Hz instant and every health loss are printed for the first n repetitions. Every reset puts the bots back in a state that does not depend on what came before |

`cg_showmiss` prints two more kinds of lines (section 2).

Removed commands: `video`, `stopvideo` (the video recorder), `mp3_calcvols` (section 33). Changed: `s_show 1` (cheat) writes the sound log, one `SNDLOG` line per sound started, replaced, merged or ended and the voices playing (section 33).

### 28. Build, tests and repository

| | OpenJK | this branch |
|---|---|---|
| New library | | `src/codemp/netcodec/`: C++20, its interface for the engine in `engine.h`, trained tables compiled in; linked by the client and the dedicated server |
| Math functions | the C library's | `libs/llvm-libc/`: the headers of LLVM libc's math functions (Apache 2.0 with LLVM exceptions, a fixed commit, `update.py` to refresh them); `src/shared/qcommon/q_libm.cpp` in the static library `qlibm`, linked by every binary that uses `q_math.c`, the single-player targets included; `-ffp-contract=off` for GCC and Clang |
| Image libraries | libjpeg 9a and libpng 1.6, built with the project on Windows only; the system's on Linux | `libs/wuffs/` (Wuffs, MIT, decodes JPEG, PNG and TGA) and `libs/stb/` (stb_image_write, writes the screenshots and the video capture), vendored and compiled inside `src/shared/rd-common/tr_image_codec.cpp`, the only file that knows them, shared by the multiplayer and the single-player renderers with the loader (`tr_image_load.cpp`); the multiplayer renderers no longer link libjpeg, libpng or zlib; `lib/jpeg-9a` and `lib/libpng` are removed from the repository, with the options `UseInternalJPEG` and `UseInternalPNG`; the single-player renderers build the same two files; the measurements: `docs/modernization/tools/imagebench/`, [surveys/image-libraries.md](surveys/image-libraries.md) |
| Compression libraries | zlib 1.2.8, built with the project on Windows only, the system's on Linux; minizip, always the repository's | `libs/zlib-ng/` (zlib-ng 2.3.3, zlib licence, its source tree built by its own CMake file on every platform), known to `src/shared/qcommon/q_pk3.cpp` only, one file for the multiplayer and the single-player engines; `lib/zlib` and `lib/minizip` are removed from the repository, with the option `UseInternalZlib`; nothing links the system's zlib any more; the measurements, the checks of the reader and its fuzz target: `docs/modernization/tools/zipbench/`, [surveys/compression-libraries.md](surveys/compression-libraries.md) |
| File system | `qcommon/files.cpp` twice: 3,071 lines in the single-player engine, 4,384 in the multiplayer one, 56 functions identical and 21 nearly | one: `src/shared/qcommon/files.cpp` and its interface `src/shared/qcommon/files.h`, built by both engines; what differs between them (the configuration files' names, the modules loaded from pk3 files, the sound buffer cleared before a read) is in each engine's `qcommon/files_engine.h`. The multiplayer engines' machine code is unchanged; the single-player engine gets the multiplayer file system, whose pure-server code stays idle in it, and opens the same files in the same order on a scripted session |
| Random numbers | `holdrand` in `q_math.c`, the C library's `rand()` | `src/shared/qcommon/q_rand.h` and `q_rand.c`; tools `docs/modernization/tools/rng/` |
| Scripted fights | | `docs/modernization/tools/gamestats/scenarios.sh` runs the scripted cases on Windows or Linux and compares them with `scenarios_golden.txt`, the same on every platform (`scenarios_golden_step14.txt` holds the counts of step 14 part 4b, the commit the first cases come from; 40 cases until plan step 20, which widens them: [progress/step-20-scenarios.md](progress/step-20-scenarios.md)) |
| Tests of the library | | round trip on about 36,000 recorded messages, 100,100 synthetic messages, malformed-input test and fuzzer, determinism hashes per compiler, movement determinism (game against cgame, compiler against compiler); client packets: 100,000 synthetic packets, the recorded packets through the library and through the engine interface, 400,000 malformed packets, and a fuzzer |
| Language and targets | C++11 (`-std=c++11`); Windows in 32 and 64 bits, Linux, macOS, the BSDs, and code for PowerPC, Alpha, Solaris, IRIX and Cygwin | C++20 for every C++ target, MSVC in strict mode; Windows x64, Linux and macOS (x86_64 and arm64) only (section 31) |
| Compilers checked | | MSVC x64, GCC 12 and 16, Clang (llvm-mingw on Windows, Clang 14 on Linux); 32-bit builds were checked until 16 November 2026 |
| `.gitattributes`, `.gitignore` | | line endings of scripts, demo files and local traces ignored |
| GitHub workflow | installs libjpeg, libpng and zlib on Linux and macOS; builds Windows and Linux in 32 and 64 bits | installs none of them, since no build links them; 64-bit jobs only (not run) |
| Study, plan, progress | | `docs/modernization/` (this folder) |

### 29. Name: ModernJK

**Before.** The game carries OpenJK's names: the programs `openjk.x86_64` and `openjkded.x86_64`, the window "OpenJK (MP)", the configuration `openjk.cfg` (`openjk_server.cfg` for the dedicated server), the settings folder `Documents\My Games\OpenJK` on Windows and `~/.local/share/openjk` on Linux, the modules in a folder `OpenJK`, the version "OpenJK-MP".

**Now.** Everything a player sees is named ModernJK: `modernjk.x86_64` and `modernjkded.x86_64`, the window "ModernJK (MP)", `modernjk.cfg` and `modernjk_server.cfg`, `Documents\My Games\ModernJK` and `~/.local/share/modernjk`, the folder `ModernJK`, the version "ModernJK-MP". The single-player game is renamed the same way: `modernjk_sp.x86_64`, the window "ModernJK (SP)", `modernjk_sp.cfg`, the version "ModernJK".

**Why.** The branch is a game of its own (15 November 2026): it does not play with OpenJK or the base game (section 30), so it does not take their names or their files.

**Gain.** An OpenJK and a ModernJK installation no longer share a configuration file or a settings folder. The cost: a configuration of OpenJK is not read any more; copy `openjk.cfg` to `modernjk.cfg` (and `openjk_server.cfg` to `modernjk_server.cfg` on a server) to keep it.

<details>
<summary>Technical details</summary>

`src/codemp/qcommon/q_shared.h`: `PRODUCT_NAME` (which names the configuration files), the window and console titles, the settings folder's names, and `MODERNJKGAME` (formerly `OPENJKGAME`), the folder the engine looks in for the modules; `src/codemp/qcommon/game_version.h`: `JK_VERSION`. The top-level `CMakeLists.txt` names the programs (`MPEngine`, `MPDed`) and the project; `src/codemp/cgame`, `src/codemp/game`, `src/codemp/ui` and the single-player `src/code/game` install the modules in `ModernJK`; the Windows resources (`src/codemp/win32/modernjk.rc`, `modernjkded.rc` and the modules') and the icons (`src/shared/icons/ModernJK.ico`, `modernjkcmd.ico`) are renamed, as are the installer's names (`cmake/Modules/InstallConfig.cmake`). The names in the build and the code that meant this repository are renamed too: `InModernJK`, `MODERNJK_VISIBILITY_FLAGS`, `ModernJKLibDir`, `modernjk_add_zlib_ng`, `netcodec_MODERNJK_DIR`, the single-player cgame calls `CG_MODERNJK_MENU_PAINT` and `CG_MODERNJK_GETMENU_BYNAME`, the build folders `build-modernjk/` and `build-netcodec/modernjk-clang`. Unchanged: the copyright notices, the comments that name OpenJK's own code or choices, and the names of the Jedi Outcast engine (`openjo_sp`, OpenJK's name for it).

</details>

### 30. Compatibility

| combination | works |
|---|---|
| Branch client with branch server | yes |
| Branch client with an OpenJK or base game server, or the reverse | no, and the connection is not refused: the protocol number is still 26, so the two sides connect and nothing they receive makes sense (decided at the end of the plan) |
| Branch build playing an OpenJK or base game demo | no |
| Demo of the branch played by OpenJK | no |
| Game and cgame modules built from OpenJK with the branch engine | not checked since the server ticks at 100 Hz (it worked before plan step 14). OpenJK's game code run at 100 Hz does not play as at 20 Hz (section 1). The server's game and the client's cgame must come from the same source, or the prediction misses |
| Mods that widened `user*` fields with the overrides files | they get the full values without the files (section 13) |
| Server browser, master server, `getstatus`, `getinfo` | yes, unchanged |
| Configuration files from OpenJK | yes, once copied under the new name (section 29); removed cvars are stored and never read |

### 31. C++20, 64-bit Windows, Linux and macOS

**Before.** OpenJK's C++ is built as C++11, and its build and code still carry 32-bit Windows and Linux, the BSDs, and branches for PowerPC, Alpha, Solaris, IRIX and Cygwin. A 32-bit Windows build made with MSVC played its sound through OpenAL and EAX (with the 32-bit `OpenAL32.dll` and `EaxMan.dll`, also copied into the 64-bit installations); every other build mixes its sound itself.

**Now.** Every C++ file is built as C++20, MSVC in its strict standard mode (the C files stay C until plan step 18). The build stops on a target that is not 64-bit or not Windows, Linux or macOS; Windows is x86_64 only, Linux and macOS x86_64 or arm64. The code of the other targets, the OpenAL and EAX path and their libraries are removed. Every warning of a clang build is fixed and every string literal is const (MSVC's strict mode refuses one given as `char *`); three bugs of the single-player game it showed are fixed as their code meant: an air kick no longer counts in the multikick, a fall kills only past the health (at least 20, five times for a howler), and the ragdoll leaves flat ground alone. The warnings of MSVC and GCC 12 are fixed the same way, and the bugs they showed with them: in multiplayer, a skin name sent by another player could write one byte past a buffer of the client, an effect file's value made only of spaces left a range unset, and the Galak mech could lob with an unset velocity; in single player, the seeker's spawn trace used a random box, the DEMP2's charged shot detonating in the air took a random direction for its effect, loading a save of the original game read past its data, and a long misc_model name overflowed its buffers. rend2, OpenJK's experimental renderer, is removed (a more recent one is to come).

**Why.** One language level and three systems in 64-bit (16 November 2026): the platforms the game is built and tested on, and a modern C++ for the work ahead (the C code in C++, plan step 18; a modern CMake project, step 19; the new platform layer, step 15).

**Gain.** No change in game: the 40 scripted scenarios give the same counts. Less code to read and keep (about 5,700 lines of sound code for a path no 64-bit build compiled). A bug fixed: the `com_affinity` setting on Windows x64 filled only half of the processor mask and left the rest uninitialized. The cost: no 32-bit build.

<details>
<summary>Technical details</summary>

Top-level `CMakeLists.txt`: `CMAKE_CXX_STANDARD` 20, required, without extensions (MSVC's `/std:c++20` implies `/permissive-`), the checks of the pointer size, the system and the processor, the x86 flags removed (`/arch:SSE2`, `-msse2`, `-mstackrealign`, `-mfpmath=sse`: none changed the x64 code). `src/shared/qcommon/q_platform.h` refuses another processor on Windows and another system. Removed: `id386`, `idppc` and their code, the `USE_OPENAL` blocks of `snd_dma.cpp`, `snd_mem.cpp`, `snd_local.h` and `sdl_sound.cpp`, `client/OpenAL`, `client/eax`, `lib/OpenAL32.lib`, `lib/ALut.lib`, the x86 binaries of `libs/SDL2`, the i686 toolchain files, the presets `msvc-x86`, `mingw-clang-x86` and `gcc-x86`, and `netcodec_pmove_101` (the original 1.01 movement code in 32-bit x87; its results: [progress/step-06-cpp-codec.md](progress/step-06-cpp-codec.md)). `codemp/rd-rend2` and the option `BuildMPRend2` removed. `CMAKE_CXX_SCAN_FOR_MODULES` off (the project has no C++20 module; MSBuild's scan files of two sources named `files.cpp` collided). The warnings and how each was checked: [progress/step-17-cxx20-64bit.md](progress/step-17-cxx20-64bit.md).

</details>

### 32. Not changed yet

| subject | where |
|---|---|
| Lag compensation: none, as in OpenJK (planned for the hitscan shots, dropped on 14 November 2026) | plan step 14, part 8 |
| The freeze when the scoreboard first shows players whose models are not loaded: shorter (20 ms per model instead of 71, section 25), but the models waiting are still loaded in one frame, their images read and decoded while the game waits | not planned (14 November 2026: enough for now) |
| A long client hitch: beyond 1.28 s the oldest snapshots are skipped (1.6 s in the base game) | tested, left as it is (plan step 14, part 11) |
| Priority and culling per client: no priority; the culling against wallhacks prototyped | plan step 16, set aside (15 November 2026) |
| The delay the other players are drawn with depends on your frame rate: about 2 ms at 100 frames per second, 7 at 125, 11 at 333 | kept as it is (15 November 2026): extrapolating would make the delay equal but guess the positions and the angles; a fixed delay would add a few milliseconds at 100 and 125 frames per second |
| Encryption, or a replacement for the removed obfuscation; several download blocks per message | plan step 12 |
| Tables retrained on games of this build; the force power levels the prediction lacks; a wider `saberAttackChainCount` | plan step 13 |
| The client's window and input rewritten on SDL 3 | plan step 15 |
| Protocol number, handshake, support of the old protocol, old demos | end of the plan |
| An acknowledgement bitfield, a snapshot history sized from the round trip | not planned (reasons in [progress/step-10-acks-reliable.md](progress/step-10-acks-reliable.md)) |

## F. The sound

### 33. The sound on OpenAL Soft

**Before.** OpenJK plays the sound with the base game's software mixer, one copy in each engine: 32 channels shared by every sound, the looping ones included; a new sound takes a free channel or the oldest one, whatever its loudness or distance; the looping sounds are added again at every frame and those of one file are merged into one channel; the mixer paints 150 ms ahead in 16-bit integers with hard clipping, takes the nearest sample when it resamples and pans only left and right; SDL 2 plays the result. MP3 files are decoded by FreeAmp's decoder of 1995. Settings choose the mixing rate (`s_khz`, the menu's "sound quality") and EAX, which OpenJK cannot play.

**Now.** One sound module for the multiplayer and both single-player games (`src/shared/sound`), on OpenAL Soft 1.25.2:

- every sound is played with a class (voice, weapon, saber, force, vehicle, world, body, ambient, interface, announcer) that gives its distances, its volume (voices or effects), its priority and its own gain (the sabers' sounds 2 dB down, tuned by ear), and, where one sound must replace another, a slot of its entity (a new line cuts the character's last one, a shot cuts the weapon's charge);
- at every frame the 64 sounds loudest where you stand are mixed, four at most of one sound and its variants (a near sound before a far new one, a fast weapon's newest shots), the looping sounds of one file one sound as before (the torches of a room one fire, not echoes of one another, a crowd's hums of one saber one hum), their sum capped as before at full volume on each ear in front, whatever the volume setting; the others are followed silently and heard again from where they have got to when they become loud enough;
- the looping sounds are started once, moved and stopped;
- real Doppler for every moving sound; under water, every sound muffled;
- the loudness after the standard for games at home, ASWG-R001 (Sony's Audio Standards Working Group, recommended by Microsoft and Nintendo too through the Game Audio Network Guild): -24 ±2 LUFS of integrated loudness (ITU-R BS.1770) at the default settings, measured on duels at -23.2 LUFS; the peaks held 2 dB under full scale by OpenAL Soft's limiter, so that their true peak stays under the standard's -1 dBTP (-1.7 dBTP at worst, a crowd with every slider at 1); the volume settings moving the loudness evenly;
- the system's default device followed (headphones plugged in, a device removed); the sound muted, not paused, while the window has not the focus (`s_muteWhenUnfocused`);
- the files decoded by the reference decoders: dr_mp3, dr_wav, libFLAC and stb_vorbis; a sound's name is looked for as `.flac`, `.wav`, `.ogg`, then `.mp3`; every sound decoded when it is registered;
- the music and the single player's dynamic music streamed, the cinematics' sound on a stream of their own;
- the video recorder removed;
- the "sound quality" and "EAX" items removed from both games' menus (a `modernjk.pk3` for each game).

**Why.** The defects below, each shown in game on the old mixer by a sound log first; one code for the three games instead of two copies; a library that mixes on its own thread in floating point, with a good resampler, and that follows the system's devices.

**Gain.**

| fix | before | now |
|---|---|---|
| A crowded fight | the 32 channels full: a new sound cut the oldest still playing, a near hit as well as a far footstep (crowd: 4,932 sounds cut in 90 s) | 64 sounds mixed, the quietest followed silently; none cut (crowd: the 64 full at 57 % of the frames, up to 147 more followed) |
| A sound that starts out of range | stopped for good at its first silent frame, even if its source comes closer (free play: 812 in 90 s) | followed silently, heard when it comes in range (none lost) |
| Doppler | on the multiplayer's missiles only, with a wrong formula: your own rocket started 2.76 times higher (about 18 semitones) for 70 ms, a missile passing close chirped as it left | real Doppler for every moving sound, the speed of sound about 12,000 units per second: a rocket about one semitone lower going away, higher coming |
| The rocket launcher's lock | its ticks and tone played at the target, at 0.41 to 0.48 of their volume 900 units away, silent beyond 1,506 | at your ears, as a cue of the screen |
| "No force" (multiplayer) | played at the first player's position (entity 0), faint or silent for everyone else | at your ears |
| Shots (multiplayer) | every shot played twice, by the weapon's sound and by its muzzle effect, 6 dB louder than in the single player; an alternate shot played the regular shot's sound beside its own | once, the muzzle effect's, as in the single player, played as the shooter's in the three games: your own at your ears, the others' from the shooter, every shot 2 dB above the sound as recorded; the alternate shots their alternate effect's own sound; your own shot heard with the gun hidden or zoomed |
| The rocket's explosion | two copies of one sound at the same instant (its effect asks for two) | one sound at twice the volume: as loud, one voice |
| Sabers hitting walls | multiplayer: every hit on a wall anywhere in the level cut the one playing elsewhere (crowd: 2,527 cut); a blade scraping a wall asked for a sound every 100 ms, each cut by the next 100 ms after its start; single player: one per character, from the character, cut by its next, for one blade drawn in N | in the three games each blade's own sounds, from the point it touches: a blade scraping a wall plays a sound every 250 ms, four at once at most, each whole, those after the first 3 dB down (a 3 s scrape: twelve); a new strike after the blade left the wall heard at once; never two of one blade within 100 ms; four at once at most of the level's hits on walls |
| A saber turning on (multiplayer) | a single saber's ignition played with a second sound, the default of the absent second saber (`enemy_saber_on`), by the client and the server; a player appearing heard turning its saber on twice, 7 ms apart (the cgame's refreshes of the weapon's model taken for weapon changes), and every player seen for the first time heard turning it on (fifteen bots arriving: 60 ignitions) | once per real change, in the client and the server: a switch to the saber, a spawn, a respawn; a removed saber silent; none for a player seen for the first time (fifteen bots arriving: 25) |
| Debris | each chunk of a breaking object cut the previous one | each chunk its own sound |
| The ambient sets' calls (Yavin's creatures) | the delay before the next call drawn again at every frame: nearly always the shortest, about every 10 s where the set asks for 10 to 50 | the delay drawn once, between the set's two times |
| The MP3 files | the old decoder inverted the polarity, decoded 50 files wrongly and cut the end of 3,054 sounds | the reference's samples; the sounds start 26 to 38 ms earlier (the encoder's header skipped) |
| The mixing | nearest-sample resampling, 16-bit integers clipped, left and right only | OpenAL Soft: the bsinc24 resampler, floating point, 3D panning |
| A cinematic's mono sound | queued as mono though decoded as stereo pairs: its first half at half speed (read in the code) | queued as stereo |
| Under water | nothing (only the EAX path, which OpenJK cannot play, muffled the sound) | every sound muffled, -30 dB at 5 kHz |
| The volume settings | `s_volume`, `s_volumeVoice` and `s_musicvolume` a straight gain: the settings heard crowded near 0 (0.05 is -26 dB), most of each slider useless | their cube, the slider moving the loudness evenly over its whole course: 0.5 is -18 dB, 1 the sounds as recorded; the defaults the same levels as before (`s_volume` 0.79, `s_musicvolume` 0.63, `s_volumeVoice` 1); a configuration set on the old curve to be set again once (with the mix's margin below, 0.048 is now about 0.51) |
| The loudness | the game mixed far louder than recommended: a duel at the default settings -15 LUFS, a crowd's peaks 15 dB above full scale at full volume, clipped by the device | the whole mix 9 dB down: a duel at the default settings -23 LUFS (ASWG-R001's -24 ±2 LUFS for games at home), OpenAL Soft's limiter holding a crowd's peaks 2 dB under full scale (sample peaks, 1 ms of look-ahead) instead of clipping, their true peak under ASWG-R001's -1 dBTP (+0.4 dBTP with the library's threshold at full scale) |
| The window loses the focus | the device paused, the sound's clock frozen | muted, the sound going on (`s_muteWhenUnfocused 0` keeps it) |
| Another sound device | the device chosen at the start kept | the system's default device followed |

Costs: a level's sounds take 36 to 44 MB as decoded samples (measured on `mp/ffa3` and `yavin1`), where the old zone held 15 to 31 MB with the voices kept as MP3; the video recorder is gone (a program made for it records the game); the single player's saved games of OpenJK are refused (their version raised: a character's voice is written as its state, its saber's last hit on a wall no longer written). The mixing's CPU and latency are measured with the reverb in plan step 21, part 5.

<details>
<summary>Technical details</summary>

| | OpenJK | this branch |
|---|---|---|
| Code | `client/snd_*.cpp`, `snd_mp3.cpp` or `cl_mp3.cpp`, `mp3code/` in each engine; `sdl/sdl_sound.cpp` | `src/shared/sound` (a static library, its tests `modernjk.sound` and three fuzzers), `engine/` compiled by each engine |
| The modules' access | the multiplayer's import tables (`S_*`, `AS_*`); the single player's numbered system calls; `gi.VoiceVolume` | one `sound::Api*` in every import table (`trap->sound`, `gi.sound`) |
| The cgames' sound | each cgame calling the engine's sound itself (`trap->S_*`, `cgi_S_*`) | one layer for the three cgames, `src/shared/cgame/cg_sound.cpp`, compiled by each against its own headers; each tree's own part in its `cgame/cg_sound.cpp` (the engine's sound, an entity's velocity, an NPC's type) |
| A sound sent by the server (multiplayer) | its channel in the event | its class, slot and loop flag packed in the same field (`BG_PackSoundEvent`, `src/shared/qcommon/q_sound.h`) |
| The data's channels (animation events, scripts, effects) | played on that channel | turned into a class and a slot (`BG_SoundUseOfChannel`) |
| Lip sync | an integer: -1, 0, 1 to 4 | an explicit state (not talking, silent, levels 1 to 4) |
| Voices | positioned, 768 to 2,018 units; quiet ones 346 to 596; the howler's and the sand creature's as normal voices | the same distances; the howler's and the sand creature's as quiet voices, as the EAX path played them |
| `CHAN_LESS_ATTEN` | 2,048 units in the multiplayer, 1,280 in the single player | the vehicle class, 2,048 to 3,298 everywhere |
| The listener's own sounds | not positioned, at 0.75 | the same |
| Latency | the mixer painting 150 ms ahead (about 70 ms estimated) | OpenAL Soft's default, 3 periods of 512 frames (about 32 ms reported on Windows) |

The work, its decisions, the inventory of every call and the sound logs: [progress/step-21-sound.md](progress/step-21-sound.md).

</details>

### 34. The reverb by zones

**Before.** OpenJK has no reverb. The base game had one, by zones (the files `eagle/<map>.eal`, made with Creative's editor), but only through its 32-bit OpenAL path with Creative's EAX Manager, on a sound card with EAX and with `s_UseOpenAL 1` (off by default in the multiplayer and in Jedi Outcast); OpenJK removed that path (section 31), so every room sounds dry, and the walls and pillars the files describe muffle nothing.

**Now.** Each map's reverb by zones, as the original's EAX path played it, through OpenAL Soft's EFX, in the multiplayer and both single-player games:

- the game's zone files converted once (`tools/eal_converter`), 57 for Jedi Academy and 26 for Jedi Outcast's single player, in each game's `modernjk.pk3` (`eagle/<map>.efxzones`); every map both games play has one;
- the reverb of the room you stand in and, on 37 of Jedi Academy's maps (15 of its 23 multiplayer maps), the reverbs of the two rooms whose door is nearest, each heard from its door as you turn; every sound sent to the reverb of its own room;
- on the other maps (Jedi Outcast's, most of the duels, the academy) the reverb of the room you stand in, turning into the next room's in one second;
- the walls between rooms and the pillars inside one muffle the sounds behind them, as the files say; the characters' voices never, as in the original;
- under water, the "underwater" reverb beside the muffling of section 33;
- `s_reverb` ("Reverb:" in the sound menus of the three games, on by default) switches the reverb off; the walls, the pillars and the water muffle all the same, for every player.

**Why.** The game's sound designers made a reverb for every map, which no one has heard since sound cards with EAX are gone (hardware EAX ended with Windows Vista), and never in a 64-bit build or on Linux.

**Gain.**

| fix | before | now |
|---|---|---|
| A room's sound | dry everywhere | the room's reverb: a hall's long tail, a corridor's short one, a cave's echo |
| The rooms around | nothing | on `mp/ffa3` in the hall of the TIE fighter, the lunch room heard through its door and the north hall to the side; their reverbs panned towards their doors in the listener's whole orientation (the original's pan took the view's pitch for a turn: looking up 60 degrees moved a door ahead to the side, looking straight up put every door in one direction of the world) |
| `mp/siege_korriban` | (the original) its file refused for one room without its first aperture, played with one reverb | its neighbours, as on the other multiplayer maps |
| A sound behind a wall | as loud as in the open | muffled as the file says: behind the glass walls of Jedi Outcast's `doom_comm`, at 0.58 of its level and its high frequencies 14 dB lower; Jedi Academy's only wall, in `t1_danger`, -1,507 mB |
| A sound behind a pillar | as loud | muffled by up to 60 dB at the high frequencies and 15 dB at the low ones, by the angle it turns around the pillar's edge (`mp/siege_desert`'s 73 pillars, `mp/ffa3`'s 5); also behind the 69 pillars whose zone the editor's renumbering had made wrong, which the original never applied (`t2_rancor`'s 12, `yavin_temple`'s 31, 14 of `mp/siege_hoth`'s) |
| The 64 sounds mixed in a crowd | chosen by distance and volume | a sound behind a wall counts as the quieter sound it is (as FMOD does) |
| Under water | the muffling of section 33 | and the "underwater" reverb |

Costs: three reverbs take about 0.74 % of a core (measured on 15 November 2026 with 32 voices), the zones' calculations an estimated 0.01 ms per frame (measured since: about 1.5 % of a core for the mixing with 64 voices, the reverb 0.9 % of it, the zones 6 to 7 us per frame; section 35 and [progress/step-21-sound.md](progress/step-21-sound.md) section 14.4); the zone files 1.1 MB of text in the two archives. The loudness measured again after ASWG-R001 (section 33): on `mp/duel1`, whose one reverb is a courtyard's, the duels at the default settings at -22.8 LUFS on average with the reverb and -23.3 without (the bots' runs differ by up to 3 dB), a crowd with every slider at 1 at -1.7 dBTP at worst; still within -24 ±2 LUFS and under -1 dBTP. A map with long reverbs measured since (section 35): the TIE fighter's hall of `mp/ffa3`, within the range.

<details>
<summary>Technical details</summary>

| | the original's EAX path | this branch |
|---|---|---|
| The data | `eagle/<map>.eal`, read by Creative's `EaxMan.dll` 3.0 (closed, 32-bit) | `eagle/<map>.efxzones`, a text file the module reads (`src/shared/sound/zones/zones.cpp`): the environments in the values of EFX's "EAX reverb", the zones' BSP, the apertures, the obstacles, the diffraction boxes; converted by `tools/eal_converter` through EalMan (`libs/ealman`, a portable rewrite of `EaxMan.dll`, which the game never links), every file checked against EalMan to the bit |
| The listener's zone | every 100 ms | every frame; out of every zone, the last zone's reverbs |
| The neighbours | the two environments whose nearest aperture is nearest, every 100 ms | the same, every 100 ms and when the listener enters another environment |
| A sound's reverb | its environment's slot when one holds it, else left as it was (EAX 4's default: the listener's) | its zone's slot, else the listener's; none for the announcer, the interface and the voices heard everywhere |
| Occlusion, obstruction | EAX 4's source properties from `GetSourceDynamicAttributes`, the voices left out | the same values, computed by the module, turned into EFX low-pass filters on the direct sound and the send as OpenAL Soft's own EAX layer turns them; the exclusion not played |
| One reverb | Jedi Academy: switched at once; Jedi Outcast: morphed in 1 s by ten steps | both: ten steps of 100 ms, the original's interpolation (`EAX3ListenerInterpolate`) on EFX's values |
| Under water | the "underwater" environment in slot 0, every source sent to it with -3000 mB of occlusion | the same, `EFX_REVERB_PRESET_UNDERWATER` |
| A file the original refused | one reverb | `mp/siege_korriban` repaired (one room lacking its first aperture); `t3_byss` one reverb |
| A zone in the file's default environment (five of Jedi Outcast's files) | that environment | the same, the converter adding it to the file |
| A diffraction box naming another zone than the one holding it (69 boxes) | never applied | given the zone that holds its centre by the converter |
| `s_UseOpenAL` | the whole EAX path, off by default in the multiplayer and Jedi Outcast | removed (section 33); `s_reverb`, the reverb alone, on by default |

`soundinfo` prints the map's zones, the listener's environment and the slots; `s_show 1` adds them to the sound log every 50 frames, with each voice's send and direct filter. The work, the data counted and the original's code read: [progress/step-21-sound.md](progress/step-21-sound.md) section 13.

</details>

### 35. The sound measured, and what the measures changed

**Before.** The new sound of sections 33 and 34 was heard but not measured: its CPU, its memory, its delay, its loudness on a map with long reverbs. The client drew the screen and swapped it before it ran the sound's frame, so a sound the game asks while drawing (a shot, a hit) waited for the swap. The music was decoded in large blocks; the mute without the focus could be changed only in the console; the project's archive was `modernjk.pk3`.

**Now.**

- the sound's frame runs between the screen's drawing and its swap, in the multiplayer and both single-player engines;
- the music decoded in small blocks, a little at every frame;
- the module's own work per frame made smaller, the same results;
- "Mute when unfocused" (`s_muteWhenUnfocused`, on by default) in the sound menus of the three games, under "Reverb", in each game's languages;
- the archive named `z_ModernJK.pk3`, so that it is read after nearly every archive a player adds to `base` (the archives of a folder are read in their names' order, the last one's files winning); the install scripts delete the old `modernjk.pk3`.

**Why.** A sound should start when the game asks it, at every frame rate, with or without vsync (GUIDELINES section 1, "For every player"); the sound's frame should not make the slowest frames; a setting players may want to change should be in the menus.

**Gain.**

| fix | before | now |
|---|---|---|
| A sound asked while the screen is drawn | waits for the swap: with vsync up to one frame (2.7 ms measured at 359 Hz; 6.9 ms at 144 Hz, 10 ms at 100 Hz), 0.9 ms without vsync at 125 frames per second | sent before the swap: 0.05 ms with vsync, 0.08 ms without |
| The music's decoding | 8,192 frames at once every 186 ms, about 0.1 ms on the main thread: the slowest frames of a level (99th percentile 118 us against 33 us with the music stopped) | 1,024 frames at a time, about one MP3 frame per frame; the same 0.74 to 1.5 s queued |
| The module's work per frame | a voice's zone found twice, two tables as large as the sounds and the events cleared at every frame, every looping voice's state asked | each once; on one demo played by both builds, 7 to 8 % less (21.7 to 20.2 us per frame at 125 frames per second, 14.4 to 13.3 us at 333) |
| The mute without the focus | the console only | its item in the sound menus |
| The project's archive | `modernjk.pk3`, before any archive named after it (`mp_*`, `pak*`, `zz*`) | `z_ModernJK.pk3`, after nearly all of them |

Measured on the development machine (Windows 11), the old mixer of section 33 against this branch:

| measure | the old mixer | this branch |
|---|---|---|
| The main thread's time in the sound per frame, fifteen bots on `mp/ffa3` | 57 to 104 us (0.80 % of a core at 100 frames per second, 1.91 % at 333) | 16 to 22 us (0.22 % to 0.54 %); OpenAL Soft's mixing thread about 1.5 % of a core more, the reverb 0.9 % of it |
| The memory | 945.8 MB private | 956.9 MB (within one sample's spread) |
| A sound asked to its onset in the Windows mix | 110 to 116 ms, 30 to 45 ms between the earliest and the latest | about 50 ms at 100, 125, 333 frames per second and with vsync, within about 10 ms |
| The loudness of a long reverb (the TIE fighter's hall of `mp/ffa3`, 3.1 s), ASWG-R001's -24 ±2 LUFS and -1 dBTP | | a crowd -21.8 to -24.4 LUFS with the reverb, -23.7 to -23.9 without; the duels that fight -24.7 to -25.4 LUFS; every true peak at -5.0 dBTP or under |

Costs: none measured for the swap's order (the same work at another moment); the music's 32 buffers hold the same frames as the 4 did.

<details>
<summary>Technical details</summary>

| | before | now |
|---|---|---|
| The sound's frame | `SCR_UpdateScreen()` (drawing, then `re->EndFrame`, the swap), then `SND_Frame` | `SCR_UpdateScreen(CL_UpdateSound)`: the function run after the drawing, before `re->EndFrame`; the module tables and the other callers keep `SCR_UpdateScreen()` |
| The music's stream | 4 buffers of 8,192 frames (`src/shared/sound/stream/stream.cpp`) | 32 buffers of 1,024 frames |
| A voice's zone | `ZoneOf`, then `ObstaclesOf` walking the BSP again for the same point | the zone found given to `ObstaclesOf` (an overload; the converter's check against EalMan goes through the former) |
| The loops' groups, the events' counts | `assign` over every registered sound and every event at every frame | only the entries the frame used cleared |
| A looping voice | its state asked of OpenAL Soft at every frame | not asked: a looping source stops only when the module stops it or the device is lost |
| OpenAL Soft's buffer on Windows | 32 ms | unchanged: a buffer sized by Windows from its period, as Microsoft documents it (22 ms, about 10 ms less delay), was measured without an underrun on the development machine and not kept, its margin smaller on machines and drivers not measured |
| The loudness's method | `session.sh`'s waits half their intended length (`wait` counts two per frame): about 29 s measured after 10 s of settling, the recording from the device's opening | 60 s after 20 s; `loudness.py --from` starts the measure at the second the session prints (read in the console's log) |

The measures and the tools: [progress/step-21-sound.md](progress/step-21-sound.md) section 14.

</details>

### 36. The sound device chosen in the menus

**Before.** The sound played on the system's default output only: to hear the game in a headset while the system's default stayed on the speakers, a player had to change the default for every application.

**Now.** "Sound device", the first row of the sound menus of the three games: "Default" (the system's default output, followed when it changes, as before) or one of the system's outputs, a click or an arrow moving to the next one; the console's `s_device` (the output's name, empty for the default), kept in the configuration. The chosen output unplugged, the sound goes to the default, and comes back to the chosen one when it is plugged in again; `soundinfo` lists the outputs, the one in use marked. On Windows and Linux alike (WASAPI, PipeWire, PulseAudio; ALSA, which reports no output plugged in, finds the chosen one again at the next start).

**Why.** A player may want the game in a headset while other applications stay on the speakers, and the menus already hold the other sound settings.

**Gain.**

| fix | before | now |
|---|---|---|
| The output the game plays on | the system's default, always | the default, or the output chosen in the menu, changed at once without a restart |
| The chosen output unplugged during a game | (no choice) | the sound on the default meanwhile, back on the chosen output when it is plugged in again |

Costs: one integer compared per frame (the setting's modification count); the outputs listed only when the menu's row is clicked, their names kept until the next click (a few hundred bytes); no change to the CPU of the mixing or to the latency.

<details>
<summary>Technical details</summary>

| | before | now |
|---|---|---|
| The device opened | `alcOpenDevice(nullptr)`, the default | the chosen name, else the default |
| The events | the default changed | the default changed, an output added (`ALC_EVENT_TYPE_DEVICE_ADDED_SOFT`) |
| The move at a frame | reopen on the default when it changed or the device was lost | `DecideMove`: the chosen output when it is there, else the default; the outputs listed only when the move depends on them |
| The names | OpenAL Soft's ("OpenAL Soft on Haut-parleurs (Realtek(R) Audio)" on Windows) | the system's ("Haut-parleurs (Realtek(R) Audio)"), which OpenAL Soft opens as well |
| The interfaces | | `sound::Api::OutputDevices`, which both interfaces already reach; an owner-drawn row (`UI_SOUND_DEVICE`) cutting the name to the width its menu gives it (`special`) |
| The menus | effects, music, voices, reverb, mute without the focus | the sound device first, the rest one row down |

The work: [progress/step-21-sound.md](progress/step-21-sound.md) section 15.

</details>

## G. The three games alike

### 37. One seed for every random number stream

**Before.** A run could be replayed only in the multiplayer, and only its game: `g_debugSeed` fixed the game module's seed, while the cgame, the menus, the engine and the renderer's weather took the clock's or the system's random values, so a demo played twice picked other variants of its sounds (footsteps, saber hits) and other effects. The single-player games of Jedi Academy and Jedi Outcast had no fixed seed at all: two sessions never played the same game.

**Now.** `com_debugSeed`, a setting of the engine, replaces `g_debugSeed`. Set before the level loads, with `fixedtime`, it seeds every random number stream of the three games at each level's start: the game, the cgame, the menus, the engine (the multiplayer's NPC navigation and client effects, the sound's ambient sets and music) and the renderer's weather, each stream with its own sequence drawn from the one seed, the game's the seed itself as before. Not set, every stream takes the clock's or the system's values, as before.

**Why.** Two sessions of one build must be comparable line for line: the sound's checks of the single player could compare only the kinds of sounds two sessions played, and those of the multiplayer had to set the variants aside. And the same system of random numbers in the three games.

**Gain.**

| fix | before | now |
|---|---|---|
| A single-player session started again with the same seed and fixed frame time | other outcomes and other sounds at every launch | the same: 160 of 160 sounds started in Jedi Academy (`yavin1b`), 70 of 70 in Jedi Outcast (`artus_mine`), 194 of 194 on `t2_trip`, the wind's one-shots at the same moments |
| A multiplayer demo played twice with the same seed | the same sounds with other variants, from the cgame's clock seed | the same: 275 of 275 sounds started |
| The seed's reach | the multiplayer's game module | every stream of the three games, each its own sequence |
| The single player's game seed | the frame's start time; the level's global time given the clock's milliseconds instead (the two swapped, both clock values, so nothing visible) | the seed, or the clock's when none is set; the global time in its own place |
| The single player's connection port | the clock's milliseconds | the system's random bytes, as in the multiplayer (no effect on the loopback) |
| A single-player level loaded by `devmap` with `fixedtime` set before | every cheat refused (`god`, `give`, `npc spawn`, `s_show`), `fixedtime` and the seed back to 0: the session on the real clock | the cheats allowed, `fixedtime` and the seed kept, as without `fixedtime` |

Costs: none. Each stream is seeded once per level, as before; no memory, no bytes on the network, no delay.

<details>
<summary>Technical details</summary>

| | before | now |
|---|---|---|
| The setting | `g_debugSeed`, the multiplayer game module's, read at `G_InitGame` | `com_debugSeed`, both engines', cheat-protected as `fixedtime`; the game's seed chosen by the engine (`SV_InitGame`, `sv_game.cpp`), the cgame, the UI and the renderers reading it themselves |
| The streams from one seed | | `Rand_InitFixed( seed, randStream_t )` in `src/shared/qcommon/q_rand.h`: SplitMix64 seeding of the stream's number above the seed's 32 bits; `RAND_GAME`, number 0, the same sequence as `Rand_Init( seed )`, so a scenario's game replays as before |
| The engine's stream | the system's random bytes, once at the start | the same, then from `com_debugSeed` at each level's start when it is set (`Sys_InitRandForLevel`: the multiplayer server's `SV_InitGame`, every client's `CL_InitCGame`), since the scripts set it in a cfg run after the engine's start |
| The single player's `ge->Init` | `com_frameTime` in the random seed's place, `Com_Milliseconds()` in the global time's | the seed, then `com_frameTime` as the global time, in the order of `InitGame`'s parameters |
| The single player's game and cgame | one module, one stream | the same (decided 25 November 2026: one stream per module, as in the multiplayer) |
| `Com_RandomBytes` | the multiplayer's engine only | `src/shared/sys/sys_main.cpp`, one definition for both engines |
| The single player's systeminfo | written in a frame where a server frame runs only; with `fixedtime 8` the first frame after `devmap` (8 ms, less than the server's 50) returned first, so the client, which shares the server's cvars, took back the spawn's `helpUsObi` 0 and reset the cheat-protected cvars | written before `SV_Frame` waits for its next frame, as the multiplayer's (`code/server/sv_main.cpp`) |
| Tests | | `modernjk.rand` (`test/qcommon/q_rand.cpp`): each stream's first values against the published algorithms computed in Python, the game's stream the same as `Rand_Init`'s, the streams different from each other, a stream seeded again repeating itself |
| `Rand_Init`, `Rand_InitFixed` | inlined where the compiler chose | never inlined: MSVC 19.52 at `/O2`, inlining the seeding into a loop over the seed, used the counter sign-extended for `(uint32_t)seed`, so that a negative seed gave every stream the same numbers (found by the test; no caller of the game in such a loop); the seed now crosses the call as 32 bits |
| The scripts | `set g_debugSeed` | `set com_debugSeed` (`tools/gamestats/gamestats_cfg.sh`, `tools/soundlevel/session.sh`, `tools/visibility/coverrun.sh`) |

A single-player session sets the seed and `fixedtime` on the command line (`+set`): a `set` of a cheat-protected cvar is refused before `devmap`.

The work: [progress/step-30-random-seed.md](progress/step-30-random-seed.md).

</details>

## H. The sound audited

### 38. The sound audited

**Before.** An audit of the sound (25 November 2026, by three agents reading the module, the engines' glue, the data paths, the reverb and the cgames), each finding checked again in the code and in the game's data, found defects of three kinds: what a player heard wrongly (walls muffling twice, a zone frozen with the reverb off, silent or wrong sounds on three single-player levels, the lips behind the voices in slow motion), what failed to recover (no sound for a game started without a device, a sound restart that failed losing every sound's handle), and work done for nothing (the music read twice at each level, the listener sent to OpenAL Soft at every frame).

**Now.** Each fixed, with a test that fails before the fix and passes after it where a test can show it.

**Why.** The sound the players hear must be the same for all, the original game's where it was right, and must come back when the device does.

**Gain.**

| fix | before | now |
|---|---|---|
| Loops of one sound behind a wall (two torches, sabers' hums), merged into one voice | the wall's attenuation counted twice, and the merged voice filtered by the wall of whichever copy held the source (a far player's hum behind a wall muffled the near one) | the wall counted once per copy, the merged voice filtered for its high frequencies and sent to the reverb as its loudest copy |
| `s_reverb 0` | the listener's zone no longer followed, while the walls' and pillars' filters used it: on `mp/siege_desert` none of the 73 pillars' boxes muffled anything for that player, who heard the enemies behind them louder than the others | the zone followed with the reverb off, the walls and pillars the same for every player |
| A set's name given twice in `sound/sound.txt` | the first definition used: Jedi Outcast's `artus_detention` fan and two vents silent, `cairn_reactor`'s 16 beams and Jedi Academy's `vjun2` beam with another beam's sounds and no stop sound | the original game's choice: the brush model's set over a local one, the later of two of one kind |
| A set without some keywords (a mod's) | silent, or a one-shot at every frame | the original game's defaults (10 to 25 s, a volume of 255, a radius of 250) |
| A general set fading in | its first one-shot at a fraction of its volume, then its delay of 10 to 50 s spent | its first one-shot once the fade has ended |
| The lips of a character speaking in Force Speed or a script's slow motion | following the frame's clock, which follows `timescale`: behind the voice | following the voice as it plays |
| A game started without a sound device | no sound until `snd_restart`, even once a headset was plugged in | the device tried again every second, quietly; the sounds the game registered meanwhile keep their handles and play |
| `snd_restart` failing (no device), then again with one | every handle the modules held pointing to nothing or to another sound until the next level | the sounds kept and given back; only the sounds that were loaded loaded again, the freed ones with their length |
| A dynamic music missing one of its transition pieces (a mod's) | no music for the whole level | played, warned, a missing transition going straight to the next state's piece |
| A level's dynamic music | read twice at its start (the preload and the first snapshot), and after each cinematic | read once per level |
| `s_allowDynamicMusic` changed | at the next music | at once |
| The multiplayer's music under a short tune (becoming the Jedi Master) | at full volume: `s_musicMult`, which the cgame sets, read by nothing | lowered to 0.3 for 5 s, as the cgame asks |
| A volume setting above 1 (the console) | the mix pushed into the limiter, which raises the quiet sounds against the loud ones for that player | at most 1 |
| `EV_MUTE_SOUND` and the sound trackers | an entity's number from the network used unchecked: a hostile server or demo could write outside the entities | a number outside the entities ignored |
| A disconnection in the multiplayer (`connect` in a game) | the level's loops and the music's duck playing on over the connection; the reverb computed in the menus (both games) | stopped; the zones left |
| A thrown saber returning, a fading corpse's hum | the hum 1.5 dB above a held saber's; the corpse's restarted at every frame for 2.2 s | at a held saber's level; the corpse's a loop |
| The single players' own saber hum in third person | on the body, muffled by a pillar or a zone between the body and the camera | at the camera while the view is the player's, as the multiplayer's |
| The listener's velocity during a death fall's camera (multiplayer), a cinematic or another entity's view (single players) | the player's: a Doppler shift on every sound while the camera stood still | none |
| The single players' jump grunt | a sound of the world, under `s_volume` and muffled by walls | a voice, under `s_volumeVoice`, never muffled, without the voice's slot so that it cuts no line (the original game played it on `CHAN_AUTO`) |
| A sound started twice in one frame in a slot | one voice at twice the gain | the second replaces the first, as a channel did |
| The zone files' matrix of obstacles | written in every file, the default obstacle at 0 mB counted as one: every sound of 65 maps looked up, at every frame, an obstacle that changed nothing | written only when one of its obstacles acts; the 83 files converted again, each checked against EalMan |
| Smaller defects | a `sound.txt` with lines ended by a carriage return alone read as one line; a WAV without its frame count said to have none; a level's name with a folder not found in `dms.dat`; the silence asked by `s_dynamic` kept on every later music; `dms.dat`'s problems shown to developers only; a hand-made zone file with a density of 0 giving values that are not numbers, or boxes out of order ignored; the reverb chosen from the map before's place until the game gave the new one; a sound at the listener with an entity's slot cutting that slot without taking it | each fixed |

Costs: none to measure. The listener and the streams' gains are now sent only when they change (OpenAL Soft computes every voice again at each change of its listener); a mono sound is moved into its buffer, not copied; the last level's dynamic music is kept in memory until the next level (its files, as the old mixer kept them).

<details>
<summary>Technical details</summary>

| | before | now |
|---|---|---|
| The merged loops (`voices/voices.cpp`) | the player copy's `VoiceZone` on the merged source | `mergedZone`: the loudest copy's, its broadband gain 1 |
| The reverb's frame (`zones/reverb.cpp`) | returned before the listener's zone with the reverb off | the zone first; nothing after a load until `SetListener` (`listenerKnown_`); a logarithmic fade from or to 0 done linearly; boxes sorted by zone at parse (`zones.cpp`) |
| The ambient sets (`ambient/`) | the first set of a name; defaults 0; lines split on `\n` | duplicates kept as the original's `CSetGroup` map did; its defaults; `\r` and `\n`; a general set's one-shots after its fade in |
| The bank (`bank/`) | `Names()`, every name loaded again at a restart | `Save()`: name, loaded, length; `Restore` loads only those that were |
| The glue (`engine/engine.cpp`) | `SND_Restart` with a local state; no retry; `Silent::Register` gave none | `kept` and its handles, the silent sound registering into it; `SND_Frame` tries a failed start every second, its messages printed only when it opens; volumes at most 1; `s_musicMult` |
| The music (`stream/music.cpp`) | every track required; `Stop` freed the level's; `pendingStart_` | explore and action required; the last level's kept (`ForgetLoaded` at a level's registration); `pendingStart_` removed (unreachable) |
| The cgames | `EV_MUTE_SOUND` unchecked, the corpse's one-shot, the hums' volumes and places, the listener's velocity, the jump grunt, the effect sound's rule written three times | checked; a loop; `kSaberHumVolume`, `CG_SaberHumOrigin`; zero velocity; `Voice`; `BG_EffectSound` in `q_sound.h` |
| OpenAL Soft's build (`Dependencies.cmake`) | JACK and PortAudio off on Linux only | off everywhere |
| The converter (`tools/eal_converter`) | the matrix written when a `d` or an obstacle appeared in it; the check's ratios compared at any level | the matrix written when an obstacle of it acts (the default one only when its level is not 0); a ratio compared where its level is not 0; 65 of the 83 files without their matrix (`assets/jka/eagle`, `assets/jk2/eagle`) |
| Tests | | the merged loops behind a wall, the reverb off from the map's load, a map loaded with the listener not yet given, a density of 0, boxes out of order, the 83 zone files the games ship read and written back, a robustness test of the reverb, the duplicate sets and the defaults, the first one-shot after the fade, the lip sync with the clock behind the sound, a restart's sounds saved and restored, the dynamic music read once and played without a transition, the setting changed while it plays, the sound events' packing, the channels, the voices' levels and the effect sound's rule; the system fuzzer loading zones and reading `dms.dat` and `sound.txt` from its input, the decoders' reading after a failed seek |

Not changed, with the reason (each in the work's document): decisions documented as they are (a missing sound never looked for again, the best format looked for first), defects that need a run to show and were not reproduced (a reopen retried every frame, races of a few microseconds, the output modes' boost), a delayed effect's sound losing its shooter (no effect of the games' data has one on a shot).

The work: [progress/step-21-sound.md](progress/step-21-sound.md) section 16.

</details>

## I. The platform layer

### 39. The multiplayer's window and input on a new platform layer

**Before.** The client's window, keyboard and mouse went through SDL 2.0.12 (2020) and a layer inherited from ioquake3: the window created again at every restart of the video, a table of thirteen video modes, every key and mouse event dated when the frame read it and rounded to a millisecond, the mouse's motion rounded to whole counts, the mouse held by the game in its menus, and keys the engine's table did not know left without any way to bind them.

**Now.** A layer written from zero on SDL 3.4.20 (`src/shared/platform`), linked into the client: no `SDL2.dll` beside it. The multiplayer's client is on it; the single player's two games follow.

**Why.** The most responsive possible, the same for every player whatever the frame rate, and no setting for hardware that no longer exists.

**Gain.**

| fix | before | now |
|---|---|---|
| The share of a tick a movement key is held | from times rounded to the millisecond at which the frame read the key: a tap of 3.2 ms in a tick of 10 ms counted 0.3 or 0.4 | from the time the key moved at, in nanoseconds, the keyboard read raw on Windows: 0.32, whatever the frame rate |
| The mouse's motion | rounded to whole counts when read | in floats up to the view angles |
| The mouse in the menus, the console, a level loading | held by the game; in a window, alt-tab or the Windows key to get it back | the system's pointer with the game's image at the size it had: free to leave the window, moving at the desktop's speed also at a low frame rate. Taken only in game |
| The pointer on a second screen | never (the mouse was held) | free in a window and in borderless fullscreen, kept on the game's screen in exclusive fullscreen (a click elsewhere would minimize the game) |
| The window at a restart of the video (`vid_restart`, Alt+Enter) | destroyed and created again | the same window, changed where it stands |
| The display | `r_mode` (thirteen sizes), `r_fullscreen`, `r_noborder`, `r_customwidth`, `r_customheight`, `r_displayRefresh`, `r_centerWindow` | `r_displayState` (0 a window, 1 borderless fullscreen, the default, 2 exclusive fullscreen), `r_screen`, `r_windowWidth` and `r_windowHeight` (1280 by 720), `r_exclusiveMode` (`1920x1080@144`, one of the screen's own modes; empty for its present mode) |
| Linux under Wayland | through XWayland, SDL 2 preferring X11 | natively, SDL 3 trying Wayland first (then X11); the window placed by the compositor, as Wayland wants, fullscreen on the screen chosen |
| The OpenGL context | sixteen attempts lowering the colour, the depth and the stencil | one request (24, 24, 8 and the samples), one more without multisampling, then an error saying what failed |
| `modelist` | the thirteen fixed sizes | each screen with its number, its desktop's mode and its own modes, written as `r_exclusiveMode` takes them |
| The console key | `cl_consoleKeys`, `cl_consoleUseScanCode`, `cl_consoleShiftRequirement` | the key under Escape on every layout; with Shift it is the key its layout marks |
| A key the engine's table has no code for (`§`, `°`, `´`, F13...) | cannot be bound | bound by a name (`U+00A7`, `F13`), up to sixteen such keys at once |
| The right Alt on Linux, where most European layouts make it Alt Gr (27 November 2026) | a key of its own (`MODESWITCH` in the controls menu): Alt+Enter with it did nothing and `RALT` was never pressed | the right Alt (`RALT`), as on Windows; Alt Gr still types its characters |
| The system's arrow in game (27 November 2026) | seen once left over the view after changes of the display, until the next Alt+Enter | hidden: the mouse's focus taken back when Windows deactivates the window for an instant |
| A character typed in a menu | sent to the UI and the cgame as a key with a flag (`K_CHAR_FLAG`) | its own event (`CharEvent`), beside `KeyEvent` |
| Escape in a menu over the game, while a row waits for the key to bind or a field is edited | the menu closes (the original game's) | the bind is cancelled or the editing ends; the menu stays |
| The screen's brightness table | set by the game for the whole screen, put back at each loss of the focus | never touched: `r_gamma` acts on the textures as they are loaded (after a `vid_restart`) until the renderers' output stage |
| A `.dat` file outside the archives, on a server that checks its clients' archives (`sv_pure 1`) | read in place of the archives' own (an exception made for the journal's files): a changed `base/ext_data/dms.dat` was taken | ignored, as the other data outside the archives |
| The journal (`journal 1` and `2`, `journal.dat`, `journaldata.dat`) | recorded and replayed the system's events, never the network's packets | removed, in the three games: demos, `com_debugSeed` with `fixedtime` and the scripted scenarios replay a session |

Costs: about 2.8 MB more in the client's executable (SDL 3 linked statically, measured: 5.8 to 8.6 MB with clang), and one DLL of 1.5 MB less beside it, 1.35 MB more on disk in all. Measured against the build before this work on 26 November 2026, a quiet machine, clang ([progress/step-15-platform.md](progress/step-15-platform.md) section 14.17): the process's CPU on a demo played in real time 1 to 25 % of a core lower (39.3 against 51.8 % at 333 images a second), the time between images as regular (99th percentile 3.59 against 3.81 ms at 333), the input margin the same, the reading of the input 5 us a frame against up to 176 us with a mouse at 8,000 reports a second, a restart of the video 330 to 420 ms against 490 to 730, the start to the main menu 0.16 to 0.19 s shorter, the memory the same; borderless fullscreen shown directly, the program owning the screen, as before.

Not there yet in the multiplayer: no joystick nor gamepad; `r_ext_multisample` is read when the program starts. In borderless fullscreen the screen goes black for about 10 ms at each alt-tab, as with the build before: NVIDIA's OpenGL driver gives the window the screen as in exclusive fullscreen, and gives it back to the compositor at each alt-tab (SDL's issue 12791).

<details>
<summary>Technical details</summary>

| | before | now |
|---|---|---|
| The library | SDL 2.0.12, a prebuilt DLL on Windows, the system's on Linux | SDL 3.4.20 in `libs/SDL3`, built with the project as a static library, its unused subsystems off; on Linux X11 and Wayland required at build and opened at run time |
| The layer | `src/shared/sdl/sdl_window.cpp`, `sdl_input.cpp` | `src/shared/platform`: `platform.h` (the clock, the start), `window/`, `input/`, and `engine/`, which each engine compiles; the C++ Core Guidelines throughout, every finding of clang-tidy an error |
| The engine's side | `WIN_Init`, `WIN_Present`, `WIN_SetGamma`, `WIN_Shutdown`, `IN_Frame`, `Sys_QueEvent` | `Platform_OpenWindow` (once), `Platform_Frame`, `Platform_Display`, `Platform_Present`; the events given straight to `CL_KeyEvent`, `CL_CharEvent`, `CL_MouseEvent`, `CL_PointerEvent` |
| The client's clock | `timeGetTime`, `CLOCK_MONOTONIC` | the layer's, SDL's (`SDL_GetTicksNS`), the one the events are dated on; the dedicated server keeps its own |
| A key's time | an `unsigned` of milliseconds, in the bind's command | an `int64_t` of nanoseconds, in the bind's command, in `kbutton_t`, up to `CL_KeyState` |
| The modules' interfaces | `KeyEvent(int)`, `MouseEvent(dx, dy)` | `KeyEvent(fakeAscii_t)`, `CharEvent(int)`, `MouseEvent(x, y)` the pointer's place in pixels; `UI_API_VERSION` 7, `CGAME_API_VERSION` 6 |
| The renderer's interface | a window created and destroyed through its imports | the display asked as it is; a new export `ext.LoadImagePixels` for the pointer's image |
| A key's name on X11 and Wayland | the layout's symbol, also one the layer does not name (Alt Gr's `ISO_Level3_Shift`, SDL's `SDLK_MODE`) | a key that types no character and whose symbol the layer does not name takes its place's code (`KeyCodeOf`), as SDL gives it on Windows and macOS |
| The mouse's focus in relative mode | lost when Windows deactivated the window between two frames, SDL drawing its arrow until the pointer itself moved | `Input::Poll` turns the relative mode off and on again on `SDL_EVENT_WINDOW_MOUSE_LEAVE` while the mouse turns the view and the window has the keyboard |
| The tests | none | `modernjk.platform` on SDL's dummy video driver; `modernjk_test_platform real` by hand for a window with a context |

The work: [progress/step-15-platform.md](progress/step-15-platform.md) section 14.

</details>

### 40. The single player's two games on the platform layer

**Before.** The single player's engine, which builds Jedi Academy and Jedi Outcast, still opened its window and read its keys and its mouse through SDL 2 and the old layer, with `SDL2.dll` beside each executable. On this branch the view did not turn in either game, with the mouse or with the keys, while the menus worked.

**Now.** Both games run on the layer of section 39, through the same engine part as the multiplayer: what that section lists holds for them too. SDL 2 and the old layer are gone from the repository.

**Why.** One system for the three games: a change of the window or of the input is made once.

**Gain.**

| fix | before | now |
|---|---|---|
| The view in game (this branch's own defect) | never turns: in a level, moving the mouse or holding the key bound to `+left` leaves the view where the level started it | turns as in the original game |
| The share of a frame a key is held | from times rounded to the millisecond at which the frame read the key | from the time the key moved at, in nanoseconds |
| The mouse's motion | rounded to whole counts when read | in floats up to the view angles |
| The mouse in the menus and the console | held by the game, the pointer drawn by the game and moved by the mouse's own counts (three times the desktop's speed in a window 1,920 wide) | the system's pointer with the game's image at the size it had (48 of the menus' 640 by 480), at the desktop's speed, free to leave the window |
| The mouse while a video plays | held | held, and no pointer over the video |
| The window, the display's settings, `modelist`, `minimize`, Alt+Enter, the console key, the keys without a code, the brightness table | as section 39's "before" | as section 39's "now" |
| Escape in a menu over the game, while a row waits for the key to bind or a field is edited | the menu closes (the original games') | the bind is cancelled or the editing ends; the menu stays |
| The right Shift, Ctrl and Alt in the engine's shortcuts (this branch's own defect, since they are keys of their own) | ignored: the right Alt with Enter does nothing, the right Ctrl with Page Up scrolls the console by one line | as the left ones: Alt+Enter, Shift+Escape, the console's and the fields' shortcuts take either side |
| Jedi Outcast, the picture stored in a saved game (OpenJK's since March 2025) | upside down: on the loading screen when the save is loaded, and beside the save in the load menu | upright, for a save made from now on |
| `SDL2.dll` beside the two executables | needed | gone |
| A setting with a range, after a number out of range was read (the three games; OpenJK's) | refused whatever its value and put back to its default: in the console, `r_primitives 1e999` then `r_primitives 1` gives `cvar 'r_primitives' must be numeric, setting to 0`, and so does every such setting after it | checked on its own value: `r_primitives 1` is taken |

Not there yet: no joystick nor gamepad.

<details>
<summary>Technical details</summary>

| | before | now |
|---|---|---|
| The view that did not turn | `CL_MouseMove` (`src/code/client/cl_input.cpp`) computed the mouse's rate as `SQRTFAST(mx * mx + my * my)`, which is `x * Q_rsqrtf(x)`: with no motion, 0 times the inverse root of 0. The old approximation of the inverse root gave a large number and the product was 0; since the inverse root is exact (section 11) it is infinite and the product is not a number, which the test for "no motion" after it let through into the view angles for the rest of the level | the function returns before the rate when the mouse did not move, as the multiplayer's does |
| The engine's side | `WIN_Init`, `IN_Frame`, the system's event queue | `Platform_OpenWindow`, `Platform_Frame`, the events given straight to `CL_KeyEvent`, `CL_CharEvent`, `CL_MouseEvent`, `CL_PointerEvent` |
| Who has the mouse | decided in the old layer | each client's `CL_MouseTurnsView`, which the engine part asks: the multiplayer's in game while no menu, console nor cgame menu takes the pointer; the single player's the same, and while a video plays |
| The UI (in the engine) | `_UI_KeyEvent(int)` with `K_CHAR_FLAG`, `_UI_MouseEvent(dx, dy)`, the pointer drawn | `_UI_KeyEvent(fakeAscii_t)`, `_UI_CharEvent(int)`, `_UI_MouseEvent(x, y)` the pointer's place in pixels; `K_CHAR_FLAG` gone from the three games |
| The renderer's interface | a window created and destroyed through its imports | the display asked as it is; the pointer's image read through `R_LoadImage`, which it already exported |
| SDL 2 | `libs/SDL2` (a prebuilt DLL for Windows), the system's on Linux, `src/shared/sdl/` | removed; the build asks for no SDL 2 package |
| The key codes (`fakeAscii_t`) | defined twice, `src/codemp/ui/keycodes.h` and `src/code/client/keycodes.h` | once, `src/shared/qcommon/q_keycodes.h` |
| A number's check (`Q_isanumber`, `src/shared/qcommon/q_string.cpp`) | `errno` read after the conversion, which sets it only when it fails: an error left by an earlier call made every number "not numeric" | `errno` cleared before the conversion |

The work: [progress/step-15-platform.md](progress/step-15-platform.md) section 14.14.

</details>

### 41. The display's rows in the setup menus

**Before.** The video page of the six setup menus (the multiplayer's, Jedi Academy's and Jedi Outcast's, from the main menu and in game) showed a video mode among eight fixed sizes from 640 by 480 to 2048 by 1536, a colour depth (16 or 32 bits) and full screen on or off; the quality presets also set the full screen, a mode of 640 by 480 or 800 by 600 and the colour depth. Since sections 39 and 40 these rows wrote settings the engines no longer read: they did nothing.

**Now.** Four rows in their place, each fed by the screens the system reports:

- **Display**: Window, Borderless, Exclusive (`r_displayState`).
- **Screen**: each screen by its number and its name, "1: AW2724HF" (`r_screen`).
- **Resolution**: in a window, the screen's own sizes that fit in the part a window can take, without the taskbar (`r_windowWidth`, `r_windowHeight`); in exclusive fullscreen, "Desktop" (the screen's present mode) then each size of the screen's modes (`r_exclusiveMode`); greyed in borderless fullscreen, showing the desktop's size.
- **Refresh rate**: in exclusive fullscreen at a size of its own, the rates the screen has at that size, the highest first (`r_exclusiveMode`); greyed otherwise, showing the desktop's rate.

The vertical sync's row takes a third value, Adaptive (`r_swapInterval -1`). The quality presets leave the display alone. "Apply" restarts the video, as before, on the same window.

**Why.** Each display setting has its row in the menus (decided 25 November 2026), and a list written in a menu cannot know a player's screens: a screen of 2560 by 1440 at 165 Hz had no row for its size nor its rate.

**Gain.**

| fix | before | now |
|---|---|---|
| The sizes offered | eight sizes written in the menu, 4:3 and 5:4 for most, whatever the screen | the screen's own: on a screen 1920 by 1080, nineteen sizes for exclusive fullscreen, and for a window the sixteen that fit above a taskbar |
| The refresh rate | not in the menus (`r_displayRefresh` in the console) | among the screen's rates at the chosen size: 360, 300, 254.87, 240... down to 23.98 on a screen of 360 Hz (eighteen rates) |
| A second screen | not in the menus | chosen by its number and name |
| The vertical sync | off or on | off, on, or adaptive: an image late for the screen's refresh is shown at once, with a tear, instead of a whole refresh later (on a driver without it, the plain vertical sync) |
| The quality presets | changed the mode to 640 by 480 or 800 by 600, the full screen and the colour depth | the picture's quality only |

<details>
<summary>Technical details</summary>

| | before | now |
|---|---|---|
| The rows | `ITEM_TYPE_MULTI` with fixed lists of `ui_r_mode`, `ui_r_colorbits`, `ui_r_fullscreen` | the state a list of `ui_r_displayState`; the screen, resolution and refresh rate rows owner-drawn (`UI_DISPLAY_SCREEN`, `UI_DISPLAY_RESOLUTION`, `UI_DISPLAY_REFRESH`), as the sound device's row is; greyed by `cvarTest` on `ui_r_displayState` and `ui_r_refreshChoice`, the menus given a `disableColor` |
| What each row shows and gives | | `src/shared/platform/menus/display.cpp`, without SDL, compiled by both engines and the multiplayer's UI module (`modernjk_platform_menus`), tested in `modernjk.platform` |
| The screens for the UI | | `Platform_Screens`, read when the window opens, at each start of the renderer and when a screen is plugged, unplugged or changes its desktop's mode or usable area; the multiplayer's UI through `uiImport_t::ext.Screens` (`UI_API_VERSION` 8) |
| `getvideosetup`, `updatevideosetup` | copied `r_mode`, `r_fullscreen`, `r_colorbits`, `r_depthbits` | copy `r_displayState`, `r_screen`, `r_windowWidth`, `r_windowHeight`, `r_exclusiveMode` |
| The texts | | `assets/jka/strings/<Language>/MODERNJK.str` and `assets/jk2/strip/MENUS4.sp`, in English, French, German and Spanish |

The work: [progress/step-15-platform.md](progress/step-15-platform.md) section 14.16.

</details>

### 42. The picture's output stage

**Before.** The game drew straight into its window. The brightness slider (`r_gamma`) changed the screen's own colour table: the whole desktop was brightened while the game ran, the table was given back at each alt-tab and taken again when the game came back (the picture changed a little each time), and the screenshots were taken without it. On this branch since section 39 the table was never touched, and the brightness was baked into the textures when they were loaded: a change needed a restart of the video, and what is not a texture (the colours of the vertices, the fog, part of the menus and the HUD) stayed as it was. The multisampling (`r_ext_multisample`) was the window's: in the console only, applied at the next restart. A change of the display (Alt+Enter, the resolution, the screen, from the menus' Apply or the console) restarted the renderer and the game's modules, about a third of a second, and brought the menus back to their first page.

**Now.** The three games draw their whole picture into an image of their own, the scene; at the end of each image one pass draws it into the window and applies the brightness there.

- **The brightness** changes the whole picture at once while the slider moves, the menus and the HUD included, the same in a window, in borderless and in exclusive fullscreen, with or without the focus; the desktop is never touched.
- **The screenshots**, the level shots and Jedi Outcast's saved pictures are read after the pass: they show what the screen shows.
- **The dark tones** keep their levels when the brightness is raised: the scene holds 16 bits a channel, and the pass adds a faint fixed noise as it rounds the picture to the screen's 8 bits, so a dark gradient shows no bands; a colour the menus or the HUD draw exactly stays as it is.
- **Anti-aliasing** (the multisampling) has its row on the advanced video page of the six setup menus: Off, 2x, 4x, 8x, as far as the graphics card allows, applied at the next image without a restart.
- **The window** itself holds the finished picture only: its colour, without depth, stencil nor samples, which the scene has.
- **A change of the display** (Alt+Enter, the resolution, the screen, the display state; from the menus, the console or a configuration file) is applied at the next image without restarting anything: the scene follows the window's size, the menus and the HUD their new scale. So does a change of size the system makes (a window moved to a screen of another scale, a Wayland compositor resizing it), which was drawn at the old size before. The menus' Apply restarts the renderer only for the texture settings that need it (and asks first, as before, since the menus then go back to their main page); otherwise it applies at once, without the question, and the player stays on the video page, whose display rows follow a change made meanwhile (Alt+Enter, the console) unless the page has a change of its own to apply.

**Why.** Recent games leave the screen's table alone (Microsoft's documentation of DirectX asks it, since the table may hold the monitor's calibration) and apply the player's brightness in a last pass of their own, so that it is the same everywhere, in the captures included, and changes at once (decided 25 November 2026; the decisions of 26 November 2026 in [progress/step-15-platform.md](progress/step-15-platform.md) section 16.2).

**Gain.**

| fix | before | now |
|---|---|---|
| The brightness slider | the desktop brightened with the game and put back at each alt-tab (this branch: a restart of the video, the textures only) | the whole picture at once, the desktop untouched |
| A screenshot with the brightness raised | as dark as without it | as bright as the screen |
| A dark gradient with the brightness raised (`r_gamma` 2), measured in four levels' dark places | 15 or 16 of the screen's 64 darkest levels, bands up to 16 levels apart (the screen's ramp, as an 8-bit scene) | 47 to 64 of them, the steps of one level left hidden by the dither |
| The multisampling | `r_ext_multisample` in the console, then a restart of the video | the Anti-aliasing row, applied at the next image |
| The single player's dissolve (the screen melting into the next) | started from what the driver had left in the window after the last swap | started from the last image drawn |
| Alt+Enter, a new resolution or screen applied | a restart of the renderer and the modules, 330 to 420 ms, the menus back to their first page | 5 to 18 ms (the system's switch of the window 3 to 13 ms), the player where they were |
| A window moved by the system to a screen of another scale | the picture drawn at the old size, cut or partial | the picture at the window's new size |
| The picture on Linux after a change of the display or of the anti-aliasing, then a level loaded (this branch's own defect, since the scene's first version) | black but for a piece of it in the bottom left corner, the size of one of the level's textures; Alt+Enter brought it back | the whole picture |
| The dynamic glow's images with the glow off (the original games') | three images of the window's size left in the graphics card's memory at each level loaded, about 34 MB at 1920 by 1080 (computed), freed at the program's end | freed at each restart of the renderer |

The cost, measured on 27 November 2026 against the build before this work (NVIDIA's RTX 5070 Ti, clang): the pass takes 15 µs of the graphics card per image at 1920 by 1080, 31 to 37 µs with 4 samples and their resolve; a demo played in borderless fullscreen at 100, 125 and 333 images a second and with the vertical sync at 165 Hz keeps the same times between images (their 99th percentile within 0.15 ms of the build before's at 100, 125 and 333, 6.4 ms against 7.6 at 165 Hz), the same delay to the screen and the same processor's load within its noise; played without a cap, 983 to 985 images a second against 979 to 986. No image of delay added. The scene's colour takes 16.6 MB of the graphics card's memory at 1920 by 1080, 66 MB with 4 samples (computed); not measured on an integrated graphics card, where its traffic, twice the window's, weighs more.

<details>
<summary>Technical details</summary>

| | before | now |
|---|---|---|
| Where the picture is drawn | the window's back buffer, with its depth, stencil and samples | a framebuffer object of the renderer (`src/shared/rd-common/output/`, its README): one colour image and one `DEPTH24_STENCIL8` image; multisampled, renderbuffers and a single-sampled copy for the resolve |
| The brightness | the screen's ramp (SDL 2), then `R_LightScaleTexture` on the textures; the multiplayer's `r_gammaShaders` (0 by default): the back buffer copied each image and drawn through ARB programs and a 3D table of 64 levels a channel | one pass in GLSL 1.30 into the window, `pow(c, 1 / r_gamma)` in floating point, the formula of the ramp's table, `r_gamma` kept in [0.5, 3] |
| What reads the picture | the back buffer: the dynamic glow's copies, the distortion's capture, the post-render entities' copy, the flares' depth, the overdraw's stencil, the dissolve | the scene, made readable first (`Output_MakeReadable`: resolved when multisampled) |
| The captures (`RB_ReadPixels`) | the back buffer, then `R_GammaCorrect` with the screen's ramp | the window's back buffer after the pass |
| The window's context | 24 bits of colour, 8 of alpha (SDL 3's default), 24 of depth, 8 of stencil, the samples of `r_ext_multisample` | 24 bits of colour only (`platform::Context::OpenGl`) |
| OpenGL | any version | 3.0 or later, checked at the start, with the driver's text when below |
| `r_ext_multisample` | latched, read when the window opened | applied at the next image; 2, 4 or 8 at most what the driver allows, which `r_ext_multisample_avail` gives the menus' row (owner-drawn, `UI_MULTISAMPLE`) |
| Overbright (`r_overBrightBits`) | needed the screen's ramp and fullscreen, 0 by default | removed; `r_mapOverBrightBits` still scales the lightmaps when loaded |
| The display's settings (`r_displayState`, `r_screen`, `r_windowWidth`, `r_windowHeight`, `r_exclusiveMode`) | latched: applied by a restart of the video (`vid_restart`), which Alt+Enter and the menus' Apply ran | applied at the next frame by the platform layer's engine part, then `CL_DisplayChanged`: the renderer's new export `DisplayChanged` (the scene's and the dynamic glow's images made again at the new size, the textures kept), the menus' pointer image, the UI's and the cgame's new call (they read the configuration again and compute their scales again); also on the system's own change of size (`SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED`) |
| The menus' Apply (`UI_UpdateVideoSetup`) | always `vid_restart` | `vid_restart` only when `r_picmip`, `r_texturebits`, `r_detailtextures`, `r_ext_compress_textures`, `r_subdivisions` or `r_allowExtensions` has a new value |
| The textures' names | chosen by the renderers (`1024 + giTextureBindNum++`), while the scene's were given by the driver: Mesa gives the name after the highest in use (`_mesa_HashFindFreeKeyBlock`), so the scene made again during a session took names the renderers then gave the next level's textures, which replaced its images | every name given by the driver (`qglGenTextures`) |
| The dynamic glow's three images | made at each start of the renderer, deleted only when `r_DynamicGlow` was on | deleted at each shutdown of the renderer, their names cleared |
| The interfaces' versions | | the multiplayer's `REF_API_VERSION` 12, `CGAME_API_VERSION` 7, `UI_API_VERSION` 9; the single player's `REF_API_VERSION` 21 and `GAME_API_VERSION` 13 (the cgame's command `CG_DISPLAY_CHANGED` is in the game's library) |
| The scene's colour | the window's, 8 bits a channel | `GL_RGBA16`, 16 bits a channel unsigned normalized, which clamps the blends as the window does (chosen by the measure of the banding against `RGBA8`; `RGBA16F` would not clamp them) |
| The dither | none | in the pass, Jorge Jimenez's interleaved gradient noise (SIGGRAPH 2014), fixed, within 0.375 of a level: an exact level kept |
| The tests | | `modernjk.rd-common` (`test/rd-common`); by hand, `modernjk_test_rd_common real` draws the 256 levels of grey through the pass on the system's driver and reads them back: exact at `r_gamma` 1, within one level of the curve otherwise |

The work: [progress/step-15-platform.md](progress/step-15-platform.md) section 16.

</details>


## J. The three games written once

### 43. The keys and the input written once for the three games

**Before.** The clients' keys (their states, their binds, the console's line and its history, the chat's line, the keys' names) and their input (the buttons of the game's commands, the view turned by the keys, the mouse and the joystick, a command's movement) were written twice: once in the multiplayer's client and once in the single player's, which builds Jedi Academy and Jedi Outcast. The two copies had drifted apart, and a fix made in one was missing from the other.

**Now.** One code for the three games (`src/shared/client`), compiled by each client; each tree keeps only where a key goes beside the console (its own modules, its own menus, a video playing), the making of its commands and the values of its game (the buttons' bits, a key's movement while walking).

**Why.** A change made once, the same behaviour for every player of the three games (decided 26 and 27 November 2026; [progress/step-15-platform.md](progress/step-15-platform.md) section 17).

**Gain.**

| fix | before | now |
|---|---|---|
| The right Shift, Ctrl and Alt in the single player | named `SHIFT`, `CTRL` and `ALT`, as the left ones: the right Ctrl bound to crouch worked until the game was started again, then the left Ctrl crouched instead of firing and the right one did nothing (reproduced in Jedi Academy) | `RSHIFT`, `RCTRL` and `RALT`, as in the multiplayer: each bind comes back on its key |
| Home and End in a console line longer than the console, in the single player | Home took the cursor to the line's start without showing it: the line stayed scrolled, the cursor out of sight (read in the code) | the line's start shown, the cursor on it, as in the multiplayer |
| Ctrl-E in a long console line, in the three games | the line's last character hidden, the cursor one column past the line shown | the last characters shown, the cursor after them, as End does |
| Ctrl-E in a short console line, then a line typed past the console's width, in the three games | the line scrolled from a negative place, and the characters drawn at its start read from the memory before the line (read in the code) | the line scrolled from its start |
| A character typed in the middle of a long console line, in the three games | the text left of the cursor moved by one at each character, the cursor staying in its column | the cursor moves, the text stays, until the cursor reaches the console's edge |
| A control character in the clipboard pasted into the console, in the three games | acted as its key: a Ctrl-C emptied the line, a Ctrl-V pasted again without end | left out: only the printable characters are typed in |
| The mouse's acceleration in the single player | one formula, `cl_mouseAccel` added to the sensitivity times the mouse's speed | the multiplayer's two: `cl_mouseAccelStyle` 1 gives the power curve of `cl_mouseAccel` and `cl_mouseAccelOffset` (0 and 5 by default, no acceleration while `cl_mouseAccel` is 0) |
| The cgame's filter of the commands, in the multiplayer | each command typed or run by a key copied into the memory shared with the cgame, which could refuse it or put another in its place; the game's cgame accepted every one | removed: a copy and a call into the cgame less for each command |

The cost: none at run time measured or expected; the console's line drawn at each frame reads the line once and copies the characters shown, as before.

<details>
<summary>Technical details</summary>

| | before | now |
|---|---|---|
| The keys' code | `src/codemp/client/cl_keys.cpp` and `keys.h`, `src/code/client/cl_keys.cpp` and `keys.h` | `src/shared/client/engine/cl_keys.cpp` and `keys.h`, compiled by each client against its own `<client/client.h>`; each tree's `client/cl_keys.cpp` holds six small functions: what a key does during a video, what Escape closes or opens, whether the client's own menu takes the keys, where a key and a character go beside the console |
| The input's code | `src/codemp/client/cl_input.cpp` and `src/code/client/cl_input.cpp`, each with its buttons, its view and its movement | `src/shared/client/engine/cl_input.cpp` and `input.h`: the buttons and their common commands, the view turned at every frame, a command's movement over its span, the mouse's movement gathered until the next command; each tree's `client/cl_input.cpp` makes its commands (per tick in the multiplayer, per frame in the single player), with four small functions (the view centred, the automap's keys and mouse, the fighters' pitch) and its game's values in its `client.h` |
| A button's share of a span | `CL_KeyState`, `IN_KeyDown`, `IN_KeyUp` in each tree; `kbutton_t` in each `client.h` | `src/shared/client/button.h`, with its tests; `kbutton_t` defined once |
| A text field's editing and the keys' names | in the engines' code | `src/shared/client` (`field.h`, `keynames.h`): a static library independent of the engines, after the C++ Core Guidelines, with its tests (`modernjk.client`) |
| `field_t` | defined in each tree's `qcommon.h` | one definition, `src/shared/client/field.h` |
| The single player's `Cmd_AddCommand` | without a command's description, the file system's code taking one through `files_engine.h` | a form taking the description, which it ignores, in its `qcommon.h` |
| The multiplayer's cgame interface | `IncomingConsoleCommand`, `TCGIncomingConsoleCommand`; `CGAME_API_VERSION` 7 | removed; `CGAME_API_VERSION` 8 |

The work: [progress/step-15-platform.md](progress/step-15-platform.md) section 17.

</details>

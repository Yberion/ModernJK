# Step 21: the sound on OpenAL Soft, in every tree

Status: ⏳ started on 23 November 2026: the inventory of the sound in both engines (sections 2 to 5) done; its decisions put (section 6) and taken the same day (sections 1, 7 and 8); part 1 (the libraries and the decoders) done the same day (section 9): `golden: SAME`, the VM validations ALL OK without a warning; committed. Then parts 2 and 3 made one (section 1): every tree moves onto the module in part 2. Part 2 started the same day: the complete inventory of what the game asks of the sound (section 11), the sound log on the old mixer and today's defects shown in game (section 11.9), the interface and its thirteen decisions, all taken (section 12). Nothing of part 2 is coded yet; where to resume: section 10.5. The library and its settings were chosen on 15 November 2026 ([../surveys/audio-libraries.md](../surveys/audio-libraries.md) section 8), the decoders on 19 November 2026 ([../surveys/audio-decoders.md](../surveys/audio-decoders.md) section 8); the plan's step: [../PLAN.md](../PLAN.md) step 21.

## 1. Decisions

| subject | decision |
|---|---|
| the library, 15 November 2026 | OpenAL Soft 1.25.2, its one-line patch for clang 23 carried until a release no longer needs it; no HRTF, asked off; the resampler `bsinc24` |
| the reverb, 15 November 2026 | kept, by zones, from the game's `.eal` files converted into a file of this game; none on a map that had none |
| the settings, 15 and 19 November 2026 | `s_khz` and `s_UseOpenAL` removed with their menu items; the system's default device followed; muted without the focus, by default |
| the decoders, 19 November 2026 | dr_mp3, dr_wav, libFLAC, stb_vorbis 1.24 (fuzzed by the module's own tests), behind the module's own interface; no Opus for now |
| the trees, 19 November 2026 | one module in `src/shared/` for both engines: the multiplayer and the single player of Jedi Academy and Jedi Outcast |
| the build, 19 November 2026 | OpenAL Soft's backends required (`ALSOFT_REQUIRE_PIPEWIRE`, `_PULSEAUDIO`, `_ALSA`, `_WASAPI`); the player needs only the runtime library of the system's sound server, loaded at run time |
| the interface, 23 November 2026 | a new interface written for the module, every call of the engines and the modules rewritten onto it (section 6, decision 1b), one entry point where several functions do the same thing |
| the parts, 23 November 2026 | the libraries and the decoders; the multiplayer; both single-player games; the reverb's zones; the measures and the listening (decision 3a) |
| the libraries, 23 November 2026 | approved, each at its newest version checked the same day: OpenAL Soft 1.25.2 (still its newest release), libFLAC 1.5.0 (still its newest), stb_vorbis 1.25 (released on 19 November 2026, after the survey's 1.24: the survey's checks run again on it), dr_mp3 and dr_wav at dr_libs' newest commit (its tags `mp3-0.7.4` and `wav-0.14.6` are older than the fixes the survey measured: checked again likewise) |
| the Linux backends, 23 November 2026 | the distribution's development packages (decision 5b): the sound servers are the system's drivers, loaded at run time; an exception in GUIDELINES section 9 |
| the reverb, 23 November 2026 | converted once by a tool outside the game (EalMan, not shipped) into a file of this game per map (decision 6a); the engine's behaviour: the listener's environment and its two neighbours through their apertures, each source sent to its own environment's reverb (decision 6c) |
| the memory, 23 November 2026 | every sound decoded to PCM when registered, the voices included (decision 7a) |
| the settings, 23 November 2026 | removed as listed in decision 9; Doppler for every moving sound in every tree, without a setting (10a); muted without the focus behind a setting, on by default (11b) |
| the video recorder, 23 November 2026 | rendered on OpenAL Soft's loopback device while recording (13a) |
| the tests, 23 November 2026 | committed sample files with decoding and robustness tests in `ctest`, and libFuzzer targets (14a) |
| Jedi Outcast's data, 23 November 2026 | checked (15b), from its own game folder (section 4) |
| the scope, 23 November 2026 | the whole sound system written again from a complete inventory of what the game needs, one code for the trees, every feature tested, today's defects fixed with their before and after (section 7) |
| the model, 23 November 2026 | one model for every tree in place of the channels (section 7, decision 2): a sound played with a class (bus, attenuation, priority) and, where one sound replaces another, a slot of its entity; a looping sound started once with a handle, moved and stopped; one attenuation per class and one lip sync, tuned by ear and eye; the voices of `CHAN_VOICE_GLOBAL` not positioned; one entry point to play a sound, and one table of the sound's functions for every module |
| the voices, 23 November 2026 | 64 real voices, the other sounds virtual, the 64 mixed chosen at every frame by audibility, a limit per sound (decision 8) |
| the latency, 23 November 2026 | OpenAL Soft's default, 3 periods of 512 frames, about 32 ms on Windows, for now (decision 12) |
| `valley.eal`, 23 November 2026 | the conversion tool gives its box its one environment, "Valley" (section 8, option a) |
| the conversion tool, 23 November 2026 | in a `tools/` folder at the repository's root, a target of the presets built on demand on Windows and Linux, EalMan in `libs/ealman` at a pinned commit, built only for the tool (section 8, option a) |
| the code, 23 November 2026 | clean and properly structured, as factored as it can be with as little code as the features need, without forcing it; every function commented with what it does (GUIDELINES section 10) |
| the video recorder, 23 November 2026, after part 1 | removed, in place of decision 13a (on the loopback device the game would be silent while recording): a player records with a program made for it (OBS Studio or another); what goes in part 2: the commands `video` and `stopvideo`, `cl_aviFrameRate`, `cl_aviMotionJpeg`, `cl_forceavidemo`, `client/cl_avi.cpp` (641 lines), the renderer's video frames (`rd-vanilla/tr_backend.cpp`, `tr_cmds.cpp`, `tr_init.cpp`, the import in `rd-common/tr_public.h`) and the mixer's hooks; and in the single player (decided the same day) `cl_avidemo`, a screenshot at every frame of a fixed rate (`src/code/client/cl_main.cpp:793`); lost with them: the capture of a demo at a fixed frame rate whatever the machine's speed |
| RTKit, 23 November 2026, after part 1 | required on Linux like the backends, as part 1 built it (section 9.1): the build needs D-Bus's development files, the player installs nothing (OpenAL Soft opens `libdbus-1.so.3` at run time and, without it or without the RTKit service, mixes at the normal priority) |
| the decoders' samples, 23 November 2026, after part 1 | 16 bits for every file, the files above 16 bits (none in the game) losing their lowest bits, below -96 dB (section 9.2) |
| the interface, 23 November 2026, after the inventory | as proposed in section 12.1 (one entry point `Play`, the classes and the entities' slots, the loops' handles, one table for every module); the listener's own sounds not positioned, as today; today's straight-line attenuation per class from today's distances, tuned by ear; Doppler at the physical speed of sound, about 12,000 units per second; the voices beyond the 64 virtual, their position advancing (FMOD's model); a sound the data starts twice at one instant and place played as one voice at twice the gain; `s_muteWhenUnfocused` 1 alone; `s_show 1` writing the sound log; `s_volume` the master volume of everything; the voice's state written as it is in the single player's saved games, their version raised; four voices of one file mixed at once, the quietest beyond them virtual; the shots as in the single player, the muzzle effect's sound only; the server's sound events kept, the class and the slot in place of the channel, the transport of the sounds placed in plan step 16 (section 12.2) |
| the parts, 23 November 2026, after part 1 | parts 2 and 3 made one: the multiplayer and both single-player games move onto the module together, since keeping the single player on the old code for one part would need the shared platform files (`src/shared/sdl/sdl_sound.cpp`, compiled by both engines in `modernjk_sys_client`, and the focus's pause in `sdl_input.cpp`) split between the engines, a split thrown away at the next part; the numbers of parts 4 and 5 kept |

## 2. The code (read on 23 November 2026)

| what | multiplayer (`src/codemp`) | single player (`src/code`, Jedi Academy and Jedi Outcast) |
|---|---|---|
| the mixer, the channels, the music, the ambient sets (`client/snd_*`) | 7,955 lines in 11 files | 7,781 lines in 11 files |
| the MP3 glue | `client/snd_mp3.cpp`, `.h` (616 lines, in the count above) | `client/cl_mp3.cpp`, `.h` (622 lines, in the count above) |
| the MP3 decoder (FreeAmp's Xing decoder, C) | `mp3code/`, 29 files, 9,109 lines | `mp3code/`, 29 files, 9,112 lines |
| the WAV reader | `GetWavinfo` in `client/snd_mem.cpp` | its copy in `client/snd_mem.cpp` |
| the device | `src/shared/sdl/sdl_sound.cpp` and `.h` (341 lines, SDL 2's callback reading the ring the mixer paints) | the same file |
| the stubs of the dedicated server | `null/null_snddma.cpp` (65 lines) | none (no dedicated server) |
| the video recorder | `client/cl_avi.cpp` (641 lines), fed by the mixer (`snd_mix.cpp`) | none (`cl_avidemo` writes screenshots only) |

The two engines' copies are nearly the same code: `snd_dma.cpp` differs on 330 of about 3,500 lines (`diff -w`), `snd_mem.cpp` on 24, `snd_mix.cpp` on 35, `snd_ambient.cpp` on 26; `snd_music.cpp` on 457, nearly all from the two trees' different text parsers (`GenericParser2`), the dynamic music's logic being the same. Jedi Outcast's engine is the single player's sources built with `JK2_MODE`, and no sound file tests that macro: Jedi Outcast plays its sound with the same code as Jedi Academy.

What only one tree has:

| feature | multiplayer | single player |
|---|---|---|
| Doppler | on the looping sounds, `s_doppler` 1 (the pitch raised only, by whole steps of the mixer) | none |
| `S_MuteSound`, `S_StopLoopingSound`, `S_Shutup` (no warning for a missing file while models load) | yes | no |
| a channel per looping sound (`S_AddLoopingSound(..., chan)`), so that its attenuation follows the channel | no (the loops all attenuate as `CHAN_AUTO`) | yes |
| `CHAN_VOICE_GLOBAL` and `CHAN_ANNOUNCER` | panned at full volume (`S_SpatializeOrigin` with a distance of 0) | centred, as the local player's own sounds (`S_Respatialize`) |
| looping sounds at once | 32 | 64 |
| `CHAN_LESS_ATTEN`'s radius at full volume | 2,048 units (8 × 256) | 1,280 units (5 × 256) |
| the lip sync's thresholds `s_threshold1` to `4` | 0.5, 4, 7, 8 | 0.3, 4, 6, 8 |
| `CHAN_MENU1` | yes | no; the order of the channels differs too (`soundChannel_t` in `src/codemp/qcommon/q_shared.h` and `src/code/game/channels.h`); the game's data names the channels (`animevents.cfg`, the scripts), never their numbers |
| the voice's loudness given to the game | to cgame only (`S_GetVoiceVolume`, the mouth) | to the game too: `gi.VoiceVolume`, a pointer to the mixer's array, read by the game's logic (an NPC that talks does not start another line, `NPC_reactions.cpp`; pain and death cut a line, `g_combat.cpp`; the face, `g_main.cpp`; `bg_pmove.cpp`) and written in the saved games (`lastVoiceVolume`); the UI reads it too (`ui_shared.cpp:6676`) |
| a cinematic's sound from a sound file | no | three cinematics play a file beside the RoQ (`music/cinematic_1`, `sound/chars/video/cinematic_5` and `_6`, `cl_cin.cpp:1902`) |
| the video recorder's sound | yes | no |

Dead code found: `S_SetLipSyncs` and `S_MP3PreProcessLipSync` (both trees) are defined and never called: they read a `lipSyncData` table that nothing fills (the removed OpenAL path's).

## 3. Who calls the sound

| caller | calls |
|---|---|
| the multiplayer's cgame and UI (`trap->S_*`, `trap->AS_*`) | about 570: `S_RegisterSound` 334, `S_StartSound` 131, `S_StartLocalSound` 45, `S_AddLoopingSound` 19, the others 1 to 6 each |
| Jedi Academy's cgame (`cgi_S_*`, `cgi_AS_*`, through numbered system calls) | 329 |
| Jedi Outcast's cgame (the same calls, its own numbers `CG_S_*_JK2`) | 267 |
| the multiplayer engine | `cl_main.cpp` (start, restart, the frame's `S_Update`, the renderer's import `S_RestartMusic` and `SND_RegisterAudio_LevelLoadEnd`), `cl_cin.cpp` (`S_RawSamples` at 22,050 Hz), `cl_keys.cpp`, `cl_cgameapi.cpp` (18 entries), `cl_uiapi.cpp` (4), `z_memman_pc.cpp` (frees the oldest sounds when the zone is full), the file system (`S_ClearSoundBuffer`, 4 calls) |
| the single-player engine | the same, plus `cl_ui.cpp` (the UI is in the engine: `S_StartLocalLoopingSound`), `cl_scrn.cpp`, `cl_cin.cpp` (the cinematics' sound files), `sv_game.cpp` (`VoiceVolume`) |
| the platform | `sdl_input.cpp`: `SNDDMA_Activate(false)` pauses the device when the window loses the focus, `true` resumes it (today's behaviour) |

## 4. The game's data (counted on 23 November 2026 in `assets0.pk3` to `assets3.pk3`)

- Reverb: 57 maps with a zone file, `eagle/<map>.eal`: 23 multiplayer, 34 single player (the 58th file, `eagle/t1_sour..eal`, 2,124 bytes beside `t1_sour.eal`'s 37,264, is never loaded: the engine asks for `eagle/<map>.eal`). A file holds the environments with every EAX 4 reverb value, a BSP of planes whose leaves give the environment that holds a point (EalMan's `GetListenerDynamicAttributes`, `getIndexIDfromBSP`), and the apertures between environments; so the converted file needs the zones' geometry, not only their presets. Jedi Outcast's data: the last item below.
- The ambient sets: `sound/sound.txt` (41,044 bytes); the dynamic music: `ext_data/dms.dat` (24,458 bytes).
- The menus that show sound settings: `ui/jamp/setup.menu` and `ui/jamp/ingame_setup.menu` (multiplayer), `ui/setup.menu` and `ui/ingamesetup.menu` (single player), each with `s_khz` and `s_UseOpenAL` beside the volumes and the language; the UIs' code still handles both (`UI_Update` runs `snd_restart` for `s_khz`, `Item_ApplyHacks` greys the EAX item).
- The files: 12,902 MP3 and 302 WAV ([../surveys/audio-decoders.md](../surveys/audio-decoders.md) section 2).
- Jedi Outcast (its game folder, read on 23 November 2026): 8,126 MP3 and 309 WAV under `sound/` and `music/`; `sound/sound.txt` and `ext_data/dms.dat`; 41 zone files, 26 for its single player's maps and 15 for its multiplayer's (which this project does not build), 3 of them changed by a later archive; every one is of the same format as Jedi Academy's (major version 3) and holds the zones' BSP (its `gema` chunk: 6 to 1,345 planes, 1 to 107 zones each pointing to one of the environments); EalMan, built as `EAXMan.dll` 3.0, loads 40 with their presets; none has the centre points or the apertures of Jedi Academy's files (their sources are the editor's test sounds, `boot1to4.wav`, `repeater.wav`, `saberhup1to8.wav`), so the engine found the listener's zone and played its environment, without neighbours; `valley.eal` does not load (`0x80040004`, EalMan's `FileInvalid`): it holds one environment, "Valley", and a BSP of 6 planes whose one inside leaf points to no zone, its zone table being empty, which EalMan refuses (a BSP needs at least one zone), where Jedi Academy's single-environment files (`mp/duel1`'s "Courtyard") have one zone holding their environment; its menus `ui/setup.menu`, `ui/ingamesetup.menu` and the multiplayer's show `s_khz` and `s_UseOpenAL` too.

## 5. A level's sounds in memory (measured on 23 November 2026)

Two silent sessions of today's build (`s_volume 0`, `s_musicvolume 0`), then `soundlist`:

| session | sounds | as 16-bit samples | voices kept as MP3 (`sound/chars/`) | the mixing rate | PCM at the files' rate |
|---|---|---|---|---|---|
| multiplayer, `mp/ffa3`, 5 bots, about 40 s | 381 | 7,506,781 samples (15.0 MB) | 53, 1,445,184 samples once decoded | 22,050 Hz (`s_khz 22` archived in this machine's `modernjk.cfg`, the menu's "High") | at most 36 MB |
| single player, `yavin1`, about 50 s | 419 | 15,329,411 samples (30.7 MB) | 88, 6,776,064 samples once decoded (13.6 MB) | 44,100 Hz | at most 44 MB |

So the multiplayer game of this machine mixes today at 22,050 Hz: what the menu's "High" sets, half the engine's default ([../surveys/audio-libraries.md](../surveys/audio-libraries.md) section 7).

## 6. Decisions to take

Put on 23 November 2026, all at once; each has the mechanism, the options, and the recommended one first.

1. **The module's door.** One static library in `src/shared/sound/` linked by both clients, OpenAL Soft and the decoders behind it. (a) The engines keep the `S_*` functions as the door, one header for both trees in place of the two `snd_public.h`: the 570 calls of the multiplayer's modules, the 596 of the single-player games and the import tables keep their shape; the module inside follows the C++ Core Guidelines entirely with clang-tidy at zero, as netcodec (GUIDELINES section 5). (b) A new interface and every call rewritten.
2. **The two copies' differences** (section 2). (a) Every feature in both trees (Doppler, `S_MuteSound`, `S_StopLoopingSound`, `S_Shutup`, a channel per loop), one `soundChannel_t` in `src/shared/qcommon` (the union, `CHAN_MENU1` included; the data names the channels, so no file changes), and the values each game tuned (the lip sync's thresholds, `CHAN_LESS_ATTEN`'s radius) given by each engine. (b) The same, with the multiplayer's values for both. And `CHAN_VOICE_GLOBAL`: (c) centred, the single player's; (d) panned at full volume, the multiplayer's.
3. **The parts.** (a) 1: the libraries vendored and built on the three compilers, the module's decoders and their tests, no change to the game; 2: the multiplayer on the module (the device, the voices and their channel rules, the loops, 3D, the music with the dynamic music, the ambient sets, the cinematics, the lip sync, the focus, the default device, the video recorder), its old code, settings and menu items removed; 3: both single-player games on the module, their old code and menu items removed; 4: the reverb's zones; 5: the measures (CPU at 100, 125, 144 with vsync and 333 frames per second, the latency) and the listening on Windows and Linux. (b) Parts 2 and 3 as one, since the copies are nearly the same code.
4. **The libraries in `libs/`** (to approve; already downloaded into `build/audiolibs/`): OpenAL Soft 1.25.2 (LGPL 2 or later) as a static library without its utilities, examples, tests and install, without the legacy EAX extension (the reverb goes through EFX), without its embedded HRTF data (160 KB, HRTF off), with WASAPI on Windows and PipeWire, PulseAudio and ALSA on Linux, the other backends off; dr_mp3 and dr_wav (dr_libs, public domain or MIT-0, at the survey's commit); libFLAC 1.5.0 (BSD-3, without its encoder and without Ogg FLAC, so without libogg); stb_vorbis 1.24 into `libs/stb`. OpenAL Soft carries its own copies of {fmt} 11.2.0 and gsl: (a) kept inside it, unchanged (two {fmt} versions in the client, in different versioned namespaces, so no clash); (b) OpenAL Soft patched onto the project's {fmt} 12.2.1 (one copy, a second patch to carry at each update).
5. **The Linux backends' headers.** OpenAL Soft compiles each backend against the sound server's headers and at run time opens the player's own library (`libpipewire-0.3.so.0`, `libpulse.so.0`, `libasound.so.2`) with `dlopen`: nothing is linked, the headers only describe the functions. (a) The three header sets vendored in `libs/` at pinned versions (PipeWire with SPA under MIT, the PulseAudio and alsa-lib headers under the LGPL 2.1 or later), OpenAL Soft's detection given their folders: nothing to install, GUIDELINES section 9 kept whole; three folders more to update, a download, the size to measure. (b) The distribution's development packages (`libpipewire-0.3-dev`, `libpulse-dev`, `libasound2-dev` on Ubuntu 22.04, to verify), listed in the README as SDL 2's and OpenGL's are, an exception written in GUIDELINES section 9; what the plan wrote on 19 November 2026 and OpenAL Soft's own build expects.
6. **The reverb's zones.** The engine's EAX path did each frame: the listener's environment found in the zones' BSP, it and the two nearest neighbouring environments through their apertures each in an effect slot, the neighbours panned towards their aperture, every source sent to its own environment's slot. Where the data comes from: (a) converted once by a tool outside the game (EalMan, LGPL, not shipped) into a text file of this game per map (the environments as EFX "EAX reverb" values, the zones' planes, the apertures) in `modernjk.pk3`, as decided on 15 November 2026: the files derive from the game's data, as the changed menus do; (b) the `.eal` read when the map loads, from the player's own archives, by EalMan vendored in `libs/` (LGPL 2.1, about 2,500 lines of C++11, no release, so a pinned commit): no file shipped, Jedi Outcast's files read if they exist. What it plays: (c) the engine's behaviour as above (three effect slots: 1.15 % of a core for 32 voices with the cubic resampler, 1.49 % with `bsinc24`, measured on 15 November 2026); (d) the listener's environment only, faded over a zone's edge, as Unity's reverb zones and dhewm3's per-area EFX do. Occlusion from the zones' geometry is in neither (EalMan does not compute it).
7. **The sounds in memory.** (a) Every sound decoded to 16-bit PCM when registered, at its own rate, the voices of `sound/chars/` included: at most 36 MB on `mp/ffa3` and 44 MB on `yavin1` (section 5), a level's load longer by about 0.3 ms per second of voice (`yavin1`'s 154 s of voices: about 45 ms); the lip sync reads the PCM at the voice's position; `s_soundpoolmegs`, the zone freeing the oldest sounds and the MP3 tag's volume (`mp3_calcvols`) removed, OpenAL Soft holding the buffers outside the zone and a level's sounds freed at the next level as today. (b) The voices kept as MP3 and decoded while they play (a decoder and a queue of buffers per playing voice): 13.6 MB less on `yavin1`, more code.
8. **The voices at once.** Today 32 channels, a new sound taking a channel by the channel rules. (a) 32, measured in part 2 (how often a fight fills them), the number decided then; (b) 64 now; (c) 128 now. OpenAL Soft's cost measured on 15 November 2026: 0.41 % of a core for 32 voices, 1.20 % for 96, without reverb.
9. **The settings removed with the old mixer**, besides `s_khz` and `s_UseOpenAL`: `s_mixahead`, `s_mixPreStep`, `s_sdlBits`, `s_sdlChannels`, `s_sdlDevSamps`, `s_sdlMixSamps` (the ring and the device of the old mixer), `s_separation` (OpenAL Soft pans), `s_threshold1` to `4` (constants, each game's values), `s_testsound` (a test tone of the old mixer), `s_soundpoolmegs` (with 7a). Kept: `s_volume`, `s_volumeVoice`, `s_musicvolume`, `s_language`, `s_allowDynamicMusic`, `s_initsound` (the tests' silent clients), `s_show` and `s_debugdynamic` (cheat-protected debug). (a) As listed; (b) another list.
10. **Doppler.** (a) OpenAL Soft's, for every moving sound in both trees, without a setting (continuous, up and down); (b) as today: the multiplayer's loops only, behind `s_doppler`.
11. **Muted without the focus.** Today the device is paused when the window loses the focus. (a) Always, without a setting; (b) a setting, on by default.
12. **The latency.** (a) The library's default: 3 periods of 512 frames, about 32 ms reported on Windows (15 November 2026), against about 70 ms estimated today; (b) a shorter period set by the engine, measured in part 2 (CPU, skips under load) before choosing.
13. **The video recorder** (multiplayer). OpenAL Soft cannot read back what a device plays. (a) While recording, the sound restarts on OpenAL Soft's loopback device (`ALC_SOFT_loopback`): each video frame renders exactly its samples into the AVI, nothing is heard while recording, and the sounds are loaded again when the recording starts and stops; (b) the recorder without sound.
14. **The module's tests.** (a) Small sample files committed (made by the reference encoders; MP3 from the MPEG conformance vectors if their licence allows), a decoding test with their hashes and a robustness test on damaged copies in `ctest`, as netcodec's, and libFuzzer targets in the fuzzer preset; the comparison on the game's files stays a tool run by hand (`tools/audiodec/`); (b) the libFuzzer targets only.
15. **Jedi Outcast's data** (its zone files, its menus): (a) built and left unchecked, as Jedi Outcast is (decided 19 November 2026: it only has to keep compiling); (b) its game data supplied and checked.

Decided on 23 November 2026: 1b, 3a, 4 (approved, each library at its newest version), 5b, 6a and 6c, 7a, 9a, 10a, 11b, 13a, 14a, 15b (section 1). Open: 2, 8 and 12 (section 7).

## 7. Open after the answers of 23 November 2026

- The scope: the whole sound system written again from what it must do, not a port of today's mixer onto OpenAL Soft: the complete inventory of what the game needs (every feature and every call: what it plays, where, how long, on which channel and why), the code made one for the trees, every feature tested, today's defects fixed, each with its before and after.
- Today's defects read in the code (23 November 2026), to show in game before they are fixed: the 32 channels are shared by every sound, the looping ones included, and when they are all busy a new sound takes the oldest channel whatever its loudness or distance (`S_PickChannel`), only the local player's sounds and the loops being spared; the looping sounds of one file are merged into one channel each frame (`S_AddLoopSounds`: several sabers' hums heard from one direction, their volumes summed and clipped); the loops are given channels again at every frame; the mixer paints 150 ms ahead at every frame and mixes in 16-bit integers with hard clipping, its resampling takes the nearest sample, its panning knows only left and right; the duplicates found in plan step 14 (a shot's sound started again at every frame of the muzzle flash, a looping sound added again in new slots) were fixed in cgame, and nothing in the mixer stops the same sound being started twice in a frame.
- Decision 2, on the new interface: one model for both trees in place of the channels: a sound played with a class (its bus, its attenuation, its priority) and, where one sound must replace another, a slot of its entity (the useful part of today's channel rules: a voice replacing the entity's previous voice); a looping sound started once with a handle, moved and stopped, instead of being added again at every frame; one attenuation per class and one lip sync for every game, tuned by ear and eye; `CHAN_VOICE_GLOBAL`'s voices not positioned. The modules (the multiplayer's import tables, the single-player games' numbered system calls) given one table of the sound's functions.
- Decision 8, the voices: what recent engines do: a limit of real voices mixed (64 by default in FMOD, `System::setSoftwareChannels`; 32 by default in Unreal Engine's Max Channels, by its users' reports and its log), many more virtual voices tracked without being mixed, the real ones chosen at every frame by audibility (Unity: "at every frame, the loudest voice is picked"; FMOD: by distance, volume, priority and occlusion), and limits per sound (Unreal Engine's Sound Concurrency: a maximum count, then the oldest or the farthest stopped, or the new one refused).
- Decision 12, the latency: FMOD's default is 4 buffers of 1,024 samples, about 53 ms on average at 48,000 Hz by its own documentation; Unity offers "best latency", "good latency" and "best performance" without stating sizes; OpenAL Soft's default, 3 periods of 512 frames, about 32 ms reported on Windows, is already below FMOD's.

Decided on 23 November 2026: the scope as above, decision 2 as proposed, 64 real voices (decision 8), OpenAL Soft's default latency for now (decision 12) (section 1).

## 8. Decided last (23 November 2026)

Both decided the same day, option a each (section 1).

- `valley.eal` (Jedi Outcast, section 4): one environment, "Valley", and a box that points to no zone. (a) The conversion tool gives the box its one environment, so that the map has the reverb its file names, as Jedi Academy's single-environment files have; (b) no reverb, as the file reads literally and as EalMan does. What the game did with it in 2002 (Creative's `EAXMan.dll` 3.0) is not known.
- The conversion tool (part 4), built on Windows and Linux: EalMan has no code of one system (its README: "cross-platform, architecture-independent"; `tools/audiobench/ealdump.cpp` builds with it). (a) In a `tools/` folder at the repository's root, a target of the project's presets built on demand, EalMan in `libs/ealman` at its pinned commit and built only for the tool (the game never links it); (b) in `docs/modernization/tools/`, with the survey's tools, EalMan downloaded into `build/` as for them.

## 9. Part 1: the libraries and the decoders (23 November 2026)

### 9.1 The libraries

Each at its newest version, checked on 23 November 2026, in `libs/` with a README (source, files kept and left out, build, licence, update):

| library | version | in `libs/` | built by |
|---|---|---|---|
| OpenAL Soft | 1.25.2 (12 May 2026), still its newest release; one patch for clang 23's libc++, fixed on its `master` since | `openal-soft/` (5.1 MB: the library, its own {fmt} 11.2.0 and GSL; without its programs, documents, tests and HRTF data) | its own CMake file, a static library: no programs, no legacy EAX extensions, no HRTF data, no C++ modules, no WaveFile backend; WASAPI required on Windows (DirectSound and WinMM off), PipeWire, PulseAudio, ALSA and RTKit required on Linux (OSS, Solaris, sndio, JACK, PortAudio off) |
| libFLAC | 1.5.0 (11 February 2025), still its newest | `flac/` (2.4 MB: the library and what its CMake file adds) | its own CMake file: the library alone, no Ogg FLAC, no hardening flags; the project's options (floating point, sanitizers) |
| dr_mp3, dr_wav | the commit `e381981a` of 19 November 2026 (unreleased v0.7.5 and v0.14.7; the tags are older than the fixes the survey measured) | `dr_libs/` (two headers) | compiled in `src/shared/sound/mp3.cpp` and `wav.cpp` |
| stb_vorbis | 1.25 (19 November 2026: "detect more bad files"), after the survey's 1.24 | `stb/stb_vorbis.c` | a static library of its own (`vendored::stb_vorbis`), no file or push-data functions; the project's options |

The newer dr_libs and stb_vorbis checked again on the game's files (`tools/audiodec/check_run.sh` and `crosscheck.py` against the survey's run, llvm-mingw clang): the same samples on every file (dr_mp3 12,902 MP3, dr_wav 302 WAV, stb_vorbis 453 Ogg Vorbis files), and no fault under AddressSanitizer on the damaged copies (6,402 MP3, 2,718 WAV, 969 FLAC, 1,221 Ogg Vorbis).

On Linux, RTKit (the mixing thread's real-time priority, through D-Bus, opened at run time) required as the backends are, so that a Linux binary does not depend on the build machine's packages: the development packages to install are `libpipewire-0.3-dev libpulse-dev libasound2-dev libdbus-1-dev pkg-config` (the repository's README).

### 9.2 The decoders (`src/shared/sound`)

- `decoder.h`: `sound::Pcm` (rate, channels, 16-bit samples interleaved), `sound::Decoder` (an interface: `Rate`, `Channels`, `Read` into a span of samples, `Seek` to a frame), `OpenDecoder` (the decoder of a file by its first bytes: `RIFF` and its other forms for WAV, `fLaC`, `OggS`, any other for MP3; nullptr when it does not read the file or the sound is neither mono nor stereo) and `Decode` (a whole file). The file stays in its caller's memory; the music will stream through `Decoder`, the sounds go through `Decode`.
- One file per format over its library (`wav.cpp`, `mp3.cpp`, `flac.cpp`, `vorbis.cpp`, declared in `formats.h`), each about 60 lines but FLAC's (its callbacks, 190 lines); `decoder.cpp` picks the format.
- The library `modernjk_sound`, built with netcodec's strict warnings and after the C++ Core Guidelines (GUIDELINES section 5); clang-tidy at zero under its own checks (`src/shared/sound/.clang-tidy`, netcodec's set), its findings errors in the clang-tidy preset; four lines carry a `NOLINT` with their reason, each forced by a library's interface (libFLAC's metadata union, stb_vorbis's `.c` header and its `unsigned char` bytes) or by the 24-bit conversion's arithmetic shift.
- Built for the engines that play the sound and for the fuzzers (`src/shared/CMakeLists.txt`); nothing in the game uses it yet.
- Every function carries a comment saying what it does (GUIDELINES section 10), and a blank line stands before and after each `if`/`else` chain, loop and `switch` (GUIDELINES section 3; clang-format 23 has no option for it, checked the same day), in the module and its tests.
- A sound decoded whole by `Decode` lasts at most ten minutes, longer refused (section 9.4); stb_vorbis works in a block of 1 MB the decoder gives it (section 9.4); everything is decoded to 16 bits: the game has no file above 16 bits, and OpenAL Soft mixes in 32-bit floats whatever the buffers hold (asked on 23 November 2026: a file above 16 bits, which a mod could bring, loses its lowest bits, about -96 dB; decoding such files to floats, which OpenAL Soft takes through `AL_EXT_FLOAT32`, offered and not decided).

### 9.3 The tests

- `modernjk.sound` (`test/sound`, 14 tests in `ctest` now): the sample files of `test/sound/data` (12 files, 125 KB, binary in `.gitattributes`), written by `docs/modernization/tools/audiodec/make_test_samples.cpp` with the reference encoders (libFLAC level 5, libvorbis quality 4, LAME 4.0 of 11 July 2026, its newest release, downloaded for it into `build/audiolibs/dl-lame`, SHA-256 `3df5124d...216eb` as Homebrew's formula gives it; the generator writes the same files again): one synthetic signal (two tones, a sweep, a little noise) as WAV (16-bit mono at 22,050 Hz, 8-bit mono at 11,025 Hz, 16-bit and 24-bit stereo at 44,100 Hz), FLAC, Ogg Vorbis and MP3 (MPEG-2 mono at 64 kbit/s, MPEG-1 joint stereo at 128 kbit/s, with LAME's Info tag), eleven minutes of silence in FLAC (15 KB), and the fuzzer's find below. Checked: each WAV's rate, channels, frames and the hash of its samples (those the generator printed); each FLAC equal to the WAV of the same signal (24-bit brought to 16 as dr_wav does); each Ogg Vorbis and MP3 with the source's frames (dr_mp3 drops the encoder's delay and padding that the Info tag gives) and a signal to noise ratio above its threshold (Ogg Vorbis 28.8 dB mono and 18.5 dB stereo, MP3 23.1 and 22.8 dB, bounded by the signal's noise, which the encoders code loosely; no hash, since stb_vorbis's samples differ by one step with MSVC); the MP3 behind an ID3v2 tag giving the same samples; a seek to a third of each file against the whole decoding; files cut inside their header refused; the fuzzer's find refused; the silence refused by `Decode` (beyond ten minutes) and read to its end as a stream; 1,800 damaged copies decoded whole and after a seek. Then OpenAL Soft on its loopback device (no sound card): HRTF off when asked off, `bsinc24` among its resamplers (its name is "23rd order Sinc"), the extensions the sound needs (`ALC_SOFT_loopback`, `ALC_SOFT_system_events`, `ALC_SOFT_reopen_device`, `ALC_EXT_EFX`, `AL_SOFT_source_resampler`), a decoded sound played (its offset advancing, the output not silent, the source stopped after its end), an EAX reverb in an effect slot. 0.2 s in Release.
- The MP3 samples, first eight silent frames written by the test (no MP3 encoder on the development machine, the licence of the MPEG conformance vectors not stated), are LAME's since the same day (approved on 23 November 2026).
- `sound_fuzz_decode` (`src/shared/sound/fuzz`), a libFuzzer target on `Decode`, `OpenDecoder`, `Seek` and `Read`, the module and the two libraries built apart instrumented; the fuzzers' preset renamed `windows-clang-fuzz` (it was `windows-clang-netcodec-fuzz`), building netcodec's and the sound's fuzzers.

### 9.4 Found on the way

- UndefinedBehaviorSanitizer (the clang AddressSanitizer preset) stopped the robustness test in libFLAC (`src/libFLAC/lpc.c`: a signed integer overflow in `FLAC__lpc_restore_signal` on a damaged copy). libFLAC's own position, written there: on bogus input these sums overflow unavoidably, in the audio path only, without a security consequence, and its fuzzing on OSS-Fuzz silences them. So libFLAC is built without that one check (`-fno-sanitize=signed-integer-overflow`) when UndefinedBehaviorSanitizer is on; its other checks stay, and AddressSanitizer covers it. The survey had run AddressSanitizer only.
- The {fmt} that OpenAL Soft carries lists its `ChangeLog.md` among its sources: kept.
- libFLAC's CMake file defines `FLAC__OVERFLOW_DETECT` in Debug and RelWithDebInfo (the AddressSanitizer and fuzzers' presets): libFLAC then writes a line on the standard error at every overflow of a damaged file, 361,000 lines for one input of the fuzzer, which took 16 s; undefined again on its target (`-U`, after its definitions on the command line).
- The first run of `sound_fuzz_decode` (23 November 2026): about 1,900 inputs in a minute (32 per second, slowed by those lines), no fault, the one slow input above. The second, with the flag undefined, found in 7,500 inputs (about 1,000 per second) an Ogg Vorbis file of 422 bytes, a damaged copy of `mono22k.ogg`, whose vendor text declares 1.7 GB: stb_vorbis 1.25 allocates that length as the file gives it, then reads it byte by byte (`start_decoder`, the comment header), so that such a file in a downloaded pk3 would freeze or stop a client. Fixed as stb_vorbis's documentation offers: all its memory from a block of 1 MB the decoder gives it (the survey's 453 files need at most 202 KB, measured the same day; its author's about 150 KB), a file that asks for more failing to open; the file kept as `test/sound/data/vorbis_vendor_length.ogg`. The same kind of growth through `Decode` closed too: a FLAC frame of one constant value gives 65,535 samples for a few bytes, so a sound decoded whole lasts at most ten minutes (the game's longest sound played whole, `music/cinematic_1.mp3`, about 52 s; its longest music about 3.6 minutes), longer refused; the music streams through `Decoder`, without that bound. The third run, with both fixes, 10 minutes: 7,859 inputs, no fault, no memory overrun, but 13 inputs per second, slowed by the eleven minutes of silence in the starting corpus, each mutation of which decodes up to ten minutes under AddressSanitizer: that file is left out of the fuzzer's starting corpus (`fuzz_decode.cpp`). The fourth run, without it, 10 minutes: 438,948 inputs (730 per second), 2,143 edges covered (1,760 in the third), no fault, no memory overrun, no slow input.

### 9.5 The first VM validations (23 November 2026)

- `validate.sh`: GCC 15 Release failed at the link of `modernjk_test_sound` (libFLAC's CMake file turns `-Winline` on; link-time optimization repeats GCC's inlining reports at the link, where the project's `-Werror` applies): `-Wno-inline` on libFLAC. Clang 23 Release and AddressSanitizer failed in OpenAL Soft's `alc/alu.cpp` (`-Werror=function-effects`, which 1.25.2 sets with clang 20 and later; with GCC's standard library `std::variant::emplace` is not marked non-blocking): OpenAL Soft's own two fixes of 13 May 2026 taken as patches (`libs/openal-soft/patches/0002`, `0003`). GCC Debug, AddressSanitizer, the server and the Debug link-time check passed; the formatting check passed.
- `validate.bat`: MSVC ALL OK, with 4,026 warnings, all in OpenAL Soft's own code: C4875 from its GSL (4,023) and C4267 in `core/voice.cpp` (3). With GCC, OpenAL Soft also gave `-Wrange-loop-construct` (4) and `-Wmaybe-uninitialized` (3, in libstdc++). Each switched off on its target with its reason (GUIDELINES section 8; `Dependencies.cmake`). The project's code gave none on the three compilers.
- The second VM validations, the same day: ALL OK without a warning (`validate.sh`: GCC 15 and Clang 23; `validate.bat`: MSVC), `modernjk.sound` passed in every preset, OpenAL Soft built with PipeWire, PulseAudio and ALSA on Linux and WASAPI on Windows. Part 1 done.

## 10. Where to resume (23 November 2026)

### 10.1 The state

- Part 1 done, checked on the development machine: the four clang presets (Release, Debug, AddressSanitizer, fuzzers) without a warning; `ctest` 14 of 14 in Release, Debug and AddressSanitizer; clang-tidy at zero on the module; clang-format clean; four fuzzing runs (section 9.4); `golden: SAME` (the 302 scenarios in 2 min 42 s, the server and the game module not touched); the VM validations ALL OK without a warning after the fixes of section 9.5 (the Linux VM needs the sound servers' development packages: `sudo apt install libpipewire-0.3-dev libpulse-dev libasound2-dev libdbus-1-dev pkg-config`): part 1 done and committed. Next: part 2, for every tree (section 1).
- Nothing in the game uses the module yet: the game still plays its sound through the old mixer, `mp3code` and SDL 2.
- Part 2, 23 November 2026: the inventory (section 11), the sound log and the defects shown (section 11.9), the interface and every decision (section 12) done; no code written for it yet (asked: the code once asked). The documents and `docs/modernization/tools/soundinv/` were not committed at the end of that session: `git status` first.
- Decided on 23 November 2026 (section 1): the files above 16 bits decoded to 16 bits like the others; the video recorder removed, the single player's `cl_avidemo` with it; RTKit required on Linux like the backends (OpenAL Soft asks the system a real-time priority for its mixing thread, first directly, then through RTKit, the D-Bus service that grants it to a program without privileges; the build needs D-Bus's headers, the player's `libdbus-1` is opened at run time; without it the thread runs at the normal priority, more exposed to gaps when the machine is loaded); plan step 29 at the very end of the plan. Nothing open.
- The folder `build/windows-clang-netcodec-fuzz`, the fuzzers' preset under its old name, can be deleted.

### 10.2 What is where (outside git, in `build/audiolibs/`, regenerable)

| folder | what |
|---|---|
| `openal-soft/` | OpenAL Soft's git clone at the tag `1.25.2` with the patch applied in its working tree; `libs/openal-soft` was exported from it with `git archive` |
| `decoders/` | the survey's downloads (`dl/`, with `SHA256SUMS.txt`), sources (`src/`), libraries (`prefix/`, `prefix-asan/`: libogg, libvorbis, libFLAC, libmpg123, libopus), corpora (`data/*.bin`: the game's MP3 and WAV, the FLAC, Ogg Vorbis and Opus sample, the damaged copies) and runs (`work/check/<compiler>/`: the lists `crosscheck.py` compares) |
| `decoders-2026-11-23/` | the decoders of part 1 (`src/dr_libs`, `src/stb`) and the benchmark built on them: `bench-rel`, `bench-asan` (`CHOSEN_ONLY`), `bench-full` (with `make_test_samples` and LAME), `check/` (the run of section 9.1) |
| `dl-2026-11-23/` | dr_mp3, dr_wav and stb_vorbis as downloaded on 23 November 2026 |
| `dl-lame/` | LAME 4.0's archive and its sources (`src/lame-4.0`), for the MP3 samples only |
| `ealman/` | EalMan at its commit `aa69e586` (part 4 vendors it in `libs/ealman`) |
| `miniaudio/`, `build-openal/`, `bin/` | the prototypes of the library survey (`tools/audiobench`) |

Outside `build/`: the worktree `E:\Git\jka\ModernJK_soundref` (detached at the commit of part 1, `soundlog.patch` applied, its build in `build/windows-clang-release`): the old sound with its log (section 11.9); the session logs in `%TEMP%\soundlog\<session>\base\qconsole.log`.

### 10.3 The commands (Git Bash, llvm-mingw, CMake and Ninja in the `PATH` as in the progress page's "How to resume")

- The test samples again (they come out the same): `cmake -S docs/modernization/tools/audiodec -B build/audiolibs/decoders-2026-11-23/bench-full -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DDEC=build/audiolibs/decoders -DREPO=. -DLIBS_PREFIX=build/audiolibs/decoders/prefix -DMINIAUDIO=build/audiolibs/miniaudio -DLAME=build/audiolibs/dl-lame/src/lame-4.0`, `cmake --build <that folder> --target make_test_samples`, then `make_test_samples.exe test/sound/data` (it prints the WAV hashes of `test/sound/decoder.cpp`).
- The decoders on the game's files: a benchmark folder configured as above with `-DCHOSEN_ONLY=ON` (and `-DASAN=ON -DLIBS_PREFIX=build/audiolibs/decoders/prefix-asan` for the damaged copies), `-DDEC` a folder whose `src/` holds `dr_libs`, `stb` and `minimp3`; then `sh docs/modernization/tools/audiodec/check_run.sh build/audiolibs/decoders <benchmark> <out> <AddressSanitizer benchmark>` and `python -I docs/modernization/tools/audiodec/crosscheck.py build/audiolibs/decoders/work/check/windows-clang <out>`.
- The memory stb_vorbis needs: `docs/modernization/tools/audiodec/vorbis_memory.cpp` (`clang++ -O2 -std=c++20 -Ilibs/stb`), on `build/audiolibs/decoders/data/sample_vorbis.bin`.
- The fuzzer: `cmake --workflow --preset windows-clang-fuzz`, the files of `test/sound/data` but `silence_11min.flac` copied into a work folder, then `build/windows-clang-fuzz/bin/sound_fuzz_decode.exe -max_total_time=600 -artifact_prefix=<folder>/ <work folder>`.
- The zone files: `docs/modernization/tools/audiobench/ealtree.py` (the RIFF chunks of `.eal` files) and `ealgema.py` (their environments' and sources' names, the header of their zones' BSP), `python -I` on files extracted from the archives; `ealdump` (`tools/audiobench/build.sh`) reads them through EalMan.

- The inventory of the calls: `python -I docs/modernization/tools/soundinv/soundinv.py . build/soundinv/calls.tsv build/soundinv/symbols.tsv` (every call of the sound in the three trees, with its arguments, live or commented; section 11.1).
- The sound log of the old mixer: the worktree `E:\Git\jka\ModernJK_soundref` (`git worktree add --detach <folder> <the commit of part 1>`, then `git apply docs/modernization/tools/soundinv/soundlog.patch` there), built with `cmake --preset windows-clang-release` and `cmake --build --preset windows-clang-release --target modernjk cgame jampgame ui rd-vanilla` (the renderer too: without it the client loads another build's and crashes); then `bash docs/modernization/tools/soundinv/soundlog_session.sh <its bin folder> <work folder> <scenarios|freeplay|crowd> [frame rate]` (a silent client, about 1.5 to 3 minutes) and `python -I docs/modernization/tools/soundinv/soundlog.py <work folder>/base/qconsole.log`. The same sessions with the new system's build and `s_show 1` give the after (decision 10a).

### 10.4 Part 2 starts with

1. ✅ (section 11) The complete inventory of what the game asks of the sound, before any design: every call (the multiplayer's modules about 570, Jedi Academy's 329, Jedi Outcast's 267, the engines', section 3) classified by what it plays (the sound's class: weapon, voice, footstep, ambience, interface, music...), where (an entity, a point, the listener), how (once, looping, streamed), on which channel and why, and every feature of sections 2 and 3 (the ambient sets and local sets, the brush models' sounds, the dynamic music and its states, the cinematics and their sound files, the lip sync and the single player's `VoiceVolume` in its logic and its saved games, the video recorder, the focus, the languages of the voices); written in this page, with what can be made one.
2. ✅ (section 11.9) A sound log for the scripted client sessions (silent clients: `s_volume 0` and `s_musicvolume 0`): every start and stop with its time, entity, class and the voices in use, real or virtual, so that the old and the new systems are compared on the same session.
3. ✅ (section 11.9: every defect shown but the debris, which no map of the sessions has) Today's defects (section 7) shown in game before they are fixed, with what a player hears before and after (GUIDELINES section 14): a fight that fills the 32 channels, several sabers' hums merged into one direction.
4. ✅ (section 12, every decision taken on 23 November 2026) The interface designed from the inventory and decided before it is written (decision 2: one entry point, the classes and the entity's slots, the handles of the looping sounds, one table of functions for every module, the single player's numbered system calls included), with the 64 real voices chosen by audibility and the limits per sound (decision 8).

### 10.5 Next: the code of part 2

Everything is decided (sections 1 and 12); what part 2 writes, in this order, each step built with clang without a warning before the next:

1. The module (`src/shared/sound`, GUIDELINES sections 3, 5, 10): the device (OpenAL Soft as `test/sound/openal.cpp` proves it: HRTF off, `bsinc24`, the default device followed, `s_muteWhenUnfocused`), the registered sounds (decoded whole, decision 7a), the voices (64 real chosen by audibility and class priority, the others virtual with their position advancing, four per file, decisions 4a and 5), the classes and the slots (section 12.1), the attenuation per class (straight line, `AL_EXT_source_distance_model`), Doppler at 12,000 units per second, the listener, the voice's state for the lip sync (read from the PCM at `AL_SAMPLE_OFFSET`), the music and the dynamic music through `sound::Decoder`, the cinematics' stream, the ambient sets, the volumes (`s_volume` the master), the log (`s_show 1`, the `SNDLOG` lines of section 11.9); its tests in `test/sound` (`modernjk.sound`) on the loopback device.
2. The engines: both `client/` folders and `src/shared/sdl` onto the module (the frame, the registration, the cinematics, the focus); the table of the sound's functions in every module's import (the single player's numbered system calls of the sound removed).
3. The modules: every call of section 11 rewritten, the commented ones too (the list: `soundinv.py`), each given its class, slot and loop handle; the data's channel names turned into classes where they are parsed; the multiplayer's events carrying the class and the slot in place of the channel (decision 8a); the shots as in the single player (7a); the HUD's cues as interface sounds (the rocket's lock, "no force"); the voice's state in JA's game and its saved games (12b).
4. Removed: the old mixers (`client/snd_*.cpp` of both engines), `snd_mp3.cpp`, `cl_mp3.cpp`, both `mp3code`, `sdl_sound.cpp`, the video recorder (section 1), the settings of decision 9 with `s_doppler`, and the menus' items of `s_khz` and `s_UseOpenAL` (copies of `ui/jamp/setup.menu`, `ingame_setup.menu`, `ui/setup.menu`, `ui/ingamesetup.menu` changed in `assets/`, and the UIs' code that drives them); `libs/stb/README.md` (the video's frames) and the repository's README (SDL no longer plays the sound) brought up to date.
5. The proof: the three compilers without a warning, `ctest`, `golden: SAME`, the sessions of section 11.9 again with the new build (each defect of sections 7 and 11.7 before and after, GUIDELINES section 14), the single player's game played and a saved game read, CHANGES.md (one table before and now for the fixes), the VM validations; the CPU at 100, 125, 144 with vsync and 333 frames per second belongs to part 5.

## 11. Part 2: the inventory of what the game asks of the sound (23 November 2026)

Every tree, since parts 2 and 3 were made one (section 1). Read in the code of the three trees; the references are to `src/codemp` (MP), `src/code` (JA; the single-player engine of both games) and `src/codeJK2` (JO) unless stated.

### 11.1 How the calls were counted

`docs/modernization/tools/soundinv/soundinv.py <repository> <calls.tsv> [<symbols.tsv>]` lists every call of the sound outside the sound's own files (`client/snd_*.cpp`, `cl_mp3.cpp`, `null_snddma.cpp`, `src/shared/sdl/sdl_sound.cpp`): the modules' entries (`trap->S_*`, `trap->AS_*`, `cgi_S_*`, `cgi_AS_*`, the UIs' `DC->` sound entries), the game modules' helpers that send a sound (`G_Sound`, `G_SoundOnEnt`, `G_EntitySound`, `G_MuteSound`...) and the engines' calls of the functions the sound files define, each with its file, line, function and arguments, and whether it is live, in a comment or in an `#if 0` block.

| calls (live, then in comments) | MP | JA | JO |
|---|---|---|---|
| the modules' entries (cgame, UI; the single player's UI is in its engine) | 516; 56, 3 in `#if 0` | 330; 41 | 242; 34 |
| the game modules' helpers | 252; 40 | 303; 45 | 194; 26 |

The counts of section 3 (570, 329, 267) were a text search that counted the declarations and the comments too. The commented-out calls are rewritten with the live ones (GUIDELINES section 10).

| entry | live calls in the modules: MP, JA, JO |
|---|---|
| `S_RegisterSound` | 314, 193 (and 13 in the UI), 147 |
| `S_StartSound` | 112, 72, 65 |
| `S_StartLocalSound` | 32 (and 3 in the UI), 2 (and 15 in the UI), 1 |
| `S_AddLoopingSound` | 15, 12, 12 |
| `S_UpdateEntityPosition` | 4, 7, 7 |
| `S_MuteSound`, `S_Shutup` | 6 and 6, none, none |
| `S_StartBackgroundTrack`, `S_StopBackgroundTrack` | 3 and 0 (and 2 and 1 in the UI), 3, 1 |
| `S_ClearLoopingSounds`, `S_Respatialize`, `S_UpdateAmbientSet` | 3, 1, 1 in each tree (JA and JO: 1, 1, 1) |
| `AS_ParseSets`, `AS_AddPrecacheEntry`, `AS_GetBModelSound`, `S_AddLocalSet` | 1, 3, 4, 1; 1, 2, 1, 1; 1, 2, 1, 1 |
| `S_GetVoiceVolume` (MP), `S_GetSampleLength` (JA, JO), `S_StopSounds` (JA's game) | 1; 1 and 1; 1 |

### 11.2 The engines' entries today

| entry | what it does | MP callers | single-player callers |
|---|---|---|---|
| `S_Init`, `S_Shutdown`, `S_BeginRegistration`, `S_DisableSounds`, `S_ReloadAllUsedSounds` (JA) | start, stop, a level's registration, stop before a video restart, reload after one | `cl_main.cpp` | `cl_main.cpp` |
| `S_Update` | the frame's mix: the listener, the loops, the music, the lip sync, the device's ring | `cl_main.cpp`, `cl_cin.cpp` | the same |
| `S_RegisterSound` | a sound loaded and given a handle (the default sound's 0 when missing) | cgame, UI, `FxSystem.h` | cgame and game, the engine's UI, `cl_cin.cpp` (a cinematic's sound file) |
| `S_StartSound` | a sound on an entity's channel, at a point (`origin`) or following the entity (`nullptr`) | cgame, the game's vehicle code compiled in cgame, `FxSystem.h` | cgame, game (`G_SoundOnEnt`), `FxSystem.cpp` |
| `S_StartLocalSound` | `S_StartSound` on the listener's entity: not positioned | cgame, UI, `FxSystem.h` | cgame, the engine's UI, `cl_cin.cpp` |
| `S_AddLoopingSound` | a looping sound added for this frame only; JA and JO add a channel, which sets its attenuation | cgame | cgame |
| `S_ClearLoopingSounds` | the frame's loops forgotten, before cgame adds them again | cgame (each frame) | cgame |
| `S_StopLoopingSound` (MP) | an entity's loops removed | none (its one call is commented out, `cgame/cg_ents.cpp:221`) | none |
| `S_StartLocalLoopingSound` | a loop on the listener | none | the UI's import, never called |
| `S_MuteSound` (MP) | the sound on an entity's channel cut | cgame: the weapon's channel at a weapon change, the force powers' sounds (`EV_MUTE_SOUND`) | none |
| `S_UpdateEntityPosition` | where the sounds that follow an entity are heard; cgame gives it every entity at every frame | cgame | cgame, game |
| `S_Respatialize` | the listener: its entity, position, axes, under water | cgame (each frame) | cgame |
| `S_StartBackgroundTrack`, `S_StopBackgroundTrack`, `S_RestartMusic`, `S_UnCacheDynamicMusic`, `Music_SetLevelName` (JA) | the music: an intro and a loop streamed from MP3, or the level's dynamic music | cgame, UI, the renderer's import (`S_RestartMusic`) | cgame, `sv_init.cpp` (the level's name for the dynamic music) |
| `S_RawSamples`, `S_CIN_StopSound` (JA) | raw samples queued (the cinematics' sound at 22,050 Hz), a cinematic's sound file stopped | `cl_cin.cpp` | `cl_cin.cpp` |
| `S_StopSounds`, `S_StopAllSounds`, `S_ClearSoundBuffer` | every sound stopped (with the music for the second); the ring cleared before a file is read | `cl_main.cpp`, `cl_keys.cpp`, `cl_scrn.cpp`, `cl_cin.cpp`; `S_ClearSoundBuffer` from the file system (`src/shared/qcommon/files.cpp`, 4 calls) | the same, and JA's game (`target_level_change`) |
| `S_GetSampleLengthInMilliSeconds` | a sound's length | none | cgame: how long a caption stays (`cg_text.cpp:261`) |
| `S_GetVoiceVolume` (MP), `s_entityWavVol` (JA, JO) | the loudness of an entity's voice: -1 talking but silent, 0 not talking, 1 to 4 by the lip sync's thresholds | cgame: the mouth (`cg_players.cpp:3700`) | the game's `gi.VoiceVolume`, a pointer to the mixer's array (section 2) |
| `S_Shutup` (MP) | no warning for a missing sound while models load | cgame | none |
| `AS_ParseSets`, `AS_AddPrecacheEntry`, `S_UpdateAmbientSet`, `S_AddLocalSet`, `AS_GetBModelSound` | the ambient sets of `sound/sound.txt`: parsed for the level's sets, the general set around the listener each frame, a local set around an entity, a brush model's sound by stage (start, middle, end) | cgame | cgame |
| `SND_FreeOldestSound`, `SND_RegisterAudio_LevelLoadEnd` | memory: the oldest sounds freed when the zone is full, the level's unused sounds freed at its end | `z_memman_pc.cpp`, the renderer's import | the same |
| `SNDDMA_Activate` | the device paused or resumed with the window's focus | `src/shared/sdl/sdl_input.cpp`, for both engines | |
| `S_FileExists` (JA) | a file's existence | none | the single-player renderer's `FS_FileExists` (`cl_main.cpp:1167`) |
| commands | `play`, `music`, `stopmusic`, `soundlist`, `soundinfo`, `soundstop`, `mp3_calcvols`, `s_dynamic` | both | both |
| the video recorder (MP) | the mixed samples written with each video frame, the mixer's clock following the video's frame rate | `snd_mix.cpp:84`, `snd_dma.cpp:1886` | none |

### 11.3 The channels today

Both engines: 32 channels for every sound (`MAX_CHANNELS`, `src/shared/qcommon/q_sound.h`), the loops included; 32 loops a frame in MP, 64 in JA and JO, the others dropped silently.

| channel | attenuation (full volume up to, silent from; a straight line between) | replaces | used for |
|---|---|---|---|
| `CHAN_AUTO` | 256 and 1,506 units | nothing: the first free channel, or the oldest | most sounds |
| `CHAN_LESS_ATTEN` | MP 2,048 and 3,298; JA and JO 1,280 and 2,530 | nothing, as `CHAN_AUTO` | vehicles, loud entities (`EF_LESS_ATTEN` in JA's loops) |
| `CHAN_WEAPON`, `CHAN_ITEM`, `CHAN_BODY` | 256 and 1,506 | the entity's sound on the same channel | the shot, the charge and the zoom; speakers; footsteps, debris |
| `CHAN_VOICE` | 768 and 2,018 | the entity's voice on any voice channel | characters' voices; mouth animated |
| `CHAN_VOICE_ATTEN` | 346 and 596 (`VOICE_ATTENUATE`, five times steeper) | the same | quiet dialogue (JA, JO scripts) |
| `CHAN_VOICE_GLOBAL` | none: full volume everywhere; MP panned, JA and JO centred | the same | broadcast dialogue (scripts) |
| `CHAN_LOCAL`, `CHAN_LOCAL_SOUND`, `CHAN_ANNOUNCER`, `CHAN_MENU1` | meant for the listener: through `S_StartLocalSound` on its entity, never attenuated; started on another entity (MP's "no force" on entity 0, section 11.7) attenuated as `CHAN_AUTO`, but JA's and JO's `CHAN_ANNOUNCER`, centred at full volume | the listener's sound on the same channel | menus, chat, announcer, radio voices |
| `CHAN_AMBIENT` | 256 and 1,506 | as `CHAN_AUTO` | the ambient sets' one-shots (`S_StartAmbientSound`) |
| `CHAN_MUSIC` | never used | | |

Rules beside the table (`S_PickChannel`, `S_StartSound`, `S_Respatialize`, `snd_dma.cpp:672`, `:920`, `:1690`): the listener's own sounds are never attenuated nor panned, at 0.75 of full volume on the channels below `CHAN_AMBIENT` and at full volume above it; no other entity's sound takes a channel holding a sound of the listener; no sound takes a loop's channel; a sound that is not a voice and is silent at a frame (out of range) is stopped for good, even if the listener comes back while it would still play; a voice keeps playing out of range so that the scripts waiting for its end keep their timing; the voices' volume is `s_volumeVoice`.

### 11.4 What the modules play

Grouped by feature; the calls are the live calls of the modules, MP / JA / JO. Where: E an entity's sound that follows it, P a fixed point, L the listener (not positioned). How: 1 once, loop (added again at every frame), stream.

| feature | calls | the sound | where, how | channel today, and why |
|---|---|---|---|---|
| shots | MP `CG_FireWeapon` 2, the muzzle effects' sounds (`FxSystem`); JA and JO the muzzle effects' sounds only | the weapon's shot | MP E and P (the effect's sound at its point, entity none), 1 | `CHAN_WEAPON` (the shot cuts the charge), the effects' `CHAN_AUTO` |
| charges and zooms | MP 3 (`EV_WEAPON_CHARGE`, `_ALT`), the zoom loop 1 (`cg_view.cpp:1128`), zoom start and end 4; JA 1 + 4 (binoculars, light amplification goggles) | the charge, the disruptor's zoom loop played as a one-shot started again every 300 ms while the zoom moves | E; the zoom loop at the view's point, world entity | `CHAN_WEAPON` so that the shot replaces it; `CHAN_LOCAL` for the zoom loop (a one-shot restarted, not a loop) |
| weapon change | MP `S_MuteSound` 5 | the weapon's channel silenced | | `CHAN_WEAPON`, to cut a charge |
| weapon select, no ammo | MP 4, JA 3 + 1, JO the same | interface sounds of the weapon | E (the player), L for JA's no ammo | `CHAN_AUTO`; JA's `CHAN_LOCAL_SOUND` |
| rocket lock | MP 4, JA 2, JO 2 | the lock's ticks and tone | P: the target's position (section 11.7) | `CHAN_AUTO` |
| weapons' loops | MP 2 (firing, ready), JA 3 (+ the stop sound 1), JO the same | the weapon's hum and firing loop | E, loop | none (loops have no channel in MP) |
| missiles' loops | MP 4, JA 3, JO 3 | rocket, vehicles' shots | E, loop; MP gives their velocity (Doppler, section 11.6) | none |
| saber | MP 2 loops, 8 on and off, 3 swings, 1 spin, 3 hits, 1 block, 2 hits on walls; JA 2 loops (+ the thrown saber's 3), 4 on and off, 2 hits, swings from the animations' events; JO the same | hum, on, off, swing, hit, block, hitting the world | E (hum at the blade's point), P (hits), 1 and loop | `CHAN_AUTO`; swings `CHAN_WEAPON` (one swing at a time); hits on walls `CHAN_WEAPON` of entity -1 (no entity) |
| force powers | MP: no force 2, drain 1, team heal and regen 2, the powers' sounds sent by the game on tracking channels (`G_Sound` with `TRACK_CHANNEL_1` to `_5`, played on the channel the event carries, moved with `S_UpdateEntityPosition` 2, cut by `EV_MUTE_SOUND`); JA and JO: no force 2, push 2 | | E; MP's no force on entity 0 (section 11.7) | `CHAN_LOCAL`, `CHAN_AUTO`; MP's tracking: the voice channel, so that a new power's sound or a mute cuts it |
| voices of characters | MP: pain 1, death 1, jump 2, land 2, taunts 1, siege voice commands 2, the animations' and the game's (`EV_ENTITY_SOUND`, `EV_GENERAL_SOUND` with their channel, 4), `CG_TryPlayCustomSound` 1; JA and JO: through the game (`G_SoundOnEnt`, `G_SpeechEvent`), `CG_TryPlayCustomSound` 1 | the character's sound set (`*pain`, `*death`, `*jump`, `*taunt`, scripted dialogue) | E, 1 | `CHAN_VOICE` (one voice per entity, the mouth animated); the gasp out of water on `CHAN_AUTO` (MP); the radio copy of a teammate's voice command `CHAN_MENU1` on the listener |
| body | MP: footsteps 6 + 1, roll 1, falls 7, water in, out and under 3, teleport and respawn 4; JA: footsteps 4, landing 3, roll 1, lava and water 6; JO near JA | | E, 1 | `CHAN_BODY` for footsteps and roll (one footstep at a time per entity), the others `CHAN_AUTO` |
| the animations' sounds (`animevents.cfg`, `animsounds` in JO) | MP 2, JA 2, JO 2 | footsteps, swings, any sound the file names, on the channel it names | E, 1 | the data's channel |
| items | MP: pickup 2, holocron 1, seeker and medpack 2, jetpack loop 1, burning loop 1; JA: pickup 1 + the game's 1, battery 1 | | E, 1 and loop | `CHAN_AUTO` |
| world and debris | MP: glass 1, chunks 7, fragments 2, ragdoll 1; JA: chunks 8, glass 1, fragments 1, bounces 4, a missile sticking 1; JO near JA | | P (glass, fragments), E of the breaking entity (chunks) | chunks on `CHAN_BODY` of their entity (each cuts the previous), the others `CHAN_AUTO` |
| movers and brush models | MP: door and mover sounds 2, their loops 3 (`AS_GetBModelSound` 4); JA 1 + the loops; JO the same | start, middle (loop), end of a door or a platform, from its sound set | E, 1 and loop | `CHAN_AUTO` |
| entities' loops (`s.loopSound`) | MP 3, JA 1, JO 1 | any looping sound a map entity or the game sets | E, loop | none in MP; JA and JO `CHAN_AUTO` or `CHAN_LESS_ATTEN` by `EF_LESS_ATTEN` |
| speakers (`target_speaker` triggered) | 1 each | | E, 1 | `CHAN_ITEM` (one at a time per speaker) |
| ROFF notetracks | 1 each (JA also in the game) | a sound a scripted move names | E, 1 | `CHAN_BODY` |
| effects (`.efx` sound primitives) | `FxSystem` 2 each | any effect's sound | MP P (entity none), JA and JO E or P | MP `CHAN_AUTO`; JA and JO the channel the effect asks |
| vehicles | MP: fly-by and engine start 2, alarms of the radar 2, linked weapons 1, hyperspace 1 (game code compiled in cgame), no ammo 1; JA's vehicles' shots' loops (above) | | E, P (alarms at the threat), L | `CHAN_LESS_ATTEN` (heard from far), `CHAN_LOCAL` |
| announcer and rewards | MP: rewards 1, countdown 6, fight 3, denied and humiliation 2, minutes left 2, siege 2, buffered announcements 1, happy and failure music 2 | | L, 1 | `CHAN_ANNOUNCER` (one at a time); `CHAN_LOCAL` |
| chat and votes | MP 5 | the chat beep | L | `CHAN_LOCAL_SOUND` |
| menus | MP UI 9; JA UI 15 (the datapad, the force powers' choice, the characters' voices of the status screen, `CHAN_VOICE` on the listener) | | L | `CHAN_LOCAL` (one at a time), `CHAN_LOCAL_SOUND`, `CHAN_AUTO` |
| captions | JA 1, JO 1 | none: the caption's time from the voice's length | | |
| music | MP: the level's music 1, the duel's 2, the menus' 2; JA: the level's 1, the credits 2; JO 1 | intro and loop, the dynamic music's states | stream | |
| the frame | 1 `S_Respatialize`, 1 to 3 `S_ClearLoopingSounds`, 1 `S_UpdateAmbientSet`, the entities' positions (`CG_SetEntitySoundPosition`, 2), MP's tracked sounds (`CG_UpdateSoundTrackers`, 2) | | | |

### 11.5 What the game modules send

| | MP (server's game, events to every client in range) | JA and JO (the game calls its cgame directly) |
|---|---|---|
| a sound on an entity or a point | `G_Sound` 175 (`EV_GENERAL_SOUND`, its channel in the event: `CHAN_AUTO` 104, `CHAN_BODY` 29, `CHAN_WEAPON` 26, `CHAN_VOICE` 4, `CHAN_ITEM` 3, `CHAN_LOCAL` 1, tracking channels 7), `G_SoundAtLoc` 3, `G_SoundTempEntity` 3 | `G_Sound` (JA 133, JO 95: `EV_GENERAL_SOUND`, `CHAN_AUTO`), `G_SoundAtSpot` (4, 2), `G_SoundBroadcast` (1, 1: to every client) |
| a sound following an entity | `G_EntitySound` 21 (`CHAN_VOICE` 17), `G_SoundOnEnt` 28 (`EV_ENTITY_SOUND`) | `G_SoundOnEnt` (JA 138: `CHAN_AUTO` 35, `CHAN_VOICE` 31, `CHAN_BODY` 29, `CHAN_ITEM` 21, `CHAN_WEAPON` 14, `CHAN_VOICE_ATTEN` 4, `CHAN_LESS_ATTEN` 3; JO 95), `G_SoundIndexOnEnt` (JA 13) |
| a voice cut | `G_MuteSound` 21 (`EV_MUTE_SOUND`, broadcast) | JA 3 |
| a scripted sound (ICARUS's `sound` task) | `Q3_PlaySound` (`g_ICARUScb.cpp:386`): the script names its channel (`CHAN_VOICE`, `CHAN_VOICE_ATTEN`, `CHAN_VOICE_GLOBAL`, `CHAN_ANNOUNCER`...), the task ends with the voice | the same in `Q3_Interface.cpp`; the captions sent with it |
| speech events | `G_SpeechEvent` 1 | JA 7, JO 1 (an NPC's lines by event: anger, chase, cover...) |
| a looping sound | `s.loopSound` (a sound index) and `s.loopIsSoundset` (a brush model's set) on any entity | the same, with `EF_LESS_ATTEN` |
| the music | the configstring `CS_MUSIC` | the same, and the dynamic music's state (`CS_DYNAMIC_MUSIC_STATE`, explore, action, boss, death, silence) |
| the voices' loudness | none | read by the game's logic (section 2) and written in the saved games |

### 11.6 The features beside the calls

- **The ambient sets** (`snd_ambient.cpp`): `sound/sound.txt` defines sets (general sets, local sets, brush models' sets) of looping sounds and one-shots chosen at random within a time and a volume range; the general set plays around the listener, chosen by the configstring `CS_AMBIENT_SET`; a local set around an entity (`S_AddLocalSet`), faded with the distance; a brush model's set gives its start, middle and end sounds. Played through `S_AddAmbientLoopingSound` (the loops, a volume of the set) and `S_StartAmbientSound` (the one-shots, on `CHAN_AMBIENT`).
- **The music**: an intro then a loop, streamed from MP3, faded when it changes; the dynamic music (`ext_data/dms.dat`) gives a level its explore, action, boss and death tracks with transition points. In JA and JO the game drives its state (`CS_DYNAMIC_MUSIC_STATE`); in MP nothing does (`S_Music_GetRequestedState` returns nothing, `snd_dma.cpp:3109`): MP's dynamic music stays on its explore track, but for the `s_dynamic` command; `s_allowDynamicMusic`, `s_musicvolume`.
- **The cinematics**: the RoQ's sound through `S_RawSamples` at 22,050 Hz; in JA three cinematics play a sound file beside the RoQ (section 2).
- **The lip sync**: each frame, each playing voice's loudness at its position gives the entity a level from -1 to 4 (`S_CheckAmplitude`, the thresholds `s_threshold1` to `4`, section 2); MP's cgame reads it for the mouth, JA's and JO's game and cgame for the face, the breath, the NPCs' reactions and the saved games.
- **The languages**: `s_language` replaces `sound/chars/` by `sound/chr_d/`, `chr_f/` or `chr_e/` when a voice is loaded (German, French, Spanish), English when the file is missing (`snd_mem.cpp:581`).
- **The memory**: the sounds live in the zone (`s_soundpoolmegs`), the oldest freed when it is full, the level's unused ones freed at its end; the voices of `sound/chars/` kept as MP3.
- **The focus**: the device paused when the window loses the focus (`SNDDMA_Activate`).
- **The video recorder** (MP): section 11.2.
- **`S_Shutup`** (MP): the warnings of missing sounds hidden while models load.
- **Doppler** (asked on 23 November 2026, where it was): only in MP, only on looping sounds given a velocity, which cgame gives only to missiles (the rocket's loop, the vehicles' shots, a missile's `loopSound`: `cgame/cg_ents.cpp:2301`, `:2313`, `:2337`, `:2362`); none in JA and JO. Its formula is not Doppler's (`S_AddLoopingSound`, `snd_dma.cpp:1239`): the pitch is multiplied by the square of the ratio between the distance in a second and the distance now, divided by 100, applied only when above 1 and capped at 50. A missile coming towards the listener is never changed; one moving away is raised (where Doppler lowers it) as soon as it is within about a ninth of its speed: a rocket (900 units per second) leaving the listener within 100 units, so every rocket the player fires; logged (section 11.9): the shooter's rocket first heard 58 units away, 2.76 times higher (about an octave and a half, 18 semitones), back to its pitch at 100 units, about 70 ms at 125 and at 333 frames per second. The mixer steps through the samples at that rate without a filter, and merges the loops of one file into one channel with the first one's factor (`S_AddLoopSounds`). Real Doppler at 900 units per second lowers a receding rocket by about a semitone and raises an approaching one by about as much (the speed of sound about 12,000 units per second, taking the player's 64 units for about 1.8 m, so a unit for about 2.8 cm).

### 11.7 Defects found by the inventory (read in the code, to show in game before they are fixed)

Besides those of section 7:

| defect | where | what a player hears |
|---|---|---|
| the Doppler's formula (section 11.6) | `snd_dma.cpp:1239` | the player's own rocket starts with a short high chirp; a missile passing close chirps as it leaves |
| the rocket launcher's lock played at the target | `cgame/cg_draw.cpp:5124`, `:5240` (JA `cg_draw.cpp:2687`) | the lock's ticks and tone come from the target, faint or silent when it is far (silent beyond 1,506 units), though they are the shooter's cue |
| "no force" played on entity 0 in MP | `cgame/cg_draw.cpp:938`, `:1144` | entity 0 is the first player, not the listener (the code came from the single player, where the player is entity 0): every other player hears it from the first player's position, faint or not at all |
| a sound out of range at one frame is stopped for good | `S_Respatialize`, `snd_dma.cpp:1735` | a sound that starts far and whose source comes closer, or that the listener walks towards, stays silent |
| `S_MuteSound` takes a channel to mute | `snd_dma.cpp:892` | when all 32 channels are busy and the entity has no sound on that channel, it cuts the oldest sound of another entity |
| the debris of one entity on its `CHAN_BODY` | `cgame/cg_effects.cpp:770` to `:802` | each chunk's sound cuts the previous one of the same breaking object |
| every saber's hits on walls share one slot | `cgame/cg_players.cpp:5538`, `cgame/cg_event.cpp:2175` (entity -1, `CHAN_WEAPON`) | a blade's hit on a wall, or a clash's flare, anywhere in the level cuts the one playing elsewhere; a blade scraping a wall starts its sound again every 100 ms |
| the shots played twice in MP | `cgame/cg_weapons.cpp:1767` plays the weapon's `flashSound` and the muzzle effect plays its own sound primitive, the same file for the blaster, the bowcaster, the disruptor, the repeater, the DEMP2, the flechette and the rocket launcher (`effects/<weapon>/muzzle_flash.efx`, `cgame/cg_weaponinit.cpp`; the bryar: `fire.wav` and `fire.mp3`) | two copies of every shot started at the same sample, 18 to 36 units apart (the rocket's, logged): they add up, so every shot is twice as loud (6 dB) as the single player's, which plays the effect's only |
| the rocket's explosion played twice | its effect's sound primitive asks for it, `count 2` (`effects/rocket/explosion.efx`) | two copies of `sound/weapons/rocket/hit_wall` at the same instant and point: the data's way to make it louder in a mixer that adds copies |
| `s_volume` is not the master volume: the voices follow `s_volumeVoice` alone (seen on 23 November 2026: a session with `s_volume 0` and `s_musicvolume 0` played its pain sounds) | `snd_mix.cpp:269`, `:270`, `:317` (JA and JO the same) | a player who turns the volume down to 0 still hears every voice (pain, death, taunts, dialogue) at `s_volumeVoice` |
| `S_StopLoopingSound` skips the loop after each one it removes | `snd_dma.cpp:1179` | none today: nothing calls it |

### 11.8 What can be made one

- **One model, as decided** (section 1): a sound played with a class and, where one sound must replace another, a slot of its entity. The inventory gives the classes: weapon (shots, charges, zooms, locks, missiles), saber, force, voice, body (footsteps, landing, water), world (debris, movers, speakers, effects, ambient sets), vehicle, interface (menus, chat, HUD cues), announcer, music. The replacements that matter: an entity's voice (a new line or a pain cuts the last, the lip sync follows one voice per entity, the scripts wait for it, a mute cuts it); an entity's weapon (the shot cuts the charge, a weapon change cuts it; the swing replaces the last swing); one announcer at a time; one menu sound at a time; a speaker's sound. The other uses of the channels look incidental (footsteps, debris and notetracks on `CHAN_BODY`) and become plain sounds.
- **The attenuation by class** in place of six channel rules, one for every tree (the trees' different values of `CHAN_LESS_ATTEN` and of the lip sync made one, decided in section 1), and the listener's own sounds positioned at their own point instead of never panned (an option of the interface's decisions).
- **The looping sounds with a handle**: cgame already keeps a list of an entity's loops (`CG_S_AddLoopingSound`, `cgame/cg_ents.cpp:136`) and adds them again at every frame; with a handle, a loop is started, moved and stopped, and keeps its own voice instead of being merged with the loops of the same file.
- **The sounds sent by the game** (MP), asked on 23 November 2026, a question of its own for the interface's decisions: each is a temporary entity of its own (`G_TempEntity`, `G_SoundTempEntity`, `game/g_utils.cpp:1042`), which holds one of the 1,024 entities for 300 ms (`EVENT_VALID_MSEC`, freed at `game/g_main.cpp:2872`), whose point is rounded to whole units (`SnapVector`, kept in plan step 6 part F), which reaches only the clients whose potentially visible set holds that point (unless broadcast), and whose channel and target ride in fields meant for other things (`saberEntityNum`, `trickedentindex`, `trickedentindex2`, `groundEntityNum`); the sound is named by its index among the level's 256 sounds (`MAX_SOUNDS`, the configstrings `CS_SOUNDS`); the client plays it when the snapshot holding it arrives. The local player's own movement and weapon sounds come from its predicted player state's events, at once. Whether these events stay, with the class or slot in place of the channel, or become a list of sounds of their own in the snapshot (the tick, the entity or the point, the sound, the class; heard by distance rather than by the visible set, which also concerns the culling of plan step 16) is put with the interface's decisions; it changes the network's format and the game module.
- **One sound set file**, `sound/sound.txt`, and one dynamic music file, `ext_data/dms.dat`, read the same way in every tree (today two copies of their parsers, section 2); MP's dynamic music given a state by its game or left on explore (to decide with the interface).
- **The voices' loudness** given to every tree's modules by one function returning an explicit state (not talking, talking but silent, a level), in place of the integer array whose -1 and 0 mean two things (GUIDELINES section 6), still written in JA's saved games.
- **The engines' calls** (section 11.2) made one in `src/shared`: the frame, the registration, the music, the cinematics' raw samples and sound files, the focus, the file system's buffer clearing (gone: OpenAL Soft mixes on its own thread, nothing to clear before a read), the memory hooks of the zone (gone with decision 7a).
- **Entries with no caller** removed: `S_StopLoopingSound`, `S_StartLocalLoopingSound`, `S_SetLipSyncs`, `S_MP3PreProcessLipSync`, the renderer's `FS_FileExists` through `S_FileExists`.

### 11.9 The sound log, and today's defects shown (23 November 2026)

The log (section 10.4, point 2): `docs/modernization/tools/soundinv/soundlog.patch` gives the old mixer, at the commit of part 1, a cheat cvar `s_log` that writes `SNDLOG` lines into the console's log: every sound started (its file, entity, channel, the listener, whether at a fixed point, its distance), every channel taken from a sound that still plays (`kick`) or from the same entity's channel (`replace`), every sound stopped when out of range (`drop`), the looping sounds merged by file, the Doppler factors, the loops dropped beyond the frame's 32, the mutes, the channels playing at every frame and, every 50th frame, the list of the busy channels; the measuring build also keeps playing without the window's focus (the pause froze the mixer's clock, so no sound ended: the first runs were wrong for that reason and were made again). `soundlog_session.sh` runs a silent client session (`s_volume`, `s_musicvolume` and `s_volumeVoice` at 0) on a listen server of `mp/ffa3`, the client a spectator following the first bot, so that the bot's ears are the listener's; `soundlog.py` summarizes a log. The old build is the worktree `E:\Git\jka\ModernJK_soundref` (section 10.2); the new system writes the same lines, so that both are compared on the same sessions (section 10.4, point 4).

| session | what it showed |
|---|---|
| scenarios, 69 s at 125 and at 333 frames per second (two bots: the rocket fired 600 units from the defender, its lock held 1.4 s on the defender 900 units away, the disruptor's sniper shot) | the Doppler: the shooter's own rocket 2.76 times higher at 58 units, back at 100 (112 frames at 125, 297 at 333, about 70 ms per rocket either way); the lock's ticks and tone (62) played at the target, 901 to 996 units away, so at 0.41 to 0.48 of their volume where an interface cue would be at full volume; every rocket's shot and explosion started twice at the same instant (42 pairs); 4 or 5 sounds stopped for good when out of range; never more than 10 channels playing |
| free play, 90 s at 125 frames per second (ten bots on `mp/ffa3`, every weapon and force power) | 25 sounds started per second, 7.6 channels playing on average; all 32 busy at 161 frames (1.4 %), and 251 sounds still playing cut for a new one (`kick`: a saber's hum, energy crackles, lightning's hits, two voices' pain); several sabers' hums merged into one channel at 9,391 of 11,250 frames, up to 9 of one file, their left and right volumes added and clipped, so heard from one blended direction; 238 saber hits on walls cut by another one anywhere in the level (entity -1, `CHAN_WEAPON`); 812 sounds stopped for good when out of range at one frame (energy crackles, footsteps, hits on walls) |
| crowd, 90 s at 125 frames per second (fifteen bots on `mp/duel1`, every weapon and force power) | 114 sounds started per second, 28.5 channels playing on average, all 32 at 548 frames (4.9 %); 4,932 sounds still playing cut for a new one, among them 1,610 energy crackles, 667 lightning's hits, 477 saber hits and the saber's swings, chosen as the oldest whatever their loudness or distance; up to 17 sabers' hums merged into one channel, at every frame; 2,527 saber hits on walls cut by another one; "no force" (6) played on entity 0, here the spectating client's own entity, 767 to 1,115 units from the listener, so at 0.31 to 0.59 of its volume; the bryar's shot started twice (18) |

Not shown by these sessions: the debris of one entity cutting each other (no breakable object on `mp/ffa3` or `mp/duel1`); it stays as read in the code (section 11.7) until a session shows it. The first run of the free play had four bots instead of ten (six of the names it asked for are no bot of the game, which the engine refused): made again. Corrected on the way: the disruptor's zoom loop, first read as restarted at every frame, is started again every 300 ms while the zoom moves (`cgame/cg_view.cpp:1127`), whatever the frame rate.

## 12. The interface, and the decisions to take (23 November 2026)

Section 10.4, point 4: the interface designed from the inventory (section 11), and every open decision at once.

### 12.1 The interface proposed

One module, `src/shared/sound`, for both engines. The engines call it directly; every module (the multiplayer's cgame and UI, the single player's cgame, game and UI) receives one table of the sound's functions through its import table, the single player's numbered system calls of the sound removed (decided in section 1).

| function (the table's) | what it does | replaces |
|---|---|---|
| `Register(name)` | loads a sound, decoded whole to 16-bit PCM (decision 7a), and gives its handle; a missing file gives the empty handle and one warning | `S_RegisterSound` |
| `Play(request)` | the one entry: a sound, its class, where (an entity it follows, a fixed point, or the listener), an entity's slot or none, looping or not, a volume (1 by default); returns the voice's handle, which a looping sound keeps to move and stop it | `S_StartSound`, `S_StartLocalSound`, `S_AddLoopingSound`, `S_AddRealLoopingSound`, `S_StartAmbientSound`, `S_AddAmbientLoopingSound` |
| `Stop(voice)`, `MoveLoop(voice, origin, velocity)` | a voice stopped; a loop that follows no entity moved | `S_ClearLoopingSounds`, `S_StopLoopingSound` |
| `StopEntity(entity, slot)` | the sound in an entity's slot stopped, or every sound of the entity when no slot is named (a sound tracker's mute, an entity gone) | `S_MuteSound`, `S_StopSounds` of the single player's game |
| `UpdateEntity(entity, origin, velocity)` | where the sounds that follow an entity are, and how fast it moves (Doppler) | `S_UpdateEntityPosition` |
| `SetListener(entity, origin, axes, velocity, underwater)` | the ears, once per frame | `S_Respatialize` |
| `VoiceState(entity)` | not talking, talking but silent, or a level from 1 to 4, read from the voice's samples at its playing position (an explicit state, GUIDELINES section 6) | `S_GetVoiceVolume`, `gi.VoiceVolume` (the saved games keep their integer, converted at that boundary) |
| `Length(sound)` | a sound's length in milliseconds | `S_GetSampleLengthInMilliSeconds` |
| `StartMusic(intro, loop)`, `StopMusic()`, `SetMusicState(state)` | the music streamed through `sound::Decoder`, the dynamic music's states | `S_StartBackgroundTrack`, `S_StopBackgroundTrack`, the configstring the engine read |
| `ParseSets()`, `PrecacheSet(name)`, `UpdateAmbientSet(name, origin)`, `AddLocalSet(...)`, `BrushModelSound(set, stage)` | the ambient sets of `sound/sound.txt`, read once for every tree | the `AS_*` functions, `S_UpdateAmbientSet`, `S_AddLocalSet` |

The engines' own calls: `Init`, `Shutdown`, `Frame`, `BeginRegistration`, `EndRegistration`, `StopAll`, the cinematics' stream (`OpenStream(rate, channels)`, `QueueStream(samples)`, `CloseStream()`, the three sound files of JA's cinematics through `Play`), the focus (`SetFocus`), the device followed by the module itself (`ALC_SOFT_system_events`, `alcReopenDeviceSOFT`), HRTF asked off, `bsinc24`.

Removed with the old code: `S_ClearSoundBuffer` (OpenAL Soft mixes on its own thread: nothing to clear before a file is read), `SND_FreeOldestSound` and `SND_RegisterAudio_LevelLoadEnd` (decision 7a: the level's sounds are freed at the next level), `S_Shutup` (one warning per missing file), `S_StartLocalLoopingSound`, `S_SetLipSyncs`, `S_MP3PreProcessLipSync`, the renderer's `FS_FileExists` through `S_FileExists`, the video recorder's hooks (section 1).

The classes (decision 13 below), each with its attenuation (OpenAL Soft's distance model set per source, `AL_EXT_source_distance_model`), its volume group and its priority in the choice of the 64 real voices:

| class | from today's | attenuation to start from (full volume up to, silent from, units) | volume | priority | slots |
|---|---|---|---|---|---|
| voice | `CHAN_VOICE` | 768, 2,018 | voice | 1 (highest) | voice |
| voice, quiet | `CHAN_VOICE_ATTEN` | 346, 596 | voice | 1 | voice |
| voice, everywhere | `CHAN_VOICE_GLOBAL` | none (full everywhere) | voice | 1 | voice |
| announcer | `CHAN_ANNOUNCER` | the listener (not positioned) | voice | 1 | one for the game |
| interface | `CHAN_LOCAL`, `CHAN_LOCAL_SOUND`, `CHAN_MENU1`, the HUD's cues (the rocket's lock, "no force") | the listener | effects | 1 | one for the menus |
| weapon | the shots, charges, zooms, missiles | 256, 1,506 | effects | 2 | weapon (the shot cuts the charge, a weapon change cuts it) |
| saber | hums, swings, hits, blocks | 256, 1,506 | effects | 2 | weapon for the swings |
| force | the powers' sounds | 256, 1,506 | effects | 3 | none; a tracker's sounds stopped with `StopEntity` |
| vehicle | `CHAN_LESS_ATTEN` | 2,048, 3,298 (MP's; JA's 1,280, 2,530 made one) | effects | 3 | none |
| world | debris, movers, effects, speakers, notetracks | 256, 1,506 | effects | 4 | item for a speaker |
| body | footsteps, landing, roll, water | 256, 1,506 | effects | 5 | none |
| ambient | the ambient sets | 256, 1,506, at the set's volume | effects | 6 | none |
| music | the music, the cinematics' stream | the listener | music | outside the 64 | |

The data names its channels (`animevents.cfg`, Jedi Outcast's `animsounds`, the ICARUS scripts, the effects' sound primitives): the parsers turn a channel's name into a class and a slot at that boundary. The network's events (MP) carry the class and the slot instead of the channel (decision 8).

### 12.2 The decisions

Each with its mechanism, its options, the recommended one first, and its costs (the 64 real voices cost OpenAL Soft 0.41 % of a core for 32 and 1.20 % for 96 without reverb, the audio survey's section 7).

1. **The listener's own sounds.** Today the listener's entity is never panned nor attenuated, at 0.75 of full volume (section 11.3). (a) Positioned like any other sound, at their own point, the listener being the camera: in third person they come from the character in front of the camera, in first person from the head, so centred; no special case. (b) Today's rule, not positioned (`AL_SOURCE_RELATIVE` at the listener). No cost either way.
2. **The attenuation's curve.** (a) Today's straight line per class (OpenAL Soft's `AL_LINEAR_DISTANCE_CLAMPED`, set per source), from today's distances in the table, tuned by ear in part 5: the distances players know; Unreal Engine's default attenuation is linear too. (b) The inverse distance (OpenAL's default, `AL_INVERSE_DISTANCE_CLAMPED`: louder near, a long quiet tail), with a distance beyond which the sound is silent: closer to physics, every distance to tune again. No cost either way.
3. **Doppler's speed of sound** (Doppler for every moving sound, decision 10a). (a) The physical one in game units, about 12,000 units per second (the player's 64 units for about 1.8 m): a rocket at 900 units per second lowered by about 1.2 semitones going away, raised by 1.3 coming; (b) slower, for a more audible effect (6,000: about 2.5 semitones). OpenAL Soft computes it from the velocities given (`UpdateEntity`, `SetListener`); no cost.
4. **The voices beyond the 64.** (a) FMOD's virtual voices: a voice that is not mixed keeps its playing position advancing and plays on from there when it is among the 64 again ("as if the event was never muted, giving the illusion of simply being out of earshot", FMOD Studio's documentation, section 16.5.2), the one-shots included: fixes today's sounds stopped for good out of range (812 in 90 s of free play) and the sounds cut by the oldest rule (4,932 in the crowd); (b) the loops virtual, a one-shot that cannot be mixed dropped. Cost: a few dozen bytes per virtual voice and an audibility per voice per frame (hundreds of voices: microseconds).
5. **The limit per sound** (decided in section 1, its number open). (a) At most 4 voices of one file at once; a fifth takes the place of the quietest of them, or is refused if it would be the quietest (Unreal Engine's "Stop Quietest"): the crowd's 17 hums of one file become its 4 loudest; (b) Unreal Engine's default, 16 per group, the farthest then the oldest stopped; (c) no limit per file, the 64 chosen by audibility alone. No cost but the count.
6. **The same sound started twice at one instant and place by the data** (the rocket's explosion, `count 2` in its effect: the old mixer added the copies, twice as loud, 6 dB). (a) One voice at twice the gain (OpenAL Soft accepts gains up to 1,000, `AL_GAIN_LIMIT_SOFT`): the loudness the data meant, one voice; (b) two voices, as the data says: the same result, one voice more; (c) one voice at its normal gain: quieter than meant.
7. **Every shot played twice in MP** (section 11.7). (a) As JA and JO: the muzzle effect's sound only, at the muzzle, the weapon's `flashSound` kept for the weapons whose effect has no sound (the thermal detonator, the trip mine, the det pack, the stun baton); (b) the `flashSound` only, the effect's sound skipped; (c) both, as today (6 dB louder than the single player).
8. **The sounds the server sends (MP).** Today each is a temporary entity for 300 ms, its point rounded to whole units, sent only to the clients whose potentially visible set holds it, its channel in fields meant for other things (section 11.8). (a) The events kept, the class and the slot in place of the channel in the same fields: no change of the network's format; the transport's defects stay; (b) a list of sounds in each snapshot (the tick, the entity or the exact point, the sound, the class), heard by distance instead of the visible set: no entity taken, the exact point, the precise instant; it changes the network's format and netcodec's schema (the tables trained again, as plan step 13 does anyway), the game module's helpers and the server's choice of what a client receives (what plan step 16 decided: a player one can hear is sent with his sounds); (b) in a part or step of its own after part 2, with step 13's training, recommended; (c) (b) in part 2.
9. **The focus's setting** (decision 11b: a setting, on by default). (a) `s_muteWhenUnfocused` 1, ioquake3's name (its default 0, `code/client/snd_main.c`); a minimised window has no focus, so one setting covers both; (b) ioquake3's two settings, `s_muteWhenUnfocused` and `s_muteWhenMinimized`, both 1.
10. **The sound log in the new system.** (a) `s_show 1` (cheat, kept by decision 9) writes the same `SNDLOG` lines as `soundlog.patch`, with the class and whether each voice is real or virtual, in place of today's `s_show` prints; (b) a cheat cvar of its own, `s_log`.
11. **The volumes.** Today `s_volume` does not reach the voices (section 11.7). (a) `s_volume` the master volume of everything, `s_volumeVoice` and `s_musicvolume` relative to it, the announcer with the voices: a player who sets 0 hears nothing; (b) today's: the voices beside the master volume.
12. **The single player's saved games** keep `lastVoiceVolume` as an integer: (a) the explicit state converted to today's integer when written and read, the saved games unchanged; (b) the state written as it is, the saved games' version raised.
13. **The interface and the classes** of section 12.1: (a) as proposed; (b) changed where you say.

Decided on 23 November 2026: 1b (the listener's own sounds not positioned, as today), 2a, 3a, 4a, 6a, 9a, 10a, 11a, 12b (the voice's state written as it is in the single player's saved games, their version raised), 13a (section 1). Then the same day: 5, four voices of one file mixed at once, the quietest beyond them made virtual instead of stopped (its position advancing, it plays again if it becomes louder than one of the four), as 4a; 7a, the shots as in the single player, the muzzle effect's sound only. Then 8a, as recommended below: the server's sound events kept in part 2, the class and the slot in place of the channel; the transport of the sounds placed in plan step 16. Nothing open.

Recommended for 8, after the question of 23 November 2026: (a) in part 2, and the transport of the sounds decided later with what a client receives as a whole (plan step 16). The hearing through walls does not come from the temporary entities alone: a player's own sounds (footsteps, pain, shots) are events of his entity, which the server sends only when that entity is in the client's visible set, so a list of sounds would not let a player hear another behind a wall; that is the question of plan step 16 (decided there on 15 November 2026: a hidden player one can hear is sent with his position and sounds). A change of the network's format is better made once, with plan step 13's training. The limits (1,024 entities, 256 sounds) were never reached.


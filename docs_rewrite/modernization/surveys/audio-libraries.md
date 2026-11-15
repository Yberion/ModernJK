# Audio libraries: one library for the client's sound on every system

Written on 15 November 2026 for plan step 15, part 3 ([../PLAN.md](../PLAN.md)), after the decisions P5 and P6 ([platform-layer.md](platform-layer.md) section 7): the sound goes through one library for every system behind a simple interface, every feature is kept, the reverb included, and rebuilding the reverb on the new library and shipping new data in a pk3 of the game's own is accepted if needed. 32-bit support is not a criterion. Nothing is implemented. Sections 2 to 6 were read in this repository, in the game's data and in the projects' documentation and source, before anything was downloaded; the downloads were then approved, and the prototypes and their measurements of the same day are in section 7.

Each source is labelled as in the other surveys: **primary** (the project's own documentation or source code), **secondary**, **community**.

## 1. Summary

- The client's sound today is about 11,400 lines of its own (`codemp/client/snd_*.cpp`) plus a 1995 MP3 decoder (`codemp/mp3code`, 8,325 lines): a software mixer of 32 channels that paints 150 ms of sound ahead at every frame, so that it mixes 15 to 50 times what is played (100 to 333 frames per second) and a sound is heard about 70 ms after it starts (estimated from the code). The reverb exists only in a path compiled for 32-bit MSVC, off by default, which needs Creative's EAX drivers and Creative's `EAXMan.dll`: in practice no player of this build hears it.
- The reverb's data is in the game: 58 files of zones, `eagle/*.eal`, 23 of them for the multiplayer maps. Their format is Creative's, but it has been documented and re-implemented by a third party (EalMan, LGPL-2.1, extended for Jedi Academy's files).
- Two libraries cover what the game needs on Windows, Linux and macOS: **OpenAL Soft** (LGPL, C++20; 3D, HRTF, an EFX reverb whose parameters are EAX's, so the game's environments map onto it as they are; no file decoding) and **miniaudio** (public domain or MIT-0, one C file; device, mixer, 3D with Doppler, pitch, groups, filters, WAV, MP3 and FLAC decoding, rendering without a device; its reverb is an extra node of its own, not EAX's parameters). FAudio (XAudio2 on SDL 3), SDL3_mixer and SoLoud each lack a part (section 4).
- Recommended at first: prototype OpenAL Soft and miniaudio on the game's sounds before choosing; the reverb's zones converted once from the `.eal` files into a file of this game, shipped in its pk3, as recent engines describe reverb by zones (Unity's reverb zones, idTech 4's per-area EFX files in dhewm3).
- Measured the same day (section 7): EalMan reads all 58 zone files, every multiplayer environment with its EAX values; a busy scene of 32 voices costs 0.41 % of a core with OpenAL Soft and 0.66 % with miniaudio, about 1 % with three reverbs for both; both devices open with about 30 ms of latency on Windows (about 70 ms estimated today); miniaudio's reverb is a Freeverb that cannot hold the game's EAX values. Recommended now: **OpenAL Soft** (section 6).

## 2. What the client's sound does today

Read in `codemp/client/` (references are to that folder unless stated).

| feature | where | notes |
|---|---|---|
| 32 mixing channels, each sound on an entity's channel (`CHAN_AUTO`, `CHAN_VOICE`, `CHAN_WEAPON`, `CHAN_ANNOUNCER`, ...) with the rules for which sound replaces which | `snd_local.h:185`; `S_PickChannel` `snd_dma.cpp:1088`; the channels `codemp/qcommon/q_shared.h:855-868` | voices are global or attenuated by channel |
| 3D: stereo panning with `s_separation`, distance attenuation | `S_SpatializeOrigin` `snd_dma.cpp:1345` | the listener set at every frame by cgame (`S_Respatialize` `:2503`) |
| looping sounds per entity, rebuilt at every frame; ambient looping sounds | `S_AddLoopingSound` `:1924`, `S_AddAmbientLoopingSound` `:1982`, `S_AddLoopSounds` `:2036` | |
| Doppler on looping sounds (`s_doppler`) | `:1956-1969` | a pitch change from the entity's velocity |
| ambient sets: sounds chosen by area from text files | `snd_ambient.cpp` (1,156 lines), `AS_ParseFile` `:725` | |
| music: an intro and a loop streamed from MP3; dynamic music (explore, action, boss, death, with transitions), `s_allowDynamicMusic` | `S_StartBackgroundTrack` `snd_dma.cpp:4521`, `:4331-4920`, `snd_music.cpp` (1,133 lines) | the music reaches the mixer as raw samples (`:4787`) |
| raw sample streams: the cinematics' sound (22,050 Hz) and the music | `S_RawSamples` `:2141`; `cl_cin.cpp:1179`, `:1189` | |
| lip sync: the loudness of each entity's voice drives its mouth | `S_DoLipSynchs` `:2701`, `S_CheckAmplitude` `:2377`, `s_entityWavVol` `:221` | |
| decoding: 12,902 MP3 and 302 WAV files in the game's `assets0` to `assets3` (counted on 15 November 2026); the Xing decoder of 1995 to 1997 | `codemp/mp3code`; `S_LoadSound_Actual` `snd_mem.cpp:732` | MP3s are unpacked to PCM when registered, except under `sound/chars/`, kept compressed and decoded while mixing (`snd_mem.cpp:715`) |
| a memory pool for the sounds, the oldest freed (`s_soundpoolmegs` 25) | `SND_FreeOldestSound` `snd_dma.cpp:5189` | |
| the video recorder takes the mixed sound | `snd_mix.cpp:182` (`CL_WriteAVIAudioFrame`); `S_GetSoundtime` follows the video's frame rate while recording (`snd_dma.cpp:2797`) | needs a mixer that can render without a device |
| settings | `s_volume`, `s_volumeVoice`, `s_musicvolume`, `s_khz` (44), `s_language` (the voices' language), `s_doppler`, `s_separation`, `s_allowDynamicMusic`, `s_mixahead`, `s_mixPreStep`, `s_threshold1` to `4`, the four `s_sdl*`, `s_soundpoolmegs`, debug ones | `snd_dma.cpp:445` onwards, `shared/sdl/sdl_sound.cpp` |
| the device: SDL 2's callback reading a ring the mixer paints, paused when the window loses the focus | `shared/sdl/sdl_sound.cpp`; `shared/sdl/sdl_input.cpp` | |

How it mixes (the platform survey, section 7, "Facts checked"): at every frame `S_Update` repaints from 50 ms to 200 ms ahead of the position the device plays (`snd_dma.cpp:2834`, `:3100`), 150 ms per frame: 15 times the sound played at 100 frames per second, 19 at 125, 22 at 144, 50 at 333; a sound started now is heard after the 50 ms ahead plus the device's block of 1,024 samples (23 ms), about 70 ms (estimated from the code, not measured).

## 3. The reverb today, and how recent engines describe it

**In the code.** The OpenAL path (`USE_OPENAL`, only with MSVC in 32-bit, `snd_local.h:31`; `s_UseOpenAL` 0 by default, `snd_dma.cpp:488`) replaces the mixer by OpenAL and adds Creative's EAX 4.0:

- `InitEAXManager` (`snd_dma.cpp:5312`) needs an OpenAL driver with EAX 4.0, or EAX 3.0 with its emulation of 4.0, then loads Creative's `EAXMan.dll` (a closed library, shipped in the retail game's folder with Creative's `OpenAL32.dll`, both 32-bit).
- `LoadEALFile` (`:5449`) reads `eagle/<map>.eal` through `EAXMan.dll`: environments (reverb presets), the geometry that says which environment holds a point, and apertures (`Aperture<n>a`, `Aperture<n>b`) between environments.
- Every frame the listener's environment is looked up and the effect slots are set (`UpdateEAXListener` `:5752`); every source gets its occlusion from the geometry (`UpdateEAXBuffer` `:6206`); an underwater preset replaces the environment while the listener is in water (`S_Respatialize` `:2537-2579`, only when an `.eal` file is loaded).
- So the reverb needed an EAX driver (Creative's sound cards; hardware EAX through DirectSound ended with Windows Vista) and has never existed in the 64-bit builds or on Linux.

**In the game's data** (counted on 15 November 2026): 58 `.eal` files in `assets0.pk3`, under `eagle/`; for multiplayer `mp/ctf1` to `ctf5`, `mp/duel1` to `duel10`, `mp/ffa1` to `ffa5`, `mp/siege_desert`, `mp/siege_hoth`, `mp/siege_korriban` (23).

**The format.** Not documented by Creative. A third party documented it (EAXManagerEAL: a RIFF container, the processed geometry in a `gema` chunk) and wrote a portable re-implementation of `EAXMan.dll`, EalMan (C++11, LGPL-2.1), extended to replace the version 3.0 of `EAXMan.dll` that Jedi Academy uses; its limits: it handles only files with one geometry set (all of Unreal Tournament's), and it returns the values without applying them (primary: its repository). Whether Jedi Academy's files have one geometry set is not known (section 9).

**How recent engines describe reverb.** By zones of space with a reverb preset, crossed by the listener: Unity's Audio Reverb Zone (a zone with an inner and an outer radius, the reverb faded in between, zones that can be mixed; primary: Unity's manual); idTech 4 (Doom 3) gives every area of a map a reverb in an `efxs/<map>.efx` file, whose second version holds exactly the parameters of OpenAL's EFX "EAX reverb" effect, and dhewm3, its maintained port, plays them through OpenAL Soft on every system (primary: dhewm3's site and repository; the file format from The Dark Mod's wiki, community). OpenAL Soft's author states that EFX can fade smoothly between zones, where Doom 3 switched at the doorstep (secondary: OpenAL Soft issue 62). The game's `.eal` files are that model already: zones with EAX presets.

## 4. The candidates

What the game needs, from section 2: a device on Windows, Linux and macOS that follows the system's default; mixing of at least 32 voices; 3D panning and attenuation; pitch (Doppler); looping sounds; streams (music, cinematics); MP3 and WAV decoding (or a decoder beside it); a reverb with zones; a low-pass filter (underwater); the loudness of a playing voice (lip sync); rendering without a device (the video recorder); a licence the GPLv2 accepts; built from source with the project (the rule: no library from the system).

| | OpenAL Soft | miniaudio | FAudio | SDL3_mixer | SoLoud |
|---|---|---|---|---|---|
| licence | LGPL 2 or later (the GPLv2 accepts it) | public domain or MIT-0 | zlib | zlib | zlib |
| form | C API, library in C++20 since 1.25.0; CMake | one C file, no dependency | C; XAudio2 API; CMake | C; on SDL 3.4 | C++ |
| device | its own: WASAPI, PipeWire, PulseAudio, ALSA, CoreAudio, ... | its own: WASAPI, Core Audio, PulseAudio, ALSA, JACK, ...; or none (the application reads the mix) | SDL 3's | SDL 3's | its own or SDL |
| 3D | yes: distance models, Doppler, cones, HRTF for headphones | yes: attenuation models (inverse as OpenAL), Doppler, cones | X3DAudio (computes the panning matrices) | "positional audio" (details not read) | yes |
| reverb | EFX "EAX reverb": the EAX parameter set; also the legacy EAX extensions (since 1.22.0, on by default only on Windows) | an extra node, `ma_reverb_node` (its model not read); otherwise to write | XAudio2's reverb effect (to verify in FAudio) | "DSP effects", none named | a Freeverb filter (to verify) |
| filters | EFX low-pass, high-pass, occlusion, air absorption | low-pass, high-pass, delay nodes in a node graph | XAudio2's filters | not read | yes |
| decoding | none | WAV, MP3, FLAC | none (XACT, WMA) | many formats | WAV, OGG, MP3 |
| without a device | `ALC_SOFT_loopback` (the long-standing extension; the ChangeLog names its B-format variant, 1.21.0) | yes: `ma_engine_read_pcm_frames` with no device | not read | "mixing to a memory buffer" | not read |
| state | 1.25.2 (dates not in the ChangeLog); used by dhewm3 (1.25.0 in the Windows build of its 1.5.5 release candidate) | 0.11.x (the exact version not read) | used by Wine and FNA (its activity not read) | 3.2.0, the first official release, on 9 March, 3.2.4 on 3 June (2026, from the release page) | last release in 2020 (to verify) |

Left out: FMOD, Wwise and the Miles Sound System are closed and cannot be shipped in a GPLv2 program; PortAudio, RtAudio, cubeb and libsoundio give a device only; Steam Audio (Apache 2.0) adds spatialization and acoustic simulation on top of a mixer, it is not one.

What the table means for this game:

- **OpenAL Soft** is the only one whose reverb takes EAX's parameters as they are: the environments of the `.eal` files become EFX reverb settings without translating them, and the underwater preset with them; occlusion is an EFX filter, as EAX's was. It brings HRTF for headphones. It decodes nothing: the MP3s need a decoder beside it (dr_mp3 or minimp3, public domain, one header each), so two libraries. It mixes on its own thread with its own device code; SDL 3's audio is not used.
- **miniaudio** is one file that does everything except EAX's reverb: decoding included (its MP3 decoder replaces the Xing one), groups for the volumes (effects, voices, music), a node graph for the filters, and the mix can be read without a device for the video recorder, or pulled by SDL 3's audio stream so that SDL stays the only device code. Its reverb node is not described as EAX's model (not read): the game's environments would be translated to its parameters, or a reverb with EAX's parameters written for it.
- **FAudio** sits on SDL 3's audio and has XAudio2's reverb and 3D, but its interface is XAudio2's (voices, submixes, COM-like objects) and it decodes nothing the game uses.
- **SDL3_mixer** decodes and mixes on SDL 3 but no reverb or filter is named in what was read, and its first official release is of March 2026.
- **SoLoud** has most of it, but its last release is of 2020.

## 5. The interface

The interface is to be simple. The engine's sound API (`snd_public.h`: about 25 `S_*` functions called by the client, cgame and the UI) stays as the game's door; below it, the library is reached through a few functions of a module of its own (C++20, as `codemp/netcodec` and the planned `codemp/platform`), which the rest of the engine does not see:

- open and close (the default device, followed when it changes; muted when the window loses the focus, by default);
- load a sound (decoded once to PCM; the `sound/chars/` MP3s kept compressed only if the measures show the memory matters);
- play a sound at a position, or attached to an entity, on a channel class (which sets its attenuation and what it replaces), with a volume and a pitch; stop it; set a loop;
- the listener's position and orientation once per frame;
- the reverb: the listener's zone looked up from the map's zone file and its preset applied, faded at the zone's edge; the underwater preset;
- one stream for the music and the cinematics;
- the loudness of an entity's voice (lip sync);
- render a block without the device (the video recorder).

The mixer, the ring shared with the device, `s_mixahead`, `s_mixPreStep`, the four `s_sdl*`, the OpenAL and EAX path and the Xing decoder go with the old code; `s_khz` goes if the library takes the device's rate (to decide with the prototype; decided the same day: it goes, with `s_UseOpenAL` and their menu items, section 8).

## 6. Options

| option | for | against | CPU, memory, latency (estimates) |
|---|---|---|---|
| **A. OpenAL Soft** (+ a one-header MP3 decoder) | the reverb as the game's data describes it (EAX parameters), occlusion, HRTF; mature, used by an idTech port for the same purpose; every system | two libraries; LGPL (accepted by the GPLv2, the sources shipped with the game's); a large library; its own device code beside SDL's | mixes once what is played, on its own thread; latency about 10 to 20 ms (device period; to measure); the reverb's cost per voice to measure |
| **B. miniaudio** | one file, no dependency, decoding included, public domain; can sit under SDL 3's audio | the reverb to adapt or write (not EAX's parameters); occlusion to build from its filters | the same model; its reverb node's cost to measure |
| **C. FAudio** | on SDL 3's audio; XAudio2's reverb and 3D | XAudio2's verbose interface; no decoding | to measure |
| **D. keep the game's mixer, pulled by SDL 3** (option (a) of P5) | no new library | the reverb to write anyway; 11,400 lines of old code kept | once what is played, latency of the device block |

**Recommendation**: prototype A and B (section 7), then choose with the numbers; A is favoured for the reverb, the hardest feature to keep, B if its reverb sounds right and one file matters more.

**Recommendation after the measurements (15 November 2026)**: A, OpenAL Soft. It plays the game's reverb as it was designed (the EAX values of every environment, the neighbouring environments panned towards their apertures, occlusion per source), it mixes the scene without reverb in less time than miniaudio (0.41 against 0.66 % of a core for 32 voices) and with three reverbs for a little more (1.15 against 0.92 %, for a reverb that does far more), and it offers HRTF (not kept, A3). Its costs: 2.1 MB of program, a one-line patch to build with clang 23, LGPL, and a decoder beside it for the MP3s (dr_mp3, the decoder miniaudio itself contains, public domain or MIT-0, one header). B stays the choice if one file is preferred and accepts a reverb that only resembles the game's.

**The reverb's data** (both libraries): a tool reads the 23 multiplayer `.eal` files once (through EalMan, or a reader of our own from the format's documentation, checked against EalMan) and writes, per map, the zones and their presets in a file of this game, shipped in a ModernJK pk3 (acceptable); a map without that file has no reverb, or one preset for the whole map (to decide). Computing the reverb from the map's geometry at load, as Steam Audio can, is the other way: heavier, and the game's sound designers' choices would be lost.

## 7. The measurements

Planned before the downloads: (1) the CPU of a busy game's sound with and without the reverb; (2) the latency of each library's device; (3) the reverb of `mp/ffa3` applied in both, listened to; (4) the size and the builds; (5) whether EalMan reads the game's 58 files. Done on 15 November 2026 on the development machine (Windows), with OpenAL Soft 1.25.2 (built as a static library), miniaudio 0.11.26 and EalMan `aa69e586`, all built with llvm-mingw clang 23 at `-O2`; the tools are in `tools/audiobench/` (`ealdump.cpp`, `audiobench.cpp`, `build.sh`), the libraries and the game's files they read in `build-netcodec/audiolibs/` (ignored by git). Not done: the builds on Linux and with MSVC, and the measures on Linux.

**The zone files (5).** EalMan, built as `EAXMan.dll` 3.0 (`EAXMANVERSION=3`), loads all 58 files without an error. 41 have environments with their centre points and apertures (the multiplayer ones: `ctf1` 25 environments and 79 apertures, `ctf2` 18 and 60, `ctf3` 9 and 24, `ctf4` 10 and 21, `ctf5` 5 and 20, `duel2` 4 and 8, `duel3` 2 and 2, `duel7` 2 and 2, `ffa1` 4 and 10, `ffa2` 16 and 52, `ffa3` 7 and 19, `ffa4` 8 and 22, `ffa5` 7 and 24, `siege_desert` 29 and 71, `siege_hoth` 23 and 55, `siege_korriban` 26 and 51), and the centre of every environment is found inside that environment. The 17 others (among them `duel1`, `duel4` to `duel6`, `duel8` to `duel10`) have no centre points: the engine played them with one reverb, the listener's environment (`snd_dma.cpp:5717`, "EAX legacy behaviour"); EalMan reads their presets as well (one each in the multiplayer ones, `duel1`'s "Courtyard" for example). The environments are named by their designers ("Tie Fighter Hall", "Lunch Room", "NW Lower Hallway" in `ffa3`) and hold every EAX 4 reverb value (size, levels, decay times and ratios, reflections and late reverb with their delays and pan vectors, echo, modulation). So the conversion of A2 is possible for every multiplayer map that has a file, without `EAXMan.dll`. What the engine did with them each frame (read in `UpdateEAXListener`, `snd_dma.cpp:5752`): the listener's environment and the two nearest neighbouring ones through their apertures, each in an effect slot, the neighbours' reverb panned towards their aperture with a strength from the angle it covers; every source sent to its own environment's slot and given its occlusion and obstruction from the geometry (`UpdateEAXBuffer`).

**The CPU (1).** `audiobench mix`: looping game sounds (a saber hum, shots, footsteps, saber hits and blocks, voices) moving on circles 100 to 1,500 units around the listener with Doppler, one in four through a low-pass filter (occlusion), the multiplayer duel music beside them; positions set every 10 ms of sound; with the reverb, the voices split over three environments of `mp/ffa3` (OpenAL Soft: three effect slots with the EAX reverb set from EalMan's values; miniaudio: three groups, each through a splitter into its reverb node); 60 s of sound at 48,000 Hz stereo, rendered without a device (OpenAL Soft's loopback device, miniaudio's engine without a device), the best of 5 runs. The share of one core this takes, to play the sound in real time:

| scene | OpenAL Soft | miniaudio |
|---|---|---|
| 32 voices, no reverb | 0.41 % | 0.66 % |
| 32 voices, 3 reverbs | 1.15 % | 0.92 % |
| 96 voices, no reverb | 1.20 % | 1.93 % |
| 96 voices, 3 reverbs | 2.07 % | 2.19 % |

These are with every sound at its own rate (44,100 Hz for the game's MP3s, counted below), resampled to the device's 48,000 Hz while mixing, as in the game; OpenAL Soft with its default resampler (a cubic spline). A first run decoded every sound to 48,000 Hz beforehand, so that only the Doppler shifts were resampled: OpenAL Soft 0.32 and 1.11 %, miniaudio 0.63 and 0.85 % for 32 voices; HRTF then cost 3.09 % for 32 voices and 3 reverbs, 7.25 % for 96 (HRTF is not kept, section 8). OpenAL Soft's resamplers for 32 voices and 3 reverbs: the cubic spline 1.19 %, the band-limited sinc of 24 points (`bsinc24`) 1.49 %, of 48 points (`bsinc48`) 1.84 % (chosen through `ALSOFT_CONF`, the library's configuration file, read as its log shows).

The engine mixes 32 channels today. Both libraries mix on their own thread, beside the game's main thread. The machine's load during the runs was not checked (the bench rules ask for a quiet machine), and today's mixer was not timed, so its cost is not in the table.

**The latency (2).** `audiobench latency`, each library's default playback device with its default settings (Windows 11, WASAPI, the machine's Realtek output, 48,000 Hz): OpenAL Soft mixes every 10 ms and reports 32 ms of latency (`ALC_DEVICE_CLOCK_LATENCY_SOFT`); miniaudio opens 3 periods of 10 ms, 30 ms of buffer. About 30 ms for both, against about 70 ms estimated for today's mixer (section 2); both have settings for shorter periods, not tried.

**The reverb (3).** A first set of extracts used the timing scene with its music, which covered the reverb; they were replaced by `audiobench listen`: no music, single sounds (two blaster shots, a saber hit, a saber block, a taunt) spaced over 7 s so that the reverb's tail is heard, in front of the listener at 180 to 390 units, in the first three environments of `mp/ffa3` one after the other ("Center", "NW Lower Hallway", "Tie Fighter Hall"), 21 s in all: without reverb, with OpenAL Soft's, with OpenAL Soft's and HRTF, with miniaudio's (the scratchpad of the session of 15 November 2026: `1_sans_reverb.wav`, `2_openal_reverb.wav`, `3_openal_reverb_hrtf_casque.wav`, `4_miniaudio_reverb.wav`). OpenAL Soft takes the environments' values as they are: its reverb is 12 to 15 dB below the direct sound over each environment (the extract with reverb minus the one without, which are sample-aligned). miniaudio's reverb node is Freeverb (verblib 0.5, "based on Freeverb", mono or stereo only, 22,050 Hz and above; primary: its header), driven by a room size, a damping, a width and two levels, and `ma_reverb_node_init` in 0.11.26 does not apply them (it keeps verblib's defaults: a full wet level, the reverb nearly as loud as the direct sound), so the bench sets them on verblib directly; the EAX values had to be approximated (the room size from the decay time, the damping from the high-frequency ratio, the level from the room and reverb levels, scaled so that the tail after each environment's last sound is as loud as OpenAL Soft's: -69, -69 and -81 dB against -69, -67 and -80), and it has no reflections, delays, pan vectors or low-frequency control. Its groups must not be spatialized themselves (they are by default and moved the direct sound).

**The sound's quality, today and with OpenAL Soft.** The menu's "Sound quality" sets `s_khz`, the rate the engine mixes at: "Low" 11 kHz, "High" 22 kHz (the game's `.menu` files), while the engine's own default is 44 (`snd_dma.cpp:454`), so choosing "High" in the menu halves the default. At 22 kHz nothing above 11 kHz is heard, at 11 kHz nothing above 5.5 kHz, although the game's sounds are recorded at 44,100 Hz (counted in `assets0` to `assets3` on 15 November 2026: 12,769 MP3 sounds and the 133 music tracks at 44,100 Hz; 266 WAV files at 22,050 Hz, 18 at 11,025, 14 at 44,100, 4 at 8,000). Each sound is brought to the mixing rate when loaded by taking the nearest sample, without a filter (`ResampleSfx`, `snd_mem.cpp:210`, whose comment says a low-pass filter is missing when downsampling): metallic artefacts at 22 or 11 kHz, steps in the WAVs at 44 kHz. The mixer adds in 16-bit integers and clips hard at full scale (`snd_mix.cpp:45`), and its Doppler only raises the pitch, by whole steps (`snd_mix.cpp:288`). OpenAL Soft mixes in 32-bit floats at the device's rate (48,000 Hz on the development machine), resamples each sound while mixing with a cubic spline by default or a band-limited sinc (`core/voice.h:35`, `core/mixer/defs.h:31`), limits the peaks instead of clipping them when the output is integer, and dithers (`alc/alc.cpp:1670`, `:1705`); its Doppler is continuous both ways. So the setting has no use left: every player hears the sounds at the rate they were recorded at.

**HRTF when headphones are detected.** OpenAL Soft turns HRTF on by itself when nothing is asked and the device is headphones (`alc/panning.cpp:1307`; WASAPI's form factor "headphones" or "headset", `alc/backends/wasapi.cpp:2197`): the engine must ask for it off (`ALC_HRTF_SOFT` false) since it is not kept.

**The size and the build (4).** A minimal program that opens a device, stripped: 2.1 MB with OpenAL Soft (its default HRTF data included), 0.64 MB with miniaudio, 9.7 KB empty. OpenAL Soft 1.25.2 does not compile with clang 23's libc++: `alc/alc.cpp:1784` takes the address of `std::unique_ptr::operator*`, which the standard does not allow a program to rely on; replaced locally by a lambda for the prototype (a patch to carry, or to check in a later release). miniaudio built as it is.

## 8. Decisions

- **A1**: which library, after the prototypes (A or B recommended to prototype; C and D listed); after the measurements, A is recommended (section 6).
- **A2**: the reverb's zones from the `.eal` files converted into a file of this game in its pk3 (recommended); maps without a file: no reverb, or one preset per map.
- **A3**: HRTF (3D for headphones), which OpenAL Soft offers: on when the system reports headphones, off, or a setting.
- **A4**: the go to download OpenAL Soft 1.25.2, miniaudio and EalMan for the prototypes of section 7.

Decided (15 November 2026): **A1** after the prototypes, then, after listening to the extracts of section 7, **OpenAL Soft** (its reverb sounds more pronounced, so better; it plays the environments' EAX values as they are, where miniaudio's Freeverb only approximates them), with a decoder beside it for the MP3s (dr_mp3 proposed) and its one-line patch for clang 23 carried until a release no longer needs it; **A2** the zones converted into a file of this game, and no reverb on a map that had none (no `.eal` file); **A3** HRTF behind a setting, then, after listening to the extract with HRTF, no HRTF at all (it sounds strange, even with headphones), asked off explicitly since OpenAL Soft would turn it on for headphones (section 7); **A4** the downloads approved: OpenAL Soft 1.25.2 (12 May 2026), miniaudio 0.11.26 (31 October 2026), EalMan at its commit `aa69e586` (it has no release), in `build-netcodec/audiolibs/` (ignored by git). Then (the same day): the "Sound quality" setting (`s_khz`) and the "EAX" one (`s_UseOpenAL`) removed with their menu items, the menus changed in the game's own pk3 (`ui/jamp/setup.menu` and `ingame_setup.menu`); OpenAL Soft's resampler `bsinc24` (1.49 % of a core for 32 voices and 3 reverbs, against 1.19 % for its default cubic spline).

## 9. What was not read or verified

- Whether some `.eal` files hold more than one geometry set (EalMan reads set 0, the only one the engine asked for, and every file loads and passes the centre check).
- miniaudio's PipeWire support (PipeWire serves PulseAudio's clients); OpenAL Soft's SDL backend, and whether it follows a change of the system's default device by itself; FAudio's reverb; SDL3_mixer's positional audio and effects; SoLoud's activity.
- The builds on Linux (GCC) and with MSVC, and every measure on Linux; today's mixer's CPU; shorter device periods; the latency from a call to the sound leaving the speakers (the figures are the libraries' own reports).
- The single-player sound (`code/client`), which keeps its own code.
- The dynamic music's details (`snd_music.cpp`) and the ambient sets' files beyond their parser.

## 10. Sources

Primary:
- miniaudio: https://github.com/mackron/miniaudio (README), https://miniaud.io/docs/manual/index.html, https://github.com/mackron/miniaudio/tree/master/extras/nodes
- OpenAL Soft: https://github.com/kcat/openal-soft (README, ChangeLog)
- SDL3_mixer: https://github.com/libsdl-org/SDL_mixer (README, releases)
- FAudio: https://github.com/FNA-XNA/FAudio
- EalMan: https://github.com/senneschall/EalMan; EAXManagerEAL: https://github.com/senneschall/EAXManagerEAL
- dhewm3: https://dhewm3.org/, https://github.com/dhewm/dhewm3
- Unity, Audio Reverb Zone: https://docs.unity.com/en-us/engine/current/manual/audio/reference/class-audio-reverb-zone
- This repository: `codemp/client/snd_*.cpp`, `codemp/mp3code`, `shared/sdl/sdl_sound.cpp`; the game's `assets0.pk3` to `assets3.pk3`

Secondary:
- OpenAL Soft issue 62 (EFX transitions between zones, Doom 3): https://github.com/kcat/openal-soft/issues/62
- EalMan issue 1 (the versions of `EAXMan.dll`, Jedi Academy's): https://github.com/senneschall/EalMan/issues/1

Community:
- The Dark Mod wiki, "Setting Reverb Data of Rooms (EAX)": https://wiki.thedarkmod.com/index.php?title=Setting_Reverb_Data_of_Rooms_%28EAX%29
- UT99.org, "State of EAX sound": https://ut99.org/viewtopic.php?t=17040

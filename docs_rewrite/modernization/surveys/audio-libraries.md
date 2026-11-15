# Audio libraries: one library for the client's sound on every system

Written on 15 November 2026 for plan step 15, part 3 ([../PLAN.md](../PLAN.md)), after the owner's decisions P5 and P6 ([platform-layer.md](platform-layer.md) section 7): the sound goes through one library for every system behind a simple interface, every feature is kept, the reverb included, and the owner accepts rebuilding the reverb on the new library and shipping new data in a pk3 of the game's own if needed. 32-bit support is not a criterion (owner). Nothing is implemented and nothing was downloaded: what is below is read in this repository, in the game's data and in the projects' documentation and source; the measurements that would decide are listed in section 7.

Each source is labelled as in the other surveys: **primary** (the project's own documentation or source code), **secondary**, **community**.

## 1. Summary

- The client's sound today is about 11,400 lines of its own (`codemp/client/snd_*.cpp`) plus a 1995 MP3 decoder (`codemp/mp3code`, 8,325 lines): a software mixer of 32 channels that paints 150 ms of sound ahead at every frame, so that it mixes 15 to 50 times what is played (100 to 333 frames per second) and a sound is heard about 70 ms after it starts (estimated from the code). The reverb exists only in a path compiled for 32-bit MSVC, off by default, which needs Creative's EAX drivers and Creative's `EAXMan.dll`: in practice no player of this build hears it.
- The reverb's data is in the game: 58 files of zones, `eagle/*.eal`, 23 of them for the multiplayer maps. Their format is Creative's, but it has been documented and re-implemented by a third party (EalMan, LGPL-2.1, extended for Jedi Academy's files).
- Two libraries cover what the game needs on Windows, Linux and macOS: **OpenAL Soft** (LGPL, C++20; 3D, HRTF, an EFX reverb whose parameters are EAX's, so the game's environments map onto it as they are; no file decoding) and **miniaudio** (public domain or MIT-0, one C file; device, mixer, 3D with Doppler, pitch, groups, filters, WAV, MP3 and FLAC decoding, rendering without a device; its reverb is an extra node of its own, not EAX's parameters). FAudio (XAudio2 on SDL 3), SDL3_mixer and SoLoud each lack a part (section 4).
- Recommended: prototype OpenAL Soft and miniaudio on the game's sounds before choosing, the CPU, the latency and the reverb measured (section 7, which needs the owner's go to download them); the reverb's zones converted once from the `.eal` files into a file of this game, shipped in its pk3, as recent engines describe reverb by zones (Unity's reverb zones, idTech 4's per-area EFX files in dhewm3).

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

What the game needs, from section 2: a device on Windows, Linux and macOS that follows the system's default; mixing of at least 32 voices; 3D panning and attenuation; pitch (Doppler); looping sounds; streams (music, cinematics); MP3 and WAV decoding (or a decoder beside it); a reverb with zones; a low-pass filter (underwater); the loudness of a playing voice (lip sync); rendering without a device (the video recorder); a licence the GPLv2 accepts; built from source with the project (the owner's rule: no library from the system).

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

The owner asked for a simple interface. The engine's sound API (`snd_public.h`: about 25 `S_*` functions called by the client, cgame and the UI) stays as the game's door; below it, the library is reached through a few functions of a module of its own (C++20, as `codemp/netcodec` and the planned `codemp/platform`), which the rest of the engine does not see:

- open and close (the default device, followed when it changes; muted when the window loses the focus, by default);
- load a sound (decoded once to PCM; the `sound/chars/` MP3s kept compressed only if the measures show the memory matters);
- play a sound at a position, or attached to an entity, on a channel class (which sets its attenuation and what it replaces), with a volume and a pitch; stop it; set a loop;
- the listener's position and orientation once per frame;
- the reverb: the listener's zone looked up from the map's zone file and its preset applied, faded at the zone's edge; the underwater preset;
- one stream for the music and the cinematics;
- the loudness of an entity's voice (lip sync);
- render a block without the device (the video recorder).

The mixer, the ring shared with the device, `s_mixahead`, `s_mixPreStep`, the four `s_sdl*`, the OpenAL and EAX path and the Xing decoder go with the old code; `s_khz` goes if the library takes the device's rate (to decide with the prototype).

## 6. Options

| option | for | against | CPU, memory, latency (estimates) |
|---|---|---|---|
| **A. OpenAL Soft** (+ a one-header MP3 decoder) | the reverb as the game's data describes it (EAX parameters), occlusion, HRTF; mature, used by an idTech port for the same purpose; every system | two libraries; LGPL (accepted by the GPLv2, the sources shipped with the game's); a large library; its own device code beside SDL's | mixes once what is played, on its own thread; latency about 10 to 20 ms (device period; to measure); the reverb's cost per voice to measure |
| **B. miniaudio** | one file, no dependency, decoding included, public domain; can sit under SDL 3's audio | the reverb to adapt or write (not EAX's parameters); occlusion to build from its filters | the same model; its reverb node's cost to measure |
| **C. FAudio** | on SDL 3's audio; XAudio2's reverb and 3D | XAudio2's verbose interface; no decoding | to measure |
| **D. keep the game's mixer, pulled by SDL 3** (option (a) of P5) | no new library | the reverb to write anyway; 11,400 lines of old code kept | once what is played, latency of the device block |

**Recommendation**: prototype A and B (section 7), then choose with the numbers; A is favoured for the reverb, the hardest feature to keep, B if its reverb sounds right and one file matters more.

**The reverb's data** (both libraries): a tool reads the 23 multiplayer `.eal` files once (through EalMan, or a reader of our own from the format's documentation, checked against EalMan) and writes, per map, the zones and their presets in a file of this game, shipped in a ModernJK pk3 (owner: acceptable); a map without that file has no reverb, or one preset for the whole map (to decide). Computing the reverb from the map's geometry at load, as Steam Audio can, is the other way: heavier, and the game's sound designers' choices would be lost.

## 7. The measurements that would decide

None is done: each needs a download, which the owner approves first. With OpenAL Soft 1.25.2 and miniaudio (the latest 0.11 release, two weeks old or more), built here with clang:

1. A test program plays the game's sounds as a busy game does (32 voices in 3D, loops with Doppler, a music stream), with and without the reverb: the CPU per second of sound, on the owner's machine and on the Debian VM.
2. The latency: the time from the call that starts a sound to the samples leaving the library, and the device period each library chooses on Windows and Linux.
3. The reverb: the `mp/ffa3` and `mp/ctf1` environments read from their `.eal` files with EalMan, applied in A (EFX "EAX reverb") and in B; listened to by the owner.
4. The binary size and the build on Windows (MSVC, clang) and Linux (GCC).
5. Whether EalMan reads the game's 58 files (its one-geometry-set limit).

## 8. Decisions for the owner

- **A1**: which library, after the prototypes (A or B recommended to prototype; C and D listed).
- **A2**: the reverb's zones from the `.eal` files converted into a file of this game in its pk3 (recommended); maps without a file: no reverb, or one preset per map.
- **A3**: HRTF (3D for headphones), which OpenAL Soft offers: on when the system reports headphones, off, or a setting.
- **A4**: the go to download OpenAL Soft 1.25.2, miniaudio and EalMan for the prototypes of section 7.

## 9. What was not read or verified

- The `.eal` files' content (one geometry set or several), and what `EAXMan.dll` does with the apertures.
- miniaudio's reverb node (its model and parameters), its latest version number and its PipeWire support (PipeWire serves PulseAudio's clients); OpenAL Soft's release dates and its SDL backend; FAudio's reverb; SDL3_mixer's positional audio and effects; SoLoud's activity.
- No library was built or run; every CPU and latency figure in this document is an estimate.
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

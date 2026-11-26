# ModernJK

ModernJK is a fork of [OpenJK](https://github.com/JACoders/OpenJK) that modernizes the multiplayer game of Jedi Academy: a new network codec in both directions, a fixed 100 Hz tick with the game's rules kept at their 20 Hz pace, the same results on Windows and Linux (one math library, one random generator), network settings fixed or computed instead of set by the player, and every library built with the project. It is not compatible with OpenJK or the base game: a ModernJK client does not play on their servers, and their demos do not play on ModernJK. **Backward compatibility is not a goal**: the protocol, the file formats, the settings and the game's own code change wherever that gives a better result, and the bugs found on the way are fixed even when the fix changes how the game plays; every change is listed in [CHANGES.md](docs/modernization/CHANGES.md).

The work is in progress and centred on the multiplayer game. The single-player game of Jedi Academy is built too, from the same sources and with the same build changes (C++23, 64-bit, the bugs that the compilers' warnings showed fixed); its game is otherwise OpenJK's, and it is only started and played briefly, not tested in depth. The single-player game of Jedi Outcast is built too, from the same sources; it was first started on 23 November 2026, its story begun, and is not tested in depth.

## Documentation

- [What ModernJK changes compared with OpenJK](docs/modernization/CHANGES.md), with the delays before and now.
- [The project's guidelines](GUIDELINES.md): the rules for the code, the documents and the changes.
- [The documentation of the work](docs/modernization/README.md): the study, the plan, the progress and the design surveys.

## Running

ModernJK needs the game data of Jedi Academy (the `base` folder of the `GameData` folder). The multiplayer client is `modernjk.x86_64` (`modernjk.x86_64.exe` on Windows) and the dedicated server `modernjkded.x86_64`; their modules go in the `ModernJK` folder next to them or in `base`. The configuration is `modernjk.cfg` (`modernjk_server.cfg` for the dedicated server), in `~/.local/share/modernjk` on Linux, and on Windows in the game's folder with the builds of the presets (portable) or in `Documents\My Games\ModernJK` otherwise. A configuration of OpenJK (`openjk.cfg`) is not read: copy it under the new name to keep it.

The single-player game is `modernjk_sp.x86_64` (`modernjk_sp.x86_64.exe` on Windows), with its renderer `rdsp-vanilla_x86_64` next to it and its game module `jagamex86_64` in the `ModernJK` folder; its configuration is `modernjk_sp.cfg`, in the same folder as the multiplayer one.

## Building

CMake presets, one per system, compiler and build type. The libraries are in `libs/` and built with the project; on Linux, OpenGL comes from the system, and the development files of X11, Wayland and the sound servers too, whose libraries SDL 3 and OpenAL Soft open at run time. Run the commands from the repository's folder; the programs and modules go to `build/<preset>/bin`.

### Prerequisites

Common:

| what | version |
|---|---|
| CMake | 4.5.0 or later |
| Ninja | any recent release (1.13.2 tested) |

Windows:

| what | version |
|---|---|
| MSVC (Visual Studio 2026) | 19.52.37029 or later |
| or clang with MinGW | llvm-mingw 20261104 (clang 23) tested |

MSVC builds from Visual Studio's x64 environment, for example in a PowerShell:

```
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
& "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\Launch-VsDevShell.ps1" -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
```

The programs built with llvm-mingw need its `bin` folder in the `PATH` to run (`libc++.dll`, `libunwind.dll`).

Linux:

| what | version |
|---|---|
| GCC or Clang | GCC 15, Clang 23 tested |
| OpenGL, development files (the client only) | the system's |
| PipeWire, PulseAudio, ALSA and D-Bus, development files, and pkg-config (the client only: OpenAL Soft's backends, required) | the system's (PipeWire 0.3.23 or later) |
| X11 (with Xext, Xcursor, Xfixes, Xi, Xrandr), Wayland, libdecor, xkbcommon, EGL and udev, development files (the client only: SDL 3's video drivers and gamepads, required) | the system's (Wayland 1.18 or later) |

On Debian or Ubuntu, OpenGL and the sound servers, then X11 and Wayland:

```
sudo apt install libgl1-mesa-dev libpipewire-0.3-dev libpulse-dev libasound2-dev libdbus-1-dev pkg-config
sudo apt install libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxkbcommon-dev libwayland-dev libdecor-0-dev libegl1-mesa-dev libudev-dev
```

### Commands

| command | what it does |
|---|---|
| `cmake --workflow --preset <preset>` | configures, builds and runs the tests |
| `cmake --build --preset <preset>` | builds again after a change |
| `cmake --build --preset <preset> --target clean` | cleans the build |
| `cmake --workflow --preset <preset> --fresh` | builds from scratch, the configuration included |
| `cmake --install build/<preset> --strip` | installs into `build/install/<preset>` (`JediAcademy`, `JediOutcast`) |
| `cmake --build --preset <preset> --target eal_converter` | builds the converter of the game's EAX zone files, out of the other builds (`tools/eal_converter`, its README) |

### Presets

| preset | what it builds |
|---|---|
| `windows-msvc-release`, `windows-msvc-debug` | everything, with MSVC |
| `windows-clang-release`, `windows-clang-debug` | everything, with clang (llvm-mingw) |
| `linux-gcc-release`, `linux-gcc-debug` | everything, with GCC 15 |
| `linux-clang-release` | everything, with Clang 23 |
| `linux-gcc-server` | the dedicated server and the multiplayer game module (the servers' build) |
| `linux-gcc-debug-lto` | everything, in Debug with link-time optimization: a check of the types and declarations across files, which `validate.sh` builds |
| `windows-msvc-asan`, `windows-clang-asan`, `linux-gcc-asan`, `linux-clang-asan` | everything, with AddressSanitizer (and UndefinedBehaviorSanitizer but with MSVC) |
| `windows-clang-fuzz` | the fuzzers: the network codec's and the sound decoders' |
| `windows-clang-tidy`, `windows-clang-cppcheck` | everything, with the report of clang-tidy or cppcheck |

The Release presets build with link-time optimization, the Debug and sanitizer presets without. Everything: the multiplayer client and dedicated server with their renderer and modules, the single-player games of Jedi Academy and Jedi Outcast, a `z_ModernJK.pk3` for each game (from `assets/jka`, installed in `JediAcademy/base`, and `assets/jk2`, in `JediOutcast/base`), the tests, the network codec's benchmarks and tools. The full checks (`validate.sh`, `validate.bat`) are in "How to resume" of [the progress page](docs/modernization/progress/README.md).

### Formatting

The C and C++ code of `src/` and `test/` is laid out by clang-format 23 (`.clang-format`; another major version lays it out differently): `clang-format -i <files>` before a change is handed over, run again until nothing changes. The Linux validation (`validate.sh`) checks it.

## Tools

The programs and scripts beside the game, none of them installed with it:

| tool | where | what it does | how it is built or run |
|---|---|---|---|
| `eal_converter` | `tools/eal_converter` | converts the game's EAX zone files (`eagle/<map>.eal`) into the reverb's zones the sound plays (`assets/jka/eagle`, `assets/jk2/eagle`), each file checked against EalMan | the target `eal_converter`, out of the other builds; `extract.py` takes the zone files out of a game's archives; its [README](tools/eal_converter/README.md) |
| `netcodec_train`, `netcodec_train_client` | `src/codemp/netcodec/tools` | train the network codec's probability tables from recorded traces (`src/codemp/netcodec/src/generated`) | built with the presets (`modernjk_BUILD_TOOLS`); the traces in `docs/modernization/data/traces` |
| `netcodec_bench_codec`, `netcodec_bench_size`, `netcodec_bench_client`, `netcodec_bench_rans` | `src/codemp/netcodec/bench` | the codec's speed and sizes against OpenJK's coder | built with the presets (`modernjk_BUILD_BENCHMARKS`) |
| `validate.sh`, `validate.bat` | `scripts` | the full checks of every preset on Linux and Windows | "How to resume" of [the progress page](docs/modernization/progress/README.md) |
| `ab_*` | `src/codemp/netcodec/scripts` | the A/B timings of two builds | "How to resume" of [the progress page](docs/modernization/progress/README.md) |
| the study's and the surveys' tools | `docs/modernization/tools` | the scripts behind every measurement of the documents: the scripted bot scenarios and their goldens (`gamestats`), the loudness and true peak (`soundlevel`), the sound libraries and decoders (`audiobench`, `audiodec`), the math and random numbers (`mathlib`, `rng`), the inventories of the code (`printinv`, `soundinv`), the culling's prototype (`visibility`) | run by hand, each with its usage at its top; not built by the presets; by subject in [the documentation of the work](docs/modernization/README.md) |

## The use of AI

The modernization is done with an AI assistant, Claude, by Anthropic: it searches the sources, measures, and writes the code, the tools and the documents. Every decision is taken by a person, written in [the plan](docs/modernization/PLAN.md) with its date; every change is built on the three compilers, checked by the tests and the scripted scenarios, tried in game where it changes the game, and committed by a person.

## Credits

- Star Wars Jedi Knight: Jedi Academy and Star Wars Jedi Knight II: Jedi Outcast, by Raven Software, published by LucasArts and Activision; Raven Software and Activision released their source code under the GPLv2 in 2013 ([Jedi Academy](https://github.com/jedis/jediacademy), [Jedi Outcast](https://github.com/jedis/jedioutcast)). The game data is theirs and is not part of this repository.
- The engine of [Quake III Arena](https://github.com/id-Software/Quake-III-Arena), by id Software, released under the GPLv2 in 2005, on which both games are built, and [ioquake3](https://github.com/ioquake/ioq3)'s work on it.
- [OpenJK](https://github.com/JACoders/OpenJK), of which ModernJK is a fork.

The libraries in `libs/`, each under its own licence:

| library | by | used for | licence |
|---|---|---|---|
| [zlib-ng](https://github.com/zlib-ng/zlib-ng) | Jean-loup Gailly, Mark Adler and the zlib-ng contributors | reading the game's archives (`.pk3`) | zlib |
| [Wuffs](https://github.com/google/wuffs) | the Wuffs Authors | decoding the JPEG, PNG and TGA images | MIT |
| [stb_image_write](https://github.com/nothings/stb) | Sean Barrett | writing the screenshots (JPEG, PNG) | public domain |
| [LLVM libc](https://github.com/llvm/llvm-project/tree/main/libc) (its math functions and text-to-float parser) | the LLVM contributors | the same floating-point results on every platform and compiler | Apache 2.0 with LLVM Exceptions |
| [gsl-lite](https://github.com/gsl-lite/gsl-lite) | Martin Moene | spans in the single-player code | MIT |
| [{fmt}](https://github.com/fmtlib/fmt) | Victor Zverovich and the {fmt} contributors | every printed and formatted text, its format checked at compile time | MIT |
| [SDL](https://github.com/libsdl-org/SDL) | Sam Lantinga and the SDL contributors | the window and the input (SDL 3, linked statically) | zlib |
| [OpenAL Soft](https://github.com/kcat/openal-soft) | Chris Robinson and the OpenAL Soft contributors | playing the sound: mixing, 3D, reverb, the sound device | LGPL 2 or later |
| [libFLAC](https://github.com/xiph/flac) | the Xiph.Org Foundation and the FLAC contributors | decoding the FLAC sounds | BSD (Xiph.Org) |
| [dr_mp3 and dr_wav](https://github.com/mackron/dr_libs) | David Reid (dr_mp3's decoder from lieff's minimp3) | decoding the MP3 and WAV sounds | public domain or MIT-0 |
| [stb_vorbis](https://github.com/nothings/stb) | Sean Barrett | decoding the Ogg Vorbis sounds | public domain or MIT |
| [EalMan](https://github.com/senneschall/EalMan) | senneschall | reading the game's EAX zone files (`eagle/<map>.eal`) for their converter, `tools/eal_converter`, which writes the reverb's zones the game plays; the game does not link it | LGPL 2.1 |

The methods the code implements:

- the random numbers (`src/shared/qcommon/q_rand.cpp`): [xoshiro128++](https://prng.di.unimi.it/), by David Blackman and Sebastiano Vigna; [SplitMix64](https://doi.org/10.1145/2660193.2660195), by Guy Steele, Doug Lea and Christine Flood, for the seeds; integers in a range by [Daniel Lemire's unbiased method](https://arxiv.org/abs/1805.10941);
- the reverb's zones (`src/shared/sound/zones`): the zone of a point and a sound's obstruction and occlusion as Creative's EAX Manager computed them, after EalMan's rewrite of it (checked equal on every zone file by `tools/eal_converter`); EAX's reverb, obstruction and occlusion turned into EFX's values as OpenAL Soft's own EAX layer turns them;
- the network codec's entropy coder (`src/codemp/netcodec`): [rANS](https://arxiv.org/abs/1311.2540), the asymmetric numeral systems of Jarek Duda, after the design of Fabian Giesen's [ryg_rans](https://github.com/rygorous/ryg_rans), whose `rans_byte.h`, public domain, is in netcodec's benchmark for comparison.

The study of the network codec used client demos recorded and supplied by a player. The prototype of the anti-wallhack culling (`docs/modernization/tools/visibility`, not part of the game) uses [tinybvh](https://github.com/jbikker/tinybvh), by Jacco Bikker, and [bvh](https://github.com/madmann91/bvh), by madmann91, both MIT, which are not in this repository.

## License

GPLv2, as OpenJK: see [LICENSE.txt](LICENSE.txt); the copyright notices of the files are kept, and every file ModernJK changed carries `Copyright (C) 2026, ModernJK contributors` (the GPLv2's section 2(a) asks the modified files to say so). The libraries in `libs/` keep their own licences (Credits).

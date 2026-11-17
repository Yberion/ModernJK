# ModernJK

ModernJK is a fork of [OpenJK](https://github.com/JACoders/OpenJK) that modernizes the multiplayer game of Jedi Academy: a new network codec in both directions, a fixed 100 Hz tick with the game's rules kept at their 20 Hz pace, the same results on Windows and Linux (one math library, one random generator), network settings fixed or computed instead of set by the player, and every library built with the project. It is not compatible with OpenJK or the base game: a ModernJK client does not play on their servers, and their demos do not play on ModernJK. **Backward compatibility is not a goal**: the protocol, the file formats, the settings and the game's own code change wherever that gives a better result, and the bugs found on the way are fixed even when the fix changes how the game plays; every change is listed in [CHANGES.md](docs_rewrite/modernization/CHANGES.md).

The work is in progress and centred on the multiplayer game. The single-player game of Jedi Academy is built too, from the same sources and with the same build changes (C++20, 64-bit, the bugs that the compilers' warnings showed fixed); its game is otherwise OpenJK's, and it is only started and played briefly, not tested in depth. The single-player game of Jedi Outcast is kept compiling, off by default, and never run.

## Documentation

- [What ModernJK changes compared with OpenJK](docs_rewrite/modernization/CHANGES.md), with the delays before and now.
- [The documentation of the work](docs_rewrite/modernization/README.md): the study, the plan, the progress and the design surveys.

## Running

ModernJK needs the game data of Jedi Academy (the `base` folder of the `GameData` folder). The multiplayer client is `modernjk.x86_64` (`modernjk.x86_64.exe` on Windows) and the dedicated server `modernjkded.x86_64`; their modules go in the `ModernJK` folder next to them or in `base`. The configuration is `modernjk.cfg` (`modernjk_server.cfg` for the dedicated server), in `Documents\My Games\ModernJK` on Windows and `~/.local/share/modernjk` on Linux. A configuration of OpenJK (`openjk.cfg`) is not read: copy it under the new name to keep it.

The single-player game is `modernjk_sp.x86_64` (`modernjk_sp.x86_64.exe` on Windows), with its renderer `rdsp-vanilla_x86_64` next to it and its game module `jagamex86_64` in the `ModernJK` folder; its configuration is `modernjk_sp.cfg`, in the same folder as the multiplayer one. A known issue (16 November 2026): in game, the mouse does not turn the view (the menus work); it is left for the new input layer of plan step 15.

## Building

CMake with MSVC, GCC or Clang, in C++20, for 64-bit Windows and Linux (macOS is kept portable, not built). The libraries are in `lib/`, except on Linux SDL2 and OpenGL, which come from the system.

What is built is chosen with CMake options, all on by default except Jedi Outcast: the multiplayer client, server, renderer and modules (`BuildMPEngine`, `BuildMPDed`, `BuildMPRdVanilla`, `BuildMPGame`, `BuildMPCGame`, `BuildMPUI`), the single-player game of Jedi Academy (`BuildSPEngine`, `BuildSPGame`, `BuildSPRdVanilla`), and that of Jedi Outcast (`BuildJK2SPEngine`, `BuildJK2SPGame`, `BuildJK2SPRdVanilla`, off). The commands below build the single-player game with the multiplayer one, except the Linux dedicated server's build. The full checks (`validate.sh`, `validate.bat`) and the other commands are in "How to resume" of [the progress page](docs_rewrite/modernization/progress/README.md).

### Linux: Ubuntu 22.04 with GCC 15 and Clang 23

Ubuntu 22.04 is the build system because its glibc (2.35) is older than the servers' (Debian 12 has 2.36): a binary runs only on a glibc at least as recent as the one it was built with. GCC 15 comes from the Ubuntu toolchain team's PPA and Clang 23 from LLVM's own repository (`llvm.sh` adds it and installs `clang-23`, `lld-23` and `lldb-23`); the system's `gcc` (11.4) and `clang` (14) are not used.

```
sudo apt install software-properties-common lsb-release wget gnupg
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update
sudo apt install build-essential cmake gcc-15 g++-15 libsdl2-dev libgl1-mesa-dev
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 23
```

`libsdl2-dev` and `libgl1-mesa-dev` are only for the client (the dedicated server needs neither). GCC builds the dedicated server and the multiplayer game module (the servers' build), then every target; Clang builds every target too. From the repository's folder (`ModernJK`):

```
cmake -S . -B build-modernjk/gcc-ded -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-15 -DCMAKE_CXX_COMPILER=g++-15 -DBuildMPEngine=OFF -DBuildMPRdVanilla=OFF -DBuildMPCGame=OFF -DBuildMPUI=OFF -DBuildSPEngine=OFF -DBuildSPGame=OFF -DBuildSPRdVanilla=OFF
cmake --build build-modernjk/gcc-ded --parallel "$(nproc)"
cmake -S . -B build-modernjk/gcc-mp -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc-15 -DCMAKE_CXX_COMPILER=g++-15
cmake --build build-modernjk/gcc-mp --parallel "$(nproc)"
cmake -S . -B build-modernjk/clang-mp -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-23 -DCMAKE_CXX_COMPILER=clang++-23
cmake --build build-modernjk/clang-mp --parallel "$(nproc)"
```

### Windows: MSVC

Visual Studio 2026 with the workload "Desktop development with C++" (the MSVC compiler, a Windows 10/11 SDK and CMake). SDL2 for Windows is in `lib/SDL2`. From a "Developer PowerShell for VS 2026", or any prompt where its `cmake` is found, in the repository's folder:

```
cmake -S . -B build-modernjk\msvc-x64 -G "Visual Studio 18 2026" -A x64 -DBuildPortableVersion=ON
cmake --build build-modernjk\msvc-x64 --config Release
cmake --install build-modernjk\msvc-x64 --config Release --prefix build-modernjk\install-x64
```

### Windows: clang (llvm-mingw)

Nothing installed: three folders unpacked and put in the `PATH` of the shell that builds (here Git Bash, which the `.sh` scripts also need), opened in the repository's folder.

- [llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases), the `ucrt-x86_64` archive (the project uses the 20261104 release, clang 23);
- [CMake](https://cmake.org/download/) (4.4.5 here), the Windows x64 zip;
- [Ninja](https://github.com/ninja-build/ninja/releases), `ninja-win.zip`.

```
export PATH="/c/path/to/llvm-mingw-ucrt-x86_64/bin:/c/path/to/cmake/bin:/c/path/to/ninja:$PATH"
cmake -G Ninja -S . -B build-netcodec/modernjk-clang -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
ninja -C build-netcodec/modernjk-clang
```

## Credits

- Star Wars Jedi Knight: Jedi Academy and Star Wars Jedi Knight II: Jedi Outcast, by Raven Software, published by LucasArts and Activision; Raven Software and Activision released their source code under the GPLv2 in 2013 ([Jedi Academy](https://github.com/jedis/jediacademy), [Jedi Outcast](https://github.com/jedis/jedioutcast)). The game data is theirs and is not part of this repository.
- The engine of [Quake III Arena](https://github.com/id-Software/Quake-III-Arena), by id Software, released under the GPLv2 in 2005, on which both games are built, and [ioquake3](https://github.com/ioquake/ioq3)'s work on it.
- [OpenJK](https://github.com/JACoders/OpenJK), of which ModernJK is a fork.

The libraries in `lib/`, each under its own licence:

| library | by | used for | licence |
|---|---|---|---|
| [zlib-ng](https://github.com/zlib-ng/zlib-ng) | Jean-loup Gailly, Mark Adler and the zlib-ng contributors | reading the game's archives (`.pk3`) | zlib |
| [Wuffs](https://github.com/google/wuffs) | the Wuffs Authors | decoding the JPEG, PNG and TGA images | MIT |
| [stb_image_write](https://github.com/nothings/stb) | Sean Barrett | writing the screenshots (JPEG, PNG) | public domain |
| [LLVM libc](https://github.com/llvm/llvm-project/tree/main/libc) (its math functions and text-to-float parser) | the LLVM contributors | the same floating-point results on every platform and compiler | Apache 2.0 with LLVM Exceptions |
| [gsl-lite](https://github.com/gsl-lite/gsl-lite) | Martin Moene | spans in the single-player code | MIT |
| [SDL](https://github.com/libsdl-org/SDL) | Sam Lantinga and the SDL contributors | the window, the input and the sound output (the copy in `lib/` for Windows; Linux uses the system's) | zlib |

The methods the code implements:

- the random numbers (`shared/qcommon/q_rand.cpp`): [xoshiro128++](https://prng.di.unimi.it/), by David Blackman and Sebastiano Vigna; [SplitMix64](https://doi.org/10.1145/2660193.2660195), by Guy Steele, Doug Lea and Christine Flood, for the seeds; integers in a range by [Daniel Lemire's unbiased method](https://arxiv.org/abs/1805.10941);
- the network codec's entropy coder (`codemp/netcodec`): [rANS](https://arxiv.org/abs/1311.2540), the asymmetric numeral systems of Jarek Duda, after the design of Fabian Giesen's [ryg_rans](https://github.com/rygorous/ryg_rans), whose `rans_byte.h`, public domain, is in netcodec's benchmark for comparison.

The study of the network codec used client demos recorded and supplied by a player. The prototype of the anti-wallhack culling (`docs_rewrite/modernization/tools/visibility`, not part of the game) uses [tinybvh](https://github.com/jbikker/tinybvh), by Jacco Bikker, and [bvh](https://github.com/madmann91/bvh), by madmann91, both MIT, which are not in this repository.

## License

GPLv2, as OpenJK: see [LICENSE.txt](LICENSE.txt); the copyright notices of the files are kept. The libraries in `lib/` keep their own licences (Credits).

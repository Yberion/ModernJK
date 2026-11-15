# ModernJK

ModernJK is a fork of [OpenJK](https://github.com/JACoders/OpenJK) that modernizes the multiplayer game of Jedi Academy: a new network codec in both directions, a fixed 100 Hz tick with the game's rules kept at their 20 Hz pace, the same results on Windows and Linux (one math library, one random generator), network settings fixed or computed instead of set by the player, and every library built with the project. It is not compatible with OpenJK or the base game: a ModernJK client does not play on their servers, and their demos do not play on ModernJK.

The work is in progress on the branch `feature/poc-netcode`. Only the multiplayer game is worked on; the single-player and Jedi Outcast code is kept compiling and is not tested.

## Documentation

- [What ModernJK changes compared with OpenJK](docs_rewrite/modernization/CHANGES.md), with the delays before and now.
- [The documentation of the work](docs_rewrite/modernization/README.md): the study, the plan, the progress and the design surveys.

## Running

ModernJK needs the game data of Jedi Academy (the `base` folder of the `GameData` folder). The multiplayer client is `modernjk.x86_64` (`modernjk.x86_64.exe` on Windows) and the dedicated server `modernjkded.x86_64`; their modules go in the `ModernJK` folder next to them or in `base`. The configuration is `modernjk.cfg` (`modernjk_server.cfg` for the dedicated server), in `Documents\My Games\ModernJK` on Windows and `~/.local/share/modernjk` on Linux. A configuration of OpenJK (`openjk.cfg`) is not read: copy it under the new name to keep it.

## Building

CMake, with MSVC, GCC or Clang, on Windows and Linux (x64; x86 while it costs nothing). The compilers and checks used are listed in section 27 of [CHANGES.md](docs_rewrite/modernization/CHANGES.md); the commands in "How to resume" of [the progress page](docs_rewrite/modernization/progress/README.md).

## License

GPLv2, as OpenJK: see [LICENSE.txt](LICENSE.txt). ModernJK is built on the work of the OpenJK contributors ([list](https://github.com/JACoders/OpenJK/graphs/contributors)); their copyright notices are kept in the files.

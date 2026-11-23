# libFLAC (vendored source tree)

libFLAC decodes the sound's FLAC files (`src/shared/sound/flac.cpp`). It is built by its own `CMakeLists.txt`, from
`Dependencies.cmake` (target `vendored::flac`) (survey and measurements: `docs/modernization/surveys/audio-decoders.md`).

- Source: the release archive `flac-1.5.0.tar.xz` of https://github.com/xiph/flac/releases/tag/1.5.0 (11 February
  2025), SHA-256 `f2c1c76592a82ffff8413ba3c4a1299b6c7ab06c734dee03fd88630485c2b920`. Kept unchanged: `CMakeLists.txt`,
  `cmake/`, `config.cmake.h.in`, `flac-config.cmake.in`, `include/`, `src/CMakeLists.txt`, `src/libFLAC/` and the
  folders of `src/share/` that `src/CMakeLists.txt` adds whatever its options (`getopt`, `grabbag`,
  `replaygain_analysis`, `replaygain_synthesis`, `utf8`) with `win_utf8_io` (libFLAC's file names on Windows), the
  licences, `AUTHORS` and `CHANGELOG.md`. Left out: the programs (`src/flac`, `src/metaflac`), libFLAC++, the tests,
  `src/utils`, `examples/`, `doc/`, `man/`, `oss-fuzz/`, the autotools files (`configure`, `Makefile.am`,
  `Makefile.in`, `*.m4` and the others) and its `README.md` (this file in its place).
- Build (`Dependencies.cmake`): off: libFLAC++, the programs, examples, tests and documents, the pkg-config and CMake
  package files, Ogg FLAC (so no libogg), the encoder's threads, `_FORTIFY_SOURCE` and the stack protector (the project
  builds without hardening). The encoder is compiled with the library and left out at the link. `FLAC__OVERFLOW_DETECT`, which its CMake file defines in Debug and RelWithDebInfo, undefined (it writes a line on the standard error at every overflow of a damaged file), and its `-Winline` turned off (GCC's reports of the inlining it declined, which link-time optimization repeats at the link, where the project's `-Werror` applies); under UndefinedBehaviorSanitizer, its predictors' signed integer overflows not reported (libFLAC's own position, `src/libFLAC/lpc.c`). The project's options
  are given to it (`modernjk_options`: the floating-point rules, and the sanitizers of the presets that have them).
- Licence: libFLAC is under the Xiph.Org BSD licence (`COPYING.Xiph`), which can be combined with the GNU GPL version
  2; `COPYING.GPL`, `COPYING.LGPL` and `COPYING.FDL` cover the programs, libFLAC++ and the documents, left out.
- To update it: replace the kept files with those of a newer release, build, run `ctest` (`modernjk.sound`) and
  `docs/modernization/tools/audiodec/check_run.sh` on the game's files.

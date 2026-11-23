# OpenAL Soft (vendored source tree)

OpenAL Soft plays the client's sound: the mixing, the 3D, the EFX reverb and the system's sound device. It is built as
a static library by its own `CMakeLists.txt`, from `Dependencies.cmake` (target `vendored::openal`), for the engines
that open a window (survey and measurements: `docs/modernization/surveys/audio-libraries.md`).

- Source: https://github.com/kcat/openal-soft at the tag `1.25.2` (commit `b2c48f7718ef3fcf67921a8b6534c4914e328970`,
  12 May 2026), exported with `git archive`. Kept unchanged but for the patch below: the library's sources (`al/`,
  `alc/`, `core/`, `common/`, `include/`, `modules/`, `alsoft-modules/`, `router/`, `resources/`), `cmake/`,
  `CMakeLists.txt` and the files it configures, `examples/common/` (a library of the examples that `CMakeLists.txt`
  declares whatever its options), its own {fmt} 11.2.0 (`fmt-11.2.0/`, without `doc/`, `.github/`, `.gitignore` and
  `.clang-format`) and GSL (`gsl/`, without `docs/`, `.clang-format`, `CMakeSettings.json`, `CONTRIBUTING.md` and
  `SECURITY.md`), the licences, `ChangeLog` and `alsoftrc.sample` (the documentation of its configuration file). Left
  out: `utils/`, `examples/` but `common/`, `registry/`, `docs/`, `.github/`, `tests/`, `presets/`, `hrtf/` (the HRTF
  data: HRTF is off), `configs/`, `logging/`, `build/`, its `README.md` (this file in its place), `.travis.yml`,
  `appveyor.yml`, `XCompile.txt`, `XCompile-Android.txt`, `.clang-tidy` and `.gitignore`.
- Build (`Dependencies.cmake`): a static library; off: its utilities, examples, tests and install, the git version, the
  legacy EAX extensions (the reverb goes through EFX), the embedded HRTF data, its C++ modules (the project scans for
  none) and the WaveFile backend. On Windows WASAPI is required, DirectSound and WinMM off; on Linux PipeWire,
  PulseAudio, ALSA and RTKit are required (their development packages: the repository's `README.md`), OSS, Solaris,
  sndio, JACK and PortAudio off, and the libraries of the sound servers and of D-Bus are opened at run time
  (`ALSOFT_DLOPEN`, its default), so that a player needs only the system's own. A required backend missing on the build
  machine stops the configuration, rather than giving a binary without it. Its {fmt} is its own (the namespace
  `fmt::v11`, the target `alsoft.fmt`), beside the project's 12.2.1 (`fmt::v12`): no clash.
  The warnings its own code gives with its own flags are switched off one by one on its target, each with its reason (`Dependencies.cmake`): with MSVC, C4875 (its GSL's `[[gsl::suppress]]` without quotes) and C4267 (one `std::pair` in `core/voice.cpp`); with GCC, `-Wrange-loop-construct` (one loop of `al/source.cpp`) and `-Wmaybe-uninitialized` (libstdc++'s `ranges::join_view`).
- Patches, in `patches/`, applied from the repository's root with `git apply`:
  - `0001-unique-ptr-dereference.patch` (15 November 2026): `alc/alc.cpp` takes the address of
    `std::unique_ptr::operator*`, which the standard does not let a program rely on, and clang 23's libc++ refuses it;
    a lambda in its place. Fixed on OpenAL Soft's `master` after 1.25.2 (`al::dereference`, seen on 23 November 2026):
    drop it at the next release.
  - `0002-no-werror-function-effects.patch` and `0003-function-effects-with-libcxx-only.patch` (23 November 2026): OpenAL Soft's commits `681d049c` and `fe0ffd63` of 13 May 2026, taken as they are. 1.25.2 turns clang's `-Wfunction-effects` on with clang 20 and later, as an error even without `ALSOFT_WERROR`; with GCC's standard library (clang on Linux), `std::variant::emplace` is not marked non-blocking and `alc/alu.cpp` fails to build. The commits keep the warning to libc++ and no longer make it an error. Drop them at the next release.
- Licence: the GNU LGPL version 2 or later (`COPYING`), which the GNU GPL version 2 accepts; parts under the BSD
  licence (`BSD-3Clause`) and pffft's (`LICENSE-pffft`); its {fmt} and GSL under the MIT licence.
- To update it: replace the kept files with those of a newer tag, apply the patches again or drop them, build, and run
  `ctest` (`modernjk.sound` plays a sound on its loopback device).

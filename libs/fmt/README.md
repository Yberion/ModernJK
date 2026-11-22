# {fmt} (vendored release files)

The formatting library behind the project's printing and formatting functions: the format strings are checked at
compile time against their arguments, and the output is the same on every platform and compiler (independent of the
locale, the floats written by one algorithm). The code calls the project's own functions, which use {fmt} inside;
netcodec's tools, tests and benchmarks, which do not depend on the engine, call it directly.

- Source: https://github.com/fmtlib/fmt at the tag `12.2.1` (commit `208219daff58f2a7438f6931cc2ddc358acf33ef`,
  7 November 2026). The files kept: `include/fmt/base.h`, `include/fmt/format.h`, `include/fmt/format-inl.h`,
  `src/format.cc` and `LICENSE`, the minimum {fmt}'s README names, built as a static library from `src/format.cc`
  (its default mode, rather than `FMT_HEADER_ONLY`); unchanged but for the patches below. Left out: the other headers
  (`args.h`, `chrono.h`, `color.h`, `compile.h`, `core.h`, `fmt-c.h`, `os.h`, `ostream.h`, `printf.h`, `ranges.h`,
  `std.h`, `xchar.h`), `src/fmt.cc` (the C++20 module), `src/fmt-c.cc` (the C interface), `src/os.cc`, `doc/`,
  `support/`, `test/`, its `CMakeLists.txt`, `ChangeLog.md`, `CONTRIBUTING.md` and `README.md`. A header left out
  comes back from the same tag when the code needs it.
- Patches, in `patches/`, applied from the repository's root with `git apply`:
  - `0001-msvc-strlen.patch` (22 November 2026): with MSVC 19.25 and later, `basic_string_view(const char*)` takes
    the length of a C string with the C library's `strlen` outside constant evaluation (tested with
    `__builtin_is_constant_evaluated`, {fmt}'s own test being always false before C++20). {fmt} uses
    `__builtin_strlen` when `__has_builtin` reports it or with GCC and clang, and otherwise a loop counting one
    character at a time; MSVC has `__builtin_strlen` but not `__has_builtin`, so it gets the loop. `strlen` rather
    than `__builtin_strlen`: MSVC generates the same code for both, and the second is missing in older versions. A
    text of 300 characters formatted in 86 ns without the patch, 31 ns with it, `snprintf`'s 26 to 32 (`fmtbench`,
    MSVC 19.52, x64); no change on x86, where MSVC writes `strlen` as a loop. The same code is on {fmt}'s `main`;
    the patch is proposed to {fmt}'s authors (their issue 5038 and pull request 5040;
    `docs/modernization/progress/step-22-fmt.md` section 6): drop it once a release counts otherwise.
- Licence: MIT (`LICENSE`), which can be combined with the GNU GPL version 2.
- To update it: replace the kept files with those of a newer tag, apply the patches again, build, and run
  `docs/modernization/tools/fmtbench/run.sh` and `run.bat` (the outputs checked against `snprintf`'s; with a copy of
  the new tag without the patches in `build/fmt-ref`, the patched and unpatched builds timed alternately).

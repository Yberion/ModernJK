# ModernJK guidelines

The rules for the project's code, documents and changes. They hold for every tree (`src/codemp`, `src/code`, `src/codeJK2`, `src/shared`, `test/`) unless a section says otherwise. Each rule was decided on a date written in [the plan](docs/modernization/PLAN.md) or in [the progress page](docs/modernization/progress/README.md); the way the work is done (sessions, machines, measurements, validations) is described there, in "Working practices" and "How to resume".

## 1. Priorities

- **Precision, reactivity, equality.** The least delay, the same results for every player, no player favoured by their machine, their frame rate or their connection. No loss of precision.
- **No backward compatibility.** The protocol, the file formats, the settings and the game's code change wherever that gives a better result. Compatibility with mods is never a criterion. The bugs found on the way are fixed, even when the fix changes how the game plays.
- **Modern designs, from sources.** A design question starts with a survey of what recent games and libraries do, with sources labelled primary, secondary or community; the JKA mods only tell which code a change touches. Nothing is kept because it exists: a well-thought design is wanted even if it means starting again from zero.
- **For every player.** A gain on the client is judged at several frame rates (100, 125, 144 with vsync, 333), never for one machine.
- **Speed first.** A smaller size or a cleaner code is not taken at the cost of speed: a cleanup shows no performance regression, measured.

## 2. Languages and platforms

- C++23 everywhere, netcodec included; C (C17) only in FreeAmp's MP3 decoder (`mp3code`).
- 64-bit Windows, Linux and macOS (macOS is not built for now; its code stays portable), on x86-64 and arm64. 32-bit and the other platforms were removed on 16 November 2026 and do not come back.
- The compilers: MSVC (Visual Studio 2026), GCC 15 and clang 23. Every change builds on the three without a warning.
- No exceptions in netcodec.

## 3. Layout

- The C and C++ code of `src/` and `test/` is laid out by clang-format 23 with the root's `.clang-format` (the LLVM style, 4 spaces, 160 columns, the layout rules of the C++ Core Guidelines' NL section that clang-format can apply). Another major version lays it out differently: use version 23 only.
- Run `clang-format -i` on the files changed until nothing changes (it can need a second run). The Linux validation checks the whole tree (`clang-format-23 --dry-run --Werror`).
- `libs/` and the generated tables (`src/codemp/netcodec/src/generated/`) are left out (`.clang-format-ignore`).
- `.editorconfig` gives the C and C++ files spaces; the CMake, shell and batch files keep tabs.
- A tool that reads the sources (such as `gen_fields.py` or `unsequenced.py`) must not depend on their layout, and a patch that applies to the tree is made again after a change of layout.

## 4. Includes

After SF.12 of the C++ Core Guidelines:

- A file below the including file's folder (the same folder or a subfolder) is included in quotes with its relative path: `"x.h"`, `"sub/x.h"`. Never `../`.
- Every other file in angles, from the roots the target declares: its tree's root and `src/`.
  - a header of the file's tree: `<qcommon/q_shared.h>`, `<game/g_local.h>`;
  - the shared code: `<shared/qcommon/q_math.h>`;
  - in `src/shared`, a header of the tree that compiles the file, resolved by each target: `<qcommon/qcommon.h>`;
  - the single player's engine from Jedi Outcast's game module, and from the files of `src/code` that module also compiles: `<code/qcommon/q_shared.h>`;
  - netcodec's public headers: `<netcodec/engine.h>`; netcodec's tests, tools and benchmarks reach its own headers through the roots their targets declare;
  - the libraries and the system headers: `<zlib-ng.h>`, `<shared/math/sin.h>` (LLVM libc), `<cstdint>`, `<windows.h>`.
- No relative path may exist under two include roots of one target, so that an angle include finds one file only.
- The order of the includes is kept as written (`SortIncludes: Never`): some depend on it.

## 5. The C++ Core Guidelines

- netcodec (`src/codemp/netcodec`) follows them entirely: named casts, `noexcept`, `[[nodiscard]]`, `constexpr`, `std::span` for buffers, RAII, initialised members, `assert` for preconditions; unsigned arithmetic on purpose for bit manipulation (C++ Core Guidelines ES.101); clang-tidy at zero on the library.
- The whole project follows the C++ Core Guidelines' Enum section: every enumeration is an `enum class` (Enum.3), its operations written once in `src/shared/qcommon/q_enum.h` (Enum.4), no unnamed enumeration (Enum.6), an underlying type and values only when necessary (Enum.7, Enum.8). Enum.1 and Enum.5 are weighed against the data formats (names turned into text in files) before they apply.
- The whole project follows the C++ Core Guidelines' layout rules (NL), through clang-format (section 3).
- Elsewhere the engine's and the game's code keeps the engine's style; a file is not rewritten to the guidelines unless that is decided.

## 6. Types and state

- **Explicit state over sentinel values.** A value that means two things (a count that is also "not set", a time that is also "never") becomes an enumeration or a flag, not a magic number or a clamp.
- **Enumerations typed in their storage.** The fields, parameters and returns hold the `enum class` itself; a conversion to or from an integer happens only at a boundary: a file, a cvar, the network, a module interface (to an integer with C++23's `std::to_underlying`); a format string takes the enumeration as it is (section 13). Look for the hidden integers too, not only what the compiler reports.
- **One definition for the trees.** A set that the multiplayer and a single player share (constants, flags, limits) has one definition in `src/shared/qcommon`, included by each tree, never two copies.

## 7. Math and random numbers

- The code calls only the entry points of `src/shared/qcommon/q_libm.h` (`Q_sinf`, `Q_sin`, `Q_sqrtf`, `Q_atof`, `Q_ScanFloats`...) and `src/shared/qcommon/q_rand.h` (`Q_irand`, `Q_flrand`, `Q_RandInt` on a `qrand_t`...): never the C library's math functions, `atof`, `sscanf` with `%f`, `M_PI` or `rand()`. A new call of these is a bug: it brings the platforms' differences back.
- A call keeps the precision of the overload the compiler chooses: in C++, `sin` of a `float` is `sinf`, so `Q_sinf`. Pi is `Q_PI`, a `double`.
- netcodec uses LLVM libc directly (`src/numeric.h`), not `q_libm.h`, to stay independent of the engine; its tests and tools keep `std::`.
- Two random draws never sit in one call's arguments or as operands of one operator (their order is not specified): `docs/modernization/tools/rng/unsequenced.py <folders>` lists such statements.
- The calls are found by the syntax tree (clang-tidy's custom checks, [step-18-cxx.md](docs/modernization/progress/step-18-cxx.md) section 6), not by a text search, which misses the overloads and the macros and reads the code of a `/* */` block as live.

## 8. Warnings

- No warning in any build of the three compilers, and every warning of the sets is an error, so that it cannot come back: `modernjk_WARNINGS_AS_ERRORS` is on by default (21 November 2026), turned off only by hand, for a compiler the project was not tested with. A group of warnings added in plan step 23 enters the set once it is fixed.
- Each warning gets the right fix, never a silencing in bulk; a `#pragma` or a flag that switches a warning off needs its reason written next to it. A library's headers come through a system include folder, outside the warnings, without a pragma.
- The sets are in `cmake/CompilerWarnings.cmake`: `modernjk_warnings` for the game, the same checks on the three compilers (`-Wall` and `-Wsign-compare` for GCC and clang, MSVC's `/W1` with the numbers that match them raised to level 1), and `netcodec_warnings` (strict) for netcodec.
- Some warnings exist on one compiler only (GCC's `-Wuseless-cast` on 32- and 64-bit size types: use `ToU32`; MSVC is stricter on old engine code): build on the three.
- The Release presets build with link-time optimization (`modernjk_ENABLE_IPO`, clang's ThinLTO; the netcodec library out of it with GCC, which makes it no faster), never the Debug and sanitizer presets (22 November 2026). What the link reports across files is an error too: GCC's `-Wodr`, `-Wlto-type-mismatch` and the warnings of the code inlined from another file (GCC gets `-Werror` at the link), MSVC's at `/LTCG`. `linux-gcc-debug-lto`, which only `validate.sh` builds, checks the code only Debug compiles the same way.

## 9. Libraries

- Every library is in `libs/`, at a pinned version, and built with the project; never the system's copy. Each library's README gives its source, its tag or commit, the files left out and how to update it; a local change is a patch file in `libs/<library>/patches/`, listed in the README, applied again at an update and dropped once a release no longer needs it.
- The one exception: SDL2 from the system on Linux, until plan step 15 replaces it with SDL 3, vendored.
- A library's licence must be compatible with the GPL v2.

## 10. Comments

- A comment says what the code does and why, in full sentences; what the code can say itself is not repeated.
- No mention of the plan, its steps or parts, no link to the documents of `docs/` and no date or attribution of a decision in the code: the explanation stays in the comment, the history in the documents. Where a link backed a comment, a short sentence gives what the document says that matters there. A tool or a data file of `docs/modernization/tools` or `docs/modernization/data` that the code reads or is generated by is named by its full path.
- Commented-out code follows the live code: when types or names change, the commented code changes the same way, so that it would compile if uncommented; string literals keep their text.
- Comments describe the code and its formats, never a prototype it came from.

## 11. Settings

- As few settings as possible. A value that needs no measuring is a constant, without a cvar.
- Debug cvars exist only for computed values, and are `CVAR_CHEAT` on the client. What must not be bypassed is enforced on the server.
- When a cvar is removed, the behaviour left is the one of its default value.
- Removing a field from a game-module table indexed by position (such as `userinfoFields` and the bit mask `g_userinfoValidate`) moves the bits after it: change the default and say how to reset stored values.

## 12. Copyright

- A file of OpenJK that is changed for the first time, by any change, gets `Copyright (C) 2026, ModernJK contributors` after its last copyright line, with that line's prefix (GPL v2, section 2(a)). In a later year, the line becomes a range (`2026 - 2027`).
- A file without a notice gets one after its origin (the lines of the source it came from, then ModernJK's); a new file gets OpenJK's notice with ModernJK in place of OpenJK.
- Third-party code under another licence keeps its own notice alone.

## 13. Printing and formatting

- Every message and every formatted text goes through `src/shared/qcommon/q_fmt.h`, in {fmt}'s syntax, checked at compile time: `Q_Print` writes its text as it is, `Q_Println` ends the line, `Q_DPrintln` (with `developer`), `Q_PrintlnWarning`, `Q_PrintlnError`, `Q_Error` (stops); `Q_Format` returns a text, `Q_FormatTo` writes into a `std::span<char>`, `Q_FilePrint` and `Q_FilePrintln` into a file. A function that ends the line has `ln` in its name. Never the printf family, a `va_list` or a static buffer.
- One line per call: a warning, an error or a developer's message written in several calls is one call, so that the whole line takes its colour and a log one record.
- Another printing function is a template taking `fmt::format_string` that calls a function taking `fmt::format_args` (its name with a `V`, as {fmt}'s `vformat`), the arguments packed by `Q_FormatArgs` (a null C string written `(null)`, as printf did: a text of data read back expects it); {fmt}'s types are written `fmt::`.
- A format read at run time (a translated text, a format of the game's data) is converted with `Q_FormatFromPrintf` and formatted with `fmt::runtime`.
- An enumeration is given to a printing or formatting function as it is: `q_enum.h`'s `format_as` writes its value; `std::to_underlying` only where the value leaves for a file, a cvar or a name table, or takes part in a computation.
- A `formatText_t` lives as long as its expression: a pointer kept is a `formatText_t` kept, a pointer returned a `static formatText_t`.
- netcodec's tests, benchmarks and tools and the tests of `test/` call {fmt} directly: `fmt::println`, a text written in pieces built by `fmt::format_to`; not `fmt::print`, whose code after its `return` cannot run with `FMT_UNICODE` 0, which MSVC's `/W4 /WX` refuses (C4702).

## 14. Changes and bugs

- Every change of behaviour against OpenJK (engine, game, cgame, settings) is described in [CHANGES.md](docs/modernization/CHANGES.md): one section per change, always in the same order (Before, Now, Why, Gain, then the technical details folded), and a line in its summary; a change that moves a delay also updates "Delays before and now", with the measure.
- A bug is shown before it is fixed: what a player sees before and after, with an example in game; the original code is kept until the fix is decided.
- A fix of the game is not done until its edge cases are reproduced, with bots or the scripted scenarios (walls, animations, one's own missiles); a fix that brings more bugs than it removes is undone.
- The scripted scenarios stay `golden: SAME`, or every difference is explained in [GOLDEN.md](docs/modernization/tools/gamestats/GOLDEN.md); the tests stay short, several checks per run.

## 15. Documents and commits

- Everything written into the repository is in English, in full sentences: documents, comments, commit messages.
- Dates are absolute ("21 November 2026"), never relative. A decision is written with its date, never with who took it or asked for it; the roles are neutral ("the development machine").
- A rule of a guide or a standard is cited with its source, never by its number alone: "SF.12 of the C++ Core Guidelines", "section 2(a) of the GPL v2".
- A commit is named by its content, never by its hash (the pinned versions of other projects excepted).
- Every status carries a mark: ✅ done, ⏳ in progress or planned, ❌ cancelled or dropped (status tables, the headings of the plan, the lists of parts).
- Commit titles follow Conventional Commits, the plan's step first: `refactor(build): step 19, part 5: ...`.

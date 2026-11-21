# Step 19: a modern CMake project

Status: **in progress** (20 November 2026). The decisions taken on 20 and 21 November 2026 (section 1); C++23 tried (section 2); every warning of the three compilers, clang-tidy and cppcheck measured (sections 3 and 4), for plan step 23, which the new warnings and every fix moved to on 21 November 2026 (section 5). Plan: [PLAN.md](../PLAN.md), step 19.

## 1. Decisions (20 November 2026)

| subject | decision |
|---|---|
| the build, 21 November 2026 | written again from the template rather than reworked, the present CMake files only the inventory of what each target needs; the sources moved under `src/`, the includes after SF.12 without `../`; the mentions of the plan removed from the code; an `assets/` folder built into `base/modernjk.pk3`; the details in [PLAN.md](../PLAN.md) step 19 |
| CMake | 4.5 at least (`cmake_minimum_required(VERSION 4.5)` in every `CMakeLists.txt`), 4.5.1 on every machine: the Ubuntu VM from Kitware's APT repository (https://apt.kitware.com, which ships it for 22.04, the system's being 3.22), Windows from cmake.org (the Windows VM has it; this machine's is the zip in `C:\Logiciels\cmake-4.5.1-windows-x86_64`) |
| libraries | they stay in `lib/`: not the template's CPM (CPM.cmake, a script over CMake's `FetchContent` that downloads the dependencies while configuring), nor git submodules, nor forks. Each library's README gives its source, its tag or commit, the files left out and how to update it; a local change is a patch file in `lib/<library>/patches/`, listed in the README, applied again with `git apply` at an update and dropped once a release no longer needs it. Today no library of `lib/` is changed; the first patch will be OpenAL Soft's line for clang 23's libc++ (step 21) |
| language | C++23 for every C++ target. MSVC has `/std:c++23` since MSVC 14.52 (Visual Studio 2026 18.12 and LTSC 18.11, 10 November 2026; `/std:c++23preview` before), and CMake 4.5 gives it for `CXX_STANDARD 23` from MSVC 19.52.37029 (`Modules/Compiler/MSVC-CXX.cmake` at the tag `v4.5.1`); GCC 15 and clang 23 have it |
| link-time optimization | kept, measured in build time, on the scenarios (`golden: SAME`) and on the netcodec benchmarks |
| hardening | not now: the defects it would contain (a buffer overflow turned into a crash rather than into code execution) are fixed in the code. Looked at again at the end of the step, kept only if it costs almost nothing, or not at all. The template's set: MSVC `/sdl`, `/guard:cf`, `/DYNAMICBASE`, `/NXCOMPAT`, `/CETCOMPAT`; GCC and clang `_GLIBCXX_ASSERTIONS`, `_FORTIFY_SOURCE=3` (not in Debug), `-fstack-protector-strong`, `-fcf-protection`, `-fstack-clash-protection` |
| warnings, 21 November 2026 | this step keeps today's level (`-Wall` for GCC and clang) and makes the three compilers check the same things (decided 21 November 2026: clang gets `-Wsign-compare`, which GCC's `-Wall` has in C++ and clang's lacks; MSVC, at its default level `/W1` until now, gets the numbers that match `-Wall`: C4018 and C4389 (signed and unsigned), C4701 and C4703 (a variable perhaps used uninitialised), C4189, C4101 and C4505 (an unused variable or function), C4062 (a switch missing an enumerator), C4805 (a `bool` mixed with an integer); 19 warnings on 16 lines when measured, with the 7 C4305 the pragmas hid), in one file of warnings for every target; `disablewarnings.h` and the 68 MSVC pragmas removed. The new warnings and their fixes: plan step 23, done last (after steps 22, 21 and 15): a group enters the set once fixed and as an error at once, every warning an error at the end |
| warnings, 20 November 2026 | the set chosen again from zero, not taken as it is from the template (`cmake/CompilerWarnings.cmake`; netcodec's `cmake/Warnings.cmake` is that list with `-Wsign-conversion`): every warning of the three compilers counted on the whole project (section 3), each flag chosen with what it finds here; every warning of the set analysed and fixed with the right fix, never silenced in bulk. To decide after the counts: the set, and when the warnings become errors |
| the VMs' build folders | left in the repository's folder (the shared folder) |
| MSVC's format annotation | `_Printf_format_string_` not added: clang already compiles the Windows code with the format attributes of step 18 (llvm-mingw); `_WIN32` names the Windows API, 64-bit included |
| the printing functions | on {fmt}, vendored, in a new plan step 22, right after this one and before step 21 (decided 21 November 2026): one implementation on every compiler rather than the standard library's `std::format` and `std::print` |
| clang-tidy | over the whole project: the common checks (bugs, the static analyzer, performance, portability, part of modernize and readability), netcodec's `.clang-tidy` as the starting point rather than the template's `*`, chosen with the findings of each check (section 4); the fixes looked at case by case, the code being old. `readability-redundant-casting` (its `--fix`) and `modernize-use-nullptr` approved. 21 November 2026: this step sets up the preset, reporting only; the 65 checks that find defects (section 5) and every fix, the two approved ones included, in plan step 23 |
| cppcheck | 2.22.0 (19 September 2026), kept if it finds what the compilers and clang-tidy miss: run once on the whole project and compared. 21 November 2026: kept in a preset with its `error` and `warning` severities, without `dangerousTypeCast`; its fixes in plan step 23 |
| parallel builds and tests | approved: `/MP` for netcodec's MSVC targets, `"jobs"` in the test presets; the benchmarks stay sequential |
| at the end | cpp-best-practices/project_validator (https://github.com/cpp-best-practices/project_validator: the project's build, tests and tooling checked against best practices) run once on the repository |

## 2. C++23 tried (20 November 2026)

Every target of the three games (the multiplayer, the single player of Jedi Academy and Jedi Outcast, `BuildJK2SP*` on) built with clang 23 (llvm-mingw 20261104) and CMake 4.5.1 in C++23, Release, with the project's present flags: 1,084 build steps, **no error and no warning**. The flag is given without changing the project: `tools/warnings/cxx23.cmake`, passed as `CMAKE_PROJECT_INCLUDE`, makes CMake emit C++23's flag where the targets ask C++20 (953 C++ files with `-std=c++23`; the 46 C files are zlib-ng's and the single player's MP3 decoder). Then on the VMs (21 November 2026), the same way, every target with the present flags in the repository's `build-modernjk/gcc-cxx23`, `clang-cxx23` and `msvc-cxx23`: GCC 15.2 and Clang 23 on Ubuntu (`-std=c++23`), MSVC 19.52.37029 (`LanguageStandard` `stdcpp23`), the 12 binaries each, **no error and no warning**. The 302 scenarios with the dedicated server and the game module of the clang build: `golden: SAME` (2 min 42 s).

## 3. The warnings counted

The tools, in `docs_rewrite/modernization/tools/warnings/`:

| file | what it does |
|---|---|
| `cxx23.cmake` | the C++23 flag where the targets ask C++20 (section 2), for the measurements until the project moves |
| `clang_weverything.sh` | every clang warning (`-Weverything`) on every target, Release and Debug (the code under `_DEBUG`), C++23; left out: the compatibility with other language versions and the struct padding |
| `gcc_weverything.sh` | every GCC warning: GCC has no `-Weverything`, so the list is the compiler's own (`-Q --help=warnings`), each option kept if `g++` accepts it for C++, without the other language versions and ABIs, the padding and the system headers; the options taking a level at their highest useful one |
| `measure_tree.py` | a copy of the code next to the repository (`E:\Git\jka\ModernJK_s19measure`, seen by both VMs) with every MSVC `#pragma warning(disable ...)` commented out (68 in 12 files: `disablewarnings.h`, the single player's `q_shared.h` and others), so that MSVC reports what they hide |
| `count_warnings.py` | the warnings of the logs per flag, each source location once (a header's warning repeats in every file that includes it; MSBuild repeats each warning in its summary), outside `lib/`, per tree, with an example; it also reads clang-tidy's and cppcheck's lines |
| `groups.py` | the flags of the three compilers in groups by what they find, each group counted per compiler (section 3.4) |

MSVC: `/Wall` on the copy, C++23, without five warnings that only inform (4710 and 4711, the inlining; 4514, an unused inline function; 4820, the padding; 5045, the Spectre mitigation) and without the system headers (`/external:anglebrackets /external:W0`).

Commands (this machine, Git Bash, from the repository's root, tools in the `PATH` as in [README.md](README.md), "How to resume"):

```
bash docs_rewrite/modernization/tools/warnings/clang_weverything.sh /e/Git/jka/ModernJK /e/Git/jka/ModernJK/build-netcodec/s19-wall
python -I docs_rewrite/modernization/tools/warnings/count_warnings.py clang.json build-netcodec/s19-wall-release.log.gz build-netcodec/s19-wall-debug.log.gz
python -I docs_rewrite/modernization/tools/warnings/measure_tree.py E:/Git/jka/ModernJK_s19measure
```

Ubuntu VM:

```
bash /media/sf_jka/ModernJK_s19measure/docs_rewrite/modernization/tools/warnings/gcc_weverything.sh /media/sf_jka/ModernJK_s19measure /media/sf_jka/ModernJK_s19measure/build-gcc-weverything
```

Windows VM (PowerShell):

```
cmake -S Z:\ModernJK_s19measure -B Z:\ModernJK_s19measure\build-msvc-wall -G "Visual Studio 18 2026" -A x64 -DBuildPortableVersion=ON -DBuildJK2SPEngine=ON -DBuildJK2SPGame=ON -DBuildJK2SPRdVanilla=ON -DCMAKE_PROJECT_INCLUDE=Z:/ModernJK_s19measure/docs_rewrite/modernization/tools/warnings/cxx23.cmake "-DCMAKE_CXX_FLAGS_RELEASE=/O2 /Ob2 /DNDEBUG /Wall /wd4710 /wd4711 /wd4514 /wd4820 /wd5045 /external:anglebrackets /external:W0"
cmake --build Z:\ModernJK_s19measure\build-msvc-wall --config Release | Tee-Object -FilePath Z:\ModernJK_s19measure\build-msvc-wall.log
```

### 3.1 clang (20 November 2026)

Every count of this document takes a location once: the same file (its path normalised, so that `code/client/../qcommon/q_shared.h` is `code/qcommon/q_shared.h`), line, column and flag. The first counts, made on 20 November 2026 before the paths were normalised, were 8 to 13 % higher. A log built by Make in parallel must be built with `-O` (each compilation's messages kept together): clang, unlike GCC, writes a message in several pieces, which other compilations cut into.

Release and Debug, 44 and 39 s with 32 jobs: 568,095 and 571,729 warning lines, **92,677 locations** outside `lib/` once each repeated location is counted once (100,756 with `lib/`), under 74 flags. The flags of the three compilers grouped by what they find, with their counts: section 3.4.

Clang 23 on the Ubuntu VM (21 November 2026), the same script with Make (`GENERATOR="Unix Makefiles"`, `-O`): Release 93 s, Debug 63 s, **92,724 locations** outside `lib/` under 75 flags; 315 of them not in the Windows counts: the code of Linux alone (`shared/sys/sys_unix.cpp`, `con_tty.cpp`, `codemp/qcommon/net_ip.cpp`) and `-Wshorten-64-to-32` (191: `long` is 64-bit on Linux, 32-bit on Windows). Its first run, without `-O`, had 23,908 warning lines cut into by other compilations; GCC's log, made the same way, had none.

### 3.2 GCC (21 November 2026)

GCC 15.2.0 on the Ubuntu VM, Release, C++23, 345 warning options (GCC's list kept for C++ by `gcc_weverything.sh`), 93 s: no error, 733,552 warning lines, **45,566 locations** outside `lib/` (48,930 with it) under 53 flags. GCC has fewer flags than clang for the same defects (one `-Wconversion` where clang has `-Wimplicit-int-float-conversion`, `-Wshorten-64-to-32` and others) and some clang lacks (`-Wuseless-cast`, `-Wduplicated-branches`, `-Wlogical-op`, `-Wnull-dereference`, `-Wformat-overflow`), and it compiles the Linux code, which clang compiled here for Windows does not. GCC reports 28 `-Wzero-as-null-pointer-constant` outside `lib/` where clang reports 580: it does not count the `0` of `sizeof(((T *)0)->x)` in `NETF` and `PSF`.

One finding of the counts already: 478 of clang's 580 `-Wzero-as-null-pointer-constant` come from two macros of `codemp/qcommon/msg.cpp`, `NETF` and `PSF` (`sizeof(((entityState_t *)0)->x)`, where C++ writes `sizeof(entityState_t::x)`), which step 18's `nullptr` pass, made in the source text, did not see inside the macros.

### 3.3 MSVC (21 November 2026)

MSVC 19.52.37029 on the Windows VM, `/Wall` on the copy whose `#pragma warning(disable ...)` are commented out (section 3), Release, C++23: no error, a log of 409 MB, **19,216 locations** outside `lib/` (20,025 with it) under 41 warning numbers. The most frequent: C4244 7,075 (a conversion that may lose data), C4365 3,154 (signed and unsigned), C5219 3,092 (an integer to a float that may lose data), C4100 2,086 (an unused parameter), C4061 1,076 (an enumerator a switch does not name), C4267 742 (`size_t` to a smaller type), C4242 616, C4464 456 (a relative include path with `..`), C4459 269 and C4456 122 (a name that hides another).

### 3.4 The three compilers by what they find

`tools/warnings/groups.py` puts the flags of the three compilers in groups by what they find; each count is the locations outside `lib/` (clang: its Windows and Linux runs together). A flag no group names would be listed under the table: none is left.

```
python -I docs_rewrite/modernization/tools/warnings/groups.py --clang clang.json clang-linux.json --gcc gcc.json --msvc msvc.json
```

| # | what the warnings find | clang | GCC | MSVC | flags |
|---|---|---|---|---|---|
| 1 | conversions that change a value (integer, float, sign, 64 to 32 bits) | 15,633 | 14,629 | 14,768 | `-Wimplicit-int-float-conversion`, `-Wfloat-conversion`, `-Wimplicit-float-conversion`, `-Wshorten-64-to-32`, `-Wimplicit-int-conversion`, `-Wsign-conversion`, `-Wimplicit-int-conversion-on-negation`, `-Wconversion`, `-Warith-conversion`, `C4244`, `C4365`, `C5219`, `C4267`, `C4242`, `C4305`, `C4245`, `C4388`, `C4018`, `C4800`, `C4310`, `-Wstring-conversion`, `-Wtautological-value-range-compare`, `-Wsign-compare` |
| 2 | float promoted to double | 4,170 | 3,678 |  | `-Wdouble-promotion` |
| 3 | C-style casts | 10,975 | 9,335 |  | `-Wold-style-cast` |
| 4 | casts that drop const, raise the alignment or change a function type | 2,199 | 1,230 | 4 | `-Wcast-qual`, `-Wcast-align`, `-Wcast-function-type-strict`, `-Wcast-function-type`, `C4191`, `-Wunaligned-access` |
| 5 | a definition without a declaration (internal linkage or a header) | 7,963 | 6,350 |  | `-Wmissing-prototypes`, `-Wmissing-variable-declarations`, `-Wmissing-declarations` |
| 6 | unused parameters, macros, constants, functions | 2,178 | 2,155 | 2,089 | `-Wunused-parameter`, `-Wunused-macros`, `-Wunused-const-variable`, `-Wused-but-marked-unused`, `C4100`, `C5264`, `-Wunused-member-function` |
| 7 | a name that hides another | 409 | 409 | 411 | `-Wshadow`, `C4456`, `C4457`, `C4459` |
| 8 | a switch on an enumeration without every enumerator, or a default | 1,357 | 1,247 | 1,076 | `-Wswitch-enum`, `-Wswitch-default`, `-Wcovered-switch-default`, `C4061` |
| 9 | implicit fall-through between cases | 61 | 46 | 22 | `-Wimplicit-fallthrough`, `C5262` |
| 10 | floats compared with == or != | 679 | 2,563 |  | `-Wfloat-equal` |
| 11 | classes: virtual destructor, override, copy and destructor rules | 297 | 249 | 50 | `-Wnon-virtual-dtor`, `-Wsuggest-override`, `-Wsuggest-destructor-override`, `-Wdeprecated-copy-dtor`, `C4265`, `C5204`, `C5267`, `-Wdeprecated-copy-with-user-provided-dtor`, `-Winconsistent-missing-destructor-override` |
| 12 | the format strings | 189 | 199 |  | `-Wformat`, `-Wformat-nonliteral`, `-Wformat-signedness`, `-Wformat-overflow`, `-Wmissing-format-attribute`, `-Wsuggest-attribute`, `-Wformat-extra-args` |
| 13 | reserved identifiers (a leading underscore and capital, a double underscore) | 1,225 |  |  | `-Wreserved-identifier`, `-Wreserved-macro-identifier` |
| 14 | code never executed | 1,467 |  | 49 | `-Wunreachable-code`, `-Wunreachable-code-break`, `-Wunreachable-code-return`, `C4702` |
| 15 | the preprocessor: an undefined macro in #if, __DATE__ and __TIME__ | 53 | 53 | 42 | `-Wundef`, `-Wdate-time`, `C4668` |
| 16 | initialisation: missing fields and braces, a variable perhaps used uninitialised | 974 | 179 | 9 | `-Wmissing-field-initializers`, `-Wmissing-braces`, `C4701`, `C4703`, `C5246`, `-Wconditional-uninitialized` |
| 17 | 0 as a null pointer | 582 | 28 |  | `-Wzero-as-null-pointer-constant` |
| 18 | extra semicolons | 671 | 56 |  | `-Wextra-semi`, `-Wextra-semi-stmt` |
| 19 | redundant: casts to the same type, declarations repeated |  | 763 |  | `-Wuseless-cast`, `-Wredundant-decls` |
| 20 | the text: invalid UTF-8, no newline at the end, /* in a comment, trailing spaces | 308 | 506 | 6 | `-Winvalid-utf8`, `-Wnewline-eof`, `-Wcomment`, `-Wtrailing-whitespace`, `C4125` |
| 21 | qualifiers ignored, empty files, nameless structs, a comma in an index, deprecated functions | 55 | 41 | 69 | `-Wignored-qualifiers`, `C5266`, `C4206`, `C4201`, `C4709`, `C4996`, `-Wpedantic`, `-Wdocumentation`, `-Wnested-anon-types`, `-Wshadow-header`, `-Wcomma`, `-Wduplicate-enum`, `-Wgnu-anonymous-struct`, `-Wunique-object-duplication` |
| 22 | found by GCC with the optimiser: null dereference, duplicated branches and conditions, logical operators |  | 100 |  | `-Wnull-dereference`, `-Wduplicated-branches`, `-Wduplicated-cond`, `-Wlogical-op` |
| 23 | a template used without its definition (the single player saved games) | 400 |  |  | `-Wundefined-func-template` |
| 24 | safe buffers: every pointer arithmetic and indexing, the C library calls | 40,483 |  |  | `-Wunsafe-buffer-usage`, `-Wunsafe-buffer-usage-in-libc-call`, `-Wunsafe-buffer-usage-in-format-attr-call`, `-Wunsafe-buffer-usage-in-container` |
| 25 | global constructors and exit-time destructors, vtables in every file | 310 |  |  | `-Wglobal-constructors`, `-Wexit-time-destructors`, `-Wweak-vtables` |
| 26 | clang 23's lifetime analysis (suggestions) | 352 | 1 |  | `-Wlifetime-safety-intra-tu-suggestions`, `-Wlifetime-safety-intra-tu-constructor-suggestions`, `-Wlifetime-safety-cross-tu-suggestions`, `-Wlifetime-safety-cross-tu-constructor-suggestions`, `-Wlifetime-safety-invalidation`, `-Wnrvo` |
| 27 | style rules: Effective C++, templates, namespaces, aggregate returns, inlining, final, elaborated types | 4 | 1,749 |  | `-Weffc++`, `-Wtemplates`, `-Wnamespaces`, `-Waggregate-return`, `-Winline`, `-Wsuggest-final-methods`, `-Wsuggest-final-types`, `-Wredundant-tags`, `-Wnoexcept`, `-Wconditionally-supported`, `-Wcatch-value`, `-Wmissing-noreturn`, `-Wredundant-parens` |
| 28 | MSVC only: relative include paths, implicitly deleted members, extern "C" and exceptions, alignment |  |  | 621 | `C4464`, `C4623`, `C4625`, `C4626`, `C5027`, `C5039`, `C5029`, `C4255` |

## 4. clang-tidy and cppcheck

**clang-tidy** (clang-tidy 23, 20 November 2026): every check of the families bugprone, clang-analyzer (the static analyzer), concurrency, cppcoreguidelines, misc, modernize, performance, portability and readability, on the 691 C++ files of the C++23 build of section 2 (the Windows target, Release), the headers of `lib/` left out; 219 s with 32 jobs. **424,093 locations** outside `lib/` under 182 checks:

| family | locations | checks that found something |
|---|---|---|
| cppcoreguidelines | 198,375 | 30 |
| readability | 104,393 | 47 |
| misc | 55,574 | 11 |
| modernize | 38,183 | 26 |
| bugprone | 25,225 | 38 |
| clang-analyzer | 1,262 | 24 |
| performance | 827 | 4 |
| portability | 194 | 1 |
| concurrency | 60 | 1 |

The checks step 19 already approved: `readability-redundant-casting` 492 (502 on 17 November, before the last parts of step 18), `modernize-use-nullptr` 24.

Command (the configuration of the families above as a `--config-file`, `HeaderFilterRegex: '.*'`, `ExcludeHeaderFilterRegex: '.*[\\/]lib[\\/].*'`):

```
python -I /c/Logiciels/llvm-mingw-20261104-ucrt-x86_64/bin/run-clang-tidy -p build-netcodec/s19-cxx23 -j 32 -quiet -config-file=<config> 'ModernJK.(codemp|code|codeJK2|shared).*\.cpp$'
```

**cppcheck** (2.22.0, 20 November 2026): the same compilation database, `--enable=warning,style,performance,portability`, the Windows platform and the compiler's predefined macros given (cppcheck does not know them: without `_WIN32` every file stopped at `q_platform.h`'s `#error`); 179 s with 32 jobs. **18,119 locations** outside `lib/` under 88 checks. What it adds was measured by the lines no finding of clang (section 3.1) or of clang-tidy shares: **3,169 locations (17 %)**, a lower bound, since another tool's finding on the same line may be about something else. The checks with most such lines: `uninitMemberVarNoCtor` 1,168 of 1,789 (a member a class without a constructor leaves uninitialised), `variableScope` 454 of 2,923, `postfixOperator` 384 of 453 (`i++` where `++i` does), `selfAssignment` 198 of 296, `nullPointerRedundantCheck` 144 of 381 (a pointer used before a test against null, or a test that is useless), `knownConditionTrueFalse` 132 of 1,106, `memsetClassFloat` 132 of 326, `shiftNegativeLHS` 36 of 42, `missingReturn` 2 of 2 (the MP3 glue of both engines, replaced in step 21). Read before they are counted as defects: most `selfAssignment` are the byte-order macros (`LittleLong`), which assign a value to itself on a little-endian machine, and the null dereference of `code/qcommon/common.cpp` line 1000 is the console's deliberate crash.

```
cppcheck --project=build-netcodec/s19-cxx23/compile_commands.json --enable=warning,style,performance,portability --inline-suppr --platform=win64 -D_WIN32=1 -D_WIN64=1 -D__MINGW32__=1 -D__MINGW64__=1 -D__x86_64__=1 -D__GNUC__=4 -D__clang__=1 --suppress=missingIncludeSystem --suppress=missingInclude -iE:/Git/jka/ModernJK/lib -j 32 "--template={file}:{line}:{column}: {severity}: {message} [{id}]" --quiet
```

(In Git Bash, `MSYS_NO_PATHCONV=1` before it, so that the arguments keep their form; a `--suppress` pattern with a drive letter is read as a line number.)

## 5. For plan step 23: the recommendation (21 November 2026)

Moved to plan step 23 on 21 November 2026, which decides the set at its start, on counts made again then. The recommendation, by the groups of section 3.4:

| groups | recommended | why |
|---|---|---|
| 1, 4, 5, 6, 7, 9, 11, 12, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 | in the set | they find lost precision (1: principle 1 of the plan), undefined or implementation-defined behaviour, dead code, a declaration that can drift from its definition, or text the compilers misread; about 33,000 sites |
| 3, the C-style casts | in the set, in the last part | the intent of each cast visible (C++ Core Guidelines ES.48, ES.49); about 11,000 sites, converted by clang-tidy's fix-its and read file by file |
| 2, float promoted to double | decided with step 22's counts | most are floats given to printf-like functions, which {fmt} removes |
| 8, a switch that does not name every enumerator | out | `-Wswitch` (in `-Wall`) already reports a switch without `default` that misses an enumerator; `-Wswitch-default` and `-Wcovered-switch-default` contradict each other |
| 10, floats compared with `==` | out | the exact comparisons of this code are meant (flags held in floats, a value compared with itself computed the same way) |
| 13, reserved identifiers | out | the animation names with a double underscore (`BOTH_A1_T__B_`) are keywords of the data: `models/players/_humanoid/animation.cfg` in `assets1.pk3` reads `both_a1_t__b_` (checked on 21 November 2026), so the flag could never reach zero |
| 24, safe buffers | out | every indexing and pointer arithmetic of the engine (40,483 sites): a rewrite on `std::span`, not a fix |
| 25 to 28 | out | style rules, clang 23's experimental lifetime suggestions, and MSVC's relative include paths and implicitly deleted members |

Only 933 of the 44,414 sites of the groups recommended (2.1 %) lie in the code steps 21 and 15 replace (the mixers, the MP3 decoders, `shared/sdl/`).

**clang-tidy**: the 65 checks that found something among: the static analyzer (`clang-analyzer-*`) but `optin.performance.Padding` (it would change the structures' layout) and `security.FloatLoopCounter` (the same as `bugprone-float-loop-counter`); `bugprone-*` but `easily-swappable-parameters` (style), `narrowing-conversions` (group 1), `reserved-identifier` (group 13), `signed-bitwise` (the flag enumerations), `branch-clone` (GCC's `-Wduplicated-branches`), `assignment-in-if-condition` (the code's idiom), `switch-missing-default-case` (`-Wswitch`), `unchecked-string-to-number-conversion` (replacing `atoi` changes the behaviour: a decision of its own), and the exception checks (the game throws none); `performance-*` but `enum-size` (the structures' layout) and `no-int-to-ptr` (the modules' interface); `misc-definitions-in-headers`, `misc-redundant-expression`, `misc-header-include-cycle`, `misc-override-with-different-visibility`; `modernize-use-nullptr`, `use-bool-literals`, `deprecated-headers`, `use-equals-default`, `use-equals-delete`; `readability-redundant-casting`, `misleading-indentation`, `inconsistent-declaration-parameter-name`, `suspicious-call-argument`; `cppcoreguidelines-pro-type-member-init`. 5,415 sites when measured: the static analyzer 1,178 (229 null dereferences, 339 dead stores, 372 `strcpy`), bugprone 1,587, readability 1,328, modernize 580, misc 459, `pro-type-member-init` 272, performance 11.

**cppcheck**, its `error` and `warning` severities, `dangerousTypeCast` left out: 38 errors (negative indices 8, uninitialised variables 7 and structure members 6, null dereferences 4, null pointer arithmetic 4, the address of a local kept beyond its function 4, negative shifts 2, dangling lifetimes 2, missing returns 2) and about 2,500 warnings (members left uninitialised, a pointer used before its test against null, out-of-bounds indices in conditions), each to be read before it is called a defect.

## 6. The parts (decided 21 November 2026)

| part | what | state |
|---|---|---|
| 1 | the sources moved under `src/`, the old CMake files changed only for the paths, the scripts and living documents on the new paths | ⏳ |
| 2 | the new build written after the template, the old one removed, the compile commands compared file by file | ⏳ |
| 3 | the includes after SF.12, no `../` | ⏳ |
| 4 | today's warning level matched across the compilers, `disablewarnings.h` removed, the warnings as errors | ⏳ |
| 5 | the mentions of the plan and the links to `docs_rewrite/` removed from the code | ⏳ |
| 6 | link-time optimization in Release, measured | ⏳ |
| 7 | the end: the scenarios on Linux, hardening, project_validator, the documents | ⏳ |

## 7. How to continue (21 November 2026)

Where things stand: every decision of the step is taken ([PLAN.md](../PLAN.md) step 19, section 1 here), the measurements are made (sections 2 to 5), the parts are decided (section 6); **nothing of the parts is implemented**, the code and the CMake files are as at the end of plan step 20. The documents and `tools/warnings/` are committed before part 1. Next: part 1, then stop for approval, as after every part.

**The machines.** This machine: llvm-mingw 20261104 (clang 23.1.5) in `C:\Logiciels\llvm-mingw-20261104-ucrt-x86_64`, CMake 4.5.1 in `C:\Logiciels\cmake-4.5.1-windows-x86_64` (unpacked on 20 November 2026; 4.4.5 still in `C:\Logiciels\cmake-4.4.5`, which the existing build folders were configured with), Ninja in `C:\Logiciels\ninja`, cppcheck 2.22.0 in `C:\Logiciels\cppcheck-2.22.0\PFiles\Cppcheck\cppcheck.exe` (extracted from its MSI with `msiexec /a`, nothing installed); none in the `PATH`. The Windows VM: Visual Studio 2026 with MSVC 19.52.37029 and CMake 4.5.1. The Ubuntu VM: GCC 15.2, Clang 23, **CMake 3.22.1 still**: CMake 4.5.1 from Kitware's APT repository must be installed before part 2 (the commands of https://apt.kitware.com for `jammy`, then `sudo apt-get install cmake`; given to the owner on 21 November 2026), not during a build.

**Measurement leftovers, not in the repository.** `E:\Git\jka\ModernJK_s19measure` (seen as `Z:\ModernJK_s19measure` and `/media/sf_jka/ModernJK_s19measure` by the VMs): the copy of section 3 with its logs (`build-gcc-weverything.log`, `build-msvc-wall.log`, `build-clang-weverything-release.log.gz`, `-debug.log.gz`) and build folders; `build-netcodec/s19-cxx23`, `s19-wall-release`, `s19-wall-debug` (clang here); `build-modernjk/gcc-cxx23`, `clang-cxx23`, `msvc-cxx23` (the VMs). All can be deleted: plan step 23 counts again with the tools of `tools/warnings/` (section 3's commands). The JSON counts were in the session's scratchpad, regenerated by `count_warnings.py`.

**Part 1, the move, as prepared** (an inventory of every path made on 21 November 2026; to check again when starting, the tree may have moved on):
- Move with `mv`, not `git mv` (the files are not staged here; the owner's `git add -A` sees the renames): `codemp` to `src/codemp`, `code` to `src/code`, `codeJK2` to `src/codeJK2`, `shared` to `src/shared`, `tests` to `test`. No ignored file lives in those folders. Leave each moved file unchanged where possible, so that git keeps it a rename.
- The top-level `CMakeLists.txt`: `SPDir`, `MPDir`, `JK2SPDir`, `SharedDir` (lines 160 to 163) to `src/...`; `add_subdirectory("tests")` to `test`; `configure_file(shared/qcommon/q_version.h.in ...)` to `src/shared/...`. The sub-folders' `CMakeLists.txt` use those variables and need nothing else.
- netcodec, one folder deeper: `CMakeLists.txt` (`${PROJECT_SOURCE_DIR}/../../lib/llvm-libc`), `bench/CMakeLists.txt` and `test/pmove/CMakeLists.txt` (`netcodec_MODERNJK_DIR` `../..`, and their `.../codemp/...` source paths), `test/CMakeLists.txt` (`../../docs_rewrite/modernization/data/traces`), `CMakePresets.json` (`binaryDir` `${sourceDir}/../../build-netcodec/...`): one more `../` each, and `codemp/` to `src/codemp/` after the tree variable. The scripts find the root with `../../..` from `scripts/` (`ab_clang.sh`, `ab_gcc.sh`, `ab_gcc_multi.sh`, `ab_gcc_podman.sh`, `ab_msvc_ref.sh`, `ab_msvc.bat` `%~dp0..\..\..`) or work from `netcodec/` with `../../build-netcodec`, `../../docs_rewrite`, `../../build-modernjk` (`validate.sh`; `validate.bat` `cd /d "%~dp0.."` and the same relative paths): one more level each. The A/B scripts copy `codemp/netcodec` of a reference tree (`$REF_DIR`, `ab-msvc\ref`): a reference commit from before part 1 has the old layout, so they must take `src/codemp/netcodec` when it exists and `codemp/netcodec` otherwise. `source_hash.ps1`, `prof/samp_patch.py`, `prof/samply_report.py`: their usage texts.
- `.gitattributes`: `codemp/netcodec/fuzz/corpus/**` and `corpus_client/**` to `src/codemp/...`.
- The tools of `docs_rewrite/modernization/tools/`: `audiodec/CMakeLists.txt` (`${REPO}/shared/...`), `mathlib/build.sh` and `build.bat` (`$ROOT/shared/...`, `%HERE%..\..\..\..\shared\...`), `zipbench/zipbench.sh`, `imagebench/imagebench.sh`, `gamestats/gamestats.sh`, `gamestats_cfg.sh`, `shot_offsets.sh` and `shot_offsets.patch` (its `a/` and `b/` paths: the measuring patch applies to this tree), the Python tools (`gen_netcodec.py`, `gen_fields.py`, `angle_hash.py`, `export_codec_trace.py`, `export_rans_trace.py`, `jkadec.py`; `data/fields.json` to check), `warnings/measure_tree.py` (its `FOLDERS`). **Unchanged**: `gamestats/scenarios_golden.patch` (it applies to the worktree `E:\Git\jka\ModernJK_golden`, at the old commit, old layout) and every path into that worktree; `libc/shared/...` of LLVM in `lib/llvm-libc` (a folder of llvm-project); URLs of OpenJK.
- The documents on the new paths (decided 21 November 2026): the living ones, [PLAN.md](../PLAN.md), [CHANGES.md](../CHANGES.md), the repository's `README.md`, [../README.md](../README.md), [README.md](README.md) (with one line: "before plan step 19 part 1, the sources were at the repository's root: `codemp/`, `code/`, `codeJK2/`, `shared/`, `tests/`; the finished steps' documents and the study keep those paths"), this document, `tools/gamestats/GOLDEN.md`, the READMEs of `lib/` (`shared/qcommon/q_libm.cpp`, `q_pk3.cpp`, `rd-common/tr_image_codec.cpp`), and the surveys of the steps to come (`platform-layer.md`, `audio-libraries.md`, `audio-decoders.md`, `visibility-culling.md`). Unchanged: the documents of the finished steps (`step-01` to `step-18`, `step-20`, `step-14-audit-100hz.md`, `snapvector-rounding-report.md`), `study/`, and the surveys of finished steps. Rewrite path by path, never by a blind replacement: `shared/`, `code/` and `tests/` also occur in prose and in other projects' paths.
- Comments of the code that name a file by its old path (`shared/qcommon/q_libm.h` and others): left for part 5, which rewrites those comments anyway (mentions of the plan, links to `docs_rewrite/`).
- Checks: before the move, configure a fresh clang build with `CMAKE_EXPORT_COMPILE_COMMANDS=ON` (configure only: building would run the old build's `copy-compile-commands`, which writes a `compile_commands.json` at the root, removed on 21 November 2026; the new build of part 2 keeps it in the build folder), keep its `compile_commands.json`; after the move, the same, and compare file by file with the paths mapped (`codemp/` to `src/codemp/` and so on): nothing else may differ. Then here: every target in Release and Debug (`build-netcodec/modernjk-clang`, `modernjk-clang-debug`, reconfigured: their caches name the old paths), netcodec's presets with `ctest` (the expected hashes of [README.md](README.md), "How to resume", point 3), the 302 scenarios `golden: SAME`. On the VMs, the full validations (`validate.sh`, `validate.bat`: netcodec's paths change) and the game builds; the VPS only at the end of the step (part 7).
- The commit title (the owner commits): `refactor(build): step 19, part 1: ...`.

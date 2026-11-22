# Step 22: the printing functions on {fmt}

Status: **in progress** (22 November 2026): the survey (sections 2 to 4) and its decisions (section 1) done; part 1, {fmt} vendored and measured, done on 22 November 2026 (section 6). Next: part 2, once approved. Plan: [PLAN.md](../PLAN.md), step 22.

## 1. Decisions

| subject | decision |
|---|---|
| the library, 20 November 2026 | {fmt} (https://github.com/fmtlib/fmt), vendored in `libs/` at a pinned release, rather than the standard library's `std::format` and `std::print`: one implementation on every compiler |
| one header, 22 November 2026 | every printing and formatting call goes through the project's own functions, declared once in a header of `src/shared/qcommon` that every tree includes, which use {fmt} inside; the code never calls {fmt} itself; the conversion tool writes calls of those functions |
| the same printing everywhere, 22 November 2026 | the printing made the same wherever it is called from, where possible (section 2.2): the header gives the same functions to every binary, the crossing into the engine done inside it |
| the logs, 22 November 2026 | a step of their own, plan step 27 |
| {fmt}'s build, 22 November 2026 | 12.2.1 in `libs/fmt`, built by the project as a static library (its default mode), rather than header-only (`FMT_HEADER_ONLY`, every file compiling the whole formatting) |
| the header's functions, 22 November 2026 | one function per level, the same in every binary: `Com_Printf` (a message), `Com_DPrintf` (a developer's message, with `developer`), `Com_Warning` (yellow), `Com_PrintError` (red, stops nothing), `Com_Error(code, ...)` (stops); the colour and the prefix written by the function, the same everywhere; rather than only today's three, the levels left to step 27 |
| the crossing into the engine, 22 November 2026 | the module passes the format and its arguments packed (`fmt::string_view` and `fmt::format_args`, {fmt}'s documented way for a function of one's own) through the import tables, and the engine formats; rather than a text formatted in the module (every module carrying {fmt}'s formatting code) |
| netcodec and the bot library, 22 November 2026 | both converted: netcodec's tools, tests and benchmarks on {fmt} directly (`libs/fmt`, not the engine's header: netcodec does not depend on the engine), the bot library's files through a `FILE*` function of the header |
| the line endings, 22 November 2026 | one line per call: the level functions add the `\n` (as C++23's and {fmt}'s `println`, and as a logging library, where a call is a record), clang-tidy's "println" replacement removing it from the formats (4,451 of the 5,184 literal formats end with `\n`); the text that builds a line in several calls (the 733 others) through a function of its own that writes it as it is; rather than the `\n` kept in the texts. The C++ Core Guidelines have no rule on it (SL.io.50, "Avoid `endl`", is about the flush `endl` adds) |
| `va`, 22 November 2026 | after part 1's measures: returning by value a text of the header's own type, up to 255 characters in the object itself (on the stack while the call's expression lasts), a longer one formatted again into an array on the heap, with `c_str()`; rather than a `std::string` (SL.str.1 of the C++ Core Guidelines, "Use `std::string` to own character sequences"; slower than today with MSVC above 15 characters, its own buffer: 4 to 25 ns) or today's 4 static buffers of 32,000 bytes filled by {fmt} (the fastest, but against I.2, "Avoid non-`const` global variables", and CP.3, "Minimize explicit sharing of writable data", of the C++ Core Guidelines, not safe between threads, cut at 32,000 and overwritten by a fifth nested call). No global state and no allocation for every text measured (the longest 131 characters); the 202 places that keep the pointer (`const char* s = va(...)`, `return va(...)`) stop compiling and keep the text instead, the others get `.c_str()`. `std::string_view` (SL.str.2) is for the functions receiving a text, not for `va`'s result (it owns nothing); the engine's functions taking a `std::string_view` are not part of this step (the engine passes its texts to C functions that need the terminating zero) |
| `Com_sprintf`, 22 November 2026 | `Com_sprintf(std::span<char> dest, format, ...)` after I.13 ("Do not pass an array as a single pointer") and F.24 ("Use a `span<T>` ... to designate a half-open sequence") of the C++ Core Guidelines: an array gives its size itself (705 of the 811 calls pass `sizeof(dest)` today), a pointer gives it explicitly (`{ptr, size}`); the text cut and a warning printed as today; the 110 `sprintf` through it |
| the formats read at run time, 22 November 2026 | one syntax in the whole code: the StringEd texts converted when they are loaded (`%s`, `%d` and `%i` to `{}`, `%%` to `%`, the braces doubled), then formatted with `fmt::runtime`, checked as they run; rather than the printf syntax kept for them (`fmt::sprintf`) or `vsnprintf`. The pk3s are the base game's: their copies are not changed |
| the enums, 22 November 2026 | one generic `format_as` in `src/shared/qcommon/q_enum.h`, with the enums' other operations (Enum.4 of the C++ Core Guidelines, "Define operations on enumerations for safe and simple use"): every `enum class` of the project printed as its value, as `to_underlying` does today, the 213 conversions in the print calls removed; rather than `to_underlying` at each call or the enumerators' names (another text, a table per enumeration) |
| the format errors at run time, 22 November 2026 | the header's functions catch `fmt::format_error` (a format read at run time that does not match its arguments, a null `const char*`) and print the raw format with the error, the game going on; today printf trusts the format (an argument missing reads the stack, undefined behaviour; `%n` calls the UCRT's invalid parameter handler, Microsoft's documentation says, and writes into memory with glibc; a null `%s` prints "(null)" with glibc and the UCRT, their documentations say, undefined in the C standard) |
| {fmt}'s long texts with MSVC, 22 November 2026 | a local patch, measured patched against unpatched on the three compilers (`fmtbench_ref`, built from `build/fmt-ref`, a copy without the patch), proposed to {fmt}'s authors if it holds; rather than accepted. The first, `buffer::append` copying a run longer than 32 characters with `memcpy`, changed nothing on the Windows VM (300 characters: 82.2 and 82.8 ns unpatched, 83.3 and 82.9 patched) and was removed: the time goes to the length of the C string, which {fmt}'s `basic_string_view(const char*)` counts one character at a time when its detection does not find `__builtin_strlen` (`__has_builtin`, or GCC and clang named): MSVC has the builtin but not `__has_builtin`; the second, `libs/fmt/patches/0001-msvc-strlen.patch`, calls the C library's `strlen` there: on the Windows VM, two rounds alternated, the text of 300 characters in 31.2 and 31.3 ns patched against 86.2 and 85.9 unpatched (`snprintf` 26.5 to 32.2), `va` returning a `std::string` 52.1 against 109.3 and 112.6; the other cases unchanged within the rounds' spread (`snprintf` itself moving by up to 5 % between them). Kept. Then the same day, after the checks of section 6: the branch limited to MSVC 19.25 and later and kept out of constant evaluation by `__builtin_is_constant_evaluated` (so that it keeps working once {fmt} fixes its own `constexpr` constructor in C++14 and C++17), rather than `__builtin_strlen` for MSVC (the same code, but missing in older versions); proposed to {fmt}'s authors, with an issue and a pull request on that constructor |
| MSVC's `%03i`, 22 November 2026 | after part 1's measures: accepted, 2 ns slower than `snprintf` with MSVC (`va` 31.6 to 31.7 ns against 29.4 to 29.9), the cost {fmt}'s own (its `Com_sprintf` the same: 30.9 against 28.9), faster with GCC and clang |
| the parts, 22 November 2026 | 1: {fmt} vendored and measured (each binary's size, the speed of `Com_Printf`, `va` and `Com_sprintf` against today's, the build times, the server's CPU on the VPS), no slower; 2: the calls made the same on the header's functions, in the printf syntax still, checked by the format attributes; 3: the syntax converted (clang-tidy one function at a time, a script of the project for `Com_sprintf`), the attributes removed; 4: the formats read at run time, the enums, the format errors. Each part built on the three compilers without a warning, `golden: SAME`, the VM validations |

## 2. The calls (22 November 2026)

Counted by text, the comments stripped and the arguments split at the top-level commas (the scripts of the session; an estimate: the parts count by the syntax tree).

### 2.1 The printf-like functions

31 functions carry a GCC format attribute (`__attribute__((format(printf, ...)))`, added in plan step 18), about 9,800 calls: `src/codemp` 4,807, `src/code` 3,257, `src/codeJK2` 1,426, `src/shared` 277, netcodec 2. No function takes a `va_list` with an attribute. The most called:

| function | calls | trees |
|---|---|---|
| `Com_Printf` | 2,240 | code, codemp (a function pointer in the multiplayer's modules) |
| `va` | 1,822 | code, codemp |
| `Printf` (the import tables' `gi.Printf`, `ri.Printf`...) | 1,439 | code, codeJK2, codemp |
| `Com_sprintf` | 790 | code, codemp |
| `Print` (`trap->Print`, `botimport.Print`, the effects' `Print`...) | 770 | code, codeJK2, codemp |
| `Com_Error` | 698 | code, codemp |
| `DebugPrint` (ICARUS, single player) | 465 | code |
| `Q3_DebugPrint` | 365 | codeJK2, codemp |
| `Com_DPrintf` | 204 | code, codemp |
| the 22 others (`I_DPrintf`, `Error`, `G_Error`, `CG_Error`, `G_LogPrintf`, `ProfilePrint`, `NET_OutOfBandPrint`, `CG_Printf`, `ScriptError`, `Com_OPrintf`, `SV_SendServerCommand`, `AAS_Error`, `Debug_Printf`, `FS_Printf`, `G_SecurityLogPrintf`, `Sys_Error`...) | about 1,000 | |

The conversions in the literal formats: `%s` 5,292, `%d` 2,346, `%i` 1,991, `%f` 776, `%c` 103, `%td` 28, `%u` 22, `%zu` 22, `%x` 18, `%hi` 8, `%lx` 6, `%g` 6, `%p` 5, a few others; a width 697 times, a precision 441, the flag `0` 58, `-` 17, a `*` width or precision 10. 213 lines print a class enum through `to_underlying`. 11 literal formats hold a brace, which {fmt} writes `{{` or `}}`.

The C library's family called directly: `sprintf` (no size) 110 (`src/code` 56, `src/codemp` 42, `src/codeJK2` 12), `fprintf` 128 (netcodec's tools and benchmarks 59, the bot library 65: its chat, weights and structures written to files, and its log), `printf` 76 (netcodec's tools 63).

### 2.2 How each part prints

| part | prints with | levels |
|---|---|---|
| the engines (client, server, common) | `Com_Printf`, `Com_DPrintf`, `Com_Error`, `Com_OPrintf`, `NET_OutOfBandPrint`, `SV_SendServerCommand`, `FS_Printf` | `Com_DPrintf` only with `developer` |
| the multiplayer's game, cgame and UI | `Com_Printf` and `Com_Error` (function pointers set to the import table) and `trap->Print`, `trap->Error` directly (the game 278 and 187), `G_LogPrintf`, `G_SecurityLogPrintf`, the UI's `DC->Print` | none |
| the bot library | `botimport.Print(type, ...)` 368, `SourceError` 120, `SourceWarning` 21, `ScriptError` 28, `AAS_Error` 20, `fprintf` | `PRT_MESSAGE`, `PRT_WARNING` (yellow, "Warning: "), `PRT_ERROR` (red, "Error: "), `PRT_FATAL` (red, "Fatal: "), `PRT_EXIT` (`Com_Error` with `ERR_DROP`) |
| the renderers | `ri.Printf(level, ...)` (multiplayer 363, single player 279) and `Com_Printf` | `PRINT_ALL`, `PRINT_WARNING` (yellow), `PRINT_DEVELOPER` (red, with `developer`), `PRINT_ERROR` (red) |
| the single players' game | `gi.Printf` (Jedi Academy 401, Jedi Outcast 350), `G_Error`, `Debug_Printf(cvar, level, ...)` | ICARUS's `WL_ERROR`, `WL_WARNING`, `WL_VERBOSE`, `WL_DEBUG` |
| the single players' cgame and UI | `CG_Printf`, `CG_Error`, `cgi_Printf`, `ui.Printf`, the effects system's `theFxHelper.Print` | none |
| ICARUS | `I_DPrintf(level, ...)`, `Q3_DebugPrint`, `DebugPrint`, `CenterPrint` | as above |

Every level comes down to five cases: a message, a warning (yellow), an error that stops nothing (red), a developer's message (with `developer`), an error that stops (`Com_Error`). The engine turns each into `Com_Printf` or `Com_DPrintf` with a colour and a prefix (`CL_RefPrintf`, `BotImport_Print`).

### 2.3 `va` and `Com_sprintf`

`va` (`q_shared.cpp`, in each engine and module) formats into one of 4 static buffers of 32,000 bytes taken in turn, so that nested calls do not overwrite each other, and returns it: 128 KB per binary, not thread-safe. `Com_sprintf(dest, size, ...)` formats with `Q_vsnprintf` (`vsnprintf` on Windows and Linux, `src/shared/qcommon/q_string.h`) and prints a warning when the text is cut.

### 2.4 The formats read at run time

About 15 calls take a format that is not a literal. Most are translated texts: the StringEd files of the game's pk3s (`strings/<language>/*.str`; `assets0.pk3` has 204 of them) hold printf conversions (`%s` 89 times, `%d` 71, `%i` 50 over every language; 36 lines in English, such as "Connecting to %s", "Awaiting connection...%i", "%d server%s found with player %s"), which the code uses as formats: `va(sStringEdTemp, cstate.servername)` in the multiplayer's UI, `va(psLoading, ...)` in cgame, `Com_Error(..., CG_GetStringEdString(...))`, `va(SE_GetString("MENUS_LOADING_MAPNAME"), ...)` in the single player. The pk3s are the base game's and are not changed.

## 3. {fmt} (22 November 2026)

From its repository and documentation (primary sources):
- the last release: 12.2.1, published on 7 November 2026 (fixes of a buffer overflow on 32-bit, a read out of bounds in `std::error_code`'s specifiers, `nullptr` with `FMT_COMPILE`); 12.2.0 on 16 June 2026. Licence: MIT. No dependency;
- the format strings checked at compile time when the compiler has C++20's `consteval` (`fmt::format_string`); `base.h` 12.2.1 turns it on for MSVC from 19.40 (`FMT_MSC_VERSION < 1940` off, "consteval is broken in some MSVC2022 versions"), GCC from 10.2, clang from 11.1; a runtime format is wrapped in `fmt::runtime` and checked when it runs;
- a function of one's own, documented: a template taking `fmt::format_string<T...>` that calls a function taking `fmt::string_view` and `fmt::format_args` (`fmt::make_format_args`), "with compile-time checks and small binary footprint", the second not being a template;
- `format_to_n` writes at most n characters and returns the full size, so a cut is seen; `memory_buffer` keeps its first 500 characters in itself (`inline_buffer_size`), then grows on the heap;
- `fmt/printf.h`: `fmt::sprintf`, `fmt::printf`, `fmt::fprintf` take the printf syntax, are type-safe and throw an exception when an argument does not match its conversion;
- locale-independent by default; floats with "correct rounding, shortness and round-trip guarantees" (Dragonbox);
- a null `const char*` argument: `report_error("string pointer is null")`, which throws `fmt::format_error` (with exceptions off: a message on stderr and `abort()`);
- class enums: `fmt::underlying(e)`, or a `format_as(e)` function written once next to the type, which formats it as the value it returns;
- its README's measures (macOS, clang `-O3`): 2,000,000 formats in 0.44 s against printf's 0.66 s; an executable of the same size as printf's in an optimised build (54 KiB), 87 against 54 KiB without optimisation; compiled in 5.0 s against 1.6 s.

## 4. The conversion tool (22 November 2026)

clang-tidy 23 (llvm-mingw) has two checks that rewrite printf calls into the `std::print` and `std::format` syntax, their replacement function being any function with that interface (their documentation names `fmt::print` and `fmt::format`): `modernize-use-std-print` (functions whose format is the first parameter, `PrintfLikeFunctions`, or the second, the first kept, `FprintfLikeFunctions`) and `modernize-use-std-format` (functions returning the text, `StrFormatLikeFunctions`). A call is left as it is when its conversion would be incomplete or unsafe; a macro in the format other than the `PRI...` of `<inttypes.h>` refuses the call; `modernize-use-std-print` does not convert a call whose result is used. Tried on a small file: `"%d players %10s\n"` became `"{} players {:>10}"` (a string right-aligned as printf does, {fmt} aligning strings to the left by default), `"%-8s|%5.1f"` `"{:8}|{:5.1f}"`, `va(...)` the replacement given; a format ending in `\n` goes to the "println" replacement, without the `\n`; one replacement name per run, so one run per function; the calls through a function pointer (`trap->Print`) are not converted, nor `Com_sprintf` (its format is the third parameter).

## 5. Open, for the decisions

The questions this survey left were put all at once on 22 November 2026 and decided the same day (section 1).

## 6. Part 1: {fmt} vendored and measured (22 November 2026)

**What was written.**
- `libs/fmt`: {fmt} 12.2.1 (commit `208219daff58f2a7438f6931cc2ddc358acf33ef`), the five files kept unchanged (`base.h`, `format.h`, `format-inl.h`, `src/format.cc`, `LICENSE`; their git blobs those of the tag), its README after the other libraries'. No target of the project uses it yet.
- `docs/modernization/tools/fmtbench`: `fmtbench.cpp` (the outputs of `snprintf` and {fmt} compared, then each case timed: `Com_sprintf` today against the header's on a `std::span<char>`, `va` today against `va` returning a `std::string` and `va` on {fmt} with today's static buffers; the header's way of calling {fmt}: a template taking `fmt::format_string` that calls a function taking `fmt::format_args`), `size_probe.cpp` (the same three texts written each way, the executables' sizes), `run.sh` (llvm-mingw clang here, GCC on the Ubuntu VM; also the compilation of 300 calls written each way) and `run.bat` (MSVC).
- Found on the way: {fmt} 12 deprecates the conversion of a `fmt::format_string` to a `fmt::string_view`; its accessor is `get()`. A `std::string_view` passed as an lvalue argument is refused at compile time ("passing views as lvalues is disallowed"). `small` is a macro of the Windows headers (`rpcndr.h`, through `windows.h`: `char`): a variable of that name broke the MSVC build of the benchmark.

**The outputs** (llvm-mingw clang, the UCRT's `snprintf`): the same text in every case on every input, and on 1,000,000 floats per format (`%f`, `%.1f`, `%.2f`, `%.3f`, `%5.1f`, `%g`, `%e`; half in the game's range, half of any magnitude from random bits): 0 differences. The cases' inputs are ASCII.

**A difference of meaning: a string's width and precision.** With a width or a precision, {fmt} decodes the string as UTF-8 and counts its display width code point by code point (`write` for `basic_string_view<char>` in `format.h`), where printf counts bytes: for a name with bytes above 127 (Latin-1) or wide characters, the padding and the cut of `%.15s` differ. The game has 57 widths on a `%s` and 12 precisions (the server's `status` prints the players' names with `%-15.15s`), none on a frequent path; their time is about 4 times printf's (below).

**The cost of the headers** (clang 23 here, an empty file, the fastest of 7): 57 ms without, 85 ms with `<fmt/base.h>`, 252 ms with `<fmt/format.h>`; a file of the game takes about 200 ms (`g_main.cpp`, syntax only). The header includes only `<fmt/base.h>` (the checked format strings, the packed arguments, `format_to_n`, `format_as`), its implementation file `<fmt/format.h>`. 300 calls written each way: 62 ms and 19,298 bytes of object with `snprintf`, 218 ms and 33,664 bytes with {fmt} (about 48 bytes more per call: the packing of the arguments).

**The sizes.** `format.cc`'s object: about 113 KB of code and data (101 KB with `FMT_USE_LOCALE=0`), the most it can add to a binary. The probe built alone (`-static` with llvm-mingw): 10,752 bytes stripped with `vsnprintf` (the UCRT's, a DLL), 809,984 with {fmt}, most of it libc++ (its exceptions, strings and locales) linked into a program that otherwise uses none; the engines and modules already link it, so their own sizes, measured in part 3, are the measure.

**The timings** (`run.sh g++-15` on the Ubuntu VM, `run.bat` on the Windows VM, before the patch below; `run.sh clang++ 20` on this machine, quiet, the VMs off), the warm time of one call in ns, today's against {fmt}'s. With clang, two rounds alternated: {fmt}'s `Com_sprintf` 0.34 to 0.77 of `snprintf`'s time but the strings with a width (3.7 times), `va` returning a text of its own type (below) 0.49 to 0.78 of today's within 255 characters and 2.7 times at 300, a `std::string` 0.37 to 1.12 (1.5 to 1.9 at 300); the patch changes nothing with clang, which has `__builtin_strlen`. With GCC and MSVC:

| case | GCC 15: `snprintf`, {fmt} span | MSVC: `snprintf`, {fmt} span | GCC 15: `va` today, string | MSVC: `va` today, string |
|---|---|---|---|---|
| `%i` | 24.5, 11.8 | 23.7, 16.8 | 24.6, 13.8 | 23.3, 17.8 |
| `%03i` | 29.1, 21.4 | 28.9, 30.9 | 29.6, 21.5 | 29.0, 31.9 |
| `%.2f` | 102.4, 41.6 | 130.4, 60.2 | 102.5, 42.4 | 130.2, 62.0 |
| `%5.1f` | 96.7, 42.4 | 133.5, 60.0 | 97.0, 43.1 | 133.6, 62.2 |
| `print "%s\n"` | 33.6, 20.3 | 48.0, 32.5 | 33.6, 32.4 | 48.4, 52.1 |
| `%s^7 %s\n` | 38.9, 24.7 | 47.7, 39.2 | 39.3, 30.7 | 46.0, 61.2 |
| `\%s\%i` | 44.8, 21.6 | 48.5, 38.3 | 44.8, 32.6 | 47.5, 55.8 |
| `%s: %i %i %i %.1f\n` | 167.3, 79.1 | 208.9, 122.2 | 172.6, 87.9 | 208.9, 147.7 |
| `%10s\|%-8s\|` (a width) | 44.5, 112.7 | 66.5, 253.1 | 44.6, 120.0 | 65.1, 278.8 |
| `%s\n`, 300 characters | 24.0, 17.2 | 32.5, 86.5 | 26.4, 24.6 | 32.7, 111.3 |

- {fmt} is faster in every case with GCC (0.41 to 0.74 of `snprintf`) but the strings with a width (2.5 times, the display width above); with MSVC too (0.45 to 0.82), but `%03i` (1.07), the width (3.8 times) and the long text (2.7 times: {fmt}'s `buffer::append` copies with a loop, "A loop is faster than memcpy on small sizes" says its comment, which GCC and clang turn into a fast copy and MSVC does not; the same loop on {fmt}'s `main`, no issue found about it).
- `std::string` without an allocation: 22 characters with libc++ (clang here), 15 with MSVC's library and with GCC's libstdc++ (`std::string{}.capacity()`; the second GCC run, its timings within 4 % of the first). `va` returning a `std::string` is faster than today's in every case with GCC, even with an allocation; with MSVC it is slower above 15 characters (by 4 to 15 ns, 3.4 times at 300 characters).
- The sizes: {fmt} adds 92,672 bytes to the probe with MSVC (11,264 to 103,936; the C library a DLL there, so this is {fmt}'s code) and 139,432 with GCC (14,472 to 153,904). The compilation of 300 calls with GCC: 45 ms and 38,064 bytes with `snprintf`, 303 ms and 42,712 bytes with {fmt}.

**`va` returning a text of its own type** (option c, 255 characters in the object; `va small`, two rounds alternated on each VM, libs/fmt with its patch), the warm time of one call in ns, today's `va` against it:

| case | GCC 15 | MSVC |
|---|---|---|
| `%i` | 24.9 to 25.4, 11.6 to 11.9 | 23.9, 15.4 to 15.6 |
| `%03i` | 28.7 to 29.0, 21.1 to 21.2 | 29.4 to 29.9, 31.6 to 31.7 |
| `%.2f` | 104.0 to 104.9, 42.3 to 43.5 | 129.5 to 130.3, 61.8 |
| `print "%s\n"` | 33.1 to 33.5, 20.7 to 20.8 | 48.1 to 48.4, 33.2 |
| `%s^7 %s\n` | 38.4 to 38.8, 25.3 to 25.6 | 46.8 to 46.9, 39.0 to 39.1 |
| `%s: %i %i %i %.1f\n` | 174.5 to 176.9, 80.9 to 81.6 | 206.8 to 207.7, 122.4 to 122.6 |
| `%10s\|%-8s\|` (a width) | 44.7 to 44.9, 113.6 to 113.9 | 65.6 to 65.9, 258.3 to 259.7 |
| `%s\n`, 300 characters (above 255) | 26.9, 42.5 to 45.9 | 26.8 to 27.6, 80.8 to 81.6 |

As fast as the static buffers filled by {fmt} within 255 characters (`va static`: within 1 ns), and faster than today's `va` but with MSVC's `%03i` (2 ns, {fmt}'s own: its `Com_sprintf` the same), the strings with a width (decided) and the texts longer than 255 characters (formatted a second time, with an allocation: 1.6 times today's with GCC, 3.0 times with MSVC; none seen in game below).

**The lengths of `va`'s texts in game** (22 November 2026; `va_lengths.patch`, measuring code kept as a patch and not in the game, and `va_lengths.sh`: the multiplayer built with it here, the 302 scenarios run on it, `golden: SAME`; 120 dedicated servers and their game modules; the client below):

| module | calls | up to 15 characters | up to 22 | 23 to 31 | 32 to 63 | 64 and more | the longest |
|---|---|---|---|---|---|---|---|
| the game module | 830,939 | 93.7 % | 95.1 % | 2.8 % | 1.8 % | 0.3 % | 131 |
| the dedicated server's engine | 2,041,541 | 22.0 % | 71.6 % | 24.9 % | 3.5 % | 0 | 53 |

The client, the same day: the same build run as a client with a listen server for 2 min 32 s, with the default settings (the main menu, `mp/ffa3` with six bots, the scoreboard, the in-game menu, `mp/ffa5` with three bots, then quit; one session, so an indication):

| module | calls | up to 15 characters | up to 22 | 23 to 31 | 32 to 63 | 64 and more | the longest |
|---|---|---|---|---|---|---|---|
| cgame | 70,655 | 25.5 % | 70.3 % | 27.2 % | 2.5 % | 0 | 51 |
| UI | 87,997 | 76.4 % | 97.8 % | 2.0 % | 0.2 % | 0 | 39 |
| the game module (the listen server) | 3,435 | 87.8 % | 88.9 % | 7.7 % | 3.0 % | 0.4 % | 212 |
| the client's engine (with its listen server) | 34,120 | 22.2 % | 71.7 % | 24.8 % | 3.5 % | 2 calls | 275 |
| the renderer (its own copy of `va`) | 1,910 | 7.0 % | 21.8 % | 48.1 % | 30.1 % | 0 | 63 |

Every text within the 255 characters of `va`'s own type but two of the client's engine (275 characters, the call not identified): two allocations in the session. The UI is loaded three times in the session (the main menu, then once per map); the main menu alone made 40,710 of its calls, before the first map.

**The patch checked against every case** (22 November 2026, before proposing it to {fmt}'s authors). Part 1's explanation, that MSVC has no `__builtin_strlen`, was wrong (corrected in section 1 and in `libs/fmt`): MSVC's standard library calls it in `char_traits<char>::length`. The tools, in `tools/fmtbench`: `strlen_builtin.cpp` (does MSVC accept `__builtin_strlen` at run time and in a constant expression; C++14, nothing of {fmt}), `strlen_fmt.cpp` (the lengths and texts at run time, the time of one call on 300 characters, and two constant cases: a `constexpr fmt::string_view` from a literal and from a constexpr object's array, {fmt}'s issue 4423), `strlen_check.bat` (MSVC 19.52 on the Windows VM, x64 and x86, `/std:c++14`, `c++17`, `c++20` and `c++latest`, against `build/fmt-ref`, `libs/fmt` and `build/fmt-a`, which `strlen_a.patch` makes: `__builtin_strlen` for MSVC). The older versions on Compiler Explorer (godbolt.org, through its API, with the test files and {fmt}'s headers only, nothing stored): compiled, run from 19.20, the code read; its timings not used (shared machines).

| finding | versions |
|---|---|
| `__builtin_strlen` exists | not in 19.10; in 19.14 and 19.20 with C++17 and later only; in every mode from 19.29 (19.52 too) |
| `strlen(s)` and `__builtin_strlen(s)` at `/O2` | the same code in every version and architecture checked: a loop of one byte at a time, in line, on x64 from 19.29 to 19.43 and on x86 (19.29, 19.44, 19.51; 19.52 measured no faster below), a call of the C library's `strlen` on x64 from 19.44 and on arm64 in 19.51 (a loop in 19.30) |
| a `constexpr fmt::string_view` from a C string | refused by every MSVC in C++14 and C++17, {fmt} unchanged included (C3615: the constructor leaves `size_` to its body, which C++20 allows and C++14 does not; 19.14 to 19.52), accepted in C++20 and C++23 with every copy. Initialising `size_` in the constructor's list fixes it (19.20 to 19.51), but the `strlen` branch is then reached in constant evaluation (`detail::is_constant_evaluated` is always false before C++20) and refused; `__builtin_strlen` is refused where it is missing |
| `__builtin_is_constant_evaluated` | from 19.25, in every mode |
| the patch as kept: `strlen` for MSVC 19.25 and later, outside `__builtin_is_constant_evaluated` | the same code as `strlen` alone (19.25 and 19.51, x64 and x86); the constant cases as with {fmt} unchanged, and accepted everywhere once `size_` is initialised (19.20 to 19.51); built and run on {fmt}'s `main` (19.20, 19.25, 19.51; the failures of 19.20 and 19.25 with `c++latest` are {fmt}'s own, the same without the patch) |

On the VM (19.52, x64, two rounds alternated, every mode), the warm time of one call on 300 characters: `string_view` 57.9 to 61.9 ns unchanged, 8.7 to 9.2 with the first patch (`strlen` alone) and with `__builtin_strlen`; `format_to_n("{}\n")` 74.9 to 79.2 ns, 22.4 to 24.2. On x86, 57.9 to 66.7 ns for the three. Then with the patch as kept (a second run, the same day): x64, `string_view` 57.7 to 61.5 ns unchanged, 8.6 to 9.2 patched; `format_to_n` 74.4 to 79.0, 22.7 to 24.9; x86, 58.1 to 62.3 and 80.8 to 94.0 for both, no change. `fmtbench`'s figures (section 1, `libs/fmt/README.md`) were taken with the first patch, whose code is the same. A third run with the C++23 modes added (`/std:c++23`, the one CMake passes for the project, and `/std:c++23preview`): both as C++20, the constant cases accepted with every copy; over the six modes, x64 `string_view` 57.8 to 58.3 ns unchanged, 8.6 to 8.7 patched, `format_to_n` 74.3 to 75.1 and 22.4 to 24.5; x86 unchanged. On Compiler Explorer, `/std:c++23preview` passes as C++20 from 19.43 to 19.51, and `/std:c++23` is unknown there (warning D9002, the mode falling back to C++14; `strlen_check.bat` reports and skips such a mode). C++11 (clang 23 here): the same results with the three copies; no constant case in C++11, where {fmt}'s constructor is not constexpr; MSVC has no C++11 mode (`/std:c++14` is its oldest, Microsoft's documentation of `/std` says).

The proposal to {fmt}'s authors: an issue and a pull request for each defect, which are independent (decided 22 November 2026): the speed (their issue 5038 and pull request 5040; the patch on their `main`, `include/fmt/core.h`), and the `constexpr` constructor (their issue 5039 and pull request 5041; `size_` initialised in its list, and a test in `test/core-test.cc` that runs whenever `FMT_USE_CONSTEXPR` is set). Checked then on Compiler Explorer: GCC refuses that constructor too in C++14 and C++17 (8.5, 10.5, 12.5, 14.4 and 16.2: "member ... size_ must be initialized by mem-initializer in 'constexpr' constructor"), clang 6 for a local variable, clang 10 and later accept it; the fix makes it pass everywhere, and the new test fails without it on GCC, MSVC and clang 6. The texts are short, say that they were written by an AI model, and are published by hand.

**Part 1 done** (22 November 2026): {fmt} 12.2.1 in `libs/fmt` with one patch (MSVC's `strlen`), the same text as `snprintf` on the three compilers, and faster on each but for the strings with a width (decided: {fmt}'s display width), MSVC's `%03i` (2 ns, {fmt}'s own) and `va`'s texts longer than 255 characters (none seen in game); `va` decided on these measures (section 1). The sizes and the build time of the game itself, and the server's CPU on the VPS, are measured once the code uses the header (part 3). `build/fmt-ref`, a copy of `libs/fmt` without its patch, stays for the A/B of `fmtbench`. Next: part 2, the calls made the same on the header's functions, once approved.

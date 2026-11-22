# Step 22: the printing functions on {fmt}

Status: **in progress** (22 November 2026): the survey (sections 2 to 4) and its decisions (section 1) done; part 1, {fmt} vendored and measured, done on 22 November 2026 (section 6); part 2 written the same day (sections 7 and 1): every target built with clang without a warning, `golden: SAME`, the VM validations ALL OK without a warning (`validate.sh`: GCC 15 and Clang 23; `validate.bat`: MSVC), the check in game passed: part 2 done. Next: part 3, once approved. Plan: [PLAN.md](../PLAN.md), step 22.

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
| the header's file, 22 November 2026 | before part 2, after its inventory (section 7): `src/shared/qcommon/q_fmt.h` and `q_fmt.cpp`, every formatting function and not only the printing ones; `va` and `Com_sprintf` move into it in part 2, one copy for the trees (the multiplayer's and the single player's are the same) |
| the names, 22 November 2026 | part 2 keeps `Com_Printf`, `Com_DPrintf`, `Com_Warning`, `Com_PrintError`, `Com_Error` (the engine calls most of them already: fewer lines changed); part 3 brings {fmt}'s syntax under `Q_Print`, `Q_DPrint`, `Q_Warning`, `Q_PrintError`, `Q_Error` (no `f`: the syntax is no longer printf's, as `fmt::print`), both sets side by side while clang-tidy converts one function at a time, so that each call is rewritten once, the `Com_` names removed at the end of part 3; rather than the `Com_` names for good (part 3 would need temporary names anyway, each call touched twice) or the `Q_` names from part 2 (2,240 engine calls renamed, then rewritten again) |
| the crossing in part 2, 22 November 2026 | one identical pair in every import table (the renderers, the multiplayer's game, cgame and UI, the single player's game), `Print(printLevel_t, format, va_list)` and `Error(errorParm_t, format, va_list)`: the module passes its arguments, the engine formats once, writes the colour and the prefix and tests `developer`; the shape decided for part 3, where the `va_list` becomes `fmt::format_args`; the modules' own formatting into 1,024-byte buffers before the crossing removed; `printType_t` and `printParm_t` removed with their entries; rather than the text formatted in the module (each module formatting, the entry changed again in part 3) or today's seven entries kept until part 3 (the levels could not reach the engine) |
| the prefixes, 22 November 2026 | `Com_Warning` writes `^3WARNING: `, `Com_PrintError` `^1ERROR: `: the forms 712 and 201 calls write by hand today, the word kept in `qconsole.log` and the dedicated console, where the colours are removed; the hand-written prefixes removed from the formats; the bot library's `Warning:` becomes `WARNING:`, its `Error:` and `Fatal:` `ERROR:` (its "fatal" messages stop nothing: each is followed by a `return` of a neutral value, the game going on; the engine's own fatal error, `Com_Error` with `ERR_FATAL`, quits the program); rather than the colour alone (the logs could not tell a warning) or the bot library's `Warning: ` and `Error: `. The 381 messages that carry only a colour, red or yellow without a word, stay messages with their colour, listed for plan step 27 |
| the developer's messages, 22 November 2026 | `Com_DPrintf` in every binary, without a colour, as the engine's today: the renderers' 35 developer messages lose the red `CL_RefPrintf` gave them; the modules' `if (developer)` blocks that only print written with it; the effects system's messages developer messages in the single player too (printed always there outside `FINAL_BUILD`, only with `developer` in the multiplayer) |
| the debug channels, 22 November 2026 | ICARUS's print functions (four copies: the single player's `g_ICARUSDebug` with its levels and its entity filter, the multiplayer engine's `developer`, the multiplayer game's `developer 2`) and `Debug_Printf` (the AI's cvars with their levels) kept with their own conditions, their bodies printing through the header's functions (`WL_ERROR` `Com_PrintError`, `WL_WARNING` `Com_Warning`, the others `Com_Printf` with their colour and word), their levels left to plan step 27; rather than each call on the header's functions directly (the scripts' messages printed without being asked, the filter lost). `Com_OPrintf` (the Windows debugger's window, the standard output on Linux) replaced by `Com_DPrintf`, the same on every system |
| the logs, 22 November 2026 | `G_LogPrintf`, `G_SecurityLogPrintf`, the bot library's `Log_Write` and `FS_Printf` left to plan step 27 (where they write, their levels, their time stamps); `Log_Write` given the format attribute; their format syntax moves to {fmt} in part 3 like every other |
| part 2's order and proof, 22 November 2026 | tree by tree: the multiplayer (the header and the engine's side first, then the bot library, the renderer, the modules, ICARUS), then the single player of Jedi Academy, then Jedi Outcast; clang Release and Debug without a warning after each tree, then `validate.sh`, `validate.bat`, `golden: SAME`. The machine code cannot stay identical (the texts and the crossing change): the proof is the format attribute on every printing function, so that the three compilers check every call; a table, made from the inventory, of the calls whose visible text changes; `golden: SAME` (the scenarios read none of these lines); a short check in game (a renderer's warning, a bot library's warning, an ICARUS error, a developer's message, a `Com_Error`) |
| where a binary's messages go, 22 November 2026 | during part 2: `Com_VPrintf` and `Com_VError` defined by each engine and module (the engine's console, or the module's import table), rather than one definition in `q_fmt.cpp` calling a pointer each binary sets when it loads: no non-`const` global variable (I.2 of the C++ Core Guidelines, "Avoid non-const global variables"), no call before the pointer is set, and a binary without its definition does not link |
| `BotAI_Print`, 22 November 2026 | during part 2: removed with its two calls and a commented-out one; empty since OpenJK, it printed nothing |
| the calls found wrong, 22 November 2026 | the three calls the format attribute finds among the 510 it did not check (section 7) fixed: `%4.2f` for `%4.2 ` in both `NPC_spawn.cpp`, the argument printed nowhere removed in `g_ICARUScb.cpp` |
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

## 7. Part 2: the calls made the same (22 November 2026)

### 7.1 The inventory

Made on the syntax tree with clang-tidy 23's query-based custom checks (`--experimental-custom-checks`, a `CustomChecks` entry in a configuration file): `callExpr(callee(decl(hasAttr("attr::Format"))))` finds every call of a function, or of a function pointer of an import table, that carries a format attribute, and the same query on `functionDecl(isVariadic())` without the attribute finds the printf-like functions the compilers never checked. clang-tidy ran once per entry of the compile databases of `windows-clang-debug` and `windows-clang-release` (1,062 entries each), so that every call is tied to the targets that compile it; the call's text is then read at the place the tree gives (the callee, the arguments split at the top-level commas, the format's literals and colour macros). The tools: `docs/modernization/tools/printinv`. Four `.cpp` files are not in the Windows databases (`sys_unix.cpp`, `con_tty.cpp` and netcodec's two fuzzers); about 440 lines of calls sit in branches the Windows builds do not compile (`#ifdef _DEBUG` in Release, `#if 0`, Linux) and about 300 calls are commented out: they are converted by text, as plan step 18 did.

9,855 calls of functions with a format attribute, and 528 calls of variadic functions without one. Without the formatting functions (`va`, `Com_sprintf`, `sprintf` and the C library's, 2,979 calls), the network's (`NET_OutOfBandPrint`, the server commands) and the logs, by the level the call means:

| level | calls | today |
|---|---|---|
| a message | about 3,300 | `Com_Printf` 1,701, the single player's `gi.Printf` 514, `ri.Printf(PRINT_ALL)` 295, cgame's `DEBUGNAME` macro 291 (with `cg_debugEvents`), the multiplayer modules' `trap->Print` 228, the effects system's `theFxHelper.Print` 123, `botimport.Print(PRT_MESSAGE)` 101, the single player's `CG_Printf` 30, a few others |
| a warning or an error written by hand in a message | 841, and 19 developer's messages | the format starts with `^3WARNING: ` 453 times, `WARNING: ` 259, `^1ERROR: ` 156, `ERROR: ` 45, and about twenty other forms (`Warning: `, `Error, `, `error : `, `ERROR : `, `Fatal: `...) |
| a developer's message | 223 | `Com_DPrintf` 188, `ri.Printf(PRINT_DEVELOPER)` 35 (red: `CL_RefPrintf` adds the colour); and 24 blocks of the modules that print under `if (developer)` |
| a warning | 195 | `ri.Printf(PRINT_WARNING)` 159 (yellow, 133 of them writing `WARNING:` by hand as well), the bot library's `SourceWarning` 17, `PRT_WARNING` 13, `ScriptWarning` 3, `COM_ParseWarning` 3 |
| an error that stops nothing | 309 | `SourceError` 116, `PRT_ERROR` 91, `PRT_FATAL` 43, `ScriptError` 26, `AAS_Error` 17, the UI's `PC_SourceError` 10, `ri.Printf(PRINT_ERROR)` 4, the game's `BotAI_Print` 2 |
| an error that stops | 943 | `Com_Error` 679 (`ERR_DROP` 518, `ERR_FATAL` 156), `trap->Error` 100, the single player's `G_Error` 89 and `CG_Error` 60, `ri.Error` 10, `gi.Error` 4, `ui.Error` 1 |
| a debug channel, with its own condition | 1,388 | ICARUS 1,289 (the single player of Jedi Academy's `DebugPrint` 465 with `g_ICARUSDebug`, Jedi Outcast's `Q3_DebugPrint` 345, the multiplayer game's `G_DebugPrint` 290 with `developer 2`, the multiplayer engine's `Q3_DebugPrint` 20 with `developer`, the interface's `I_DPrintf` 169), Ratl's and Ragl's `ProfilePrint` 45, `Boba_Printf` 29, `Debug_Printf` 13, `Com_OPrintf` 12 |
| a log | 99 | `G_LogPrintf` 46, the bot library's `Log_Write` 36, `FS_Printf` 9, `G_SecurityLogPrintf` 8 |

How each binary reaches the console: the multiplayer modules call `Com_Printf` and `Com_Error`, function pointers set to `trap->Print` and `trap->Error`, which the engine sets to its own `Com_Printf` and `Com_Error`; the renderer's `ri.Printf(level, ...)` goes to `CL_RefPrintf` or `SV_RefPrintf`, which colour by level (`PRINT_WARNING` yellow, `PRINT_DEVELOPER` red with `developer`, `PRINT_ERROR` red), and its own `Com_Printf`, `Com_Error` and `Com_OPrintf` (`tr_subs.cpp`) format into a buffer of 1,024 bytes before calling `ri`; the bot library, linked into the engine, calls `botimport.Print(type, ...)`, which `BotImport_Print` turns into `Com_Printf` with `^3Warning: `, `^1Error: ` or `^1Fatal: `; the single player's game formats into a buffer of 1,024 bytes (`Com_Printf`, `Com_Error`, `G_Error` in `g_main.cpp`) and calls `gi.Printf` and `gi.Error`, its cgame through the system calls `CG_PRINT` and `CG_ERROR` (`cgi_Printf`, `cgi_Error`). ICARUS's four print functions each write `^1ERROR: `, `^3WARNING: `, `^2INFO: ` or `^4DEBUG: `, the last after reading back the entity's number the format starts with (`"%4d ..."`).

### 7.2 Found by the inventory

- **About 510 calls never checked.** Twelve printf-like functions had no format attribute: the multiplayer game's `G_DebugPrint` (290 calls), the bot library's `SourceError` (116), `Log_Write` (36) and `SourceWarning` (17), the single player's `Boba_Printf` (29), the UI's `PC_SourceError` (10), the game's `BotAI_Print` (2), and Jedi Outcast's and the multiplayer's `Q3_DebugPrint` through local declarations without the attribute (13 calls; `Q3_CenterPrint`, `Debug_NPCPrintf`, `COM_ParseError` and `Log_WriteTimeStamped` have no call). Given the attribute (temporarily, every entry compiled, then the sources restored), they report three calls:
  - `src/codemp/game/NPC_spawn.cpp` (`G_DebugPrint`) and `src/codeJK2/game/NPC_spawn.cpp` (`Q3_DebugPrint`): `"NPC %s could not spawn, waiting %4.2 secs to try again\n"`, the conversion's `f` missing. Shown with `developer 2` (the multiplayer) or ICARUS's debug level (Jedi Outcast) when an NPC's spawn point is taken and it tries again; the UCRT's `vsnprintf` writes `NPC trooper1 could not spawn, waiting  secs to try again`, the number missing (glibc not tried); with `%4.2f`, `waiting 2.50 secs`.
  - `src/codemp/game/g_ICARUScb.cpp` (`Q3_SetDefendTarget unimplemented`): the entity's number passed and printed nowhere; nothing visible.
- **The same level printed differently by each part.** A renderer's warning is yellow, with `WARNING:` (133 calls) or without (26), and 124 more renderer warnings are written as messages with `^3WARNING:` by hand; the bot library writes `Warning:`, `Error:` and `Fatal:`; a renderer's developer's message is red, the engine's has no colour; the effects system's messages are printed always in the single player (outside `FINAL_BUILD`) and only with `developer` in the multiplayer.
- **Nothing reads these lines.** Neither the scenarios' tools (`docs/modernization/tools/gamestats`) nor the code compare a printed text with `WARNING` or `ERROR`.

### 7.3 What was written (22 November 2026)

- **`src/shared/qcommon/q_fmt.h` and `q_fmt.cpp`**, in `modernjk_shared_common` (every engine and module): `Com_Printf`, `Com_DPrintf`, `Com_Warning`, `Com_PrintError` and `Com_Error`, each with its format attribute, hand their arguments and their level (`printLevel_t`: message, developer, warning, error) to `Com_VPrintf` or `Com_VError`, which each engine and module defines, the only part that differs between binaries (a module reaches the engine only through the import table the engine hands it when it loads it); `va` and `Com_sprintf` moved there from both trees' `q_shared.cpp`, unchanged. `errorParm_t` is declared there without its values (`enum class errorParm_t;`): the multiplayer's set has five codes, the single player's three, and each tree's `q_shared.h` keeps its own. The guard that turns `__attribute__` off for MSVC moved from both trees' `q_shared.h` to `src/shared/qcommon/q_platform.h`, which `q_fmt.h` includes.
- **The engines** (`src/codemp/qcommon/common.cpp`, `src/code/qcommon/common.cpp`): their `Com_Printf`, `Com_DPrintf` and `Com_Error` became `Com_VPrintf` (the developer's test, then `^3WARNING: ` or `^1ERROR: ` before the formatted text, then the console and the log as before) and `Com_VError`; the multiplayer's `Com_OPrintf` removed.
- **The import tables**: one `Print(printLevel_t, format, va_list)` and one `Error(errorParm_t, format, va_list)`, with the format attribute, in the multiplayer's game, cgame, UI and renderer tables and in the single players' game and renderer tables, set to the engine's `Com_VPrintf` and `Com_VError`; each module's `Com_VPrintf` and `Com_VError` call them (`trap`, `gi`, `ri`), the multiplayer modules' `Com_Printf` and `Com_Error` pointers removed. Removed with nothing in their place: the renderers' `printParm_t`, `CL_RefPrintf`, `SV_RefPrintf`, `ri.OPrintf` and their own `Com_Printf`, `Com_Error`, `Com_DPrintf` (`tr_subs.cpp`); the bot library's `Print`, `printType_t`, `BotImport_Print` and `AAS_Error` (the bot library is linked into the engine and calls `q_fmt.h`'s functions itself); the single player's UI's `Printf` and `Error` (the UI is in the engine); cgame's `CG_PRINT` and `CG_ERROR` system calls, `cgi_Printf`, `cgi_Error`, `CG_Printf` and `CG_Error` (cgame is in the game's binary); the single players' `G_Error` and their games' own `Com_Printf` and `Com_Error`; the menus' `DC->Print` and `DC->Error`; the effects system's `Print` in the three trees. Each table's version raised (the multiplayer's game 2, cgame 3, UI 4, renderer 10, bot library 3; the single players' game 11, renderer 19, UI 4), so that an engine refuses a module of the old interface.
- **The calls**, by `docs/modernization/tools/printinv/convert.py` (by text, so that the branches the builds do not compile and the commented-out calls follow; `convert_test.py` checks it), then by hand for the 10 calls commented out on several lines or written without their level in a commented-out block: 3,062 calls rewritten by the tool (section 7.4); a call that continues a line an earlier print started stays a message (`continuations.py` lists the candidates: one, the renderer's "missing opening brace" warning written in three calls). `G_Error` and `CG_Error` became `Com_Error` with `ERR_DROP`, the code they passed. The blocks that only print under `if (developer)` became `Com_DPrintf` (4 in the multiplayer game, 13 in the single players); the single player's turret printed only with `developer 1` exactly, through a test on a renderer's print level, and prints with any `developer` now. ICARUS's "error : unable to allocate a new task group", printed after the channel's own `ERROR: `, lost its "error : " (the three trees).
- **The format attribute** on every printf-like function that lacked one (section 7.2): `G_DebugPrint`, `Q3_DebugPrint` (its local declarations), `Q3_CenterPrint`, `SourceError`, `SourceWarning`, `Log_Write`, `Log_WriteTimeStamped`, `PC_SourceError` (both UIs), `Boba_Printf`, `Debug_NPCPrintf`, `COM_ParseError`. The three calls it found fixed: `%4.2f` in both `NPC_spawn.cpp`, the argument printed nowhere removed in `g_ICARUScb.cpp`. Left without one, and not printing functions: `VM_Call`, `VM_DllSyscall` and the single player's `SV_GameSendServerCommand` (called only through `gi.SendServerCommand`, which carries it).
- **Kept as they were**: ICARUS's print functions and `Debug_Printf` with their own conditions (decided), their bodies on the header's functions. **Removed**: the multiplayer game's `BotAI_Print`, empty since OpenJK, with its calls (decided the same day). **netcodec**: its legacy benchmark, which does not link `q_fmt.cpp`, given a silent `Com_Warning` beside its silent `Com_Printf`; its player-movement test defines its own `Com_VPrintf` and `Com_VError`.
- 381 files changed (3,753 lines added, 4,391 removed), laid out by clang-format 23.

**Checked on this machine**: every target built with clang in Release and in Debug, no warning; `ctest` 12 of 12; the 302 scenarios `golden: SAME` (2 min 39 s); the inventory made again on the converted code: `Com_Printf` 2,842 calls, `Com_Warning` 781, `Com_PrintError` 357, `Com_DPrintf` 375, `Com_Error` 940, no printing function without the format attribute, the old functions and table entries gone from every tree, comments included (only `l_crc.cpp`'s include comment names `botimport.Print`, as it did before this part, the file using nothing of it). clang-tidy reports an error in SDL2's header for the three `sdl_*.cpp` files and still reads their calls. Then the same day, the VM validations ALL OK without a warning (`validate.sh`: GCC 15 and Clang 23; `validate.bat`: MSVC), and the check in game passed (the MSVC build on Windows: a setting out of range writes its `WARNING:` in yellow, `unset` of an engine's setting its `ERROR:` in red, where both were white with `WARNING:` and `Error:`; with `developer 1` the renderer's loading messages in white; a bot added, a map that does not exist refused; the single player of Jedi Academy's console as before). A warning written in several calls is yellow up to the end of its first call only: the console starts every call in white (`CL_ConsolePrint`), so the rest of the line, printed by `Com_Printf`, is white; part 3's function for a line written in several calls takes it.

**Part 2 done** (22 November 2026).

### 7.4 The texts that change

From `convert.py`'s logs (`changes.py`), the 3,062 calls rewritten by the start of their visible text, the colour codes and the level word: 2,404 print the same text as before.

| before | now | calls |
|---|---|---|
| `WARNING: ` without a colour | `^3WARNING: ` | 140 |
| the bot library's `^1Error: `, and `^1Error: ` written by hand | `^1ERROR: ` | 111 |
| the bot library's `^1Fatal: ` (`PRT_FATAL`, `AAS_Error`) | `^1ERROR: ` | 68 |
| `ERROR: ` without a colour (and `ERROR:  `) | `^1ERROR: ` | 24 |
| `^3` alone, a renderer's warning without the word | `^3WARNING: ` | 28 |
| `^3Warning: ` (the bot library's and by hand) | `^3WARNING: ` | 26 |
| `Warning: ` without a colour | `^3WARNING: ` | 13 |
| `Error: ` without a colour | `^1ERROR: ` | 11 |
| a warning written in red, `^1WARNING: ` | `^3WARNING: ` | 6 |
| `^1ERROR:  `, `^1ERROR : ` (the spacing) | `^1ERROR: ` | 5 |
| a renderer's developer's message, red by `CL_RefPrintf` | without that red (its own colour kept: `^1^1` becomes `^1`, `^1^3WARNING: ` `^3WARNING: `) | 50 |
| `Com_OPrintf`, the Windows debugger's window or the standard output | the console and the log, with `developer` | 45 |
| the effects system's messages in the single players, printed always outside `FINAL_BUILD` (so never in Release) | with `developer`, in every build | 90 |
| the effects system's messages in the multiplayer, with `developer` | the same | 40 |

### 7.5 The messages left for plan step 27

At the end of part 2, the 419 messages whose format starts with a red or yellow colour and no level word (373), or with a level word that is not a prefix (`Error, expected:`, `error reading file`, `...WARNING: `, a leading newline, `WARNING (FS_ComparePaks): `, and the developer's warnings), made by `colour_only.py`; they stay messages (decided), to sort into levels with step 27.

<details>
<summary>The 419 messages</summary>

| place | function | the format's start |
|---|---|---|
| `src/code/cgame/cg_camera.cpp:1633` | `Com_Printf` | `^1ROFF camera playback failed\n` |
| `src/code/cgame/cg_credits.cpp:294` | `Com_Printf` | `^3CG_Credits_Init(): No current support for cards after scroll!\n` |
| `src/code/cgame/cg_credits.cpp:317` | `Com_Printf` | `^3CG_Credits_Init(): bad keyword \"%s\"!\n` |
| `src/code/cgame/cg_ents.cpp:2411` | `Com_Printf` | `^3Type-specific notetrack error: %s\n` |
| `src/code/cgame/cg_main.cpp:870` | `Com_Printf` | `^1Failed to load model file %s\n` |
| `src/code/cgame/cg_main.cpp:882` | `Com_Printf` | `^1Failed to load model file %s\n` |
| `src/code/cgame/cg_main.cpp:895` | `Com_Printf` | `^1Failed to load model file %s\n` |
| `src/code/cgame/cg_main.cpp:914` | `Com_Printf` | `^1Failed to load animation file set models/players/%s\n` |
| `src/code/cgame/cg_main.cpp:2580` | `Com_Printf` | `^3hud menu file not found: %s, using default\n` |
| `src/code/cgame/cg_players.cpp:549` | `Com_Printf` | `^1Bad animFileIndex: %d\n` |
| `src/code/cgame/cg_text.cpp:616` | `Com_Printf` | `^1CG_CenterPrint: cannot find reference '%s' in StringPackage!\n` |
| `src/code/client/cl_cin.cpp:1960` | `Com_Printf` | `^1PlayCinematic(): Failed to open \"%s\"\n` |
| `src/code/client/cl_main.cpp:876` | `Com_Printf` | `^3%s` |
| `src/code/client/cl_mp3.cpp:42` | `Com_Printf` | `^1%s(%s)\n` |
| `src/code/client/cl_mp3.cpp:67` | `Com_Printf` | `^1%s\n(File: %s)\n` |
| `src/code/client/cl_mp3.cpp:85` | `Com_Printf` | `^1%s\n(File: %s)\n` |
| `src/code/client/cl_mp3.cpp:102` | `Com_Printf` | `^1MP3Stream_InitPlayingTimeFields(): %s\n(File: %s)\n` |
| `src/code/client/cl_mp3.cpp:157` | `Com_Printf` | `^1%s\n(File: %s)\n` |
| `src/code/client/cl_mp3.cpp:273` | `Com_Printf` | `^3File \"%s\": %s\n` |
| `src/code/client/snd_dma.cpp:604` | `Com_Printf` | `^1Sound name exceeds MAX_QPATH - %s\n` |
| `src/code/client/snd_dma.cpp:627` | `Com_DPrintf` | `^3WARNING: could not find %s - using default\n` |
| `src/code/client/snd_dma.cpp:694` | `Com_Printf` | `^3...overrides %s\n` |
| `src/code/client/snd_dma.cpp:719` | `Com_Printf` | `^1***kicking %s\n` |
| `src/code/client/snd_dma.cpp:2422` | `Com_Printf` | `^1Couldn't open music file %s\n` |
| `src/code/client/snd_dma.cpp:2496` | `Com_Printf` | `^1Error streaming file %s: %s\n` |
| `src/code/client/snd_dma.cpp:2872` | `Com_Printf` | `^1Dynamic music did not have both 'action' and 'explore' versions, inh` |
| `src/code/client/snd_dma.cpp:2879` | `Com_Printf` | `^1Unable to find music \"%s\" as explicit track or dynamic music entry` |
| `src/code/client/snd_dma.cpp:3006` | `Com_Printf` | `^1StreamedRead failure on music track\n` |
| `src/code/client/snd_mem.cpp:610` | `Com_Printf` | `^3Foreign file missing: \"%s\"! (using English...)\n` |
| `src/code/client/snd_mem.cpp:728` | `Com_Printf` | `^3**** MP3 %s final unpack size %d different to previous value %d\n` |
| `src/code/client/snd_music.cpp:144` | `Com_Printf` | `^3%s` |
| `src/code/client/snd_music.cpp:682` | `Com_Printf` | `^1Music_GetFileNameForState( %d ) unhandled case!\n` |
| `src/code/client/snd_music.cpp:824` | `Com_Printf` | `^1Music_AllowedToTransition() unable to find entry marker \"%s\" in \"` |
| `src/code/client/snd_music.cpp:833` | `Com_Printf` | `^1Music_AllowedToTransition() unable to find %s version of \"%s\"\n` |
| `src/code/client/snd_music.cpp:848` | `Com_Printf` | `^1Music_AllowedToTransition(): No code to transition from music type %` |
| `src/code/client/snd_music.cpp:857` | `Com_Printf` | `^1Music_AllowedToTransition(): Illegal exit point %zu, max = %zu (musi` |
| `src/code/game/AI_Jedi.cpp:1076` | `Com_Printf` | `^3%s Saber Attack Set: medium\n` |
| `src/code/game/AI_Jedi.cpp:1079` | `Com_Printf` | `^1%s Saber Attack Set: strong\n` |
| `src/code/game/AI_Jedi.cpp:2465` | `Com_Printf` | `^1jump not safe, cancelling!` |
| `src/code/game/AI_Jedi.cpp:2476` | `Com_Printf` | `^1jump not safe, cancelling!\n` |
| `src/code/game/AI_Jedi.cpp:3308` | `Com_Printf` | `^1enemy saber dist: %4.2f\n` |
| `src/code/game/NPC_spawn.cpp:3670` | `Com_Printf` | `^1NPC_Spawn Error: Out of entities!\n` |
| `src/code/game/NPC_spawn.cpp:3679` | `Com_Printf` | `^1Error, expected:\n NPC spawn [NPC type (from NCPCs.cfg)]\n` |
| `src/code/game/NPC_spawn.cpp:3687` | `Com_Printf` | `^1Error, expected:\n NPC spawn vehicle [NPC type (from NCPCs.cfg)]\n` |
| `src/code/game/NPC_spawn.cpp:3765` | `Com_Printf` | `^1Error, Expected:\n` |
| `src/code/game/NPC_spawn.cpp:3766` | `Com_Printf` | `^1NPC kill '[NPC targetname]' - kills NPCs with certain targetname\n` |
| `src/code/game/NPC_spawn.cpp:3767` | `Com_Printf` | `^1or\n` |
| `src/code/game/NPC_spawn.cpp:3768` | `Com_Printf` | `^1NPC kill 'all' - kills all NPCs\n` |
| `src/code/game/NPC_spawn.cpp:3769` | `Com_Printf` | `^1or\n` |
| `src/code/game/NPC_spawn.cpp:3770` | `Com_Printf` | `^1NPC team '[teamname]' - kills all NPCs of a certain team ('nonally' ` |
| `src/code/game/NPC_spawn.cpp:3778` | `Com_Printf` | `^1NPC_Kill Error: 'npc kill team' requires a team name!\n` |
| `src/code/game/NPC_spawn.cpp:3779` | `Com_Printf` | `^1Valid team names are:\n` |
| `src/code/game/NPC_spawn.cpp:3781` | `Com_Printf` | `^1%s\n` |
| `src/code/game/NPC_spawn.cpp:3783` | `Com_Printf` | `^1nonally - kills all but your teammates\n` |
| `src/code/game/NPC_spawn.cpp:3794` | `Com_Printf` | `^1NPC_Kill Error: team '%s' not recognized\n` |
| `src/code/game/NPC_spawn.cpp:3795` | `Com_Printf` | `^1Valid team names are:\n` |
| `src/code/game/NPC_spawn.cpp:3797` | `Com_Printf` | `^1%s\n` |
| `src/code/game/NPC_spawn.cpp:3799` | `Com_Printf` | `^1nonally - kills all but your teammates\n` |
| `src/code/game/NPC_stats.cpp:1083` | `Com_Printf` | `^1Failed find animation file name models/players/%s\n` |
| `src/code/game/NPC_stats.cpp:1099` | `Com_Printf` | `^1Failed to load animation file set models/players/%s/animation.cfg\n` |
| `src/code/game/bg_panimate.cpp:4209` | `Com_Printf` | `^1PM_SetAnimFinal: Invalid Anim Index (%d)!\n` |
| `src/code/game/bg_panimate.cpp:4212` | `Com_Printf` | `^1PM_SetAnimFinal: Invalid Anim File Index (%d)!\n` |
| `src/code/game/bg_panimate.cpp:4237` | `Com_Printf` | `^1PM_SetAnimFinal: Anim %s does not exist in this model (%s)!\n` |
| `src/code/game/bg_vehicleLoad.cpp:203` | `Com_Printf` | `^3BG_ParseVehWeaponParm: VEC3 sscanf() failed to read 3 floats ('angle` |
| `src/code/game/bg_vehicleLoad.cpp:860` | `Com_Printf` | `^3BG_ParseVehicleParm: VEC3 sscanf() failed to read 3 floats ('angle' ` |
| `src/code/game/bg_vehicleLoad.cpp:1359` | `Com_Printf` | `error reading file\n` |
| `src/code/game/bg_vehicleLoad.cpp:1448` | `Com_Printf` | `error reading file\n` |
| `src/code/game/g_breakable.cpp:454` | `Com_Printf` | `^3Recursive misc_model_breakable_die. Use targets probably pointing ba` |
| `src/code/game/g_client.cpp:896` | `Com_Printf` | `^1WRAPPED\n` |
| `src/code/game/g_client.cpp:921` | `Com_Printf` | `^1WRAPPED\n` |
| `src/code/game/g_client.cpp:1611` | `Com_Printf` | `^1G_SetG2PlayerModel: cannot load model %s\n` |
| `src/code/game/g_client.cpp:1627` | `Com_Printf` | `^1couldn't load playerModel %s!\n` |
| `src/code/game/g_client.cpp:1828` | `Com_Printf` | `^1G_ChangePlayerModel: cannot find NPC %s\n` |
| `src/code/game/g_combat.cpp:3210` | `Com_Printf` | `^3combatant %s died, killer anim = %s\n` |
| `src/code/game/g_items.cpp:1067` | `Com_Printf` | `^1FinishSpawningItem: removing %s startsolid at %s (in a %s)\n` |
| `src/code/game/g_items.cpp:1071` | `Com_Printf` | `^1FinishSpawningItem: removing %s startsolid at %s\n` |
| `src/code/game/g_roff.cpp:382` | `Com_Printf` | `^1Type-specific NoteTrack error: %s\n` |
| `src/code/game/g_roff.cpp:563` | `Com_Printf` | `^1MAX_ROFFS count exceeded. Skipping load of .ROF '%s'\n` |
| `src/code/game/g_roff.cpp:586` | `Com_Printf` | `^1Could not open .ROF file '%s'\n` |
| `src/code/game/g_roff.cpp:595` | `Com_Printf` | `^1Invalid .ROF format '%s'\n` |
| `src/code/game/g_spawn.cpp:867` | `Com_Printf` | `^1G_CallSpawn: NULL classname\n` |
| `src/code/game/g_spawn.cpp:968` | `Com_Printf` | `^3G_ParseField: VEC3 sscanf() failed to read 3 floats ('angle' key bug` |
| `src/code/game/g_spawn.cpp:980` | `Com_Printf` | `^3G_ParseField: VEC4 sscanf() failed to read 4 floats\n` |
| `src/code/game/g_svcmds.cpp:491` | `Com_Printf` | `^3Lightsaber Combat Style: Medium\n` |
| `src/code/game/g_svcmds.cpp:495` | `Com_Printf` | `^1Lightsaber Combat Style: Strong\n` |
| `src/code/game/g_svcmds.cpp:597` | `Com_Printf` | `^1runscript: can't find targetname %s\n` |
| `src/code/game/g_svcmds.cpp:605` | `Com_Printf` | `^1usage: runscript <ent targetname> scriptname\n` |
| `src/code/game/g_svcmds.cpp:614` | `Com_Printf` | `^1'playerteam' - change player team, requires a team name!\n` |
| `src/code/game/g_svcmds.cpp:615` | `Com_Printf` | `^1Current team is: %s\n` |
| `src/code/game/g_svcmds.cpp:616` | `Com_Printf` | `^1Valid team names are:\n` |
| `src/code/game/g_svcmds.cpp:618` | `Com_Printf` | `^1%s\n` |
| `src/code/game/g_svcmds.cpp:625` | `Com_Printf` | `^1'playerteam' unrecognized team name %s!\n` |
| `src/code/game/g_svcmds.cpp:626` | `Com_Printf` | `^1Current team is: %s\n` |
| `src/code/game/g_svcmds.cpp:627` | `Com_Printf` | `^1Valid team names are:\n` |
| `src/code/game/g_svcmds.cpp:629` | `Com_Printf` | `^1%s\n` |
| `src/code/game/g_svcmds.cpp:644` | `Com_Printf` | `^1control <NPC_targetname>\n` |
| `src/code/game/g_svcmds.cpp:657` | `Com_Printf` | `^1grab <NPC_targetname>\n` |
| `src/code/game/g_svcmds.cpp:673` | `Com_Printf` | `^1USAGE: playerModel <NPC Name>\n playerModel <g2model> <skinhead> <sk` |
| `src/code/game/g_svcmds.cpp:703` | `Com_Printf` | `^1USAGE: playerTint <red 0 - 255> <green 0 - 255> <blue 0 - 255>\n` |
| `src/code/game/g_svcmds.cpp:769` | `Com_Printf` | `^1Invalid difficulty cvar set! g_spskill (%i) [0-2] is valid range onl` |
| `src/code/game/g_target.cpp:786` | `Com_Printf` | `^1SP_target_scriptrunner %s has no USESCRIPT\n` |
| `src/code/game/g_trigger.cpp:402` | `Com_Printf` | `^3trigger_multiple has random >= wait\n` |
| `src/code/game/wp_saber.cpp:1360` | `Com_Printf` | `^1damage: fake, hitLoc %d\n` |
| `src/code/game/wp_saber.cpp:1363` | `Com_Printf` | `^1damage: %4.2f, hitLoc %d\n` |
| `src/code/game/wp_saber.cpp:2308` | `Com_Printf` | `^1%d saber hit owner directly %4.2f\n` |
| `src/code/game/wp_saber.cpp:4473` | `Com_Printf` | `^1%s knockaway %s's attack, new move = %s, anim = %s\n` |
| `src/code/game/wp_saber.cpp:4528` | `Com_Printf` | `^1%s parry broken (bounce/deflect)!\n` |
| `src/code/game/wp_saber.cpp:4531` | `Com_Printf` | `^1%s parry broken (follow-through)!\n` |
| `src/code/qcommon/cm_patch.cpp:769` | `Com_DPrintf` | `WARNING: CM_SetBorderInward: mixed plane sides\n` |
| `src/code/qcommon/cm_patch.cpp:978` | `Com_DPrintf` | `WARNING: CM_AddFacetBevels... invalid bevel\n` |
| `src/code/qcommon/cmd.cpp:151` | `Com_DPrintf` | `^3EXEC_NOW %s\n` |
| `src/code/qcommon/cmd.cpp:156` | `Com_DPrintf` | `^3EXEC_NOW %s\n` |
| `src/code/qcommon/common.cpp:1275` | `Com_Printf` | `^3Com_WriteConfig_f: The filename \"%s\" is reserved! Please choose an` |
| `src/code/qcommon/cvar.cpp:391` | `Com_DPrintf` | `^3Warning: cvar \"%s\" given initial values: \"%s\" and \"%s\"\n` |
| `src/code/qcommon/cvar.cpp:1293` | `Com_DPrintf` | `^3WARNING: Unsetting CVAR_ROM cvar '%s', since it is also CVAR_ARCHIVE` |
| `src/code/qcommon/msg.cpp:120` | `Com_Printf` | `^1MSG_WriteBits: buffer Full writing %d in %d bits\n` |
| `src/code/qcommon/msg.cpp:136` | `Com_Printf` | `^1MSG_WriteBits: overflow writing %d in %d bits\n` |
| `src/code/qcommon/msg.cpp:150` | `Com_Printf` | `^1MSG_WriteBits: overflow writing %d in %d bits\n` |
| `src/code/qcommon/ojk_saved_game.cpp:77` | `Com_Printf` | `^1File \"%s\" has version # %d (expecting %d)\n` |
| `src/code/qcommon/ojk_saved_game.cpp:83` | `Com_Printf` | `^1Failed to read a version.\n` |
| `src/code/qcommon/ojk_saved_game.cpp:579` | `Com_Printf` | `^1Error during savegame-rename. Check \"%s\" for write-protect or disk` |
| `src/code/qcommon/q_shared.cpp:829` | `Com_Printf` | `^3Can't use keys or values with a '%c': %s = %s\n` |
| `src/code/qcommon/stringed_ingame.cpp:846` | `Com_DPrintf` | `^3SE_Load(): Couldn't load \"%s\"!\n` |
| `src/code/qcommon/strip.cpp:902` | `Com_DPrintf` | `^3JK2SP_Register: Registered client string package '%s' with ID %02x\n` |
| `src/code/qcommon/strip.cpp:905` | `Com_DPrintf` | `^3JK2SP_Register: Registered string package '%s' with ID %02x\n` |
| `src/code/qcommon/strip.cpp:933` | `Com_DPrintf` | `^3JK2SP_UnRegister: Package '%s' with ID %02x\n` |
| `src/code/qcommon/z_memman_pc.cpp:322` | `Com_Printf` | `^1Z_Malloc(): Failed to alloc %d bytes (TAG_%s) !!!!!\n` |
| `src/code/rd-vanilla/tr_main.cpp:954` | `Com_DPrintf` | `WARNING: recursive mirror/portal found\n` |
| `src/code/rd-vanilla/tr_scene.cpp:136` | `Com_DPrintf` | `^3WARNING: RE_AddPolyToScene: NULL poly shader\n` |
| `src/code/rd-vanilla/tr_scene.cpp:148` | `Com_DPrintf` | `^3WARNING: RE_AddPolyToScene: r_max_polys or r_max_polyverts reached\n` |
| `src/code/rd-vanilla/tr_shader.cpp:2937` | `Com_DPrintf` | `WARNING: shader '%s' has lightmap but no lightmap stage!\n` |
| `src/code/rd-vanilla/tr_shader.cpp:3118` | `Com_Printf` | `^1Shader name exceeds MAX_QPATH! %s\n` |
| `src/code/server/sv_savegame.cpp:132` | `Com_Printf` | `^1USAGE: wipe <name>\n` |
| `src/code/server/sv_savegame.cpp:136` | `Com_Printf` | `^1Can't wipe 'auto'\n` |
| `src/code/server/sv_savegame.cpp:194` | `Com_Printf` | `^1Bad loadgame name.\n` |
| `src/code/server/sv_savegame.cpp:199` | `Com_Printf` | `^1Can't load from \"current\"\n` |
| `src/code/server/sv_savegame.cpp:272` | `Com_Printf` | `^1Server is not running\n` |
| `src/code/server/sv_savegame.cpp:277` | `Com_Printf` | `^1You must be in a game to save.\n` |
| `src/code/server/sv_savegame.cpp:290` | `Com_Printf` | `^1\nCan't savegame while dead!\n` |
| `src/code/server/sv_savegame.cpp:292` | `Com_Printf` | `^1\n%s\n` |
| `src/code/server/sv_savegame.cpp:302` | `Com_Printf` | `^1\nCan't savegame while dead!\n` |
| `src/code/server/sv_savegame.cpp:304` | `Com_Printf` | `^1\n%s\n` |
| `src/code/server/sv_savegame.cpp:315` | `Com_Printf` | `^1Can't save to 'current'\n` |
| `src/code/server/sv_savegame.cpp:320` | `Com_Printf` | `^1Bad savegame name.\n` |
| `src/code/server/sv_savegame.cpp:355` | `Com_Printf` | `^1Failed.\n` |
| `src/code/server/sv_savegame.cpp:357` | `Com_Printf` | `^1%s.\n` |
| `src/code/ui/ui_atoms.cpp:441` | `Com_Printf` | `^3UI_RegisterFont: MAX_FONTS (%i) exceeded\n` |
| `src/code/ui/ui_main.cpp:2579` | `Com_Printf` | `^3menu file not found: %s, using default\n` |
| `src/code/ui/ui_shared.cpp:10313` | `Com_Printf` | `^3Menu_HandleKey[F12]: Unknown screenshot format assigned! This should` |
| `src/codeJK2/cgame/cg_camera.cpp:1227` | `Com_Printf` | `^1ROFF camera playback failed\n` |
| `src/codeJK2/cgame/cg_credits.cpp:277` | `Com_Printf` | `^3CG_Credits_Init(): No current support for cards after scroll!\n` |
| `src/codeJK2/cgame/cg_credits.cpp:300` | `Com_Printf` | `^3CG_Credits_Init(): bad keyword \"%s\"!\n` |
| `src/codeJK2/cgame/cg_ents.cpp:2194` | `Com_Printf` | `^3Type-specific notetrack error: %s\n` |
| `src/codeJK2/cgame/cg_main.cpp:819` | `Com_Printf` | `^1Failed to load model file %s\n` |
| `src/codeJK2/cgame/cg_main.cpp:831` | `Com_Printf` | `^1Failed to load model file %s\n` |
| `src/codeJK2/cgame/cg_main.cpp:844` | `Com_Printf` | `^1Failed to load model file %s\n` |
| `src/codeJK2/cgame/cg_main.cpp:862` | `Com_Printf` | `^1Failed to load animation file set models/players/%s\n` |
| `src/codeJK2/cgame/cg_main.cpp:2127` | `Com_Printf` | `^3menu file not found: %s, using default\n` |
| `src/codeJK2/cgame/cg_players.cpp:442` | `Com_Printf` | `^1Bad animFileIndex: %d\n` |
| `src/codeJK2/cgame/cg_players.cpp:556` | `Com_Printf` | `^1ParseAnimationSndBlock: sound %s does not exist (animsound.cfg %s)!\` |
| `src/codeJK2/cgame/cg_text.cpp:605` | `Com_Printf` | `^1CG_CenterPrint: cannot find reference '%s' in StringPackage!\n` |
| `src/codeJK2/game/AI_Jedi.cpp:638` | `Com_Printf` | `^3%s Saber Attack Set: medium\n` |
| `src/codeJK2/game/AI_Jedi.cpp:641` | `Com_Printf` | `^1%s Saber Attack Set: strong\n` |
| `src/codeJK2/game/AI_Jedi.cpp:2059` | `Com_Printf` | `^1enemy saber dist: %4.2f\n` |
| `src/codeJK2/game/NPC_spawn.cpp:2571` | `Com_Printf` | `^1NPC_Spawn Error: Out of entities!\n` |
| `src/codeJK2/game/NPC_spawn.cpp:2580` | `Com_Printf` | `^1Error, expected:\n NPC spawn [NPC type (from NCPCs.cfg)]\n` |
| `src/codeJK2/game/NPC_spawn.cpp:2684` | `Com_Printf` | `^1Error, Expected:\n` |
| `src/codeJK2/game/NPC_spawn.cpp:2685` | `Com_Printf` | `^1NPC kill '[NPC targetname]' - kills NPCs with certain targetname\n` |
| `src/codeJK2/game/NPC_spawn.cpp:2686` | `Com_Printf` | `^1or\n` |
| `src/codeJK2/game/NPC_spawn.cpp:2687` | `Com_Printf` | `^1NPC kill 'all' - kills all NPCs\n` |
| `src/codeJK2/game/NPC_spawn.cpp:2688` | `Com_Printf` | `^1or\n` |
| `src/codeJK2/game/NPC_spawn.cpp:2689` | `Com_Printf` | `^1NPC team '[teamname]' - kills all NPCs of a certain team ('nonally' ` |
| `src/codeJK2/game/NPC_spawn.cpp:2697` | `Com_Printf` | `^1NPC_Kill Error: 'npc kill team' requires a team name!\n` |
| `src/codeJK2/game/NPC_spawn.cpp:2698` | `Com_Printf` | `^1Valid team names are:\n` |
| `src/codeJK2/game/NPC_spawn.cpp:2700` | `Com_Printf` | `^1%s\n` |
| `src/codeJK2/game/NPC_spawn.cpp:2702` | `Com_Printf` | `^1nonally - kills all but your teammates\n` |
| `src/codeJK2/game/NPC_spawn.cpp:2713` | `Com_Printf` | `^1NPC_Kill Error: team '%s' not recognized\n` |
| `src/codeJK2/game/NPC_spawn.cpp:2714` | `Com_Printf` | `^1Valid team names are:\n` |
| `src/codeJK2/game/NPC_spawn.cpp:2716` | `Com_Printf` | `^1%s\n` |
| `src/codeJK2/game/NPC_spawn.cpp:2718` | `Com_Printf` | `^1nonally - kills all but your teammates\n` |
| `src/codeJK2/game/NPC_stats.cpp:433` | `Com_Printf` | `^1Failed find animation file name models/players/%s/animation.cfg\n` |
| `src/codeJK2/game/NPC_stats.cpp:447` | `Com_Printf` | `^1Failed to load animation file set models/players/%s/animation.cfg\n` |
| `src/codeJK2/game/NPC_stats.cpp:1931` | `Com_Printf` | `error reading file\n` |
| `src/codeJK2/game/g_ICARUS.cpp:328` | `Com_Printf` | `^1Could not open file '%s'\n` |
| `src/codeJK2/game/g_client.cpp:1210` | `Com_Printf` | `^1couldn't load playerModel %s!\n` |
| `src/codeJK2/game/g_combat.cpp:2918` | `Com_Printf` | `^3combatant %s died, killer anim = %s\n` |
| `src/codeJK2/game/g_items.cpp:800` | `Com_Printf` | `^1FinishSpawningItem: removing %s startsolid at %s (in a %s)\n` |
| `src/codeJK2/game/g_items.cpp:804` | `Com_Printf` | `^1FinishSpawningItem: removing %s startsolid at %s\n` |
| `src/codeJK2/game/g_main.cpp:1074` | `Com_Printf` | `^1Not saving .nav file due to fatal nav errors\n` |
| `src/codeJK2/game/g_navigator.cpp:1779` | `Com_Printf` | `^1NAV ERROR: envalid ent %d\n` |
| `src/codeJK2/game/g_navigator.cpp:1788` | `Com_Printf` | `^1NAV ERROR: tried to fail invalid waypoint %d\n` |
| `src/codeJK2/game/g_navigator.cpp:1797` | `Com_Printf` | `^1NAV ERROR: tried to fail invalid waypoint %d\n` |
| `src/codeJK2/game/g_navigator.cpp:1881` | `Com_Printf` | `^1NAV ERROR: too many blocked waypoint connections (%d)!!!\n` |
| `src/codeJK2/game/g_ref.cpp:203` | `Com_Printf` | `^1Duplicate tag name \"%s\"\n` |
| `src/codeJK2/game/g_roff.cpp:382` | `Com_Printf` | `^1MAX_ROFFS count exceeded. Skipping load of .ROF '%s'\n` |
| `src/codeJK2/game/g_roff.cpp:405` | `Com_Printf` | `^1Could not open .ROF file '%s'\n` |
| `src/codeJK2/game/g_roff.cpp:414` | `Com_Printf` | `^1Invalid roff format '%s'\n` |
| `src/codeJK2/game/g_spawn.cpp:703` | `Com_Printf` | `^1G_CallSpawn: NULL classname\n` |
| `src/codeJK2/game/g_spawn.cpp:724` | `Com_Printf` | `^1%s doesn't have a spawn function\n` |
| `src/codeJK2/game/g_spawn.cpp:802` | `Com_Printf` | `^3G_ParseField: VEC3 sscanf() failed to read 3 floats ('angle' key bug` |
| `src/codeJK2/game/g_spawn.cpp:815` | `Com_Printf` | `^3G_ParseField: VEC4 sscanf() failed to read 4 floats\n` |
| `src/codeJK2/game/g_svcmds.cpp:323` | `Com_Printf` | `^3Lightsaber Combat Style: Medium\n` |
| `src/codeJK2/game/g_svcmds.cpp:327` | `Com_Printf` | `^1Lightsaber Combat Style: Strong\n` |
| `src/codeJK2/game/g_svcmds.cpp:377` | `Com_Printf` | `^1runscript: can't find targetname %s\n` |
| `src/codeJK2/game/g_svcmds.cpp:385` | `Com_Printf` | `^1usage: runscript <ent targetname> scriptname\n` |
| `src/codeJK2/game/g_svcmds.cpp:394` | `Com_Printf` | `^1'playerteam' - change player team, requires a team name!\n` |
| `src/codeJK2/game/g_svcmds.cpp:395` | `Com_Printf` | `^1Current team is: %s\n` |
| `src/codeJK2/game/g_svcmds.cpp:396` | `Com_Printf` | `^1Valid team names are:\n` |
| `src/codeJK2/game/g_svcmds.cpp:398` | `Com_Printf` | `^1%s\n` |
| `src/codeJK2/game/g_svcmds.cpp:405` | `Com_Printf` | `^1'playerteam' unrecognized team name %s!\n` |
| `src/codeJK2/game/g_svcmds.cpp:406` | `Com_Printf` | `^1Current team is: %s\n` |
| `src/codeJK2/game/g_svcmds.cpp:407` | `Com_Printf` | `^1Valid team names are:\n` |
| `src/codeJK2/game/g_svcmds.cpp:409` | `Com_Printf` | `^1%s\n` |
| `src/codeJK2/game/g_svcmds.cpp:424` | `Com_Printf` | `^1control <NPC_targetname>\n` |
| `src/codeJK2/game/g_svcmds.cpp:478` | `Com_Printf` | `^1Invalid difficulty cvar set! g_spskill (%i) [0-2] is valid range onl` |
| `src/codeJK2/game/g_trigger.cpp:379` | `Com_Printf` | `^3trigger_multiple has random >= wait\n` |
| `src/codeJK2/game/wp_saber.cpp:769` | `Com_Printf` | `^1damage: %4.2f, hitLoc %d\n` |
| `src/codeJK2/game/wp_saber.cpp:1487` | `Com_Printf` | `^1%d saber hit owner directly %4.2f\n` |
| `src/codeJK2/game/wp_saber.cpp:2704` | `Com_Printf` | `^1%s knockaway %s's attack, new move = %s, anim = %s\n` |
| `src/codeJK2/game/wp_saber.cpp:2753` | `Com_Printf` | `^1%s parry broken (bounce/deflect)!\n` |
| `src/codeJK2/game/wp_saber.cpp:2756` | `Com_Printf` | `^1%s parry broken (follow-through)!\n` |
| `src/codemp/cgame/cg_ents.cpp:3423` | `Com_Printf` | `^3Type-specific notetrack error: %s\n` |
| `src/codemp/cgame/cg_main.cpp:1580` | `Com_Printf` | `^1menu file not found: %s, using default\n` |
| `src/codemp/cgame/cg_main.cpp:1584` | `Com_Printf` | `^1menu file too large: %s is %i, max allowed is %i\n` |
| `src/codemp/cgame/cg_main.cpp:2122` | `Com_Printf` | `^3hud menu file not found: %s, using default\n` |
| `src/codemp/client/cl_avi.cpp:333` | `Com_Printf` | `^1cl_aviFrameRate must be >= 1\n` |
| `src/codemp/client/cl_cin.cpp:1714` | `Com_Printf` | `^1PlayCinematic(): Failed to open \"%s\"\n` |
| `src/codemp/client/cl_main.cpp:1478` | `Com_Printf` | `\nWARNING: You are missing some files referenced by the server:\n%sYou` |
| `src/codemp/client/cl_main.cpp:2557` | `Com_Printf` | `error adding favorite server: too many favorite servers\n` |
| `src/codemp/client/cl_main.cpp:2560` | `Com_Printf` | `error adding favorite server: server already exists\n` |
| `src/codemp/client/snd_dma.cpp:608` | `Com_Printf` | `^1Sound name exceeds MAX_QPATH - %s\n` |
| `src/codemp/client/snd_dma.cpp:700` | `Com_Printf` | `^3...overrides %s\n` |
| `src/codemp/client/snd_dma.cpp:725` | `Com_Printf` | `^1***kicking %s\n` |
| `src/codemp/client/snd_dma.cpp:2470` | `Com_Printf` | `^1Couldn't open music file %s\n` |
| `src/codemp/client/snd_dma.cpp:2544` | `Com_Printf` | `^1Error streaming file %s: %s\n` |
| `src/codemp/client/snd_dma.cpp:2920` | `Com_Printf` | `^1Dynamic music did not have both 'action' and 'explore' versions, inh` |
| `src/codemp/client/snd_dma.cpp:2927` | `Com_Printf` | `^1Unable to find music \"%s\" as explicit track or dynamic music entry` |
| `src/codemp/client/snd_dma.cpp:3054` | `Com_Printf` | `^1StreamedRead failure on music track\n` |
| `src/codemp/client/snd_mem.cpp:612` | `Com_Printf` | `^3Foreign file missing: \"%s\"! (using English...)\n` |
| `src/codemp/client/snd_mem.cpp:731` | `Com_Printf` | `^3**** MP3 %s final unpack size %d different to previous value %d\n` |
| `src/codemp/client/snd_mp3.cpp:42` | `Com_Printf` | `^1%s(%s)\n` |
| `src/codemp/client/snd_mp3.cpp:67` | `Com_Printf` | `^1%s\n(File: %s)\n` |
| `src/codemp/client/snd_mp3.cpp:85` | `Com_Printf` | `^1%s\n(File: %s)\n` |
| `src/codemp/client/snd_mp3.cpp:102` | `Com_Printf` | `^1MP3Stream_InitPlayingTimeFields(): %s\n(File: %s)\n` |
| `src/codemp/client/snd_mp3.cpp:157` | `Com_Printf` | `^1%s\n(File: %s)\n` |
| `src/codemp/client/snd_mp3.cpp:273` | `Com_Printf` | `^3File \"%s\": %s\n` |
| `src/codemp/client/snd_music.cpp:117` | `Com_Printf` | `^1Error parsing music data ( in \"%s\" ):\n%s\n` |
| `src/codemp/client/snd_music.cpp:132` | `Com_Printf` | `^3%s` |
| `src/codemp/client/snd_music.cpp:790` | `Com_Printf` | `^1Music_GetFileNameForState( %d ) unhandled case!\n` |
| `src/codemp/client/snd_music.cpp:932` | `Com_Printf` | `^1Music_AllowedToTransition() unable to find entry marker \"%s\" in \"` |
| `src/codemp/client/snd_music.cpp:941` | `Com_Printf` | `^1Music_AllowedToTransition() unable to find %s version of \"%s\"\n` |
| `src/codemp/client/snd_music.cpp:956` | `Com_Printf` | `^1Music_AllowedToTransition(): No code to transition from music type %` |
| `src/codemp/client/snd_music.cpp:965` | `Com_Printf` | `^1Music_AllowedToTransition(): Illegal exit point %d, max = %zu (music` |
| `src/codemp/game/NPC_AI_Jedi.cpp:1225` | `Com_Printf` | `^3%s Saber Attack Set: medium\n` |
| `src/codemp/game/NPC_AI_Jedi.cpp:1228` | `Com_Printf` | `^1%s Saber Attack Set: strong\n` |
| `src/codemp/game/NPC_AI_Jedi.cpp:2697` | `Com_Printf` | `^1enemy saber dist: %4.2f\n` |
| `src/codemp/game/NPC_spawn.cpp:3581` | `Com_Printf` | `^1NPC_Spawn Error: Out of entities!\n` |
| `src/codemp/game/NPC_spawn.cpp:3593` | `Com_Printf` | `^1Error, expected one of:\n^7 NPC spawn [NPC type (from ext_data/NPCs)` |
| `src/codemp/game/NPC_spawn.cpp:3733` | `Com_Printf` | `^1Error, Expected:\n` |
| `src/codemp/game/NPC_spawn.cpp:3734` | `Com_Printf` | `^1NPC kill '[NPC targetname]' - kills NPCs with certain targetname\n` |
| `src/codemp/game/NPC_spawn.cpp:3735` | `Com_Printf` | `^1or\n` |
| `src/codemp/game/NPC_spawn.cpp:3736` | `Com_Printf` | `^1NPC kill 'all' - kills all NPCs\n` |
| `src/codemp/game/NPC_spawn.cpp:3737` | `Com_Printf` | `^1or\n` |
| `src/codemp/game/NPC_spawn.cpp:3738` | `Com_Printf` | `^1NPC team '[teamname]' - kills all NPCs of a certain team ('nonally' ` |
| `src/codemp/game/NPC_spawn.cpp:3746` | `Com_Printf` | `^1NPC_Kill Error: 'npc kill team' requires a team name!\n` |
| `src/codemp/game/NPC_spawn.cpp:3747` | `Com_Printf` | `^1Valid team names are:\n` |
| `src/codemp/game/NPC_spawn.cpp:3749` | `Com_Printf` | `^1%s\n` |
| `src/codemp/game/NPC_spawn.cpp:3751` | `Com_Printf` | `^1nonally - kills all but your teammates\n` |
| `src/codemp/game/NPC_spawn.cpp:3762` | `Com_Printf` | `^1NPC_Kill Error: team '%s' not recognized\n` |
| `src/codemp/game/NPC_spawn.cpp:3763` | `Com_Printf` | `^1Valid team names are:\n` |
| `src/codemp/game/NPC_spawn.cpp:3765` | `Com_Printf` | `^1%s\n` |
| `src/codemp/game/NPC_spawn.cpp:3767` | `Com_Printf` | `^1nonally - kills all but your teammates\n` |
| `src/codemp/game/NPC_stats.cpp:3099` | `Com_Printf` | `error reading file\n` |
| `src/codemp/game/ai_util.cpp:578` | `Com_Printf` | `^1Personality file exceeds maximum length\n` |
| `src/codemp/game/ai_util.cpp:599` | `Com_Printf` | `^1Personality file contains no GeneralBotInfo group\n` |
| `src/codemp/game/ai_wpnav.cpp:320` | `Com_Printf` | `^3Waypoint %i\nFlags - %i (%s) (w%f)\nOrigin - (%i %i %i)\n` |
| `src/codemp/game/ai_wpnav.cpp:342` | `Com_Printf` | `^1FATAL ERROR: Could not allocated memory for waypoint\n` |
| `src/codemp/game/ai_wpnav.cpp:474` | `Com_Printf` | `^3Waypoint number %i does not exist\n` |
| `src/codemp/game/ai_wpnav.cpp:489` | `Com_Printf` | `^3Waypoint index %i should exist, but does not (?)\n` |
| `src/codemp/game/ai_wpnav.cpp:540` | `Com_Printf` | `^3Waypoint number %i does not exist\n` |
| `src/codemp/game/ai_wpnav.cpp:555` | `Com_Printf` | `^3Waypoint index %i should exist, but does not (?)\n` |
| `src/codemp/game/ai_wpnav.cpp:608` | `Com_Printf` | `^3Waypoint number %i does not exist\n` |
| `src/codemp/game/ai_wpnav.cpp:623` | `Com_Printf` | `^3Waypoint index %i should exist, but does not (?)\n` |
| `src/codemp/game/ai_wpnav.cpp:674` | `Com_Printf` | `^3Waypoint number %i does not exist\n` |
| `src/codemp/game/ai_wpnav.cpp:689` | `Com_Printf` | `^3Waypoint index %i should exist, but does not (?)\n` |
| `src/codemp/game/ai_wpnav.cpp:701` | `Com_Printf` | `^3WPFlagsModify: Waypoint %i does not exist\n` |
| `src/codemp/game/ai_wpnav.cpp:992` | `Com_Printf` | `^3Point %i is not connected to %i - Repairing...\n` |
| `src/codemp/game/ai_wpnav.cpp:1185` | `Com_Printf` | `^1Could not link %i to %i, unreachable by node branching.\n` |
| `src/codemp/game/ai_wpnav.cpp:1190` | `Com_Printf` | `^3Since points cannot be connected, point %i has been flagged as only-` |
| `src/codemp/game/ai_wpnav.cpp:1262` | `Com_Printf` | `^1Could not link %i to %i, waypoint limit hit.\n` |
| `src/codemp/game/ai_wpnav.cpp:1280` | `Com_Printf` | `^3Finished connecting %i to %i.\n` |
| `src/codemp/game/ai_wpnav.cpp:1824` | `Com_Printf` | `^3Bot route data not found for %s\n` |
| `src/codemp/game/ai_wpnav.cpp:1829` | `Com_Printf` | `^1Route file exceeds maximum length\n` |
| `src/codemp/game/ai_wpnav.cpp:3091` | `Com_Printf` | `^3bot_wp_add^7 - Add a waypoint (optional int parameter will insert th` |
| `src/codemp/game/ai_wpnav.cpp:3093` | `Com_Printf` | `^3bot_wp_rem^7 - Remove a waypoint (removes last unless waypoint index` |
| `src/codemp/game/ai_wpnav.cpp:3094` | `Com_Printf` | `^3bot_wp_addflagged^7 - Same as wp_add, but adds a flagged point (type` |
| `src/codemp/game/ai_wpnav.cpp:3095` | `Com_Printf` | `^3bot_wp_switchflags^7 - Switches flags on an existing waypoint (type ` |
| `src/codemp/game/ai_wpnav.cpp:3096` | `Com_Printf` | `^3bot_wp_tele^7 - Teleport yourself to the specified waypoint's locati` |
| `src/codemp/game/ai_wpnav.cpp:3097` | `Com_Printf` | `^3bot_wp_killoneways^7 - Removes oneway (backward and forward) flags o` |
| `src/codemp/game/ai_wpnav.cpp:3098` | `Com_Printf` | `^3bot_wp_save^7 - Saves all waypoint data into a file for later use\n` |
| `src/codemp/game/ai_wpnav.cpp:3151` | `Com_Printf` | `^3You didn't specify an index. Assuming last.\n` |
| `src/codemp/game/ai_wpnav.cpp:3178` | `Com_Printf` | `^3Flag string needed for bot_wp_addflagged\nj - Jump point\nd - Duck p` |
| `src/codemp/game/ai_wpnav.cpp:3241` | `Com_Printf` | `^3Flag string needed for bot_wp_switchflags\nType bot_wp_addflagged fo` |
| `src/codemp/game/ai_wpnav.cpp:3291` | `Com_Printf` | `^3Waypoint number (to modify) needed for bot_wp_switchflags\nSyntax: b` |
| `src/codemp/game/bg_panimate.cpp:1929` | `Com_Printf` | `^1ParseAnimationSndBlock: sound %s does not exist (animevents.cfg %s)!` |
| `src/codemp/game/bg_vehicleLoad.cpp:170` | `Com_Printf` | `^3BG_ParseVehWeaponParm: VEC3 sscanf() failed to read 3 floats ('angle` |
| `src/codemp/game/bg_vehicleLoad.cpp:779` | `Com_Printf` | `^3BG_ParseVehicleParm: VEC3 sscanf() failed to read 3 floats ('angle' ` |
| `src/codemp/game/bg_vehicleLoad.cpp:1221` | `Com_Printf` | `error reading file\n` |
| `src/codemp/game/bg_vehicleLoad.cpp:1284` | `Com_Printf` | `error reading file\n` |
| `src/codemp/game/g_bot.cpp:119` | `Com_Printf` | `^1file not found: %s\n` |
| `src/codemp/game/g_bot.cpp:123` | `Com_Printf` | `^1file too large: %s is %i, max allowed is %i\n` |
| `src/codemp/game/g_bot.cpp:766` | `Com_Printf` | `^3Unable to delay spawn\n` |
| `src/codemp/game/g_bot.cpp:1203` | `Com_Printf` | `^1file not found: %s\n` |
| `src/codemp/game/g_bot.cpp:1207` | `Com_Printf` | `^1file too large: %s is %i, max allowed is %i\n` |
| `src/codemp/game/g_bot.cpp:1269` | `Com_Printf` | `^1Invalid bot number: %i\n` |
| `src/codemp/game/g_main.cpp:2692` | `Com_Printf` | `^1Not saving .nav file due to fatal nav errors\n` |
| `src/codemp/game/g_misc.cpp:2986` | `Com_Printf` | `^1Duplicate tag name \"%s\"\n` |
| `src/codemp/game/g_trigger.cpp:552` | `Com_Printf` | `^3trigger_multiple has random >= wait\n` |
| `src/codemp/icarus/GameInterface.cpp:373` | `Com_Printf` | `^1Could not open file '%s'\n` |
| `src/codemp/qcommon/RoffSystem.cpp:324` | `Com_Printf` | `^3Ignoring. File '%s' already cached.\n` |
| `src/codemp/qcommon/RoffSystem.cpp:335` | `Com_Printf` | `^1Could not open .ROF file '%s'\n` |
| `src/codemp/qcommon/RoffSystem.cpp:342` | `Com_Printf` | `^1cache failed: roff <%s> does not exist or is not a valid roff\n` |
| `src/codemp/qcommon/RoffSystem.cpp:431` | `Com_Printf` | `^1unload failed: roff <%i> does not exist\n` |
| `src/codemp/qcommon/RoffSystem.cpp:563` | `Com_Printf` | `^3ROFF not found: id <%d>\n` |
| `src/codemp/qcommon/RoffSystem.cpp:689` | `Com_Printf` | `^1Purge failed: Entity <%i> not found\n` |
| `src/codemp/qcommon/RoffSystem.cpp:751` | `Com_Printf` | `^1ROFF System Error:\n` |
| `src/codemp/qcommon/cm_patch.cpp:742` | `Com_DPrintf` | `WARNING: CM_SetBorderInward: mixed plane sides\n` |
| `src/codemp/qcommon/cm_patch.cpp:949` | `Com_DPrintf` | `WARNING: CM_AddFacetBevels... invalid bevel\n` |
| `src/codemp/qcommon/cmd.cpp:153` | `Com_DPrintf` | `^3EXEC_NOW %s\n` |
| `src/codemp/qcommon/cmd.cpp:158` | `Com_DPrintf` | `^3EXEC_NOW %s\n` |
| `src/codemp/qcommon/common.cpp:1373` | `Com_Printf` | `^3Com_WriteConfig_f: The filename \"%s\" is reserved! Please choose an` |
| `src/codemp/qcommon/cvar.cpp:406` | `Com_DPrintf` | `^3Warning: cvar \"%s\" given initial values: \"%s\" and \"%s\"\n` |
| `src/codemp/qcommon/cvar.cpp:1497` | `Com_DPrintf` | `^3WARNING: Unsetting CVAR_ROM cvar '%s', since it is also CVAR_ARCHIVE` |
| `src/codemp/qcommon/q_shared.cpp:933` | `Com_Printf` | `^3Can't use keys or values with a '%c': %s = %s\n` |
| `src/codemp/qcommon/q_shared.cpp:972` | `Com_Printf` | `^3Can't use keys or values with a '%c': %s = %s\n` |
| `src/codemp/qcommon/stringed_ingame.cpp:847` | `Com_DPrintf` | `^3SE_Load(): Couldn't load \"%s\"!\n` |
| `src/codemp/qcommon/z_memman_pc.cpp:253` | `Com_Printf` | `^1Z_Malloc(): Failed to alloc %d bytes (TAG_%s) !!!!!\n` |
| `src/codemp/rd-dedicated/tr_ghoul2.cpp:3000` | `Com_Printf` | `^3R_LoadMDXM: %s has wrong version (%i should be %i)\n` |
| `src/codemp/rd-dedicated/tr_ghoul2.cpp:3037` | `Com_Printf` | `^3R_LoadMDXM: missing animation file %s for mesh %s\n` |
| `src/codemp/rd-dedicated/tr_ghoul2.cpp:3294` | `Com_Printf` | `^3R_LoadMDXA: %s has wrong version (%i should be %i)\n` |
| `src/codemp/rd-dedicated/tr_ghoul2.cpp:3435` | `Com_Printf` | `^3R_LoadMDXA: %s has no frames\n` |
| `src/codemp/rd-dedicated/tr_model.cpp:300` | `Com_DPrintf` | `^1RE_RegisterModels_LevelLoadEnd():\n` |
| `src/codemp/rd-dedicated/tr_model.cpp:327` | `Com_DPrintf` | `^1Dumping \"%s\"` |
| `src/codemp/rd-dedicated/tr_model.cpp:330` | `Com_DPrintf` | `^1, used on lvl %d\n` |
| `src/codemp/rd-dedicated/tr_model.cpp:348` | `Com_DPrintf` | `^1RE_RegisterModels_LevelLoadEnd(): Ok\n` |
| `src/codemp/rd-dedicated/tr_model.cpp:998` | `Com_DPrintf` | `^1Model name exceeds MAX_QPATH\n` |
| `src/codemp/rd-dedicated/tr_model.cpp:1061` | `Com_Printf` | `^3RE_RegisterModel: R_AllocModel() failed for '%s'\n` |
| `src/codemp/rd-dedicated/tr_model.cpp:1127` | `Com_Printf` | `^3RE_RegisterModel: unknown fileid for %s\n` |
| `src/codemp/rd-dedicated/tr_model.cpp:1182` | `Com_Printf` | `^3RE_RegisterModel: couldn't load %s\n` |
| `src/codemp/rd-dedicated/tr_model.cpp:1236` | `Com_Printf` | `^3R_LoadMD3: %s has wrong version (%i should be %i)\n` |
| `src/codemp/rd-dedicated/tr_model.cpp:1272` | `Com_Printf` | `^3R_LoadMD3: %s has no frames\n` |
| `src/codemp/rd-dedicated/tr_shader.cpp:1027` | `Com_Printf` | `^1Lightmap requested but none available for shader %s\n` |
| `src/codemp/rd-dedicated/tr_shader.cpp:2424` | `Com_Printf` | `^3Shader %s has a stage with no image\n` |
| `src/codemp/rd-dedicated/tr_shader.cpp:2615` | `Com_DPrintf` | `WARNING: shader '%s' has lightmap but no lightmap stage!\n` |
| `src/codemp/rd-dedicated/tr_shader.cpp:3108` | `Com_Printf` | `^3R_GetShaderByHandle: out of range hShader '%d'\n` |
| `src/codemp/rd-dedicated/tr_shader.cpp:3112` | `Com_Printf` | `^3R_GetShaderByHandle: out of range hShader '%d'\n` |
| `src/codemp/rd-vanilla/tr_arb.cpp:216` | `Com_Printf` | `^1Failed to compile gamma correction pixel shader. Error at character ` |
| `src/codemp/rd-vanilla/tr_backend.cpp:61` | `Com_Printf` | `^3GL_Bind: NULL image\n` |
| `src/codemp/rd-vanilla/tr_ghoul2.cpp:3820` | `Com_Printf` | `^3R_LoadMDXM: %s has wrong version (%i should be %i)\n` |
| `src/codemp/rd-vanilla/tr_ghoul2.cpp:3858` | `Com_Printf` | `^3R_LoadMDXM: missing animation file %s for mesh %s\n` |
| `src/codemp/rd-vanilla/tr_ghoul2.cpp:4186` | `Com_Printf` | `^3R_LoadMDXA: %s has wrong version (%i should be %i)\n` |
| `src/codemp/rd-vanilla/tr_ghoul2.cpp:4330` | `Com_Printf` | `^3R_LoadMDXA: %s has no frames\n` |
| `src/codemp/rd-vanilla/tr_image.cpp:784` | `Com_DPrintf` | `^1, levused %d` |
| `src/codemp/rd-vanilla/tr_image.cpp:791` | `Com_DPrintf` | `^1RE_RegisterMedia_GetLevel(): %d` |
| `src/codemp/rd-vanilla/tr_image.cpp:798` | `Com_DPrintf` | `^1RE_RegisterImages_LevelLoadEnd():\n` |
| `src/codemp/rd-vanilla/tr_image.cpp:815` | `Com_DPrintf` | `^1Dumping image \"%s\"\n` |
| `src/codemp/rd-vanilla/tr_image.cpp:838` | `Com_DPrintf` | `^1RE_RegisterImages_LevelLoadEnd(): Ok\n` |
| `src/codemp/rd-vanilla/tr_main.cpp:915` | `Com_DPrintf` | `^1WARNING: recursive mirror/portal found\n` |
| `src/codemp/rd-vanilla/tr_mesh.cpp:305` | `Com_DPrintf` | `^1R_AddMD3Surfaces: no such frame %d to %d for '%s'\n` |
| `src/codemp/rd-vanilla/tr_mesh.cpp:363` | `Com_DPrintf` | `^1WARNING: no shader for surface %s in skin %s\n` |
| `src/codemp/rd-vanilla/tr_mesh.cpp:366` | `Com_DPrintf` | `^1WARNING: shader %s in skin %s not found\n` |
| `src/codemp/rd-vanilla/tr_model.cpp:321` | `Com_DPrintf` | `^1RE_RegisterModels_LevelLoadEnd():\n` |
| `src/codemp/rd-vanilla/tr_model.cpp:347` | `Com_DPrintf` | `^1Dumping \"%s\"` |
| `src/codemp/rd-vanilla/tr_model.cpp:350` | `Com_DPrintf` | `^1, used on lvl %d\n` |
| `src/codemp/rd-vanilla/tr_model.cpp:368` | `Com_DPrintf` | `^1RE_RegisterModels_LevelLoadEnd(): Ok\n` |
| `src/codemp/rd-vanilla/tr_model.cpp:1153` | `Com_DPrintf` | `^1Model name exceeds MAX_QPATH\n` |
| `src/codemp/rd-vanilla/tr_model.cpp:1217` | `Com_Printf` | `^3RE_RegisterModel: R_AllocModel() failed for '%s'\n` |
| `src/codemp/rd-vanilla/tr_model.cpp:1286` | `Com_Printf` | `^3RE_RegisterModel: unknown fileid for %s\n` |
| `src/codemp/rd-vanilla/tr_model.cpp:1341` | `Com_Printf` | `^3RE_RegisterModel: couldn't load %s\n` |
| `src/codemp/rd-vanilla/tr_model.cpp:1403` | `Com_Printf` | `^3R_LoadMD3: %s has wrong version (%i should be %i)\n` |
| `src/codemp/rd-vanilla/tr_model.cpp:1439` | `Com_Printf` | `^3R_LoadMD3: %s has no frames\n` |
| `src/codemp/rd-vanilla/tr_scene.cpp:150` | `Com_DPrintf` | `^3WARNING: RE_AddPolyToScene: r_max_polys or r_max_polyverts reached\n` |
| `src/codemp/rd-vanilla/tr_shade_calc.cpp:398` | `Com_Printf` | `^3Autosprite shader %s had odd vertex count` |
| `src/codemp/rd-vanilla/tr_shade_calc.cpp:401` | `Com_Printf` | `^3Autosprite shader %s had odd index count` |
| `src/codemp/rd-vanilla/tr_shade_calc.cpp:470` | `Com_Printf` | `^3Autosprite2 shader %s had odd vertex count` |
| `src/codemp/rd-vanilla/tr_shade_calc.cpp:473` | `Com_Printf` | `^3Autosprite2 shader %s had odd index count` |
| `src/codemp/rd-vanilla/tr_shader.cpp:1048` | `Com_Printf` | `^1Lightmap requested but none available for shader %s\n` |
| `src/codemp/rd-vanilla/tr_shader.cpp:2677` | `Com_Printf` | `^3Shader %s has a stage with no image\n` |
| `src/codemp/rd-vanilla/tr_shader.cpp:2868` | `Com_DPrintf` | `WARNING: shader '%s' has lightmap but no lightmap stage!\n` |
| `src/codemp/rd-vanilla/tr_shader.cpp:3137` | `Com_DPrintf` | `^1Couldn't find image for shader %s\n` |
| `src/codemp/rd-vanilla/tr_shader.cpp:3446` | `Com_Printf` | `^3R_GetShaderByHandle: out of range hShader '%d'\n` |
| `src/codemp/rd-vanilla/tr_shader.cpp:3450` | `Com_Printf` | `^3R_GetShaderByHandle: out of range hShader '%d'\n` |
| `src/codemp/rd-vanilla/tr_shader.cpp:3677` | `Com_Printf` | `^3 (found \"%s\" on line %d)` |
| `src/codemp/rd-vanilla/tr_shader.cpp:3679` | `Com_Printf` | `^3.\n` |
| `src/codemp/server/NPCNav/navigator.cpp:1828` | `Com_Printf` | `^1NAV ERROR: envalid ent %d\n` |
| `src/codemp/server/NPCNav/navigator.cpp:1837` | `Com_Printf` | `^1NAV ERROR: tried to fail invalid waypoint %d\n` |
| `src/codemp/server/NPCNav/navigator.cpp:1846` | `Com_Printf` | `^1NAV ERROR: tried to fail invalid waypoint %d\n` |
| `src/codemp/server/NPCNav/navigator.cpp:1930` | `Com_Printf` | `^1NAV ERROR: too many blocked waypoint connections (%d)!!!\n` |
| `src/codemp/ui/ui_gameinfo.cpp:124` | `Com_Printf` | `^1file not found: %s\n` |
| `src/codemp/ui/ui_gameinfo.cpp:128` | `Com_Printf` | `^1file too large: %s is %i, max allowed is %i` |
| `src/codemp/ui/ui_gameinfo.cpp:243` | `Com_Printf` | `^1file not found: %s\n` |
| `src/codemp/ui/ui_gameinfo.cpp:247` | `Com_Printf` | `^1file too large: %s is %i, max allowed is %i` |
| `src/codemp/ui/ui_gameinfo.cpp:323` | `Com_Printf` | `^1Invalid bot number: %i\n` |
| `src/codemp/ui/ui_main.cpp:1014` | `Com_Printf` | `^1menu file not found: %s, using default\n` |
| `src/codemp/ui/ui_main.cpp:1018` | `Com_Printf` | `^1menu file too large: %s is %i, max allowed is %i\n` |
| `src/codemp/ui/ui_main.cpp:1094` | `Com_Printf` | `^3Bad 1st parameter for keyword 'cursor'\n` |
| `src/codemp/ui/ui_main.cpp:1362` | `Com_Printf` | `^3menu file not found: %s, using default\n` |
| `src/codemp/ui/ui_main.cpp:9174` | `Com_Printf` | `^1UI_BuildPlayerModel_List: Errors parsing '%s'\n` |
| `src/codemp/ui/ui_shared.cpp:268` | `Com_Printf` | `^1String pool has been exhausted.\n` |
| `src/codemp/ui/ui_shared.cpp:4084` | `Com_Printf` | `^3Menu_HandleKey[F12]: Unknown screenshot format assigned! This should` |
| `src/codemp/ui/ui_shared.cpp:7105` | `Com_Printf` | `^3Unknown item style value '%s'\n` |
| `src/codemp/ui/ui_shared.cpp:7120` | `Com_Printf` | `^3Unknown item style value\n` |
| `src/codemp/ui/ui_shared.cpp:7359` | `Com_Printf` | `^3Unknown text alignment value\n` |
| `src/codemp/ui/ui_shared.cpp:8456` | `Com_Printf` | `^3Unknown menu style value\n` |
| `src/codemp/ui/ui_shared.cpp:8563` | `Com_Printf` | `^3Unknown desc alignment value\n` |
| `src/shared/qcommon/files.cpp:3268` | `Com_Printf` | `^3WARNING (FS_ComparePaks): referenced pk3 files cut off due to too lo` |
| `src/shared/sdl/sdl_window.cpp:653` | `Com_Printf` | `...WARNING: fullscreen unavailable in this mode\n` |
| `src/shared/sdl/sdl_window.cpp:656` | `Com_Printf` | `...WARNING: could not set the given mode (%d)\n` |
| `src/shared/sdl/sdl_window.cpp:659` | `Com_Printf` | `...ERROR: no display modes could be found.\n` |

</details>


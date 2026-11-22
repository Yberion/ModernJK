# Step 22: the printing functions on {fmt}

Status: **in progress** (22 November 2026): the survey (sections 2 to 5), its decisions to take. Plan: [PLAN.md](../PLAN.md), step 22.

## 1. Decisions

| subject | decision |
|---|---|
| the library, 20 November 2026 | {fmt} (https://github.com/fmtlib/fmt), vendored in `libs/` at a pinned release, rather than the standard library's `std::format` and `std::print`: one implementation on every compiler |
| one header, 22 November 2026 | every printing and formatting call goes through the project's own functions, declared once in a header of `src/shared/qcommon` that every tree includes, which use {fmt} inside; the code never calls {fmt} itself; the conversion tool writes calls of those functions |
| the same printing everywhere, 22 November 2026 | the printing made the same wherever it is called from, where possible (section 2.2): the header gives the same functions to every binary, the crossing into the engine done inside it |
| the logs, 22 November 2026 | a step of their own, plan step 27 |

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

The questions this survey leaves, put all at once: the release and the build of {fmt}; the header's functions and names, the levels and the "println" variants; `va`; `Com_sprintf` and the 110 `sprintf`; the formats read at run time; the crossing into the engine; the class enums; the format errors at run time; netcodec's tools and the bot library's files; the format attributes; the parts; the measures (size, speed, build time).

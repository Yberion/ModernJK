# Step 30: the single player's random numbers as the multiplayer's

Status: ✅ **done** (25 November 2026): the inventory (section 2) and its four decisions, taken the same day (section 1); the code (section 3); the clang presets Release, Debug and AddressSanitizer without a warning, `ctest` 15 of 15 in each (the streams' tests added); two single-player sessions of one build with the same seed and `fixedtime` start the same sounds, line for line, in Jedi Academy and in Jedi Outcast, and a multiplayer demo played twice with the same seed too; `golden: SAME` with clang (section 4). A defect of the single player found on the way and fixed the same day: section 5. The VM validations' first round: `validate.sh` ALL OK, `validate.bat` failing the new test in MSVC's Release preset, a miscompilation of MSVC, worked around the same day (section 6); the second round ALL OK on both VMs without a warning; `golden: SAME` on the VPS (GCC 15, Linux, 5 min 3 s). The plan's step: [../PLAN.md](../PLAN.md) step 30.

## 1. Decisions (25 November 2026)

The aim, given at the start: the same system of random numbers in the three games, the one the multiplayer has.

| subject | decision |
|---|---|
| how far a fixed seed reaches | every stream of the three games (b, of two): when the seed is set, it seeds the engine's stream, the game's, the multiplayer's cgame and UI, and the renderers' weather, each stream given its own sequence from the one seed, the game's the seed itself as before; not set, every stream takes the clock's or the system's values as before. Rejected (a): the game's stream only, as the multiplayer had it, which left the engine's stream (the sound's ambient sets and music) and the multiplayer's cgame (the sounds' variants, the effects) different from one session to the next |
| the setting's name | `com_debugSeed`, registered by the engine and read by the modules (b, of two), in place of the multiplayer game module's `g_debugSeed` (a, kept, would have given an engine setting a game module's prefix); the bench scripts changed with it |
| the single player's game and cgame | one stream (a, of two): they are one module, and the multiplayer's rule is one stream per module. Rejected (b): a stream of the cgame's own, about 360 calls redirected (194 in Jedi Academy, 168 in Jedi Outcast) and the shared files sorted, for little: the single player's game has no fixed tick, so its outcomes depend on the frame rate already |
| the single player's connection port (`qport`) | from the system's random bytes, as the multiplayer's (b, of two), rather than from the clock (a, left as it was): it has no effect in a game on the loopback, the change makes the engines alike; `Com_RandomBytes` one definition for both engines |
| `fixedtime` before `devmap`, which turned the single player's cheats off (section 5) | the single player's `SV_Frame` writes the serverinfo and the systeminfo before it waits for its next frame, as the multiplayer's does (b, of three): a test of bits per frame. Rejected: (a) leaving it, the sessions setting `helpUsObi 1` before `devmap`; (c) `devmap` setting `helpUsObi` before the level is spawned, which would keep it changed when the level fails to load |
| MSVC's miscompiled seeding in a loop (section 6) | `Rand_Init` and `Rand_InitFixed` never inlined (a, of two), so that the seed crosses the call as a 32-bit argument: one call per level, every caller protected, the test unchanged. Rejected (b): the test's seeds from an array, which would have left the miscompilation possible for a later caller in a loop. Not done: a report to Microsoft with a file reproducing it |

## 2. The inventory (25 November 2026)

Every place where the three games seed a stream or draw outside one, before this step.

| stream | multiplayer | single player (Jedi Academy and Jedi Outcast) |
|---|---|---|
| game | `g_debugSeed` when it is not zero, else `Com_Milliseconds()` given by the server (`codemp/game/g_main.cpp`) | the engine's `com_frameTime`, given as the seed (`code/server/sv_game.cpp`), and `Rand_Init` in `InitGame` of each game; `Com_Milliseconds()` given in the place of the level's global time (section 4) |
| cgame | its own stream, from the clock (`codemp/cgame/cg_main.cpp`) | the game's module, so the game's stream |
| UI | its own stream, from the clock (`codemp/ui/ui_main.cpp`) | in the engine, so the engine's stream |
| engine | the system's random bytes (`Sys_InitRand`), once at the start, before the command line is read | the same |
| renderer (the weather) | the clock (`rd-vanilla/tr_WorldEffects.cpp`) | the same |
| effects | in the client engine, so the engine's stream | in the cgame, so the game's stream |
| the movement's draws | seeded by the command's tick (`bg_pmove.cpp`), for the prediction | none to align: the single player's prediction returns before `Pmove` (`if (1) ... return` in `cg_predict.cpp`) |
| `qport` | the system's random bytes | the clock (`code/qcommon/common.cpp`), on the loopback only |

- What the sound draws: the module draws from the engine's stream only for the ambient sets' one-shots (their volume, their wait, their wave) and the music's random entry; the sounds' numbered variants (`saberhit%d`, the voices' `anger%d`) are drawn by the game and the cgame. The sound's clock is `cls.realtime`, which follows `fixedtime`.
- The multiplayer's server engine draws for the game too: the NPCs' navigation (`codemp/server/NPCNav/navigator.cpp`, the retries of a failed edge) draws from the engine's stream.
- Nothing else in the single player depends on the clock: the remaining `Milliseconds()` calls time the loading and the frame rate's display.
- The bench scripts set the seed in a cfg executed after the engine's start, not with `+set`, so the engine reads the seed at each level's start, not once at its own start.

## 3. What was written

| where | what |
|---|---|
| `src/shared/qcommon/q_rand.h`, `q_rand.cpp` | `randStream_t` (`RAND_GAME`, `RAND_ENGINE`, `RAND_CGAME`, `RAND_UI`, `RAND_RENDERER`) and `Rand_InitFixed( seed, stream )`: SplitMix64 seeding of the stream's number above the seed's 32 bits, so that `RAND_GAME`, number 0, gives `Rand_Init( seed )`'s sequence and the game of a scenario replays as before |
| both engines' `common.cpp`, `qcommon.h` | `com_debugSeed`, cheat-protected as `fixedtime`; the multiplayer game module's `g_debugSeed` removed |
| `src/shared/sys/sys_main.cpp`, `sys_public.h` | `Sys_InitRandForLevel`: the engine's stream from `com_debugSeed` when it is set, called at a level's start by the multiplayer server (`SV_InitGame`) and by every client (`CL_InitCGame`); `Com_RandomBytes` moved from the multiplayer's `common.cpp`, one definition for both engines |
| `code/server/sv_game.cpp`, `codemp/server/sv_gameapi.cpp` | the game's seed: `com_debugSeed` when it is set, else the clock's; in the single player, the seed and the global time given in their places (section 4) |
| `codemp/cgame/cg_main.cpp`, `codemp/ui/ui_main.cpp`, both `rd-vanilla/tr_WorldEffects.cpp` | each stream from `com_debugSeed` when it is set, else the clock's |
| `code/qcommon/common.cpp` | the single player's `qport` from `Com_RandomBytes` |
| `code/server/sv_main.cpp` | the serverinfo and the systeminfo written before `SV_Frame` waits for its next frame (section 5) |
| `test/qcommon/q_rand.cpp`, `test/CMakeLists.txt` | `modernjk.rand`: each stream's first four values for the seeds 7 and -1 against those `docs/modernization/tools/rng/randref.py` computes in Python from the published algorithms (SplitMix64, xoshiro128++), not from the C++; the game's stream the same as `Rand_Init`'s for seven seeds (0, 1, 7, -1, 123456789 and the two ends of `int`); for every seed from -50 to 50, the five streams' first values all different; a stream seeded again drawing the same values |
| `docs/modernization/tools/gamestats/gamestats_cfg.sh`, `tools/soundlevel/session.sh`, `tools/visibility/coverrun.sh` | `com_debugSeed` in place of `g_debugSeed` |

No cost at run time: each stream is seeded once per level, as before; no memory, no bytes on the network, no delay.

## 4. Checks (25 November 2026)

- The clang presets Release, Debug and AddressSanitizer without a warning; `ctest` 15 of 15 in each; clang-format clean; clang-tidy (the preset `windows-clang-tidy`, every target) reports nothing on the lines added, the findings of the files around them being older.
- The single player, by the sound log (`s_show 1`): sessions of the build (three saber NPCs spawned, the player switching weapons and swinging), the seed and `fixedtime 8` given on the command line, compared start by start (the sound, its entity, class, place, slot and loop; the time stamps and distances left out). The ends of the sounds follow the real time and are not compared, as in step 21.

| game, map | seed 7, two sessions | seed 8 (and 9) against seed 7 |
|---|---|---|
| Jedi Academy, `yavin1b` | 160 of 160 starts the same | 141 starts, different from the first |
| Jedi Outcast, `artus_mine` | 70 of 70 the same | 103 starts, different from the first |
| Jedi Academy, `t2_trip` (the ambient set `windy`, whose one-shots the engine's stream draws) | 194 of 194 the same, the one-shot `windy_d` at the same moment in both | other one-shots at other moments (`windy_c` and `windy_b`; `windy_b` with seed 9) |

- The order of the single player's arguments, found by the first check: the engine gave `ge->Init` the frame's start time (`com_frameTime`) in the place of the random seed and `Com_Milliseconds()` in the place of the level's global time, against the order of `InitGame`'s parameters (`levelTime, randomSeed, globalTime`); both are clock values, so the inversion was not visible, and `level.globalTime` is only written into the saved games. With the seed given in the global time's place, two sessions differed from their first sound (`anger2` against `anger3`); the arguments in their places, they agree.
- The multiplayer: a demo recorded once (two bots on `mp/duel1`, the client following the first, 2,999 frames), played twice in timedemo with `com_debugSeed 7`: 275 of 275 starts the same, the sounds' variants included; played without the seed, it differs at its third start (`stone_run2` against `stone_run3`), as step 21 found.
- The multiplayer's scenarios: `golden: SAME`, the 302 of `tools/gamestats/scenarios.sh` with the clang build on this machine (2 min 38 s): the game's stream is seeded as before, so the counts did not move.
- On the VPS (25 November 2026), the server and the game module of the second round's `linux-gcc-server` build: `golden: SAME`, 5 min 3 s.

## 5. Found on the way and fixed: `fixedtime` before `devmap` turned the single player's cheats off

- **What a tester saw.** Jedi Academy or Jedi Outcast started with `+set fixedtime 8`, then `devmap yavin1b`: every cheat refused ("s_show is cheat protected.", "Cheats are not enabled on this server." for `god`, `give`, `npc spawn`), and `fixedtime` back to 0, so the session ran on the real clock. Without `fixedtime`, the same `devmap` allowed the cheats. The multiplayer was not concerned: its `SV_Frame` writes the systeminfo at every call.
- **Why.** The single player's server and client share one set of cvars. `devmap` sets `helpUsObi` 1 after the level is spawned, and the server wrote it into its systeminfo only in a frame where a server frame runs. With `fixedtime 8`, the first frame advances 8 ms, less than a server frame (50 ms at `sv_fps` 20), so `SV_Frame` returned before it updated the systeminfo; the client received the systeminfo of the spawn, `helpUsObi` 0, wrote it into the shared cvar and reset every cheat-protected cvar (`Cvar_SetCheatState`), `fixedtime` and `com_debugSeed` among them. With the real clock, the first frame after the loading lasts long enough for a server frame, and the systeminfo was updated first.
- **The fix** (decided 25 November 2026, b of three, section 1): `SV_Frame` writes the serverinfo and the systeminfo before it waits for its next frame, as the multiplayer's does. Cost: the test of two bits of `cvar_modifiedFlags` per frame, as before; a configstring written only when a cvar of those kinds changed.
- **Reproduced, before and after** (`god`, then `s_show 1`, then the cvars printed; the cheats refused counted in the log):

| case | before | after |
|---|---|---|
| Jedi Academy, `+set fixedtime 8 +set com_debugSeed 7`, `devmap yavin1b` | cheats refused; `fixedtime` and `com_debugSeed` back to 0 | cheats allowed; `fixedtime` 8, `com_debugSeed` 7 |
| Jedi Outcast, the same, `devmap artus_mine` | cheats refused | cheats allowed; `fixedtime` 8, `com_debugSeed` 7 |
| Jedi Academy, the same, `map yavin1b` | cheats refused, the cvars back to 0 | the same, as wanted: `map` allows no cheat |
| Jedi Academy, the real clock, `devmap yavin1b` | cheats allowed | the same |
| Jedi Academy, the real clock, `map yavin1b` | cheats refused | the same |

- With the fix, the seeded sessions of section 4 need no `helpUsObi 1` before `devmap`: run again without it, Jedi Academy 160 of 160 and Jedi Outcast 70 of 70 starts the same, no cheat refused.

## 6. The VM validations, and MSVC's miscompiled seeding

- First round (25 November 2026): `validate.sh` ALL OK without a warning (GCC 15 and Clang 23, `ctest` 15 of 15 in its five presets); `validate.bat` without a warning but one step failed, `modernjk.rand` in MSVC's Release preset (Debug and AddressSanitizer passing).
- **What failed.** `TestStreamsDiffer` seeds the five streams with every seed from -50 to 50, the seed the loop's counter: for the 50 negative seeds, the five streams drew the same first value (500 checks failed, 50 seeds times 10 pairs). The known values of seed -1 (a constant), the game's stream against `Rand_Init` and the clang and GCC builds passed. The binary of the Windows VM, run on this machine, fails the same way.
- **Why.** MSVC 19.52.37029 at `/O2` inlined `Rand_InitFixed` into the loop and kept the counter in a 64-bit register, sign-extended (`movq $-0x32, %r13`), which it then used as the zero-extended `(uint32_t)seed` of `((uint64_t)stream << 32) | (uint32_t)seed` (`orq %r13, %rdx`, the upper half never cleared): for a negative seed, the upper 32 bits all set overwrote the stream's number. The C++ has no undefined behaviour; the same function, given a seed that is not a loop's counter, is compiled right (the seeds of `TestGameStreamIsRandInit` are moved with `movl`, which clears the upper half). No caller of the game seeds a stream in a loop over the seed (the seeds come from cvars and parameters), so nothing in game was affected; `Rand_Init` has the same conversion.
- **The fix** (decided 25 November 2026, a of two, section 1): `Rand_Init` and `Rand_InitFixed` are never inlined (`__declspec(noinline)` with MSVC, `[[gnu::noinline]]` elsewhere, in `q_rand.cpp`), so the seed crosses the call as a 32-bit argument and is converted in the function. The values are unchanged; a call per level.
- Second round (25 November 2026), with the fix: ALL OK on both VMs without a warning (`validate.sh`: GCC 15 and Clang 23; `validate.bat`: MSVC), `ctest` 15 of 15 in every preset, `modernjk.rand` passing in MSVC's Release preset.

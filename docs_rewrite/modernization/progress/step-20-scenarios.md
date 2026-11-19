# Step 20: the scripted scenarios widened

The 40 scripted bot fights of plan step 14 (10 saber, 6 force, 24 weapon cases) check that the game is unchanged after each change, on a part of it only. Plan step 20 widens them to every game type, force power, weapon, saber move, team and item, the vehicles and the NPCs, each with the statistics it needs ([../PLAN.md](../PLAN.md) step 20, with the decisions of 19 November 2026). The golden counts of the new scenarios are recorded at the commit the current golden comes from, with the scenarios ported to it, and compared with the build of the day: every difference is explained by a fix decided since, or it is a bug.

## 1. Parts

| part | group | state |
|---|---|---|
| 1 | the tools (the shared cfg, maps, game types and bots per case, several scenarios per server), the statistics, the old commit's worktree, the force powers | ✅ done (19 November 2026): Windows and Linux |
| 2 | the weapons missing (stun baton, melee, the old bryar, the emplaced gun, the turret) | ⏳ |
| 3 | the saber: thrown and blocked, the saber locks, the special moves | ⏳ |
| 4 | the game types | ⏳ |
| 5 | teams and spectators | ⏳ |
| 6 | the holdable items and the items picked up | ⏳ |
| 7 | the vehicles | ⏳ |
| 8 | the NPCs | ⏳ |

## 2. The old commit

The current golden counts (`scenarios40_golden.txt` until this step) were recorded at step 14 part 4b (13 November 2026, one user command per tick), not on 12 November as the plan said: the commit before it (12 November 2026, `sv_fps` removed) only took the rate out of the cases' names, and part 4b changed two saber cases (`saber_fast_idle`, `saber_fast_attack`: the legs follow the velocity only from 1 unit per second). Since then `g_scenario` changed only by the types of plan steps 17 and 18.

That commit is checked out in a git worktree of its own, `E:\Git\jka\ModernJK_golden` (detached), and built there with the same llvm-mingw clang (`build-clang`: `openjkded.x86_64` and `jampgamex86_64`, 156 warnings of that time). Its game is still C (`g_scenario.c`, `qboolean`, unscoped enumerations), so the scenarios written on the build of the day are ported to it by hand; the port is kept as `tools/gamestats/scenarios_golden.patch` (`git apply` in a worktree of that commit), so that the golden can be made again. The server was named `openjkded.x86_64.exe` then: `gamestats_windows.sh` takes `DED_EXE=` besides `BUILD=`.

Check before any change (19 November 2026): the 40 cases with that build, `golden: SAME` in 81 s.

## 3. The tools

- `gamestats_cfg.sh` (new): the `measure.cfg` that `gamestats.sh` (Linux) and `gamestats_windows.sh` give the server, written once for both (`measure_cfg`), with `MAP=`, `GAMETYPE=` and `BOTS=` (`"<name>[:<team>] ..."`) from the environment; `SCENARIO=` takes several scenarios separated by `;`, played one after the other on the same server, each printing its own statistics, the waits computed from their repetitions (the `<minutes>` argument is no longer used for scenarios); the watchdog counts one "game statistics" line per scenario (`measure_stats`). The cfg no longer calls `gamestats` once more after the scenario, which gave every case of the old golden a seventh line, all zeros or what happened after the scenario.
- `measure_home`: an empty `modernjk_server.cfg` and `openjk_server.cfg` in each run's folder (section 4).
- `gamestats_scn.py`: every scenario of a run, named from its command in the run's `measure.cfg` (`saber 96 fast idle` is `saber_fast_idle`, as before); the new statistics lines kept.
- `scenarios.sh` (was `scenarios40.sh`), golden `scenarios_golden.txt` (was `scenarios40_golden.txt`): one server per line, `<name>|<map>|<g_gametype>|<bots>|<cvars before the map>|<scenarios>`; the 40 cases of step 14 one per server as before, the new ones grouped; the servers with the most scenarios start first.

## 4. Found: the test servers read the machine's settings

The dedicated server executes the archived settings of its configuration file, `modernjk_server.cfg`, found in `fs_homepath` or else in `fs_basepath`, the game's folder. The development machine's sets `g_forceRegenTime 0` (the force refilled at once: a team heal cost the attacker nothing), `g_friendlyFire 1`, `g_friendlySaber 1`, `g_spawnInvulnerability 3000` and others; the VPS has its own. So the counts depended on the machine; and the old commit's server read `openjk_server.cfg`, absent from the game's folder, so it ran with the defaults. The 40 cases happen not to depend on these settings (`golden: SAME` with both). Fixed (19 November 2026) by `measure_home`: the empty files in the run's own folder hide the game folder's, so that every run starts from the game's defaults and the settings the cfg sets.

## 5. The module (`codemp/game/g_scenario.cpp`)

- A third bot, the ally: in a team game the first bot is the attacker, the first bot that is not its teammate the defender (in free for all the second bot, as before), and a teammate the ally, placed 64 units on the attacker's right (its place checked with the attacker's).
- `scenario force <reps> <power> [level] [distance] [defender's level]`: the level 1 to 3 (3 by default, as the 40); the 16 powers (`heal levitation speed push pull telepathy teamheal teamforce see saberthrow` added); the side the power needs (the neutral powers on the dark side, as before); the instant powers used three times per repetition, the button released in between; levitation a jump held 1 s to the right, speed switched on and a run to the right; saber throw the alternate attack held 1 s. Push and pull: the defender's level in the same power is set (0 by default), since the bots' own level (3 for the Reborn) resisted every push; against saber throw the defender holds no saber (as against the weapons), so that the thrown saber hits. Before heal the attacker has 40 health, before team heal the ally 40, before team energize the ally 20 force points.
- A `scenario statistics` line with the gamestats of a scenario, per role, sampled at the 20 Hz instants of the counted repetitions: health gained and lost, force gained and spent, the instants off the ground, knocked down and in the attacker's mind trick, the distance moved, and the place at each repetition's end against its start, rounded to the unit and summed.
- `gamestats` prints the other ten powers (starts and milliseconds held) on a line of their own, `force statistics of the other powers`, so that the line of the six of step 14 is unchanged.
- `scenario_trace` prints the force points (`fp`) and the ally (`T`).

## 6. Part 1: the force powers

16 servers, 44 scenarios of 48 repetitions: the six powers of step 14 at levels 1 and 2 (in free for all), heal, levitation, speed, push, pull, telepathy, sight and saber throw at levels 1 to 3, push and pull once more at level 3 against a defender at level 3, team heal and team energize at levels 1 to 3 in a team game (`g_gametype 6`) with the ally. With the 40 of step 14: 56 servers, about 66 s on the development machine (30 at a time).

What the counts show:

| power | what changes with the level |
|---|---|
| lightning, grip, drain | the damage or the force drained, the force spent |
| rage, protect, absorb | the force spent, rage's health cost |
| heal | the health given (5, 10 or 25 per use) |
| levitation | the time off the ground, the distance, the force spent |
| push | the distance the defender is sent; at level 3 knocked down (1,440 instants) and hurt by its fall; at level 3 against level 3 resisted (the defender moved 5 units) |
| pull | the distance; at level 3 knocked down; resisted at level 3 against 3 |
| saber throw | the damage (at level 2, 11 deaths) |
| team heal, team energize | the force the attacker spends (50, 33, 25); the ally gets 50 health or the force points |
| speed, telepathy, sight | nothing: their durations outlast the 2 s of a repetition, so the time held is the same at every level; the force spent and the mind-tricked instants are counted |

## 7. The golden

Recorded on 19 November 2026 at the old commit with `scenarios_golden.patch`: 84 scenarios, 672 lines. The build of the day gives the same 672 lines (no difference to explain for this part), and two runs of it the same file; the 40 cases of step 14 give the same counts as the golden of before this step, without its seventh line.

On Linux (19 November 2026): the game module built by GCC 15 on the Linux VM (no warning; MSVC's build of the day none either), the widened set on the VPS (4 servers at a time): `golden: SAME`.

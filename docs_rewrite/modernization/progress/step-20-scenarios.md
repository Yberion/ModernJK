# Step 20: the scripted scenarios widened

The 40 scripted bot fights of plan step 14 (10 saber, 6 force, 24 weapon cases) check that the game is unchanged after each change, on a part of it only. Plan step 20 widens them to every game type, force power, weapon, saber move, team and item, the vehicles and the NPCs, each with the statistics it needs ([../PLAN.md](../PLAN.md) step 20, with the decisions of 19 November 2026). The golden counts of the new scenarios are recorded at the commit the current golden comes from, with the scenarios ported to it, and compared with the build of the day: every difference is explained by a fix decided since, or it is a bug.

## 1. Parts

| part | group | state |
|---|---|---|
| 1 | the tools (the shared cfg, maps, game types and bots per case, several scenarios per server), the statistics, the old commit's worktree, the force powers | ✅ done (19 November 2026): Windows and Linux |
| 2 | the weapons missing (stun baton, melee, the old bryar, the emplaced gun, the turrets) and the charges at several lengths | ✅ done (19 November 2026): Windows and Linux |
| 3 | the saber: thrown and blocked, the saber locks, the special moves | ⏳ |
| 4 | the game types | ⏳ |
| 5 | teams and spectators | ⏳ |
| 6 | the holdable items and the items picked up | ⏳ |
| 7 | the vehicles | ⏳ |
| 8 | the NPCs | ⏳ |

## 2. The old commit

The current golden counts (`scenarios40_golden.txt` until this step) were recorded at step 14 part 4b (13 November 2026, one user command per tick), not on 12 November as the plan said: the commit before it (12 November 2026, `sv_fps` removed) only took the rate out of the cases' names, and part 4b changed two saber cases (`saber_fast_idle`, `saber_fast_attack`: the legs follow the velocity only from 1 unit per second). Since then `g_scenario` changed only by the types of plan steps 17 and 18.

That commit is checked out in a git worktree of its own, `E:\Git\jka\ModernJK_golden`, on the branch `scenarios-golden` (created at that commit on 19 November 2026; each part's port is committed on it), and built there with the same llvm-mingw clang (`build-clang`: `openjkded.x86_64` and `jampgamex86_64`, 156 warnings of that time). Its game is still C (`g_scenario.c`, `qboolean`, unscoped enumerations), so the scenarios written on the build of the day are ported to it by hand; the port is kept as `tools/gamestats/scenarios_golden.patch` (`git apply` in a worktree of that commit), so that the golden can be made again. The server was named `openjkded.x86_64.exe` then: `gamestats_windows.sh` takes `DED_EXE=` besides `BUILD=`.

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

## 8. Part 2: the weapons

12 servers, 45 scenarios of 48 repetitions (with the 40 of step 14 and part 1: 68 servers, 129 scenarios, about 1 min 40 s on the development machine). Two cases of step 14 changed meaning, since they tested nothing: `weapon_6_1`, the disruptor's alternate, only zoomed in and never fired (it is now the sniper shot: zoomed in for 300 ms, the zoom locked on release, then the shot charged for `hold=` ms), and `weapon_14_1` set off det packs that were never thrown (a pack is now thrown, then set off 800 ms later). The other cases of step 14 and part 1 give the same counts as before.

The module (section 5) gains:

- the options `hold=<ms>` (how long the fire button is held, 1000 by default as in step 14) and `rep=<ms>` (a repetition, 2000 by default); the cfg reads `rep=` to wait for a scenario's end, and a positional argument that is an option counts as missing;
- the fire mode 2, both buttons;
- the emplaced gun (17): spawned once 52 units in front of the attacker (clear of its box), mounted with the use key at every repetition (looking 20 degrees down, since the use key's trace leaves the eyes level with the top of the gun's box), fired 2 degrees up, taken off it at every reset;
- `scenario turret <reps> <g2|hoth>`: a `misc_turretG2` (upside down, on the floor) or a `misc_turret` (Hoth's, with its top) spawned 48 units in front of the attacker, which is made untargeted (`FL_NOTARGET`), so that the turret fires at the defender;
- at every reset the disruptor's zoom, the rocket launcher's lock and a charge cleared; what a scenario spawned is freed at its end;
- the entities are spawned by their spawn functions with no keys (`level.numSpawnVars` cleared first: the spawn functions read the keys of the last entity the map loaded otherwise).

| weapon | cases | what the counts show |
|---|---|---|
| stun baton (1) | primary, alternate, at 40 units | the same hits (the alternate is the primary) |
| melee (2) | primary, alternate at 40 units; with `g_debugMelee 1` the alternate in the eight directions and both buttons (`rep=6000`) | the alternate punches as the primary (moving in the eight directions in turn); with `g_debugMelee` it kicks in the direction moved (a kick needs a direction: standing still nothing happens) and knocks down, and both buttons grab: a punch moving forward, a throw moving back (4 s long, so a repetition of 6 s) |
| old bryar (16) | primary, alternate held 100 to 1,400 ms | the charge's damage |
| bryar (4) alternate, bowcaster (7) primary | held 100 to 1,400 ms | the charge; below |
| DEMP2 (9) alternate | held 300 to 2,800 ms | the charge, up to 3 units |
| disruptor (6) sniper | held 100 to 2,500 ms | the damage up to full charge (1.7 s), the kills |
| rocket launcher (11) alternate | held 300 to 1,400 ms | the lock: from about 0.9 s held every rocket kills |
| thermal detonator (12) | primary and alternate held 100, 450, 1,400 ms | the throw's speed: the splash only, then direct hits |
| emplaced gun (17) | primary, alternate | the alternate fires as the primary; a third of the bolts hit (below) |
| turrets | `misc_turretG2`, `misc_turret` | the G2 turret's hits, the Hoth turret's kills |

Found on the way (19 November 2026), none changed:

- The weapons' charge is measured against the logic clock, not the command's time. The charge starts at the command that presses the button (`weaponChargeTime`, the command's time) and is counted when the shot is fired against `level.time`, the last 20 Hz instant, which in this branch is up to 50 ms before the command that releases the button (exactly 50 ms in the scenarios, since a bot's command for a 20 Hz instant runs on the tick before it). In game: the bryar's alternate held 400 ms fires a shot of 1 unit (19 damage on the body in the scenario) instead of 2 (44); held exactly 1 s, 4 units instead of 5. The same count for the old bryar, the bowcaster, the DEMP2, the disruptor's sniper shot, the thermal detonator's throw and the rocket launcher's lock. Measured against the releasing command's time, 400 ms would give 2 units. In OpenJK at `sv_fps` 20 the charge ends at the time of the server frame that fires, so it is the time held plus the gap between the server's clock and the time of the command that releases the button: none for a bot, whose commands carry the server's time (the time held, rounded to the 50 ms frames), but for a player it varies with the ping and the packets' timing (read from the code, not measured). Decided on 19 November 2026 (confirmed the same night after that comparison): counted to the time of the command that releases the button, exactly the time held for every player whatever the ping; to do at the start of the next session, before part 3.
- The emplaced gun's two barrels fire 10 units left and right of the view, alternately: aimed at the middle of a defender 204 units away, every bolt passes beside its model (the trace of a missile against a player tests the model, not only its box); 2 degrees up, 15 of 41 hit, 4 degrees up 10, level, 6 up and 2, 3 or 5 down none. Where each weapon's shots start against the crosshair is to be reviewed (asked on 19 November 2026, its place not set).

On Linux (19 November 2026): the game module built by GCC 15 on the Linux VM and by MSVC, no warning; the 129 scenarios on the VPS: `golden: SAME`.

# Step 20: the scripted scenarios widened

The 40 scripted bot fights of plan step 14 (10 saber, 6 force, 24 weapon cases) check that the game is unchanged after each change, on a part of it only. Plan step 20 widens them to every game type, force power, weapon, saber move, team and item, the vehicles and the NPCs, each with the statistics it needs ([../PLAN.md](../PLAN.md) step 20, with the decisions of 19 November 2026). The golden counts of the new scenarios are recorded at the commit the current golden comes from, with the scenarios ported to it, and compared with the build of the day: every difference is explained by a fix decided since, or it is a bug.

## 1. Parts

| part | group | state |
|---|---|---|
| 1 | the tools (the shared cfg, maps, game types and bots per case, several scenarios per server), the statistics, the old commit's worktree, the force powers | ✅ done (19 November 2026): Windows and Linux |
| 2 | the weapons missing (stun baton, melee, the old bryar, the emplaced gun, the turrets) and the charges at several lengths | ✅ done (19 November 2026): Windows and Linux |
| 3 | the saber: thrown and blocked, the saber locks, the special moves | ✅ done (19 November 2026): Windows and Linux |
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
- `scenarios.sh` (was `scenarios40.sh`), golden `scenarios_golden.txt` (was `scenarios40_golden.txt`): one server per line, `<name>|<map>|<g_gametype>|<bots>|<cvars before the map>|<scenarios>`; the 40 cases of step 14 one per server as before, the new ones grouped; the servers with the most scenarios start first. Since the charge's fix (section 9) two golden files: `scenarios_golden.txt`, the build of the day's counts once their differences with the old commit are explained, and `scenarios_golden_step14.txt`, the old commit's; `GOLDEN=<file>` compares a run with another file than the first (`GOLDEN=scenarios_golden_step14.txt` for the old commit's build).

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

Since the charge's fix (19 November 2026, section 9), the build of the day differs from the old commit by design, so the golden is two files. `scenarios_golden_step14.txt` holds the old commit's counts, the reference of this step's method; `scenarios_golden.txt` holds the build of the day's, the check after every change of the code (`golden: SAME` means nothing moved since the last state whose differences were explained). Each part from part 3 on: its scenarios run at the old commit go into the first file, compared with the build of the day, every difference explained (the decided fixes, as the charge, or a bug), then the build of the day's counts go into the second.

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

Found on the way (19 November 2026), the first fixed the same night (section 9):

- The weapons' charge is measured against the logic clock, not the command's time. The charge starts at the command that presses the button (`weaponChargeTime`, the command's time) and is counted when the shot is fired against `level.time`, the last 20 Hz instant, which in this branch is up to 50 ms before the command that releases the button (exactly 50 ms in the scenarios, since a bot's command for a 20 Hz instant runs on the tick before it). In game: the bryar's alternate held 400 ms fires a shot of 1 unit (19 damage on the body in the scenario) instead of 2 (44); held exactly 1 s, 4 units instead of 5. The same count for the old bryar, the bowcaster, the DEMP2, the disruptor's sniper shot, the thermal detonator's throw and the rocket launcher's lock. Measured against the releasing command's time, 400 ms would give 2 units. In OpenJK at `sv_fps` 20 the charge ends at the time of the server frame that fires, so it is the time held plus the gap between the server's clock and the time of the command that releases the button: none for a bot, whose commands carry the server's time (the time held, rounded to the 50 ms frames), but for a player it varies with the ping and the packets' timing (read from the code, not measured). Decided on 19 November 2026 (confirmed the same night after that comparison): counted to the time of the command that releases the button, exactly the time held for every player whatever the ping; to do at the start of the next session, before part 3.
- The emplaced gun's two barrels fire 10 units left and right of the view, alternately: aimed at the middle of a defender 204 units away, every bolt passes beside its model (the trace of a missile against a player tests the model, not only its box); 2 degrees up, 15 of 41 hit, 4 degrees up 10, level, 6 up and 2, 3 or 5 down none. Where each weapon's shots start against the crosshair is to be reviewed (asked on 19 November 2026, its place not set).

On Linux (19 November 2026): the game module built by GCC 15 on the Linux VM and by MSVC, no warning; the 129 scenarios on the VPS: `golden: SAME`.

## 9. The weapons' charge counted to the releasing command

Decided on 19 November 2026 (section 8) and done the same night, before part 3 ([../CHANGES.md](../CHANGES.md) section 24). The charges and the rocket launcher's lock are counted to `ps.commandTime`, the time of the command that fires, in place of `level.time` (seven places of `g_weapon.cpp`: the bryars, the disruptor's sniper shot, the bowcaster, the DEMP2, the rocket's lock, the thermal detonator, the vehicles' homing missiles). Pmove sets `commandTime` to the command's time before `PM_Weapon`, and `ClientEvents` fires right after Pmove, for the players, the bots, the NPCs and the vehicles alike. A `misc_weapon_shooter` sends no command: its `commandTime` is set to `level.time` before it fires, so that its charge stays full (only single-player maps of the game have one). cgame counted to `cg.cmdTime` already.

The old count's gap, for a bot: its command for a time runs on the tick before it, and `level.time` is the last 20 Hz instant at that tick, so the gap was 50 ms when the release falls on a 20 Hz instant (every case below, held a multiple of 50 ms), 30 ms held 780 ms, 10 ms held 760. An NPC's commands are dated 50 ms before the instant, so its charges were 50 ms longer than held; they are exact now (no NPC scenario yet: part 8).

Against the old commit, 18 scenarios differ (44 of 1,040 lines), each by the charge counted 50 ms longer:

| weapon | cases that change | why | cases that do not |
|---|---|---|---|
| bryar and old bryar, alternate (a unit per 200 ms, 1 to 5) | held 400, 600, 800 and 1,000 ms (`weapon_4_1`, `weapon_16_1_256_hold1000`) | a unit more: 19 to 44, 44 to 66, 66 to 88, 88 to 110 damage per shot; held 1 s, every shot kills | 100 and 1,400 ms (the minimum, the maximum) |
| bowcaster, primary (1, 3 or 5 bolts, an even count taken one lower) | held 600 ms (1 bolt to 3), 1,000 ms (`weapon_7_0`, 3 to 5) | more bolts, each weaker (50, 40, 30): the middle one hits, the others miss (94 misses to 188) | 100, 400, 800, 1,400 ms |
| disruptor, sniper shot (a unit per 50 ms, counted twice) | held 100, 500 and 1,000 ms (`weapon_6_1`) | 2 damage more, 3 with the head's factor of 1.3 | 1,700 and 2,500 ms (full charge) |
| DEMP2, alternate (a unit per 700 ms) | held 1,400 ms (1 unit to 2), 2,100 ms (2 to 3) | a unit more | 300, 1,000, 2,800 ms |
| thermal detonator (full speed at 900 ms) | held 450 ms | 0.44 to 0.50 of full speed: the primary hits without bouncing first, the alternate hits directly instead of by its splash (2,064 to 2,352 damage) | 100, 1,000, 1,400 ms (the minimum 0.15, the maximum) |
| rocket launcher, alternate (homes from 10 of 16 units of 75 ms) | held 750 ms, a case added for it | locked: every rocket homes and kills (85 damage per hit and no kill before) | 300, 700, 900, 1,000, 1,400 ms (on the same side of the threshold either way) |

The rocket's case: 780 ms, tried first, locks with both counts (30 ms of gap, 750 ms counted); 750 ms is the length on either side of the threshold. Two runs of the build of the day give the same file; the old commit's gives its former 1,032 lines and the new case's 8.

On Linux (19 November 2026): the game module built by GCC 15 on the Linux VM and by MSVC; the 130 scenarios on the VPS: `golden: SAME` against the new `scenarios_golden.txt`.

## 10. Part 3: the saber

Decided on 19 November 2026, after reading the code of the special moves, the locks and the throw:

- not in the multiplayer game, so not scripted: the pull attack (`PM_CheckPullAttack`, under `#if 0` since Raven's "disabling these for MP"), and the dual sabers' kicks (only the staff kicks; with two sabers the alternate attack throws one);
- the saber locks happen only between players in a duel (`WP_SabersCheckLock`), so the scenario puts both bots in a private duel already started, on `mp/ffa3`, with `g_saberLocking 1`;
- the throws: the single saber and the dual sabers, against an armed defender; the staff's alternate attack is a kick, and it is thrown only with one blade off, which is not scripted;
- a new statistics line for every scenario, the special moves started, so that a scenario that does a plain attack instead shows it;
- 35 scenarios of 48 repetitions, by the modes below.

The module (section 5) gains `scenario saber <reps> <style> <mode> [distance]`, the modes after the two of step 14 (`idle`, `attack`) given in the table, each the attacker's script, the defender idle unless said. Every reset also ends a saber lock and its private duel. The jumps, rolls and the staff's back flip set the attacker's jump at level 3; the throw its saber throw at level 3.

The statistics: `gamestats` prints a line `saber special move statistics (moves started)`, each move from the back stab to the hilt bash (`LS_A_BACKSTAB` to `LS_HILT_BASH`) that a client started, by its name in `saberMoveData`, the spaces as underscores; the `scenario statistics` line counts the instants each role spent in a saber lock (after the instants mind tricked). Both are in every scenario's output, so the two goldens were recorded again; every scenario of before gives the same counts but for them.

| mode | the attacker's script | styles | the move, in each of the 48 repetitions | what the counts show |
|---|---|---|---|---|
| `kata` | attack and alternate attack together, standing | the five | `specialfast`, `specialmed`, `specialstr`, `dualspinprot`, `StfSoulCal` | the force spent (50); the fast and medium katas kill every time, the strong one only touches (1 damage a repetition) |
| `back` | facing away from the defender 56 units behind, backward and attack | the five | `Back_Stab` (fast, staff), `Back_Att` (medium, strong, dual) | the damage; the dual sabers' kill every time |
| `backcrouch` | the same, crouched | medium, strong | `CR_Back_Att` | the damage |
| `jump` | a jump, then forward and attack while rising | medium, strong, dual, staff | `Flip_Slash`, `Jump_Att`, `DualJump_Atk`, `DualJumpAtkRStaff` | the medium flip goes over the defender and lands behind it, without a hit; the others hit, the dual and staff ones kill |
| `lunge` | crouched, then forward and attack | fast, dual, staff | `Lunge_Att`, `DualSpinAtk`, `StfSpinAtk` | the damage; the lunge kills |
| `cartwheel` | a jump, then the side and attack while rising, right and left in turn | medium, dual, staff | the staff's `ButterflyRight` and `ButterflyLeft`, 24 each; none with the single and dual sabers | see below |
| `backflip` | backward, jump and attack | staff | `BkFlip_Atk` | the time off the ground, no hit |
| `kick` | alternate attack and a direction, forward, back, right, left in turn | staff | `StfKickFwd`, `StfKickBack`, `StfKickRight`, `StfKickLeft`, 12 each | no hit (below) |
| `airkick` | a force jump, then the same | staff | the four air kicks, 12 each | the force spent, the time off the ground, no hit |
| `rollstab` | a run, crouched into a roll, attack held to its end; 400 units apart, repetitions of 3 s | medium, dual, staff | `RollStab` | the damage |
| `stabdown` | the defender knocked down by the scenario, forward and attack | medium, dual, staff | `StabDown`, `StabDownDual`, `StabDownStf` | the damage |
| `throw` | the alternate attack held 1 s, 128 units apart (the room a throw needs in front), the saber throw allowed | fast, dual | no special move | `EV_SABER_BLOCK` (83 and 52) and the saber knocked out of the air and bouncing on the floor, `EV_GRENADE_BOUNCE` (32 each); the force spent |
| `lock` | both the same attack, top-down then each diagonal in turn, held 600 ms; during a lock the attacker presses attack every 100 ms | dual, staff | 16 locks each | each lock 6 instants (300 ms) for both roles, then the attacker's win; with the staff, its hits |

What was found on the way (19 November 2026), none changed:

- The single and dual sabers' cartwheel is played on the legs only (`PM_SetSaberMove`, "force only on legs" from Raven): the torso keeps the jump, so the saber move stays the ready one and no blade hits. Its counts are the force it costs (10 a cartwheel), the time off the ground and the move to the side. A jump begun to the side is a side flip instead (the force jump's flip), during which no attack starts, so the scenario jumps straight up first.
- The single saber's attacks never locked: with the same attack on both sides, their blades meet when one of them is no longer attacking, and a lock needs both. The dual sabers and the staff lock at their 16 clashes; a clash locks with a probability of 10 % (`g_saberLockFactor` 2), so the lock server sets it to 20, a lock at every clash that meets the game's other conditions.
- A clash locks only a player who is pressing a button or moving at that moment (`G_ClientIdleInWorld`), so the lock scenario holds the attack 600 ms.
- The pushes of a saber lock: the game reads the attack button once per 20 Hz frame, at the frame's first command and before that command sets the buttons (`g_active.cpp`), so a press counts only if the command before it made it, one tick in five at 100 Hz; a press begun on another tick and held is not counted at all. In OpenJK at `sv_fps` 20 the same rule counts a press begun on the last command before a server frame (read from the code, not measured). The scenario presses from the bot's command at the instant's time. To review with the pushes' design, not decided.
- The staff's kicks: their trace runs from the hip to the right foot, about 44 units ahead and 18 to the right; at 32, 40, 48 and 56 units the defender in front is never hit (24 units is too close: the two bots stand in each other). Not looked at further.

10 servers more, 165 scenarios in 78 servers, about 1 min 40 s on the development machine (30 servers at a time). At the old commit, with the port (`scenarios_golden.patch`): the 35 scenarios of this part give the same 315 lines as the build of the day; the two files differ only by the 44 lines of the charge (section 9). Two runs of the build of the day give the same file.

On Linux (19 November 2026): the game module built by GCC 15 on the Linux VM and by MSVC, no warning; the 165 scenarios on the VPS: `golden: SAME`.

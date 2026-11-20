# Step 20: the scripted scenarios widened

The 40 scripted bot fights of plan step 14 (10 saber, 6 force, 24 weapon cases) check that the game is unchanged after each change, on a part of it only. Plan step 20 widens them to every game type, force power, weapon, saber move, team and item, the vehicles and the NPCs, each with the statistics it needs ([../PLAN.md](../PLAN.md) step 20, with the decisions of 19 November 2026). The golden counts of the new scenarios are recorded at the commit the current golden comes from, with the scenarios ported to it, and compared with the build of the day: every difference is explained by a fix decided since, or it is a bug.

## 1. Parts

| part | group | state |
|---|---|---|
| 1 | the tools (the shared cfg, maps, game types and bots per case, several scenarios per server), the statistics, the old commit's worktree, the force powers | ✅ done (19 November 2026): Windows and Linux |
| 2 | the weapons missing (stun baton, melee, the old bryar, the emplaced gun, the turrets) and the charges at several lengths | ✅ done (19 November 2026): Windows and Linux |
| 3 | the saber: thrown and blocked, the saber locks, the special moves | ✅ done (19 November 2026): Windows and Linux |
| 4a | the game types without objectives: holocron, Jedi Master, duel, power duel, team | ✅ done (20 November 2026): Windows and Linux |
| 4b | the game types with objectives: CTF, CTY, siege | ✅ done (20 November 2026): Windows and Linux |
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
- Since part 4a (20 November 2026): a case's seventh field, optional, is the free play before its scenarios in ticks (`FREEPLAY=` of `gamestats_cfg.sh`, 30 s by default; 1 s for the power duel, section 11); outside free for all a scenario's name starts with its game type (`duel_saber_fast_idle`; the team powers of part 1, played in a team game, became `team_force_teamheal_1` and the like); the `score statistics` line is kept. Two servers play the same commands with other settings, as `weapon_2_1_40` with and without `g_debugMelee`: the team cases with the friendly fire off, then on, told apart by their order in the file.

## 4. Found: the test servers read the machine's settings

The dedicated server executes the archived settings of its configuration file, `modernjk_server.cfg`, found in `fs_homepath` or else in `fs_basepath`, the game's folder. The development machine's sets `g_forceRegenTime 0` (the force refilled at once: a team heal cost the attacker nothing), `g_friendlyFire 1`, `g_friendlySaber 1`, `g_spawnInvulnerability 3000` and others; the VPS has its own. So the counts depended on the machine; and the old commit's server read `openjk_server.cfg`, absent from the game's folder, so it ran with the defaults. The 40 cases happen not to depend on these settings (`golden: SAME` with both). Fixed (19 November 2026) by `measure_home`: the empty files in the run's own folder hide the game folder's, so that every run starts from the game's defaults and the settings the cfg sets.

## 5. The module (`codemp/game/g_scenario.cpp`)

- A third bot, the ally: in a team game the first bot is the attacker, the first bot that is not its teammate the defender (in free for all the second bot, as before), and a teammate the ally, placed 64 units on the attacker's right (its place checked with the attacker's).
- `scenario force <reps> <power> [level] [distance] [defender's level]`: the level 1 to 3 (3 by default, as the 40); the 16 powers (`heal levitation speed push pull telepathy teamheal teamforce see saberthrow` added); the side the power needs (the neutral powers on the dark side, as before); the instant powers used three times per repetition, the button released in between; levitation a jump held 1 s to the right, speed switched on and a run to the right; saber throw the alternate attack held 1 s. Push and pull: the defender's level in the same power is set (0 by default), since the bots' own level (3 for the Reborn) resisted every push; against saber throw the defender holds no saber (as against the weapons), so that the thrown saber hits. Before heal the attacker has 40 health, before team heal the ally 40, before team energize the ally 20 force points.
- A `scenario statistics` line with the gamestats of a scenario, per role, sampled at the 20 Hz instants of the counted repetitions: health gained and lost, force gained and spent, the instants off the ground, knocked down and in the attacker's mind trick, the distance moved, and the place at each repetition's end against its start, rounded to the unit and summed.
- `gamestats` prints the other ten powers (starts and milliseconds held) on a line of their own, `force statistics of the other powers`, so that the line of the six of step 14 is unchanged.
- `scenario_trace` prints the force points (`fp`) and the ally (`T`).
- Since part 4a (20 November 2026, section 11):
  - the bots are the first ones that are not spectators (a duel's queue); in a power duel the defender is the lone duelist (`OnSameTeam` compares the duel teams), reset to its health at spawn, 150 (`g_powerDuelStartHealth`), the others to 100 as before;
  - the option `pickup=<units>`: in a holocron game a `misc_holocron` of the force scenario's power, in a Jedi Master game the map's Jedi Master saber, put that far in front of the attacker, which walks forward 300 ms onto it, its script starting 400 ms later; every reset puts it back in place, clears the holocrons carried and drops the Jedi Master's saber as at its death without a killer (`ThrowSaberToAttacker`); in a Jedi Master game the bots hold the melee and the bryar, as they spawn, until the attacker takes the saber;
  - the option `target=ally`: the attacker's teammate takes the defender's place and role (the statistics' defender is then the teammate), the defender the ally's;
  - the lock mode starts no private duel in the duel game type, where every clash may lock;
  - a line `score statistics` in every scenario: each role's score and captures, and the red and blue team scores, gained over the counted repetitions; the goldens were recorded again, every scenario of before giving the same counts but for it.
- Since part 4b (20 November 2026, section 12):
  - the place: on a map without `info_player_deathmatch`, among the CTF spawn points (`team_CTF_redspawn`, then `bluespawn`), then the siege's (`info_player_siegeteam1`, then `2`); with `pickup=` in CTF and CTY, the place behind the attacker is checked too (its flag goes there);
  - `pickup=<units>` in CTF and CTY: the enemy flag that far in front of the attacker, its own flag as far behind it, on the floor; at every reset the flags go back to their bases (`Team_ResetFlags`: a dropped one freed, a carried one cleared by the reset of the powerups), then the bases are moved there; the times the flag bonuses look back to (`lasthurtcarrier`, `lastreturnedflag`, `lastfraggedcarrier`, `flagsince`) are cleared, since a bonus looks 8 or 10 s back, longer than a repetition;
  - `scenario flag <reps> <capture|return> [distance]` (CTF and CTY, with `pickup=`): capture, the attacker walks onto the enemy flag (as every `pickup=`), then back 800 ms onto its own; return, the defender holds the rocket launcher and fires at the carrier, aiming the launcher's muzzle (12 units in front of the eyes, 8 to the right, 4 below) at its chest, then turns to the flag dropped and runs onto it once it has landed (700 ms);
  - `scenario objective <reps> <number> [distance]` (siege): the attacker uses its team's `info_siege_objective` of that number at the start of every repetition (`siegeTriggerUse`, as a player completing it), and every reset sets it not complete again (`G_SiegeSetObjectiveComplete`);
  - the option `away`: the defender turned away from the attacker;
  - in siege, a dead bot, or one waiting for the next wave as a spectator with 1 point of health (`ClientRespawn` with `g_siegeRespawn`), respawns at once (`ClientSpawn`, its corpse kept as `ClientRespawn` does);
  - push and pull: the defender knows the power when its level is above 0 (a siege class may not give it, and a power not known does not resist); the scenarios of before give the same counts.

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

## 11. Part 4a: the game types without objectives

Decided on 20 November 2026, after reading the code of the game types and trying the scenarios of before in each ([../PLAN.md](../PLAN.md) step 20): the game types in two parts, 4a (holocron, Jedi Master, duel, power duel, team) and 4b (CTF, CTY, siege, whose objectives and flags need more of the module); 33 scenarios of 48 repetitions on 9 servers, each testing what its game type changes.

What the game types change, read from the code:

- Holocron: every frame `HolocronUpdate` sets every force power to level 0 but those of the holocrons carried (level 3), the saber attack and the jump to level 1, the saber defence to 0; the scenario's levels are overwritten. No map of the game has a holocron (`misc_holocron`).
- Jedi Master: the Jedi Master's saber is put at a spawn point drawn at random when the map loads; who touches it gets 200 health, every power at level 3 (but team heal, team energize, drain and absorb; telepathy at 2) and regenerates the force four times faster; the others have the jump at level 1, the melee and the bryar, and cannot hurt each other. `JediMasterUpdate` sets the levels every frame.
- Duel and power duel: the saber's damage is not doubled as in the other game types (`w_saber.cpp`, also in siege), a clash may lock without a private duel, the force jump drains 1 instead of 5, the health and armour items are removed. In a power duel the lone duelist parries as if its block level were doubled, regenerates the force faster, spawns with 150 health and never locks; a dead player becomes a spectator (`ClientRespawn`), and the lone duelist's death ends the round.
- Team: the friendly fire (`g_friendlyFire`, `g_friendlySaber`), the team score from the kills (`AddScore`).

| server (map, game type) | scenarios | what the counts show |
|---|---|---|
| `holocron` (`mp/ffa3`, 1) | lightning, push and heal with `pickup=32`; the medium style's attack (`idle`) | the holocron picked up at every repetition (`EV_ITEM_PICKUP` 48) and its power at level 3: lightning 1,430 damage, push knocks the defender down (1,056 instants) and hurts it by its fall (3,696), heal gives 2,400 health; the saber at the levels the holocrons leave (attack 1, defence 0): 600 damage |
| `jedimaster` (`mp/ffa3`, 2) | the medium style's attack 96 units away with `pickup=32`, lightning with `pickup=32` | the attacker becomes the Jedi Master at every repetition (`EV_BECOME_JEDIMASTER` 48, 100 health gained each time, 2 lost since health above the maximum decays by 1 a second); its saber kills 12 times (score 12), its lightning takes 1,437 |
| `duel_saber_idle`, `duel_saber_attack` (`mp/duel1`, 3) | the ten saber cases of step 14 (the five styles, `idle` and `attack`) | the saber's damage without the doubling (fast 204, medium 162, strong 750, dual 162, staff 246 in `idle`); another map than the step 14 cases', so not compared with them |
| `duel_lock` (`mp/duel1`, 3) | the dual sabers' and the staff's locks, `g_saberLocking 1`, `g_saberLockFactor 20`, no private duel | no lock (below) |
| `duel_levitation` (`mp/duel1`, 3) | the force jump at levels 1 to 3 | the force spent (1,824, 1,248, 1,104), the time off the ground |
| `powerduel` (`mp/duel1`, 4) | the five styles' attack (`idle`) against the lone duelist, a free play of 1 s | the lone duelist at 150 health; it parries the medium style better than a duelist (126 damage against 162 in a duel), the other styles the same; no death |
| `team_friendlyfire0`, `team_friendlyfire1` (`mp/ffa3`, 6) | the medium style's attack, the thermal detonator and lightning against the attacker's teammate (`target=ally`), the friendly fire off then on (`g_friendlyFire 1`, `g_friendlySaber 1`); the dual sabers' back attack against the enemy | off: the teammate loses no health (the saber's counters count its wounds before the friendly fire's test: 24 wounds, 570 damage); on: 134, 4,368 and 1,437; the back attack kills at every repetition, 48 points to the attacker and the red team |

Found on the way (20 November 2026), none changed:

- The duel game type's saber locks: the duel's halved saber damage makes a clash's damage 1, the damage of an idle touch (`SABER_NONATTACK_DAMAGE`), and the game tries a lock only above it (`w_saber.cpp`, the saber against saber collision). The same clash on `mp/duel1` at the same instant (a temporary trace of the damage): 2 in free for all with a private duel, which locks 8 times in 24 repetitions; 1 in the duel game type, no lock in 48 repetitions with the dual sabers or the staff. In game: in a duel, the same attacks that lock in a private duel of free for all clash and part. Its place not set.
- A power duel whose lone duelist dies in the free play ends its round ("Exit: Powerduel ended.") and stays in its intermission: the scenario never ends. Not looked at further; the power duel's free play is 1 s (the case's seventh field), shorter than the bots take to kill.

At the old commit, with the port (`scenarios_golden.patch`): the 33 scenarios give the same counts as the build of the day; the two files differ only by the 18 scenarios of the charge (section 9), 47 lines: the 44 of before and the score line of three of them (`weapon_4_1`, `weapon_16_1_256_hold1000`, `weapon_11_1_256_hold750_rep3000`), whose every shot kills with the charge counted to the releasing command (48 points). The 165 scenarios of before give the same counts as the goldens of before but for the new line and the team powers' new names. Two runs of the build of the day give the same file. 198 scenarios on 87 servers, about 2 min on the development machine (30 servers at a time).

On Linux (20 November 2026): the game module built by GCC 15 on the Linux VM and by MSVC, no warning; the 198 scenarios on the VPS: `golden: SAME`.

## 12. Part 4b: the game types with objectives

Decided on 20 November 2026 ([../PLAN.md](../PLAN.md) step 20, the decisions of part 4 and the nine of part 4b), after reading the flags' and the siege's code: 17 scenarios of 48 repetitions on 7 servers more, and one on a server of before.

What the game types change, read from the code:

- CTF and CTY: taking the enemy flag (`Team_TouchEnemyFlag`, 10 points), the capture when its carrier touches its own flag at its base (`Team_TouchOurFlag`: 100 points, a capture, a point for the team, 25 points to each teammate, the flags back to their bases), the flag dropped where its carrier dies (`TossClientItems`, thrown 45 degrees to its left), returned by a player of its team who touches it (10 points), the carrier's killer 20 points more (`Team_FragBonuses`). In CTY a carrier has the ysalamiri's effect (`BG_HasYsalamiri`): no force power, and none used on it. `capturelimit` (8 by default) ends the game: the servers set it to 0. An item is touched when the player's origin is within 44 to 50 units of it along x and 36 along y and z (`BG_PlayerTouchesItem`), not when their boxes meet.
- Siege: nothing is damaged before the round begins (5 s after a player in each team); the dead wait for the next wave as spectators with 1 point of health (20 s at least); the saber's attack level tripled for the attacker and its damage not doubled (`w_saber.cpp`); the disruptor's shot 50 instead of 30, its sniper shot's charge capped at 200 instead of 60 (full after 5 s instead of 1.5 s); the repeater's alternate with a smaller splash; the rocket's lock at 150 ms a unit instead of 75 (homing from 10 units: 1.5 s held instead of 0.75 s); a player turned more than 60 degrees away from the one pulling it cannot counter the pull (`CanCounterThrow`); with a saber, the arm of the one pulling is free 200 ms after a pull instead of 400; a player not holding a saber takes four times the saber's damage (100 at most), one holding it three quarters of the explosions'; the damage to oneself 1.5 times instead of half. The classes' flags `CFL_MORESABERDMG` and `CFL_STRONGAGAINSTPHYSICAL` are in the code but no class of the game has them. The bots take their team's first class (no bot chooses one): on `siege_hoth`, the Imperial Snowtrooper and the Rebel Infantry, which give no force power and impose no saber style, with 100 health.
- The objective: `siege_hoth`'s objective 2 of the imperials (the bridge, `final -1`) neither counts for the round nor fires a target, so completing it at every repetition changes nothing else; it gives 20 points to the player who completes it (`BroadcastObjectiveCompletion`) and an `EV_SIEGE_OBJECTIVECOMPLETE`. No team score: in siege the game adds none (`AddScore` adds the team's only in a team game).

| server (map, game type) | scenarios | what the counts show |
|---|---|---|
| `ctf_capture`, `cty_capture` (`mp/ctf3`, 8 and 9; attacker, defender, ally) | a capture with the flags 64 units away (`flag 48 capture 128 pickup=64`); lightning while carrying the flag (`force 48 lightning 3 128 pickup=64`) | 48 captures: the attacker 5,280 points (10 and 100 each time), the ally 1,200 (25), the red team 48; lightning while carrying: 1,437 damage in CTF, none in CTY (refused), the flag taken each time (480 points); the captures the same in both |
| `ctf_return`, `cty_return` (`mp/ctf3`, 8 and 9; attacker, defender) | the carrier killed by the defender's rocket 256 units away, then the flag returned (`flag 48 return 256 pickup=64 rep=3000`) | the rocket hits the chest and kills at every repetition (100, 48 deaths), the defender hurt by its own splash (5); the defender 1,488 points (1, 20 and 10 each time), three CTF messages per repetition (taken, carrier killed, returned); the same in both |
| `siege_saber_weapons` (`mp/siege_hoth`, 7) | the medium style's attack (`idle`), the disruptor's shot, its sniper shot held 1.7 s, the repeater's alternate | the saber 198 damage (`saber_medium_idle`, the step 14 case on `mp/ffa3` in 96 repetitions: 1,200); the disruptor 65 a shot (50 on the head), every second shot kills (in free for all 39, no death); the sniper shot 179 (in free for all 169: the charge 68 instead of 60, its cap); the repeater's alternate 5,328 (6,192 in free for all, the same 48 deaths) |
| `siege_rocket` (`mp/siege_hoth`, 7) | the rocket's lock held 0.9, 1.4 and 1.5 s | at 0.9 and 1.4 s no homing (6 and 9 units): 85 a hit on the arm, no death (in free for all both home and kill); at 1.5 s the rocket homes, 100, 48 deaths |
| `siege_pull_objective` (`mp/siege_hoth`, 7) | pull at level 3 against a defender at level 3, facing the attacker then turned away (`away`); the objective 2 | facing, one pull in three goes through (below), turned away none is countered (the defender moves 7,061 then 7,266 units, 960 instants off the ground in both); the objective completed at every repetition: 48 events, 960 points to the attacker |
| `force_pull_levels` (`mp/ffa3`, 0; the server of part 1) | pull at level 3 against a defender at level 3 turned away, after the cases of before | the same counts as facing the attacker: outside siege the defender counters whichever way it faces |

Found on the way (20 November 2026):

- The flags 64 units from the attacker, not 32 as decided: an item is touched from 44 to 50 units away (`BG_PlayerTouchesItem`), so at 32 the attacker touched both flags from its place and took and captured at every frame (444 captures in 6 repetitions). At 64 a flag is out of reach in every direction, and the 300 ms walk of `pickup=` reaches it.
- A rocket aimed from the eyes at the carrier passes beside the model's head (the shot leaves the launcher's muzzle, 8 units to the right; the missiles hit the ghoul2 model); aimed lower from the eyes it hits an arm (85: 85 % of the damage) or the legs (70 %, `G_LocationBasedDamageModifier`), which the carrier survives, thrown back onto its own flag, where it captures. Aimed from the muzzle at the chest, the rocket kills.
- In siege a defender facing the one who pulls it counters the first and the third pull of a repetition, not the second: with a saber the puller's arm is free 200 ms after a pull (`w_force.cpp`, "hold less so can attack right after a pull"), so its second pull, 600 ms after the first, finds the defender still raising its weapon after its counter (`weaponTime` 100, `CanCounterThrow`). In free for all the second pull does not happen. A temporary trace of `CanCounterThrow` showed it; the siege's rule, not changed.

At the old commit, with the port (`scenarios_golden.patch`): the 17 scenarios give the same counts as the build of the day but two of the siege, both by the charge's fix (section 9): the sniper shot held 1.7 s (176 instead of 179: the charge counted to 1,650 ms, 66), and the lock held 1.5 s (at the old commit counted to 1,450 ms, 9 units: no homing, 85 a hit, no death; the score line differs too). The two files now differ by 53 lines, 20 scenarios. The 198 scenarios of before give the same counts as the goldens of before. Two runs of the build of the day give the same file. 215 scenarios on 94 servers, about 2 min on the development machine (30 servers at a time).

On Linux (20 November 2026): the game module built by GCC 15 on the Linux VM and by MSVC, no warning; the 215 scenarios on the VPS: `golden: SAME`.

Decided on 20 November 2026, after part 4b: the findings of parts 2 to 4b are the base game's (the same code in 1.01, `E:\Git\jka\jediacademy\codemp`) and are kept, nothing changed: the duel game type's saber locks (`w_saber.c`, a lock tried only above a clash's damage of 1), the power duel left in its intermission (`CheckIntermissionExit` the same), a saber lock's pushes read once per server frame (`g_active.c`, `saberLockHitCheckTime`), the single and dual sabers' cartwheel on the legs only (`bg_saber.c`, "force only on legs"), the staff's kicks (`G_KickTrace`), the emplaced gun's barrels 10 units beside the view (`WP_FireEmplaced`), the siege's second pull through a defender facing the puller (`w_force.c`, the arm free 200 ms after a pull and the 60 degrees). Of these, the duel's locks, the power duel's intermission and the staff's kicks were read in the code, not measured in the 1.01 game. The single player's view that does not turn, new in ModernJK, is fixed with plan step 15 (SDL 3).

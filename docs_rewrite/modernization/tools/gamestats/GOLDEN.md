# The scenarios' goldens

The reference counts of the scripted bot scenarios (plan steps 14 and 20; [../../progress/step-20-scenarios.md](../../progress/step-20-scenarios.md)), in one place: what the two files hold, how to run and compare them, and every difference between them with its reason. Any difference not listed here is a bug until it is explained.

State: 20 November 2026, plan step 20 part 8. 289 scenarios, 14 lines each, 4,046 lines per file.

## 1. The two files

| file | what it holds | recorded with |
|---|---|---|
| `scenarios_golden_step14.txt` | the counts of the old commit: plan step 14 part 4b (13 November 2026), with the scenarios written since ported to it | the branch `scenarios-golden`, worktree `E:\Git\jka\ModernJK_golden` (C code); its port of the scenarios is also kept as `scenarios_golden.patch` |
| `scenarios_golden.txt` | the counts of the build of the day, once every difference with the first file is explained | the main branch |

`scenarios.sh` compares a run with `scenarios_golden.txt` by default (`golden: SAME` or `DIFF`); `GOLDEN=scenarios_golden_step14.txt` compares with the other. Both files have LF line endings; a run's output on Windows has CRLF (`diff --strip-trailing-cr`, or `tr -d '\r'` before writing a golden).

## 2. The format

One line per statistics line of a scenario: `<scenario name>: <line>`. The name comes from the scenario's command in the run's `measure.cfg` (`gamestats_scn.py`): outside free for all the game type first, then the type and the arguments after the repetitions, the options' `=` removed (`saber 96 fast idle` is `saber_fast_idle`; `flag 48 capture 128 pickup=64` in CTF is `ctf_flag_capture_128_pickup64`). The scenarios come in the order of the servers in `cases()` of `scenarios.sh`, and in each server in the order of its scenarios.

Each scenario has 14 lines, in this order:

1. the saber line (swings, wounds, damage, blocks, parries, locks, idle touches, missiles blocked, force damage, deaths);
2. `force statistics` (the six powers of step 14);
3. `force statistics of the other powers`;
4. `event statistics` (the events by type, without `EV_BODYFADE`, `EV_ITEM_RESPAWN`, `EV_GLOBAL_SOUND`, `EV_PLAYDOORSOUND` and `EV_DEATH1` to `3`, which the free play leaves or a static counter cycles: `IGNORE` in `gamestats_scn.py`);
5. `damage statistics` (the health taken and the deaths by cause);
6. `shot statistics`;
7. `style statistics`;
8. `saber special move statistics`;
9. `scenario statistics` (by role: health and force gained and lost, instants off the ground, knocked down, mind tricked, in a saber lock, distance moved, end place);
10. `score statistics` (by role: score and captures; the red and blue team scores);
11. `team statistics` (by role, the spectator's included: team changes, instants as a free spectator and distance moved then, instants following each role, distance from the player followed);
12. `item statistics` (the items picked up by item; the health taken from what is not a client by cause, as a shield, a sentry, an E-Web, a corpse or a mine; by role: armour and ammunition gained and lost, instants cloaked, flying a jetpack, zoomed in);
13. `vehicle statistics` (the health taken from the NPCs and vehicles by cause, which the damage line counts too; the vehicle's health and shield lost and its destructions; by role: boardings, exits, instants aboard, distance moved aboard);
14. `npc statistics` (by role: instants held by a monster, as the NPC's enemy, in a pain animation; in an NPC scenario the NPC is the defender).

The duration and frame count of the saber line's header are removed (they depend on the rate).

The lines added over the parts, each time with every scenario of before checked unchanged without the new line:

| since | lines per scenario | the change |
|---|---|---|
| plan step 14 (`scenarios40_golden.txt`) | 7 | lines 1, 2 and 4 to 7, and a seventh from a second `gamestats` after the scenario (zeros, or what happened after it) |
| part 1 (19 November 2026) | 8 | the seventh line of step 14 gone; lines 3 and 9 added |
| part 3 (19 November 2026) | 9 | line 8 added; line 9 gained the instants in a saber lock |
| part 4a (20 November 2026) | 10 | line 10 added; the scenarios outside free for all named after their game type |
| part 5 (20 November 2026) | 11 | line 11 added |
| part 6 (20 November 2026) | 12 | line 12 added |
| part 7 (20 November 2026) | 13 | line 13 added |
| part 8 (20 November 2026) | 14 | line 14 added |

## 3. The differences between the two files

68 lines in 34 scenarios, from two fixes. 54 lines in 20 scenarios come from the first: the weapons' charge and the rocket's lock counted to the time of the command that releases the button, no longer to the last 20 Hz instant, up to 50 ms earlier (19 November 2026; [../../progress/step-20-scenarios.md](../../progress/step-20-scenarios.md) sections 9 and 12, [../../CHANGES.md](../../CHANGES.md) section 24). For a bot the gap is 50 ms in every case below, each held a multiple of 50 ms: the build of the day counts the charge 50 ms longer than the old commit.

| scenario | lines | why |
|---|---|---|
| `weapon_4_1` | 5 | the bryar's alternate held 1 s: a unit more (88 to 110 damage a shot), every shot kills (the saber line, events, damage, scenario and score lines) |
| `weapon_4_1_256_hold400`, `hold600`, `hold800` | 2 each | the bryar's alternate: a unit more (19 to 44, 44 to 66, 66 to 88 a shot) |
| `weapon_16_1_256_hold1000` | 5 | the old bryar's alternate held 1 s: as `weapon_4_1` |
| `weapon_16_1_256_hold400`, `hold600`, `hold800` | 2 each | the old bryar's alternate: as the bryar's |
| `weapon_7_0` | 3 | the bowcaster held 1 s: 5 bolts instead of 3, the outer ones miss |
| `weapon_7_0_256_hold600` | 3 | the bowcaster held 600 ms: 3 bolts instead of 1 |
| `weapon_6_1` | 1 | the disruptor's sniper shot held 1 s: 2 damage more, 3 with the head's factor |
| `weapon_6_1_256_hold100`, `hold500` | 2 each | the sniper shot: as `weapon_6_1` |
| `weapon_9_1_256_hold1400_rep3000`, `hold2100_rep4000` | 2 each | the DEMP2's alternate: a unit more |
| `weapon_12_0_256_hold450` | 2 | the thermal detonator held 450 ms: 0.50 of full speed instead of 0.44, it hits without bouncing first |
| `weapon_12_1_256_hold450` | 3 | its alternate: it hits directly instead of by its splash |
| `weapon_11_1_256_hold750_rep3000` | 5 | the rocket's lock held 750 ms: 10 units (the homing threshold) instead of 9; every rocket homes and kills (85 a hit and no kill at the old commit) |
| `siege_weapon_6_1_256_hold1700_rep3600` | 1 | the siege's sniper shot held 1.7 s: the charge 68 instead of 66 (179 a shot instead of 176; in free for all the cap of 60 hides it) |
| `siege_weapon_11_1_256_hold1500_rep3000` | 6 | the siege's rocket lock held 1.5 s: 10 units of 150 ms instead of 9, it homes and kills (as `weapon_11_1_256_hold750_rep3000`); its splash then hurts the corpse (the item line) |

The other charges held give the same counts in both: at their minimum or maximum, or on the same side of a threshold either way.

The second fix, the names of the last events (17 November 2026, plan step 18 part 4; [../../CHANGES.md](../../CHANGES.md) section 24, the multiplayer logs): the old commit's list of event names (`eventnames`, `bg_misc.c`) stops before the last events, from `EV_ANGER1` to `EV_SIEGESPEC` ("fixme, added a bunch that aren't here!"), so its `event statistics` line prints `(null)` where the build of the day prints these names, with the same counts: 14 lines in 14 scenarios.

| scenario | lines | why |
|---|---|---|
| `siege_vehicle_atst_vehicle_ride_512_rep5000` | 1 | the defender killed by the AT-ST early in each repetition waits for the next wave as a siege spectator (`EV_SIEGESPEC` 48), the only scenario where the event happens |
| the 13 NPC scenarios (part 8: `npc_rancor_180_rep16000`, `weapon_11_0_256_npcrancor_rep3000`, `saber_strong_idle_112_npcrancor_rep3000`, `npc_wampa_128_rep8000`, `saber_strong_idle_64_npcwampa_rep6000`, `force_lightning_3_128_npcwampa_rep6000`, `npc_stormtrooper_128_rep6000`, `weapon_4_0_256_npcstormtrooper_rep6000`, `force_grip_3_128_npcstormtrooper_rep4000`, `force_push_3_128_npcstormtrooper_rep4000`, `npc_reborn_64_rep10000`, `saber_medium_idle_64_npcreborn_rep6000`, `force_push_3_128_npcreborn_rep4000`) | 1 each | the NPCs' voice events (`EV_ANGER1` to `3` at their first sight of the bot, the reborn's `EV_COMBAT1` to `3`, `EV_TAUNT1` to `3` and `EV_GLOAT1`) |

## 4. Names given twice

Two servers play the same commands with other settings; their scenarios are told apart by their place in the file:

| name | first | second |
|---|---|---|
| `weapon_2_1_40` | server `weapon_2` | server `weapon_2_debugmelee` (`g_debugMelee 1`: the alternate kicks) |
| `team_saber_medium_idle_targetally`, `team_weapon_12_0_256_targetally`, `team_force_lightning_3_128_targetally` | server `team_friendlyfire0` (the friendly fire off) | server `team_friendlyfire1` (`g_friendlyFire 1`, `g_friendlySaber 1`) |
| `team_team_switch` | server `team_switch` | server `team_switch_balance` (`g_teamForceBalance 1`: the change refused) |

## 5. Commands

On Windows (Git Bash), the build of the day, then the old commit:

```
bash /e/Git/jka/ModernJK/docs_rewrite/modernization/tools/gamestats/scenarios.sh <prefix> /e/Git/jka/ModernJK/build-netcodec/modernjk-clang/jampgamex86_64.dll
BUILD=E:/Git/jka/ModernJK_golden/build-clang DED_EXE=openjkded.x86_64.exe GOLDEN=scenarios_golden_step14.txt bash /e/Git/jka/ModernJK/docs_rewrite/modernization/tools/gamestats/scenarios.sh <prefix> /e/Git/jka/ModernJK_golden/build-clang/jampgamex86_64.dll
```

The outputs are `C:/Users/<user>/AppData/Local/Temp/gamestats/scenarios_<prefix>.txt`. Comparing the two runs must show only the lines of section 3:

```
diff --strip-trailing-cr <old commit's output> <build of the day's output>
```

On Linux (the VPS), the build of the day: `DED=<modernjkded> BASE=<fs_basepath> scenarios.sh <prefix> <jampgame .so>`, 18 servers at a time by default, with `gamestats.sh`, `gamestats_cfg.sh`, `gamestats_scn.py` and `scenarios_golden.txt` beside it; the outputs in `~/.cache/gamestats` (`GAMESTATS_DIR` for another folder).

## 6. A part of plan step 20, for the goldens

1. The scenarios written and tried on the build of the day, then ported by exact replacements to `codemp/game/g_scenario.c` of `scenarios-golden` and built there (no warning from it).
2. Both builds run; their outputs compared: only section 3's lines, and the part's new scenarios the same in both, or every new difference explained (and added to section 3).
3. If the format changed, every scenario of before checked against the goldens of before without the new line.
4. Two runs of the build of the day give the same file (`cmp`).
5. The goldens written with LF: the old commit's output to `scenarios_golden_step14.txt`, the build of the day's to `scenarios_golden.txt`; the patch written again from the worktree (`git diff "$(git merge-base HEAD master)" -- . ':(exclude)README.md'`).
6. This document updated (the counts, the format, section 3, section 4).

# ModernJK: the `scenarios-golden` branch

This branch is ModernJK as it was at plan step 14, part 4b (13 November 2026: one user command per tick), with the scripted bot scenarios of plan step 20 ported to it. It exists for one thing: to record the reference counts of those scenarios, `scenarios_golden_step14.txt` in the main branch (`docs_rewrite/modernization/tools/gamestats/`). It is never merged, and nothing else changes in it.

## Why

The scenarios check that the game is unchanged after each change of the code. Their first counts (the 40 fights of step 14) were recorded at this commit. When step 20 widened them to every force power, weapon, saber move, game type, team, item, vehicle and NPC, the new counts had to come from the same game, not from the build of the day, or the golden would only record what the build of the day does. So each new scenario is written on the build of the day first (the C++ types catch the mistakes), then ported here, where the game is still C (`qboolean`, unscoped enumerations), and run on both builds. Every difference between the two is explained by a fix decided since, or it is a bug. The method and the decisions: `docs_rewrite/modernization/PLAN.md` step 20 and `docs_rewrite/modernization/progress/step-20-scenarios.md` in the main branch.

The main branch keeps two goldens: `scenarios_golden_step14.txt`, the counts of this branch, and `scenarios_golden.txt`, the counts of the build of the day once their differences with this branch are explained.

## What the branch changes

Five files of the game module, nothing in the engine:

| file | change |
|---|---|
| `codemp/game/g_scenario.c` | the scenarios: the `scenario` server command, the bots' scripts, the reset at every repetition, the statistics per role and the scores |
| `codemp/game/g_svcmds.c` | the lines `gamestats` prints: the other force powers, the special saber moves started |
| `codemp/game/g_local.h` | the counters of the special saber moves, the items picked up and the health taken from what is not a client |
| `codemp/game/g_items.c`, `codemp/game/g_combat.c` | the items picked up and the health taken from what is not a client counted |

The same changes are kept in the main branch as `docs_rewrite/modernization/tools/gamestats/scenarios_golden.patch`, so that this build can be made again without the branch (`git apply` in a worktree of the step 14 part 4b commit). After each part, the patch is written again from here:

```
git diff "$(git merge-base HEAD master)" -- . ':(exclude)README.md' > <main worktree>/docs_rewrite/modernization/tools/gamestats/scenarios_golden.patch
```

## Using it

In a worktree of its own, beside the main one:

```
git worktree add ../ModernJK_golden scenarios-golden
```

Built with the same compiler as the main branch (llvm-mingw clang on Windows), Release:

```
cmake -G Ninja -S . -B build-clang -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
ninja -C build-clang openjkded.x86_64 jampgamex86_64
```

The dedicated server was named `openjkded` then. The scenarios are run by the main branch's script, given this build, and compared with this branch's golden:

```
BUILD=<this worktree>/build-clang DED_EXE=openjkded.x86_64.exe GOLDEN=scenarios_golden_step14.txt \
	bash <main worktree>/docs_rewrite/modernization/tools/gamestats/scenarios.sh <prefix> <this worktree>/build-clang/jampgamex86_64.dll
```

It ends with `golden: SAME` when this build gives the recorded counts. The output of a run, `scenarios_<prefix>.txt`, is compared with the build of the day's with `diff` (the line endings converted to LF first). A part: written and tested on the build of the day, ported here by exact replacements, built, both builds run, compared, every difference explained, committed here, the patch written again, then the goldens recorded.

## The scenarios

The full syntax is at the top of `codemp/game/g_scenario.c`. Two bots, an attacker and a defender, sometimes a third one as the attacker's ally, are placed face to face and reset at every repetition (2 s, or `rep=`), then play a fixed script of commands timed in milliseconds. The first repetition is not counted. The statistics of the counted ones are printed at the end. The script `scenarios.sh` gives each server its map, game type, bots and settings, and plays several scenarios of one group one after the other on the same server: 271 scenarios on 107 servers, about 2 min with 30 servers at a time. Outside free for all, a scenario's name starts with its game type (`duel_saber_fast_idle`).

| group | scenarios | what they play |
|---|---|---|
| saber (step 14) | 10 | the five styles (fast, medium, strong, dual sabers, staff), an attack in each of the eight directions in turn, 96 repetitions, the defender idle or attacking at the same time |
| force (step 14) | 6 | lightning, grip, drain, rage, protect, absorb at level 3 |
| weapons (step 14) | 24 | the weapons 4 to 15, primary and alternate fire |
| force powers (step 20, part 1) | 44 | the six of step 14 at levels 1 and 2; heal, levitation, speed, push, pull, telepathy, sight and saber throw at levels 1 to 3; push and pull against a defender at level 3; team heal and team energize at levels 1 to 3 in a team game, with the ally |
| weapons (step 20, part 2) | 46 | the stun baton, the melee (and its kicks and grabs with `g_debugMelee`), the old bryar, the emplaced gun and two turrets spawned by the scenario; the charges at several lengths (the bryars, the bowcaster, the DEMP2, the disruptor's sniper shot, the rocket launcher's lock, the thermal detonator) |
| saber moves (step 20, part 3) | 35 | the katas of the five styles, the back attacks, standing and crouched, the jump attacks, the lunges, the cartwheels, the staff's back flip and its kicks on the ground and in the air, the roll stab, the stab down on a defender knocked down, the throws of the single and dual sabers against an armed defender, the saber locks in a private duel |
| game types (step 20, part 4a) | 33 | the holocron picked up, then its power used (lightning, push, heal), and a saber attack at the levels of the holocron game; the Jedi Master's saber picked up, then a saber attack or lightning; in the duel game type the saber cases of step 14, the locks and the force jump; in a power duel the five styles against the lone duelist; in a team game the attacker's teammate as the target, the friendly fire off and on, and a kill at every repetition for the team score |
| game types (step 20, part 4b) | 17 | in CTF and CTY, the flags moved next to the bots at every reset: a capture, lightning used while carrying the flag (refused in CTY), the carrier killed by the defender's rocket and the flag returned; in siege, the siege's rules (the saber's attack, the disruptor's shot and sniper shot, the repeater's alternate, the rocket's lock at 0.9, 1.4 and 1.5 s, the pull of a defender facing the attacker or turned away) and an objective completed at every repetition; in free for all, the pull of a defender turned away |
| teams and spectators (step 20, part 5) | 9 | in a team game the defender changes team at every repetition, then the same refused by the balance (`g_teamForceBalance 1`); in CTF the carrier changes team, which sends the flag back; in free for all a third bot made a spectator follows the attacker during a saber attack and a rocket, then the game's `follow1` and `follow2`, cycles with attack and the alternate attack then stops following with a jump, and flies forward as a free spectator |
| items (step 20, part 6) | 47 | the 11 holdable items used (the medpacs, the binoculars, the cloak, the jetpack, the seeker, the sentry and the E-Web shooting at the defender, the shield its owner shoots into, the health and ammunition dispensers on a teammate, the ammunition thrown); every item picked up: the health, the two armours, a holdable item, every weapon and every ammunition; the four powerups before a force power |

Each scenario prints, after its repetitions:

- the saber statistics: swings, wounds, damage, blocks, parries, locks, idle touches, missiles blocked, force damage, deaths;
- the force powers: starts, periodic events and time held;
- the events of the game by type;
- the health taken and the deaths by cause;
- the shots by weapon and fire mode;
- the saber attacks started and bounced by style, and the special saber moves started by move;
- by role: the health and force gained and lost, the instants off the ground, knocked down, mind tricked and in a saber lock, the distance moved and the end place against the start;
- what the counted repetitions added to the scores: each role's score and captures, the red and blue team scores;
- by role, the spectator's included: the team changes, the instants as a free spectator and the distance moved then, the instants following each role and the distance from the player followed;
- the items picked up, the health taken from what is not a client by cause, and by role the armour and ammunition gained and lost and the instants cloaked, flying a jetpack and zoomed in.

## Differences with the build of the day

The two builds give the same counts but where a fix decided since changes them. Each is explained in `docs_rewrite/modernization/progress/step-20-scenarios.md` of the main branch, and listed line by line in `docs_rewrite/modernization/tools/gamestats/GOLDEN.md`:

| since | what | scenarios |
|---|---|---|
| 19 November 2026 | the weapons' charge counted to the command that releases the button, no longer to the last 20 Hz instant, which was up to 50 ms earlier | 20 (the bryars', the bowcaster's, the DEMP2's, the sniper shot's, the rocket lock's and the thermal detonator's charges; since part 4b, the siege's sniper shot held 1.7 s and rocket lock held 1.5 s); in four of them the score too, since the charge decides whether every shot kills |

The vehicles and the NPCs (parts 7 and 8) are to come.

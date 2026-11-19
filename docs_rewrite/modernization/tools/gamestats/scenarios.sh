#!/bin/bash
# The scripted scenarios (plan steps 14 and 20), SEED=1 and FAST=1, several servers at a time, a server playing one or
# several scenarios one after the other (gamestats_cfg.sh); every scenario's statistics in one file
# (<runs>/scenarios_<prefix>.txt) to compare two platforms or two builds with diff, and against the recorded counts:
# scenarios_golden.txt, the build of the day's once its differences with the old commit are explained, and
# scenarios_golden_step14.txt, the old commit's (step 14 part 4b, the commit the 40 first scenarios' counts come from,
# with the later scenarios ported to it: plan step 20); GOLDEN=<file> compares with another file than the first.
# Usage (Windows, Git Bash; gamestats_windows.sh's build and game folders, BUILD and DED_EXE for another build):
#   scenarios.sh <prefix> <jampgame dll> [servers at a time, 30]
#   the old commit: BUILD=E:/Git/jka/ModernJK_golden/build-clang DED_EXE=openjkded.x86_64.exe
#   GOLDEN=scenarios_golden_step14.txt scenarios.sh <prefix> E:/Git/jka/ModernJK_golden/build-clang/jampgamex86_64.dll
# Usage (Linux; gamestats.sh's arguments in the environment):
#   DED=<modernjkded> BASE=<fs_basepath> scenarios.sh <prefix> <jampgame .so> [servers at a time, 4]
# 30 servers at a time on the development machine, 4 on the VPS (1 vCPU, 2 GB).
set -u
PREFIX=$1
MODULE=$2
HERE=$(cd "$(dirname "$0")" && pwd)
case "$(uname -s)" in
	MINGW*|MSYS*) WINDOWS=1; PAR=${3:-30}; RUNS="${GAMESTATS_DIR:-C:/Users/$USERNAME/AppData/Local/Temp/gamestats}" ;;
	*) WINDOWS=0; PAR=${3:-4}; RUNS="${GAMESTATS_DIR:-/tmp/gamestats}" ;;
esac
PYTHON=$(command -v python3 || command -v python)

# one server per line: <name>|<map>|<g_gametype>|<bots>|<cvars before the map>|<scenario>[;<scenario>...]
cases() {
	local ffa="mp/ffa3|0|Jedi Reborn" team="mp/ffa3|6|Jedi:red Reborn:blue Jedi:red" force="g_forcePowerDisable 0"
	local s d f w l
	# the 40 of step 14, one server each
	for s in fast medium strong dual staff; do for d in idle attack; do echo "saber_${s}_${d}|$ffa||saber 96 $s $d"; done; done
	for f in lightning grip drain rage protect absorb; do echo "force_${f}|$ffa|$force|force 48 $f"; done
	for w in 4 5 6 7 8 9 10 11 12 13 14 15; do for a in 0 1; do echo "weapon_${w}_${a}|$ffa|g_weaponDisable 0|weapon 48 $w $a"; done; done
	# step 20, the force powers: the six of step 14 at levels 1 and 2, the others at levels 1 to 3, a server per power
	for f in lightning grip drain rage protect absorb; do echo "force_${f}_levels|$ffa|$force|force 48 $f 1;force 48 $f 2"; done
	for f in heal levitation speed push pull telepathy see saberthrow; do
		l="force 48 $f 1;force 48 $f 2;force 48 $f 3"
		# push and pull: the defender at level 0 in the power, then at level 3, which resists a push of level 3
		case $f in push|pull) l="$l;force 48 $f 3 128 3" ;; esac
		echo "force_${f}_levels|$ffa|$force|$l"
	done
	for f in teamheal teamforce; do echo "force_${f}_levels|$team|$force|force 48 $f 1;force 48 $f 2;force 48 $f 3"; done
	# step 20, the weapons: the ones missing (stun baton, melee, the old bryar, the emplaced gun, the turrets) and the
	# charges at other lengths than the 1 s of step 14 (the bryars' alternate: a unit per 200 ms up to 5; the bowcaster's
	# primary: the same; the DEMP2's alternate: a unit per 700 ms up to 3; the disruptor's sniper shot: a unit per 50 ms,
	# full at 1.7 s; the rocket launcher's lock: full at 1.2 s; the thermal detonator: full speed at 0.9 s); a charge
	# released late gets a longer repetition (rep=), so that its shot lands before the reset
	local wd="g_weaponDisable 0" h
	echo "weapon_1|$ffa|$wd|weapon 48 1 0 40;weapon 48 1 1 40"
	echo "weapon_2|$ffa|$wd|weapon 48 2 0 40;weapon 48 2 1 40"
	# g_debugMelee: the alternate kicks (the eight directions), both buttons grab (a punch or a throw, 4 s)
	echo "weapon_2_debugmelee|$ffa|$wd;g_debugMelee 1|weapon 48 2 1 40;weapon 48 2 2 40 rep=6000"
	l=""; for h in 100 400 600 800; do l="$l;weapon 48 4 1 256 hold=$h"; done
	echo "weapon_4_charges|$ffa|$wd|${l#;};weapon 48 4 1 256 hold=1400 rep=3000"
	l="weapon 48 16 0"; for h in 100 400 600 800 1000; do l="$l;weapon 48 16 1 256 hold=$h"; done
	echo "weapon_16|$ffa|$wd|$l;weapon 48 16 1 256 hold=1400 rep=3000"
	echo "weapon_6_charges|$ffa|$wd|weapon 48 6 1 256 hold=100;weapon 48 6 1 256 hold=500;weapon 48 6 1 256 hold=1700 rep=3600;weapon 48 6 1 256 hold=2500 rep=4400"
	l=""; for h in 100 400 600 800; do l="$l;weapon 48 7 0 256 hold=$h"; done
	echo "weapon_7_charges|$ffa|$wd|${l#;};weapon 48 7 0 256 hold=1400 rep=3000"
	echo "weapon_9_charges|$ffa|$wd|weapon 48 9 1 256 hold=300;weapon 48 9 1 256 hold=1400 rep=3000;weapon 48 9 1 256 hold=2100 rep=4000;weapon 48 9 1 256 hold=2800 rep=4500"
	# the lock homes from 10 of its 16 units (750 ms held): held 750 ms, it locks with the charge counted to the releasing
	# command, not with the charge counted to the last 20 Hz instant before it (700 ms: the bot's release runs on the tick
	# before its time)
	echo "weapon_11_charges|$ffa|$wd|weapon 48 11 1 256 hold=300;weapon 48 11 1 256 hold=700 rep=3000;weapon 48 11 1 256 hold=750 rep=3000;weapon 48 11 1 256 hold=900 rep=3000;weapon 48 11 1 256 hold=1400 rep=3000"
	l=""; for a in 0 1; do for h in 100 450; do l="$l;weapon 48 12 $a 256 hold=$h"; done; l="$l;weapon 48 12 $a 256 hold=1400 rep=3500"; done
	echo "weapon_12_charges|$ffa|$wd|${l#;}"
	echo "weapon_17|$ffa|$wd|weapon 48 17 0;weapon 48 17 1"
	echo "turret|$ffa|$wd|turret 48 g2;turret 48 hoth"
	# step 20, the saber's special moves (g_scenario.cpp's saber modes), a server or two per style; the throws 128 units
	# away, the room a throw needs in front, with the saber throw allowed (g_forcePowerDisable without its bit); the roll
	# stab 400 units away, where the roll ends short of the defender, in repetitions of 3 s, long enough for the stab
	echo "saber_fast_moves|$ffa||saber 48 fast kata;saber 48 fast back;saber 48 fast lunge"
	echo "saber_medium_moves|$ffa||saber 48 medium kata;saber 48 medium back;saber 48 medium backcrouch;saber 48 medium jump"
	echo "saber_medium_moves2|$ffa||saber 48 medium cartwheel;saber 48 medium stabdown;saber 48 medium rollstab 400 rep=3000"
	echo "saber_strong_moves|$ffa||saber 48 strong kata;saber 48 strong back;saber 48 strong backcrouch;saber 48 strong jump"
	echo "saber_dual_moves|$ffa||saber 48 dual kata;saber 48 dual back;saber 48 dual jump;saber 48 dual lunge"
	echo "saber_dual_moves2|$ffa||saber 48 dual cartwheel;saber 48 dual stabdown;saber 48 dual rollstab 400 rep=3000"
	echo "saber_staff_moves|$ffa||saber 48 staff kata;saber 48 staff back;saber 48 staff jump;saber 48 staff lunge;saber 48 staff cartwheel"
	echo "saber_staff_moves2|$ffa||saber 48 staff backflip;saber 48 staff kick;saber 48 staff airkick;saber 48 staff stabdown;saber 48 staff rollstab 400 rep=3000"
	echo "saber_throw|$ffa|g_forcePowerDisable 32765|saber 48 fast throw 128;saber 48 dual throw 128"
	# the saber locks: a clash locks at once (g_saberLockFactor 20, every other condition of the game kept); the single
	# saber's attacks never meet the defender's while both are attacking, so only the dual sabers and the staff lock
	echo "saber_lock|$ffa|g_saberLocking 1;g_saberLockFactor 20|saber 48 dual lock;saber 48 staff lock"
}

one() {
	local name map gametype bots pre scn
	IFS='|' read -r name map gametype bots pre scn <<< "$1"
	if [ "$WINDOWS" = 1 ]; then
		SEED=1 FAST=1 MAP="$map" GAMETYPE="$gametype" BOTS="$bots" PREMAP="$pre" SCENARIO="$scn" \
			bash "$HERE/gamestats_windows.sh" 1 "${PREFIX}_${name}|$MODULE" > /dev/null 2>&1
	else
		SEED=1 FAST=1 MAP="$map" GAMETYPE="$gametype" BOTS="$bots" PREMAP="$pre" SCENARIO="$scn" \
			bash "$HERE/gamestats.sh" "$DED" "$BASE" "$MODULE" 1 "${PREFIX}_${name}" > /dev/null 2>&1
	fi
	echo "done $name"
}
export -f one
export PREFIX MODULE HERE WINDOWS DED="${DED:-}" BASE="${BASE:-}"

# the servers with the most scenarios first, so that the last ones to start are short
cases | awk -F'|' '{ s = $6; print gsub( /;/, "", s ) "\t" $0 }' | sort -s -rn -k1,1 | cut -f2- | xargs -P "$PAR" -I{} bash -c 'one "$@"' _ {}
tags=()
for c in $(cases | cut -d'|' -f1); do
	tags+=("${PREFIX}_${c}")
done
"$PYTHON" -I "$HERE/gamestats_scn.py" --dump "$RUNS" "${PREFIX}_" "${tags[@]}" > "$RUNS/scenarios_${PREFIX}.txt"
echo "statistics: $RUNS/scenarios_${PREFIX}.txt"
# the recorded counts (GOLDEN, scenarios_golden.txt by default): the same on Windows and Linux, with clang, GCC and MSVC
GOLDEN=${GOLDEN:-scenarios_golden.txt}
if diff <(tr -d '\r' < "$HERE/$GOLDEN") <(tr -d '\r' < "$RUNS/scenarios_${PREFIX}.txt") > /dev/null; then
	echo "golden: SAME"
else
	echo "golden: DIFF (diff $HERE/$GOLDEN $RUNS/scenarios_${PREFIX}.txt)"
fi

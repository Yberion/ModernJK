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
#   DED=<modernjkded> BASE=<fs_basepath> scenarios.sh <prefix> <jampgame .so> [servers at a time, 18]
# 30 servers at a time on the development machine, 18 on the VPS (1 vCPU, 2 GB; 12 took 5 min 44 s, its core 35 % busy
# at most, 24 took 4 min 58 s, the core saturated, on 20 November 2026): in fast mode a server waits at least
# 1 ms between two frames (Com_Frame), so it plays at most 10 times faster than real time and sleeps most of that time
# (a scenario's frame takes about 0.06 ms on the VPS). The runs' files: GAMESTATS_DIR, by default the user's
# Temp/gamestats on Windows, ${XDG_CACHE_HOME:-$HOME/.cache}/gamestats on Linux (not /tmp, often in memory).
set -u
PREFIX=$1
MODULE=$2
HERE=$(cd "$(dirname "$0")" && pwd)
case "$(uname -s)" in
	MINGW*|MSYS*) WINDOWS=1; PAR=${3:-30}; RUNS="${GAMESTATS_DIR:-C:/Users/$USERNAME/AppData/Local/Temp/gamestats}" ;;
	*) WINDOWS=0; PAR=${3:-18}; RUNS="${GAMESTATS_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/gamestats}" ;;
esac
PYTHON=$(command -v python3 || command -v python)

# one server per line: <name>|<map>|<g_gametype>|<bots>|<cvars before the map>|<scenario>[;<scenario>...][|<free play>]
# (the free play before the scenarios in ticks of 10 ms, 3000 when not given: gamestats_cfg.sh's FREEPLAY)
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
		# the pull of a defender turned away, countered in free for all (not in siege: part 4b)
		case $f in pull) l="$l;force 48 pull 3 128 3 away" ;; esac
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
	# step 20 part 10: the emplaced gun aimed at the defender's chest from between its two barrels, as its crosshair shows
	echo "weapon_17|$ffa|$wd|weapon 48 17 0;weapon 48 17 1;weapon 48 17 0 aim=eyes at=chest"
	echo "turret|$ffa|$wd|turret 48 g2;turret 48 hoth"
	# step 20 part 10: the defender's head aimed at from the eyes, as a bot, an NPC or a crosshair at the centre of the
	# screen, then from the weapon's muzzle point, as the dynamic crosshair shows (surveys/shot-offsets.md): a bolt
	# without a box (the bryar), missiles with a box (the DEMP2, the rocket, the concussion rifle), the concussion
	# rifle's beam from the muzzle point, the disruptor's from the eyes
	l=""; for w in "4 0" "9 0" "11 0"; do for a in eyes muzzle; do l="$l;weapon 48 $w 256 aim=$a"; done; done
	echo "weapon_aim1|$ffa|$wd|${l#;}"
	l=""; for w in "15 0" "15 1" "6 0"; do for a in eyes muzzle; do l="$l;weapon 48 $w 256 aim=$a"; done; done
	echo "weapon_aim2|$ffa|$wd|${l#;}"
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
	# step 20, the game types (part 4a), the scenarios named after their game type (gamestats_scn.py). Holocron: a
	# holocron of the power 32 units in front of the attacker, which walks onto it (pickup=), and a saber attack at the
	# levels the holocrons leave (attack 1, defence 0); Jedi Master: the attacker walks onto the Jedi Master's saber, then
	# attacks; duel: the saber cases of step 14, the locks (a clash may lock without a private duel) and the force jump,
	# whose cost is lower; power duel: the five styles against the lone duelist, after a free play of 1 s (its death
	# ends the round, after which the game stays in its intermission); team: the attacker's teammate as the target
	# (target=ally), the friendly fire off then on, and a kill at every repetition for the team score
	local holo="mp/ffa3|1|Jedi Reborn" jm="mp/ffa3|2|Jedi Reborn" duel="mp/duel1|3|Jedi Reborn" pduel="mp/duel1|4|Jedi Reborn Jedi"
	echo "holocron|$holo|$force|force 48 lightning 3 128 pickup=32;force 48 push 3 128 pickup=32;force 48 heal 3 128 pickup=32;saber 48 medium idle"
	echo "jedimaster|$jm|$force|saber 48 medium idle 96 pickup=32;force 48 lightning 3 128 pickup=32"
	for d in idle attack; do
		l=""; for s in fast medium strong dual staff; do l="$l;saber 48 $s $d"; done
		echo "duel_saber_$d|$duel||${l#;}"
	done
	echo "duel_lock|$duel|g_saberLocking 1;g_saberLockFactor 20|saber 48 dual lock;saber 48 staff lock"
	echo "duel_levitation|$duel|$force|force 48 levitation 1;force 48 levitation 2;force 48 levitation 3"
	l=""; for s in fast medium strong dual staff; do l="$l;saber 48 $s idle"; done
	echo "powerduel|$pduel||${l#;}|100"
	l="saber 48 medium idle target=ally;weapon 48 12 0 256 target=ally;force 48 lightning 3 128 target=ally"
	echo "team_friendlyfire0|$team|$wd;$force|$l;saber 48 dual back"
	echo "team_friendlyfire1|$team|$wd;$force;g_friendlyFire 1;g_friendlySaber 1|$l"
	# step 20, the game types with objectives (part 4b). CTF and CTY (mp/ctf3, capturelimit 0, which would end the game
	# after 8 captures): at every reset the flags at their bases, the enemy flag 64 units in front of the attacker
	# (pickup=), its own as far behind it (an item is touched from 44 to 50 units away, BG_PlayerTouchesItem); a capture,
	# with the ally beside the attacker for the team bonus; lightning used while carrying the flag (refused in CTY); the
	# carrier killed by the defender's rocket, then the flag dropped returned, without the ally, whom the rocket's splash
	# would hurt
	local g n
	for g in 8 9; do
		if [ "$g" = 8 ]; then n=ctf; else n=cty; fi
		echo "${n}_capture|mp/ctf3|$g|Jedi:red Reborn:blue Jedi:red|capturelimit 0;$force|flag 48 capture 128 pickup=64;force 48 lightning 3 128 pickup=64"
		echo "${n}_return|mp/ctf3|$g|Jedi:red Reborn:blue|capturelimit 0|flag 48 return 256 pickup=64 rep=3000"
	done
	# siege (mp/siege_hoth; the bots take their team's first class, which gives no force power and imposes no saber
	# style; a dead bot respawned at once): the siege's rules, the saber's attack (the attacker's level tripled, the
	# damage not doubled), the disruptor's shot (50 instead of 30) and sniper shot (the charge capped at 200 instead of
	# 60: held 1.7 s as in free for all), the repeater's alternate (a smaller splash), the rocket's lock (homing from
	# 1.5 s held instead of 0.75 s), the pull of a defender turned away (not countered); the imperials' objective 2
	# completed at every repetition (it neither counts for the round nor fires a target); each server well under the
	# round's 20 minutes
	local siege="mp/siege_hoth|7|Jedi:red Reborn:blue"
	echo "siege_saber_weapons|$siege||saber 48 medium idle;weapon 48 6 0;weapon 48 6 1 256 hold=1700 rep=3600;weapon 48 8 1"
	echo "siege_rocket|$siege||weapon 48 11 1 256 hold=900 rep=3000;weapon 48 11 1 256 hold=1400 rep=3000;weapon 48 11 1 256 hold=1500 rep=3000"
	echo "siege_pull_objective|$siege|$force|force 48 pull 3 128 3;force 48 pull 3 128 3 away;objective 48 2"
	# step 20, teams and spectators (part 5). In a team game the defender changes team at every repetition (a suicide,
	# not counted against its team, then a spawn point of its new team; the reset puts it back without a death), then
	# the same refused by the balance (g_teamForceBalance 1: two red against one blue); in CTF the carrier changes team,
	# which sends the flag back to its base; in free for all a third bot made a spectator follows the attacker during the
	# medium style's attack and a rocket, then the game's follow1 and follow2 (the first and second players by client
	# number), cycles with attack and the alternate attack then stops following with a jump, and flies forward 1 s as a
	# free spectator, through the defender
	echo "team_switch|$team||team 48 switch"
	echo "team_switch_balance|$team|g_teamForceBalance 1|team 48 switch"
	echo "ctf_switch|mp/ctf3|8|Jedi:red Reborn:blue|capturelimit 0|flag 48 switch 128 pickup=64"
	local spec="mp/ffa3|0|Jedi Reborn Jedi"
	echo "spectator_follow|$spec|$wd|saber 48 medium idle spectator;weapon 48 11 0 spectator;weapon 48 11 0 spectator=follow1;weapon 48 11 0 spectator=follow2"
	echo "spectator_commands|$spec||spectate 48 cycle;spectate 48 fly"
	# step 20, the items (part 6), with the weapons allowed (with the saber only, the game spawns no item). The 11
	# holdable items used: the medpacs (the attacker at 40 health), the binoculars, the cloak, the jetpack (a jump, the
	# use key in the air, then the jump held), the ammunition dispenser on nothing (the ammunition thrown); the seeker, the
	# sentry and the E-Web shooting at the defender, in repetitions of 4 s, and the shield its owner shoots into; the
	# health and ammunition dispensers on the teammate in front (target=ally, 48 units away). The items picked up, 64
	# units in front of the attacker (pickup=), which has 40 health, no armour, no weapon and no ammunition before: the
	# health, the two armours, a holdable item, every weapon and every ammunition; the four powerups before a force power
	# (the enlightenments raise its level, the ysalamiri refuses it, the boon refills the force faster)
	echo "holdable_use|$ffa|$wd|holdable 48 medpac;holdable 48 medpac_big;holdable 48 binoculars;holdable 48 cloak;holdable 48 jetpack;holdable 48 ammodisp"
	echo "holdable_shots|$ffa|$wd|holdable 48 seeker 256 rep=4000;holdable 48 sentry 256 rep=4000;holdable 48 eweb 256 rep=4000;holdable 48 shield"
	echo "holdable_dispensers|$team|$wd|holdable 48 healthdisp 48 target=ally;holdable 48 ammodisp 48 target=ally"
	local items1="item_medpak_instant item_shield_sm_instant item_shield_lrg_instant item_seeker weapon_stun_baton weapon_melee weapon_saber weapon_blaster_pistol"
	local items2="weapon_concussion_rifle weapon_bryar_pistol weapon_blaster weapon_disruptor weapon_bowcaster weapon_repeater weapon_demp2 weapon_flechette"
	local items3="weapon_rocket_launcher weapon_thermal weapon_trip_mine weapon_det_pack weapon_emplaced weapon_turretwp ammo_thermal ammo_tripmine"
	local items4="ammo_detpack ammo_force ammo_blaster ammo_powercell ammo_metallic_bolts ammo_rockets ammo_all"
	local k=0 it
	for g in "$items1" "$items2" "$items3" "$items4"; do
		k=$(( k + 1 )); l=""
		for it in $g; do l="$l;item 48 $it pickup=64"; done
		echo "item_pickup$k|$ffa|$wd|${l#;}"
	done
	echo "item_powerups|$ffa|$wd;$force|force 48 lightning 1 128 item=item_force_enlighten_dark pickup=64;force 48 heal 1 128 item=item_force_enlighten_light pickup=64;force 48 lightning 3 128 item=item_ysalimari pickup=64;force 48 protect 3 128 item=item_force_boon pickup=64"
	# step 20, the vehicles (part 7), on mp/siege_korriban in siege (siege_hoth's turrets fire at the vehicles and the
	# bots past 256 units, mp/ctf2's spawn points are under roofs): the vehicle spawned at every reset in front of the
	# attacker, which boards it with the use key; the swoop fires 1 s at the defender, drives 1 s into it, its rider
	# jumps off; the AT-ST fires its laser 1 s and a rocket, walks 1 s, its rider gets off with the use key; the X-wing
	# takes off, flies forward and fires 1 s; the defender's rocket destroys the swoop (80 health) and the AT-ST (set to
	# 100 health), their explosions 4.5 and 2.45 s later; one server each, the longest about 6 minutes
	local korriban="mp/siege_korriban|7|Jedi:red Reborn:blue"
	echo "vehicle_swoop_ride|$korriban||vehicle 48 swoop_mp ride 256 rep=4000"
	echo "vehicle_atst_ride|$korriban||vehicle 48 atst_vehicle ride 512 rep=5000"
	echo "vehicle_xwing_ride|$korriban||vehicle 48 x-wing ride 768 rep=5000"
	echo "vehicle_swoop_destroy|$korriban||vehicle 48 swoop_mp destroy 384 rep=7000"
	echo "vehicle_atst_destroy|$korriban||vehicle 48 atst_vehicle destroy 640 rep=6000"
	# step 20, the NPCs (part 8), in free for all with one bot: the NPC spawned at every reset in the defender's place,
	# with its own AI, which finds the bot itself; the bot stands with its saber (npc), or plays its script against the
	# NPC (npc=), its view following it. The rancor roars 10.3 s at the first sight of the bot (BOTH_STAND1TO2), then
	# attacks it (smash, grab, bite, meal): repetitions of 16 s, 180 units away, within its reach; during the roar the
	# bot's rocket and strong attack hit it. The wampa roars too, then slashes; the trooper fires its blaster at the bot
	# with its saber, then holding the bryar, which it fires back; the bot grips it and pushes it; the reborn attacks
	# with its saber, the bot attacks it and pushes it
	local npc="mp/ffa3|0|Jedi"
	echo "npc_rancor|$npc||npc 48 rancor 180 rep=16000"
	echo "npc_rancor_hits|$npc|$wd|weapon 48 11 0 256 npc=rancor rep=3000;saber 48 strong idle 112 npc=rancor rep=3000"
	echo "npc_wampa|$npc||npc 48 wampa 128 rep=8000"
	echo "npc_wampa_hits|$npc|$force|saber 48 strong idle 64 npc=wampa rep=6000;force 48 lightning 3 128 npc=wampa rep=6000"
	echo "npc_trooper|$npc|$wd|npc 48 stormtrooper 128 rep=6000;weapon 48 4 0 256 npc=stormtrooper rep=6000"
	echo "npc_trooper_force|$npc|$force|force 48 grip 3 128 npc=stormtrooper rep=4000;force 48 push 3 128 npc=stormtrooper rep=4000"
	echo "npc_reborn|$npc||npc 48 reborn 64 rep=10000"
	echo "npc_reborn_hits|$npc|$force|saber 48 medium idle 64 npc=reborn rep=6000;force 48 push 3 128 npc=reborn rep=4000"
}

one() {
	local name map gametype bots pre scn fp
	IFS='|' read -r name map gametype bots pre scn fp <<< "$1"
	if [ "$WINDOWS" = 1 ]; then
		SEED=1 FAST=1 MAP="$map" GAMETYPE="$gametype" BOTS="$bots" PREMAP="$pre" SCENARIO="$scn" FREEPLAY="$fp" \
			bash "$HERE/gamestats_windows.sh" 1 "${PREFIX}_${name}|$MODULE" > /dev/null 2>&1
	else
		SEED=1 FAST=1 MAP="$map" GAMETYPE="$gametype" BOTS="$bots" PREMAP="$pre" SCENARIO="$scn" FREEPLAY="$fp" \
			bash "$HERE/gamestats.sh" "$DED" "$BASE" "$MODULE" 1 "${PREFIX}_${name}" > /dev/null 2>&1
	fi
	echo "done $name"
}
export -f one
export PREFIX MODULE HERE WINDOWS DED="${DED:-}" BASE="${BASE:-}"

# the servers that play longest first (their scenarios' repetitions and the uncounted first one, of rep= ms each, 2000 by
# default), so that the last ones to start are short
cases | awk -F'|' '{ n = split( $6, s, ";" ); t = 0
	for ( i = 1; i <= n; i++ ) { split( s[i], w, " " ); r = 2000; if ( match( s[i], /rep=[0-9]+/ ) ) r = substr( s[i], RSTART + 4, RLENGTH - 4 ); t += ( w[2] + 1 ) * r }
	print t "\t" $0 }' | sort -s -rn -k1,1 | cut -f2- | xargs -P "$PAR" -I{} bash -c 'one "$@"' _ {}
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
echo "time: $(( SECONDS / 60 )) min $(( SECONDS % 60 )) s"	# since the script started (bash's SECONDS)

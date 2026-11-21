#!/bin/bash
# Plan step 20 part 9: where each shot starts against the view (surveys/shot-offsets.md); since part 10b the patch applies
# to the fix (the options aim= and at= and the hit location statistics are the game module's since part 10a), and its
# flash poses the gun's model as the server poses the player's. The measuring code is not in the game: apply
# shot_offsets.patch, build the game module, run this script, then restore the code:
#   git -C /e/Git/jka/ModernJK apply docs/modernization/tools/gamestats/shot_offsets.patch
#   ninja -C /e/Git/jka/ModernJK/build-netcodec/modernjk-clang jampgamex86_64
#   bash /e/Git/jka/ModernJK/docs/modernization/tools/gamestats/shot_offsets.sh
#   git -C /e/Git/jka/ModernJK checkout -- codemp/game
# Each case runs on its own server (gamestats_windows.sh, SEED=1, FAST=1); for each it prints the scenario's damage and
# shot lines, the hits by location (its hit location statistics line) and the shots' lines (DBG shot, the most frequent
# first). The servers' logs are kept as $GAMESTATS_DIR/shot_<batch>_<case>.log.
set -u
G=$(cd "$(dirname "$0")" && pwd)
DLL=${DLL:-/e/Git/jka/ModernJK/build-netcodec/modernjk-clang/jampgamex86_64.dll}
D="${GAMESTATS_DIR:-C:/Users/Yberion/AppData/Local/Temp/gamestats}"	# as gamestats_windows.sh

run() { # <batch> <case>...: a case is <name>|<map>|<g_gametype>|<bots>|<cvars before the map>|<scenarios>
	local tag=$1; shift
	for c in "$@"; do
		IFS='|' read -r name map gt bots pre scn <<< "$c"
		SEED=1 FAST=1 MAP="$map" GAMETYPE="$gt" BOTS="$bots" PREMAP="$pre" SCENARIO="$scn" \
			bash "$G/gamestats_windows.sh" 1 "${tag}_$name|$DLL" > /dev/null 2>&1 &
	done
	wait
	for c in "$@"; do
		IFS='|' read -r name _ <<< "$c"
		cp "$D/gs_${tag}_$name/base/qconsole.log" "$D/shot_${tag}_$name.log"
		echo "=== $name"
		grep -a "^.\{20\}\(scenario \|damage statistics\|shot statistics\|hit location statistics\|DBG eweb\)" "$D/shot_${tag}_$name.log" \
			| cut -c21- | sed -E 's/damage statistics [^:]*: /dmg: /; s/shot statistics[^:]*: /shot: /; s/hit location statistics[^:]*: /hits: /' \
			| uniq
		grep -a "DBG shot" "$D/shot_${tag}_$name.log" | cut -c21- | sed -E 's/^DBG shot rep[0-9]+ t[0-9]+ //' | sort | uniq -c \
			| sort -rn | head -4
	done
}

ffa="mp/ffa3|0|Jedi Reborn" P="g_weaponDisable 0;g_knockback 0" F="g_forcePowerDisable 0;g_knockback 0"
kor="mp/siege_korriban|7|Jedi:red Reborn:blue" npc="mp/ffa3|0|Jedi"

# a: each weapon's head aimed at from the eyes (a bot, an NPC, a player with cg_dynamicCrosshair 0) and from the
# muzzle (the default crosshair), 256 units away, without the spread; the stun baton and the melee at 40 units
a=()
for w in "4 0" "5 0" "5 1" "6 0" "7 0 hold=100" "8 0" "8 1" "9 0" "10 0" "11 0" "15 0" "15 1" "16 0"; do
	n=$(echo "$w" | cut -d' ' -f1-2 | tr ' ' _)
	for aim in eyes muzzle; do a+=("w${n}_$aim|$ffa|$P|weapon 8 $w 256 aim=$aim nospread"); done
done
a+=("w5_0_chest_eyes|$ffa|$P|weapon 8 5 0 256 aim=eyes at=chest nospread"
	"w1_0_eyes|$ffa|$P|weapon 8 1 0 40 aim=eyes nospread"
	"w2_0_eyes|$ffa|$P|weapon 8 2 0 40 aim=eyes nospread"
	"w12_0|$ffa|$P|weapon 8 12 0 256 hold=450 nospread flash")
run a "${a[@]}"

# b: the other shooters, the spread kept, and the gun's *flash bolt in third person for every weapon
b=("emplaced|$ffa|$P|weapon 8 17 0 nospread"
	"turretg2|$ffa|$P|turret 8 g2 nospread"
	"turrethoth|$ffa|$P|turret 8 hoth nospread"
	"seeker|$ffa|$P|holdable 8 seeker 256 rep=4000 nospread"
	"sentry|$ffa|$P|holdable 8 sentry 256 rep=4000 nospread"
	"eweb|$ffa|$P|holdable 8 eweb 256 rep=4000 nospread"
	"eweb_eyes_chest|$ffa|$P|holdable 8 eweb 256 rep=4000 aim=eyes at=chest nospread"
	"swoop|$kor|g_knockback 0|vehicle 8 swoop_mp ride 256 rep=4000 nospread"
	"atst|$kor|g_knockback 0|vehicle 8 atst_vehicle ride 512 rep=5000 nospread"
	"xwing|$kor|g_knockback 0|vehicle 8 x-wing ride 768 rep=5000 nospread"
	"trooper_saber|$npc|$P|npc 8 stormtrooper 128 rep=6000 nospread"
	"trooper_bryar|$npc|$P|weapon 8 4 0 256 npc=stormtrooper rep=6000 nospread"
	"throw1|$ffa|$F|force 8 saberthrow 1 nospread"
	"throw2|$ffa|$F|force 8 saberthrow 2 nospread"
	"throw3|$ffa|$F|force 8 saberthrow 3 nospread"
	"sp_w5_1|$ffa|$P|weapon 8 5 1 256 aim=muzzle"
	"sp_w8_0|$ffa|$P|weapon 8 8 0 256 aim=muzzle"
	"sp_w10_0|$ffa|$P|weapon 8 10 0 256 aim=muzzle"
	"sp_w7_0|$ffa|$P|weapon 8 7 0 256 aim=muzzle")
for w in 4 5 6 7 8 9 10 11 12 13 14 15 16; do b+=("fl_w$w|$ffa|$P|weapon 4 $w 0 256 aim=muzzle nospread flash"); done
run b "${b[@]}"

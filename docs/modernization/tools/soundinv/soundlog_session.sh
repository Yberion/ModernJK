#!/bin/bash
# A silent multiplayer client session that writes the sound log (the SNDLOG lines of soundlog.patch, while s_log is
# set): a listen server on mp/ffa3, its bots, the client a spectator following the first bot, whose ears are the
# listener's. Silent: s_volume, s_musicvolume and s_volumeVoice 0 (the sound is loaded and mixed, nothing is heard; the
# voices follow s_volumeVoice alone, not s_volume).
#
# usage: soundlog_session.sh <bin folder> <work folder> <scenarios|freeplay|crowd> [com_maxfps, 125]
#   scenarios: two bots, the attacker followed: its rockets fired 600 units from the defender (the missile's looping
#              sound), its rocket launcher's lock held 1.4 s on the defender 900 units away (mp/ffa3 has no room for
#              more), the disruptor's zoom
#   freeplay:  ten bots with every weapon and force power, 90 s of free play
#   crowd:     fifteen bots likewise on a smaller map (MAP, mp/duel1), 90 s
# The log is <work folder>/base/qconsole.log; soundlog.py reads it.
set -e
BIN=$1
R=$2
KIND=$3
FPS=${4:-125}
GAME="E:/Jeux/Jedi Academy default - ModernJK"
rm -rf "$R"
mkdir -p "$R/base"
cp "$BIN/cgamex86_64.dll" "$BIN/jampgamex86_64.dll" "$BIN/uix86_64.dll" "$R/base/"
: > "$R/base/modernjk.cfg"
{
	echo "set sv_maxclients 16"
	echo "set g_gametype 0"
	echo "set fraglimit 0"
	echo "set timelimit 0"
	echo "set g_saberLocking 0"
	echo "set g_privateDuel 0"
	echo "set com_maxfps $FPS"
	echo "set g_weaponDisable 0"
	echo "set g_forcePowerDisable 0"
	echo "wait 100"
	echo "devmap ${MAP:-$([ "$KIND" = crowd ] && echo mp/duel1 || echo mp/ffa3)}"
	echo "wait 200"
	if [ "$KIND" = scenarios ]; then
		echo "addbot Jedi 4"
		echo "addbot Reborn 4"
	else
		BOTS="Jedi Reborn Tavion Desann Alora Cultist Chiss Rodian Noghri Weequay"
		[ "$KIND" = crowd ] && BOTS="$BOTS Trandoshan Stormtrooper Shadowtrooper Mercenary Reelo"
		for b in $BOTS; do echo "addbot $b 4"; done
	fi
	echo "wait 300"
	echo "team spectator"
	echo "wait 100"
	echo "follow 1"
	echo "wait 100"
	echo "s_log 1"
	if [ "$KIND" = scenarios ]; then
		# 6 repetitions of 3 s each and the uncounted first: 21 s; wait counts the command buffer's runs, two per frame
		echo "scenario weapon 6 11 0 600 rep=3000"
		echo "wait $(( 46 * FPS ))"
		echo "scenario weapon 6 11 1 900 hold=1400 rep=3000"
		echo "wait $(( 46 * FPS ))"
		echo "scenario weapon 6 6 1 600 hold=1000 rep=3000"
		echo "wait $(( 46 * FPS ))"
		echo "scenario off"
	else
		echo "wait $(( 180 * FPS ))"
	fi
	echo "s_log 0"
	echo "wait 50"
	echo "quit"
} > "$R/base/session.cfg"
cd "$BIN"
export PATH="/c/Logiciels/llvm-mingw-20261104-ucrt-x86_64/bin:$PATH"
timeout 600 ./modernjk.x86_64.exe +set fs_basepath "$GAME" +set fs_homepath "$R" +set s_volume 0 +set s_musicvolume 0 +set s_volumeVoice 0 \
	+set logfile 2 +set r_fullscreen 0 +set r_mode 3 +set developer 0 +exec session.cfg > "$R/stdout.txt" 2>&1 || true
grep -c "SNDLOG" "$R/base/qconsole.log" || true

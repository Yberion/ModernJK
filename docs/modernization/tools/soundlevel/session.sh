#!/bin/bash
# A silent multiplayer session whose mix is written to a file instead of the sound card, for loudness.py: the new build
# through OpenAL Soft's wave writer (a client built with BACKEND_WAVE, see below), the old build (the old mixer)
# through SDL's disk driver; the sound log on (s_show 1 on the new build, s_log 1 on an old build with soundlog.patch).
# The client is a spectator; 60 s are recorded after 20 s of settling.
#   ambience  mp/ffa3 without bots, at the spectators' point
#   duel      two bots on mp/duel1 in free for all, the client following the first
#   crowd     fifteen bots on mp/duel1 in free for all, the client following the first
#   hums      the same fifteen in one team of a team game (they do not fight: mostly the sabers' hums)
# The force is off in every session (g_forcePowerDisable 262143), the bots' runs differing less without it.
# Settings (the client's cvars): VOL (s_volume, default 1), VOICE (s_volumeVoice, 1), MUSIC (s_musicvolume, 0); SEED
# (g_debugSeed, 7). The default settings are VOL=0.79 VOICE=1 MUSIC=0.63.
# usage: session.sh <old|new> <ambience|duel|crowd|hums> <bin folder> <work folder>
# The new build with the wave writer: a throwaway worktree of the repository whose Dependencies.cmake no longer lists
# BACKEND_WAVE among OpenAL Soft's options turned off, built with the windows-clang-release preset (its target
# modernjk), the modules copied from the repository's build; OpenAL Soft writes 32-bit float, so the peaks above full
# scale are kept.
set -e
KIND=$1
SESSION=$2
BIN=$3
R=$4
GAME="E:/Jeux/Jedi Academy default - ModernJK"
LOGCVAR=s_show
[ "$KIND" = old ] && LOGCVAR=s_log
MAP=mp/duel1
GAMETYPE=0
BOTS=""
TEAM=""
FOLLOW=1
case "$SESSION" in
ambience) MAP=mp/ffa3; FOLLOW=0 ;;
duel) BOTS="Jedi Reborn" ;;
crowd) BOTS="Jedi Reborn Tavion Desann Alora Cultist Chiss Rodian Noghri Weequay Trandoshan Stormtrooper Shadowtrooper Mercenary Reelo" ;;
hums) BOTS="Jedi Reborn Tavion Desann Alora Cultist Chiss Rodian Noghri Weequay Trandoshan Stormtrooper Shadowtrooper Mercenary Reelo"; GAMETYPE=6; TEAM=" red" ;;
*) echo "unknown session $SESSION"; exit 1 ;;
esac
rm -rf "$R"
mkdir -p "$R/base"
cp "$BIN/cgamex86_64.dll" "$BIN/jampgamex86_64.dll" "$BIN/uix86_64.dll" "$R/base/"
: > "$R/base/modernjk.cfg" # an empty configuration in the home path, so that the game folder's is not read
{
	echo "set sv_maxclients 16"
	echo "set g_gametype $GAMETYPE"
	echo "set fraglimit 0"
	echo "set timelimit 0"
	echo "set g_saberLocking 0"
	echo "set g_privateDuel 0"
	echo "set com_maxfps 125"
	echo "set g_weaponDisable 0"
	echo "set g_forcePowerDisable 262143"
	echo "set g_debugSeed ${SEED:-7}"
	echo "wait 100"
	echo "devmap $MAP"
	echo "wait 1000"
	for b in $BOTS; do echo "addbot $b 4$TEAM"; done
	echo "wait 500"
	echo "team spectator"
	echo "wait 100"
	[ "$FOLLOW" = 1 ] && echo "follow 1"
	echo "wait 2500"
	echo "$LOGCVAR 1"
	echo "wait $((120 * 60))"
	echo "$LOGCVAR 0"
	echo "wait 50"
	echo "quit"
} > "$R/base/session.cfg"
if [ "$KIND" = old ]; then
	export SDL_AUDIODRIVER=disk
	export SDL_DISKAUDIOFILE="$(cygpath -w "$R")\\out.raw"
else
	printf '[general]\ndrivers = wave\n[wave]\nfile = %s\n' "$(cygpath -m "$R")/out.wav" > "$R/alsoft.ini"
	export ALSOFT_CONF="$(cygpath -w "$R")\\alsoft.ini"
	export ALSOFT_DRIVERS=wave
fi
export PATH="/c/Logiciels/llvm-mingw-20261104-ucrt-x86_64/bin:$PATH"
cd "$BIN"
timeout 400 ./modernjk.x86_64.exe +set fs_basepath "$GAME" +set fs_homepath "$R" +set s_volume "${VOL:-1}" \
	+set s_musicvolume "${MUSIC:-0}" +set s_volumeVoice "${VOICE:-1}" +set s_muteWhenUnfocused 0 +set logfile 2 \
	+set r_fullscreen 0 +set r_mode 3 +exec session.cfg > "$R/stdout.txt" 2>&1 || true
ls -la "$R"/out.*

#!/bin/bash
# A silent multiplayer session whose mix is written to a file instead of the sound card, for loudness.py: the new build
# through OpenAL Soft's wave writer (a client built with BACKEND_WAVE, see below), the old build (the old mixer)
# through SDL's disk driver; the sound log on (s_show 1 on the new build, s_log 1 on an old build with soundlog.patch).
# The client is a spectator; 60 s are measured after 20 s of settling. The recording starts when the device opens;
# at its end the script reads in the console's log (its times, to the second) how far into the recording the measure
# starts, and prints it for loudness.py's --from.
#   ambience  mp/ffa3 without bots, at the spectators' point
#   duel      two bots on mp/duel1 in free for all, the client following the first
#   crowd     fifteen bots on mp/duel1 in free for all, the client following the first
#   hums      the same fifteen in one team of a team game (they do not fight: mostly the sabers' hums)
#   hall      the same fifteen on mp/ffa3 in free for all, the client fixed at the centre of the TIE fighter's hall (the
#             "Center" point of its zone file's environment "Tie Fighter Hall", a reverb of 3.1 s)
#   ffa3duel  two bots on mp/ffa3 in free for all, the client following the first
# The force is off in every session (g_forcePowerDisable 262143), the bots' runs differing less without it.
# Settings (the client's cvars): VOL (s_volume, default 1), VOICE (s_volumeVoice, 1), MUSIC (s_musicvolume, 0), REVERB
# (s_reverb, 1); SEED (com_debugSeed, 7). The default settings are VOL=0.79 VOICE=1 MUSIC=0.63 REVERB=1. The bin folder's
# z_ModernJK.pk3, when it has one, is put in the session's home folder: the zone files and the menus of the build
# measured, not those installed in the game's folder.
# usage: session.sh <old|new> <ambience|duel|crowd|hums|hall|ffa3duel> <bin folder> <work folder>
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
OPENED="sound device:" # the console's line printed when the recording starts
if [ "$KIND" = old ]; then
	LOGCVAR=s_log
	OPENED="Starting SDL audio callback..."
fi
MAP=mp/duel1
GAMETYPE=0
BOTS=""
TEAM=""
FOLLOW=1
VIEWPOS=""
CROWD="Jedi Reborn Tavion Desann Alora Cultist Chiss Rodian Noghri Weequay Trandoshan Stormtrooper Shadowtrooper Mercenary Reelo"
case "$SESSION" in
ambience) MAP=mp/ffa3; FOLLOW=0 ;;
duel) BOTS="Jedi Reborn" ;;
crowd) BOTS="$CROWD" ;;
hums) BOTS="$CROWD"; GAMETYPE=6; TEAM=" red" ;;
hall) MAP=mp/ffa3; BOTS="$CROWD"; FOLLOW=0; VIEWPOS="-631 756 95 0" ;;
ffa3duel) MAP=mp/ffa3; BOTS="Jedi Reborn" ;;
*) echo "unknown session $SESSION"; exit 1 ;;
esac
rm -rf "$R"
mkdir -p "$R/base"
cp "$BIN/cgamex86_64.dll" "$BIN/jampgamex86_64.dll" "$BIN/uix86_64.dll" "$R/base/"
[ -f "$BIN/z_ModernJK.pk3" ] && cp "$BIN/z_ModernJK.pk3" "$R/base/"
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
	echo "set com_debugSeed ${SEED:-7}"
	# wait counts two per frame (the command buffer runs twice in a frame): 250 a second at 125 frames per second
	echo "wait 200"
	echo "devmap $MAP"
	echo "wait 2000"
	for b in $BOTS; do echo "addbot $b 4$TEAM"; done
	echo "wait 1000"
	echo "team spectator"
	echo "wait 200"
	[ "$FOLLOW" = 1 ] && echo "follow 1"
	[ -n "$VIEWPOS" ] && echo "setviewpos $VIEWPOS"
	echo "wait $((250 * 20))"
	echo "soundinfo"
	echo "echo loudness: the measure starts"
	echo "$LOGCVAR 1"
	echo "wait $((250 * 60))"
	echo "$LOGCVAR 0"
	echo "wait 100"
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
	+set s_musicvolume "${MUSIC:-0}" +set s_volumeVoice "${VOICE:-1}" +set s_muteWhenUnfocused 0 +set s_reverb "${REVERB:-1}" +set logfile 2 \
	+set r_fullscreen 0 +set r_mode 3 +exec session.cfg > "$R/stdout.txt" 2>&1 || true
ls -la "$R"/out.*
LOG="$R/base/qconsole.log"
OPENED_AT=$(grep -m1 -F "$OPENED" "$LOG" | cut -c1-19)
STARTS_AT=$(grep -m1 -F "loudness: the measure starts" "$LOG" | cut -c1-19)
[ -n "$OPENED_AT" ] && [ -n "$STARTS_AT" ] || { echo "no recording's or measure's start in $LOG"; exit 1; }
echo "the measure starts $(($(date -d "$STARTS_AT" +%s) - $(date -d "$OPENED_AT" +%s))) s into the recording (loudness.py --from)"

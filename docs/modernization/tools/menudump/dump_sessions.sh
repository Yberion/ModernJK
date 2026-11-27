#!/bin/bash
# The menus' dump of the three games for one build, with menudump_tmpdbg.py's trace inserted and built: silent
# sessions of the multiplayer (its UI's menus at the start, its cgame's HUD menus on mp/duel1), of Jedi Academy's and
# of Jedi Outcast's single player (their menus at the start); each log's TMPDBG lines, the time stamps cut, in
# <out>_mp.dump, <out>_ja.dump, <out>_jo.dump. Two builds' dumps are then compared with diff.
# usage: dump_sessions.sh <bin folder> <out prefix> [scratch folder]
# example: dump_sessions.sh /e/Git/jka/ModernJK/build/windows-clang-release/bin new
#          dump_sessions.sh /e/Git/jka/ModernJK_s3ref/build/windows-clang-release/bin ref
#          for g in mp ja jo; do diff ref_$g.dump new_$g.dump; done
set -e
BIN=$1
OUT=$2
WORK=${3:-$PWD}
export PATH="/c/Logiciels/llvm-mingw-20261104-ucrt-x86_64/bin:$PATH"
JA="E:/Jeux/Jedi Academy default - ModernJK"
JO="E:/Jeux/Star Wars Jedi Knight II - Jedi Outcast"
COMMON='+set s_initsound 0 +set logfile 2 +set r_displayState 0 +set r_windowWidth 1280 +set r_windowHeight 720 +set com_maxfps 125'

# one session: <game> <home> <executable> <commands>
session() {
	local game=$1 home=$2 exe=$3 commands=$4
	printf '%b' "$commands" > "$home/base/session.cfg"
	(cd "$BIN" && timeout 120 "./$exe" +set fs_basepath "$game" +set fs_homepath "$home" $COMMON +exec session.cfg > "$home/stdout.txt" 2>&1) ||
		echo "the client of $exe did not end by itself"
}

rm -rf "$WORK/${OUT}_mp" "$WORK/${OUT}_ja" "$WORK/${OUT}_jo"
mkdir -p "$WORK/${OUT}_mp/base" "$WORK/${OUT}_ja/base" "$WORK/${OUT}_jo/base"
cp "$BIN/cgamex86_64.dll" "$BIN/jampgamex86_64.dll" "$BIN/uix86_64.dll" "$WORK/${OUT}_mp/base/"
: > "$WORK/${OUT}_mp/base/modernjk.cfg"
cp "$BIN/jagamex86_64.dll" "$WORK/${OUT}_ja/base/"
[ -f "$BIN/z_ModernJK.pk3" ] && cp "$BIN/z_ModernJK.pk3" "$WORK/${OUT}_ja/base/"
cp "$BIN/jospgamex86_64.dll" "$WORK/${OUT}_jo/base/"
[ -f "$BIN/jk2/z_ModernJK.pk3" ] && cp "$BIN/jk2/z_ModernJK.pk3" "$WORK/${OUT}_jo/base/"

session "$JA" "$WORK/${OUT}_mp" modernjk.x86_64.exe 'wait 100\ndevmap mp/duel1\nwait 600\nquit\n'
session "$JA" "$WORK/${OUT}_ja" modernjk_sp.x86_64.exe 'wait 200\nquit\n'
session "$JO" "$WORK/${OUT}_jo" openjo_sp.x86_64.exe 'wait 200\nquit\n'

for g in mp ja jo; do
	grep "TMPDBG" "$WORK/${OUT}_$g/base/qconsole.log" | cut -c21- > "$WORK/${OUT}_$g.dump"
	echo "$g: $(wc -l < "$WORK/${OUT}_$g.dump") lines, $(grep -c 'TMPDBG dump after' "$WORK/${OUT}_$g.dump") menu files"
done

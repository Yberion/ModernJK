#!/bin/bash
# TMP: where the bots stand against the layer, for one layer mode (the same game with the same seed)
# usage: coverrun.sh <tag> <map> <layer mode>   (GT=<gametype>)
tag="$1"; map="$2"; mode="$3"
BUILD=E:/Git/jka/ModernJK/build/windows-clang-release/bin
export PATH="/c/Logiciels/llvm-mingw-20261104-ucrt-x86_64/bin:$PATH"
GAME="E:/Jeux/Jedi Academy default - ModernJK"
SCR="${SCR:?set SCR to the work folder: the tools built there, their outputs written there}"
home="$SCR/cov_$tag"; rm -rf "$home"; mkdir -p "$home/base"; cp "$BUILD/jampgamex86_64.dll" "$home/base/"
names=(Alora Chewbacca Cultist Desann Gran Noghri Rodian Tavion)
{
	echo "set sv_maxclients 32"; echo "set g_gametype ${GT:-0}"; echo "set fraglimit 0"; echo "set timelimit 0"; echo "set capturelimit 0"
	echo "set g_weaponDisable 524279"; echo "set g_forcePowerDisable 163837"; echo "set g_saberLocking 0"; echo "set g_privateDuel 0"
	echo "set g_debugSeed 5"; echo "set fixedtime 10"; echo "set timedemo 1"
	echo "map $map"; echo "wait 100"
	echo "TMPvisprep $mode"
	for (( i = 0; i < 31; i++ )); do echo "addbot ${names[$(( i % 8 ))]} 4"; done
	echo "set fixedtime 10"; echo "set timedemo 1"; echo "wait 1000"
	for (( i = 0; i < 120; i++ )); do echo "TMPviscover"; echo "wait 100"; done
	echo "TMPviscover print"; echo "quit"
} > "$home/base/c.cfg"
( cd "$BUILD" && lldb -b -o run -- ./modernjkded.x86_64.exe +set fs_basepath "$GAME" +set fs_homepath "$home" +set net_enabled 0 +set dedicated 1 +set logfile 2 +exec c.cfg ) > "$home/lldb.txt" 2>&1
grep -a "TMPVIS" "$home/base/qconsole.log" | sed 's/^[0-9-]* [0-9:]* //'

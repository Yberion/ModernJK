#!/bin/bash
# TMP visibility prototype run: one local dedicated server, the table built, checked, then measured with bots
# usage: visrun.sh <tag> <map> <build args> [bots] [samples] [lead ms]
tag="$1"; map="$2"; build="$3"; bots="${4:-31}"; samples="${5:-60}"; lead="${6:-110}"
BUILD=E:/Git/jka/ModernJK/build-netcodec/modernjk-clang
export PATH="/c/Logiciels/llvm-mingw-20261104-ucrt-x86_64/bin:$PATH"
GAME="E:/Jeux/Jedi Academy default - ModernJK"
SCR="${SCR:?set SCR to the work folder: the tools built there, their outputs written there}"
home="$SCR/run_$tag"
rm -rf "$home"; mkdir -p "$home/base"
cp "$BUILD/jampgamex86_64.dll" "$home/base/"
names=(Alora Chewbacca Cultist Desann Gran Noghri Rodian Tavion)
{
	echo "set sv_maxclients 32"
	echo "set g_gametype ${GT:-0}"
	echo "set fraglimit 0"
	echo "set timelimit 0"
	echo "set g_weaponDisable 524279"
	echo "set g_forcePowerDisable 163837"
	echo "set g_saberLocking 0"
	echo "set g_privateDuel 0"
	echo "map $map"
	echo "wait 100"
	echo "TMPvisbuild $build"
	echo "TMPvischeck 200000"
	if [ "$bots" -gt 0 ]; then
		for (( i = 0; i < bots; i++ )); do echo "addbot ${names[$(( i % 8 ))]} 4"; done
		echo "set fixedtime 10"; echo "set timedemo 1"
		echo "wait 1000"
		for (( i = 0; i < samples; i++ )); do echo "TMPvisgame $lead"; echo "wait 100"; done
		echo "TMPvisgame print"
	fi
	echo "quit"
} > "$home/base/vis.cfg"
( cd "$BUILD" && lldb -b -o run -- ./modernjkded.x86_64.exe \
	+set fs_basepath "$GAME" +set fs_homepath "$home" +set net_enabled 0 +set dedicated 1 +set logfile 2 +exec vis.cfg ) > "$home/lldb.txt" 2>&1
grep -a "TMPVIS" "$home/base/qconsole.log"

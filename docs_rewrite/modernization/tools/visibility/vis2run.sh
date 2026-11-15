#!/bin/bash
# TMP layer visibility table: build, then measure with bots
# usage: vis2run.sh <tag> <map> <build args> [bots] [samples]
tag="$1"; map="$2"; build="$3"; bots="${4:-31}"; samples="${5:-60}"
BUILD=E:/Git/jka/ModernJK/build-netcodec/modernjk-clang
export PATH="/c/Logiciels/llvm-mingw-20261104-ucrt-x86_64/bin:$PATH"
GAME="E:/Jeux/Jedi Academy default - ModernJK"
SCR="${SCR:?set SCR to the work folder: the tools built there, their outputs written there}"
home="$SCR/v2_$tag"
rm -rf "$home"; mkdir -p "$home/base"
cp "$BUILD/jampgamex86_64.dll" "$home/base/"
names=(Alora Chewbacca Cultist Desann Gran Noghri Rodian Tavion)
{
	echo "set sv_maxclients 32"; echo "set g_gametype ${GT:-0}"; echo "set fraglimit 0"; echo "set timelimit 0"; echo "set capturelimit 0"
	echo "set g_weaponDisable 524279"; echo "set g_forcePowerDisable 163837"; echo "set g_saberLocking 0"; echo "set g_privateDuel 0"
	echo "map $map"; echo "wait 100"
	echo "TMPvis2build $build"
	if [ "$bots" -gt 0 ]; then
		for (( i = 0; i < bots; i++ )); do echo "addbot ${names[$(( i % 8 ))]} 4"; done
		echo "set fixedtime 10"; echo "set timedemo 1"; echo "wait 1000"
		for (( i = 0; i < samples; i++ )); do echo "TMPvis2game ${LEAD:-110}"; echo "wait 100"; done
		echo "TMPvis2game print"
	fi
	echo "quit"
} > "$home/base/v.cfg"
( cd "$BUILD" && lldb -b -o run -- ./modernjkded.x86_64.exe \
	+set fs_basepath "$GAME" +set fs_homepath "$home" +set net_enabled 0 +set dedicated 1 +set logfile 2 +exec v.cfg ) > "$home/lldb.txt" 2>&1
grep -a "TMPVIS2" "$home/base/qconsole.log" | sed 's/^[0-9-]* [0-9:]* TMPVIS2 //'

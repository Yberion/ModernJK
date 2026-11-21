#!/bin/bash
# TMP: export cells from the server, compute the table offline (vistool, tinybvh), measure it with bots
# usage: vis4run.sh <tag> <map> <vistool mode and arguments...>   (GT=<gametype>, BOTS, SAMPLES)
tag="$1"; map="$2"; shift 2
BUILD=E:/Git/jka/ModernJK/build/windows-clang-release/bin
export PATH="/c/Logiciels/llvm-mingw-20261104-ucrt-x86_64/bin:$PATH"
GAME="E:/Jeux/Jedi Academy default - ModernJK"
SCR="${SCR:?set SCR to the work folder: the tools built there, their outputs written there}"
name="${map##*/}"
[ -f "$SCR/$name.tris" ] || python -I "$SCR/bsptris.py" "$map" "$SCR/$name.tris" > /dev/null
exp="$SCR/$name.export"
if [ ! -f "$exp" ]; then
	home="$SCR/v4_${tag}_exp"; rm -rf "$home"; mkdir -p "$home/base"; cp "$BUILD/jampgamex86_64.dll" "$home/base/"
	{ echo "set g_gametype ${GT:-0}"; echo "map $map"; echo "wait 100"; echo "TMPvisexport 64 128 2 32 $exp"; echo "quit"; } > "$home/base/e.cfg"
	( cd "$BUILD" && lldb -b -o run -- ./modernjkded.x86_64.exe +set fs_basepath "$GAME" +set fs_homepath "$home" +set net_enabled 0 +set dedicated 1 +set logfile 2 +exec e.cfg ) > "$home/lldb.txt" 2>&1
	grep -a "TMPVIS export" "$home/base/qconsole.log" | sed 's/^[0-9-]* [0-9:]* //'
fi
prefix="$SCR/v4_${tag}_table"
"$SCR/vistool.exe" "$SCR/$name.tris" "$exp" "$prefix" "$@"
home="$SCR/v4_${tag}_game"; rm -rf "$home"; mkdir -p "$home/base"; cp "$BUILD/jampgamex86_64.dll" "$home/base/"
names=(Alora Chewbacca Cultist Desann Gran Noghri Rodian Tavion)
bots="${BOTS:-31}"
{
	echo "set sv_maxclients 32"; echo "set g_gametype ${GT:-0}"; echo "set fraglimit 0"; echo "set timelimit 0"; echo "set capturelimit 0"
	echo "set g_weaponDisable 524279"; echo "set g_forcePowerDisable 163837"; echo "set g_saberLocking 0"; echo "set g_privateDuel 0"
	echo "map $map"; echo "wait 100"
	echo "TMPvis3load 64 128 2 1 $prefix"
	for (( i = 0; i < bots; i++ )); do echo "addbot ${names[$(( i % 8 ))]} 4"; done
	echo "set fixedtime 10"; echo "set timedemo 1"; echo "wait 1000"
	echo "TMPvisgamedump $SCR/v4_${tag}_pairs.bin"
	for (( i = 0; i < ${SAMPLES:-60}; i++ )); do echo "TMPvis2game ${LEAD:-110}"; echo "wait 100"; done
	echo "TMPvisgamedump stop"; echo "TMPvis2game print"; echo "quit"
} > "$home/base/g.cfg"
( cd "$BUILD" && lldb -b -o run -- ./modernjkded.x86_64.exe +set fs_basepath "$GAME" +set fs_homepath "$home" +set net_enabled 0 +set dedicated 1 +set logfile 2 +exec g.cfg ) > "$home/lldb.txt" 2>&1
grep -a "TMPVIS" "$home/base/qconsole.log" | sed 's/^[0-9-]* [0-9:]* //'
"$SCR/truth.exe" "$SCR/$name.tris" "$SCR/v4_${tag}_pairs.bin"

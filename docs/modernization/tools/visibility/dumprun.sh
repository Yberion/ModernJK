#!/bin/bash
# TMP: record rays of the option b table (one slice) with the engine's verdicts, then time the engine's tracer on them
# usage: dumprun.sh <map> <out rays file> [slices] [one ray in n]
map="$1"; out="$2"; K="${3:-64}"; every="${4:-8}"
BUILD=E:/Git/jka/ModernJK/build/windows-clang-release/bin
export PATH="/c/Logiciels/llvm-mingw-20261104-ucrt-x86_64/bin:$PATH"
GAME="E:/Jeux/Jedi Academy default - ModernJK"
SCR="${SCR:?set SCR to the work folder: the tools built there, their outputs written there}"
home="$SCR/dump_${map//\//_}"
rm -rf "$home"; mkdir -p "$home/base"
cp "$BUILD/jampgamex86_64.dll" "$home/base/"
{
	echo "set g_gametype ${GT:-0}"; echo "map $map"; echo "wait 100"
	echo "TMPvisdump $out $every"
	echo "TMPvis3slice 64 128 2 32 $K 0 $home/slice.bin"
	echo "TMPvisdump stop"
	echo "TMPvisreplay $out"
	echo "quit"
} > "$home/base/d.cfg"
( cd "$BUILD" && lldb -b -o run -- ./modernjkded.x86_64.exe \
	+set fs_basepath "$GAME" +set fs_homepath "$home" +set net_enabled 0 +set dedicated 1 +set logfile 2 +exec d.cfg ) > "$home/lldb.txt" 2>&1
grep -a "TMPVIS" "$home/base/qconsole.log" | sed 's/^[0-9-]* [0-9:]* //'

#!/bin/bash
# TMP option b: the camera rows computed by K server processes, then measured with bots
# usage: vis3run.sh <tag> <map> <footprint> <air> <spots> <directions> <K> [bots] [samples]
tag="$1"; map="$2"; fp="$3"; air="$4"; spots="$5"; dirs="$6"; K="$7"; bots="${8:-31}"; samples="${9:-60}"
BUILD=E:/Git/jka/ModernJK/build/windows-clang-release/bin
export PATH="/c/Logiciels/llvm-mingw-20261104-ucrt-x86_64/bin:$PATH"
GAME="E:/Jeux/Jedi Academy default - ModernJK"
SCR="${SCR:?set SCR to the work folder: the tools built there, their outputs written there}"
prefix="$SCR/v3_${tag}_slice"
start=$(date +%s)
for (( k = 0; k < K; k++ )); do
	home="$SCR/v3_${tag}_s$k"
	rm -rf "$home"; mkdir -p "$home/base"
	cp "$BUILD/jampgamex86_64.dll" "$home/base/"
	{ echo "set g_gametype ${GT:-0}"; echo "map $map"; echo "wait 100"; echo "TMPvis3slice $fp $air $spots $dirs $K $k $prefix$k.bin"; echo "quit"; } > "$home/base/s.cfg"
	( cd "$BUILD" && lldb -b -o run -- ./modernjkded.x86_64.exe \
		+set fs_basepath "$GAME" +set fs_homepath "$home" +set net_enabled 0 +set dedicated 1 +set logfile 2 +exec s.cfg ) > "$home/lldb.txt" 2>&1 &
done
wait
echo "slices done in $(( $(date +%s) - start )) s"
for (( k = 0; k < K; k++ )); do grep -a "TMPVIS3 slice" "$SCR/v3_${tag}_s$k/base/qconsole.log" | sed 's/^[0-9-]* [0-9:]* //'; done | sort -t, -k1 | head -3
home="$SCR/v3_${tag}_game"
rm -rf "$home"; mkdir -p "$home/base"
cp "$BUILD/jampgamex86_64.dll" "$home/base/"
names=(Alora Chewbacca Cultist Desann Gran Noghri Rodian Tavion)
{
	echo "set sv_maxclients 32"; echo "set g_gametype ${GT:-0}"; echo "set fraglimit 0"; echo "set timelimit 0"; echo "set capturelimit 0"
	echo "set g_weaponDisable 524279"; echo "set g_forcePowerDisable 163837"; echo "set g_saberLocking 0"; echo "set g_privateDuel 0"
	echo "map $map"; echo "wait 100"
	echo "TMPvis3load $fp $air $spots $K $prefix"
	if [ "$bots" -gt 0 ]; then
		for (( i = 0; i < bots; i++ )); do echo "addbot ${names[$(( i % 8 ))]} 4"; done
		echo "set fixedtime 10"; echo "set timedemo 1"; echo "wait 1000"
		for (( i = 0; i < samples; i++ )); do echo "TMPvis2game ${LEAD:-110}"; echo "wait 100"; done
		echo "TMPvis2game print"
	fi
	echo "quit"
} > "$home/base/g.cfg"
( cd "$BUILD" && lldb -b -o run -- ./modernjkded.x86_64.exe \
	+set fs_basepath "$GAME" +set fs_homepath "$home" +set net_enabled 0 +set dedicated 1 +set logfile 2 +exec g.cfg ) > "$home/lldb.txt" 2>&1
grep -a "TMPVIS" "$home/base/qconsole.log" | sed 's/^[0-9-]* [0-9:]* //'

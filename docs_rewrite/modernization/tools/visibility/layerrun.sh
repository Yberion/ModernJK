#!/bin/bash
# TMP walkable layer count on each map given
BUILD=E:/Git/jka/ModernJK/build-netcodec/modernjk-clang
export PATH="/c/Logiciels/llvm-mingw-20261104-ucrt-x86_64/bin:$PATH"
GAME="E:/Jeux/Jedi Academy default - netcodev_openjk"
SCR="${SCR:?set SCR to the work folder: the tools built there, their outputs written there}"
for map in "$@"; do
	home="$SCR/layer_${map//\//_}"
	rm -rf "$home"; mkdir -p "$home/base"
	cp "$BUILD/jampgamex86_64.dll" "$home/base/"
	{
		echo "set sv_maxclients 32"; echo "set g_gametype ${GT:-0}"; echo "set fraglimit 0"; echo "set timelimit 0"; echo "set capturelimit 0"
		echo "set g_weaponDisable 524279"; echo "set g_forcePowerDisable 163837"; echo "set g_saberLocking 0"; echo "set g_privateDuel 0"
		echo "map $map"; echo "wait 100"; echo "TMPlayer"
		if [ "${BOTS:-0}" -gt 0 ]; then
			names=(Alora Chewbacca Cultist Desann Gran Noghri Rodian Tavion)
			for (( i = 0; i < BOTS; i++ )); do echo "addbot ${names[$(( i % 8 ))]} 4"; done
			echo "set fixedtime 10"; echo "set timedemo 1"; echo "wait 1000"
			for (( i = 0; i < ${SAMPLES:-300}; i++ )); do echo "TMPlayercheck"; echo "wait 100"; done
			echo "TMPlayercheck print"
		fi
		echo "quit"
	} > "$home/base/l.cfg"
	( cd "$BUILD" && lldb -b -o run -- ./modernjkded.x86_64.exe \
		+set fs_basepath "$GAME" +set fs_homepath "$home" +set net_enabled 0 +set dedicated 1 +set logfile 2 +exec l.cfg ) > "$home/lldb.txt" 2>&1
	echo "== $map"
	grep -a "TMPLAYER" "$home/base/qconsole.log" | sed 's/^[0-9-]* [0-9:]* TMPLAYER //'
done

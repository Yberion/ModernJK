#!/bin/bash
# Windows version of gamestats.sh for the development machine (llvm-mingw clang build, run under lldb: the clang dedicated
# server crashes without a console). Runs local dedicated servers (no network) with 16 saber bots and prints
# gamestats after a measured time. The runs go in $GAMESTATS_DIR/gs_<tag>; GAME is set below, BUILD too unless the
# environment gives another folder (to time two builds of the server against each other).
# Usage: [FAST=0|1] [SEED=<n>] [PREMAP='<cvar> <value>;...'] [SCENARIO=...] gamestats_windows.sh <minutes> '<tag>|<game dll>[|<cvar value>]' ...   (all run in parallel)
# SCENARIO, MAP, GAMETYPE and BOTS as in gamestats.sh and gamestats_cfg.sh (the cfg both scripts write).
# The commands go in a cfg run by +exec: the engine keeps only 32 "+" commands of its command line
# (MAX_CONSOLE_LINES); the measured time is chained in waits of 30 seconds. FAST=1 (default) sets fixedtime and
# timedemo: the server runs its ticks as fast as it can, each advancing the game by 10 ms. SEED=<n>: server n of
# the command line (from 0) gets the game seed SEED+n and, in fast mode, the fixed frame from before the map loads, so
# that every run replays the same game (see gamestats.sh).
# A server that does not exit after its quit (seen in fast mode under lldb) is killed once its statistics are written.
set -u
. "$(dirname "$0")/gamestats_cfg.sh"
MINUTES="$1"
shift
SCRATCH="${GAMESTATS_DIR:-C:/Users/Yberion/AppData/Local/Temp/gamestats}"
mkdir -p "$SCRATCH"
BUILD="${BUILD:-E:/Git/jka/ModernJK/build/windows-clang-release/bin}"	# the folder of the dedicated server
DED_EXE="${DED_EXE:-modernjkded.x86_64.exe}"	# its name (openjkded.x86_64.exe before the project was renamed)
GAME="E:/Jeux/Jedi Academy default - ModernJK"
export PATH="/c/Logiciels/llvm-mingw-20261104-ucrt-x86_64/x86_64-w64-mingw32/bin:/c/Logiciels/llvm-mingw-20261104-ucrt-x86_64/bin:$PATH"

# kills the dedicated server whose command line holds this text (its own fs_homepath)
kill_server() {
	powershell -NoProfile -Command "Get-CimInstance Win32_Process -Filter \"Name='$DED_EXE'\" | Where-Object { \$_.CommandLine -like '*$1*' } | ForEach-Object { Stop-Process -Id \$_.ProcessId -Force }" > /dev/null 2>&1
}

run() {
	local tag="$1" dll="$2" extra="$3" idx="$4"
	local home="$SCRATCH/gs_${tag}"
	rm -rf "$home"
	measure_home "$home"
	cp "$dll" "$home/base/jampgamex86_64.dll"
	measure_cfg "$MINUTES" "$extra" "$idx" 1 > "$home/base/measure.cfg"
	( cd "$BUILD" && lldb -b -o run -- "./$DED_EXE" \
		+set fs_basepath "$GAME" +set fs_homepath "$home" +set net_enabled 0 +set logfile 2 +exec measure.cfg ) > "$home/lldb.txt" 2>&1 &
	local pid=$!
	# watchdog: once the last statistics are written (measure_stats), give the server 20 seconds to quit, then kill it
	# (10 seconds cut the last lines of a busy server's statistics, 18 November 2026); checked every second, so that a
	# server that quits frees its place at once (a fixed wait of 20 seconds kept every place idle that long, 20 November
	# 2026)
	local waited=0
	while kill -0 "$pid" 2> /dev/null; do
		local done_count
		done_count=$(grep -a -c "game statistics" "$home/base/qconsole.log" 2> /dev/null)
		if [ "${done_count:-0}" -ge "$(measure_stats)" ]; then
			if [ "$waited" -ge 20 ]; then
				kill_server "gs_${tag}"
				break
			fi
			waited=$(( waited + 1 ))
		fi
		sleep 1
	done
	wait "$pid" 2> /dev/null
	grep -a "game statistics\|server CPU" "$home/base/qconsole.log" | tail -2 | sed "s/^/[$tag] /"
}

idx=0
for spec in "$@"; do
	IFS='|' read -r tag dll extra <<< "$spec"
	if [ -z "$tag" ] || [ ! -f "$dll" ]; then
		echo "invalid configuration '$spec': expected '<tag>|<game dll>[|<cvars>]'" >&2
		continue
	fi
	run "$tag" "$dll" "${extra:-}" "$idx" &
	idx=$(( idx + 1 ))
done
wait

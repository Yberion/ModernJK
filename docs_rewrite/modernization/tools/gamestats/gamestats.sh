#!/bin/bash
# Combat statistics of the game module with bots, for plan step 14: the game plays at the 100 Hz tick as it did at
# sv_fps 20. Runs local dedicated servers (no network, sabers only, 16 bots on mp/ffa3, or a scripted scenario of two
# bots) in parallel and prints the "gamestats" and "svnetstats" lines of each after the measured time.
#
# Usage: [FAST=1] [SEED=<n>] [PREMAP='<cvar> <value>;...'] [SCENARIO='saber 96 medium idle'] gamestats.sh <modernjkded binary> <fs_basepath> <game module> <minutes> '<tag>[|<cvars>]' ...
#   <fs_basepath>  the directory holding base/assets*.pk3
#   <game module>  the jampgame module to test (jampgamex86_64.so); each run copies it into its own fs_homepath
#   <cvars>        cheat cvars set after devmap, separated by "; set "; pass each configuration as one
#                  argument (quoted): split at its spaces it would lose its cvars. For example
#                  'd_saberSPStyleDamage 0'
#   SCENARIO       scripted scenarios (codemp/game/g_scenario.cpp) instead of the free-for-all, one or several
#                  separated by ";", played one after the other, each with its own gamestats; <minutes> is then
#                  not used (the waits come from the repetitions); MAP, GAMETYPE and BOTS: gamestats_cfg.sh
#   SEED           a first game seed: server n of the command line (from 0) gets SEED+n and, with FAST=1, the fixed frame
#                  from before the map loads, so that every run replays the same game; without it the seed comes from
#                  the clock and the free play from the machine's load
# Example (four servers; FAST=1 on a machine nobody plays on):
#   FAST=1 gamestats.sh /srv/jka/modernjkded.x86_64 /srv/jka /srv/jka/base/jampgamex86_64.so 10 \
#     ra rb rc rd
#   FAST=1 SCENARIO='saber 96 strong attack' gamestats.sh ... 4 s
#
# The commands go in a cfg run by +exec: the engine keeps only 32 "+" commands of its command line
# (MAX_CONSOLE_LINES), and a long "wait" never fires, so the measured time is chained in waits of 30 seconds.
# Each run's files are in ${GAMESTATS_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/gamestats}/<tag>, on the disk: /tmp is often
# in memory, which the runs of every prefix filled (20 November 2026).
set -u
. "$(dirname "$0")/gamestats_cfg.sh"

if [ $# -lt 5 ]; then
	sed -n '2,22p' "$0"
	exit 1
fi

BINARY="$1"
BASEPATH="$2"
MODULE="$3"
MINUTES="$4"
shift 4
OUT="${GAMESTATS_DIR:-${XDG_CACHE_HOME:-$HOME/.cache}/gamestats}"

run() {
	local tag="$1" cvars="$2" idx="$3"
	local home="$OUT/$tag"
	rm -rf "$home"
	measure_home "$home"
	cp "$MODULE" "$home/base/"
	measure_cfg "$MINUTES" "$cvars" "$idx" 0 > "$home/base/measure.cfg"
	"$BINARY" +set fs_basepath "$BASEPATH" +set fs_homepath "$home" +set net_enabled 0 +set logfile 2 \
		+exec measure.cfg < /dev/null > "$home/stdout.txt" 2>&1 &
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
				kill -9 "$pid"
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
	IFS='|' read -r tag cvars <<< "$spec"
	run "$tag" "${cvars:-}" "$idx" &
	idx=$(( idx + 1 ))
done
wait

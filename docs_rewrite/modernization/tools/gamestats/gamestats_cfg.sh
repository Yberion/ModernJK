#!/bin/bash
# The measure.cfg that gamestats.sh and gamestats_windows.sh give the dedicated server (sourced by both). Settings from
# the environment, besides FAST, SEED and PREMAP (described in the two scripts):
#   MAP       the map (mp/ffa3)
#   GAMETYPE  g_gametype (0, free for all)
#   BOTS      the bots of a scenario, "<name>[:<team>] ..." ("Jedi Reborn"); in a team game the first bot is the
#             attacker, the first that is not its teammate the defender, a teammate the ally (g_scenario.cpp)
#   SCENARIO  one scenario, or several separated by ";" played one after the other on the same server, each with its own
#             statistics (gamestats_scn.py names them from the cfg); the waits are computed from their repetitions
#   FREEPLAY  the free play before the scenarios, in ticks of 10 ms (3000, 30 s); shorter in a power duel, where the
#             lone duelist's death in the free play ends the round and the game stays in its intermission

# measure_home <fs_homepath of the run>: the dedicated server executes the archived settings of its configuration file,
# found in fs_homepath or else in fs_basepath, the game's folder, whose settings differ between machines (found on
# 19 November 2026: g_forceRegenTime 0 on the development machine refilled the force at once): an empty file in the
# run's own folder hides it, so that every run starts from the game's defaults and the settings below
# (openjk_server.cfg for builds from before the project was renamed)
measure_home() {
	mkdir -p "$1/base"
	: > "$1/base/modernjk_server.cfg"
	: > "$1/base/openjk_server.cfg"
}

# measure_cfg <minutes of free play> <cvars set after devmap> <index of the server> <FAST when the environment has none>
measure_cfg() {
	local minutes="$1" extra="$2" idx="$3" fast="${FAST:-$4}"
	local chunk=3000	# ticks in 30 s (TICK_MSEC, 10 ms); a longer wait never fires
	local i
	echo "set sv_maxclients 20"
	echo "set g_gametype ${GAMETYPE:-0}"
	echo "set fraglimit 0"
	echo "set timelimit 0"
	echo "set g_weaponDisable 524279"
	# the test server's settings, on top of the game's defaults (measure_home): sabers and force jump only, no saber
	# locks, no private duels
	echo "set g_forcePowerDisable 163837"
	echo "set g_saberLocking 0"
	echo "set g_privateDuel 0"
	# PREMAP: cvars set before the map loads (latched ones), separated by ";", for example PREMAP="g_forcePowerDisable 0"
	local pre c
	IFS=';' read -ra pre <<< "${PREMAP:-}"
	for c in "${pre[@]}"; do echo "set $c"; done
	# the game seeds its random numbers with the milliseconds since the process started: a random wait before
	# the map loads, in ticks of 10 ms (400 values, at most 4 s), gives each server its own seed (40 values of a
	# 50 ms frame gave the same seed, so the same game, twice)
	echo "wait $(( RANDOM % 400 + 1 ))"
	# SEED: server n of the command line (from 0) gets the game seed SEED+n (g_debugSeed) and, in fast mode, the fixed
	# frame from before the map loads (the frames of the map's first seconds otherwise follow the real time, so the free
	# play depended on the machine's load): every run then replays the same game, its free play included
	if [ -n "${SEED:-}" ]; then
		echo "set g_debugSeed $(( SEED + idx ))"
		if [ "$fast" = "1" ]; then echo "set fixedtime 10"; echo "set timedemo 1"; fi
	fi
	echo "devmap ${MAP:-mp/ffa3}"
	if [ -n "$extra" ]; then echo "set $extra"; fi
	# FAST=1: each tick advances the game by 10 ms and the server runs them as fast as it can (about 8 times real
	# time at 125 Hz with 16 bots; it takes a whole core per server, so not next to a server people play on)
	if [ "$fast" = "1" ]; then echo "set fixedtime 10"; echo "set timedemo 1"; fi
	if [ -n "${SCENARIO:-}" ]; then
		# scripted scenarios (codemp/game/g_scenario.cpp): each prints its own gamestats at its end, which the watchdog
		# counts (measure_stats)
		local b name team s reps rep ticks scns
		for b in ${BOTS:-Jedi Reborn}; do
			IFS=':' read -r name team <<< "$b"
			echo "addbot $name 4${team:+ $team}"
		done
		echo "wait ${FREEPLAY:-$chunk}"
		echo "gamestats"
		IFS=';' read -ra scns <<< "$SCENARIO"
		for s in "${scns[@]}"; do
			echo "scenario $s"
			# the repetitions and the uncounted first one, of rep= ms each (2000 by default, g_scenario.cpp), and a margin
			# of 1 s
			read -r _ reps _ <<< "$s"
			rep=2000
			[[ " $s " =~ \ rep=([0-9]+)\  ]] && rep=${BASH_REMATCH[1]}
			ticks=$(( ( reps + 1 ) * rep / 10 + 100 ))
			while [ "$ticks" -gt 0 ]; do
				echo "wait $(( ticks < chunk ? ticks : chunk ))"
				ticks=$(( ticks - chunk ))
			done
		done
		echo "quit"
	else
		for name in Reborn Reborn Reborn Reborn Reborn Reborn Jedi Jedi Jedi Jedi Alora Desann Tavion Cultist Cultist Cultist; do
			echo "addbot $name 4"
		done
		echo "wait $chunk"
		echo "gamestats"
		echo "svnetstats"
		for (( i = 0; i < minutes * 2; i++ )); do
			echo "wait $chunk"
		done
		echo "gamestats"
		echo "svnetstats"
		echo "quit"
	fi
}

# the "game statistics" lines of a finished run: the one after the free play, then one per scenario or the last one
measure_stats() {
	if [ -n "${SCENARIO:-}" ]; then
		local scns
		IFS=';' read -ra scns <<< "$SCENARIO"
		echo $(( 1 + ${#scns[@]} ))
	else
		echo 2
	fi
}

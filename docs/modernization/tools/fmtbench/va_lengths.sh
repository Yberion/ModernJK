#!/bin/bash
# The lengths of va's texts in game (plan step 22: is a std::string returned by va mostly within its own buffer, 22
# characters with libc++, 15 with libstdc++ and MSVC's library?). va_lengths.patch counts them in each module; this
# script builds the multiplayer with it on this machine (llvm-mingw clang, Git Bash), runs the scripted scenarios,
# and sums the counts.
#
# usage, from the repository's root:
#   va_lengths.sh build        the patch applied, build/va-lengths built (the dedicated server, the client, the game,
#                              cgame and UI modules, the renderer), the patch removed again: the sources unchanged
#   va_lengths.sh run <prefix> the 302 scenarios on that build (golden: SAME expected), the counts in
#                              build/va-lengths/counts (the client: run build/va-lengths/bin/modernjk.x86_64.exe with
#                              VA_LENGTHS_DIR set to that folder)
#   va_lengths.sh sum          the counts summed per module
set -e
REPO=$(pwd)
PATCH="$REPO/docs/modernization/tools/fmtbench/va_lengths.patch"
B="$REPO/build/va-lengths"
COUNTS="$B/counts"

case "$1" in
build)
	git apply "$PATCH"
	trap 'git apply -R "$PATCH"' EXIT
	cmake --preset windows-clang-release -B "$B" -Dmodernjk_ENABLE_IPO=OFF > /dev/null
	cmake --build "$B" --target modernjkded modernjk jampgame cgame ui rd-vanilla
	;;
run)
	mkdir -p "$COUNTS"
	VA_LENGTHS_DIR=$(cygpath -m "$COUNTS") BUILD=$(cygpath -m "$B/bin") \
		bash "$REPO/docs/modernization/tools/gamestats/scenarios.sh" "${2:?prefix}" "$(cygpath -m "$B/bin")/jampgamex86_64.dll"
	;;
sum)
	cat "$COUNTS"/va_lengths_*.txt | awk '
		{ m = $1; for (i = 2; i <= 9; ++i) c[m, i] += $i; if ($11 > longest[m]) longest[m] = $11; mods[m] = 1 }
		END {
			split("0-15 16-22 23-31 32-63 64-127 128-255 256-1023 1024+", label, " ")
			for (m in mods) {
				total = 0; for (i = 2; i <= 9; ++i) total += c[m, i]
				printf "%s: %d calls, the longest %d characters\n", m, total, longest[m]
				cum = 0
				for (i = 2; i <= 9; ++i) {
					cum += c[m, i]
					printf "  %-9s %12d  %6.2f %%  (up to here %6.2f %%)\n", label[i - 1], c[m, i], 100 * c[m, i] / total, 100 * cum / total
				}
			}
		}'
	;;
*)
	echo "usage: va_lengths.sh build | run <prefix> | sum"
	exit 2
	;;
esac

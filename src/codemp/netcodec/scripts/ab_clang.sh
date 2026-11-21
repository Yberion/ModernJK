#!/bin/bash
# A/B (this Windows machine, Git Bash, llvm-mingw clang): bench_codec of a reference (REF=<commit>, default HEAD, or
# REF_DIR=<a copy holding src/codemp/netcodec or codemp/netcodec>) against the working tree, Release, run alternately on logical CPU 20 (the
# second CCD; the first, with the 3D V-Cache, is left to the desktop) with high priority. Builds in build-netcodec/ab-clang.
# Prints the warm and cold timings per run, then the best of each build against ref with the checks of
# ab_summary.awk. Only meaningful on a quiet machine (no game, the VMs off), to check before a run.
# Usage, from anywhere: [REF=<commit> | REF_DIR=<dir>] [CUR_DIR=<dir>] [NOALIGN=1] [CXXFLAGS=...] [PRESET=<preset>]
#   [BENCH=client] bash src/codemp/netcodec/scripts/ab_clang.sh [rounds] [--no-build]
# BENCH=client: netcodec_bench_client on the recorded client packets (docs/modernization/data/clientpackets/*.jkcp)
# instead of bench_codec; the times are then per client packet.
# CUR_DIR: a copy holding src/codemp/netcodec (or codemp/netcodec) timed instead of the working tree. Functions and loops are aligned to 64 bytes
# in both builds, as ab_gcc.sh does: without it, where the code falls moves the timings by up to 5 % (an older reference
# commit, built before the library aligned its code, would otherwise be compared unaligned against an aligned build).
# NOALIGN=1: each build keeps the flags of its own CMakeLists.txt (only to time the alignment itself). CXXFLAGS: extra
# compiler flags for both builds.
export LC_ALL=C   # numbers with a decimal point in awk (a French locale reads 1404.9 as 1404)
HERE="$(cd "$(dirname "$0")" && pwd)"
R="$(cd "$HERE/../../../.." && pwd -W 2>/dev/null || pwd)"
. "$HERE/ab_tree.sh"
S="$R/build-netcodec/ab-clang"
mkdir -p "$S"
export PATH="/c/Logiciels/llvm-mingw-20261104-ucrt-x86_64/bin:/c/Logiciels/cmake-4.4.5/bin:/c/Logiciels/ninja:$PATH"
ROUNDS=${1:-6}
REF=${REF:-HEAD}
PRESET=${PRESET:-mingw-clang-release}
BENCH=${BENCH:-codec}                    # codec (server messages) or client (client packets)
ALIGN="-falign-functions=64 -falign-loops=64"
[ "${NOALIGN:-0}" = 1 ] && ALIGN=""
# The reference is built with its own ModernJK sources (the legacy coder, msg.cpp, and the states' layout, q_shared.h):
# those of its commit, or REF_DIR's when it holds src/codemp/qcommon/msg.cpp (codemp/ in the older layout), else the
# working tree's.
build() {   # $1 = name (ref, cur), $2 = its ModernJK sources
	(cd "$S/$1/$(ab_netcodec "$S/$1")" &&
	 cmake --preset "$PRESET" -DCMAKE_CXX_FLAGS="$ALIGN ${CXXFLAGS:-}" -Dnetcodec_MODERNJK_DIR="$2" -Dnetcodec_TRACE_DIR="$R/docs/modernization/data/traces" > "$S/build-$1.log" 2>&1 &&
	 cmake --build --preset "$PRESET" --target "netcodec_bench_$BENCH" >> "$S/build-$1.log" 2>&1) ||
	{ echo "build of $1 failed:"; tail -30 "$S/build-$1.log"; exit 1; }
}
if [[ " $* " != *" --no-build "* ]]; then
	rm -rf "$S/ref" "$S/cur"
	mkdir -p "$S/ref" "$S/cur"
	REF_OJ="$S/ref"
	if [ -n "$REF_DIR" ]; then
		ab_copy "$REF_DIR" "$S/ref" || exit 1
		if ab_has_engine "$REF_DIR"; then REF_OJ="$(cd "$REF_DIR" && pwd -W)"; else REF_OJ="$R"; fi
		echo "ref = the copy in $REF_DIR" > "$S/ref.txt"
	else
		ab_archive "$REF" "$S/ref" || exit 1
		echo "ref = $REF ($(cd "$R" && git log -1 --format='%h %s' "$REF" | cut -c1-100))" > "$S/ref.txt"
	fi
	ab_copy "${CUR_DIR:-$R}" "$S/cur" || exit 1
	echo "cur = ${CUR_DIR:-the working tree}; both built with: ${ALIGN:-no forced alignment} ${CXXFLAGS:-}; bench_$BENCH" > "$S/cur.txt"
	rm -rf "$S/cur/build-netcodec"
	build ref "$REF_OJ"
	build cur "$R"
fi
echo "$(cat "$S/ref.txt"); $(cat "$S/cur.txt")$([[ " $* " == *" --no-build "* ]] && echo ' (as last built)')"
TR=$(cygpath -w "$R/docs/modernization/data/traces/codec_trace.bin")
PACKETS=()
for p in "$R"/docs/modernization/data/clientpackets/*.jkcp; do PACKETS+=("$(cygpath -w "$p")"); done
for i in $(seq 1 "$ROUNDS"); do
	for v in ref cur; do
		exe=$(cygpath -w "$S/$v/build-netcodec/$PRESET/bench/netcodec_bench_$BENCH.exe")
		printf '%s %d: ' $v $i
		if [ "$BENCH" = client ]; then
			cmd //c start "" //b //wait //affinity 0x100000 //high "$exe" "${PACKETS[@]}" | awk -f "$HERE/ab_client.awk"
		else
			cmd //c start "" //b //wait //affinity 0x100000 //high "$exe" "$TR" 10 \
				| awk '/mismatches/{split($0,a,"mismatches "); mm=a[2]+0}
				       /encode netcodec/{en=$3; we=$6} /encode legacy/{wel=$6} /decode netcodec/{dn=$3; wd=$6} /decode legacy/{wdl=$6}
				       END{printf "warm encode %.1f (legacy %.1f) decode %.1f (legacy %.1f) mismatches %d | cold encode %.0f decode %.0f\n", we, wel, wd, wdl, mm, en, dn}'
		fi
	done
done | tee "$S/runs.txt"
awk -f "$HERE/ab_summary.awk" "$S/runs.txt"

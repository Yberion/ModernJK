#!/bin/bash
# A/B (Linux VM): bench_codec of a reference commit (REF, default HEAD) against the working tree, GCC 15 Release,
# run alternately on core 1; prints the warm and cold timings per run, then the best of each build and the difference.
# Usage, from anywhere: [REF=<commit> | REF_DIR=<copy>] [CUR_DIR=<copy>] [NOINL=1] [NOALIGN=1] [CXXFLAGS=...] [BENCH=client]
#   bash src/codemp/netcodec/scripts/ab_gcc.sh [rounds]
# BENCH=client: netcodec_bench_client on the recorded client packets (docs/modernization/data/clientpackets/*.jkcp)
# instead of bench_codec; the times are then per client packet.
# CUR_DIR: a directory holding src/codemp/netcodec or codemp/netcodec (a variant prepared on the host) timed instead
# of the working tree; REF_DIR: the same for the reference (instead of a commit).
# The reference is built with its own ModernJK sources (the legacy coder, msg.cpp, and the states' layout, q_shared.h):
# those of its commit, or REF_DIR's when it holds src/codemp/qcommon/msg.cpp (codemp/ in the older layout), else the
# working tree's.
# Functions, loops and jumps are aligned to 64 bytes in both builds: without it, where the code falls in memory moves
# the timings by 10 to 20 % under GCC (the unchanged legacy coder included). NOALIGN=1: no alignment forced by this
# script, each build keeps the flags of its own CMakeLists.txt (to time the shipped settings of two commits).
# NOINL=1: the working-tree copy without the forced inlining of PredictAngle. CXXFLAGS: extra compiler flags for
# both builds.
# Checks: a netcodec mismatch fails the run; the unchanged legacy coder moving by more than 4 % between runs means the
# machine was busy and the numbers should not be used.
set -e
export LC_ALL=C   # numbers with a decimal point in awk (a French locale reads 1404.9 as 1404)
R="$(cd "$(dirname "$0")/../../../.." && pwd)"
. "$(dirname "$0")/ab_tree.sh"
ROUNDS=${1:-6}
REF=${REF:-HEAD}
BENCH=${BENCH:-codec}   # codec (server messages) or client (client packets)
W=/tmp/netcodec-ab
G() { git -c safe.directory='*' -C "$R" "$@"; }
rm -rf "$W" && mkdir -p "$W/ref" "$W/cur"
REF_OJ="$W/ref"
if [ -n "${REF_DIR:-}" ]; then ab_copy "$REF_DIR" "$W/ref"
	if ab_has_engine "$REF_DIR"; then REF_OJ="$REF_DIR"; else REF_OJ="$R"; fi
else ab_archive "$REF" "$W/ref"; fi
ab_copy "${CUR_DIR:-$R}" "$W/cur"
if [ -n "${REF_DIR:-}" ]; then RDESC="the copy in $REF_DIR"; else RDESC="$REF ($(G log -1 --format='%h %s' "$REF" | cut -c1-100))"; fi
echo "ref = $RDESC; cur = ${CUR_DIR:-the working tree}; bench_$BENCH"
if [ "${NOINL:-0}" = 1 ]; then
	F="$W/cur/$(ab_netcodec "$W/cur")/src/field_codec.h"
	sed -i 's/\[\[nodiscard\]\] NETCODEC_ALWAYS_INLINE std::optional<int64_t> PredictAngle(/[[nodiscard]] inline std::optional<int64_t> PredictAngle(/' "$F"
	grep -q "nodiscard\]\] inline std::optional<int64_t> PredictAngle" "$F" || { echo "NOINL: PredictAngle not found in $F"; exit 1; }
	echo "cur: PredictAngle not forced inline"
fi
ALIGN="-falign-functions=64 -falign-loops=64 -falign-jumps=64"
[ "${NOALIGN:-0}" = 1 ] && ALIGN="" && echo "no alignment forced: each build has its own CMakeLists.txt flags"
build() {   # $1 = name (ref, cur), $2 = its ModernJK sources
	cmake -S "$W/$1/$(ab_netcodec "$W/$1")" -B "$W/build-$1" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++-15 \
		-DCMAKE_CXX_FLAGS="$ALIGN ${CXXFLAGS:-}" \
		-Dnetcodec_MODERNJK_DIR="$2" -Dnetcodec_TRACE_DIR="$R/docs/modernization/data/traces" > "$W/build-$1.log" 2>&1 &&
	cmake --build "$W/build-$1" --target "netcodec_bench_$BENCH" -j 4 >> "$W/build-$1.log" 2>&1 ||
	{ echo "build of $1 failed:"; tail -30 "$W/build-$1.log"; exit 1; }
}
echo "building the reference and the working tree ..."
build ref "$REF_OJ"
build cur "$R"
echo "functions of the library not inlined (code symbols), only in ref (<) or only in cur (>):"
for v in ref cur; do
	nm -C --defined-only "$W/build-$v/libnetcodec.a" | awk '$2 ~ /^[TtWw]$/ {$1=""; $2=""; print}' | sed 's/^  //' | sort -u > "$W/syms-$v.txt"
done
diff "$W/syms-ref.txt" "$W/syms-cur.txt" | grep '^[<>]' | cut -c1-200 || echo "  (none)"
echo "sizes of the code sections: ref $(size -t "$W/build-ref/libnetcodec.a" | tail -1 | awk '{print $1}'), cur $(size -t "$W/build-cur/libnetcodec.a" | tail -1 | awk '{print $1}')"
T="$R/docs/modernization/data/traces/codec_trace.bin"
PIN=""
command -v taskset > /dev/null && PIN="taskset -c 1"
for i in $(seq 1 "$ROUNDS"); do
	for v in ref cur; do
		printf '%s %d: ' $v $i
		if [ "$BENCH" = client ]; then
			$PIN "$W/build-$v/bench/netcodec_bench_client" "$R"/docs/modernization/data/clientpackets/*.jkcp \
				| awk -f "$(dirname "$0")/ab_client.awk"
		else
			$PIN "$W/build-$v/bench/netcodec_bench_codec" "$T" 10 \
				| awk '/mismatches/{split($0,a,"mismatches "); mm=a[2]+0}
				       /encode netcodec/{en=$3; e=$6} /decode netcodec/{dn=$3; d=$6} /encode legacy/{el=$6} /decode legacy/{dl=$6}
				       END{printf "warm encode %.1f (legacy %.1f) decode %.1f (legacy %.1f) mismatches %d | cold encode %.0f decode %.0f\n", e, el, d, dl, mm, en, dn}'
		fi
	done
done | tee "$W/runs.txt"
awk -f "$(dirname "$0")/ab_summary.awk" "$W/runs.txt"

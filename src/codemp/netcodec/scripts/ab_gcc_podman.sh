#!/bin/bash
# A/B (Linux VM): bench_codec built with a recent GCC in a podman container, run on the VM outside it.
# Three builds: a reference commit (REF, default HEAD), the working tree without the forced inlining of PredictAngle
# (noinl), the working tree (cur); code aligned to 64 bytes in all of them, as ab_gcc.sh. Prints the warm and cold
# timings per run, then the best of each build against ref, with the checks of ab_summary.awk.
# Usage, from anywhere: [REF=<commit>] bash src/codemp/netcodec/scripts/ab_gcc_podman.sh [rounds] [image]
# (default image: docker.io/library/gcc:16)
# The reference is built with its own commit's legacy benchmark sources, the two others with the working tree's. Each
# copy is built in its own tree's layout (ab_tree.sh); a tree configured from its root needs CMake 4.5, which the
# container downloads from Kitware's releases (its distribution's is older).
set -e
export LC_ALL=C   # numbers with a decimal point in awk (a French locale reads 1404.9 as 1404)
R="$(cd "$(dirname "$0")/../../../.." && pwd)"
. "$(dirname "$0")/ab_tree.sh"
ROUNDS=${1:-6}
IMG=${2:-docker.io/library/gcc:16}
REF=${REF:-HEAD}
CMAKE_VERSION=4.5.1
W=/tmp/netcodec-abp
G() { git -c safe.directory='*' -C "$R" "$@"; }
rm -rf "$W" && mkdir -p "$W/ref" "$W/cur" "$W/noinl"
ab_archive "$REF" "$W/ref"
echo "ref = $REF ($(G log -1 --format='%h %s' "$REF" | cut -c1-100)); cur = the working tree"
ab_copy "$R" "$W/cur"
cp -r "$W/cur/." "$W/noinl/"
F="$W/noinl/src/codemp/netcodec/src/field_codec.h"
sed -i 's/\[\[nodiscard\]\] NETCODEC_ALWAYS_INLINE std::optional<int64_t> PredictAngle(/[[nodiscard]] inline std::optional<int64_t> PredictAngle(/' "$F"
grep -q "nodiscard\]\] inline std::optional<int64_t> PredictAngle" "$F" || { echo "noinl: PredictAngle not found in $F"; exit 1; }
# the layout of each copy, and the tree of its legacy benchmark's sources inside the container (copied: rootless
# podman cannot read the VirtualBox shared folder)
for v in ref noinl cur; do ab_layout "$W/$v" > "$W/$v.layout"; done
mkdir -p "$W/oj/src" "$W/oj/libs/gsl-lite" && cp -r "$R/src/codemp" "$R/src/shared" "$W/oj/src/" &&
	cp -r "$R/libs/gsl-lite/include" "$W/oj/libs/gsl-lite/"
echo "building in $IMG ..."
podman run --rm -v "$W:/w" -e CMAKE_VERSION="$CMAKE_VERSION" "$IMG" bash -c '
	set -e
	(apt-get update -qq && apt-get install -y -qq cmake) > /dev/null 2>&1
	if grep -q project /w/*.layout; then
		curl -sSL "https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-linux-x86_64.tar.gz" |
			tar -xz -C /opt
		export PATH="/opt/cmake-$CMAKE_VERSION-linux-x86_64/bin:$PATH"
	fi
	g++ --version | head -1
	cmake --version | head -1
	for v in ref noinl cur; do
		oj=/w/oj
		[ $v = ref ] && oj=/w/ref
		flags="-DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DCMAKE_EXE_LINKER_FLAGS=-static"
		align="-falign-functions=64 -falign-loops=64 -falign-jumps=64"
		if [ "$(cat /w/$v.layout)" = project ]; then
			cmake -S /w/$v -B /w/build-$v $flags -DCMAKE_CXX_FLAGS="$align" \
				-Dmodernjk_BUILD_MP_CLIENT=OFF -Dmodernjk_BUILD_MP_SERVER=OFF -Dmodernjk_BUILD_MP_GAME=OFF \
				-Dmodernjk_BUILD_SP=OFF -Dmodernjk_BUILD_JK2=OFF -Dmodernjk_BUILD_ASSETS_PK3=OFF -DBUILD_TESTING=OFF \
				-Dmodernjk_BUILD_TOOLS=OFF -Dnetcodec_ENGINE_DIR=$oj -Dnetcodec_TRACE_DIR=/w/no-traces > /w/build-$v.log 2>&1
		else
			n=src/codemp/netcodec
			[ -d /w/$v/$n ] || n=codemp/netcodec
			cmake -S /w/$v/$n -B /w/build-$v $flags -DCMAKE_CXX_FLAGS="$align" \
				-Dnetcodec_MODERNJK_DIR=$oj -Dnetcodec_TRACE_DIR=/w/no-traces > /w/build-$v.log 2>&1
		fi &&
		cmake --build /w/build-$v --target netcodec_bench_codec -j 4 >> /w/build-$v.log 2>&1 ||
		{ echo "build of $v failed:"; tail -30 /w/build-$v.log; exit 1; }
	done'
echo "functions of the library not inlined, compared with ref (< only in ref, > only in the other build):"
for v in ref noinl cur; do
	nm -C --defined-only "$(ab_library "$W/$v" "$W/build-$v")" | awk '$2 ~ /^[TtWw]$/ {$1=""; $2=""; print}' | sed 's/^  //' | sort -u > "$W/syms-$v.txt"
done
for v in noinl cur; do
	echo "  $v:"
	diff "$W/syms-ref.txt" "$W/syms-$v.txt" | grep '^[<>]' | cut -c1-160 | sed 's/^/    /' || echo "    (none)"
done
T="$R/docs/modernization/data/traces/codec_trace.bin"
PIN=""
command -v taskset > /dev/null && PIN="taskset -c 1"
for i in $(seq 1 "$ROUNDS"); do
	for v in ref noinl cur; do
		printf '%s %d: ' $v $i
		$PIN "$(ab_bench "$W/$v" "$W/build-$v" codec)" "$T" 10 \
			| awk '/mismatches/{split($0,a,"mismatches "); mm=a[2]+0}
			       /encode netcodec/{en=$3; e=$6} /decode netcodec/{dn=$3; d=$6} /encode legacy/{el=$6} /decode legacy/{dl=$6}
			       END{printf "warm encode %.1f (legacy %.1f) decode %.1f (legacy %.1f) mismatches %d | cold encode %.0f decode %.0f\n", e, el, d, dl, mm, en, dn}'
	done
done | tee "$W/runs.txt"
awk -f "$(dirname "$0")/ab_summary.awk" "$W/runs.txt"

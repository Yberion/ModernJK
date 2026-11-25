#!/usr/bin/env bash
# Validation on Linux (GCC 15 and Clang 23): check that the C and C++ code is laid out as clang-format 23 lays it out
# (.clang-format; the machines share the sources, so this check runs here only), configure, build and test every Linux
# preset (the game, netcodec and
# their tests; the AddressSanitizer preset with the sanitizers), build the servers' preset (the dedicated server
# and the game module) and the check of the code only Debug compiles (GCC's link-time optimization reports its types
# and declarations across files), build the zone files' converter (tools/eal_converter, out of the presets' builds) with
# both compilers, then run the size benchmark (Release and AddressSanitizer), the codec benchmark (two runs) and
# the trainers, which must rewrite the committed tables unchanged. Needs the traces of docs/modernization/data/traces
# (export_rans_trace.py, export_codec_trace.py, export_demo_traces.py, and the client traces of
# netcodec_bench_client --trace).
# Run from anywhere: scripts/validate.sh
# Output: the console and build/validate-linux.log; the summary (with each step's time) and the benchmark figures
# come last.
set -u
cd "$(dirname "$0")/.."
B=build
T=docs/modernization/data/traces
G=src/codemp/netcodec/src/generated
mkdir -p "$B"
LOG="$B/validate-linux.log"
exec > >(tee "$LOG") 2>&1

# the sources' contents at the start, compared at the end: a file changed during the run makes it a mix (FAIL)
srchash() {
	find CMakeLists.txt CMakePresets.json ProjectOptions.cmake Dependencies.cmake cmake src test tools assets libs/ealman -type f -print0 |
		sort -z | xargs -0 sha1sum | sha1sum
}
START_HASH=$(srchash)

JOBS=$(nproc 2>/dev/null || echo 2)
summary=()
failed=0
step() {
	local name=$1 start=$SECONDS
	shift
	echo
	echo "=== $name"
	if "$@"; then
		summary+=("ok    $name ($((SECONDS - start)) s)")
	else
		summary+=("FAIL  $name ($((SECONDS - start)) s)")
		failed=1
	fi
}

step "formatting, clang-format 23" bash -c "find src test tools -type f \\( -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp' \
	-o -name '*.inl' -o -name '*.inc' \\) -print0 | xargs -0 -P '$JOBS' -n 50 clang-format-23 --dry-run --Werror"
for p in linux-gcc-release linux-gcc-debug linux-gcc-asan linux-clang-release linux-clang-asan; do
	step "$p: configure" cmake --preset "$p"
	step "$p: build" cmake --build --preset "$p" -j "$JOBS"
	step "$p: tests" ctest --preset "$p"
done
step "linux-gcc-server: configure" cmake --preset linux-gcc-server
step "linux-gcc-server: build" cmake --build --preset linux-gcc-server -j "$JOBS"
step "linux-gcc-debug-lto: configure" cmake --preset linux-gcc-debug-lto
step "linux-gcc-debug-lto: build" cmake --build --preset linux-gcc-debug-lto -j "$JOBS"
step "linux-gcc-release: zone converter" cmake --build --preset linux-gcc-release -j "$JOBS" --target eal_converter
step "linux-clang-release: zone converter" cmake --build --preset linux-clang-release -j "$JOBS" --target eal_converter

OUT=$(mktemp -d)
R=$B/linux-gcc-release/bin
step "size benchmark, linux-gcc-release" bash -c "'$R/netcodec_bench_size' '$T/holdout.bin' > '$OUT/size'"
step "size benchmark, linux-gcc-asan" bash -c "'$B/linux-gcc-asan/bin/netcodec_bench_size' '$T/holdout.bin' > '$OUT/size-asan'"
step "codec benchmark, linux-gcc-release, run 1" bash -c "'$R/netcodec_bench_codec' '$T/codec_trace.bin' 10 > '$OUT/codec1'"
step "codec benchmark, linux-gcc-release, run 2" bash -c "'$R/netcodec_bench_codec' '$T/codec_trace.bin' 10 > '$OUT/codec2'"
step "trainer linux-gcc-release writes the committed tables" bash -c \
	"'$R/netcodec_train' '$T/train.bin' '$OUT/tables.inc' && cmp '$OUT/tables.inc' '$G/jka_tables.inc'"
step "client trainer linux-gcc-release writes the committed client tables" bash -c \
	"'$R/netcodec_train_client' '$OUT/client_tables.inc' '$T/client_train.bin' && cmp '$OUT/client_tables.inc' '$G/jka_client_tables.inc'"

echo
echo "=== results"
for f in size size-asan; do
	echo "--- $f"
	sed -n '1,2p;5,6p' "$OUT/$f" 2>/dev/null
done
for f in codec1 codec2; do
	echo "--- $f"
	cat "$OUT/$f" 2>/dev/null
done
if [ "$(srchash)" != "$START_HASH" ]; then
	summary+=("FAIL  sources unchanged during the run (a source changed: run again)")
	failed=1
fi
echo
echo "=== summary"
printf '%s\n' "${summary[@]}"
if [ "$failed" -eq 0 ]; then echo "ALL OK"; else echo "SOME STEPS FAILED"; fi
echo "log: $LOG"
rm -rf "$OUT"
exit "$failed"

#!/bin/bash
# The formatting benchmark of plan step 22 with one compiler: builds fmtbench and the size probe in
# build/fmtbench-<compiler>, checks that snprintf and {fmt} write the same text, times every case, compares the
# stripped sizes, and times the compilation of 300 calls written each way.
#
# usage: run.sh <C++ compiler: clang++, g++-15...> [core]
#   core   the core the timed thread is pinned to (core 20 on the development machine; nothing else running)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/../../../.." && pwd)
CXX=${1:?usage: run.sh <C++ compiler> [core]}
CORE=${2:-}
B="$REPO/build/fmtbench-$(basename "$CXX")"

# build/fmt-ref, a copy of libs/fmt without its patches, if present: both timed, alternated, twice
REF="$REPO/build/fmt-ref"
REF_ARG=""
[ -d "$REF" ] && REF_ARG="-DFMT_REF=$REF"
cmake -S "$HERE" -B "$B" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX" -DREPO="$REPO" $REF_ARG > /dev/null
cmake --build "$B" > "$B/build.log" 2>&1 || { cat "$B/build.log"; exit 1; }
if [ -n "$REF_ARG" ]; then
	for round in 1 2; do
		echo "=== round $round: {fmt} without libs/fmt's patches (build/fmt-ref)"
		"$B/fmtbench_ref" ${CORE:+--core "$CORE"}
		echo "=== round $round: libs/fmt"
		"$B/fmtbench" ${CORE:+--core "$CORE"}
	done
else
	"$B/fmtbench" ${CORE:+--core "$CORE"}
fi

echo
echo "sizes, stripped (bytes)"
STRIP=$(command -v llvm-strip || command -v strip)
for p in size_printf size_fmt; do
	exe="$B/$p"
	[ -f "$exe.exe" ] && exe="$exe.exe"
	"$STRIP" -o "$B/$p.stripped" "$exe"
	printf "  %-12s %9d\n" "$p" "$(wc -c < "$B/$p.stripped")"
done
obj=$(find "$B" -name 'format.cc.o*' | head -1)
printf "  %-12s %9d (libs/fmt/src/format.cc's object, every section)\n" "format.cc" "$(wc -c < "$obj")"

echo
echo "compilation of 300 calls (-O2, fastest of 5, ms)"
C="$B/compile"
mkdir -p "$C"
{
	echo '#include <cstdarg>'
	echo '#include <cstdio>'
	echo 'int old_sprintf(char* dest, int size, const char* format, ...) __attribute__((format(printf, 3, 4)));'
	echo 'void calls(char* buf, int n, const char* s, double f) {'
	for i in $(seq 1 100); do
		echo "    old_sprintf(buf, 256, \"line $i: %i\\n\", n);"
		echo "    old_sprintf(buf, 256, \"print \\\"%s $i\\n\\\"\", s);"
		echo "    old_sprintf(buf, 256, \"$i %s: %i %.2f\\n\", s, n, f);"
	done
	echo '}'
} > "$C/calls_printf.cpp"
{
	echo '#include <fmt/base.h>'
	echo '#include <span>'
	echo 'size_t new_vsprintf(std::span<char> dest, fmt::string_view format, fmt::format_args args);'
	echo 'template <typename... T> size_t new_sprintf(std::span<char> dest, fmt::format_string<T...> format, T&&... args) {'
	echo '    return new_vsprintf(dest, format.get(), fmt::make_format_args(args...));'
	echo '}'
	echo 'void calls(std::span<char> buf, int n, const char* s, double f) {'
	for i in $(seq 1 100); do
		echo "    new_sprintf(buf, \"line $i: {}\\n\", n);"
		echo "    new_sprintf(buf, \"print \\\"{} $i\\n\\\"\", s);"
		echo "    new_sprintf(buf, \"$i {}: {} {:.2f}\\n\", s, n, f);"
	done
	echo '}'
} > "$C/calls_fmt.cpp"
for f in calls_printf calls_fmt; do
	best=999999
	for r in 1 2 3 4 5; do
		t0=$(date +%s%N)
		"$CXX" -std=c++23 -O2 -c -I"$REPO/libs/fmt/include" "$C/$f.cpp" -o "$C/$f.o"
		t1=$(date +%s%N)
		ms=$(( (t1 - t0) / 1000000 ))
		[ "$ms" -lt "$best" ] && best=$ms
	done
	printf "  %-13s %6d ms, object %7d bytes\n" "$f" "$best" "$(wc -c < "$C/$f.o")"
done

#!/usr/bin/env bash
# Plan step 19: every warning GCC has, on every target, to count them per flag (count_warnings.py). GCC has no
# -Weverything: the list is the compiler's own (-Q --help=warnings), each option kept if g++ accepts it for C++,
# without the language versions and ABIs other than C++23, the struct padding and the system headers; the options
# that take a level are added at their highest useful one. Release build (some warnings need the optimizer), C++23.
#
# Usage: gcc_weverything.sh <source tree> <build folder>; the log is <build folder>.log. CC and CXX choose the
# compilers (gcc-15 and g++-15 by default).
set -u
src=$1
build=$2
cc=${CC:-gcc-15}
cxx=${CXX:-g++-15}

accepted() { [ -z "$("$cxx" "$1" -x c++ -std=c++23 -fsyntax-only - </dev/null 2>&1)" ]; }

flags=()
while read -r opt _; do
	case $opt in
		-W*=*|-W*\<*) continue ;;
		-Wc++*-compat|-Wc++-compat|-Wc90-c99-compat|-Wc99-c11-compat|-Wc11-c23-compat|-Wtraditional*) continue ;;
		-Wlong-long|-Wabi*|-Wpadded|-Wsystem-headers|-Werror*) continue ;;
		-W*) accepted "$opt" && flags+=("$opt") ;;
	esac
done < <("$cxx" -Q --help=warnings 2>/dev/null)
for opt in -Wformat=2 -Warray-bounds=2 -Wshift-overflow=2 -Wplacement-new=2 -Wformat-overflow=2 \
	-Wformat-truncation=2 -Wstringop-overflow=4 -Wcatch-value=3 -Wuse-after-free=3 -Wunused-const-variable=2 \
	-Wimplicit-fallthrough=5 -Wbidi-chars=any -Wnormalized=nfkc; do
	accepted "$opt" && flags+=("$opt")
done
echo "${#flags[@]} warning options: ${flags[*]}"

cmake -S "$src" -B "$build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$cc" -DCMAKE_CXX_COMPILER="$cxx" \
	-DBuildJK2SPEngine=ON -DBuildJK2SPGame=ON -DBuildJK2SPRdVanilla=ON \
	-DCMAKE_PROJECT_INCLUDE="$src/docs_rewrite/modernization/tools/warnings/cxx23.cmake" \
	"-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG ${flags[*]} -fdiagnostics-show-option -fno-diagnostics-show-caret" \
	|| exit 1
start=$(date +%s)
# -O: each compilation's messages together (in parallel they cut into each other's lines)
cmake --build "$build" --parallel "$(nproc)" -- -k -O >"$build.log" 2>&1
echo "build exit $? in $(( $(date +%s) - start )) s; $(grep -c 'warning:' "$build.log") warning lines in $build.log"

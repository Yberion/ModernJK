#!/usr/bin/env bash
# Plan step 19: every warning clang has (-Weverything), on every target, Release and Debug (the code under _DEBUG),
# C++23, to count them per flag (count_warnings.py). Left out: the compatibility with other language versions and
# the struct padding. The flags go in CMAKE_CXX_FLAGS_<config>, which come after the project's own -Wno-... options.
#
# Usage: clang_weverything.sh <source tree> <build folder prefix>; the logs are <prefix>-release.log.gz and
# <prefix>-debug.log.gz. CC and CXX choose the compilers (clang and clang++ by default), GENERATOR the CMake
# generator (Ninja by default; "Unix Makefiles" where Ninja is not installed).
set -u
src=$1
prefix=$2
generator=${GENERATOR:-Ninja}
# Make: -O keeps each compilation's messages together (in parallel they cut into each other's lines)
if [ "$generator" = Ninja ]; then keep_going="-k 0"; else keep_going="-k -O"; fi
w="-Weverything -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-pre-c++14-compat -Wno-pre-c++17-compat"
w="$w -Wno-pre-c++20-compat -Wno-pre-c++20-compat-pedantic -Wno-pre-c++23-compat -Wno-c++20-compat -Wno-padded"
w="$w -fno-caret-diagnostics -fdiagnostics-show-option"
for cfg in Release Debug; do
	lower=$(echo "$cfg" | tr 'A-Z' 'a-z')
	upper=$(echo "$cfg" | tr 'a-z' 'A-Z')
	if [ "$cfg" = Release ]; then base="-O3 -DNDEBUG"; else base="-g"; fi
	cmake -G "$generator" -S "$src" -B "$prefix-$lower" -DCMAKE_BUILD_TYPE="$cfg" \
		-DCMAKE_C_COMPILER="${CC:-clang}" -DCMAKE_CXX_COMPILER="${CXX:-clang++}" \
		-DBuildJK2SPEngine=ON -DBuildJK2SPGame=ON -DBuildJK2SPRdVanilla=ON \
		-DCMAKE_PROJECT_INCLUDE="$src/docs_rewrite/modernization/tools/warnings/cxx23.cmake" \
		"-DCMAKE_CXX_FLAGS_$upper=$base $w" >/dev/null || exit 1
	start=$(date +%s)
	# shellcheck disable=SC2086
	cmake --build "$prefix-$lower" --parallel "$(nproc)" -- $keep_going 2>&1 | gzip >"$prefix-$lower.log.gz"
	echo "$cfg: $(( $(date +%s) - start )) s"
done

#!/bin/bash
# The decided audio decoders on the Linux VM (surveys/audio-decoders.md, section 8): libogg, libvorbis and libFLAC built
# from the sources the survey downloaded (not the distribution's), the decided decoders and their references built
# with GCC 15 and with Clang 23, normally and with AddressSanitizer, then run on the corpora (check_run.sh); the lists
# are compared with the Windows ones afterwards (crosscheck.py).
# usage: bash linux_check.sh        (from anywhere on the Linux VM; a log per step in <decoders>/work/check/)
R=/media/sf_jka/ModernJK
D=$R/build-netcodec/audiolibs/decoders
T=$R/docs_rewrite/modernization/tools/audiodec
W=$D/work/check
mkdir -p "$W"
J="$(nproc)"

lib() { # build-folder prefix log-name extra-flags source [cmake options...]
	local b=$1 p=$2 log=$3 flags=$4 src=$5
	shift 5
	cmake -S "$src" -B "$b" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
		-DCMAKE_C_FLAGS="$flags" -DCMAKE_CXX_FLAGS="$flags" -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX="$p" \
		-DCMAKE_PREFIX_PATH="$p" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 "$@" > "$W/$log.log" 2>&1 &&
		cmake --build "$b" --parallel "$J" >> "$W/$log.log" 2>&1 &&
		cmake --install "$b" >> "$W/$log.log" 2>&1 || { echo "FAIL: $log (see $W/$log.log)"; return 1; }
}

libs() { # folder prefix flags tag
	lib "$1/ogg" "$2" "$4-ogg" "$3" "$D/src/libogg-1.3.6" -DINSTALL_DOCS=OFF &&
		lib "$1/vorbis" "$2" "$4-vorbis" "$3" "$D/src/libvorbis-1.3.7" &&
		lib "$1/flac" "$2" "$4-flac" "$3" "$D/src/flac-1.5.0" -DBUILD_CXXLIBS=OFF -DBUILD_PROGRAMS=OFF \
			-DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DBUILD_DOCS=OFF -DINSTALL_MANPAGES=OFF -DWITH_OGG=ON \
			-DENABLE_MULTITHREADING=OFF
}

bench() { # folder prefix asan tag
	rm -rf "$1" # configured again from nothing
	cmake -S "$T" -B "$1" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
		-DDEC="$D" -DREPO="$R" -DLIBS_PREFIX="$2" -DASAN="$3" -DCHOSEN_ONLY=ON > "$W/$4-bench.log" 2>&1 &&
		cmake --build "$1" --parallel "$J" >> "$W/$4-bench.log" 2>&1 || { echo "FAIL: $4-bench (see $W/$4-bench.log)"; return 1; }
	echo "$4: compiler warnings in the benchmark's build: $(grep -c 'warning:' "$W/$4-bench.log")"
}

for cc in gcc clang; do
	if [ $cc = gcc ]; then CC=gcc-15 CXX=g++-15; else CC=clang-23 CXX=clang++-23; fi
	B=$D/linux-$cc
	SAN="-fsanitize=address -fno-omit-frame-pointer"
	echo "== $cc ($CC)"
	libs "$B/build" "$B/prefix" "" "linux-$cc" &&
		libs "$B/build-asan" "$B/prefix-asan" "$SAN" "linux-$cc-asan" &&
		bench "$B/bench" "$B/prefix" OFF "linux-$cc" &&
		bench "$B/bench-asan" "$B/prefix-asan" ON "linux-$cc-asan" &&
		sh "$T/check_run.sh" "$D" "$B/bench" "$W/linux-$cc" "$B/bench-asan" &&
		echo "$cc: done, $W/linux-$cc/summary.txt"
done

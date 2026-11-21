#!/bin/sh
# Build the reference decoder libraries of the audio decoder survey (surveys/audio-decoders.md) with llvm-mingw clang,
# Release, static, each from its own CMake project, installed into one prefix.
# usage: sh build_libs.sh <decoders folder holding src/> [asan]   (the sources extracted there by the survey)
#   asan: the same with AddressSanitizer, into build-asan/ and prefix-asan/, for the robustness runs
set -e
D=$1
S=$D/src
B=$D/build
P=$D/prefix
SAN=""
if [ "$2" = asan ]; then
	B=$D/build-asan
	P=$D/prefix-asan
	SAN="-fsanitize=address -fno-omit-frame-pointer"
fi
export PATH="/c/Logiciels/llvm-mingw-20261104-ucrt-x86_64/bin:/c/Logiciels/cmake-4.4.5/bin:/c/Logiciels/ninja:$PATH"
export CFLAGS="$SAN" CXXFLAGS="$SAN" LDFLAGS="$SAN"
COMMON="-G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX=$P -DCMAKE_PREFIX_PATH=$P -DCMAKE_POLICY_VERSION_MINIMUM=3.5"

build() { # name source-dir [cmake options...]
	name=$1; src=$2; shift 2
	cmake -S "$src" -B "$B/$name" $COMMON "$@" > "$B/$name.log" 2>&1
	cmake --build "$B/$name" >> "$B/$name.log" 2>&1
	cmake --install "$B/$name" >> "$B/$name.log" 2>&1
	echo "built $name"
}

mkdir -p "$B" "$P"
build ogg "$S/libogg-1.3.6" -DINSTALL_DOCS=OFF
build vorbis "$S/libvorbis-1.3.7"
build flac "$S/flac-1.5.0" -DBUILD_CXXLIBS=OFF -DBUILD_PROGRAMS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_TESTING=OFF -DBUILD_DOCS=OFF \
	-DINSTALL_MANPAGES=OFF -DWITH_OGG=ON -DENABLE_MULTITHREADING=OFF
build opus "$S/opus-1.6.1" -DOPUS_BUILD_TESTING=OFF -DOPUS_BUILD_PROGRAMS=OFF
build mpg123 "$S/mpg123-1.33.7/ports/cmake" -DBUILD_LIBOUT123=OFF -DBUILD_PROGRAMS=OFF -DNETWORK=OFF
# Its copy of WavPack calls _BitScanReverse without <intrin.h>, which clang refuses (an error since C99): included here.
build nyquist "$S/libnyquist" -DLIBNYQUIST_BUILD_EXAMPLE=OFF "-DCMAKE_C_FLAGS=-include intrin.h $SAN"

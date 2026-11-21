#!/bin/sh
# Builds ealdump and audiobench with llvm-mingw clang on Windows, from the libraries downloaded into
# build-netcodec/audiolibs (OpenAL Soft 1.25.2 built there as a static library in build-openal, miniaudio 0.11.26,
# EalMan aa69e586). Usage, from the repository root: sh docs/modernization/tools/audiobench/build.sh
set -e
ROOT=$(cd "$(dirname "$0")/../../../.." && pwd)
LIBS=$ROOT/build-netcodec/audiolibs
TOOL=$ROOT/docs/modernization/tools/audiobench
OUT=$LIBS/bin
mkdir -p "$OUT/obj"
MA=$LIBS/miniaudio
EAL=$LIBS/ealman/src
clang -O2 -c "$MA/miniaudio.c" -o "$OUT/obj/miniaudio.o"
clang -O2 -c "$MA/extras/nodes/ma_reverb_node/ma_reverb_node.c" -o "$OUT/obj/ma_reverb_node.o"
for f in Chunk EalMan; do
	clang++ -std=c++17 -O2 -DEAXMANVERSION=3 -c "$EAL/$f.cpp" -o "$OUT/obj/$f.o"
done
clang++ -std=c++17 -O2 -DEAXMANVERSION=3 -I"$EAL" "$TOOL/ealdump.cpp" "$OUT/obj/Chunk.o" "$OUT/obj/EalMan.o" \
	-o "$OUT/ealdump.exe"
clang++ -std=c++17 -O2 -DEAXMANVERSION=3 -DAL_LIBTYPE_STATIC -DAL_ALEXT_PROTOTYPES \
	-I"$LIBS/openal-soft/include" -I"$MA" -I"$MA/extras/nodes/ma_reverb_node" -I"$EAL" \
	"$TOOL/audiobench.cpp" "$OUT/obj/miniaudio.o" "$OUT/obj/ma_reverb_node.o" "$OUT/obj/Chunk.o" "$OUT/obj/EalMan.o" \
	"$LIBS/build-openal/libOpenAL32.a" -lwinmm -lole32 -luuid -lavrt -lshell32 -luser32 \
	-o "$OUT/audiobench.exe"
ls -la "$OUT"/*.exe

#!/bin/sh
# Run the decided decoders and their references on the corpora, one list per executable (name, frames, hash, ...),
# for the cross-check between compilers and platforms (crosscheck.py), then the robustness runs if an AddressSanitizer
# build is given.
# usage: sh check_run.sh <decoders folder> <bench build folder> <out folder> [<asan bench build folder>]
D=$1
B=$2
OUT=$3
A=$4
mkdir -p "$OUT"
: > "$OUT/summary.txt"
run() { # executable corpus [--compare]
	echo "== $1 $2" >> "$OUT/summary.txt"
	"$B/$1" "$D/data/$2.bin" --runs 1 --list "$OUT/$1.tsv" $3 >> "$OUT/summary.txt" 2>&1
	echo "exit $?" >> "$OUT/summary.txt"
}
run bench_mp3code game_mp3
run bench_drmp3 game_mp3 --compare
run bench_enginewav game_wav
run bench_drwav game_wav --compare
run bench_libflac sample_flac
run bench_vorbisfile sample_vorbis
run bench_stbvorbis sample_vorbis --compare
if [ -n "$A" ]; then
	mutate() { # executable corpus copies
		echo "== asan $1 $2 x$3" >> "$OUT/summary.txt"
		ASAN_OPTIONS=halt_on_error=0 "$A/$1" "$D/data/$2.bin" --mutate "$3" > "$OUT/asan_$1.txt" 2>&1
		echo "exit $?, AddressSanitizer reports: $(grep -c 'ERROR: AddressSanitizer' "$OUT/asan_$1.txt")" >> "$OUT/summary.txt"
		grep -m1 -E '^decoder' "$OUT/asan_$1.txt" >> "$OUT/summary.txt"
	}
	mutate bench_drmp3 robust_mp3 3
	mutate bench_drwav robust_wav 9
	mutate bench_libflac robust_flac 3
	mutate bench_stbvorbis robust_vorbis 3
fi
echo "== done" >> "$OUT/summary.txt"

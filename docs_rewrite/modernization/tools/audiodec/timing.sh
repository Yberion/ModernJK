#!/bin/sh
# The speed runs of the audio decoder survey: every decoder on the whole corpus of its format, 5 decodings per file
# (the first cold, the fastest warm), on one logical processor, in two rounds that alternate the decoders.
# usage: sh timing.sh <decoders folder> <out file>       (a quiet machine: the VMs off)
D=$1
OUT=$2
B=$D/build/bench
: > "$OUT"
run() { # decoder corpus
	echo "== $1 $2" >> "$OUT"
	"$B/bench_$1.exe" "$D/data/$2.bin" --runs 5 --core 20 >> "$OUT" 2>&1
}
for round in 1 2; do
	echo "#### round $round" >> "$OUT"
	for d in mp3code minimp3 drmp3 mpg123 ma_mp3; do run $d game_mp3; done
	for d in enginewav drwav ma_wav; do run $d game_wav; done
	for d in libflac drflac ma_flac; do run $d sample_flac; done
	for d in vorbisfile stbvorbis ma_vorbis; do run $d sample_vorbis; done
	for d in opusfile ma_opus; do run $d sample_opus; done
done
echo "#### done" >> "$OUT"

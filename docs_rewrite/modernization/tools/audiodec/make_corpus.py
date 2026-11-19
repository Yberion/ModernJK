"""Write the game's MP3 and WAV files into two corpus files for the decoder benchmark.

usage: python -I make_corpus.py <game base folder with assets*.pk3> <output folder>

Format of a corpus file: b"ADC1", u32 count, then per file: u16 name length, name (UTF-8),
u32 data length, data. Little-endian. Files are written sorted by name, so two runs give the same file.
"""
import pathlib
import struct
import sys
import zipfile


def write_corpus(path, entries):
    with open(path, "wb") as f:
        f.write(b"ADC1")
        f.write(struct.pack("<I", len(entries)))
        for name, data in entries:
            n = name.encode("utf-8")
            f.write(struct.pack("<H", len(n)))
            f.write(n)
            f.write(struct.pack("<I", len(data)))
            f.write(data)


def main():
    base = pathlib.Path(sys.argv[1])
    out = pathlib.Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    files = {}
    for pk3 in sorted(base.glob("assets*.pk3")):
        with zipfile.ZipFile(pk3) as z:
            for info in z.infolist():
                low = info.filename.lower()
                if low.endswith(".mp3") or low.endswith(".wav"):
                    files[info.filename] = z.read(info)  # a later archive overrides an earlier one, as the game does
    for ext in ("mp3", "wav"):
        entries = sorted((n, d) for n, d in files.items() if n.lower().endswith("." + ext))
        write_corpus(out / f"game_{ext}.bin", entries)
        print(ext, len(entries), sum(len(d) for _, d in entries), "bytes")


main()

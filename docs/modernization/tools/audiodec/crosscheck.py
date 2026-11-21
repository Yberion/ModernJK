"""Compare the lists of check_run.sh / msvc_check.bat between compilers and platforms: for each executable, the files
whose decoding (frames and hash of the 16-bit samples) is the same in every folder, and those that differ.

usage: python -I crosscheck.py <reference folder> <folder>...
"""
import pathlib
import sys


def read(path):
    rows = {}
    for line in path.read_bytes().decode("utf-8").splitlines():
        cols = line.split("\t")
        if len(cols) >= 8:
            rows[cols[0]] = (cols[1], cols[2], cols[7])  # ok, frames, hash
    return rows


ref = pathlib.Path(sys.argv[1])
others = [pathlib.Path(a) for a in sys.argv[2:]]
for lst in sorted(ref.glob("bench_*.tsv")):
    base = read(lst)
    for other in others:
        path = other / lst.name
        if not path.exists():
            print(f"{lst.stem:18} {other.name:14} missing")
            continue
        rows = read(path)
        same = sum(1 for n, v in base.items() if rows.get(n) == v)
        diff = [n for n, v in base.items() if rows.get(n) != v]
        print(f"{lst.stem:18} {other.name:14} same {same} of {len(base)}" + (f", first different: {diff[0]}" if diff else ""))

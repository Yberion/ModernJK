"""Runs a clang-tidy configuration on every entry of a compile database, each through a database of that entry alone,
and exports each entry's replacements (--export-fixes) into a folder, for fmtconv.py apply; prints the entries that
did not compile (their replacements may be missing).

Usage: python -I run_fix.py <repository> <build folder> <config.yaml> <fixes folder>
"""
import json
import os
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

TIDY = os.environ.get("CLANG_TIDY", "C:/Logiciels/llvm-mingw-20261104-ucrt-x86_64/bin/clang-tidy.exe")
ERROR = re.compile(r"^\S.*?:\d+:\d+: error: |^error: ")


def run(i, entry, cfg, tmp, out):
    d = tmp / str(i)
    d.mkdir(parents=True, exist_ok=True)
    (d / "compile_commands.json").write_bytes(json.dumps([entry]).encode("utf-8"))
    r = subprocess.run([TIDY, f"--config-file={cfg}", "-p", str(d), "--header-filter=.*",
                        f"--export-fixes={out / f'{i}.yaml'}", entry["file"]], capture_output=True)
    text = r.stdout.decode("utf-8", "replace")
    return entry["file"], [line for line in text.splitlines() if ERROR.match(line)]


def main():
    repo, build, cfg, out = sys.argv[1], Path(sys.argv[2]), sys.argv[3], Path(sys.argv[4])
    out.mkdir(parents=True, exist_ok=True)
    db = json.loads((build / "compile_commands.json").read_bytes().decode("utf-8"))
    db = [e for e in db if re.search(r"/(src|test)/.*\.(cpp|c)$", e["file"].replace("\\", "/"))]
    failed = 0
    with tempfile.TemporaryDirectory() as tmp, ThreadPoolExecutor(max_workers=os.cpu_count()) as ex:
        for f, errors in ex.map(lambda a: run(a[0], a[1], cfg, Path(tmp), out), enumerate(db)):
            if errors:
                failed += 1
                print(f, errors[0][:200])
    print("entries", len(db), "with errors", failed)


main()

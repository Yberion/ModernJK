"""Runs a query-based clang-tidy check (fmtcalls.yaml, variadic.yaml) on every entry of a compile database and
collects the calls and declarations it reports, each tied to the targets that compile it (one clang-tidy run per
entry, through a database of that entry alone). The calls are then read by parse.py.

Usage: python -I run_tidy.py <repository> <build folder> <out.json> [<check.yaml>]
clang-tidy 23 or later (its --experimental-custom-checks); CLANG_TIDY names its program, llvm-mingw's by default.
"""
import json
import os
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

HERE = Path(__file__).resolve().parent
TIDY = os.environ.get("CLANG_TIDY", "C:/Logiciels/llvm-mingw-20261104-ucrt-x86_64/bin/clang-tidy.exe")
CONFIG = sys.argv[4] if len(sys.argv) > 4 else str(HERE / "fmtcalls.yaml")
DIAG = re.compile(r"^(.*?):(\d+):(\d+): warning: (fmtcall|fmtdecl) \[")
ERROR = re.compile(r"^\S.*?:\d+:\d+: error: |^error: ")


def target_of(entry):
    for text in (entry.get("output", ""), entry.get("command", "")):
        m = re.search(r"CMakeFiles/([^/]+)\.dir/", text.replace("\\", "/"))
        if m:
            return m.group(1)
    return "?"


def norm(p, repo):
    p = os.path.normpath(p).replace("\\", "/")
    r = repo.replace("\\", "/")
    return p[len(r) + 1:] if p.lower().startswith(r.lower() + "/") else p


def run(i, entry, repo, dbroot):
    d = dbroot / str(i)
    d.mkdir(parents=True, exist_ok=True)
    (d / "compile_commands.json").write_bytes(json.dumps([entry]).encode("utf-8"))
    r = subprocess.run([TIDY, "--experimental-custom-checks", f"--config-file={CONFIG}", "-p", str(d),
                        "--header-filter=.*", entry["file"]], capture_output=True)
    text = r.stdout.decode("utf-8", "replace")
    hits = []
    for line in text.splitlines():
        m = DIAG.match(line)
        if m:
            hits.append((m.group(4), norm(m.group(1), repo), int(m.group(2)), int(m.group(3))))
    errors = [line for line in text.splitlines() if ERROR.match(line)]
    return target_of(entry), norm(entry["file"], repo), hits, errors


def main():
    repo, build, out = sys.argv[1], sys.argv[2], sys.argv[3]
    db = json.loads((Path(build) / "compile_commands.json").read_bytes().decode("utf-8"))
    # the sources and tests only (not the resource scripts, which clang-tidy cannot read)
    db = [e for e in db if re.search(r"/(src|test)/.*\.(cpp|c)$", e["file"].replace("\\", "/"))]
    calls, decls, failed = {}, {}, []
    with tempfile.TemporaryDirectory() as tmp, ThreadPoolExecutor(max_workers=os.cpu_count()) as ex:
        for tgt, src, hits, errors in ex.map(lambda a: run(a[0], a[1], repo, Path(tmp)), enumerate(db)):
            if errors:
                failed.append({"target": tgt, "file": src, "errors": errors[:5]})
            for kind, p, ln, col in hits:
                dst = calls if kind == "fmtcall" else decls
                dst.setdefault(f"{p}:{ln}:{col}", set()).add(tgt)
    res = {"calls": {k: sorted(v) for k, v in calls.items()}, "decls": {k: sorted(v) for k, v in decls.items()},
           "failed": failed, "entries": len(db)}
    Path(out).write_bytes(json.dumps(res, indent=1).encode("utf-8"))
    print("entries", len(db), "calls", len(calls), "declarations", len(decls), "failed", len(failed))


main()

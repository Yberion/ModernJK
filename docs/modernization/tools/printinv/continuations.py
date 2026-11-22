"""The calls of a level's function (Com_Warning, Com_PrintError) that continue a line an earlier print started: a print
whose format does not end with a newline within the six lines before. Such a call would write its level's prefix in
the middle of the line; it is a message instead. Prints them for review.

Usage: python -I continuations.py <repository> <path>...
"""
import re
import sys
from pathlib import Path

LEVEL = re.compile(r"\b(Com_Warning|Com_PrintError)\s*\(")
PRINT = re.compile(r"\b(Com_Printf|Com_DPrintf|Com_Warning|Com_PrintError)\s*\((.*)$")


def main():
    repo = Path(sys.argv[1])
    for a in sys.argv[2:]:
        p = repo / a
        for f in sorted(p.rglob("*")) if p.is_dir() else [p]:
            if f.suffix not in (".cpp", ".h", ".c"):
                continue
            lines = f.read_bytes().decode("latin-1").split("\n")
            for i, line in enumerate(lines):
                if not LEVEL.search(line) or line.lstrip().startswith("//"):
                    continue
                for j in range(max(0, i - 6), i):
                    m = PRINT.search(lines[j])
                    if not m or lines[j].lstrip().startswith("//"):
                        continue
                    literals = re.findall(r'"((?:[^"\\]|\\.)*)"', m.group(2))
                    if literals and not literals[-1].endswith("\\n"):
                        print(f"{f.relative_to(repo).as_posix()}:{i + 1}: after line {j + 1}: {line.strip()[:110]}")
                        break


main()

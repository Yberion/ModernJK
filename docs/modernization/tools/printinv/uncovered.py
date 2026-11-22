"""The calls of the printing and formatting functions the syntax tree did not see: the branches the builds do not
compile (`#if 0`, `#ifdef _DEBUG` in Release, Linux on Windows) and the files no target of the database compiles.
Counts by text the lines outside comments and strings that call one of the functions, less the lines levels.py has.

Usage: python -I uncovered.py <repository> <levels.json>
"""
import collections
import json
import re
import subprocess
import sys
from pathlib import Path

NAMES = ["Com_Printf", "Com_DPrintf", "Com_Warning", "Com_PrintError", "Com_Error", "Com_OPrintf", "Printf", "Print",
         "Error", "CG_Printf", "CG_Error", "G_Error", "DebugPrint", "Q3_DebugPrint", "G_DebugPrint", "I_DPrintf",
         "Debug_Printf", "SourceError", "SourceWarning", "ScriptError", "ScriptWarning", "AAS_Error", "va",
         "Com_sprintf"]
CALL = re.compile(r"(?<![\w])(?:" + "|".join(NAMES) + r")\s*\(")


def main():
    repo = Path(sys.argv[1])
    rows = json.loads(Path(sys.argv[2]).read_bytes())
    seen = {(x["loc"].rsplit(":", 2)[0], int(x["loc"].rsplit(":", 2)[1])) for x in rows}
    files = subprocess.run(["git", "-C", str(repo), "ls-files", "src", "test"], capture_output=True).stdout.decode().split()
    missed, where = collections.Counter(), collections.Counter()
    for f in (f for f in files if re.search(r"\.(cpp|h)$", f)):
        text = (repo / f).read_bytes().decode("latin-1")
        text = re.sub(r"/\*.*?\*/", lambda m: re.sub(r"[^\n]", " ", m.group(0)), text, flags=re.S)
        for i, line in enumerate(text.split("\n"), 1):
            line = re.sub(r'"(?:[^"\\]|\\.)*"', '""', line).split("//")[0]
            m = CALL.search(line)
            if not m or (f, i) in seen:
                continue
            # a declaration, a definition or a function pointer's declaration
            if re.search(r"^\s*(?:extern\s+|static\s+|NORETURN\s+|QDECL\s+)*(?:void|int|char|const char|bool)\b[\s*]", line):
                continue
            if re.search(r"\(\s*(?:QDECL\s*)?\*\s*\w+\s*\)", line):
                continue
            missed[m.group(0).rstrip("( ")] += 1
            where[f] += 1
    print(sum(missed.values()), "lines not seen by the syntax tree")
    for k, n in missed.most_common():
        print(" ", n, k)
    for k, n in where.most_common(30):
        print(" ", n, k)


main()

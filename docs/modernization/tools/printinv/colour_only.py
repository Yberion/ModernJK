"""The messages left for plan step 27: the Com_Printf and Com_DPrintf calls whose format starts with a red or yellow
colour but no level word, and those whose level word is not a prefix (no colon, or after "..." or a newline), as a
Markdown table.

Usage: python -I colour_only.py <levels.json>
"""
import json
import re
import sys
from pathlib import Path

rows = json.loads(Path(sys.argv[1]).read_bytes())
out = []
for x in rows:
    if x["fn"] not in ("Com_Printf", "Com_DPrintf") or x["format"] is None:
        continue
    colour_only = re.match(r"\^[13]", x["format"]) and not x["word"]
    if colour_only or x["word"]:
        file, line, _ = x["loc"].rsplit(":", 2)
        text = x["format"][:70].replace("|", "\\|")
        out.append((file, int(line), x["fn"], text))
out.sort()
print(f"{len(out)} messages\n")
print("| place | function | the format's start |")
print("|---|---|---|")
for file, line, fn, text in out:
    print(f"| `{file}:{line}` | `{fn}` | `{text}` |")

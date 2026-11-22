"""The table of the texts that change, from convert.py's logs: the calls grouped by the start of their visible text
before and after (the colour codes and the level word), with a count and an example each.

Usage: python -I changes.py <changes.jsonl>...
"""
import collections
import json
import re
import sys
from pathlib import Path

HEAD = re.compile(r"^((?:\^\d)*)((?:WARNING|Warning|warning|ERROR|Error|error|FATAL|Fatal)\s*:\s*)?")


def head(s):
    m = HEAD.match(s)
    return m.group(0), s[m.end():]


def main():
    rows = []
    for f in sys.argv[1:]:
        rows += [json.loads(l) for l in Path(f).read_bytes().decode("utf-8").splitlines() if l.strip()]
    rows = [r for r in rows if "skipped" not in r and r["target"] != "Com_Error"]
    groups = collections.defaultdict(list)
    for r in rows:
        sp_fx = r["callee"] in ("theFxHelper.Print", "helper.Print") and not r["file"].startswith("src/codemp/")
        if sp_fx:  # the single players' effects messages: printed outside FINAL_BUILD, developer's messages now
            key = (r["callee"], "(outside FINAL_BUILD)", r["target"])
        elif r["before"].startswith(("(debugger)", "(developer)")):  # a destination, not a text
            key = (r["callee"], r["before"].split(")")[0] + ")", r["target"])
        else:
            hb, rb = head(r["before"])
            ha, ra = head(r["after"])
            if hb == ha and rb == ra:
                key = ("same text",)
            else:
                key = (r["callee"] + (":" + r["level"] if r["level"] else ""), repr(hb), repr(ha), r["target"])
        groups[key].append(r)
    print("calls changed:", len(rows))
    for key, rs in sorted(groups.items(), key=lambda kv: -len(kv[1])):
        ex = rs[0]
        print(f"{len(rs):5}  {' | '.join(key)}  e.g. {ex['file']}:{ex['line']} {ex['before'][:50]!r} -> {ex['after'][:50]!r}")


main()

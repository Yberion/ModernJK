"""Sorts the calls parse.py read by the level they mean: the function, its level argument, and the colour and word
the format starts with (a message that writes `^3WARNING: ` by hand is a warning). Writes one row per call and prints
the table of step 22's section 7.

Usage: python -I levels.py <out.json> <parse.json> [<parse.json>...]
"""
import collections
import json
import re
import sys
from pathlib import Path

COLOURS = {"S_COLOR_BLACK": "^0", "S_COLOR_RED": "^1", "S_COLOR_GREEN": "^2", "S_COLOR_YELLOW": "^3",
           "S_COLOR_BLUE": "^4", "S_COLOR_CYAN": "^5", "S_COLOR_MAGENTA": "^6", "S_COLOR_WHITE": "^7",
           "S_COLOR_ORANGE": "^8", "S_COLOR_GREY": "^9"}

# the callee (its last member access, or its name) -> (the format's argument, the level's argument), from 1
FUNCTIONS = {
    "Com_Printf": (1, None), "Com_DPrintf": (1, None), "Com_Warning": (1, None), "Com_PrintError": (1, None),
    "Com_OPrintf": (1, None), "CG_Printf": (1, None), "trap->Print": (1, None), "gi.Printf": (1, None),
    "ui.Printf": (1, None), "DC->Print": (1, None), "theFxHelper.Print": (1, None), "helper.Print": (1, None),
    "ri.OPrintf": (1, None), "ri.Printf": (2, 1), "botimport.Print": (2, 1),
    "I_DPrintf": (2, 1), "DebugPrint": (2, 1), "Q3_DebugPrint": (2, 1), "G_DebugPrint": (2, 1), "Debug_Printf": (3, 2),
    "Boba_Printf": (1, None), "COM_ParseWarning": (1, None), "SourceError": (2, None), "SourceWarning": (2, None),
    "ScriptError": (2, None), "ScriptWarning": (2, None), "PC_SourceError": (2, None), "AAS_Error": (1, None),
    "Com_Error": (2, 1), "trap->Error": (2, 1), "ri.Error": (2, 1), "gi.Error": (2, 1), "ui.Error": (2, 1),
    "G_Error": (1, None), "CG_Error": (1, None), "Sys_Error": (1, None), "CenterPrint": (1, None),
    "I_CenterPrint": (1, None), "G_LogPrintf": (1, None), "G_SecurityLogPrintf": (1, None), "Log_Write": (1, None),
    "ProfilePrint": (1, None), "FS_Printf": (2, None), "NET_OutOfBandPrint": (3, None),
    "SV_SendServerCommand": (2, None), "SendServerCommand": (2, None), "printf": (1, None), "fprintf": (2, None),
    "vfprintf": (2, None), "va": (1, None), "Com_sprintf": (3, None), "sprintf": (2, None), "snprintf": (3, None),
    "Q_vsnprintf": (3, None), "vsnprintf": (3, None), "vsprintf": (2, None),
}
MESSAGE = {"Com_Printf", "trap->Print", "gi.Printf", "ui.Printf", "DC->Print", "CG_Printf", "theFxHelper.Print",
           "helper.Print"}
WARNING = {"Com_Warning", "SourceWarning", "ScriptWarning", "COM_ParseWarning"}
ERROR = {"Com_PrintError", "SourceError", "ScriptError", "AAS_Error", "PC_SourceError"}
STOP = {"Com_Error", "trap->Error", "ri.Error", "gi.Error", "ui.Error", "G_Error", "CG_Error"}
DEBUG = {"I_DPrintf", "DebugPrint", "Q3_DebugPrint", "G_DebugPrint", "Debug_Printf", "Boba_Printf", "ProfilePrint",
         "Com_OPrintf", "ri.OPrintf"}
LOG = {"G_LogPrintf", "G_SecurityLogPrintf", "Log_Write", "FS_Printf"}
BY_LEVEL = {"PRINT_ALL": "message", "PRINT_DEVELOPER": "developer", "PRINT_WARNING": "warning", "PRINT_ERROR": "error",
            "PRT_MESSAGE": "message", "PRT_WARNING": "warning", "PRT_ERROR": "error", "PRT_FATAL": "error",
            "PRT_EXIT": "stop"}


def key_of(callee):
    c = re.sub(r"^::", "", re.sub(r"\(.*?\)", "()", callee, flags=re.S)).replace("std::", "")
    for k in (c, re.sub(r"^.*?(\w+(?:\.|->)\w+)$", r"\1", c), re.findall(r"\w+", c)[-1]):
        if k in FUNCTIONS:
            return k
    return None


def format_text(arg):
    """The format's text when it is made of literals and colour macros, else None."""
    out, rest = [], arg.strip()
    while rest:
        m = re.match(r'"((?:[^"\\]|\\.)*)"\s*', rest)
        if m:
            out.append(m.group(1))
        else:
            m = re.match(r"(S_COLOR_\w+)\s*", rest)
            if not m or m.group(1) not in COLOURS:
                return None
            out.append(COLOURS[m.group(1)])
        rest = rest[m.end():]
    return "".join(out)


def start(t):
    """The colour and the level word the text starts with."""
    m = re.match(r"((?:\^\d)*)((?:\\n|\s|\.\.\.)*)(\w+)?", t or "")
    word = (m.group(3) or "").upper()
    return (m.group(1)[-2:] if m.group(1) else ""), (word if word in ("WARNING", "ERROR", "FATAL") else "")


def level_of(fn, lvl, word):
    if lvl in BY_LEVEL and fn in ("ri.Printf", "botimport.Print"):
        f = BY_LEVEL[lvl]
    elif fn in MESSAGE:
        f = "message"
    elif fn == "Com_DPrintf":
        f = "developer"
    elif fn in WARNING:
        f = "warning"
    elif fn in ERROR:
        f = "error"
    elif fn in STOP:
        f = "stop"
    elif fn in DEBUG:
        f = "debug channel"
    elif fn in LOG:
        f = "log"
    else:
        f = "formatting and others"
    if f in ("message", "developer") and word:
        f += " written " + ("warning" if word == "WARNING" else "error")
    return f


def main():
    rows, seen = [], set()
    for f in sys.argv[2:]:
        for x in json.loads(Path(f).read_bytes())["calls"]:
            k = key_of(x["callee"])
            if not k or x["loc"] in seen:
                continue
            seen.add(x["loc"])
            fi, li = FUNCTIONS[k]
            a = x["args"]
            t = format_text(a[fi - 1]) if len(a) >= fi else None
            col, word = start(t) if t is not None else ("?", "")
            lvl = a[li - 1].split("::")[-1] if li and len(a) >= li else ""
            rows.append({"loc": x["loc"], "tree": x["tree"], "fn": k, "level": lvl, "means": level_of(k, lvl, word),
                         "format": t, "raw": a[fi - 1] if len(a) >= fi else None, "colour": col, "word": word,
                         "targets": x["targets"]})
    Path(sys.argv[1]).write_bytes(json.dumps(rows, indent=1).encode("utf-8"))
    means = collections.defaultdict(collections.Counter)
    for x in rows:
        means[x["means"]][x["fn"] + (":" + x["level"] if x["level"] else "")] += 1
    for m, c in sorted(means.items(), key=lambda kv: -sum(kv[1].values())):
        print(sum(c.values()), m)
        for k, n in c.most_common():
            print("     ", n, k)


main()

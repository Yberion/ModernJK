"""Rewrites the printing calls onto q_fmt.h's functions, by text, so that the branches the builds do not compile and
the commented-out calls follow too: the callee, the level argument removed, and for a warning or an error the colour
and the level word the format starts with removed (the level's function writes them). Writes a JSON line per call
changed, with the visible start of the text before and after, for the table of the texts that change.

Usage: python -I convert.py <repository> <changes.jsonl> <rules> <path>...
<rules>: mp (the multiplayer's import tables and the bot library), fx (the effects system's messages, developer's
messages); a path is a file or a folder of the repository.
"""
import json
import re
import sys
from pathlib import Path

WORD = re.compile(r"(WARNING|Warning|warning|ERROR|Error|error|FATAL|Fatal)\s*:\s*")
COLOUR_MACRO = re.compile(r"S_COLOR_(BLACK|RED|GREEN|YELLOW|BLUE|CYAN|MAGENTA|WHITE|ORANGE|GREY)\b")
COLOURS = {"BLACK": "^0", "RED": "^1", "GREEN": "^2", "YELLOW": "^3", "BLUE": "^4", "CYAN": "^5", "MAGENTA": "^6",
           "WHITE": "^7", "ORANGE": "^8", "GREY": "^9"}
NEW_PREFIX = {"Com_Printf": "", "Com_DPrintf": "", "Com_Warning": "^3WARNING: ", "Com_PrintError": "^1ERROR: "}

# the callees rewritten: callee -> (has a level argument, the old prefix by level, the target by level)
MP = {
    "trap->Print": (False, {None: ""}, {None: "message"}),
    "trap->Error": (False, {None: None}, {None: "Com_Error"}),
    "ri.Printf": (True, {"PRINT_ALL": "", "PRINT_WARNING": "^3", "PRINT_DEVELOPER": "^1", "PRINT_ERROR": "^1"},
                  {"PRINT_ALL": "message", "PRINT_WARNING": "Com_Warning", "PRINT_DEVELOPER": "Com_DPrintf",
                   "PRINT_ERROR": "Com_PrintError"}),
    "ri.Error": (False, {None: None}, {None: "Com_Error"}),
    "ri.OPrintf": (False, {None: "(debugger)"}, {None: "Com_DPrintf"}),
    "Com_OPrintf": (False, {None: "(debugger)"}, {None: "Com_DPrintf"}),
    "botimport.Print": (True, {"PRT_MESSAGE": "", "PRT_WARNING": "^3Warning: ", "PRT_ERROR": "^1Error: ",
                               "PRT_FATAL": "^1Fatal: "},
                        {"PRT_MESSAGE": "message", "PRT_WARNING": "Com_Warning", "PRT_ERROR": "Com_PrintError",
                         "PRT_FATAL": "Com_PrintError"}),
    "AAS_Error": (False, {None: "^1Fatal: "}, {None: "Com_PrintError"}),
    "DC->Print": (False, {None: ""}, {None: "message"}),
    "DC->Error": (False, {None: None}, {None: "Com_Error"}),
    "Com_Printf": (False, {None: ""}, {None: "message"}),
}
FX = {"theFxHelper.Print": (False, {None: "(developer)"}, {None: "Com_DPrintf"})}
# the single players' (Jedi Academy's and Jedi Outcast's): their games' import table, cgame's and the UI's functions,
# the renderer's; G_Error and CG_Error stopped the game with ERR_DROP, which Com_Error is given
SP = {
    "gi.Printf": (False, {None: ""}, {None: "message"}),
    "gi.Error": (False, {None: None}, {None: "Com_Error"}),
    "ui.Printf": (False, {None: ""}, {None: "message"}),
    "ui.Error": (False, {None: None}, {None: "Com_Error"}),
    "CG_Printf": (False, {None: ""}, {None: "message"}),
    "CG_Error": (False, {None: None}, {None: "Com_Error+drop"}),
    "G_Error": (False, {None: None}, {None: "Com_Error+drop"}),
    "ri.Printf": MP["ri.Printf"],
    "ri.Error": MP["ri.Error"],
    "DC->Print": MP["DC->Print"],
    "Com_Printf": MP["Com_Printf"],
}
SPFX = {"theFxHelper.Print": (False, {None: ""}, {None: "Com_DPrintf"}),
        "helper.Print": (False, {None: ""}, {None: "Com_DPrintf"})}
RULES = {"mp": MP, "fx": FX, "sp": SP, "spfx": SPFX}


def skip_string(t, i):
    q, j = t[i], i + 1
    while j < len(t) and t[j] != q:
        j += 2 if t[j] == "\\" else 1
    return j + 1


def arg_spans(t, i):
    """t[i] is the call's '(': the spans of its arguments and the index of its ')', or None."""
    spans, start, depth, j, n = [], i + 1, 0, i + 1, len(t)
    while j < n:
        c = t[j]
        if c in "\"'":
            j = skip_string(t, j)
            continue
        if t.startswith("//", j):
            k = t.find("\n", j)
            j = n if k < 0 else k
            continue
        if t.startswith("/*", j):
            k = t.find("*/", j)
            j = n if k < 0 else k + 2
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                spans.append((start, j))
                return spans, j
            depth -= 1
        elif c == "," and depth == 0:
            spans.append((start, j))
            start = j + 1
        j += 1
    return None


def tokens(arg):
    """The format argument as tokens (kind, text): ws, str, colour, or rest (anything else, once)."""
    out, i = [], 0
    while i < len(arg):
        m = re.match(r"\s+", arg[i:])
        if m:
            out.append(("ws", m.group(0)))
        elif arg[i] == '"':
            j = skip_string(arg, i)
            out.append(("str", arg[i:j]))
            i = j
            continue
        elif COLOUR_MACRO.match(arg, i):
            m = COLOUR_MACRO.match(arg, i)
            out.append(("colour", m.group(0)))
        else:
            out.append(("rest", arg[i:]))
            break
        i += len(m.group(0))
    return out


def visible(toks):
    """The text the format's literals and colour macros make, up to the first other token."""
    s = []
    for k, t in toks:
        if k == "str":
            s.append(t[1:-1])
        elif k == "colour":
            s.append(COLOURS[t[len("S_COLOR_"):]])
        elif k == "rest":
            break
    return "".join(s)


def first(toks):
    for i, (k, _) in enumerate(toks):
        if k != "ws":
            return i
    return None


def drop(toks, i):
    del toks[i]
    while i < len(toks) and toks[i][0] == "ws":
        del toks[i]


def strip_level(arg):
    """The format without the colour and the level word it starts with; None when they cannot be removed simply."""
    toks = tokens(arg)
    while True:
        i = first(toks)
        if i is None:
            return None
        k, t = toks[i]
        if k == "colour":
            drop(toks, i)
            continue
        if k == "str" and re.match(r"\^\d", t[1:-1]):
            content = t[3:-1]
            if content or (i + 1 < len(toks) and any(k2 in ("str", "colour") for k2, _ in toks[i + 1:])):
                if content:
                    toks[i] = ("str", '"' + content + '"')
                else:
                    drop(toks, i)
                continue
        break
    i = first(toks)
    if i is not None and toks[i][0] == "str":
        content = toks[i][1][1:-1]
        m = WORD.match(content)
        if m:
            content = content[m.end():]
            rest = [k2 for k2, _ in toks[i + 1:] if k2 != "ws"]
            if content or not rest or rest[0] != "str":
                toks[i] = ("str", '"' + content + '"')
            else:
                drop(toks, i)
    return "".join(t for _, t in toks)


def target_of_message(fmt):
    m = re.match(r"(\^\d)*" + WORD.pattern, visible(tokens(fmt)))
    if not m:
        return "Com_Printf"
    return "Com_Warning" if m.group(2).upper() == "WARNING" else "Com_PrintError"


def convert(text, rules, path, log):
    names = sorted(rules, key=len, reverse=True)
    pat = re.compile(r"(?<![\w.>:])((?:::)?(?:" + "|".join(re.escape(n) for n in names) + r"))\s*\(")
    out, pos = [], 0
    for m in pat.finditer(text):
        if m.start() < pos:
            continue
        callee = m.group(1)
        has_level, old_prefixes, targets = rules[callee.lstrip(":")]
        before = text[text.rfind("\n", 0, m.start()) + 1:m.start()]
        if re.search(r"\b(void|QDECL|int|char|bool|extern|static|NORETURN)\s*\**\s*$", before):
            continue  # a declaration or a definition, not a call
        paren = m.end() - 1
        r = arg_spans(text, paren)
        if r is None:
            log.append({"file": path, "offset": m.start(), "callee": callee, "skipped": "arguments not read"})
            continue
        spans, close = r
        lines = text.count("\n", m.start(), close)
        # a commented-out call on several lines: its lines' // read as comments by arg_spans, so it does not close
        if lines and "//" in before:
            log.append({"file": path, "offset": m.start(), "callee": callee, "skipped": "commented on several lines"})
            continue
        if lines > 30:
            log.append({"file": path, "offset": m.start(), "callee": callee, "skipped": f"{lines} lines"})
            continue
        args = [text[a:b] for a, b in spans]
        level = None
        if has_level:
            level = args[0].strip().split("::")[-1]
            if level not in targets:
                log.append({"file": path, "offset": m.start(), "callee": callee, "skipped": f"level {level}"})
                continue
        target = targets[level]
        fmt_i = 1 if has_level else 0
        if target == "Com_Error":
            new_args = args
        elif target == "Com_Error+drop":
            target = "Com_Error"
            new_args = ["errorParm_t::ERR_DROP", " " + args[0].lstrip()] + args[1:]
        else:
            fmt = args[fmt_i] if fmt_i < len(args) else ""
            if target == "message":
                target = target_of_message(fmt)
            new_fmt = fmt
            if target in ("Com_Warning", "Com_PrintError"):
                stripped = strip_level(fmt)
                if stripped is None:
                    log.append({"file": path, "offset": m.start(), "callee": callee, "skipped": "format"})
                    continue
                new_fmt = stripped
            new_args = args[fmt_i:]
            new_args[0] = new_fmt
        if target == callee and new_args == args:
            continue
        line = text.count("\n", 0, m.start()) + 1
        old_vis = visible(tokens(args[fmt_i])) if fmt_i < len(args) else ""
        new_vis = visible(tokens(new_args[0])) if new_args else ""
        old_prefix = old_prefixes.get(level, old_prefixes.get(None))
        log.append({"file": path, "line": line, "callee": callee, "level": level, "target": target,
                    "before": (old_prefix if old_prefix is not None else "") + old_vis,
                    "after": NEW_PREFIX.get(target, "") + new_vis})
        out.append(text[pos:m.start()])
        # with the level's argument removed, the format loses the space that followed its comma
        first_arg = new_args[0].lstrip() if has_level else new_args[0]
        out.append(target + "(" + first_arg + "".join("," + a for a in new_args[1:]) + ")")
        pos = close + 1
    out.append(text[pos:])
    return "".join(out)


def main():
    repo, changes, rules = Path(sys.argv[1]), Path(sys.argv[2]), RULES[sys.argv[3]]
    files = []
    for a in sys.argv[4:]:
        p = repo / a
        files += sorted(p.rglob("*")) if p.is_dir() else [p]
    log = []
    for f in files:
        if f.suffix not in (".cpp", ".h", ".c") or not f.is_file():
            continue
        rel = f.relative_to(repo).as_posix()
        text = f.read_bytes().decode("latin-1")
        new = convert(text, rules, rel, log)
        if new != text:
            f.write_bytes(new.encode("latin-1"))
    with changes.open("ab") as fh:
        for e in log:
            fh.write((json.dumps(e) + "\n").encode("utf-8"))
    print(len([e for e in log if "skipped" not in e]), "calls changed,", len([e for e in log if "skipped" in e]),
          "skipped")


if __name__ == "__main__":
    main()

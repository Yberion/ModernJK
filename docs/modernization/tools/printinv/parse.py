"""Reads every call and declaration run_tidy.py found at the place clang-tidy gives: the callee as written
(`ri.Printf`, `trap->Print`, `Quake3Game()->DebugPrint`), the arguments split at their top-level commas, the targets
that compile it. A call written in a macro's body is reported at the macro's use (`DEBUGNAME(...)`).

Usage: python -I parse.py <repository> <out.json> <run_tidy.json> [<run_tidy.json>...]
"""
import json
import re
import sys
from pathlib import Path

repo = Path(sys.argv[1])
cache = {}


def lines_of(p):
    # latin-1 keeps one character per byte, so that clang-tidy's columns (bytes) stay right
    if p not in cache:
        cache[p] = (repo / p).read_bytes().decode("latin-1").split("\n")
    return cache[p]


def text_at(p, ln, col):
    ls = lines_of(p)
    return "\n".join([ls[ln - 1][col - 1:]] + ls[ln:ln + 80])


def skip_group(t, i):
    """The index after the parenthesised group that starts at t[i]."""
    d = 0
    while i < len(t):
        d += t[i] == "("
        d -= t[i] == ")"
        i += 1
        if d == 0:
            break
    return i


def split_args(t, i):
    """The arguments of the list whose '(' is before t[i], or None if it does not close."""
    args, cur, depth, n = [], [], 0, len(t)
    while i < n:
        c = t[i]
        if c in "\"'":
            j = i + 1
            while j < n and t[j] != c:
                j += 2 if t[j] == "\\" else 1
            cur.append(t[i:j + 1])
            i = j + 1
            continue
        if t.startswith("//", i):
            j = t.find("\n", i)
            i = n if j < 0 else j
            continue
        if t.startswith("/*", i):
            j = t.find("*/", i)
            i = n if j < 0 else j + 2
            continue
        if c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                args.append("".join(cur).strip())
                return [re.sub(r"\s+", " ", a) for a in args]
            depth -= 1
        elif c == "," and depth == 0:
            args.append("".join(cur).strip())
            cur = []
            i += 1
            continue
        cur.append(c)
        i += 1
    return None


def split_call(t):
    """t starts at the call: (the callee's text, its arguments), or None."""
    i, n = 0, len(t)
    while i < n:
        if t[i] in ";{}":
            return None
        if t[i] == "(":
            j = skip_group(t, i)
            grouping = not re.search(r"\w$", t[:i].rstrip())
            # a group before the callee's name, or a call whose result is the object of a member access
            if grouping or re.match(r"\s*(->|\.)", t[j:]):
                i = j
                continue
            break
        i += 1
    if i >= n:
        return None
    args = split_args(t, i + 1)
    return (t[:i].strip(), args) if args is not None else None


def decl_info(p, ln, col):
    t = text_at(p, ln, col)
    t = t[: t.find(";") + 1] if ";" in t else t[:600]
    m = re.search(r"format\s*\(\s*printf\s*,\s*(\d+)\s*,\s*(\d+)\s*\)", t)
    nm = re.search(r"\(\s*(?:QDECL\s*)?\*\s*(\w+)\s*\)\s*\(", t) or re.search(r"(\w+)\s*\(", t)
    return {"name": nm.group(1) if nm else "?", "fmt": int(m.group(1)) if m else None,
            "text": re.sub(r"\s+", " ", t)[:200]}


def tree_of(p):
    parts = p.split("/")
    if parts[0] != "src":
        return parts[0]
    if parts[1] == "codemp" and parts[2] == "netcodec":
        return "netcodec"
    return parts[1] + "/" + parts[2] if len(parts) > 3 else parts[1]


def main():
    calls, decls = {}, {}
    for f in sys.argv[3:]:
        d = json.loads(Path(f).read_bytes())
        for k, v in d["calls"].items():
            calls.setdefault(k, set()).update(v)
        for k, v in d["decls"].items():
            decls.setdefault(k, set()).update(v)
    out_decls = []
    for k, v in sorted(decls.items()):
        p, ln, col = k.rsplit(":", 2)
        info = decl_info(p, int(ln), int(col))
        info.update(loc=k, targets=sorted(v))
        out_decls.append(info)
    out_calls, bad = [], []
    for k, v in sorted(calls.items()):
        p, ln, col = k.rsplit(":", 2)
        r = split_call(text_at(p, int(ln), int(col)))
        if not r or not re.search(r"\w\s*$", r[0]):
            bad.append({"loc": k, "text": text_at(p, int(ln), int(col))[:120]})
            continue
        callee = re.sub(r"\s+", "", r[0])
        out_calls.append({"loc": k, "tree": tree_of(p), "callee": callee, "name": re.findall(r"\w+", callee)[-1],
                          "args": r[1], "targets": sorted(v)})
    out = {"decls": out_decls, "calls": out_calls, "bad": bad}
    Path(sys.argv[2]).write_bytes(json.dumps(out, indent=1).encode("utf-8"))
    print("declarations", len(out_decls), "calls", len(out_calls), "not read", len(bad))


main()

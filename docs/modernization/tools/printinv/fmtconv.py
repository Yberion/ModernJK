"""Plan step 22 part 3: the printf-like calls rewritten into {fmt}'s syntax by clang-tidy's modernize-use-std-print and
modernize-use-std-format, which convert a call from its argument types (a %c given an int, a %u given an int, a %p
given a typed pointer) but only for a free function whose format is the first or the second argument, and refuse a
format holding a macro. So:

  prepass  every call the syntax tree found (parse.py's output) becomes QX_SHIM(QX_Pack_<id>(<object>, <arguments
           before the format>), <format>, <arguments>), one function clang-tidy converts for every callee at once; the
           macros of its format are hidden in its literal ("@@S_COLOR_YELLOW@@...")
  apply    the replacements clang-tidy exported (--export-fixes, one file per entry of the compile database), the
           identical ones once, applied from the end of each file; conflicting ones refused and listed
  post     QX_PRINT and QX_PRINTLN (clang-tidy's two replacement names) turned into the final functions, the line's end
           written back into the format of a function that writes its text as it is, the macros restored; a QX_SHIM
           left (a call clang-tidy refused) turned back into its original spelling and listed

Usage: python -I fmtconv.py prepass <repository> <parse.json> <log.json>
       python -I fmtconv.py apply <repository> <fixes folder> <log.json>
       python -I fmtconv.py post <repository> <log.json> <path>...
The sources are read and written in latin-1 (one character per byte, so that clang's byte offsets and columns hold).
"""
import json
import re
import sys
import time
from pathlib import Path

# callee -> (the arguments before the format, the function for a format without its line's end, the function for a
# format that ended its line, whether that function writes its text as it is: the end of line written back)
SHIMS = {
    "Com_Printf": (0, "Q_Print", "Q_Println", False),
    "Com_DPrintf": (0, "QX_DPrintPart", "Q_DPrintln", False),
    "Com_Warning": (0, "QX_WarningPart", "Q_PrintlnWarning", False),
    "Com_PrintError": (0, "QX_ErrorPart", "Q_PrintlnError", False),
    "Com_Error": (1, "Q_Error", "Q_Error", False),
    "Com_sprintf": (2, "Q_FormatTo", "Q_FormatTo", True),
    "sprintf": (1, "Q_FormatTo", "Q_FormatTo", True),
    "fprintf": (1, "Q_FilePrint", "Q_FilePrintln", False),
    "printf": (0, "Q_FilePrint", "Q_FilePrintln", False),
    "Q3_DebugPrint": (1, "Q3_DebugPrintln", "Q3_DebugPrintln", False),
    "DebugPrint": (1, "DebugPrintln", "DebugPrintln", False),
    "G_DebugPrint": (1, "G_DebugPrintln", "G_DebugPrintln", False),
    "I_DPrintf": (1, "I_DPrintln", "I_DPrintln", False),
    "SourceError": (1, "SourceError", "SourceError", False),
    "SourceWarning": (1, "SourceWarning", "SourceWarning", False),
    "ScriptError": (1, "ScriptError", "ScriptError", False),
    "ScriptWarning": (1, "ScriptWarning", "ScriptWarning", False),
    "PC_SourceError": (1, "PC_SourceError", "PC_SourceError", False),
    "COM_ParseWarning": (0, "COM_ParseWarning", "COM_ParseWarning", False),
    "SendServerCommand": (1, "SendServerCommand", "SendServerCommand", True),
    "G_LogPrintf": (0, "QX_LogPrintPart", "G_LogPrintln", False),
    "G_SecurityLogPrintf": (0, "QX_LogPrintPart", "G_SecurityLogPrintln", False),
    "Log_Write": (0, "Log_Write", "Log_Write", True),
    "FS_Printf": (1, "FS_Print", "FS_Print", True),
    "NET_OutOfBandPrint": (2, "NET_OutOfBandPrint", "NET_OutOfBandPrint", True),
    "SV_SendServerCommand": (1, "SV_SendServerCommand", "SV_SendServerCommand", True),
    "Boba_Printf": (0, "Boba_Println", "Boba_Println", False),
    "ProfilePrint": (0, "ProfilePrintln", "ProfilePrintln", False),
    "Debug_Printf": (2, "Debug_Println", "Debug_Println", False),
    "Sys_Error": (0, "Sys_Error", "Sys_Error", False),
    "I_CenterPrint": (0, "I_CenterPrint", "I_CenterPrint", True),
    "CenterPrint": (0, "CenterPrint", "CenterPrint", True),
}
# printf writes to the standard output: its final function takes the file first
STDOUT = {"printf"}
# the functions that do not return, shimmed by a function that does not either (a function declared not to return
# calls them last)
NORETURN = {"Com_Error", "Sys_Error"}
PRI = re.compile(r"(PRI|SCN)[diouxX](8|16|32|64|PTR|MAX|LEAST\d+|FAST\d+)$")


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


def read(p):
    return p.read_bytes().decode("latin-1")


def write(p, s):
    # a file held for an instant by another program (an editor, an antivirus) refuses the first open on Windows
    for attempt in range(20):
        try:
            p.write_bytes(s.encode("latin-1"))
            return
        except OSError:
            if attempt == 19:
                raise
            time.sleep(0.25)


def offset_of(text, line, col):
    i = 0
    for _ in range(line - 1):
        i = text.index("\n", i) + 1
    return i + col - 1


def split_callee(callee):
    """(object, separator, name) of a callee as written, whitespace removed."""
    c = re.sub(r"\s+", "", callee)
    m = re.match(r"^(.*)(->|\.)(\w+)$", c)
    if m and not c.startswith("std::"):
        return m.group(1), m.group(2), m.group(3)
    return None, None, re.sub(r"^(::|std::)", "", c)


def match_callee(text, i, callee):
    """The index after the callee written from text[i], whitespace allowed between its characters, or None."""
    for ch in re.sub(r"\s+", "", callee):
        while text[i] in " \t\r\n":
            i += 1
        if text[i] != ch:
            return None
        i += 1
    return i


def text_macros(repo):
    """The macros defined as one string literal without a conversion (S_COLOR_YELLOW "^3", DLL_EXT ".dll"), which
    can be hidden in a format; a name defined anywhere with a % (EXTERNAL_LIGHTMAP "lm_%04d.tga") is left out."""
    define = re.compile(r'^\s*#\s*define\s+([A-Za-z_]\w*)\s+"((?:[^"\\]|\\.)*)"\s*(?://.*|/\*.*)?$', re.M)
    ok, bad = set(), set()
    for p in (repo / "src").rglob("*"):
        if p.suffix in (".h", ".cpp", ".c"):
            for m in define.finditer(read(p)):
                (bad if "%" in m.group(2) or "{" in m.group(2) or "}" in m.group(2) else ok).add(m.group(1))
    return ok - bad


def hide_macros(fmt, macros):
    """A format of literals and text macros (a colour, a file extension) made one literal, each macro kept as
    @@NAME@@ in it, so that clang-tidy converts it; None when it holds something else or no literal (a variable)."""
    parts, rest, hid, literal = [], fmt.strip(), False, False
    lead = fmt[:len(fmt) - len(fmt.lstrip())]
    while rest:
        m = re.match(r'"((?:[^"\\]|\\.)*)"\s*', rest)
        if m:
            parts.append(m.group(1))
            literal = True
        else:
            m = re.match(r"([A-Za-z_]\w*)\s*", rest)
            if not m or m.group(1) not in macros:
                return None
            parts.append("@@" + m.group(1) + "@@")
            hid = True
        rest = rest[m.end():]
    return lead + '"' + "".join(parts) + '"' if hid and literal else None


def prepass(repo, parse_json, log):
    calls = json.loads(Path(parse_json).read_bytes())["calls"]
    macros = text_macros(repo)
    by_file = {}
    for x in calls:
        obj, sep, name = split_callee(x["callee"])
        # netcodec and the tests do not include q_fmt.h (converted on {fmt} directly), nor Ravl: by hand
        if name not in SHIMS or x["loc"].startswith(("src/codemp/netcodec/", "test/", "src/code/Ravl/")):
            continue
        p, ln, col = x["loc"].rsplit(":", 2)
        by_file.setdefault(p, []).append((int(ln), int(col), x["callee"], obj, sep, name))
    done, skipped, packs = 0, [], set()
    for p, items in sorted(by_file.items()):
        path = repo / p
        text = read(path)
        edits = []
        for ln, col, callee, obj, sep, name in set(items):
            i = offset_of(text, ln, col)
            j = match_callee(text, i, callee)
            if j is None:
                skipped.append(f"{p}:{ln}:{col} callee not found")
                continue
            k = j
            while text[k] in " \t\r\n":
                k += 1
            if text[k] != "(":
                skipped.append(f"{p}:{ln}:{col} no call")
                continue
            r = arg_spans(text, k)
            nargs, _, _, _ = SHIMS[name]
            if not r or len(r[0]) <= nargs:
                skipped.append(f"{p}:{ln}:{col} arguments")
                continue
            spans = r[0]
            fmt_start, fmt_end = spans[nargs]
            fmt = text[fmt_start:fmt_end]
            hidden = hide_macros(fmt, macros)
            pack_args = ([obj] if obj else []) + [text[s:e].strip() for s, e in spans[:nargs]]
            kind = {None: "", "->": "Arrow", ".": "Dot"}[sep]
            pack = f"QX_Pack{kind}_{name}"
            packs.add(pack)
            shim = "QX_SHIM_NR" if name in NORETURN else "QX_SHIM"
            head = f"{shim}({pack}(" + ", ".join(pack_args) + "), "
            # the text from the callee to the format, then the format (its macros hidden)
            edits.append((i, fmt_start, head))
            if hidden is not None:
                edits.append((fmt_start, fmt_end, hidden))
            done += 1
        for s, e, new in sorted(edits, key=lambda x: -x[0]):
            text = text[:s] + new + text[e:]
        write(path, text)
    Path(log).write_bytes(json.dumps({"converted": done, "skipped": skipped, "packs": sorted(packs)}, indent=1).encode())
    print("shimmed", done, "skipped", len(skipped))
    # the declarations clang-tidy needs to parse the shims, for q_fmt.h while the conversion runs
    # int: a call whose result is used still compiles, and clang-tidy leaves it (it converts only a statement)
    decl = ["struct QXPack {};", "int QX_SHIM(QXPack, const char*, ...) __attribute__((format(printf, 2, 3)));",
            "NORETURN int QX_SHIM_NR(QXPack, const char*, ...) __attribute__((format(printf, 2, 3)));"]
    decl += [f"template <typename... A> QXPack {pk}(const A&...);" for pk in sorted(packs)]
    print("\n".join(decl))


def yaml_scalar(s):
    """A scalar of clang's YAML: plain, 'single quoted' ('' for a quote) or "double quoted" (escapes)."""
    s = s.strip()
    if s.startswith("'"):
        return s[1:-1].replace("''", "'")
    if s.startswith('"'):
        out, i, body = [], 0, s[1:-1]
        esc = {"n": "\n", "t": "\t", "r": "\r", '"': '"', "\\": "\\", "0": "\0", "/": "/", " ": " "}
        while i < len(body):
            c = body[i]
            if c == "\\":
                d = body[i + 1]
                if d == "x":
                    out.append(chr(int(body[i + 2:i + 4], 16)))
                    i += 4
                    continue
                if d == "u":
                    out.append(chr(int(body[i + 2:i + 6], 16)))
                    i += 6
                    continue
                out.append(esc[d])
                i += 2
                continue
            out.append(c)
            i += 1
        return "".join(out)
    return s




def apply(repo, fixes_dir, log):
    reps = {}
    for f in sorted(Path(fixes_dir).rglob("*.yaml")):
        lines = f.read_bytes().decode("utf-8").split("\n")
        # a replacement is four lines in a row: - FilePath, Offset, Length, ReplacementText (clang writes each
        # scalar on one line, a text holding a line's end in double quotes with escapes); only the conversions'
        # own: a compiler error's fix-it ("did you mean ...?") would rename a function not yet declared
        check = ""
        for n in range(len(lines) - 3):
            d = re.match(r"^\s*- DiagnosticName:\s*(\S+)", lines[n])
            if d:
                check = d.group(1)
            if not check.startswith("modernize-"):
                continue
            m0 = re.match(r"^\s*- FilePath:\s*(.*)$", lines[n])
            m1 = re.match(r"^\s*Offset:\s*(\d+)\s*$", lines[n + 1])
            m2 = re.match(r"^\s*Length:\s*(\d+)\s*$", lines[n + 2])
            m3 = re.match(r"^\s*ReplacementText:\s*(.*)$", lines[n + 3])
            if not (m0 and m1 and m2 and m3):
                continue
            path = yaml_scalar(m0.group(1)).replace("\\", "/")
            text = yaml_scalar(m3.group(1))
            # the include of an empty header name (the options that name none)
            if text.startswith('#include ""'):
                continue
            reps.setdefault(path.lower(), (path, set()))[1].add((int(m1.group(1)), int(m2.group(1)), text))
    applied, conflicts = 0, []
    for key, (path, rs) in sorted(reps.items()):
        rs = sorted(rs, key=lambda r: (r[0], r[1]))
        keep = []
        for r in rs:
            if keep and r[0] < keep[-1][0] + keep[-1][1]:
                conflicts.append(f"{path}:{r[0]} overlaps {keep[-1][0]}")
                continue
            if keep and r[0] == keep[-1][0] and r[1] == 0 and keep[-1][1] == 0:
                conflicts.append(f"{path}:{r[0]} two insertions")
                continue
            keep.append(r)
        p = Path(path)
        text = p.read_bytes().decode("latin-1")
        for off, length, new in sorted(keep, key=lambda r: -r[0]):
            text = text[:off] + new.encode("utf-8").decode("latin-1") + text[off + length:]
            applied += 1
        write(p, text)
    Path(log).write_bytes(json.dumps({"applied": applied, "conflicts": conflicts}, indent=1).encode())
    print("applied", applied, "conflicts", len(conflicts))


def last_literal_end(arg):
    """The index of the closing quote of the format's last literal."""
    j, end = 0, None
    while j < len(arg):
        if arg[j] == '"':
            k = skip_string(arg, j)
            end = k - 1
            j = k
            continue
        j += 1
    return end


def unhide(text):
    """Every "...@@NAME@@..." literal split back into its literals and macros."""
    def split(m):
        body = m.group(1)
        if "@@" not in body:
            return m.group(0)
        pieces = re.split(r"@@([A-Za-z_]\w*)@@", body)
        out = []
        for n, piece in enumerate(pieces):
            if n % 2:
                out.append(piece)
            elif piece:
                out.append('"' + piece + '"')
        return " ".join(out)
    lines = text.split("\n")
    for n, line in enumerate(lines):
        if re.search(r"@@[A-Za-z_]\w*@@", line):
            lines[n] = re.sub(r'"((?:[^"\\]|\\.)*)"', split, line)
    return "\n".join(lines)


def final_call(name, kind, pack_args, fmt_args, println):
    nargs, part, line, asis = SHIMS[name]
    fn = line if println else part
    obj, rest = (pack_args[0], pack_args[1:]) if kind else (None, pack_args)
    if name in ("Com_sprintf",):
        dest, size = rest
        m = re.match(r"sizeof\s*\(?\s*(.+?)\s*\)?$", size)
        span = dest if m and m.group(1) == dest else f"std::span({dest}, {size})"
        rest = [span]
    if name in STDOUT:
        rest = ["stdout"]
    callee = f"{obj}{'->' if kind == 'Arrow' else '.'}{fn}" if obj else fn
    return callee, rest


def post(repo, log, paths):
    files = []
    for a in paths:
        p = repo / a
        files += [p] if p.is_file() else [q for q in p.rglob("*") if q.suffix in (".cpp", ".h", ".c", ".hpp", ".inl")]
    out, left = 0, []
    call = re.compile(r"\bQX_(PRINTLN|PRINT|SHIM_NR|SHIM)\s*\(")
    for p in files:
        text = read(p)
        if "QX_" not in text and "@@" not in text:
            continue
        pos = 0
        while True:
            m = call.search(text, pos)
            if not m:
                break
            r = arg_spans(text, m.end() - 1)
            spans, close = r
            args = [text[s:e] for s, e in spans]
            pm = re.match(r"\s*QX_Pack(Arrow|Dot|)_(\w+)\s*\(", args[0])
            if not pm:
                # not a shimmed call (a declaration)
                pos = m.end()
                continue
            pos = m.start()
            ps = arg_spans(args[0], pm.end() - 1)
            raw = [args[0][s:e].strip() for s, e in ps[0]]
            pack_args = [] if raw == [""] else raw
            kind, name = pm.group(1), pm.group(2)
            rest_args = args[1:]
            if m.group(1) in ("SHIM", "SHIM_NR"):
                # refused by clang-tidy: back to its spelling, converted by hand
                nargs = SHIMS[name][0]
                obj, others = (pack_args[0], pack_args[1:]) if kind else (None, pack_args)
                callee = (f"{obj}{'->' if kind == 'Arrow' else '.'}{name}" if obj else name)
                new = callee + "(" + ", ".join(others + [a.strip() for a in rest_args]) + ")"
                line = text.count("\n", 0, m.start()) + 1
                left.append(f"{p.relative_to(repo).as_posix()}:{line} {name}")
            else:
                println = m.group(1) == "PRINTLN"
                callee, first = final_call(name, kind, pack_args, rest_args, println)
                fmt = rest_args[0]
                if println and SHIMS[name][3]:
                    e = last_literal_end(fmt)
                    fmt = fmt[:e] + "\\n" + fmt[e:]
                new = callee + "(" + ", ".join(first + [fmt.strip()] + [a.strip() for a in rest_args[1:]]) + ")"
                out += 1
            text = text[:m.start()] + new + text[close + 1:]
        text = unhide(text)
        write(p, text)
    Path(log).write_bytes(json.dumps({"converted": out, "left": left}, indent=1).encode())
    print("converted", out, "left as they were", len(left))


def main():
    cmd, repo = sys.argv[1], Path(sys.argv[2])
    if cmd == "prepass":
        prepass(repo, sys.argv[3], sys.argv[4])
    elif cmd == "apply":
        apply(repo, sys.argv[3], sys.argv[4])
    elif cmd == "post":
        post(repo, sys.argv[3], sys.argv[4:])


if __name__ == "__main__":
    main()

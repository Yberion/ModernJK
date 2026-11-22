"""Plan step 22 part 3: va returned a char* to a static buffer; Q_Format returns a formatText_t, which a function taking
a C string reads through c_str(). Appends .c_str() to every Q_Format call but those given to a printing or formatting
function (formatText_t is formatted as its characters there) and those whose text is kept (an initialiser, an
assignment, a return: the object must be kept instead, by hand), which are listed.

Usage: python -I cstr.py <repository> <log.json> <path>...
"""
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fmtconv import arg_spans, read, write  # noqa: E402

# the functions that take a format and its arguments: a formatText_t among the arguments is formatted as its text
FMT_FUNCTIONS = {"Q_Print", "Q_Println", "Q_DPrintln", "Q_PrintlnWarning", "Q_PrintlnError", "Q_Error", "Q_Format",
                 "Q_FormatTo", "Q_FilePrint", "Q_FilePrintln", "DebugPrintln", "Q3_DebugPrintln", "G_DebugPrintln",
                 "I_DPrintln", "G_LogPrintln", "G_SecurityLogPrintln", "Log_Write", "FS_Print", "NET_OutOfBandPrint",
                 "SV_SendServerCommand", "SourceError", "SourceWarning", "ScriptError", "ScriptWarning",
                 "PC_SourceError", "COM_ParseWarning", "Boba_Println", "ProfilePrintln", "Debug_Println", "Sys_Error",
                 "I_CenterPrint", "CenterPrint", "gi.SendServerCommand"}
CALL = re.compile(r"\bQ_Format\s*\(")


def enclosing_callee(text, i):
    """The callee of the call whose argument list holds text[i] directly, or None."""
    depth, j = 0, i - 1
    while j >= 0:
        c = text[j]
        if c in ")]}":
            depth += 1
        elif c in "([{":
            if depth == 0:
                if c != "(":
                    return None
                m = re.search(r"([\w.]+(?:->\w+)?)\s*$", text[:j])
                return m.group(1) if m else None
            depth -= 1
        elif c in ";" and depth == 0:
            return None
        j -= 1
    return None


def keep(repo, log_in, log_out):
    """The texts kept in a local const char*: a formatText_t declared after the variable holds the text as long as
    the variable may point into it, `s = (sText = Q_Format(...)).c_str();` (an expression, right under an if
    without braces too); a variable declared elsewhere (a parameter, a global) is listed."""
    kept = json.loads(Path(log_in).read_bytes())["kept"]
    by_file, left, done = {}, [], 0
    for w in kept:
        p, ln = w.rsplit(":", 1)
        by_file.setdefault(p, []).append(int(ln))
    for p, lines in sorted(by_file.items()):
        path = repo / p
        text = read(path)
        starts = [0] + [i + 1 for i, c in enumerate(text) if c == "\n"]
        inserts, edits = {}, []
        for ln in lines:
            s = starts[ln - 1]
            m = re.compile(r"\b(\w+)\s*=\s*(Q_Format\s*\()").search(text, s, text.find("\n", s))
            if not m:
                left.append(f"{p}:{ln}")
                continue
            name = m.group(1)
            decl = None
            for d in re.finditer(r"\bchar\s*\*\s*(?:const\s+)?" + name + r"\b\s*(?:=[^;,]*)?[;,]", text[:s]):
                decl = d
            if not decl:
                left.append(f"{p}:{ln} {name}")
                continue
            end = text.index(";", decl.start()) + 1
            line_start = text.rfind("\n", 0, decl.start()) + 1
            indent = re.match(r"[ \t]*", text[line_start:]).group(0)
            inserts[end] = f"\n{indent}formatText_t {name}Text; // holds the text {name} points into"
            r = arg_spans(text, m.end() - 1)
            edits.append((m.start(2), r[1] + 1, name))
            done += 1
        changes = [(e, e, t) for e, t in inserts.items()]
        for start, close, name in edits:
            changes.append((start, close, f"({name}Text = {text[start:close]}).c_str()"))
        for a, b, new in sorted(changes, key=lambda x: -x[0]):
            text = text[:a] + new + text[b:]
        write(path, text)
    Path(log_out).write_bytes(json.dumps({"kept": done, "left": left}, indent=1).encode())
    print("texts kept in a formatText_t", done, "left, by hand", len(left))


def main():
    if sys.argv[1] == "keep":
        keep(Path(sys.argv[2]), sys.argv[3], sys.argv[4])
        return
    repo, log = Path(sys.argv[1]), sys.argv[2]
    files = []
    for a in sys.argv[3:]:
        p = repo / a
        files += [p] if p.is_file() else [q for q in p.rglob("*") if q.suffix in (".cpp", ".h", ".c")]
    added, kept, inside = 0, [], 0
    for p in sorted(files):
        text = read(p)
        # the header declares Q_Format, it calls nothing
        if "Q_Format(" not in text or p.name in ("q_fmt.h", "q_fmt.cpp"):
            continue
        edits = []
        for m in CALL.finditer(text):
            r = arg_spans(text, m.end() - 1)
            if not r:
                continue
            close = r[1]
            after = text[close + 1:close + 12]
            if after.startswith((".c_str()", ".size()")):
                continue
            where = f"{p.relative_to(repo).as_posix()}:{text.count(chr(10), 0, m.start()) + 1}"
            b = m.start() - 1
            while b >= 0 and text[b] in " \t\r\n":
                b -= 1
            before = text[max(0, b - 6):b + 1]
            if (text[b] == "=" and text[b - 1] not in "=!<>") or before.endswith("return"):
                kept.append(where)
                continue
            callee = enclosing_callee(text, m.start())
            name = callee.split("->")[-1] if callee else None
            if callee and (callee in FMT_FUNCTIONS or (name in FMT_FUNCTIONS and name != "SendServerCommand")):
                inside += 1
                continue
            edits.append(close + 1)
        for e in reversed(edits):
            text = text[:e] + ".c_str()" + text[e:]
            added += 1
        if edits:
            write(p, text)
    Path(log).write_bytes(json.dumps({"added": added, "kept": kept, "formatted": inside}, indent=1).encode())
    print(".c_str() added", added, "given to a formatting function", inside, "kept, by hand", len(kept))


if __name__ == "__main__":
    main()

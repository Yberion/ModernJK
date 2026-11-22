"""Plan step 22 part 3: the printf-like calls clang-tidy cannot see (the branches the builds do not compile, the files
no target compiles, the commented-out code) rewritten into {fmt}'s syntax by text, onto the same functions as
fmtconv.py. Without the argument types, a conversion whose output depends on them (%u, %x, %X, %o of a signed value,
%p of a typed pointer, %h of an int) is written as clang-tidy writes it for the usual type and listed to be read by
hand; a call it cannot convert (a format that is not literals and text macros, %*, %n, a precision on an integer, a
commented-out call on several lines) is left as it is and listed.

Usage: python -I fmttext.py <repository> <log.json> <path>...
"""
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fmtconv import SHIMS, STDOUT, arg_spans, read, skip_string, write  # noqa: E402

SPEC = re.compile(r"%([-+ #0]*)(\*|\d+)?(?:\.(\*|\d+))?(hh|h|ll|l|j|z|t|L|I64)?([diouxXeEfFgGaAcspn])")
TYPED = {"u": "%u of a signed value", "x": "%x of a signed value", "X": "%X of a signed value",
         "o": "%o of a signed value", "p": "%p of a typed pointer"}
# the callees by their spelling: a name, or an object's member (-> or .)
NAMES = sorted(set(SHIMS) | {"va"}, key=len, reverse=True)
CALL = re.compile(r"(?<![\w])((?:[\w\]\)]+(?:->|\.))?)(" + "|".join(NAMES) + r")\s*\(")
SAME_NAME = {n for n, (_, part, line, _) in SHIMS.items() if n in (part, line)}


def convert_body(body, notes):
    """The content of one string literal converted; None when a conversion cannot be."""
    out, i = [], 0
    while i < len(body):
        ch = body[i]
        if ch == "\\":
            out.append(body[i:i + 2])
            i += 2
            continue
        if ch in "{}":
            out.append(ch * 2)
            i += 1
            continue
        if ch != "%":
            out.append(ch)
            i += 1
            continue
        if body[i + 1:i + 2] == "%":
            out.append("%")
            i += 2
            continue
        m = SPEC.match(body, i)
        if not m:
            return None
        fl, w, p, ln, cv = m.groups()
        if w == "*" or p == "*" or cv == "n" or (p and cv in "diouxXc"):
            return None
        if cv in TYPED:
            notes.add(TYPED[cv])
        if ln in ("h", "hh"):
            notes.add("%h of an int")
        spec = ""
        if cv == "s":
            spec += ("" if "-" in fl else ">") if w else ""
        elif "-" in fl:
            spec += "<"
        spec += "+" if "+" in fl else (" " if " " in fl else "")
        spec += "#" if "#" in fl else ""
        spec += "0" if "0" in fl and "-" not in fl and cv != "s" else ""
        spec += w or ""
        spec += "." + p if p else ""
        spec += {"d": "", "i": "", "u": "", "s": "", "p": "", "c": "c", "f": "f", "F": "F", "e": "e", "E": "E",
                 "g": "g", "G": "G", "a": "a", "A": "A", "x": "x", "X": "X", "o": "o"}[cv]
        out.append("{:" + spec + "}" if spec else "{}")
        i = m.end()
    return "".join(out)


def convert_format(fmt, notes):
    """The format argument (literals and macros) converted, and whether it ended its line; None if it cannot be."""
    pieces, j, last = [], 0, None
    while j < len(fmt):
        c = fmt[j]
        if c == '"':
            k = skip_string(fmt, j)
            body = convert_body(fmt[j + 1:k - 1], notes)
            if body is None:
                return None
            pieces.append('"' + body + '"')
            last = len(pieces) - 1
            j = k
            continue
        m = re.match(r"\s+|[A-Za-z_]\w*", fmt[j:])
        if not m:
            return None
        if m.group(0)[0].isalpha() or m.group(0)[0] == "_":
            if re.match(r"(PRI|SCN)", m.group(0)):
                return None
        pieces.append(m.group(0))
        j += m.end()
    if last is None:
        return None
    ended = pieces[last].endswith('\\n"')
    return pieces, last, ended


def rewrite(text, m, notes):
    """The call at m rewritten, or None."""
    obj, name = m.group(1), m.group(2)
    open_ = m.end() - 1
    r = arg_spans(text, open_)
    if not r:
        return None
    spans, close = r
    args = [text[s:e] for s, e in spans]
    if name == "va":
        nargs, fn_part, fn_line, asis = 0, "Q_Format", "Q_Format", True
    else:
        nargs, fn_part, fn_line, asis = SHIMS[name]
    if len(args) <= nargs:
        return None
    # a function whose name both syntaxes share: a format with a brace is in {fmt}'s already
    if name in SAME_NAME and "{" in args[nargs]:
        return None
    conv = convert_format(args[nargs], notes)
    if conv is None:
        return None
    pieces, last, ended = conv
    if ended and not asis:
        pieces[last] = pieces[last][:-3] + '"'
    fmt = "".join(pieces).strip()
    fn = fn_line if ended else fn_part
    before = [a.strip() for a in args[:nargs]]
    if name == "Com_sprintf":
        dest, size = before
        sm = re.match(r"sizeof\s*\(?\s*(.+?)\s*\)?$", size)
        before = [dest if sm and sm.group(1) == dest else f"std::span({dest}, {size})"]
    if name in STDOUT:
        before = ["stdout"]
    new = obj + fn + "(" + ", ".join(before + [fmt] + [a.strip() for a in args[nargs + 1:]]) + ")"
    return new, close


def main():
    repo, log = Path(sys.argv[1]), sys.argv[2]
    files = []
    for a in sys.argv[3:]:
        p = repo / a
        files += [p] if p.is_file() else [q for q in p.rglob("*") if q.suffix in (".cpp", ".h", ".c")]
    # netcodec and the tests call {fmt} directly (clang-tidy's run of fmtconv_fmt.yaml)
    files = [p for p in files if "netcodec" not in p.parts and "test" not in p.relative_to(repo).parts[:1]]
    done, left, typed = 0, [], []
    for p in sorted(files):
        text = read(p)
        out, pos, changed = [], 0, False
        for m in CALL.finditer(text):
            if m.start() < pos:
                continue
            line_start = text.rfind("\n", 0, m.start()) + 1
            prefix = text[line_start:m.start()]
            # a declaration or definition, not a call
            if re.search(r"(\bvoid|\bint|\bchar\s*\*|QDECL|NORETURN|::)\s*$", prefix):
                continue
            where = f"{p.relative_to(repo).as_posix()}:{text.count(chr(10), 0, m.start()) + 1}"
            r = arg_spans(text, m.end() - 1)
            span_text = text[m.start():r[1] + 1] if r else ""
            # a commented-out call on several lines, or one whose arguments leave a /* */ comment
            if "//" in prefix and "\n" in span_text or "*/" in span_text:
                left.append(where + " commented on several lines")
                continue
            notes = set()
            res = rewrite(text, m, notes)
            if res is None:
                # already in {fmt}'s syntax (a name both syntaxes share), or not convertible
                if r and re.search(r"%[-+ #0]*[\d*]*(?:\.[\d*]+)?[hlLzjt]*[a-zA-Z]", span_text):
                    left.append(where + " " + m.group(2))
                continue
            new, close = res
            if new == text[m.start():close + 1]:
                continue
            out.append((m.start(), close + 1, new))
            pos = close + 1
            done += 1
            for n in sorted(notes):
                typed.append(where + " " + n)
        for s, e, new in reversed(out):
            text = text[:s] + new + text[e:]
            changed = True
        if changed:
            write(p, text)
    Path(log).write_bytes(json.dumps({"converted": done, "left": left, "typed": typed}, indent=1).encode())
    print("converted", done, "left", len(left), "to read for their types", len(typed))


if __name__ == "__main__":
    main()

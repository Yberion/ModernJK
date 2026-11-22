"""Plan step 22 part 3: a printf-like free function rewritten onto {fmt}'s documented shape: a template taking the
checked format (fmt::format_string) that calls a function taking the packed arguments (fmt::format_args), where the
old body formatted with va_start and Q_vsnprintf or vfprintf. Its declaration becomes the function's declaration and
the template; its definition becomes the function's, the va_list removed and the formatting done by Q_VFormatToCut
or Q_VFilePrint. A body of another shape is left and listed.

Usage: python -I vfunc.py <repository> <spec.json>
spec: [[old name, new name, packed function's name, definition file, [declaration files]], ...]; a function declared
only in its definition's file gets its declaration and template written before the definition.
"""
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from fmtconv import read, write  # noqa: E402

ATTR = r"(?:\s*__attribute__\s*\(\(\s*format\s*\(\s*printf\s*,\s*\d+\s*,\s*\d+\s*\)\s*\)\))?"


def signature(old):
    """A declaration or definition of old: (prefix, parameters before the format, the format's name)."""
    return re.compile(r"(?m)^(?P<lead>[ \t]*)(?P<pre>(?:extern\s+|static\s+)?(?:NORETURN\s+)?void\s+(?:NORETURN\s+)?(?:QDECL\s+)?)"
                      + re.escape(old) + r"\s*\((?P<params>[^()]*?)(?:,\s*)?(?:const\s+)?char\s*\*\s*(?P<fmt>\w+)\s*,\s*\.\.\.\s*\)"
                      + ATTR)


def names(params):
    out = []
    for p in [x.strip() for x in params.split(",") if x.strip()]:
        out.append(re.findall(r"\w+", p)[-1])
    return out


def template(lead, pre, new, vname, params, fmt):
    noret = "NORETURN " if "NORETURN" in pre else ""
    static = "static " if "static" in pre else ""
    p = params.strip().rstrip(",")
    sep = ", " if p else ""
    call = ", ".join(names(p) + [f"{fmt}.get()", "fmt::make_format_args(args...)"])
    return (f"{lead}{static}{noret}void {vname}({p}{sep}fmt::string_view {fmt}, fmt::format_args args);\n"
            f"{lead}template <typename... T> {noret}void {new}({p}{sep}fmt::format_string<T...> {fmt}, T&&... args)\n"
            f"{lead}{{\n{lead}    {vname}({call});\n{lead}}}")


def body_end(text, i):
    depth = 0
    for j in range(i, len(text)):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return j
    return None


def rewrite_body(body, fmt):
    ap = re.search(r"\bva_list\s+(\w+)\s*;[ \t]*\n?", body)
    if not ap:
        return None
    a = ap.group(1)
    body = body[:ap.start()] + body[ap.end():]
    q = re.escape(a)
    f = re.escape(fmt)

    def snprintf(m):
        buf, size = m.group(1).strip(), m.group(2).strip()
        sm = re.match(r"sizeof\s*\(?\s*(.+?)\s*\)?$", size)
        span = buf if sm and sm.group(1) == buf else f"std::span({buf}, {size})"
        return f"Q_VFormatToCut({span}, {fmt}, args);"

    body, n1 = re.subn(r"va_start\s*\(\s*" + q + r"\s*,\s*" + f + r"\s*\)\s*;\s*Q_vsnprintf\s*\(\s*([^,]+),\s*([^,]+),\s*"
                       + f + r"\s*,\s*" + q + r"\s*\)\s*;\s*va_end\s*\(\s*" + q + r"\s*\)\s*;", snprintf, body)
    body, n2 = re.subn(r"va_start\s*\(\s*" + q + r"\s*,\s*" + f + r"\s*\)\s*;\s*vfprintf\s*\(\s*([^,]+),\s*" + f
                       + r"\s*,\s*" + q + r"\s*\)\s*;\s*va_end\s*\(\s*" + q + r"\s*\)\s*;",
                       lambda m: f"Q_VFilePrint({m.group(1).strip()}, lineEnd_t::NONE, {fmt}, args);", body)
    if n1 + n2 != 1 or re.search(r"\bva_(start|end)\b|\b" + q + r"\b", body):
        return None
    return body


def main():
    repo = Path(sys.argv[1])
    spec = json.loads(Path(sys.argv[2]).read_bytes())
    left = []
    for old, new, vname, deffile, decls in spec:
        sig = signature(old)
        # the definition: its signature and body
        path = repo / deffile
        text = read(path)
        defs = [m for m in sig.finditer(text) if re.match(r"\s*\{", text[m.end():])]
        if len(defs) != 1:
            left.append(f"{deffile} {old}: {len(defs)} definitions")
            continue
        m = defs[0]
        start = text.index("{", m.end())
        end = body_end(text, start)
        body = rewrite_body(text[start:end + 1], m.group("fmt"))
        if body is None:
            left.append(f"{deffile} {old}: body")
            continue
        params = m.group("params").strip().rstrip(",")
        sep = ", " if params else ""
        pre = m.group("pre").replace("QDECL ", "").replace("extern ", "")
        head = f"{m.group('lead')}{pre}{vname}({params}{sep}fmt::string_view {m.group('fmt')}, fmt::format_args args)\n"
        local = not decls
        text = text[:m.start()] + (template(m.group("lead"), m.group("pre"), new, vname, params, m.group("fmt")) + "\n\n" if local else "") + head + body + text[end + 1:]
        # its other declarations in the file (a forward declaration with the attribute)
        for d in list(sig.finditer(text)):
            if text[d.end():].lstrip().startswith(";"):
                k = text.index(";", d.end())
                text = text[:d.start()] + template(d.group("lead"), d.group("pre"), new, vname, d.group("params"), d.group("fmt")) + text[k + 1:]
                break
        write(path, text)
        for df in decls:
            p = repo / df
            t = read(p)
            ds = [d for d in sig.finditer(t) if t[d.end():].lstrip().startswith(";")]
            if len(ds) != 1:
                left.append(f"{df} {old}: {len(ds)} declarations")
                continue
            d = ds[0]
            k = t.index(";", d.end())
            t = t[:d.start()] + template(d.group("lead"), d.group("pre"), new, vname, d.group("params"), d.group("fmt")) + t[k + 1:]
            write(p, t)
    print("left, by hand:", left)


if __name__ == "__main__":
    main()

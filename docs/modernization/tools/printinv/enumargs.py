"""Part 4: in the calls of the functions that take a checked format (fmt::format_string), an argument that is exactly
`to_underlying(X)` becomes `X`, the enumeration printed through q_enum.h's format_as. Commented-out calls the same.
netcodec is left out (it does not include q_enum.h). usage: python -I enumargs.py <repo> [--write]"""
import re
import subprocess
import sys
import time

root = sys.argv[1]
write = "--write" in sys.argv
out = subprocess.run(["git", "-C", root, "grep", "-h", "-oE", r"\b[A-Za-z_][A-Za-z0-9_]*\s*\([^()]*fmt::format_string<T\.\.\.>",
                      "--", "src"], capture_output=True, text=True).stdout
names = sorted({re.sub(r"\s*\(.*", "", l) for l in out.splitlines()})
CALL = re.compile(r"\b(" + "|".join(map(re.escape, names)) + r")\s*\(")
files = subprocess.run(["git", "-C", root, "grep", "-l", "-E", r"to_underlying\(", "--", "src", ":!src/codemp/netcodec"],
                       capture_output=True, text=True).stdout.split()


def args_of(text, start):
    """Top-level argument spans of the call whose '(' is at start-1: [(begin, end)], end of call."""
    depth, i, quote, spans, b = 0, start, None, [], start
    while i < len(text):
        c = text[i]
        if quote:
            if c == "\\":
                i += 1
            elif c == quote:
                quote = None
        elif c in "\"'":
            quote = c
        elif c in "([{":
            depth += 1
        elif c in ")]}":
            if depth == 0:
                spans.append((b, i))
                return spans, i
            depth -= 1
        elif c == "," and depth == 0:
            spans.append((b, i))
            b = i + 1
        i += 1
    return None, i


UNDER = re.compile(r"^(\s*)to_underlying\((.*)\)(\s*)$", re.S)
total = 0
for path in files:
    full = f"{root}/{path}"
    text = open(full, encoding="latin-1", newline="").read()
    edits = []
    for m in CALL.finditer(text):
        spans, end = args_of(text, m.end())
        if not spans:
            continue
        for b, e in spans:
            arg = text[b:e]
            u = UNDER.match(arg)
            if not u:
                continue
            inner = u.group(2)
            # the parentheses of to_underlying( ... ) must enclose the whole argument
            depth = 0
            balanced = True
            for c in inner:
                depth += c == "("
                depth -= c == ")"
                if depth < 0:
                    balanced = False
                    break
            if balanced and depth == 0:
                edits.append((b, e, u.group(1) + inner + u.group(3)))
    if not edits:
        continue
    edits = sorted(set(edits), reverse=True)
    total += len(edits)
    for b, e, new in edits:
        line = text.count("\n", 0, b) + 1
        if not write:
            print(f"{path}:{line}: {text[b:e].strip()[:90]}")
        text = text[:b] + new + text[e:]
    if write:
        for attempt in range(5):
            try:
                open(full, "w", encoding="latin-1", newline="").write(text)
                break
            except OSError:
                time.sleep(0.5)
print("functions:", len(names), "arguments:", total, file=sys.stderr)

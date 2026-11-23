"""The inventory of the sound's callers (plan step 21, part 2).

Lists every call of the sound's functions outside the sound's own files, in the three trees and the shared code:
the modules' calls through their import tables (trap->S_*, trap->AS_*, cgi_S_*, cgi_AS_*, the UI's DC-> entries) and
the engines' calls of the functions the sound files define. Each call is written with its file, line, enclosing
function and its arguments split at the top level, and whether it sits in a comment or an inactive #if 0 block.

    python -I docs/modernization/tools/soundinv/soundinv.py <repository> <output.tsv> [<symbols.tsv>]

The second output, when given, lists the functions the sound files define and how many calls each has outside them.
"""

import re
import sys
from pathlib import Path

# The sound's own files: their definitions are the sound's functions; their calls are not counted.
SOUND_FILES = {
    "mp": ["src/codemp/client/snd_ambient.cpp", "src/codemp/client/snd_dma.cpp", "src/codemp/client/snd_mem.cpp",
           "src/codemp/client/snd_mix.cpp", "src/codemp/client/snd_mp3.cpp", "src/codemp/client/snd_music.cpp",
           "src/codemp/null/null_snddma.cpp"],
    "sp": ["src/code/client/snd_ambient.cpp", "src/code/client/snd_dma.cpp", "src/code/client/snd_mem.cpp",
           "src/code/client/snd_mix.cpp", "src/code/client/cl_mp3.cpp", "src/code/client/snd_music.cpp"],
    "shared": ["src/shared/sdl/sdl_sound.cpp"],
}

# The trees searched for callers, and the name each call is filed under.
TREES = [("mp", "src/codemp"), ("ja", "src/code"), ("jo", "src/codeJK2"), ("shared", "src/shared")]

# The modules' entries: trap->S_X, trap->AS_X (multiplayer cgame and UI), cgi_S_X, cgi_AS_X (single-player cgame).
MODULE_CALL = re.compile(r"\b(trap->(?:S|AS)_\w+|cgi_(?:S|AS)_\w+|ui\.S_\w+|"
                         r"DC->(?:startLocalSound|registerSound|startBackgroundTrack|stopBackgroundTrack))\s*\(")

# The game modules' helpers that send a sound to the clients (an event, a temporary entity) or play it (the single
# player's game calls its cgame directly).
GAME_CALL = re.compile(r"(?<![\w>.])(G_Sound|G_SoundAtLoc|G_SoundAtSpot|G_SoundBroadcast|G_EntitySound|G_MuteSound|"
                       r"G_SoundOnEnt|G_SoundIndexOnEnt|G_SpeechEvent|G_SoundTempEntity)\s*\(")

DEFINITION = re.compile(r"^(?:static\s+|inline\s+)*[A-Za-z_][\w\s\*&:<>,]*?\b([A-Za-z_]\w*)\s*\(([^;]*)$")


def blank_comments(text):
    """Returns the text with its comments and string literals' contents replaced by spaces (newlines kept), and a
    mask that is True for every character inside a comment."""

    out = []
    mask = []
    i = 0
    n = len(text)

    while i < n:
        c = text[i]

        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(text[i:j])
            mask.extend([True] * (j - i))
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(text[i:j])
            mask.extend([True] * (j - i))
            i = j
        elif c == '"' or c == "'":
            j = i + 1

            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1

            j = min(j + 1, n)
            out.append(text[i:j])
            mask.extend([False] * (j - i))
            i = j
        else:
            out.append(c)
            mask.append(False)
            i += 1

    return "".join(out), mask


def inactive_lines(lines):
    """Returns the set of line numbers (1-based) inside #if 0 blocks."""

    inactive = set()
    # each open #if: [whether its parent is inactive, whether it is an #if 0, whether the current branch is inactive]
    stack = []

    for number, line in enumerate(lines, 1):
        s = line.strip()
        parent = bool(stack) and stack[-1][2]

        if re.match(r"#\s*if", s):
            zero = re.match(r"#\s*if\s+0\b", s) is not None
            stack.append([parent, zero, parent or zero])
        elif re.match(r"#\s*(else|elif)", s) and stack:
            outer, zero, _ = stack[-1]
            # the #else of an #if 0 is live; any other branch keeps its parent's state
            stack[-1][2] = outer
        elif re.match(r"#\s*endif", s) and stack:
            stack.pop()

        if stack and stack[-1][2]:
            inactive.add(number)

    return inactive


def split_arguments(text, start):
    """Returns the arguments of the call whose '(' is at start, split at the top level, and the index after ')'."""

    depth = 0
    args = []
    current = []
    i = start

    while i < len(text):
        c = text[i]

        if c in "([{":
            depth += 1

            if depth > 1:
                current.append(c)
        elif c in ")]}":
            depth -= 1

            if depth == 0:
                args.append("".join(current).strip())
                return args, i + 1

            current.append(c)
        elif c == "," and depth == 1:
            args.append("".join(current).strip())
            current = []
        else:
            current.append(c)

        i += 1

    return args, i


def enclosing_functions(lines):
    """Returns, for every line (1-based), the name of the function defined last at the start of a line before it."""

    names = [""] * (len(lines) + 1)
    current = ""

    for number, line in enumerate(lines, 1):
        m = DEFINITION.match(line)

        if m and m.group(1) not in ("if", "while", "for", "switch", "return", "sizeof", "defined"):
            current = m.group(1)

        names[number] = current

    return names


def sound_symbols(root):
    """Returns {name: group} for every function the sound files define at the start of a line."""

    symbols = {}

    for group, files in SOUND_FILES.items():
        for name in files:
            lines = (root / name).read_bytes().decode("utf-8", "replace").splitlines()

            for line in lines:
                m = DEFINITION.match(line)

                if m and not line.startswith(("static", " ", "\t")) and m.group(1) not in ("if", "while", "for"):
                    symbols.setdefault(m.group(1), set()).add(group)

    return symbols


def main():
    root = Path(sys.argv[1])
    output = Path(sys.argv[2])
    symbols_out = Path(sys.argv[3]) if len(sys.argv) > 3 else None
    own = {str(root / f) for files in SOUND_FILES.values() for f in files}
    symbols = sound_symbols(root)
    engine_call = re.compile(r"(?<![\w>.])(" + "|".join(sorted(symbols, key=len, reverse=True)) + r")\s*\(")
    rows = []
    counts = {name: 0 for name in symbols}

    for tree, folder in TREES:
        for path in sorted((root / folder).rglob("*")):
            if path.suffix not in (".cpp", ".h", ".c") or str(path) in own:
                continue

            raw = path.read_bytes().decode("utf-8", "replace")
            _, mask = blank_comments(raw)
            lines = raw.splitlines()
            functions = enclosing_functions(lines)
            inactive = inactive_lines(lines)
            relative = path.relative_to(root).as_posix()

            for pattern, kind in ((MODULE_CALL, "module"), (engine_call, "engine"), (GAME_CALL, "game")):
                for m in pattern.finditer(raw):
                    callee = m.group(1)
                    line = raw.count("\n", 0, m.start()) + 1
                    prefix = raw[raw.rfind("\n", 0, m.start()) + 1:m.start()]

                    # declarations and definitions of the entries are not calls
                    if kind == "engine" and re.search(r"\b(void|int|float|bool|sfxHandle_t|qhandle_t|char)\s*\**\s*$", prefix):
                        continue

                    if kind in ("module", "game") and re.search(r"\b(void|int|float|bool|sfxHandle_t)\s*\**\s*$", prefix):
                        continue

                    args, _ = split_arguments(raw, m.end() - 1)
                    where = "comment" if mask[m.start()] else ("inactive" if line in inactive else "live")
                    rows.append((tree, kind, relative, line, functions[line], callee, where,
                                 " | ".join(" ".join(a.split()) for a in args)))

                    if kind == "engine" and where == "live":
                        counts[callee] += 1

    with output.open("w", encoding="utf-8", newline="\n") as f:
        f.write("tree\tkind\tfile\tline\tfunction\tcallee\twhere\targuments\n")

        for row in rows:
            f.write("\t".join(str(v) for v in row) + "\n")

    if symbols_out:
        with symbols_out.open("w", encoding="utf-8", newline="\n") as f:
            f.write("symbol\tgroups\tcalls\n")

            for name in sorted(symbols):
                f.write(f"{name}\t{','.join(sorted(symbols[name]))}\t{counts[name]}\n")

    print(f"{len(rows)} calls written to {output}")


if __name__ == "__main__":
    main()

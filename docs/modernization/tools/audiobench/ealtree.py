# Prints the RIFF chunk tree of .eal files: fourcc, size, and children where a chunk's data parses as chunks.
# usage: python -I ealtree.py <file.eal>...
import struct
import sys


def is_tag(b):
    return len(b) == 4 and all(32 <= c < 127 for c in b)


def parse(data, off, end):
    """Returns a list of (tag, size, start) if [off, end) is exactly a sequence of chunks, else None."""
    out = []
    while off < end:
        if end - off < 8 or not is_tag(data[off:off + 4]):
            return None
        size = struct.unpack_from("<I", data, off + 4)[0]
        nxt = off + 8 + size + (size & 1)
        if off + 8 + size > end:
            return None
        out.append((data[off:off + 4].decode(), size, off + 8))
        off = min(nxt, end)
    return out


def walk(data, chunks, depth, lines):
    for tag, size, start in chunks:
        extra = ""
        if tag == "LIST" and size >= 4 and is_tag(data[start:start + 4]):
            lines.append(f"{'  ' * depth}LIST {size} '{data[start:start + 4].decode()}'")
            kids = parse(data, start + 4, start + size)
            if kids:
                walk(data, kids, depth + 1, lines)
            continue
        kids = parse(data, start, start + size) if size >= 8 else None
        if size == 4 and not kids:
            extra = f" = {struct.unpack_from('<I', data, start)[0]}"
        lines.append(f"{'  ' * depth}{tag} {size}{extra}")
        if kids:
            walk(data, kids, depth + 1, lines)


for path in sys.argv[1:]:
    data = open(path, "rb").read()
    riff_size = struct.unpack_from("<I", data, 4)[0]
    print(f"== {path.replace(chr(92), '/').rsplit('/', 1)[-1]}: {len(data)} bytes, RIFF size {riff_size} (+8 = {riff_size + 8})")
    top = parse(data, 12, min(len(data), riff_size + 8))
    if top is None:
        # walk as far as the chunks go, to show where it breaks
        off, top = 12, []
        while off + 8 <= len(data) and is_tag(data[off:off + 4]):
            size = struct.unpack_from("<I", data, off + 4)[0]
            top.append((data[off:off + 4].decode(), size, off + 8))
            if off + 8 + size > len(data):
                print(f"  chunk {data[off:off + 4].decode()} at {off} claims {size} bytes, past the end ({len(data)})")
                top.pop()
                break
            off += 8 + size + (size & 1)
        print(f"  stops at offset {off} of {len(data)}")
    lines = []
    walk(data, top, 1, lines)
    print("\n".join(lines))

"""Write a smaller corpus for the robustness runs: every n-th file of a corpus, and only files below a size.

usage: python -I subset.py <corpus.bin> <out.bin> <every n> <max bytes>
"""
import struct
import sys

src, dst, every, limit = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
entries = []
with open(src, "rb") as f:
    assert f.read(4) == b"ADC1"
    (n,) = struct.unpack("<I", f.read(4))
    for i in range(n):
        (nl,) = struct.unpack("<H", f.read(2))
        name = f.read(nl)
        (dl,) = struct.unpack("<I", f.read(4))
        data = f.read(dl)
        if i % every == 0 and dl <= limit:
            entries.append((name, data))
with open(dst, "wb") as f:
    f.write(b"ADC1" + struct.pack("<I", len(entries)))
    for name, data in entries:
        f.write(struct.pack("<H", len(name)) + name + struct.pack("<I", len(data)) + data)
print(dst, len(entries), "files")

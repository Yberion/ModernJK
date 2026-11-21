"""Decode the MPEG audio conformance vectors (minimp3's `vectors/`: the ISO bitstreams with their reference
decodings, 16-bit) with each benchmark executable and compare with the reference.

usage: python -I vectors.py <vectors folder> <bench folder> <work folder> <decoder>...   (decoder: mp3code, minimp3, ...)

For each vector: the offset (0, 529, 576, 1,105, 1,152, 1,681, 2,304 or 2,833 frames either way: one or two frames
of 1,152 samples, with or without the encoder's delay of 529) and polarity that match best, the
largest difference, and the PSNR against full scale over the overlap, as minimp3's test counts it (it passes a
vector above 96 dB).
"""
import math
import pathlib
import struct
import subprocess
import sys

vec, bench, work = (pathlib.Path(a) for a in sys.argv[1:4])
decoders = sys.argv[4:]
work.mkdir(parents=True, exist_ok=True)
bits = sorted(b for b in vec.glob("*.bit") if b.with_suffix(".pcm").exists())  # the ones with a reference

corpus = work / "vectors.bin"
with open(corpus, "wb") as f:
    f.write(b"ADC1" + struct.pack("<I", len(bits)))
    for b in bits:
        n = b.name.encode()
        d = b.read_bytes()
        f.write(struct.pack("<H", len(n)) + n + struct.pack("<I", len(d)) + d)


def s16(path):
    d = path.read_bytes()
    return struct.unpack(f"<{len(d) // 2}h", d[: len(d) // 2 * 2])


def compare(ref, got, ch):
    best = None
    for k in (0, 529, 576, 1105, 1152, 1681, 2304, 2833):
        for off in {k, -k}:
            for sign in (1, -1):
                lo = max(0, -off)
                hi = min(len(ref) // ch, len(got) // ch - off)
                if hi <= lo:
                    continue
                a = ref[lo * ch : hi * ch]
                b = got[(lo + off) * ch : (hi + off) * ch]
                step = max(1, len(a) // 20000)
                sad = sum(abs(x - sign * y) for x, y in zip(a[::step], b[::step]))
                if best is None or sad < best[0]:
                    best = (sad, off, sign)
    if best is None:
        return None
    _, off, sign = best
    lo = max(0, -off)
    hi = min(len(ref) // ch, len(got) // ch - off)
    a = ref[lo * ch : hi * ch]
    b = got[(lo + off) * ch : (hi + off) * ch]
    mx, se = 0, 0
    for x, y in zip(a, b):
        e = abs(x - sign * y)
        mx = max(mx, e)
        se += e * e
    mse = se / max(1, len(a))
    psnr = 999.0 if mse == 0 else 10 * math.log10(32767.0 * 32767.0 / mse)
    return off, sign, mx, psnr, len(got) // ch - len(ref) // ch


for dec in decoders:
    exe = bench / f"bench_{dec}.exe"
    passed = failed = refused = inverted = 0
    worst = (999.0, "")
    rows = []
    for i, b in enumerate(bits):
        out = work / f"{dec}_{i}.raw"
        r = subprocess.run([str(exe), str(corpus), "--dump", str(i), str(out)], capture_output=True, text=True)
        if r.returncode != 0:
            refused += 1
            rows.append(f"  {b.name}: refused")
            continue
        ch = int(r.stdout.split(":")[1].split()[0])
        ref = s16(b.with_suffix(".pcm"))
        got = s16(out)
        out.unlink()
        c = compare(ref, got, ch)
        if c is None:
            refused += 1
            rows.append(f"  {b.name}: refused (no overlap: {len(got)} samples, {ch} channels, reference {len(ref)} samples)")
            continue
        off, sign, mx, psnr, delta = c
        inverted += sign < 0
        ok = psnr > 96.0
        passed += ok
        failed += not ok
        worst = min(worst, (psnr, b.name))
        rows.append(f"  {b.name}: offset {off}, {'inverted' if sign < 0 else 'same polarity'}, max diff {mx}, PSNR {psnr:.1f} dB, length {delta:+d} frames")
    print(f"{dec}: {len(bits)} vectors, PSNR > 96 dB {passed}, below {failed}, refused {refused}, polarity inverted {inverted}, worst {worst[0]:.1f} dB ({worst[1]})")
    for row in rows:
        if "refused" in row or "inverted" in row or float(row.split("PSNR ")[1].split()[0]) <= 96.0:
            print(row)

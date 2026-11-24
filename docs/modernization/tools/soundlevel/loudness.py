"""Integrated loudness after ITU-R BS.1770-4 (LUFS) of a game's recorded mix, and its sample peak (dBFS, not the
oversampled true peak): the K-weighting (a high shelf, then a high-pass, at the file's sample rate as libebur128 derives
them), mean squares over blocks of 400 ms every 100 ms, an absolute gate at -70 LUFS and a relative gate 10 LU under
the gated mean; stereo, both channels weighted 1; the first 2 s left out (the session's start).
Reads a WAV written by OpenAL Soft's wave writer (16-bit or 32-bit float) or the raw output of SDL's disk driver (.raw:
16-bit stereo at 44.1 kHz). Python only, no numpy (about 20 s for a minute of stereo at 48 kHz).
usage: python -I loudness.py --selftest | <file> ...
The self-test checks the coefficients at 48 kHz against BS.1770's table and EBU Tech 3341's first case (a 1 kHz sine at
-23 dBFS on both channels reads -23.0 LUFS)."""
import math
import struct
import sys
from array import array
from pathlib import Path


def load(path):
    """The left and right samples (floats, full scale 1.0) and the sample rate."""
    path = Path(path)
    raw = path.read_bytes()
    if path.suffix == ".raw":
        a = array("h")
        a.frombytes(raw[: len(raw) // 4 * 4])
        return [x / 32768.0 for x in a[0::2]], [x / 32768.0 for x in a[1::2]], 44100
    pos = 12
    fmt = None
    data = b""
    while pos + 8 <= len(raw):
        cid, size = struct.unpack_from("<4sI", raw, pos)
        body = raw[pos + 8 : pos + 8 + size]
        if cid == b"fmt ":
            tag, channels, rate, _, _, bits = struct.unpack_from("<HHIIHH", body)
            if tag == 0xFFFE:
                tag = struct.unpack_from("<H", body, 24)[0]
            fmt = (tag, channels, rate, bits)
        elif cid == b"data":
            data = raw[pos + 8 :] if size in (0, 0xFFFFFFFF) else body
            break
        pos += 8 + size + (size & 1)
    tag, channels, rate, bits = fmt
    if tag == 3:
        a = array("f")
        a.frombytes(data[: len(data) // 4 * 4])
        scale = 1.0
    elif bits == 16:
        a = array("h")
        a.frombytes(data[: len(data) // 2 * 2])
        scale = 1.0 / 32768.0
    else:
        raise SystemExit(f"format {fmt}")
    n = len(a) // channels * channels
    return [x * scale for x in a[0:n:channels]], [x * scale for x in a[1:n:channels]], rate


def k_filters(rate):
    """The two biquads of the K-weighting at a sample rate: (b0, b1, b2, a1, a2) each."""
    f0, gain, q = 1681.974450955533, 3.999843853973347, 0.7071752369554196
    k = math.tan(math.pi * f0 / rate)
    vh = 10.0 ** (gain / 20.0)
    vb = vh ** 0.4996667741545416
    a0 = 1.0 + k / q + k * k
    shelf = ((vh + vb * k / q + k * k) / a0, 2.0 * (k * k - vh) / a0, (vh - vb * k / q + k * k) / a0,
             2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0)
    f0, q = 38.13547087602444, 0.5003270373238773
    k = math.tan(math.pi * f0 / rate)
    a0 = 1.0 + k / q + k * k
    highpass = (1.0, -2.0, 1.0, 2.0 * (k * k - 1.0) / a0, (1.0 - k / q + k * k) / a0)
    return shelf, highpass


def biquad(x, coefficients):
    b0, b1, b2, a1, a2 = coefficients
    y = [0.0] * len(x)
    x1 = x2 = y1 = y2 = 0.0
    for i, v in enumerate(x):
        out = b0 * v + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2
        x2, x1, y2, y1 = x1, v, y1, out
        y[i] = out
    return y


def integrated(left, right, rate):
    """The gated loudness of a stereo signal, in LUFS."""
    shelf, highpass = k_filters(rate)
    block, step = int(0.4 * rate), int(0.1 * rate)
    prefixes = []
    for channel in (left, right):
        weighted = biquad(biquad(channel, shelf), highpass)
        prefix = [0.0]
        for v in weighted:
            prefix.append(prefix[-1] + v * v)
        prefixes.append(prefix)
    powers = [sum((p[start + block] - p[start]) / block for p in prefixes) for start in range(0, len(left) - block + 1, step)]

    def lufs(power):
        return -0.691 + 10.0 * math.log10(max(power, 1e-20))

    gated = [p for p in powers if lufs(p) > -70.0]
    if not gated:
        return float("-inf")
    relative = lufs(sum(gated) / len(gated)) - 10.0
    gated = [p for p in gated if lufs(p) > relative]
    return lufs(sum(gated) / len(gated))


def selftest():
    shelf, highpass = k_filters(48000)
    expected = (1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585,
                1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621)
    worst = max(abs(a - b) for a, b in zip(shelf + highpass, expected))
    print(f"coefficients at 48 kHz against BS.1770's table: largest difference {worst:.2e}")
    rate, amplitude = 48000, 10.0 ** (-23.0 / 20.0)
    sine = [amplitude * math.sin(2.0 * math.pi * 1000.0 * i / rate) for i in range(rate * 10)]
    print(f"1 kHz sine at -23 dBFS on both channels (EBU Tech 3341's first case, -23.0 expected): {integrated(sine, sine, rate):.2f} LUFS")


if __name__ == "__main__":
    if sys.argv[1:] == ["--selftest"]:
        selftest()
    else:
        for name in sys.argv[1:]:
            left, right, rate = load(name)
            start = 2 * rate
            peak = max(max(abs(v) for v in left[start:]), max(abs(v) for v in right[start:]))
            print(f"{name}: {integrated(left[start:], right[start:], rate):.1f} LUFS, sample peak {20.0 * math.log10(max(peak, 1e-9)):.1f} dBFS")

"""Integrated loudness after ITU-R BS.1770-4 (LUFS) of a game's recorded mix, its sample peak (dBFS) and its true peak
(dBTP, BS.1770-4 Annex 2, the measure ASWG-R001 caps at -1 dBTP): the K-weighting (a high shelf, then a high-pass, at
the file's sample rate as libebur128 derives them), mean squares over blocks of 400 ms every 100 ms, an absolute gate at
-70 LUFS and a relative gate 10 LU under the gated mean; stereo, both channels weighted 1; the true peak the largest of
the signal over-sampled four times by the recommendation's filter, on either channel; the first 2 s left out (the
session's start).
Reads a WAV written by OpenAL Soft's wave writer (16-bit or 32-bit float) or the raw output of SDL's disk driver (.raw:
16-bit stereo at 44.1 kHz). Python only, no numpy (about 20 s for a minute of stereo at 48 kHz, more for the true peak
of a mix that stays near its peak).
usage: python -I loudness.py --selftest | <file> ...
The self-test checks the coefficients at 48 kHz against BS.1770's table, EBU Tech 3341's first case (a 1 kHz sine at
-23 dBFS on both channels reads -23.0 LUFS) and its true-peak cases 15 to 19 (sines at fs/4, fs/6 and fs/8 in phases
that put the peak between samples, within +0.2/-0.4 dB); cases 20 to 23 need the EBU's files."""
import math
import operator
import struct
import sys
from array import array
from pathlib import Path

# ITU-R BS.1770-4 Annex 2's interpolating filter for the true peak (four times over-sampling, 48 taps in 4 phases of 12),
# as the recommendation prints it; phase 2 is phase 1 reversed and phase 3 phase 0 reversed
TRUE_PEAK_PHASES = (
    (0.0017089843750, 0.0109863281250, -0.0196533203125, 0.0332031250000, -0.0594482421875, 0.1373291015625,
     0.9721679687500, -0.1022949218750, 0.0476074218750, -0.0266113281250, 0.0148925781250, -0.0083007812500),
    (-0.0291748046875, 0.0292968750000, -0.0517578125000, 0.0891113281250, -0.1665039062500, 0.4650878906250,
     0.7797851562500, -0.2003173828125, 0.1015625000000, -0.0582275390625, 0.0330810546875, -0.0189208984375),
)
TRUE_PEAK_PHASES += tuple(phase[::-1] for phase in reversed(TRUE_PEAK_PHASES))


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


def true_peak(channel):
    """The largest magnitude of a channel over-sampled four times by BS.1770-4's filter (linear, full scale 1.0). The
    blocks are filtered loudest first, and the search stops at the first block whose largest sample, times the largest
    sum of magnitudes of a phase, cannot exceed the peak found: the same result as filtering every sample."""
    taps = [phase[::-1] for phase in TRUE_PEAK_PHASES]  # against a window of 12 samples, the oldest first
    reach = max(sum(abs(t) for t in phase) for phase in TRUE_PEAK_PHASES)
    length, size = len(TRUE_PEAK_PHASES[0]), 256
    padded = [0.0] * (length - 1) + list(channel)  # padded[n:n + length] ends at channel[n]
    blocks = sorted(((max(map(abs, padded[start : start + size + length - 1])), start) for start in range(0, len(channel), size)), reverse=True)
    best = 0.0
    for largest, start in blocks:
        if largest * reach <= best:
            break
        for n in range(start, min(start + size, len(channel))):
            window = padded[n : n + length]
            for phase in taps:
                best = max(best, abs(sum(map(operator.mul, phase, window))))
    return best


def selftest():
    shelf, highpass = k_filters(48000)
    expected = (1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585,
                1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621)
    worst = max(abs(a - b) for a, b in zip(shelf + highpass, expected))
    print(f"coefficients at 48 kHz against BS.1770's table: largest difference {worst:.2e}")
    rate, amplitude = 48000, 10.0 ** (-23.0 / 20.0)
    sine = [amplitude * math.sin(2.0 * math.pi * 1000.0 * i / rate) for i in range(rate * 10)]
    print(f"1 kHz sine at -23 dBFS on both channels (EBU Tech 3341's first case, -23.0 expected): {integrated(sine, sine, rate):.2f} LUFS")
    # EBU Tech 3341's true-peak cases 15 to 19: (case, divisor of the sample rate, amplitude, phase in degrees, dBTP)
    fade = rate // 100  # the 10 ms fade-in and fade-out the cases ask for
    for case, divisor, amplitude, phase, expected in ((15, 4, 0.50, 0.0, -6.0), (16, 4, 0.50, 45.0, -6.0), (17, 6, 0.50, 60.0, -6.0),
                                                       (18, 8, 0.50, 67.5, -6.0), (19, 4, 1.41, 45.0, 3.0)):
        count = rate
        tone = [amplitude * min(1.0, i / fade, (count - 1 - i) / fade) * math.sin(2.0 * math.pi * i / divisor + math.radians(phase)) for i in range(count)]
        level = 20.0 * math.log10(true_peak(tone))
        verdict = "ok" if expected - 0.4 <= level <= expected + 0.2 else "OUT OF TOLERANCE"
        print(f"true-peak case {case} (fs/{divisor}, {amplitude:.2f} FFS, {phase} degrees, {expected:+.1f} dBTP expected): {level:+.2f} dBTP {verdict}")


if __name__ == "__main__":
    if sys.argv[1:] == ["--selftest"]:
        selftest()
    else:
        for name in sys.argv[1:]:
            left, right, rate = load(name)
            start = 2 * rate
            peak = max(max(abs(v) for v in left[start:]), max(abs(v) for v in right[start:]))
            peak_true = max(true_peak(left[start:]), true_peak(right[start:]))
            print(f"{name}: {integrated(left[start:], right[start:], rate):.1f} LUFS, sample peak {20.0 * math.log10(max(peak, 1e-9)):.1f} dBFS, "
                  f"true peak {20.0 * math.log10(max(peak_true, 1e-9)):.1f} dBTP")

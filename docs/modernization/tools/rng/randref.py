"""The first values of the streams Rand_InitFixed seeds (src/shared/qcommon/q_rand.h), computed from the published
algorithms rather than from the C++: SplitMix64 seeding (Steele, Lea, Flood) of the 64-bit value stream << 32 | seed
as a 32-bit unsigned value, then xoshiro128++ (Blackman, Vigna, reference code at https://prng.di.unimi.it/). The
values test/qcommon/q_rand.cpp checks. Usage: python -I randref.py"""
M64 = (1 << 64) - 1
M32 = (1 << 32) - 1
STREAMS = ("RAND_GAME", "RAND_ENGINE", "RAND_CGAME", "RAND_UI", "RAND_RENDERER")


def seed_state(seed64):
    s = []
    for _ in range(2):
        seed64 = (seed64 + 0x9E3779B97F4A7C15) & M64
        z = seed64
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M64
        z ^= z >> 31
        s += [z & M32, z >> 32]
    return s


def rotl(x, k):
    return ((x << k) | (x >> (32 - k))) & M32


def next_u32(s):
    result = (rotl((s[0] + s[3]) & M32, 7) + s[0]) & M32
    t = (s[1] << 9) & M32
    s[2] ^= s[0]
    s[3] ^= s[1]
    s[1] ^= s[2]
    s[0] ^= s[3]
    s[2] ^= t
    s[3] = rotl(s[3], 11)
    return result


for stream, seed in [(0, 7), (1, 7), (2, 7), (3, 7), (4, 7), (0, -1), (4, -1)]:
    s = seed_state(((stream << 32) | (seed & M32)) & M64)
    print(f"{STREAMS[stream]:13} seed {seed:2}:", ", ".join(f"0x{next_u32(s):08x}u" for _ in range(4)))

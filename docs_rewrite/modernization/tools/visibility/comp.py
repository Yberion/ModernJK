import sys, zlib, lzma
d = open(sys.argv[1], "rb").read()
n = int(sys.argv[2]); rb = (n + 7) // 8
# rows XORed with the previous row, a common trick for spatially coherent rows
x = bytearray(d)
for r in range(n - 1, 0, -1):
    a = r * rb; b = (r - 1) * rb
    x[a:a + rb] = bytes(p ^ q for p, q in zip(d[a:a + rb], d[b:b + rb]))
print("raw %.2f MB, zlib9 %.2f MB, lzma %.2f MB, xor+zlib9 %.2f MB" % (len(d) / 1048576, len(zlib.compress(d, 9)) / 1048576,
      len(lzma.compress(d, preset=6)) / 1048576, len(zlib.compress(bytes(x), 9)) / 1048576))

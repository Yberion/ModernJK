# For each .eal file: the environments' and sources' names, and the gema chunk's header (sources, environments,
# BSP start, planes, leaves). usage: python -I ealgema.py <file.eal>...
import struct
import sys


def chunks(data, off, end):
    while off + 8 <= end:
        tag = data[off:off + 4].decode("latin-1")
        size = struct.unpack_from("<I", data, off + 4)[0]
        yield tag, off + 8, size
        off += 8 + size + (size & 1)


def names(data, start, size):
    return [data[i:i + 32].split(b"\0")[0].decode("latin-1") for i in range(start, start + size, 32)]


for path in sys.argv[1:]:
    data = open(path, "rb").read()
    info = {"envs": [], "srcs": [], "gema": None}
    for tag, start, size in chunks(data, 12, len(data)):
        if tag != "LIST":
            continue
        kind = data[start:start + 4].decode("latin-1")
        for sub, s2, z2 in chunks(data, start + 4, start + size):
            if sub == "nams" and kind == "envp":
                info["envs"] = names(data, s2, z2)
            if sub == "nams" and kind == "srcp":
                info["srcs"] = names(data, s2, z2)
            if sub == "gema":
                info["gema"] = struct.unpack_from("<5I", data, s2)
    g = info["gema"]
    name = path.replace("\\", "/").rsplit("/", 1)[-1]
    srcs = sorted(set(info["srcs"]))
    print(f"{name}: envs {len(info['envs'])}, gema srcs/envs/start/planes/leaves {g}, source names {srcs[:6]}{' ...' if len(srcs) > 6 else ''}")

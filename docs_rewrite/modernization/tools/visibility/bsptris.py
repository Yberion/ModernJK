"""TMP: the drawn opaque triangles of a map's world model (RBSP), for the visibility tracer benchmark.
usage: python -I bsptris.py <map name, e.g. mp/ffa3> <out.tris>"""
import re, struct, sys, zipfile

BASE = "E:/Jeux/Jedi Academy default - netcodev_openjk/base/"
PAKS = ["assets0.pk3", "assets1.pk3", "assets2.pk3", "assets3.pk3"]  # later ones override earlier ones
SURF_NODRAW, SURF_SKY = 0x00200000, 0x00002000
CONTENTS_WATER, CONTENTS_FOG, CONTENTS_TRANSLUCENT = 0x4, 0x8, 0x80000000

def read_map(name):
    data = None
    for p in PAKS:
        z = zipfile.ZipFile(BASE + p)
        for n in z.namelist():
            if n.lower() == "maps/%s.bsp" % name.lower():
                data = z.read(n)
    return data

def shader_scripts():
    """shader name -> (surfaceparms, first stage tokens, any autosprite)"""
    out = {}
    for p in PAKS:
        z = zipfile.ZipFile(BASE + p)
        for n in z.namelist():
            if not n.lower().endswith(".shader"):
                continue
            t = re.sub(r"//[^\n]*", "", z.read(n).decode("latin-1"))
            toks = re.findall(r"\{|\}|[^\s{}]+", t)
            depth, name, body, stages, cur = 0, None, [], [], None
            for tok in toks:
                if tok == "{":
                    depth += 1
                    if depth == 2:
                        cur = []
                elif tok == "}":
                    if depth == 2 and cur is not None:
                        stages.append(cur); cur = None
                    depth -= 1
                    if depth == 0 and name:
                        out[name.lower()] = (body, stages)
                        name, body, stages = None, [], []
                elif depth == 0:
                    name = tok
                elif depth == 1:
                    body.append(tok.lower())
                elif depth == 2 and cur is not None:
                    cur.append(tok.lower())
    return out

def opaque_script(entry):
    body, stages = entry
    if "autosprite" in body or "autosprite2" in body:
        return False, "autosprite"
    if not stages:
        return True, ""
    first = stages[0]
    for i, tk in enumerate(first):
        if tk == "alphafunc":
            return False, "alphafunc"
        if tk == "blendfunc":
            a = first[i + 1] if i + 1 < len(first) else ""
            b = first[i + 2] if i + 2 < len(first) else ""
            if a in ("add", "filter", "blend"):
                return False, "blend"
            if not (a == "gl_one" and b == "gl_zero"):
                return False, "blend"
    return True, ""

def bezier(c, u, v):
    # c: 3x3 control points, biquadratic
    def b(t):
        return ((1 - t) ** 2, 2 * t * (1 - t), t * t)
    bu, bv = b(u), b(v)
    return tuple(sum(bu[i] * bv[j] * c[j][i][k] for i in range(3) for j in range(3)) for k in range(3))

def main():
    name, outp = sys.argv[1], sys.argv[2]
    d = read_map(name)
    ident, version = struct.unpack_from("<4si", d, 0)
    lumps = [struct.unpack_from("<ii", d, 8 + 8 * i) for i in range(18)]
    def lump(i):
        o, l = lumps[i]
        return d[o:o + l]
    shaders = []
    sl = lump(1)
    for i in range(len(sl) // 72):
        nm, sf, cf = struct.unpack_from("<64sii", sl, i * 72)
        shaders.append((nm.split(b"\0")[0].decode("latin-1").lower(), sf & 0xFFFFFFFF, cf & 0xFFFFFFFF))
    models = lump(7)
    first_surf, num_surf = struct.unpack_from("<6f2i", models, 0)[6:8]
    verts = lump(10)
    idx = lump(11)
    surfs = lump(13)
    scripts = shader_scripts()
    tris = []
    meta = []
    skipped = {}
    kinds = {}
    for s in range(first_surf, first_surf + num_surf):
        f = struct.unpack_from("<7i", surfs, s * 148)
        shader_num, fog, stype, fv, nv, fi, ni = f
        pw, ph = struct.unpack_from("<2i", surfs, s * 148 + 140)
        sname, sf, cf = shaders[shader_num]
        reason = ""
        if stype == 4 or stype == 0:
            reason = "flare"
        elif sf & SURF_NODRAW:
            reason = "nodraw"
        elif cf & (CONTENTS_TRANSLUCENT | CONTENTS_FOG | CONTENTS_WATER):
            reason = "trans/fog/water"
        elif sname in scripts:
            ok, why = opaque_script(scripts[sname])
            if not ok:
                reason = why
        if reason:
            skipped[reason] = skipped.get(reason, 0) + 1
            continue
        P = [struct.unpack_from("<3f", verts, (fv + k) * 80) for k in range(nv)]
        before = len(tris)
        if stype in (1, 3):
            for k in range(0, ni, 3):
                a, b, c = (struct.unpack_from("<i", idx, (fi + k + m) * 4)[0] for m in range(3))
                tris.append(P[a] + P[b] + P[c])
        elif stype == 2:
            n = 8
            for py in range(0, ph - 1, 2):
                for px in range(0, pw - 1, 2):
                    c = [[P[(py + j) * pw + px + i] for i in range(3)] for j in range(3)]
                    g = [[bezier(c, u / n, v / n) for u in range(n + 1)] for v in range(n + 1)]
                    for v in range(n):
                        for u in range(n):
                            tris.append(g[v][u] + g[v][u + 1] + g[v + 1][u + 1])
                            tris.append(g[v][u] + g[v + 1][u + 1] + g[v + 1][u])
        kinds[stype] = kinds.get(stype, 0) + len(tris) - before
        meta.extend([(shader_num, stype, s)] * (len(tris) - before))
    with open(outp + ".meta", "w", encoding="utf-8") as m:
        for (sn, st, si) in meta:
            m.write("%s %d %d\n" % (shaders[sn][0], st, si))
    with open(outp, "wb") as o:
        o.write(struct.pack("<i", len(tris)))
        for t in tris:
            o.write(struct.pack("<9f", *t))
    print("%s: %d triangles (planar %d, patch %d, soup %d); surfaces skipped %s" % (
        name, len(tris), kinds.get(1, 0), kinds.get(2, 0), kinds.get(3, 0), skipped))

main()

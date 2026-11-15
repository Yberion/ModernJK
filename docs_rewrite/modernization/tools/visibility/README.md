# The visibility culling prototype (step 14 part 11)

The tools that produced the numbers of [../../surveys/visibility-culling.md](../../surveys/visibility-culling.md) section 8, kept as they were run on 15 November 2026. Measurement only: nothing here is part of the engine.

## How they were run

The server side is a temporary patch: one of the `.inc` files was included at the top of `codemp/server/sv_ccmds.cpp` (an `#include` of its absolute path after `server/sv_gameapi.h`) and its commands were added at the start of `SV_AddOperatorCommands`; both lines were removed after each measure. The dedicated server was the clang build of `build-netcodec/openjk-clang`, started under lldb with a home folder per run. The scripts expect `SCR` to name a work folder holding the built tools; they write their runs there.

The offline tools are C++20, built with llvm-mingw clang (`-O2 -msse4.2 -static`); those using tinybvh need its header folder on the include path: tinybvh (MIT) at commit `f1c18c3f22e878ebb10d9ff088b47b68004d5daa` (31 October 2026; it publishes no releases), from https://github.com/jbikker/tinybvh. `libbench.cpp` also needs madmann91/bvh (MIT) at commit `ad9f7ee3fa` (16 October 2026), from https://github.com/madmann91/bvh, its `src` folder on the include path. Neither library is in this repository.

## The files

| file | what it does |
|---|---|
| `tmpvis.inc` | first attempt: a table of every open cube (`TMPvisbuild`), its check on random point pairs (`TMPvischeck`), the measure with bots against the truth (`TMPvisgame`); the engine's tracer |
| `tmplayer.inc` | the walkable layer counted (`TMPlayer`) and checked against the bots' positions (`TMPlayercheck`) |
| `tmpvis2.inc` | the layer's cells (floor cells by footprint and level, air cells), the symmetric table and the camera rows on the engine's tracer (`TMPvis2build`, `TMPvis3slice`, `TMPvis3load`), the exports for the offline tools (`TMPvisexport`, `TMPvisexport2` with the shared lattice), the measure with bots and its dump (`TMPvis2game`, `TMPvisgamedump`), the rays recorded and replayed (`TMPvisdump`, `TMPvisreplay`), the layer's coverage (`TMPvisprep`, `TMPviscover`) |
| `bsptris.py` | the drawn opaque triangles of a map's world model, from the BSP and the shader scripts (patches tessellated); `<out>.meta` gives each triangle's shader |
| `bvhbench.cpp` | a scalar BVH written for the comparison, timed on recorded rays, verdicts against the engine's |
| `libbench.cpp` | tinybvh (binary, 4- and 8-wide layouts) and madmann91/bvh v2 on the same rays |
| `why.cpp`, `diag.cpp` | which surfaces block the rays the engine lets through; one missed pair looked at closely |
| `vistool.cpp` | the table on tinybvh from `TMPvisexport`: every pair, or near pairs and coarse far blocks |
| `vistool2.cpp` | the table on tinybvh from `TMPvisexport2`: the shared lattice for near pairs, a fan of rays marking far blocks |
| `truth.cpp`, `truth2.cpp` | the measured pairs judged again on the drawn triangles, by distance; `truth2` reads the extended dump and sorts the misses by what each player stands on |
| `comp.py` | the compressed sizes of a table file |
| `visrun.sh` to `vis7run.sh` | the successive runs: build or export, compute, measure with 31 bots, judge |
| `layerrun.sh`, `coverrun.sh`, `dumprun.sh` | the layer's counts, its coverage of the bots' positions, the rays recorded |

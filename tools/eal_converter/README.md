# eal_converter

Converts the EAX zone files of Jedi Academy and Jedi Outcast (`eagle/<map>.eal`, made with Creative's editor and read
by its EAX Manager in the original game) into the files the sound reads (`eagle/<map>.efxzones`, in each game's
`z_ModernJK.pk3`): a map's reverb by zones, played through OpenAL Soft's EFX in place of Creative's EAX. The converted
files are in the repository (`assets/jka/eagle`, `assets/jk2/eagle`); the tool is run again only when the conversion
changes.

## What it writes

The format is read and written by `src/shared/sound/zones/zones.cpp`, which says it line by line. From each file:

- the environments ("Tie Fighter Hall", "Lunch Room"), their EAX values turned into those of EFX's "EAX reverb" effect
  as OpenAL Soft's own EAX layer turns them (`EaxReverbCommitter::commit` in `libs/openal-soft/al/effects/reverb.cpp`);
  the file's default environment added when zones play it (EAX Manager's environment -1, which the original engine
  played there);
- the zones' BSP (its planes, each leaf a zone or none), the obstacles and the matrix of the obstacle between two zones
  (left out when none of its obstacles acts: the default obstacle of every game file is at 0 mB, so that 65 of the 83
  files have no matrix), the diffraction boxes and their model, as EalMan read them, in the axes of Creative's editor; one repair: a box whose
  zone is not the one that holds its centre (the zones renumbered in the editor after the boxes were placed, which EAX
  Manager never applies) is given that zone, both in the file and in EalMan's data for the check, and the boxes are
  sorted again by zone (69 boxes of the game's files: `t2_rancor`'s 12, `yavin_temple`'s 31...);
- the apertures, as the original engine took them (Jedi Academy's `code/client/snd_dma.cpp`, `LoadEALFile`): the
  environments numbered by the `Center` sources, an aperture a pair `Aperture<n>a` and `Aperture<n>b` inside one
  environment. One repair: an environment may lack an aperture number the others have (the engine refused the whole file
  then, and played one reverb). A file without them is played with one reverb, the listener's environment.

A file EalMan refuses but whose one environment it read gets that environment over the whole map (Jedi Outcast's
`valley.eal`, whose one box points to no zone). A file whose map name would end with a dot is skipped (Jedi Academy's
`t1_sour..eal`, an empty draft beside `t1_sour.eal`, which the engine never loads).

## Its check

Every file written is read back by the sound's reader, then compared with EalMan, which was checked against Creative's
`EaxMan.dll`: the environment of every point of a grid over the BSP and of the planes' base points, and the obstruction
and occlusion of 200,000 random pairs of a listener and a source and of 4,000 more around each diffraction box, to the
bit (an obstacle's ratios compared only where its level is not 0, a level of 0 making the same filters whatever its
ratios: a file without a matrix gives no obstacle where EalMan gives its default one at 0 mB). The tool prints, for each file, how many pairs were obstructed and occluded, so that the check is seen to reach
them, and ends with an error when a file differs.

## Running it

From the repository's folder, the preset's binaries in `build/<preset>/bin` (llvm-mingw's `bin` folder in the `PATH`
on Windows):

```
cmake --build --preset windows-clang-release --target eal_converter
python -I tools/eal_converter/extract.py "<Jedi Academy>/GameData/base" <folder>/jka
build/windows-clang-release/bin/eal_converter <folder>/jka/eagle assets/jka/eagle
python -I tools/eal_converter/extract.py "<Jedi Outcast>/GameData/base" <folder>/jk2 ctf_ duel_ ffa_
build/windows-clang-release/bin/eal_converter <folder>/jk2/eagle assets/jk2/eagle
```

`extract.py` copies the zone files out of the game's archives in the order the engine reads them; Jedi Outcast's
multiplayer maps are left out (`ctf_`, `duel_`, `ffa_`), since the project builds only its single player. The tool is
built only on demand (the target is out of `all`); the validation scripts build it so that it keeps building with the
three compilers. It links EalMan (`libs/ealman`, LGPL 2.1), which the game never links.

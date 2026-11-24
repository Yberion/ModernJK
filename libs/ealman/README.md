# EalMan (vendored sources)

EalMan reads the EAX zone files of Jedi Academy and Jedi Outcast (`eagle/<map>.eal`): the reverb's environments, the
zones' BSP, the sources placed in the editor (the centres and apertures of the environments), the obstacles between
zones and the diffraction boxes. It is a portable rewrite of Creative's `EaxMan.dll`, built here with
`EAXMANVERSION=3`, the interface of the version 3.0 the games shipped. Only the converter of the zone files,
`tools/eal_converter`, links it; the game never does: the converter writes the files the game reads
(`eagle/<map>.efxzones`).

- Source: https://github.com/senneschall/EalMan at the commit `aa69e586b7d243d5287c2111b8228c487a4692fb` (4 November
  2026), `src/` kept unchanged but for the patch below. Left out: its tests and their data (`tests/`, the files of
  Unreal Tournament and the results of `EaxMan.dll` they are compared with), its CMake file (the target is
  `vendored_ealman` in the root's `Dependencies.cmake`) and its README.
- Patches, in `patches/`, applied from this folder with `git apply`:
  - `0001-data-accessor.patch` (24 November 2026): `src/EalMan.h` gives the data it read (`GetData`, to read it and
    to change it), which its interface keeps private: the converter writes the zones' BSP, the obstacles and the
    diffraction boxes into the game's file, and repairs the boxes whose zone is not theirs in EalMan's data too, so that
    its check compares the two repaired alike. Keep it while EalMan offers no such access.
- Checked by its author against `EaxMan.dll` 2.0.1.1, the version of Creative's EAX patches for Unreal Tournament, on
  1.5 x 10^12 positions (its README); not against the version 3.0.
- Licence: GNU Lesser General Public License 2.1 (`LICENSE`), compatible with the GPL v2.
- To update it: replace `src/` with a newer commit's, apply the patch again, build `eal_converter` and convert the zone
  files again; its check compares the module's calculations with EalMan's on every file and must find no difference.

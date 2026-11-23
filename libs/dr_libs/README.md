# dr_mp3 and dr_wav (vendored headers)

dr_mp3 decodes the sound's MP3 files (its decoder is minimp3's) and dr_wav its WAV files. Their implementations are
compiled in `src/shared/sound/mp3.cpp` and `wav.cpp` (`DR_MP3_IMPLEMENTATION`, `DR_WAV_IMPLEMENTATION`, without their
file functions); nothing else includes them (survey and measurements: `docs/modernization/surveys/audio-decoders.md`).

- Source: https://github.com/mackron/dr_libs at the commit `e381981afa52800b9a3864df548353ff67252fbc` (19 November
  2026): `dr_mp3.h` (v0.7.5, not released yet) and `dr_wav.h` (v0.14.7, not released yet), unchanged. The newest tags,
  `mp3-0.7.4` and `wav-0.14.6`, are older than the fixes the survey measured. Left out: the other headers (`dr_flac.h`:
  libFLAC decodes FLAC), the tests and the repository's README.
- Checked on 23 November 2026 against the survey's commit: the same samples on the game's 12,902 MP3 and 302 WAV
  files, no fault on 6,402 and 2,718 damaged copies under AddressSanitizer
  (`docs/modernization/progress/step-21-sound.md`).
- Licence: public domain or MIT No Attribution, at the user's choice (the end of each header).
- To update them: replace both headers with a newer commit's, run `ctest` (`modernjk.sound`) and
  `docs/modernization/tools/audiodec/check_run.sh` on the game's files.

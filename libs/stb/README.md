# stb_image_write and stb_vorbis (vendored files)

- `stb_image_write.h` writes the images the games save: JPEG and PNG screenshots, the screenshot of a saved game. It is
  compiled inside `src/shared/rd-common/tr_image_codec.cpp` and nothing else includes it (survey:
  `docs/modernization/surveys/image-libraries.md`). The images the game loads are decoded by Wuffs (`libs/wuffs`), which
  writes none.
- `stb_vorbis.c` decodes the sound's Ogg Vorbis files: compiled as a library of its own (`vendored::stb_vorbis`,
  `Dependencies.cmake`, without its file and push-data functions), its declarations included by
  `src/shared/sound/vorbis.cpp` (survey: `docs/modernization/surveys/audio-decoders.md`).

- Sources: https://github.com/nothings/stb, unchanged:
  - `stb_image_write.h`, version 1.18, at the commit `9f755b604db3492df2cbeb39bf84b81e3e3932e3` (12 November 2026);
  - `stb_vorbis.c`, version 1.25, at the commit `033efa1bcb750136ac98e997d53833b758e0089b` (19 November 2026), the
    survey's 1.24 with fixes for damaged files; checked on 23 November 2026: the same samples as 1.24 on the survey's
    453 Ogg Vorbis files, no fault on 1,221 damaged copies under AddressSanitizer
    (`docs/modernization/progress/step-21-sound.md`).
- Licence: MIT or public domain, at the user's choice (the end of each file).

# Audio decoders: the files the sound reads, in every tree

Written on 19 November 2026 for plan step 21 ([../PLAN.md](../PLAN.md)), the sound on OpenAL Soft in every tree. OpenAL Soft plays samples and decodes no file ([audio-libraries.md](audio-libraries.md) section 4), so a decoder sits beside it. This survey chooses it, in place of the MP3 decoders (`codemp/mp3code`, `code/mp3code`), their glue (`codemp/client/snd_mp3.cpp`, `code/client/cl_mp3.cpp`) and the engine's own WAV reader (`GetWavinfo`). Decided the same day: a performant and modern library that reads at least MP3, WAV, FLAC, Ogg Vorbis and Opus by default; the candidates found by a search, not from memory; the survey made before the line endings of plan step 18. Nothing is implemented.

State: ✅ decided on 19 November 2026 (section 8): dr_mp3, dr_wav, libFLAC and stb_vorbis 1.24 behind the module's own interface, no Opus for now; their build checked on Linux (GCC 15, Clang 23) and with MSVC (section 5.8).

Each source is labelled as in the other surveys: **primary** (the project's own documentation, source code or release page), **secondary** (a project or a publication that reports on another), **community** (a forum, a blog, a pull request of a third party). A number a project gives about itself is primary for what it claims, not a measurement of this survey.

## 1. Summary

Measured on 19 November 2026:

- **The engine's decoder is wrong in five ways** (section 5.2), each checked against two independent decoders (minimp3 and libmpg123, which agree with each other within 2 steps on all 12,902 files) and against the MPEG conformance vectors: every sound comes out with its polarity inverted (inaudible alone); its samples are off by one step against the reference (93 to 95 dB where the decoders above pass 96); 50 of the game's files are decoded wrongly, by up to 3,898 steps of 32,768 (37 footsteps, `sound/player/boon.mp3`, `sound/weapons/thermal/warning.mp3` ...); it stops 1 to 5 frames early (26 to 130 ms cut) on 3,054 files; and on damaged input it reads outside its buffer (a file shorter than 128 bytes, among others). It also plays the Xing header frame as 26 ms of silence before 12,531 sounds, as minimp3's frame decoder does; dr_mp3 and libmpg123 skip it.
- **MP3**: minimp3 (its frame decoder) and dr_mp3 are within 2 steps of libmpg123 on every file of the game and pass the conformance vectors; libmpg123 refuses a stream of one frame (a sound shorter than 52 ms would be silent) and reports an error at a truncated last frame (59 of the game's files); minimp3's file loader refuses 11,505 of the game's files, whose Xing header says 0 frames.
- **WAV, FLAC**: dr_wav and dr_flac give exactly the engine's samples and the source's; **Ogg Vorbis**: stb_vorbis 1.24 is within one step of libvorbis; **Opus**: miniaudio's decoder is opusfile itself.
- **Robustness** on 12,534 damaged copies of the game's files under AddressSanitizer: no fault in dr_libs, minimp3, stb_vorbis 1.24, libmpg123, libFLAC, libvorbis, opusfile; faults in the engine's decoder, in libnyquist (three) and in the older stb_vorbis that miniaudio bundles (1.22).
- **libnyquist** is out: it crashes on valid FLAC files and on damaged MP3 and WAV, it does not give back 16-bit samples exactly, and its build needs a fix with clang.
- **Every compiler** (section 5.8): dr_mp3, dr_wav and libFLAC give the same samples with clang on Windows, GCC 15 and Clang 23 on Linux and MSVC; stb_vorbis too but with MSVC, one step apart on 0.07 % of the samples (its math functions); no fault under AddressSanitizer, no compiler warning.
- **Speed** (section 5.6): every MP3 decoder is faster than the engine's, by 28 % (dr_mp3) to 34 % (libmpg123); libFLAC 13 % faster than dr_flac; stb_vorbis 29 % faster than libvorbis; on the same sample, Opus takes 2.9 times stb_vorbis's time and 4 to 4.6 times FLAC's.
- **Decided** (19 November 2026, sections 7 and 8), format by format: dr_mp3, dr_wav, libFLAC and stb_vorbis 1.24 behind the plan step 21 module's own interface, about 330 KB; Opus not read for now (no file uses it; added when a use comes).

## 2. What exists

Read in this repository and counted in the game's `assets0.pk3` to `assets3.pk3` (`E:\Jeux\Jedi Academy default - ModernJK\base`) on 19 November 2026:

| what | where | facts |
|---|---|---|
| the MP3 files | the game's archives | 12,902 files, 316 MB; all MPEG-1 Layer III at 44,100 Hz; 12,441 mono with a Xing or Info header (variable bit rate, first frame at 48 kbit/s), 328 mono at constant 64 kbit/s, 132 stereo at 160 kbit/s (the music), one at 112; 12,563 end with an ID3v1 tag, 336 begin with an ID3v2 tag |
| the engine's tag in the MP3s | `MP3_ReadSpecialTagInfo`, `codemp/client/snd_mp3.cpp:182` | the ID3v1 tag's comment holds the sound's maximum volume (`sKEY_MAXVOL`, for the lip sync) and its album field the unpacked size (`sKEY_UNCOMP`); written by the `mp3_calcvols` command (`snd_mem.cpp:519`). A new decoder must skip the tag; the engine keeps reading it |
| the WAV files | the game's archives | 302 files, all PCM (format tag 1), mono: 265 at 22,050 Hz 16-bit, 15 at 11,025 Hz 8-bit, 11 at 44,100 Hz 16-bit, the rest 8,000 to 44,100 Hz in 8 or 16 bits |
| the WAV reader | `GetWavinfo`, `codemp/client/snd_mem.cpp:129` (the single player has its copy) | the engine's own RIFF parser; any other encoding refused ("Microsoft PCM format only", `:169`) |
| the MP3 decoders | `codemp/mp3code`, `code/mp3code` | the Xing decoder of 1995 to 1997, in C; 20,163 lines for the two copies |
| the MP3 glue | `codemp/client/snd_mp3.cpp` (572 lines), `code/client/cl_mp3.cpp` (571) | unpacks an MP3 to PCM when it is registered (`MP3_UnpackRawPCM`, `snd_mem.cpp:395`), except under `sound/chars/` (`SOUND_CHARS_DIR`, `:710`), whose voices stay compressed and are decoded while they play (`MP3Stream_InitFromFile`, `:783`) |
| the cinematics | 14 RoQ files | their sound is in the RoQ stream, decoded by `cl_cin.cpp`, not by a file decoder: outside this survey |

No Ogg, FLAC or Opus file is in the game.

## 3. What other engines and libraries use

| project | decoders | source |
|---|---|---|
| Godot (master, November 2026) | MP3 by dr_mp3 (dr_libs, "mp3-0.7.4", 2026), Ogg Vorbis by libvorbis 1.3.7 with libogg 1.3.6, WAV by its own code with IMA-ADPCM or QOA compression (qoa, 2026) | primary: [thirdparty/README.md](https://github.com/godotengine/godot/blob/master/thirdparty/README.md), [importing audio samples](https://docs.godotengine.org/en/stable/tutorials/assets_pipeline/importing_audio_samples.html) |
| Godot, history | stb_vorbis replaced by libvorbis in Godot 4.0 (merged 10 September 2021): "Fixes a variety of crashing bugs in stb_vorbis", for about 5 % larger imported files; Opus support proposed in a pull request adding libopus and opusfile (open in April 2026, its merge not seen) | primary: [PR 52406](https://github.com/godotengine/godot/pull/52406), [PR 117921](https://github.com/godotengine/godot/pull/117921) |
| raylib (raudio) | dr_wav, dr_mp3, dr_flac (off by default), stb_vorbis, qoa; the device by miniaudio | primary: [raudio.c](https://github.com/raysan5/raylib/blob/master/src/raudio.c); secondary: [raylib's dependencies](https://github.com/raysan5/raylib/wiki/raylib-dependencies) |
| SDL3_mixer | both kinds, chosen at build: dr_mp3 and dr_flac (dr_libs), stb_vorbis, and libmpg123, libFLAC, libvorbis or Tremor, opusfile with libopus, WavPack, libxmp, libgme, FluidSynth; zlib licence; needs SDL 3.4.0 | primary: [SDL_mixer's `src`](https://github.com/libsdl-org/SDL_mixer/tree/main/src) and `external` folders |
| ioquake3 | Ogg Vorbis by libvorbis and libogg, Ogg Opus by opusfile and libopus (also for its voice chat), bundled or the system's | community: [ioq3's Makefile](https://github.com/mickael9/ioq3/blob/urt/Makefile) (a fork), [OpenIndiana's package](https://github.com/OpenIndiana/oi-userland/pull/28322) |
| Unreal Engine | imports .wav, .ogg, .flac, .aif, .opus, .mp3 and converts them to 16-bit WAV; ships Bink Audio (the default), ADPCM, PCM, Opus, RAD Audio or the platform's codec | primary: [importing audio files](https://dev.epicgames.com/documentation/unreal-engine/importing-audio-files), [`ESoundAssetCompressionType`](https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/ESoundAssetCompressionType?lang=en-US) |

So the engines that ship their decoders as source take either the single-file decoders (dr_libs, stb_vorbis, qoa) or the reference libraries (libvorbis, libopus, libFLAC, libmpg123), Godot moving from the first kind to the second for Vorbis after crashes.

## 4. The candidates found

Found by web searches on 19 November 2026 (decoding libraries for MP3, WAV, FLAC, Ogg Vorbis and Opus in C and C++, single-file decoders, comparisons and benchmarks, what engines bundle), then checked on each project's own page. Activity read from GitHub's API the same day (last push or last commit on the default branch, latest release); fuzzing read from OSS-Fuzz's project list (`projects/<name>/project.yaml` in google/oss-fuzz).

### 4.1 Single-file decoders (one format each, permissive)

| library | formats | licence | version, activity | fuzzing | notes |
|---|---|---|---|---|---|
| **dr_libs** (dr_wav, dr_mp3, dr_flac) | WAV: RIFF, RIFX, AIFF, RF64, W64; unsigned 8-bit, signed 12 to 32-bit PCM, 32 and 64-bit float, A-law, µ-law, Microsoft ADPCM, IMA ADPCM. MP3 "based off minimp3". FLAC | public domain or MIT-0 | dr_wav 0.14.7; last commit 18 November 2026 | OSS-Fuzz: `dr_wav_fuzzer`, `dr_flac_fuzzer`, `dr_mp3_fuzzer` | Godot's and raylib's MP3 decoder; no Vorbis or Opus. Primary: [repository](https://github.com/mackron/dr_libs), [dr_wav.h](https://raw.githubusercontent.com/mackron/dr_libs/master/dr_wav.h) |
| **minimp3** | MP1, MP2, MP3 | CC0-1.0 | no release; last commit 18 November 2026 | its own `fuzz` folders, Coverity; no OSS-Fuzz project | SSE and NEON; "Conformance test passed on all vectors (PSNR > 96db)", 1.5 to 2.8 million clock ticks per second of sound on an i7-6700K. Primary: [README](https://github.com/lieff/minimp3) |
| **stb_vorbis** | Ogg Vorbis | public domain or MIT | 1.24; stb's last commit 12 November 2026 | OSS-Fuzz: `stb_vorbis_fuzzer` | Godot left it for its crashes (section 3); stb's README: "it may take significant time for security fixes to be implemented or merged". Community: a corrupted-file test under AddressSanitizer failed 4 of 700 cases where libvorbis failed none ([OpenEngine PR 19](https://github.com/Game-Crafters-Guild/OpenEngine/pull/19)). Primary: [repository](https://github.com/nothings/stb) |
| **miniflac** | FLAC, Ogg FLAC | not read | not read | not read | no C library call, no allocation, push or pull API; primary: [repository](https://github.com/jprjr/miniflac) |
| **libfoxenflac** | FLAC | LGPL-2.1 | last push 29 March 2025 | not read | no allocation; "about 25% faster than the flac reference decoder application" on x86-64 (its claim); no seeking. Primary: [repository](https://github.com/astoeckel/libfoxenflac) |
| **qoa** | QOA (the "Quite OK Audio" format, lossy) | MIT | last push 8 November 2026 | none: "This implementation has not yet been fuzzed. Don't use it with untrusted input." | used by Godot and raylib; no file of the game is QOA. Primary: [repository](https://github.com/phoboslab/qoa) |

### 4.2 The decoders of miniaudio

| library | formats | licence | version, activity | fuzzing | notes |
|---|---|---|---|---|---|
| **miniaudio's `ma_decoder`** | WAV, FLAC, MP3 built in (the dr_libs code); Ogg Vorbis and Opus through decoders of its `extras` folder on libvorbis and libopus with opusfile | public domain or MIT-0 | 0.11.26 (31 October 2026); already in `build-netcodec/audiolibs/` from the survey of 15 November 2026 | no OSS-Fuzz project; a heap use-after-free reported in its libopus decoder (community: [issue 1155](https://github.com/mackron/miniaudio/issues/1155)) | the device and mixer compile out (`MA_NO_DEVICE_IO`, `MA_NO_ENGINE`, `MA_NO_NODE_GRAPH`, `MA_NO_RESOURCE_MANAGER`), leaving one interface over every format, read by frames with seeking. Primary: [manual](https://miniaud.io/docs/manual/index.html), [custom decoder example](https://github.com/mackron/miniaudio/blob/master/examples/custom_decoder.c) |

### 4.3 The reference libraries (one format each)

| library | formats | licence | version, activity | fuzzing | notes |
|---|---|---|---|---|---|
| **libmpg123** | MPEG 1, 2 and 2.5, layers 1 to 3 | LGPL-2.1 | 1.33.7 (2 August 2026), a security release | OSS-Fuzz | assembler optimisations chosen at run time on x86, x86-64 and ARM; "500 times realtime" on a Core 2 Duo (its claim). Primary: [mpg123.de](https://www.mpg123.de/) |
| **libFLAC** | FLAC, Ogg FLAC | BSD-3-Clause (the library) | 1.5.0 (11 February 2025); last push 28 October 2026 | OSS-Fuzz | the reference. Primary: [repository](https://github.com/xiph/flac) |
| **libvorbis** with **libogg** | Ogg Vorbis | BSD-3-Clause | libvorbis 1.3.7 (4 July 2020), last push 4 August 2026; libogg 1.3.6 (16 June 2025) | OSS-Fuzz (`vorbis`) | the reference; Godot's choice. Allocates while decoding, a reason once given for stb_vorbis (community: [Godot issue 7496](https://github.com/godotengine/godot/issues/7496)). Primary: [vorbis](https://github.com/xiph/vorbis), [ogg](https://github.com/xiph/ogg) |
| **Tremor** | Ogg Vorbis, fixed point | BSD-3-Clause | not read | not read | for processors without floating point (secondary: [Wikipedia](https://en.wikipedia.org/wiki/Tremor_(software))); no reason to use it on this project's targets |
| **libopus** with **opusfile** | Opus in Ogg | BSD-3-Clause | libopus 1.6.1 (14 January 2026); opusfile 0.12 (27 June 2020), last push 29 March 2026 | OSS-Fuzz (`opus`, `opusfile`) | the reference; opusfile needs libogg. Primary: [opus-codec.org downloads](https://opus-codec.org/downloads/) |

### 4.4 Libraries that read several formats behind one interface

| library | formats | licence | version, activity | fuzzing | notes |
|---|---|---|---|---|---|
| **libsndfile** | WAV and many PCM containers, FLAC, Ogg Vorbis, Ogg Opus (since 1.0.29), MP3 (since 1.1.0) | LGPL-2.1 | 1.2.2 (13 August 2023); last push 28 October 2026 | OSS-Fuzz | the compressed formats through libFLAC, libvorbis, libopus, libmpg123 and LAME, so five libraries more. Primary: [supported formats](https://libsndfile.github.io/libsndfile/formats.html), [releases](https://github.com/libsndfile/libsndfile/releases) |
| **libnyquist** | WAV (with IMA ADPCM), MP3, Ogg Vorbis, Ogg Opus, FLAC, WavPack, Musepack | BSD-2-Clause | no release; last commit 28 March 2026 | no OSS-Fuzz project | C++11; bundles libFLAC, libogg, libvorbis, minimp3, Musepack, libopus, WavPack (and RtAudio); output always 32-bit float; whether it streams not stated. Primary: [README](https://github.com/ddiakopoulos/libnyquist), [`third_party`](https://github.com/ddiakopoulos/libnyquist/tree/master/third_party) |
| **CHOC** (Tracktion) | WAV, FLAC, Ogg Vorbis, MP3 (read only); no Opus | ISC | no release; last push 23 October 2026 | not read | header-only C++; which decoders it embeds not read. Primary: [repository](https://github.com/Tracktion/choc) |
| **FFmpeg** (libavcodec, libavformat) | every format here and many more | LGPL-2.1 or later (GPL with some options) | last push 19 November 2026 | OSS-Fuzz | its legal page advises dynamic linking for the LGPL; large. Primary: [legal](https://ffmpeg.org/legal.html) |
| **glint** | decodes MP3, AAC-LC, Opus, Ogg Vorbis, FLAC; encodes MP3, AAC, Opus (CELT only) | MIT | v0.10.0 (2 November 2026); created 23 June 2026; 3 stars | ASan and UBSan fuzzing of its own | C++17, written anew from the specifications; its conformance figures are its own (the 12 Opus test vectors passed, 117 to 131 dB against FFmpeg). Primary: [README](https://github.com/CrispStrobe/glint) |
| **orpheic-decoders** | MP3, FLAC, DST; no Vorbis, Opus or WAV | MIT | created 16 November 2026; 1 star | not read | C++17, "written by Claude, an AI model from Anthropic" (its page); its own measurements: MP3 2.25 times and FLAC 2.37 times FFmpeg's speed, one minute of MP3 in 9.4 ms against 25.87 for mpg123 and 27.02 for minimp3 (i9-14900K). Primary: [orpheic.com/decoders](https://orpheic.com/decoders/), [repository](https://github.com/orpheic/orpheic-decoders) |
| **micro-decoder** | FLAC, MP3, Opus, Vorbis, WAV | Apache-2.0 | created 21 September 2026; 0 stars | not read | for the ESP32 and host platforms. Primary: [repository](https://github.com/FutureProofHomes/micro-decoder) |

### 4.5 Found and set aside

| library | why |
|---|---|
| Symphonia | Rust, no C interface (planned, not offered): a second language and tool chain in the client. MPL-2.0; "generally +/-15% the performance of FFMpeg" (its claim). Primary: [repository](https://github.com/pdeljanov/Symphonia) |
| SDL3_mixer, SDL_sound | they need SDL, and the sound of plan step 21 does not use SDL |
| SDL_audiolib | LGPL-3.0, wraps dr_libs, libvorbis and opusfile; last push 25 February 2023 |
| libsoundwave | a 2018 fork of libnyquist without MP3, not maintained since |
| SoLoud | a sound engine, not a decoder; last release 7 February 2020 |
| Audiere | last release 14 February 2006 |

### 4.6 Measurements published by others

| measurement | numbers | source |
|---|---|---|
| MP3, one minute at 128 kbit/s, i9-14900K | mpg123 25.87 ms, minimp3 27.02 ms, FFmpeg 24.4 ms | primary for its claim (Orpheic, which compares its own decoder): [orpheic.com/decoders](https://orpheic.com/decoders/) |
| MP3, distance from a double-precision decoding (millionths of full scale) | minimp3 0.48, mpg123 1.18, FFmpeg 1.34 | the same page; the reference is its own decoder ("a witness, not an outside authority") |
| MP3, dr_mp3 against mpg123 | the same accuracy (worst average error −141.5 and −141.6 dBFS) | community: [vibe PR 89](https://github.com/cmicali/vibe/pull/89) |
| Vorbis, stereo file | libvorbis 10.8 ms, stb_vorbis 6.0 ms; stb_vorbis wrong on a six-channel file | community: [gui_forms PR 8](https://github.com/falseywinchnet/gui_forms/pull/8) |

None times every candidate on one machine, none uses this game's files: section 5 does.

## 5. The measurements

### 5.1 Method

Measured on 19 November 2026 on the development machine, with the tools of `tools/audiodec/` (llvm-mingw clang 23, Release, x86-64 without `-march`, linked statically):

- the candidates chosen in section 6, built from their sources (`build_libs.sh` for the libraries with a CMake project: libogg 1.3.6, libvorbis 1.3.7, libFLAC 1.5.0, libopus 1.6.1, libmpg123 1.33.7, libnyquist at its last commit; the single-file ones compiled into the harness: dr_libs, minimp3, stb_vorbis 1.24 at commits of 18 and 19 November 2026, miniaudio 0.11.26; opusfile 0.12 from its four files); the engine's decoder (`codemp/mp3code`, with the project's math library) called as the engine calls it (`snd_mp3.cpp`: the size pass, then the decoding; mono unless the file is stereo); the engine's WAV reader rewritten as `GetWavinfo` and `ResampleSfx` read (PCM only, 8-bit made 16-bit with `(x - 128) << 8`);
- the game's files (`make_corpus.py`: 12,902 MP3 and 302 WAV from the four archives); for FLAC, Ogg Vorbis and Opus, which the game lacks, a sample (`encode.cpp`: every stereo file, the 132 pieces of music, and every 40th mono file, 321 sounds; 453 files, 8,337 s) decoded by dr_mp3 and encoded by the reference encoders: libFLAC level 5, libvorbis quality 4, libopusenc at 48 kbit/s mono and 96 kbit/s stereo;
- one executable per decoder and format (`bench.cpp` with `dec_*.cpp`): every file decoded to 16-bit samples, hashed, and compared with a second decoder of the same files (`--compare`), aligned since the decoders may drop the encoder's delay and padding or the Xing frame, both polarities tried; per file and in total: identical or not, the largest difference, the frames gained or lost, the signal to noise ratio;
- the MPEG conformance vectors (82 bitstreams with their reference decoding, in minimp3's repository) through `vectors.py`, a vector passing above 96 dB as minimp3's test counts it;
- robustness: the same executables built with AddressSanitizer (`-fsanitize-recover=address`, every library built with it too: `build_libs.sh <folder> asan`), on damaged copies of the files (`--mutate`: cut short at a random length, 1 to 16 bytes changed, a block of 64 bytes overwritten), every fault counted;
- the code a decoder adds: the stripped executable against the harness with no decoder (`dec_none.cpp`).

### 5.2 MP3

The game's 12,902 files, 33,940 s of sound (as minimp3 decodes them):

| decoder | files decoded | against libmpg123 | against the engine's | start | end |
|---|---|---|---|---|---|
| the engine's (`mp3code`) | 12,902 | polarity inverted on every file; 50 files differ by more than 2 steps, up to 3,898; signal to noise 70.7 dB in all | | plays the Xing frame (26 ms of silence) | stops 1 to 5 frames early on 3,054 files |
| minimp3 (frame decoder) | 12,902 | within 2 steps on every file, 103.2 dB in all | inverted, 1 step apart but on the 50 files | plays the Xing frame | to the last frame |
| minimp3 (`minimp3_ex.h` loader) | refuses the 11,505 files whose Xing header says 0 frames (it trusts that count; 128 of the first 300 files) | | | | |
| dr_mp3 | 12,902 | within 2 steps on every file (12,893 within one), 103.2 dB in all (its decoder is minimp3's) | as minimp3 | skips the Xing frame (12,443 files) and the encoder's delay of 529 samples where it finds it (88) | to the last frame |
| libmpg123 | 12,902 | | | skips the Xing frame (11,504 files) and the encoder's delay (1,028) | to the last frame; the 59 files cut inside their last frame (50 of them footsteps) end in an error after their samples, kept by the harness |
| miniaudio | 12,902 | as dr_mp3, whose code it holds | as dr_mp3 | as dr_mp3 | to the last frame |
| libnyquist | refuses the same files as minimp3's loader, which it uses | | | | |

The 50 files the engine decodes wrongly (minimp3 and libmpg123 agree on them within 2 steps): 37 footsteps (`sound/player/footsteps/`), `sound/player/boon.mp3` (3,898 steps), `sound/player/enlightenment.mp3` (1,748), the Mark 1 droid's three walk sounds (`sound/chars/mark1/misc/mark1_fs_walk_*.mp3`, up to 860), `sound/weapons/thermal/warning.mp3` (708; 16.8 dB), and doors, movers and effects. On `mark1_fs_walk_1.mp3` the engine follows the others for five frames, then departs from them.

The conformance vectors (82 with a reference):

| decoder | above 96 dB | refused | notes |
|---|---|---|---|
| the engine's | 5 | 9 | inverted on all; 68 within one step but at 93 to 95 dB (the step is wrong on many samples); wrong: `l3-si_block` 64.9 dB, `l3-test45` and `l3-test46` 31 dB, `M2L3_noise` 27.9 dB, `l1-fl6` 33.6 dB; one frame more at the start of the Layer III vectors |
| minimp3 | 71 | 10 (the non-standard ones: APE tags, a file of ID3v2 only, truncated or empty Xing tags) | |
| dr_mp3 | 73 | 8 | `l3-he_mode`: stops where the stream changes between mono and stereo (115,200 frames short) |
| libmpg123 | 36 | 37 | refuses every stream of one frame ("Cannot read next header, a one-frame stream?"): the 25 one-frame vectors `ILL2_*` and `ILL4_*`, and some of the non-standard ones; `l3-test45`, `l3-test46` 33 to 37 dB |
| miniaudio | 71 | 10 | dr_mp3's |

The game's files are all MPEG-1 Layer III, but conformance is a criterion of its own: files added to the game later may be anything the format allows (decided on 19 November 2026).

### 5.3 WAV, FLAC, Ogg Vorbis, Opus

| format, files | decoder | result |
|---|---|---|
| WAV, the game's 302 | dr_wav, miniaudio | identical to the engine's reader on every file |
| | libnyquist | 245 files differ, by up to 256 steps (its conversion through normalised floats does not keep 16-bit samples) |
| FLAC, the 453 of the sample | libFLAC, dr_flac, miniaudio | the source's samples exactly, every file, with or without the total length in the header |
| | libnyquist | crashes (a write through a null pointer in `memcpy`) on every file written as a stream, without the total length in STREAMINFO (valid FLAC: libFLAC writes it so without a seek callback); with the length, 444 of 453 differ from the source (its float conversion) |
| Ogg Vorbis, 453 | stb_vorbis 1.24 | within one step of libvorbis on every file, 83.0 dB |
| | miniaudio (its stb_vorbis 1.22) | within 2 steps, 74.5 dB |
| | libnyquist | 448 of 453 differ from libvorbis |
| Opus, 453 | miniaudio (its libopus decoder) | identical to opusfile (it is opusfile) |
| | libnyquist | every file differs from opusfile |

Every Opus candidate decodes with libopus; no decoder of Opus independent of it was in the list measured.

### 5.4 Robustness

Damaged copies under AddressSanitizer (every 6th MP3 below 200 KB: 2,134 files, 3 copies each; every WAV, 9 copies; the FLAC, Vorbis and Opus files of the sample below 400 KB, 3 copies each):

| format, damaged inputs | no fault | faults |
|---|---|---|
| MP3, 6,402 | minimp3, dr_mp3, libmpg123, miniaudio | the engine's: 4 reads outside the buffer (1 byte each): the test of the rear ID3 tag reads 128 bytes before the end, so before the start of a file shorter than 128 bytes (`BYTESREMAINING_ACCOUNT_FOR_REAR_TAG`, `C_MP3_GetUnpackedSize`), and in `head_info` (2) and `C_MP3_UnpackRawPCM`; libnyquist: an access violation in `mp3_decode_internal` |
| WAV, 2,718 | the engine's reader (rewritten here), dr_wav, miniaudio | libnyquist: a heap read overflow in `WavDecoder::LoadFromBuffer` |
| FLAC, 969 | libFLAC, dr_flac, miniaudio, libnyquist | |
| Ogg Vorbis, 1,221 | libvorbis, stb_vorbis 1.24, libnyquist | miniaudio: its stb_vorbis 1.22 asks `malloc` for 0xFFFFFFFFE5AABB68 bytes (a negative size) in `start_decoder` |
| Opus, 1,224 | opusfile, miniaudio, libnyquist | |

What a decoder still decodes of the damaged inputs (a file counts if any sample comes out): MP3 6,350 (minimp3, miniaudio, the engine's 6,352), 6,284 (libmpg123), 6,280 (dr_mp3); FLAC 967 (dr_flac, miniaudio), 935 (libFLAC); Ogg Vorbis 851 (stb_vorbis), 800 (libvorbis); Opus 603 (opusfile, miniaudio).

### 5.5 Size and memory

The code each adds to a statically linked program (stripped, against the harness alone, 335,872 bytes):

| decoder | bytes |
|---|---|
| the engine's WAV reader | 1,024 |
| minimp3 | 35,840 |
| dr_mp3 | 57,856 |
| dr_wav | 67,584 |
| stb_vorbis 1.24 | 71,168 |
| dr_flac | 109,568 |
| libFLAC | 133,120 |
| libopus with opusfile and libogg | 284,160 |
| libvorbis with vorbisfile and libogg | 305,152 |
| libmpg123 | 317,952 |
| the engine's MP3 decoder (with the project's math library, which the client has anyway) | 390,656 |
| miniaudio's decoders: WAV, FLAC, MP3 | 437,760 |
| the same with stb_vorbis | 513,024 |
| libnyquist (every format) | 2,183,168 |

The decoders' own state, outside what they allocate: minimp3's `mp3dec_t` 6,668 bytes, dr_mp3's `drmp3` 32,376, dr_wav's `drwav` 408, miniaudio's `ma_decoder` 552. The heap each uses while decoding is not measured.

### 5.6 Speed

Measured on 19 November 2026, the VMs off (`timing.sh`): every decoder on the whole corpus of its format, each file decoded 5 times (the first is the cold time, the fastest the warm one, summed over the files; the output's allocation included), on logical processor 20, in two rounds alternating the decoders. The two rounds within 1 % of each other, the engine's decoder within 4.7 %; the warm time of the first round:

| format, sound | decoder | warm | per second of sound | against the engine's |
|---|---|---|---|---|
| MP3, the game's 12,902 files, about 33,700 s | the engine's (with its size pass, as the engine calls it) | 13.39 s | 0.40 ms | |
| | libmpg123 | 8.81 s | 0.26 ms | −34 % |
| | minimp3 | 9.22 s | 0.27 ms | −31 % |
| | miniaudio (dr_mp3) | 9.43 s | 0.28 ms | −30 % |
| | dr_mp3 | 9.67 s | 0.29 ms | −28 % |
| WAV, the game's 302, 755 s | the engine's reader | 0.4 ms | | |
| | dr_wav | 0.5 ms | | |
| | miniaudio | 1.0 ms | | |
| FLAC, the sample, 8,337 s | libFLAC | 3.07 s | 0.37 ms | |
| | miniaudio (dr_flac) | 3.30 s | 0.40 ms | |
| | dr_flac | 3.53 s | 0.42 ms | |
| Ogg Vorbis, the sample, 8,337 s | miniaudio (its stb_vorbis 1.22) | 4.74 s | 0.57 ms | |
| | stb_vorbis 1.24 | 4.88 s | 0.59 ms | |
| | libvorbis | 6.90 s | 0.83 ms | |
| Opus, the sample, 8,337 s | miniaudio (opusfile) | 14.22 s | 1.71 ms | |
| | opusfile | 14.28 s | 1.71 ms | |

The cold times are 0.2 to 4 % above the warm ones. The sample of FLAC, Vorbis and Opus holds the music (stereo), the MP3 corpus mostly mono sounds: a second of sound costs more there, so the formats compare only within the same sample. The sound is decoded when it is registered (a map's load); the music and the voices of `sound/chars/` are decoded while they play, about 0.3 ms per second of sound in MP3 (0.003 ms per frame at 100 frames per second, less at 333), 1.7 ms in Opus. dr_mp3 and minimp3 run the same decoder, and miniaudio holds dr_mp3 and dr_flac: their differences (dr_mp3 5 % above minimp3, 2.5 % above miniaudio; dr_flac 7 % above miniaudio) are the loading around the decoder (dr_libs' "open and read everything" functions grow their output as they go), which an integration reading by frames into a buffer allocated once does not pay.

### 5.7 Accuracy at a glance

A step is 1/32,768 of full scale (−90 dBFS); a difference of 1 or 2 steps is rounding.

MP3, the game's 12,902 files against libmpg123:

| decoder | polarity | files within 1 step | largest difference | files beyond 2 steps | signal to noise in all | worst file | end of the sound |
|---|---|---|---|---|---|---|---|
| minimp3 | right | 12,893 | 2 | 0 | 103.2 dB | 75.4 dB | whole |
| dr_mp3 | right | 12,893 | 2 | 0 | 103.2 dB | 75.4 dB | whole |
| miniaudio (dr_mp3) | right | 12,893 | 2 | 0 | 103.2 dB | 75.4 dB | whole |
| the engine's | inverted | 12,845 | 3,898 | 50 | 70.7 dB | 16.8 dB | cut on 3,054 files |

MP3, the 82 conformance vectors with a reference (above 96 dB passes): dr_mp3 73, minimp3 71, miniaudio 71, libmpg123 36 (37 refused, every one-frame stream among them), the engine's 5 (section 5.2).

| format, files | against | decoder | result |
|---|---|---|---|
| WAV, 302 | the engine's reader | dr_wav, miniaudio | identical |
| | | libnyquist | 245 files differ, up to 256 steps |
| FLAC, 453 | the source | libFLAC, dr_flac, miniaudio | identical, 453 of 453 |
| | | libnyquist | 9 of 453 identical; crashes without the total length in the header |
| Ogg Vorbis, 453 | libvorbis | stb_vorbis 1.24 | within 1 step, 83.0 dB (worst file 72.7 dB) |
| | | miniaudio (its stb_vorbis 1.22) | within 2 steps, 74.5 dB (worst file 53.0 dB) |
| | | libnyquist | 448 of 453 differ |
| Opus, 453 | opusfile | miniaudio | identical (it is opusfile) |
| | | libnyquist | every file differs |

### 5.8 The decided decoders on Linux and with MSVC

Checked on 19 November 2026, after the decisions of section 8, on the VMs: `linux_check.sh` (Ubuntu 22.04, GCC 15 and Clang 23) and `msvc_check.bat` (Visual Studio 2026) build libogg, libvorbis and libFLAC from the sources downloaded for the survey (not the distribution's), then the decided decoders and their references (`CHOSEN_ONLY`), normally and with AddressSanitizer, and run them on the corpora (`check_run.sh`); `crosscheck.py` compares the hash of every file's 16-bit samples with the decodings of llvm-mingw clang on Windows.

| compiler | dr_mp3 (12,902 files), dr_wav (302), libFLAC (453) | stb_vorbis 1.24 (453) | damaged inputs under AddressSanitizer | compiler warnings |
|---|---|---|---|---|
| GCC 15, Linux | the same samples as clang on Windows, every file | the same, every file | no fault (6,402 MP3, 2,718 WAV, 969 FLAC, 1,221 Vorbis) | none (make's clock skew of the shared folder only) |
| Clang 23, Linux | the same, every file | the same, every file | no fault | none from the decoders (one in the engine's MP3 decoder, `/*` in a comment, which the project's `-Wno-comment` silences) |
| MSVC, Windows | the same, every file | 452 of 453 files differ by one step on about 0.07 % of their samples (libvorbis the same way) | no fault (MSVC stops at the first) | none |

The warnings counted are the compilers' defaults (the benchmark is not built with `-Wall` or `/W4`). The difference of MSVC on Vorbis is rounding in the floating-point math functions both decoders call (`sin`, `cos`, `exp` ...), inaudible and outside the game's simulation: stb_vorbis calls them directly (27 places) with no macro to replace them, so the same samples on every platform would need the library changed or the C library's names redefined before it: decided on 19 November 2026, stb_vorbis stays unchanged and the difference is accepted. dr_mp3, dr_wav and libFLAC call none and give the same samples everywhere. Fixed on the way in the tools: the libraries' install folder passed as `LIBS_PREFIX` (`PREFIX` is a name CMake's compiler identification uses: passed on the command line, it broke the identification, so no `-std=c++20` and no optimisation), `CMAKE_POLICY_VERSION_MINIMUM=3.5` for libvorbis 1.3.7 with CMake 4, `NOMINMAX` before `<windows.h>`, MSVC's AddressSanitizer runtime copied next to the programs as netcodec does.

## 6. Downloads

Decided on 19 November 2026 (option a): measured dr_libs, minimp3, stb_vorbis, miniaudio's decoders, libmpg123, libFLAC, libvorbis with libogg, libopus with opusfile, libopusenc (for the Opus sample) and libnyquist; not measured: libsndfile (the same reference libraries behind it), CHOC (no Opus), micro-decoder (written for the ESP32), glint and orpheic-decoders (weeks and days old), miniflac and libfoxenflac. The samples compared with the MPEG conformance vectors (shipped with minimp3), with the source for the lossless formats, with libvorbis for Vorbis and with opusfile for Opus, rather than with a library chosen as reference. Downloaded into `build-netcodec/audiolibs/decoders/` (ignored by git): the releases' archives, their SHA-256 checked against Xiph's list for libopus and opusfile; the Opus test vectors of RFC 8251 (not used yet).

## 7. Options

Every option reads the five formats; the Opus decoder is libopus with opusfile and libogg in all of them (284 KB, 1.7 ms per second of sound), the only one measured. For every option, against today: the polarity right, the 50 wrongly decoded files right, the ends of 3,054 sounds no longer cut; with dr_mp3 or libmpg123 the 26 ms of silence before 12,531 sounds dropped (the sound starts 26 ms earlier, 38 ms where the encoder's delay is known).

| option | MP3, WAV, FLAC, Ogg Vorbis | for | against | code added | MP3 time (game) |
|---|---|---|---|---|---|
| **A. the single-file decoders behind the module's own interface** | dr_mp3, dr_wav, dr_flac, stb_vorbis 1.24 | the smallest code; each within 1 to 2 steps of the references; each fuzzed by OSS-Fuzz (dr_libs, stb_vorbis, opus, opusfile, vorbis) and no fault here; dr_mp3 skips the Xing frame and the known delay; public domain or MIT-0, one file each, built with the project | the interface written for them (the plan step 21 module has its own interface anyway: load a sound, stream the music); dr_mp3 stops where a stream changes between mono and stereo (one conformance vector; minimp3's frame decoder does not) | about 307 KB + Opus | 9.67 s (−28 %) |
| **B. miniaudio's decoders** (`ma_decoder`, the device and mixer compiled out) | the same dr_libs code built in, stb_vorbis 1.24 in place of the 1.22 it bundles, its libopus decoder | one interface for every format already written (read by frames, seek, streaming) | 130 KB more than A for code the client does not use (its conversions); its bundled stb_vorbis must be replaced (the fault of section 5.4); a use-after-free reported in its libopus decoder (community, issue 1155), to check | about 513 KB + Opus | 9.43 s (−30 %) |
| **C. the reference libraries** | libmpg123, dr_wav (no reference library for WAV in the list), libFLAC, libvorbis | the references, fuzzed by OSS-Fuzz; MP3 9 % and FLAC 13 % faster than A | libmpg123 refuses a stream of one frame (a sound shorter than 52 ms plays nothing) and is LGPL-2.1 (as OpenAL Soft); libvorbis 41 % slower than stb_vorbis and allocates while decoding; four CMake projects to vendor | about 824 KB + Opus | 8.81 s (−34 %) |

The WAV reader apart (dr_wav, 68 KB, reads float, ADPCM, A-law, µ-law, AIFF, RF64 and W64 besides PCM), the engine's own reader would do for the game's files: identical, 1 KB, no fault, PCM only.

Format by format (asked the same day, from the speed and the accuracy), the options above mixed: the best decoder of each format, behind the module's own interface (miniaudio's would not hold libFLAC, and costs the code, the old stb_vorbis and the report of option B):

| format | recommended | accuracy | speed | why, against the others |
|---|---|---|---|---|
| MP3 | **dr_mp3** | 103.2 dB against libmpg123, as minimp3; the most conformance vectors passed (73 of 82) | −28 % against the engine's (−30 % read by frames, as miniaudio does) | the start right with no code of ours (the Xing frame and the known delay skipped: the sound starts 26 to 38 ms earlier), no fault, public domain, 58 KB. minimp3's frame decoder is the same decoder 2.5 to 5 % faster, but the Xing frame and the delay are then ours to handle; libmpg123 is 9 % faster but plays nothing for a one-frame stream, passes 36 vectors, is LGPL and 318 KB. dr_mp3's one limit: it stops where a stream changes between mono and stereo (one vector; no file of the game) |
| WAV | **dr_wav** | identical to the engine's reader | 0.5 ms for the game's 302 files | the formats beyond PCM, no fault, 68 KB |
| FLAC | **libFLAC** | identical to the source, as dr_flac | 7 to 13 % faster than dr_flac | the reference, fuzzed by OSS-Fuzz, no fault, BSD-3, 133 KB; one CMake project more to vendor. dr_flac: one file, the dr_libs family, 23 KB less, slower |
| Ogg Vorbis | **stb_vorbis 1.24** | within 1 step of libvorbis (83.0 dB) | 29 % faster than libvorbis | no fault here and fuzzed by OSS-Fuzz, one file, 71 KB; libvorbis is the reference but 41 % slower and allocates while decoding. Its history of crashes (Godot left it in 2021 for them; stb's README warns that security fixes can take long) asks for the module's own fuzzing of it, as netcodec's decoders have |
| Opus | **opusfile with libopus and libogg** | the reference (the only decoder measured) | 1.7 ms per second of sound | the only one; 284 KB |

Code added: about 614 KB (58 + 68 + 133 + 71 + 284); about 330 KB without Opus (decided the same day, section 8).

## 8. Decisions to take

1. MP3: dr_mp3 (recommended), minimp3's frame decoder, or libmpg123.
2. WAV: dr_wav (recommended) or the engine's own reader (PCM only).
3. FLAC: libFLAC (recommended) or dr_flac.
4. Ogg Vorbis: stb_vorbis 1.24 (recommended) or libvorbis.
5. Opus: opusfile with libopus and libogg (the only one).
6. The interface: the module's own (recommended) or miniaudio's decoders (option B).
Then the build checked on the Linux VM (Ubuntu 22.04, GCC 15 and Clang 23).

Decided (19 November 2026): 1, dr_mp3; 2, dr_wav; 3, libFLAC; 4, stb_vorbis 1.24, fuzzed by the module's own tests; 5, no Opus for now: no file of the game uses it, Ogg Vorbis already gives a free compressed format and FLAC a lossless one, and it would cost as much code as the four others (284 KB against about 330) with three libraries and the slowest decoding; it is added (one decoder behind the interface, with libopus, opusfile and libogg) when a use comes, music or voices; 6, the module's own interface. Then their build checked on Linux and with MSVC (section 5.8): the same samples as on Windows with clang for dr_mp3, dr_wav and libFLAC on every compiler, stb_vorbis's one step apart on 0.07 % of the samples with MSVC; no fault, no warning.

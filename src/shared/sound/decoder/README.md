# The decoders

A sound file of the game decoded into 16-bit samples, mono or stereo, from the file's bytes in memory. The format is chosen by the file's first bytes, not by its name: `RIFF` (and its other forms) for WAV, `fLaC` for FLAC, `OggS` for Ogg Vorbis, anything else for MP3. Every sample is 16 bits: a file of more loses its lowest bits, below -96 dB.

In the game: every sound file is an MP3 or a WAV (12,902 MP3 and 302 WAV in Jedi Academy's archives). A blaster's shot, `sound/weapons/blaster/fire.mp3`, is decoded whole when the level loads; the music of Yavin's swamp, `music/yavin1/swamp_explore.mp3`, is decoded as it plays. FLAC and Ogg Vorbis are for the files a mod brings.

| file | what it does |
|---|---|
| `decoder.h` | the interface: `Pcm` (a whole sound), `Decoder` (a file read as it plays: `Read`, `Seek`, `Frames`), `OpenDecoder` and `Decode` |
| `decoder.cpp` | the choice of the format; `Decode`, bounded to ten minutes of sound |
| `formats.h` | one function per format, each over its library |
| `wav.cpp` | dr_wav |
| `mp3.cpp` | dr_mp3, which drops the encoder's delay and padding when the file's header gives them |
| `flac.cpp` | libFLAC, through its callbacks |
| `vorbis.cpp` | stb_vorbis, all its memory from a block of 1 MB, so that a damaged file cannot make it allocate what it declares |

A sound is decoded whole when it is registered (`../bank`); the music is read through a `Decoder` as it plays (`../stream`).

## Example

```cpp
const std::vector<std::byte> file = ReadWholeFile("sound/weapons/blaster/fire.mp3");
if (std::optional<sound::Pcm> pcm = sound::Decode(file)) {
    // pcm->rate, pcm->channels, pcm->samples (interleaved)
}

std::unique_ptr<sound::Decoder> music = sound::OpenDecoder(file); // the file must outlive it
std::vector<std::int16_t> block(4096 * music->Channels());
const std::size_t frames = music->Read(block); // 0 at the end
```

## Tests

`test/sound/decoder.cpp` (each format on the sample files, against the WAV of the same signal, seeking, the frame counts, files that are not sounds, damaged copies) and the fuzzer `fuzz/fuzz_decode.cpp`.

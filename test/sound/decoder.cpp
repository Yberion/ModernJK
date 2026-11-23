/*
===========================================================================
Copyright (C) 2026, ModernJK contributors

This file is part of the ModernJK source code.

ModernJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

// The tests of src/shared/sound/decoder.h: each decoder on the sample files of test/sound/data (written by
// docs/modernization/tools/audiodec/make_test_samples.cpp, which printed the WAV hashes below), the FLAC files against
// the WAV of the same signal, the Ogg Vorbis and MP3 files by their signal to noise ratio against it, an MP3 behind an
// ID3v2 tag, seeking, files that are not sounds, and damaged copies of every sample (under the sanitizers' presets, a
// read outside a buffer stops the test).
#include "sound.h"

#include <check.h>

#include <shared/sound/decoder.h>

#include <cmath>
#include <cstdint>
#include <random>

namespace {

struct Expected {
    const char* file;
    int rate;
    int channels;
    std::size_t frames;
    std::uint64_t hash;
};

// what make_test_samples printed: the 8-bit file as (x - 128) << 8, the 24-bit one as its upper 16 bits
constexpr Expected kWavs[] = {
    {"mono22k_s16.wav", 22050, 1, 5512, 0x91f21f2d3d0982efull},
    {"mono11k_u8.wav", 11025, 1, 2756, 0x0fe6dcb311d7c135ull},
    {"stereo44k_s16.wav", 44100, 2, 4410, 0x4ff04829507f61b0ull},
    {"stereo44k_s24.wav", 44100, 2, 4410, 0x95c853f9edc58700ull},
};

// FNV-1a over the samples' little-endian bytes, as make_test_samples hashes them
std::uint64_t Hash(const std::vector<std::int16_t>& samples)
{
    std::uint64_t h = 1'469'598'103'934'665'603ull;

    for (const std::int16_t s : samples) {
        const auto u = static_cast<std::uint16_t>(s);

        for (int b = 0; b < 2; ++b) {
            h ^= (u >> (8 * b)) & 0xffu;
            h *= 1'099'511'628'211ull;
        }
    }

    return h;
}

// the ratio, in decibels, of the reference's power to the power of what the decoding changed
double SignalToNoise(const std::vector<std::int16_t>& reference, const std::vector<std::int16_t>& decoded)
{
    double signal = 0;
    double noise = 0;

    for (std::size_t i = 0; i < reference.size() && i < decoded.size(); ++i) {
        const double r = reference[i];
        const double d = decoded[i] - r;
        signal += r * r;
        noise += d * d;
    }

    return noise == 0 ? 999.0 : 10.0 * std::log10(signal / noise);
}

// a file's bytes as the decoders take them
std::span<const std::byte> Bytes(const std::vector<std::byte>& v)
{
    return v;
}

// a seek to a third of the file then a read to its end gives the whole decoding's samples from there, within
// tolerance steps
void CheckSeek(const std::vector<std::byte>& file, const std::vector<std::int16_t>& whole, int channels, int tolerance)
{
    const auto decoder = sound::OpenDecoder(file);
    CHECK(decoder != nullptr);

    if (!decoder) {
        return;
    }

    const std::size_t frames = whole.size() / static_cast<std::size_t>(channels);
    const std::size_t from = frames / 3;
    CHECK(decoder->Seek(from));
    std::vector<std::int16_t> rest(whole.size() - from * static_cast<std::size_t>(channels));
    CHECK_EQUAL(decoder->Read(rest), frames - from);
    int worst = 0;

    for (std::size_t i = 0; i < rest.size(); ++i) {
        worst = std::max(worst, std::abs(rest[i] - whole[from * static_cast<std::size_t>(channels) + i]));
    }

    CHECK(worst <= tolerance);
}

// every sample file decoded whole and after a seek, against what it must give
void TestFiles(const std::filesystem::path& data)
{
    for (const Expected& e : kWavs) {
        const auto file = test::ReadFile(data / e.file);
        const auto pcm = sound::Decode(Bytes(file));
        CHECK(pcm.has_value());

        if (!pcm) {
            continue;
        }

        CHECK_EQUAL(pcm->rate, e.rate);
        CHECK_EQUAL(pcm->channels, e.channels);
        CHECK_EQUAL(pcm->samples.size(), e.frames * static_cast<std::size_t>(e.channels));
        CHECK_EQUAL(Hash(pcm->samples), e.hash);
        CheckSeek(file, pcm->samples, pcm->channels, 0);
    }

    // FLAC is lossless: the samples of the WAV of the same signal, 24-bit brought to 16 as dr_wav does
    for (const char* name : {"stereo44k_s16", "stereo44k_s24"}) {
        const auto flac = test::ReadFile(data / (std::string(name) + ".flac"));
        const auto fromFlac = sound::Decode(Bytes(flac));
        const auto fromWav = sound::Decode(Bytes(test::ReadFile(data / (std::string(name) + ".wav"))));
        CHECK(fromFlac.has_value() && fromWav.has_value());

        if (fromFlac && fromWav) {
            CHECK_EQUAL(fromFlac->rate, 44100);
            CHECK_EQUAL(fromFlac->channels, 2);
            CHECK(fromFlac->samples == fromWav->samples);
            CheckSeek(flac, fromFlac->samples, 2, 0);
        }
    }

    // Ogg Vorbis and MP3 are lossy: the frames of the source (dr_mp3 drops the encoder's delay and padding that LAME's
    // Info tag gives), and its signal within the encoder's error; no hash, since stb_vorbis's samples differ by one step
    // between compilers (its math functions)
    struct Lossy {
        const char* file;
        const char* source;
        double snr; // in dB
    };
    // the ratios measured with stb_vorbis 1.25, dr_mp3 and clang on 23 November 2026, bounded by the signal's white
    // noise, which the encoders code loosely: Ogg Vorbis 28.8 dB mono and 18.5 dB stereo, MP3 (LAME 4.0, 64 and
    // 128 kbit/s) 23.1 and 22.8 dB; a sample off by one frame gives 12 to 16 dB, a decoding gone wrong below 0
    constexpr Lossy kLossy[] = {
        {"mono22k.ogg", "mono22k_s16.wav", 27.0},
        {"stereo44k.ogg", "stereo44k_s16.wav", 17.0},
        {"mono22k.mp3", "mono22k_s16.wav", 21.0},
        {"stereo44k.mp3", "stereo44k_s16.wav", 21.0},
    };

    for (const Lossy& l : kLossy) {
        const auto file = test::ReadFile(data / l.file);
        const auto decoded = sound::Decode(Bytes(file));
        const auto source = sound::Decode(Bytes(test::ReadFile(data / l.source)));
        CHECK(decoded.has_value() && source.has_value());

        if (decoded && source) {
            CHECK_EQUAL(decoded->rate, source->rate);
            CHECK_EQUAL(decoded->channels, source->channels);
            CHECK_EQUAL(decoded->samples.size(), source->samples.size());
            CHECK(SignalToNoise(source->samples, decoded->samples) > l.snr);
            CheckSeek(file, decoded->samples, decoded->channels, 1);
        }
    }

    // an MP3 behind an ID3v2 tag of 20 bytes, which dr_mp3 skips
    const auto mp3 = test::ReadFile(data / "mono22k.mp3");
    std::vector<std::byte> tagged = {std::byte{'I'}, std::byte{'D'}, std::byte{'3'}, std::byte{3}, std::byte{0},
                                     std::byte{0},   std::byte{0},   std::byte{0},   std::byte{0}, std::byte{20}};
    tagged.resize(tagged.size() + 20);
    tagged.insert(tagged.end(), mp3.begin(), mp3.end());
    const auto plain = sound::Decode(Bytes(mp3));
    const auto behindTag = sound::Decode(Bytes(tagged));
    CHECK(plain.has_value() && behindTag.has_value() && plain->samples == behindTag->samples);
}

// files the decoders must refuse, and the bound of a sound decoded whole
void TestNotSounds(const std::filesystem::path& data)
{
    CHECK(!sound::Decode({}).has_value());
    const auto wav = test::ReadFile(data / "mono22k_s16.wav");
    const auto flac = test::ReadFile(data / "stereo44k_s16.flac");
    const auto ogg = test::ReadFile(data / "mono22k.ogg");
    // each file cut inside its header
    CHECK(!sound::Decode(Bytes(wav).first(20)).has_value());
    CHECK(!sound::Decode(Bytes(flac).first(20)).has_value());
    CHECK(!sound::Decode(Bytes(ogg).first(20)).has_value());

    // the fuzzer's find of 23 November 2026, a damaged copy of mono22k.ogg whose vendor text declares 1.7 GB: refused by
    // stb_vorbis within the memory it is given
    CHECK(!sound::Decode(Bytes(test::ReadFile(data / "vorbis_vendor_length.ogg"))).has_value());

    // eleven minutes of silence (15 KB of FLAC): beyond what a sound decoded whole may last, yet read to its end as a
    // stream
    const auto silence = test::ReadFile(data / "silence_11min.flac");
    CHECK(!sound::Decode(Bytes(silence)).has_value());

    if (const auto decoder = sound::OpenDecoder(silence)) {
        std::vector<std::int16_t> out(65536);
        std::size_t frames = 0;
        bool silent = true;

        for (std::size_t n = decoder->Read(out); n > 0; n = decoder->Read(out)) {
            frames += n;
            silent = silent && std::all_of(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(n), [](std::int16_t s) { return s == 0; });
        }

        CHECK_EQUAL(frames, std::size_t{8000 * 660});
        CHECK(silent);
    }
    else {
        CHECK(false);
    }
}

// Damaged copies: cut short, 1 to 16 bytes changed, or a block of 64 bytes overwritten, then decoded whole and read
// after a seek; nothing is checked but that the decoders stop cleanly
void TestDamaged(const std::filesystem::path& data)
{
    std::minstd_rand random(20'261'123);
    auto below = [&random](std::size_t n) { return n == 0 ? std::size_t{0} : static_cast<std::size_t>(random()) % n; };

    for (const char* name : {"mono22k_s16.wav", "mono11k_u8.wav", "stereo44k_s24.wav", "stereo44k_s16.flac", "stereo44k_s24.flac", "mono22k.ogg",
                             "stereo44k.ogg", "mono22k.mp3", "stereo44k.mp3"}) {
        const auto original = test::ReadFile(data / name);

        for (int copy = 0; copy < 200; ++copy) {
            std::vector<std::byte> damaged = original;

            switch (copy % 3) {
            case 0:
                damaged.resize(below(damaged.size()));
                break;
            case 1:
                for (std::size_t n = 1 + below(16); n > 0; --n) {
                    damaged[below(damaged.size())] = static_cast<std::byte>(random());
                }

                break;
            default: {
                const std::size_t at = below(damaged.size() - 64);

                for (std::size_t i = 0; i < 64; ++i) {
                    damaged[at + i] = static_cast<std::byte>(random());
                }
            } break;
            }
            (void)sound::Decode(Bytes(damaged));

            if (const auto decoder = sound::OpenDecoder(damaged)) {
                std::vector<std::int16_t> out(1024);

                if (decoder->Seek(below(4096))) {
                    while (decoder->Read(out) > 0) {
                    }
                }
            }
        }
    }
}

} // namespace

// the decoders' tests, on the files of the data folder
void RunDecoderTests(const std::filesystem::path& data)
{
    TestFiles(data);
    TestNotSounds(data);
    TestDamaged(data);
}

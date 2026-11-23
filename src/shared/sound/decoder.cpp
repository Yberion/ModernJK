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

#include "decoder.h"

#include "formats.h"

#include <algorithm>
#include <string_view>

namespace sound {

namespace {

// whether the file's first bytes are the characters of tag
[[nodiscard]] bool StartsWith(std::span<const std::byte> file, std::string_view tag) noexcept
{
    return file.size() >= tag.size() && std::ranges::equal(file.first(tag.size()), tag, [](std::byte b, char c) { return std::to_integer<char>(b) == c; });
}

// the decoder of the format the file's first bytes name, or nullptr when that decoder cannot open it
[[nodiscard]] std::unique_ptr<Decoder> OpenByContent(std::span<const std::byte> file)
{
    // RIFF and its big-endian, 64-bit and Wave64 forms are dr_wav's to tell apart
    if (StartsWith(file, "RIFF") || StartsWith(file, "RIFX") || StartsWith(file, "RF64") || StartsWith(file, "riff")) {
        return OpenWav(file);
    }

    if (StartsWith(file, "fLaC")) {
        return OpenFlac(file);
    }

    if (StartsWith(file, "OggS")) {
        return OpenVorbis(file);
    }

    // an MP3 has no signature of its own: an ID3 tag or a frame header; dr_mp3 skips what comes before the first frame
    return OpenMp3(file);
}

} // namespace

// OpenByContent's decoder, kept only for a mono or stereo sound with a rate, what the sound plays
std::unique_ptr<Decoder> OpenDecoder(std::span<const std::byte> file)
{
    auto decoder = OpenByContent(file);

    if (!decoder || decoder->Rate() <= 0 || decoder->Channels() < 1 || decoder->Channels() > 2) {
        return nullptr;
    }

    return decoder;
}

// the decoder's samples read in chunks into one vector until it gives no more
std::optional<Pcm> Decode(std::span<const std::byte> file)
{
    const auto decoder = OpenDecoder(file);

    if (!decoder) {
        return std::nullopt;
    }

    Pcm pcm{.rate = decoder->Rate(), .channels = decoder->Channels(), .samples = {}};
    const auto channels = static_cast<std::size_t>(pcm.channels);
    // A sound decoded whole lasts at most ten minutes, the game's longest (its music) under four: a damaged or hostile
    // file cannot make it grow without bound (a FLAC frame of one constant value gives 65,535 samples for a few bytes)
    constexpr std::size_t maxSeconds = 600;
    const std::size_t maxSamples = static_cast<std::size_t>(pcm.rate) * maxSeconds * channels;
    constexpr std::size_t chunkFrames = 16384;

    for (;;) {
        const std::size_t used = pcm.samples.size();
        pcm.samples.resize(used + (chunkFrames * channels));
        const std::size_t frames = decoder->Read(std::span(pcm.samples).subspan(used));
        pcm.samples.resize(used + (frames * channels));

        if (frames == 0) {
            break;
        }

        if (pcm.samples.size() > maxSamples) {
            return std::nullopt;
        }
    }

    if (pcm.samples.empty()) {
        return std::nullopt;
    }

    return pcm;
}

} // namespace sound

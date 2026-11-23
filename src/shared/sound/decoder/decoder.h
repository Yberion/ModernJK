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

// decoder.h -- the sound's file decoders: WAV, MP3, FLAC and Ogg Vorbis, chosen by the file's content, read from a
// file its caller keeps in memory, and giving 16-bit samples interleaved by channel, mono or stereo.
// Example: Decode(file) gives a whole sound when it is registered; OpenDecoder(file) gives a decoder the music reads
// block by block, Read(out) returning 0 at the end.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace sound {

// The rates a file may have: a rate outside them is a damaged or hostile file, refused (the game's are 11,025 to
// 44,100 Hz).
inline constexpr int kMinRate = 1000;
inline constexpr int kMaxRate = 192000;

// A whole sound: its rate in hertz, its channels (1 or 2) and its samples, interleaved by channel.
struct Pcm {
    int rate = 0;
    int channels = 0;
    std::vector<std::int16_t> samples;
};

// A file decoded as it is read: a stream of music goes through one, and a sound is decoded whole through one. It reads
// the file from the memory given to OpenDecoder, which must outlive it.
class Decoder {
public:
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;
    Decoder(Decoder&&) = delete;
    Decoder& operator=(Decoder&&) = delete;
    virtual ~Decoder() = default;

    [[nodiscard]] int Rate() const noexcept { return rate_; }
    [[nodiscard]] int Channels() const noexcept { return channels_; }

    // Decodes up to out.size() / Channels() frames into out and returns the frames written: 0 at the end of the file,
    // and at a damaged part the decoder cannot go past.
    [[nodiscard]] virtual std::size_t Read(std::span<std::int16_t> out) = 0;
    // Moves to a frame counted from the start; false when the decoder cannot read from there.
    [[nodiscard]] virtual bool Seek(std::uint64_t frame) = 0;
    // The file's frames, nullopt when the format does not tell them; reading goes on from where it was.
    [[nodiscard]] virtual std::optional<std::uint64_t> Frames() = 0;

protected:
    Decoder() = default;
    // set by each format's decoder once its library has read the file's header
    void SetFormat(int rate, int channels) noexcept
    {
        rate_ = rate;
        channels_ = channels;
    }

private:
    int rate_ = 0;
    int channels_ = 0;
};

// The decoder of a file, by its first bytes: RIFF for WAV, fLaC for FLAC, OggS for Ogg Vorbis, any other for MP3;
// nullptr when that decoder cannot read the file, the sound has neither one channel nor two, or its rate is outside
// kMinRate to kMaxRate.
[[nodiscard]] std::unique_ptr<Decoder> OpenDecoder(std::span<const std::byte> file);

// The whole file decoded; nullopt when no decoder reads it, no sample comes out, it lasts more than ten minutes, or it
// holds more samples than ten minutes at 48,000 Hz in stereo. What a damaged file gives before its damage is kept.
[[nodiscard]] std::optional<Pcm> Decode(std::span<const std::byte> file);

} // namespace sound

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

// stream.h -- a sound streamed to the listener: one OpenAL Soft source, not positioned, fed through a queue of buffers
// that the frame fills again as they are played, from a decoder (the music) or from samples the engine gives (the
// cinematics).
// Example: the music's stream asks its decoder for 1,024 frames each time a buffer has played; the cinematics' asks
// the samples the engine queued.
#pragma once

#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <span>
#include <vector>

namespace sound {

class Stream {
public:
    // Fills its span with frames, interleaved by channel, and returns the frames written: 0 when it has none now.
    using Fill = std::function<std::size_t(std::span<std::int16_t>)>;

    Stream(int rate, int channels, int resampler);
    Stream(const Stream&) = delete;
    Stream& operator=(const Stream&) = delete;
    Stream(Stream&&) = delete;
    Stream& operator=(Stream&&) = delete;
    ~Stream();

    // At every frame: the buffers played filled again by fill and queued, the source started again if it ran dry.
    void Update(const Fill& fill);
    void SetGain(float gain);
    // Whether everything queued has been played and the last fill gave nothing.
    [[nodiscard]] bool Ended() const;
    // The frames played so far.
    [[nodiscard]] std::int64_t PlayedFrames() const;
    [[nodiscard]] int Rate() const noexcept { return rate_; }
    [[nodiscard]] int Channels() const noexcept { return channels_; }

private:
    int rate_ = 0;
    int channels_ = 0;
    std::uint32_t source_ = 0;
    std::vector<std::uint32_t> buffers_;
    std::vector<std::uint32_t> idle_; // the buffers not queued
    std::vector<std::int16_t> scratch_;
    std::deque<std::int64_t> queued_; // the frames of each queued buffer, in queue order
    std::int64_t processedFrames_ = 0;
    bool fillEmpty_ = false;    // the last fill gave nothing
    std::optional<float> gain_; // the gain given to the source, none yet
};

} // namespace sound

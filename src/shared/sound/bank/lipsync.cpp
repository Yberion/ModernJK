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

// lipsync.cpp -- a sound's loudness for the lip sync (lipsync.h). The measure is the one Jedi Academy's mixer made at
// 44,100 Hz: every 800 samples (about 18 ms), the mean of ten samples taken 100 apart, each divided by 256 and
// squared; the thresholds are Jedi Academy's, as fractions of the sound's peak.
#include "lipsync.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace sound {

namespace {

constexpr int kReferenceRate = 44100;
constexpr int kBlockAtReference = 800;
constexpr int kSpacingAtReference = 100;
constexpr int kSamplesPerBlock = 10;

// the loudness below each level, as a fraction of the peak: silent, then levels 1 to 3; level 4 above the last
constexpr std::array<float, 4> kThresholds = {0.3f, 4.0f, 6.0f, 8.0f};

// a sample's measure: its value divided by 256, its upper byte, the scale the thresholds were set on
constexpr int Upper(std::int16_t sample) noexcept
{
    return sample / 256;
}

} // namespace

// the peak over every sample, then each block's ten samples
Loudness MeasureLoudness(std::span<const std::int16_t> samples, int rate)
{
    Loudness loudness;
    loudness.frames = static_cast<std::int64_t>(samples.size());
    loudness.blockFrames = std::max(1, rate * kBlockAtReference / kReferenceRate);
    const std::size_t spacing = static_cast<std::size_t>(std::max(1, rate * kSpacingAtReference / kReferenceRate));

    for (const std::int16_t sample : samples) {
        loudness.peak = std::max(loudness.peak, std::abs(Upper(sample)));
    }

    const auto blockFrames = static_cast<std::size_t>(loudness.blockFrames);
    loudness.blocks.reserve((samples.size() / blockFrames) + 1);

    for (std::size_t start = 0; start < samples.size(); start += blockFrames) {
        int total = 0;
        int count = 0;

        for (std::size_t i = 0; i < kSamplesPerBlock && start + (i * spacing) < samples.size(); ++i) {
            const int value = Upper(samples[start + (i * spacing)]);
            total += value * value;
            ++count;
        }

        loudness.blocks.push_back(static_cast<std::uint16_t>(total / count));
    }

    return loudness;
}

// the block's loudness against the thresholds scaled by the peak
VoiceState LipSyncState(const Loudness& loudness, std::int64_t frame)
{
    if (frame < 0) {
        return VoiceState::Silent;
    }

    if (frame >= loudness.frames) {
        return VoiceState::NotTalking;
    }

    const auto block = static_cast<std::size_t>(frame / loudness.blockFrames);
    const auto value = static_cast<float>(loudness.blocks[block]);
    const auto peak = static_cast<float>(loudness.peak);

    // a block of zeros is silent even in a sound with no peak, where every threshold is 0
    if (value == 0.0f || value < peak * kThresholds[0]) {
        return VoiceState::Silent;
    }

    if (value < peak * kThresholds[1]) {
        return VoiceState::Level1;
    }

    if (value < peak * kThresholds[2]) {
        return VoiceState::Level2;
    }

    if (value < peak * kThresholds[3]) {
        return VoiceState::Level3;
    }

    return VoiceState::Level4;
}

} // namespace sound

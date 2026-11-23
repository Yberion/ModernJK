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

// lipsync.h -- what the lip sync reads of a sound: its loudness measured once, when it is registered, in blocks of
// about 18 ms, and its peak; a voice's state is its block's loudness against the peak.
// Example: a voice played 1.2 s ago at 22,050 Hz is at its frame 26,460, whose block's loudness against the voice's
// peak gives the face its level, from silent to 4.
#pragma once

#include <shared/sound/sound.h>

#include <cstdint>
#include <span>
#include <vector>

namespace sound {

struct Loudness {
    std::int64_t frames = 0;           // the sound's
    int peak = 0;                      // the highest |sample| / 256
    int blockFrames = 1;               // the frames of a block, at the sound's rate
    std::vector<std::uint16_t> blocks; // per block, the mean of ten samples' (sample / 256) squared
};

// Measures a mono sound's loudness, block by block.
[[nodiscard]] Loudness MeasureLoudness(std::span<const std::int16_t> samples, int rate);

// The voice's state at a frame of its sound: NotTalking past its end.
[[nodiscard]] VoiceState LipSyncState(const Loudness& loudness, std::int64_t frame);

} // namespace sound

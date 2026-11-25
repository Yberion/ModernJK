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

// sets.h -- the ambient sets of sound/sound.txt: a general set plays around the listener, a local set at an entity, a
// brush model's set gives a door's or a platform's sounds; each a looping sound and one-shots chosen at random.
// Example: "generalSet yavin_jungle" with "timeBetweenWaves 10 50" and seven creatures' calls in "subWaves" plays one
// every 10 to 50 s around the listener, over the jungle's loop.
#pragma once

#include <shared/sound/bank/bank.h>
#include <shared/sound/host.h>
#include <shared/sound/sound.h>

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace sound {

enum class SetKind : std::uint8_t {
    General,
    Local,
    BrushModel,
};

struct AmbientSet {
    std::string name; // lower case
    SetKind kind = SetKind::General;
    // a keyword not given keeps the original game's default
    int timeStart = 10;                // seconds between two one-shots, at least
    int timeEnd = 25;                  // and at most
    int volumeStart = 255;             // a one-shot's volume, from 0 to 255, at least
    int volumeEnd = 255;               // and at most
    std::vector<SoundHandle> subWaves; // the one-shots; a brush model's sounds by stage
    SoundHandle loopedWave = kNoSound;
    int radius = 250; // a local set's, in units
};

// The sets of sound/sound.txt named in wanted (lower case), their sounds registered; a set wanted and missing, or a
// keyword the file misspells, warned.
[[nodiscard]] std::vector<AmbientSet> ParseAmbientSets(std::string_view text, const std::set<std::string, std::less<>>& wanted, Bank& bank, const Host& host);

} // namespace sound

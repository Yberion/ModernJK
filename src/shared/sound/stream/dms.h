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

// dms.h -- a level's dynamic music, read from ext_data/dms.dat: its explore, action and boss pieces, each with its
// entry markers (the times it may start at) and its exits (the times it may leave at, and the transition piece played
// then), and the death piece every level has; and the rules of the moves between them.
// Example: yavin1b uses yavin1's entry: its explore piece is music/yavin1/swamp_explore, its action piece
// music/yavin1/swamp_action, whose exits lead through its transitions (music/yavin1/swamp_atr00...) back to explore.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sound {

// A piece of a level's dynamic music.
enum class Track : std::uint8_t {
    Explore,
    Action,
    Boss,
    Death,
    ActionTransition0, // from action to explore, by the action piece's exits
    ActionTransition1,
    ActionTransition2,
    ActionTransition3,
    ExploreTransition0, // from explore to silence, by the explore piece's exits
    ExploreTransition1,
    ExploreTransition2,
    ExploreTransition3,
};

// What the dynamic music plays: a piece, or silence.
enum class DynamicState : std::uint8_t {
    Explore,
    Action,
    Boss,
    Death,
    Transition,
    Silence,
};

inline constexpr int kTransitionsPerPiece = 4;

struct MusicPiece {
    struct Exit {
        std::string nextFile;
        std::string nextMark; // the explore piece's marker an action piece's transition leads to; empty for explore's
    };

    struct ExitTime {
        float time = 0.0f; // seconds
        std::size_t exit = 0;
    };

    std::string file;
    std::map<std::string, float, std::less<>> entries; // marker, seconds
    std::vector<Exit> exits;
    std::vector<ExitTime> exitTimes; // sorted by time
};

// A transition allowed at a moment of a piece: its track, and where the piece after it starts.
struct Transition {
    Track track = Track::ExploreTransition0;
    float entryTime = 0.0f;
};

struct DynamicMusic {
    std::string level;   // the level's name, as asked
    std::string set;     // the folder of its pieces, after its "uses"
    std::string bossSet; // the folder of its boss piece ("useboss")
    std::optional<MusicPiece> explore;
    std::optional<MusicPiece> action;
    std::optional<MusicPiece> boss;
};

// A level's dynamic music from the text of ext_data/dms.dat; nullopt when the file has none for it or is wrong, the
// reason in error.
[[nodiscard]] std::optional<DynamicMusic> ParseDynamicMusic(std::string_view text, std::string_view level, std::string& error);

// The file of a track without its extension, empty when the music has no such track.
[[nodiscard]] std::string TrackFile(const DynamicMusic& music, Track track);

// Whether a state may be left for another now: death and boss music once started are not, but for silence after boss,
// nor is a transition, except for action.
[[nodiscard]] bool CanInterrupt(DynamicState current, DynamicState proposed) noexcept;

// The transition a piece may take at a moment, within 0.3 s of one of its exit times.
[[nodiscard]] std::optional<Transition> AllowedTransition(const DynamicMusic& music, Track piece, float seconds);

// The piece's entry markers' times, in the order of their names.
[[nodiscard]] std::vector<float> EntryTimes(const DynamicMusic& music, Track piece);

} // namespace sound

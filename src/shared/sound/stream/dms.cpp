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

// dms.cpp -- a level's dynamic music (dms.h). The file is a tree of groups, "name { ... }", and of pairs, "key value"
// on one line, with "//" comments: "musicfiles" holds the pieces, "levelmusic" each level's explore, action and boss
// pieces, a level able to use another's ("uses") or another's boss piece ("useboss").
#include "dms.h"

#include <fmt/format.h>

// LLVM libc, as netcodec takes the library's math, independent of the engines' q_libm.h
#include <shared/libc_common.h>
#include <src/__support/FPUtil/BasicOperations.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <span>
#include <type_traits>
#include <utility>

namespace sound {

namespace {

// how close to an exit time a piece may leave: too wide and the change is sloppy, too narrow and a slow frame misses it
constexpr float kExitTolerance = 0.3f;

struct Group {
    std::string name;
    std::vector<std::pair<std::string, std::string>> pairs;
    std::vector<Group> groups;
};

struct Token {
    std::string_view text;
    int line = 0;
};

// two names equal whatever their case
bool SameName(std::string_view a, std::string_view b) noexcept
{
    return std::ranges::equal(a, b, [](char x, char y) { return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y)); });
}

// the text's words and braces with their lines, the comments left out
std::vector<Token> Tokenize(std::string_view text)
{
    std::vector<Token> tokens;
    int line = 1;
    std::size_t i = 0;

    while (i < text.size()) {
        const char c = text[i];

        if (c == '\n') {
            ++line;
            ++i;
        }
        else if (std::isspace(static_cast<unsigned char>(c)) != 0) {
            ++i;
        }
        else if (c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
            while (i < text.size() && text[i] != '\n') {
                ++i;
            }
        }
        else if (c == '{' || c == '}') {
            tokens.push_back({.text = text.substr(i, 1), .line = line});
            ++i;
        }
        else {
            const std::size_t start = i;

            while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i])) == 0 && text[i] != '{' && text[i] != '}') {
                ++i;
            }

            tokens.push_back({.text = text.substr(start, i - start), .line = line});
        }
    }

    return tokens;
}

// the groups' depth the file may reach: the game's is 4; a file nesting deeper is damaged or hostile (each level a
// recursion of the parser and of the groups' destructor)
constexpr int kMaxDepth = 32;

// a group's content up to its closing brace: a word followed by an opening brace starts a group, any other word is a
// pair's key, its value the next word on the same line; false when the groups nest beyond kMaxDepth
bool ParseGroup(std::span<const Token> tokens, std::size_t& i, Group& group, int depth)
{
    if (depth > kMaxDepth) {
        return false;
    }

    while (i < tokens.size()) {
        const Token& token = tokens[i++];

        if (token.text == "}") {
            return true;
        }

        if (token.text == "{") {
            continue;
        }

        if (i < tokens.size() && tokens[i].text == "{") {
            ++i;
            group.groups.emplace_back();
            group.groups.back().name = token.text;

            if (!ParseGroup(tokens, i, group.groups.back(), depth + 1)) {
                return false;
            }
        }
        else if (i < tokens.size() && tokens[i].line == token.line && tokens[i].text != "}") {
            group.pairs.emplace_back(token.text, tokens[i++].text);
        }
        else {
            group.pairs.emplace_back(token.text, "");
        }
    }

    return true;
}

// a group's subgroup of a name, whatever its case; nullptr for none
const Group* FindGroup(const Group& group, std::string_view name)
{
    const auto found = std::ranges::find_if(group.groups, [name](const Group& g) { return SameName(g.name, name); });
    return found != group.groups.end() ? &*found : nullptr;
}

// a group's value of a key, whatever its case; nullptr for none
const std::string* FindPair(const Group& group, std::string_view key)
{
    const auto found = std::ranges::find_if(group.pairs, [key](const auto& pair) { return SameName(pair.first, key); });
    return found != group.pairs.end() ? &found->second : nullptr;
}

// a number of seconds, 0 when the text holds none
float Seconds(std::string_view text) noexcept
{
    float value = 0.0f;
    std::from_chars(text.data(), text.data() + text.size(), value);
    return value;
}

// the piece of musicfiles named name: its entry markers, its exits and their times; nullopt, the reason in error, when
// it is missing or breaks the rules of its kind (explore and action need entries and exits, four exits at most; boss
// has no exit)
std::optional<MusicPiece> ParsePiece(const Group& files, std::string_view name, Track kind, std::string& error)
{
    const Group* group = FindGroup(files, name);

    if (group == nullptr) {
        error = fmt::format("no musicfiles entry \"{}\"", name);
        return std::nullopt;
    }

    MusicPiece piece;
    piece.file = name;

    for (const Group& entry : group->groups) {
        if (SameName(entry.name, "entry")) {
            for (const auto& [key, value] : entry.pairs) {
                piece.entries[key] = Seconds(value);
            }
        }
    }

    for (const Group& exit : group->groups) {
        if (!SameName(exit.name, "exit")) {
            continue;
        }

        MusicPiece::Exit point;

        for (const auto& [key, value] : exit.pairs) {
            if (SameName(key, "nextfile")) {
                point.nextFile = value;
            }
            else if (SameName(key, "nextmark")) {
                point.nextMark = value;
            }
            else if (key.size() >= 4 && SameName(std::string_view(key).substr(0, 4), "time")) {
                piece.exitTimes.push_back({.time = Seconds(value), .exit = piece.exits.size()});
            }
        }

        piece.exits.push_back(std::move(point));
    }

    if (kind == Track::Boss && !piece.exits.empty()) {
        error = fmt::format("\"{}\" has transitions, which boss music may not have", name);
        return std::nullopt;
    }

    if (kind != Track::Boss && (piece.entries.empty() || piece.exits.empty())) {
        error = fmt::format("\"{}\" needs its entry and exit groups", name);
        return std::nullopt;
    }

    if (piece.exits.size() > kTransitionsPerPiece) {
        error = fmt::format("\"{}\" has more than {} transitions", name, kTransitionsPerPiece);
        return std::nullopt;
    }

    std::ranges::sort(piece.exitTimes, {}, &MusicPiece::ExitTime::time);
    return piece;
}

// where a level's music keeps the piece of a kind: explore, action or boss
std::optional<MusicPiece>& PieceSlot(DynamicMusic& music, Track kind) noexcept
{
    if (kind == Track::Explore) {
        return music.explore;
    }

    return kind == Track::Action ? music.action : music.boss;
}

// the piece of a track, nullptr when the music has none
const MusicPiece* PieceOf(const DynamicMusic& music, Track piece) noexcept
{
    switch (piece) {
    case Track::Explore:
        return music.explore ? &*music.explore : nullptr;
    case Track::Action:
        return music.action ? &*music.action : nullptr;
    case Track::Boss:
        return music.boss ? &*music.boss : nullptr;
    default:
        return nullptr;
    }
}

} // namespace

// the level's entry in levelmusic, followed through its "uses" (ten at most), then its pieces read
std::optional<DynamicMusic> ParseDynamicMusic(std::string_view text, std::string_view level, std::string& error)
{
    const std::vector<Token> tokens = Tokenize(text);
    Group root;
    std::size_t i = 0;

    if (!ParseGroup(tokens, i, root, 0)) {
        error = fmt::format("groups nested deeper than {}", kMaxDepth);
        return std::nullopt;
    }

    const Group* files = FindGroup(root, "musicfiles");
    const Group* levels = FindGroup(root, "levelmusic");

    if (files == nullptr || levels == nullptr) {
        error = "no musicfiles or levelmusic group";
        return std::nullopt;
    }

    DynamicMusic music;
    music.level = level;
    const Group* entry = nullptr;
    std::string search(level);

    for (int uses = 0; !search.empty(); ++uses) {
        if (uses == 10) {
            error = fmt::format("\"{}\" uses itself in a loop", level);
            return std::nullopt;
        }

        music.set = search;
        music.bossSet = search;
        entry = FindGroup(*levels, search);

        if (entry == nullptr) {
            return std::nullopt;
        }

        const std::string* next = FindPair(*entry, "uses");
        search = next != nullptr ? *next : std::string();
    }

    // an empty level's name names no entry
    if (entry == nullptr) {
        return std::nullopt;
    }

    // a value "placeholder" stands for a piece not written
    const auto named = [entry](std::string_view key) -> std::string {
        const std::string* value = FindPair(*entry, key);
        return value != nullptr && !SameName(*value, "placeholder") ? *value : std::string();
    };

    std::string boss = named("boss");

    if (const std::string useBoss = named("useboss"); !useBoss.empty()) {
        const Group* other = FindGroup(*levels, useBoss);
        const std::string* otherBoss = other != nullptr ? FindPair(*other, "boss") : nullptr;

        if (otherBoss == nullptr) {
            error = fmt::format("'useboss' \"{}\" has no boss entry", useBoss);
            return std::nullopt;
        }

        boss = *otherBoss;
        music.bossSet = useBoss;
    }

    const std::array<std::pair<std::string, Track>, 3> pieces = {{{named("explore"), Track::Explore}, {named("action"), Track::Action}, {boss, Track::Boss}}};

    for (const auto& [name, kind] : pieces) {
        if (name.empty()) {
            continue;
        }

        std::optional<MusicPiece> piece = ParsePiece(*files, name, kind, error);

        if (!piece) {
            return std::nullopt;
        }

        PieceSlot(music, kind) = std::move(piece);
    }

    // an action piece's transition leads to a marker of the explore piece
    if (music.action) {
        for (const MusicPiece::Exit& exit : music.action->exits) {
            if (!exit.nextMark.empty() && (!music.explore || !music.explore->entries.contains(exit.nextMark))) {
                error = fmt::format("no entry point \"{}\" in the explore piece", exit.nextMark);
                return std::nullopt;
            }
        }
    }

    return music;
}

// music/<set>/<piece>, the boss piece from its own set, the death piece the same for every level
std::string TrackFile(const DynamicMusic& music, Track track)
{
    const auto transition = [&music](const std::optional<MusicPiece>& piece, int index) -> std::string {
        if (!piece || static_cast<std::size_t>(index) >= piece->exits.size()) {
            return {};
        }

        return fmt::format("music/{}/{}", music.set, piece->exits[static_cast<std::size_t>(index)].nextFile);
    };

    switch (track) {
    case Track::Explore:
        return music.explore ? fmt::format("music/{}/{}", music.set, music.explore->file) : std::string();
    case Track::Action:
        return music.action ? fmt::format("music/{}/{}", music.set, music.action->file) : std::string();
    case Track::Boss:
        return music.boss ? fmt::format("music/{}/{}", music.bossSet, music.boss->file) : std::string();
    case Track::Death:
        return "music/death_music";
    case Track::ActionTransition0:
    case Track::ActionTransition1:
    case Track::ActionTransition2:
    case Track::ActionTransition3:
        return transition(music.action, std::to_underlying(track) - std::to_underlying(Track::ActionTransition0));
    case Track::ExploreTransition0:
    case Track::ExploreTransition1:
    case Track::ExploreTransition2:
    case Track::ExploreTransition3:
        return transition(music.explore, std::to_underlying(track) - std::to_underlying(Track::ExploreTransition0));
    }

    return {};
}

bool CanInterrupt(DynamicState current, DynamicState proposed) noexcept
{
    if (proposed == DynamicState::Death) {
        return true;
    }

    if (current == DynamicState::Death) {
        return false;
    }

    if (proposed == DynamicState::Boss) {
        return true;
    }

    if (current == DynamicState::Boss) {
        return proposed == DynamicState::Silence;
    }

    if (proposed == DynamicState::Action) {
        return true;
    }

    return current != DynamicState::Transition;
}

// the first exit time within the tolerance; an explore piece's transition goes to silence, an action piece's to the
// explore piece at its exit's marker
std::optional<Transition> AllowedTransition(const DynamicMusic& music, Track piece, float seconds)
{
    const MusicPiece* current = PieceOf(music, piece);

    if (current == nullptr || (piece != Track::Explore && piece != Track::Action)) {
        return std::nullopt;
    }

    for (const MusicPiece::ExitTime& exitTime : current->exitTimes) {
        if (LIBC_NAMESPACE::fputil::abs(exitTime.time - seconds) > kExitTolerance) {
            continue;
        }

        const MusicPiece::Exit& exit = current->exits[exitTime.exit];
        const auto index = static_cast<std::underlying_type_t<Track>>(exitTime.exit);

        if (piece == Track::Explore) {
            return Transition{.track = static_cast<Track>(std::to_underlying(Track::ExploreTransition0) + index)};
        }

        Transition transition{.track = static_cast<Track>(std::to_underlying(Track::ActionTransition0) + index)};

        if (!exit.nextMark.empty() && music.explore) {
            transition.entryTime = music.explore->entries.find(exit.nextMark)->second;
        }

        return transition;
    }

    return std::nullopt;
}

std::vector<float> EntryTimes(const DynamicMusic& music, Track piece)
{
    std::vector<float> times;

    if (const MusicPiece* current = PieceOf(music, piece)) {
        for (const auto& entry : current->entries) {
            times.push_back(entry.second);
        }
    }

    return times;
}

} // namespace sound

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

// sets.cpp -- the ambient sets of sound/sound.txt (sets.h). The file is read line by line: a line "generalSet name",
// "localSet name" or "bmodelSet name" starts a set, the lines after it give its keywords, a comment (';') ends it; the
// header's "type" must be "ambientSet".
#include "sets.h"

#include <fmt/format.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <optional>
#include <tuple>
#include <utility>

namespace sound {

namespace {

// the one-shots of a set
constexpr std::size_t kMaxSubWaves = 8;

// the bounds of a set's numbers: an hour between two one-shots, the volume's 255, a radius across any map
constexpr int kMaxSeconds = 3600;
constexpr int kMaxVolume = 255;
constexpr int kMaxRadius = 65536;

// a text in lower case
std::string Lower(std::string_view text)
{
    std::string result(text);
    std::ranges::transform(result, result.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    return result;
}

// a line's words
std::vector<std::string_view> Words(std::string_view line)
{
    std::vector<std::string_view> words;
    std::size_t i = 0;

    while (i < line.size()) {
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])) != 0) {
            ++i;
        }

        const std::size_t start = i;

        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])) == 0) {
            ++i;
        }

        if (i > start) {
            words.push_back(line.substr(start, i - start));
        }
    }

    return words;
}

// the integer a word holds, 0 when none
int Integer(std::string_view word) noexcept
{
    int value = 0;
    std::from_chars(word.data(), word.data() + word.size(), value);
    return value;
}

// the kind of set a line's first word starts, nullopt for none
std::optional<SetKind> KindOf(std::string_view word)
{
    const std::string lower = Lower(word);

    if (lower == "generalset") {
        return SetKind::General;
    }

    if (lower == "localset") {
        return SetKind::Local;
    }

    if (lower == "bmodelset") {
        return SetKind::BrushModel;
    }

    return std::nullopt;
}

// a pair of integers in order, the file sometimes writing them swapped, each within its bounds
std::pair<int, int> Range(const std::vector<std::string_view>& words, int low, int high)
{
    const int a = std::clamp(words.size() > 1 ? Integer(words[1]) : 0, low, high);
    const int b = std::clamp(words.size() > 2 ? Integer(words[2]) : 0, low, high);
    return std::minmax(a, b);
}

// a keyword applied to its set; false for a keyword unknown or not allowed for the set's kind: the general and local
// sets take the times, the one-shots, the loop and the volumes, a local set its radius too, a brush model's set its
// sounds alone
bool ApplyKeyword(AmbientSet& set, const std::vector<std::string_view>& words, Bank& bank, const Host& host)
{
    const std::string keyword = Lower(words[0]);
    const bool timed = set.kind != SetKind::BrushModel;

    if (keyword == "subwaves" && words.size() > 1) {
        for (std::size_t i = 2; i < words.size(); ++i) {
            if (set.subWaves.size() == kMaxSubWaves) {
                host.print(Message::Debug, fmt::format("too many one-shots in the ambient set \"{}\"", set.name));
                break;
            }

            set.subWaves.push_back(bank.Register(fmt::format("sound/{}/{}", words[1], words[i])));
        }

        return true;
    }

    if (timed && keyword == "timebetweenwaves") {
        std::tie(set.timeStart, set.timeEnd) = Range(words, 0, kMaxSeconds);
        return true;
    }

    if (timed && keyword == "volrange") {
        std::tie(set.volumeStart, set.volumeEnd) = Range(words, 0, kMaxVolume);
        return true;
    }

    if (timed && keyword == "loopedwave" && words.size() > 1) {
        set.loopedWave = bank.Register(fmt::format("sound/{}", words[1]));
        return true;
    }

    if (set.kind == SetKind::Local && keyword == "radius" && words.size() > 1) {
        set.radius = std::clamp(Integer(words[1]), 0, kMaxRadius);
        return true;
    }

    return false;
}

} // namespace

std::vector<AmbientSet> ParseAmbientSets(std::string_view text, const std::set<std::string, std::less<>>& wanted, Bank& bank, const Host& host)
{
    std::vector<AmbientSet> sets;
    bool inSet = false;
    std::size_t start = 0;

    while (start < text.size()) {
        std::size_t end = text.find_first_of("\r\n", start); // a line ended by a newline, a carriage return or both

        if (end == std::string_view::npos) {
            end = text.size();
        }

        const std::vector<std::string_view> words = Words(text.substr(start, end - start));
        start = end + 1;

        if (words.empty()) {
            continue;
        }

        if (words[0].front() == ';') {
            inSet = false;
            continue;
        }

        if (const std::optional<SetKind> kind = KindOf(words[0])) {
            const std::string name = words.size() > 1 ? Lower(words[1]) : std::string();
            inSet = wanted.contains(name);

            if (inSet) {
                sets.emplace_back();
                sets.back().name = name;
                sets.back().kind = *kind;
            }

            continue;
        }

        if (Lower(words[0]) == "type") {
            if (words.size() < 2 || Lower(words[1]) != "ambientset") {
                host.print(Message::Error, "sound/sound.txt is not a file of ambient sets");
                return {};
            }

            continue;
        }

        if (inSet && !ApplyKeyword(sets.back(), words, bank, host)) {
            host.print(Message::Debug, fmt::format(R"(unknown keyword "{}" in the ambient set "{}")", words[0], sets.back().name));
            inSet = false;
        }
    }

    // a name given twice keeps the definition the original game kept: it read the general sets, then the local ones, then
    // the brush models', each replacing an earlier set of its name, so that the last of the latest kind wins (the game's
    // file has a local and a brush model's set "fanblade", the brush model's being the fan's)
    std::vector<AmbientSet> kept;

    for (AmbientSet& set : sets) {
        const auto same = std::ranges::find(kept, set.name, &AmbientSet::name);

        if (same == kept.end()) {
            kept.push_back(std::move(set));
        }
        else if (set.kind >= same->kind) {
            *same = std::move(set);
        }
    }

    for (const std::string& name : wanted) {
        if (std::ranges::none_of(kept, [&name](const AmbientSet& set) { return set.name == name; })) {
            host.print(Message::Warning, fmt::format("no ambient set \"{}\" in sound/sound.txt", name));
        }
    }

    return kept;
}

} // namespace sound

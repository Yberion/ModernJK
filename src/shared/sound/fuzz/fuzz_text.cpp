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

// fuzz_text.cpp -- libFuzzer target on the sound's three readers of text files, which come from the players' archives:
// each input read as ext_data/dms.dat for a few levels, every answer about the music it gives asked, then as
// sound/sound.txt for a few sets, then as a zone file (eagle/<map>.efxzones), the zone of a few points and the
// obstacles between them asked of what it reads. A starting corpus: the game's dms.dat and sound.txt, extracted from
// its archives, and zone files of assets/jka/eagle. Example:
//   sound_fuzz_text <work folder>
#include <shared/sound/ambient/sets.h>
#include <shared/sound/bank/bank.h>
#include <shared/sound/stream/dms.h>
#include <shared/sound/zones/zones.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <optional>
#include <vector>

namespace {

constexpr std::array kTracks = {sound::Track::Explore,
                                sound::Track::Action,
                                sound::Track::Boss,
                                sound::Track::Death,
                                sound::Track::ActionTransition0,
                                sound::Track::ActionTransition3,
                                sound::Track::ExploreTransition0,
                                sound::Track::ExploreTransition3};

// a host without files: every sound a set names is missing, which the bank warns about without OpenAL
sound::Host MakeHost()
{
    sound::Host host;
    host.readFile = [](std::string_view) -> std::optional<std::vector<std::byte>> { return std::nullopt; };
    host.fileExists = [](std::string_view) { return false; };
    host.print = [](sound::Message, std::string_view) {};
    host.random = [](int min, int) { return min; };
    return host;
}

} // namespace

// one input, as libFuzzer calls it
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    const std::string text(reinterpret_cast<const char*>(data), size);

    for (const char* level : {"lvl", "yavin1", "yavin1b", "hoth2", ""}) {
        std::string error;

        if (const auto music = sound::ParseDynamicMusic(text, level, error)) {
            for (const sound::Track track : kTracks) {
                (void)sound::TrackFile(*music, track);
                (void)sound::EntryTimes(*music, track);

                for (const float seconds : {0.0f, 0.1f, 3.0f, 69.3f, -1.0f, 1e30f}) {
                    (void)sound::AllowedTransition(*music, track, seconds);
                }
            }
        }
    }

    const sound::Host host = MakeHost();
    sound::Bank bank(host);
    const std::set<std::string, std::less<>> wanted = {"yavin_jungle", "kejim_console", "impdoor1", "a", ""};
    (void)sound::ParseAmbientSets(text, wanted, bank, host);

    std::string error;

    if (const std::optional<sound::ZoneFile> zones = sound::ParseZoneFile(text, error)) {
        constexpr std::array<sound::ZonePoint, 4> kPoints = {
            {{}, {.x = 128.0f, .y = 64.0f, .z = -256.0f}, {.x = -1000.0f, .y = 10.0f, .z = 2000.0f}, {.x = 40000.0f}}};

        for (const sound::ZonePoint& listener : kPoints) {
            const std::optional<int> zone = sound::ZoneOf(*zones, listener);

            for (const sound::ZonePoint& source : kPoints) {
                (void)sound::ObstaclesOf(*zones, zone, listener, source);
            }
        }
    }

    return 0;
}

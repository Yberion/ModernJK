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

// The tests of a blade's contacts with the world (BG_ContactSound in src/shared/qcommon/q_sound.h), on sounds counted by
// the test at frames of 1 ms, from the rule's constants: a 3 s scrape plays its sounds kScrapeSoundInterval apart,
// kScrapeSounds at once at most (with sounds slightly longer than that many intervals, the next one waiting for the
// first to end); a strike after the blade left the wall plays at once, but not within kContactSoundInterval of the
// blade's last sound.
#include "sound.h"

#include <check.h>

#include <shared/qcommon/q_sound.h>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace {

// the sounds the rule started: when each started, a voice's handle its index plus 1
struct Sounds {
    std::vector<int> starts;
    std::vector<float> volumes;
    int now = 0;
    int length = 1000; // milliseconds

    [[nodiscard]] bool IsPlaying(sound::VoiceHandle voice) const
    {
        const auto index = static_cast<std::size_t>(voice);
        return index != 0 && now < starts[index - 1] + length;
    }

    [[nodiscard]] std::ptrdiff_t Playing() const
    {
        return std::ranges::count_if(starts, [this](int start) { return now < start + length; });
    }

    // the blade touching the wall at this frame
    void Touch(contactSound_t& contact)
    {
        BG_ContactSound(
            contact, now, [this](sound::VoiceHandle voice) { return IsPlaying(voice); },
            [this](float volume) {
                starts.push_back(now);
                volumes.push_back(volume);
                return static_cast<sound::VoiceHandle>(starts.size());
            });
    }
};

} // namespace

void RunContactTests(const std::filesystem::path& /*data*/)
{
    // a 3 s scrape of sounds 19 ms longer than kScrapeSounds intervals: the first kScrapeSounds one interval apart, the
    // next one held until the first ends
    constexpr auto kAtOnce = static_cast<int>(kScrapeSounds);
    Sounds scrape;
    scrape.length = (kScrapeSoundInterval * kAtOnce) + 19;
    contactSound_t contact;
    std::ptrdiff_t most = 0;

    for (scrape.now = 0; scrape.now < 3000; ++scrape.now) {
        scrape.Touch(contact);
        most = std::max(most, scrape.Playing());
    }

    CHECK_EQUAL(most, std::ptrdiff_t{kAtOnce});
    CHECK(scrape.starts.size() > kScrapeSounds);

    if (scrape.starts.size() > kScrapeSounds) {
        CHECK_EQUAL(scrape.starts[kScrapeSounds - 1], kScrapeSoundInterval * (kAtOnce - 1));
        CHECK_EQUAL(scrape.starts[kScrapeSounds], scrape.length);
    }

    for (std::size_t i = 1; i < scrape.starts.size(); ++i) {
        CHECK(scrape.starts[i] - scrape.starts[i - 1] >= kScrapeSoundInterval);
        CHECK(scrape.volumes[i] == kScrapeSoundVolume);
    }

    CHECK(!scrape.volumes.empty() && scrape.volumes[0] == 1.0f);

    // a strike after a break: at once, beside the sounds still playing (1 s each); one 48 ms after the last sound waits
    // until kContactSoundInterval (100 ms)
    Sounds strikes;
    contactSound_t blade;
    strikes.Touch(blade);
    blade.touching = false;
    strikes.now = 200;
    strikes.Touch(blade);
    CHECK_EQUAL(strikes.starts.size(), 2U);
    blade.touching = false;
    strikes.now = 248;
    strikes.Touch(blade);
    CHECK_EQUAL(strikes.starts.size(), 2U);
    strikes.now = 304;
    strikes.Touch(blade);
    CHECK_EQUAL(strikes.starts.size(), 3U);
    CHECK(std::ranges::all_of(strikes.volumes, [](float volume) { return volume == 1.0f; }));
    CHECK_EQUAL(strikes.Playing(), std::ptrdiff_t{3});
}

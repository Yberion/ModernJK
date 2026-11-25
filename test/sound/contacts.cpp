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
// blade's last sound. The other rules of q_sound.h: a sound event packed and read back for every class, slot and loop, a
// field out of range read as a sound of the world without a slot; the channels' classes and slots (CHAN_MENU1 for a tree
// that has it); a voice's state as the faces' level.
#include "sound.h"

#include <check.h>

#include <shared/qcommon/q_sound.h>

#include <algorithm>
#include <cstddef>
#include <utility>
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

// the channels of a tree's soundChannel_t, the multiplayer's CHAN_MENU1 among them
enum class TestChannel {
    CHAN_AUTO,
    CHAN_LOCAL,
    CHAN_WEAPON,
    CHAN_VOICE,
    CHAN_VOICE_ATTEN,
    CHAN_ITEM,
    CHAN_BODY,
    CHAN_AMBIENT,
    CHAN_LOCAL_SOUND,
    CHAN_ANNOUNCER,
    CHAN_LESS_ATTEN,
    CHAN_MENU1,
    CHAN_VOICE_GLOBAL,
    CHAN_MUSIC
};

// the same without CHAN_MENU1, as the single players' have them
enum class SingleChannel {
    CHAN_AUTO,
    CHAN_LOCAL,
    CHAN_WEAPON,
    CHAN_VOICE,
    CHAN_VOICE_ATTEN,
    CHAN_ITEM,
    CHAN_BODY,
    CHAN_AMBIENT,
    CHAN_LOCAL_SOUND,
    CHAN_ANNOUNCER,
    CHAN_LESS_ATTEN,
    CHAN_VOICE_GLOBAL,
    CHAN_MUSIC
};

bool Uses(soundUse_t use, sound::SoundClass soundClass, sound::Slot slot)
{
    return use.soundClass == soundClass && use.slot == slot;
}

void TestEvents()
{
    // every class, slot and loop packed into the event's field and read back as they were, within its eight bits
    for (int c = 0; c <= std::to_underlying(sound::SoundClass::Ambient); ++c) {
        for (int s = 0; s <= std::to_underlying(sound::Slot::Menu); ++s) {
            for (const bool loop : {false, true}) {
                const soundEvent_t event{.soundClass = static_cast<sound::SoundClass>(c), .slot = static_cast<sound::Slot>(s), .loop = loop};
                const int packed = BG_PackSoundEvent(event);
                const soundEvent_t back = BG_UnpackSoundEvent(packed);
                CHECK(packed >= 0 && packed < 256);
                CHECK(back.soundClass == event.soundClass && back.slot == event.slot && back.loop == event.loop);
            }
        }
    }

    // a class and a slot beyond the last ones (a damaged or hostile field): a sound of the world without a slot
    const soundEvent_t odd = BG_UnpackSoundEvent(0xF | (0x7 << SOUND_EVENT_SLOT_SHIFT));
    CHECK(odd.soundClass == sound::SoundClass::World && odd.slot == sound::Slot::None && !odd.loop);

    // the channels the game's data names
    CHECK(Uses(BG_SoundUseOfChannel(TestChannel::CHAN_WEAPON), sound::SoundClass::Weapon, sound::Slot::Weapon));
    CHECK(Uses(BG_SoundUseOfChannel(TestChannel::CHAN_VOICE), sound::SoundClass::Voice, sound::Slot::Voice));
    CHECK(Uses(BG_SoundUseOfChannel(TestChannel::CHAN_VOICE_ATTEN), sound::SoundClass::VoiceQuiet, sound::Slot::Voice));
    CHECK(Uses(BG_SoundUseOfChannel(TestChannel::CHAN_VOICE_GLOBAL), sound::SoundClass::VoiceEverywhere, sound::Slot::Voice));
    CHECK(Uses(BG_SoundUseOfChannel(TestChannel::CHAN_ITEM), sound::SoundClass::World, sound::Slot::Item));
    CHECK(Uses(BG_SoundUseOfChannel(TestChannel::CHAN_BODY), sound::SoundClass::Body, sound::Slot::None));
    CHECK(Uses(BG_SoundUseOfChannel(TestChannel::CHAN_LOCAL), sound::SoundClass::Interface, sound::Slot::Menu));
    CHECK(Uses(BG_SoundUseOfChannel(TestChannel::CHAN_LOCAL_SOUND), sound::SoundClass::Interface, sound::Slot::Menu));
    CHECK(Uses(BG_SoundUseOfChannel(TestChannel::CHAN_MENU1), sound::SoundClass::Interface, sound::Slot::Menu));
    CHECK(Uses(BG_SoundUseOfChannel(TestChannel::CHAN_ANNOUNCER), sound::SoundClass::Announcer, sound::Slot::Announcer));
    CHECK(Uses(BG_SoundUseOfChannel(TestChannel::CHAN_LESS_ATTEN), sound::SoundClass::Vehicle, sound::Slot::None));
    CHECK(Uses(BG_SoundUseOfChannel(TestChannel::CHAN_AMBIENT), sound::SoundClass::Ambient, sound::Slot::None));
    CHECK(Uses(BG_SoundUseOfChannel(TestChannel::CHAN_AUTO), sound::SoundClass::World, sound::Slot::None));
    CHECK(Uses(BG_SoundUseOfChannel(TestChannel::CHAN_MUSIC), sound::SoundClass::World, sound::Slot::None));
    CHECK(Uses(BG_SoundUseOfChannel(SingleChannel::CHAN_VOICE), sound::SoundClass::Voice, sound::Slot::Voice));
    CHECK(Uses(BG_SoundUseOfChannel(SingleChannel::CHAN_LOCAL_SOUND), sound::SoundClass::Interface, sound::Slot::Menu));

    // an effect's sound: a shot's a weapon sound at the shots' volume, another a sound of the world as recorded
    static_assert(BG_EffectSound(true).soundClass == sound::SoundClass::Weapon && BG_EffectSound(true).volume == kShotVolume);
    static_assert(BG_EffectSound(false).soundClass == sound::SoundClass::World && BG_EffectSound(false).volume == 1.0f);

    // a voice's state as the faces' level: 0 not talking, -1 in a silence, then 1 to 4
    static_assert(BG_VoiceLevel(sound::VoiceState::NotTalking) == 0 && BG_VoiceLevel(sound::VoiceState::Silent) == -1);
    static_assert(BG_VoiceLevel(sound::VoiceState::Level1) == 1 && BG_VoiceLevel(sound::VoiceState::Level4) == 4);
}

} // namespace

void RunContactTests(const std::filesystem::path& /*data*/)
{
    TestEvents();

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

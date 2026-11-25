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

// The tests of src/shared/sound/bank and of the names of the sounds' files: a name keyed without its case and its
// extension, the best format found first, a stereo file mixed to mono, a missing file warned once, a voice looked for in
// the player's language first, a sound's length, the sounds a level did not use freed and loaded again; the lip sync's
// loudness on a signal made for it.
#include "sound.h"

#include <check.h>

#include <shared/sound/bank/bank.h>
#include <shared/sound/bank/lipsync.h>
#include <shared/sound/decoder/decoder.h>
#include <shared/sound/device/device.h>
#include <shared/sound/names.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace {

// the names as the sound keys them
void TestNames()
{
    CHECK(sound::NormalizeSoundName("Sound\\Weapons\\Fire.WAV") == "sound/weapons/fire");
    CHECK(sound::NormalizeSoundName("sound/a.b/c") == "sound/a.b/c");
    CHECK(sound::NormalizeSoundName("music/x.mp3") == "music/x");
}

// the bank on the loopback device's context
void TestRegister(const std::filesystem::path& data)
{
    test::Host host(data);
    const std::unique_ptr<sound::Device> device = sound::Device::OpenLoopback(host.host, 48000);
    CHECK(device != nullptr);

    if (device == nullptr) {
        return;
    }

    sound::Bank bank(host.host);

    // FLAC before WAV: the first path asked is the FLAC file, which exists
    host.asked.clear();
    const sound::SoundHandle stereo = bank.Register("sound/x/stereo44k_s16.wav");
    CHECK(stereo != sound::kNoSound);
    CHECK(host.asked.size() == 1 && host.asked.front() == "sound/x/stereo44k_s16.flac");

    // mixed to mono: the stereo file's frames, at its rate
    const auto pcm = sound::Decode(test::ReadFile(data / "stereo44k_s16.wav"));
    const sound::Sound* loaded = bank.Find(stereo);
    CHECK(loaded != nullptr && pcm.has_value());

    if (loaded != nullptr && pcm) {
        CHECK_EQUAL(loaded->rate, 44100);
        CHECK_EQUAL(static_cast<std::size_t>(loaded->frames), pcm->samples.size() / 2);
    }

    // one handle per name, whatever its case and extension
    CHECK_EQUAL(bank.Register("SOUND/x/Stereo44k_s16.mp3"), stereo);

    // a missing file: kNoSound and one developer's message, its name not looked for again
    const std::size_t debugs = host.Count(sound::Message::Debug);
    CHECK_EQUAL(bank.Register("sound/missing"), sound::kNoSound);
    CHECK_EQUAL(host.Count(sound::Message::Debug), debugs + 1);
    host.asked.clear();
    CHECK_EQUAL(bank.Register("sound/missing"), sound::kNoSound);
    CHECK(host.asked.empty());
    CHECK_EQUAL(host.Count(sound::Message::Debug), debugs + 1);
    CHECK_EQUAL(bank.Register("*pain"), sound::kNoSound);

    // a voice in German looked for in sound/chr_d/ first
    bank.SetLanguage(sound::Language::German);
    host.asked.clear();
    CHECK(bank.Register("sound/chars/mono22k_s16") != sound::kNoSound);
    CHECK(!host.asked.empty() && host.asked.front().starts_with("sound/chr_d/"));
    bank.SetLanguage(sound::Language::English);

    // the length, from the frames and the rate
    const sound::SoundHandle mono = bank.Register("sound/y/mono22k_s16");
    const auto monoPcm = sound::Decode(test::ReadFile(data / "mono22k_s16.wav"));
    CHECK(monoPcm.has_value() && bank.Length(mono) == static_cast<int>(monoPcm->samples.size() * 1000 / 22050));
    CHECK_EQUAL(bank.Length(sound::kNoSound), 0);

    // a new level: what it does not use is freed, its length kept, loaded again when played
    bank.BeginLevel();
    CHECK_EQUAL(bank.Register("sound/y/mono22k_s16"), mono);
    const std::vector<sound::SoundHandle> unused = bank.Unused();
    CHECK(std::ranges::find(unused, stereo) != unused.end());
    CHECK(std::ranges::find(unused, mono) == unused.end());
    bank.Free(unused);
    CHECK(bank.Find(stereo) == nullptr);
    CHECK(bank.Length(stereo) > 0);

    // saved for a restart: the same handles, the loaded sound loaded again, the freed one restored with its length and
    // its file read only when a level uses it
    {
        const std::vector<sound::Bank::Saved> saved = bank.Save();
        sound::Bank restarted(host.host);

        for (const sound::Bank::Saved& sound : saved) {
            (void)restarted.Restore(sound);
        }

        CHECK(restarted.Find(mono) != nullptr && restarted.Find(stereo) == nullptr && restarted.Length(stereo) == bank.Length(stereo));
        host.asked.clear();
        CHECK(restarted.Use(stereo) != nullptr && !host.asked.empty());
    }

    CHECK(bank.Use(stereo) != nullptr);

    // a name restored after a restart whose file is gone: its handle all the same, its file looked for once per level
    const sound::SoundHandle restored = bank.Restore({.name = "sound/gone/track.v2"});
    CHECK(restored != sound::kNoSound);
    CHECK_EQUAL(bank.Name(restored), "sound/gone/track.v2");
    host.asked.clear();
    CHECK(bank.Use(restored) == nullptr);
    CHECK(bank.Use(restored) == nullptr);
    CHECK(host.asked.empty());
    bank.BeginLevel();
    CHECK(bank.Use(restored) == nullptr);
    CHECK(!host.asked.empty());

    // the variants of a sound one event: the names that differ only by a trailing number
    const sound::SoundHandle hit1 = bank.Restore({.name = "sound/weapons/saber/saberhitwall1"});
    const sound::SoundHandle hit3 = bank.Restore({.name = "sound/weapons/saber/saberhitwall3"});
    const sound::SoundHandle hum = bank.Restore({.name = "sound/weapons/saber/saberhum1"});
    CHECK_EQUAL(bank.Event(hit1), bank.Event(hit3));
    CHECK(bank.Event(hit1) != bank.Event(hum));
    CHECK(bank.Event(hit1) < bank.EventCount() && bank.Event(hum) < bank.EventCount());
}

// a signal of silence, then loud, then quiet: its states silent, level 4, level 1, then not talking past its end
void TestLipSync()
{
    constexpr int kRate = 44100;
    constexpr std::size_t kPart = kRate / 10;
    std::vector<std::int16_t> samples(3 * kPart, 0);

    for (std::size_t i = kPart; i < 2 * kPart; ++i) {
        samples[i] = (i % 2 == 0) ? 20000 : -20000;
    }

    for (std::size_t i = 2 * kPart; i < 3 * kPart; ++i) {
        samples[i] = (i % 2 == 0) ? 2000 : -2000;
    }

    const sound::Loudness loudness = sound::MeasureLoudness(samples, kRate);
    CHECK_EQUAL(loudness.peak, 78);
    CHECK(sound::LipSyncState(loudness, 100) == sound::VoiceState::Silent);
    CHECK(sound::LipSyncState(loudness, kPart + 100) == sound::VoiceState::Level4);
    CHECK(sound::LipSyncState(loudness, 2 * kPart + 1000) == sound::VoiceState::Level1);
    CHECK(sound::LipSyncState(loudness, 3 * kPart) == sound::VoiceState::NotTalking);

    // a silent sound has no peak: silent all the same
    const std::vector<std::int16_t> silence(kPart, 0);
    CHECK(sound::LipSyncState(sound::MeasureLoudness(silence, kRate), 0) == sound::VoiceState::Silent);
}

} // namespace

void RunBankTests(const std::filesystem::path& data)
{
    TestNames();
    TestRegister(data);
    TestLipSync();
}

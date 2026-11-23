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

// The tests of src/shared/sound/voices, through sound::System on the loopback device: a sound played to its end, four
// voices of one sound mixed at most, 64 voices mixed of more, a sound out of range virtual then mixed when the listener
// comes, its position having advanced, a slot replacing its sound, a sound started twice at once played as one voice,
// the voice's state for the lip sync, the stops, the mute without the focus, the master volume over the voices, the
// filter under water, and the sound log.
#include "sound.h"

#include <check.h>

#include <shared/sound/system.h>

#include <AL/al.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr int kRate = 48000;
constexpr int kStep = 10; // milliseconds

// a system on the loopback device, its clock and its output
struct Run {
    explicit Run(const std::filesystem::path& data) : host(data), system(sound::System::Create(host.host, kRate)) {}

    // the frames of the time given, mixed by steps, the system's frame before each step
    void Advance(int milliseconds)
    {
        for (int elapsed = 0; elapsed < milliseconds; elapsed += kStep) {
            now += kStep;
            system->Frame(now);
            system->GetDevice().Render(out);
        }
    }

    // silent but for OpenAL Soft's dither of its 16-bit output, a step or two
    [[nodiscard]] bool Silent() const
    {
        return std::ranges::all_of(out, [](std::int16_t s) { return s >= -4 && s <= 4; });
    }

    test::Host host;
    std::unique_ptr<sound::System> system;
    int now = 0;
    std::vector<std::int16_t> out = std::vector<std::int16_t>(static_cast<std::size_t>(kRate * kStep / 1000) * 2);
};

// a request of a sound at a point
sound::Request At(sound::SoundHandle sound, sound::SoundClass soundClass, int entity, float x)
{
    sound::Request request;
    request.sound = sound;
    request.soundClass = soundClass;
    request.place = sound::Place::Point;
    request.entity = entity;
    request.origin = {x, 0.0f, 0.0f};
    return request;
}

// the listener at a point, looking along x
sound::Listener ListenerAt(float x, int entity = 0)
{
    sound::Listener listener;
    listener.entity = entity;
    listener.origin = {x, 0.0f, 0.0f};
    listener.axes = {{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
    return listener;
}

void TestPlay(const std::filesystem::path& data)
{
    Run run(data);
    CHECK(run.system != nullptr);

    if (run.system == nullptr) {
        return;
    }

    sound::System& system = *run.system;
    system.SetListener(ListenerAt(0.0f));
    const sound::SoundHandle tone = system.Register("sound/a/mono22k_s16");
    CHECK(tone != sound::kNoSound);

    // a sound at the listener played to its end (0.25 s)
    sound::Request local;
    local.sound = tone;
    local.soundClass = sound::SoundClass::Interface;
    local.place = sound::Place::Listener;
    system.Play(local);
    run.Advance(kStep);
    CHECK_EQUAL(system.Mixed(), 1U);
    CHECK(!run.Silent());
    run.Advance(400);
    CHECK_EQUAL(system.Playing(), 0U);

    // six voices of one sound: four mixed
    for (int i = 0; i < 6; ++i) {
        system.Play(At(tone, sound::SoundClass::World, 100 + i, 10.0f * static_cast<float>(i + 1)));
    }

    run.Advance(kStep);
    CHECK_EQUAL(system.Playing(), 6U);
    CHECK_EQUAL(system.Mixed(), 4U);
    system.StopAll();

    // twenty sounds four times, looping: 64 mixed of 80
    for (int s = 0; s < 20; ++s) {
        const sound::SoundHandle handle = system.Register("sound/s" + std::to_string(s) + "/mono22k_s16");

        for (int i = 0; i < 4; ++i) {
            sound::Request request = At(handle, sound::SoundClass::World, 200 + s * 4 + i, 10.0f + static_cast<float>(i));
            request.loop = true;
            system.Play(request);
        }
    }

    run.Advance(kStep);
    CHECK_EQUAL(system.Playing(), 80U);
    CHECK_EQUAL(system.Mixed(), 64U);
    system.StopAll();
    CHECK_EQUAL(system.Playing(), 0U);

    // out of range: virtual, then mixed when the listener comes, ending at its own time all the same
    system.Play(At(tone, sound::SoundClass::Weapon, 300, 5000.0f));
    run.Advance(kStep);
    CHECK_EQUAL(system.Playing(), 1U);
    CHECK_EQUAL(system.Mixed(), 0U);
    system.SetListener(ListenerAt(4900.0f));
    run.Advance(kStep);
    CHECK_EQUAL(system.Mixed(), 1U);
    run.Advance(300);
    CHECK_EQUAL(system.Playing(), 0U);
    system.SetListener(ListenerAt(0.0f));

    // a slot: the entity's new voice replaces its last; another entity's beside it
    sound::Request voice = At(tone, sound::SoundClass::Voice, 5, 50.0f);
    voice.slot = sound::Slot::Voice;
    system.Play(voice);
    system.Play(voice);
    run.Advance(kStep);
    CHECK_EQUAL(system.Playing(), 1U);
    voice.entity = 6;
    system.Play(voice);
    CHECK_EQUAL(system.Playing(), 2U);
    CHECK(system.GetVoiceState(5) != sound::VoiceState::NotTalking);
    CHECK(system.GetVoiceState(7) == sound::VoiceState::NotTalking);
    run.Advance(400);
    CHECK(system.GetVoiceState(5) == sound::VoiceState::NotTalking);

    // twice at one instant and place: one voice; again at the next frame: two
    const sound::Request shot = At(tone, sound::SoundClass::Weapon, 8, 20.0f);
    system.Play(shot);
    system.Play(shot);
    CHECK_EQUAL(system.Playing(), 1U);
    run.Advance(kStep);
    system.Play(shot);
    CHECK_EQUAL(system.Playing(), 2U);
    system.StopEntity(8);
    CHECK_EQUAL(system.Playing(), 0U);

    // a loop played until it is stopped
    sound::Request loop = At(tone, sound::SoundClass::World, 9, 30.0f);
    loop.loop = true;
    const sound::VoiceHandle handle = system.Play(loop);
    run.Advance(1000);
    CHECK_EQUAL(system.Playing(), 1U);
    system.Stop(handle);
    CHECK_EQUAL(system.Playing(), 0U);

    // muted without the focus; the master volume at 0 silences the voices too; under water, the filter
    sound::Request talk = At(tone, sound::SoundClass::Voice, 10, 0.0f);
    talk.loop = true;
    system.Play(talk);
    run.Advance(50);
    CHECK(!run.Silent());
    system.SetMuteWhenUnfocused(true);
    system.SetFocus(false);
    run.Advance(50);
    CHECK(run.Silent());
    system.SetFocus(true);
    run.Advance(50);
    CHECK(!run.Silent());
    system.SetVolumes(0.0f, 1.0f, 1.0f);
    run.Advance(50);
    CHECK(run.Silent());
    system.SetVolumes(1.0f, 1.0f, 1.0f);
    sound::Listener underwater = ListenerAt(0.0f);
    underwater.underwater = true;
    system.SetListener(underwater);
    run.Advance(50);
    CHECK(!run.Silent());
    CHECK_EQUAL(alGetError(), AL_NO_ERROR);
    system.StopAll();

    // the log: one line per sound started
    std::vector<std::string> lines;
    system.SetLog([&lines](std::string_view line) { lines.emplace_back(line); });
    system.Play(shot);
    system.SetLog({});
    CHECK(lines.size() == 1 && lines.front().starts_with("SNDLOG ") && lines.front().find(" start ") != std::string::npos);
}

// the highest sample of the output
int Peak(const std::vector<std::int16_t>& out)
{
    int peak = 0;

    for (const std::int16_t sample : out) {
        peak = std::max(peak, sample < 0 ? -static_cast<int>(sample) : static_cast<int>(sample));
    }

    return peak;
}

// a looping sound's handle after it stopped finds nothing, even once its entry is reused; the listener's own sound at
// 0.75 of the same sound at the listener; a voice before a crowd of lower classes; the device lost then back, the loops
// surviving; a level's end freeing a sound that plays; the clock going back
void TestRules(const std::filesystem::path& data)
{
    Run run(data);

    if (run.system == nullptr) {
        return;
    }

    sound::System& system = *run.system;
    system.SetListener(ListenerAt(0.0f, 7));
    const sound::SoundHandle tone = system.Register("sound/a/mono22k_s16");

    // a stale handle
    sound::Request loop = At(tone, sound::SoundClass::World, 20, 10.0f);
    loop.loop = true;
    const sound::VoiceHandle old = system.Play(loop);
    system.Stop(old);
    const sound::VoiceHandle reused = system.Play(loop);
    CHECK(reused != old);
    CHECK(!system.IsPlaying(old));
    system.Stop(old);
    system.MoveLoop(old, {500.0f, 0.0f, 0.0f}, {});
    CHECK(system.IsPlaying(reused));
    system.StopAll();

    // the listener's own sound: centred, at 0.75 of its volume
    sound::Request atListener;
    atListener.sound = tone;
    atListener.soundClass = sound::SoundClass::Weapon;
    atListener.place = sound::Place::Listener;
    atListener.loop = true;
    system.Play(atListener);
    run.Advance(100);
    const int full = Peak(run.out);
    system.StopAll();
    system.SetListener(ListenerAt(0.0f, 7));
    sound::Request own = At(tone, sound::SoundClass::Weapon, 7, 300.0f);
    own.loop = true;
    system.Play(own);
    run.Advance(100);
    const int scaled = Peak(run.out);
    CHECK(full > 1000 && scaled > 0);
    CHECK(test::CloseFraction(static_cast<double>(scaled) / static_cast<double>(full), 0.75, 0.06));
    system.StopAll();
    system.SetListener(ListenerAt(0.0f, 7));

    // 72 body loops of 18 sounds, then a voice: the voice is mixed, the crowd keeps 63
    for (int s = 0; s < 18; ++s) {
        const sound::SoundHandle handle = system.Register("sound/b" + std::to_string(s) + "/mono22k_s16");

        for (int i = 0; i < 4; ++i) {
            sound::Request body = At(handle, sound::SoundClass::Body, 100 + (s * 4) + i, 5.0f);
            body.loop = true;
            system.Play(body);
        }
    }

    sound::Request voice = At(tone, sound::SoundClass::Voice, 300, 900.0f);
    voice.loop = true;
    const sound::VoiceHandle talking = system.Play(voice);
    run.Advance(20);
    CHECK(system.IsReal(talking));
    CHECK_EQUAL(system.Mixed(), 64U);

    // the device lost: nothing mixed, every voice kept; back: mixed again
    system.GetDevice().SimulateLoss(true);
    run.Advance(50);
    CHECK_EQUAL(system.Mixed(), 0U);
    CHECK_EQUAL(system.Playing(), 73U);
    system.GetDevice().SimulateLoss(false);
    run.Advance(20);
    CHECK_EQUAL(system.Mixed(), 64U);
    CHECK(system.IsReal(talking));
    system.StopAll();

    // a level's end freeing a sound that plays: its voice stopped first
    const sound::SoundHandle other = system.Register("sound/c/mono11k_u8");
    sound::Request leftover = At(other, sound::SoundClass::World, 30, 10.0f);
    leftover.loop = true;
    const sound::VoiceHandle playing = system.Play(leftover);
    run.Advance(20);
    system.BeginRegistration();
    static_cast<void>(system.Register("sound/a/mono22k_s16"));
    system.EndRegistration();
    CHECK(!system.IsPlaying(playing));
    run.Advance(20);

    // the clock going back: the voices go on
    const sound::VoiceHandle again = system.Play(loop);
    run.Advance(20);
    run.now -= 1000;
    run.Advance(20);
    CHECK(system.IsPlaying(again));
    system.StopAll();

    // a sound to follow an entity needs one
    sound::Request nobody;
    nobody.sound = tone;
    nobody.place = sound::Place::Entity;
    CHECK(system.Play(nobody) == sound::VoiceHandle::None);
}

// the sound restarted: the same handles for the same names, and no handle of the sound before found in the new one
void TestRestart(const std::filesystem::path& data)
{
    test::Host host(data);
    std::unique_ptr<sound::System> first = sound::System::Create(host.host, kRate);

    if (first == nullptr) {
        return;
    }

    const sound::SoundHandle a = first->Register("sound/a/mono22k_s16");
    const sound::SoundHandle b = first->Register("sound/b/stereo44k_s16");
    sound::Request loop = At(b, sound::SoundClass::World, 3, 10.0f);
    loop.loop = true;
    const sound::VoiceHandle kept = first->Play(loop);
    const sound::System::State state = first->SaveState();
    first.reset();
    const std::unique_ptr<sound::System> second = sound::System::Create(host.host, kRate, state.serial);
    second->RestoreState(state);
    CHECK_EQUAL(second->Register("sound/a/mono22k_s16"), a);
    CHECK_EQUAL(second->Register("sound/b/stereo44k_s16"), b);
    const sound::VoiceHandle fresh = second->Play(loop);
    CHECK(fresh != kept);
    CHECK(!second->IsPlaying(kept));
    CHECK(second->IsPlaying(fresh));
}

} // namespace

void RunVoicesTests(const std::filesystem::path& data)
{
    TestPlay(data);
    TestRules(data);
    TestRestart(data);
}

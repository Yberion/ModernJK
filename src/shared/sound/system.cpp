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

// system.cpp -- the sound as an engine runs it (system.h): the device opened first, so that the other parts create
// their sources and buffers in its context; the frame's order (the ambient sets and the music start their sounds, then
// the voices choose and update, then the device is followed); the volumes, the focus, the cinematics' stream. Example:
// an engine calls Frame(now) once per frame after its game modules, and SetFocus when its window gains or loses the
// focus.
#include "system.h"

#include <shared/sound/decoder/decoder.h>
#include <shared/sound/names.h>

#include <fmt/format.h>

#include <algorithm>
#include <utility>

namespace sound {

std::unique_ptr<System> System::Create(const Host& host, std::optional<int> loopbackRate, std::uint32_t firstSerial)
{
    std::unique_ptr<System> system(new System(host, loopbackRate, firstSerial));
    return system->device_ ? std::move(system) : nullptr;
}

// the device opened first, so that every part after it creates its sources and buffers in its context
System::System(Host host, std::optional<int> loopbackRate, std::uint32_t firstSerial)
    : host_(std::move(host)), device_(loopbackRate ? Device::OpenLoopback(host_, *loopbackRate) : Device::OpenSystem(host_)), bank_(host_),
      voices_(bank_, reverb_, Resampler(), firstSerial), music_(host_, Resampler()), ambient_(bank_, voices_, host_)
{
}

// the parts that hold sources and buffers released before the device closes
System::~System()
{
    StopAll();
}

void System::SetListener(const Listener& listener)
{
    listener_ = listener.origin;
    voices_.SetListener(listener);
    reverb_.SetListener(listener);
}

// the map's zone file read and given to the reverb; a damaged one reported and left out, the map then without reverb
void System::LoadZones(std::string_view map)
{
    zonesMap_ = map;
    std::optional<ZoneFile> zones;

    if (!map.empty()) {
        const std::string path = fmt::format("eagle/{}.efxzones", map);
        const std::optional<std::vector<std::byte>> bytes = host_.readFile(path);

        if (bytes) {
            std::string error;
            zones = ParseZoneFile(ToText(*bytes), error);

            if (!zones) {
                host_.print(Message::Warning, fmt::format("{}: {}; no reverb on this map", path, error));
            }
        }
    }

    reverb_.Load(std::move(zones));
}

// the general set plays at the listener, wherever the game gives its origin
void System::UpdateAmbientSet(std::string_view name, const Vector& /*origin*/)
{
    ambient_.UpdateGeneral(name);
}

// the device followed first; the ambient sets and the music, whose sounds the voices then start. While the device is
// lost, every voice is virtual and the streams wait; once it is back, the voices are mixed again where they have got to.
void System::Frame(int now)
{
    const DeviceState state = device_->Frame(now);

    if (state == DeviceState::Reopened) {
        voices_.Recover();
    }

    const bool mixing = state != DeviceState::Lost;
    voices_.SetListenerBoost(device_->ListenerBoost());
    ambient_.Frame(now, listener_);

    if (mixing) {
        music_.Frame(now);
        FeedStream();
        reverb_.Frame(now);
    }

    voices_.Frame(now, mixing);
}

// the cinematics' stream fed from the samples queued
void System::FeedStream()
{
    if (!stream_) {
        return;
    }

    stream_->Update([this](std::span<std::int16_t> out) -> std::size_t {
        const auto channels = static_cast<std::size_t>(stream_->Channels());
        const std::size_t frames = std::min(out.size(), streamQueue_.size()) / channels;
        const std::size_t samples = frames * channels;
        std::copy_n(streamQueue_.begin(), samples, out.begin());
        streamQueue_.erase(streamQueue_.begin(), streamQueue_.begin() + static_cast<std::ptrdiff_t>(samples));
        return frames;
    });
}

void System::EndRegistration()
{
    const std::vector<SoundHandle> unused = bank_.Unused();
    voices_.StopSounds(unused);
    bank_.Free(unused);
}

void System::StopSounds()
{
    ambient_.StopAll();
    voices_.StopAll();
}

void System::StopAll()
{
    StopSounds();
    music_.Stop();
    CloseStream();
}

void System::SetFocus(bool focused)
{
    focused_ = focused;
    device_->SetMuted(muteWhenUnfocused_ && !focused_);
}

void System::SetMuteWhenUnfocused(bool mute)
{
    muteWhenUnfocused_ = mute;
    device_->SetMuted(muteWhenUnfocused_ && !focused_);
}

void System::SetVolumes(float effects, float voices, float music)
{
    effectsVolume_ = effects;
    voices_.SetVolumes(effects, voices);
    music_.SetVolume(music);

    if (stream_) {
        stream_->SetGain(effects);
    }
}

// a new stream when the format changes, the samples of the old one dropped; a format the sound does not play (no rate,
// more than two channels) closes the stream
void System::OpenStream(int rate, int channels)
{
    if (rate < kMinRate || rate > kMaxRate || channels < 1 || channels > 2) {
        CloseStream();
        return;
    }

    if (stream_ && stream_->Rate() == rate && stream_->Channels() == channels) {
        return;
    }

    streamQueue_.clear();
    stream_ = std::make_unique<Stream>(rate, channels, Resampler());
    stream_->SetGain(effectsVolume_);
}

// the samples queued; beyond half a second waiting, the oldest dropped, so that a cinematic's sound does not stay late
// after a long frame
void System::QueueStream(std::span<const std::int16_t> samples)
{
    if (!stream_) {
        return;
    }

    streamQueue_.insert(streamQueue_.end(), samples.begin(), samples.end());
    const std::size_t limit = static_cast<std::size_t>(stream_->Rate() / 2) * static_cast<std::size_t>(stream_->Channels());

    if (streamQueue_.size() > limit) {
        streamQueue_.erase(streamQueue_.begin(), streamQueue_.begin() + static_cast<std::ptrdiff_t>(streamQueue_.size() - limit));
    }
}

void System::CloseStream()
{
    stream_.reset();
    streamQueue_.clear();
}

System::State System::SaveState() const
{
    State state;
    state.sounds = bank_.Names();
    state.sets.assign(ambient_.Precached().begin(), ambient_.Precached().end());
    state.intro = music_.LastIntro();
    state.loop = music_.LastLoop();
    state.level = music_.Level();
    state.zones = zonesMap_;
    state.serial = voices_.NextSerial();
    return state;
}

// the names registered again in their order, so that every handle the modules hold finds its sound; a name whose file
// has since gone takes its handle all the same, kept by the bank as a sound that does not load
void System::RestoreState(const State& state)
{
    for (const std::string& name : state.sounds) {
        bank_.Restore(name);
    }

    for (const std::string& set : state.sets) {
        ambient_.Precache(set);
    }

    if (!state.sets.empty()) {
        ambient_.Parse();
    }

    music_.SetLevel(state.level);
    LoadZones(state.zones);

    if (!state.intro.empty() || !state.loop.empty() || !state.level.empty()) {
        music_.Start(state.intro, state.loop);
    }
}

std::string System::Info() const
{
    return fmt::format("device: {}\nvoices: {} playing, {} mixed of {}\nsounds registered: {}\n{}\n{}", device_->Name(), voices_.Playing(), voices_.Mixed(),
                       kRealVoices, bank_.Count() - 1, music_.Describe(), reverb_.Info());
}

std::vector<std::string> System::List() const
{
    std::vector<std::string> lines;

    for (SoundHandle handle = 1; handle < bank_.Count(); ++handle) {
        lines.push_back(bank_.Describe(handle));
    }

    return lines;
}

} // namespace sound

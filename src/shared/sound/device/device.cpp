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

// device.cpp -- OpenAL Soft's device and context (device.h): the chosen output or the default device opened with HRTF
// off, its context made current with the straight-line distance model, the speed of sound in game units and bsinc24; the
// system's events of a new default device and of an output that appears, which come on OpenAL Soft's own thread, only
// mark the device, and the next frame reopens it in place. Example: headphones plugged in while playing become the
// system's default device; the sound moves to them at the next frame, every sound going on.
#include "device.h"

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>
#include <utility>

namespace sound {

namespace {

// HRTF off: the game's sounds are mixed for speakers and headphones alike, without a head-related filter; OpenAL Soft's
// limiter on, which it leaves off for a device mixing in floating point (the usual format of a Windows device), so that
// a crowd's peaks are lowered to 2 dB under full scale (its threshold, patched) instead of clipping, their true peak
// under ASWG-R001's -1 dBTP.
constexpr std::array<ALCint, 5> kSystemAttributes = {ALC_HRTF_SOFT, ALC_FALSE, ALC_OUTPUT_LIMITER_SOFT, ALC_TRUE, 0};

// the events that move the sound to another device: the system's default changed, an output appeared (the chosen one
// plugged back); the device in use disappearing is seen at every frame, by ALC_CONNECTED. WASAPI, PipeWire, PulseAudio
// and CoreAudio report them, ALSA does not.
constexpr std::array<ALCenum, 2> kEvents = {ALC_EVENT_TYPE_DEFAULT_DEVICE_CHANGED_SOFT, ALC_EVENT_TYPE_DEVICE_ADDED_SOFT};

// the index of bsinc24, the band-limited sinc of 24 points, which the library names "23rd order Sinc"; 0, the
// library's default, when it has none
int FindBsinc24()
{
    const ALint count = alGetInteger(AL_NUM_RESAMPLERS_SOFT);

    for (ALint i = 0; i < count; ++i) {
        const ALchar* name = alGetStringiSOFT(AL_RESAMPLER_NAME_SOFT, i);

        if (name != nullptr && std::string_view(name) == "23rd order Sinc") {
            return i;
        }
    }

    return 0;
}

// a device's name as the system gives it: OpenAL Soft names a Windows device "OpenAL Soft on <the system's name>" and
// opens it by either name
std::string SystemName(std::string_view name)
{
    constexpr std::string_view prefix = "OpenAL Soft on ";

    if (name.starts_with(prefix)) {
        name.remove_prefix(prefix.size());
    }

    return std::string(name);
}

// called by OpenAL Soft on its own thread: a change of the default playback device, or a playback device that appears,
// marks the device, which the next frame looks at
void ALC_APIENTRY OnEvent(ALCenum eventType, ALCenum deviceType, ALCdevice* /*device*/, ALCsizei /*length*/, const ALCchar* /*message*/,
                          void* userParam) noexcept
{
    if (deviceType != ALC_PLAYBACK_DEVICE_SOFT) {
        return;
    }

    auto* events = static_cast<DeviceEvents*>(userParam);

    if (eventType == ALC_EVENT_TYPE_DEFAULT_DEVICE_CHANGED_SOFT) {
        events->defaultChanged.store(true);
    }
    else if (eventType == ALC_EVENT_TYPE_DEVICE_ADDED_SOFT) {
        events->added.store(true);
    }
}

} // namespace

DeviceMove DecideMove(const DeviceSeen& seen, const std::function<bool()>& chosenPresent)
{
    const bool playing = seen.connected && !seen.lost;
    const bool wanted = seen.reselect || !playing || (seen.defaultChanged && seen.onDefault) || (seen.added && seen.hasChoice && !seen.onChosen);

    if (!wanted) {
        return DeviceMove::Stay;
    }

    if (seen.hasChoice && chosenPresent()) {
        return seen.onChosen && playing ? DeviceMove::Stay : DeviceMove::ToChosen;
    }

    // the chosen output not there, or none chosen: the default, unless it plays already and has not changed
    if (seen.onDefault && playing && !seen.defaultChanged) {
        return DeviceMove::Stay;
    }

    return DeviceMove::ToDefault;
}

// measured on OpenAL Soft 1.25.2's loopback device, HRTF off (test/sound/device.cpp measures them again): a mono
// source at the listener and one fully to the right, the loudest channel of each; stereo 0.596 against 1.0
float ListenerBoostOf(int outputMode) noexcept
{
    switch (outputMode) {
    case ALC_STEREO_BASIC_SOFT:
        return 1.678f;
    case ALC_STEREO_UHJ_SOFT:
        return 1.502f;
    case ALC_SURROUND_5_1_SOFT:
        return 1.490f;
    case ALC_SURROUND_6_1_SOFT:
        return 1.400f;
    case ALC_SURROUND_7_1_SOFT:
        return 1.256f;
    default: // mono, quad: the centre as loud as the side
        return 1.0f;
    }
}

// opens the chosen output, or the default device (the null name) when none is chosen or it does not open, then its
// context
std::unique_ptr<Device> Device::OpenSystem(const Host& host, std::string_view chosen)
{
    const std::string name(chosen);
    ALCdevice* device = name.empty() ? nullptr : alcOpenDevice(name.c_str());
    const bool onChosen = device != nullptr;

    if (!name.empty() && !onChosen) {
        host.print(Message::Info, fmt::format("the sound device \"{}\" is not there: the system's default until it is", name));
    }

    if (device == nullptr) {
        device = alcOpenDevice(nullptr);
    }

    if (device == nullptr) {
        host.print(Message::Error, "no sound device could be opened");
        return nullptr;
    }

    std::unique_ptr<Device> result(new Device(host, device, false));
    result->chosen_ = name;
    result->openedAs_ = onChosen ? name : std::string();
    result->onChosen_ = onChosen;

    if (!result->CreateContext(kSystemAttributes)) {
        return nullptr;
    }

    // the events reported on OpenAL Soft's thread, acted on at the next frame
    alcEventCallbackSOFT(OnEvent, &result->events_);
    alcEventControlSOFT(static_cast<ALCsizei>(kEvents.size()), kEvents.data(), ALC_TRUE);
    host.print(Message::Info, fmt::format("sound device: {}", result->Name()));
    return result;
}

// the outputs' names, a list of strings each ended by a null, the last by two
std::vector<std::string> Device::OutputNames()
{
    std::vector<std::string> names;
    const ALCchar* name = alcGetString(nullptr, ALC_ALL_DEVICES_SPECIFIER);

    while (name != nullptr && *name != '\0') {
        names.push_back(SystemName(name));
        name += std::strlen(name) + 1;
    }

    return names;
}

// opens the loopback device with the format Render writes
std::unique_ptr<Device> Device::OpenLoopback(const Host& host, int rate)
{
    ALCdevice* device = alcLoopbackOpenDeviceSOFT(nullptr);

    if (device == nullptr) {
        host.print(Message::Error, "OpenAL Soft's loopback device could not be opened");
        return nullptr;
    }

    std::unique_ptr<Device> result(new Device(host, device, true));
    const std::array<ALCint, 9> attributes = {
        ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT, ALC_FORMAT_TYPE_SOFT, ALC_SHORT_SOFT, ALC_FREQUENCY, rate, ALC_HRTF_SOFT, ALC_FALSE, 0};

    if (!result->CreateContext(attributes)) {
        return nullptr;
    }

    return result;
}

Device::Device(const Host& host, ALCdevice* device, bool loopback) : host_(host), device_(device), loopback_(loopback) {}

// the context removed before the device closes, and the events' callback with it
Device::~Device()
{
    if (!loopback_) {
        alcEventControlSOFT(static_cast<ALCsizei>(kEvents.size()), kEvents.data(), ALC_FALSE);
        alcEventCallbackSOFT(nullptr, nullptr);
    }

    if (context_ != nullptr) {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context_);
    }

    alcCloseDevice(device_);
}

// the context made current, and what is set once for every source: distances in the game's units with a straight-line
// attenuation per source between its two distances, Doppler at the speed of sound, the resampler
bool Device::CreateContext(std::span<const int> attributes)
{
    context_ = alcCreateContext(device_, attributes.data());

    if (context_ == nullptr || alcMakeContextCurrent(context_) == ALC_FALSE) {
        host_.print(Message::Error, "the sound device's context could not be created");
        return false;
    }

    alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
    alSpeedOfSound(kSpeedOfSound);
    alDopplerFactor(1.0f);
    resampler_ = FindBsinc24();
    ApplyGain();
    ReadOutputMode();
    return true;
}

// the boost of the listener's sounds for the output mode the device plays in now
void Device::ReadOutputMode()
{
    ALCint mode = 0;
    alcGetIntegerv(device_, ALC_OUTPUT_MODE_SOFT, 1, &mode);
    listenerBoost_ = ListenerBoostOf(mode);
}

// the listener's gain: the mix's (kMixGain), or silence while muted
void Device::ApplyGain() const
{
    alListenerf(AL_GAIN, muted_ ? 0.0f : kMixGain);
}

// reopens the device on the output DecideMove gives, once per second while none opens, warned once per loss; the
// context, the sources and the buffers stay as they are, the sources possibly stopped by OpenAL Soft (the voices start
// them again)
DeviceState Device::Frame(int now)
{
    if (loopback_) {
        const bool wasLost = lost_;
        lost_ = simulatedLoss_;

        if (lost_) {
            return DeviceState::Lost;
        }

        return wasLost ? DeviceState::Reopened : DeviceState::Playing;
    }

    ALCint connected = ALC_TRUE;
    alcGetIntegerv(device_, ALC_CONNECTED, 1, &connected);
    DeviceSeen seen;
    seen.reselect = std::exchange(reselect_, false);
    seen.connected = connected == ALC_TRUE;
    seen.lost = lost_;
    seen.defaultChanged = events_.defaultChanged.exchange(false);
    seen.added = events_.added.exchange(false);

    // nothing happened: the device in use goes on, nothing else asked
    if (!seen.reselect && seen.connected && !seen.lost && !seen.defaultChanged && !seen.added) {
        return DeviceState::Playing;
    }

    seen.hasChoice = !chosen_.empty();
    seen.onChosen = onChosen_;
    seen.onDefault = openedAs_.empty();

    // a lost device is tried again once per second, or at once when the system reports a change
    if (lost_ && !seen.reselect && !seen.defaultChanged && !seen.added && now - nextAttempt_ < 0) {
        return DeviceState::Lost;
    }

    const DeviceMove move = DecideMove(seen, [this] {
        const std::vector<std::string> names = OutputNames();
        return std::ranges::find(names, chosen_) != names.end();
    });

    if (move == DeviceMove::Stay) {
        return lost_ ? DeviceState::Lost : DeviceState::Playing;
    }

    // a reopened device's sources may all have been stopped (OpenAL Soft clears its voices on a reopen as on a loss):
    // the voices are mixed again whatever moved the device
    const auto reopen = [this](bool toChosen) {
        if (alcReopenDeviceSOFT(device_, toChosen ? chosen_.c_str() : nullptr, kSystemAttributes.data()) != ALC_TRUE) {
            return false;
        }

        host_.print(Message::Info, fmt::format("sound device: {}", Name()));
        ReadOutputMode();
        lost_ = false;
        openedAs_ = toChosen ? chosen_ : std::string();
        onChosen_ = toChosen;
        return true;
    };

    // the chosen output listed but refusing to open: the default in its place
    const bool toChosen = move == DeviceMove::ToChosen;

    if (reopen(toChosen) || (toChosen && reopen(false))) {
        return DeviceState::Reopened;
    }

    if (!lost_) {
        host_.print(Message::Warning, "the sound device is gone and no other opens; trying again every second");
    }

    lost_ = true;
    nextAttempt_ = now + 1000;
    return DeviceState::Lost;
}

// the names compared here, when the choice changes, not at every frame
void Device::SetChosen(std::string_view name)
{
    chosen_ = name;
    onChosen_ = !chosen_.empty() && openedAs_ == chosen_;
    reselect_ = true;
}

// asked at every frame: the listener's gain sent only when it changes
void Device::SetMuted(bool muted)
{
    if (muted == muted_) {
        return;
    }

    muted_ = muted;
    ApplyGain();
}

// the loopback device's next frames, two samples per frame
void Device::Render(std::span<std::int16_t> out)
{
    if (loopback_) {
        alcRenderSamplesSOFT(device_, out.data(), static_cast<ALCsizei>(out.size() / 2));
    }
}

// the name the system gives the device now playing
std::string Device::Name() const
{
    const ALCchar* name = alcGetString(device_, ALC_ALL_DEVICES_SPECIFIER);

    if (name == nullptr) {
        name = alcGetString(device_, ALC_DEVICE_SPECIFIER);
    }

    return name != nullptr ? SystemName(name) : std::string();
}

} // namespace sound

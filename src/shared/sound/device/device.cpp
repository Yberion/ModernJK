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

// device.cpp -- OpenAL Soft's device and context (device.h): the default device opened with HRTF off, its context made
// current with the straight-line distance model, the speed of sound in game units and bsinc24; the system's event of a
// new default device, which comes on OpenAL Soft's own thread, only marks the device, and the next frame reopens it in
// place. Example: headphones plugged in while playing become the system's default device; the sound moves to them at
// the next frame, every sound going on.
#include "device.h"

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>

#include <fmt/format.h>

#include <array>
#include <string_view>

namespace sound {

namespace {

// HRTF off: the game's sounds are mixed for speakers and headphones alike, without a head-related filter; OpenAL Soft's
// limiter on, which it leaves off for a device mixing in floating point (the usual format of a Windows device), so that
// a crowd's peaks are lowered to full scale instead of clipping.
constexpr std::array<ALCint, 5> kSystemAttributes = {ALC_HRTF_SOFT, ALC_FALSE, ALC_OUTPUT_LIMITER_SOFT, ALC_TRUE, 0};

// the event that moves the sound to another device: the system's default changed (the device in use disappearing is
// seen at every frame, by ALC_CONNECTED)
constexpr std::array<ALCenum, 1> kEvents = {ALC_EVENT_TYPE_DEFAULT_DEVICE_CHANGED_SOFT};

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

// called by OpenAL Soft on its own thread: a change of the default playback device marks the device to reopen at the
// next frame
void ALC_APIENTRY OnEvent(ALCenum eventType, ALCenum deviceType, ALCdevice* /*device*/, ALCsizei /*length*/, const ALCchar* /*message*/,
                          void* userParam) noexcept
{
    if (deviceType == ALC_PLAYBACK_DEVICE_SOFT && eventType == ALC_EVENT_TYPE_DEFAULT_DEVICE_CHANGED_SOFT) {
        static_cast<std::atomic<bool>*>(userParam)->store(true);
    }
}

} // namespace

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

// opens the default device (the null name), then its context
std::unique_ptr<Device> Device::OpenSystem(const Host& host)
{
    ALCdevice* device = alcOpenDevice(nullptr);

    if (device == nullptr) {
        host.print(Message::Error, "no sound device could be opened");
        return nullptr;
    }

    std::unique_ptr<Device> result(new Device(host, device, false));

    if (!result->CreateContext(kSystemAttributes)) {
        return nullptr;
    }

    // the default device followed: the events reported on OpenAL Soft's thread, acted on at the next frame
    alcEventCallbackSOFT(OnEvent, &result->defaultChanged_);
    alcEventControlSOFT(static_cast<ALCsizei>(kEvents.size()), kEvents.data(), ALC_TRUE);
    host.print(Message::Info, fmt::format("sound device: {}", result->Name()));
    return result;
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

// reopens the device on the system's default when the default changed or the device in use is gone, once per second
// while none opens, warned once per loss; the context, the sources and the buffers stay as they are, the sources
// possibly stopped by OpenAL Soft (the voices start them again)
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
    const bool changed = defaultChanged_.exchange(false);

    if (!changed && connected == ALC_TRUE && !lost_) {
        return DeviceState::Playing;
    }

    if (lost_ && !changed && now - nextAttempt_ < 0) {
        return DeviceState::Lost;
    }

    // a reopened device's sources may all have been stopped (OpenAL Soft clears its voices on a reopen as on a loss):
    // the voices are mixed again whatever moved the device
    if (alcReopenDeviceSOFT(device_, nullptr, kSystemAttributes.data()) == ALC_TRUE) {
        host_.print(Message::Info, fmt::format("sound device: {}", Name()));
        ReadOutputMode();
        lost_ = false;
        return DeviceState::Reopened;
    }

    if (!lost_) {
        host_.print(Message::Warning, "the sound device is gone and no other opens; trying again every second");
    }

    lost_ = true;
    nextAttempt_ = now + 1000;
    return DeviceState::Lost;
}

void Device::SetMuted(bool muted)
{
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

    return name != nullptr ? std::string(name) : std::string();
}

} // namespace sound

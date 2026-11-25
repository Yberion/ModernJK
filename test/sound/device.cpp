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

// The tests of src/shared/sound/device: OpenAL Soft as the sound uses it, on its loopback device (rendered into memory,
// no sound card needed): HRTF off, the resampler bsinc24, the extensions the sound needs (the default device followed
// and reopened, EFX's EAX reverb), a decoded sound played to its end; then sound::Device on the loopback device, its
// resampler found and its mute silencing the output; the boost of the sounds at the listener for every output mode.
#include "sound.h"

#include <check.h>

#include <shared/sound/decoder/decoder.h>
#include <shared/sound/device/device.h>

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace {

constexpr ALCint kRate = 48000;

// the index of bsinc24, the band-limited sinc of 24 points, which the library names "23rd order Sinc"; -1 if none
ALint Bsinc24()
{
    const ALint count = alGetInteger(AL_NUM_RESAMPLERS_SOFT);

    for (ALint i = 0; i < count; ++i) {
        if (std::string_view(alGetStringiSOFT(AL_RESAMPLER_NAME_SOFT, i)) == "23rd order Sinc") {
            return i;
        }
    }

    return -1;
}

// a decoded sound played through a source, rendered half way then past its end
void Play(const std::filesystem::path& data, ALCdevice* device)
{
    const auto pcm = sound::Decode(test::ReadFile(data / "mono22k_s16.wav"));
    CHECK(pcm.has_value());

    if (!pcm) {
        return;
    }

    ALuint buffer = 0;
    ALuint source = 0;
    alGenBuffers(1, &buffer);
    alBufferData(buffer, AL_FORMAT_MONO16, pcm->samples.data(), static_cast<ALsizei>(pcm->samples.size() * sizeof(std::int16_t)), pcm->rate);
    alGenSources(1, &source);
    alSourcei(source, AL_BUFFER, static_cast<ALint>(buffer));
    const ALint resampler = Bsinc24();
    CHECK(resampler >= 0);
    alSourcei(source, AL_SOURCE_RESAMPLER_SOFT, resampler);
    alSourcePlay(source);

    // half the sound: the source plays, and its offset is where the lip sync reads the voice's samples
    std::vector<std::int16_t> out(static_cast<std::size_t>(kRate / 8) * 2);
    alcRenderSamplesSOFT(device, out.data(), kRate / 8);
    ALint state = 0;
    ALint offset = 0;
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    alGetSourcei(source, AL_SAMPLE_OFFSET, &offset);
    CHECK_EQUAL(state, AL_PLAYING);
    CHECK(offset > 0 && static_cast<std::size_t>(offset) < pcm->samples.size());
    CHECK(std::ranges::any_of(out, [](std::int16_t s) { return s != 0; }));

    // past its end (0.25 s): stopped
    alcRenderSamplesSOFT(device, out.data(), kRate / 8);
    alcRenderSamplesSOFT(device, out.data(), kRate / 8);
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    CHECK_EQUAL(state, AL_STOPPED);

    alDeleteSources(1, &source);
    alDeleteBuffers(1, &buffer);
}

// an EAX reverb, the one the zones' environments will use, set in an effect slot
void Reverb()
{
    ALuint effect = 0;
    ALuint slot = 0;
    alGenEffects(1, &effect);
    alEffecti(effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
    CHECK_EQUAL(alGetError(), AL_NO_ERROR);
    alGenAuxiliaryEffectSlots(1, &slot);
    alAuxiliaryEffectSloti(slot, AL_EFFECTSLOT_EFFECT, static_cast<ALint>(effect));
    CHECK_EQUAL(alGetError(), AL_NO_ERROR);
    alDeleteAuxiliaryEffectSlots(1, &slot);
    alDeleteEffects(1, &effect);
}

// sound::Device on the loopback device: bsinc24 found, a tone heard, nothing while muted, the tone again after
void TestDevice(const std::filesystem::path& data)
{
    test::Host host(data);
    const std::unique_ptr<sound::Device> device = sound::Device::OpenLoopback(host.host, kRate);
    CHECK(device != nullptr);

    if (device == nullptr) {
        return;
    }

    CHECK(device->Resampler() > 0);
    const auto pcm = sound::Decode(test::ReadFile(data / "mono22k_s16.wav"));
    CHECK(pcm.has_value());

    if (!pcm) {
        return;
    }

    ALuint buffer = 0;
    ALuint source = 0;
    alGenBuffers(1, &buffer);
    alBufferData(buffer, AL_FORMAT_MONO16, pcm->samples.data(), static_cast<ALsizei>(pcm->samples.size() * sizeof(std::int16_t)), pcm->rate);
    alGenSources(1, &source);
    alSourcei(source, AL_BUFFER, static_cast<ALint>(buffer));
    alSourcei(source, AL_LOOPING, AL_TRUE);
    alSourcePlay(source);
    std::vector<std::int16_t> out(static_cast<std::size_t>(kRate / 20) * 2);
    // silent but for OpenAL Soft's dither of its 16-bit output, a step or two
    const auto silent = [&out] { return std::ranges::all_of(out, [](std::int16_t s) { return s >= -4 && s <= 4; }); };
    device->Render(out);
    CHECK(!silent());
    device->SetMuted(true);
    device->Render(out);
    device->Render(out);
    CHECK(silent());
    device->SetMuted(false);
    device->Render(out);
    device->Render(out);
    CHECK(!silent());
    alDeleteSources(1, &source);
    alDeleteBuffers(1, &buffer);
    CHECK_EQUAL(alGetError(), AL_NO_ERROR);
}

// the loudest channel's level of a 1 kHz mono tone rendered on a loopback device, at the listener or fully to the right
float Loudest(ALCdevice* device, int channels, bool right)
{
    std::vector<std::int16_t> pcm(static_cast<std::size_t>(kRate));

    for (std::size_t i = 0; i < pcm.size(); ++i) {
        pcm[i] = static_cast<std::int16_t>(16384.0 * std::sin(2.0 * 3.14159265358979 * 1000.0 * static_cast<double>(i) / kRate));
    }

    ALuint buffer = 0;
    ALuint source = 0;
    alGenBuffers(1, &buffer);
    alBufferData(buffer, AL_FORMAT_MONO16, pcm.data(), static_cast<ALsizei>(pcm.size() * sizeof(std::int16_t)), kRate);
    alGenSources(1, &source);
    alSourcei(source, AL_BUFFER, static_cast<ALint>(buffer));
    alSourcei(source, AL_LOOPING, AL_TRUE);
    alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);
    alSource3f(source, AL_POSITION, right ? 1.0f : 0.0f, 0.0f, 0.0f);
    alSourcePlay(source);
    std::vector<float> out(static_cast<std::size_t>(channels) * (kRate / 10));
    alcRenderSamplesSOFT(device, out.data(), kRate / 10); // the resampler's start
    alcRenderSamplesSOFT(device, out.data(), kRate / 10);
    double loudest = 0.0;

    for (int c = 0; c < channels; ++c) {
        double sum = 0.0;

        for (std::size_t i = static_cast<std::size_t>(c); i < out.size(); i += static_cast<std::size_t>(channels)) {
            sum += static_cast<double>(out[i]) * out[i];
        }

        loudest = std::max(loudest, sum);
    }

    alDeleteSources(1, &source);
    alDeleteBuffers(1, &buffer);
    return static_cast<float>(std::sqrt(loudest));
}

// the listener's boost of every output mode the table lists, measured again: the side's loudest channel over the
// centre's; and sound::Device's on its loopback device, stereo
void TestListenerBoost(const std::filesystem::path& data)
{
    struct Mode {
        ALCint format;
        int channels;
        ALCint mode; // 0: the format's own
    };

    for (const Mode& mode : {Mode{ALC_STEREO_SOFT, 2, ALC_STEREO_BASIC_SOFT}, Mode{ALC_STEREO_SOFT, 2, ALC_STEREO_UHJ_SOFT}, Mode{ALC_QUAD_SOFT, 4, 0},
                             Mode{ALC_5POINT1_SOFT, 6, 0}, Mode{ALC_6POINT1_SOFT, 7, 0}, Mode{ALC_7POINT1_SOFT, 8, 0}}) {
        ALCdevice* device = alcLoopbackOpenDeviceSOFT(nullptr);
        std::vector<ALCint> attributes = {
            ALC_FORMAT_CHANNELS_SOFT, mode.format, ALC_FORMAT_TYPE_SOFT, ALC_FLOAT_SOFT, ALC_FREQUENCY, kRate, ALC_HRTF_SOFT, ALC_FALSE,
            ALC_OUTPUT_LIMITER_SOFT,  ALC_FALSE};

        if (mode.mode != 0) {
            attributes.insert(attributes.end(), {ALC_OUTPUT_MODE_SOFT, mode.mode});
        }

        attributes.push_back(0);
        ALCcontext* context = alcCreateContext(device, attributes.data());
        CHECK(context != nullptr && alcMakeContextCurrent(context));
        ALCint reported = 0;
        alcGetIntegerv(device, ALC_OUTPUT_MODE_SOFT, 1, &reported);
        const float measured = Loudest(device, mode.channels, true) / Loudest(device, mode.channels, false);
        CHECK(std::abs(measured - sound::ListenerBoostOf(reported)) < 0.01f);
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context);
        alcCloseDevice(device);
    }

    test::Host host(data);
    const std::unique_ptr<sound::Device> device = sound::Device::OpenLoopback(host.host, kRate);
    CHECK(device != nullptr && device->ListenerBoost() == sound::ListenerBoostOf(ALC_STEREO_BASIC_SOFT));
}

// the output a frame moves to, in each case the menu and the system make; the outputs listed only when the move depends
// on whether the chosen one is there
void TestDecideMove()
{
    struct Case {
        sound::DeviceSeen seen;
        bool chosenPresent;
        sound::DeviceMove move;
        bool listed;
    };

    using sound::DeviceMove;
    const Case cases[] = {
        // the default followed: nothing happens; the default changes; another output appears
        {{}, false, DeviceMove::Stay, false},
        {{.defaultChanged = true}, false, DeviceMove::ToDefault, false},
        {{.added = true}, false, DeviceMove::Stay, false},
        // the chosen output playing: the default changes, another output appears
        {{.defaultChanged = true, .hasChoice = true, .onChosen = true, .onDefault = false}, true, DeviceMove::Stay, false},
        {{.added = true, .hasChoice = true, .onChosen = true, .onDefault = false}, true, DeviceMove::Stay, false},
        // the chosen output unplugged: the default; plugged back: the chosen one; another output appears meanwhile
        {{.connected = false, .hasChoice = true, .onChosen = true, .onDefault = false}, false, DeviceMove::ToDefault, true},
        {{.added = true, .hasChoice = true}, true, DeviceMove::ToChosen, true},
        {{.added = true, .hasChoice = true}, false, DeviceMove::Stay, true},
        // the menu: an output chosen, there or not; another one; back to the default
        {{.reselect = true, .hasChoice = true}, true, DeviceMove::ToChosen, true},
        {{.reselect = true, .hasChoice = true}, false, DeviceMove::Stay, true},
        {{.reselect = true, .hasChoice = true, .onDefault = false}, true, DeviceMove::ToChosen, true},
        {{.reselect = true, .onDefault = false}, false, DeviceMove::ToDefault, false},
        {{.reselect = true}, false, DeviceMove::Stay, false},
        // no device since a loss: the chosen output if it is there, else the default
        {{.lost = true, .hasChoice = true, .onChosen = true, .onDefault = false}, true, DeviceMove::ToChosen, true},
        {{.lost = true, .hasChoice = true, .onChosen = true, .onDefault = false}, false, DeviceMove::ToDefault, true},
        {{.lost = true}, false, DeviceMove::ToDefault, false},
    };

    for (const Case& c : cases) {
        bool listed = false;
        const DeviceMove move = sound::DecideMove(c.seen, [&] {
            listed = true;
            return c.chosenPresent;
        });
        CHECK(move == c.move);
        CHECK(listed == c.listed);
    }
}

} // namespace

// a loopback device and its context set up as the sound will, then the checks above on it
void RunDeviceTests(const std::filesystem::path& data)
{
    CHECK(alcIsExtensionPresent(nullptr, "ALC_SOFT_loopback"));
    CHECK(alcIsExtensionPresent(nullptr, "ALC_SOFT_system_events"));
    CHECK(alcIsExtensionPresent(nullptr, "ALC_ENUMERATE_ALL_EXT"));
    TestDecideMove();
    ALCdevice* device = alcLoopbackOpenDeviceSOFT(nullptr);
    CHECK(device != nullptr);

    if (!device) {
        return;
    }

    CHECK(alcIsRenderFormatSupportedSOFT(device, kRate, ALC_STEREO_SOFT, ALC_SHORT_SOFT));
    const ALCint attributes[] = {
        ALC_FORMAT_CHANNELS_SOFT, ALC_STEREO_SOFT, ALC_FORMAT_TYPE_SOFT, ALC_SHORT_SOFT, ALC_FREQUENCY, kRate, ALC_HRTF_SOFT, ALC_FALSE, 0};
    ALCcontext* context = alcCreateContext(device, attributes);
    CHECK(context != nullptr);

    if (context && alcMakeContextCurrent(context)) {
        ALCint hrtf = ALC_TRUE;
        alcGetIntegerv(device, ALC_HRTF_SOFT, 1, &hrtf);
        CHECK_EQUAL(hrtf, ALC_FALSE);
        CHECK(alcIsExtensionPresent(device, "ALC_SOFT_reopen_device"));
        CHECK(alcIsExtensionPresent(device, "ALC_EXT_EFX"));
        CHECK(alIsExtensionPresent("AL_SOFT_source_resampler"));
        Play(data, device);
        Reverb();
        CHECK_EQUAL(alGetError(), AL_NO_ERROR);
        alcMakeContextCurrent(nullptr);
    }

    if (context) {
        alcDestroyContext(context);
    }

    alcCloseDevice(device);
    TestDevice(data);
    TestListenerBoost(data);
}

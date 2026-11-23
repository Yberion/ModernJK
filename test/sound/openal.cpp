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

// OpenAL Soft as the sound uses it, on its loopback device (rendered into memory, no sound card needed): HRTF off,
// the resampler bsinc24, the extensions the sound needs (the default device followed and reopened, EFX's EAX reverb),
// and a decoded sound played to its end.
#include "sound.h"

#include <check.h>

#include <shared/sound/decoder.h>

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alc.h>
#include <AL/alext.h>
#include <AL/efx.h>

#include <cstdint>
#include <string_view>

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

} // namespace

// a loopback device and its context set up as the sound will, then the checks above on it
void RunOpenALTests(const std::filesystem::path& data)
{
    CHECK(alcIsExtensionPresent(nullptr, "ALC_SOFT_loopback"));
    CHECK(alcIsExtensionPresent(nullptr, "ALC_SOFT_system_events"));
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
}

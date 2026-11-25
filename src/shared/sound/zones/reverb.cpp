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

// reverb.cpp -- the zones' reverb (reverb.h): at every frame the listener's zone found, its environment kept in a slot,
// the two neighbours chosen again every 100 ms, the pans set from the listener's axes, a map with one reverb faded from
// one environment into the next; a sound's send and filters from its zone and the obstacles, with the conversions of
// OpenAL Soft's own EAX layer (al/source.cpp: an obstruction and an occlusion in mB into the gains of a low-pass
// filter). Example: under water, every sound in the world goes through the filter that keeps its low frequencies and
// takes 30 dB off its high ones, and to the "underwater" reverb alone.
#include "reverb.h"

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alext.h>
#include <AL/efx-presets.h>
#include <AL/efx.h>

#include <fmt/format.h>

// LLVM libc's math, as netcodec takes the library's math, independent of the engines' q_libm.h
#include <shared/libc_common.h>
#include <shared/math/acosf.h>
#include <shared/math/logf.h>
#include <shared/math/powf.h>
#include <shared/math/sinf.h>
#include <src/__support/FPUtil/sqrt.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <numbers>
#include <string>
#include <utility>

namespace sound {
namespace {

// the neighbours chosen again every 100 ms, as the original engine did
constexpr int kChoiceInterval = 100;

// a map with one reverb fades into the next environment in ten steps of 100 ms, as Jedi Outcast's original did
constexpr int kFadeSteps = 10;
constexpr int kFadeStepLength = 100;

// the original's occlusion under water: -3000 mB, the low frequencies kept, 1.37 of it on the reverb
constexpr SourceObstacles kUnderwater{.occlusion = -3000, .occlusionLF = 0.0f, .occlusionRoom = 1.37f};

// the lowest level in mB, silence, as OpenAL Soft's EAX layer takes it
constexpr float kSilence = -10000.0f;

// a level in mB as a gain, as OpenAL Soft's level_mb_to_gain
float LevelToGain(float level) noexcept
{
    if (level <= kSilence) {
        return 0.0f;
    }

    return LIBC_NAMESPACE::shared::powf(10.0f, level / 2000.0f);
}

// a level in mB as a filter's gain, which cannot pass 1 (an obstacle of a positive level in a damaged file)
float FilterGain(float level) noexcept
{
    return std::min(LevelToGain(level), 1.0f);
}

// a gain as a level in mB, silence for 0
float GainToLevel(float gain) noexcept
{
    constexpr float kTenLog = 2000.0f / std::numbers::ln10_v<float>; // 2,000 mB per factor of 10 of the gain

    if (gain <= 0.0f) {
        return kSilence;
    }

    return std::max(kTenLog * LIBC_NAMESPACE::shared::logf(gain), kSilence);
}

// the share of an occlusion on a path at the low frequencies, as OpenAL Soft's eax_calculate_dst_occlusion_mb
float OcclusionLow(int occlusion, float pathRatio, float lfRatio) noexcept
{
    return static_cast<float>(occlusion) * std::max(pathRatio * lfRatio, pathRatio + lfRatio - 1.0f);
}

// the direct sound's filter: the obstruction and the occlusion (its whole on the direct path) at the low frequencies,
// and what the high frequencies lose beyond them, as OpenAL Soft's eax_create_direct_filter_param
void DirectFilter(const SourceObstacles& o, VoiceZone& zone) noexcept
{
    const float low = (static_cast<float>(o.obstruction) * o.obstructionLF) + OcclusionLow(o.occlusion, 1.0f, o.occlusionLF);
    const float high = static_cast<float>(o.obstruction) + static_cast<float>(o.occlusion);
    zone.directGain = FilterGain(low);
    zone.directGainHF = FilterGain(high - low);
}

// the send's filter: the occlusion's share on the reverb, as OpenAL Soft's eax_create_room_filter_param (no exclusion)
void SendFilter(const SourceObstacles& o, VoiceZone& zone) noexcept
{
    const float low = OcclusionLow(o.occlusion, o.occlusionRoom, o.occlusionLF);
    const float high = static_cast<float>(o.occlusion) * o.occlusionRoom;
    zone.sendGain = FilterGain(low);
    zone.sendGainHF = FilterGain(high - low);
}

// the "underwater" reverb, EFX's preset of EAX's environment
Reverb Underwater() noexcept
{
    const EFXEAXREVERBPROPERTIES p = EFX_REVERB_PRESET_UNDERWATER;
    return {.density = p.flDensity,
            .diffusion = p.flDiffusion,
            .gain = p.flGain,
            .gainHF = p.flGainHF,
            .gainLF = p.flGainLF,
            .decayTime = p.flDecayTime,
            .decayHFRatio = p.flDecayHFRatio,
            .decayLFRatio = p.flDecayLFRatio,
            .reflectionsGain = p.flReflectionsGain,
            .reflectionsDelay = p.flReflectionsDelay,
            .reflectionsPan = {p.flReflectionsPan[0], p.flReflectionsPan[1], p.flReflectionsPan[2]},
            .lateReverbGain = p.flLateReverbGain,
            .lateReverbDelay = p.flLateReverbDelay,
            .lateReverbPan = {p.flLateReverbPan[0], p.flLateReverbPan[1], p.flLateReverbPan[2]},
            .echoTime = p.flEchoTime,
            .echoDepth = p.flEchoDepth,
            .modulationTime = p.flModulationTime,
            .modulationDepth = p.flModulationDepth,
            .airAbsorptionGainHF = p.flAirAbsorptionGainHF,
            .hfReference = p.flHFReference,
            .lfReference = p.flLFReference,
            .roomRolloffFactor = p.flRoomRolloffFactor,
            .decayHFLimit = p.iDecayHFLimit != 0};
}

// a value between two, linearly
float Linear(float a, float b, float t) noexcept
{
    return a == b ? a : (a * (1.0f - t)) + (b * t);
}

// a value between two positive ones on a logarithmic scale, the offset added first and taken off after (the delays,
// which can be 0)
float Logarithmic(float a, float b, float t, float offset = 0.0f) noexcept
{
    if (a == b) {
        return a;
    }

    const float from = a + offset;
    const float to = b + offset;
    return (from * LIBC_NAMESPACE::shared::powf(to / from, t)) - offset;
}

// a gain between two, linearly on the scale of mB
float Level(float a, float b, float t) noexcept
{
    return a == b ? a : LevelToGain(Linear(GainToLevel(a), GainToLevel(b), t));
}

// a pan vector shortened to a length of 1 at most, as the original did before it interpolated one
Vector Clamped(const Vector& v) noexcept
{
    const float length = LIBC_NAMESPACE::fputil::sqrt<float>((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]));
    return length > 1.0f ? Vector{v[0] / length, v[1] / length, v[2] / length} : v;
}

// a pan between two, linearly, each shortened first
Vector PanBetween(const Vector& a, const Vector& b, float t) noexcept
{
    const Vector from = Clamped(a);
    const Vector to = Clamped(b);
    return {a[0] == b[0] ? a[0] : Linear(from[0], to[0], t), a[1] == b[1] ? a[1] : Linear(from[1], to[1], t), a[2] == b[2] ? a[2] : Linear(from[2], to[2], t)};
}

// a reverb a fraction of the way from one to another, as Jedi Outcast's original interpolated EAX's values: the levels
// linearly in mB, the size (here the density) and the times on a logarithmic scale, the rest linearly
Reverb Between(const Reverb& a, const Reverb& b, float t) noexcept
{
    if (t >= 1.0f) {
        return b;
    }

    if (t <= 0.0f) {
        return a;
    }

    constexpr float kDelayOffset = 0.0001f; // the original's, so that a delay of 0 has a logarithm
    return {.density = Logarithmic(a.density, b.density, t),
            .diffusion = Linear(a.diffusion, b.diffusion, t),
            .gain = Level(a.gain, b.gain, t),
            .gainHF = Level(a.gainHF, b.gainHF, t),
            .gainLF = Level(a.gainLF, b.gainLF, t),
            .decayTime = Logarithmic(a.decayTime, b.decayTime, t),
            .decayHFRatio = Logarithmic(a.decayHFRatio, b.decayHFRatio, t),
            .decayLFRatio = Logarithmic(a.decayLFRatio, b.decayLFRatio, t),
            .reflectionsGain = Level(a.reflectionsGain, b.reflectionsGain, t),
            .reflectionsDelay = Logarithmic(a.reflectionsDelay, b.reflectionsDelay, t, kDelayOffset),
            .reflectionsPan = PanBetween(a.reflectionsPan, b.reflectionsPan, t),
            .lateReverbGain = Level(a.lateReverbGain, b.lateReverbGain, t),
            .lateReverbDelay = Logarithmic(a.lateReverbDelay, b.lateReverbDelay, t, kDelayOffset),
            .lateReverbPan = PanBetween(a.lateReverbPan, b.lateReverbPan, t),
            .echoTime = Logarithmic(a.echoTime, b.echoTime, t),
            .echoDepth = Linear(a.echoDepth, b.echoDepth, t),
            .modulationTime = Logarithmic(a.modulationTime, b.modulationTime, t),
            .modulationDepth = Linear(a.modulationDepth, b.modulationDepth, t),
            .airAbsorptionGainHF = Level(a.airAbsorptionGainHF, b.airAbsorptionGainHF, t),
            .hfReference = Logarithmic(a.hfReference, b.hfReference, t),
            .lfReference = Logarithmic(a.lfReference, b.lfReference, t),
            .roomRolloffFactor = Linear(a.roomRolloffFactor, b.roomRolloffFactor, t),
            .decayHFLimit = a.decayHFLimit && b.decayHFLimit};
}

// a value of the effect within its range, so that a damaged file cannot make OpenAL Soft refuse it
void SetValue(std::uint32_t effect, ALenum parameter, float value, float low, float high)
{
    alEffectf(effect, parameter, std::clamp(value, low, high));
}

// a vector from the listener to a point, in the listener's space as the reverb's pans take it: right, up, forward
Vector Local(const Listener& listener, const Vector& point) noexcept
{
    const Vector d = {point[0] - listener.origin[0], point[1] - listener.origin[1], point[2] - listener.origin[2]};
    const auto& axes = listener.axes; // forward, left, up
    return {-((d[0] * axes[1][0]) + (d[1] * axes[1][1]) + (d[2] * axes[1][2])), (d[0] * axes[2][0]) + (d[1] * axes[2][1]) + (d[2] * axes[2][2]),
            (d[0] * axes[0][0]) + (d[1] * axes[0][1]) + (d[2] * axes[0][2])};
}

float Length(const Vector& v) noexcept
{
    return LIBC_NAMESPACE::fputil::sqrt<float>((v[0] * v[0]) + (v[1] * v[1]) + (v[2] * v[2]));
}

// the centre of an aperture, the middle of its two ends, in the game's axes
Vector Centre(const Aperture& a) noexcept
{
    return ToGame({.x = (a.first.x + a.second.x) / 2.0f, .y = (a.first.y + a.second.y) / 2.0f, .z = (a.first.z + a.second.z) / 2.0f});
}

// how strongly a reverb comes through an aperture: the chord over the arc of the angle its two ends cover from the
// listener, 1 for an aperture seen as a point, 2/pi for one seen across half the space, as the original computed it
float Strength(const Listener& listener, const Aperture& a) noexcept
{
    const Vector first = Local(listener, ToGame(a.first));
    const Vector second = Local(listener, ToGame(a.second));
    const float lengths = Length(first) * Length(second);

    if (lengths <= 0.0f) {
        return 1.0f;
    }

    const float cosine = std::clamp(((first[0] * second[0]) + (first[1] * second[1]) + (first[2] * second[2])) / lengths, -1.0f, 1.0f);
    const float angle = LIBC_NAMESPACE::shared::acosf(cosine);
    constexpr float kPoint = 1e-6f;
    return angle < kPoint ? 1.0f : 2.0f * LIBC_NAMESPACE::shared::sinf(angle / 2.0f) / angle;
}

} // namespace

ZoneReverb::ZoneReverb()
{
    for (Slot& slot : slots_) {
        alGenAuxiliaryEffectSlots(1, &slot.slot);
        alGenEffects(1, &slot.effect);
        alEffecti(slot.effect, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
    }
}

ZoneReverb::~ZoneReverb()
{
    for (const Slot& slot : slots_) {
        alAuxiliaryEffectSloti(slot.slot, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL);
        alDeleteAuxiliaryEffectSlots(1, &slot.slot);
        alDeleteEffects(1, &slot.effect);
    }
}

void ZoneReverb::Load(std::optional<ZoneFile> zones)
{
    zones_ = std::move(zones);
    listenerZone_.reset();
    listenerZoneNow_.reset();
    neighbours_ = {};
    neighbourApertures_ = {};
    lastChoice_.reset();
    underwater_ = false;
    fading_ = false;

    for (Slot& slot : slots_) {
        Empty(slot);
    }
}

// called at every frame with the setting: only a change does something
void ZoneReverb::SetEnabled(bool enabled)
{
    if (enabled == enabled_) {
        return;
    }

    enabled_ = enabled;

    if (!enabled_) {
        for (Slot& slot : slots_) {
            Empty(slot);
        }

        underwater_ = false;
        lastChoice_.reset();
    }
}

// the listener's zone, the water, then the slots of a map with neighbours or with one reverb
void ZoneReverb::Frame(int now)
{
    if (!enabled_ || !zones_) {
        return;
    }

    const ZoneFile& zones = *zones_;
    listenerZoneNow_ = ZoneOf(zones, ToZonePoint(listener_.origin));

    if (listenerZoneNow_) {
        listenerZone_ = listenerZoneNow_;
    }

    // under water, the "underwater" reverb alone in the first slot; out of it, the zones' again at once
    if (listener_.underwater) {
        if (!underwater_) {
            for (Slot& slot : slots_) {
                Empty(slot);
            }

            Apply(slots_[0], Underwater());
            underwater_ = true;
            fading_ = false;
            lastChoice_.reset();
        }

        return;
    }

    if (underwater_) {
        Empty(slots_[0]);
        underwater_ = false;
    }

    if (!listenerZone_) {
        return;
    }

    const int environment = zones.zoneEnvironment[static_cast<std::size_t>(*listenerZone_)];

    if (zones.apertures.empty()) {
        OneReverb(zones, environment, now);
    }
    else {
        Neighbours(zones, environment, now);
    }
}

// the listener's environment in the first slot: at once the first time, faded from the reverb set now otherwise
void ZoneReverb::OneReverb(const ZoneFile& zones, int environment, int now)
{
    Slot& slot = slots_[0];
    const Reverb& target = zones.environments[static_cast<std::size_t>(environment)].reverb;

    if (!slot.applied) {
        current_ = target;
        fading_ = false;
        slot.environment = environment;
        Apply(slot, current_);
        return;
    }

    if (slot.environment != environment) {
        fadeFrom_ = current_;
        fadeTo_ = target;
        fadeStart_ = now;
        fadeStep_ = 0;
        fading_ = true;
        slot.environment = environment;
    }

    if (!fading_) {
        return;
    }

    const int step = (now - fadeStart_) / kFadeStepLength;

    if (step >= kFadeSteps) {
        current_ = fadeTo_;
        fading_ = false;
        Apply(slot, current_);
    }
    else if (step > fadeStep_) {
        current_ = Between(fadeFrom_, fadeTo_, static_cast<float>(step) / static_cast<float>(kFadeSteps));
        fadeStep_ = step;
        Apply(slot, current_);
    }
}

// the two environments whose nearest aperture is nearest chosen again every 100 ms, and at once when the listener
// enters another environment (so that the one it leaves, a neighbour now, keeps its slot rather than being emptied and
// loaded again with its reverb cut); the listener's environment and theirs kept in the slots, the others emptied;
// every slot panned from the listener's axes
void ZoneReverb::Neighbours(const ZoneFile& zones, int environment, int now)
{
    if (!lastChoice_ || now - *lastChoice_ >= kChoiceInterval || choiceEnvironment_ != environment) {
        lastChoice_ = now;
        choiceEnvironment_ = environment;
        std::array<float, 2> distances = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        neighbours_ = {};
        neighbourApertures_ = {};
        const auto environments = static_cast<int>(zones.environments.size());

        for (int candidate = 0; candidate < environments; ++candidate) {
            if (candidate == environment) {
                continue;
            }

            float nearest = std::numeric_limits<float>::max();
            std::optional<std::size_t> nearestAperture;

            for (std::size_t i = 0; i < zones.apertures.size(); ++i) {
                if (zones.apertures[i].environment != candidate) {
                    continue;
                }

                const Vector centre = Centre(zones.apertures[i]);
                const float distance = Length({centre[0] - listener_.origin[0], centre[1] - listener_.origin[1], centre[2] - listener_.origin[2]});

                if (distance < nearest) {
                    nearest = distance;
                    nearestAperture = i;
                }
            }

            // kept among the two nearest, the first found first at an equal distance, as the original
            if (nearestAperture && nearest < distances[1]) {
                const std::size_t place = nearest < distances[0] ? 0 : 1;

                if (place == 0) {
                    distances[1] = distances[0];
                    neighbours_[1] = neighbours_[0];
                    neighbourApertures_[1] = neighbourApertures_[0];
                }

                distances[place] = nearest;
                neighbours_[place] = candidate;
                neighbourApertures_[place] = nearestAperture;
            }
        }
    }

    const auto wanted = [this, environment](const std::optional<int>& held) {
        return held && (*held == environment || held == neighbours_[0] || held == neighbours_[1]);
    };

    for (Slot& slot : slots_) {
        if (slot.applied && !wanted(slot.environment)) {
            Empty(slot);
        }
    }

    for (const std::optional<int>& want : {std::optional<int>(environment), neighbours_[0], neighbours_[1]}) {
        if (!want || SlotOf(*want) != nullptr) {
            continue;
        }

        const int missing = *want;

        for (Slot& slot : slots_) {
            if (!slot.applied) {
                slot.environment = missing;
                Apply(slot, zones.environments[static_cast<std::size_t>(missing)].reverb);
                break;
            }
        }
    }

    for (Slot& slot : slots_) {
        if (!slot.applied || !slot.environment) {
            continue;
        }

        const int held = *slot.environment;
        Reverb panned = zones.environments[static_cast<std::size_t>(held)].reverb;
        const Vector pan = PanOf(zones, held, environment);
        panned.reflectionsPan = pan;
        panned.lateReverbPan = pan;

        if (panned != *slot.applied) {
            Apply(slot, panned);
        }
    }
}

// a slot's pan: a neighbour's towards the aperture it was chosen by, as strong as that aperture is open to the
// listener; the listener's own away from its environment's nearest aperture, as strong as that one is closed
Vector ZoneReverb::PanOf(const ZoneFile& zones, int held, int listenerEnvironment) const
{
    std::optional<std::size_t> aperture;
    const bool own = held == listenerEnvironment;

    if (own) {
        float nearest = std::numeric_limits<float>::max();

        for (std::size_t i = 0; i < zones.apertures.size(); ++i) {
            if (zones.apertures[i].environment != listenerEnvironment) {
                continue;
            }

            const float distance = Length(Local(listener_, Centre(zones.apertures[i])));

            if (distance < nearest) {
                nearest = distance;
                aperture = i;
            }
        }
    }
    else {
        aperture = held == neighbours_[0] ? neighbourApertures_[0] : neighbourApertures_[1];
    }

    if (!aperture) {
        return {};
    }

    const Aperture& a = zones.apertures[*aperture];
    const Vector toward = Local(listener_, Centre(a));
    const float length = Length(toward);

    if (length <= 0.0f) {
        return {};
    }

    const float strength = own ? -(1.0f - Strength(listener_, a)) : Strength(listener_, a);
    return {toward[0] / length * strength, toward[1] / length * strength, toward[2] / length * strength};
}

// the slot holding an environment, nullptr for none
const ZoneReverb::Slot* ZoneReverb::SlotOf(int environment) const
{
    for (const Slot& slot : slots_) {
        if (slot.applied && slot.environment == environment) {
            return &slot;
        }
    }

    return nullptr;
}

ZoneReverb::Slot* ZoneReverb::SlotOf(int environment)
{
    for (Slot& slot : slots_) {
        if (slot.applied && slot.environment == environment) {
            return &slot;
        }
    }

    return nullptr;
}

// a slot emptied: no effect attached, so OpenAL Soft computes nothing for it
void ZoneReverb::Empty(Slot& slot)
{
    if (slot.applied) {
        alAuxiliaryEffectSloti(slot.slot, AL_EFFECTSLOT_EFFECT, AL_EFFECT_NULL);
    }

    slot.applied.reset();
    slot.environment.reset();
}

// a reverb's values given to the slot's effect, each within EFX's range, and the effect attached to the slot (OpenAL
// Soft copies it then: a change of the pans alone moves the reverb's gains, a change of its other values fades the old
// reverb into the new one)
void ZoneReverb::Apply(Slot& slot, const Reverb& r)
{
    const std::uint32_t e = slot.effect;
    SetValue(e, AL_EAXREVERB_DENSITY, r.density, AL_EAXREVERB_MIN_DENSITY, AL_EAXREVERB_MAX_DENSITY);
    SetValue(e, AL_EAXREVERB_DIFFUSION, r.diffusion, AL_EAXREVERB_MIN_DIFFUSION, AL_EAXREVERB_MAX_DIFFUSION);
    SetValue(e, AL_EAXREVERB_GAIN, r.gain, AL_EAXREVERB_MIN_GAIN, AL_EAXREVERB_MAX_GAIN);
    SetValue(e, AL_EAXREVERB_GAINHF, r.gainHF, AL_EAXREVERB_MIN_GAINHF, AL_EAXREVERB_MAX_GAINHF);
    SetValue(e, AL_EAXREVERB_GAINLF, r.gainLF, AL_EAXREVERB_MIN_GAINLF, AL_EAXREVERB_MAX_GAINLF);
    SetValue(e, AL_EAXREVERB_DECAY_TIME, r.decayTime, AL_EAXREVERB_MIN_DECAY_TIME, AL_EAXREVERB_MAX_DECAY_TIME);
    SetValue(e, AL_EAXREVERB_DECAY_HFRATIO, r.decayHFRatio, AL_EAXREVERB_MIN_DECAY_HFRATIO, AL_EAXREVERB_MAX_DECAY_HFRATIO);
    SetValue(e, AL_EAXREVERB_DECAY_LFRATIO, r.decayLFRatio, AL_EAXREVERB_MIN_DECAY_LFRATIO, AL_EAXREVERB_MAX_DECAY_LFRATIO);
    SetValue(e, AL_EAXREVERB_REFLECTIONS_GAIN, r.reflectionsGain, AL_EAXREVERB_MIN_REFLECTIONS_GAIN, AL_EAXREVERB_MAX_REFLECTIONS_GAIN);
    SetValue(e, AL_EAXREVERB_REFLECTIONS_DELAY, r.reflectionsDelay, AL_EAXREVERB_MIN_REFLECTIONS_DELAY, AL_EAXREVERB_MAX_REFLECTIONS_DELAY);
    alEffectfv(e, AL_EAXREVERB_REFLECTIONS_PAN, r.reflectionsPan.data());
    SetValue(e, AL_EAXREVERB_LATE_REVERB_GAIN, r.lateReverbGain, AL_EAXREVERB_MIN_LATE_REVERB_GAIN, AL_EAXREVERB_MAX_LATE_REVERB_GAIN);
    SetValue(e, AL_EAXREVERB_LATE_REVERB_DELAY, r.lateReverbDelay, AL_EAXREVERB_MIN_LATE_REVERB_DELAY, AL_EAXREVERB_MAX_LATE_REVERB_DELAY);
    alEffectfv(e, AL_EAXREVERB_LATE_REVERB_PAN, r.lateReverbPan.data());
    SetValue(e, AL_EAXREVERB_ECHO_TIME, r.echoTime, AL_EAXREVERB_MIN_ECHO_TIME, AL_EAXREVERB_MAX_ECHO_TIME);
    SetValue(e, AL_EAXREVERB_ECHO_DEPTH, r.echoDepth, AL_EAXREVERB_MIN_ECHO_DEPTH, AL_EAXREVERB_MAX_ECHO_DEPTH);
    SetValue(e, AL_EAXREVERB_MODULATION_TIME, r.modulationTime, AL_EAXREVERB_MIN_MODULATION_TIME, AL_EAXREVERB_MAX_MODULATION_TIME);
    SetValue(e, AL_EAXREVERB_MODULATION_DEPTH, r.modulationDepth, AL_EAXREVERB_MIN_MODULATION_DEPTH, AL_EAXREVERB_MAX_MODULATION_DEPTH);
    SetValue(e, AL_EAXREVERB_AIR_ABSORPTION_GAINHF, r.airAbsorptionGainHF, AL_EAXREVERB_MIN_AIR_ABSORPTION_GAINHF, AL_EAXREVERB_MAX_AIR_ABSORPTION_GAINHF);
    SetValue(e, AL_EAXREVERB_HFREFERENCE, r.hfReference, AL_EAXREVERB_MIN_HFREFERENCE, AL_EAXREVERB_MAX_HFREFERENCE);
    SetValue(e, AL_EAXREVERB_LFREFERENCE, r.lfReference, AL_EAXREVERB_MIN_LFREFERENCE, AL_EAXREVERB_MAX_LFREFERENCE);
    SetValue(e, AL_EAXREVERB_ROOM_ROLLOFF_FACTOR, r.roomRolloffFactor, AL_EAXREVERB_MIN_ROOM_ROLLOFF_FACTOR, AL_EAXREVERB_MAX_ROOM_ROLLOFF_FACTOR);
    alEffecti(e, AL_EAXREVERB_DECAY_HFLIMIT, r.decayHFLimit ? AL_TRUE : AL_FALSE);
    alAuxiliaryEffectSloti(slot.slot, AL_EFFECTSLOT_EFFECT, static_cast<ALint>(e));
    slot.applied = r;
}

// a sound's send and filters: none out of the world; under water the water's filter, and in the world the send to the
// "underwater" reverb; else its zone's reverb (the listener's when its zone has none in a slot, or for a sound at the
// listener) and the obstacles between its zone and the listener's (none for a voice)
VoiceZone ZoneReverb::Of(const Vector& position, Hearing hearing) const
{
    VoiceZone result;

    if (hearing == Hearing::NotInWorld) {
        return result;
    }

    if (listener_.underwater) {
        DirectFilter(kUnderwater, result);

        if (underwater_ && hearing != Hearing::OwnNotInWorld) {
            result.slot = slots_[0].slot;
            SendFilter(kUnderwater, result);
        }

        return result;
    }

    if (!zones_ || hearing == Hearing::OwnNotInWorld) {
        return result;
    }

    const ZoneFile& zones = *zones_;
    const Slot* listenerSlot = nullptr;

    if (listenerZone_) {
        listenerSlot = SlotOf(zones.zoneEnvironment[static_cast<std::size_t>(*listenerZone_)]);
    }

    const Slot* send = listenerSlot;

    if (hearing != Hearing::AtListener) {
        const ZonePoint point = ToZonePoint(position);
        const std::optional<int> zone = ZoneOf(zones, point);

        if (zone && !zones.apertures.empty()) {
            const Slot* own = SlotOf(zones.zoneEnvironment[static_cast<std::size_t>(*zone)]);
            send = own != nullptr ? own : listenerSlot;
        }

        if (hearing == Hearing::InWorld) {
            const SourceObstacles obstacles = ObstaclesOf(zones, listenerZoneNow_, zone, ToZonePoint(listener_.origin), point);
            DirectFilter(obstacles, result);
            SendFilter(obstacles, result);
        }
    }

    if (send != nullptr) {
        result.slot = send->slot;
    }

    return result;
}

std::string ZoneReverb::Info() const
{
    if (!zones_) {
        return "zones: none on this map";
    }

    const ZoneFile& zones = *zones_;
    std::string text = fmt::format("zones: {} environments, {} zones, {}{}", zones.environments.size(), zones.zoneEnvironment.size(),
                                   zones.apertures.empty() ? "one reverb" : "neighbours", enabled_ ? "" : ", the reverb off");

    if (listenerZone_) {
        const int environment = zones.zoneEnvironment[static_cast<std::size_t>(*listenerZone_)];
        text +=
            fmt::format("\n  listener: {}{}", zones.environments[static_cast<std::size_t>(environment)].name, listenerZoneNow_ ? "" : " (out of every zone)");
    }

    for (const Slot& slot : slots_) {
        if (underwater_ && &slot == slots_.data()) {
            text += "\n  slot: underwater";
        }
        else if (slot.environment) {
            text += fmt::format("\n  slot: {}", zones.environments[static_cast<std::size_t>(*slot.environment)].name);
        }
    }

    return text;
}

} // namespace sound

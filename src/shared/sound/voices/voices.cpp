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

// voices.cpp -- the sounds playing (voices.h): the voices kept in a table whose free entries are reused, a handle being
// an entry's index and the voice's serial number, unique in the session, so that a stopped voice's handle finds nothing;
// at every frame the ended voices removed, the audible ones sorted and the first 64 given the sources, the others'
// positions advancing on the clock; a source given only the values that changed since the last frame, every change
// applied in one batch (alDeferUpdatesSOFT). Example: eighty sabers hum at once: the 64 loudest at the listener are
// mixed, four at most of one sound file, and a saber that comes nearer takes the source of one going away; a hum that
// does not move costs no call once it plays.
#include "voices.h"

#include "classes.h"

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alext.h>
#include <AL/efx.h>

#include <fmt/format.h>

// LLVM libc's square root, as netcodec takes the library's math, independent of the engines' q_libm.h
#include <shared/libc_common.h>
#include <src/__support/FPUtil/sqrt.h>

#include <algorithm>
#include <limits>

namespace sound {

namespace {

// the entities whose position is kept: every entity of both engines
constexpr int kMaxEntities = 4096;

// the high frequencies' gain heard under water: -30 dB at 5 kHz, the original game's EAX occlusion of -3000 mB
constexpr float kUnderwaterGainHF = 0.0316f;

// a merged duplicate can be louder than the sound as recorded: OpenAL Soft accepts gains up to AL_GAIN_LIMIT_SOFT
constexpr float kMaxGain = 16.0f;

// a handle: the voice's index plus 1 in its low 32 bits, its serial number in its high 32 bits
constexpr unsigned kSerialShift = 32;
constexpr std::uint64_t kIndexMask = 0xFFFFFFFFULL;

// the distance between two points
float Distance(const Vector& a, const Vector& b) noexcept
{
    const float x = a[0] - b[0];
    const float y = a[1] - b[1];
    const float z = a[2] - b[2];
    return LIBC_NAMESPACE::fputil::sqrt<float>((x * x) + (y * y) + (z * z));
}

// the class's straight-line attenuation at a distance: 1 up to its full distance, 0 from its silent one
float Attenuation(const ClassInfo& info, float distance) noexcept
{
    if (distance <= info.fullUpTo) {
        return 1.0f;
    }

    if (distance >= info.silentFrom) {
        return 0.0f;
    }

    return 1.0f - ((distance - info.fullUpTo) / (info.silentFrom - info.fullUpTo));
}

// the slots one for the whole game: one announcement, one menu sound
bool IsGlobalSlot(Slot slot) noexcept
{
    return slot == Slot::Announcer || slot == Slot::Menu;
}

// what the log writes of an entity: its number, -1 for none
int EntityOf(const Request& request) noexcept
{
    return request.entity.value_or(-1);
}

} // namespace

// the 64 sources, each taking the resampler, and the under-water filter
Voices::Voices(Bank& bank, int resampler, std::uint32_t firstSerial) : bank_(bank), nextSerial_(firstSerial == 0 ? 1 : firstSerial)
{
    sources_.resize(kRealVoices);
    alGenSources(static_cast<ALsizei>(sources_.size()), sources_.data());
    sourceVoice_.resize(sources_.size());
    sourceState_.resize(sources_.size());

    for (const std::uint32_t source : sources_) {
        alSourcei(source, AL_SOURCE_RESAMPLER_SOFT, resampler);
        alSourcef(source, AL_MAX_GAIN, kMaxGain);
    }

    alGenFilters(1, &lowPass_);
    alFilteri(lowPass_, AL_FILTER_TYPE, AL_FILTER_LOWPASS);
    alFilterf(lowPass_, AL_LOWPASS_GAIN, 1.0f);
    alFilterf(lowPass_, AL_LOWPASS_GAINHF, kUnderwaterGainHF);
}

// the sources stopped and deleted without the voices' bookkeeping, which could allocate in a destructor
Voices::~Voices()
{
    alSourceStopv(static_cast<ALsizei>(sources_.size()), sources_.data());
    alDeleteSources(static_cast<ALsizei>(sources_.size()), sources_.data());
    alDeleteFilters(1, &lowPass_);
}

// a new voice, or the sum of the volumes when the same sound starts again at the same place before the next frame; the
// voice in the same slot stopped first
VoiceHandle Voices::Play(const Request& request)
{
    if ((request.place == Place::Entity && !request.entity) || bank_.Use(request.sound) == nullptr) {
        return VoiceHandle::None;
    }

    if (!request.loop) {
        for (const std::size_t index : started_) {
            Voice& voice = voices_[index];
            const Request& other = voice.request;

            if (voice.active && !other.loop && other.sound == request.sound && other.place == request.place && other.entity == request.entity &&
                other.origin == request.origin && other.slot == request.slot && other.soundClass == request.soundClass) {
                voice.request.volume += request.volume;
                Log("merge {} ent={} volume={:.2f}", bank_.Name(request.sound), EntityOf(request), voice.request.volume);
                return HandleOf(index);
            }
        }
    }

    if (request.slot != Slot::None) {
        for (std::size_t i = 0; i < voices_.size(); ++i) {
            if (voices_[i].active && InSlot(voices_[i].request, request.entity, request.slot)) {
                Log("replace {} ent={} slot={} audible={}", bank_.Name(voices_[i].request.sound), EntityOf(voices_[i].request), Name(request.slot),
                    Audibility(voices_[i]) > 0.0f ? 1 : 0);
                Release(i);
            }
        }
    }

    std::size_t index = voices_.size();

    if (free_.empty()) {
        voices_.emplace_back();
    }
    else {
        index = free_.back();
        free_.pop_back();
    }

    Voice& voice = voices_[index];
    voice.active = true;
    voice.serial = nextSerial_;
    nextSerial_ = nextSerial_ == std::numeric_limits<std::uint32_t>::max() ? 1 : nextSerial_ + 1;
    voice.request = request;
    voice.velocity = {};
    voice.start.reset();
    voice.source.reset();
    started_.push_back(index);
    Log("start {} ent={} class={} place={} slot={} loop={} lis={} dist={:.0f}", bank_.Name(request.sound), EntityOf(request), Name(request.soundClass),
        Name(request.place), Name(request.slot), request.loop ? 1 : 0, listener_.entity,
        request.place == Place::Listener ? 0.0f : Distance(PositionOf(voice), listener_.origin));
    return HandleOf(index);
}

void Voices::Stop(VoiceHandle handle)
{
    if (const Voice* voice = Find(handle)) {
        Log("stop {} ent={}", bank_.Name(voice->request.sound), EntityOf(voice->request));
        Release(static_cast<std::size_t>(voice - voices_.data()));
    }
}

void Voices::MoveLoop(VoiceHandle handle, const Vector& origin, const Vector& velocity)
{
    if (Voice* voice = Find(handle)) {
        voice->request.origin = origin;
        voice->velocity = velocity;
    }
}

void Voices::SetVolume(VoiceHandle handle, float volume)
{
    if (Voice* voice = Find(handle)) {
        voice->request.volume = volume;
    }
}

bool Voices::IsPlaying(VoiceHandle handle) const
{
    return Find(handle) != nullptr;
}

bool Voices::IsReal(VoiceHandle handle) const
{
    const Voice* voice = Find(handle);
    return voice != nullptr && voice->source.has_value();
}

void Voices::StopSlot(int entity, Slot slot)
{
    for (std::size_t i = 0; i < voices_.size(); ++i) {
        if (voices_[i].active && InSlot(voices_[i].request, entity, slot)) {
            Log("mute {} ent={} slot={}", bank_.Name(voices_[i].request.sound), EntityOf(voices_[i].request), Name(slot));
            Release(i);
        }
    }
}

void Voices::StopEntity(int entity)
{
    for (std::size_t i = 0; i < voices_.size(); ++i) {
        if (voices_[i].active && voices_[i].request.place != Place::Listener && voices_[i].request.entity == entity) {
            Log("stop {} ent={}", bank_.Name(voices_[i].request.sound), entity);
            Release(i);
        }
    }
}

void Voices::StopAll()
{
    for (std::size_t i = 0; i < voices_.size(); ++i) {
        if (voices_[i].active) {
            Release(i);
        }
    }

    entities_.clear();
}

void Voices::StopSounds(std::span<const SoundHandle> sounds)
{
    for (std::size_t i = 0; i < voices_.size(); ++i) {
        if (voices_[i].active && std::ranges::find(sounds, voices_[i].request.sound) != sounds.end()) {
            Release(i);
        }
    }
}

void Voices::UpdateEntity(int entity, const Vector& origin, const Vector& velocity)
{
    if (entity < 0 || entity >= kMaxEntities) {
        return;
    }

    if (static_cast<std::size_t>(entity) >= entities_.size()) {
        entities_.resize(static_cast<std::size_t>(entity) + 1);
    }

    entities_[static_cast<std::size_t>(entity)] = {.known = true, .origin = origin, .velocity = velocity};
}

// the ears: OpenAL Soft's listener takes the game's axes as they are, its right being forward x up as the game's is
void Voices::SetListener(const Listener& listener)
{
    listener_ = listener;
    const std::array<float, 6> orientation = {listener.axes[0][0], listener.axes[0][1], listener.axes[0][2],
                                              listener.axes[2][0], listener.axes[2][1], listener.axes[2][2]};
    alListenerfv(AL_POSITION, listener.origin.data());
    alListenerfv(AL_VELOCITY, listener.velocity.data());
    alListenerfv(AL_ORIENTATION, orientation.data());
}

// the voices that ended removed; then the 64 to mix chosen among the audible ones, by priority, then loudness, the voices
// already mixed first among equals, then the oldest, four of one sound at most; the voices that lose their source made
// virtual, those that gain one made real, every real voice's source updated, all applied at once. Without a device,
// every voice virtual on its clock, none mixed.
void Voices::Frame(int now, bool mixing)
{
    now_ = now;
    ++frames_;
    started_.clear();

    for (std::size_t i = 0; i < voices_.size(); ++i) {
        Voice& voice = voices_[i];

        if (!voice.active) {
            continue;
        }

        if (!voice.start) {
            voice.start = now;
        }

        const Sound* sound = bank_.Find(voice.request.sound);

        if (sound == nullptr) {
            Release(i);
            continue;
        }

        if (!mixing && voice.source) {
            MakeVirtual(i, true);
        }

        if (voice.source) {
            ALint state = AL_PLAYING;
            alGetSourcei(sources_[*voice.source], AL_SOURCE_STATE, &state);

            if (state == AL_STOPPED) {
                Log("end {} ent={}", bank_.Name(voice.request.sound), EntityOf(voice.request));
                Release(i);
            }
        }
        else if (!voice.request.loop && FrameOf(voice, *sound) >= sound->frames) {
            Log("end {} ent={} virtual", bank_.Name(voice.request.sound), EntityOf(voice.request));
            Release(i);
        }
    }

    if (!mixing) {
        LogFrame();
        return;
    }

    std::vector<Candidate>& candidates = candidates_;
    candidates.clear();

    for (std::size_t i = 0; i < voices_.size(); ++i) {
        const Voice& voice = voices_[i];

        if (!voice.active) {
            continue;
        }

        const float audibility = Audibility(voice);

        if (audibility > 0.0f) {
            candidates.push_back({.priority = Info(voice.request.soundClass).priority,
                                  .audibility = audibility,
                                  .real = voice.source.has_value(),
                                  .serial = voice.serial,
                                  .index = i});
        }
    }

    std::ranges::sort(candidates, [](const Candidate& a, const Candidate& b) {
        if (a.priority != b.priority) {
            return a.priority < b.priority;
        }

        if (a.audibility != b.audibility) {
            return a.audibility > b.audibility;
        }

        if (a.real != b.real) {
            return a.real;
        }

        return a.serial < b.serial;
    });

    std::vector<bool>& chosen = chosen_;
    chosen.assign(voices_.size(), false);
    std::vector<int>& perSound = perSound_;
    perSound.assign(static_cast<std::size_t>(bank_.Count()), 0);
    std::size_t mixed = 0;

    for (const Candidate& candidate : candidates) {
        if (mixed == kRealVoices) {
            break;
        }

        int& count = perSound[static_cast<std::size_t>(voices_[candidate.index].request.sound)];

        if (count == kVoicesPerSound) {
            continue;
        }

        ++count;
        ++mixed;
        chosen[candidate.index] = true;
    }

    alDeferUpdatesSOFT();

    for (std::size_t i = 0; i < voices_.size(); ++i) {
        if (voices_[i].active && voices_[i].source && !chosen[i]) {
            MakeVirtual(i, false);
        }
    }

    std::size_t nextSource = 0;

    for (std::size_t i = 0; i < voices_.size(); ++i) {
        if (!chosen[i] || voices_[i].source) {
            continue;
        }

        while (sourceVoice_[nextSource]) {
            ++nextSource;
        }

        MakeReal(i, nextSource);
    }

    for (const Voice& voice : voices_) {
        if (voice.active && voice.source) {
            UpdateSource(voice, *voice.source);
        }
    }

    alProcessUpdatesSOFT();
    LogFrame();
}

void Voices::Recover()
{
    for (std::size_t i = 0; i < voices_.size(); ++i) {
        if (voices_[i].active && voices_[i].source) {
            MakeVirtual(i, true);
        }
    }
}

// the state of the entity's voice in its voice slot, from the loudness of its sound where it plays now
VoiceState Voices::GetVoiceState(int entity) const
{
    for (const Voice& voice : voices_) {
        if (!voice.active || voice.request.slot != Slot::Voice || voice.request.place == Place::Listener || voice.request.entity != entity) {
            continue;
        }

        const Sound* sound = bank_.Find(voice.request.sound);

        if (sound == nullptr) {
            return VoiceState::NotTalking;
        }

        return LipSyncState(sound->loudness, voice.start ? FrameOf(voice, *sound) : 0);
    }

    return VoiceState::NotTalking;
}

std::size_t Voices::Playing() const noexcept
{
    return static_cast<std::size_t>(std::ranges::count_if(voices_, [](const Voice& voice) { return voice.active; }));
}

std::size_t Voices::Mixed() const noexcept
{
    return static_cast<std::size_t>(std::ranges::count_if(sourceVoice_, [](const std::optional<std::size_t>& voice) { return voice.has_value(); }));
}

// the voice of a handle, nullptr once it has ended or been stopped
Voices::Voice* Voices::Find(VoiceHandle handle)
{
    const auto value = static_cast<std::uint64_t>(handle);
    const std::uint64_t index = value & kIndexMask;

    if (index == 0 || index > voices_.size()) {
        return nullptr;
    }

    Voice& voice = voices_[index - 1];
    return voice.active && voice.serial == (value >> kSerialShift) ? &voice : nullptr;
}

// the same for a voice only read
const Voices::Voice* Voices::Find(VoiceHandle handle) const
{
    const auto value = static_cast<std::uint64_t>(handle);
    const std::uint64_t index = value & kIndexMask;

    if (index == 0 || index > voices_.size()) {
        return nullptr;
    }

    const Voice& voice = voices_[index - 1];
    return voice.active && voice.serial == (value >> kSerialShift) ? &voice : nullptr;
}

// a voice's index and serial number in one handle, the index counted from 1 so that None is no voice
VoiceHandle Voices::HandleOf(std::size_t index) const
{
    // std::size_t is std::uint64_t on Linux, which GCC's -Wuseless-cast refuses to cast: a widening brace instead
    return static_cast<VoiceHandle>((static_cast<std::uint64_t>(voices_[index].serial) << kSerialShift) | std::uint64_t{index + 1});
}

// whether a voice is in a slot: the announcer's and the menus' whatever its entity, the others the entity's own
bool Voices::InSlot(const Request& request, std::optional<int> entity, Slot slot) noexcept
{
    if (request.slot != slot || slot == Slot::None) {
        return false;
    }

    return IsGlobalSlot(slot) || (entity && request.entity == entity && request.place != Place::Listener);
}

// a sound of the listener's own entity, played at the listener: centred, as the player hears their own steps
bool Voices::IsOwn(const Request& request) const noexcept
{
    return request.place != Place::Listener && request.entity == listener_.entity;
}

// where a voice is heard from: its point, or its entity's last position (the listener's while the entity has none)
Vector Voices::PositionOf(const Voice& voice) const
{
    if (voice.request.place == Place::Entity && voice.request.entity) {
        const int entity = *voice.request.entity;

        if (entity >= 0 && static_cast<std::size_t>(entity) < entities_.size() && entities_[static_cast<std::size_t>(entity)].known) {
            return entities_[static_cast<std::size_t>(entity)].origin;
        }

        return listener_.origin;
    }

    return voice.request.origin;
}

// how fast a voice's source moves: its entity's velocity, or a loop's at its point
Vector Voices::VelocityOf(const Voice& voice) const
{
    if (voice.request.place == Place::Entity && voice.request.entity) {
        const int entity = *voice.request.entity;

        if (entity >= 0 && static_cast<std::size_t>(entity) < entities_.size()) {
            return entities_[static_cast<std::size_t>(entity)].velocity;
        }

        return {};
    }

    return voice.velocity;
}

// the source's gain: the voice's volume in its volume group, at the listener's own volume for its own sounds; the
// attenuation is OpenAL Soft's
float Voices::Gain(const Voice& voice) const
{
    const ClassInfo info = Info(voice.request.soundClass);
    const float group = info.group == VolumeGroup::Voices ? voiceVolume_ : 1.0f;
    const float own = IsOwn(voice.request) ? info.ownVolume : 1.0f;
    return voice.request.volume * group * own;
}

// how loud the voice is at the listener: its gain and its class's attenuation at its distance, as OpenAL Soft computes it
float Voices::Audibility(const Voice& voice) const
{
    const ClassInfo info = Info(voice.request.soundClass);

    if (!info.positioned || voice.request.place == Place::Listener || IsOwn(voice.request)) {
        return Gain(voice);
    }

    return Gain(voice) * Attenuation(info, Distance(PositionOf(voice), listener_.origin));
}

// the frame a voice plays now, from the time it started; a looping sound's within its sound
std::int64_t Voices::FrameOf(const Voice& voice, const Sound& sound) const
{
    const std::int64_t elapsed = std::max<std::int64_t>(0, now_ - voice.start.value_or(now_));
    const std::int64_t frame = elapsed * sound.rate / 1000;
    return voice.request.loop && sound.frames > 0 ? frame % sound.frames : frame;
}

// the voice removed, its source stopped and freed, its handle no longer valid
void Voices::Release(std::size_t index)
{
    Voice& voice = voices_[index];

    if (voice.source) {
        const std::uint32_t source = sources_[*voice.source];
        alSourceStop(source);
        alSourcei(source, AL_BUFFER, 0);
        sourceVoice_[*voice.source].reset();
        voice.source.reset();
    }

    voice.active = false;
    free_.push_back(index);
}

// the voice given a source, which plays its sound from where the voice has advanced to; everything sent to the source,
// whose last values are unknown
void Voices::MakeReal(std::size_t index, std::size_t source)
{
    Voice& voice = voices_[index];
    const Sound* sound = bank_.Find(voice.request.sound);

    if (sound == nullptr) {
        return;
    }

    const std::uint32_t id = sources_[source];
    voice.source = source;
    sourceVoice_[source] = index;
    sourceState_[source].known = false;
    alSourcei(id, AL_BUFFER, static_cast<ALint>(sound->buffer));
    alSourcei(id, AL_LOOPING, voice.request.loop ? AL_TRUE : AL_FALSE);
    alSourcei(id, AL_SAMPLE_OFFSET, static_cast<ALint>(FrameOf(voice, *sound)));
    UpdateSource(voice, source);
    alSourcePlay(id);
}

// the voice's source freed, its start moved so that its position keeps advancing from where the source played it, or,
// fromClock, where its clock says it is (a source OpenAL Soft stopped when the device was lost reads 0)
void Voices::MakeVirtual(std::size_t index, bool fromClock)
{
    Voice& voice = voices_[index];
    const Sound* sound = bank_.Find(voice.request.sound);

    if (!voice.source || sound == nullptr) {
        return;
    }

    const std::size_t source = *voice.source;
    const std::uint32_t id = sources_[source];

    if (!fromClock) {
        ALint offset = 0;
        alGetSourcei(id, AL_SAMPLE_OFFSET, &offset);
        voice.start = now_ - static_cast<int>(static_cast<std::int64_t>(offset) * 1000 / sound->rate);
    }

    alSourceStop(id);
    alSourcei(id, AL_BUFFER, 0);
    sourceVoice_[source].reset();
    voice.source.reset();
}

// a real voice's source: at the listener without attenuation for a sound not positioned or of the listener's own
// entity; elsewhere at its point, moving at its velocity, attenuated between its class's distances; under water, every
// sound in the water heard through the low-pass filter. Only what differs from the source's last values is sent.
void Voices::UpdateSource(const Voice& voice, std::size_t source)
{
    const std::uint32_t id = sources_[source];
    SourceState& last = sourceState_[source];
    const ClassInfo info = Info(voice.request.soundClass);
    const bool atListener = !info.positioned || voice.request.place == Place::Listener || IsOwn(voice.request);
    const bool inWater = listener_.underwater && (info.positioned || IsOwn(voice.request));
    const float gain = Gain(voice);
    const std::uint32_t filter = inWater ? lowPass_ : AL_FILTER_NULL;
    const Vector position = atListener ? Vector{} : PositionOf(voice);
    const Vector velocity = atListener ? Vector{} : VelocityOf(voice);
    const float fullUpTo = atListener ? 0.0f : info.fullUpTo;
    const float silentFrom = atListener ? 0.0f : info.silentFrom;

    if (!last.known || last.gain != gain) {
        alSourcef(id, AL_GAIN, gain);
    }

    if (!last.known || last.filter != filter) {
        alSourcei(id, AL_DIRECT_FILTER, static_cast<ALint>(filter));
    }

    if (!last.known || last.relative != atListener) {
        alSourcei(id, AL_SOURCE_RELATIVE, atListener ? AL_TRUE : AL_FALSE);
        alSourcef(id, AL_ROLLOFF_FACTOR, atListener ? 0.0f : 1.0f);
    }

    if (!last.known || last.position != position) {
        alSourcefv(id, AL_POSITION, position.data());
    }

    if (!last.known || last.velocity != velocity) {
        alSourcefv(id, AL_VELOCITY, velocity.data());
    }

    if (!last.known || last.fullUpTo != fullUpTo || last.silentFrom != silentFrom) {
        alSourcef(id, AL_REFERENCE_DISTANCE, fullUpTo);
        alSourcef(id, AL_MAX_DISTANCE, silentFrom);
    }

    last = {.known = true,
            .gain = gain,
            .filter = filter,
            .relative = atListener,
            .position = position,
            .velocity = velocity,
            .fullUpTo = fullUpTo,
            .silentFrom = silentFrom};
}

// the frame's line of the log: the voices mixed (busy, the field the log's reader counts) and the virtual ones; every
// 50th frame, the list of the voices
void Voices::LogFrame() const
{
    if (!log_) {
        return;
    }

    const std::size_t playing = Playing();
    const std::size_t mixed = Mixed();
    Log("frame lis={} busy={} virtual={}", listener_.entity, mixed, playing - mixed);

    if (frames_ % 50 == 0) {
        for (const Voice& voice : voices_) {
            if (voice.active) {
                Log("voice {} ent={} class={} {} age={}", bank_.Name(voice.request.sound), EntityOf(voice.request), Name(voice.request.soundClass),
                    voice.source ? "real" : "virtual", now_ - voice.start.value_or(now_));
            }
        }
    }
}

} // namespace sound

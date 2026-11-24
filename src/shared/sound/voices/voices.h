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

// voices.h -- the sounds playing: every sound played is a voice, real when OpenAL Soft mixes it on one of the 64
// sources, virtual otherwise, its playing position advancing all the same so that it plays on from there when it is
// mixed again. At every frame the voices to mix are chosen by their class's priority, then by how loud they are where
// the listener is, the voices already mixed first among equals, four at most of one event (the variants of a sound together), the
// voices weighed by their audibility and by what remains of them, so that a near sound beats a far new one and a fast
// weapon's new shot the end of an old one; the loops of one sound one voice,
// as the old mixer summed them on one cycle, so that eleven torches with the same fire do not echo one another. A sound in an entity's slot replaces the one
// there, the announcer's and the menus' slots being one for the whole game; a sound the game starts twice at one instant and place is one voice at the sum of
// the volumes. While the device is lost every voice is virtual, and mixed again on the next device where it has got to. Example: Play a pain in entity 12's
// voice slot: the line it was saying stops; GetVoiceState(12) gives its mouth's level until the pain ends.
#pragma once

#include <shared/sound/bank/bank.h>
#include <shared/sound/sound.h>

#include <fmt/format.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace sound {

// The voices mixed at once, and the voices of one event among them.
inline constexpr std::size_t kRealVoices = 64;
inline constexpr int kVoicesPerEvent = 4;

class Voices {
public:
    // The sources taking a resampler of OpenAL Soft; the voices' serial numbers starting from firstSerial, so that the
    // handles of a sound restarted never match those of the one before.
    Voices(Bank& bank, int resampler, std::uint32_t firstSerial);
    Voices(const Voices&) = delete;
    Voices& operator=(const Voices&) = delete;
    Voices(Voices&&) = delete;
    Voices& operator=(Voices&&) = delete;
    ~Voices();

    // A sound started, mixed from the next frame; None for kNoSound, a sound that cannot load, or a sound to follow an
    // entity without one.
    VoiceHandle Play(const Request& request);
    // A voice stopped; nothing when it has ended.
    void Stop(VoiceHandle handle);
    // A looping sound at a point moved, with its velocity for Doppler.
    void MoveLoop(VoiceHandle handle, const Vector& origin, const Vector& velocity);
    // A voice's volume, 1 for its sound as recorded.
    void SetVolume(VoiceHandle handle, float volume);
    // Whether a voice still plays.
    [[nodiscard]] bool IsPlaying(VoiceHandle handle) const;
    // Whether a voice is mixed now, on one of the 64 sources.
    [[nodiscard]] bool IsReal(VoiceHandle handle) const;
    // The sound in an entity's slot stopped; the announcer's and the menus' slot whatever the entity.
    void StopSlot(int entity, Slot slot);
    // Every sound of an entity stopped.
    void StopEntity(int entity);
    // Every voice stopped, and the entities' positions forgotten.
    void StopAll();
    // Stops the voices of sounds the bank is about to free.
    void StopSounds(std::span<const SoundHandle> sounds);
    // Where an entity's sounds are heard from, and how fast it moves.
    void UpdateEntity(int entity, const Vector& origin, const Vector& velocity);
    // The ears, given to OpenAL Soft.
    void SetListener(const Listener& listener);
    // The gain of the sounds at the listener and of the listener's own (Device::ListenerBoost).
    void SetListenerBoost(float boost) noexcept { listenerBoost_ = boost; }
    // The effects' and the voices' volumes, from 0 to 1, each its volume group's.
    void SetVolumes(float effects, float voices) noexcept
    {
        effectsVolume_ = effects;
        voiceVolume_ = voices;
    }
    // Where the lines of the sound log go; empty for none.
    void SetLog(std::function<void(std::string_view)> log) { log_ = std::move(log); }

    // At every frame, now in milliseconds: the ended voices removed, the voices to mix chosen, the sources updated;
    // without a device to mix on, every voice virtual.
    void Frame(int now, bool mixing);
    // After the device came back: every voice that was real made virtual where its clock says it is, OpenAL Soft
    // having stopped its source, so that the next frame mixes it again from there.
    void Recover();
    [[nodiscard]] VoiceState GetVoiceState(int entity) const;
    // The voices playing, and those mixed.
    [[nodiscard]] std::size_t Playing() const noexcept;
    [[nodiscard]] std::size_t Mixed() const noexcept;
    // The serial number the next voice takes.
    [[nodiscard]] std::uint32_t NextSerial() const noexcept { return nextSerial_; }

private:
    struct Voice {
        bool active = false;
        std::uint32_t serial = 0; // unique among the session's voices, in its handle
        Request request;
        Vector velocity{};                 // a looping sound's at a point
        std::optional<int> start;          // the frame's time it started at, once mixed or counted
        std::optional<std::size_t> source; // its source while it is real
        // the loops of its sound merged into it at this frame, set on the copy that plays them: their summed gain
        // and the direction they come from, in the listener's space
        std::optional<float> mergedGain;
        Vector mergedDirection{};
    };

    struct Entity {
        bool known = false; // its position given since the level started
        Vector origin{};
        Vector velocity{};
    };

    // what a source was last given, so that a frame sends OpenAL Soft only what changed
    struct SourceState {
        bool known = false; // nothing sent since the source took its voice
        float gain = 0.0f;
        std::uint32_t filter = 0;
        bool relative = false;
        Vector position{};
        Vector velocity{};
        float fullUpTo = 0.0f;
        float silentFrom = 0.0f;
    };

    // a voice the frame may mix
    // the loops of one sound at a frame: how many, their summed and loudest audibility, the most their sum may reach,
    // the direction they come from weighted by it, the copy that plays them (the one holding a source, else the
    // loudest), the best priority
    struct LoopGroup {
        int copies = 0;
        float sum = 0.0f;
        float loudest = 0.0f;
        float cap = 0.0f;
        Vector direction{};
        std::optional<std::size_t> player;
        bool playerReal = false;
    };

    struct Candidate {
        int priority = 0;
        bool real = false;
        bool loop = false;
        bool eligible = false; // among the four of its event
        float score = 0.0f;    // its audibility weighted by what remains of it, a mixed voice's raised a little
        std::uint32_t serial = 0;
        std::size_t index = 0;
    };

    [[nodiscard]] Voice* Find(VoiceHandle handle);
    [[nodiscard]] const Voice* Find(VoiceHandle handle) const;
    [[nodiscard]] VoiceHandle HandleOf(std::size_t index) const;
    [[nodiscard]] static bool InSlot(const Request& request, std::optional<int> entity, Slot slot) noexcept;
    [[nodiscard]] bool IsOwn(const Request& request) const noexcept;
    [[nodiscard]] Vector PositionOf(const Voice& voice) const;
    [[nodiscard]] Vector VelocityOf(const Voice& voice) const;
    [[nodiscard]] float ClassVolume(SoundClass soundClass) const noexcept;
    [[nodiscard]] float Gain(const Voice& voice) const;
    [[nodiscard]] float Audibility(const Voice& voice) const;
    [[nodiscard]] float Remaining(const Voice& voice) const;
    void MergeLoops();
    [[nodiscard]] std::int64_t FrameOf(const Voice& voice, const Sound& sound) const;
    void Release(std::size_t index);
    void MakeReal(std::size_t index, std::size_t source);
    void MakeVirtual(std::size_t index, bool fromClock);
    void UpdateSource(const Voice& voice, std::size_t source);
    void LogFrame() const;

    // a line of the sound log, formatted only when the log is on
    template <typename... Args> void Log(fmt::format_string<Args...> format, Args&&... args) const
    {
        if (log_) {
            log_(fmt::format("SNDLOG {} {}", now_, fmt::format(format, std::forward<Args>(args)...)));
        }
    }

    Bank& bank_;
    std::vector<Voice> voices_;
    std::vector<std::size_t> free_;
    std::vector<std::size_t> started_; // the voices started since the last frame, for the duplicates
    std::vector<std::uint32_t> sources_;
    std::vector<std::optional<std::size_t>> sourceVoice_;
    std::vector<SourceState> sourceState_;
    std::vector<Entity> entities_;
    std::vector<Candidate> candidates_; // the frame's working arrays, kept to spare their allocations
    std::vector<bool> chosen_;
    std::vector<int> perEvent_;
    std::vector<LoopGroup> loopGroups_;
    Listener listener_;
    std::uint32_t lowPass_ = 0; // the filter of the sounds heard under water
    std::uint32_t nextSerial_ = 1;
    float effectsVolume_ = 1.0f;
    float listenerBoost_ = 1.0f;
    float voiceVolume_ = 1.0f;
    int now_ = 0;
    int frames_ = 0;
    std::function<void(std::string_view)> log_;
};

} // namespace sound

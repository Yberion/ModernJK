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

// system.h -- the sound as an engine runs it: the device, the registered sounds, the voices, the music, the
// cinematics' stream and the ambient sets in one object, which is also the Api every module receives.
// Example: an engine creates it once, gives its modules the Api, and runs it at every frame:
//     std::unique_ptr<sound::System> sound = sound::System::Create(host);
//     sound->SetVolumes(effects, voices, music);
//     sound->Frame(milliseconds);
#pragma once

#include "host.h"
#include "sound.h"

#include <shared/sound/ambient/ambient.h>
#include <shared/sound/bank/bank.h>
#include <shared/sound/device/device.h>
#include <shared/sound/stream/music.h>
#include <shared/sound/stream/stream.h>
#include <shared/sound/voices/voices.h>

#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sound {

class System final : public Api {
public:
    // The sound on the system's default device, or on OpenAL Soft's loopback device at a rate (the tests); nullptr when
    // no device opens.
    [[nodiscard]] static std::unique_ptr<System> Create(const Host& host, std::optional<int> loopbackRate = std::nullopt, std::uint32_t firstSerial = 1);

    System(const System&) = delete;
    System& operator=(const System&) = delete;
    System(System&&) = delete;
    System& operator=(System&&) = delete;
    ~System() override;

    // Api
    SoundHandle Register(std::string_view name) override { return bank_.Register(name); }
    VoiceHandle Play(const Request& request) override { return voices_.Play(request); }
    void Stop(VoiceHandle voice) override { voices_.Stop(voice); }
    [[nodiscard]] bool IsPlaying(VoiceHandle voice) const override { return voices_.IsPlaying(voice); }
    void MoveLoop(VoiceHandle voice, const Vector& origin, const Vector& velocity) override { voices_.MoveLoop(voice, origin, velocity); }
    void StopSlot(int entity, Slot slot) override { voices_.StopSlot(entity, slot); }
    void StopEntity(int entity) override { voices_.StopEntity(entity); }
    void StopSounds() override;
    void UpdateEntity(int entity, const Vector& origin, const Vector& velocity) override { voices_.UpdateEntity(entity, origin, velocity); }
    void SetListener(const Listener& listener) override;
    [[nodiscard]] VoiceState GetVoiceState(int entity) const override { return voices_.GetVoiceState(entity); }
    [[nodiscard]] int Length(SoundHandle sound) const override { return bank_.Length(sound); }
    void StartMusic(std::string_view intro, std::string_view loop) override { music_.Start(intro, loop); }
    void StopMusic() override { music_.Stop(); }
    void ClearPrecachedSets() override { ambient_.ClearPrecached(); }
    void PrecacheSet(std::string_view name) override { ambient_.Precache(name); }
    void ParseSets() override { ambient_.Parse(); }
    void UpdateAmbientSet(std::string_view name, const Vector& origin) override;
    void AddLocalSet(std::string_view name, int entity, const Vector& origin) override { ambient_.AddLocal(name, entity, origin); }
    [[nodiscard]] SoundHandle BrushModelSound(std::string_view name, BrushModelStage stage) const override { return ambient_.BrushModelSound(name, stage); }

    // At every frame, now in milliseconds, after the game's calls.
    void Frame(int now);
    // A level's registration starts, then ends: the sounds the level did not register or play are freed at its end.
    void BeginRegistration() { bank_.BeginLevel(); }
    void EndRegistration();
    // Every sound stopped, the music and the cinematics' stream too.
    void StopAll();
    // The window's focus, and whether the sound is muted without it.
    void SetFocus(bool focused);
    void SetMuteWhenUnfocused(bool mute);
    // The effects' volume (every sound but the voices, and the cinematics), the voices' and the music's, each from 0
    // to 1 and independent of the others, as the game's menus present them.
    void SetVolumes(float effects, float voices, float music);
    void SetLanguage(Language language) noexcept { bank_.SetLanguage(language); }
    void SetMusicState(MusicState state) { music_.SetState(state); }
    void SetMusicLevel(std::string_view level) { music_.SetLevel(level); }
    void AllowDynamicMusic(bool allowed) noexcept { music_.AllowDynamic(allowed); }
    void SetMusicDebug(bool debug) noexcept { music_.SetDebug(debug); }
    // Where the sound log's lines go; empty for none.
    void SetLog(std::function<void(std::string_view)> log) { voices_.SetLog(std::move(log)); }

    // The cinematics' sound: a stream opened at its format, its samples queued as they come, then closed.
    void OpenStream(int rate, int channels);
    void QueueStream(std::span<const std::int16_t> samples);
    void CloseStream();

    void RestartMusic() { music_.Restart(); }

    // What a restarted sound takes from the one before: the names registered (the same handles again), the ambient
    // sets precached, the music asked for and the dynamic music's level, the voices' next serial number (so that no
    // handle a module kept finds a voice of the new sound).
    struct State {
        std::vector<std::string> sounds;
        std::vector<std::string> sets;
        std::string intro;
        std::string loop;
        std::string level;
        std::uint32_t serial = 1;
    };
    [[nodiscard]] State SaveState() const;
    void RestoreState(const State& state);

    // What the soundinfo command prints.
    [[nodiscard]] std::string Info() const;
    // What the soundlist command prints, one line per sound registered.
    [[nodiscard]] std::vector<std::string> List() const;
    [[nodiscard]] Device& GetDevice() noexcept { return *device_; }
    // Whether a voice is mixed now, on one of the 64 sources.
    [[nodiscard]] bool IsReal(VoiceHandle voice) const { return voices_.IsReal(voice); }
    // The voices playing, and those mixed.
    [[nodiscard]] std::size_t Playing() const noexcept { return voices_.Playing(); }
    [[nodiscard]] std::size_t Mixed() const noexcept { return voices_.Mixed(); }

private:
    System(Host host, std::optional<int> loopbackRate, std::uint32_t firstSerial);
    void FeedStream();
    [[nodiscard]] int Resampler() const noexcept { return device_ ? device_->Resampler() : 0; }

    Host host_;
    std::unique_ptr<Device> device_;
    Bank bank_;
    Voices voices_;
    Music music_;
    Ambient ambient_;
    std::unique_ptr<Stream> stream_;
    float effectsVolume_ = 1.0f; // the cinematics' stream's gain
    std::deque<std::int16_t> streamQueue_;
    Vector listener_{};
    bool focused_ = true;
    bool muteWhenUnfocused_ = true;
};

} // namespace sound

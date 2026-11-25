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

// bank.h -- the registered sounds: a name given a handle once, its file found in the player's language and decoded
// whole into an OpenAL buffer, mono, with what the lip sync reads of it; the sounds a level did not use freed when the
// next level's registration ends, and loaded again if they are played after.
// Example: Register("sound/weapons/blaster/fire.wav") gives a handle once and the same handle after; Length(handle)
// tells how long a caption stays; BeginLevel, the level's registrations, then Free(Unused()) at the end of its load.
#pragma once

#include "lipsync.h"

#include <shared/sound/host.h>
#include <shared/sound/sound.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sound {

// The language of the characters' voices: sound/chars/ is read from sound/chr_d/, chr_f/ or chr_e/ when the file
// exists there.
enum class Language : std::uint8_t {
    English,
    German,
    French,
    Spanish,
};

// A registered sound while it is loaded.
struct Sound {
    std::uint32_t buffer = 0; // OpenAL's
    int rate = 0;
    std::int64_t frames = 0;
    Loudness loudness;
};

class Bank {
public:
    explicit Bank(const Host& host) : host_(host) {}
    Bank(const Bank&) = delete;
    Bank& operator=(const Bank&) = delete;
    Bank(Bank&&) = delete;
    Bank& operator=(Bank&&) = delete;
    ~Bank();

    // The handle of a sound, loaded if it is not; kNoSound, with one warning, when its file does not exist.
    [[nodiscard]] SoundHandle Register(std::string_view name);
    // A sound as a restarted bank gets it back: its name as stored, whether it was loaded, its length.
    struct Saved {
        std::string name;
        bool loaded = true;
        int milliseconds = 0;
    };
    // A sound registered again after a restart, in its old order: it takes the next handle even when its file no longer
    // loads, so that the names after it keep theirs; loaded again only if it was, else kept with its length until a
    // level uses it.
    SoundHandle Restore(const Saved& saved);
    // A sound to play, loaded again if a level freed it; nullptr for kNoSound or a sound that cannot load.
    [[nodiscard]] const Sound* Use(SoundHandle handle);
    // A sound as it is, nullptr when it is not loaded.
    [[nodiscard]] const Sound* Find(SoundHandle handle) const;
    // The name a sound was registered with.
    [[nodiscard]] std::string_view Name(SoundHandle handle) const;
    // The event a sound plays: the sounds whose names differ only by a trailing number, the variants the game draws
    // among (sound/weapons/saber/saberhitwall1 to 3), are one event; from 0 to EventCount() - 1.
    [[nodiscard]] int Event(SoundHandle handle) const;
    [[nodiscard]] int EventCount() const noexcept { return static_cast<int>(events_.size()); }
    // A sound's length in milliseconds, known once it has loaded, freed or not; 0 for kNoSound.
    [[nodiscard]] int Length(SoundHandle handle) const;
    // A line of the soundlist command: the sound's name, rate, length and whether it is loaded.
    [[nodiscard]] std::string Describe(SoundHandle handle) const;
    // The sounds registered, in the order of their handles from 1: restored in that order, they get the same handles.
    [[nodiscard]] std::vector<Saved> Save() const;
    // The handles registered so far, kNoSound excluded: from 1 to Count() - 1.
    [[nodiscard]] SoundHandle Count() const noexcept { return static_cast<SoundHandle>(entries_.size()); }

    void SetLanguage(Language language) noexcept { language_ = language; }
    // A level's registration starts: the sounds it registers or plays are marked as used by it.
    void BeginLevel() noexcept { ++level_; }
    // The loaded sounds the current level has not used.
    [[nodiscard]] std::vector<SoundHandle> Unused() const;
    // Frees sounds' buffers; their handles stay, loaded again when played.
    void Free(std::span<const SoundHandle> handles);
    // Frees every sound's buffer.
    void FreeAll();

private:
    struct Entry {
        std::string name;
        std::optional<Sound> sound;
        int milliseconds = 0;           // its length, kept when it is freed
        int level = 0;                  // the last level that used it
        std::optional<int> failedLevel; // the level in which its file last failed to load: not tried again in it
        int event = 0;
    };

    [[nodiscard]] std::optional<Sound> Load(std::string_view name) const;
    // the entry loaded if it is not, once per level when its file fails; its length kept
    void EnsureLoaded(Entry& entry);
    [[nodiscard]] SoundHandle RegisterKey(std::string key);
    [[nodiscard]] int EventOf(std::string_view name);
    [[nodiscard]] std::optional<std::vector<std::byte>> ReadFile(std::string_view name) const;

    const Host& host_;
    std::vector<Entry> entries_{Entry{}}; // entries_[0] stands for kNoSound
    std::unordered_map<std::string, SoundHandle> handles_;
    std::unordered_map<std::string, int> events_; // a name without its trailing number, its event
    std::unordered_set<std::string> missing_;     // the names whose file does not exist, warned once
    Language language_ = Language::English;
    int level_ = 0;
};

} // namespace sound

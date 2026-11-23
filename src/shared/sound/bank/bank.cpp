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

// bank.cpp -- the registered sounds (bank.h): a name keyed once, its file read through the host in the best format
// found (in the player's language first for a voice), decoded whole, mixed to mono, measured for the lip sync and
// given to OpenAL Soft as a buffer; each sound's last level counted, so that a level frees what it did not use.
// Example: for a French player, Register("sound/chars/kyle/misc/pain25.wav") reads sound/chr_f/kyle/misc/pain25.mp3,
// or the English sound/chars/kyle/misc/pain25.mp3 when the French one is missing.
#include "bank.h"

#include <shared/sound/decoder/decoder.h>
#include <shared/sound/names.h>

#include <AL/al.h>

#include <fmt/format.h>

namespace sound {

namespace {

constexpr std::string_view kVoices = "sound/chars/";

// the folder of the characters' voices in a language, as long as "sound/chars/" so that the rest of the name stays
std::string_view VoicesFolder(Language language) noexcept
{
    switch (language) {
    case Language::German:
        return "sound/chr_d/";
    case Language::French:
        return "sound/chr_f/";
    case Language::Spanish:
        return "sound/chr_e/";
    case Language::English:
        break;
    }

    return kVoices;
}

// a stereo sound mixed to mono, each frame the mean of its two samples
std::vector<std::int16_t> ToMono(const Pcm& pcm)
{
    if (pcm.channels == 1) {
        return pcm.samples;
    }

    std::vector<std::int16_t> mono(pcm.samples.size() / 2);

    for (std::size_t i = 0; i < mono.size(); ++i) {
        mono[i] = static_cast<std::int16_t>((pcm.samples[2 * i] + pcm.samples[(2 * i) + 1]) / 2);
    }

    return mono;
}

} // namespace

Bank::~Bank()
{
    FreeAll();
}

// the handle of a known name, or a new entry once its file loads; a missing file warned once
SoundHandle Bank::Register(std::string_view name)
{
    return RegisterKey(NormalizeSoundName(name));
}

// Register for a name already keyed
SoundHandle Bank::RegisterKey(std::string key)
{
    if (const auto found = handles_.find(key); found != handles_.end()) {
        Entry& entry = entries_[static_cast<std::size_t>(found->second)];
        entry.level = level_;
        EnsureLoaded(entry);
        return found->second;
    }

    if (missing_.contains(key)) {
        return kNoSound;
    }

    std::optional<Sound> sound = Load(key);

    if (!sound) {
        host_.print(Message::Warning, fmt::format("could not find sound {}", key));
        missing_.insert(std::move(key));
        return kNoSound;
    }

    const auto handle = static_cast<SoundHandle>(entries_.size());
    const auto milliseconds = static_cast<int>(sound->frames * 1000 / sound->rate);
    entries_.push_back(Entry{.name = key, .sound = std::move(sound), .milliseconds = milliseconds, .level = level_, .failedLevel = std::nullopt});
    handles_.emplace(std::move(key), handle);
    return handle;
}

// the name as the bank stored it, not normalized again (a dot in a name without extension stays)
SoundHandle Bank::Restore(std::string_view name)
{
    const SoundHandle handle = RegisterKey(std::string(name));

    if (handle != kNoSound) {
        return handle;
    }

    const auto added = static_cast<SoundHandle>(entries_.size());
    entries_.push_back(Entry{.name = std::string(name), .sound = std::nullopt, .milliseconds = 0, .level = level_, .failedLevel = level_});
    handles_.emplace(std::string(name), added);
    return added;
}

// the entry marked as used by this level, and loaded again if a level freed it
const Sound* Bank::Use(SoundHandle handle)
{
    if (handle <= kNoSound || handle >= Count()) {
        return nullptr;
    }

    Entry& entry = entries_[static_cast<std::size_t>(handle)];
    entry.level = level_;
    EnsureLoaded(entry);
    return entry.sound ? &*entry.sound : nullptr;
}

void Bank::EnsureLoaded(Entry& entry)
{
    if (entry.sound || entry.failedLevel == level_) {
        return;
    }

    entry.sound = Load(entry.name);

    if (!entry.sound) {
        entry.failedLevel = level_;
        return;
    }

    entry.failedLevel.reset();
    entry.milliseconds = static_cast<int>(entry.sound->frames * 1000 / entry.sound->rate);
}

const Sound* Bank::Find(SoundHandle handle) const
{
    if (handle <= kNoSound || handle >= Count()) {
        return nullptr;
    }

    const Entry& entry = entries_[static_cast<std::size_t>(handle)];
    return entry.sound ? &*entry.sound : nullptr;
}

std::string_view Bank::Name(SoundHandle handle) const
{
    if (handle <= kNoSound || handle >= Count()) {
        return {};
    }

    return entries_[static_cast<std::size_t>(handle)].name;
}

std::vector<std::string> Bank::Names() const
{
    std::vector<std::string> names;

    for (std::size_t i = 1; i < entries_.size(); ++i) {
        names.push_back(entries_[i].name);
    }

    return names;
}

int Bank::Length(SoundHandle handle) const
{
    if (handle <= kNoSound || handle >= Count()) {
        return 0;
    }

    return entries_[static_cast<std::size_t>(handle)].milliseconds;
}

std::string Bank::Describe(SoundHandle handle) const
{
    if (handle <= kNoSound || handle >= Count()) {
        return {};
    }

    const Entry& entry = entries_[static_cast<std::size_t>(handle)];

    if (!entry.sound) {
        return fmt::format("{:5} ms  (freed)  {}", entry.milliseconds, entry.name);
    }

    return fmt::format("{:5} ms  {:5} Hz  {:8} B  {}", entry.milliseconds, entry.sound->rate, entry.sound->frames * 2, entry.name);
}

std::vector<SoundHandle> Bank::Unused() const
{
    std::vector<SoundHandle> unused;

    for (std::size_t i = 1; i < entries_.size(); ++i) {
        if (entries_[i].sound && entries_[i].level != level_) {
            unused.push_back(static_cast<SoundHandle>(i));
        }
    }

    return unused;
}

void Bank::Free(std::span<const SoundHandle> handles)
{
    for (const SoundHandle handle : handles) {
        if (handle <= kNoSound || handle >= Count()) {
            continue;
        }

        std::optional<Sound>& sound = entries_[static_cast<std::size_t>(handle)].sound;

        if (sound) {
            alDeleteBuffers(1, &sound->buffer);
            sound.reset();
        }
    }
}

void Bank::FreeAll()
{
    for (Entry& entry : entries_) {
        if (entry.sound) {
            alDeleteBuffers(1, &entry.sound->buffer);
            entry.sound.reset();
        }
    }
}

// the file decoded, mixed to mono, measured for the lip sync and given to OpenAL; nullopt when no file of that name
// exists or none decodes. A name starting with '*' stands for a character's own sound, which the game resolves to a
// file before it registers it: none is loaded under that name.
std::optional<Sound> Bank::Load(std::string_view name) const
{
    if (name.empty() || name.front() == '*') {
        return std::nullopt;
    }

    const std::optional<std::vector<std::byte>> file = ReadFile(name);

    if (!file) {
        return std::nullopt;
    }

    const std::optional<Pcm> pcm = Decode(*file);

    if (!pcm) {
        host_.print(Message::Warning, fmt::format("could not decode sound {}", name));
        return std::nullopt;
    }

    const std::vector<std::int16_t> mono = ToMono(*pcm);
    Sound sound{.rate = pcm->rate, .frames = static_cast<std::int64_t>(mono.size()), .loudness = MeasureLoudness(mono, pcm->rate)};
    alGenBuffers(1, &sound.buffer);
    alBufferData(sound.buffer, AL_FORMAT_MONO16, mono.data(), static_cast<ALsizei>(mono.size() * sizeof(std::int16_t)), pcm->rate);
    return sound;
}

// the first file found among the name's extensions, in the player's language first for a character's voice, then in
// English
std::optional<std::vector<std::byte>> Bank::ReadFile(std::string_view name) const
{
    const std::string_view folder = VoicesFolder(language_);

    if (folder != kVoices && name.starts_with(kVoices)) {
        std::string localized(folder);
        localized += name.substr(kVoices.size());

        if (auto file = ReadSoundFile(host_, localized)) {
            return file;
        }

        host_.print(Message::Debug, fmt::format("no {} in the player's language, English played", name));
    }

    return ReadSoundFile(host_, name);
}

} // namespace sound

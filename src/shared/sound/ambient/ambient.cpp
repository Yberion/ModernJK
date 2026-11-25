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

// ambient.cpp -- the ambient sets played (ambient.h): at every frame the general set the game asked for, a new one
// fading in over a second while the one before fades out; the local sets at the volume their distance gives; each
// set's loop started once and kept at its volume, its one-shot started when its delay has passed, the next delay drawn
// then. Example: Yavin's jungle (one-shots every 10 to 50 s) plays a creature's call, draws 37 s, and plays the next one 37 s
// later, whatever the frame rate.
#include "ambient.h"

#include <shared/sound/names.h>

#include <shared/libc_common.h>
#include <src/__support/FPUtil/sqrt.h>

#include <algorithm>
#include <cctype>

namespace sound {

namespace {

// the general sets' cross-fade
constexpr int kFadeMilliseconds = 1000;

constexpr float kMaxSetVolume = 255.0f;

// a text in lower case
std::string Lower(std::string_view text)
{
    std::string result(text);
    std::ranges::transform(result, result.begin(), [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    return result;
}

// a name kept in lower case equal to another whatever its case, without making a copy of it
bool SameName(std::string_view lower, std::string_view name) noexcept
{
    return std::ranges::equal(lower, name, [](char a, char b) { return a == static_cast<char>(std::tolower(static_cast<unsigned char>(b))); });
}

// the distance between two points
float Distance(const Vector& a, const Vector& b) noexcept
{
    const float x = a[0] - b[0];
    const float y = a[1] - b[1];
    const float z = a[2] - b[2];
    return LIBC_NAMESPACE::fputil::sqrt<float>((x * x) + (y * y) + (z * z));
}

// a local set's volume at a distance: full within half its radius, then down to silence at its radius (the class's
// own attenuation applying besides)
float LocalVolume(const AmbientSet& set, float distance) noexcept
{
    if (set.radius <= 0) {
        return 0.0f;
    }

    const float half = static_cast<float>(set.radius) * 0.5f;

    if (distance < half) {
        return 1.0f;
    }

    return std::clamp((static_cast<float>(set.radius) - distance) / half, 0.0f, 1.0f);
}

} // namespace

// a set the level asks for, read by the next Parse
void Ambient::Precache(std::string_view name)
{
    wanted_.insert(Lower(name));
}

// the sets precached read from sound/sound.txt, everything playing stopped first (the sets' indices change)
void Ambient::Parse()
{
    StopAll();
    sets_.clear();
    asked_.reset();
    generalAsked_ = false;
    const std::optional<std::vector<std::byte>> file = host_.readFile("sound/sound.txt");

    if (!file) {
        host_.print(Message::Error, "could not read the ambient sets, sound/sound.txt");
        return;
    }

    sets_ = ParseAmbientSets(ToText(*file), wanted_, bank_, host_);
}

// the general set asked for in this frame
void Ambient::UpdateGeneral(std::string_view name)
{
    generalAsked_ = true;
    asked_ = Find(name);
}

// a local set at its entity for this frame; another set at the same entity replaces the one it played; a set coming
// back at its entity keeps the time of its next one-shot, as the game kept it per entity
void Ambient::AddLocal(std::string_view name, int entity, const Vector& origin)
{
    const std::optional<std::size_t> set = Find(name);

    if (!set) {
        return;
    }

    const auto [found, added] = locals_.try_emplace(entity);
    Local& local = found->second;

    if (!added && local.set != *set) {
        StopSet(local);
        local = Local{};
    }

    if (added) {
        if (const auto shot = localShots_.find(entity); shot != localShots_.end() && shot->second.first == *set) {
            local.nextShot = shot->second.second;
        }
    }

    local.set = *set;
    local.origin = origin;
    local.asked = true;
}

SoundHandle Ambient::BrushModelSound(std::string_view name, BrushModelStage stage) const
{
    const std::optional<std::size_t> set = Find(name);
    const auto index = static_cast<std::size_t>(stage);

    if (!set || index >= sets_[*set].subWaves.size()) {
        return kNoSound;
    }

    return sets_[*set].subWaves[index];
}

// the general set changed: the one playing fades out while the new one fades in; none asked for in this frame, none
// plays; then the local sets at their volumes, those not asked for stopped
void Ambient::Frame(int now, const Vector& listener)
{
    now_ = now;
    const std::optional<std::size_t> target = generalAsked_ ? asked_ : std::nullopt;
    generalAsked_ = false;

    if (!target) {
        if (general_) {
            StopSet(*general_);
            general_.reset();
        }

        if (previous_) {
            StopSet(*previous_);
            previous_.reset();
        }
    }
    else if (!general_ || general_->set != *target) {
        if (previous_) {
            StopSet(*previous_);
        }

        // the set playing fades out from where its own fade had got to
        if (general_) {
            general_->fadeFrom = GeneralVolume(*general_);
            general_->fadeStart = now;
            general_->fadingOut = true;
        }

        previous_ = general_;
        general_ = General{};
        general_->set = *target;
        general_->fadeStart = now;
    }

    // a general set fading in plays its loop, its first one-shot once it is heard at its volume: drawn during the fade, it
    // came at a fraction of it and spent its delay of 10 to 50 s
    if (general_) {
        const float volume = GeneralVolume(*general_);
        PlaySet(*general_, volume, Place::Listener, std::nullopt, {}, volume >= 1.0f);
    }

    if (previous_) {
        const float volume = GeneralVolume(*previous_);

        if (volume > 0.0f) {
            PlaySet(*previous_, volume, Place::Listener, std::nullopt, {});
        }
        else {
            StopSet(*previous_);
            previous_.reset();
        }
    }

    for (auto it = locals_.begin(); it != locals_.end();) {
        Local& local = it->second;

        if (!local.asked) {
            StopSet(local);

            if (local.nextShot) {
                localShots_[it->first] = {local.set, *local.nextShot};
            }

            it = locals_.erase(it);
            continue;
        }

        local.asked = false;
        PlaySet(local, LocalVolume(sets_[local.set], Distance(local.origin, listener)), Place::Point, it->first, local.origin);
        ++it;
    }
}

// every set stopped, the local sets' times forgotten
void Ambient::StopAll()
{
    for (auto& [entity, local] : locals_) {
        StopSet(local);
    }

    locals_.clear();
    localShots_.clear();

    if (general_) {
        StopSet(*general_);
        general_.reset();
    }

    if (previous_) {
        StopSet(*previous_);
        previous_.reset();
    }
}

// a set by its name, whatever its case
std::optional<std::size_t> Ambient::Find(std::string_view name) const
{
    const auto found = std::ranges::find_if(sets_, [name](const AmbientSet& set) { return SameName(set.name, name); });
    return found != sets_.end() ? std::optional<std::size_t>(static_cast<std::size_t>(found - sets_.begin())) : std::nullopt;
}

// a general set's volume along its fade over a second: up from 0, or down from where it was
float Ambient::GeneralVolume(const General& general) const noexcept
{
    const float t = std::clamp(static_cast<float>(now_ - general.fadeStart) / static_cast<float>(kFadeMilliseconds), 0.0f, 1.0f);
    return general.fadingOut ? general.fadeFrom * (1.0f - t) : t;
}

// the set's loop kept at its volume and place; a one-shot when it is due, at a volume drawn within the set's range
// scaled by the set's volume, the delay before the next one drawn now
void Ambient::PlaySet(Playing& playing, float volume, Place place, std::optional<int> entity, const Vector& origin, bool oneShots)
{
    const AmbientSet& set = sets_[playing.set];

    if (set.loopedWave != kNoSound) {
        if (!voices_.IsPlaying(playing.loop)) {
            Request loop;
            loop.sound = set.loopedWave;
            loop.soundClass = SoundClass::Ambient;
            loop.place = place;
            loop.entity = entity;
            loop.origin = origin;
            loop.loop = true;
            loop.volume = volume;
            playing.loop = voices_.Play(loop);
        }
        else {
            voices_.SetVolume(playing.loop, volume);
            voices_.MoveLoop(playing.loop, origin, {});
        }
    }

    if (!oneShots || set.subWaves.empty() || (playing.nextShot && now_ < *playing.nextShot)) {
        return;
    }

    // a one-shot that would be silent (a set fading in from 0, a range of 0) waits for a frame where it is heard
    const int drawn =
        host_.random(static_cast<int>(volume * static_cast<float>(set.volumeStart)), static_cast<int>(volume * static_cast<float>(set.volumeEnd)));
    const int shot = std::min(drawn, static_cast<int>(volume * kMaxSetVolume));

    if (shot <= 0) {
        return;
    }

    playing.nextShot = now_ + (host_.random(set.timeStart, set.timeEnd) * 1000);
    const int wave = host_.random(0, static_cast<int>(set.subWaves.size()) - 1);

    Request request;
    request.sound = set.subWaves[static_cast<std::size_t>(wave)];
    request.soundClass = SoundClass::Ambient;
    request.place = place;
    request.entity = entity;
    request.origin = origin;
    request.volume = static_cast<float>(shot) / kMaxSetVolume;
    voices_.Play(request);
}

// a set's loop stopped
void Ambient::StopSet(Playing& playing)
{
    voices_.Stop(playing.loop);
    playing.loop = VoiceHandle::None;
}

} // namespace sound

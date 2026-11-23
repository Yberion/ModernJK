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

// ambient.h -- the ambient sets played: the level's general set around the listener, faded over a second into the
// next one when it changes; the local sets at their entities, louder as the listener comes within their radius; each
// set's loop kept playing, and its one-shots chosen at random, the delay before the next one drawn once, when one
// starts, between the set's two times.
// Example: the level's configstring names the general set "yavin_jungle": UpdateGeneral("yavin_jungle") at every frame
// plays it; a console with the local set "kejim_console" is heard within its radius of 300 units.
#pragma once

#include "sets.h"

#include <shared/sound/bank/bank.h>
#include <shared/sound/host.h>
#include <shared/sound/sound.h>
#include <shared/sound/voices/voices.h>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sound {

class Ambient {
public:
    Ambient(Bank& bank, Voices& voices, const Host& host) : bank_(bank), voices_(voices), host_(host) {}
    Ambient(const Ambient&) = delete;
    Ambient& operator=(const Ambient&) = delete;
    Ambient(Ambient&&) = delete;
    Ambient& operator=(Ambient&&) = delete;
    ~Ambient() = default;

    void ClearPrecached() { wanted_.clear(); }
    // The sets precached, in lower case.
    [[nodiscard]] const std::set<std::string, std::less<>>& Precached() const noexcept { return wanted_; }
    void Precache(std::string_view name);
    // Reads sound/sound.txt for the sets precached.
    void Parse();
    // The general set to play this frame; none when it is not called in a frame or names no set.
    void UpdateGeneral(std::string_view name);
    // A local set to play this frame at an entity.
    void AddLocal(std::string_view name, int entity, const Vector& origin);
    [[nodiscard]] SoundHandle BrushModelSound(std::string_view name, BrushModelStage stage) const;
    // At every frame, after the game's calls: the fades, the loops' volumes, the one-shots due; the sets not asked for
    // in this frame stopped.
    void Frame(int now, const Vector& listener);
    void StopAll();

private:
    struct Playing {
        std::size_t set = 0;
        VoiceHandle loop = VoiceHandle::None;
        std::optional<int> nextShot; // when the next one-shot starts; at once when the set starts
    };

    struct General : Playing {
        int fadeStart = 0;
        bool fadingOut = false;
        float fadeFrom = 1.0f; // the volume a fade out starts from
    };

    struct Local : Playing {
        Vector origin{};
        bool asked = false;
    };

    [[nodiscard]] std::optional<std::size_t> Find(std::string_view name) const;
    [[nodiscard]] float GeneralVolume(const General& general) const noexcept;
    void PlaySet(Playing& playing, float volume, Place place, std::optional<int> entity, const Vector& origin);
    void StopSet(Playing& playing);

    Bank& bank_;
    Voices& voices_;
    const Host& host_;
    std::set<std::string, std::less<>> wanted_;
    std::vector<AmbientSet> sets_;
    std::optional<General> general_;
    std::optional<General> previous_;  // the general set fading out
    std::optional<std::size_t> asked_; // the general set asked for in this frame
    bool generalAsked_ = false;
    std::map<int, Local> locals_;
    std::map<int, std::pair<std::size_t, int>> localShots_; // per entity, a local set gone and the time of its next one-shot
    int now_ = 0;
};

} // namespace sound

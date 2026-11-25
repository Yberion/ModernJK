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

// reverb.h -- a map's reverb by zones on OpenAL Soft, as the original engine's EAX path played it: three effect slots
// holding EFX's "EAX reverb", the listener's environment and, on a map whose file has apertures, the two environments
// whose nearest aperture is nearest, each panned towards that aperture (the listener's own away from its nearest one);
// on a map with one reverb, the listener's environment, faded into the next in 1 s by ten steps; under water, the
// "underwater" reverb alone. Each sound in the world is sent to its zone's reverb (the listener's when its zone has
// none in a slot), its direct sound and its send muffled by the obstacles between its zone and the listener's.
// Example: on mp/ffa3, a player in the "Tie Fighter Hall" hears the hall's reverb, the "Lunch Room"'s through its door
// ahead, and a shot fired in the Lunch Room sent to the Lunch Room's reverb:
//     reverb.Load(ParseZoneFile(text, error));
//     reverb.SetListener(listener);
//     reverb.Frame(now);
//     const VoiceZone zone = reverb.Of(shotPosition, Hearing::InWorld);   // its send's slot, its filters' gains
#pragma once

#include "zones.h"

#include <shared/sound/sound.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace sound {

// How a sound is heard, for the zones: not in the world (the announcer, the interface, the voices heard everywhere: no
// reverb, no filter), the same from the listener's own entity (no reverb, but the water's filter under water), at the
// listener (the listener's own sounds and those around it: the listener's reverb), in the world (sent to its zone's
// reverb, muffled by the obstacles), or a character's voice in the world (sent, never muffled, as the original left the
// voices out).
enum class Hearing : std::uint8_t {
    NotInWorld,
    OwnNotInWorld,
    AtListener,
    InWorld,
    VoiceInWorld,
};

// What the zones give a sound: the effect slot its send goes to (OpenAL's name of it, AL_EFFECTSLOT_NULL, 0, for none,
// as OpenAL takes it) and the gains of the low-pass filters on its direct sound and on that send (1 for none).
struct VoiceZone {
    std::uint32_t slot = 0;
    float directGain = 1.0f;
    float directGainHF = 1.0f;
    float sendGain = 1.0f;
    float sendGainHF = 1.0f;

    friend bool operator==(const VoiceZone&, const VoiceZone&) = default;
};

class ZoneReverb {
public:
    // The three effect slots and their effects, in the current OpenAL context.
    ZoneReverb();
    ZoneReverb(const ZoneReverb&) = delete;
    ZoneReverb& operator=(const ZoneReverb&) = delete;
    ZoneReverb(ZoneReverb&&) = delete;
    ZoneReverb& operator=(ZoneReverb&&) = delete;
    ~ZoneReverb();

    // A map's zones, nullopt for a map without (no reverb then); the slots emptied.
    void Load(std::optional<ZoneFile> zones);
    // The reverb on or off; the obstacles' and the water's filters stay either way.
    void SetEnabled(bool enabled);
    // The ears, as the voices take them.
    void SetListener(const Listener& listener)
    {
        listener_ = listener;
        listenerKnown_ = true;
    }
    // Once per frame, before the voices: the listener's zone, the slots' environments and pans, the fade, the water.
    void Frame(int now);
    // What a sound heard from a point (in the game's axes) gets.
    [[nodiscard]] VoiceZone Of(const Vector& position, Hearing hearing) const;
    // The broadband gain of a sound's direct filter, which lowers its audibility in the choice of the voices mixed.
    [[nodiscard]] static float Audibility(const VoiceZone& zone) noexcept { return zone.directGain; }
    // What soundinfo prints: the zones of the map, the listener's environment, the slots' environments.
    [[nodiscard]] std::string Info() const;
    // What a slot holds (the tests read it): its OpenAL name, its environment (none when empty or under water), the
    // reverb attached to it (none when empty).
    static constexpr std::size_t kSlots = 3;
    [[nodiscard]] std::uint32_t SlotName(std::size_t slot) const { return slots_[slot].slot; }
    [[nodiscard]] std::optional<int> SlotEnvironment(std::size_t slot) const { return slots_[slot].environment; }
    [[nodiscard]] const std::optional<Reverb>& SlotReverb(std::size_t slot) const { return slots_[slot].applied; }

private:
    struct Slot {
        std::uint32_t slot = 0;
        std::uint32_t effect = 0;
        std::optional<int> environment; // the environment it holds, none when empty or under water
        std::optional<Reverb> applied;  // the reverb attached, none when the slot is empty
    };

    static void Empty(Slot& slot);
    static void Apply(Slot& slot, const Reverb& reverb);
    void Neighbours(const ZoneFile& zones, int environment, int now);
    void OneReverb(const ZoneFile& zones, int environment, int now);
    [[nodiscard]] Vector PanOf(const ZoneFile& zones, int held, int listenerEnvironment) const;
    [[nodiscard]] const Slot* SlotOf(int environment) const;
    [[nodiscard]] Slot* SlotOf(int environment);

    std::array<Slot, kSlots> slots_{};
    std::optional<ZoneFile> zones_;
    bool enabled_ = true;
    Listener listener_;
    bool listenerKnown_ = false;         // given since the zones were loaded: the map before's place is no place on this one
    std::optional<int> listenerZone_;    // the last zone the listener was in, whose reverb it keeps out of every zone
    std::optional<int> listenerZoneNow_; // the zone it is in now, none out of every zone, for the obstacles
    std::array<std::optional<int>, 2> neighbours_{};
    std::array<std::optional<std::size_t>, 2> neighbourApertures_{}; // the aperture each neighbour is panned towards
    std::optional<int> lastChoice_;                                  // when the neighbours were last chosen
    std::optional<int> choiceEnvironment_;                           // the listener's environment then
    bool underwater_ = false;
    // the fade of a map with one reverb: from and to, the reverb set now, when it started and the last step set
    Reverb fadeFrom_;
    Reverb fadeTo_;
    Reverb current_;
    int fadeStart_ = 0;
    int fadeStep_ = 0;
    bool fading_ = false;
};

} // namespace sound

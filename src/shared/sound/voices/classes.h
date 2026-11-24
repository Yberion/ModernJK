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

// classes.h -- what each class of sound gives the sounds played with it: the attenuation, a straight line between the
// distance up to which it is at full volume and the distance from which it is silent (the distances the game's sounds
// were balanced for), whether it is positioned at all, its volume group, its priority when the voices to mix are chosen,
// the volume of the listener's own sounds, which are not positioned (0.75 for the body, the weapons, the force, the
// world and the voices, so that they do not stand out over the sounds around; full for the others), and the class's own
// gain, as a bus's (the sabers 2 dB down, tuned by ear: a crowd's sabers stood out over the rest).
// Example: a Weapon sound is at full volume up to 256 units, silent from 1,506, follows the effects' volume and comes
// second in the choice of the voices mixed, after the voices and the interface.
#pragma once

#include <shared/sound/sound.h>

#include <cstdint>
#include <string_view>

namespace sound {

// The volume setting a class follows: the effects' (s_volume) or the voices' (s_volumeVoice), as the game always had it.
enum class VolumeGroup : std::uint8_t {
    Effects,
    Voices,
};

struct ClassInfo {
    float fullUpTo = 0.0f;   // units
    float silentFrom = 0.0f; // units
    bool positioned = true;
    VolumeGroup group = VolumeGroup::Effects;
    int priority = 1;       // 1 first
    float ownVolume = 1.0f; // a sound of the listener's own entity
    float gain = 1.0f;      // every sound of the class
};

// The class's attenuation, volume group and priority.
[[nodiscard]] constexpr ClassInfo Info(SoundClass soundClass) noexcept
{
    switch (soundClass) {
    case SoundClass::Voice:
        return {.fullUpTo = 768.0f, .silentFrom = 2018.0f, .group = VolumeGroup::Voices, .priority = 1, .ownVolume = 0.75f};
    case SoundClass::VoiceQuiet:
        return {.fullUpTo = 346.0f, .silentFrom = 596.0f, .group = VolumeGroup::Voices, .priority = 1, .ownVolume = 0.75f};
    case SoundClass::VoiceEverywhere:
        return {.positioned = false, .group = VolumeGroup::Voices, .priority = 1};
    case SoundClass::Announcer:
        return {.positioned = false, .priority = 1};
    case SoundClass::Interface:
        return {.positioned = false, .priority = 1};
    case SoundClass::Weapon:
        return {.fullUpTo = 256.0f, .silentFrom = 1506.0f, .priority = 2, .ownVolume = 0.75f};
    case SoundClass::Saber:
        return {.fullUpTo = 256.0f, .silentFrom = 1506.0f, .priority = 2, .ownVolume = 0.75f, .gain = 0.79f};
    case SoundClass::Force:
        return {.fullUpTo = 256.0f, .silentFrom = 1506.0f, .priority = 3, .ownVolume = 0.75f};
    case SoundClass::Vehicle:
        return {.fullUpTo = 2048.0f, .silentFrom = 3298.0f, .priority = 3};
    case SoundClass::World:
        return {.fullUpTo = 256.0f, .silentFrom = 1506.0f, .priority = 4, .ownVolume = 0.75f};
    case SoundClass::Body:
        return {.fullUpTo = 256.0f, .silentFrom = 1506.0f, .priority = 5, .ownVolume = 0.75f};
    case SoundClass::Ambient:
        return {.fullUpTo = 256.0f, .silentFrom = 1506.0f, .priority = 6};
    }

    return {};
}

// The class's name, as the sound log writes it.
[[nodiscard]] constexpr std::string_view Name(SoundClass soundClass) noexcept
{
    switch (soundClass) {
    case SoundClass::Voice:
        return "voice";
    case SoundClass::VoiceQuiet:
        return "voicequiet";
    case SoundClass::VoiceEverywhere:
        return "voiceeverywhere";
    case SoundClass::Announcer:
        return "announcer";
    case SoundClass::Interface:
        return "interface";
    case SoundClass::Weapon:
        return "weapon";
    case SoundClass::Saber:
        return "saber";
    case SoundClass::Force:
        return "force";
    case SoundClass::Vehicle:
        return "vehicle";
    case SoundClass::World:
        return "world";
    case SoundClass::Body:
        return "body";
    case SoundClass::Ambient:
        return "ambient";
    }

    return "";
}

// The slot's name, as the sound log writes it.
[[nodiscard]] constexpr std::string_view Name(Slot slot) noexcept
{
    switch (slot) {
    case Slot::None:
        return "none";
    case Slot::Voice:
        return "voice";
    case Slot::Weapon:
        return "weapon";
    case Slot::Item:
        return "item";
    case Slot::Announcer:
        return "announcer";
    case Slot::Menu:
        return "menu";
    }

    return "";
}

// The place's name, as the sound log writes it.
[[nodiscard]] constexpr std::string_view Name(Place place) noexcept
{
    switch (place) {
    case Place::Entity:
        return "entity";
    case Place::Point:
        return "point";
    case Place::Listener:
        return "listener";
    }

    return "";
}

} // namespace sound

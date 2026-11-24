/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors
Copyright (C) 2026, ModernJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
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

#pragma once

// q_sound.h -- what the game modules of the three trees (the multiplayer, Jedi Academy's and Jedi Outcast's single
// player) share about the sound: the class and slot of a channel the game's data names, a voice's level for the faces'
// animations, a sound event's packing, the volumes of the shots and the saber's hum, a blade's contacts with the world.

#include <shared/sound/sound.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <optional>
#include <utility>

// What a sound the game's data names by its channel (the animations' events, the scripts, the effects) plays as: its
// class, and the slot of its entity it takes; for the soundChannel_t of each tree, whose channels have the same names
// (the multiplayer's alone has CHAN_MENU1)
struct soundUse_t {
    sound::SoundClass soundClass = sound::SoundClass::World;
    sound::Slot slot = sound::Slot::None;
};

template <typename Channel> constexpr soundUse_t BG_SoundUseOfChannel(Channel channel)
{
    if constexpr (requires { Channel::CHAN_MENU1; }) {
        if (channel == Channel::CHAN_MENU1) {
            return {sound::SoundClass::Interface, sound::Slot::Menu};
        }
    }

    switch (channel) {
    case Channel::CHAN_WEAPON:
        return {sound::SoundClass::Weapon, sound::Slot::Weapon};
    case Channel::CHAN_VOICE:
        return {sound::SoundClass::Voice, sound::Slot::Voice};
    case Channel::CHAN_VOICE_ATTEN:
        return {sound::SoundClass::VoiceQuiet, sound::Slot::Voice};
    case Channel::CHAN_VOICE_GLOBAL:
        return {sound::SoundClass::VoiceEverywhere, sound::Slot::Voice};
    case Channel::CHAN_ITEM:
        return {sound::SoundClass::World, sound::Slot::Item};
    case Channel::CHAN_BODY:
        return {sound::SoundClass::Body, sound::Slot::None};
    case Channel::CHAN_LOCAL:
    case Channel::CHAN_LOCAL_SOUND:
        return {sound::SoundClass::Interface, sound::Slot::Menu};
    case Channel::CHAN_ANNOUNCER:
        return {sound::SoundClass::Announcer, sound::Slot::Announcer};
    case Channel::CHAN_LESS_ATTEN:
        return {sound::SoundClass::Vehicle, sound::Slot::None};
    case Channel::CHAN_AMBIENT:
        return {sound::SoundClass::Ambient, sound::Slot::None};
    default: // CHAN_AUTO, CHAN_MUSIC
        break;
    }

    return {};
}

// A voice's state as the loudness level the faces' animations count with: 0 not talking, -1 talking in a silence, 1 to 4
// talking at a level (the head's bob moves with the level's changes, a face's skin and animation take the level)
constexpr int BG_VoiceLevel(sound::VoiceState state)
{
    switch (state) {
    case sound::VoiceState::NotTalking:
        return 0;
    case sound::VoiceState::Silent:
        return -1;
    default:
        return std::to_underlying(state) - std::to_underlying(sound::VoiceState::Level1) + 1;
    }
}

// A sound event of the game (EV_GENERAL_SOUND, EV_ENTITY_SOUND, EV_MUTE_SOUND): the sound's class, its slot, and whether
// it loops until its entity's sounds are stopped (a force power's sound on its tracker), packed in the one field of the
// event that carries them
struct soundEvent_t {
    sound::SoundClass soundClass = sound::SoundClass::World;
    sound::Slot slot = sound::Slot::None;
    bool loop = false;
};

// the field: the class in its low four bits, the slot in the next three, the loop in the eighth
inline constexpr int SOUND_EVENT_CLASS_MASK = 0xF;
inline constexpr int SOUND_EVENT_SLOT_SHIFT = 4;
inline constexpr int SOUND_EVENT_SLOT_MASK = 0x7;
inline constexpr int SOUND_EVENT_LOOP = 1 << 7;

inline int BG_PackSoundEvent(const soundEvent_t& event)
{
    return std::to_underlying(event.soundClass) | (std::to_underlying(event.slot) << SOUND_EVENT_SLOT_SHIFT) | (event.loop ? SOUND_EVENT_LOOP : 0);
}

// a weapon's shot, the sound of its muzzle effect played as the shooter's: 2 dB above the sound as recorded, tuned by
// ear once the shots were no longer played twice
inline constexpr float kShotVolume = 1.26f;

// a lit saber's hum: OpenAL Soft pans a sound in front at 0.596 of full on each ear where the old mixer gave 0.5, so
// the hum is brought back to its old level in front (1.5 dB down), the panning kept
inline constexpr float kSaberHumVolume = 0.84f;

// A blade's contacts with the world: while the blade touches the world without a break, a new sound every 250 ms, four
// at most playing at once, each heard whole; a contact after the blade left the world starts its sound at once, beside
// the others; never two within 100 ms of each other (a blade grazing an edge, touching it at one frame in two). Example:
// a blade dragged 3 s along a wall with sounds of 1 s plays twelve of them, four at a time; two strikes of a wall 0.5 s
// apart play two. The sounds at once times the interval cover a sound's length (the game's last about 1 s), so that a
// scrape's sounds keep their spacing. A contact's first sound is at full volume; the sounds that follow it while the blade
// scrapes 3 dB down, since four of them overlap.
inline constexpr int kContactSoundInterval = 100;  // milliseconds between two sounds of a blade at least
inline constexpr int kScrapeSoundInterval = 250;   // milliseconds between two sounds of a scrape
inline constexpr std::size_t kScrapeSounds = 4;    // the sounds of a scrape playing at once at most
inline constexpr float kScrapeSoundVolume = 0.71f; // a scrape's sounds after the contact's first

struct contactSound_t {
    std::array<sound::VoiceHandle, kScrapeSounds> voices{}; // its last sounds, the oldest replaced by the next
    std::size_t next = 0;                                   // where its next sound goes in voices
    std::optional<int> started;                             // when its last sound started
    bool touching = false;                                  // touching the world without a break since then
};

// A blade touching the world at time now: its next sound started by start(volume), which returns its voice, when it is
// due; isPlaying(voice) tells whether one of its sounds still plays.
template <typename IsPlaying, typename Start> void BG_ContactSound(contactSound_t& contact, int now, IsPlaying isPlaying, Start start)
{
    if (contact.started) {
        const int since = now - *contact.started;

        if (since < kContactSoundInterval) {
            return;
        }

        if (contact.touching && (since < kScrapeSoundInterval || std::ranges::count_if(contact.voices, isPlaying) >= std::ssize(contact.voices))) {
            return;
        }
    }

    contact.voices[contact.next] = start(contact.touching ? kScrapeSoundVolume : 1.0f);
    contact.next = (contact.next + 1) % kScrapeSounds;
    contact.started = now;
    contact.touching = true;
}

// the event's sound from its field; a class or a slot out of range read as a sound of the world without a slot
inline soundEvent_t BG_UnpackSoundEvent(int packed)
{
    soundEvent_t event;
    const int soundClass = packed & SOUND_EVENT_CLASS_MASK;
    const int slot = (packed >> SOUND_EVENT_SLOT_SHIFT) & SOUND_EVENT_SLOT_MASK;

    if (soundClass <= std::to_underlying(sound::SoundClass::Ambient)) {
        event.soundClass = static_cast<sound::SoundClass>(soundClass);
    }

    if (slot <= std::to_underlying(sound::Slot::Menu)) {
        event.slot = static_cast<sound::Slot>(slot);
    }

    event.loop = (packed & SOUND_EVENT_LOOP) != 0;
    return event;
}

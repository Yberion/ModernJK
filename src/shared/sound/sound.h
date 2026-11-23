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

// sound.h -- what the game's modules see of the sound: the class, slot and place a sound is played with, the request
// that plays it, the handles of the sounds and of the voices playing them, a voice's state for the lip sync, the
// music's states, and Api, the sound's functions that every module receives through its import table.
// Example: a module plays a shot in its entity's weapon slot, which stops the charge playing there:
//     sound::Request shot;
//     shot.sound = fireSound;
//     shot.soundClass = sound::SoundClass::Weapon;
//     shot.entity = cent->currentState.number;
//     shot.slot = sound::Slot::Weapon;
//     api->Play(shot);
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace sound {

// A point, a direction or a velocity in the game's units, x y z as the engines' vec3_t.
using Vector = std::array<float, 3>;

// A registered sound: its index among the registered sounds, kNoSound for a file that was not found.
using SoundHandle = std::int32_t;
inline constexpr SoundHandle kNoSound = 0;

// A playing voice, kept by its caller to move or stop it; None for no voice. A handle is never given twice in a session,
// so that a voice that ended is never confused with a new one.
// NOLINTNEXTLINE(performance-enum-size): a handle's values are not its enumerators: an index and a serial number
enum class VoiceHandle : std::uint64_t { None = 0 };

// What a sound is: it gives the sound's attenuation, its volume (the voices' or the effects') and its priority when
// the voices to mix are chosen.
enum class SoundClass : std::uint8_t {
    Voice,           // a character's voice, the mouth animated
    VoiceQuiet,      // quiet dialogue, heard from near
    VoiceEverywhere, // dialogue heard everywhere at full volume, not positioned
    Announcer,       // the game's announcements, not positioned
    Interface,       // menus, chat, the HUD's cues, not positioned
    Weapon,          // shots, charges, zooms, missiles
    Saber,           // hums, swings, hits, blocks
    Force,           // the force powers
    Vehicle,         // vehicles and loud entities, heard from far
    World,           // debris, movers, effects, speakers, scripted moves
    Body,            // footsteps, landings, rolls, water
    Ambient,         // the ambient sets
};

// Where one sound replaces another: a sound started in a slot stops the one playing in the same slot of the same entity,
// or anywhere for the announcer's and the menus' slots, which are one for the whole game.
enum class Slot : std::uint8_t {
    None,      // plays beside the entity's other sounds
    Voice,     // the entity's voice: a new line or a pain cuts the last one, the lip sync follows it
    Weapon,    // the shot cuts the charge, a weapon change cuts it, a swing cuts the last swing
    Item,      // a speaker's sound
    Announcer, // one announcement at a time
    Menu,      // one menu sound at a time
};

// Where a sound is heard from.
enum class Place : std::uint8_t {
    Entity,   // its entity, wherever UpdateEntity moves it
    Point,    // a fixed point, which MoveLoop moves for a looping sound
    Listener, // the listener's ears: not positioned
};

// A sound to play.
struct Request {
    SoundHandle sound = kNoSound;
    SoundClass soundClass = SoundClass::World;
    Place place = Place::Entity;
    std::optional<int> entity; // the entity it follows (Place::Entity, required), or whose slot it takes (Place::Point)
    Vector origin{};           // the point it is heard from (Place::Point)
    Slot slot = Slot::None;
    bool loop = false;   // played until it is stopped
    float volume = 1.0f; // 1 for the sound as recorded
};

// The ears, set once per frame: an entity's sounds are the listener's own sounds when it is the listener's entity.
struct Listener {
    int entity = 0;
    Vector origin{};
    std::array<Vector, 3> axes{}; // forward, left, up
    Vector velocity{};
    bool underwater = false;
};

// What an entity's voice is doing, from the voice's samples at its playing position.
enum class VoiceState : std::uint8_t {
    NotTalking,
    Silent, // talking, in a silence between words
    Level1, // talking, from the quietest level to the loudest
    Level2,
    Level3,
    Level4,
};

// The dynamic music's states, which the single player's game sets.
enum class MusicState : std::uint8_t {
    Explore,
    Action,
    Boss,
    Death,
    Silence,
};

// The sounds of a brush model's set: when a door or a platform starts, while it moves, when it stops.
enum class BrushModelStage : std::uint8_t {
    Start,
    Middle,
    End,
};

// The sound's functions, as every module receives them.
class Api {
public:
    Api(const Api&) = delete;
    Api& operator=(const Api&) = delete;
    Api(Api&&) = delete;
    Api& operator=(Api&&) = delete;
    virtual ~Api() = default;

    // Loads a sound, decoded whole, and gives its handle; kNoSound and one warning when its file does not exist. A
    // module may register a sound only to load it before it plays, without keeping the handle.
    virtual SoundHandle Register(std::string_view name) = 0;
    // Plays a sound; the handle of the voice playing it, which a looping sound needs to be moved and stopped.
    virtual VoiceHandle Play(const Request& request) = 0;
    // Stops a voice; nothing when it has ended.
    virtual void Stop(VoiceHandle voice) = 0;
    // Whether a voice still plays: a looping sound may have been stopped by the engine (a level change, a cinematic, the
    // sound restarted), which its module then starts again.
    [[nodiscard]] virtual bool IsPlaying(VoiceHandle voice) const = 0;
    // Moves a looping sound heard from a point.
    virtual void MoveLoop(VoiceHandle voice, const Vector& origin, const Vector& velocity) = 0;
    // Stops the sound in an entity's slot, or the announcer's or the menus' whatever the entity.
    virtual void StopSlot(int entity, Slot slot) = 0;
    // Stops every sound of an entity.
    virtual void StopEntity(int entity) = 0;
    // Stops every sound, the music and the ambient sets' loops going on (a level about to change).
    virtual void StopSounds() = 0;
    // Where an entity's sounds are heard from, and how fast it moves (for Doppler).
    virtual void UpdateEntity(int entity, const Vector& origin, const Vector& velocity) = 0;
    // The ears, once per frame.
    virtual void SetListener(const Listener& listener) = 0;
    // What the entity's voice is doing now.
    [[nodiscard]] virtual VoiceState GetVoiceState(int entity) const = 0;
    // A sound's length in milliseconds, 0 for kNoSound.
    [[nodiscard]] virtual int Length(SoundHandle sound) const = 0;

    // Plays a music: an intro, then a loop (the intro again when the loop is empty), or a level's dynamic music.
    virtual void StartMusic(std::string_view intro, std::string_view loop) = 0;
    virtual void StopMusic() = 0;

    // The ambient sets of sound/sound.txt: the list of the level's sets cleared, a set added to it, then the list read.
    virtual void ClearPrecachedSets() = 0;
    virtual void PrecacheSet(std::string_view name) = 0;
    virtual void ParseSets() = 0;
    // The general set played around the listener; called at every frame.
    virtual void UpdateAmbientSet(std::string_view name, const Vector& origin) = 0;
    // A local set played at an entity; called at every frame while it plays, stopped at the first frame it is not.
    virtual void AddLocalSet(std::string_view name, int entity, const Vector& origin) = 0;
    // The sound of a brush model's set at a stage, kNoSound when the set has none.
    [[nodiscard]] virtual SoundHandle BrushModelSound(std::string_view name, BrushModelStage stage) const = 0;

protected:
    Api() = default;
};

} // namespace sound

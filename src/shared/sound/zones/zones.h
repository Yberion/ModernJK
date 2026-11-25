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

// zones.h -- a map's reverb zones, as its file eagle/<map>.efxzones gives them: the environments (each a reverb, in
// the values of EFX's "EAX reverb" effect), the zones' BSP that says which zone holds a point, the apertures between
// environments, the obstacles between zones and the diffraction boxes inside one; the file read and written, the zone
// of a point and a source's obstruction and occlusion computed as Creative's EAX Manager did. The file is written once
// by tools/eal_converter from the game's EAX zone files (eagle/<map>.eal); the game reads it with the map.
// Example: the reverb of the room the listener stands in, and how much a wall between them muffles a source:
//     std::optional<ZoneFile> zones = ParseZoneFile(text, error);
//     const std::optional<int> listenerZone = ZoneOf(*zones, ToZonePoint(listener));
//     const Reverb& reverb = zones->environments[zones->zoneEnvironment[*listenerZone]].reverb;
//     const SourceObstacles muffled = ObstaclesOf(*zones, listenerZone, ToZonePoint(listener), ToZonePoint(source));
#pragma once

#include <shared/sound/sound.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sound {

// A point in the zone file's axes, those of Creative's editor: x as the game's, y up (the game's z), z the game's y.
// The calculations stay in these axes so that they follow EAX Manager's operation by operation.
struct ZonePoint {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    friend bool operator==(const ZonePoint&, const ZonePoint&) = default;
};

// A point of the game in the zone file's axes: the game's y and z swapped, no arithmetic.
[[nodiscard]] constexpr ZonePoint ToZonePoint(const Vector& game) noexcept
{
    return {.x = game[0], .y = game[2], .z = game[1]};
}

// A point of the zone file in the game's axes.
[[nodiscard]] constexpr Vector ToGame(const ZonePoint& point) noexcept
{
    return {point.x, point.z, point.y};
}

// An environment's reverb, in the values and the order of EFX's "EAX reverb" effect (OpenAL Soft's
// EFXEAXREVERBPROPERTIES): its defaults are the effect's.
struct Reverb {
    float density = 1.0f;
    float diffusion = 1.0f;
    float gain = 0.32f;
    float gainHF = 0.89f;
    float gainLF = 1.0f;
    float decayTime = 1.49f;
    float decayHFRatio = 0.83f;
    float decayLFRatio = 1.0f;
    float reflectionsGain = 0.05f;
    float reflectionsDelay = 0.007f;
    Vector reflectionsPan{};
    float lateReverbGain = 1.26f;
    float lateReverbDelay = 0.011f;
    Vector lateReverbPan{};
    float echoTime = 0.25f;
    float echoDepth = 0.0f;
    float modulationTime = 0.25f;
    float modulationDepth = 0.0f;
    float airAbsorptionGainHF = 0.994f;
    float hfReference = 5000.0f;
    float lfReference = 250.0f;
    float roomRolloffFactor = 0.0f;
    bool decayHFLimit = true;

    friend bool operator==(const Reverb&, const Reverb&) = default;
};

// An environment: its name in the editor ("Tie Fighter Hall") and its reverb.
struct Environment {
    std::string name;
    Reverb reverb;

    friend bool operator==(const Environment&, const Environment&) = default;
};

// A child of a plane of the BSP: another plane, a zone, or a leaf in no zone.
enum class ChildKind : std::uint8_t {
    Plane,
    Zone,
    NoZone,
};

struct Child {
    ChildKind kind = ChildKind::NoZone;
    std::uint32_t index = 0; // the plane's or the zone's, for those kinds

    friend bool operator==(const Child&, const Child&) = default;
};

// A plane of the zones' BSP: a point is in front when its distance along the normal from the base point is at least
// -1, behind otherwise.
struct Plane {
    ZonePoint base;
    ZonePoint normal;
    Child front;
    Child back;

    friend bool operator==(const Plane&, const Plane&) = default;
};

// What an obstacle does to the sounds behind it: an obstruction muffles the direct sound alone, an occlusion the direct
// sound and the reverb.
enum class ObstacleKind : std::uint8_t {
    Obstructs,
    Occludes,
};

// An obstacle between two zones: its level at the high frequencies in mB, the share of it at the low frequencies, the
// share of it on the reverb (an occlusion's), its kind.
struct Obstacle {
    std::string name;
    int level = 0;
    float lfRatio = 0.0f;
    float roomRatio = 0.0f;
    ObstacleKind kind = ObstacleKind::Occludes;

    friend bool operator==(const Obstacle&, const Obstacle&) = default;
};

// The obstacle between a listener's zone and a source's: none, the file's default obstacle, or one of its list.
enum class ObstacleRefKind : std::uint8_t {
    None,
    Default,
    Listed,
};

struct ObstacleRef {
    ObstacleRefKind kind = ObstacleRefKind::None;
    std::uint16_t index = 0; // in the list, for Listed

    friend bool operator==(const ObstacleRef&, const ObstacleRef&) = default;
};

// How a diffraction box muffles the direct sound: the most it takes in mB, the share of it at the low frequencies, and
// the angle around the box's edge (0 to 90 degrees) over which it grows to the most.
struct DiffractionModel {
    int maxAttenuation = 0;
    float lfRatio = 0.0f;
    int angleMaxAttenuation = 0;

    friend bool operator==(const DiffractionModel&, const DiffractionModel&) = default;
};

// A box inside a zone (a pillar, a crate) that obstructs a source in the same zone when it stands between the source
// and the listener; the zone is none for a box in no zone. A zone the file does not have is kept (mp/duel6 has three
// such boxes): no source is ever in it, so the box never obstructs, as with EAX Manager.
struct DiffractionBox {
    std::optional<int> zone;
    ZonePoint min;
    ZonePoint max;

    friend bool operator==(const DiffractionBox&, const DiffractionBox&) = default;
};

// An aperture of an environment towards its neighbours (a door, a window): its two ends, both inside the environment.
struct Aperture {
    int environment = 0;
    ZonePoint first;
    ZonePoint second;

    friend bool operator==(const Aperture&, const Aperture&) = default;
};

// A map's zone file, read.
struct ZoneFile {
    std::vector<Environment> environments;
    std::vector<int> zoneEnvironment; // each zone's environment
    Child start;                      // the BSP's root
    std::vector<Plane> planes;
    Obstacle defaultObstacle;
    std::vector<Obstacle> obstacles;
    // the obstacle between a listener's zone (the rows) and a source's (the columns), the leaves in no zone first: (the
    // zones + 1) squared entries, or none when the file has no obstacle at all
    std::vector<ObstacleRef> matrix;
    DiffractionModel diffraction;
    std::vector<DiffractionBox> boxes; // in the file's order: by their zone, which the calculation relies on
    std::vector<Aperture> apertures;   // none for a map played with one reverb, the listener's environment

    friend bool operator==(const ZoneFile&, const ZoneFile&) = default;
};

// What the obstacles do to a source: its obstruction (mB at the high frequencies) and the share of it at the low ones,
// its occlusion (mB) and the shares of it at the low frequencies and on the reverb. The defaults are EAX Manager's for
// a source with no obstacle.
struct SourceObstacles {
    int obstruction = 0;
    float obstructionLF = 0.0f;
    int occlusion = 0;
    float occlusionLF = 0.25f;
    float occlusionRoom = 0.5f;

    friend bool operator==(const SourceObstacles&, const SourceObstacles&) = default;
};

// The file read; nullopt, the reason in error, when it breaks its format or its limits (it comes from the players'
// archives).
[[nodiscard]] std::optional<ZoneFile> ParseZoneFile(std::string_view text, std::string& error);

// The file's text, which ParseZoneFile reads back to the same ZoneFile (every float written exactly).
[[nodiscard]] std::string WriteZoneFile(const ZoneFile& zones);

// The zone that holds a point, nullopt in none (or beyond 32,768 units on an axis, as EAX Manager).
[[nodiscard]] std::optional<int> ZoneOf(const ZoneFile& zones, const ZonePoint& point) noexcept;

// What the obstacles do to a source heard by a listener in a zone: the obstacle between their zones, then the
// diffraction boxes when they are in the same zone, as EAX Manager computed them.
[[nodiscard]] SourceObstacles ObstaclesOf(const ZoneFile& zones, std::optional<int> listenerZone, const ZonePoint& listener, const ZonePoint& source) noexcept;

// The same, the source's zone already found (ZoneOf), so that its BSP is not walked again.
[[nodiscard]] SourceObstacles ObstaclesOf(const ZoneFile& zones, std::optional<int> listenerZone, std::optional<int> sourceZone, const ZonePoint& listener,
                                          const ZonePoint& source) noexcept;

} // namespace sound

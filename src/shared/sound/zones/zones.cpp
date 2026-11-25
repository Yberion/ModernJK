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

// zones.cpp -- the zone file's reader and writer, and the zones' two calculations: the zone of a point, walked down the
// BSP, and a source's obstruction and occlusion, from the obstacle between the listener's zone and the source's, then
// from the diffraction boxes between them in one zone. The calculations follow Creative's EAX Manager as EalMan
// (libs/ealman) rewrote it, operation by operation, in the file's axes, so that tools/eal_converter finds the same
// results to the bit on every file it converts.
// Example: a line of the file, a plane of the BSP whose front child is the plane 12 and whose back child the zone 3:
//     plane 128 0 -64 0 1 0 p12 z3
#include "zones.h"

#include <fmt/format.h>

// LLVM libc's square root, as netcodec takes the library's math, independent of the engines' q_libm.h
#include <shared/libc_common.h>
#include <src/__support/FPUtil/sqrt.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <system_error>
#include <utility>

namespace sound {
namespace {

// the file's first line
constexpr std::string_view kHeader = "efxzones 1";

// the limits a file from the players' archives is held to, far above the game's files (at most 26 environments, 107
// zones, 1,345 planes, 3 obstacles, 73 boxes and 79 apertures), so that a damaged file cannot take much memory
constexpr std::size_t kMaxEnvironments = 256;
constexpr std::size_t kMaxZones = 1024;
constexpr std::size_t kMaxPlanes = 65536;
constexpr std::size_t kMaxObstacles = 256;
constexpr std::size_t kMaxBoxes = 4096;
constexpr std::size_t kMaxApertures = 4096;
constexpr std::size_t kMaxName = 255;

// beyond this distance from the origin on an axis, EAX Manager places a point in no zone
constexpr float kMaxCoordinate = 32768.0f;

// a point is in front of a plane up to this distance behind it, as EAX Manager decides
constexpr float kPlaneThickness = -1.0f;

// the words of a line, taken one after another
class Words {
public:
    explicit Words(std::string_view line) noexcept : rest_(line) {}

    // the next word, empty at the line's end
    [[nodiscard]] std::string_view Next() noexcept
    {
        SkipSpaces();
        std::string_view word = rest_;
        word.remove_suffix(rest_.size() - std::min(rest_.find(' '), rest_.size()));
        rest_.remove_prefix(word.size());
        return word;
    }

    // what is left of the line after the spaces that follow the last word taken: a name
    [[nodiscard]] std::string_view Rest() noexcept
    {
        SkipSpaces();
        return rest_;
    }

    [[nodiscard]] bool AtEnd() noexcept
    {
        SkipSpaces();
        return rest_.empty();
    }

private:
    void SkipSpaces() noexcept
    {
        while (!rest_.empty() && rest_.front() == ' ') {
            rest_.remove_prefix(1);
        }
    }

    std::string_view rest_;
};

// a word read as a finite float, nullopt when it is not one entirely
std::optional<float> ToFloat(std::string_view word) noexcept
{
    float value = 0.0f;
    const auto [end, error] = std::from_chars(word.data(), word.data() + word.size(), value);

    if (word.empty() || error != std::errc{} || end != word.data() + word.size() || !std::isfinite(value)) {
        return std::nullopt;
    }

    return value;
}

// a word read as an integer, nullopt when it is not one entirely
std::optional<int> ToInt(std::string_view word) noexcept
{
    int value = 0;
    const auto [end, error] = std::from_chars(word.data(), word.data() + word.size(), value);

    if (word.empty() || error != std::errc{} || end != word.data() + word.size()) {
        return std::nullopt;
    }

    return value;
}

// a word read as an index below a count, nullopt otherwise
std::optional<std::uint32_t> ToIndex(std::string_view word, std::size_t count) noexcept
{
    const std::optional<int> value = ToInt(word);

    if (!value || *value < 0 || std::cmp_greater_equal(*value, count)) {
        return std::nullopt;
    }

    return static_cast<std::uint32_t>(*value);
}

// the next floats of a line into the places given, false when one is missing or not a float
template <typename... Floats> bool ReadFloats(Words& words, Floats&... values) noexcept
{
    bool ok = true;
    const auto read = [&words, &ok](float& value) {
        const std::optional<float> number = ToFloat(words.Next());
        ok = ok && number.has_value();
        value = number.value_or(0.0f);
    };
    (read(values), ...);
    return ok;
}

// a child of a plane: "p<plane>", "z<zone>" or "-"; its index checked against the counts at the file's end
std::optional<Child> ToChild(std::string_view word) noexcept
{
    if (word == "-") {
        return Child{};
    }

    if (word.size() < 2 || (word.front() != 'p' && word.front() != 'z')) {
        return std::nullopt;
    }

    std::string_view digits = word;
    digits.remove_prefix(1);
    const std::optional<int> index = ToInt(digits);

    if (!index || *index < 0) {
        return std::nullopt;
    }

    return Child{.kind = word.front() == 'p' ? ChildKind::Plane : ChildKind::Zone, .index = static_cast<std::uint32_t>(*index)};
}

// an obstacle's kind: "obstructs" or "occludes"
std::optional<ObstacleKind> ToObstacleKind(std::string_view word) noexcept
{
    if (word == "obstructs") {
        return ObstacleKind::Obstructs;
    }

    if (word == "occludes") {
        return ObstacleKind::Occludes;
    }

    return std::nullopt;
}

// an obstacle's level, shares and kind, then its name when the line has one
bool ReadObstacle(Words& words, Obstacle& obstacle, bool named)
{
    const std::optional<int> level = ToInt(words.Next());
    const bool numbers = ReadFloats(words, obstacle.lfRatio, obstacle.roomRatio);
    const std::optional<ObstacleKind> kind = ToObstacleKind(words.Next());

    if (!level || !numbers || !kind) {
        return false;
    }

    obstacle.level = *level;
    obstacle.kind = *kind;

    if (named) {
        obstacle.name = words.Rest();
        return obstacle.name.size() <= kMaxName;
    }

    return words.AtEnd();
}

// an environment's reverb, its 27 values in EFX's order, then its name
bool ReadEnvironment(Words& words, Environment& environment)
{
    Reverb& r = environment.reverb;
    const bool numbers = ReadFloats(words, r.density, r.diffusion, r.gain, r.gainHF, r.gainLF, r.decayTime, r.decayHFRatio, r.decayLFRatio, r.reflectionsGain,
                                    r.reflectionsDelay, r.reflectionsPan[0], r.reflectionsPan[1], r.reflectionsPan[2], r.lateReverbGain, r.lateReverbDelay,
                                    r.lateReverbPan[0], r.lateReverbPan[1], r.lateReverbPan[2], r.echoTime, r.echoDepth, r.modulationTime, r.modulationDepth,
                                    r.airAbsorptionGainHF, r.hfReference, r.lfReference, r.roomRolloffFactor);
    const std::string_view limit = words.Next();

    if (!numbers || (limit != "0" && limit != "1")) {
        return false;
    }

    r.decayHFLimit = limit == "1";
    environment.name = words.Rest();
    return environment.name.size() <= kMaxName;
}

// a row of the obstacles' matrix: one word per source's zone, "-" none, "d" the default obstacle, else its index
bool ReadRow(Words& words, std::vector<ObstacleRef>& matrix, std::size_t& length)
{
    length = 0;

    for (std::string_view word = words.Next(); !word.empty(); word = words.Next()) {
        ObstacleRef ref;

        if (word == "d") {
            ref.kind = ObstacleRefKind::Default;
        }
        else if (word != "-") {
            const std::optional<std::uint32_t> index = ToIndex(word, kMaxObstacles);

            if (!index) {
                return false;
            }

            ref = {.kind = ObstacleRefKind::Listed, .index = static_cast<std::uint16_t>(*index)};
        }

        if (++length > kMaxZones + 1) {
            return false;
        }

        matrix.push_back(ref);
    }

    return true;
}

// whether a child names a plane or a zone the file has
bool ChildIsValid(const Child& child, std::size_t planes, std::size_t zones) noexcept
{
    switch (child.kind) {
    case ChildKind::Plane:
        return child.index < planes;
    case ChildKind::Zone:
        return child.index < zones;
    case ChildKind::NoZone:
        return true;
    }

    return false;
}

// the references between the parts of a file read: every index within its list, the matrix square
bool IsConsistent(const ZoneFile& zones, const std::vector<std::size_t>& rowLengths, std::string& error)
{
    const std::size_t zoneCount = zones.zoneEnvironment.size();

    if (zones.environments.empty() || zoneCount == 0) {
        error = "no environment or no zone";
        return false;
    }

    for (const int environment : zones.zoneEnvironment) {
        if (static_cast<std::size_t>(environment) >= zones.environments.size()) {
            error = "a zone's environment does not exist";
            return false;
        }
    }

    if (!ChildIsValid(zones.start, zones.planes.size(), zoneCount)) {
        error = "the root names a plane or a zone that does not exist";
        return false;
    }

    for (const Plane& plane : zones.planes) {
        if (!ChildIsValid(plane.front, zones.planes.size(), zoneCount) || !ChildIsValid(plane.back, zones.planes.size(), zoneCount)) {
            error = "a plane names a plane or a zone that does not exist";
            return false;
        }
    }

    if (!rowLengths.empty()) {
        const bool square = rowLengths.size() == zoneCount + 1 && std::ranges::all_of(rowLengths, [zoneCount](std::size_t n) { return n == zoneCount + 1; });

        if (!square) {
            error = "the obstacles' matrix is not one row and one column per zone and for no zone";
            return false;
        }
    }

    for (const ObstacleRef& ref : zones.matrix) {
        if (ref.kind == ObstacleRefKind::Listed && ref.index >= zones.obstacles.size()) {
            error = "the matrix names an obstacle that does not exist";
            return false;
        }
    }

    for (const Aperture& aperture : zones.apertures) {
        if (static_cast<std::size_t>(aperture.environment) >= zones.environments.size()) {
            error = "an aperture's environment does not exist";
            return false;
        }
    }

    return true;
}

// one line of the file into what it describes; false, the reason in error, when it breaks the format or a limit
bool ReadLine(std::string_view line, ZoneFile& zones, std::vector<std::size_t>& rowLengths, bool& startSeen, std::string& error)
{
    Words words(line);
    const std::string_view keyword = words.Next();

    if (keyword.empty()) {
        return true;
    }

    if (keyword == "environment") {
        Environment& environment = zones.environments.emplace_back();

        if (zones.environments.size() > kMaxEnvironments || !ReadEnvironment(words, environment)) {
            error = "an environment's line is wrong or there are too many";
            return false;
        }

        return true;
    }

    if (keyword == "zone") {
        const std::optional<std::uint32_t> environment = ToIndex(words.Next(), kMaxEnvironments);

        if (!environment || !words.AtEnd() || zones.zoneEnvironment.size() >= kMaxZones) {
            error = "a zone's line is wrong or there are too many";
            return false;
        }

        zones.zoneEnvironment.push_back(static_cast<int>(*environment));
        return true;
    }

    if (keyword == "start") {
        const std::optional<Child> start = ToChild(words.Next());

        if (!start || startSeen || !words.AtEnd()) {
            error = "the root's line is wrong or given twice";
            return false;
        }

        zones.start = *start;
        startSeen = true;
        return true;
    }

    if (keyword == "plane") {
        Plane plane;
        const bool numbers = ReadFloats(words, plane.base.x, plane.base.y, plane.base.z, plane.normal.x, plane.normal.y, plane.normal.z);
        const std::optional<Child> front = ToChild(words.Next());
        const std::optional<Child> back = ToChild(words.Next());

        if (!numbers || !front || !back || !words.AtEnd() || zones.planes.size() >= kMaxPlanes) {
            error = "a plane's line is wrong or there are too many";
            return false;
        }

        plane.front = *front;
        plane.back = *back;
        zones.planes.push_back(plane);
        return true;
    }

    if (keyword == "default-obstacle") {
        if (!ReadObstacle(words, zones.defaultObstacle, false)) {
            error = "the default obstacle's line is wrong";
            return false;
        }

        return true;
    }

    if (keyword == "obstacle") {
        Obstacle& obstacle = zones.obstacles.emplace_back();

        if (zones.obstacles.size() > kMaxObstacles || !ReadObstacle(words, obstacle, true)) {
            error = "an obstacle's line is wrong or there are too many";
            return false;
        }

        return true;
    }

    if (keyword == "row") {
        std::size_t length = 0;

        if (rowLengths.size() > kMaxZones || !ReadRow(words, zones.matrix, length)) {
            error = "a row of the obstacles' matrix is wrong or there are too many";
            return false;
        }

        rowLengths.push_back(length);
        return true;
    }

    if (keyword == "diffraction") {
        DiffractionModel& model = zones.diffraction;
        const std::optional<int> most = ToInt(words.Next());
        const bool share = ReadFloats(words, model.lfRatio);
        const std::optional<int> angle = ToInt(words.Next());

        if (!most || !share || !angle || !words.AtEnd()) {
            error = "the diffraction's line is wrong";
            return false;
        }

        model.maxAttenuation = *most;
        model.angleMaxAttenuation = *angle;
        return true;
    }

    if (keyword == "box") {
        DiffractionBox box;
        const std::string_view zone = words.Next();
        const bool numbers = ReadFloats(words, box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z);
        const std::optional<std::uint32_t> index = ToIndex(zone, kMaxZones);

        if ((zone != "-" && !index) || !numbers || !words.AtEnd() || zones.boxes.size() >= kMaxBoxes) {
            error = "a box's line is wrong or there are too many";
            return false;
        }

        if (index) {
            box.zone = static_cast<int>(*index);
        }

        zones.boxes.push_back(box);
        return true;
    }

    if (keyword == "aperture") {
        Aperture aperture;
        const std::optional<std::uint32_t> environment = ToIndex(words.Next(), kMaxEnvironments);
        const bool numbers = ReadFloats(words, aperture.first.x, aperture.first.y, aperture.first.z, aperture.second.x, aperture.second.y, aperture.second.z);

        if (!environment || !numbers || !words.AtEnd() || zones.apertures.size() >= kMaxApertures) {
            error = "an aperture's line is wrong or there are too many";
            return false;
        }

        aperture.environment = static_cast<int>(*environment);
        zones.apertures.push_back(aperture);
        return true;
    }

    error = fmt::format("unknown line \"{}\"", keyword);
    return false;
}

// a child as the file writes it
std::string ChildText(const Child& child)
{
    switch (child.kind) {
    case ChildKind::Plane:
        return fmt::format("p{}", child.index);
    case ChildKind::Zone:
        return fmt::format("z{}", child.index);
    case ChildKind::NoZone:
        return "-";
    }

    return "-";
}

// an obstacle's kind as the file writes it
std::string_view KindText(ObstacleKind kind) noexcept
{
    return kind == ObstacleKind::Obstructs ? "obstructs" : "occludes";
}

// the order EAX Manager gives the zones in its lists of boxes: the leaves in no zone first
int Order(std::optional<int> zone) noexcept
{
    return zone.value_or(-1);
}

// whether a point is in front of a plane, as EAX Manager decides it (the product's terms summed in its order)
bool IsInFront(const ZonePoint& point, const Plane& plane) noexcept
{
    const float dx = point.x - plane.base.x;
    const float dy = point.y - plane.base.y;
    const float dz = point.z - plane.base.z;
    return (dx * plane.normal.x) + (dy * plane.normal.y) + (dz * plane.normal.z) >= kPlaneThickness;
}

// whether a point lies within EAX Manager's bounds on every axis
bool IsInBounds(const ZonePoint& point) noexcept
{
    const auto within = [](float v) { return v >= -kMaxCoordinate && v <= kMaxCoordinate; };
    return within(point.x) && within(point.y) && within(point.z);
}

// the zone of a point within the bounds, walked down the BSP; nullopt for a leaf in no zone, or a walk longer than the
// planes (a cycle a damaged file could hold)
std::optional<int> WalkTree(const ZoneFile& zones, const ZonePoint& point) noexcept
{
    Child child = zones.start;

    for (std::size_t steps = 0; child.kind == ChildKind::Plane; ++steps) {
        if (steps >= zones.planes.size()) {
            return std::nullopt;
        }

        const Plane& plane = zones.planes[child.index];
        child = IsInFront(point, plane) ? plane.front : plane.back;
    }

    if (child.kind == ChildKind::Zone) {
        return static_cast<int>(child.index);
    }

    return std::nullopt;
}

// a vector divided by its length when that length is finite and not zero, as EAX Manager normalises
ZonePoint Normalized(ZonePoint v) noexcept
{
    const float length = LIBC_NAMESPACE::fputil::sqrt<float>((v.x * v.x) + (v.y * v.y) + (v.z * v.z));

    if (std::isfinite(length) && length != 0.0f) {
        v.x /= length;
        v.y /= length;
        v.z /= length;
    }

    return v;
}

// a box's side a source lies beyond, and where the line from it to the listener crosses that side's plane
struct Crossing {
    int side = 0; // 0 and 1 beyond the box in y (the game's height), 2 and 3 in z, 4 and 5 in x: EAX Manager's numbers
    ZonePoint base;
    ZonePoint normal;
};

// the sides of a box a point lies beyond, as EAX Manager's six bits
constexpr unsigned kAboveZ = 1U;
constexpr unsigned kBelowZ = 2U;
constexpr unsigned kAboveY = 4U;
constexpr unsigned kBelowY = 8U;
constexpr unsigned kAboveX = 16U;
constexpr unsigned kBelowX = 32U;

unsigned Beyond(const DiffractionBox& box, const ZonePoint& point) noexcept
{
    unsigned bits = 0;
    const auto axis = [&bits](float value, float min, float max, unsigned above, unsigned below) {
        if (min <= value) {
            if (max < value) {
                bits |= above;
            }
        }
        else {
            bits |= below;
        }
    };
    axis(point.x, box.min.x, box.max.x, kAboveX, kBelowX);
    axis(point.y, box.min.y, box.max.y, kAboveY, kBelowY);
    axis(point.z, box.min.z, box.max.z, kAboveZ, kBelowZ);
    return bits;
}

// where the line from a to b crosses a plane, nullopt when the crossing is not finite (the line along the plane)
std::optional<ZonePoint> Intersection(const ZonePoint& a, const ZonePoint& b, const ZonePoint& base, const ZonePoint& normal) noexcept
{
    const float denominator = ((b.x - a.x) * normal.x) + ((b.y - a.y) * normal.y) + ((b.z - a.z) * normal.z);
    const float partB = (((base.x - a.x) * normal.x) + ((base.y - a.y) * normal.y) + ((base.z - a.z) * normal.z)) / denominator;
    const float partA = 1.0f - partB;

    if (!std::isfinite(partB)) {
        return std::nullopt;
    }

    return ZonePoint{.x = (partA * a.x) + (partB * b.x), .y = (partA * a.y) + (partB * b.y), .z = (partA * a.z) + (partB * b.z)};
}

// whether a crossing of a side's plane lies on the box's face
bool IsOnFace(const ZonePoint& p, int side, const DiffractionBox& box) noexcept
{
    const auto within = [](float v, float min, float max) { return v >= min && v <= max; };

    switch (side) {
    case 0:
    case 1:
        return within(p.x, box.min.x, box.max.x) && within(p.y, box.min.y, box.max.y);
    case 2:
    case 3:
        return within(p.x, box.min.x, box.max.x) && within(p.z, box.min.z, box.max.z);
    case 4:
    case 5:
        return within(p.y, box.min.y, box.max.y) && within(p.z, box.min.z, box.max.z);
    default:
        return false;
    }
}

// the obstruction of a source by a box between it and the listener, from the angle the sound turns around the box's
// nearest edge, as EAX Manager computes it (its tests of the listener's place on the edge's axes kept as they are)
int Diffraction(const DiffractionModel& model, const ZonePoint& listener, const ZonePoint& source, const ZonePoint& crossing, int side,
                const DiffractionBox& box) noexcept
{
    ZonePoint edge = crossing;
    const auto nearest = [](float value, float min, float max) { return value >= (min + max) * 0.5f ? min : max; };

    switch (side) {
    case 0:
    case 1:
        if (listener.y >= box.min.y || listener.y <= box.max.y) {
            edge.y = nearest(crossing.y, box.min.y, box.max.y);
        }
        else {
            edge.x = nearest(crossing.x, box.min.x, box.max.x);
        }

        break;
    case 2:
    case 3:
        if (listener.z < box.min.z && listener.z > box.max.z) {
            edge.x = nearest(crossing.x, box.min.x, box.max.x);
        }
        else {
            edge.z = crossing.z < (box.min.z + box.max.z) * 0.5f ? box.max.z : box.min.z;
        }

        break;
    case 4:
    case 5:
        if (listener.y >= box.min.y || listener.y <= box.max.y) {
            edge.y = nearest(crossing.y, box.min.y, box.max.y);
        }
        else {
            edge.z = crossing.z >= (box.min.z + box.max.z) * 0.5f ? box.max.z : box.min.z;
        }

        break;
    default:
        return 0;
    }

    const ZonePoint toListener = Normalized({.x = listener.x - edge.x, .y = listener.y - edge.y, .z = listener.z - edge.z});
    const ZonePoint toSource = Normalized({.x = source.x - edge.x, .y = source.y - edge.y, .z = source.z - edge.z});
    const float dot = (toListener.x * toSource.x) + (toListener.y * toSource.y) + (toListener.z * toSource.z);

    // beyond a right angle around the edge, the obstruction grows with the angle up to the model's angle
    if (dot < 0.0f) {
        const float angle = dot + 1.0f;
        const float angleMax = static_cast<float>(model.angleMaxAttenuation) / 90.0f;

        if (angle < angleMax) {
            const float level = static_cast<float>(model.maxAttenuation) * angle / angleMax;
            constexpr auto kLowest = static_cast<float>(std::numeric_limits<int>::min());
            constexpr auto kHighest = static_cast<float>(std::numeric_limits<int>::max());

            if (!std::isfinite(level) || level < kLowest || level > kHighest) {
                return std::numeric_limits<int>::min();
            }

            return static_cast<int>(level);
        }
    }

    return model.maxAttenuation;
}

// the obstruction of the boxes of the listener's and the source's zone that stand between them, 0 for none: the boxes
// of that zone taken in the file's order, the first a line crosses deciding, as EAX Manager does
int BoxesObstruction(const ZoneFile& zones, int zone, const ZonePoint& listener, const ZonePoint& source) noexcept
{
    const std::size_t count = zones.boxes.size();
    std::size_t first = 0;

    // the first box of the zone; none when the boxes, sorted by zone, pass it
    for (std::size_t i = 0; i < count; ++i) {
        first = i;
        const int boxZone = Order(zones.boxes[i].zone);

        if (boxZone == zone) {
            break;
        }

        if (zone <= boxZone) {
            return 0;
        }

        first = i + 1;
    }

    int obstruction = 0;

    for (std::size_t i = first; i < count && Order(zones.boxes[i].zone) == zone; ++i) {
        const DiffractionBox& box = zones.boxes[i];
        const unsigned listenerSides = Beyond(box, listener);
        const unsigned sourceSides = Beyond(box, source);

        // the listener and the source beyond one same side: the box is not between them
        if ((listenerSides & sourceSides) != 0) {
            continue;
        }

        std::array<Crossing, 3> crossings{};
        std::size_t crossingCount = 0;
        const auto add = [&crossings, &crossingCount](int side, const ZonePoint& base, const ZonePoint& normal) {
            crossings[crossingCount++] = {.side = side, .base = base, .normal = normal};
        };

        if ((sourceSides & kBelowX) != 0) {
            add(4, box.min, {.x = -1.0f});
        }

        if ((sourceSides & kAboveX) != 0) {
            add(5, box.max, {.x = 1.0f});
        }

        if ((sourceSides & kBelowY) != 0) {
            add(2, box.min, {.y = -1.0f});
        }

        if ((sourceSides & kAboveY) != 0) {
            add(3, box.max, {.y = 1.0f});
        }

        if ((sourceSides & kBelowZ) != 0) {
            add(0, box.min, {.z = -1.0f});
        }

        if ((sourceSides & kAboveZ) != 0) {
            add(1, box.max, {.z = 1.0f});
        }

        for (std::size_t k = 0; k < crossingCount; ++k) {
            const std::optional<ZonePoint> crossing = Intersection(source, listener, crossings[k].base, crossings[k].normal);

            if (crossing && IsOnFace(*crossing, crossings[k].side, box)) {
                obstruction = std::min(obstruction, Diffraction(zones.diffraction, listener, source, *crossing, crossings[k].side, box));
                break;
            }
        }

        if (obstruction < zones.diffraction.maxAttenuation) {
            return zones.diffraction.maxAttenuation;
        }
    }

    return obstruction;
}

} // namespace

std::optional<ZoneFile> ParseZoneFile(std::string_view text, std::string& error)
{
    ZoneFile zones;
    std::vector<std::size_t> rowLengths;
    bool startSeen = false;
    bool headerSeen = false;

    while (!text.empty()) {
        const std::size_t end = text.find('\n');
        std::string_view line = text.substr(0, end);
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);

        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }

        if (!headerSeen) {
            if (line != kHeader) {
                error = "not a zone file of this version";
                return std::nullopt;
            }

            headerSeen = true;
            continue;
        }

        if (!ReadLine(line, zones, rowLengths, startSeen, error)) {
            return std::nullopt;
        }
    }

    if (!headerSeen || !startSeen) {
        error = headerSeen ? "no root" : "empty file";
        return std::nullopt;
    }

    if (!IsConsistent(zones, rowLengths, error)) {
        return std::nullopt;
    }

    // the boxes in the order the obstruction's search takes them, by zone, a file written by hand giving them in another
    std::ranges::stable_sort(zones.boxes, {}, [](const DiffractionBox& box) { return Order(box.zone); });
    return zones;
}

std::string WriteZoneFile(const ZoneFile& zones)
{
    std::string text = fmt::format("{}\n", kHeader);
    const auto out = std::back_inserter(text);

    for (const Environment& environment : zones.environments) {
        const Reverb& r = environment.reverb;
        fmt::format_to(out, "environment {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}\n", r.density, r.diffusion, r.gain,
                       r.gainHF, r.gainLF, r.decayTime, r.decayHFRatio, r.decayLFRatio, r.reflectionsGain, r.reflectionsDelay, r.reflectionsPan[0],
                       r.reflectionsPan[1], r.reflectionsPan[2], r.lateReverbGain, r.lateReverbDelay, r.lateReverbPan[0], r.lateReverbPan[1],
                       r.lateReverbPan[2], r.echoTime, r.echoDepth, r.modulationTime, r.modulationDepth, r.airAbsorptionGainHF, r.hfReference, r.lfReference,
                       r.roomRolloffFactor, r.decayHFLimit ? 1 : 0, environment.name);
    }

    for (const int environment : zones.zoneEnvironment) {
        fmt::format_to(out, "zone {}\n", environment);
    }

    fmt::format_to(out, "start {}\n", ChildText(zones.start));

    for (const Plane& p : zones.planes) {
        fmt::format_to(out, "plane {} {} {} {} {} {} {} {}\n", p.base.x, p.base.y, p.base.z, p.normal.x, p.normal.y, p.normal.z, ChildText(p.front),
                       ChildText(p.back));
    }

    const Obstacle& d = zones.defaultObstacle;
    fmt::format_to(out, "default-obstacle {} {} {} {}\n", d.level, d.lfRatio, d.roomRatio, KindText(d.kind));

    for (const Obstacle& o : zones.obstacles) {
        fmt::format_to(out, "obstacle {} {} {} {} {}\n", o.level, o.lfRatio, o.roomRatio, KindText(o.kind), o.name);
    }

    const std::size_t width = zones.zoneEnvironment.size() + 1;

    for (std::size_t row = 0; width != 0 && row * width < zones.matrix.size(); ++row) {
        text += "row";

        for (std::size_t column = 0; column < width; ++column) {
            const ObstacleRef& ref = zones.matrix[(row * width) + column];

            switch (ref.kind) {
            case ObstacleRefKind::None:
                text += " -";
                break;
            case ObstacleRefKind::Default:
                text += " d";
                break;
            case ObstacleRefKind::Listed:
                fmt::format_to(out, " {}", ref.index);
                break;
            }
        }

        text += '\n';
    }

    const DiffractionModel& m = zones.diffraction;
    fmt::format_to(out, "diffraction {} {} {}\n", m.maxAttenuation, m.lfRatio, m.angleMaxAttenuation);

    for (const DiffractionBox& b : zones.boxes) {
        const std::string zone = b.zone ? fmt::format("{}", *b.zone) : "-";
        fmt::format_to(out, "box {} {} {} {} {} {} {}\n", zone, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z);
    }

    for (const Aperture& a : zones.apertures) {
        fmt::format_to(out, "aperture {} {} {} {} {} {} {}\n", a.environment, a.first.x, a.first.y, a.first.z, a.second.x, a.second.y, a.second.z);
    }

    return text;
}

std::optional<int> ZoneOf(const ZoneFile& zones, const ZonePoint& point) noexcept
{
    if (!IsInBounds(point)) {
        return std::nullopt;
    }

    return WalkTree(zones, point);
}

SourceObstacles ObstaclesOf(const ZoneFile& zones, std::optional<int> listenerZone, const ZonePoint& listener, const ZonePoint& source) noexcept
{
    return ObstaclesOf(zones, listenerZone, ZoneOf(zones, source), listener, source);
}

SourceObstacles ObstaclesOf(const ZoneFile& zones, std::optional<int> listenerZone, std::optional<int> sourceZone, const ZonePoint& listener,
                            const ZonePoint& source) noexcept
{
    SourceObstacles result;

    if (!IsInBounds(source)) {
        return result;
    }

    const int listenerIndex = Order(listenerZone);
    const int sourceIndex = Order(sourceZone);

    // the obstacle between the two zones, the leaves in no zone in the first row and column
    if (!zones.matrix.empty()) {
        const std::size_t width = zones.zoneEnvironment.size() + 1;
        const ObstacleRef ref = zones.matrix[(width * static_cast<std::size_t>(listenerIndex + 1)) + static_cast<std::size_t>(sourceIndex + 1)];
        const Obstacle* obstacle = nullptr;

        if (ref.kind == ObstacleRefKind::Listed) {
            obstacle = &zones.obstacles[ref.index];
        }
        else if (ref.kind == ObstacleRefKind::Default) {
            obstacle = &zones.defaultObstacle;
        }

        if (obstacle != nullptr && obstacle->kind == ObstacleKind::Obstructs) {
            result.obstruction = obstacle->level;
            result.obstructionLF = obstacle->lfRatio;
        }
        else if (obstacle != nullptr) {
            result.occlusion = obstacle->level;
            result.occlusionLF = obstacle->lfRatio;
            result.occlusionRoom = obstacle->roomRatio;
        }
    }

    // in one zone, the diffraction boxes between them
    if (listenerIndex == sourceIndex) {
        const int obstruction = BoxesObstruction(zones, sourceIndex, listener, source);

        if (obstruction < 0) {
            result.obstruction = std::min(result.obstruction, obstruction);
            result.obstructionLF = std::max(result.obstructionLF, zones.diffraction.lfRatio);
        }
    }

    return result;
}

} // namespace sound

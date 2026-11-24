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

// eal_converter.cpp -- converts the game's EAX zone files (eagle/<map>.eal, read by EalMan, libs/ealman) into the
// files the sound reads (eagle/<map>.efxzones, src/shared/sound/zones): the environments in the values of EFX's "EAX
// reverb" effect, by the conversion OpenAL Soft's own EAX layer makes; the zones' BSP, the obstacles and the diffraction
// boxes as the file holds them, but a box whose zone is not the one holding its centre given that zone; the apertures as
// the original engine took them (its LoadEALFile), with one repair: an environment may lack an aperture number the others
// have. Every file written is read back and checked against EalMan (its data repaired the same way): the environment of
// every point of a grid, and the obstruction and occlusion of random pairs of a listener and a source, around every
// diffraction box too, must be the same to the bit.
// Example: python -I tools/eal_converter/extract.py <game>/base <eal folder>, then
//     eal_converter <eal folder>/eagle <repository>/assets/jka/eagle
#include <shared/sound/zones/zones.h>

#include <EalMan.h>

#include <fmt/format.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;

using sound::ZoneFile;
using sound::ZonePoint;

// EAX Manager's flag in a BSP child that marks a leaf
constexpr std::uint32_t kLeaf = 0x80000000U;

// EAX's flag of a reverb whose high frequencies decay no slower than the air absorbs them
constexpr std::uint32_t kDecayHFLimit = 0x20U;

// the grid's points per axis, the random pairs per file and around each box
constexpr int kGridSteps = 48;
constexpr int kPairs = 200000;
constexpr int kPairsPerBox = 4000;

// how far around the BSP's planes and the boxes the points are taken
constexpr float kMargin = 256.0f;

// EAX Manager's bounds on each axis
constexpr float kBound = 32768.0f;

// a level in mB as a gain, as OpenAL Soft's level_mb_to_gain
float LevelToGain(float level)
{
    if (level <= -10000.0f) {
        return 0.0f;
    }

    return std::pow(10.0f, level / 2000.0f);
}

ZonePoint ToPoint(const EMPoint& p)
{
    return {.x = p.fX, .y = p.fY, .z = p.fZ};
}

EMPoint ToEM(const ZonePoint& p)
{
    return {.fX = p.x, .fY = p.y, .fZ = p.z};
}

// an environment's EAX values as EFX's "EAX reverb", as OpenAL Soft's EaxReverbCommitter::commit converts them
sound::Reverb ToReverb(const EAXListenerProperties& e)
{
    const float size = e.flEnvironmentSize;
    return {.density = std::min(size * size * size / 16.0f, 1.0f),
            .diffusion = e.flEnvironmentDiffusion,
            .gain = LevelToGain(static_cast<float>(e.lRoom)),
            .gainHF = LevelToGain(static_cast<float>(e.lRoomHF)),
            .gainLF = LevelToGain(static_cast<float>(e.lRoomLF)),
            .decayTime = e.flDecayTime,
            .decayHFRatio = e.flDecayHFRatio,
            .decayLFRatio = e.flDecayLFRatio,
            .reflectionsGain = LevelToGain(static_cast<float>(e.lReflections)),
            .reflectionsDelay = e.flReflectionsDelay,
            .reflectionsPan = {e.vReflectionsPan.fX, e.vReflectionsPan.fY, e.vReflectionsPan.fZ},
            .lateReverbGain = LevelToGain(static_cast<float>(e.lReverb)),
            .lateReverbDelay = e.flReverbDelay,
            .lateReverbPan = {e.vReverbPan.fX, e.vReverbPan.fY, e.vReverbPan.fZ},
            .echoTime = e.flEchoTime,
            .echoDepth = e.flEchoDepth,
            .modulationTime = e.flModulationTime,
            .modulationDepth = e.flModulationDepth,
            .airAbsorptionGainHF = LevelToGain(e.flAirAbsorptionHF),
            .hfReference = e.flHFReference,
            .lfReference = e.flLFReference,
            .roomRolloffFactor = e.flRoomRolloffFactor,
            .decayHFLimit = (e.ulFlags & kDecayHFLimit) != 0};
}

sound::Obstacle ToObstacle(const MaterialAttributes& m, std::string name)
{
    return {.name = std::move(name),
            .level = m.lLevel,
            .lfRatio = m.fLFRatio,
            .roomRatio = m.fRoomRatio,
            .kind = m.dwFlags == EMMATERIAL_OBSTRUCTS ? sound::ObstacleKind::Obstructs : sound::ObstacleKind::Occludes};
}

// a BSP child of EAX Manager's data: a plane, or a leaf whose zone may be none
sound::Child ToChild(const EalData& data, std::uint32_t child)
{
    if ((child & kLeaf) == 0) {
        return {.kind = sound::ChildKind::Plane, .index = child};
    }

    const int zone = data.gemaBSPouterNodes[child & ~kLeaf].indexEnvID;

    if (zone < 0) {
        return {};
    }

    return {.kind = sound::ChildKind::Zone, .index = static_cast<std::uint32_t>(zone)};
}

// the instances of a source named in the editor, none when the name is not there
std::vector<EMPoint> Instances(const EalMan& eal, const std::string& name)
{
    std::vector<EMPoint> points;
    int32_t id = 0;
    int32_t count = 0;

    if (eal.GetSourceID(name, id) != 0 || eal.GetSourceNumInstances(id, count) != 0) {
        return points;
    }

    for (int32_t i = 0; i < count; ++i) {
        EMPoint p{};
        eal.GetSourceInstancePos(id, i, p);
        points.push_back(p);
    }

    return points;
}

// the environment that holds a point, as EAX Manager answers the engine (-1 for none)
int EnvironmentAt(EalMan& eal, const EMPoint& p)
{
    int32_t environment = -1;

    if (eal.GetListenerDynamicAttributes(0, p, environment, 0) != 0) {
        return -2;
    }

    return environment;
}

// the apertures as the engine's LoadEALFile took them: the environments numbered by the "Center" sources, each aperture
// number n a pair "Aperture<n>a" and "Aperture<n>b" with at most one of each in an environment; the one change, a
// repair: an environment may lack a number the others have (the engine refused the file then, playing one reverb)
std::vector<sound::Aperture> Apertures(EalMan& eal, std::string& report)
{
    const std::vector<EMPoint> centres = Instances(eal, "Center");

    if (centres.empty()) {
        report = "one reverb: no centres";
        return {};
    }

    const int environments = static_cast<int>(centres.size());
    std::vector<std::vector<sound::Aperture>> byEnvironment(centres.size());
    std::string strict; // the first of the engine's count checks the file breaks

    for (int number = 0;; ++number) {
        const std::vector<EMPoint> a = Instances(eal, fmt::format("Aperture{}a", number));
        const std::vector<EMPoint> b = Instances(eal, fmt::format("Aperture{}b", number));

        if (a.empty()) {
            break;
        }

        if (strict.empty() && number == 0 && (std::ssize(a) != environments || std::ssize(b) != environments)) {
            strict = fmt::format("{} Aperture0a and {} Aperture0b for {} centres", a.size(), b.size(), environments);
        }

        if (strict.empty() && a.size() != b.size()) {
            strict = fmt::format("{} Aperture{}a and {} Aperture{}b", a.size(), number, b.size(), number);
        }

        std::vector<std::optional<EMPoint>> ends[2];
        ends[0].resize(centres.size());
        ends[1].resize(centres.size());
        const std::vector<EMPoint>* sides[2] = {&a, &b};

        for (int side = 0; side < 2; ++side) {
            for (const EMPoint& p : *sides[side]) {
                const int environment = EnvironmentAt(eal, p);

                if (environment < 0 || environment >= environments) {
                    report = fmt::format("one reverb: Aperture{}{} at {} {} {} in the environment {} ({} centres)", number, side == 0 ? 'a' : 'b', p.fX, p.fY,
                                         p.fZ, environment, environments);
                    return {};
                }

                std::optional<EMPoint>& end = ends[side][static_cast<std::size_t>(environment)];

                if (end) {
                    report = fmt::format("one reverb: two Aperture{}{} in the environment {}", number, side == 0 ? 'a' : 'b', environment);
                    return {};
                }

                end = p;
            }
        }

        for (std::size_t environment = 0; environment < centres.size(); ++environment) {
            if (ends[0][environment].has_value() != ends[1][environment].has_value()) {
                report = fmt::format("one reverb: the environment {} has one end of Aperture{}", environment, number);
                return {};
            }

            if (ends[0][environment]) {
                byEnvironment[environment].push_back(
                    {.environment = static_cast<int>(environment), .first = ToPoint(*ends[0][environment]), .second = ToPoint(*ends[1][environment])});
            }
        }
    }

    std::vector<sound::Aperture> apertures;

    for (const std::vector<sound::Aperture>& list : byEnvironment) {
        apertures.insert(apertures.end(), list.begin(), list.end());
    }

    if (apertures.empty()) {
        report = "one reverb: no apertures";
        return {};
    }

    report = fmt::format("neighbours: {} environments, {} apertures", environments, apertures.size());

    if (!strict.empty()) {
        report += fmt::format(" (repaired: the original engine refused the file, {})", strict);
    }

    return apertures;
}

// the zone file of an .eal that EalMan read, and the index given to the file's default environment when a zone plays it
// (EAX Manager's environment -1: the original engine played that environment there); nullopt, the reason in error, when
// the file holds what the sound's file cannot (a zone in no environment at all)
std::optional<ZoneFile> Convert(EalMan& eal, std::optional<int>& defaultEnvironment, std::string& report, std::string& error)
{
    const EalData& data = eal.GetData();
    ZoneFile zones;

    for (std::size_t i = 0; i < data.environments.size(); ++i) {
        zones.environments.push_back({.name = data.envNames[i], .reverb = ToReverb(data.environments[i])});
    }

    for (const int32_t environment : data.gemaEnvIDs) {
        if (environment == static_cast<int32_t>(EMFLAG_IDDEFAULT)) {
            if (!defaultEnvironment) {
                defaultEnvironment = static_cast<int>(zones.environments.size());
                zones.environments.push_back({.name = "Default", .reverb = ToReverb(data.defEnvironment)});
            }

            zones.zoneEnvironment.push_back(*defaultEnvironment);
            continue;
        }

        if (environment < 0 || static_cast<std::size_t>(environment) >= data.environments.size()) {
            error = fmt::format("a zone in the environment {}", environment);
            return std::nullopt;
        }

        zones.zoneEnvironment.push_back(environment);
    }

    zones.start = ToChild(data, data.gemaStartID);

    for (const SplitNode& node : data.gemaBSPinnerNodes) {
        zones.planes.push_back(
            {.base = ToPoint(node.base), .normal = ToPoint(node.normal), .front = ToChild(data, node.childFront), .back = ToChild(data, node.childBack)});
    }

    zones.defaultObstacle = ToObstacle(data.defObstacle, "");

    for (std::size_t i = 0; i < data.obstacles.size(); ++i) {
        zones.obstacles.push_back(ToObstacle(data.obstacles[i], data.obsNames[i]));
    }

    bool anyObstacle = false;

    for (const int32_t entry : data.gemaEnvironmentMatrix) {
        sound::ObstacleRef ref;

        if (entry == -1) {
            ref.kind = sound::ObstacleRefKind::Default;
        }
        else if (entry >= 0) {
            ref = {.kind = sound::ObstacleRefKind::Listed, .index = static_cast<std::uint16_t>(entry)};
        }

        anyObstacle = anyObstacle || ref.kind != sound::ObstacleRefKind::None;
        zones.matrix.push_back(ref);
    }

    if (!anyObstacle) {
        zones.matrix.clear();
    }

    zones.diffraction = {.maxAttenuation = data.gdfm.MaxAttenuation, .lfRatio = data.gdfm.LFRatio, .angleMaxAttenuation = data.gdfm.AngleMaxAttenuation};

    for (const DiffractionBox& box : data.gemaDiffBox) {
        sound::DiffractionBox converted{.zone = std::nullopt, .min = ToPoint(box.empMin), .max = ToPoint(box.empMax)};

        if (box.lSubspaceID >= 0) {
            converted.zone = box.lSubspaceID;
        }

        zones.boxes.push_back(converted);
    }

    zones.apertures = Apertures(eal, report);
    return zones;
}

// a file EalMan refuses but whose one environment it read: that environment over the whole map
std::optional<ZoneFile> OneEnvironment(const EalMan& eal)
{
    const EalData& data = eal.GetData();

    if (data.environments.size() != 1 || data.envNames.size() != 1) {
        return std::nullopt;
    }

    ZoneFile zones;
    zones.environments.push_back({.name = data.envNames[0], .reverb = ToReverb(data.environments[0])});
    zones.zoneEnvironment.push_back(0);
    zones.start = {.kind = sound::ChildKind::Zone, .index = 0};
    return zones;
}

bool SameBits(float a, float b)
{
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}

// the box that holds the BSP's base points, widened by the margin and kept within EAX Manager's bounds
std::pair<ZonePoint, ZonePoint> Bounds(const ZoneFile& zones)
{
    ZonePoint low{.x = kBound, .y = kBound, .z = kBound};
    ZonePoint high{.x = -kBound, .y = -kBound, .z = -kBound};
    const auto add = [&low, &high](const ZonePoint& p) {
        low = {.x = std::min(low.x, p.x), .y = std::min(low.y, p.y), .z = std::min(low.z, p.z)};
        high = {.x = std::max(high.x, p.x), .y = std::max(high.y, p.y), .z = std::max(high.z, p.z)};
    };

    for (const sound::Plane& plane : zones.planes) {
        add(plane.base);
    }

    const auto clamp = [](float v) { return std::clamp(v, -kBound, kBound); };
    return {{.x = clamp(low.x - kMargin), .y = clamp(low.y - kMargin), .z = clamp(low.z - kMargin)},
            {.x = clamp(high.x + kMargin), .y = clamp(high.y + kMargin), .z = clamp(high.z + kMargin)}};
}

// what a check counted: the points whose environment was compared, the pairs of a listener and a source whose obstacles
// were compared, those of them obstructed and occluded (so that the check is seen to reach the obstacles), the
// differences
struct Counts {
    int points = 0;
    int pairs = 0;
    int obstructed = 0;
    int occluded = 0;
    int differences = 0;
};

// the zone file against EalMan: every point's environment, then the obstacles of random pairs and of pairs around each
// box
Counts Check(EalMan& eal, const ZoneFile& zones, std::optional<int> defaultEnvironment)
{
    const auto [low, high] = Bounds(zones);
    Counts counts;
    int& differences = counts.differences;
    std::vector<ZonePoint> grid;

    for (int i = 0; i <= kGridSteps; ++i) {
        for (int j = 0; j <= kGridSteps; ++j) {
            for (int k = 0; k <= kGridSteps; ++k) {
                const auto at = [](float a, float b, int step) { return a + ((b - a) * static_cast<float>(step) / static_cast<float>(kGridSteps)); };
                grid.push_back({.x = at(low.x, high.x, i), .y = at(low.y, high.y, j), .z = at(low.z, high.z, k)});
            }
        }
    }

    // the planes' base points and a unit off them on each side, where the thickness of a plane decides
    for (const sound::Plane& plane : zones.planes) {
        for (const float off : {-1.0f, 0.0f, 1.0f}) {
            grid.push_back(
                {.x = plane.base.x + (off * plane.normal.x), .y = plane.base.y + (off * plane.normal.y), .z = plane.base.z + (off * plane.normal.z)});
        }
    }

    // the points beyond EAX Manager's bounds (around a plane far out) left out: it answers no environment for them
    std::erase_if(grid, [](const ZonePoint& p) { return std::abs(p.x) > kBound || std::abs(p.y) > kBound || std::abs(p.z) > kBound; });

    for (const ZonePoint& p : grid) {
        const std::optional<int> zone = sound::ZoneOf(zones, p);
        int ours = zone ? zones.zoneEnvironment[static_cast<std::size_t>(*zone)] : -1;

        if (defaultEnvironment && ours == *defaultEnvironment) {
            ours = -1;
        }

        if (ours != EnvironmentAt(eal, ToEM(p)) && ++differences <= 5) {
            fmt::println("  environment differs at {} {} {}", p.x, p.y, p.z);
        }
    }

    counts.points = static_cast<int>(grid.size());
    std::mt19937 random(12345U);
    const auto pick = [&random](const ZonePoint& a, const ZonePoint& b) {
        std::uniform_real_distribution<float> x(a.x, b.x);
        std::uniform_real_distribution<float> y(a.y, b.y);
        std::uniform_real_distribution<float> z(a.z, b.z);
        return ZonePoint{.x = x(random), .y = y(random), .z = z(random)};
    };
    const auto compare = [&eal, &zones, &counts, &differences](const ZonePoint& listener, const ZonePoint& source) {
        int32_t environment = 0;
        eal.GetListenerDynamicAttributes(0, ToEM(listener), environment, EMFLAG_LOCKPOSITION);
        sound::SourceObstacles theirs;
        EMPoint virtualPosition{};
        eal.GetSourceDynamicAttributes(0, ToEM(source), theirs.obstruction, theirs.obstructionLF, theirs.occlusion, theirs.occlusionLF, theirs.occlusionRoom,
                                       virtualPosition, 0);
        const sound::SourceObstacles ours = sound::ObstaclesOf(zones, sound::ZoneOf(zones, listener), listener, source);
        const bool same = ours.obstruction == theirs.obstruction && SameBits(ours.obstructionLF, theirs.obstructionLF) && ours.occlusion == theirs.occlusion &&
                          SameBits(ours.occlusionLF, theirs.occlusionLF) && SameBits(ours.occlusionRoom, theirs.occlusionRoom);
        ++counts.pairs;
        counts.obstructed += ours.obstruction < 0 ? 1 : 0;
        counts.occluded += ours.occlusion < 0 ? 1 : 0;

        if (!same && ++differences <= 5) {
            fmt::println("  obstacles differ: listener {} {} {}, source {} {} {}: {} {} {} {} {} against {} {} {} {} {}", listener.x, listener.y, listener.z,
                         source.x, source.y, source.z, ours.obstruction, ours.obstructionLF, ours.occlusion, ours.occlusionLF, ours.occlusionRoom,
                         theirs.obstruction, theirs.obstructionLF, theirs.occlusion, theirs.occlusionLF, theirs.occlusionRoom);
        }
    };

    for (int i = 0; i < kPairs; ++i) {
        compare(pick(low, high), pick(low, high));
    }

    for (const sound::DiffractionBox& box : zones.boxes) {
        const auto clamp = [](float v) { return std::clamp(v, -kBound, kBound); };
        const ZonePoint near{.x = clamp(box.min.x - kMargin), .y = clamp(box.min.y - kMargin), .z = clamp(box.min.z - kMargin)};
        const ZonePoint far{.x = clamp(box.max.x + kMargin), .y = clamp(box.max.y + kMargin), .z = clamp(box.max.z + kMargin)};

        for (int i = 0; i < kPairsPerBox; ++i) {
            compare(pick(near, far), pick(near, far));
        }
    }

    return counts;
}

// the diffraction boxes whose zone is not the one that holds their centre (the zones renumbered in the editor after the
// boxes were placed, which EAX Manager never applies) given that zone, in the zone file and in EalMan's data alike, so
// that the check compares the two repaired the same way; both lists then sorted again by zone, the order EAX Manager's
// search through them expects; a box whose centre is in no zone kept. The number of boxes given their zone.
int RepairBoxes(EalMan& eal, ZoneFile& zones)
{
    std::vector<DiffractionBox>& theirs = eal.GetData().gemaDiffBox;
    int repaired = 0;

    for (std::size_t i = 0; i < zones.boxes.size(); ++i) {
        sound::DiffractionBox& box = zones.boxes[i];
        const ZonePoint centre{.x = (box.min.x + box.max.x) / 2.0f, .y = (box.min.y + box.max.y) / 2.0f, .z = (box.min.z + box.max.z) / 2.0f};
        const std::optional<int> zone = sound::ZoneOf(zones, centre);

        if (zone && zone != box.zone) {
            box.zone = *zone;
            theirs[i].lSubspaceID = *zone;
            ++repaired;
        }
    }

    if (repaired > 0) {
        std::ranges::stable_sort(zones.boxes, {}, [](const sound::DiffractionBox& box) { return box.zone.value_or(-1); });
        std::ranges::stable_sort(theirs, {}, &DiffractionBox::lSubspaceID);
    }

    return repaired;
}

// one .eal into its .efxzones, checked; false on a failure
bool ConvertFile(const fs::path& input, const fs::path& output)
{
    EalMan eal;
    const int32_t loaded = eal.LoadDataSet(input.string(), 0);
    std::string report;
    std::string error;
    std::optional<ZoneFile> zones;
    std::optional<int> defaultEnvironment;
    bool checkable = true;

    if (loaded == 0) {
        zones = Convert(eal, defaultEnvironment, report, error);
    }
    else {
        zones = OneEnvironment(eal);
        report = fmt::format("refused by EalMan ({:#010x}): its one environment given to the whole map", static_cast<std::uint32_t>(loaded));
        error = fmt::format("refused by EalMan ({:#010x})", static_cast<std::uint32_t>(loaded));
        checkable = false;
    }

    if (!zones) {
        fmt::println("{}: FAILED, {}", input.generic_string(), error);
        return false;
    }

    if (checkable) {
        if (const int repaired = RepairBoxes(eal, *zones); repaired > 0) {
            report += fmt::format("; {} boxes given the zone that holds their centre", repaired);
        }
    }

    const std::string text = sound::WriteZoneFile(*zones);
    std::string readError;
    const std::optional<ZoneFile> back = sound::ParseZoneFile(text, readError);

    if (!back || *back != *zones) {
        fmt::println("{}: FAILED, the file written does not read back ({})", input.generic_string(), readError);
        return false;
    }

    const Counts counts = checkable ? Check(eal, *back, defaultEnvironment) : Counts{};

    if (defaultEnvironment) {
        report += "; the file's default environment played in some zones";
    }

    fs::create_directories(output.parent_path());
    std::ofstream file(output, std::ios::binary);
    file << text;

    if (!file) {
        fmt::println("{}: FAILED, cannot write {}", input.generic_string(), output.generic_string());
        return false;
    }

    std::size_t boxes = zones->boxes.size();
    std::size_t pairsWithObstacle = 0;

    for (const sound::ObstacleRef& ref : zones->matrix) {
        pairsWithObstacle += ref.kind == sound::ObstacleRefKind::Listed ? 1 : 0;
    }

    fmt::println("{}: {} environments, {} zones, {} planes, {} pairs of zones with an obstacle, {} boxes; {}; {}", input.filename().string(),
                 zones->environments.size(), zones->zoneEnvironment.size(), zones->planes.size(), pairsWithObstacle, boxes, report,
                 checkable ? fmt::format("checked on {} points and {} pairs ({} obstructed, {} occluded), {} differences", counts.points, counts.pairs,
                                         counts.obstructed, counts.occluded, counts.differences)
                           : "not checked");
    return counts.differences == 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 3) {
        fmt::println("usage: eal_converter <folder of eagle/*.eal> <folder of the eagle/*.efxzones written>");
        return 2;
    }

    const fs::path inputRoot = argv[1];
    const fs::path outputRoot = argv[2];
    std::vector<fs::path> inputs;

    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(inputRoot)) {
        if (entry.is_regular_file() && entry.path().extension() == ".eal") {
            inputs.push_back(entry.path());
        }
    }

    std::ranges::sort(inputs);
    int failures = 0;

    for (const fs::path& input : inputs) {
        // the engine asks for eagle/<map>.eal: a name whose map would end with a dot names no map
        if (input.stem().string().ends_with('.')) {
            fmt::println("{}: skipped, no map is named so", input.filename().string());
            continue;
        }

        fs::path output = outputRoot / fs::relative(input, inputRoot);
        output.replace_extension(".efxzones");
        failures += ConvertFile(input, output) ? 0 : 1;
    }

    fmt::println("{} files, {} failed", inputs.size(), failures);
    return failures == 0 ? 0 : 1;
}

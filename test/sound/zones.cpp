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

// The tests of src/shared/sound/zones, on a zone file made here: two rooms on either side of the plane x = 0, a hall
// (x < 0) and a room, an occluding wall between them, a pillar in the hall, an aperture on each side of the door. They
// check: the file written and read back the same, every truncation of it read or refused without a fault, the damaged
// files refused, a cycle in the BSP walked without hanging; the zone of a point, the plane's thickness of 1 included;
// the wall's occlusion between the rooms, the pillar's obstruction between two points of the hall and none beside it;
// then through sound::System on the loopback device: the hall and the room in the slots, a sound's reverb heard after
// it ended and none with the reverb off, a sound behind the wall quieter than one at the same distance in the hall, the
// water's reverb, a map with one reverb changing environment, and no OpenAL error. On four rooms in a row, the reverb
// alone (its slots read): the two nearest neighbours of three, the first found at an equal distance, the reverbs kept
// out of every zone and a source there sent to the listener's; the pans, ahead and below with the view raised, the
// listener's own away from its door; the fade's third step on the logarithmic scale; the filters' and the sends' gains
// of every way of hearing, under water too; a voice behind the wall made virtual before 64 as loud in the hall; the
// zones' map kept through a restart.
#include "sound.h"

#include <check.h>

#include <shared/sound/system.h>
#include <shared/sound/zones/reverb.h>
#include <shared/sound/zones/zones.h>

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

constexpr int kRate = 48000;
constexpr int kStep = 10; // milliseconds

using sound::ZonePoint;

// the two rooms: the zone 0 (the hall, x < 0) behind the plane, the zone 1 (the room) in front of it
sound::ZoneFile TwoRooms(bool apertures)
{
    sound::ZoneFile zones;
    zones.environments.push_back({.name = "Hall", .reverb = {}});
    sound::Reverb room;
    room.decayTime = 0.5f;
    zones.environments.push_back({.name = "Room", .reverb = room});
    zones.zoneEnvironment = {0, 1};
    zones.start = {.kind = sound::ChildKind::Plane, .index = 0};
    zones.planes.push_back(
        {.base = {}, .normal = {.x = 1.0f}, .front = {.kind = sound::ChildKind::Zone, .index = 1}, .back = {.kind = sound::ChildKind::Zone, .index = 0}});
    zones.obstacles.push_back({.name = "Wall", .level = -2000, .lfRatio = 0.25f, .roomRatio = 0.5f, .kind = sound::ObstacleKind::Occludes});
    // no zone, the hall, the room; the wall between the hall and the room both ways
    const sound::ObstacleRef none{};
    const sound::ObstacleRef wall{.kind = sound::ObstacleRefKind::Listed, .index = 0};
    zones.matrix = {none, none, none, none, none, wall, none, wall, none};
    zones.diffraction = {.maxAttenuation = -6000, .lfRatio = 0.25f, .angleMaxAttenuation = 90};
    // a pillar in the hall, away from the line between the rooms' sounds (z the game's y)
    zones.boxes.push_back({.zone = 0, .min = {.x = -300.0f, .y = -100.0f, .z = 200.0f}, .max = {.x = -200.0f, .y = 100.0f, .z = 300.0f}});

    if (apertures) {
        zones.apertures.push_back({.environment = 0, .first = {.x = -1.0f, .y = 0.0f, .z = -64.0f}, .second = {.x = -1.0f, .y = 0.0f, .z = 64.0f}});
        zones.apertures.push_back({.environment = 1, .first = {.x = 1.0f, .y = 0.0f, .z = -64.0f}, .second = {.x = 1.0f, .y = 0.0f, .z = 64.0f}});
    }

    return zones;
}

void TestFile()
{
    const sound::ZoneFile zones = TwoRooms(true);
    const std::string text = sound::WriteZoneFile(zones);
    std::string error;
    const std::optional<sound::ZoneFile> back = sound::ParseZoneFile(text, error);
    CHECK(back.has_value() && *back == zones);

    // every truncation read or refused, never a fault
    for (std::size_t length = 0; length < text.size(); ++length) {
        (void)sound::ParseZoneFile(std::string_view(text).substr(0, length), error);
    }

    CHECK(!sound::ParseZoneFile("efxzones 2\n", error).has_value());
    CHECK(!sound::ParseZoneFile(text.substr(0, text.find("start")), error).has_value());

    std::string badPlane = text;
    badPlane.replace(badPlane.find("start p0"), 8, "start p5");
    CHECK(!sound::ParseZoneFile(badPlane, error).has_value());

    std::string shortRow = text;
    shortRow.replace(shortRow.find("row - - -"), 9, "row - -");
    CHECK(!sound::ParseZoneFile(shortRow, error).has_value());

    // a plane whose front is itself: read, and a point in front of it in no zone rather than a walk without end
    std::string cycle = text;
    cycle.replace(cycle.find(" z1 z0"), 6, " p0 z0");
    const std::optional<sound::ZoneFile> looped = sound::ParseZoneFile(cycle, error);
    CHECK(looped.has_value() && !sound::ZoneOf(*looped, {.x = 10.0f}).has_value());

    // boxes a hand-made file gives out of the zones' order: read in that order, which the obstruction's search needs
    sound::ZoneFile unordered = TwoRooms(true);
    unordered.boxes.insert(unordered.boxes.begin(),
                           {.zone = 1, .min = {.x = 200.0f, .y = -100.0f, .z = 200.0f}, .max = {.x = 300.0f, .y = 100.0f, .z = 300.0f}});
    const std::optional<sound::ZoneFile> sorted = sound::ParseZoneFile(sound::WriteZoneFile(unordered), error);
    CHECK(sorted.has_value() && sorted->boxes.size() == 2 && sorted->boxes[0].zone == 0 && sorted->boxes[1].zone == 1);
}

// the zone files both games ship (assets/jka/eagle and its mp folder, assets/jk2/eagle, 83 in all): each read, and written back as it
// was, so that a change of the reader that refuses one shows here rather than as a map without reverb
void TestShippedFiles(const std::filesystem::path& assets)
{
    int files = 0;

    for (const char* game : {"jka", "jk2"}) {
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(assets / game / "eagle")) {
            if (entry.path().extension() != ".efxzones") {
                continue;
            }

            ++files;
            std::ifstream in(entry.path(), std::ios::binary);
            const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
            std::string error;
            const std::optional<sound::ZoneFile> zones = sound::ParseZoneFile(text, error);
            CHECK(zones.has_value());

            if (zones) {
                const std::optional<sound::ZoneFile> again = sound::ParseZoneFile(sound::WriteZoneFile(*zones), error);
                CHECK(again.has_value() && *again == *zones);
            }
        }
    }

    CHECK_EQUAL(files, 83);
}

void TestCalculations()
{
    const sound::ZoneFile zones = TwoRooms(true);
    CHECK(sound::ZoneOf(zones, {.x = -100.0f}) == 0);
    CHECK(sound::ZoneOf(zones, {.x = 100.0f}) == 1);
    CHECK(sound::ZoneOf(zones, {.x = -0.5f}) == 1); // within the plane's thickness of 1, in front
    CHECK(sound::ZoneOf(zones, {.x = -2.0f}) == 0);
    CHECK(!sound::ZoneOf(zones, {.x = 40000.0f}).has_value());

    const ZonePoint hall{.x = -100.0f};
    const sound::SourceObstacles wall = sound::ObstaclesOf(zones, 0, hall, {.x = 100.0f});
    CHECK(wall.occlusion == -2000 && wall.occlusionLF == 0.25f && wall.occlusionRoom == 0.5f && wall.obstruction == 0);
    CHECK(sound::ObstaclesOf(zones, 0, hall, {.x = -150.0f}) == sound::SourceObstacles{});

    // the pillar between two points of the hall, then beside the line
    const sound::SourceObstacles pillar = sound::ObstaclesOf(zones, 0, {.x = -350.0f, .z = 250.0f}, {.x = -150.0f, .z = 250.0f});
    CHECK(pillar.obstruction < 0 && pillar.obstruction >= -6000 && pillar.obstructionLF == 0.25f);
    CHECK(sound::ObstaclesOf(zones, 0, {.x = -350.0f, .z = 400.0f}, {.x = -150.0f, .z = 400.0f}).obstruction == 0);
}

// a system on the loopback device with the two rooms as the map "test"
struct Run {
    Run(const std::filesystem::path& data, bool apertures) : host(data)
    {
        host.texts["eagle/test.efxzones"] = sound::WriteZoneFile(TwoRooms(apertures));
        system = sound::System::Create(host.host, kRate);
    }

    void Advance(int milliseconds)
    {
        for (int elapsed = 0; elapsed < milliseconds; elapsed += kStep) {
            now += kStep;
            system->Frame(now);
            system->GetDevice().Render(out);
        }
    }

    [[nodiscard]] int Loudest(int milliseconds)
    {
        int loudest = 0;

        for (int elapsed = 0; elapsed < milliseconds; elapsed += kStep) {
            Advance(kStep);

            for (const std::int16_t sample : out) {
                loudest = std::max(loudest, sample < 0 ? -static_cast<int>(sample) : static_cast<int>(sample));
            }
        }

        return loudest;
    }

    test::Host host;
    std::unique_ptr<sound::System> system;
    int now = 0;
    std::vector<std::int16_t> out = std::vector<std::int16_t>(static_cast<std::size_t>(kRate * kStep / 1000) * 2);
};

// the listener at a point of the game, looking along x
sound::Listener ListenerAt(float x, bool underwater = false)
{
    sound::Listener listener;
    listener.entity = 0;
    listener.origin = {x, 0.0f, 0.0f};
    listener.axes = {{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
    listener.underwater = underwater;
    return listener;
}

sound::Request WorldAt(sound::SoundHandle sound, float x, bool loop)
{
    sound::Request request;
    request.sound = sound;
    request.soundClass = sound::SoundClass::World;
    request.place = sound::Place::Point;
    request.entity = 1;
    request.origin = {x, 0.0f, 0.0f};
    request.loop = loop;
    return request;
}

void TestReverb(const std::filesystem::path& data)
{
    Run run(data, true);
    CHECK(run.system != nullptr);

    if (run.system == nullptr) {
        return;
    }

    sound::System& system = *run.system;
    system.LoadZones("test");
    system.SetListener(ListenerAt(-100.0f));
    run.Advance(20);
    const std::string info = system.Info();
    CHECK(info.find("slot: Hall") != std::string::npos && info.find("slot: Room") != std::string::npos);

    // a short tone in the hall: its reverb heard after it ended, none with the reverb off
    const sound::SoundHandle tone = system.Register("sound/a/mono22k_s16");
    const int toneLength = system.Length(tone);
    system.Play(WorldAt(tone, -50.0f, false));
    run.Advance(toneLength + 100);
    const int tail = run.Loudest(100);
    CHECK(tail > 20);
    system.SetReverb(false);
    run.Advance(2000);
    system.Play(WorldAt(tone, -50.0f, false));
    run.Advance(toneLength + 100);
    CHECK(run.Loudest(100) <= 4);
    CHECK(system.Info().find("the reverb off") != std::string::npos);

    // behind the wall, quieter than at the same distance in the hall, both ahead of the listener (the reverb off, the
    // direct sounds alone): the wall's -2000 mB, a quarter of it at the low frequencies, about 0.56 of the tone's level
    const sound::VoiceHandle behind = system.Play(WorldAt(tone, 100.0f, true));
    run.Advance(50);
    const int muffled = run.Loudest(300);
    system.Stop(behind);
    system.SetListener(ListenerAt(-500.0f));
    const sound::VoiceHandle beside = system.Play(WorldAt(tone, -300.0f, true));
    run.Advance(50);
    const int clear = run.Loudest(300);
    system.Stop(beside);
    CHECK(muffled > 0 && muffled < clear * 3 / 4);

    // under water, the water's reverb alone
    system.SetReverb(true);
    system.SetListener(ListenerAt(-100.0f, true));
    run.Advance(20);
    CHECK(system.Info().find("slot: underwater") != std::string::npos);
    system.SetListener(ListenerAt(-100.0f));
    run.Advance(20);
    CHECK(system.Info().find("slot: Hall") != std::string::npos);
    CHECK_EQUAL(alGetError(), AL_NO_ERROR);
    system.StopAll();
}

// two loops of one sound behind the wall, merged into one voice: the wall heard once, so about twice one loop's level,
// where the merged voice took the wall's filter again over the copies' sum, already muffled (the reverb off, the direct
// sounds alone)
void TestMergedLoops(const std::filesystem::path& data)
{
    Run run(data, true);

    if (run.system == nullptr) {
        return;
    }

    sound::System& system = *run.system;
    system.LoadZones("test");
    system.SetReverb(false);
    system.SetListener(ListenerAt(-100.0f));
    const sound::SoundHandle tone = system.Register("sound/a/mono22k_s16");
    sound::Request first = WorldAt(tone, 100.0f, true);
    first.volume = 0.3f;
    const sound::VoiceHandle a = system.Play(first);
    run.Advance(50);
    const int one = run.Loudest(300);
    sound::Request second = WorldAt(tone, 120.0f, true);
    second.volume = 0.3f;
    const sound::VoiceHandle b = system.Play(second);
    run.Advance(50);
    const int two = run.Loudest(300);
    system.Stop(a);
    system.Stop(b);
    CHECK(one > 0 && two > one * 3 / 2);
}

void TestOneReverb(const std::filesystem::path& data)
{
    Run run(data, false);

    if (run.system == nullptr) {
        return;
    }

    sound::System& system = *run.system;
    system.LoadZones("test");
    system.SetListener(ListenerAt(-100.0f));
    run.Advance(20);
    CHECK(system.Info().find("one reverb") != std::string::npos && system.Info().find("slot: Hall") != std::string::npos);

    // into the room: the slot fades to the room's reverb, through its steps, without an error
    system.SetListener(ListenerAt(100.0f));
    run.Advance(1200);
    CHECK(system.Info().find("slot: Room") != std::string::npos);
    CHECK_EQUAL(alGetError(), AL_NO_ERROR);

    // a map without a file: no reverb, and the file's absence is no error
    system.LoadZones("nofile");
    run.Advance(20);
    CHECK(system.Info().find("none on this map") != std::string::npos);
    CHECK_EQUAL(run.host.Count(sound::Message::Warning), 0U);
}

// four rooms in a row, x < 0, then up to 1000, 2000 and 3000, nothing beyond: a hall, a room, a far room and a farther
// one, each with a door at each end towards its neighbours (the aperture's ends 64 units to each side of the axis)
sound::ZoneFile FourRooms()
{
    sound::ZoneFile zones;

    for (const char* name : {"Hall", "Room", "Far", "Farther"}) {
        zones.environments.push_back({.name = name, .reverb = {}});
    }

    zones.zoneEnvironment = {0, 1, 2, 3};
    zones.start = {.kind = sound::ChildKind::Plane, .index = 0};

    for (std::uint32_t i = 0; i < 4; ++i) {
        const bool last = i == 3;
        zones.planes.push_back({.base = {.x = 1000.0f * static_cast<float>(i)},
                                .normal = {.x = 1.0f},
                                .front = last ? sound::Child{} : sound::Child{.kind = sound::ChildKind::Plane, .index = i + 1},
                                .back = {.kind = sound::ChildKind::Zone, .index = i}});
    }

    const auto door = [&zones](int environment, float x) {
        zones.apertures.push_back({.environment = environment, .first = {.x = x, .y = 0.0f, .z = -64.0f}, .second = {.x = x, .y = 0.0f, .z = 64.0f}});
    };
    door(0, -1.0f);
    door(1, 1.0f);
    door(1, 999.0f);
    door(2, 1001.0f);
    door(2, 1999.0f);
    door(3, 2001.0f);
    return zones;
}

// the environments the slots hold, in a set's order
std::vector<int> Held(const sound::ZoneReverb& reverb)
{
    std::vector<int> held;

    for (std::size_t i = 0; i < sound::ZoneReverb::kSlots; ++i) {
        if (const std::optional<int> environment = reverb.SlotEnvironment(i)) {
            held.push_back(*environment);
        }
    }

    std::ranges::sort(held);
    return held;
}

// the slot holding an environment, 0 for none
std::uint32_t SlotHolding(const sound::ZoneReverb& reverb, int environment)
{
    for (std::size_t i = 0; i < sound::ZoneReverb::kSlots; ++i) {
        if (reverb.SlotEnvironment(i) == environment) {
            return reverb.SlotName(i);
        }
    }

    return 0;
}

// the pan of the slot holding an environment
sound::Vector PanOf(const sound::ZoneReverb& reverb, int environment)
{
    for (std::size_t i = 0; i < sound::ZoneReverb::kSlots; ++i) {
        if (reverb.SlotEnvironment(i) == environment && reverb.SlotReverb(i)) {
            return reverb.SlotReverb(i)->lateReverbPan;
        }
    }

    return {};
}

bool Near(float a, float b)
{
    return a > b - 0.001f && a < b + 0.001f;
}

void TestChoiceAndPans(const std::filesystem::path& data)
{
    Run run(data, true); // for its OpenAL context

    if (run.system == nullptr) {
        return;
    }

    sound::ZoneReverb reverb;
    reverb.Load(FourRooms());
    reverb.SetListener(ListenerAt(-100.0f));
    reverb.Frame(0);
    CHECK((Held(reverb) == std::vector<int>{0, 1, 2}));

    // the room's door ahead: its pan straight ahead, as strong as the chord over the arc of the door's 2 atan(64 / 101)
    const sound::Vector room = PanOf(reverb, 1);
    CHECK(Near(room[0], 0.0f) && Near(room[1], 0.0f) && room[2] > 0.94f && room[2] < 0.955f);
    // the hall's own reverb away from its door, weakly
    const sound::Vector hall = PanOf(reverb, 0);
    CHECK(hall[2] < 0.0f && hall[2] > -0.1f);

    // the view raised 60 degrees: the door ahead and below
    sound::Listener raised = ListenerAt(-100.0f);
    raised.axes = {{{0.5f, 0.0f, 0.866025f}, {0.0f, 1.0f, 0.0f}, {-0.866025f, 0.0f, 0.5f}}};
    reverb.SetListener(raised);
    reverb.Frame(10);
    const sound::Vector up = PanOf(reverb, 1);
    CHECK(up[1] < -0.7f && up[2] > 0.4f && Near(up[0], 0.0f));

    // through the door into the room, 10 ms later: the hall, a neighbour now, keeps its slot, its reverb not cut
    const std::uint32_t hallSlot = SlotHolding(reverb, 0);
    reverb.SetListener(ListenerAt(100.0f));
    reverb.Frame(20);
    CHECK((Held(reverb) == std::vector<int>{0, 1, 2}) && SlotHolding(reverb, 0) == hallSlot);

    // in the far room, the room and the farther one at 501 units each: both kept, the hall dropped; then, once the
    // neighbours are chosen again 100 ms later, the same
    reverb.SetListener(ListenerAt(1500.0f));
    reverb.Frame(200);
    CHECK((Held(reverb) == std::vector<int>{1, 2, 3}));
    reverb.Frame(250);
    CHECK((Held(reverb) == std::vector<int>{1, 2, 3}));

    // into the farther room, then out of every zone: its reverbs kept, a source there sent to its reverb
    reverb.SetListener(ListenerAt(2500.0f));
    reverb.Frame(400);
    reverb.SetListener(ListenerAt(3500.0f));
    reverb.Frame(600);
    CHECK((Held(reverb) == std::vector<int>{1, 2, 3}) || (Held(reverb) == std::vector<int>{2, 3}));
    CHECK(reverb.Of({3600.0f, 0.0f, 0.0f}, sound::Hearing::InWorld).slot == SlotHolding(reverb, 3));
    CHECK_EQUAL(alGetError(), AL_NO_ERROR);
}

void TestFilters(const std::filesystem::path& data)
{
    Run run(data, true);

    if (run.system == nullptr) {
        return;
    }

    // one reverb: the third step of the fade from the hall (1.49 s) to the room (0.5 s), on the logarithmic scale
    {
        sound::ZoneReverb reverb;
        reverb.Load(TwoRooms(false));
        reverb.SetListener(ListenerAt(-100.0f));
        reverb.Frame(0);
        reverb.SetListener(ListenerAt(100.0f));
        reverb.Frame(10);
        CHECK(reverb.SlotReverb(0) && Near(reverb.SlotReverb(0)->decayTime, 1.49f));
        reverb.Frame(360);
        const float third = 1.49f * std::pow(0.5f / 1.49f, 0.3f);
        CHECK(reverb.SlotReverb(0) && reverb.SlotReverb(0)->decayTime > third - 0.001f && reverb.SlotReverb(0)->decayTime < third + 0.001f);
        reverb.Frame(1010);
        CHECK(reverb.SlotReverb(0) && reverb.SlotReverb(0)->decayTime == 0.5f);

        // another map loaded: no reverb chosen from the place on the map before (the room) until the game gives the new
        // one, the hall's then at once
        reverb.Load(TwoRooms(false));
        reverb.Frame(1020);
        CHECK(!reverb.SlotReverb(0));
        reverb.SetListener(ListenerAt(-100.0f));
        reverb.Frame(1030);
        CHECK(reverb.SlotReverb(0) && Near(reverb.SlotReverb(0)->decayTime, 1.49f));
    }

    // a hand-made file whose room has a density of 0: the fade from it gives numbers, linearly on that value, where the
    // logarithm from 0 gave a value that is not a number
    {
        sound::ZoneFile zeroDensity = TwoRooms(false);
        zeroDensity.environments[1].reverb.density = 0.0f;
        sound::ZoneReverb reverb;
        reverb.Load(zeroDensity);
        reverb.SetListener(ListenerAt(100.0f));
        reverb.Frame(0);
        reverb.SetListener(ListenerAt(-100.0f));
        reverb.Frame(10);
        reverb.Frame(360);
        const std::optional<sound::Reverb>& fading = reverb.SlotReverb(0);
        CHECK(fading && !std::isnan(fading->density) && fading->density > 0.0f && fading->density < 1.0f);
    }

    sound::ZoneReverb reverb;
    reverb.Load(TwoRooms(true));
    reverb.SetListener(ListenerAt(-100.0f));
    reverb.Frame(0);
    const std::uint32_t hall = SlotHolding(reverb, 0);
    const std::uint32_t room = SlotHolding(reverb, 1);
    CHECK(hall != 0 && room != 0 && hall != room);

    // behind the wall (-2000 mB, a quarter at the low frequencies, half on the reverb): the direct sound at 10^(-500 /
    // 2000) and its high frequencies 1500 mB lower; the send at -250 mB and 750 mB lower; sent to the room's reverb
    const sound::VoiceZone wall = reverb.Of({100.0f, 0.0f, 0.0f}, sound::Hearing::InWorld);
    CHECK(wall.slot == room && Near(wall.directGain, 0.56234f) && Near(wall.directGainHF, 0.17783f) && Near(wall.sendGain, 0.74989f) &&
          Near(wall.sendGainHF, 0.42170f));
    // a voice there: sent, never muffled; at the listener: the hall's reverb; out of the world: nothing
    CHECK((reverb.Of({100.0f, 0.0f, 0.0f}, sound::Hearing::VoiceInWorld) == sound::VoiceZone{.slot = room}));
    CHECK((reverb.Of({100.0f, 0.0f, 0.0f}, sound::Hearing::AtListener) == sound::VoiceZone{.slot = hall}));
    CHECK((reverb.Of({100.0f, 0.0f, 0.0f}, sound::Hearing::NotInWorld) == sound::VoiceZone{}));
    CHECK((reverb.Of({100.0f, 0.0f, 0.0f}, sound::Hearing::OwnNotInWorld) == sound::VoiceZone{}));

    // the pillar between two points of the hall: an obstruction, the direct sound alone filtered, the send not
    reverb.SetListener(ListenerAt(-350.0f));
    reverb.Frame(10);
    const sound::VoiceZone pillar = reverb.Of({-150.0f, 250.0f, 0.0f}, sound::Hearing::InWorld);
    reverb.SetListener({.entity = 0, .origin = {-350.0f, 250.0f, 0.0f}, .axes = ListenerAt(0.0f).axes});
    reverb.Frame(20);
    const sound::VoiceZone behindPillar = reverb.Of({-150.0f, 250.0f, 0.0f}, sound::Hearing::InWorld);
    CHECK(pillar.directGainHF == 1.0f);
    CHECK(behindPillar.directGainHF < 1.0f && behindPillar.directGain < 1.0f && behindPillar.sendGain == 1.0f && behindPillar.sendGainHF == 1.0f);

    // under water: the direct sound's high frequencies 3000 mB lower, the send at -1110 mB to the water's reverb; the
    // listener's own sound not in the world filtered but not sent
    reverb.SetListener(ListenerAt(-100.0f, true));
    reverb.Frame(30);
    const sound::VoiceZone water = reverb.Of({100.0f, 0.0f, 0.0f}, sound::Hearing::InWorld);
    CHECK(water.slot == reverb.SlotName(0) && water.directGain == 1.0f && Near(water.directGainHF, 0.031623f) && Near(water.sendGain, 0.27861f) &&
          Near(water.sendGainHF, 0.031623f));
    const sound::VoiceZone own = reverb.Of({}, sound::Hearing::OwnNotInWorld);
    CHECK(own.slot == 0 && Near(own.directGainHF, 0.031623f));

    // the reverb off from before the map loads: no slot, but the listener's zone followed, so that the wall muffles what
    // is behind it from either room, as for a player with the reverb on, and nothing in the listener's own room
    {
        sound::ZoneReverb off;
        off.SetEnabled(false);
        off.Load(TwoRooms(true));
        off.SetListener(ListenerAt(-100.0f));
        off.Frame(0);
        const sound::VoiceZone fromHall = off.Of({100.0f, 0.0f, 0.0f}, sound::Hearing::InWorld);
        CHECK(fromHall.slot == 0 && Near(fromHall.directGain, 0.56234f) && Near(fromHall.directGainHF, 0.17783f));
        off.SetListener(ListenerAt(100.0f));
        off.Frame(10);
        CHECK(Near(off.Of({-100.0f, 0.0f, 0.0f}, sound::Hearing::InWorld).directGain, 0.56234f));
        CHECK(off.Of({150.0f, 0.0f, 0.0f}, sound::Hearing::InWorld).directGain == 1.0f);
    }

    CHECK_EQUAL(alGetError(), AL_NO_ERROR);
}

// what a frame meets without a fault or an OpenAL error: a map loaded with fewer environments while the slots hold the
// map before's, a listener at a point that is not a number (in no zone), the clock going back during a fade; then the
// reverb of the place given again
void TestRobustness(const std::filesystem::path& data)
{
    Run run(data, true);

    if (run.system == nullptr) {
        return;
    }

    sound::ZoneReverb reverb;
    reverb.Load(TwoRooms(true));
    reverb.SetListener(ListenerAt(100.0f));
    reverb.Frame(0);
    sound::ZoneFile one = TwoRooms(true);
    one.environments.pop_back();
    one.zoneEnvironment = {0, 0};
    one.apertures.pop_back();
    reverb.Load(one);
    reverb.SetListener(ListenerAt(std::numeric_limits<float>::quiet_NaN()));
    reverb.Frame(100);
    (void)reverb.Of({std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f}, sound::Hearing::InWorld);
    reverb.SetListener(ListenerAt(-100.0f));
    reverb.Frame(200);
    CHECK(reverb.SlotReverb(0) && Near(reverb.SlotReverb(0)->decayTime, 1.49f));

    sound::ZoneReverb fading;
    fading.Load(TwoRooms(false));
    fading.SetListener(ListenerAt(-100.0f));
    fading.Frame(1000);
    fading.SetListener(ListenerAt(100.0f));
    fading.Frame(1010);
    fading.Frame(600);
    fading.Frame(2500);
    CHECK(fading.SlotReverb(0) && fading.SlotReverb(0)->decayTime == 0.5f);
    CHECK_EQUAL(alGetError(), AL_NO_ERROR);
}

// 64 sounds in the hall and one as loud behind the wall, all started at once, the one behind the wall first: muffled,
// it is the one left virtual, where its age alone would have mixed it
void TestChoice(const std::filesystem::path& data)
{
    Run run(data, true);

    if (run.system == nullptr) {
        return;
    }

    sound::System& system = *run.system;
    system.LoadZones("test");
    system.SetListener(ListenerAt(-100.0f));
    run.Advance(20);
    const sound::VoiceHandle behind = system.Play(WorldAt(system.Register("sound/w/mono22k_s16"), 100.0f, false));
    std::vector<sound::VoiceHandle> hall;

    for (int i = 0; i < 64; ++i) {
        hall.push_back(system.Play(WorldAt(system.Register("sound/h" + std::to_string(i) + "/mono22k_s16"), -300.0f, false)));
    }

    run.Advance(10);
    CHECK(system.IsPlaying(behind) && !system.IsReal(behind) && std::ranges::all_of(hall, [&system](sound::VoiceHandle h) { return system.IsReal(h); }));

    // the zones' map kept through a restart
    CHECK(system.SaveState().zones == "test");
    system.StopAll();
}

} // namespace

void RunZonesTests(const std::filesystem::path& data, const std::filesystem::path& assets)
{
    TestFile();
    TestShippedFiles(assets);
    TestCalculations();
    TestReverb(data);
    TestMergedLoops(data);
    TestOneReverb(data);
    TestChoiceAndPans(data);
    TestFilters(data);
    TestChoice(data);
    TestRobustness(data);
}

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

// The tests of src/shared/sound/ambient, through sound::System on the loopback device: the sets of a sound.txt written
// for them (the sets asked for only, a missing one warned, a comment ending a set, a keyword a kind does not take), a
// brush model's sounds by stage, a general set's one-shots at the delay drawn once per one-shot (the old mixer drew it
// again at every frame, so that they came at the shortest delay) and its loop started once, a local set, and the sets
// stopped when the game no longer asks for them.
#include "sound.h"

#include <check.h>

#include <shared/sound/system.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr int kRate = 48000;
constexpr int kStep = 100; // milliseconds

constexpr std::string_view kSets = R"(type ambientSet
amsdir w:/game/base/sound

;*********************
;birds
;*********************
generalSet birds
timeBetweenWaves 20 10
subWaves ambience mono22k_s16 mono11k_u8
loopedWave ambience/mono22k_s16
volRange 100 200

localSet console
timeBetweenWaves 5 5
subWaves movers mono22k_s16
radius 300
volRange 255 255

bmodelSet door
subWaves movers mono22k_s16 missing_file mono11k_u8
radius 100

localSet fan
loopedWave movers/mono22k_s16
radius 200

bmodelSet beam
subWaves movers mono22k_s16

bmodelSet fan
subWaves movers missing_file mono11k_u8

bmodelSet beam
subWaves movers mono11k_u8

generalSet notwanted
loopedWave ambience/mono22k_s16
)";

void TestSets(const std::filesystem::path& data)
{
    test::Host host(data);
    host.texts["sound/sound.txt"] = std::string(kSets);
    host.random = [](int /*min*/, int max) { return max; };
    const std::unique_ptr<sound::System> system = sound::System::Create(host.host, kRate);
    CHECK(system != nullptr);

    if (system == nullptr) {
        return;
    }

    std::vector<std::int16_t> out(static_cast<std::size_t>(kRate * kStep / 1000) * 2);
    int now = 0;
    std::vector<std::string> lines;
    system->SetLog([&lines](std::string_view line) { lines.emplace_back(line); });
    system->ClearPrecachedSets();

    for (const char* name : {"Birds", "console", "door", "fan", "beam", "absent"}) {
        system->PrecacheSet(name);
    }

    const std::size_t warnings = host.Count(sound::Message::Warning);
    system->ParseSets();

    // the one missing set warned (the brush model's missing file is a developer's message); the keyword its kind does
    // not take ends its set
    CHECK_EQUAL(host.Count(sound::Message::Warning), warnings + 1);
    CHECK(system->BrushModelSound("door", sound::BrushModelStage::Start) != sound::kNoSound);
    CHECK(system->BrushModelSound("door", sound::BrushModelStage::Middle) == sound::kNoSound);
    CHECK(system->BrushModelSound("door", sound::BrushModelStage::End) != sound::kNoSound);
    CHECK(system->BrushModelSound("notwanted", sound::BrushModelStage::Start) == sound::kNoSound);

    // a name given twice: the brush model's set over the local one, the later of two brush models' sets, as the original
    // game kept them (the game's "fanblade" and "reactor_beam")
    CHECK(system->BrushModelSound("fan", sound::BrushModelStage::Middle) != sound::kNoSound);
    CHECK(system->BrushModelSound("beam", sound::BrushModelStage::Start) == system->Register("sound/movers/mono11k_u8"));

    // a general set for 59 s, the draws at their highest: its loop started once, a one-shot every 20 s (from the end of
    // the set's fade in of a second: at 1.1, 21.1 and 41.1 s), where the old mixer, drawing at every frame, played one
    // about every 10 s
    for (int elapsed = 0; elapsed < 59000; elapsed += kStep) {
        system->UpdateAmbientSet("birds", {});
        now += kStep;
        system->Frame(now);
        system->GetDevice().Render(out);
    }

    const auto starts = [&lines](std::string_view loop) {
        return std::ranges::count_if(lines, [loop](const std::string& line) {
            return line.find(" start ") != std::string::npos && line.find("class=ambient") != std::string::npos && line.find(loop) != std::string::npos;
        });
    };

    CHECK_EQUAL(starts("loop=1"), 1);
    CHECK_EQUAL(starts("loop=0"), 3);

    // the first one-shot heard at the set's volume, once its fade in has ended: drawn during it, it came at a fraction of
    // its volume and spent its delay
    const auto first = std::ranges::find_if(lines, [](const std::string& line) {
        return line.find(" start ") != std::string::npos && line.find("class=ambient") != std::string::npos && line.find("loop=0") != std::string::npos;
    });
    CHECK(first != lines.end() && std::stoi(first->substr(std::string_view("SNDLOG ").size())) >= 1000);

    // a local set at an entity, then neither: everything stopped
    system->AddLocalSet("console", 40, {100.0f, 0.0f, 0.0f});
    system->UpdateAmbientSet("birds", {});
    now += kStep;
    system->Frame(now);
    CHECK(system->Playing() >= 2U);

    // the one-shots playing out (0.25 s)
    for (int elapsed = 0; elapsed < 500; elapsed += kStep) {
        now += kStep;
        system->Frame(now);
        system->GetDevice().Render(out);
    }

    CHECK_EQUAL(system->Playing(), 0U);
    system->SetLog({});
}

// sets without their keywords take the original game's defaults: a one-shot every 10 to 25 s at full volume, a local
// set heard within 250 units; the file's lines ended by a carriage return alone
void TestDefaults(const std::filesystem::path& data)
{
    test::Host host(data);
    host.texts["sound/sound.txt"] = "type ambientSet\rgeneralSet plain\rsubWaves ambience mono22k_s16\rlocalSet near\rloopedWave ambience/mono11k_u8\r";
    host.random = [](int /*min*/, int max) { return max; };
    const std::unique_ptr<sound::System> system = sound::System::Create(host.host, kRate);
    CHECK(system != nullptr);

    if (system == nullptr) {
        return;
    }

    std::vector<std::int16_t> out(static_cast<std::size_t>(kRate * kStep / 1000) * 2);
    std::vector<std::string> lines;
    system->SetLog([&lines](std::string_view line) { lines.emplace_back(line); });
    system->ClearPrecachedSets();
    system->PrecacheSet("plain");
    system->PrecacheSet("near");
    system->ParseSets();

    // 59 s, the draws at their highest: the general set's one-shots every 25 s from the end of its fade in (1.1, 26.1 and
    // 51.1 s), the local set's loop started once
    for (int now = kStep; now < 59000 + kStep; now += kStep) {
        system->UpdateAmbientSet("plain", {});
        system->AddLocalSet("near", 40, {100.0f, 0.0f, 0.0f});
        system->Frame(now);
        system->GetDevice().Render(out);
    }

    const auto starts = [&lines](std::string_view what) {
        return std::ranges::count_if(lines, [what](const std::string& line) {
            return line.find(" start ") != std::string::npos && line.find("class=ambient") != std::string::npos && line.find(what) != std::string::npos;
        });
    };

    CHECK_EQUAL(starts("loop=0"), 3);
    CHECK_EQUAL(starts("loop=1"), 1);
    system->SetLog({});
}

// two general sets of a loop each: changing from one to the other plays both for a second, then the new one alone; the
// sets read again between the game's call and the frame
void TestCrossFade(const std::filesystem::path& data)
{
    test::Host host(data);
    host.texts["sound/sound.txt"] = "type ambientSet\ngeneralSet one\nloopedWave a/mono22k_s16\ngeneralSet two\nloopedWave b/mono11k_u8\n";
    const std::unique_ptr<sound::System> system = sound::System::Create(host.host, kRate);

    if (system == nullptr) {
        return;
    }

    int now = 0;
    const auto frames = [&](const char* set, int milliseconds) {
        for (int elapsed = 0; elapsed < milliseconds; elapsed += kStep) {
            system->UpdateAmbientSet(set, {});
            now += kStep;
            system->Frame(now);
        }
    };
    system->PrecacheSet("one");
    system->PrecacheSet("two");
    system->ParseSets();
    frames("one", 1500);
    CHECK_EQUAL(system->Playing(), 1U);
    frames("two", 500);
    CHECK_EQUAL(system->Playing(), 2U);
    frames("two", 1000);
    CHECK_EQUAL(system->Playing(), 1U);
    system->UpdateAmbientSet("two", {});
    system->ParseSets();
    now += kStep;
    system->Frame(now);
    CHECK_EQUAL(system->Playing(), 0U);
}

} // namespace

void RunAmbientTests(const std::filesystem::path& data)
{
    TestSets(data);
    TestDefaults(data);
    TestCrossFade(data);
}

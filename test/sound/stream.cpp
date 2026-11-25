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

// The tests of src/shared/sound/stream: the dynamic music read from a text written for them (a level using another's
// entry and another's boss piece, the tracks' files, the exits' tolerance, the entry markers, the rules of the changes,
// the errors), a music that is not dynamic played and looped, a dynamic music changing state through its transition,
// and the cinematics' stream, all through sound::System on the loopback device.
#include "sound.h"

#include <check.h>

#include <shared/sound/stream/dms.h>
#include <shared/sound/system.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr int kRate = 48000;
constexpr int kStep = 10; // milliseconds

constexpr std::string_view kDms = R"(// a level, another using it, and a third giving its boss piece
musicfiles
{
	lvl_explore
	{
		entry
		{
			marker0	0.000
			marker1	10.5
		}
		exit
		{
			nextfile	lvl_etr00
			time00	0.1
			time01	20.0
		}
	}
	lvl_action
	{
		entry
		{
			marker0	0
			marker1	30
		}
		exit
		{
			nextfile	lvl_atr00
			nextmark	marker1
			time0	5.0
		}
	}
	boss_piece
	{
	}
}
levelmusic
{
	lvl
	{
		explore	lvl_explore
		action	lvl_action
		useboss	other
	}
	alias
	{
		uses	alias2
	}
	alias2
	{
		uses	lvl
	}
	other
	{
		boss	boss_piece
	}
}
)";

void TestDynamicMusicData()
{
    std::string error;
    const std::optional<sound::DynamicMusic> music = sound::ParseDynamicMusic(kDms, "alias", error);
    CHECK(music.has_value() && error.empty());

    if (!music) {
        return;
    }

    CHECK_EQUAL(music->set, "lvl");
    CHECK_EQUAL(music->bossSet, "other");
    CHECK_EQUAL(sound::TrackFile(*music, sound::Track::Explore), "music/lvl/lvl_explore");
    CHECK_EQUAL(sound::TrackFile(*music, sound::Track::Boss), "music/other/boss_piece");
    CHECK_EQUAL(sound::TrackFile(*music, sound::Track::ActionTransition0), "music/lvl/lvl_atr00");
    CHECK_EQUAL(sound::TrackFile(*music, sound::Track::ExploreTransition0), "music/lvl/lvl_etr00");
    CHECK(sound::TrackFile(*music, sound::Track::ExploreTransition1).empty());
    CHECK_EQUAL(sound::TrackFile(*music, sound::Track::Death), "music/death_music");

    // an exit within 0.3 s; an action piece's transition leading to the explore piece's marker
    const auto fromExplore = sound::AllowedTransition(*music, sound::Track::Explore, 20.2f);
    CHECK(fromExplore.has_value() && fromExplore->track == sound::Track::ExploreTransition0);
    CHECK(!sound::AllowedTransition(*music, sound::Track::Explore, 20.4f).has_value());
    const auto fromAction = sound::AllowedTransition(*music, sound::Track::Action, 5.0f);
    CHECK(fromAction.has_value() && fromAction->track == sound::Track::ActionTransition0 && fromAction->entryTime == 10.5f);
    CHECK(sound::EntryTimes(*music, sound::Track::Action) == std::vector<float>({0.0f, 30.0f}));

    // the changes allowed
    using sound::CanInterrupt;
    using sound::DynamicState;
    CHECK(CanInterrupt(DynamicState::Explore, DynamicState::Death));
    CHECK(!CanInterrupt(DynamicState::Death, DynamicState::Explore));
    CHECK(CanInterrupt(DynamicState::Boss, DynamicState::Silence));
    CHECK(!CanInterrupt(DynamicState::Boss, DynamicState::Action));
    CHECK(CanInterrupt(DynamicState::Transition, DynamicState::Action));
    CHECK(!CanInterrupt(DynamicState::Transition, DynamicState::Explore));

    // a level without music, and none at all (an empty name, which the fuzzer found reading a null entry); a
    // transition to a marker the explore piece lacks
    CHECK(!sound::ParseDynamicMusic(kDms, "nothing", error).has_value());
    CHECK(!sound::ParseDynamicMusic(kDms, "", error).has_value());
    std::string broken(kDms);
    broken.replace(broken.find("nextmark\tmarker1"), 16, "nextmark\tmarker9");
    error.clear();
    CHECK(!sound::ParseDynamicMusic(broken, "lvl", error).has_value() && !error.empty());

    // groups nested a hundred thousand deep, which would overflow the stack of a reader without a bound
    std::string deep;

    for (int i = 0; i < 100000; ++i) {
        deep += "a{";
    }

    error.clear();
    CHECK(!sound::ParseDynamicMusic(deep, "lvl", error).has_value() && !error.empty());
}

// a system on the loopback device
struct Run {
    explicit Run(const std::filesystem::path& data) : host(data), system(sound::System::Create(host.host, kRate)) {}

    void Advance(int milliseconds)
    {
        for (int elapsed = 0; elapsed < milliseconds; elapsed += kStep) {
            now += kStep;
            system->Frame(now);
            system->GetDevice().Render(out);
        }
    }

    // silent but for OpenAL Soft's dither of its 16-bit output, a step or two
    [[nodiscard]] bool Silent() const
    {
        return std::ranges::all_of(out, [](std::int16_t s) { return s >= -4 && s <= 4; });
    }

    test::Host host;
    std::unique_ptr<sound::System> system;
    int now = 0;
    std::vector<std::int16_t> out = std::vector<std::int16_t>(static_cast<std::size_t>(kRate * kStep / 1000) * 2);
};

// a WAV file of a square wave, seconds long, stereo at 22,050 Hz
std::string LongWav(int seconds)
{
    constexpr int kRate = 22050;
    const int frames = kRate * seconds;
    std::string file;
    const auto put = [&file](std::uint32_t value, int bytes) {
        for (int i = 0; i < bytes; ++i) {
            file.push_back(static_cast<char>((value >> (8 * i)) & 0xFFU));
        }
    };
    const auto dataBytes = static_cast<std::uint32_t>(frames * 4);
    file += "RIFF";
    put(36 + dataBytes, 4);
    file += "WAVEfmt ";
    put(16, 4);
    put(1, 2);
    put(2, 2);
    put(kRate, 4);
    put(kRate * 4, 4);
    put(4, 2);
    put(16, 2);
    file += "data";
    put(dataBytes, 4);

    for (int i = 0; i < frames * 2; ++i) {
        put((i / 40) % 2 == 0 ? 0x2000U : 0xE000U, 2);
    }

    return file;
}

// a file of the data folder as a text the host gives under another name
std::string Bytes(const std::filesystem::path& file)
{
    const std::vector<std::byte> bytes = test::ReadFile(file);
    std::string text(bytes.size(), '\0');
    std::ranges::transform(bytes, text.begin(), [](std::byte b) { return static_cast<char>(b); });
    return text;
}

void TestMusic(const std::filesystem::path& data)
{
    Run run(data);
    CHECK(run.system != nullptr);

    if (run.system == nullptr) {
        return;
    }

    sound::System& system = *run.system;

    // a file: played, then looped (the intro again)
    system.StartMusic("music/mono22k_s16", "");
    run.Advance(100);
    CHECK(!run.Silent());
    run.Advance(1000);
    CHECK(system.Info().find("music music/mono22k_s16") != std::string::npos);
    CHECK(!run.Silent());
    system.StopMusic();
    run.Advance(100);
    CHECK(run.Silent());

    // a dynamic music of short pieces: explore, then silence through its transition at its exit time (0.1 s)
    const std::string piece = Bytes(data / "stereo44k_s16.wav");
    run.host.texts["ext_data/dms.dat"] = std::string(kDms);

    for (const char* file : {"music/lvl/lvl_explore.wav", "music/lvl/lvl_action.wav", "music/lvl/lvl_etr00.wav", "music/lvl/lvl_atr00.wav",
                             "music/other/boss_piece.wav", "music/death_music.wav"}) {
        run.host.texts[file] = piece;
    }

    system.StartMusic("lvl", "");
    run.Advance(50);
    CHECK(system.Info().find("dynamic music lvl (lvl): explore") != std::string::npos);
    CHECK(!run.Silent());
    system.SetMusicState(sound::MusicState::Silence);
    run.Advance(kStep);
    CHECK(system.Info().find(": transition") != std::string::npos);
    run.Advance(1500);
    CHECK(system.Info().find(": silence") != std::string::npos);
    system.SetMusicState(sound::MusicState::Action);
    run.Advance(kStep);
    CHECK(system.Info().find(": action") != std::string::npos);
    system.SetMusicState(sound::MusicState::Death);
    run.Advance(kStep);
    CHECK(system.Info().find(": death") != std::string::npos);
    system.SetMusicState(sound::MusicState::Explore);
    run.Advance(kStep);
    CHECK(system.Info().find(": death") != std::string::npos);
    system.StopMusic();

    // boss music from another level's entry, the level's dynamic music started again without reading a file (the level's
    // start preloads it, its first snapshot starts it again); the dynamic music then not allowed: its explore piece as a
    // file at once, the music started again without the game asking, and the dynamic music back when allowed again
    run.host.asked.clear();
    system.StartMusic("lvl", "");
    CHECK(run.host.asked.empty());
    system.SetMusicState(sound::MusicState::Boss);
    run.Advance(kStep);
    CHECK(system.Info().find(": boss") != std::string::npos);
    system.AllowDynamicMusic(false);
    run.Advance(kStep);
    CHECK(system.Info().find("music music/lvl/lvl_explore") != std::string::npos);
    system.AllowDynamicMusic(true);
    run.Advance(kStep);
    CHECK(system.Info().find("dynamic music lvl (lvl): explore") != std::string::npos);
    system.StopMusic();

    // a new level whose transition piece is missing (a mod's): the dynamic music played all the same, warned, the
    // silence asked for reached at the explore piece's exit time without the transition
    system.BeginRegistration();
    run.host.texts.erase("music/lvl/lvl_etr00.wav");
    const std::size_t warnings = run.host.Count(sound::Message::Warning);
    system.StartMusic("lvl", "");
    run.Advance(50);
    CHECK(system.Info().find("dynamic music lvl (lvl): explore") != std::string::npos);
    CHECK_EQUAL(run.host.Count(sound::Message::Warning), warnings + 1);
    system.SetMusicState(sound::MusicState::Silence);
    run.Advance(200);
    CHECK(system.Info().find(": silence") != std::string::npos);
    system.StopMusic();
    run.host.texts["music/lvl/lvl_etr00.wav"] = piece;

    // an intro then a loop of another format, on a stream of its own; then the music asked for played again
    system.StartMusic("music/mono22k_s16", "music/stereo44k_s16");
    run.Advance(500);
    CHECK(system.Info().find("music music/stereo44k_s16") != std::string::npos);
    system.RestartMusic();
    run.Advance(kStep);
    CHECK(system.Info().find("music music/mono22k_s16") != std::string::npos);
    system.StopMusic();

    // a dynamic piece of six seconds loops by fading its start in over its end, two seconds before it (a new level, whose
    // files are read again)
    for (const char* file : {"music/lvl/lvl_explore.wav", "music/lvl/lvl_action.wav"}) {
        run.host.texts[file] = LongWav(6);
    }

    system.BeginRegistration();

    system.StartMusic("lvl", "");
    run.Advance(3000);
    CHECK(system.Info().find("fading") == std::string::npos);
    run.Advance(1500);
    CHECK(system.Info().find(", fading music/lvl/lvl_explore") != std::string::npos);
    system.StopMusic();

    // the cinematics' stream
    system.OpenStream(22050, 1);
    std::vector<std::int16_t> tone(2205);

    for (std::size_t i = 0; i < tone.size(); ++i) {
        tone[i] = (i / 25 % 2 == 0) ? 8000 : -8000;
    }

    system.QueueStream(tone);
    run.Advance(50);
    CHECK(!run.Silent());
    system.CloseStream();
    run.Advance(50);
    CHECK(run.Silent());
}

} // namespace

void RunStreamTests(const std::filesystem::path& data)
{
    TestDynamicMusicData();
    TestMusic(data);
}

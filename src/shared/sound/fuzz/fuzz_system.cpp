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

// fuzz_system.cpp -- libFuzzer target on the whole sound as a game drives it, on OpenAL Soft's loopback device: each
// input a sequence of calls (play a sound of any class, place, slot, volume, with an entity or none, stop it, move it,
// stop an entity's slot or all its sounds, move entities and the listener, run frames and render them, ask a voice's
// state, start and change the music and the dynamic music, play and read again the ambient sets, feed the cinematics'
// stream at any format, register and free a level's sounds, change the volumes and the focus, lose the device and get
// it back, put the clock back, load the reverb's zones whole, cut or not at all with the reverb on or off, read a
// dms.dat or a sound.txt the input gives, restart the sound), every byte choosing the next call and its arguments; the sanitizers stop
// at a read outside a buffer, a use after free or an undefined operation. Example, from an empty corpus:
//   sound_fuzz_system <work folder>
#include <shared/sound/system.h>
#include <shared/sound/zones/zones.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

// a WAV file of a square wave: its rate, its channels, its frames
std::vector<std::byte> MakeWav(int rate, int channels, int frames)
{
    std::vector<std::byte> file;
    const auto put = [&file](std::uint32_t value, int bytes) {
        for (int i = 0; i < bytes; ++i) {
            file.push_back(static_cast<std::byte>((value >> (8 * i)) & 0xFFU));
        }
    };
    const auto tag = [&file](const char* text) {
        for (int i = 0; i < 4; ++i) {
            file.push_back(static_cast<std::byte>(text[i]));
        }
    };
    const auto dataBytes = static_cast<std::uint32_t>(frames * channels * 2);
    tag("RIFF");
    put(36 + dataBytes, 4);
    tag("WAVE");
    tag("fmt ");
    put(16, 4);
    put(1, 2);
    put(static_cast<std::uint32_t>(channels), 2);
    put(static_cast<std::uint32_t>(rate), 4);
    put(static_cast<std::uint32_t>(rate * channels * 2), 4);
    put(static_cast<std::uint32_t>(channels * 2), 2);
    put(16, 2);
    tag("data");
    put(dataBytes, 4);

    for (int i = 0; i < frames * channels; ++i) {
        put((i / 20) % 2 == 0 ? 0x2000U : 0xE000U, 2);
    }

    return file;
}

constexpr std::string_view kDms = R"(musicfiles
{
	lvl_explore
	{
		entry
		{
			marker0	0
		}
		exit
		{
			nextfile	lvl_etr00
			time00	0.05
		}
	}
	lvl_action
	{
		entry
		{
			marker0	0
		}
		exit
		{
			nextfile	lvl_atr00
			nextmark	marker0
			time00	0.05
		}
	}
}
levelmusic
{
	lvl
	{
		explore	lvl_explore
		action	lvl_action
	}
}
)";

constexpr std::string_view kSets = R"(type ambientSet
generalSet general
timeBetweenWaves 0 1
subWaves x a b
loopedWave a
volRange 100 255
localSet local
timeBetweenWaves 0 1
subWaves x a
loopedWave b
volRange 0 255
radius 300
)";

// the sound, its loopback device, its files: two short sounds, a music and the dynamic music's pieces, the two texts
struct Fixture {
    Fixture()
    {
        const std::vector<std::byte> mono = MakeWav(22050, 1, 2205);
        const std::vector<std::byte> stereo = MakeWav(44100, 2, 4410);
        files["sound/x/a.wav"] = mono;
        files["sound/x/b.wav"] = stereo;
        files["sound/a.wav"] = mono;
        files["sound/b.wav"] = stereo;

        for (const char* name : {"music/m.wav", "music/lvl/lvl_explore.wav", "music/lvl/lvl_action.wav", "music/lvl/lvl_etr00.wav", "music/lvl/lvl_atr00.wav",
                                 "music/death_music.wav"}) {
            files[name] = stereo;
        }

        files["ext_data/dms.dat"] = Bytes(kDms);
        files["sound/sound.txt"] = Bytes(kSets);
        zones = Bytes(sound::WriteZoneFile(TwoRooms()));
        files["eagle/rooms.efxzones"] = zones;
        host.readFile = [this](std::string_view path) -> std::optional<std::vector<std::byte>> {
            const auto found = files.find(std::string(path));
            return found != files.end() ? std::optional(found->second) : std::nullopt;
        };
        host.fileExists = [this](std::string_view path) { return files.contains(std::string(path)); };
        host.print = [](sound::Message, std::string_view) {};
        host.random = [this](int min, int max) { return min + static_cast<int>(draw++ % static_cast<unsigned>(max - min + 1)); };
        system = sound::System::Create(host, 48000);
        system->PrecacheSet("general");
        system->PrecacheSet("local");
        system->ParseSets();
        sounds = {sound::kNoSound, system->Register("sound/x/a"), system->Register("sound/x/b")};
    }

    // a hall and a room, a wall between them both ways, a pillar's box in the hall, an aperture each
    static sound::ZoneFile TwoRooms()
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
        const sound::ObstacleRef none{};
        const sound::ObstacleRef wall{.kind = sound::ObstacleRefKind::Listed, .index = 0};
        zones.matrix = {none, none, none, none, none, wall, none, wall, none};
        zones.diffraction = {.maxAttenuation = -6000, .lfRatio = 0.25f, .angleMaxAttenuation = 90};
        zones.boxes.push_back({.zone = 0, .min = {.x = -300.0f, .y = -100.0f, .z = 200.0f}, .max = {.x = -200.0f, .y = 100.0f, .z = 300.0f}});
        zones.apertures.push_back({.environment = 0, .first = {.x = -1.0f, .y = 0.0f, .z = -64.0f}, .second = {.x = -1.0f, .y = 0.0f, .z = 64.0f}});
        zones.apertures.push_back({.environment = 1, .first = {.x = 1.0f, .y = 0.0f, .z = -64.0f}, .second = {.x = 1.0f, .y = 0.0f, .z = 64.0f}});
        return zones;
    }

    static std::vector<std::byte> Bytes(std::string_view text)
    {
        std::vector<std::byte> bytes(text.size());

        for (std::size_t i = 0; i < text.size(); ++i) {
            bytes[i] = static_cast<std::byte>(text[i]);
        }

        return bytes;
    }

    std::map<std::string, std::vector<std::byte>> files;
    std::vector<std::byte> zones; // the rooms' zone file as written
    sound::Host host;
    std::unique_ptr<sound::System> system;
    std::array<sound::SoundHandle, 3> sounds{};
    unsigned draw = 0;
    int now = 0;
};

// the input read byte by byte, 0 past its end
class Input {
public:
    Input(const std::uint8_t* data, std::size_t size) : data_(data), size_(size) {}

    [[nodiscard]] bool Empty() const { return position_ >= size_; }

    std::uint8_t Byte() { return position_ < size_ ? data_[position_++] : 0; }

    float Coordinate() { return (static_cast<float>(Byte()) - 128.0f) * 32.0f; }

    sound::Vector Point() { return {Coordinate(), Coordinate(), Coordinate()}; }

    int Entity() { return static_cast<int>(Byte() % 66) - 2; }

    // the next bytes, as many as the next byte says
    std::vector<std::byte> Bytes()
    {
        std::vector<std::byte> bytes(Byte());

        for (std::byte& b : bytes) {
            b = static_cast<std::byte>(Byte());
        }

        return bytes;
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t position_ = 0;
};

} // namespace

// one input, as libFuzzer calls it: its calls on the sound, then everything stopped for the next input
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    static Fixture fixture;
    Input input(data, size);
    std::vector<sound::VoiceHandle> voices;
    std::vector<std::int16_t> out;

    while (!input.Empty()) {
        sound::System& system = *fixture.system;

        switch (input.Byte() % 22) {
        case 0: {
            sound::Request request;
            request.sound = fixture.sounds[input.Byte() % fixture.sounds.size()];
            request.soundClass = static_cast<sound::SoundClass>(input.Byte() % 12);
            request.place = static_cast<sound::Place>(input.Byte() % 3);
            if (input.Byte() % 8 != 0) {
                request.entity = input.Entity();
            }

            request.origin = input.Point();
            request.slot = static_cast<sound::Slot>(input.Byte() % 6);
            request.loop = (input.Byte() & 1U) != 0;
            request.volume = static_cast<float>(input.Byte()) / 64.0f;
            voices.push_back(system.Play(request));
            break;
        }
        case 1:
            if (!voices.empty()) {
                system.Stop(voices[input.Byte() % voices.size()]);
            }

            break;
        case 2:
            if (!voices.empty()) {
                system.MoveLoop(voices[input.Byte() % voices.size()], input.Point(), input.Point());
            }

            break;
        case 3:
            system.StopSlot(input.Entity(), static_cast<sound::Slot>(input.Byte() % 6));
            break;
        case 4:
            system.StopEntity(input.Entity());
            break;
        case 5:
            system.UpdateEntity(input.Entity(), input.Point(), input.Point());
            break;
        case 6: {
            sound::Listener listener;
            listener.entity = input.Entity();
            listener.origin = input.Point();
            listener.axes = {{{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}};
            listener.velocity = input.Point();
            listener.underwater = (input.Byte() & 1U) != 0;
            system.SetListener(listener);
            break;
        }
        case 7:
            fixture.now += input.Byte();
            system.Frame(fixture.now);
            out.resize(static_cast<std::size_t>(input.Byte()) * 16);
            system.GetDevice().Render(out);
            break;
        case 8:
            (void)system.GetVoiceState(input.Entity());
            break;
        case 9: {
            constexpr std::array<const char*, 4> kMusic = {"music/m", "lvl", "", "music/missing"};
            system.StartMusic(kMusic[input.Byte() % kMusic.size()], "");
            break;
        }
        case 10:
            system.SetMusicState(static_cast<sound::MusicState>(input.Byte() % 5));
            break;
        case 11:
            system.UpdateAmbientSet((input.Byte() & 1U) != 0 ? "general" : "missing", {});
            system.AddLocalSet("local", input.Entity(), input.Point());
            break;
        case 12:
            switch (input.Byte() % 3) {
            case 0:
                system.OpenStream((input.Byte() & 1U) != 0 ? 22050 : 44100, 1 + (input.Byte() & 1U));
                break;
            case 1: {
                const std::vector<std::int16_t> samples(static_cast<std::size_t>(input.Byte()) * 8, 1000);
                system.QueueStream(samples);
                break;
            }
            default:
                system.CloseStream();
                break;
            }

            break;
        case 13:
            system.BeginRegistration();
            (void)system.Register("sound/x/a");
            system.EndRegistration();
            break;
        case 14:
            system.SetVolumes(static_cast<float>(input.Byte()) / 255.0f, static_cast<float>(input.Byte()) / 255.0f, static_cast<float>(input.Byte()) / 255.0f);
            system.SetFocus((input.Byte() & 1U) != 0);
            break;
        case 15:
            (void)system.Length(fixture.sounds[input.Byte() % fixture.sounds.size()]);
            system.StopMusic();
            break;
        case 16:
            // the sets read again between the game's calls and the frame; the clock going back
            system.ParseSets();
            fixture.now -= input.Byte();
            break;
        case 17: {
            // a stream of any format, the sound refusing those it does not play
            constexpr std::array<int, 6> kRates = {0, -1, 999, 22050, 192000, 2'000'000};
            system.OpenStream(kRates[input.Byte() % kRates.size()], static_cast<int>(input.Byte() % 4));
            break;
        }
        case 18:
            system.GetDevice().SimulateLoss((input.Byte() & 1U) != 0);

            if (!voices.empty()) {
                (void)system.IsPlaying(voices[input.Byte() % voices.size()]);
                (void)system.IsReal(voices[input.Byte() % voices.size()]);
            }

            system.StopSounds();
            break;
        case 19: {
            // the rooms' zones loaded whole, cut or not at all, the reverb on or off
            const std::uint8_t which = input.Byte();
            std::vector<std::byte> file = fixture.zones;
            file.resize(which % 3 == 2 ? std::min<std::size_t>(file.size(), input.Byte() * 8U) : file.size());
            fixture.files["eagle/rooms.efxzones"] = file;
            system.LoadZones(which % 3 == 1 ? "" : "rooms");
            system.SetReverb((input.Byte() & 1U) != 0);
            break;
        }
        case 20:
            // the dynamic music's file or the ambient sets' from the input, read at once
            if ((input.Byte() & 1U) != 0) {
                fixture.files["ext_data/dms.dat"] = input.Bytes();
                system.BeginRegistration();
                system.StartMusic("lvl", "");
            }
            else {
                fixture.files["sound/sound.txt"] = input.Bytes();
                system.PrecacheSet("general");
                system.ParseSets();
            }

            break;
        default: {
            // the sound restarted: what it had restored into a new one, every handle kept by the input now stale
            const sound::System::State state = system.SaveState();
            fixture.system.reset();
            fixture.system = sound::System::Create(fixture.host, 48000, state.serial);
            fixture.system->RestoreState(state);
            break;
        }
        }
    }

    fixture.system->GetDevice().SimulateLoss(false);
    fixture.system->StopAll();
    return 0;
}

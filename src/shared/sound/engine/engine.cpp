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

// engine.cpp -- the sound as an engine runs it (engine.h): one sound::System, created at the start with a host that
// reads the engine's file system, prints in its console and draws from its random numbers; the settings read at every
// frame and given to the system; the console's commands; a silent Api when the sound is off, so that the modules call
// it without checking. Example: s_volume set to 0.5 in the console takes effect at the next frame, the effects and the
// cinematics at 0.125 (-18 dB, the cube of the setting), the voices at s_volumeVoice and the music at s_musicvolume as
// before.
#include "engine.h"

#include <qcommon/qcommon.h>

#include <shared/sound/system.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

// the sound's functions when the sound is off: nothing plays, every question answers nothing
class Silent final : public sound::Api {
public:
    sound::SoundHandle Register(std::string_view /*name*/) override { return sound::kNoSound; }
    sound::VoiceHandle Play(const sound::Request& /*request*/) override { return sound::VoiceHandle::None; }
    void Stop(sound::VoiceHandle /*voice*/) override {}
    bool IsPlaying(sound::VoiceHandle /*voice*/) const override { return false; }
    void MoveLoop(sound::VoiceHandle /*voice*/, const sound::Vector& /*origin*/, const sound::Vector& /*velocity*/) override {}
    void StopSlot(int /*entity*/, sound::Slot /*slot*/) override {}
    void StopEntity(int /*entity*/) override {}
    void StopSounds() override {}
    void UpdateEntity(int /*entity*/, const sound::Vector& /*origin*/, const sound::Vector& /*velocity*/) override {}
    void SetListener(const sound::Listener& /*listener*/) override {}
    sound::VoiceState GetVoiceState(int /*entity*/) const override { return sound::VoiceState::NotTalking; }
    int Length(sound::SoundHandle /*sound*/) const override { return 0; }
    void StartMusic(std::string_view /*intro*/, std::string_view /*loop*/) override {}
    void StopMusic() override {}
    void ClearPrecachedSets() override {}
    void PrecacheSet(std::string_view /*name*/) override {}
    void ParseSets() override {}
    void UpdateAmbientSet(std::string_view /*name*/, const sound::Vector& /*origin*/) override {}
    void AddLocalSet(std::string_view /*name*/, int /*entity*/, const sound::Vector& /*origin*/) override {}
    sound::SoundHandle BrushModelSound(std::string_view /*name*/, sound::BrushModelStage /*stage*/) const override { return sound::kNoSound; }
};

std::unique_ptr<sound::System> sys;
Silent silent;

// the one Api the modules keep for the whole session: it calls the sound in use, or the silent one, so that a module's
// pointer stays good when the sound restarts or is switched off and on
class Forward final : public sound::Api {
public:
    sound::SoundHandle Register(std::string_view name) override { return Target().Register(name); }
    sound::VoiceHandle Play(const sound::Request& request) override { return Target().Play(request); }
    void Stop(sound::VoiceHandle voice) override { Target().Stop(voice); }
    bool IsPlaying(sound::VoiceHandle voice) const override { return Target().IsPlaying(voice); }
    void MoveLoop(sound::VoiceHandle voice, const sound::Vector& origin, const sound::Vector& velocity) override { Target().MoveLoop(voice, origin, velocity); }
    void StopSlot(int entity, sound::Slot slot) override { Target().StopSlot(entity, slot); }
    void StopEntity(int entity) override { Target().StopEntity(entity); }
    void StopSounds() override { Target().StopSounds(); }
    void UpdateEntity(int entity, const sound::Vector& origin, const sound::Vector& velocity) override { Target().UpdateEntity(entity, origin, velocity); }
    void SetListener(const sound::Listener& listener) override { Target().SetListener(listener); }
    sound::VoiceState GetVoiceState(int entity) const override { return Target().GetVoiceState(entity); }
    int Length(sound::SoundHandle sound) const override { return Target().Length(sound); }
    void StartMusic(std::string_view intro, std::string_view loop) override { Target().StartMusic(intro, loop); }
    void StopMusic() override { Target().StopMusic(); }
    void ClearPrecachedSets() override { Target().ClearPrecachedSets(); }
    void PrecacheSet(std::string_view name) override { Target().PrecacheSet(name); }
    void ParseSets() override { Target().ParseSets(); }
    void UpdateAmbientSet(std::string_view name, const sound::Vector& origin) override { Target().UpdateAmbientSet(name, origin); }
    void AddLocalSet(std::string_view name, int entity, const sound::Vector& origin) override { Target().AddLocalSet(name, entity, origin); }
    sound::SoundHandle BrushModelSound(std::string_view name, sound::BrushModelStage stage) const override { return Target().BrushModelSound(name, stage); }

private:
    // the sound in use
    static sound::Api& Target()
    {
        if (sys) {
            return *sys;
        }

        return silent;
    }
};

Forward forward;

cvar_t* s_initsound;
cvar_t* s_volume;
cvar_t* s_volumeVoice;
cvar_t* s_musicvolume;
cvar_t* s_language;
cvar_t* s_muteWhenUnfocused;
cvar_t* s_allowDynamicMusic;
cvar_t* s_show;
cvar_t* s_debugdynamic;
cvar_t* s_reverb;

bool focused = true;
bool logging = false;

// reads the engine's file system, prints in its console, draws from its random numbers
sound::Host MakeHost()
{
    sound::Host host;
    host.readFile = [](std::string_view path) -> std::optional<std::vector<std::byte>> {
        const std::string name(path);
        void* buffer = nullptr;
        const long length = FS_ReadFile(name.c_str(), &buffer);

        if (length < 0 || buffer == nullptr) {
            return std::nullopt;
        }

        std::vector<std::byte> bytes(static_cast<std::size_t>(length));
        std::memcpy(bytes.data(), buffer, bytes.size());
        FS_FreeFile(buffer);
        return bytes;
    };
    host.fileExists = [](std::string_view path) {
        const std::string name(path);
        fileHandle_t file = 0;
        FS_FOpenFileRead(name.c_str(), &file, true);

        if (file == 0) {
            return false;
        }

        FS_FCloseFile(file);
        return true;
    };
    host.print = [](sound::Message kind, std::string_view text) {
        switch (kind) {
        case sound::Message::Info:
            Q_Println("{}", text);
            break;
        case sound::Message::Warning:
            Q_PrintlnWarning("{}", text);
            break;
        case sound::Message::Error:
            Q_PrintlnError("{}", text);
            break;
        case sound::Message::Debug:
            Q_DPrintln("{}", text);
            break;
        }
    };
    host.random = [](int min, int max) { return Q_irand(min, max); };
    return host;
}

// the voices' language from s_language's names
sound::Language LanguageOf(const char* name)
{
    if (Q_stricmp(name, "DEUTSCH") == 0) {
        return sound::Language::German;
    }

    if (Q_stricmp(name, "FRANCAIS") == 0) {
        return sound::Language::French;
    }

    if (Q_stricmp(name, "ESPANOL") == 0) {
        return sound::Language::Spanish;
    }

    return sound::Language::English;
}

// play <sound> ...: each sound at the listener, as a menu's
void Play_f()
{
    for (int i = 1; i < Cmd_Argc(); ++i) {
        sound::Request request;
        request.sound = SND_Api().Register(Cmd_Argv(i));
        request.soundClass = sound::SoundClass::Interface;
        request.place = sound::Place::Listener;
        SND_Api().Play(request);
    }
}

// music <intro> [loop]
void Music_f()
{
    if (Cmd_Argc() == 2) {
        SND_Api().StartMusic(Cmd_Argv(1), Cmd_Argv(1));
    }
    else if (Cmd_Argc() == 3) {
        SND_Api().StartMusic(Cmd_Argv(1), Cmd_Argv(2));
    }
    else {
        Q_Println("music <musicfile> [loopfile]");
    }
}

// stopmusic: the music stopped
void StopMusic_f()
{
    SND_Api().StopMusic();
}

// every sound registered, with its length, its rate and its memory
void SoundList_f()
{
    if (!sys) {
        return;
    }

    const std::vector<std::string> lines = sys->List();

    for (const std::string& line : lines) {
        Q_Println("{}", line);
    }

    Q_Println("{} sounds registered", lines.size());
}

// soundinfo: the device, the voices, the sounds registered, the music
void SoundInfo_f()
{
    if (!sys) {
        Q_Println("the sound is not started");
        return;
    }

    Q_Println("{}", sys->Info());
}

// soundstop: every sound and the music stopped
void SoundStop_f()
{
    SND_StopAll();
}

// s_dynamic <explore|action|boss|death|silence>: the dynamic music's state, for testing
void Dynamic_f()
{
    if (Cmd_Argc() != 2) {
        Q_Println("s_dynamic <explore|action|boss|death|silence>");
        return;
    }

    const char* name = Cmd_Argv(1);
    sound::MusicState state = sound::MusicState::Explore;

    if (Q_stricmp(name, "action") == 0) {
        state = sound::MusicState::Action;
    }
    else if (Q_stricmp(name, "boss") == 0) {
        state = sound::MusicState::Boss;
    }
    else if (Q_stricmp(name, "death") == 0) {
        state = sound::MusicState::Death;
    }
    else if (Q_stricmp(name, "silence") == 0) {
        state = sound::MusicState::Silence;
    }

    SND_SetMusicState(state);
}

constexpr const char* kCommands[] = {"play", "music", "stopmusic", "soundlist", "soundinfo", "soundstop", "s_dynamic"};

// a volume setting as a gain: its cube, so that a slider moves the loudness evenly over its whole course, where a linear
// gain crowded the settings heard near 0 (0.05 was -26 dB); now 0.36 gives -26 dB, 0.5 -18 dB, 1 the sounds as recorded
float VolumeGain(float setting)
{
    const float position = std::max(setting, 0.0f);
    return position * position * position;
}

// the settings given to the system, at every frame since the console changes them at any time
void ApplySettings()
{
    sys->SetVolumes(VolumeGain(s_volume->value), VolumeGain(s_volumeVoice->value), VolumeGain(s_musicvolume->value));
    sys->SetMuteWhenUnfocused(s_muteWhenUnfocused->integer != 0);
    sys->AllowDynamicMusic(s_allowDynamicMusic->integer != 0);
    sys->SetMusicDebug(s_debugdynamic->integer != 0);
    sys->SetReverb(s_reverb->integer != 0);

    // s_show 1: the sound log, each line of it in the console
    const bool log = s_show->integer == 1;

    if (log != logging) {
        logging = log;
        sys->SetLog(log ? std::function<void(std::string_view)>([](std::string_view line) { Q_Println("{}", line); }) : nullptr);
    }
}

// the settings registered, then the sound on the default device; serial the first voice's serial number, so that a
// restarted sound gives no handle twice
void Start(std::uint32_t serial)
{
    Q_Println("\n------- sound initialization -------");
    s_initsound = Cvar_Get("s_initsound", "1", cvarFlags_t::CVAR_ARCHIVE);
    s_volume = Cvar_Get("s_volume", "0.79", cvarFlags_t::CVAR_ARCHIVE);
    s_volumeVoice = Cvar_Get("s_volumeVoice", "1.0", cvarFlags_t::CVAR_ARCHIVE);
    s_musicvolume = Cvar_Get("s_musicvolume", "0.63", cvarFlags_t::CVAR_ARCHIVE);
    s_language = Cvar_Get("s_language", "english", cvarFlags_t::CVAR_ARCHIVE | cvarFlags_t::CVAR_NORESTART);
    s_muteWhenUnfocused = Cvar_Get("s_muteWhenUnfocused", "1", cvarFlags_t::CVAR_ARCHIVE);
    s_allowDynamicMusic = Cvar_Get("s_allowDynamicMusic", "1", cvarFlags_t::CVAR_ARCHIVE_ND);
    s_show = Cvar_Get("s_show", "0", cvarFlags_t::CVAR_CHEAT);
    s_debugdynamic = Cvar_Get("s_debugdynamic", "0", cvarFlags_t::CVAR_CHEAT);
    s_reverb = Cvar_Get("s_reverb", "1", cvarFlags_t::CVAR_ARCHIVE);

    if (s_initsound->integer == 0) {
        Q_Println("not initializing.");
        return;
    }

    sys = sound::System::Create(MakeHost(), std::nullopt, serial);

    if (!sys) {
        return;
    }

    Cmd_AddCommand("play", Play_f);
    Cmd_AddCommand("music", Music_f);
    Cmd_AddCommand("stopmusic", StopMusic_f);
    Cmd_AddCommand("soundlist", SoundList_f);
    Cmd_AddCommand("soundinfo", SoundInfo_f);
    Cmd_AddCommand("soundstop", SoundStop_f);
    Cmd_AddCommand("s_dynamic", Dynamic_f);
    logging = false;
    sys->SetLanguage(LanguageOf(s_language->string));
    sys->SetFocus(focused);
    ApplySettings();
}

} // namespace

void SND_Init()
{
    Start(1);
}

void SND_Shutdown()
{
    if (!sys) {
        return;
    }

    for (const char* command : kCommands) {
        Cmd_RemoveCommand(command);
    }

    sys.reset();
}

void SND_Restart()
{
    const sound::System::State state = sys ? sys->SaveState() : sound::System::State{};
    SND_Shutdown();
    Start(state.serial);

    if (sys) {
        sys->RestoreState(state);
    }
}

void SND_Frame(int now)
{
    if (!sys) {
        return;
    }

    ApplySettings();
    sys->Frame(now);
}

// the voices' language read when a level starts, since its voices are loaded then
void SND_BeginRegistration()
{
    if (sys) {
        sys->SetLanguage(LanguageOf(s_language->string));
        sys->BeginRegistration();
    }
}

void SND_EndRegistration()
{
    if (sys) {
        sys->EndRegistration();
    }
}

void SND_StopSounds()
{
    if (sys) {
        sys->StopSounds();
    }
}

void SND_StopAll()
{
    if (sys) {
        sys->StopAll();
    }
}

void SND_SetFocus(bool hasFocus)
{
    focused = hasFocus;

    if (sys) {
        sys->SetFocus(focused);
    }
}

void SND_RestartMusic()
{
    if (sys) {
        sys->RestartMusic();
    }
}

void SND_LoadZones(const char* map)
{
    if (sys) {
        sys->LoadZones(map);
    }
}

void SND_SetMusicLevel(const char* level)
{
    if (sys) {
        sys->SetMusicLevel(level);
    }
}

void SND_SetMusicState(sound::MusicState state)
{
    if (sys) {
        sys->SetMusicState(state);
    }
}

void SND_OpenStream(int rate, int channels)
{
    if (sys) {
        sys->OpenStream(rate, channels);
    }
}

void SND_QueueStream(std::span<const std::int16_t> samples)
{
    if (sys) {
        sys->QueueStream(samples);
    }
}

void SND_CloseStream()
{
    if (sys) {
        sys->CloseStream();
    }
}

sound::Api& SND_Api()
{
    return forward;
}

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

// music.cpp -- the music (music.h): a file's intro then its loop, the loop read from its start without a gap when it
// has the intro's rate and channels; a level's dynamic music, its files read once when it starts, the piece playing
// and the piece fading out each on a stream of its own, the state's changes made when the rules of dms.h allow them.
// Example: a fight starts in yavin1: swamp_action starts at full volume at one of its markers while swamp_explore fades
// out; when it ends, swamp_action plays on to one of its exit times, then its transition piece, then swamp_explore from
// the marker the transition leads to.
#include "music.h"

#include <shared/sound/names.h>

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <utility>

namespace sound {

namespace {

// a cross-fade's length; a piece loops this long twice before its end
constexpr int kFadeMilliseconds = 1000;
constexpr float kLoopBeforeEnd = 2.0f * kFadeMilliseconds / 1000.0f;

constexpr std::string_view kDynamicMusicFile = "ext_data/dms.dat";

// every track a dynamic music may need
constexpr std::array kTracks = {Track::Explore,
                                Track::Action,
                                Track::Boss,
                                Track::Death,
                                Track::ActionTransition0,
                                Track::ActionTransition1,
                                Track::ActionTransition2,
                                Track::ActionTransition3,
                                Track::ExploreTransition0,
                                Track::ExploreTransition1,
                                Track::ExploreTransition2,
                                Track::ExploreTransition3};

// a name without its folder
std::string_view WithoutFolder(std::string_view name)
{
    const std::size_t slash = name.find_last_of("/\\");
    return slash == std::string_view::npos ? name : name.substr(slash + 1);
}

// the track a state plays from its start, nullopt for silence
std::optional<Track> TrackOf(DynamicState state) noexcept
{
    switch (state) {
    case DynamicState::Explore:
        return Track::Explore;
    case DynamicState::Action:
        return Track::Action;
    case DynamicState::Boss:
        return Track::Boss;
    case DynamicState::Death:
        return Track::Death;
    case DynamicState::Transition:
    case DynamicState::Silence:
        break;
    }

    return std::nullopt;
}

// what the debug lines call a state
std::string_view StateName(DynamicState state) noexcept
{
    switch (state) {
    case DynamicState::Explore:
        return "explore";
    case DynamicState::Action:
        return "action";
    case DynamicState::Boss:
        return "boss";
    case DynamicState::Death:
        return "death";
    case DynamicState::Transition:
        return "transition";
    case DynamicState::Silence:
        return "silence";
    }

    return "";
}

} // namespace

// a file named as asked, else a dynamic music (an explicit file needs a folder in its name, a level's name has none)
void Music::Start(std::string_view intro, std::string_view loop)
{
    Stop();
    silent_ = false;
    lastIntro_ = intro;
    lastLoop_ = loop;

    if (loop.empty()) {
        loop = intro;
    }

    const std::string introName = NormalizeSoundName(intro);
    const std::string loopName = NormalizeSoundName(loop);
    std::string label(WithoutFolder(introName));

    // the level's name without its folder, as dms.dat names it (map test/foo is foo)
    if (label.empty()) {
        const std::string level = NormalizeSoundName(level_);
        label = WithoutFolder(level);
    }

    if (!allowDynamic_ && LoadDynamic(label)) {
        const std::string explore = LowerSoundName(TrackFile(*dynamic_, Track::Explore));
        dynamic_.reset();
        tracks_.clear();
        StartPlain(explore, explore);
        return;
    }

    if (introName.contains('/') && MusicExists(introName)) {
        StartPlain(introName, MusicExists(loopName) ? loopName : introName);
        return;
    }

    if (LoadDynamic(label)) {
        actual_.reset();
        requested_ = DynamicState::Explore;
        Switch(Track::Explore, false, 0.0f);
        return;
    }

    if (!introName.empty()) {
        host_.print(Message::Warning, fmt::format("unable to find music \"{}\" as a file or a dynamic music", introName));
    }
}

void Music::Restart()
{
    const std::string intro = lastIntro_;
    const std::string loop = lastLoop_;
    Start(intro, loop);
}

// the setting changed: the music playing started again under it, a dynamic music or its explore piece as a file
void Music::AllowDynamic(bool allowed)
{
    if (allowed == allowDynamic_) {
        return;
    }

    allowDynamic_ = allowed;

    if (current_ || dynamic_) {
        Restart();
    }
}

void Music::ForgetLoaded()
{
    cachedLevel_.clear();
    cachedMusic_.reset();
    cachedTracks_.clear();
}

// the music stopped at once; the music asked for kept for Restart, the last level's dynamic music for its next start
void Music::Stop()
{
    current_.reset();
    fading_.reset();
    dynamic_.reset();
    tracks_.clear();
    actual_.reset();
    loop_.clear();
}

// the state as the game names it; boss and death only when the level has them, else the state stays
void Music::SetState(MusicState state)
{
    silent_ = state == MusicState::Silence;

    switch (state) {
    case MusicState::Explore:
        requested_ = DynamicState::Explore;
        break;
    case MusicState::Action:
        requested_ = DynamicState::Action;
        break;
    case MusicState::Boss:
        if (tracks_.contains(Track::Boss)) {
            requested_ = DynamicState::Boss;
        }

        break;
    case MusicState::Death:
        if (tracks_.contains(Track::Death)) {
            requested_ = DynamicState::Death;
        }

        break;
    case MusicState::Silence:
        requested_ = DynamicState::Silence;
        break;
    }
}

void Music::Frame(int now)
{
    now_ = now;

    if (dynamic_) {
        FrameDynamic();
    }
    else {
        FramePlain();
    }

    if (fading_) {
        Feed(*fading_, volume_ * Fade(*fading_));

        if (now_ - fading_->fadeStart >= kFadeMilliseconds || fading_->stream->Ended()) {
            fading_.reset();
        }
    }
}

std::string Music::Describe() const
{
    const std::string fading = fading_ ? fmt::format(", fading {}", fading_->name) : std::string();

    if (dynamic_) {
        const std::string playing = current_ ? current_->name : std::string("nothing");
        return fmt::format("dynamic music {} ({}): {}, playing {}{}", dynamic_->level, dynamic_->set, StateName(Actual()), playing, fading);
    }

    if (current_) {
        return fmt::format("music {}, then {}{}", current_->name, loop_, fading);
    }

    return "no music" + fading;
}

// a music's file by a name already keyed (normalized once, so that a name holding a dot keeps it)
std::shared_ptr<const std::vector<std::byte>> Music::ReadMusic(std::string_view name) const
{
    std::optional<std::vector<std::byte>> file = ReadSoundFile(host_, name);
    return file ? std::make_shared<const std::vector<std::byte>>(std::move(*file)) : nullptr;
}

// whether a music's file of a name already keyed exists
bool Music::MusicExists(std::string_view name) const
{
    return !name.empty() && SoundFileExists(host_, name);
}

// a player of a file from a time in seconds, its stream at the file's rate and channels; nullopt when it does not decode
std::unique_ptr<Music::Player> Music::Open(std::shared_ptr<const std::vector<std::byte>> file, std::string_view name, float seconds) const
{
    if (file == nullptr) {
        return nullptr;
    }

    std::unique_ptr<Decoder> decoder = OpenDecoder(*file);

    if (decoder == nullptr) {
        host_.print(Message::Warning, fmt::format("could not decode music {}", name));
        return nullptr;
    }

    auto player = std::make_unique<Player>();
    player->file = std::move(file);
    player->name = name;
    player->fadeStart = now_;
    player->frames = decoder->Frames();

    // a time the data gives, kept within the file (a time of 1e30 seconds would not fit the frame's count)
    const float length = player->frames ? static_cast<float>(*player->frames) / static_cast<float>(decoder->Rate()) : 3600.0f;
    seconds = std::clamp(seconds, 0.0f, length);

    if (seconds > 0.0f) {
        player->startFrame = static_cast<std::uint64_t>(seconds * static_cast<float>(decoder->Rate()));

        if (!decoder->Seek(player->startFrame)) {
            player->startFrame = 0;
        }
    }

    player->stream = std::make_unique<Stream>(decoder->Rate(), decoder->Channels(), resampler_);
    player->decoder = std::move(decoder);
    return player;
}

// ext_data/dms.dat read for the level, the files of its tracks found and read; false when it has no dynamic music or
// lacks the file of its explore or action piece, warned; another piece's file missing warned and that piece left out, as
// the original game did; the last level's kept
bool Music::LoadDynamic(std::string_view level)
{
    dynamic_.reset();
    tracks_.clear();

    if (level.empty()) {
        return false;
    }

    if (cachedMusic_ && level == cachedLevel_) {
        dynamic_ = std::make_unique<DynamicMusic>(*cachedMusic_);
        tracks_ = cachedTracks_;
        return true;
    }

    const std::optional<std::vector<std::byte>> text = host_.readFile(kDynamicMusicFile);

    if (!text) {
        return false;
    }

    std::string error;
    std::optional<DynamicMusic> music = ParseDynamicMusic(ToText(*text), level, error);

    if (!music) {
        if (!error.empty()) {
            host_.print(Message::Warning, fmt::format("{}: {}", kDynamicMusicFile, error));
        }

        return false;
    }

    if (!music->explore || !music->action) {
        host_.print(Message::Warning, fmt::format("the dynamic music of {} lacks its explore or action piece", level));
        return false;
    }

    for (const Track track : kTracks) {
        const std::string file = TrackFile(*music, track);

        if (file.empty()) {
            continue;
        }

        std::shared_ptr<const std::vector<std::byte>> bytes = ReadMusic(LowerSoundName(file));

        if (bytes == nullptr) {
            host_.print(Message::Warning, fmt::format("the dynamic music of {} lacks its file {}", level, file));

            if (track == Track::Explore || track == Track::Action) {
                tracks_.clear();
                return false;
            }

            continue;
        }

        tracks_.emplace(track, std::move(bytes));
    }

    cachedLevel_ = level;
    cachedMusic_ = *music;
    cachedTracks_ = tracks_;
    dynamic_ = std::make_unique<DynamicMusic>(std::move(*music));
    return true;
}

// a player's fade now, along a straight line over a second
float Music::Fade(const Player& player) const noexcept
{
    const float t = std::clamp(static_cast<float>(now_ - player.fadeStart) / static_cast<float>(kFadeMilliseconds), 0.0f, 1.0f);
    return player.fadeFrom + ((player.fadeTo - player.fadeFrom) * t);
}

// the seconds of its file a player has played
float Music::Elapsed(const Player& player)
{
    const auto frames = static_cast<std::int64_t>(player.startFrame) + player.stream->PlayedFrames();
    return static_cast<float>(frames) / static_cast<float>(player.stream->Rate());
}

// the state the dynamic music plays
DynamicState Music::Actual() const noexcept
{
    if (!actual_) {
        return DynamicState::Silence;
    }

    switch (*actual_) {
    case Track::Explore:
        return DynamicState::Explore;
    case Track::Action:
        return DynamicState::Action;
    case Track::Boss:
        return DynamicState::Boss;
    case Track::Death:
        return DynamicState::Death;
    default:
        return DynamicState::Transition;
    }
}

// a music that is not dynamic: its intro from its start, its loop after it
void Music::StartPlain(std::string_view intro, std::string_view loop)
{
    loop_ = loop;
    current_ = Open(ReadMusic(intro), intro, 0.0f);
}

// the piece playing fades out from where its fade is; a new piece fades in, or starts at full volume
void Music::Switch(std::optional<Track> track, bool startsFull, float seconds)
{
    if (current_) {
        current_->fadeFrom = Fade(*current_);
        current_->fadeTo = 0.0f;
        current_->fadeStart = now_;
        fading_ = std::move(current_);
    }

    actual_ = track;

    if (track) {
        const auto file = tracks_.find(*track);
        current_ = file != tracks_.end() ? Open(file->second, TrackFile(*dynamic_, *track), seconds) : nullptr;

        if (current_) {
            current_->track = track;
            current_->fadeFrom = startsFull ? 1.0f : 0.0f;
            current_->fadeTo = 1.0f;
        }
    }

    if (debug_) {
        host_.print(Message::Info, fmt::format("dynamic music: {}", current_ ? current_->name : std::string("silence")));
    }
}

// the state asked for, when what plays allows it: action to explore and explore or action to silence only at one of
// the piece's exit times, through its transition piece; the others at once, action at one of its entry markers
void Music::ChangeState()
{
    const DynamicState actual = Actual();

    if (requested_ == actual || !CanInterrupt(actual, requested_)) {
        return;
    }

    const auto throughTransition = [this](Track piece, DynamicState next) {
        if (!current_) {
            return;
        }

        if (const std::optional<Transition> transition = AllowedTransition(*dynamic_, piece, Elapsed(*current_))) {
            // a transition piece the level's files lack (a mod's): straight to the next state's piece
            if (!tracks_.contains(transition->track)) {
                Switch(next == DynamicState::Explore ? std::optional<Track>(Track::Explore) : std::nullopt, false, transition->entryTime);
                return;
            }

            Switch(transition->track, false, 0.0f);

            if (current_) {
                current_->next = next;
                current_->nextTime = transition->entryTime;
            }
        }
    };

    switch (requested_) {
    case DynamicState::Explore:
        if (actual == DynamicState::Action) {
            throughTransition(Track::Action, DynamicState::Explore);
        }
        else if (actual == DynamicState::Silence) {
            Switch(Track::Explore, false, 0.0f);
        }
        else {
            Switch(std::nullopt, false, 0.0f);
        }

        break;
    case DynamicState::Silence:
        if (actual == DynamicState::Action || actual == DynamicState::Explore) {
            throughTransition(actual == DynamicState::Action ? Track::Action : Track::Explore, DynamicState::Silence);
        }
        else {
            Switch(std::nullopt, false, 0.0f);
        }

        break;
    case DynamicState::Action:
        if (actual == DynamicState::Silence) {
            Switch(Track::Action, false, 0.0f);
        }
        else {
            const std::vector<float> entries = EntryTimes(*dynamic_, Track::Action);
            float entry = 0.0f;

            // a random entry marker, not the same twice in a row
            if (!entries.empty()) {
                int draw = host_.random(0, static_cast<int>(entries.size()) - 1);

                if (lastEntry_ == draw && entries.size() > 1) {
                    draw = (draw + 1) % static_cast<int>(entries.size());
                }

                lastEntry_ = draw;
                entry = entries[static_cast<std::size_t>(draw)];
            }

            Switch(Track::Action, true, entry);
        }

        break;
    case DynamicState::Boss:
        Switch(Track::Boss, false, 0.0f);
        break;
    case DynamicState::Death:
        Switch(Track::Death, true, 0.0f);
        break;
    case DynamicState::Transition:
        break;
    }
}

// a player's stream filled from its decoder, the music that is not dynamic going on into its loop
void Music::Feed(Player& player, float gain)
{
    player.stream->SetGain(gain);
    player.stream->Update([this, &player](std::span<std::int16_t> out) -> std::size_t {
        if (player.ended) {
            return 0;
        }

        if (!player.track && !dynamic_) {
            return FillPlain(player, out);
        }

        const std::size_t frames = player.decoder->Read(out);
        player.ended = frames == 0;
        return frames;
    });
}

// the intro's frames, then the loop's from its start, without a gap when the loop has the intro's rate and channels; a
// loop whose file has gone ends the music (Start replaces a loop that is no file by the intro)
std::size_t Music::FillPlain(Player& player, std::span<std::int16_t> out)
{
    const std::size_t frames = player.decoder->Read(out);

    if (frames > 0) {
        return frames;
    }

    // the same file again from its start, without reading it again
    if (player.name == loop_) {
        const std::size_t again = player.decoder->Seek(0) ? player.decoder->Read(out) : 0;
        player.ended = again == 0;
        return again;
    }

    if (!MusicExists(loop_)) {
        player.ended = true;
        return 0;
    }

    std::shared_ptr<const std::vector<std::byte>> file = ReadMusic(loop_);
    std::unique_ptr<Decoder> decoder = file != nullptr ? OpenDecoder(*file) : nullptr;

    if (decoder == nullptr || decoder->Rate() != player.stream->Rate() || decoder->Channels() != player.stream->Channels()) {
        player.ended = true;
        return 0;
    }

    player.file = std::move(file);
    player.decoder = std::move(decoder);
    player.name = loop_;

    // a loop that gives no frame ends the music instead of being read again at every frame
    const std::size_t first = player.decoder->Read(out);
    player.ended = first == 0;
    return first;
}

// the music that is not dynamic: silent while the game asks for silence; a loop of another format started on a stream
// of its own once the intro has played out
void Music::FramePlain()
{
    if (!current_) {
        return;
    }

    Feed(*current_, silent_ ? 0.0f : volume_);

    // a loop of another format than the intro's, on a stream of its own; the same file again would play nothing more
    if (current_->ended && current_->stream->Ended()) {
        const bool another = current_->name != loop_ && MusicExists(loop_);
        current_ = another ? Open(ReadMusic(loop_), loop_, 0.0f) : nullptr;
    }
}

// the state changed when allowed; the piece playing fed, and two seconds before its end either followed by what its
// transition leads to, or looped by fading its start in over its end (the death piece fading to silence)
void Music::FrameDynamic()
{
    ChangeState();

    if (!current_) {
        return;
    }

    Feed(*current_, volume_ * Fade(*current_));

    // a piece too short to fade over its own end, or of a length its file does not give, moves on at its end
    const std::optional<std::uint64_t> frames = current_->frames;
    const float length = frames ? static_cast<float>(*frames) / static_cast<float>(current_->stream->Rate()) : 0.0f;
    const bool fades = length > 2.0f * kLoopBeforeEnd;
    const bool atEnd = current_->ended && current_->stream->Ended();

    if (!atEnd && (!fades || length - Elapsed(*current_) >= kLoopBeforeEnd)) {
        return;
    }

    const Player& playing = *current_;

    if (playing.next) {
        const DynamicState next = *playing.next;
        const float time = playing.nextTime;
        Switch(TrackOf(next), false, time);
        return;
    }

    if (!playing.track) {
        return;
    }

    const Track track = *playing.track;
    Switch(track, false, 0.0f);

    if (current_ && track == Track::Death) {
        current_->fadeTo = 0.0f;
    }
}

} // namespace sound

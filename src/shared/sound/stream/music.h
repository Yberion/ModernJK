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

// music.h -- the music: a file streamed, an intro then a loop; or a level's dynamic music (dms.h), its pieces
// cross-faded over a second as its state changes (explore, action, boss, death, silence), a change waiting for one of
// the piece's exit times and played through its transition piece, a piece looped by fading its start in over its end.
// Example: Start("music/mp/duel", "") plays a file and loops it; Start("yavin1", "") plays that level's dynamic music,
// and SetState(MusicState::Action) its fight.
#pragma once

#include "dms.h"
#include "stream.h"

#include <shared/sound/decoder/decoder.h>
#include <shared/sound/host.h>
#include <shared/sound/sound.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sound {

class Music {
public:
    Music(const Host& host, int resampler) : host_(host), resampler_(resampler) {}
    Music(const Music&) = delete;
    Music& operator=(const Music&) = delete;
    Music(Music&&) = delete;
    Music& operator=(Music&&) = delete;
    ~Music() = default;

    // An intro then its loop (the intro again when the loop is empty) when the intro is a file, or else the dynamic
    // music of the level the intro names (without its folder), or of the level set by SetLevel when the intro is empty.
    void Start(std::string_view intro, std::string_view loop);
    void Stop();
    // Plays again the music Start was last asked for, from its start (after a cinematic, or the sound restarted).
    void Restart();
    // The level SetLevel gave.
    [[nodiscard]] const std::string& Level() const noexcept { return level_; }
    // The intro and the loop Start was last asked for.
    [[nodiscard]] const std::string& LastIntro() const noexcept { return lastIntro_; }
    [[nodiscard]] const std::string& LastLoop() const noexcept { return lastLoop_; }
    // The level the dynamic music plays when a music asks for none by name.
    void SetLevel(std::string_view level) { level_ = level; }
    // The dynamic music's state the game asks for; silence also mutes a music that is not dynamic.
    void SetState(MusicState state);
    void SetVolume(float volume) noexcept { volume_ = volume; }
    // Whether a level's dynamic music may play: when not, its explore piece plays as a music that is not dynamic.
    void AllowDynamic(bool allowed) noexcept { allowDynamic_ = allowed; }
    // Whether the dynamic music's changes are printed.
    void SetDebug(bool debug) noexcept { debug_ = debug; }
    // At every frame, now in milliseconds: the streams filled, the fades advanced, the state's changes made.
    void Frame(int now);
    // What plays, for the soundinfo command.
    [[nodiscard]] std::string Describe() const;

private:
    struct Player {
        std::shared_ptr<const std::vector<std::byte>> file;
        std::unique_ptr<Decoder> decoder;
        std::unique_ptr<Stream> stream;
        std::string name;
        std::optional<Track> track;          // its piece of the dynamic music
        std::uint64_t startFrame = 0;        // where the decoder started
        std::optional<std::uint64_t> frames; // the file's length
        float fadeFrom = 1.0f;
        float fadeTo = 1.0f;
        int fadeStart = 0;
        bool ended = false;               // the decoder gave its last frame
        std::optional<DynamicState> next; // the state a transition piece leads to, at its end
        float nextTime = 0.0f;            // where the piece after it starts
    };

    [[nodiscard]] std::shared_ptr<const std::vector<std::byte>> ReadMusic(std::string_view name) const;
    [[nodiscard]] bool MusicExists(std::string_view name) const;
    [[nodiscard]] std::unique_ptr<Player> Open(std::shared_ptr<const std::vector<std::byte>> file, std::string_view name, float seconds) const;
    [[nodiscard]] bool LoadDynamic(std::string_view level);
    [[nodiscard]] float Fade(const Player& player) const noexcept;
    [[nodiscard]] static float Elapsed(const Player& player);
    [[nodiscard]] DynamicState Actual() const noexcept;
    void StartPlain(std::string_view intro, std::string_view loop);
    void Switch(std::optional<Track> track, bool startsFull, float seconds);
    void ChangeState();
    void Feed(Player& player, float gain);
    void FramePlain();
    void FrameDynamic();
    [[nodiscard]] std::size_t FillPlain(Player& player, std::span<std::int16_t> out);

    const Host& host_;
    int resampler_ = 0;
    std::unique_ptr<Player> current_;
    std::unique_ptr<Player> fading_;
    // a music that is not dynamic
    std::string loop_;
    std::string pendingStart_; // a loop naming a dynamic music, started at the next frame
    bool silent_ = false;
    // the dynamic music
    std::unique_ptr<DynamicMusic> dynamic_;
    std::map<Track, std::shared_ptr<const std::vector<std::byte>>> tracks_;
    std::optional<Track> actual_; // nullopt: silence
    DynamicState requested_ = DynamicState::Explore;
    std::optional<int> lastEntry_; // the action piece's entry marker drawn last, none yet
    float volume_ = 1.0f;
    bool allowDynamic_ = true;
    bool debug_ = false;
    std::string level_;
    std::string lastIntro_;
    std::string lastLoop_;
    int now_ = 0;
};

} // namespace sound

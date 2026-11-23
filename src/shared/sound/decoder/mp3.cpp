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

// The MP3 decoder, over dr_mp3 (minimp3's decoder): MPEG-1, 2 and 2.5, layers I to III. It skips the ID3 tags, and the
// frame of a Xing or LAME header with the encoder's delay and padding where that header gives them, so that a sound
// starts with its first sample.
#include "formats.h"

#define DR_MP3_IMPLEMENTATION
#define DR_MP3_NO_STDIO
#include <dr_mp3.h>

#include <vector>

namespace sound {

namespace {

class Mp3 final : public Decoder {
public:
    Mp3() = default;
    // dr_mp3 keeps a pointer to its own state: the decoder is neither copied nor moved
    Mp3(const Mp3&) = delete;
    Mp3& operator=(const Mp3&) = delete;
    Mp3(Mp3&&) = delete;
    Mp3& operator=(Mp3&&) = delete;
    // frees what dr_mp3 allocated, if it opened the file
    ~Mp3() override
    {
        if (open_) {
            drmp3_uninit(&mp3_);
        }
    }

    // dr_mp3 reads the file through a pointer to its own state, so the state is set up where it stays
    [[nodiscard]] bool Open(std::span<const std::byte> file) noexcept
    {
        open_ = drmp3_init_memory(&mp3_, file.data(), file.size(), nullptr) != DRMP3_FALSE;

        if (open_) {
            SetFormat(static_cast<int>(mp3_.sampleRate), static_cast<int>(mp3_.channels));
        }

        return open_;
    }

    // dr_mp3 decodes frame by frame, without the encoder's delay and padding when the file's header gives them
    std::size_t Read(std::span<std::int16_t> out) override
    {
        return static_cast<std::size_t>(drmp3_read_pcm_frames_s16(&mp3_, out.size() / static_cast<std::size_t>(Channels()), out.data()));
    }

    // dr_mp3 alone decodes every frame up to the one asked (23 ms for 60 s of music): the first seek beyond the start
    // gives it a table of 256 points over the file (0.7 ms to build, then 0.2 ms for any seek)
    bool Seek(std::uint64_t frame) override
    {
        if (frame > 0 && seekPoints_.empty()) {
            drmp3_uint32 count = kSeekPoints;
            seekPoints_.resize(count);

            if (drmp3_calculate_seek_points(&mp3_, &count, seekPoints_.data()) != DRMP3_FALSE && count > 0) {
                seekPoints_.resize(count);
                drmp3_bind_seek_table(&mp3_, count, seekPoints_.data());
            }
            else {
                seekPoints_.assign(1, drmp3_seek_point{});
            }
        }

        return drmp3_seek_to_pcm_frame(&mp3_, frame) != DRMP3_FALSE;
    }

    // dr_mp3 counts them through the file's frames, then goes back to where it was: the music asks once per piece
    std::optional<std::uint64_t> Frames() override
    {
        const drmp3_uint64 frames = drmp3_get_pcm_frame_count(&mp3_);
        return frames > 0 ? std::optional<std::uint64_t>(frames) : std::nullopt;
    }

private:
    static constexpr drmp3_uint32 kSeekPoints = 256;

    drmp3 mp3_{};
    bool open_ = false;
    std::vector<drmp3_seek_point> seekPoints_; // the seek table dr_mp3 keeps a pointer to, built at the first seek
};

} // namespace

// an Mp3 set up on the file, or nullptr when dr_mp3 finds no frame in it
std::unique_ptr<Decoder> OpenMp3(std::span<const std::byte> file)
{
    auto decoder = std::make_unique<Mp3>();

    if (!decoder->Open(file)) {
        return nullptr;
    }

    return decoder;
}

} // namespace sound

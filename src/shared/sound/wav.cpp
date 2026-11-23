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

// The WAV decoder, over dr_wav: PCM of every width, IEEE floats, ADPCM, A-law and µ-law, in RIFF, RF64 and Wave64.
#include "formats.h"

#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#define DR_WAV_NO_WCHAR
#include <dr_wav.h>

namespace sound {

namespace {

class Wav final : public Decoder {
public:
    Wav() = default;
    // dr_wav keeps a pointer to its own state: the decoder is neither copied nor moved
    Wav(const Wav&) = delete;
    Wav& operator=(const Wav&) = delete;
    Wav(Wav&&) = delete;
    Wav& operator=(Wav&&) = delete;
    // frees what dr_wav allocated, if it opened the file
    ~Wav() override
    {
        if (open_) {
            drwav_uninit(&wav_);
        }
    }

    // dr_wav reads the file through a pointer to its own state, so the state is set up where it stays
    [[nodiscard]] bool Open(std::span<const std::byte> file) noexcept
    {
        open_ = drwav_init_memory(&wav_, file.data(), file.size(), nullptr) != DRWAV_FALSE;

        if (open_) {
            SetFormat(static_cast<int>(wav_.sampleRate), wav_.channels);
        }

        return open_;
    }

    // dr_wav converts every encoding to 16-bit samples as it reads
    std::size_t Read(std::span<std::int16_t> out) override
    {
        return static_cast<std::size_t>(drwav_read_pcm_frames_s16(&wav_, out.size() / static_cast<std::size_t>(Channels()), out.data()));
    }

    // dr_wav goes straight to the frame in PCM, and decodes from the start in the compressed encodings
    bool Seek(std::uint64_t frame) override { return drwav_seek_to_pcm_frame(&wav_, frame) != DRWAV_FALSE; }

private:
    drwav wav_{};
    bool open_ = false;
};

} // namespace

// a Wav set up on the file, or nullptr when dr_wav does not read it
std::unique_ptr<Decoder> OpenWav(std::span<const std::byte> file)
{
    auto decoder = std::make_unique<Wav>();

    if (!decoder->Open(file)) {
        return nullptr;
    }

    return decoder;
}

} // namespace sound

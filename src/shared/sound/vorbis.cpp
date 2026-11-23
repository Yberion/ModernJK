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

// The Ogg Vorbis decoder, over stb_vorbis (compiled in its own library from stb_vorbis.c; its declarations here).
#include "formats.h"

#include <algorithm>
#include <array>
#include <climits>
#include <utility>

// stb_vorbis comes as one .c file, whose declarations alone this define gives
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c> // NOLINT(bugprone-suspicious-include)

namespace sound {

namespace {

// stb_vorbis allocates what a file declares (the vendor's text of a 422-byte file asked for 1.7 GB): all its memory
// comes from a block of ours instead, as its documentation offers, so that such a file fails to open. The survey's 453
// files need at most 202 KB (setup and temporary memory, 23 November 2026), its author's at most about 150 KB.
constexpr int kMemory = 1024 * 1024;
using Memory = std::array<char, kMemory>;

class Vorbis final : public Decoder {
public:
    // takes over an opened stb_vorbis and the memory it lives in, and reads the format from it
    Vorbis(stb_vorbis* vorbis, std::unique_ptr<Memory> memory) noexcept : vorbis_(vorbis), memory_(std::move(memory))
    {
        const stb_vorbis_info info = stb_vorbis_get_info(vorbis_);
        SetFormat(static_cast<int>(info.sample_rate), info.channels);
    }
    // it owns the stb_vorbis: neither copied nor moved
    Vorbis(const Vorbis&) = delete;
    Vorbis& operator=(const Vorbis&) = delete;
    Vorbis(Vorbis&&) = delete;
    Vorbis& operator=(Vorbis&&) = delete;
    // closes stb_vorbis before its memory goes with the members
    ~Vorbis() override { stb_vorbis_close(vorbis_); }

    // stb_vorbis decodes packet by packet into interleaved 16-bit samples
    std::size_t Read(std::span<std::int16_t> out) override
    {
        const auto samples = static_cast<int>(std::min<std::size_t>(out.size(), INT_MAX));
        const int frames = stb_vorbis_get_samples_short_interleaved(vorbis_, Channels(), out.data(), samples);
        return frames > 0 ? static_cast<std::size_t>(frames) : 0;
    }

    // stb_vorbis finds the Ogg page that holds the frame by bisecting the file, then decodes from it
    bool Seek(std::uint64_t frame) override { return frame <= UINT_MAX && stb_vorbis_seek(vorbis_, static_cast<unsigned int>(frame)) != 0; }

private:
    stb_vorbis* vorbis_;
    std::unique_ptr<Memory> memory_; // stb_vorbis's, in which vorbis_ lives
};

} // namespace

// a Vorbis set up on the file within its block of memory, or nullptr when stb_vorbis does not open it
std::unique_ptr<Decoder> OpenVorbis(std::span<const std::byte> file)
{
    if (file.size() > INT_MAX) {
        return nullptr;
    }

    auto memory = std::make_unique_for_overwrite<Memory>();
    const stb_vorbis_alloc alloc{.alloc_buffer = memory->data(), .alloc_buffer_length_in_bytes = kMemory};
    int error = 0;
    // stb_vorbis reads bytes as unsigned char, which may alias any object
    const auto* bytes = reinterpret_cast<const unsigned char*>(file.data()); // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    stb_vorbis* vorbis = stb_vorbis_open_memory(bytes, static_cast<int>(file.size()), &error, &alloc);

    if (vorbis == nullptr) {
        return nullptr;
    }

    return std::make_unique<Vorbis>(vorbis, std::move(memory));
}

} // namespace sound

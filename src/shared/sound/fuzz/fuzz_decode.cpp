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

// fuzz_decode.cpp -- libFuzzer target on the sound's decoders: each input decoded whole, then opened again, its frames
// counted, and read again after a seek to a frame its last two bytes give. The files of test/sound/data are a starting
// corpus, but silence_11min.flac, whose ten minutes of decoding at every input slow the fuzzer about a hundredfold (copy
// the others into the work folder). Example:
//   sound_fuzz_decode <work folder>
#include <shared/sound/decoder/decoder.h>

#include <cstddef>
#include <cstdint>
#include <span>

// one input, as libFuzzer calls it: decoded whole, then opened again, its frames counted, moved and read to its end, the
// move failed or not (the music reads on after a failed one)
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    const std::span<const std::byte> file(reinterpret_cast<const std::byte*>(data), size);
    (void)sound::Decode(file);

    if (const auto decoder = sound::OpenDecoder(file)) {
        (void)decoder->Frames();
        const std::uint64_t frame = size >= 2 ? (std::uint64_t{data[size - 2]} << 8 | data[size - 1]) : 0;

        (void)decoder->Seek(frame);
        std::int16_t out[2048];

        while (decoder->Read(out) > 0) {
        }
    }

    return 0;
}

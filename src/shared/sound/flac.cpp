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

// The FLAC decoder, over libFLAC's stream decoder: the file through its read, seek, tell, length and end callbacks, the
// format from its STREAMINFO block, each block of samples brought to 16 bits (the widths above 16 lose their lowest
// bits, those below gain zeros) and kept until Read takes it.
#include "formats.h"

#include <FLAC/stream_decoder.h>

#include <algorithm>
#include <cstring>

namespace sound {

namespace {

class Flac final : public Decoder {
public:
    Flac() = default;
    // libFLAC's callbacks receive this object's address: the decoder is neither copied nor moved
    Flac(const Flac&) = delete;
    Flac& operator=(const Flac&) = delete;
    Flac(Flac&&) = delete;
    Flac& operator=(Flac&&) = delete;
    // frees libFLAC's decoder
    ~Flac() override
    {
        if (decoder_ != nullptr) {
            FLAC__stream_decoder_delete(decoder_);
        }
    }

    // creates libFLAC's decoder on the file and reads the metadata up to the first frame, so that the format is known
    [[nodiscard]] bool Open(std::span<const std::byte> file)
    {
        file_ = file;
        decoder_ = FLAC__stream_decoder_new();

        if (decoder_ == nullptr || FLAC__stream_decoder_init_stream(decoder_, OnRead, OnSeek, OnTell, OnLength, OnEof, OnWrite, OnMetadata, OnError, this) !=
                                       FLAC__STREAM_DECODER_INIT_STATUS_OK) {
            return false;
        }

        return FLAC__stream_decoder_process_until_end_of_metadata(decoder_) != 0 && Rate() > 0;
    }

    // copies whole frames from the decoded block, and has libFLAC decode the next block when it is used up
    std::size_t Read(std::span<std::int16_t> out) override
    {
        const auto channels = static_cast<std::size_t>(Channels());
        out = out.first(out.size() - (out.size() % channels));
        std::size_t written = 0;

        while (written < out.size()) {
            if (taken_ == pending_.size()) {
                pending_.clear();
                taken_ = 0;

                // a damaged frame calls OnError and libFLAC goes on with the next one; a failed call stops
                if (FLAC__stream_decoder_get_state(decoder_) == FLAC__STREAM_DECODER_END_OF_STREAM || FLAC__stream_decoder_process_single(decoder_) == 0) {
                    break;
                }

                continue;
            }

            const std::size_t n = std::min(out.size() - written, pending_.size() - taken_);
            std::copy_n(pending_.begin() + static_cast<std::ptrdiff_t>(taken_), n, out.begin() + static_cast<std::ptrdiff_t>(written));
            taken_ += n;
            written += n;
        }

        return written / channels;
    }

    // libFLAC finds the frame through the seek table or by bisecting the file, then decodes from its block
    bool Seek(std::uint64_t frame) override
    {
        pending_.clear();
        taken_ = 0;

        if (FLAC__stream_decoder_seek_absolute(decoder_, frame) != 0) {
            return true;
        }

        // a failed seek leaves the decoder waiting for a flush
        FLAC__stream_decoder_flush(decoder_);
        return false;
    }

private:
    // the object a callback's user data points to
    static Flac& Self(void* self) noexcept { return *static_cast<Flac*>(self); }

    // copies the next bytes of the file into libFLAC's buffer, at most as many as it asks for
    static FLAC__StreamDecoderReadStatus OnRead(const FLAC__StreamDecoder* /*decoder*/, FLAC__byte* buffer, std::size_t* bytes, void* self)
    {
        Flac& f = Self(self);
        const std::size_t n = std::min(*bytes, f.file_.size() - f.position_);
        std::memcpy(buffer, f.file_.data() + f.position_, n);
        f.position_ += n;
        *bytes = n;
        return n > 0 ? FLAC__STREAM_DECODER_READ_STATUS_CONTINUE : FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    }

    // moves the reading position to a byte of the file, for libFLAC's seeking
    static FLAC__StreamDecoderSeekStatus OnSeek(const FLAC__StreamDecoder* /*decoder*/, FLAC__uint64 offset, void* self)
    {
        Flac& f = Self(self);

        if (offset > f.file_.size()) {
            return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
        }

        f.position_ = static_cast<std::size_t>(offset);
        return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
    }

    // gives libFLAC the reading position
    static FLAC__StreamDecoderTellStatus OnTell(const FLAC__StreamDecoder* /*decoder*/, FLAC__uint64* offset, void* self)
    {
        *offset = Self(self).position_;
        return FLAC__STREAM_DECODER_TELL_STATUS_OK;
    }

    // gives libFLAC the file's length
    static FLAC__StreamDecoderLengthStatus OnLength(const FLAC__StreamDecoder* /*decoder*/, FLAC__uint64* length, void* self)
    {
        *length = Self(self).file_.size();
        return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
    }

    // tells libFLAC whether the whole file has been read
    static FLAC__bool OnEof(const FLAC__StreamDecoder* /*decoder*/, void* self)
    {
        const Flac& f = Self(self);
        return f.position_ >= f.file_.size() ? 1 : 0;
    }

    // receives a decoded block, one array per channel, and appends it interleaved and in 16 bits to the pending samples
    static FLAC__StreamDecoderWriteStatus OnWrite(const FLAC__StreamDecoder* /*decoder*/, const FLAC__Frame* frame, const FLAC__int32* const* buffer,
                                                  void* self)
    {
        Flac& f = Self(self);
        const FLAC__FrameHeader& header = frame->header;
        const auto channels = static_cast<unsigned>(f.Channels());

        // a frame that does not match the stream's format (damaged) is skipped rather than mixed up
        if (header.channels != channels || header.bits_per_sample == 0 || header.bits_per_sample > 32) {
            return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
        }

        const unsigned bits = header.bits_per_sample;
        const std::size_t base = f.pending_.size();
        f.pending_.resize(base + (std::size_t{header.blocksize} * channels));
        std::int16_t* out = f.pending_.data() + base;

        for (unsigned i = 0; i < header.blocksize; ++i) {
            for (unsigned c = 0; c < channels; ++c) {
                const FLAC__int32 s = buffer[c][i];
                // the arithmetic shift C++20 defines for a negative value: the floor, as dr_wav brings 24 bits to 16
                *out++ = static_cast<std::int16_t>(bits >= 16 ? s >> (bits - 16) // NOLINT(bugprone-signed-bitwise)
                                                              : s * static_cast<FLAC__int32>(1U << (16 - bits)));
            }
        }

        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    }

    // takes the rate and the channels from the STREAMINFO block; the other blocks (tags, pictures) are not used
    static void OnMetadata(const FLAC__StreamDecoder* /*decoder*/, const FLAC__StreamMetadata* metadata, void* self)
    {
        if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
            // libFLAC's metadata is a union whose member the type gives
            const FLAC__StreamMetadata_StreamInfo& info = metadata->data.stream_info; // NOLINT(cppcoreguidelines-pro-type-union-access)
            Self(self).SetFormat(static_cast<int>(info.sample_rate), static_cast<int>(info.channels));
        }
    }

    // a damaged frame: libFLAC skips it and goes on with the next one, which is what the sound wants
    static void OnError(const FLAC__StreamDecoder* /*decoder*/, FLAC__StreamDecoderErrorStatus /*status*/, void* /*self*/) {}

    FLAC__StreamDecoder* decoder_ = nullptr;
    std::span<const std::byte> file_;
    std::size_t position_ = 0;
    std::vector<std::int16_t> pending_;
    std::size_t taken_ = 0;
};

} // namespace

// a Flac set up on the file, or nullptr when libFLAC does not read its metadata
std::unique_ptr<Decoder> OpenFlac(std::span<const std::byte> file)
{
    auto decoder = std::make_unique<Flac>();

    if (!decoder->Open(file)) {
        return nullptr;
    }

    return decoder;
}

} // namespace sound

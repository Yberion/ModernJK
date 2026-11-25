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

// stream.cpp -- a sound streamed to the listener (stream.h): at every frame the buffers OpenAL Soft has played taken
// back, filled again and queued, the source played again if it ran dry; the frames played counted from the buffers
// taken back and the offset in the one playing. Example: a cinematic queues a fraction of a second of samples at each
// frame; the stream plays them as they come.
#include "stream.h"

#define AL_ALEXT_PROTOTYPES
#include <AL/al.h>
#include <AL/alext.h>

#include <array>

namespace sound {

namespace {

// 32 buffers of 1,024 frames: from 0.74 s (at 44,100 Hz) to 1.5 s (at 22,050 Hz) queued, so that a frame that takes
// long, a level loading, does not starve the music at once; small buffers, so that a frame refills about one MP3 frame
// (about 12 us) where a buffer of 8,192 frames took about 0.1 ms in one frame every 186 ms
constexpr std::size_t kBuffers = 32;
constexpr std::size_t kFramesPerBuffer = 1024;

// the stream's buffers, made by OpenAL Soft
std::vector<std::uint32_t> GenerateBuffers()
{
    std::vector<std::uint32_t> buffers(kBuffers);
    alGenBuffers(static_cast<ALsizei>(buffers.size()), buffers.data());
    return buffers;
}

} // namespace

// the source at the listener, without attenuation, taking the resampler; its buffers all idle
Stream::Stream(int rate, int channels, int resampler)
    : rate_(rate), channels_(channels), buffers_(GenerateBuffers()), idle_(buffers_), scratch_(kFramesPerBuffer * static_cast<std::size_t>(channels))
{
    constexpr std::array<float, 3> kZero{};
    alGenSources(1, &source_);
    alSourcei(source_, AL_SOURCE_RELATIVE, AL_TRUE);
    alSourcefv(source_, AL_POSITION, kZero.data());
    alSourcef(source_, AL_ROLLOFF_FACTOR, 0.0f);
    alSourcei(source_, AL_SOURCE_RESAMPLER_SOFT, resampler);
}

Stream::~Stream()
{
    alSourceStop(source_);
    alSourcei(source_, AL_BUFFER, 0);
    alDeleteSources(1, &source_);
    alDeleteBuffers(static_cast<ALsizei>(buffers_.size()), buffers_.data());
}

// the played buffers taken back, every idle buffer filled and queued while fill has frames, the source played again
// when it ran dry before its buffers came
void Stream::Update(const Fill& fill)
{
    ALint processed = 0;
    alGetSourcei(source_, AL_BUFFERS_PROCESSED, &processed);

    for (ALint i = 0; i < processed; ++i) {
        std::uint32_t buffer = 0;
        alSourceUnqueueBuffers(source_, 1, &buffer);
        idle_.push_back(buffer);
        processedFrames_ += queued_.front();
        queued_.pop_front();
    }

    fillEmpty_ = false;

    while (!idle_.empty()) {
        const std::size_t frames = fill(scratch_);

        if (frames == 0) {
            fillEmpty_ = true;
            break;
        }

        const std::uint32_t buffer = idle_.back();
        idle_.pop_back();
        alBufferData(buffer, channels_ == 2 ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16, scratch_.data(),
                     static_cast<ALsizei>(frames * static_cast<std::size_t>(channels_) * sizeof(std::int16_t)), rate_);
        alSourceQueueBuffers(source_, 1, &buffer);
        queued_.push_back(static_cast<std::int64_t>(frames));
    }

    ALint state = AL_STOPPED;
    alGetSourcei(source_, AL_SOURCE_STATE, &state);

    if (state != AL_PLAYING && !queued_.empty()) {
        alSourcePlay(source_);
    }
}

// the stream's gain
void Stream::SetGain(float gain) const
{
    alSourcef(source_, AL_GAIN, gain);
}

// everything played and nothing more to play
bool Stream::Ended() const
{
    return fillEmpty_ && queued_.empty();
}

// the frames of the buffers played, and the offset in the one playing
std::int64_t Stream::PlayedFrames() const
{
    ALint offset = 0;
    alGetSourcei(source_, AL_SAMPLE_OFFSET, &offset);
    return processedFrames_ + offset;
}

} // namespace sound

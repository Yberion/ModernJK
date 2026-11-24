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

// device.h -- OpenAL Soft's device and context as the sound uses them: the system's default device, followed when it
// changes or disappears, HRTF off, the resampler bsinc24, the distances in the game's units, the mute without the
// window's focus; or OpenAL Soft's loopback device, rendered on demand, for the tests.
// Example: the window loses its focus: SetMuted(true) silences every sound, which goes on playing; the system's
// default device changes: the next Frame moves the sound to it.
#pragma once

#include <shared/sound/host.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

struct ALCdevice;
struct ALCcontext;

namespace sound {

// What a frame finds of the device: playing, lost (no device to mix on), or back after a loss.
enum class DeviceState : std::uint8_t {
    Playing,
    Lost,
    Reopened,
};

// The speed of sound in the game's units per second, for Doppler: 343.3 m/s, a player's 64 units taken for 1.8 m.
inline constexpr float kSpeedOfSound = 343.3f * 64.0f / 1.8f;

// The whole mix 9 dB down (the listener's gain), so that the game follows ASWG-R001, the loudness standard for games at
// home: a fight at the default settings at -24 LUFS, within 2 LU (ITU-R BS.1770 loudness); the game's sounds were mixed
// far louder (a duel -15 LUFS at the default settings, peaks above full scale).
inline constexpr float kMixGain = 0.355f;

// OpenAL Soft pans a sound at the listener to the centre, quieter on each channel than a sound fully to one side is on
// its own: the gain that makes the centre as loud on its loudest channel as the side, for an output mode
// (ALC_OUTPUT_MODE_SOFT); 1 for a mode not listed. The old mixer played the listener's sounds at full volume on both
// ears, as loud as a sound fully to one side on its ear.
[[nodiscard]] float ListenerBoostOf(int outputMode) noexcept;

class Device {
public:
    // Opens the system's default device; nullptr, the reason printed, when none opens.
    [[nodiscard]] static std::unique_ptr<Device> OpenSystem(const Host& host);
    // Opens OpenAL Soft's loopback device: stereo 16-bit samples at a rate, rendered by Render, no sound card needed.
    [[nodiscard]] static std::unique_ptr<Device> OpenLoopback(const Host& host, int rate);

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = delete;
    Device& operator=(Device&&) = delete;
    ~Device();

    // At every frame, now in milliseconds: moves to the system's default device when it changed, or when the device in use
    // disappeared, trying again once per second while none opens.
    [[nodiscard]] DeviceState Frame(int now);
    // The loopback device lost until told otherwise, so that the tests play a device unplugged and plugged back.
    void SimulateLoss(bool lost) noexcept { simulatedLoss_ = lost; }
    // The resampler every source takes: bsinc24's index, or the library's default (0) when it lacks it.
    [[nodiscard]] int Resampler() const noexcept { return resampler_; }
    // Silences every sound while muted, keeping them playing.
    void SetMuted(bool muted);
    // The gain that brings a sound at the listener to the level of a sound fully to one side, for the output mode of
    // the device in use (ListenerBoostOf).
    [[nodiscard]] float ListenerBoost() const noexcept { return listenerBoost_; }
    // Renders frames of the loopback device into out, stereo interleaved.
    void Render(std::span<std::int16_t> out);
    // The device's name, as the system gives it.
    [[nodiscard]] std::string Name() const;

private:
    Device(const Host& host, ALCdevice* device, bool loopback);
    [[nodiscard]] bool CreateContext(std::span<const int> attributes);
    void ApplyGain() const;
    void ReadOutputMode();

    const Host& host_;
    ALCdevice* device_ = nullptr;
    ALCcontext* context_ = nullptr;
    bool loopback_ = false;
    int resampler_ = 0;
    bool muted_ = false;
    float listenerBoost_ = 1.0f;
    bool lost_ = false;          // the device in use is gone and no other opened yet
    int nextAttempt_ = 0;        // when to try reopening again
    bool simulatedLoss_ = false; // the tests' unplugged loopback device
    std::atomic<bool> defaultChanged_{false};
};

} // namespace sound

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

// formats.h -- one decoder per format, each in its own file over its library; OpenDecoder (decoder.cpp) picks one.
// Each returns nullptr when its library cannot open the file.
// Example: OpenFlac(file) decodes through libFLAC; OpenDecoder calls it for a file starting with "fLaC".
#pragma once

#include "decoder.h"

namespace sound {

[[nodiscard]] std::unique_ptr<Decoder> OpenWav(std::span<const std::byte> file);    // dr_wav
[[nodiscard]] std::unique_ptr<Decoder> OpenMp3(std::span<const std::byte> file);    // dr_mp3
[[nodiscard]] std::unique_ptr<Decoder> OpenFlac(std::span<const std::byte> file);   // libFLAC
[[nodiscard]] std::unique_ptr<Decoder> OpenVorbis(std::span<const std::byte> file); // stb_vorbis

} // namespace sound

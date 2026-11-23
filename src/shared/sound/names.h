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

// names.h -- the names of the sounds' and the music's files: a name keyed without its extension, and its file found
// among the extensions the decoders read, from the best format to the least good.
// Example: NormalizeSoundName("Sound\Weapons\Fire.WAV") is "sound/weapons/fire"; ReadSoundFile(host,
// "sound/weapons/fire") reads fire.flac, fire.wav, fire.ogg or fire.mp3, the first that exists.
#pragma once

#include "host.h"

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace sound {

// A name as the sound keys it: lower case, '/' as separator, without its extension.
[[nodiscard]] std::string NormalizeSoundName(std::string_view name);

// A name in lower case with '/' as separator, nothing dropped: a name that has no extension, such as a dynamic music's
// piece, may hold a dot.
[[nodiscard]] std::string LowerSoundName(std::string_view name);

// The first file of a name without extension among .flac, .wav (both lossless), .ogg and .mp3; nullopt for none.
[[nodiscard]] std::optional<std::vector<std::byte>> ReadSoundFile(const Host& host, std::string_view base);

// Whether a file of a name without extension exists among those extensions.
[[nodiscard]] bool SoundFileExists(const Host& host, std::string_view base);

// A text file's bytes as characters.
[[nodiscard]] std::string ToText(std::span<const std::byte> file);

} // namespace sound

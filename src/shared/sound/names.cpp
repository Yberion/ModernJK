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

// names.cpp -- the names of the sounds' and the music's files (names.h): a name in lower case with '/', its extension
// dropped; the extensions tried in order. Example: "Music\Yavin1\Intro.MP3" is "music/yavin1/intro", read from
// intro.flac, intro.wav, intro.ogg or intro.mp3, the first that exists.
#include "names.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace sound {

namespace {

// from the best format to the least good: lossless, then lossy
constexpr std::array<std::string_view, 4> kExtensions = {".flac", ".wav", ".ogg", ".mp3"};

} // namespace

std::string LowerSoundName(std::string_view name)
{
    std::string result(name);
    std::ranges::transform(result, result.begin(), [](char c) { return c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c))); });
    return result;
}

std::string NormalizeSoundName(std::string_view name)
{
    std::string result = LowerSoundName(name);
    const std::size_t dot = result.find_last_of('.');
    const std::size_t slash = result.find_last_of('/');

    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        result.erase(dot);
    }

    return result;
}

std::optional<std::vector<std::byte>> ReadSoundFile(const Host& host, std::string_view base)
{
    for (const std::string_view extension : kExtensions) {
        if (auto file = host.readFile(std::string(base) + std::string(extension))) {
            return file;
        }
    }

    return std::nullopt;
}

std::string ToText(std::span<const std::byte> file)
{
    std::string text(file.size(), '\0');
    std::ranges::transform(file, text.begin(), [](std::byte b) { return static_cast<char>(b); });
    return text;
}

bool SoundFileExists(const Host& host, std::string_view base)
{
    return std::ranges::any_of(kExtensions, [&](std::string_view extension) { return host.fileExists(std::string(base) + std::string(extension)); });
}

} // namespace sound

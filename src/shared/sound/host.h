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

// host.h -- what the sound asks of the program that runs it: the game's files, and a place for its messages. The
// engines give theirs (engine/), the tests their own, so that the sound depends on no engine.
// Example: the tests' host reads the sample files of test/sound/data and keeps the messages (test/sound/sound.h); an
// engine's reads its file system and prints in its console.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace sound {

// The kind of a message, which the program shows as it shows its own.
enum class Message : std::uint8_t {
    Info,
    Warning,
    Error,
    Debug, // shown only to a developer
};

struct Host {
    // A whole file of the game's file system; nullopt when it does not exist.
    std::function<std::optional<std::vector<std::byte>>(std::string_view path)> readFile;
    // Whether a file of the game's file system exists, without reading it.
    std::function<bool(std::string_view path)> fileExists;
    // One line of the sound's messages.
    std::function<void(Message kind, std::string_view text)> print;
    // A random integer from min to max, both included, from the program's own generator.
    std::function<int(int min, int max)> random;
};

} // namespace sound

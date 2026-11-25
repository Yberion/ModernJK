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

// The parts of modernjk_test_sound and what they share: the files of the data folder, and a host for the sound that
// reads them, keeps its messages and draws its random numbers from a fixed seed.
#pragma once

#include <shared/sound/host.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace test {

// a whole file, empty when it cannot be read
inline std::vector<std::byte> ReadFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    const std::vector<char> chars{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    std::vector<std::byte> bytes(chars.size());
    std::ranges::transform(chars, bytes.begin(), [](char c) { return static_cast<std::byte>(c); });
    return bytes;
}

// The sound's host in the tests: a path reads the data folder's file of the same name, its folders dropped
// ("sound/weapons/mono22k_s16.wav" reads mono22k_s16.wav), or a text the test gives by its whole path; every path asked
// and every message kept.
class Host {
public:
    explicit Host(std::filesystem::path data) : data_(std::move(data))
    {
        host.readFile = [this](std::string_view path) -> std::optional<std::vector<std::byte>> {
            asked.emplace_back(path);

            if (const auto text = texts.find(std::string(path)); text != texts.end()) {
                std::vector<std::byte> bytes(text->second.size());
                std::ranges::transform(text->second, bytes.begin(), [](char c) { return static_cast<std::byte>(c); });
                return bytes;
            }

            const std::filesystem::path file = data_ / std::filesystem::path(std::string(path)).filename();

            if (!std::filesystem::exists(file)) {
                return std::nullopt;
            }

            return ReadFile(file);
        };
        host.fileExists = [this](std::string_view path) {
            return texts.contains(std::string(path)) || std::filesystem::exists(data_ / std::filesystem::path(std::string(path)).filename());
        };
        host.print = [this](sound::Message kind, std::string_view text) { messages.emplace_back(kind, text); };
        host.random = [this](int min, int max) { return random ? random(min, max) : std::uniform_int_distribution<int>(min, max)(generator); };
    }

    Host(const Host&) = delete;
    Host& operator=(const Host&) = delete;
    Host(Host&&) = delete;
    Host& operator=(Host&&) = delete;
    ~Host() = default;

    // the messages of a kind
    [[nodiscard]] std::size_t Count(sound::Message kind) const
    {
        return static_cast<std::size_t>(std::ranges::count_if(messages, [kind](const auto& m) { return m.first == kind; }));
    }

    sound::Host host;
    std::vector<std::string> asked;
    std::vector<std::pair<sound::Message, std::string>> messages;
    std::map<std::string, std::string> texts;
    std::function<int(int, int)> random; // the draws, when a test sets them
    std::mt19937 generator{1};

private:
    std::filesystem::path data_;
};

} // namespace test

void RunDecoderTests(const std::filesystem::path& data);
void RunDeviceTests(const std::filesystem::path& data);
void RunBankTests(const std::filesystem::path& data);
void RunVoicesTests(const std::filesystem::path& data);
void RunStreamTests(const std::filesystem::path& data);
void RunAmbientTests(const std::filesystem::path& data);
void RunContactTests(const std::filesystem::path& data);
void RunZonesTests(const std::filesystem::path& data, const std::filesystem::path& assets);

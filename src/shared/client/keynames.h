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

// keynames.h -- the keys' names, the same in the three games (keynames.cpp): the engine's table of the keys, the name
// a bind writes for a key and the key a name gives back, and the sixteen codes the table leaves free for the keys it
// has none for (a function key beyond F12, a key of a layout whose character is not in the game's set), which such a
// key takes with its name. Functions of the table they are given: the engine holds the table and the keys' binds.
// Example: the right Ctrl is a key of its own, written RCTRL in the configuration and read back as itself:
//     const std::optional<fakeAscii_t> key = client_input::KeyOfName(names, "rctrl"); // fakeAscii_t::A_CTRL2
//     const client_input::KeyText text = client_input::NameOfKey(names, *key);            // "RCTRL"
#pragma once

#include <shared/qcommon/q_keycodes.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

// A key of the engine's table. The engines' type, in their shape.
struct keyname_t {
    std::uint16_t upper; // the key whose state and bind this one shares (a lower-case letter's upper-case one)
    std::uint16_t lower; // the key's lower-case character, its own code for a key that is not a letter; 0 for none
    const char* name;    // its name in binds; null for a key written as its character
    fakeAscii_t keynum;  // the key a bind of this name is made on
    bool menukey;        // never read
};

// The engine's table of the keys, each at its code.
using keyNames_t = EnumArray<keyname_t, fakeAscii_t, fakeAscii_t::MAX_KEYS>;

namespace client_input {

// The table as a game starts: the keys of the engine's codes, the free codes without a name.
[[nodiscard]] const keyNames_t& DefaultKeyNames() noexcept;

// The key whose state and bind a key shares: a lower-case letter's upper-case one, the key itself otherwise.
[[nodiscard]] fakeAscii_t SharedKey(const keyNames_t& names, fakeAscii_t key) noexcept;

// The key a name of a bind gives: for one character, the key that types it (the key sharing its state); for a name of
// the table, in any case, that entry's key; for "0x" and two hexadecimal digits, the key of that code; else nothing.
[[nodiscard]] std::optional<fakeAscii_t> KeyOfName(const keyNames_t& names, std::string_view name) noexcept;

// A key's name, held by value: at most 31 characters and an ending zero.
struct KeyText {
    std::array<char, 32> characters{};

    [[nodiscard]] const char* c_str() const noexcept { return characters.data(); }
};

// A key's name as binds write it: its name in the table, else the character it types, else "0x" and its code in
// hexadecimal; "<OUT OF RANGE>" for a code outside the table.
[[nodiscard]] KeyText NameOfKey(const keyNames_t& names, fakeAscii_t key) noexcept;

// A key as the bind list shows it first: the character it types, else its name in the table, else "0x" and its code
// in hexadecimal; "<OUT OF RANGE>" for a code outside the table.
[[nodiscard]] KeyText CharacterOfKey(const keyNames_t& names, fakeAscii_t key) noexcept;

// The number of codes the table leaves free for the keys it has none for: fakeAscii_t::A_UNDEFINED_7 to A_UNDEFINED_22.
inline constexpr int kDynamicKeyCount = std::to_underlying(fakeAscii_t::A_UNDEFINED_22) - std::to_underlying(fakeAscii_t::A_UNDEFINED_7) + 1;

// The keys without a code of their own: each takes one of the free codes when it is first pressed or named in a bind,
// with its name, which binds write and read back.
class DynamicKeys {
public:
    // The code for a key's name: the one already given to that name, in any case; else one never given; else the first
    // whose key mayTake allows to lose its code (in the engine: a key without a bind, not held), A_UNDEFINED_7's
    // first; nothing when none is left. A code given keeps the name (its first 31 characters), which the table's
    // entry then points to: this object outlives the table's use.
    [[nodiscard]] std::optional<fakeAscii_t> CodeFor(keyNames_t& names, std::string_view name, const std::array<bool, kDynamicKeyCount>& mayTake) noexcept;

private:
    std::array<std::array<char, 32>, kDynamicKeyCount> names_{};
};

} // namespace client_input

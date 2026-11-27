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

// The tests of the keys' names (src/shared/client/keynames.h): no two keys of the table named alike (the single
// player named the right Shift, Ctrl and Alt as the left ones); every key's name read back as the key; a name in any
// case, one character, "0x" and two digits, nothing for another text; a code outside the table; the free codes given
// to names, taken back from a key without a bind, none left when all are bound.
#include "client.h"

#include <check.h>

#include <shared/client/keynames.h>

#include <array>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>

namespace {

// a name in upper case, to compare names whatever their case
std::string Upper(std::string_view name)
{
    std::string upper(name);
    for (char& character : upper) {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
    return upper;
}

// no two entries of the table share a name; the right modifiers are keys of their own
void TestNamesApart()
{
    const keyNames_t& names = client_input::DefaultKeyNames();

    for (const keyname_t& a : names) {
        for (const keyname_t& b : names) {
            if (&a != &b && a.name != nullptr && b.name != nullptr) {
                CHECK(Upper(a.name) != Upper(b.name));
            }
        }
    }

    CHECK(client_input::KeyOfName(names, "SHIFT") == fakeAscii_t::A_SHIFT);
    CHECK(client_input::KeyOfName(names, "RSHIFT") == fakeAscii_t::A_SHIFT2);
    CHECK(client_input::KeyOfName(names, "CTRL") == fakeAscii_t::A_CTRL);
    CHECK(client_input::KeyOfName(names, "rctrl") == fakeAscii_t::A_CTRL2);
    CHECK(client_input::KeyOfName(names, "ALT") == fakeAscii_t::A_ALT);
    CHECK(client_input::KeyOfName(names, "RAlt") == fakeAscii_t::A_ALT2);
    CHECK_EQUAL(std::string_view(client_input::NameOfKey(names, fakeAscii_t::A_CTRL2).c_str()), "RCTRL");
}

// every key of the table: the name binds write for it gives the key back (an entry's own key for a named entry, the
// key sharing its state otherwise)
void TestNamesReadBack()
{
    const keyNames_t& names = client_input::DefaultKeyNames();

    for (fakeAscii_t key = fakeAscii_t::A_NULL + 1; key < fakeAscii_t::MAX_KEYS; key++) {
        const client_input::KeyText text = client_input::NameOfKey(names, key);
        const std::optional<fakeAscii_t> back = client_input::KeyOfName(names, text.c_str());
        const fakeAscii_t expected = names[key].name != nullptr ? names[key].keynum : key;

        CHECK(back.has_value());
        if (back) {
            CHECK(client_input::SharedKey(names, *back) == client_input::SharedKey(names, expected));
        }
    }
}

// the forms of a name: one character, "0x" and two digits, nothing for another text; a code outside the table
void TestNameForms()
{
    const keyNames_t& names = client_input::DefaultKeyNames();

    CHECK(client_input::KeyOfName(names, "a") == fakeAscii_t::A_CAP_A);
    CHECK(client_input::KeyOfName(names, "A") == fakeAscii_t::A_CAP_A);
    CHECK(client_input::KeyOfName(names, "0x82") == fakeAscii_t::A_CTRL2);
    CHECK(client_input::KeyOfName(names, "0xaF") == fakeAscii_t::A_UNDEFINED_7);
    CHECK(!client_input::KeyOfName(names, "0X82"));
    CHECK(!client_input::KeyOfName(names, "0x8G"));
    CHECK(!client_input::KeyOfName(names, "0x082"));
    CHECK(!client_input::KeyOfName(names, ""));
    CHECK(!client_input::KeyOfName(names, "NOTAKEY"));

    CHECK_EQUAL(std::string_view(client_input::NameOfKey(names, fakeAscii_t::A_LOW_A).c_str()), "A");
    CHECK_EQUAL(std::string_view(client_input::NameOfKey(names, fakeAscii_t::A_UNDEFINED_7).c_str()), "0xAF");
    CHECK_EQUAL(std::string_view(client_input::NameOfKey(names, fakeAscii_t::MAX_KEYS).c_str()), "<OUT OF RANGE>");
    CHECK_EQUAL(std::string_view(client_input::CharacterOfKey(names, fakeAscii_t::A_CAP_A).c_str()), "A");
    // a key below the joysticks' shows its code as a character first, even a named one; the joysticks' and the
    // auxiliary keys their names
    CHECK_EQUAL(std::string_view(client_input::CharacterOfKey(names, fakeAscii_t::A_MOUSE1).c_str()), "\x8D");
    CHECK_EQUAL(std::string_view(client_input::CharacterOfKey(names, fakeAscii_t::A_JOY0).c_str()), "JOY0");
    CHECK_EQUAL(std::string_view(client_input::CharacterOfKey(names, fakeAscii_t::MAX_KEYS).c_str()), "<OUT OF RANGE>");
}

// the free codes: given to a name once, found again in any case, a code never given first, one taken back from a key
// that may lose it, none when all are bound; a long name cut to 31 characters
void TestDynamicKeys()
{
    keyNames_t names = client_input::DefaultKeyNames();
    client_input::DynamicKeys dynamicKeys;
    std::array<bool, client_input::kDynamicKeyCount> mayTake{};

    CHECK(dynamicKeys.CodeFor(names, "F13", mayTake) == fakeAscii_t::A_UNDEFINED_7);
    CHECK(dynamicKeys.CodeFor(names, "f13", mayTake) == fakeAscii_t::A_UNDEFINED_7);
    CHECK(client_input::KeyOfName(names, "F13") == fakeAscii_t::A_UNDEFINED_7);
    CHECK_EQUAL(std::string_view(client_input::NameOfKey(names, fakeAscii_t::A_UNDEFINED_7).c_str()), "F13");

    for (int i = 1; i < client_input::kDynamicKeyCount; ++i) {
        const std::string name = "U+00" + std::to_string(160 + i);
        CHECK(dynamicKeys.CodeFor(names, name, mayTake) == fakeAscii_t::A_UNDEFINED_7 + i);
    }

    CHECK(!dynamicKeys.CodeFor(names, "F24", mayTake));

    mayTake[3] = true;
    mayTake[5] = true;
    CHECK(dynamicKeys.CodeFor(names, "F24", mayTake) == fakeAscii_t::A_UNDEFINED_7 + 3);
    CHECK(client_input::KeyOfName(names, "F24") == fakeAscii_t::A_UNDEFINED_7 + 3);
    CHECK(!client_input::KeyOfName(names, "U+00163"));

    keyNames_t fresh = client_input::DefaultKeyNames();
    client_input::DynamicKeys longNames;
    const std::string longName(40, 'K');
    CHECK(longNames.CodeFor(fresh, longName, mayTake) == fakeAscii_t::A_UNDEFINED_7);
    CHECK_EQUAL(std::string_view(fresh[fakeAscii_t::A_UNDEFINED_7].name).size(), static_cast<std::size_t>(31));
}

} // namespace

void RunKeyNamesTests()
{
    TestNamesApart();
    TestNamesReadBack();
    TestNameForms();
    TestDynamicKeys();
}

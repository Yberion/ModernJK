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

// The tests of a text field (src/shared/client/field.h): characters typed, inserted and put in place, the line's room;
// the control characters (Ctrl-H, Ctrl-C, Ctrl-V, Ctrl-A, Ctrl-E); the keys (Delete, the arrows, Home, End, Insert,
// Shift with Insert or the keypad's 0); the cursor kept in view on a line longer than the field, Ctrl-E as End, a line
// shortened from its end; what a field shows whatever it holds.
#include "client.h"

#include <check.h>

#include <shared/client/field.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>

namespace {

using client_input::FieldRequest;

// a field holding a text, its cursor at a place, of a width
field_t Field(std::string_view text, int cursor, int width)
{
    field_t field{};
    std::ranges::copy(text, std::begin(field.buffer));
    field.cursor = cursor;
    field.widthInChars = width;
    return field;
}

// the field's text
std::string Text(const field_t& field)
{
    return std::string(static_cast<const char*>(field.buffer));
}

// a text typed in a field, character by character, not in overstrike
void Type(field_t& field, std::string_view text)
{
    for (const char character : text) {
        CHECK(client_input::TypeCharacter(field, static_cast<unsigned char>(character), false) == FieldRequest::None);
    }
}

// a key pressed in a field, nothing asked of its owner
void Press(field_t& field, fakeAscii_t key, int times = 1)
{
    for (int i = 0; i < times; ++i) {
        CHECK(client_input::PressKey(field, key, false) == FieldRequest::None);
    }
}

// what a field shows stays in its line, the cursor's column in the field's width
void CheckView(const field_t& field)
{
    const client_input::FieldView view = client_input::VisibleField(field);
    const int length = static_cast<int>(Text(field).size());

    CHECK(view.start >= 0);
    CHECK(view.length >= 0);
    CHECK(view.start + view.length <= length);
}

// characters typed, inserted in the middle, put in place in overstrike, the line's room
void TestTyping()
{
    field_t field = Field("", 0, 20);
    Type(field, "abc");
    CHECK_EQUAL(Text(field), "abc");
    CHECK_EQUAL(field.cursor, 3);

    field.cursor = 1;
    Type(field, "x");
    CHECK_EQUAL(Text(field), "axbc");
    CHECK_EQUAL(field.cursor, 2);

    CHECK(client_input::TypeCharacter(field, 'y', true) == FieldRequest::None);
    CHECK_EQUAL(Text(field), "axyc");
    CHECK_EQUAL(field.cursor, 3);

    // overstrike at the line's end adds the character
    field.cursor = 4;
    CHECK(client_input::TypeCharacter(field, 'z', true) == FieldRequest::None);
    CHECK_EQUAL(Text(field), "axycz");

    // a character of the engine's one-byte set above 127 goes in as its byte
    Type(field, "\xE9");
    CHECK_EQUAL(Text(field), "axycz\xE9");

    // the line keeps room for a leading slash and its ending zero
    field_t full = Field(std::string(MAX_EDIT_LINE - 2, 'a'), MAX_EDIT_LINE - 2, 20);
    Type(full, "b");
    CHECK_EQUAL(Text(full).size(), static_cast<std::size_t>(MAX_EDIT_LINE - 2));
    CHECK(client_input::TypeCharacter(full, 'b', true) == FieldRequest::None);
    CHECK_EQUAL(Text(full).find('b'), std::string::npos);
}

// Ctrl-H, Ctrl-C, Ctrl-V, Ctrl-A and Ctrl-E; another control character does nothing
void TestControlCharacters()
{
    field_t field = Field("hello", 5, 20);
    CHECK(client_input::TypeCharacter(field, 'h' - 'a' + 1, false) == FieldRequest::None);
    CHECK_EQUAL(Text(field), "hell");
    CHECK_EQUAL(field.cursor, 4);

    field.cursor = 0;
    CHECK(client_input::TypeCharacter(field, 'h' - 'a' + 1, false) == FieldRequest::None);
    CHECK_EQUAL(Text(field), "hell");

    CHECK(client_input::TypeCharacter(field, 'e' - 'a' + 1, false) == FieldRequest::None);
    CHECK_EQUAL(field.cursor, 4);
    CHECK(client_input::TypeCharacter(field, 'a' - 'a' + 1, false) == FieldRequest::None);
    CHECK_EQUAL(field.cursor, 0);

    CHECK(client_input::TypeCharacter(field, 'v' - 'a' + 1, false) == FieldRequest::Paste);
    CHECK_EQUAL(Text(field), "hell");

    CHECK(client_input::TypeCharacter(field, 'b' - 'a' + 1, false) == FieldRequest::None);
    CHECK_EQUAL(Text(field), "hell");

    CHECK(client_input::TypeCharacter(field, 'c' - 'a' + 1, false) == FieldRequest::None);
    CHECK_EQUAL(Text(field), "");
    CHECK_EQUAL(field.cursor, 0);
    CHECK_EQUAL(field.scroll, 0);
    CHECK_EQUAL(field.widthInChars, 20);
}

// Delete, the arrows, Home and End, Insert, Shift with Insert or with the keypad's 0
void TestKeys()
{
    field_t field = Field("abc", 1, 20);
    Press(field, fakeAscii_t::A_DELETE);
    CHECK_EQUAL(Text(field), "ac");
    CHECK_EQUAL(field.cursor, 1);

    field.cursor = 2;
    Press(field, fakeAscii_t::A_DELETE);
    CHECK_EQUAL(Text(field), "ac");

    Press(field, fakeAscii_t::A_CURSOR_RIGHT);
    CHECK_EQUAL(field.cursor, 2);
    Press(field, fakeAscii_t::A_CURSOR_LEFT, 3);
    CHECK_EQUAL(field.cursor, 0);
    Press(field, fakeAscii_t::A_END);
    CHECK_EQUAL(field.cursor, 2);
    Press(field, fakeAscii_t::A_HOME);
    CHECK_EQUAL(field.cursor, 0);

    CHECK(client_input::PressKey(field, fakeAscii_t::A_INSERT, false) == FieldRequest::SwitchOverstrike);
    CHECK(client_input::PressKey(field, fakeAscii_t::A_INSERT, true) == FieldRequest::Paste);
    CHECK(client_input::PressKey(field, fakeAscii_t::A_KP_0, true) == FieldRequest::Paste);
    Press(field, fakeAscii_t::A_KP_0);
    CHECK_EQUAL(Text(field), "ac");
}

// a line longer than the field: typed to its end, edited in its middle, Home, End and Ctrl-E, shortened from its end
void TestScroll()
{
    // a field 10 wide shows 9 characters, its last column kept for the cursor
    field_t field = Field("", 0, 10);
    Type(field, "abcdefghijklmnopqrst");
    CHECK_EQUAL(field.cursor, 20);
    CHECK_EQUAL(field.scroll, 11);
    client_input::FieldView view = client_input::VisibleField(field);
    CHECK_EQUAL(view.start, 11);
    CHECK_EQUAL(view.length, 9);
    CHECK_EQUAL(view.cursorColumn, 9);

    Press(field, fakeAscii_t::A_HOME);
    CHECK_EQUAL(field.scroll, 0);
    CHECK_EQUAL(client_input::VisibleField(field).cursorColumn, 0);

    // End shows the line's last characters, the cursor after them
    Press(field, fakeAscii_t::A_END);
    view = client_input::VisibleField(field);
    CHECK_EQUAL(view.start + view.length, 20);
    CHECK_EQUAL(view.cursorColumn, 9);

    // so does Ctrl-E (it left the last character hidden)
    CHECK(client_input::TypeCharacter(field, 'a' - 'a' + 1, false) == FieldRequest::None);
    CHECK(client_input::TypeCharacter(field, 'e' - 'a' + 1, false) == FieldRequest::None);
    view = client_input::VisibleField(field);
    CHECK_EQUAL(view.start + view.length, 20);
    CHECK_EQUAL(view.cursorColumn, 9);

    // a character typed in the middle of the line shown moves the cursor, not the text
    Press(field, fakeAscii_t::A_CURSOR_LEFT, 5);
    Type(field, "z");
    CHECK_EQUAL(field.scroll, 11);
    CHECK_EQUAL(field.cursor, 16);

    // the line shortened from its end shows its last characters again
    Press(field, fakeAscii_t::A_END);
    for (int i = 0; i < 5; ++i) {
        CHECK(client_input::TypeCharacter(field, 'h' - 'a' + 1, false) == FieldRequest::None);
    }
    CHECK_EQUAL(field.cursor, 16);
    CHECK_EQUAL(field.scroll, 7);

    // Ctrl-E on a short line, then a line typed past the field: the line scrolled from its start (it scrolled from a
    // negative place, which drew the memory before the line)
    field_t shortLine = Field("abc", 0, 10);
    CHECK(client_input::TypeCharacter(shortLine, 'e' - 'a' + 1, false) == FieldRequest::None);
    CHECK_EQUAL(shortLine.scroll, 0);
    Type(shortLine, "defghijklmnopq");
    CHECK(shortLine.scroll >= 0);
    view = client_input::VisibleField(shortLine);
    CHECK_EQUAL(view.start + view.length, 17);
    CheckView(shortLine);
}

// what a field shows stays in its line whatever it holds: a scroll out of the line, a width changed, no width
void TestView()
{
    field_t field = Field("abcdefghijklmnopqrst", 20, 10);
    for (const int scroll : {-50, -1, 0, 5, 11, 12, 1000}) {
        field.scroll = scroll;
        CheckView(field);
    }

    field.scroll = 0;
    for (const int width : {0, 1, 2, 9, 20, 21, 300}) {
        field.widthInChars = width;
        CheckView(field);
    }

    field.widthInChars = 0;
    CHECK_EQUAL(client_input::VisibleField(field).length, 0);

    // a line that fits is shown whole from its start
    field_t fits = Field("abc", 3, 10);
    fits.scroll = 2;
    const client_input::FieldView view = client_input::VisibleField(fits);
    CHECK_EQUAL(view.start, 0);
    CHECK_EQUAL(view.length, 3);
    CHECK_EQUAL(view.cursorColumn, 3);
}

} // namespace

void RunFieldTests()
{
    TestTyping();
    TestControlCharacters();
    TestKeys();
    TestScroll();
    TestView();
}

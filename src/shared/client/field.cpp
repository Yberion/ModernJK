/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
Copyright (C) 2013 - 2015, OpenJK contributors
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

// field.cpp -- a line of text being typed (field.h), from the engines' Field_KeyDownEvent, Field_CharEvent and
// Field_VariableSizeDraw. The line is a C string in a buffer of MAX_EDIT_LINE characters; every edit moves its
// characters within that buffer and then puts the scroll where the cursor is in view, which is the one rule the
// engines applied to some keys and not to others (Ctrl-E left the last character hidden, a character typed after it
// scrolled from a negative place).
#include "field.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <span>

namespace client_input {

namespace {

// the control character Ctrl and a letter type
constexpr int Control(char letter) noexcept
{
    return letter - 'a' + 1;
}

// the line's characters, as a span of the buffer
std::span<char, MAX_EDIT_LINE> Characters(field_t& field) noexcept
{
    return {field.buffer};
}

// a character of the line by its place, which the callers keep in the buffer
char& At(std::span<char, MAX_EDIT_LINE> characters, int place) noexcept
{
    assert(place >= 0 && place < MAX_EDIT_LINE);
    return characters[static_cast<std::size_t>(place)];
}

// the line's length: its characters before the ending zero, at most one less than the buffer holds, so that the
// moves below stay in the buffer whatever it holds
int Length(const field_t& field) noexcept
{
    const std::span<const char, MAX_EDIT_LINE - 1> line = std::span<const char, MAX_EDIT_LINE>(field.buffer).first<MAX_EDIT_LINE - 1>();
    return static_cast<int>(std::ranges::find(line, '\0') - line.begin());
}

// the cursor in view, the line scrolled no further than it needs: the cursor's column between 0 and the width's last,
// the last characters shown at most up to the field's end
void KeepCursorInView(field_t& field) noexcept
{
    const int length = Length(field);
    const int lastColumn = std::max(field.widthInChars - 1, 0);

    field.cursor = std::clamp(field.cursor, 0, length);
    field.scroll = std::clamp(field.scroll, field.cursor - lastColumn, field.cursor);
    field.scroll = std::clamp(field.scroll, 0, std::max(length - lastColumn, 0));
}

// the character under the cursor removed, the rest moved down with the ending zero
void DeleteAtCursor(field_t& field, int length) noexcept
{
    if (field.cursor < length) {
        const std::span<char, MAX_EDIT_LINE> characters = Characters(field);
        std::copy(characters.begin() + field.cursor + 1, characters.begin() + length + 1, characters.begin() + field.cursor);
    }
}

// a printable character at the cursor, in place of the one under it in overstrike, while the line keeps room for a
// leading slash and its ending zero
void PutCharacter(field_t& field, char character, bool overstrike, int length) noexcept
{
    const std::span<char, MAX_EDIT_LINE> characters = Characters(field);

    if (overstrike) {
        if (field.cursor >= MAX_EDIT_LINE - 2) {
            return;
        }
        At(characters, field.cursor) = character;
        ++field.cursor;

        if (field.cursor == length + 1) {
            At(characters, field.cursor) = '\0';
        }
        return;
    }

    if (length >= MAX_EDIT_LINE - 2) {
        return;
    }
    std::copy_backward(characters.begin() + field.cursor, characters.begin() + length + 1, characters.begin() + length + 2);
    At(characters, field.cursor) = character;
    ++field.cursor;
}

} // namespace

FieldRequest PressKey(field_t& field, fakeAscii_t key, bool shift) noexcept
{
    if ((key == fakeAscii_t::A_INSERT || key == fakeAscii_t::A_KP_0) && shift) {
        return FieldRequest::Paste;
    }

    const int length = Length(field);
    FieldRequest request = FieldRequest::None;
    field.cursor = std::clamp(field.cursor, 0, length);

    switch (key) {
    case fakeAscii_t::A_DELETE:
        DeleteAtCursor(field, length);
        break;
    case fakeAscii_t::A_CURSOR_RIGHT:
        field.cursor = std::min(field.cursor + 1, length);
        break;
    case fakeAscii_t::A_CURSOR_LEFT:
        field.cursor = std::max(field.cursor - 1, 0);
        break;
    case fakeAscii_t::A_HOME:
        field.cursor = 0;
        break;
    case fakeAscii_t::A_END:
        field.cursor = length;
        break;
    case fakeAscii_t::A_INSERT:
        request = FieldRequest::SwitchOverstrike;
        break;
    default:
        break;
    }

    KeepCursorInView(field);
    return request;
}

FieldRequest TypeCharacter(field_t& field, int character, bool overstrike) noexcept
{
    if (character == Control('v')) {
        return FieldRequest::Paste;
    }

    if (character == Control('c')) {
        ClearField(field);
        return FieldRequest::None;
    }

    const int length = Length(field);
    field.cursor = std::clamp(field.cursor, 0, length);

    if (character == Control('h')) {
        if (field.cursor > 0) {
            --field.cursor;
            DeleteAtCursor(field, length);
        }
    }
    else if (character == Control('a')) {
        field.cursor = 0;
    }
    else if (character == Control('e')) {
        field.cursor = length;
    }
    else if (character >= ' ') {
        PutCharacter(field, static_cast<char>(character), overstrike, length);
    }

    KeepCursorInView(field);
    return FieldRequest::None;
}

void ClearField(field_t& field) noexcept
{
    std::ranges::fill(Characters(field), '\0');
    field.cursor = 0;
    field.scroll = 0;
}

FieldView VisibleField(const field_t& field) noexcept
{
    const int length = Length(field);
    const int lastColumn = std::max(field.widthInChars - 1, 0);
    const int start = length <= lastColumn ? 0 : std::clamp(field.scroll, 0, length - lastColumn);

    return {.start = start, .length = std::min(lastColumn, length - start), .cursorColumn = field.cursor - start};
}

} // namespace client_input

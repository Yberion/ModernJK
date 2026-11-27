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

// field.h -- a line of text being typed, the same in the console, the chat and the three games (field.cpp): what a
// key or a character does to it, and which part of it is shown. Functions of the field they are given only: the
// engine draws the field, holds the overstrike switch and reads the clipboard, which a field asks for by what these
// functions return. After every key and character the cursor stays in view and the line is not scrolled further than
// it needs, so a long line typed, edited in its middle, then ended with End or Ctrl-E shows its last characters.
// Example: the console's line takes the player's characters, and is drawn from what VisibleField gives:
//     if (client_input::TypeCharacter(field, character, overstrike) == client_input::FieldRequest::Paste) {
//         // each character of the clipboard typed in turn
//     }
//     const client_input::FieldView view = client_input::VisibleField(field); // view.length characters from view.start
#pragma once

#include <shared/qcommon/q_keycodes.h>
#include <shared/qcommon/q_limits.h>

// A line of text being typed: its characters, ended by a zero; the cursor's place among them; the first one shown;
// how many characters wide the line is drawn. The engines' type, in their shape: the engines hand its buffer around as
// a C string.
struct field_t {
    int cursor;
    int scroll;
    int widthInChars;
    char buffer[MAX_EDIT_LINE]; // NOLINT(cppcoreguidelines-avoid-c-arrays, modernize-avoid-c-arrays): the engines' C string
};

namespace client_input {

// What a key or a character asks of the field's owner, beyond the field itself.
enum class FieldRequest {
    None,
    Paste,            // the clipboard's text, to type in character by character
    SwitchOverstrike, // the overstrike mode on if it was off, off if it was on
};

// A key pressed in a field: Delete deletes the character under the cursor, the arrows move the cursor by one, Home and
// End take it to the line's start and end; Shift with Insert, or with the keypad's 0 (Insert without num lock), asks
// for the clipboard; Insert alone asks to switch the overstrike mode. Another key does nothing.
[[nodiscard]] FieldRequest PressKey(field_t& field, fakeAscii_t key, bool shift) noexcept;

// A character typed in a field, in the engine's one-byte set: Ctrl-V asks for the clipboard, Ctrl-C empties the line,
// Ctrl-H deletes the character before the cursor, Ctrl-A and Ctrl-E take the cursor to the line's start and end as Home
// and End do; another character below 32 does nothing. A printable character goes in at the cursor, or in place of the
// character under it in overstrike, while the line keeps room for a leading slash and its ending zero.
[[nodiscard]] FieldRequest TypeCharacter(field_t& field, int character, bool overstrike) noexcept;

// An empty line, its width kept.
void ClearField(field_t& field) noexcept;

// The part of a field that is drawn: length characters from start, and the cursor's column counted from start.
struct FieldView {
    int start = 0;
    int length = 0;
    int cursorColumn = 0;
};

// What a field shows: its last column is kept for the cursor; the whole line when it fits, else the characters from
// the first one shown, never past the line's end, whatever the field holds (a width changed since, a line taken back
// from the history).
[[nodiscard]] FieldView VisibleField(const field_t& field) noexcept;

} // namespace client_input

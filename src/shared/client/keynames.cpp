/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
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

// keynames.cpp -- the keys' names (keynames.h), from the engines' key table and their Key_StringToKeynum,
// Key_KeynumToString, Key_KeynumToAscii and Key_DynamicKey. The table is indexed by the key's code; an entry names its
// key, or leaves it to be written as the character it types. Three entries name another key than their own, the
// codes of Shift with Space and the two Enter keys, which the engine never sends: SHIFT_SPACE, SHIFT_ENTER and
// SHIFT_KP_ENTER give Space, Enter and the keypad's Enter back.
#include "keynames.h"

#include <algorithm>
#include <cstddef>
#include <span>
#include <string_view>

namespace client_input {

namespace {

// Do not change a key's name (the third field): binds are written in the configuration files by these names, and the
// menus look up a key's displayed name as "KEYNAMES_KEYNAME_" and this name in the game's strings.
// The entries are positional, one a line in the fields' order: designated ones would make each of the 320 lines
// twice as long.
// NOLINTBEGIN(modernize-use-designated-initializers)
constexpr keyNames_t kDefaultKeyNames = {{{0x00, 0x00, nullptr, fakeAscii_t::A_NULL, false},
                                          {0x01, 0x01, "SHIFT", fakeAscii_t::A_SHIFT, false},
                                          {0x02, 0x02, "CTRL", fakeAscii_t::A_CTRL, false},
                                          {0x03, 0x03, "ALT", fakeAscii_t::A_ALT, false},
                                          {0x04, 0x04, "CAPSLOCK", fakeAscii_t::A_CAPSLOCK, false},
                                          {0x05, 0x05, "KP_NUMLOCK", fakeAscii_t::A_NUMLOCK, false},
                                          {0x06, 0x06, "SCROLLLOCK", fakeAscii_t::A_SCROLLLOCK, false},
                                          {0x07, 0x07, "PAUSE", fakeAscii_t::A_PAUSE, false},
                                          {0x08, 0x08, "BACKSPACE", fakeAscii_t::A_BACKSPACE, false},
                                          {0x09, 0x09, "TAB", fakeAscii_t::A_TAB, false},
                                          {0x0a, 0x0a, "ENTER", fakeAscii_t::A_ENTER, false},
                                          {0x0b, 0x0b, "KP_PLUS", fakeAscii_t::A_KP_PLUS, false},
                                          {0x0c, 0x0c, "KP_MINUS", fakeAscii_t::A_KP_MINUS, false},
                                          {0x0d, 0x0d, "KP_ENTER", fakeAscii_t::A_KP_ENTER, false},
                                          {0x0e, 0x0e, "KP_DEL", fakeAscii_t::A_KP_PERIOD, false},
                                          {0x0f, 0x0f, nullptr, fakeAscii_t::A_PRINTSCREEN, false},
                                          {0x10, 0x10, "KP_INS", fakeAscii_t::A_KP_0, false},
                                          {0x11, 0x11, "KP_END", fakeAscii_t::A_KP_1, false},
                                          {0x12, 0x12, "KP_DOWNARROW", fakeAscii_t::A_KP_2, false},
                                          {0x13, 0x13, "KP_PGDN", fakeAscii_t::A_KP_3, false},
                                          {0x14, 0x14, "KP_LEFTARROW", fakeAscii_t::A_KP_4, false},
                                          {0x15, 0x15, "KP_5", fakeAscii_t::A_KP_5, false},
                                          {0x16, 0x16, "KP_RIGHTARROW", fakeAscii_t::A_KP_6, false},
                                          {0x17, 0x17, "KP_HOME", fakeAscii_t::A_KP_7, false},
                                          {0x18, 0x18, "KP_UPARROW", fakeAscii_t::A_KP_8, false},
                                          {0x19, 0x19, "KP_PGUP", fakeAscii_t::A_KP_9, false},
                                          {0x1a, 0x1a, "CONSOLE", fakeAscii_t::A_CONSOLE, false},
                                          {0x1b, 0x1b, "ESCAPE", fakeAscii_t::A_ESCAPE, false},
                                          {0x1c, 0x1c, "F1", fakeAscii_t::A_F1, true},
                                          {0x1d, 0x1d, "F2", fakeAscii_t::A_F2, true},
                                          {0x1e, 0x1e, "F3", fakeAscii_t::A_F3, true},
                                          {0x1f, 0x1f, "F4", fakeAscii_t::A_F4, true},

                                          {0x20, 0x20, "SPACE", fakeAscii_t::A_SPACE, false},
                                          {'!', '!', nullptr, fakeAscii_t::A_PLING, false},
                                          {'"', '"', nullptr, fakeAscii_t::A_DOUBLE_QUOTE, false},
                                          {'#', '#', nullptr, fakeAscii_t::A_HASH, false},
                                          {'$', '$', nullptr, fakeAscii_t::A_STRING, false},
                                          {'%', '%', nullptr, fakeAscii_t::A_PERCENT, false},
                                          {'&', '&', nullptr, fakeAscii_t::A_AND, false},
                                          {0x27, 0x27, nullptr, fakeAscii_t::A_SINGLE_QUOTE, false},
                                          {'(', '(', nullptr, fakeAscii_t::A_OPEN_BRACKET, false},
                                          {')', ')', nullptr, fakeAscii_t::A_CLOSE_BRACKET, false},
                                          {'*', '*', nullptr, fakeAscii_t::A_STAR, false},
                                          {'+', '+', nullptr, fakeAscii_t::A_PLUS, false},
                                          {',', ',', nullptr, fakeAscii_t::A_COMMA, false},
                                          {'-', '-', nullptr, fakeAscii_t::A_MINUS, false},
                                          {'.', '.', nullptr, fakeAscii_t::A_PERIOD, false},
                                          {'/', '/', nullptr, fakeAscii_t::A_FORWARD_SLASH, false},
                                          {'0', '0', nullptr, fakeAscii_t::A_0, false},
                                          {'1', '1', nullptr, fakeAscii_t::A_1, false},
                                          {'2', '2', nullptr, fakeAscii_t::A_2, false},
                                          {'3', '3', nullptr, fakeAscii_t::A_3, false},
                                          {'4', '4', nullptr, fakeAscii_t::A_4, false},
                                          {'5', '5', nullptr, fakeAscii_t::A_5, false},
                                          {'6', '6', nullptr, fakeAscii_t::A_6, false},
                                          {'7', '7', nullptr, fakeAscii_t::A_7, false},
                                          {'8', '8', nullptr, fakeAscii_t::A_8, false},
                                          {'9', '9', nullptr, fakeAscii_t::A_9, false},
                                          {':', ':', nullptr, fakeAscii_t::A_COLON, false},
                                          {';', ';', "SEMICOLON", fakeAscii_t::A_SEMICOLON, false},
                                          {'<', '<', nullptr, fakeAscii_t::A_LESSTHAN, false},
                                          {'=', '=', nullptr, fakeAscii_t::A_EQUALS, false},
                                          {'>', '>', nullptr, fakeAscii_t::A_GREATERTHAN, false},
                                          {'?', '?', nullptr, fakeAscii_t::A_QUESTION, false},

                                          {'@', '@', nullptr, fakeAscii_t::A_AT, false},
                                          {'A', 'a', nullptr, fakeAscii_t::A_CAP_A, false},
                                          {'B', 'b', nullptr, fakeAscii_t::A_CAP_B, false},
                                          {'C', 'c', nullptr, fakeAscii_t::A_CAP_C, false},
                                          {'D', 'd', nullptr, fakeAscii_t::A_CAP_D, false},
                                          {'E', 'e', nullptr, fakeAscii_t::A_CAP_E, false},
                                          {'F', 'f', nullptr, fakeAscii_t::A_CAP_F, false},
                                          {'G', 'g', nullptr, fakeAscii_t::A_CAP_G, false},
                                          {'H', 'h', nullptr, fakeAscii_t::A_CAP_H, false},
                                          {'I', 'i', nullptr, fakeAscii_t::A_CAP_I, false},
                                          {'J', 'j', nullptr, fakeAscii_t::A_CAP_J, false},
                                          {'K', 'k', nullptr, fakeAscii_t::A_CAP_K, false},
                                          {'L', 'l', nullptr, fakeAscii_t::A_CAP_L, false},
                                          {'M', 'm', nullptr, fakeAscii_t::A_CAP_M, false},
                                          {'N', 'n', nullptr, fakeAscii_t::A_CAP_N, false},
                                          {'O', 'o', nullptr, fakeAscii_t::A_CAP_O, false},
                                          {'P', 'p', nullptr, fakeAscii_t::A_CAP_P, false},
                                          {'Q', 'q', nullptr, fakeAscii_t::A_CAP_Q, false},
                                          {'R', 'r', nullptr, fakeAscii_t::A_CAP_R, false},
                                          {'S', 's', nullptr, fakeAscii_t::A_CAP_S, false},
                                          {'T', 't', nullptr, fakeAscii_t::A_CAP_T, false},
                                          {'U', 'u', nullptr, fakeAscii_t::A_CAP_U, false},
                                          {'V', 'v', nullptr, fakeAscii_t::A_CAP_V, false},
                                          {'W', 'w', nullptr, fakeAscii_t::A_CAP_W, false},
                                          {'X', 'x', nullptr, fakeAscii_t::A_CAP_X, false},
                                          {'Y', 'y', nullptr, fakeAscii_t::A_CAP_Y, false},
                                          {'Z', 'z', nullptr, fakeAscii_t::A_CAP_Z, false},
                                          {'[', '[', nullptr, fakeAscii_t::A_OPEN_SQUARE, false},
                                          {0x5c, 0x5c, nullptr, fakeAscii_t::A_BACKSLASH, false},
                                          {']', ']', nullptr, fakeAscii_t::A_CLOSE_SQUARE, false},
                                          {'^', '^', nullptr, fakeAscii_t::A_CARET, false},
                                          {'_', '_', nullptr, fakeAscii_t::A_UNDERSCORE, false},

                                          {0x60, 0x60, nullptr, fakeAscii_t::A_LEFT_SINGLE_QUOTE, false},
                                          {'A', 'a', nullptr, fakeAscii_t::A_LOW_A, false},
                                          {'B', 'b', nullptr, fakeAscii_t::A_LOW_B, false},
                                          {'C', 'c', nullptr, fakeAscii_t::A_LOW_C, false},
                                          {'D', 'd', nullptr, fakeAscii_t::A_LOW_D, false},
                                          {'E', 'e', nullptr, fakeAscii_t::A_LOW_E, false},
                                          {'F', 'f', nullptr, fakeAscii_t::A_LOW_F, false},
                                          {'G', 'g', nullptr, fakeAscii_t::A_LOW_G, false},
                                          {'H', 'h', nullptr, fakeAscii_t::A_LOW_H, false},
                                          {'I', 'i', nullptr, fakeAscii_t::A_LOW_I, false},
                                          {'J', 'j', nullptr, fakeAscii_t::A_LOW_J, false},
                                          {'K', 'k', nullptr, fakeAscii_t::A_LOW_K, false},
                                          {'L', 'l', nullptr, fakeAscii_t::A_LOW_L, false},
                                          {'M', 'm', nullptr, fakeAscii_t::A_LOW_M, false},
                                          {'N', 'n', nullptr, fakeAscii_t::A_LOW_N, false},
                                          {'O', 'o', nullptr, fakeAscii_t::A_LOW_O, false},
                                          {'P', 'p', nullptr, fakeAscii_t::A_LOW_P, false},
                                          {'Q', 'q', nullptr, fakeAscii_t::A_LOW_Q, false},
                                          {'R', 'r', nullptr, fakeAscii_t::A_LOW_R, false},
                                          {'S', 's', nullptr, fakeAscii_t::A_LOW_S, false},
                                          {'T', 't', nullptr, fakeAscii_t::A_LOW_T, false},
                                          {'U', 'u', nullptr, fakeAscii_t::A_LOW_U, false},
                                          {'V', 'v', nullptr, fakeAscii_t::A_LOW_V, false},
                                          {'W', 'w', nullptr, fakeAscii_t::A_LOW_W, false},
                                          {'X', 'x', nullptr, fakeAscii_t::A_LOW_X, false},
                                          {'Y', 'y', nullptr, fakeAscii_t::A_LOW_Y, false},
                                          {'Z', 'z', nullptr, fakeAscii_t::A_LOW_Z, false},
                                          {'{', '{', nullptr, fakeAscii_t::A_OPEN_BRACE, false},
                                          {'|', '|', nullptr, fakeAscii_t::A_BAR, false},
                                          {'}', '}', nullptr, fakeAscii_t::A_CLOSE_BRACE, false},
                                          {'~', '~', nullptr, fakeAscii_t::A_TILDE, false},
                                          {0x7f, 0x7f, "DEL", fakeAscii_t::A_DELETE, false},

                                          {0x80, 0x80, "EURO", fakeAscii_t::A_EURO, false},
                                          {0x81, 0x81, "RSHIFT", fakeAscii_t::A_SHIFT2, false},
                                          {0x82, 0x82, "RCTRL", fakeAscii_t::A_CTRL2, false},
                                          {0x83, 0x83, "RALT", fakeAscii_t::A_ALT2, false},
                                          {0x84, 0x84, "F5", fakeAscii_t::A_F5, true},
                                          {0x85, 0x85, "F6", fakeAscii_t::A_F6, true},
                                          {0x86, 0x86, "F7", fakeAscii_t::A_F7, true},
                                          {0x87, 0x87, "F8", fakeAscii_t::A_F8, true},
                                          {0x88, 0x88, "CIRCUMFLEX", fakeAscii_t::A_CIRCUMFLEX, false},
                                          {0x89, 0x89, "MWHEELUP", fakeAscii_t::A_MWHEELUP, false},
                                          {0x8a, 0x9a, nullptr, fakeAscii_t::A_CAP_SCARON, false}, // ******
                                          {0x8b, 0x8b, "MWHEELDOWN", fakeAscii_t::A_MWHEELDOWN, false},
                                          {0x8c, 0x9c, nullptr, fakeAscii_t::A_CAP_OE, false}, // ******
                                          {0x8d, 0x8d, "MOUSE1", fakeAscii_t::A_MOUSE1, false},
                                          {0x8e, 0x8e, "MOUSE2", fakeAscii_t::A_MOUSE2, false},
                                          {0x8f, 0x8f, "INS", fakeAscii_t::A_INSERT, false},
                                          {0x90, 0x90, "HOME", fakeAscii_t::A_HOME, false},
                                          {0x91, 0x91, "PGUP", fakeAscii_t::A_PAGE_UP, false},
                                          {0x92, 0x92, nullptr, fakeAscii_t::A_RIGHT_SINGLE_QUOTE, false},
                                          {0x93, 0x93, nullptr, fakeAscii_t::A_LEFT_DOUBLE_QUOTE, false},
                                          {0x94, 0x94, nullptr, fakeAscii_t::A_RIGHT_DOUBLE_QUOTE, false},
                                          {0x95, 0x95, "F9", fakeAscii_t::A_F9, true},
                                          {0x96, 0x96, "F10", fakeAscii_t::A_F10, true},
                                          {0x97, 0x97, "F11", fakeAscii_t::A_F11, true},
                                          {0x98, 0x98, "F12", fakeAscii_t::A_F12, true},
                                          {0x99, 0x99, nullptr, fakeAscii_t::A_TRADEMARK, false},
                                          {0x8a, 0x9a, nullptr, fakeAscii_t::A_LOW_SCARON, false}, // ******
                                          {0x9b, 0x9b, "SHIFT_ENTER", fakeAscii_t::A_ENTER, false},
                                          {0x8c, 0x9c, nullptr, fakeAscii_t::A_LOW_OE, false}, // ******
                                          {0x9d, 0x9d, "END", fakeAscii_t::A_END, false},
                                          {0x9e, 0x9e, "PGDN", fakeAscii_t::A_PAGE_DOWN, false},
                                          {0x9f, 0xff, nullptr, fakeAscii_t::A_CAP_YDIERESIS, false}, // ******

                                          {0xa0, 0, "SHIFT_SPACE", fakeAscii_t::A_SPACE, false},
                                          {0xa1, 0xa1, nullptr, fakeAscii_t::A_EXCLAMDOWN, false},      // upside down '!' - undisplayable
                                          {L'\u00A2', L'\u00A2', nullptr, fakeAscii_t::A_CENT, false},  // cent sign
                                          {L'\u00A3', L'\u00A3', nullptr, fakeAscii_t::A_POUND, false}, // pound (as in currency) symbol
                                          {0xa4, 0, "SHIFT_KP_ENTER", fakeAscii_t::A_KP_ENTER, false},
                                          {L'\u00A5', L'\u00A5', nullptr, fakeAscii_t::A_YEN, false}, // yen symbol
                                          {0xa6, 0xa6, "MOUSE3", fakeAscii_t::A_MOUSE3, false},
                                          {0xa7, 0xa7, "MOUSE4", fakeAscii_t::A_MOUSE4, false},
                                          {0xa8, 0xa8, "MOUSE5", fakeAscii_t::A_MOUSE5, false},
                                          {L'\u00A9', L'\u00A9', nullptr, fakeAscii_t::A_COPYRIGHT, false}, // copyright symbol
                                          {0xaa, 0xaa, "UPARROW", fakeAscii_t::A_CURSOR_UP, false},
                                          {0xab, 0xab, "DOWNARROW", fakeAscii_t::A_CURSOR_DOWN, false},
                                          {0xac, 0xac, "LEFTARROW", fakeAscii_t::A_CURSOR_LEFT, false},
                                          {0xad, 0xad, "RIGHTARROW", fakeAscii_t::A_CURSOR_RIGHT, false},
                                          {L'\u00AE', L'\u00AE', nullptr, fakeAscii_t::A_REGISTERED, false}, // registered trademark symbol
                                          {0xaf, 0, nullptr, fakeAscii_t::A_UNDEFINED_7, false},
                                          {0xb0, 0, nullptr, fakeAscii_t::A_UNDEFINED_8, false},
                                          {0xb1, 0, nullptr, fakeAscii_t::A_UNDEFINED_9, false},
                                          {0xb2, 0, nullptr, fakeAscii_t::A_UNDEFINED_10, false},
                                          {0xb3, 0, nullptr, fakeAscii_t::A_UNDEFINED_11, false},
                                          {0xb4, 0, nullptr, fakeAscii_t::A_UNDEFINED_12, false},
                                          {0xb5, 0, nullptr, fakeAscii_t::A_UNDEFINED_13, false},
                                          {0xb6, 0, nullptr, fakeAscii_t::A_UNDEFINED_14, false},
                                          {0xb7, 0, nullptr, fakeAscii_t::A_UNDEFINED_15, false},
                                          {0xb8, 0, nullptr, fakeAscii_t::A_UNDEFINED_16, false},
                                          {0xb9, 0, nullptr, fakeAscii_t::A_UNDEFINED_17, false},
                                          {0xba, 0, nullptr, fakeAscii_t::A_UNDEFINED_18, false},
                                          {0xbb, 0, nullptr, fakeAscii_t::A_UNDEFINED_19, false},
                                          {0xbc, 0, nullptr, fakeAscii_t::A_UNDEFINED_20, false},
                                          {0xbd, 0, nullptr, fakeAscii_t::A_UNDEFINED_21, false},
                                          {0xbe, 0, nullptr, fakeAscii_t::A_UNDEFINED_22, false},
                                          {L'\u00BF', L'\u00BF', nullptr, fakeAscii_t::A_QUESTION_DOWN, false}, // upside-down question mark

                                          {L'\u00C0', L'\u00E0', nullptr, fakeAscii_t::A_CAP_AGRAVE, false},
                                          {L'\u00C1', L'\u00E1', nullptr, fakeAscii_t::A_CAP_AACUTE, false},
                                          {L'\u00C2', L'\u00E2', nullptr, fakeAscii_t::A_CAP_ACIRCUMFLEX, false},
                                          {L'\u00C3', L'\u00E3', nullptr, fakeAscii_t::A_CAP_ATILDE, false},
                                          {L'\u00C4', L'\u00E4', nullptr, fakeAscii_t::A_CAP_ADIERESIS, false},
                                          {L'\u00C5', L'\u00E5', nullptr, fakeAscii_t::A_CAP_ARING, false},
                                          {L'\u00C6', L'\u00E6', nullptr, fakeAscii_t::A_CAP_AE, false},
                                          {L'\u00C7', L'\u00E7', nullptr, fakeAscii_t::A_CAP_CCEDILLA, false},
                                          {L'\u00C8', L'\u00E8', nullptr, fakeAscii_t::A_CAP_EGRAVE, false},
                                          {L'\u00C9', L'\u00E9', nullptr, fakeAscii_t::A_CAP_EACUTE, false},
                                          {L'\u00CA', L'\u00EA', nullptr, fakeAscii_t::A_CAP_ECIRCUMFLEX, false},
                                          {L'\u00CB', L'\u00EB', nullptr, fakeAscii_t::A_CAP_EDIERESIS, false},
                                          {L'\u00CC', L'\u00EC', nullptr, fakeAscii_t::A_CAP_IGRAVE, false},
                                          {L'\u00CD', L'\u00ED', nullptr, fakeAscii_t::A_CAP_IACUTE, false},
                                          {L'\u00CE', L'\u00EE', nullptr, fakeAscii_t::A_CAP_ICIRCUMFLEX, false},
                                          {L'\u00CF', L'\u00EF', nullptr, fakeAscii_t::A_CAP_IDIERESIS, false},
                                          {L'\u00D0', L'\u00F0', nullptr, fakeAscii_t::A_CAP_ETH, false},
                                          {L'\u00D1', L'\u00F1', nullptr, fakeAscii_t::A_CAP_NTILDE, false},
                                          {L'\u00D2', L'\u00F2', nullptr, fakeAscii_t::A_CAP_OGRAVE, false},
                                          {L'\u00D3', L'\u00F3', nullptr, fakeAscii_t::A_CAP_OACUTE, false},
                                          {L'\u00D4', L'\u00F4', nullptr, fakeAscii_t::A_CAP_OCIRCUMFLEX, false},
                                          {L'\u00D5', L'\u00F5', nullptr, fakeAscii_t::A_CAP_OTILDE, false},
                                          {L'\u00D6', L'\u00F6', nullptr, fakeAscii_t::A_CAP_ODIERESIS, false},
                                          {L'\u00D7', L'\u00D7', "KP_STAR", fakeAscii_t::A_MULTIPLY, false},
                                          {L'\u00D8', L'\u00F8', nullptr, fakeAscii_t::A_CAP_OSLASH, false},
                                          {L'\u00D9', L'\u00F9', nullptr, fakeAscii_t::A_CAP_UGRAVE, false},
                                          {L'\u00DA', L'\u00FA', nullptr, fakeAscii_t::A_CAP_UACUTE, false},
                                          {L'\u00DB', L'\u00FB', nullptr, fakeAscii_t::A_CAP_UCIRCUMFLEX, false},
                                          {L'\u00DC', L'\u00FC', nullptr, fakeAscii_t::A_CAP_UDIERESIS, false},
                                          {L'\u00DD', L'\u00FD', nullptr, fakeAscii_t::A_CAP_YACUTE, false},
                                          {L'\u00DE', L'\u00FE', nullptr, fakeAscii_t::A_CAP_THORN, false},
                                          {L'\u00DF', L'\u00DF', nullptr, fakeAscii_t::A_GERMANDBLS, false},

                                          {L'\u00C0', L'\u00E0', nullptr, fakeAscii_t::A_LOW_AGRAVE, false},
                                          {L'\u00C1', L'\u00E1', nullptr, fakeAscii_t::A_LOW_AACUTE, false},
                                          {L'\u00C2', L'\u00E2', nullptr, fakeAscii_t::A_LOW_ACIRCUMFLEX, false},
                                          {L'\u00C3', L'\u00E3', nullptr, fakeAscii_t::A_LOW_ATILDE, false},
                                          {L'\u00C4', L'\u00E4', nullptr, fakeAscii_t::A_LOW_ADIERESIS, false},
                                          {L'\u00C5', L'\u00E5', nullptr, fakeAscii_t::A_LOW_ARING, false},
                                          {L'\u00C6', L'\u00E6', nullptr, fakeAscii_t::A_LOW_AE, false},
                                          {L'\u00C7', L'\u00E7', nullptr, fakeAscii_t::A_LOW_CCEDILLA, false},
                                          {L'\u00C8', L'\u00E8', nullptr, fakeAscii_t::A_LOW_EGRAVE, false},
                                          {L'\u00C9', L'\u00E9', nullptr, fakeAscii_t::A_LOW_EACUTE, false},
                                          {L'\u00CA', L'\u00EA', nullptr, fakeAscii_t::A_LOW_ECIRCUMFLEX, false},
                                          {L'\u00CB', L'\u00EB', nullptr, fakeAscii_t::A_LOW_EDIERESIS, false},
                                          {L'\u00CC', L'\u00EC', nullptr, fakeAscii_t::A_LOW_IGRAVE, false},
                                          {L'\u00CD', L'\u00ED', nullptr, fakeAscii_t::A_LOW_IACUTE, false},
                                          {L'\u00CE', L'\u00EE', nullptr, fakeAscii_t::A_LOW_ICIRCUMFLEX, false},
                                          {L'\u00CF', L'\u00EF', nullptr, fakeAscii_t::A_LOW_IDIERESIS, false},
                                          {L'\u00D0', L'\u00F0', nullptr, fakeAscii_t::A_LOW_ETH, false},
                                          {L'\u00D1', L'\u00F1', nullptr, fakeAscii_t::A_LOW_NTILDE, false},
                                          {L'\u00D2', L'\u00F2', nullptr, fakeAscii_t::A_LOW_OGRAVE, false},
                                          {L'\u00D3', L'\u00F3', nullptr, fakeAscii_t::A_LOW_OACUTE, false},
                                          {L'\u00D4', L'\u00F4', nullptr, fakeAscii_t::A_LOW_OCIRCUMFLEX, false},
                                          {L'\u00D5', L'\u00F5', nullptr, fakeAscii_t::A_LOW_OTILDE, false},
                                          {L'\u00D6', L'\u00F6', nullptr, fakeAscii_t::A_LOW_ODIERESIS, false},
                                          {L'\u00F7', L'\u00F7', "KP_SLASH", fakeAscii_t::A_DIVIDE, false},
                                          {L'\u00D8', L'\u00F8', nullptr, fakeAscii_t::A_LOW_OSLASH, false},
                                          {L'\u00D9', L'\u00F9', nullptr, fakeAscii_t::A_LOW_UGRAVE, false},
                                          {L'\u00DA', L'\u00FA', nullptr, fakeAscii_t::A_LOW_UACUTE, false},
                                          {L'\u00DB', L'\u00FB', nullptr, fakeAscii_t::A_LOW_UCIRCUMFLEX, false},
                                          {L'\u00DC', L'\u00FC', nullptr, fakeAscii_t::A_LOW_UDIERESIS, false},
                                          {L'\u00DD', L'\u00FD', nullptr, fakeAscii_t::A_LOW_YACUTE, false},
                                          {L'\u00DE', L'\u00FE', nullptr, fakeAscii_t::A_LOW_THORN, false},
                                          {0x9f, 0xff, nullptr, fakeAscii_t::A_LOW_YDIERESIS, false}, // *******

                                          {0x100, 0x100, "JOY0", fakeAscii_t::A_JOY0, false},
                                          {0x101, 0x101, "JOY1", fakeAscii_t::A_JOY1, false},
                                          {0x102, 0x102, "JOY2", fakeAscii_t::A_JOY2, false},
                                          {0x103, 0x103, "JOY3", fakeAscii_t::A_JOY3, false},
                                          {0x104, 0x104, "JOY4", fakeAscii_t::A_JOY4, false},
                                          {0x105, 0x105, "JOY5", fakeAscii_t::A_JOY5, false},
                                          {0x106, 0x106, "JOY6", fakeAscii_t::A_JOY6, false},
                                          {0x107, 0x107, "JOY7", fakeAscii_t::A_JOY7, false},
                                          {0x108, 0x108, "JOY8", fakeAscii_t::A_JOY8, false},
                                          {0x109, 0x109, "JOY9", fakeAscii_t::A_JOY9, false},
                                          {0x10a, 0x10a, "JOY10", fakeAscii_t::A_JOY10, false},
                                          {0x10b, 0x10b, "JOY11", fakeAscii_t::A_JOY11, false},
                                          {0x10c, 0x10c, "JOY12", fakeAscii_t::A_JOY12, false},
                                          {0x10d, 0x10d, "JOY13", fakeAscii_t::A_JOY13, false},
                                          {0x10e, 0x10e, "JOY14", fakeAscii_t::A_JOY14, false},
                                          {0x10f, 0x10f, "JOY15", fakeAscii_t::A_JOY15, false},
                                          {0x110, 0x110, "JOY16", fakeAscii_t::A_JOY16, false},
                                          {0x111, 0x111, "JOY17", fakeAscii_t::A_JOY17, false},
                                          {0x112, 0x112, "JOY18", fakeAscii_t::A_JOY18, false},
                                          {0x113, 0x113, "JOY19", fakeAscii_t::A_JOY19, false},
                                          {0x114, 0x114, "JOY20", fakeAscii_t::A_JOY20, false},
                                          {0x115, 0x115, "JOY21", fakeAscii_t::A_JOY21, false},
                                          {0x116, 0x116, "JOY22", fakeAscii_t::A_JOY22, false},
                                          {0x117, 0x117, "JOY23", fakeAscii_t::A_JOY23, false},
                                          {0x118, 0x118, "JOY24", fakeAscii_t::A_JOY24, false},
                                          {0x119, 0x119, "JOY25", fakeAscii_t::A_JOY25, false},
                                          {0x11a, 0x11a, "JOY26", fakeAscii_t::A_JOY26, false},
                                          {0x11b, 0x11b, "JOY27", fakeAscii_t::A_JOY27, false},
                                          {0x11c, 0x11c, "JOY28", fakeAscii_t::A_JOY28, false},
                                          {0x11d, 0x11d, "JOY29", fakeAscii_t::A_JOY29, false},
                                          {0x11e, 0x11e, "JOY30", fakeAscii_t::A_JOY30, false},
                                          {0x11f, 0x11f, "JOY31", fakeAscii_t::A_JOY31, false},

                                          {0x120, 0x120, "AUX0", fakeAscii_t::A_AUX0, false},
                                          {0x121, 0x121, "AUX1", fakeAscii_t::A_AUX1, false},
                                          {0x122, 0x122, "AUX2", fakeAscii_t::A_AUX2, false},
                                          {0x123, 0x123, "AUX3", fakeAscii_t::A_AUX3, false},
                                          {0x124, 0x124, "AUX4", fakeAscii_t::A_AUX4, false},
                                          {0x125, 0x125, "AUX5", fakeAscii_t::A_AUX5, false},
                                          {0x126, 0x126, "AUX6", fakeAscii_t::A_AUX6, false},
                                          {0x127, 0x127, "AUX7", fakeAscii_t::A_AUX7, false},
                                          {0x128, 0x128, "AUX8", fakeAscii_t::A_AUX8, false},
                                          {0x129, 0x129, "AUX9", fakeAscii_t::A_AUX9, false},
                                          {0x12a, 0x12a, "AUX10", fakeAscii_t::A_AUX10, false},
                                          {0x12b, 0x12b, "AUX11", fakeAscii_t::A_AUX11, false},
                                          {0x12c, 0x12c, "AUX12", fakeAscii_t::A_AUX12, false},
                                          {0x12d, 0x12d, "AUX13", fakeAscii_t::A_AUX13, false},
                                          {0x12e, 0x12e, "AUX14", fakeAscii_t::A_AUX14, false},
                                          {0x12f, 0x12f, "AUX15", fakeAscii_t::A_AUX15, false},
                                          {0x130, 0x130, "AUX16", fakeAscii_t::A_AUX16, false},
                                          {0x131, 0x131, "AUX17", fakeAscii_t::A_AUX17, false},
                                          {0x132, 0x132, "AUX18", fakeAscii_t::A_AUX18, false},
                                          {0x133, 0x133, "AUX19", fakeAscii_t::A_AUX19, false},
                                          {0x134, 0x134, "AUX20", fakeAscii_t::A_AUX20, false},
                                          {0x135, 0x135, "AUX21", fakeAscii_t::A_AUX21, false},
                                          {0x136, 0x136, "AUX22", fakeAscii_t::A_AUX22, false},
                                          {0x137, 0x137, "AUX23", fakeAscii_t::A_AUX23, false},
                                          {0x138, 0x138, "AUX24", fakeAscii_t::A_AUX24, false},
                                          {0x139, 0x139, "AUX25", fakeAscii_t::A_AUX25, false},
                                          {0x13a, 0x13a, "AUX26", fakeAscii_t::A_AUX26, false},
                                          {0x13b, 0x13b, "AUX27", fakeAscii_t::A_AUX27, false},
                                          {0x13c, 0x13c, "AUX28", fakeAscii_t::A_AUX28, false},
                                          {0x13d, 0x13d, "AUX29", fakeAscii_t::A_AUX29, false},
                                          {0x13e, 0x13e, "AUX30", fakeAscii_t::A_AUX30, false},
                                          {0x13f, 0x13f, "AUX31", fakeAscii_t::A_AUX31, false}}};
// NOLINTEND(modernize-use-designated-initializers)

// the code of an index of the free codes
constexpr fakeAscii_t DynamicKey(std::size_t index) noexcept
{
    return fakeAscii_t::A_UNDEFINED_7 + static_cast<int>(index);
}

// whether a key's code is in the table
constexpr bool InTable(fakeAscii_t key) noexcept
{
    return key >= fakeAscii_t::A_NULL && key < fakeAscii_t::MAX_KEYS;
}

// an ASCII letter in lower case, another character as it is
constexpr char Lower(char character) noexcept
{
    return character >= 'A' && character <= 'Z' ? static_cast<char>(character - 'A' + 'a') : character;
}

// two names equal but for the case of their ASCII letters
constexpr bool EqualNoCase(std::string_view a, std::string_view b) noexcept
{
    return std::ranges::equal(a, b, [](char x, char y) { return Lower(x) == Lower(y); });
}

// the value of a hexadecimal digit, either case; -1 for another character
constexpr int HexDigit(char character) noexcept
{
    const char lower = Lower(character);

    if (lower >= '0' && lower <= '9') {
        return lower - '0';
    }
    if (lower >= 'a' && lower <= 'f') {
        return lower - 'a' + 10;
    }
    return -1;
}

// a text as a key's name: its first 31 characters
KeyText Text(std::string_view text) noexcept
{
    KeyText result;
    const std::size_t count = std::min(text.size(), result.characters.size() - 1);
    std::copy_n(text.begin(), count, result.characters.begin());
    return result;
}

// the character a key types, for a key below the joysticks' that types one: Space and the two Enter keys as the codes
// of their Shift forms, which a bind of that one character reads back as the key; nothing otherwise
std::optional<KeyText> Character(const keyNames_t& names, fakeAscii_t key) noexcept
{
    if (key <= fakeAscii_t::A_NULL || key >= fakeAscii_t::A_JOY0 || names[key].lower == 0) {
        return std::nullopt;
    }

    auto shown = static_cast<fakeAscii_t>(names[key].upper);
    if (key == fakeAscii_t::A_SPACE) {
        shown = fakeAscii_t::A_SHIFT_SPACE;
    }
    else if (key == fakeAscii_t::A_ENTER) {
        shown = fakeAscii_t::A_SHIFT_ENTER;
    }
    else if (key == fakeAscii_t::A_KP_ENTER) {
        shown = fakeAscii_t::A_SHIFT_KP_ENTER;
    }

    KeyText text;
    text.characters[0] = static_cast<char>(std::to_underlying(shown));
    return text;
}

// a key's name in the table, if it has one
std::optional<KeyText> Name(const keyNames_t& names, fakeAscii_t key) noexcept
{
    if (names[key].name == nullptr) {
        return std::nullopt;
    }
    return Text(names[key].name);
}

// "0x" and a key's code in two hexadecimal digits, the form KeyOfName reads back
KeyText Hex(fakeAscii_t key) noexcept
{
    constexpr std::string_view kDigits = "0123456789ABCDEF";
    const auto code = static_cast<unsigned>(std::to_underlying(key));
    const std::array<char, 4> text{'0', 'x', kDigits[(code >> 4U) & 15U], kDigits[code & 15U]};
    return Text(std::string_view(text.data(), text.size()));
}

} // namespace

const keyNames_t& DefaultKeyNames() noexcept
{
    return kDefaultKeyNames;
}

fakeAscii_t SharedKey(const keyNames_t& names, fakeAscii_t key) noexcept
{
    return static_cast<fakeAscii_t>(names[key].upper);
}

std::optional<fakeAscii_t> KeyOfName(const keyNames_t& names, std::string_view name) noexcept
{
    if (name.empty()) {
        return std::nullopt;
    }

    if (name.size() == 1) {
        return SharedKey(names, static_cast<fakeAscii_t>(static_cast<unsigned char>(name.front())));
    }

    const auto* const entry =
        std::ranges::find_if(names, [name](const keyname_t& candidate) { return candidate.name != nullptr && EqualNoCase(name, candidate.name); });
    if (entry != names.end()) {
        return entry->keynum;
    }

    if (name.size() == 4 && name.starts_with("0x") && HexDigit(name[2]) >= 0 && HexDigit(name[3]) >= 0) {
        return static_cast<fakeAscii_t>((HexDigit(name[2]) * 16) + HexDigit(name[3]));
    }

    return std::nullopt;
}

KeyText NameOfKey(const keyNames_t& names, fakeAscii_t key) noexcept
{
    if (!InTable(key)) {
        return Text("<OUT OF RANGE>");
    }
    return Name(names, key).or_else([&] { return Character(names, key); }).value_or(Hex(key));
}

KeyText CharacterOfKey(const keyNames_t& names, fakeAscii_t key) noexcept
{
    if (!InTable(key)) {
        return Text("<OUT OF RANGE>");
    }
    return Character(names, key).or_else([&] { return Name(names, key); }).value_or(Hex(key));
}

std::optional<fakeAscii_t> DynamicKeys::CodeFor(keyNames_t& names, std::string_view name, const std::array<bool, kDynamicKeyCount>& mayTake) noexcept
{
    std::optional<std::size_t> neverGiven;
    std::optional<std::size_t> taken;

    for (std::size_t i = 0; i < names_.size(); ++i) {
        const std::string_view given = names_[i].data();

        if (!given.empty() && EqualNoCase(given, name)) {
            return DynamicKey(i);
        }

        if (given.empty()) {
            neverGiven = neverGiven.value_or(i);
        }
        else if (!taken && mayTake[i]) {
            taken = i;
        }
    }

    const std::optional<std::size_t> slot = neverGiven ? neverGiven : taken;
    if (!slot) {
        return std::nullopt;
    }

    std::array<char, 32>& stored = names_[*slot];
    stored = Text(name).characters;
    names[DynamicKey(*slot)].name = stored.data();
    return DynamicKey(*slot);
}

} // namespace client_input

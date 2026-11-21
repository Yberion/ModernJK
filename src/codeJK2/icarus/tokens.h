/*
===========================================================================
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors
Copyright (C) 2026, ModernJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
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

#pragma once

// tokens.h -- the identifiers of a compiled script (.IBI) in one space: the tokenizer's tokens, then the interpreter's
// tokens, block identifiers and types, each part starting where the one before ends. A block's or a block member's
// identifier holds any of them, so they are one enumeration; the values are those of the compiled scripts.

enum class icarusToken_t
{
	TK_EOF = -1,
	TK_UNDEFINED,
	TK_COMMENT,
	TK_EOL,
	TK_CHAR,
	TK_STRING,
	TK_INT,
	TK_INTEGER = TK_INT,
	TK_FLOAT,
	TK_IDENTIFIER,
	TK_USERDEF,

	//Token defines
	TK_BLOCK_START = TK_USERDEF,
	TK_BLOCK_END,
	TK_VECTOR_START,
	TK_VECTOR_END,
	TK_OPEN_PARENTHESIS,
	TK_CLOSED_PARENTHESIS,
	TK_VECTOR,
	TK_GREATER_THAN,
	TK_LESS_THAN,
	TK_EQUALS,
	TK_NOT,

	NUM_USER_TOKENS,

	//ID defines
	ID_AFFECT = NUM_USER_TOKENS,
	ID_SOUND,
	ID_MOVE,
	ID_ROTATE,
	ID_WAIT,
	ID_BLOCK_START,
	ID_BLOCK_END,
	ID_SET,
	ID_LOOP,
	ID_LOOPEND,
	ID_PRINT,
	ID_USE,
	ID_FLUSH,
	ID_RUN,
	ID_KILL,
	ID_REMOVE,
	ID_CAMERA,
	ID_GET,
	ID_RANDOM,
	ID_IF,
	ID_ELSE,
	ID_REM,
	ID_TASK,
	ID_DO,
	ID_DECLARE,
	ID_FREE,
	ID_DOWAIT,
	ID_SIGNAL,
	ID_WAITSIGNAL,
	ID_PLAY,

	ID_TAG,
	ID_EOF,
	NUM_IDS,

	//Type defines
	//Wait types
	TYPE_WAIT_COMPLETE	 = NUM_IDS,
	TYPE_WAIT_TRIGGERED,

	//Set types
	TYPE_ANGLES,
	TYPE_ORIGIN,

	//Affect types
	TYPE_INSERT,
	TYPE_FLUSH,

	//Camera types
	TYPE_PAN,
	TYPE_ZOOM,
	TYPE_MOVE,
	TYPE_FADE,
	TYPE_PATH,
	TYPE_ENABLE,
	TYPE_DISABLE,
	TYPE_SHAKE,
	TYPE_ROLL,
	TYPE_TRACK,
	TYPE_DISTANCE,
	TYPE_FOLLOW,

	//Variable type
	TYPE_VARIABLE,

	TYPE_EOF,
	NUM_TYPES
};

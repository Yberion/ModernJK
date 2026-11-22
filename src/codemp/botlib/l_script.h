/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
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

/*****************************************************************************
 * name:		l_script.h
 *
 * desc:		lexicographical parser
 *
 * $Archive: /source/code/botlib/l_script.h $
 * $Author: Mrelusive $
 * $Revision: 2 $
 * $Modtime: 10/05/99 3:32p $
 * $Date: 10/05/99 3:42p $
 *
 *****************************************************************************/

#pragma once

// undef if binary numbers of the form 0b... or 0B... are not allowed
#define BINARYNUMBERS
// undef if not using the token.intvalue and token.floatvalue
#define NUMBERVALUE
// use dollar sign also as punctuation
#define DOLLAR

// maximum token length
inline constexpr int MAX_TOKEN = 1024;

#if defined(BSPC) && !defined(QDECL)
#define QDECL
#endif

// script flags
enum class scriptParseFlags_t {
    SCFL_NONE = 0,
    SCFL_NOERRORS = 0x0001,
    SCFL_NOWARNINGS = 0x0002,
    SCFL_NOSTRINGWHITESPACES = 0x0004,
    SCFL_NOSTRINGESCAPECHARS = 0x0008,
    SCFL_PRIMITIVE = 0x0010,
    SCFL_NOBINARYNUMBERS = 0x0020,
    SCFL_NONUMBERVALUES = 0x0040,
};
template <> inline constexpr bool enableEnumFlags<scriptParseFlags_t> = true;

// token types: tokenType_t, in q_shared.h

// string sub type
//---------------
//		the length of the string
// literal sub type
//----------------
//		the ASCII code of the literal
// number sub type
//---------------
inline constexpr int TT_DECIMAL = 0x0008; // decimal number
inline constexpr int TT_HEX = 0x0100;     // hexadecimal number
inline constexpr int TT_OCTAL = 0x0200;   // octal number
#ifdef BINARYNUMBERS
inline constexpr int TT_BINARY = 0x0400;   // binary number
#endif                                     // BINARYNUMBERS
inline constexpr int TT_FLOAT = 0x0800;    // floating point number
inline constexpr int TT_INTEGER = 0x1000;  // integer number
inline constexpr int TT_LONG = 0x2000;     // long number
inline constexpr int TT_UNSIGNED = 0x4000; // unsigned number
// punctuation sub type
//--------------------
enum class punctuationId_t {
    P_RSHIFT_ASSIGN = 1,
    P_LSHIFT_ASSIGN,
    P_PARMS,
    P_PRECOMPMERGE,

    P_LOGIC_AND,
    P_LOGIC_OR,
    P_LOGIC_GEQ,
    P_LOGIC_LEQ,
    P_LOGIC_EQ,
    P_LOGIC_UNEQ,

    P_MUL_ASSIGN,
    P_DIV_ASSIGN,
    P_MOD_ASSIGN,
    P_ADD_ASSIGN,
    P_SUB_ASSIGN,
    P_INC,
    P_DEC,

    P_BIN_AND_ASSIGN,
    P_BIN_OR_ASSIGN,
    P_BIN_XOR_ASSIGN,
    P_RSHIFT,
    P_LSHIFT,

    P_POINTERREF,
    P_CPP1,
    P_CPP2,
    P_MUL,
    P_DIV,
    P_MOD,
    P_ADD,
    P_SUB,
    P_ASSIGN,

    P_BIN_AND,
    P_BIN_OR,
    P_BIN_XOR,
    P_BIN_NOT,

    P_LOGIC_NOT,
    P_LOGIC_GREATER,
    P_LOGIC_LESS,

    P_REF,
    P_COMMA,
    P_SEMICOLON,
    P_COLON,
    P_QUESTIONMARK,

    P_PARENTHESESOPEN,
    P_PARENTHESESCLOSE,
    P_BRACEOPEN,
    P_BRACECLOSE,
    P_SQBRACKETOPEN,
    P_SQBRACKETCLOSE,
    P_BACKSLASH,

    P_PRECOMP,
    P_DOLLAR,
    P_ATSIGN,
};
// name sub type
//-------------
//		the length of the name

// punctuation
typedef struct punctuation_s {
    const char* p;              // punctuation character(s)
    punctuationId_t n;          // punctuation indication
    struct punctuation_s* next; // next punctuation
} punctuation_t;

// token
typedef struct token_s {
    char string[MAX_TOKEN]; // available token
    tokenType_t type;       // last read token type
    int subtype;            // last read token sub type
#ifdef NUMBERVALUE
    unsigned long int intvalue; // integer value
    long double floatvalue;     // floating point value
#endif                          // NUMBERVALUE
    char* whitespace_p;         // start of white space before token
    char* endwhitespace_p;      // start of white space before token
    int line;                   // line the token was on
    int linescrossed;           // lines crossed in white space
    struct token_s* next;       // next token in chain
} token_t;

// the punctuation of a TT_PUNCTUATION token, kept in its sub type (a field that holds other things for the other types)
[[nodiscard]] inline punctuationId_t Token_Punctuation(const token_t& token)
{
    return static_cast<punctuationId_t>(token.subtype);
}
inline void Token_SetPunctuation(token_t& token, punctuationId_t punctuation)
{
    token.subtype = to_underlying(punctuation);
}

// script file
typedef struct script_s {
    char filename[1024];         // file name of the script
    char* buffer;                // buffer containing the script
    char* script_p;              // current pointer in the script
    char* end_p;                 // pointer to the end of the script
    char* lastscript_p;          // script pointer before reading token
    char* whitespace_p;          // begin of the white space
    char* endwhitespace_p;       // end of the white space
    int length;                  // length of the script in bytes
    int line;                    // current line in script
    int lastline;                // line before reading token
    int tokenavailable;          // set by UnreadLastToken
    scriptParseFlags_t flags;    // several script flags
    punctuation_t* punctuations; // the punctuations used in the script
    punctuation_t** punctuationtable;
    token_t token;         // available token
    struct script_s* next; // next script in a chain
} script_t;

// read a token from the script
int PS_ReadToken(script_t* script, token_t* token);
// expect a certain token
int PS_ExpectTokenString(script_t* script, char* string);
// expect a certain token type
int PS_ExpectTokenType(script_t* script, tokenType_t type, int subtype, token_t* token);
// expect a token
int PS_ExpectAnyToken(script_t* script, token_t* token);
// returns true when the token is available
int PS_CheckTokenString(script_t* script, char* string);
// returns true and reads the token when a token with the given type is available
int PS_CheckTokenType(script_t* script, tokenType_t type, int subtype, token_t* token);
// skip tokens until the given token string is read
int PS_SkipUntilString(script_t* script, char* string);
// unread the last token read from the script
void PS_UnreadLastToken(script_t* script);
// unread the given token
void PS_UnreadToken(script_t* script, token_t* token);
// returns the next character of the read white space, returns nullptr if none
char PS_NextWhiteSpaceChar(script_t* script);
// remove any leading and trailing double quotes from the token
void StripDoubleQuotes(char* string);
// remove any leading and trailing single quotes from the token
void StripSingleQuotes(char* string);
// read a possible signed integer
signed long int ReadSignedInt(script_t* script);
// read a possible signed floating point number
long double ReadSignedFloat(script_t* script);
// set an array with punctuations, nullptr restores default C/C++ set
void SetScriptPunctuations(script_t* script, punctuation_t* p);
// set script flags
void SetScriptFlags(script_t* script, scriptParseFlags_t flags);
// get script flags
scriptParseFlags_t GetScriptFlags(script_t* script);
// reset a script
void ResetScript(script_t* script);
// returns true if at the end of the script
int EndOfScript(script_t* script);
// returns a pointer to the punctuation with the given number
const char* PunctuationFromNum(script_t* script, punctuationId_t num);
// load a script from the given file at the given offset with the given length
script_t* LoadScriptFile(const char* filename);
// load a script from the given memory with the given length
script_t* LoadScriptMemory(char* ptr, int length, const char* name);
// free a script
void FreeScript(script_t* script);
// set the base folder to load files from
void PS_SetBaseFolder(const char* path);
// print a script error with filename and line number
void VScriptError(script_t* script, fmt::string_view str, fmt::format_args args);
template <typename... T> void ScriptError(script_t* script, fmt::format_string<T...> str, T&&... args)
{
    VScriptError(script, str.get(), Q_FormatArgs(args...));
}
// print a script warning with filename and line number
void VScriptWarning(script_t* script, fmt::string_view str, fmt::format_args args);
template <typename... T> void ScriptWarning(script_t* script, fmt::format_string<T...> str, T&&... args)
{
    VScriptWarning(script, str.get(), Q_FormatArgs(args...));
}

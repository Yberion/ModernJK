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

#ifndef __STRIPPUB_H
#define __STRIPPUB_H

inline constexpr int STRIP_VERSION = 1;

inline constexpr int MAX_STRINGS = 256;

enum class spLanguage_t {
    SP_LANGUAGE_ENGLISH = 0,
    SP_LANGUAGE_FRENCH,
    SP_LANGUAGE_GERMAN,
    SP_LANGUAGE_BRITISH,
    SP_LANGUAGE_KOREAN,
    SP_LANGUAGE_TAIWANESE,
    SP_LANGUAGE_ITALIAN,
    SP_LANGUAGE_SPANISH,
    SP_LANGUAGE_JAPANESE,
    SP_LANGUAGE_10,
    SP_LANGUGAGE_MAX,
    SP_LANGUAGE_ALL = 255
};

inline constexpr int SP_PACKAGE = 0xff00;
inline constexpr int SP_STRING = 0x00ff;

#define SP_GET_PACKAGE(x) ((x & SP_PACKAGE) >> 8)

// Flags
enum class spStringFlags_t {
    SP_FLAG_NONE = 0,
    SP_FLAG1 = 0x00000001, // CENTERED
    SP_FLAG2 = 0x00000002,
    SP_FLAG3 = 0x00000004,
    SP_FLAG4 = 0x00000008,
    SP_FLAG5 = 0x00000010,
    SP_FLAG6 = 0x00000020,
    SP_FLAG7 = 0x00000040,
    SP_FLAG8 = 0x00000080,
    SP_FLAG9 = 0x00000100,
    SP_FLAG_ORIGINAL = 0x00000200,
};
template <> inline constexpr bool enableEnumFlags<spStringFlags_t> = true;

// Registration
enum class spRegistrationFlags_t {
    SP_REGISTER_NONE = 0,
    SP_REGISTER_CLIENT = (0x01),
    SP_REGISTER_SERVER = (0x02),
    SP_REGISTER_MENU = (0x04),
    SP_REGISTER_REQUIRED = (0x08),
};
template <> inline constexpr bool enableEnumFlags<spRegistrationFlags_t> = true;

// Registration
bool JK2SP_Register(const char* Package, spRegistrationFlags_t Registration);
void JK2SP_Unload(spRegistrationFlags_t Registration);

// Direct string functions
int JK2SP_GetStringID(const char* Reference);
#ifdef _DEBUG
const char* SP_GetReferenceText(unsigned short ID, const char*& psPackageName, const char*& psPackageReference, const char*& psText);
#endif
const char* JK2SP_GetStringText(unsigned short ID);
const char* JK2SP_GetStringTextString(const char* Reference);

// Initialization
void JK2SP_Init(void);
void JK2SP_CheckForLanguageUpdates(void);
inline void JK2SP_Shutdown(void)
{
    JK2SP_Unload(spRegistrationFlags_t::SP_REGISTER_CLIENT | spRegistrationFlags_t::SP_REGISTER_SERVER | spRegistrationFlags_t::SP_REGISTER_MENU |
                 spRegistrationFlags_t::SP_REGISTER_REQUIRED);
}

extern cvar_t* sp_language;
// query function from font code
//
/*inline bool Language_IsKorean(void)
{
    return (sp_language && sp_language->integer == spLanguage_t::SP_LANGUAGE_KOREAN);
}

inline bool Language_IsTaiwanese(void)
{
    return (sp_language && sp_language->integer == spLanguage_t::SP_LANGUAGE_TAIWANESE);
}

inline bool Language_IsJapanese(void)
{
    return (sp_language && sp_language->integer == spLanguage_t::SP_LANGUAGE_JAPANESE);
}
*/
inline int Language_GetIntegerValue(void)
{
    if (sp_language) {
        return sp_language->integer;
    }

    return 0;
}

#endif // __STRIPPUB_H

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

#include "q_string.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "q_color.h"

int Q_isprint(int c)
{
    if (c >= 0x20 && c <= 0x7E)
        return (1);
    return (0);
}

int Q_isprintext(int c)
{
    if (c >= 0x20 && c <= 0x7E)
        return (1);
    if (c >= 0x80 && c <= 0xFE)
        return (1);
    return (0);
}

int Q_isgraph(int c)
{
    if (c >= 0x21 && c <= 0x7E)
        return (1);
    if (c >= 0x80 && c <= 0xFE)
        return (1);
    return (0);
}

int Q_islower(int c)
{
    if (c >= 'a' && c <= 'z')
        return (1);
    return (0);
}

int Q_isupper(int c)
{
    if (c >= 'A' && c <= 'Z')
        return (1);
    return (0);
}

int Q_isalpha(int c)
{
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
        return (1);
    return (0);
}

bool Q_isanumber(const char* s)
{
    const char* p;
    double ret;

    if (*s == '\0')
        return false;

    // the conversion sets errno when it fails and never clears it: an error left by an earlier call is not this text's
    errno = 0;
    ret = Q_strtod(s, &p);

    if (ret == HUGE_VAL || errno == ERANGE)
        return false;

    return (*p == '\0');
}

bool Q_isintegral(float f)
{
    return ((int)f == f);
}

char* Q_strrchr(const char* string, int c)
{
    char cc = c;
    char* s;
    char* sp = (char*)nullptr;

    s = (char*)string;

    while (*s) {
        if (*s == cc)
            sp = s;
        s++;
    }
    if (cc == 0)
        sp = s;

    return sp;
}

/*
=============
Q_strncpyz

Safe strncpy that ensures a trailing zero
=============
*/
void Q_strncpyz(char* dest, const char* src, int destsize)
{
    assert(src);
    assert(dest);
    assert(destsize > 0);
    if (destsize < 1) // a size computed from a length or an offset that does not fit an int: nothing is written
        return;

    strncpy(dest, src, destsize - 1);
    dest[destsize - 1] = 0;
}

int Q_stricmpn(const char* s1, const char* s2, int n)
{
    int c1, c2;

    if (s1 == nullptr) {
        if (s2 == nullptr)
            return 0;
        else
            return -1;
    }
    else if (s2 == nullptr)
        return 1;

    do {
        c1 = *s1++;
        c2 = *s2++;

        if (!n--) {
            return 0; // strings are equal until end point
        }

        if (c1 != c2) {
            if (c1 >= 'a' && c1 <= 'z') {
                c1 -= ('a' - 'A');
            }
            if (c2 >= 'a' && c2 <= 'z') {
                c2 -= ('a' - 'A');
            }
            if (c1 != c2) {
                return c1 < c2 ? -1 : 1;
            }
        }
    } while (c1);

    return 0; // strings are equal
}

int Q_stricmp(const char* s1, const char* s2)
{
    return (s1 && s2) ? Q_stricmpn(s1, s2, 99999) : -1;
}

int Q_strncmp(const char* s1, const char* s2, int n)
{
    int c1, c2;

    do {
        c1 = *s1++;
        c2 = *s2++;

        if (!n--) {
            return 0; // strings are equal until end point
        }

        if (c1 != c2) {
            return c1 < c2 ? -1 : 1;
        }
    } while (c1);

    return 0; // strings are equal
}

char* Q_strlwr(char* s1)
{
    char* s;

    s = s1;
    while (*s) {
        *s = tolower(*s);
        s++;
    }
    return s1;
}

char* Q_strupr(char* s1)
{
    char* s;

    s = s1;
    while (*s) {
        *s = toupper(*s);
        s++;
    }
    return s1;
}

// never goes past bounds or leaves without a terminating 0
void Q_strcat(char* dest, int size, const char* src)
{
    int l1;

    l1 = strlen(dest);
    if (l1 >= size) {
        // Q_Error(errorParm_t::ERR_FATAL, "Q_strcat: already overflowed");
        return;
    }
    if (strlen(src) + 1 > (size_t)(size - l1)) { // do the error here instead of in Q_strncpyz to get a meaningful msg
        // Q_Error(errorParm_t::ERR_FATAL, "Q_strcat: cannot append \"{}\" to \"{}\"", src, dest);
        return;
    }
    Q_strncpyz(dest + l1, src, size - l1);
}

/*
 * Find the first occurrence of find in s.
 */
const char* Q_stristr(const char* s, const char* find)
{
    char c, sc;
    size_t len;

    if ((c = *find++) != 0) {
        if (c >= 'a' && c <= 'z') {
            c -= ('a' - 'A');
        }
        len = strlen(find);
        do {
            do {
                if ((sc = *s++) == 0)
                    return nullptr;
                if (sc >= 'a' && sc <= 'z') {
                    sc -= ('a' - 'A');
                }
            } while (sc != c);
        } while (Q_stricmpn(s, find, len) != 0);
        s--;
    }
    return s;
}

int Q_PrintStrlen(const char* string)
{
    int len;
    const char* p;

    if (!string) {
        return 0;
    }

    len = 0;
    p = string;
    while (*p) {
        if (Q_IsColorString(p)) {
            p += 2;
            continue;
        }
        p++;
        len++;
    }

    return len;
}

int Q_PrintStrLenTo(const char* str, int chars, char* color)
{
    int offset = 0;
    char lastColor = 0;
    int i;

    for (i = 0; i < chars && str[i]; i++) {
        if (Q_IsColorString(&str[i])) {
            i++;
            lastColor = str[i];
        }
        else {
            offset++;
        }
    }

    if (color) {
        *color = lastColor;
    }

    return offset;
}

char* Q_CleanStr(char* string)
{
    char* d;
    char* s;
    int c;

    s = string;
    d = string;
    while ((c = *s) != 0) {
        if (Q_IsColorString(s)) {
            s++;
        }
        else if (c >= 0x20 && c <= 0x7E) {
            *d++ = c;
        }
        s++;
    }
    *d = '\0';

    return string;
}

/*
==================
Q_StripColor

Strips coloured strings in-place using multiple passes: "fgs^^56fds" -> "fgs^6fds" -> "fgsfds"

This function modifies INPUT (is mutable)

(Also strips ^8 and ^9)
==================
*/
void Q_StripColor(char* text)
{
    bool doPass = true;
    char* read;
    char* write;

    while (doPass) {
        doPass = false;
        read = write = text;
        while (*read) {
            if (Q_IsColorStringExt(read)) {
                doPass = true;
                read += 2;
            }
            else {
                // Avoid writing the same data over itself
                if (write != read) {
                    *write = *read;
                }
                write++;
                read++;
            }
        }
        if (write < read) {
            // Add trailing NUL byte if string has shortened
            *write = '\0';
        }
    }
}

/*
Q_strstrip

Description:	Replace strip[x] in string with repl[x] or remove characters entirely
Mutates:		string
Return:			--

Examples:		Q_strstrip( "Bo\nb is h\rairy!!", "\n\r!", "123" );	// "Bo1b is h2airy33"
Q_strstrip( "Bo\nb is h\rairy!!", "\n\r!", "12" );	// "Bo1b is h2airy"
Q_strstrip( "Bo\nb is h\rairy!!", "\n\r!", nullptr );	// "Bob is hairy"
*/

void Q_strstrip(char* string, const char* strip, const char* repl)
{
    char *out = string, *p = string, c;
    const char* s = strip;
    int replaceLen = repl ? strlen(repl) : 0, offset = 0;
    bool recordChar = true;

    while ((c = *p++) != '\0') {
        recordChar = true;
        for (s = strip; *s; s++) {
            offset = s - strip;
            if (c == *s) {
                if (!repl || offset >= replaceLen)
                    recordChar = false;
                else
                    c = repl[offset];
                break;
            }
        }
        if (recordChar)
            *out++ = c;
    }
    *out = '\0';
}

/*
Q_strchrs

Description:	Find any characters in a string. Think of it as a shorthand strchr loop.
Mutates:		--
Return:			first instance of any characters found
otherwise nullptr
*/

const char* Q_strchrs(const char* string, const char* search)
{
    const char *p = string, *s = search;

    while (*p != '\0') {
        for (s = search; *s; s++) {
            if (*p == *s)
                return p;
        }
        p++;
    }

    return nullptr;
}

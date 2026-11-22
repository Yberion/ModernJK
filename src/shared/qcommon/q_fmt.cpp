/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
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

// q_fmt.cpp -- the functions of q_fmt.h that are the same in every engine and module: the formatting into a text, a
// buffer or a file, and the rewriting of a printf format read at run time. The levels' functions are templates of
// q_fmt.h that hand the format and the packed arguments to the binary's Q_VPrint and Q_VError.
#include <algorithm>
#include <cassert>
#include <string_view>

#include <fmt/format.h>

#include "q_fmt.h"

namespace {

// a format that does not match its arguments at run time (a null const char*, a negative width given as an argument,
// a format read at run time): the format written as it is with the error, so that the message is still seen and the
// game goes on
size_t FormatError(std::span<char> dest, fmt::string_view fmt, const fmt::format_error& error)
{
    const auto result = fmt::format_to_n(dest.data(), dest.size() - 1, "{} [format error: {}]", fmt, error.what());
    *result.out = '\0';
    return result.size;
}

// whether the text after a % is a printf conversion: flags, width, precision, length, then the conversion's letter
bool IsPrintfConversion(const char* p)
{
    const auto skip = [&p](std::string_view set) {
        while (*p && set.find(*p) != std::string_view::npos)
            ++p;
    };
    skip("-+#0");
    skip("*0123456789");
    if (*p == '.') {
        ++p;
        skip("*0123456789");
    }
    skip("hljztL");
    return *p && std::string_view("diouxXeEfFgGaAcspn").find(*p) != std::string_view::npos;
}

} // namespace

fmt::format_context::iterator fmt::formatter<formatText_t>::format(const formatText_t& text, fmt::format_context& ctx) const
{
    return fmt::formatter<fmt::string_view>::format({text.c_str(), text.size()}, ctx);
}

formatText_t Q_VFormat(fmt::string_view fmt, fmt::format_args args)
{
    formatText_t text;
    try {
        const auto result = fmt::vformat_to_n(text.inline_, formatText_t::INLINE_SIZE - 1, fmt, args);
        text.size_ = result.size;
        if (result.size < formatText_t::INLINE_SIZE) {
            *result.out = '\0';
        }
        else {
            text.heap_ = std::make_unique_for_overwrite<char[]>(result.size + 1);
            const auto again = fmt::vformat_to_n(text.heap_.get(), result.size, fmt, args);
            *again.out = '\0';
        }
    }
    catch (const fmt::format_error& error) {
        text.heap_.reset();
        text.size_ = std::min(FormatError(text.inline_, fmt, error), formatText_t::INLINE_SIZE - 1);
    }
    return text;
}

size_t Q_VFormatToCut(std::span<char> dest, fmt::string_view fmt, fmt::format_args args)
{
    assert(!dest.empty());
    try {
        const auto result = fmt::vformat_to_n(dest.data(), dest.size() - 1, fmt, args);
        *result.out = '\0';
        return result.size;
    }
    catch (const fmt::format_error& error) {
        return FormatError(dest, fmt, error);
    }
}

size_t Q_VFormatTo(std::span<char> dest, fmt::string_view fmt, fmt::format_args args)
{
    const size_t length = Q_VFormatToCut(dest, fmt, args);
    if (length >= dest.size())
        Q_Println("Q_FormatTo: Output length {} too short, require {} bytes.", dest.size(), length + 1);
    return length;
}

bool Q_VFilePrint(std::FILE* file, lineEnd_t end, fmt::string_view fmt, fmt::format_args args)
{
    fmt::memory_buffer buffer;
    try {
        fmt::vformat_to(fmt::appender(buffer), fmt, args);
    }
    catch (const fmt::format_error& error) {
        buffer.clear();
        fmt::format_to(fmt::appender(buffer), "{} [format error: {}]", fmt, error.what());
    }
    if (end == lineEnd_t::NEWLINE)
        buffer.push_back('\n');
    return fwrite(buffer.data(), 1, buffer.size(), file) == buffer.size();
}

// The game's data holds formats in printf's syntax, read at run time: the StringEd texts of the pk3s ("Impossible
// d'ouvrir '%s'"), the sounds of animevents.cfg, a target_speaker's noise, the menus' team orders. Those files are the
// base game's and the mods' and are left as they are, so each such text is rewritten into {fmt}'s syntax when it is
// read, then formatted with fmt::runtime like the formats of the code.
void Q_FormatFromPrintf(std::span<char> dest, const char* printfFormat, const char* origin)
{
    assert(!dest.empty());
    const size_t limit = dest.size() - 1;
    size_t n = 0;
    bool unknown = false;
    const auto put = [&](char c) {
        if (n < limit)
            dest[n++] = c;
    };
    for (const char* p = printfFormat; *p; ++p) {
        if (*p == '{' || *p == '}') {
            put(*p);
            put(*p);
        }
        else if (*p == '%' && (p[1] == 's' || p[1] == 'd' || p[1] == 'i')) {
            put('{');
            put('}');
            ++p;
        }
        else if (*p == '%' && p[1] == '%') {
            put('%');
            ++p;
        }
        else {
            // a % that does not start a printf conversion is the character itself ("50% more", "400 %."); a
            // conversion other than %s, %d and %i is one this function does not know
            if (*p == '%' && IsPrintfConversion(p + 1))
                unknown = true;
            put(*p);
        }
    }
    dest[n] = '\0';
    if (unknown)
        Q_PrintlnWarning("{}: a conversion of \"{}\" is not one of %s, %d, %i and %%, kept as it is", origin, printfFormat);
}

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

// q_fmt.h -- the printing and formatting functions, the same in every engine and module of every tree, on {fmt}: the
// format strings are checked against their arguments at compile time. A message goes through the function of its
// level, and the engine writes the level's colour and prefix, so that a level looks the same wherever it is printed
// from. A function whose name holds "ln" ends the line after the text. Each engine and module defines Q_VPrint and
// Q_VError, where its messages and errors go: the engine's own console, or the engine through the module's import
// table, which receives the format and the packed arguments and formats them.
#pragma once

#include <cstddef>
#include <cstdio>
#include <memory>
#include <span>
#include <type_traits>

#include <fmt/base.h>

#include "q_platform.h"

// the codes of Q_Error; each tree defines its own set in its qcommon/q_shared.h
enum class errorParm_t;

// the level of a message
enum class printLevel_t {
    PRINT_MESSAGE,
    PRINT_DEVELOPER, // printed only with developer
    PRINT_WARNING,   // written after "^3WARNING: "
    PRINT_ERROR      // written after "^1ERROR: "; stops nothing, unlike Q_Error
};

// whether a message ends its line
enum class lineEnd_t {
    NEWLINE, // the line ended after the text
    NONE     // the text written as it is, a part of a line that the next message continues
};

namespace q_fmt_detail {

// a null C string written "(null)", as printf wrote it: {fmt} refuses a null pointer, and a text of data read back
// expects the word (the player's state saved in a cvar, which holds the name of a saber the player may not have)
template <typename T> constexpr decltype(auto) NullAsWord(T& arg)
{
    if constexpr (std::is_pointer_v<T> && std::is_same_v<std::remove_cv_t<std::remove_pointer_t<T>>, char>) {
        const char* text = arg;
        return text ? text : "(null)";
    }
    else {
        return (arg);
    }
}

template <typename... A> constexpr auto Pack(A&&... args)
{
    return fmt::make_format_args(args...);
}

} // namespace q_fmt_detail

// the arguments packed for a function taking fmt::format_args, as fmt::make_format_args does, a null C string written
// "(null)"
template <typename... T> constexpr auto Q_FormatArgs(T&... args)
{
    return q_fmt_detail::Pack(q_fmt_detail::NullAsWord(args)...);
}

// where the messages and the errors of an engine or a module go, defined by each of them
void Q_VPrint(printLevel_t level, lineEnd_t end, fmt::string_view fmt, fmt::format_args args);
NORETURN void Q_VError(errorParm_t code, fmt::string_view fmt, fmt::format_args args);

// a message written as it is: a part of a line, which the next message continues
template <typename... T> void Q_Print(fmt::format_string<T...> fmt, T&&... args)
{
    Q_VPrint(printLevel_t::PRINT_MESSAGE, lineEnd_t::NONE, fmt.get(), Q_FormatArgs(args...));
}

// a message, the line ended
template <typename... T> void Q_Println(fmt::format_string<T...> fmt, T&&... args)
{
    Q_VPrint(printLevel_t::PRINT_MESSAGE, lineEnd_t::NEWLINE, fmt.get(), Q_FormatArgs(args...));
}

// a developer's message, printed only with developer, the line ended
template <typename... T> void Q_DPrintln(fmt::format_string<T...> fmt, T&&... args)
{
    Q_VPrint(printLevel_t::PRINT_DEVELOPER, lineEnd_t::NEWLINE, fmt.get(), Q_FormatArgs(args...));
}

// a warning, the line ended
template <typename... T> void Q_PrintlnWarning(fmt::format_string<T...> fmt, T&&... args)
{
    Q_VPrint(printLevel_t::PRINT_WARNING, lineEnd_t::NEWLINE, fmt.get(), Q_FormatArgs(args...));
}

// an error that stops nothing, the line ended
template <typename... T> void Q_PrintlnError(fmt::format_string<T...> fmt, T&&... args)
{
    Q_VPrint(printLevel_t::PRINT_ERROR, lineEnd_t::NEWLINE, fmt.get(), Q_FormatArgs(args...));
}

// an error that stops: the game, the connection or the program, by its code
template <typename... T> NORETURN void Q_Error(errorParm_t code, fmt::format_string<T...> fmt, T&&... args)
{
    Q_VError(code, fmt.get(), Q_FormatArgs(args...));
}

// clang reports a pointer kept past the life of the object it points into
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(clang::lifetimebound)
#define Q_LIFETIMEBOUND [[clang::lifetimebound]]
#endif
#endif
#ifndef Q_LIFETIMEBOUND
#define Q_LIFETIMEBOUND
#endif

// a formatted text returned by value: up to 255 characters in the object itself, a longer text formatted a second time
// into an array on the heap. It lives as long as the expression that made it: a function that keeps the text keeps
// the object, not its c_str().
class formatText_t {
public:
    [[nodiscard]] const char* c_str() const noexcept Q_LIFETIMEBOUND { return heap_ ? heap_.get() : inline_; }
    [[nodiscard]] size_t size() const noexcept { return size_; }

private:
    friend formatText_t Q_VFormat(fmt::string_view fmt, fmt::format_args args);

    static constexpr size_t INLINE_SIZE = 256;
    char inline_[INLINE_SIZE]; // written before it is read: not initialised, for speed
    std::unique_ptr<char[]> heap_;
    size_t size_ = 0;
};

// a formatted text given to a printing or formatting function is written as its characters ({fmt} takes a format_as
// returning a number only); format is defined in q_fmt.cpp, which includes the formatting code
template <> struct fmt::formatter<formatText_t> : fmt::formatter<fmt::string_view> {
    fmt::format_context::iterator format(const formatText_t& text, fmt::format_context& ctx) const;
};

// the text formatted, returned by value
formatText_t Q_VFormat(fmt::string_view fmt, fmt::format_args args);
template <typename... T> [[nodiscard]] formatText_t Q_Format(fmt::format_string<T...> fmt, T&&... args)
{
    return Q_VFormat(fmt.get(), Q_FormatArgs(args...));
}

// the text formatted into dest, ended by a zero and cut to its size with a message; returns the text's full length
size_t Q_VFormatTo(std::span<char> dest, fmt::string_view fmt, fmt::format_args args);
template <typename... T> size_t Q_FormatTo(std::span<char> dest, fmt::format_string<T...> fmt, T&&... args)
{
    return Q_VFormatTo(dest, fmt.get(), Q_FormatArgs(args...));
}

// the same, cut to dest's size without a word: for the functions that format into their own buffer
size_t Q_VFormatToCut(std::span<char> dest, fmt::string_view fmt, fmt::format_args args);

// the text written into a file, as it is or with the line ended; false when not all of it was written
bool Q_VFilePrint(std::FILE* file, lineEnd_t end, fmt::string_view fmt, fmt::format_args args);
template <typename... T> bool Q_FilePrint(std::FILE* file, fmt::format_string<T...> fmt, T&&... args)
{
    return Q_VFilePrint(file, lineEnd_t::NONE, fmt.get(), Q_FormatArgs(args...));
}
template <typename... T> bool Q_FilePrintln(std::FILE* file, fmt::format_string<T...> fmt, T&&... args)
{
    return Q_VFilePrint(file, lineEnd_t::NEWLINE, fmt.get(), Q_FormatArgs(args...));
}

// a format written in printf's syntax (a translated text, a format of the game's data) rewritten into {fmt}'s, to be
// formatted with fmt::runtime: %s, %d and %i become {}, %% becomes %, a brace is doubled; any other conversion is kept
// as it is with a warning naming where the text came from. dest holds the result, ended by a zero, cut to its size.
void Q_FormatFromPrintf(std::span<char> dest, const char* printfFormat, const char* origin);

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

// The checks of the tests, without a framework: a check that fails prints its place and its expression, and the
// program's exit status is the count of failed checks (0: passed).
#pragma once

#include <cstdio>

namespace test {

inline int failures = 0;

inline void Fail(const char* file, int line, const char* expression)
{
    std::printf("%s:%d: check failed: %s\n", file, line, expression);
    ++failures;
}

// |a - b| at most fraction times |a| and times |b| (Boost.Test's CHECK_CLOSE_FRACTION)
template <typename T> bool CloseFraction(T a, T b, T fraction)
{
    const T difference = a > b ? a - b : b - a;
    const T absA = a < 0 ? -a : a;
    const T absB = b < 0 ? -b : b;
    return difference <= fraction * absA && difference <= fraction * absB;
}

} // namespace test

#define CHECK(condition)                                                                                                                                       \
    do {                                                                                                                                                       \
        if (!(condition))                                                                                                                                      \
            ::test::Fail(__FILE__, __LINE__, #condition);                                                                                                      \
    } while (false)
#define CHECK_EQUAL(a, b) CHECK((a) == (b))
#define CHECK_NE(a, b) CHECK((a) != (b))
#define CHECK_CLOSE_FRACTION(a, b, fraction) CHECK(::test::CloseFraction(a, b, fraction))
#define CHECK_THROW(expression, exception)                                                                                                                     \
    do {                                                                                                                                                       \
        bool thrown = false;                                                                                                                                   \
        try {                                                                                                                                                  \
            (void)(expression);                                                                                                                                \
        }                                                                                                                                                      \
        catch (const exception&) {                                                                                                                             \
            thrown = true;                                                                                                                                     \
        }                                                                                                                                                      \
        catch (...) {                                                                                                                                          \
        }                                                                                                                                                      \
        if (!thrown)                                                                                                                                           \
            ::test::Fail(__FILE__, __LINE__, #expression " throws " #exception);                                                                               \
    } while (false)

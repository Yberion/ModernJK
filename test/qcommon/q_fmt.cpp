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

// The tests of src/shared/qcommon/q_fmt.h and q_fmt.cpp: the rewriting of a printf format read at run time
// (Q_FormatFromPrintf: the text rewritten and whether it warned), a null C string written "(null)".
// Usage: modernjk_test_fmt; prints each failed check, exits with their count.
#include <shared/qcommon/q_fmt.h>

#include <check.h>

#include <cstdlib>
#include <cstring>

enum class errorParm_t { ERR_FATAL };

namespace {

int warnings = 0;

bool Rewrites(const char* printfFormat, const char* expected, bool warns)
{
    char text[256];
    warnings = 0;
    Q_FormatFromPrintf(text, printfFormat, "test");
    return std::strcmp(text, expected) == 0 && (warnings != 0) == warns;
}

} // namespace

// the messages of q_fmt.cpp: its warnings counted
void Q_VPrint(printLevel_t level, lineEnd_t, fmt::string_view, fmt::format_args)
{
    if (level == printLevel_t::PRINT_WARNING)
        ++warnings;
}

void Q_VError(errorParm_t, fmt::string_view, fmt::format_args)
{
    std::abort();
}

int main()
{
    // the conversions it knows, a doubled %, the braces doubled
    CHECK(Rewrites("%s and %d and %i", "{} and {} and {}", false));
    CHECK(Rewrites("100%%", "100%", false));
    CHECK(Rewrites("{x}", "{{x}}", false));
    // a % that starts no conversion is the character itself, with no warning
    CHECK(Rewrites("augmente de 400 %.", "augmente de 400 %.", false));
    CHECK(Rewrites("50% more", "50% more", false));
    CHECK(Rewrites("100 % sur", "100 % sur", false));
    CHECK(Rewrites("% s", "% s", false));
    CHECK(Rewrites("de 10 %-20 % ", "de 10 %-20 % ", false));
    CHECK(Rewrites("end %", "end %", false));
    // the other conversions kept as they are, with a warning
    CHECK(Rewrites("%.2f", "%.2f", true));
    CHECK(Rewrites("%5d", "%5d", true));
    CHECK(Rewrites("%-08.3lf", "%-08.3lf", true));
    CHECK(Rewrites("%c", "%c", true));
    CHECK(Rewrites("%x", "%x", true));
    CHECK(Rewrites("%*d", "%*d", true));
    CHECK(Rewrites("%ld", "%ld", true));
    // cut to the destination's size, ended by a zero
    char small[8];
    Q_FormatFromPrintf(small, "%s and more", "test");
    CHECK(std::strcmp(small, "{} and ") == 0);

    // a null C string written "(null)", as printf did, in a text and in a buffer
    const char* none = nullptr;
    char* noneMutable = nullptr;
    CHECK(std::strcmp(Q_Format("{} {} {}", none, noneMutable, "x").c_str(), "(null) (null) x") == 0);
    char buffer[32];
    Q_FormatTo(buffer, "<{}>", none);
    CHECK(std::strcmp(buffer, "<(null)>") == 0);

    fmt::println("{} failed check(s)", ::test::failures);
    return ::test::failures;
}

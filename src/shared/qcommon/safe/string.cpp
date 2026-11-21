/*
===========================================================================
Copyright (C) 2015 - 2023, OpenJK contributors
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

#include "string.h"
#include "sscanf.h"

#include <cctype>
#include <algorithm>

namespace Q {
Ordering stricmp(const gsl::cstring_span& lhs, const gsl::cstring_span& rhs) NOEXCEPT
{
    auto lIt = lhs.begin();
    auto rIt = rhs.begin();
    auto lEnd = lhs.end();
    auto rEnd = rhs.end();
    while (lIt != lEnd) {
        if (rIt == rEnd) {
            // rhs is prefix of lhs
            return Ordering::GT;
        }
        if (std::tolower(*lIt) < std::tolower(*rIt)) {
            return Ordering::LT;
        }
        if (std::tolower(*lIt) > std::tolower(*rIt)) {
            return Ordering::GT;
        }
        ++lIt;
        ++rIt;
    }
    if (rIt == rEnd) {
        // lhs == rhs
        return Ordering::EQ;
    }
    // lhs is a prefix of rhs
    return Ordering::LT;
}

gsl::cstring_span substr(const gsl::cstring_span& lhs, const std::string::size_type pos, const std::string::size_type count)
{
    if (pos > lhs.size()) {
        throw std::out_of_range("Q::substr called with out-of-bounds pos parameter!");
    }
    auto start = lhs.begin() + pos;
    auto end = count == std::string::npos ? lhs.end() : std::min(start + count, lhs.end());
    gsl::cstring_span result{start, end};
    return result;
}

int svtoi(const gsl::cstring_span& view)
{
    int result = 0;
    Q::sscanf(view, result);
    return result;
}

float svtof(const gsl::cstring_span& view)
{
    float result = 0.f;
    Q::sscanf(view, result);
    return result;
}
} // namespace Q

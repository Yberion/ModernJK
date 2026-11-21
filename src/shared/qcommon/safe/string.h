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

#pragma once

#include "sscanf.h"
#include "gsl.h"
#include "qcommon/q_platform.h"

#include <cassert>
#include <iostream>
#include <string>

namespace Q {
enum class Ordering : int { LT = -1, EQ = 0, GT = 1 };
Ordering stricmp(const gsl::cstring_span& lhs, const gsl::cstring_span& rhs) NOEXCEPT;
/// Case-insensitive less comparator for cstring_view; e.g. for case insensitive std::map
struct CStringViewILess {
    bool operator()(const gsl::cstring_span& lhs, const gsl::cstring_span& rhs) const NOEXCEPT { return stricmp(lhs, rhs) == Ordering::LT; }
};

gsl::cstring_span substr(const gsl::cstring_span& lhs, const std::string::size_type pos = 0, const std::string::size_type count = std::string::npos);

int svtoi(const gsl::cstring_span& view);
float svtof(const gsl::cstring_span& view);
} // namespace Q

// operator<< overloads
namespace std {
inline std::ostream& operator<<(std::ostream& stream, Q::Ordering ordering)
{
    switch (ordering) {
    case Q::Ordering::EQ:
        return stream << "EQ";
    case Q::Ordering::LT:
        return stream << "LT";
    case Q::Ordering::GT:
        return stream << "GT";
    default:
        assert(false);
        return stream;
    }
}
} // namespace std

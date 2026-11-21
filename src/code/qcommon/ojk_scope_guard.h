/*
===========================================================================
Copyright (C) 2016, OpenJK contributors
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

//
// Calls specified callbacks on scope enter (optional) and exit.
//

#ifndef OJK_SCOPE_GUARD_INCLUDED
#define OJK_SCOPE_GUARD_INCLUDED

#include <functional>
#include <utility>

namespace ojk {

class ScopeGuard {
public:
    using Callback = std::function<void()>;

    ScopeGuard(Callback leave_callback) : leave_callback_(leave_callback) {}

    ScopeGuard(Callback enter_callback, Callback leave_callback) : ScopeGuard(leave_callback) { enter_callback(); }

    ScopeGuard(const ScopeGuard& that) = delete;

    ScopeGuard& operator=(const ScopeGuard& that) = delete;

    ~ScopeGuard() { leave_callback_(); }

private:
    Callback leave_callback_;
}; // ScopeGuard

} // namespace ojk

#endif // OJK_SCOPE_GUARD_INCLUDED

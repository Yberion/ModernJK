#============================================================================
# Copyright (C) 2026, ModernJK contributors
#
# This file is part of the ModernJK source code.
#
# ModernJK is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
#============================================================================

# A compiler cache in front of the compilers, after cpp-best-practices/cmake_template (cmake/Cache.cmake). Off by
# default (modernjk_ENABLE_CACHE): no machine of the project has one installed.
function(modernjk_enable_cache)
	set(modernjk_CACHE_PROGRAM ccache CACHE STRING "Compiler cache: ccache or sccache")
	set_property(CACHE modernjk_CACHE_PROGRAM PROPERTY STRINGS ccache sccache)
	find_program(modernjk_CACHE_BINARY NAMES ${modernjk_CACHE_PROGRAM})
	if(modernjk_CACHE_BINARY)
		message(STATUS "Compiler cache: ${modernjk_CACHE_BINARY}")
		set(CMAKE_CXX_COMPILER_LAUNCHER "${modernjk_CACHE_BINARY}" CACHE FILEPATH "C++ compiler cache")
		set(CMAKE_C_COMPILER_LAUNCHER "${modernjk_CACHE_BINARY}" CACHE FILEPATH "C compiler cache")
	else()
		message(WARNING "${modernjk_CACHE_PROGRAM} requested but not found: no compiler cache")
	endif()
endfunction()

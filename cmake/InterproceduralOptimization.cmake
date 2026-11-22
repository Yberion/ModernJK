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

# Link-time optimization of every target configured after the call, after cpp-best-practices/cmake_template
# (cmake/InterproceduralOptimization.cmake), in the Release configuration only: never in a Debug build nor in the
# sanitizers' RelWithDebInfo. Refused where the toolchain cannot do it. CMake gives clang ThinLTO (-flto=thin), as
# fast as clang's full link-time optimization here and quicker to build.
macro(modernjk_enable_ipo)
	include(CheckIPOSupported)
	check_ipo_supported(RESULT modernjk_ipo_supported OUTPUT modernjk_ipo_output)
	if(modernjk_ipo_supported)
		set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
	else()
		message(SEND_ERROR "Link-time optimization is not supported: ${modernjk_ipo_output}")
	endif()
endmacro()

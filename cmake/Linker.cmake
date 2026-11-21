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

# The linker, after cpp-best-practices/cmake_template (cmake/Linker.cmake): the toolchain's own by default, or the one
# named in modernjk_LINKER (CMake's CMAKE_LINKER_TYPE: LLD, MOLD, GOLD, BFD, MSVC, ...), for every target.
set(modernjk_LINKER "" CACHE STRING "Linker of every target (CMAKE_LINKER_TYPE); empty: the toolchain's own")
if(modernjk_LINKER)
	set(CMAKE_LINKER_TYPE "${modernjk_LINKER}")
endif()

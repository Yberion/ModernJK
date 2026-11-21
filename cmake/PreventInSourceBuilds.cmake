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

# Refuses a build in the source tree: the build files would mix with the sources, and git would see them.
# After cpp-best-practices/cmake_template (cmake/PreventInSourceBuilds.cmake).
function(modernjk_assure_out_of_source_builds)
	# the real paths, so that a symbolic link cannot hide the source tree
	file(REAL_PATH "${CMAKE_SOURCE_DIR}" srcdir)
	file(REAL_PATH "${CMAKE_BINARY_DIR}" bindir)
	if(srcdir STREQUAL bindir)
		message(FATAL_ERROR "In-source builds are disabled: use a preset (cmake --preset <name>) or a build folder")
	endif()
endfunction()

modernjk_assure_out_of_source_builds()

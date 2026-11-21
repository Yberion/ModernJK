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

# Settings every build of the project gets, after cpp-best-practices/cmake_template (cmake/StandardProjectSettings.cmake).
# Adapted: Release is the default build type (the template's is RelWithDebInfo: -O2, and less inlining with MSVC); no
# colours are forced into the compilers' messages (the logs of the validations are read as plain text); no
# vcvarsall.bat run from CMake (MSVC is built from its own x64 environment, which the scripts load).

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "No build type given: Release")
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release)
endif()

# compile_commands.json in the build folder, for clang-tidy, cppcheck and the editors
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

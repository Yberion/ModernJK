#============================================================================
# Copyright (C) 2013 - 2018, OpenJK contributors
#
# This file is part of the OpenJK source code.
#
# OpenJK is free software; you can redistribute it and/or modify it
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
#
# The build, after cpp-best-practices/cmake_template: the options and the interface targets in ProjectOptions.cmake,
# the libraries of libs/ in Dependencies.cmake, one module per concern in cmake/, the presets in CMakePresets.json:
#
#   cmake --workflow --preset <preset>          configures, builds and tests
#   cmake --install build/<preset> --strip      installs into build/install/<preset>

cmake_minimum_required(VERSION 4.5.0)

# the standard's own flags (-std=c++23, not -std=gnu++23)
set(CMAKE_CXX_EXTENSIONS OFF)
# the project has no C++ module: no scan of every source for them
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
# MSVC, in the builds with debugging information: in each object (/Z7), gathered into one PDB per binary by the
# linker, rather than one PDB written by every compilation running in parallel
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)

project(ModernJK
	DESCRIPTION "Jedi Academy's and Jedi Outcast's engines and game code"
	LANGUAGES C CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(FATAL_ERROR "ModernJK builds only for 64-bit targets (with MSVC, from the x64 environment of Visual Studio)")
endif()
if(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
	message(FATAL_ERROR "ModernJK builds only for Windows and Linux, not ${CMAKE_SYSTEM_NAME}")
endif()

include(cmake/PreventInSourceBuilds.cmake)
# before the libraries of libs/, so that they get the build type and their compile commands too
include(cmake/StandardProjectSettings.cmake)
include(ProjectOptions.cmake)
modernjk_setup_options()
modernjk_global_options()
include(Dependencies.cmake)
modernjk_setup_dependencies()
modernjk_local_options()

# The processor in the binaries' names (jampgamex86_64.dll, jampgamearm64.so); q_platform.h defines the same string
if(WIN32 OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
	set(modernjk_ARCH x86_64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
	set(modernjk_ARCH arm64)
else()
	message(FATAL_ERROR "ModernJK builds only for x86_64 and arm64, not ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Every program and module of the project in the build folder's bin/, where the engines find the modules and the DLLs
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
# Every symbol hidden but those the code exports (the modules' entry points)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# The folders of the install (cmake --install): the game folders of Jedi Academy and Jedi Outcast
set(modernjk_INSTALL_JKA JediAcademy)
set(modernjk_INSTALL_JK2 JediOutcast)

if(BUILD_TESTING)
	enable_testing()
endif()

add_subdirectory(src)

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
if(modernjk_BUILD_ASSETS_PK3)
	add_subdirectory(assets)
endif()

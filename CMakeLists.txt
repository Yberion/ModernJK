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

# The <min>...<policy_max> syntax is needed to remain compatible with 3.1, but also newer versions that dropped 3.1 compatibility
# More info here: https://cmake.org/cmake/help/latest/command/cmake_minimum_required.html
cmake_minimum_required(VERSION 3.1...3.31)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9" CACHE STRING "Minimum OS X deployment version")

set(InModernJK ON)

set(ProjectName "ModernJK" CACHE STRING "Project Name")
project(${ProjectName})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# C++20 for every C++ target, without compiler extensions; with MSVC, /std:c++20 implies the strict standard mode
# (/permissive-)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# The project has no C++20 module: no dependency scan (MSBuild's scan files of two sources named files.cpp in one
# target collided, warning MSB8074)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Only Windows, Linux and macOS, in 64-bit (docs/modernization/PLAN.md, step 17)
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
	message(FATAL_ERROR "ModernJK builds only for 64-bit targets")
endif()
if(NOT WIN32 AND NOT APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
	message(FATAL_ERROR "ModernJK builds only for Windows, Linux and macOS, not ${CMAKE_SYSTEM_NAME}")
endif()

#=============================================================================
#
# Customizable options
#
#=============================================================================
option(BuildPortableVersion "Build portable version (does not read or write files from your user/home directory" OFF)

option(BuildMPEngine "Whether to create projects for the MP client (modernjk.exe)" ON)
option(BuildMPRdVanilla "Whether to create projects for the MP default renderer (rd-vanilla_x86.dll)" ON)
option(BuildMPDed "Whether to create projects for the MP dedicated server (modernjkded.exe)" ON)
option(BuildMPGame "Whether to create projects for the MP server-side gamecode (jampgamex86.dll)" ON)
option(BuildMPCGame "Whether to create projects for the MP clientside gamecode (cgamex86.dll)" ON)
option(BuildMPUI "Whether to create projects for the MP UI code (uix86.dll)" ON)

option(BuildSPEngine "Whether to create projects for the SP engine (modernjk_sp.exe)" ON)
option(BuildSPGame "Whether to create projects for the SP gamecode (jagamex86.dll)" ON)
option(BuildSPRdVanilla "Whether to create projects for the SP default renderer (rdsp-vanilla_x86.dll)" ON)

option(BuildJK2SPEngine "Whether to create projects for the jk2 SP engine (openjo_sp.exe)" OFF)
option(BuildJK2SPGame "Whether to create projects for the jk2 sp gamecode mod (jk2gamex86.dll)" OFF)
option(BuildJK2SPRdVanilla "Whether to create projects for the jk2 sp renderer (rdjosp-vanilla_x86.dll)" OFF)

option(BuildTests "Whether to build automatic unit tests (requires Boost)" OFF)
option(UseAddressSanitizer "Whether to enable runtime address sanitizer" OFF)
option(UseUndefinedSanitizer "Whether to enable runtime Undefined Behavior sanitizer" OFF)

include(CMakeDependentOption)
cmake_dependent_option(BuildSymbolServer "Build WIP Windows Symbol Server (experimental and unused)" OFF "NOT WIN32 OR NOT MSVC" OFF)

# Configure the use of bundled libraries.  By default, we assume the user is on
# a platform that does not require any bundling.

set(UseInternalSDL2Default   OFF)

if(UseInternalLibs OR WIN32)
  set(UseInternalSDL2Default   ON)
endif()

option(UseInternalSDL2   "If set, use bundled SDL2."    ${UseInternalSDL2Default})

# This option won't appear on non-Apple platforms.
if(APPLE)
  option(MakeApplicationBundles "Whether to build .app application bundles for engines built" ON)
endif()

#=============================================================================
#
# Custom CMake Modules needed
#
#=============================================================================
list(INSERT CMAKE_MODULE_PATH 0 "${CMAKE_SOURCE_DIR}/cmake/Modules")

if(CMAKE_EXPORT_COMPILE_COMMANDS)
	add_custom_target(
		copy-compile-commands ALL
		${CMAKE_COMMAND} -E copy_if_different
		${CMAKE_BINARY_DIR}/compile_commands.json
		${CMAKE_CURRENT_LIST_DIR})
endif()

#=============================================================================
#
# Architecture/OS defines
#
#=============================================================================
# ${Architecture} must match ARCH_STRING in q_platform.h,
# and is used in DLL names (jampgamex86_64.dll, jampgamearm64.dylib, jampgamex86_64.so).
# Windows: x86_64 only (q_platform.h refuses another processor).
if(WIN32)
	set(Architecture "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
	set(Architecture "arm64")
	add_definitions(-DPNG_ARM_NEON_OPT=0)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^x86.64$")
	set(Architecture "x86_64")
else()
	message(FATAL_ERROR "ModernJK builds only for x86_64 and arm64, not ${CMAKE_SYSTEM_PROCESSOR}")
endif()

message(STATUS "Architecture is ${Architecture}")

if(WIN32 AND CMAKE_VERSION VERSION_LESS "3.4")
	message(WARNING "Building on Windows platform with CMake version less than 3.4 is deprecated. Manifest file will fail to be included.")
endif()

#=============================================================================
#
# Binary names
#
#=============================================================================
set(SPEngine "modernjk_sp.${Architecture}")
set(SPGame "jagame${Architecture}")
set(SPRDVanillaRenderer "rdsp-vanilla_${Architecture}")
set(MPEngine "modernjk.${Architecture}")
set(MPVanillaRenderer "rd-vanilla_${Architecture}")
set(MPDed "modernjkded.${Architecture}")
set(MPGame "jampgame${Architecture}")
set(MPCGame "cgame${Architecture}")
set(MPUI "ui${Architecture}")
set(JK2SPEngine "openjo_sp.${Architecture}")
set(JK2SPGame "jospgame${Architecture}")
set(JK2SPVanillaRenderer "rdjosp-vanilla_${Architecture}")
set(AssetsPk3 "modernjk-${Architecture}.pk3")
# Library names
set(MPBotLib "botlib")
set(SharedLib "shared")

#=============================================================================
#
# Paths
#
#=============================================================================
set(SPDir "${CMAKE_SOURCE_DIR}/code")
set(MPDir "${CMAKE_SOURCE_DIR}/codemp")
set(JK2SPDir "${CMAKE_SOURCE_DIR}/codeJK2")
set(SharedDir ${CMAKE_SOURCE_DIR}/shared)
set(ModernJKLibDir "${CMAKE_SOURCE_DIR}/lib")
set(GSLIncludeDirectory "${ModernJKLibDir}/gsl-lite/include")

include(InstallConfig)

#=============================================================================
#
# Compiler definitions/flags
#
#=============================================================================

# Operating system settings
if (APPLE)
	set(SharedDefines ${SharedDefines} "MACOS_X")
endif()

if (NOT WIN32 AND NOT APPLE)
	set(SharedDefines ${SharedDefines} "ARCH_STRING=\"${Architecture}\"")
endif()

# Compiler settings
if(MSVC)
	set(SharedDefines ${SharedDefines} "NOMINMAX")
	set(SharedDefines ${SharedDefines} "_CRT_SECURE_NO_WARNINGS")
	set(SharedDefines ${SharedDefines} "_SCL_SECURE_NO_WARNINGS")
	set(SharedDefines ${SharedDefines} "_CRT_NONSTDC_NO_DEPRECATE")

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

	# We don't try to control symbol visibility under MSVC.
	set(MODERNJK_VISIBILITY_FLAGS "")
elseif (("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU") OR ("${CMAKE_C_COMPILER_ID}" MATCHES "Clang"))
	# I hope this doesn't come back to bite me in the butt later on.
	# Realistically though, can the C and CXX compilers be different?

	# Visibility can't be set project-wide -- it needs to be specified on a
	# per-target basis.  This is primarily due to the bundled copy of ZLib.
	# ZLib explicitly declares symbols hidden, rather than defaulting to hidden.
	#
	# Note that -fvisibility=hidden is stronger than -fvisibility-inlines-hidden.
	set(MODERNJK_VISIBILITY_FLAGS "-fvisibility=hidden")

	# removes the -rdynamic flag at linking (which causes crashes for some reason)
	set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")
	set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "")

	if(UseAddressSanitizer)
		# also raise stack size drastically (to 64MiB), since the sanitizer adds overhead to stack frames
		add_compile_options(-fsanitize=address)
		add_link_options(-fsanitize=address -z stack-size=4000000)
	endif()
	if(UseUndefinedSanitizer)
		add_compile_options(-fsanitize=undefined)
		add_link_options(-fsanitize=undefined)
	endif()

	# additional flags for debug configuration
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -ggdb")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb")

	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

	# the same floating-point results on every compiler and target: no fused multiply-add contracted from a separate
	# multiply and add (MSVC's /fp:precise does not contract); docs/modernization/surveys/deterministic-math.md
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffp-contract=off")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffp-contract=off")

	if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-comment")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsigned-char")

		if(WIN32)
			# Link libgcc and libstdc++ statically
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc")
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++")
		endif()
	elseif("${CMAKE_C_COMPILER_ID}" MATCHES "Clang")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-comment")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsigned-char")
	endif()

	if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-invalid-offsetof")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-comment")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsigned-char")
	elseif("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-comment")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-invalid-offsetof")
		# a class enum given to a printf-like function's %i or %d, which -Wformat accepts
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wformat-pedantic")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsigned-char")
	endif()
else()
	message(ERROR "Unsupported compiler")
endif()

set(SharedDefines ${SharedDefines} "$<$<CONFIG:Debug>:_DEBUG>")
set(SharedDefines ${SharedDefines} "$<$<NOT:$<CONFIG:Debug>>:FINAL_BUILD>")

if(BuildPortableVersion)
	set(BUILD_PORTABLE ON)
endif()

#=============================================================================
#
# Generate version file
#
#=============================================================================

# Reproducible builds
# https://reproducible-builds.org/specs/source-date-epoch/
if ("$ENV{SOURCE_DATE_EPOCH}" STREQUAL "")
	message(STATUS "SOURCE_DATE_EPOCH is not set: SOURCE_DATE will be set to the compile-time date")
else()
	execute_process(
		COMMAND "date" "--date=@$ENV{SOURCE_DATE_EPOCH}" "+%b %_d %Y"
		OUTPUT_VARIABLE SOURCE_DATE
		ERROR_QUIET
		OUTPUT_STRIP_TRAILING_WHITESPACE)
	message(STATUS "SOURCE_DATE_EPOCH is set ($ENV{SOURCE_DATE_EPOCH}): SOURCE_DATE set to \"${SOURCE_DATE}\"")
endif()

# Current Git tag/version
# ideally we would separate the hash suffix into its own variable, but sed is not available everywhere
execute_process(
	COMMAND "git" "describe" "--tag" "--always"
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	OUTPUT_VARIABLE GIT_TAG
	RESULT_VARIABLE GIT_TAG_ERROR
	OUTPUT_STRIP_TRAILING_WHITESPACE)
if(GIT_TAG_ERROR AND NOT GIT_TAG_ERROR EQUAL 0)
	message(WARNING "Unable to determine Git tag")
	set(GIT_TAG vUNKNOWN)
endif()
message(STATUS "Git tag is ${GIT_TAG}")

configure_file(shared/qcommon/q_version.h.in shared/qcommon/q_version.h @ONLY)

#=============================================================================

# Files shared across all projects
#
#=============================================================================
set(SharedCommonFiles
	"${SharedDir}/qcommon/q_color.h"
	"${SharedDir}/qcommon/q_color.cpp"
	"${SharedDir}/qcommon/q_math.h"
	"${SharedDir}/qcommon/q_math.cpp"
	"${SharedDir}/qcommon/q_rand.h"
	"${SharedDir}/qcommon/q_rand.cpp"
	"${SharedDir}/qcommon/q_string.h"
	"${SharedDir}/qcommon/q_string.cpp"
	"${SharedDir}/qcommon/q_platform.h"
	"${CMAKE_BINARY_DIR}/shared/qcommon/q_version.h"
	)
# The math functions whose results IEEE 754 does not fix (shared/qcommon/q_libm.h, included by q_math.h), the same
# bits on every platform: LLVM libc's correctly rounded functions (lib/llvm-libc, C++17 inside). Every binary built
# with SharedCommonFiles links it.
add_library(qlibm STATIC "${SharedDir}/qcommon/q_libm.h" "${SharedDir}/qcommon/q_libm.cpp")
target_include_directories(qlibm PRIVATE "${CMAKE_SOURCE_DIR}/lib/llvm-libc")
target_compile_definitions(qlibm PRIVATE ${SharedDefines})   # ARCH_STRING and the others, for q_platform.h
set_target_properties(qlibm PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden
	VISIBILITY_INLINES_HIDDEN ON)

set(SharedCommonSafeFiles
	"${SharedDir}/qcommon/safe/gsl.h"
	"${SharedDir}/qcommon/safe/string.cpp"
	"${SharedDir}/qcommon/safe/string.h"
	"${SharedDir}/qcommon/safe/sscanf.h"
	"${SharedDir}/qcommon/safe/limited_vector.h"
	)

#=============================================================================
#
# Adding all of the projects
#
#=============================================================================
# zlib-ng, vendored (lib/zlib-ng/README.md): it inflates the pk3 files, for shared/qcommon/q_pk3.cpp only, under
# its own names (zng_...). Built by its own CMake file on every platform; in a function, so that the settings of
# its build stay out of this project's.
function(modernjk_add_zlib_ng)
	set(BUILD_SHARED_LIBS OFF)
	set(BUILD_TESTING OFF)
	set(SKIP_INSTALL_ALL ON)
	set(WITH_GZFILEOP OFF)
	# its CMake file includes CPack as this project did already: its package description goes to its own folder,
	# not over this project's
	unset(CPack_CMake_INCLUDED)
	set(CPACK_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/lib/zlib-ng/CPackConfig.cmake")
	set(CPACK_SOURCE_OUTPUT_CONFIG_FILE "${CMAKE_BINARY_DIR}/lib/zlib-ng/CPackSourceConfig.cmake")
	add_subdirectory(lib/zlib-ng EXCLUDE_FROM_ALL)
	set_target_properties(zlib-ng PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()
modernjk_add_zlib_ng()

# Add projects
add_subdirectory(${SPDir})
if(BuildJK2SPGame)
	add_subdirectory("${JK2SPDir}/game")
endif()

add_subdirectory(${MPDir})

if(BuildSymbolServer)
	add_subdirectory("tools/WinSymbol")
endif()

if(BuildTests)
	enable_testing()
	add_subdirectory("tests")
endif()

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

# The libraries the project uses, in place of the template's CPM (which downloads them while configuring): each is
# vendored in libs/ at a version its README names, and built with the project. The one exception, until SDL 3 is
# vendored: SDL2 on Linux comes from the system (libsdl2-dev), as OpenGL does (the graphics driver's).
#
# Each is a target of its own, its include folders marked as system ones (outside the project's warnings).

function(modernjk_setup_dependencies)
	set(libs "${PROJECT_SOURCE_DIR}/libs")

	set(engines OFF)
	if(modernjk_BUILD_MP_CLIENT OR modernjk_BUILD_MP_SERVER OR modernjk_BUILD_SP OR modernjk_BUILD_JK2)
		set(engines ON)
	endif()

	# zlib-ng inflates the pk3 files (src/shared/qcommon/q_pk3.cpp), under its own names (zng_...), for the engines; its
	# own CMake file, its settings kept in this function's scope
	if(engines)
		set(BUILD_SHARED_LIBS OFF)
		set(BUILD_TESTING OFF)
		set(SKIP_INSTALL_ALL ON)
		set(WITH_GZFILEOP OFF)
		# its CMake file includes CPack: its package description stays in its own build folder
		set(CPACK_OUTPUT_CONFIG_FILE "${PROJECT_BINARY_DIR}/libs/zlib-ng/CPackConfig.cmake")
		set(CPACK_SOURCE_OUTPUT_CONFIG_FILE "${PROJECT_BINARY_DIR}/libs/zlib-ng/CPackSourceConfig.cmake")
		add_subdirectory("${libs}/zlib-ng" "${PROJECT_BINARY_DIR}/libs/zlib-ng" EXCLUDE_FROM_ALL SYSTEM)
		set_target_properties(zlib-ng PROPERTIES POSITION_INDEPENDENT_CODE ON)
	endif()

	# LLVM libc's correctly rounded math functions and its text parser, header-only
	add_library(vendored_llvm_libc INTERFACE)
	target_include_directories(vendored_llvm_libc SYSTEM INTERFACE "${libs}/llvm-libc")
	add_library(vendored::llvm_libc ALIAS vendored_llvm_libc)

	# gsl-lite, header-only (the single player's code and src/shared/qcommon/safe)
	add_library(vendored_gsl_lite INTERFACE)
	target_include_directories(vendored_gsl_lite SYSTEM INTERFACE "${libs}/gsl-lite/include")
	add_library(vendored::gsl_lite ALIAS vendored_gsl_lite)

	# Wuffs (JPEG, PNG and TGA decoding) and stb_image_write (writing), included by the renderers' image code
	add_library(vendored_wuffs INTERFACE)
	target_include_directories(vendored_wuffs SYSTEM INTERFACE "${libs}/wuffs")
	add_library(vendored::wuffs ALIAS vendored_wuffs)
	add_library(vendored_stb INTERFACE)
	target_include_directories(vendored_stb SYSTEM INTERFACE "${libs}/stb")
	add_library(vendored::stb ALIAS vendored_stb)

	# {fmt}, the formatting behind src/shared/qcommon/q_fmt.h and netcodec's tools, as a static library. Without the
	# locale (the L specifier), which nothing uses; FMT_UNICODE 0 since MSVC does not compile the project with /utf-8,
	# which {fmt} asserts otherwise (in this release FMT_UNICODE changes nothing else). Every target that includes its
	# headers gets the same definitions, so that the arguments a module packs are read the same way by the engine.
	add_library(vendored_fmt STATIC "${libs}/fmt/src/format.cc")
	target_include_directories(vendored_fmt SYSTEM PUBLIC "${libs}/fmt/include")
	target_compile_definitions(vendored_fmt PUBLIC FMT_USE_LOCALE=0 FMT_UNICODE=0)
	target_compile_features(vendored_fmt PRIVATE cxx_std_23)
	set_target_properties(vendored_fmt PROPERTIES POSITION_INDEPENDENT_CODE ON)
	add_library(vendored::fmt ALIAS vendored_fmt)

	# the engines that open a window
	if(modernjk_BUILD_MP_CLIENT OR modernjk_BUILD_SP OR modernjk_BUILD_JK2)
		find_package(OpenGL REQUIRED)
		if(WIN32)
			# SDL2 for Windows, built by its authors (libs/SDL2): the DLL beside the engines, SDL2main for WinMain
			add_library(SDL2::SDL2 SHARED IMPORTED GLOBAL)
			set_target_properties(SDL2::SDL2 PROPERTIES
				IMPORTED_LOCATION "${libs}/SDL2/bin/x64/SDL2.dll"
				IMPORTED_IMPLIB "${libs}/SDL2/lib/x64/SDL2.lib"
				INTERFACE_INCLUDE_DIRECTORIES "${libs}/SDL2/include")
			add_library(SDL2::SDL2main STATIC IMPORTED GLOBAL)
			set_target_properties(SDL2::SDL2main PROPERTIES IMPORTED_LOCATION "${libs}/SDL2/lib/x64/SDL2main.lib")
		else()
			find_package(SDL2 REQUIRED CONFIG)
			if(NOT TARGET SDL2::SDL2)   # a configuration file that gives only variables
				add_library(SDL2::SDL2 INTERFACE IMPORTED GLOBAL)
				string(STRIP "${SDL2_LIBRARIES}" sdl2_libraries)
				set_target_properties(SDL2::SDL2 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIRS}"
					INTERFACE_LINK_LIBRARIES "${sdl2_libraries}")
			endif()
		endif()
	endif()
endfunction()

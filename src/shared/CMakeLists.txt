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

# The code the engines and the modules of every tree share. The sets of sources are interface targets: a target that
# links one compiles its files with its own settings (each module and each engine has its own definitions).

# The version header: the git tag, the date of the sources when SOURCE_DATE_EPOCH gives it (a reproducible build;
# otherwise the compiler's __DATE__), and whether the engines are portable
execute_process(COMMAND git describe --tag --always
	WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
	OUTPUT_VARIABLE GIT_TAG
	RESULT_VARIABLE git_result
	OUTPUT_STRIP_TRAILING_WHITESPACE
	ERROR_QUIET)
if(NOT git_result EQUAL 0)
	# a machine without git builds too: information, not a defect
	message(STATUS "No git tag found: the version is vUNKNOWN")
	set(GIT_TAG vUNKNOWN)
endif()
if(DEFINED ENV{SOURCE_DATE_EPOCH})
	string(TIMESTAMP SOURCE_DATE "%b %d %Y" UTC)
endif()
set(BUILD_PORTABLE ${modernjk_PORTABLE})
configure_file(qcommon/q_version.h.in "${PROJECT_BINARY_DIR}/generated/qcommon/q_version.h" @ONLY)
add_library(modernjk_version INTERFACE)
target_include_directories(modernjk_version INTERFACE "${PROJECT_BINARY_DIR}/generated")

# The definitions every engine and module is built with
add_library(modernjk_defines INTERFACE)
target_compile_definitions(modernjk_defines INTERFACE $<IF:$<CONFIG:Debug>,_DEBUG,FINAL_BUILD>)
if(NOT WIN32)
	# the processor's name in the binaries' names; q_platform.h has Windows's
	target_compile_definitions(modernjk_defines INTERFACE "ARCH_STRING=\"${modernjk_ARCH}\"")
endif()
if(MSVC)
	# windows.h's min and max macros off; the C library's functions MSVC deprecates (strcpy, fopen, ...) allowed
	target_compile_definitions(modernjk_defines INTERFACE NOMINMAX _CRT_SECURE_NO_WARNINGS _SCL_SECURE_NO_WARNINGS
		_CRT_NONSTDC_NO_DEPRECATE)
endif()

# The math functions whose results IEEE 754 does not fix (qcommon/q_libm.h), the same bits on every platform: LLVM
# libc's correctly rounded functions
add_library(qlibm STATIC qcommon/q_libm.cpp)
target_link_libraries(qlibm PRIVATE modernjk_options modernjk_warnings modernjk_defines vendored::llvm_libc)
set_target_properties(qlibm PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Colours, printing and formatting, vectors, random numbers and strings: in every engine and module
add_library(modernjk_shared_common INTERFACE)
target_sources(modernjk_shared_common INTERFACE qcommon/q_color.cpp qcommon/q_fmt.cpp qcommon/q_math.cpp
	qcommon/q_rand.cpp qcommon/q_string.cpp)
target_link_libraries(modernjk_shared_common INTERFACE qlibm vendored::fmt)

# The file system and its pk3 reader: in every engine
if(TARGET zlib-ng::zlib)
	add_library(modernjk_filesystem INTERFACE)
	target_sources(modernjk_filesystem INTERFACE qcommon/files.cpp qcommon/q_pk3.cpp)
	target_link_libraries(modernjk_filesystem INTERFACE zlib-ng::zlib)
endif()

# Checked strings and files (the single player's)
add_library(modernjk_safe_string INTERFACE)
target_sources(modernjk_safe_string INTERFACE qcommon/safe/string.cpp)
target_link_libraries(modernjk_safe_string INTERFACE vendored::gsl_lite)
add_library(modernjk_safe_files INTERFACE)
target_sources(modernjk_safe_files INTERFACE qcommon/safe/files.cpp)
target_link_libraries(modernjk_safe_files INTERFACE vendored::gsl_lite)

# The renderers' image reading and writing, and their noise
add_library(modernjk_rd_common INTERFACE)
target_sources(modernjk_rd_common INTERFACE rd-common/tr_image_codec.cpp rd-common/tr_image_load.cpp
	rd-common/tr_noise.cpp)
target_link_libraries(modernjk_rd_common INTERFACE vendored::wuffs vendored::stb)

# The sounds a cgame plays, the same in the three games: compiled by each cgame against its own headers
# (<cgame/cg_local.h>), the tree's own part in its cgame/cg_sound.cpp
add_library(modernjk_cgame_sound INTERFACE)
target_sources(modernjk_cgame_sound INTERFACE cgame/cg_sound.cpp)

# The sound of the engines that play it, and its decoders' fuzzers
if(TARGET vendored::flac)
	add_subdirectory(sound)
endif()

# The system layer of the engines that open a window (on SDL2), and of the dedicated server
if(modernjk_BUILD_MP_CLIENT OR modernjk_BUILD_SP OR modernjk_BUILD_JK2)
	add_library(modernjk_sys_client INTERFACE)
	target_sources(modernjk_sys_client INTERFACE sys/sys_main.cpp sys/sys_event.cpp sys/con_log.cpp sdl/sdl_window.cpp
		sdl/sdl_input.cpp)
	if(WIN32)
		target_sources(modernjk_sys_client INTERFACE sys/sys_win32.cpp sys/con_passive.cpp sys/win_manifest.manifest)
		target_link_libraries(modernjk_sys_client INTERFACE SDL2::SDL2main SDL2::SDL2 winmm)
	else()
		target_sources(modernjk_sys_client INTERFACE sys/sys_unix.cpp sys/con_tty.cpp)
		target_link_libraries(modernjk_sys_client INTERFACE SDL2::SDL2 OpenGL::GL ${CMAKE_DL_LIBS})
	endif()
endif()
add_library(modernjk_sys_dedicated INTERFACE)
target_sources(modernjk_sys_dedicated INTERFACE sys/sys_main.cpp sys/sys_event.cpp sys/con_log.cpp)
if(WIN32)
	target_sources(modernjk_sys_dedicated INTERFACE sys/sys_win32.cpp sys/con_win32.cpp)
	target_link_libraries(modernjk_sys_dedicated INTERFACE winmm)
else()
	target_sources(modernjk_sys_dedicated INTERFACE sys/sys_unix.cpp sys/con_tty.cpp)
	target_link_libraries(modernjk_sys_dedicated INTERFACE ${CMAKE_DL_LIBS})
endif()

# An engine's DLLs (SDL2's on Windows) copied beside it after each link, so that it runs from the build folder
function(modernjk_copy_runtime_dlls target)
	if(WIN32)
		add_custom_command(TARGET ${target} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_RUNTIME_DLLS:${target}> $<TARGET_FILE_DIR:${target}>
			COMMAND_EXPAND_LISTS)
	endif()
endfunction()

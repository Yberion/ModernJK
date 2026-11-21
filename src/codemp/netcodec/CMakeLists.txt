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

# netcodec: the snapshot and message codec of the multiplayer, a library with no dependency on the engine (the engines
# include netcodec/engine.h), with its tests, benchmarks, tools and fuzzers.
#
# Layout after cpp-best-practices/cmake_template: include/ (the public headers), src/, test/, bench/, tools/, fuzz/.
# Built by the project's presets: the library with the engines, the rest with the tests' and benchmarks' options.

# The settings of every netcodec target, on top of the project's
add_library(netcodec_options INTERFACE)
target_link_libraries(netcodec_options INTERFACE modernjk_options vendored::llvm_libc)
# the index checks of std::span and of the containers in Debug (libstdc++, libc++; MSVC checks by default in Debug)
target_compile_definitions(netcodec_options INTERFACE
	$<$<CONFIG:Debug>:_GLIBCXX_ASSERTIONS _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG>)
if(NOT MSVC)
	# functions and loops on 64-byte boundaries: otherwise an unrelated change that moves the code by a few bytes moves
	# the hot loops across fetch blocks and changes the encoding time by up to 5 % (clang) or 10 to 20 % (GCC)
	target_compile_options(netcodec_options INTERFACE -falign-functions=64 -falign-loops=64)
endif()
if(modernjk_BUILD_FUZZ_TESTS)
	# every netcodec target instrumented for the fuzzers' coverage
	if(MINGW)   # llvm-mingw ships libFuzzer but its driver refuses -fsanitize=fuzzer: the coverage it implies
		target_compile_options(netcodec_options INTERFACE -fsanitize-coverage=inline-8bit-counters,pc-table,trace-cmp)
	else()
		target_compile_options(netcodec_options INTERFACE -fsanitize=fuzzer-no-link)
	endif()
endif()

add_library(netcodec STATIC
	src/client_codec.cpp
	src/engine.cpp
	src/message_codec.cpp
	src/model.cpp)
add_library(netcodec::netcodec ALIAS netcodec)
target_include_directories(netcodec PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(netcodec PRIVATE netcodec_options netcodec_warnings)
modernjk_clang_tidy_as_errors(netcodec)

# The library's internal headers, for the tests and tools that look inside it; the trace readers of test/, for the
# benchmarks and tools
add_library(netcodec_internal INTERFACE)
target_include_directories(netcodec_internal INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
add_library(netcodec_traces INTERFACE)
target_include_directories(netcodec_traces INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/test")

# The programs of the tests, benchmarks and tools: the library's settings; the static runtime with MinGW (no DLL to
# find)
function(netcodec_add_program name)
	add_executable(${name} ${ARGN})
	target_link_libraries(${name} PRIVATE netcodec netcodec_options netcodec_warnings)
	if(MSVC)
		target_compile_definitions(${name} PRIVATE _CRT_SECURE_NO_WARNINGS)   # fopen
	endif()
	if(MINGW)
		target_link_options(${name} PRIVATE -static)
	endif()
	if(MSVC AND modernjk_ENABLE_SANITIZER_ADDRESS)
		# MSVC's AddressSanitizer runtime is a DLL of the compiler's folder, not in the PATH: copied beside the program
		cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH msvc_bin)
		add_custom_command(TARGET ${name} POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"${msvc_bin}/clang_rt.asan_dynamic-x86_64.dll" "$<TARGET_FILE_DIR:${name}>")
	endif()
endfunction()

# The traces the tests, benchmarks and tools read: made from recordings, not committed
# (docs/modernization/tools/python/export_demo_traces.py and the others)
set(netcodec_TRACE_DIR "${PROJECT_SOURCE_DIR}/docs/modernization/data/traces" CACHE PATH "Folder of netcodec's traces")
# The tree whose src/codemp/qcommon/msg.cpp and huffman.cpp the legacy benchmark and the movement test build: this one,
# or another for the A/B scripts' copies
set(netcodec_ENGINE_DIR "${PROJECT_SOURCE_DIR}" CACHE PATH "Tree of the engine code netcodec's benchmarks and tests build")

if(BUILD_TESTING)
	add_subdirectory(test)
endif()
if(modernjk_BUILD_BENCHMARKS)
	add_subdirectory(bench)
endif()
if(modernjk_BUILD_TOOLS)
	add_subdirectory(tools)
endif()
if(modernjk_BUILD_FUZZ_TESTS)
	add_subdirectory(fuzz)
endif()

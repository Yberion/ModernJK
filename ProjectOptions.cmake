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

# The project's options and the interface targets that carry them, after cpp-best-practices/cmake_template
# (ProjectOptions.cmake): every target of the project links modernjk_options (the language, the floating-point
# determinism, the sanitizers, the hardening) and one warnings target (modernjk_warnings, or netcodec_warnings for
# netcodec's targets), privately.

include(CMakeDependentOption)

macro(modernjk_setup_options)
	# What is built: every target by default
	option(modernjk_BUILD_MP_CLIENT "The multiplayer client, its renderer, cgame and ui modules" ON)
	option(modernjk_BUILD_MP_SERVER "The multiplayer dedicated server" ON)
	option(modernjk_BUILD_MP_GAME "The multiplayer game module (the servers' and the client's)" ON)
	option(modernjk_BUILD_SP "Jedi Academy's single player: engine, game module, renderer" ON)
	option(modernjk_BUILD_JK2 "Jedi Outcast's single player: engine, game module, renderer" ON)
	option(modernjk_BUILD_ASSETS_PK3 "modernjk.pk3, from the files of assets/" ON)
	option(BUILD_TESTING "The tests (CTest): netcodec's and test/" ON)
	option(modernjk_BUILD_BENCHMARKS "netcodec's benchmarks" ON)
	option(modernjk_BUILD_TOOLS "The tools: netcodec's table trainers, the zone files' converter (tools/eal_converter, built on demand)" ON)
	option(modernjk_BUILD_FUZZ_TESTS "The libFuzzer targets: netcodec's, the sound decoders' (clang, with the sanitizers)" OFF)
	option(modernjk_PORTABLE "The engines read and write no file in the user's folders, only in the game's" OFF)

	# How it is built
	# on by default, as the template's for a project built on its own: a warning cannot come back; off by hand for a
	# compiler the project was not tested with
	option(modernjk_WARNINGS_AS_ERRORS "The project's warnings as errors (netcodec's always are)" ON)
	option(modernjk_ENABLE_SANITIZER_ADDRESS "AddressSanitizer" OFF)
	option(modernjk_ENABLE_SANITIZER_UNDEFINED "UndefinedBehaviorSanitizer (not with MSVC)" OFF)
	option(modernjk_ENABLE_CLANG_TIDY "clang-tidy on every C++ file while it compiles (reporting)" OFF)
	option(modernjk_ENABLE_CPPCHECK "cppcheck on every C++ file while it compiles (reporting)" OFF)
	option(modernjk_ENABLE_IPO "Link-time optimization (Release only)" OFF)
	option(modernjk_ENABLE_HARDENING "Hardening of the generated code (cmake/Hardening.cmake)" OFF)
	option(modernjk_ENABLE_CACHE "A compiler cache (ccache or sccache)" OFF)

	if(modernjk_BUILD_FUZZ_TESTS AND NOT MINGW)
		include(cmake/LibFuzzer.cmake)
		modernjk_check_libfuzzer_support(modernjk_libfuzzer_supported)
		if(NOT modernjk_libfuzzer_supported)
			message(SEND_ERROR "modernjk_BUILD_FUZZ_TESTS: this compiler does not link libFuzzer (-fsanitize=fuzzer)")
		endif()
	endif()
endmacro()

# What applies to every target configured afterwards, the libraries of libs/ included
macro(modernjk_global_options)
	if(modernjk_ENABLE_IPO)
		include(cmake/InterproceduralOptimization.cmake)
		modernjk_enable_ipo()
	endif()
	include(cmake/Linker.cmake)
	if(modernjk_ENABLE_CACHE)
		include(cmake/Cache.cmake)
		modernjk_enable_cache()
	endif()
endmacro()

# What applies to the project's own targets
macro(modernjk_local_options)
	include(cmake/CompilerWarnings.cmake)
	add_library(modernjk_warnings INTERFACE)
	modernjk_set_project_warnings(modernjk_warnings ${modernjk_WARNINGS_AS_ERRORS})
	add_library(netcodec_warnings INTERFACE)
	modernjk_set_netcodec_warnings(netcodec_warnings)

	add_library(modernjk_options INTERFACE)
	target_compile_features(modernjk_options INTERFACE cxx_std_23)
	# No debugging information in Release: with it, clang 23 generates different instructions in some files (an
	# address computed by a shift where it used lea)
	if(MSVC)
		# no fused multiply-add contracted from a separate multiply and add (MSVC's default, said explicitly)
		target_compile_options(modernjk_options INTERFACE /fp:precise)
	else()
		# the same floating-point results on every compiler and target: no fused multiply-add contracted from a
		# separate multiply and add
		target_compile_options(modernjk_options INTERFACE -ffp-contract=off)
		# char is signed, as the code assumes, on arm64 too (where it is unsigned by default)
		target_compile_options(modernjk_options INTERFACE -fsigned-char)
	endif()

	include(cmake/Sanitizers.cmake)
	modernjk_enable_sanitizers(modernjk_options ${modernjk_ENABLE_SANITIZER_ADDRESS} ${modernjk_ENABLE_SANITIZER_UNDEFINED})

	if(modernjk_ENABLE_HARDENING)
		include(cmake/Hardening.cmake)
		modernjk_enable_hardening(modernjk_options)
	endif()

	include(cmake/StaticAnalyzers.cmake)
	if(modernjk_ENABLE_CLANG_TIDY)
		modernjk_enable_clang_tidy()
	endif()
	if(modernjk_ENABLE_CPPCHECK)
		modernjk_enable_cppcheck()
	endif()
endmacro()

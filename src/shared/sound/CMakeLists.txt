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

# The sound of every engine that plays it: one static library, built with the strict warnings netcodec has, its
# headers included from src/ (<shared/sound/decoder.h>)
add_library(modernjk_sound STATIC decoder.cpp flac.cpp mp3.cpp vorbis.cpp wav.cpp)
target_include_directories(modernjk_sound PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(modernjk_sound
	PRIVATE modernjk_options netcodec_warnings vendored::dr_libs vendored::flac vendored::stb_vorbis)
# clean under its own clang-tidy checks (.clang-tidy here), so their findings are errors in the clang-tidy preset
modernjk_clang_tidy_as_errors(modernjk_sound)

# The libFuzzer target on the decoders (clang only): the module and the decoders' libraries instrumented for its
# coverage, libFuzzer giving main(); llvm-mingw's driver refuses -fsanitize=fuzzer, so there the coverage it implies and
# libFuzzer's archive from the compiler's resource folder, as netcodec's fuzzers do
if(modernjk_BUILD_FUZZ_TESTS)
	add_executable(sound_fuzz_decode fuzz/fuzz_decode.cpp)
	target_link_libraries(sound_fuzz_decode PRIVATE modernjk_sound modernjk_options netcodec_warnings)
	foreach(target modernjk_sound FLAC vendored_stb_vorbis sound_fuzz_decode)
		if(MINGW)
			target_compile_options(${target} PRIVATE -fsanitize-coverage=inline-8bit-counters,pc-table,trace-cmp)
		else()
			target_compile_options(${target} PRIVATE -fsanitize=fuzzer-no-link)
		endif()
	endforeach()
	if(MINGW)
		execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -print-resource-dir OUTPUT_VARIABLE clang_resources
			OUTPUT_STRIP_TRAILING_WHITESPACE)
		target_link_libraries(sound_fuzz_decode PRIVATE "${clang_resources}/lib/windows/libclang_rt.fuzzer-x86_64.a")
	else()
		target_link_options(sound_fuzz_decode PRIVATE -fsanitize=fuzzer)
	endif()
endif()

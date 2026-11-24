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

# The sound's decoders (decoder/): a static library of their own, which the fuzzer takes without OpenAL Soft, built with
# the strict warnings netcodec has, its headers included from src/ (<shared/sound/decoder/decoder.h>)
add_library(modernjk_sound_decoder STATIC decoder/decoder.cpp decoder/flac.cpp decoder/mp3.cpp decoder/vorbis.cpp decoder/wav.cpp)
target_include_directories(modernjk_sound_decoder PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(modernjk_sound_decoder
	PRIVATE modernjk_options netcodec_warnings vendored::dr_libs vendored::flac vendored::stb_vorbis)
# clean under its own clang-tidy checks (.clang-tidy here), so their findings are errors in the clang-tidy preset
modernjk_clang_tidy_as_errors(modernjk_sound_decoder)

# The reverb's zones (zones/): the zone files' reader and writer and the zones' calculations, a static library of its own
# without OpenAL Soft, which the fuzzer, the tests and the zone files' converter (tools/eal_converter) take too
add_library(modernjk_sound_zones STATIC zones/zones.cpp)
target_include_directories(modernjk_sound_zones PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(modernjk_sound_zones PUBLIC vendored::fmt PRIVATE modernjk_options netcodec_warnings vendored::llvm_libc)
modernjk_clang_tidy_as_errors(modernjk_sound_zones)

# The sound of every engine that plays it, on OpenAL Soft: one static library independent of the engines, which give
# it their files and take its messages (host.h); each engine compiles engine/ itself, against its own headers
if(TARGET vendored::openal)
	add_library(modernjk_sound STATIC names.cpp system.cpp ambient/ambient.cpp ambient/sets.cpp bank/bank.cpp bank/lipsync.cpp device/device.cpp stream/dms.cpp stream/music.cpp stream/stream.cpp
		voices/voices.cpp zones/reverb.cpp)
	target_include_directories(modernjk_sound PUBLIC "${PROJECT_SOURCE_DIR}/src")
	target_link_libraries(modernjk_sound
		PUBLIC modernjk_sound_decoder modernjk_sound_zones vendored::openal vendored::fmt
		PRIVATE modernjk_options netcodec_warnings vendored::llvm_libc)
	modernjk_clang_tidy_as_errors(modernjk_sound)

	# What each engine compiles itself against its own headers (<qcommon/qcommon.h>): the settings, the commands, the
	# host over its file system and console, the Api of its modules
	add_library(modernjk_sound_engine INTERFACE)
	target_sources(modernjk_sound_engine INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/engine/engine.cpp")
	target_link_libraries(modernjk_sound_engine INTERFACE modernjk_sound)
endif()

# The sound's libFuzzer targets (clang only): the module and the decoders' libraries instrumented for their coverage,
# libFuzzer giving main(); llvm-mingw's driver refuses -fsanitize=fuzzer, so there the coverage it implies and
# libFuzzer's archive from the compiler's resource folder, as netcodec's fuzzers do
if(modernjk_BUILD_FUZZ_TESTS)
	# the decoders on any file; the readers of the game's two text files; the whole sound driven by a sequence of
	# calls, on OpenAL Soft's loopback device
	add_executable(sound_fuzz_decode fuzz/fuzz_decode.cpp)
	target_link_libraries(sound_fuzz_decode PRIVATE modernjk_sound_decoder modernjk_options netcodec_warnings)
	add_executable(sound_fuzz_text fuzz/fuzz_text.cpp)
	target_link_libraries(sound_fuzz_text PRIVATE modernjk_sound modernjk_options netcodec_warnings)
	add_executable(sound_fuzz_system fuzz/fuzz_system.cpp)
	target_link_libraries(sound_fuzz_system PRIVATE modernjk_sound modernjk_options netcodec_warnings)
	set(fuzzers sound_fuzz_decode sound_fuzz_text sound_fuzz_system)
	foreach(target modernjk_sound_decoder modernjk_sound_zones modernjk_sound FLAC vendored_stb_vorbis ${fuzzers})
		if(MINGW)
			target_compile_options(${target} PRIVATE -fsanitize-coverage=inline-8bit-counters,pc-table,trace-cmp)
		else()
			target_compile_options(${target} PRIVATE -fsanitize=fuzzer-no-link)
		endif()
	endforeach()
	if(MINGW)
		execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -print-resource-dir OUTPUT_VARIABLE clang_resources
			OUTPUT_STRIP_TRAILING_WHITESPACE)
	endif()
	foreach(target ${fuzzers})
		if(MINGW)
			target_link_libraries(${target} PRIVATE "${clang_resources}/lib/windows/libclang_rt.fuzzer-x86_64.a")
		else()
			target_link_options(${target} PRIVATE -fsanitize=fuzzer)
		endif()
	endforeach()
endif()

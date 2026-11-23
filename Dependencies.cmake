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
# vendored: SDL2 on Linux comes from the system (libsdl2-dev), as OpenGL does (the graphics driver's). The system's
# drivers are not libraries of the project: on Linux the headers of OpenGL and of the sound servers that OpenAL Soft's
# backends need (PipeWire, PulseAudio, ALSA, and D-Bus for RTKit) come from the distribution's development packages, the
# libraries themselves being the player's own.
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

	# The decoders of the sound (src/shared/sound), for the engines that play it and for the fuzzers
	if(modernjk_BUILD_MP_CLIENT OR modernjk_BUILD_SP OR modernjk_BUILD_JK2 OR modernjk_BUILD_FUZZ_TESTS)
		# libFLAC, by its own CMake file: the library alone, without Ogg FLAC; its encoder is compiled with it and left
		# out at the link
		foreach(off BUILD_CXXLIBS BUILD_PROGRAMS BUILD_EXAMPLES BUILD_TESTING BUILD_DOCS INSTALL_MANPAGES
				INSTALL_PKGCONFIG_MODULES INSTALL_CMAKE_CONFIG_MODULE WITH_OGG ENABLE_MULTITHREADING WITH_FORTIFY_SOURCE
				WITH_STACK_PROTECTOR)
			set(${off} OFF)
		endforeach()
		add_subdirectory("${libs}/flac" "${PROJECT_BINARY_DIR}/libs/flac" EXCLUDE_FROM_ALL SYSTEM)
		add_library(vendored::flac ALIAS FLAC)
		# its CMake file defines FLAC__OVERFLOW_DETECT in Debug and RelWithDebInfo, which writes a line on the standard
		# error at every overflow a damaged file causes (hundreds of thousands for one file): undefined again, the
		# definitions coming before the options on the command line
		target_compile_options(FLAC PRIVATE $<IF:$<C_COMPILER_ID:MSVC>,/UFLAC__OVERFLOW_DETECT,-UFLAC__OVERFLOW_DETECT>)
		# its CMake file also turns on -Winline for its developers: GCC's reports of the inlining it declined, not
		# defects, which link-time optimization repeats at the link, where the project's -Werror applies
		if(NOT MSVC)
			target_compile_options(FLAC PRIVATE -Wno-inline)
		endif()

		# dr_mp3 and dr_wav, single headers whose implementation the sound's decoders compile; stb_vorbis, one C file
		add_library(vendored_dr_libs INTERFACE)
		target_include_directories(vendored_dr_libs SYSTEM INTERFACE "${libs}/dr_libs")
		add_library(vendored::dr_libs ALIAS vendored_dr_libs)
		add_library(vendored_stb_vorbis STATIC "${libs}/stb/stb_vorbis.c")
		target_compile_definitions(vendored_stb_vorbis PUBLIC STB_VORBIS_NO_STDIO STB_VORBIS_NO_PUSHDATA_API)
		target_include_directories(vendored_stb_vorbis SYSTEM INTERFACE "${libs}/stb")
		add_library(vendored::stb_vorbis ALIAS vendored_stb_vorbis)

		# the project's options on the two decoders built apart, as on the code that includes dr_libs: its
		# floating-point rules, and the sanitizers of the presets that have them, since they read files of any origin
		target_link_libraries(vendored_stb_vorbis PRIVATE modernjk_options)
		# but libFLAC's own position (src/libFLAC/lpc.c): the sums of its predictors overflow on a damaged file,
		# harmless in the audio path, and its fuzzing (OSS-Fuzz) does not report them; UndefinedBehaviorSanitizer keeps
		# every other check there. An interface target linked after modernjk_options, so that its flag follows the
		# sanitizer's.
		add_library(vendored_flac_overflow INTERFACE)
		if(modernjk_ENABLE_SANITIZER_UNDEFINED AND NOT MSVC)
			target_compile_options(vendored_flac_overflow INTERFACE -fno-sanitize=signed-integer-overflow)
		endif()
		target_link_libraries(FLAC PRIVATE modernjk_options vendored_flac_overflow)
	endif()

	# the engines that open a window, and the fuzzers, one of which plays the sound on OpenAL Soft's loopback device
	if(modernjk_BUILD_MP_CLIENT OR modernjk_BUILD_SP OR modernjk_BUILD_JK2 OR modernjk_BUILD_FUZZ_TESTS)
		# The sound (src/shared/sound): OpenAL Soft plays it, a static library built by its own CMake file without
		# its programs, its legacy EAX extension (the reverb goes through EFX), its HRTF data (HRTF is off) and its
		# C++ modules (the project scans for none); the system's sound servers are its required backends, so that a
		# build machine without one fails at configuration, and on Linux it opens their libraries at run time
		set(LIBTYPE STATIC)
		foreach(off UTILS EXAMPLES TESTS INSTALL INSTALL_CONFIG INSTALL_HRTF_DATA INSTALL_AMBDEC_PRESETS INSTALL_EXAMPLES
				INSTALL_UTILS UPDATE_BUILD_VERSION EAX EMBED_HRTF_DATA ENABLE_MODULES BACKEND_WAVE)
			set(ALSOFT_${off} OFF)
		endforeach()
		if(WIN32)
			set(ALSOFT_REQUIRE_WASAPI ON)
			set(ALSOFT_BACKEND_DSOUND OFF)
			set(ALSOFT_BACKEND_WINMM OFF)
		else()
			foreach(required PIPEWIRE PULSEAUDIO ALSA RTKIT)
				set(ALSOFT_REQUIRE_${required} ON)
			endforeach()
			foreach(off OSS SOLARIS SNDIO JACK PORTAUDIO)
				set(ALSOFT_BACKEND_${off} OFF)
			endforeach()
		endif()
		add_subdirectory("${libs}/openal-soft" "${PROJECT_BINARY_DIR}/libs/openal-soft" EXCLUDE_FROM_ALL SYSTEM)
		add_library(vendored::openal ALIAS OpenAL)
		# the warnings its own code gives with its own flags, seen on 23 November 2026, each switched off with its reason
		if(MSVC)
			target_compile_options(OpenAL PRIVATE
				# its GSL writes [[gsl::suppress(x)]] without quotes, which MSVC 19.52 deprecates (4,023 times)
				/wd4875
				# core/voice.cpp:1238 makes a std::pair<..., unsigned int> of a size_t, a count of samples bounded by
				# the device's update
				/wd4267)
		elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			target_compile_options(OpenAL PRIVATE
				# al/source.cpp:2573 copies a gsl::not_null pointer in a loop (rewritten on its master since)
				-Wno-range-loop-construct
				# GCC 15 in libstdc++'s ranges::join_view over std::optional, which OpenAL Soft instantiates: members it
				# cannot see initialised through the view
				-Wno-maybe-uninitialized)
		endif()
	endif()

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

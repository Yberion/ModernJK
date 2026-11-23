# The audio decoder benchmark of surveys/audio-decoders.md: one executable per decoder and format.
#   cmake -S <this folder> -B <build> -DCMAKE_BUILD_TYPE=Release -DDEC=<decoders folder: src/, prefix/, build/>
#         -DREPO=<repository root> -DMINIAUDIO=<miniaudio folder> [-DLIBS_PREFIX=<the libraries' install folder> (not PREFIX: CMake's compiler identification uses that name)]
#         [-DCHOSEN_ONLY=ON: only the decoders decided (dr_mp3, dr_wav, libFLAC, stb_vorbis) and their references,
#          for the checks on Linux (linux_check.sh) and with MSVC (msvc_check.bat)] [-DASAN=ON]
cmake_minimum_required(VERSION 3.20)
project(audiodec C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DEC "" CACHE PATH "decoders folder")
set(REPO "" CACHE PATH "repository root")
set(MINIAUDIO "" CACHE PATH "miniaudio folder")
option(ASAN "AddressSanitizer, on libraries built with it (build_libs.sh asan)" OFF)
option(CHOSEN_ONLY "only the decoders decided and their references" OFF)
set(S "${DEC}/src")
if(ASAN)
	set(LIBS_PREFIX "${DEC}/prefix-asan" CACHE PATH "the libraries' install folder")
	if(MSVC)
		add_compile_options(/fsanitize=address /Zi)   # stops at the first fault
		add_link_options(/INCREMENTAL:NO)
	else()
		add_compile_options(-fsanitize=address -fsanitize-recover=address -fno-omit-frame-pointer)   # ASAN_OPTIONS=halt_on_error=0 counts every fault
		add_link_options(-fsanitize=address)   # with llvm-mingw its runtime is a DLL: llvm-mingw's bin in the PATH to run
	endif()
else()
	set(LIBS_PREFIX "${DEC}/prefix" CACHE PATH "the libraries' install folder")
	if(MINGW)
		add_link_options(-static)   # no runtime DLL to find
	endif()
endif()
set(P "${LIBS_PREFIX}")
if(MSVC)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
	add_compile_options(-ffp-contract=off)   # as the project's top-level CMakeLists.txt
endif()
if(NOT WIN32 AND NOT APPLE)   # q_platform.h takes the architecture from the build there, as the project gives it
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
		add_compile_definitions(ARCH_STRING="arm64")
	else()
		add_compile_definitions(ARCH_STRING="x86_64")
	endif()
endif()

function(find_lib var)
	find_library(${var} NAMES ${ARGN} PATHS "${P}/lib" NO_DEFAULT_PATH REQUIRED)
endfunction()
find_lib(OGG_LIB ogg libogg)
find_lib(VORBIS_LIB vorbis libvorbis)
find_lib(VORBISFILE_LIB vorbisfile libvorbisfile)
find_lib(FLAC_LIB FLAC libFLAC)
set(FLAC_LIBS "${FLAC_LIB}" "${OGG_LIB}")
set(VORBIS_LIBS "${VORBISFILE_LIB}" "${VORBIS_LIB}" "${OGG_LIB}")

# The engine's MP3 decoder, as the engine compiles it (C, with the project's math library).
add_library(qlibm STATIC "${REPO}/src/shared/qcommon/q_libm.cpp")
target_include_directories(qlibm PRIVATE "${REPO}/libs/llvm-libc" "${REPO}/src")
file(GLOB MP3CODE_SRC "${REPO}/src/codemp/mp3code/*.c")
add_library(mp3code STATIC ${MP3CODE_SRC})
target_include_directories(mp3code PRIVATE "${REPO}/src/codemp" "${REPO}/src")
target_link_libraries(mp3code PUBLIC qlibm)

set(HEADERS "${S}/dr_libs" "${S}/minimp3" "${S}/stb" "${P}/include")
if(MINIAUDIO)
	list(APPEND HEADERS "${MINIAUDIO}")
endif()

# bench(<name> <decoder file> <definitions> [REF <reference>] [LIBS ...])
function(bench name file defs)
	cmake_parse_arguments(B "" "REF" "LIBS" ${ARGN})
	set(srcs bench.cpp ${file})
	if(B_REF)
		list(APPEND srcs refs.cpp)
		list(APPEND defs REFERENCE_DECODER REF_${B_REF})
	endif()
	add_executable(${name} ${srcs})
	target_compile_definitions(${name} PRIVATE ${defs} FLAC__NO_DLL)
	target_include_directories(${name} PRIVATE ${HEADERS})
	target_link_libraries(${name} PRIVATE ${B_LIBS})
	if(MSVC AND ASAN)
		# MSVC's AddressSanitizer runtime is a DLL of the compiler's directory, not in the PATH: copied next to the
		# program, as netcodec's CMakeLists.txt does
		get_filename_component(msvc_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)
		add_custom_command(TARGET ${name} POST_BUILD COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"${msvc_bin}/clang_rt.asan_dynamic-x86_64.dll" "$<TARGET_FILE_DIR:${name}>")
	endif()
endfunction()

# The decoders decided (19 November 2026) and their references.
bench(bench_mp3code dec_mp3.cpp DEC_MP3CODE LIBS mp3code)
bench(bench_drmp3 dec_mp3.cpp DEC_DRMP3 REF MP3CODE LIBS mp3code)
bench(bench_enginewav dec_wav.cpp DEC_ENGINEWAV)
bench(bench_drwav dec_wav.cpp DEC_DRWAV REF ENGINEWAV)
bench(bench_libflac dec_other.cpp DEC_LIBFLAC LIBS ${FLAC_LIBS})
bench(bench_vorbisfile dec_other.cpp DEC_VORBISFILE LIBS ${VORBIS_LIBS})
bench(bench_stbvorbis dec_other.cpp DEC_STBVORBIS REF VORBISFILE LIBS ${VORBIS_LIBS})
if(CHOSEN_ONLY)
	return()
endif()

find_lib(VORBISENC_LIB vorbisenc libvorbisenc)
find_lib(OPUS_LIB opus libopus)
find_lib(MPG123_LIB mpg123 libmpg123)
set(MPG123_LIBS "${MPG123_LIB}")
if(WIN32)
	list(APPEND MPG123_LIBS shlwapi)
endif()

# opusfile has no CMake project: its four files without the HTTP part.
add_library(opusfile STATIC "${S}/opusfile-0.12/src/info.c" "${S}/opusfile-0.12/src/internal.c"
	"${S}/opusfile-0.12/src/opusfile.c" "${S}/opusfile-0.12/src/stream.c")
target_include_directories(opusfile PUBLIC "${S}/opusfile-0.12/include" "${P}/include" "${P}/include/opus")
target_link_libraries(opusfile PUBLIC "${OPUS_LIB}" "${OGG_LIB}")

# libopusenc (no CMake project either), for the encoder of the samples only.
file(GLOB OPUSENC_SRC "${S}/libopusenc-0.3/src/*.c")
add_library(opusenc STATIC ${OPUSENC_SRC})
target_include_directories(opusenc PUBLIC "${S}/libopusenc-0.3/include" "${P}/include/opus")
target_compile_definitions(opusenc PRIVATE OUTSIDE_SPEEX RANDOM_PREFIX=libopusenc FLOATING_POINT
	PACKAGE_NAME="libopusenc" PACKAGE_VERSION="0.3")
target_link_libraries(opusenc PUBLIC "${OPUS_LIB}")

# The encoder of the FLAC, Ogg Vorbis and Opus samples.
add_executable(encode encode.cpp)
target_compile_definitions(encode PRIVATE FLAC__NO_DLL)
target_include_directories(encode PRIVATE ${HEADERS})
target_link_libraries(encode PRIVATE opusenc "${FLAC_LIB}" "${VORBISENC_LIB}" "${VORBIS_LIB}" "${OGG_LIB}")

# The sample files of the sound module's tests (test/sound/data), by the same reference encoders and LAME for MP3
# (-DLAME=<LAME's source folder>, the release archive unpacked; LAME has no CMake project, so its library's files with a
# config.h of the few definitions they need, rather than its configMS.h, whose integer macros break stdint.h).
set(LAME "" CACHE PATH "LAME's source folder, for the MP3 samples")
add_executable(make_test_samples make_test_samples.cpp)
target_compile_definitions(make_test_samples PRIVATE FLAC__NO_DLL)
target_include_directories(make_test_samples PRIVATE "${P}/include")
target_link_libraries(make_test_samples PRIVATE "${FLAC_LIB}" "${VORBISENC_LIB}" "${VORBIS_LIB}" "${OGG_LIB}")
if(LAME)
	file(GLOB LAME_SRC "${LAME}/libmp3lame/*.c")
	list(FILTER LAME_SRC EXCLUDE REGEX "mpglib_interface\\.c$")
	file(WRITE "${CMAKE_BINARY_DIR}/lame/config.h" [[
#define STDC_HEADERS 1
#define HAVE_INTTYPES_H 1
#define HAVE_STDINT_H 1
#define HAVE_STRING_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_LIMITS_H 1
#define PROTOTYPES 1
#define USE_FAST_LOG 1
#define LAME_LIBRARY_BUILD 1
#define PACKAGE "lame"
typedef float ieee754_float32_t;
typedef double ieee754_float64_t;
typedef long double ieee854_float80_t;
]])
	add_library(mp3lame STATIC ${LAME_SRC})
	target_compile_definitions(mp3lame PRIVATE HAVE_CONFIG_H)
	target_include_directories(mp3lame PRIVATE "${CMAKE_BINARY_DIR}/lame" "${LAME}/libmp3lame" PUBLIC "${LAME}/include")
	target_compile_definitions(make_test_samples PRIVATE WITH_LAME)
	target_link_libraries(make_test_samples PRIVATE mp3lame)
endif()

if(EXISTS "${P}/lib/liblibnyquist.a")
	set(NYQ_LIBS "${P}/lib/liblibnyquist.a")
else()
	set(NYQ_LIBS "${DEC}/build/nyquist/lib/liblibnyquist.a")
endif()

# MP3: against the engine's decoder.
bench(bench_minimp3 dec_mp3.cpp DEC_MINIMP3 REF MP3CODE LIBS mp3code)
bench(bench_mpg123 dec_mp3.cpp DEC_MPG123 REF MP3CODE LIBS mp3code ${MPG123_LIBS})
bench(bench_ma_mp3 dec_multi.cpp DEC_MINIAUDIO REF MP3CODE LIBS mp3code)
# The same, against libmpg123 (an implementation independent of minimp3's and of the engine's).
bench(bench_minimp3_vs_mpg123 dec_mp3.cpp DEC_MINIMP3 REF MPG123 LIBS ${MPG123_LIBS})
bench(bench_mp3code_vs_mpg123 dec_mp3.cpp DEC_MP3CODE REF MPG123 LIBS mp3code ${MPG123_LIBS})
bench(bench_drmp3_vs_mpg123 dec_mp3.cpp DEC_DRMP3 REF MPG123 LIBS ${MPG123_LIBS})
bench(bench_ma_mp3_vs_mpg123 dec_multi.cpp DEC_MINIAUDIO REF MPG123 LIBS ${MPG123_LIBS})
bench(bench_nyq_mp3 dec_multi.cpp "DEC_NYQUIST;NYQ_EXT=\"mp3\"" REF MP3CODE LIBS mp3code ${NYQ_LIBS})
target_include_directories(bench_nyq_mp3 PRIVATE "${S}/libnyquist/include")

# WAV: against the engine's reader.
bench(bench_ma_wav dec_multi.cpp DEC_MINIAUDIO REF ENGINEWAV)
bench(bench_nyq_wav dec_multi.cpp "DEC_NYQUIST;NYQ_EXT=\"wav\"" REF ENGINEWAV LIBS ${NYQ_LIBS})
target_include_directories(bench_nyq_wav PRIVATE "${S}/libnyquist/include")

# FLAC: against libFLAC (and every decoding against the source's hash, outside).
bench(bench_drflac dec_other.cpp DEC_DRFLAC REF LIBFLAC LIBS ${FLAC_LIBS})
bench(bench_ma_flac dec_multi.cpp DEC_MINIAUDIO REF LIBFLAC LIBS ${FLAC_LIBS})
bench(bench_nyq_flac dec_multi.cpp "DEC_NYQUIST;NYQ_EXT=\"flac\"" LIBS ${NYQ_LIBS})
target_include_directories(bench_nyq_flac PRIVATE "${S}/libnyquist/include")

# Ogg Vorbis: against libvorbis, the reference.
bench(bench_ma_vorbis dec_multi.cpp "DEC_MINIAUDIO;MA_WITH_STBVORBIS" REF VORBISFILE LIBS ${VORBIS_LIBS})
bench(bench_nyq_vorbis dec_multi.cpp "DEC_NYQUIST;NYQ_EXT=\"ogg\"" LIBS ${NYQ_LIBS})
target_include_directories(bench_nyq_vorbis PRIVATE "${S}/libnyquist/include")

# The code each decoder adds: the same executables without a reference, against the harness alone.
bench(size_none dec_none.cpp "")
bench(size_minimp3 dec_mp3.cpp DEC_MINIMP3)
bench(size_drmp3 dec_mp3.cpp DEC_DRMP3)
bench(size_mpg123 dec_mp3.cpp DEC_MPG123 LIBS ${MPG123_LIBS})
bench(size_drwav dec_wav.cpp DEC_DRWAV)
bench(size_drflac dec_other.cpp DEC_DRFLAC)
bench(size_stbvorbis dec_other.cpp DEC_STBVORBIS)
bench(size_ma_builtin dec_multi.cpp DEC_MINIAUDIO)
bench(size_ma_stbvorbis dec_multi.cpp "DEC_MINIAUDIO;MA_WITH_STBVORBIS")
bench(size_nyq dec_multi.cpp "DEC_NYQUIST;NYQ_EXT=\"mp3\"" LIBS ${NYQ_LIBS})
target_include_directories(size_nyq PRIVATE "${S}/libnyquist/include")

# Opus: against opusfile (all of them decode with libopus).
bench(bench_opusfile dec_other.cpp DEC_OPUSFILE LIBS opusfile)
bench(bench_ma_opus "dec_multi.cpp;${MINIAUDIO}/extras/decoders/libopus/miniaudio_libopus.c" "DEC_MINIAUDIO;MA_WITH_LIBOPUS" REF OPUSFILE LIBS opusfile)
bench(bench_nyq_opus dec_multi.cpp "DEC_NYQUIST;NYQ_EXT=\"opus\"" LIBS ${NYQ_LIBS})
target_include_directories(bench_nyq_opus PRIVATE "${S}/libnyquist/include")

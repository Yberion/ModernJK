# Benchmarks. ryg/rans_byte.h is the original ryg_rans header (public domain), kept unchanged for comparison:
# included as a system header so that the project's warnings do not apply to it.
netcodec_add_program(netcodec_bench_rans bench_rans.cpp)
target_include_directories(netcodec_bench_rans PRIVATE "${PROJECT_SOURCE_DIR}/test")
target_include_directories(netcodec_bench_rans SYSTEM PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/ryg")

# Coded size of the corpus traces against the bytes the server sent (docs/modernization/tools/python/export_demo_traces.py).
netcodec_add_program(netcodec_bench_size bench_size.cpp)
target_include_directories(netcodec_bench_size PRIVATE "${PROJECT_SOURCE_DIR}/test")

# Codec benchmark against the legacy coding: OpenJK's own src/codemp/qcommon/msg.cpp and huffman.cpp, built as the
# engine builds them (C++20, FINAL_BUILD, its headers), without the project's warnings (engine code, unchanged).
set(netcodec_MODERNJK_DIR "${PROJECT_SOURCE_DIR}/../../.." CACHE PATH "ModernJK source tree, for the legacy benchmark")
if(EXISTS "${netcodec_MODERNJK_DIR}/src/codemp/qcommon/msg.cpp")
	add_library(netcodec_legacy OBJECT
		"${netcodec_MODERNJK_DIR}/src/codemp/qcommon/msg.cpp"
		"${netcodec_MODERNJK_DIR}/src/codemp/qcommon/huffman.cpp"
		legacy/legacy_codec.cpp
		legacy/legacy_stubs.cpp)
	set_target_properties(netcodec_legacy PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
	target_include_directories(netcodec_legacy PRIVATE
		"${netcodec_MODERNJK_DIR}/src/codemp" "${netcodec_MODERNJK_DIR}/src/shared" "${netcodec_MODERNJK_DIR}/libs/gsl-lite/include")
	target_compile_definitions(netcodec_legacy PRIVATE FINAL_BUILD $<$<BOOL:${MSVC}>:_CRT_SECURE_NO_WARNINGS>)
	if(NOT WIN32)   # ModernJK's CMakeLists.txt defines it outside Windows (q_platform.h does on Windows)
		target_compile_definitions(netcodec_legacy PRIVATE "ARCH_STRING=\"${CMAKE_SYSTEM_PROCESSOR}\"")
	endif()
	target_compile_options(netcodec_legacy PRIVATE $<IF:$<BOOL:${MSVC}>,/W0,-w>)
	# aligned as the library (CMakeLists.txt): the legacy timings are the A/B scripts' check of a busy machine, so they
	# must not move with where the code falls
	if(NOT MSVC)
		target_compile_options(netcodec_legacy PRIVATE -falign-functions=64 -falign-loops=64)
	endif()
	# not netcodec_options: the legacy code is not instrumented by the sanitizers
	if(MSVC AND netcodec_ENABLE_SANITIZER_ADDRESS)
		# except AddressSanitizer with MSVC: its standard library refuses to link objects built with and without it, or
		# with different container annotations (same settings as cmake/Sanitizers.cmake)
		target_compile_options(netcodec_legacy PRIVATE /fsanitize=address)
		target_compile_definitions(netcodec_legacy PRIVATE _DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
	endif()

	netcodec_add_program(netcodec_bench_codec bench_codec.cpp)
	target_include_directories(netcodec_bench_codec PRIVATE "${PROJECT_SOURCE_DIR}/test" "${CMAKE_CURRENT_SOURCE_DIR}")
	target_link_libraries(netcodec_bench_codec PRIVATE netcodec_legacy)

	# The client packets recorded by the server in the legacy format (plan step 8, part 1) against the legacy coding.
	netcodec_add_program(netcodec_bench_client bench_client.cpp)
	target_include_directories(netcodec_bench_client PRIVATE "${PROJECT_SOURCE_DIR}/test" "${CMAKE_CURRENT_SOURCE_DIR}")
	target_link_libraries(netcodec_bench_client PRIVATE netcodec_legacy)
else()
	message(STATUS "netcodec: ModernJK sources not found in ${netcodec_MODERNJK_DIR}, no legacy benchmark")
endif()

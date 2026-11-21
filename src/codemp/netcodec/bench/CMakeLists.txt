# netcodec's benchmarks. ryg/rans_byte.h is the original ryg_rans header (public domain), kept unchanged for
# comparison: a system include folder, outside the project's warnings.
netcodec_add_program(netcodec_bench_rans bench_rans.cpp)
target_link_libraries(netcodec_bench_rans PRIVATE netcodec_traces)
target_include_directories(netcodec_bench_rans SYSTEM PRIVATE ryg)

# The coded size of the corpus traces against the bytes the server sent (export_demo_traces.py)
netcodec_add_program(netcodec_bench_size bench_size.cpp)
target_link_libraries(netcodec_bench_size PRIVATE netcodec_traces)

# The legacy coding the codec replaced: OpenJK's own msg.cpp and huffman.cpp, built as the engines build them (their
# settings and warnings), not instrumented by the sanitizers
set(legacy_engine "${netcodec_ENGINE_DIR}/src")
add_library(netcodec_legacy OBJECT
	"${legacy_engine}/codemp/qcommon/msg.cpp"
	"${legacy_engine}/codemp/qcommon/huffman.cpp"
	legacy/legacy_codec.cpp
	legacy/legacy_stubs.cpp)
target_include_directories(netcodec_legacy PRIVATE "${legacy_engine}/codemp" "${legacy_engine}/shared")
target_link_libraries(netcodec_legacy PRIVATE modernjk_warnings modernjk_defines)
target_compile_features(netcodec_legacy PRIVATE cxx_std_23)
if(MSVC)
	target_compile_options(netcodec_legacy PRIVATE /fp:precise)
	if(modernjk_ENABLE_SANITIZER_ADDRESS)
		# MSVC's standard library refuses to link objects built with and without AddressSanitizer, or with different
		# container annotations (cmake/Sanitizers.cmake): the legacy code instrumented there only
		target_compile_options(netcodec_legacy PRIVATE /fsanitize=address)
		target_compile_definitions(netcodec_legacy PRIVATE _DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
	endif()
else()
	# the engines' settings (ProjectOptions.cmake), and the library's alignment: the legacy timings are the A/B
	# scripts' check of a busy machine, so they must not move with where the code falls
	target_compile_options(netcodec_legacy PRIVATE -ffp-contract=off -fsigned-char -falign-functions=64
		-falign-loops=64)
endif()

# The codec against the legacy coding on the corpus
netcodec_add_program(netcodec_bench_codec bench_codec.cpp)
target_include_directories(netcodec_bench_codec PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(netcodec_bench_codec PRIVATE netcodec_traces netcodec_legacy)

# The client packets the server recorded in the legacy format, against the legacy coding
netcodec_add_program(netcodec_bench_client bench_client.cpp)
target_include_directories(netcodec_bench_client PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(netcodec_bench_client PRIVATE netcodec_traces netcodec_legacy)

# libFuzzer targets on the message decoder and on the client packet decoder (clang only). The library and the targets
# are instrumented for coverage (netcodec_options); libFuzzer provides main().
if(MINGW)
	# llvm-mingw's driver refuses -fsanitize=fuzzer: libFuzzer's archive from the compiler's resource folder
	execute_process(COMMAND "${CMAKE_CXX_COMPILER}" -print-resource-dir OUTPUT_VARIABLE clang_resources
		OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
foreach(target decode client)
	netcodec_add_program(netcodec_fuzz_${target} fuzz_${target}.cpp)
	target_link_libraries(netcodec_fuzz_${target} PRIVATE netcodec_internal netcodec_traces)
	if(MINGW)
		target_link_libraries(netcodec_fuzz_${target} PRIVATE "${clang_resources}/lib/windows/libclang_rt.fuzzer-x86_64.a")
	else()
		target_compile_options(netcodec_fuzz_${target} PRIVATE -fsanitize=fuzzer)
		target_link_options(netcodec_fuzz_${target} PRIVATE -fsanitize=fuzzer)
	endif()
endforeach()

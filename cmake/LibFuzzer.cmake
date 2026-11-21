# Whether the compiler links libFuzzer through -fsanitize=fuzzer, after cpp-best-practices/cmake_template
# (cmake/LibFuzzer.cmake). llvm-mingw ships libFuzzer but its driver refuses that flag: netcodec's fuzzers link the
# archive themselves there (src/codemp/netcodec/fuzz).
function(modernjk_check_libfuzzer_support var)
	include(CheckCXXSourceCompiles)
	set(CMAKE_REQUIRED_FLAGS -fsanitize=fuzzer)
	set(CMAKE_REQUIRED_LINK_OPTIONS -fsanitize=fuzzer)
	check_cxx_source_compiles([[
		#include <cstddef>
		#include <cstdint>
		extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *, std::size_t) { return 0; }
	]] ${var})
	set(${var} ${${var}} PARENT_SCOPE)
endfunction()

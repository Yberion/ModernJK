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

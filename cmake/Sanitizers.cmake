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

# AddressSanitizer and UndefinedBehaviorSanitizer, after cpp-best-practices/cmake_template (cmake/Sanitizers.cmake),
# given to the targets that link the interface target. The template's leak, thread and memory sanitizers are not taken:
# no test of the project uses threads, and the memory sanitizer needs an instrumented standard library.
# MSVC has AddressSanitizer only; clang (llvm-mingw on Windows too) and GCC have both.
function(modernjk_enable_sanitizers target address undefined)
	set(sanitizers "")
	if(address)
		list(APPEND sanitizers address)
	endif()
	if(undefined)
		if(MSVC)
			message(WARNING "MSVC has no UndefinedBehaviorSanitizer: only AddressSanitizer is used")
		else()
			list(APPEND sanitizers undefined)
		endif()
	endif()
	if(NOT sanitizers)
		return()
	endif()
	list(JOIN sanitizers "," list)
	if(MSVC)
		# the debugging information comes from the build type (RelWithDebInfo in the AddressSanitizer presets)
		target_compile_options(${target} INTERFACE /fsanitize=${list})
		# the standard library's container annotations need every object built with them: off, as some are not
		target_compile_definitions(${target} INTERFACE _DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
		# AddressSanitizer refuses incremental linking
		target_link_options(${target} INTERFACE /INCREMENTAL:NO)
	else()
		# a fault stops the program at once (no recovery), with readable stacks
		target_compile_options(${target} INTERFACE -fsanitize=${list} -fno-omit-frame-pointer -fno-sanitize-recover=all)
		target_link_options(${target} INTERFACE -fsanitize=${list})
	endif()
endfunction()

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

# The compiler warnings of the project's targets, after cpp-best-practices/cmake_template (cmake/CompilerWarnings.cmake),
# whose list is not taken as it is: each set below is chosen for this code.
#
# Two sets, each given to its targets through an interface target:
#  - modernjk_set_project_warnings: the game's and the engines' present level, -Wall for GCC and clang, MSVC at its
#    default level;
#  - modernjk_set_netcodec_warnings: netcodec's strict set, always as errors (netcodec is clean under it).
# They become one set once the game's code is clean under netcodec's.

function(modernjk_set_project_warnings target warnings_as_errors)
	if(MSVC)
		set(warnings "")
		if(warnings_as_errors)
			list(APPEND warnings /WX)
		endif()
	else()
		set(warnings
			-Wall
			# a /* inside a comment: OpenJK's code has many, harmless
			-Wno-comment
			# offsetof on classes that are not standard-layout: the field tables (msg.cpp, the save games) use it
			$<$<COMPILE_LANGUAGE:CXX>:-Wno-invalid-offsetof>)
		if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			# a class enum given to a printf-like function's %i or %d, which -Wformat accepts
			list(APPEND warnings $<$<COMPILE_LANGUAGE:CXX>:-Wformat-pedantic>)
		endif()
		if(warnings_as_errors)
			list(APPEND warnings -Werror)
		endif()
	endif()
	target_compile_options(${target} INTERFACE ${warnings})
endfunction()

function(modernjk_set_netcodec_warnings target)
	set(msvc_warnings
		/W4
		/w14242 # conversion, possible loss of data
		/w14254 # operator conversion of bit fields, possible loss of data
		/w14263 # member function does not override any base class virtual member function
		/w14265 # class has virtual functions, but its destructor is not virtual
		/w14287 # unsigned / negative constant mismatch
		/we4289 # loop control variable used outside the for-loop scope
		/w14296 # expression is always true or false
		/w14311 # pointer truncation
		/w14545 /w14546 /w14547 /w14549 # suspicious comma expressions
		/w14555 # expression has no effect
		/w14619 # pragma warning: there is no warning number
		/w14640 # thread-unsafe static member initialisation
		/w14826 # sign-extended conversion
		/w14905 /w14906 # string literal cast to LPSTR / LPWSTR
		/w14928 # illegal copy-initialisation
		/permissive-
		/WX)
	set(clang_warnings
		-Wall -Wextra -Wpedantic
		-Wshadow
		-Wnon-virtual-dtor
		-Wold-style-cast
		-Wcast-align
		-Wunused
		-Woverloaded-virtual
		-Wconversion
		-Wsign-conversion
		-Wnull-dereference
		-Wdouble-promotion
		-Wformat=2
		-Wimplicit-fallthrough
		-Werror)
	set(gcc_warnings
		${clang_warnings}
		-Wmisleading-indentation
		-Wduplicated-cond
		-Wduplicated-branches
		-Wlogical-op
		-Wuseless-cast
		-Wsuggest-override)
	if(MSVC)
		target_compile_options(${target} INTERFACE ${msvc_warnings})
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		target_compile_options(${target} INTERFACE ${clang_warnings})
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		target_compile_options(${target} INTERFACE ${gcc_warnings})
	else()
		message(AUTHOR_WARNING "No warning set for the compiler '${CMAKE_CXX_COMPILER_ID}'")
	endif()
endfunction()

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

# Hardening of the generated code, after cpp-best-practices/cmake_template (cmake/Hardening.cmake): checks that turn
# a memory defect into a crash rather than into code execution. Off by default (modernjk_ENABLE_HARDENING): the defects
# are fixed in the code instead.
include(CheckCXXCompilerFlag)

function(modernjk_enable_hardening target)
	set(options "")
	set(link_options "")
	set(definitions "")
	if(MSVC)
		list(APPEND options /sdl /DYNAMICBASE /guard:cf)
		list(APPEND link_options /NXCOMPAT /CETCOMPAT)
	else()
		list(APPEND definitions _GLIBCXX_ASSERTIONS)
		list(APPEND options $<$<NOT:$<CONFIG:Debug>>:-U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3>)
		check_cxx_compiler_flag(-fstack-protector-strong modernjk_has_stack_protector)
		if(modernjk_has_stack_protector)
			list(APPEND options -fstack-protector-strong)
		endif()
		check_cxx_compiler_flag(-fcf-protection modernjk_has_cf_protection)
		if(modernjk_has_cf_protection)
			list(APPEND options -fcf-protection)
		endif()
		check_cxx_compiler_flag(-fstack-clash-protection modernjk_has_clash_protection)
		if(modernjk_has_clash_protection AND (CMAKE_SYSTEM_NAME STREQUAL "Linux" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
			list(APPEND options -fstack-clash-protection)
		endif()
	endif()
	target_compile_options(${target} INTERFACE ${options})
	target_link_options(${target} INTERFACE ${link_options})
	target_compile_definitions(${target} INTERFACE ${definitions})
endfunction()

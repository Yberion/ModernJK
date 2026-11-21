#============================================================================
# Copyright (C) 2013 - 2018, OpenJK contributors
# Copyright (C) 2026, ModernJK contributors
#
# This file is part of the OpenJK source code.
#
# OpenJK is free software; you can redistribute it and/or modify it
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

# The tests of src/shared/qcommon/safe, without a framework (check.h)
add_executable(modernjk_test_safe main.cpp safe/string.cpp safe/limited_vector.cpp)
target_include_directories(modernjk_test_safe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src/shared")
target_link_libraries(modernjk_test_safe PRIVATE modernjk_safe_string modernjk_options modernjk_warnings
	modernjk_defines)
if(MINGW)
	target_link_options(modernjk_test_safe PRIVATE -static)
endif()
add_test(NAME modernjk.safe COMMAND modernjk_test_safe)

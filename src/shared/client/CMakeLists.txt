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

# The clients' code that needs no engine (a text field's editing, the keys' names): one static library, built with
# the strict warnings netcodec has, its headers included from src/ (<shared/client/field.h>)
add_library(modernjk_client STATIC field.cpp keynames.cpp)
target_include_directories(modernjk_client PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(modernjk_client PRIVATE modernjk_options netcodec_warnings)
# clean under its own clang-tidy checks (.clang-tidy here), so their findings are errors in the clang-tidy preset
modernjk_clang_tidy_as_errors(modernjk_client)

# The clients' own code made one for the three games (engine/), which each client compiles against its own headers
# (<client/client.h>), each tree's part in its own client/ files
add_library(modernjk_client_engine INTERFACE)
target_sources(modernjk_client_engine INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/engine/cl_keys.cpp")
target_link_libraries(modernjk_client_engine INTERFACE modernjk_client)

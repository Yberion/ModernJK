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

# The renderers' output stage: one static library independent of the renderers, built with the strict warnings
# netcodec has, its headers included from src/ (<shared/rd-common/output/stage.h>); position independent, since the
# renderers are shared libraries
add_library(modernjk_render_output STATIC gl.cpp stage.cpp)
target_include_directories(modernjk_render_output PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(modernjk_render_output PRIVATE vendored::fmt modernjk_options netcodec_warnings)
set_target_properties(modernjk_render_output PROPERTIES POSITION_INDEPENDENT_CODE ON)
# clean under its own clang-tidy checks (.clang-tidy here), so their findings are errors in the clang-tidy preset
modernjk_clang_tidy_as_errors(modernjk_render_output)

# The stage as a renderer runs it (engine/), which each renderer compiles against its own headers
add_library(modernjk_render_output_engine INTERFACE)
target_sources(modernjk_render_output_engine INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/engine/engine.cpp")
target_link_libraries(modernjk_render_output_engine INTERFACE modernjk_render_output)

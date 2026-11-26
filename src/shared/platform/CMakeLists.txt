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

# The platform layer of every engine that opens a window, on SDL 3: one static library independent of the engines,
# built with the strict warnings netcodec has, its headers included from src/ (<shared/platform/platform.h>)
add_library(modernjk_platform STATIC platform.cpp input/input.cpp window/window.cpp)
target_include_directories(modernjk_platform PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(modernjk_platform PUBLIC vendored::sdl vendored::fmt PRIVATE modernjk_options netcodec_warnings)
# clean under its own clang-tidy checks (.clang-tidy here), so their findings are errors in the clang-tidy preset
modernjk_clang_tidy_as_errors(modernjk_platform)

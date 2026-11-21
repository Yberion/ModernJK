#============================================================================
# Copyright (C) 2013 - 2018, OpenJK contributors
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

# The bot library, linked by both engines (which provide the shared code it calls)
add_library(botlib STATIC
	be_aas_bspq3.cpp
	be_aas_cluster.cpp
	be_aas_debug.cpp
	be_aas_entity.cpp
	be_aas_file.cpp
	be_aas_main.cpp
	be_aas_move.cpp
	be_aas_optimize.cpp
	be_aas_reach.cpp
	be_aas_route.cpp
	be_aas_routealt.cpp
	be_aas_sample.cpp
	be_ai_char.cpp
	be_ai_chat.cpp
	be_ai_gen.cpp
	be_ai_goal.cpp
	be_ai_move.cpp
	be_ai_weap.cpp
	be_ai_weight.cpp
	be_ea.cpp
	be_interface.cpp
	l_crc.cpp
	l_libvar.cpp
	l_log.cpp
	l_memory.cpp
	l_precomp.cpp
	l_script.cpp
	l_struct.cpp)
target_include_directories(botlib PRIVATE "${PROJECT_SOURCE_DIR}/src/shared" "${PROJECT_SOURCE_DIR}/src/codemp")
target_compile_definitions(botlib PRIVATE BOTLIB)
target_link_libraries(botlib PRIVATE modernjk_options modernjk_warnings modernjk_defines)

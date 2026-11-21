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

# The multiplayer client's renderer (OpenGL), which the client loads
add_library(rd-vanilla MODULE
	G2_API.cpp
	G2_bolts.cpp
	G2_bones.cpp
	G2_misc.cpp
	G2_surfaces.cpp
	tr_WorldEffects.cpp
	tr_arb.cpp
	tr_backend.cpp
	tr_bsp.cpp
	tr_cmds.cpp
	tr_curve.cpp
	tr_decals.cpp
	tr_ghoul2.cpp
	tr_image.cpp
	tr_init.cpp
	tr_light.cpp
	tr_main.cpp
	tr_marks.cpp
	tr_mesh.cpp
	tr_model.cpp
	tr_quicksprite.cpp
	tr_scene.cpp
	tr_shade.cpp
	tr_shade_calc.cpp
	tr_shader.cpp
	tr_shadows.cpp
	tr_skin.cpp
	tr_sky.cpp
	tr_subs.cpp
	tr_surface.cpp
	tr_surfacesprites.cpp
	tr_world.cpp
	../ghoul2/G2_gore.cpp
	../qcommon/matcomp.cpp
	../qcommon/q_shared.cpp
	../rd-common/tr_font.cpp)
set_target_properties(rd-vanilla PROPERTIES OUTPUT_NAME "rd-vanilla_${modernjk_ARCH}" PREFIX "")
target_include_directories(rd-vanilla PRIVATE "${PROJECT_SOURCE_DIR}/src/codemp" "${PROJECT_SOURCE_DIR}/src/shared"
	"${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(rd-vanilla PRIVATE modernjk_shared_common modernjk_rd_common OpenGL::GL modernjk_options
	modernjk_warnings modernjk_defines)
install(TARGETS rd-vanilla LIBRARY DESTINATION "${modernjk_INSTALL_JKA}")

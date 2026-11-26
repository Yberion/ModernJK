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

# The single player's renderers (OpenGL): Jedi Academy's, and Jedi Outcast's from the same sources with JK2_MODE
add_library(sp_renderer_common INTERFACE)
target_sources(sp_renderer_common INTERFACE
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
	tr_draw.cpp
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
	tr_stl.cpp
	tr_subs.cpp
	tr_surface.cpp
	tr_surfacesprites.cpp
	tr_world.cpp
	../qcommon/matcomp.cpp
	../qcommon/q_shared.cpp
	../rd-common/tr_font.cpp)
target_include_directories(sp_renderer_common INTERFACE "${PROJECT_SOURCE_DIR}/src/code" "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(sp_renderer_common INTERFACE RENDERER _JK2EXE)
target_link_libraries(sp_renderer_common INTERFACE vendored::gsl_lite modernjk_shared_common modernjk_safe_string
	modernjk_rd_common modernjk_render_output_engine OpenGL::GL)

if(modernjk_BUILD_SP)
	add_library(rdsp-vanilla MODULE)
	set_target_properties(rdsp-vanilla PROPERTIES OUTPUT_NAME "rdsp-vanilla_${modernjk_ARCH}" PREFIX "")
	target_link_libraries(rdsp-vanilla PRIVATE sp_renderer_common modernjk_options modernjk_warnings modernjk_defines)
	install(TARGETS rdsp-vanilla LIBRARY DESTINATION "${modernjk_INSTALL_JKA}")
endif()

if(modernjk_BUILD_JK2)
	add_library(rdjosp-vanilla MODULE)
	set_target_properties(rdjosp-vanilla PROPERTIES OUTPUT_NAME "rdjosp-vanilla_${modernjk_ARCH}" PREFIX "")
	target_compile_definitions(rdjosp-vanilla PRIVATE JK2_MODE)
	target_link_libraries(rdjosp-vanilla PRIVATE sp_renderer_common modernjk_options modernjk_warnings modernjk_defines)
	install(TARGETS rdjosp-vanilla LIBRARY DESTINATION "${modernjk_INSTALL_JK2}")
endif()

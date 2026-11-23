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

# The multiplayer: the message codec (netcodec), the bot library, the modules (each in its own folder) and the two
# engines, the client and the dedicated server, defined here since their sources span the tree.

if(modernjk_BUILD_MP_CLIENT OR modernjk_BUILD_MP_SERVER OR BUILD_TESTING OR modernjk_BUILD_BENCHMARKS
		OR modernjk_BUILD_TOOLS OR modernjk_BUILD_FUZZ_TESTS)
	add_subdirectory(netcodec)
endif()
if(modernjk_BUILD_MP_GAME)
	add_subdirectory(game)
endif()
if(modernjk_BUILD_MP_CLIENT)
	add_subdirectory(cgame)
	add_subdirectory(ui)
	add_subdirectory(rd-vanilla)
endif()
if(NOT modernjk_BUILD_MP_CLIENT AND NOT modernjk_BUILD_MP_SERVER)
	return()
endif()

add_subdirectory(botlib)

# What both engines compile: the common layer, the server (the client hosts games too) and the scripts
add_library(mp_engine_common INTERFACE)
target_sources(mp_engine_common INTERFACE
	icarus/BlockStream.cpp
	icarus/GameInterface.cpp
	icarus/Instance.cpp
	icarus/Interface.cpp
	icarus/Memory.cpp
	icarus/Q3_Interface.cpp
	icarus/Q3_Registers.cpp
	icarus/Sequence.cpp
	icarus/Sequencer.cpp
	icarus/TaskManager.cpp
	qcommon/GenericParser2.cpp
	qcommon/RoffSystem.cpp
	qcommon/cm_load.cpp
	qcommon/cm_patch.cpp
	qcommon/cm_polylib.cpp
	qcommon/cm_test.cpp
	qcommon/cm_trace.cpp
	qcommon/cmd.cpp
	qcommon/common.cpp
	qcommon/cvar.cpp
	qcommon/demo_record.cpp
	qcommon/huffman.cpp
	qcommon/matcomp.cpp
	qcommon/md4.cpp
	qcommon/md5.cpp
	qcommon/msg.cpp
	qcommon/net_chan.cpp
	qcommon/net_ip.cpp
	qcommon/persistence.cpp
	qcommon/q_shared.cpp
	qcommon/stringed_ingame.cpp
	qcommon/stringed_interface.cpp
	qcommon/vm.cpp
	qcommon/z_memman_pc.cpp
	server/NPCNav/navigator.cpp
	server/sv_bot.cpp
	server/sv_ccmds.cpp
	server/sv_challenge.cpp
	server/sv_client.cpp
	server/sv_game.cpp
	server/sv_gameapi.cpp
	server/sv_init.cpp
	server/sv_main.cpp
	server/sv_net_chan.cpp
	server/sv_netcodec.cpp
	server/sv_snapshot.cpp
	server/sv_world.cpp
	../shared/sys/snapvector.cpp)
target_include_directories(mp_engine_common INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(mp_engine_common INTERFACE botlib netcodec::netcodec modernjk_shared_common modernjk_filesystem
	modernjk_version)
if(WIN32)
	target_link_libraries(mp_engine_common INTERFACE wsock32)
endif()

if(modernjk_BUILD_MP_CLIENT)
	add_executable(modernjk WIN32)
	set_target_properties(modernjk PROPERTIES OUTPUT_NAME "modernjk.${modernjk_ARCH}")
	target_sources(modernjk PRIVATE
		client/FXExport.cpp
		client/FxPrimitives.cpp
		client/FxScheduler.cpp
		client/FxSystem.cpp
		client/FxTemplate.cpp
		client/FxUtil.cpp
		client/cl_cgame.cpp
		client/cl_cgameapi.cpp
		client/cl_cin.cpp
		client/cl_console.cpp
		client/cl_input.cpp
		client/cl_keys.cpp
		client/cl_lan.cpp
		client/cl_main.cpp
		client/cl_net_chan.cpp
		client/cl_netcodec.cpp
		client/cl_parse.cpp
		client/cl_scrn.cpp
		client/cl_ui.cpp
		client/cl_uiapi.cpp)
	if(WIN32)
		target_sources(modernjk PRIVATE win32/modernjk.rc)
	endif()
	target_link_libraries(modernjk PRIVATE mp_engine_common modernjk_sys_client modernjk_sound_engine modernjk_options
		modernjk_warnings modernjk_defines)
	modernjk_copy_runtime_dlls(modernjk)
	install(TARGETS modernjk RUNTIME DESTINATION "${modernjk_INSTALL_JKA}")
	if(WIN32)
		install(IMPORTED_RUNTIME_ARTIFACTS SDL2::SDL2 RUNTIME DESTINATION "${modernjk_INSTALL_JKA}")
	endif()
endif()

if(modernjk_BUILD_MP_SERVER)
	add_executable(modernjkded)
	set_target_properties(modernjkded PROPERTIES OUTPUT_NAME "modernjkded.${modernjk_ARCH}")
	# the dedicated server's renderer (the models' bones and tags, for the server's traces) and the null client
	target_sources(modernjkded PRIVATE
		ghoul2/G2_gore.cpp
		null/null_client.cpp
		null/null_input.cpp
		null/null_renderer.cpp
		rd-dedicated/G2_API.cpp
		rd-dedicated/G2_bolts.cpp
		rd-dedicated/G2_bones.cpp
		rd-dedicated/G2_misc.cpp
		rd-dedicated/G2_surfaces.cpp
		rd-dedicated/tr_backend.cpp
		rd-dedicated/tr_ghoul2.cpp
		rd-dedicated/tr_init.cpp
		rd-dedicated/tr_main.cpp
		rd-dedicated/tr_mesh.cpp
		rd-dedicated/tr_model.cpp
		rd-dedicated/tr_shader.cpp
		rd-dedicated/tr_skin.cpp)
	if(WIN32)
		target_sources(modernjkded PRIVATE win32/modernjkded.rc)
	endif()
	target_compile_definitions(modernjkded PRIVATE DEDICATED)
	target_link_libraries(modernjkded PRIVATE mp_engine_common modernjk_sys_dedicated modernjk_options
		modernjk_warnings modernjk_defines)
	install(TARGETS modernjkded RUNTIME DESTINATION "${modernjk_INSTALL_JKA}")
endif()

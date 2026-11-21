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

# Jedi Academy's single player: its game module and the renderers (each in its folder), and the engine, defined here
# since its sources span the tree. Jedi Outcast's engine and renderer are the same sources built with JK2_MODE.

if(modernjk_BUILD_SP)
	add_subdirectory(game)
endif()
add_subdirectory(rd-vanilla)

add_library(sp_engine_common INTERFACE)
target_sources(sp_engine_common INTERFACE
	client/cl_cgame.cpp
	client/cl_cin.cpp
	client/cl_console.cpp
	client/cl_input.cpp
	client/cl_keys.cpp
	client/cl_main.cpp
	client/cl_mp3.cpp
	client/cl_parse.cpp
	client/cl_scrn.cpp
	client/cl_ui.cpp
	client/snd_ambient.cpp
	client/snd_dma.cpp
	client/snd_mem.cpp
	client/snd_mix.cpp
	client/snd_music.cpp
	client/vmachine.cpp
	game/genericparser2.cpp
	mp3code/cdct.c
	mp3code/csbt.c
	mp3code/csbtb.c
	mp3code/csbtl3.c
	mp3code/cup.c
	mp3code/cupini.c
	mp3code/cupl1.c
	mp3code/cupl3.c
	mp3code/cwin.c
	mp3code/cwinb.c
	mp3code/cwinm.c
	mp3code/hwin.c
	mp3code/l3dq.c
	mp3code/l3init.c
	mp3code/mdct.c
	mp3code/mhead.c
	mp3code/msis.c
	mp3code/towave.c
	mp3code/uph.c
	mp3code/upsf.c
	qcommon/cm_load.cpp
	qcommon/cm_patch.cpp
	qcommon/cm_polylib.cpp
	qcommon/cm_test.cpp
	qcommon/cm_trace.cpp
	qcommon/cmd.cpp
	qcommon/common.cpp
	qcommon/cvar.cpp
	qcommon/md4.cpp
	qcommon/msg.cpp
	qcommon/net_chan.cpp
	qcommon/ojk_saved_game.cpp
	qcommon/persistence.cpp
	qcommon/q_shared.cpp
	qcommon/stringed_ingame.cpp
	qcommon/stringed_interface.cpp
	qcommon/strip.cpp
	qcommon/z_memman_pc.cpp
	server/exe_headers.cpp
	server/sv_ccmds.cpp
	server/sv_client.cpp
	server/sv_game.cpp
	server/sv_init.cpp
	server/sv_main.cpp
	server/sv_savegame.cpp
	server/sv_snapshot.cpp
	server/sv_world.cpp
	ui/ui_atoms.cpp
	ui/ui_connect.cpp
	ui/ui_main.cpp
	ui/ui_saber.cpp
	ui/ui_shared.cpp
	ui/ui_syscalls.cpp)
target_include_directories(sp_engine_common INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/src")
# _JK2EXE: the single player's engine (Jedi Academy's as well as Jedi Outcast's)
target_compile_definitions(sp_engine_common INTERFACE _JK2EXE)
target_link_libraries(sp_engine_common INTERFACE modernjk_version vendored::gsl_lite modernjk_shared_common
	modernjk_filesystem modernjk_safe_string modernjk_safe_files modernjk_sys_client)

if(modernjk_BUILD_SP)
	add_executable(modernjk_sp WIN32)
	set_target_properties(modernjk_sp PROPERTIES OUTPUT_NAME "modernjk_sp.${modernjk_ARCH}")
	if(WIN32)
		target_sources(modernjk_sp PRIVATE win32/winquake.rc)
	endif()
	target_link_libraries(modernjk_sp PRIVATE sp_engine_common modernjk_options modernjk_warnings modernjk_defines)
	modernjk_copy_runtime_dlls(modernjk_sp)
	install(TARGETS modernjk_sp RUNTIME DESTINATION "${modernjk_INSTALL_JKA}")
	if(WIN32)
		install(IMPORTED_RUNTIME_ARTIFACTS SDL2::SDL2 RUNTIME DESTINATION "${modernjk_INSTALL_JKA}")
	endif()
endif()

if(modernjk_BUILD_JK2)
	add_executable(openjo_sp WIN32)
	set_target_properties(openjo_sp PROPERTIES OUTPUT_NAME "openjo_sp.${modernjk_ARCH}")
	if(WIN32)
		target_sources(openjo_sp PRIVATE ../codeJK2/win32/winquake.rc)
	endif()
	target_compile_definitions(openjo_sp PRIVATE JK2_MODE)
	target_link_libraries(openjo_sp PRIVATE sp_engine_common modernjk_options modernjk_warnings modernjk_defines)
	modernjk_copy_runtime_dlls(openjo_sp)
	install(TARGETS openjo_sp RUNTIME DESTINATION "${modernjk_INSTALL_JK2}")
	if(WIN32)
		install(IMPORTED_RUNTIME_ARTIFACTS SDL2::SDL2 RUNTIME DESTINATION "${modernjk_INSTALL_JK2}")
	endif()
endif()

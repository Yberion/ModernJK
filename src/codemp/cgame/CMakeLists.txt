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

# The multiplayer client's game module: prediction, effects, drawing; it shares the movement code (bg_*) with the
# game module
add_library(cgame MODULE
	cg_consolecmds.cpp
	cg_cvar.cpp
	cg_draw.cpp
	cg_drawtools.cpp
	cg_effects.cpp
	cg_ents.cpp
	cg_event.cpp
	cg_info.cpp
	cg_light.cpp
	cg_localents.cpp
	cg_main.cpp
	cg_marks.cpp
	cg_newDraw.cpp
	cg_players.cpp
	cg_playerstate.cpp
	cg_predict.cpp
	cg_saga.cpp
	cg_scoreboard.cpp
	cg_servercmds.cpp
	cg_snapshot.cpp
	cg_sound.cpp
	cg_spawn.cpp
	cg_turret.cpp
	cg_view.cpp
	cg_weaponinit.cpp
	cg_weapons.cpp
	fx_blaster.cpp
	fx_bowcaster.cpp
	fx_bryarpistol.cpp
	fx_demp2.cpp
	fx_disruptor.cpp
	fx_flechette.cpp
	fx_force.cpp
	fx_heavyrepeater.cpp
	fx_rocketlauncher.cpp
	../game/AnimalNPC.cpp
	../game/FighterNPC.cpp
	../game/SpeederNPC.cpp
	../game/WalkerNPC.cpp
	../game/bg_g2_utils.cpp
	../game/bg_misc.cpp
	../game/bg_panimate.cpp
	../game/bg_pmove.cpp
	../game/bg_saber.cpp
	../game/bg_saberLoad.cpp
	../game/bg_saga.cpp
	../game/bg_slidemove.cpp
	../game/bg_vehicleLoad.cpp
	../game/bg_weapons.cpp
	../qcommon/q_shared.cpp
	../ui/ui_shared.cpp)
set_target_properties(cgame PROPERTIES OUTPUT_NAME "cgame${modernjk_ARCH}" PREFIX "")
target_include_directories(cgame PRIVATE "${PROJECT_SOURCE_DIR}/src/codemp" "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(cgame PRIVATE _CGAME)
target_link_libraries(cgame PRIVATE modernjk_shared_common modernjk_version modernjk_options modernjk_warnings
	modernjk_defines)
install(TARGETS cgame LIBRARY DESTINATION "${modernjk_INSTALL_JKA}/ModernJK")
install(TARGETS cgame LIBRARY DESTINATION "${modernjk_INSTALL_JKA}/base")

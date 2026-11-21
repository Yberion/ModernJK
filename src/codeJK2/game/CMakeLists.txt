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

# Jedi Outcast's single-player game module: the game and its client side (cgame) in one module; a few files come from
# Jedi Academy's tree
add_library(jospgame MODULE
	AI_Atst.cpp
	AI_Default.cpp
	AI_Droid.cpp
	AI_GalakMech.cpp
	AI_Grenadier.cpp
	AI_Howler.cpp
	AI_ImperialProbe.cpp
	AI_Interrogator.cpp
	AI_Jedi.cpp
	AI_Mark1.cpp
	AI_Mark2.cpp
	AI_MineMonster.cpp
	AI_Remote.cpp
	AI_Seeker.cpp
	AI_Sentry.cpp
	AI_Sniper.cpp
	AI_Stormtrooper.cpp
	AI_Utils.cpp
	G_Timer.cpp
	NPC.cpp
	NPC_behavior.cpp
	NPC_combat.cpp
	NPC_goal.cpp
	NPC_misc.cpp
	NPC_move.cpp
	NPC_reactions.cpp
	NPC_senses.cpp
	NPC_sounds.cpp
	NPC_spawn.cpp
	NPC_stats.cpp
	NPC_utils.cpp
	Q3_Interface.cpp
	Q3_Registers.cpp
	bg_misc.cpp
	bg_pangles.cpp
	bg_panimate.cpp
	bg_pmove.cpp
	bg_slidemove.cpp
	g_ICARUS.cpp
	g_active.cpp
	g_breakable.cpp
	g_camera.cpp
	g_client.cpp
	g_cmds.cpp
	g_combat.cpp
	g_functions.cpp
	g_fx.cpp
	g_inventory.cpp
	g_itemLoad.cpp
	g_items.cpp
	g_main.cpp
	g_mem.cpp
	g_misc.cpp
	g_misc_model.cpp
	g_missile.cpp
	g_mover.cpp
	g_nav.cpp
	g_navigator.cpp
	g_navnew.cpp
	g_object.cpp
	g_objectives.cpp
	g_ref.cpp
	g_roff.cpp
	g_savegame.cpp
	g_session.cpp
	g_spawn.cpp
	g_svcmds.cpp
	g_target.cpp
	g_trigger.cpp
	g_turret.cpp
	g_usable.cpp
	g_utils.cpp
	g_weapon.cpp
	g_weaponLoad.cpp
	genericparser2.cpp
	wp_atst.cpp
	wp_blaster_rifle.cpp
	wp_bot_laser.cpp
	wp_bowcaster.cpp
	wp_bryar_pistol.cpp
	wp_demp2.cpp
	wp_det_pack.cpp
	wp_disruptor.cpp
	wp_emplaced_gun.cpp
	wp_flechette.cpp
	wp_melee.cpp
	wp_repeater.cpp
	wp_rocket_launcher.cpp
	wp_saber.cpp
	wp_stun_baton.cpp
	wp_thermal.cpp
	wp_trip_mine.cpp
	../cgame/FX_ATSTMain.cpp
	../cgame/FX_Blaster.cpp
	../cgame/FX_Bowcaster.cpp
	../cgame/FX_BryarPistol.cpp
	../cgame/FX_DEMP2.cpp
	../cgame/FX_Disruptor.cpp
	../cgame/FX_Emplaced.cpp
	../cgame/FX_Flechette.cpp
	../cgame/FX_HeavyRepeater.cpp
	../cgame/FX_RocketLauncher.cpp
	../cgame/FxPrimitives.cpp
	../cgame/FxScheduler.cpp
	../cgame/FxSystem.cpp
	../cgame/FxTemplate.cpp
	../cgame/FxUtil.cpp
	../cgame/cg_camera.cpp
	../cgame/cg_consolecmds.cpp
	../cgame/cg_credits.cpp
	../cgame/cg_draw.cpp
	../cgame/cg_drawtools.cpp
	../cgame/cg_effects.cpp
	../cgame/cg_ents.cpp
	../cgame/cg_event.cpp
	../cgame/cg_info.cpp
	../cgame/cg_lights.cpp
	../cgame/cg_localents.cpp
	../cgame/cg_main.cpp
	../cgame/cg_marks.cpp
	../cgame/cg_players.cpp
	../cgame/cg_playerstate.cpp
	../cgame/cg_predict.cpp
	../cgame/cg_scoreboard.cpp
	../cgame/cg_servercmds.cpp
	../cgame/cg_snapshot.cpp
	../cgame/cg_syscalls.cpp
	../cgame/cg_text.cpp
	../cgame/cg_view.cpp
	../cgame/cg_weapons.cpp
	../icarus/BlockStream.cpp
	../icarus/Instance.cpp
	../icarus/Sequence.cpp
	../icarus/Sequencer.cpp
	../icarus/TaskManager.cpp
	../../code/Rufl/hstring.cpp
	../../code/qcommon/q_shared.cpp
	../../code/qcommon/tri_coll_test.cpp
	../../code/ui/gameinfo.cpp)
if(WIN32)
	target_sources(jospgame PRIVATE ../win32/game.rc)
endif()
set_target_properties(jospgame PROPERTIES OUTPUT_NAME "jospgame${modernjk_ARCH}" PREFIX "")
target_include_directories(jospgame PRIVATE "${PROJECT_SOURCE_DIR}/src/codeJK2" "${CMAKE_CURRENT_SOURCE_DIR}"
	"${PROJECT_SOURCE_DIR}/src/shared")
target_compile_definitions(jospgame PRIVATE JK2_MODE SP_GAME)
target_link_libraries(jospgame PRIVATE vendored::gsl_lite modernjk_version modernjk_shared_common modernjk_safe_string
	modernjk_safe_files modernjk_options modernjk_warnings modernjk_defines)
install(TARGETS jospgame LIBRARY DESTINATION "${modernjk_INSTALL_JK2}/ModernJK")

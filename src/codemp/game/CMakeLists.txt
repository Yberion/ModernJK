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

# The multiplayer game module, which the dedicated server and the client load
add_library(jampgame MODULE
	AnimalNPC.cpp
	FighterNPC.cpp
	NPC.cpp
	NPC_AI_Atst.cpp
	NPC_AI_Default.cpp
	NPC_AI_Droid.cpp
	NPC_AI_GalakMech.cpp
	NPC_AI_Grenadier.cpp
	NPC_AI_Howler.cpp
	NPC_AI_ImperialProbe.cpp
	NPC_AI_Interrogator.cpp
	NPC_AI_Jedi.cpp
	NPC_AI_Mark1.cpp
	NPC_AI_Mark2.cpp
	NPC_AI_MineMonster.cpp
	NPC_AI_Rancor.cpp
	NPC_AI_Remote.cpp
	NPC_AI_Seeker.cpp
	NPC_AI_Sentry.cpp
	NPC_AI_Sniper.cpp
	NPC_AI_Stormtrooper.cpp
	NPC_AI_Utils.cpp
	NPC_AI_Wampa.cpp
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
	SpeederNPC.cpp
	WalkerNPC.cpp
	ai_main.cpp
	ai_util.cpp
	ai_wpnav.cpp
	bg_g2_utils.cpp
	bg_misc.cpp
	bg_panimate.cpp
	bg_pmove.cpp
	bg_saber.cpp
	bg_saberLoad.cpp
	bg_saga.cpp
	bg_slidemove.cpp
	bg_vehicleLoad.cpp
	bg_weapons.cpp
	g_ICARUScb.cpp
	g_active.cpp
	g_bot.cpp
	g_client.cpp
	g_cmds.cpp
	g_combat.cpp
	g_cvar.cpp
	g_exphysics.cpp
	g_items.cpp
	g_log.cpp
	g_main.cpp
	g_mem.cpp
	g_misc.cpp
	g_missile.cpp
	g_mover.cpp
	g_nav.cpp
	g_navnew.cpp
	g_object.cpp
	g_saga.cpp
	g_scenario.cpp
	g_session.cpp
	g_spawn.cpp
	g_svcmds.cpp
	g_target.cpp
	g_team.cpp
	g_timer.cpp
	g_trigger.cpp
	g_turret.cpp
	g_turret_G2.cpp
	g_utils.cpp
	g_vehicleTurret.cpp
	g_vehicles.cpp
	g_weapon.cpp
	tri_coll_test.cpp
	w_force.cpp
	w_saber.cpp
	../qcommon/q_shared.cpp)
set_target_properties(jampgame PROPERTIES OUTPUT_NAME "jampgame${modernjk_ARCH}" PREFIX "")
target_include_directories(jampgame PRIVATE "${PROJECT_SOURCE_DIR}/src/codemp" "${PROJECT_SOURCE_DIR}/src/shared")
target_compile_definitions(jampgame PRIVATE _GAME)
target_link_libraries(jampgame PRIVATE modernjk_shared_common modernjk_version modernjk_options modernjk_warnings
	modernjk_defines)
# in the ModernJK folder, and in base so that the game uses it without the folder chosen
install(TARGETS jampgame LIBRARY DESTINATION "${modernjk_INSTALL_JKA}/ModernJK")
install(TARGETS jampgame LIBRARY DESTINATION "${modernjk_INSTALL_JKA}/base")

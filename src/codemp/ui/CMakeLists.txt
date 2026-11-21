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

# The multiplayer menus' module
add_library(ui MODULE
	ui_atoms.cpp
	ui_cvar.cpp
	ui_force.cpp
	ui_gameinfo.cpp
	ui_main.cpp
	ui_saber.cpp
	ui_shared.cpp
	../game/bg_misc.cpp
	../game/bg_saberLoad.cpp
	../game/bg_saga.cpp
	../game/bg_vehicleLoad.cpp
	../game/bg_weapons.cpp
	../qcommon/q_shared.cpp)
set_target_properties(ui PROPERTIES OUTPUT_NAME "ui${modernjk_ARCH}" PREFIX "")
target_include_directories(ui PRIVATE "${PROJECT_SOURCE_DIR}/src/codemp" "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(ui PRIVATE UI_BUILD)
target_link_libraries(ui PRIVATE modernjk_shared_common modernjk_version modernjk_options modernjk_warnings
	modernjk_defines)
install(TARGETS ui LIBRARY DESTINATION "${modernjk_INSTALL_JKA}/ModernJK")
install(TARGETS ui LIBRARY DESTINATION "${modernjk_INSTALL_JKA}/base")

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

# The determinism of the game's player movement (pmove_det.cpp): bg_pmove.cpp and the files it needs built as the game
# module (_GAME) and as cgame (_CGAME), with the modules' own settings (not netcodec's: the point is how the shipped
# game code behaves). Its output is compared between builds and compilers (validate.sh, validate.bat).
set(engine "${netcodec_ENGINE_DIR}/src")
foreach(side game cgame)
	set(t netcodec_pmove_${side})
	add_executable(${t}
		pmove_det.cpp
		"${engine}/codemp/game/bg_g2_utils.cpp"
		"${engine}/codemp/game/bg_misc.cpp"
		"${engine}/codemp/game/bg_panimate.cpp"
		"${engine}/codemp/game/bg_pmove.cpp"
		"${engine}/codemp/game/bg_saber.cpp"
		"${engine}/codemp/game/bg_saberLoad.cpp"
		"${engine}/codemp/game/bg_saga.cpp"
		"${engine}/codemp/game/bg_slidemove.cpp"
		"${engine}/codemp/game/bg_vehicleLoad.cpp"
		"${engine}/codemp/game/bg_weapons.cpp"
		"${engine}/codemp/qcommon/q_shared.cpp")
	string(TOUPPER "_${side}" side_definition)
	target_compile_definitions(${t} PRIVATE ${side_definition})
	target_include_directories(${t} PRIVATE "${engine}/codemp" "${engine}")
	target_link_libraries(${t} PRIVATE modernjk_shared_common modernjk_version modernjk_options modernjk_warnings
		modernjk_defines)
	if(MINGW)
		target_link_options(${t} PRIVATE -static)
	endif()
	add_test(NAME netcodec.pmove_${side} COMMAND ${t} 20000)
endforeach()

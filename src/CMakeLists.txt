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

# The four trees: the code both engines share (shared), the multiplayer (codemp, with netcodec), Jedi Academy's single
# player (code, whose engine Jedi Outcast's also builds from) and Jedi Outcast's game module (codeJK2).
add_subdirectory(shared)
add_subdirectory(codemp)
if(modernjk_BUILD_SP OR modernjk_BUILD_JK2)
	add_subdirectory(code)
endif()
if(modernjk_BUILD_JK2)
	add_subdirectory(codeJK2/game)
endif()

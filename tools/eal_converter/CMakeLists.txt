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

# The converter of the game's EAX zone files (eagle/<map>.eal, read by EalMan) into the sound's (eagle/<map>.efxzones),
# built on demand (cmake --build --preset <preset> --target eal_converter), with the strict warnings of the sound whose
# zones' code it takes
add_executable(eal_converter EXCLUDE_FROM_ALL eal_converter.cpp)
target_link_libraries(eal_converter PRIVATE modernjk_sound_zones vendored::ealman vendored::fmt modernjk_options netcodec_warnings)

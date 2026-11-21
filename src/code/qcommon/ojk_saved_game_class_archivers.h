/*
===========================================================================
Copyright (C) 2016, OpenJK contributors
Copyright (C) 2026, ModernJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

//
// Saved game specialized archivers
//


#ifndef OJK_SAVED_GAME_CLASS_ARCHIVERS_INCLUDED
#define OJK_SAVED_GAME_CLASS_ARCHIVERS_INCLUDED


#include "qcommon/q_math.h"
#include "ojk_saved_game_helper_fwd.h"


namespace ojk
{


template<>
class SavedGameClassArchiver<cplane_t>
{
public:
	static constexpr bool is_implemented = true;

	static void sg_export(
		SavedGameHelper& saved_game,
		const cplane_t& instance)
	{
		saved_game.write<float>(instance.normal);
		saved_game.write<float>(instance.dist);
		saved_game.write<uint8_t>(instance.type);
		saved_game.write<uint8_t>(instance.signbits);
		saved_game.write<uint8_t>(instance.pad);
	}

	static void sg_import(
		SavedGameHelper& saved_game,
		cplane_t& instance)
	{
		saved_game.read<float>(instance.normal);
		saved_game.read<float>(instance.dist);
		saved_game.read<uint8_t>(instance.type);
		saved_game.read<uint8_t>(instance.signbits);
		saved_game.read<uint8_t>(instance.pad);
	}
};


} // ojk


#endif // OJK_SAVED_GAME_CLASS_ARCHIVERS_INCLUDED

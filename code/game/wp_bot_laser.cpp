/*
===========================================================================
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

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

#include "g_local.h"
#include "w_local.h"

//	Bot Laser
//---------------------------------------------------------
void WP_BotLaser( gentity_t *ent )
//---------------------------------------------------------
{
	gentity_t	*missile = CreateMissile( muzzle, forwardVec, BRYAR_PISTOL_VEL, 10000, ent );

	missile->classname = "bryar_proj";
	missile->s.weapon = weapon_t::WP_BRYAR_PISTOL;

	missile->damage = BRYAR_PISTOL_DAMAGE;
	missile->dflags = damageFlags_t::DAMAGE_DEATH_KNOCKBACK;
	missile->methodOfDeath = meansOfDeath_t::MOD_ENERGY;
	missile->clipmask = MASK_SHOT | contentsFlags_t::CONTENTS_LIGHTSABER;
}
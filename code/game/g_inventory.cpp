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

//g_inventory.cpp
#include "g_local.h"

/*
================
Goodie Keys
================
*/
bool INV_GoodieKeyGive( gentity_t *target )
{
	if ( !target || !target->client )
	{
		return (false);
	}

	target->client->ps.inventory[inventory_t::INV_GOODIE_KEY]++;
	return (true);

}

bool INV_GoodieKeyTake( gentity_t *target )
{
	if ( !target || !target->client )
	{
		return (false);
	}

	if (target->client->ps.inventory[inventory_t::INV_GOODIE_KEY])
	{
		target->client->ps.inventory[inventory_t::INV_GOODIE_KEY]--;
		return (true);
	}

	//had no keys
	return (false);
}

std::optional<inventory_t> INV_GoodieKeyCheck( gentity_t *target )
{
	if ( !target || !target->client )
	{
		return (std::nullopt);
	}

	if ( target->client->ps.inventory[inventory_t::INV_GOODIE_KEY] )
	{//found a key
		return (inventory_t::INV_GOODIE_KEY);
	}

	//no keys
	return (std::nullopt);
}

/*
================
Security Keys
================
*/
bool INV_SecurityKeyGive( gentity_t *target, const char *keyname )
{
	if ( target == nullptr || keyname == nullptr || target->client == nullptr )
	{
		return false;
	}

	for ( int i = 0; i < MAX_SECURITY_KEYS; i++ )
	{
		if ( target->client->ps.security_key_message[i][0] == '\0' )
		{//fill in the first empty slot we find with this key
			target->client->ps.inventory[inventory_t::INV_SECURITY_KEY]++;	// He got the key
			Q_strncpyz( target->client->ps.security_key_message[i], keyname, MAX_SECURITY_KEY_MESSSAGE );
			return true;
		}
	}
	//couldn't find an empty slot
	return false;
}

void INV_SecurityKeyTake( gentity_t *target, const char *keyname )
{
	if ( !target || !keyname || !target->client )
	{
		return;
	}

	for ( int i = 0; i < MAX_SECURITY_KEYS; i++ )
	{
		if ( target->client->ps.security_key_message[i][0] != '\0' )
		{
			if ( !Q_stricmp( keyname, target->client->ps.security_key_message[i] ) )
			{
				target->client->ps.inventory[inventory_t::INV_SECURITY_KEY]--;	// Take the key
				target->client->ps.security_key_message[i][0] = '\0';
				return;
			}
		}
		/*
		//don't do this because we could have removed one that's between 2 valid ones
		else
		{
			break;
		}
		*/
	}
}

bool INV_SecurityKeyCheck( gentity_t *target, const char *keyname )
{
	if ( !target || !keyname || !target->client )
	{
		return (false);
	}

	for ( int i = 0; i < MAX_SECURITY_KEYS; i++ )
	{
		if ( target->client->ps.inventory[inventory_t::INV_SECURITY_KEY] && target->client->ps.security_key_message[i][0] != '\0' )
		{
			if ( !Q_stricmp( keyname, target->client->ps.security_key_message[i] ) )
			{
				return (true);
			}
		}
		/*
		//don't do this because we could have removed one that's between 2 valid ones
		else
		{
			break;
		}
		*/
	}

	return (false);
}

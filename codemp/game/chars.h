/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
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

#pragma once

//===========================================================================
//
// Name:			chars.h
// Function:		bot characteristics
// Programmer:		Mr Elusive (MrElusive@idsoftware.com)
// Last update:		1999-09-08
// Tab Size:		4 (real tabs)
//===========================================================================


//========================================================
//========================================================
//name
//gender of the bot
//attack skill
// >  0.0 && <  0.2 = don't move
// >  0.3 && <  1.0 = aim at enemy during retreat
// >  0.0 && <  0.4 = only move forward/backward
// >= 0.4 && <  1.0 = circle strafing
// >  0.7 && <  1.0 = random strafe direction change
//weapon weight file
//view angle difference to angle change factor
//maximum view angle change
//reaction time in seconds
//accuracy when aiming
//weapon specific aim accuracy
//skill when aiming
// >  0.0 && <  0.9 = aim is affected by enemy movement
// >  0.4 && <= 0.8 = enemy linear leading
// >  0.8 && <= 1.0 = enemy exact movement leading
// >  0.5 && <= 1.0 = prediction shots when enemy is not visible
// >  0.6 && <= 1.0 = splash damage by shooting nearby geometry
//weapon specific aim skill
//========================================================
//chat
//========================================================
//file with chats
//name of the chat character
//characters per minute type speed
//tendency to insult/praise
//tendency to chat misc
//tendency to chat at start or end of level
//tendency to chat entering or exiting the game
//tendency to chat when killed someone
//tendency to chat when died
//tendency to chat when enemy suicides
//tendency to chat when hit while talking
//tendency to chat when bot was hit but didn't dye
//tendency to chat when bot hit the enemy but enemy didn't dye
//tendency to randomly chat
//tendency to reply
//========================================================
//movement
//========================================================
//tendency to crouch
//tendency to jump
//tendency to walk
//tendency to jump using a weapon
//tendency to use the grapple hook when available
//========================================================
//goal
//========================================================
//item weight file
//the aggression of the bot
//the self preservation of the bot (rockets near walls etc.)
//how likely the bot is to take revenge
//tendency to camp
//========================================================
//========================================================
//tendency to get easy frags
//how alert the bot is (view distance)
//how much the bot fires it's weapon

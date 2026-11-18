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


// This file must be identical in the quake and utils directories

// contents flags are seperate bits
// a given brush can contribute multiple content bits

// these definitions also need to be in q_shared.h!

enum class contentsFlags_t : uint32_t
{
	CONTENTS_NONE			= 0x00000000u,
	CONTENTS_SOLID			= 0x00000001u,	// Default setting. An eye is never valid in a solid
	CONTENTS_LAVA			= 0x00000002u,	//
	CONTENTS_WATER			= 0x00000004u,	//
	CONTENTS_FOG			= 0x00000008u,	//
	CONTENTS_PLAYERCLIP		= 0x00000010u,	// Player physically blocked
	CONTENTS_MONSTERCLIP	= 0x00000020u,	// NPCs cannot physically pass through
	CONTENTS_BOTCLIP		= 0x00000040u,	// do not enter - NPCs try not to enter these
	CONTENTS_SHOTCLIP		= 0x00000080u,	// shots physically blocked
	CONTENTS_BODY			= 0x00000100u,	// should never be on a brush, only in game
	CONTENTS_CORPSE			= 0x00000200u,	// should never be on a brush, only in game
	CONTENTS_TRIGGER		= 0x00000400u,	//
	CONTENTS_NODROP			= 0x00000800u,	// don't leave bodies or items (death fog, lava)
	CONTENTS_TERRAIN		= 0x00001000u,	// volume contains terrain data
	CONTENTS_LADDER			= 0x00002000u,	//
	CONTENTS_ABSEIL			= 0x00004000u,	// used like ladder to define where an NPC can abseil
	CONTENTS_OPAQUE			= 0x00008000u,	// defaults to on, when off, solid can be seen through
	CONTENTS_OUTSIDE		= 0x00010000u,	// volume is considered to be in the outside (i.e. not indoors)
	CONTENTS_SLIME			= 0x00020000u,	// CHC needs this since we use same tools
	CONTENTS_LIGHTSABER		= 0x00040000u,	// ""
	CONTENTS_TELEPORTER		= 0x00080000u,	// ""
	CONTENTS_ITEM			= 0x00100000u,	// ""
	CONTENTS_UNUSED00200000	= 0x00200000u,	//
	CONTENTS_UNUSED00400000	= 0x00400000u,	//
	CONTENTS_UNUSED00800000	= 0x00800000u,	//
	CONTENTS_UNUSED01000000	= 0x01000000u,	//
	CONTENTS_UNUSED02000000	= 0x02000000u,	//
	CONTENTS_UNUSED04000000	= 0x04000000u,	//
	CONTENTS_DETAIL			= 0x08000000u,	// brushes not used for the bsp
	CONTENTS_INSIDE			= 0x10000000u,	// volume is considered to be inside (i.e. indoors)
	CONTENTS_UNUSED20000000	= 0x20000000u,	//
	CONTENTS_UNUSED40000000	= 0x40000000u,	//
	CONTENTS_TRANSLUCENT	= 0x80000000u,	// don't consume surface fragments inside
	CONTENTS_ALL			= 0xFFFFFFFFu,
};
using enum contentsFlags_t;
template <> inline constexpr bool enableEnumFlags<contentsFlags_t> = true;

#include "qcommon/q_surfaceflags.h"

#define MATERIAL_BITS			5
#define MATERIAL_MASK			0x1f	// mask to get the material type

#define MATERIAL_NONE			0			// for when the artist hasn't set anything up =)
#define MATERIAL_SOLIDWOOD		1			// freshly cut timber
#define MATERIAL_HOLLOWWOOD		2			// termite infested creaky wood
#define MATERIAL_SOLIDMETAL		3			// solid girders
#define MATERIAL_HOLLOWMETAL	4			// hollow metal machines
#define MATERIAL_SHORTGRASS		5			// manicured lawn
#define MATERIAL_LONGGRASS		6			// long jungle grass
#define MATERIAL_DIRT			7			// hard mud
#define MATERIAL_SAND			8			// sandy beach
#define MATERIAL_GRAVEL			9			// lots of small stones
#define MATERIAL_GLASS			10			//
#define MATERIAL_CONCRETE		11			// hardened concrete pavement
#define MATERIAL_MARBLE			12			// marble floors
#define MATERIAL_WATER			13			// light covering of water on a surface
#define MATERIAL_SNOW			14			// freshly laid snow
#define MATERIAL_ICE			15			// packed snow/solid ice
#define MATERIAL_FLESH			16			// hung meat, corpses in the world
#define MATERIAL_MUD			17			// wet soil
#define MATERIAL_BPGLASS		18			// bulletproof glass
#define MATERIAL_DRYLEAVES		19			// dried up leaves on the floor
#define MATERIAL_GREENLEAVES	20			// fresh leaves still on a tree
#define MATERIAL_FABRIC			21			// Cotton sheets
#define MATERIAL_CANVAS			22			// tent material
#define MATERIAL_ROCK			23			//
#define MATERIAL_RUBBER			24			// hard tire like rubber
#define MATERIAL_PLASTIC		25			//
#define MATERIAL_TILES			26			// tiled floor
#define MATERIAL_CARPET			27			// lush carpet
#define MATERIAL_PLASTER		28			// drywall style plaster
#define MATERIAL_SHATTERGLASS	29			// glass with the Crisis Zone style shattering
#define MATERIAL_ARMOR			30			// body armor
#define MATERIAL_COMPUTER		31			// computers/electronic equipment
#define MATERIAL_LAST			32			// number of materials

// A surface's material is a number in the low MATERIAL_BITS bits of its flags (an index of the material tables)
[[nodiscard]] inline int SURF_Material( surfaceFlags_t flags ) { return static_cast<int>( to_underlying( flags ) & MATERIAL_MASK ); }
[[nodiscard]] inline surfaceFlags_t SURF_MaterialBits( int material ) { return static_cast<surfaceFlags_t>( material ); }

// Defined as a macro here so one change will affect all the relevant files

#define MATERIALS	\
	"none",			\
	"solidwood",	\
	"hollowwood",	\
	"solidmetal",	\
	"hollowmetal",	\
	"shortgrass",	\
	"longgrass",	\
	"dirt",	   		\
	"sand",	   		\
	"gravel",		\
	"glass",		\
	"concrete",		\
	"marble",		\
	"water",		\
	"snow",	   		\
	"ice",			\
	"flesh",		\
	"mud",			\
	"bpglass",		\
	"dryleaves",	\
	"greenleaves",	\
	"fabric",		\
	"canvas",		\
	"rock",			\
	"rubber",		\
	"plastic",		\
	"tiles",		\
	"carpet",		\
	"plaster",		\
	"shatterglass",	\
	"armor",		\
	"computer"/* this was missing, see enums above, plus ShaderEd2 pulldown options */

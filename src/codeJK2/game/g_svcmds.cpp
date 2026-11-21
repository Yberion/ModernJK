/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors
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
#include "g_headers.h"

#include "g_local.h"
#include "g_icarus.h"
#include "wp_saber.h"

extern void Q3_SetViewEntity(int entID, const char *name);
extern bool G_ClearViewEntity( gentity_t *ent );

extern team_t TranslateTeamName( const char *name );
extern EnumArray<const char *, team_t, team_t::TEAM_NUM_TEAMS>	TeamNames;

/*
===================
Svcmd_EntityList_f
===================
*/
void	Svcmd_EntityList_f (void) {
	int			e;
	gentity_t		*check;

	check = g_entities;
	for (e = 0; e < globals.num_entities ; e++, check++) {
		if ( !check->inuse ) {
			continue;
		}
		gi.Printf("%3i:", e);
		switch ( check->s.eType ) {
		case entityType_t::ET_GENERAL:
			gi.Printf( "ET_GENERAL          " );
			break;
		case entityType_t::ET_PLAYER:
			gi.Printf( "ET_PLAYER           " );
			break;
		case entityType_t::ET_ITEM:
			gi.Printf( "ET_ITEM             " );
			break;
		case entityType_t::ET_MISSILE:
			gi.Printf( "ET_MISSILE          " );
			break;
		case entityType_t::ET_MOVER:
			gi.Printf( "ET_MOVER            " );
			break;
		case entityType_t::ET_BEAM:
			gi.Printf( "ET_BEAM             " );
			break;
		case entityType_t::ET_PORTAL:
			gi.Printf( "ET_PORTAL           " );
			break;
		case entityType_t::ET_SPEAKER:
			gi.Printf( "ET_SPEAKER          " );
			break;
		case entityType_t::ET_PUSH_TRIGGER:
			gi.Printf( "ET_PUSH_TRIGGER     " );
			break;
		case entityType_t::ET_TELEPORT_TRIGGER:
			gi.Printf( "ET_TELEPORT_TRIGGER " );
			break;
		case entityType_t::ET_INVISIBLE:
			gi.Printf( "ET_INVISIBLE        " );
			break;
		case entityType_t::ET_THINKER:
			gi.Printf( "ET_THINKER          " );
			break;
		case entityType_t::ET_CLOUD:
			gi.Printf( "ET_CLOUD            " );
			break;
		default:
			gi.Printf( "%-3i                ", to_underlying( check->s.eType ) );
			break;
		}

		if ( check->classname ) {
			gi.Printf("%s", check->classname);
		}
		gi.Printf("\n");
	}
}

gclient_t	*ClientForString( const char *s ) {
	gclient_t	*cl;
	int			i;
	int			idnum;

	// numeric values are just slot numbers
	if ( s[0] >= '0' && s[0] <= '9' ) {
		idnum = atoi( s );
		if ( idnum < 0 || idnum >= level.maxclients ) {
			Com_Printf( "Bad client slot: %i\n", idnum );
			return nullptr;
		}

		cl = &level.clients[idnum];
		if ( cl->pers.connected == clientConnected_t::CON_DISCONNECTED ) {
			gi.Printf( "Client %i is not connected\n", idnum );
			return nullptr;
		}
		return cl;
	}

	// check for a name match
	for ( i=0 ; i < level.maxclients ; i++ ) {
		cl = &level.clients[i];
		if ( cl->pers.connected == clientConnected_t::CON_DISCONNECTED ) {
			continue;
		}
		if ( !Q_stricmp( cl->pers.netname, s ) ) {
			return cl;
		}
	}

	gi.Printf( "User %s is not on the server\n", s );

	return nullptr;
}

//---------------------------
extern void G_StopCinematicSkip( void );
extern void G_StartCinematicSkip( void );
extern void ExitEmplacedWeapon( gentity_t *ent );
static void Svcmd_ExitView_f( void )
{
extern cvar_t	*g_skippingcin;
	static int exitViewDebounce = 0;
	if ( exitViewDebounce > level.time )
	{
		return;
	}
	exitViewDebounce = level.time + 500;
	if ( in_camera )
	{//see if we need to exit an in-game cinematic
		if ( g_skippingcin->integer )	// already doing cinematic skip?
		{// yes...   so stop skipping...
			G_StopCinematicSkip();
		}
		else
		{// no... so start skipping...
			G_StartCinematicSkip();
		}
	}
	else if ( !G_ClearViewEntity( player ) )
	{//didn't exit control of a droid or turret
		//okay, now try exiting emplaced guns or AT-ST's
		if ( Any( player->s.eFlags & entityFlags_t::EF_LOCKED_TO_WEAPON ) )
		{//get out of emplaced gun
			ExitEmplacedWeapon( player );
		}
		else if ( player->client && player->client->NPC_class == class_t::CLASS_ATST )
		{//a player trying to get out of his ATST
			GEntity_UseFunc( player->activator, player, player );
		}
	}
}

gentity_t *G_GetSelfForPlayerCmd( void )
{
	if ( g_entities[0].client->ps.viewEntity > 0
		&& g_entities[0].client->ps.viewEntity < ENTITYNUM_WORLD
		&& g_entities[g_entities[0].client->ps.viewEntity].client
		&& g_entities[g_entities[0].client->ps.viewEntity].s.weapon == weapon_t::WP_SABER )
	{//you're controlling another NPC
		return (&g_entities[g_entities[0].client->ps.viewEntity]);
	}
	else
	{
		return (&g_entities[0]);
	}
}

static void Svcmd_SaberColor_f()
{
	const char *color = gi.argv(1);
	if ( !VALIDSTRING( color ))
	{
		gi.Printf( "Usage:  saberColor <color>\n" );
		gi.Printf( "valid colors:  red, orange, yellow, green, blue, and purple\n" );

		return;
	}

	gentity_t *self = G_GetSelfForPlayerCmd();

	if ( !Q_stricmp( color, "red" ))
	{
		self->client->ps.saberColor = saber_colors_t::SABER_RED;
	}
	else if ( !Q_stricmp( color, "green" ))
	{
		self->client->ps.saberColor = saber_colors_t::SABER_GREEN;
	}
	else if ( !Q_stricmp( color, "yellow" ))
	{
		self->client->ps.saberColor = saber_colors_t::SABER_YELLOW;
	}
	else if ( !Q_stricmp( color, "orange" ))
	{
		self->client->ps.saberColor = saber_colors_t::SABER_ORANGE;
	}
	else if ( !Q_stricmp( color, "purple" ))
	{
		self->client->ps.saberColor = saber_colors_t::SABER_PURPLE;
	}
	else if ( !Q_stricmp( color, "blue" ))
	{
		self->client->ps.saberColor = saber_colors_t::SABER_BLUE;
	}
	else
	{
		gi.Printf( "Usage:  saberColor <color>\n" );
		gi.Printf( "valid colors:  red, orange, yellow, green, blue, and purple\n" );
	}
}

struct SetForceCmd {
	const char *desc;
	const char *cmdname;
	const forcePowerLevels_t maxlevel;
};

EnumArray<SetForceCmd, forcePowers_t, forcePowers_t::NUM_FORCE_POWERS> SetForceTable = {{
	{ "forceHeal",			"setForceHeal",			forcePowerLevels_t::FORCE_LEVEL_3	},
	{ "forceJump",			"setForceJump",			forcePowerLevels_t::FORCE_LEVEL_3	},
	{ "forceSpeed",			"setForceSpeed",		forcePowerLevels_t::FORCE_LEVEL_3	},
	{ "forcePush",			"setForcePush",			forcePowerLevels_t::FORCE_LEVEL_3	},
	{ "forcePull",			"setForcePull",			forcePowerLevels_t::FORCE_LEVEL_3	},
	{ "forceMindTrick",		"setForceMindTrick",	forcePowerLevels_t::FORCE_LEVEL_4	},
	{ "forceGrip",			"setForceGrip",			forcePowerLevels_t::FORCE_LEVEL_3	},
	{ "forceLightning",		"setForceLightning",	forcePowerLevels_t::FORCE_LEVEL_3	},
	{ "saberThrow",			"setSaberThrow",		forcePowerLevels_t::FORCE_LEVEL_3	},
	{ "saberDefense",		"setSaberDefense",		forcePowerLevels_t::FORCE_LEVEL_3	},
	{ "saberOffense",		"setSaberOffense",		forcePowerLevels_t::FORCE_LEVEL_5	},
}};

static void Svcmd_ForceSetLevel_f( forcePowers_t forcePower )
{
	if ( !g_entities[0].client )
	{
		return;
	}
	if ( !g_cheats->integer )
	{
		gi.SendServerCommand( 0, "print \"Cheats are not enabled on this server.\n\"");
		return;
	}
	const char *newVal = gi.argv(1);
	if ( !VALIDSTRING( newVal ) )
	{
		gi.Printf( "Current %s level is %d\n", SetForceTable[forcePower].desc, to_underlying( g_entities[0].client->ps.forcePowerLevel[forcePower] ) );
		gi.Printf( "Usage:  %s <level> (0 - %i)\n", SetForceTable[forcePower].cmdname, to_underlying( SetForceTable[forcePower].maxlevel ) );
		return;
	}
	const forcePowerLevels_t val = static_cast<forcePowerLevels_t>( atoi(newVal) );
	if ( val > forcePowerLevels_t::FORCE_LEVEL_0 )
	{
		g_entities[0].client->ps.forcePowersKnown |= Bit( forcePower );
	}
	else
	{
		g_entities[0].client->ps.forcePowersKnown &= ~Bit( forcePower );
	}
	g_entities[0].client->ps.forcePowerLevel[forcePower] = val;
	if ( g_entities[0].client->ps.forcePowerLevel[forcePower] < forcePowerLevels_t::FORCE_LEVEL_0 )
	{
		g_entities[0].client->ps.forcePowerLevel[forcePower] = forcePowerLevels_t::FORCE_LEVEL_0;
	}
	else if ( g_entities[0].client->ps.forcePowerLevel[forcePower] > SetForceTable[forcePower].maxlevel )
	{
		g_entities[0].client->ps.forcePowerLevel[forcePower] = SetForceTable[forcePower].maxlevel;
	}
}

extern bool PM_SaberInStart( saberMoveName_t move );
extern bool PM_SaberInTransition( saberMoveName_t move );
extern bool PM_SaberInAttack( saberMoveName_t move );
void Svcmd_SaberAttackCycle_f( void )
{
	if ( !g_entities[0].client )
	{
		return;
	}

	gentity_t *self = G_GetSelfForPlayerCmd();
	if ( self->s.weapon != weapon_t::WP_SABER )
	{
		return;
	}

	forcePowerLevels_t	saberAnimLevel;
	if ( !self->s.number )
	{
		saberAnimLevel = cg.saberAnimLevelPending;
	}
	else
	{
		saberAnimLevel = self->client->ps.saberAnimLevel;
	}
	saberAnimLevel++;
	if ( self->client->ps.forcePowerLevel[forcePowers_t::FP_SABER_OFFENSE] == forcePowerLevels_t::FORCE_LEVEL_1 )
	{
		saberAnimLevel = forcePowerLevels_t::FORCE_LEVEL_2;
	}
	else if ( self->client->ps.forcePowerLevel[forcePowers_t::FP_SABER_OFFENSE] == forcePowerLevels_t::FORCE_LEVEL_2 )
	{
		if ( saberAnimLevel > forcePowerLevels_t::FORCE_LEVEL_2 )
		{
			saberAnimLevel = forcePowerLevels_t::FORCE_LEVEL_1;
		}
	}
	else
	{//level 3
		if ( saberAnimLevel > self->client->ps.forcePowerLevel[forcePowers_t::FP_SABER_OFFENSE] )
		{//wrap around
			saberAnimLevel = forcePowerLevels_t::FORCE_LEVEL_1;
		}
	}

	if ( !self->s.number )
	{
		cg.saberAnimLevelPending = saberAnimLevel;
	}
	else
	{
		self->client->ps.saberAnimLevel = saberAnimLevel;
	}

#ifndef FINAL_BUILD
	switch ( saberAnimLevel )
	{
	case forcePowerLevels_t::FORCE_LEVEL_1:
		gi.Printf( S_COLOR_BLUE "Lightsaber Combat Style: Fast\n" );
		//LIGHTSABERCOMBATSTYLE_FAST
		break;
	case forcePowerLevels_t::FORCE_LEVEL_2:
		gi.Printf( S_COLOR_YELLOW "Lightsaber Combat Style: Medium\n" );
		//LIGHTSABERCOMBATSTYLE_MEDIUM
		break;
	case forcePowerLevels_t::FORCE_LEVEL_3:
		gi.Printf( S_COLOR_RED "Lightsaber Combat Style: Strong\n" );
		//LIGHTSABERCOMBATSTYLE_STRONG
		break;
	case forcePowerLevels_t::FORCE_LEVEL_4:
		gi.Printf( S_COLOR_CYAN "Lightsaber Combat Style: Desann\n" );
		//LIGHTSABERCOMBATSTYLE_DESANN
		break;
	case forcePowerLevels_t::FORCE_LEVEL_5:
		gi.Printf( S_COLOR_MAGENTA "Lightsaber Combat Style: Tavion\n" );
		//LIGHTSABERCOMBATSTYLE_TAVION
		break;
	default:
		break;
	}
	//gi.Printf("\n");
#endif
}

template <forcePowers_t power>
static void Svcmd_ForceSetLevel_f(void)
{
	Svcmd_ForceSetLevel_f(power);
}

static void Svcmd_SetForceAll_f(void)
{
	for ( forcePowers_t i = forcePowers_t::FP_HEAL; i <= forcePowers_t::FP_SABER_OFFENSE; i++ )
	{
		Svcmd_ForceSetLevel_f( i );
	}
}

static void Svcmd_SetSaberAll_f(void)
{
	Svcmd_ForceSetLevel_f( forcePowers_t::FP_SABERTHROW );
	Svcmd_ForceSetLevel_f( forcePowers_t::FP_SABER_DEFENSE );
	Svcmd_ForceSetLevel_f( forcePowers_t::FP_SABER_OFFENSE );
}

static void Svcmd_RunScript_f(void)
{
	const char *cmd2 = gi.argv(1);

	if ( cmd2 && cmd2[0] )
	{
		const char *cmd3 = gi.argv(2);
		if ( cmd3 && cmd3[0] )
		{
			gentity_t *found = nullptr;
			if ( (found = G_Find(nullptr, FOFS(targetname), cmd2 ) ) != nullptr )
			{
				ICARUS_RunScript( found, cmd3 );
			}
			else
			{
				//can't find cmd2
				gi.Printf( S_COLOR_RED "runscript: can't find targetname %s\n", cmd2 );
			}
		}
		else
		{
			ICARUS_RunScript( &g_entities[0], cmd2 );
		}
	}
	else
	{
		gi.Printf( S_COLOR_RED "usage: runscript <ent targetname> scriptname\n" );
	}
}

static void Svcmd_PlayerTeam_f(void)
{
	const char *cmd2 = gi.argv(1);

	if ( !*cmd2 || !cmd2[0] )
	{
		gi.Printf( S_COLOR_RED "'playerteam' - change player team, requires a team name!\n" );
		gi.Printf( S_COLOR_RED "Current team is: %s\n", TeamNames[g_entities[0].client->playerTeam] );
		gi.Printf( S_COLOR_RED "Valid team names are:\n");
		for ( team_t n = team_t::TEAM_FREE + 1; n < team_t::TEAM_NUM_TEAMS; n++ )
		{
			gi.Printf( S_COLOR_RED "%s\n", TeamNames[n] );
		}
	}
	else
	{
		team_t team = TranslateTeamName( cmd2 );

		if ( team == team_t::TEAM_FREE )
		{
			gi.Printf( S_COLOR_RED "'playerteam' unrecognized team name %s!\n", cmd2 );
			gi.Printf( S_COLOR_RED "Current team is: %s\n", TeamNames[g_entities[0].client->playerTeam] );
			gi.Printf( S_COLOR_RED "Valid team names are:\n");
			for ( team_t n = team_t::TEAM_FREE; n < team_t::TEAM_NUM_TEAMS; n++ )
			{
				gi.Printf( S_COLOR_RED "%s\n", TeamNames[n] );
			}
		}
		else
		{
			g_entities[0].client->playerTeam = team;
			//FIXME: convert Imperial, Malon, Hirogen and Klingon to Scavenger?
		}
	}
}

static void Svcmd_Control_f(void)
{
	const char	*cmd2 = gi.argv(1);
	if ( !*cmd2 || !cmd2[0] )
	{
		if ( !G_ClearViewEntity( &g_entities[0] ) )
		{
			gi.Printf( S_COLOR_RED "control <NPC_targetname>\n" );
		}
	}
	else
	{
		Q3_SetViewEntity( 0, cmd2 );
	}
}

static void Svcmd_Secrets_f(void)
{
	const gentity_t *pl = &g_entities[0];
	if(pl->client->sess.missionStats.totalSecrets < 1)
	{
		gi.Printf( "There are" S_COLOR_RED " NO " S_COLOR_WHITE "secrets on this map!\n" );
	}
	else if(pl->client->sess.missionStats.secretsFound == pl->client->sess.missionStats.totalSecrets)
	{
		gi.Printf( "You've found all " S_COLOR_GREEN "%i" S_COLOR_WHITE " secrets on this map!\n", pl->client->sess.missionStats.secretsFound );
	}
	else
	{
		gi.Printf( "You've found " S_COLOR_GREEN "%i" S_COLOR_WHITE " out of " S_COLOR_GREEN "%i" S_COLOR_WHITE " secrets!\n", pl->client->sess.missionStats.secretsFound, pl->client->sess.missionStats.totalSecrets );
	}
}

// PADAWAN - g_spskill 0 + cg_crosshairForceHint 1 + handicap 100
// JEDI - g_spskill 1 + cg_crosshairForceHint 1 + handicap 100
// JEDI KNIGHT - g_spskill 2 + cg_crosshairForceHint 0 + handicap 100
// JEDI MASTER - g_spskill 2 + cg_crosshairForceHint 0 + handicap 50

extern cvar_t *g_spskill;
static void Svcmd_Difficulty_f(void)
{
	if(gi.argc() == 1)
	{
		if(g_spskill->integer == 0)
		{
			gi.Printf( S_COLOR_GREEN "Current Difficulty: Padawan" S_COLOR_WHITE "\n" );
		}
		else if(g_spskill->integer == 1)
		{
			gi.Printf( S_COLOR_GREEN "Current Difficulty: Jedi" S_COLOR_WHITE "\n" );
		}
		else if(g_spskill->integer == 2)
		{
			int crosshairHint = gi.Cvar_VariableIntegerValue("cg_crosshairForceHint");
			int handicap = gi.Cvar_VariableIntegerValue("handicap");
			if(handicap == 100 && crosshairHint == 0)
			{
				gi.Printf( S_COLOR_GREEN "Current Difficulty: Jedi Knight" S_COLOR_WHITE "\n" );
			}
			else if(handicap == 50 && crosshairHint == 0)
			{
				gi.Printf( S_COLOR_GREEN "Current Difficulty: Jedi Master" S_COLOR_WHITE "\n" );
			}
			else
			{
				gi.Printf( S_COLOR_GREEN "Current Difficulty: Jedi Knight (Custom)" S_COLOR_WHITE "\n" );
				gi.Printf( S_COLOR_GREEN "Crosshair Force Hint: %i" S_COLOR_WHITE "\n", crosshairHint != 0 ? 1 : 0 );
				gi.Printf( S_COLOR_GREEN "Handicap: %i" S_COLOR_WHITE "\n", handicap );
			}
		}
		else
		{
			gi.Printf( S_COLOR_RED "Invalid difficulty cvar set! g_spskill (%i) [0-2] is valid range only" S_COLOR_WHITE "\n", g_spskill->integer );
		}
	}
}

enum class commandFlags_t
{
	CMD_NONE				= 0x00000000u,
	CMD_CHEAT				= 0x00000001u,
	CMD_ALIVE				= 0x00000002u,
};
template <> inline constexpr bool enableEnumFlags<commandFlags_t> = true;

typedef struct svcmd_s {
	const char	*name;
	void		(*func)(void);
	commandFlags_t	flags;
} svcmd_t;

static int svcmdcmp( const void *a, const void *b ) {
	return Q_stricmp( (const char *)a, ((svcmd_t*)b)->name );
}

// FIXME some of these should be made CMD_ALIVE too!
static svcmd_t svcmds[] = {
	{ "entitylist",					Svcmd_EntityList_f,							commandFlags_t::CMD_NONE },
	{ "game_memory",				Svcmd_GameMem_f,							commandFlags_t::CMD_NONE },

	{ "nav",						Svcmd_Nav_f,								commandFlags_t::CMD_CHEAT },
	{ "npc",						Svcmd_NPC_f,								commandFlags_t::CMD_CHEAT },
	{ "use",						Svcmd_Use_f,								commandFlags_t::CMD_CHEAT },
	{ "ICARUS",						Svcmd_ICARUS_f,								commandFlags_t::CMD_CHEAT },

	{ "saberColor",					Svcmd_SaberColor_f,							commandFlags_t::CMD_CHEAT },

	{ "setForceJump",				Svcmd_ForceSetLevel_f<forcePowers_t::FP_LEVITATION>,		commandFlags_t::CMD_CHEAT },
	{ "setSaberThrow",				Svcmd_ForceSetLevel_f<forcePowers_t::FP_SABERTHROW>,		commandFlags_t::CMD_CHEAT },
	{ "setForceHeal",				Svcmd_ForceSetLevel_f<forcePowers_t::FP_HEAL>,				commandFlags_t::CMD_CHEAT },
	{ "setForcePush",				Svcmd_ForceSetLevel_f<forcePowers_t::FP_PUSH>,				commandFlags_t::CMD_CHEAT },
	{ "setForcePull",				Svcmd_ForceSetLevel_f<forcePowers_t::FP_PULL>,				commandFlags_t::CMD_CHEAT },
	{ "setForceSpeed",				Svcmd_ForceSetLevel_f<forcePowers_t::FP_SPEED>,			commandFlags_t::CMD_CHEAT },
	{ "setForceGrip",				Svcmd_ForceSetLevel_f<forcePowers_t::FP_GRIP>,				commandFlags_t::CMD_CHEAT },
	{ "setForceLightning",			Svcmd_ForceSetLevel_f<forcePowers_t::FP_LIGHTNING>,		commandFlags_t::CMD_CHEAT },
	{ "setMindTrick",				Svcmd_ForceSetLevel_f<forcePowers_t::FP_TELEPATHY>,		commandFlags_t::CMD_CHEAT },
	{ "setSaberDefense",			Svcmd_ForceSetLevel_f<forcePowers_t::FP_SABER_DEFENSE>,	commandFlags_t::CMD_CHEAT },
	{ "setSaberOffense",			Svcmd_ForceSetLevel_f<forcePowers_t::FP_SABER_OFFENSE>,	commandFlags_t::CMD_CHEAT },
	{ "setForceAll",				Svcmd_SetForceAll_f,						commandFlags_t::CMD_CHEAT },
	{ "setSaberAll",				Svcmd_SetSaberAll_f,						commandFlags_t::CMD_CHEAT },

	{ "saberAttackCycle",			Svcmd_SaberAttackCycle_f,					commandFlags_t::CMD_NONE },

	{ "runscript",					Svcmd_RunScript_f,							commandFlags_t::CMD_CHEAT },

	{ "playerTeam",					Svcmd_PlayerTeam_f,							commandFlags_t::CMD_CHEAT },

	{ "control",					Svcmd_Control_f,							commandFlags_t::CMD_CHEAT },

	{ "exitview",					Svcmd_ExitView_f,							commandFlags_t::CMD_NONE },

	{ "secrets",					Svcmd_Secrets_f,							commandFlags_t::CMD_NONE },
	{ "difficulty",					Svcmd_Difficulty_f,							commandFlags_t::CMD_NONE },

	//{ "say",						Svcmd_Say_f,						true },
	//{ "toggleallowvote",			Svcmd_ToggleAllowVote_f,			false },
	//{ "toggleuserinfovalidation",	Svcmd_ToggleUserinfoValidation_f,	false },
};
static const size_t numsvcmds = ARRAY_LEN( svcmds );

/*
=================
ConsoleCommand
=================
*/
bool	ConsoleCommand( void ) {
	const char *cmd = gi.argv(0);
	const svcmd_t *command = (const svcmd_t *)Q_LinearSearch( cmd, svcmds, numsvcmds, sizeof( svcmds[0] ), svcmdcmp );

	if ( !command )
		return false;

	if ( Any( command->flags & commandFlags_t::CMD_CHEAT )
		&& !g_cheats->integer )
	{
		gi.Printf( "Cheats are not enabled on this server.\n" );
		return true;
	}
	else if ( Any( command->flags & commandFlags_t::CMD_ALIVE )
		&& (g_entities[0].health <= 0) )
	{
		gi.Printf( "You must be alive to use this command.\n" );
		return true;
	}
	else
		command->func();
	return true;
}


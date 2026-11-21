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

// cg_event.cpp -- handle entity events at snapshot or playerstate transitions

#include "cg_local.h"
#include "fx_local.h"
#include "ui/ui_shared.h"
#include "ui/ui_public.h"

// for the voice chats
#include "ui/menudef.h"

#include "ghoul2/G2.h"
//==========================================================================

extern bool WP_SaberBladeUseSecondBladeStyle( saberInfo_t *saber, int bladeNum );
extern bool CG_VehicleWeaponImpact( centity_t *cent );
extern bool CG_InFighter( void );
extern bool CG_InATST( void );
extern int cg_saberFlashTime;
extern vec3_t cg_saberFlashPos;
extern EnumArray<const char *, forcePowers_t, forcePowers_t::NUM_FORCE_POWERS + 1> showPowersName;

extern int cg_siegeDeathTime;
extern int cg_siegeDeathDelay;
extern int cg_vehicleAmmoWarning;
extern int cg_vehicleAmmoWarningTime;

//I know, not siege, but...
enum class tauntTypes_t
{
	TAUNT_TAUNT = 0,
	TAUNT_BOW,
	TAUNT_MEDITATE,
	TAUNT_FLOURISH,
	TAUNT_GLOAT
};
/*
===================
CG_PlaceString

Also called by scoreboard drawing
===================
*/
const char	*CG_PlaceString( int rank ) {
	static char	str[64];
	char	*s;
	const char	*t;
	// number extenstions, eg 1st, 2nd, 3rd, 4th etc.
	// note that the rules are different for french, but by changing the required strip strings they seem to work
	char sST[10];
	char sND[10];
	char sRD[10];
	char sTH[10];
	char sTiedFor[64];	// german is much longer, super safe...

	trap->SE_GetStringTextString("MP_INGAME_NUMBER_ST",sST, sizeof(sST) );
	trap->SE_GetStringTextString("MP_INGAME_NUMBER_ND",sND, sizeof(sND) );
	trap->SE_GetStringTextString("MP_INGAME_NUMBER_RD",sRD, sizeof(sRD) );
	trap->SE_GetStringTextString("MP_INGAME_NUMBER_TH",sTH, sizeof(sTH) );
	trap->SE_GetStringTextString("MP_INGAME_TIED_FOR" ,sTiedFor,sizeof(sTiedFor) );
	strcat(sTiedFor," ");	// save worrying about translators adding spaces or not

	if ( rank & RANK_TIED_FLAG ) {
		rank &= ~RANK_TIED_FLAG;
		t = sTiedFor;//"Tied for ";
	} else {
		t = "";
	}

	if ( rank == 1 ) {
		s = va("1%s",sST);//S_COLOR_BLUE "1st" S_COLOR_WHITE;		// draw in blue
	} else if ( rank == 2 ) {
		s = va("2%s",sND);//S_COLOR_RED "2nd" S_COLOR_WHITE;		// draw in red
	} else if ( rank == 3 ) {
		s = va("3%s",sRD);//S_COLOR_YELLOW "3rd" S_COLOR_WHITE;		// draw in yellow
	} else if ( rank == 11 ) {
		s = va("11%s",sTH);
	} else if ( rank == 12 ) {
		s = va("12%s",sTH);
	} else if ( rank == 13 ) {
		s = va("13%s",sTH);
	} else if ( rank % 10 == 1 ) {
		s = va("%i%s", rank,sST);
	} else if ( rank % 10 == 2 ) {
		s = va("%i%s", rank,sND);
	} else if ( rank % 10 == 3 ) {
		s = va("%i%s", rank,sRD);
	} else {
		s = va("%i%s", rank,sTH);
	}

	Com_sprintf( str, sizeof( str ), "%s%s", t, s );
	return str;
}

bool CG_ThereIsAMaster(void);

/*
=============
CG_Obituary
=============
*/
static void CG_Obituary( entityState_t *ent ) {
	meansOfDeath_t	mod;
	int			target, attacker;
	const char		*message;
	const char	*targetInfo;
	const char	*attackerInfo;
	char		targetName[32];
	char		attackerName[32];
	gender_t	gender;
	clientInfo_t	*ci;


	target = ent->otherEntityNum;
	attacker = ent->otherEntityNum2;
	mod = static_cast<meansOfDeath_t>( ent->eventParm );

	if ( target < 0 || target >= MAX_CLIENTS ) {
		trap->Error( errorParm_t::ERR_DROP, "CG_Obituary: target out of range" );
	}
	ci = &cgs.clientinfo[target];

	if ( attacker < 0 || attacker >= MAX_CLIENTS ) {
		attacker = ENTITYNUM_WORLD;
		attackerInfo = nullptr;
	} else {
		attackerInfo = CG_ConfigString( CS_PLAYERS + attacker );
	}

	targetInfo = CG_ConfigString( CS_PLAYERS + target );
	if ( !targetInfo ) {
		return;
	}
	Q_strncpyz( targetName, Info_ValueForKey( targetInfo, "n" ), sizeof(targetName) - 2);
	strcat( targetName, S_COLOR_WHITE );

	// check for single client messages

	switch( mod ) {
	case meansOfDeath_t::MOD_SUICIDE:
	case meansOfDeath_t::MOD_FALLING:
	case meansOfDeath_t::MOD_CRUSH:
	case meansOfDeath_t::MOD_WATER:
	case meansOfDeath_t::MOD_SLIME:
	case meansOfDeath_t::MOD_LAVA:
	case meansOfDeath_t::MOD_TRIGGER_HURT:
		message = "DIED_GENERIC";
		break;
	case meansOfDeath_t::MOD_TARGET_LASER:
		message = "DIED_LASER";
		break;
	default:
		message = nullptr;
		break;
	}

	// Attacker killed themselves.  Ridicule them for it.
	if (attacker == target) {
		gender = ci->gender;
		switch (mod) {
		case meansOfDeath_t::MOD_BRYAR_PISTOL:
		case meansOfDeath_t::MOD_BRYAR_PISTOL_ALT:
		case meansOfDeath_t::MOD_BLASTER:
		case meansOfDeath_t::MOD_TURBLAST:
		case meansOfDeath_t::MOD_DISRUPTOR:
		case meansOfDeath_t::MOD_DISRUPTOR_SPLASH:
		case meansOfDeath_t::MOD_DISRUPTOR_SNIPER:
		case meansOfDeath_t::MOD_BOWCASTER:
		case meansOfDeath_t::MOD_REPEATER:
		case meansOfDeath_t::MOD_REPEATER_ALT:
		case meansOfDeath_t::MOD_FLECHETTE:
			if ( gender == gender_t::GENDER_FEMALE )
				message = "SUICIDE_SHOT_FEMALE";
			else if ( gender == gender_t::GENDER_NEUTER )
				message = "SUICIDE_SHOT_GENDERLESS";
			else
				message = "SUICIDE_SHOT_MALE";
			break;
		case meansOfDeath_t::MOD_REPEATER_ALT_SPLASH:
		case meansOfDeath_t::MOD_FLECHETTE_ALT_SPLASH:
		case meansOfDeath_t::MOD_ROCKET:
		case meansOfDeath_t::MOD_ROCKET_SPLASH:
		case meansOfDeath_t::MOD_ROCKET_HOMING:
		case meansOfDeath_t::MOD_ROCKET_HOMING_SPLASH:
		case meansOfDeath_t::MOD_THERMAL:
		case meansOfDeath_t::MOD_THERMAL_SPLASH:
		case meansOfDeath_t::MOD_TRIP_MINE_SPLASH:
		case meansOfDeath_t::MOD_TIMED_MINE_SPLASH:
		case meansOfDeath_t::MOD_DET_PACK_SPLASH:
		case meansOfDeath_t::MOD_VEHICLE:
		case meansOfDeath_t::MOD_CONC:
		case meansOfDeath_t::MOD_CONC_ALT:
			if ( gender == gender_t::GENDER_FEMALE )
				message = "SUICIDE_EXPLOSIVES_FEMALE";
			else if ( gender == gender_t::GENDER_NEUTER )
				message = "SUICIDE_EXPLOSIVES_GENDERLESS";
			else
				message = "SUICIDE_EXPLOSIVES_MALE";
			break;
		case meansOfDeath_t::MOD_DEMP2:
			if ( gender == gender_t::GENDER_FEMALE )
				message = "SUICIDE_ELECTROCUTED_FEMALE";
			else if ( gender == gender_t::GENDER_NEUTER )
				message = "SUICIDE_ELECTROCUTED_GENDERLESS";
			else
				message = "SUICIDE_ELECTROCUTED_MALE";
			break;
		case meansOfDeath_t::MOD_FALLING:
			if ( gender == gender_t::GENDER_FEMALE )
				message = "SUICIDE_FALLDEATH_FEMALE";
			else if ( gender == gender_t::GENDER_NEUTER )
				message = "SUICIDE_FALLDEATH_GENDERLESS";
			else
				message = "SUICIDE_FALLDEATH_MALE";
			break;
		default:
			if ( gender == gender_t::GENDER_FEMALE )
				message = "SUICIDE_GENERICDEATH_FEMALE";
			else if ( gender == gender_t::GENDER_NEUTER )
				message = "SUICIDE_GENERICDEATH_GENDERLESS";
			else
				message = "SUICIDE_GENERICDEATH_MALE";
			break;
		}
	}

	if (target != attacker && target < MAX_CLIENTS && attacker < MAX_CLIENTS)
	{
		goto clientkilled;
	}

	if (message) {
		gender = ci->gender;

		if (!message[0])
		{
			if ( gender == gender_t::GENDER_FEMALE )
				message = "SUICIDE_GENERICDEATH_FEMALE";
			else if ( gender == gender_t::GENDER_NEUTER )
				message = "SUICIDE_GENERICDEATH_GENDERLESS";
			else
				message = "SUICIDE_GENERICDEATH_MALE";
		}
		message = (char *)CG_GetStringEdString("MP_INGAME", message);

		trap->Print( "%s %s\n", targetName, message);
		return;
	}

clientkilled:

	// check for kill messages from the current clientNum
	if ( attacker == cg.snap->ps.clientNum ) {
		const char	*s;

		if ( cgs.gametype < gametype_t::GT_TEAM && cgs.gametype != gametype_t::GT_DUEL && cgs.gametype != gametype_t::GT_POWERDUEL ) {
			if (cgs.gametype == gametype_t::GT_JEDIMASTER &&
				attacker < MAX_CLIENTS &&
				!ent->isJediMaster &&
				!cg.snap->ps.isJediMaster &&
				CG_ThereIsAMaster())
			{
				char part1[512];
				char part2[512];
				trap->SE_GetStringTextString("MP_INGAME_KILLED_MESSAGE", part1, sizeof(part1));
				trap->SE_GetStringTextString("MP_INGAME_JMKILLED_NOTJM", part2, sizeof(part2));
				s = va("%s %s\n%s\n", part1, targetName, part2);
			}
			else if (cgs.gametype == gametype_t::GT_JEDIMASTER &&
				attacker < MAX_CLIENTS &&
				!ent->isJediMaster &&
				!cg.snap->ps.isJediMaster)
			{ //no JM, saber must be out
				char part1[512];
				trap->SE_GetStringTextString("MP_INGAME_KILLED_MESSAGE", part1, sizeof(part1));
				/*
				kmsg1 = "for 0 points.\nGo for the saber!";
				strcpy(part2, kmsg1);

				s = va("%s %s %s\n", part1, targetName, part2);
				*/
				s = va("%s %s\n", part1, targetName);
			}
			else if (cgs.gametype == gametype_t::GT_POWERDUEL)
			{
				s = "";
			}
			else
			{
				char sPlaceWith[256];
				char sKilledStr[256];
				trap->SE_GetStringTextString("MP_INGAME_PLACE_WITH",     sPlaceWith, sizeof(sPlaceWith));
				trap->SE_GetStringTextString("MP_INGAME_KILLED_MESSAGE", sKilledStr, sizeof(sKilledStr));

				s = va("%s %s.\n%s %s %i.", sKilledStr, targetName,
					CG_PlaceString( cg.snap->ps.persistant[persEnum_t::PERS_RANK] + 1 ),
					sPlaceWith,
					cg.snap->ps.persistant[persEnum_t::PERS_SCORE] );
			}
		} else {
			char sKilledStr[256];
			trap->SE_GetStringTextString("MP_INGAME_KILLED_MESSAGE", sKilledStr, sizeof(sKilledStr));
			s = va("%s %s", sKilledStr, targetName );
		}
		//if (!(cg_singlePlayerActive.integer && cg_cameraOrbit.integer)) {
			CG_CenterPrint( s, SCREEN_HEIGHT * 0.30, BIGCHAR_WIDTH );
		//}
		// print the text message as well
	}

	// check for double client messages
	if ( !attackerInfo ) {
		attacker = ENTITYNUM_WORLD;
		strcpy( attackerName, "noname" );
	} else {
		Q_strncpyz( attackerName, Info_ValueForKey( attackerInfo, "n" ), sizeof(attackerName) - 2);
		strcat( attackerName, S_COLOR_WHITE );
		// check for kill messages about the current clientNum
		if ( target == cg.snap->ps.clientNum ) {
			Q_strncpyz( cg.killerName, attackerName, sizeof( cg.killerName ) );
		}
	}

	if ( attacker != ENTITYNUM_WORLD ) {
		switch (mod) {
		case meansOfDeath_t::MOD_STUN_BATON:
			message = "KILLED_STUN";
			break;
		case meansOfDeath_t::MOD_MELEE:
			message = "KILLED_MELEE";
			break;
		case meansOfDeath_t::MOD_SABER:
			message = "KILLED_SABER";
			break;
		case meansOfDeath_t::MOD_BRYAR_PISTOL:
		case meansOfDeath_t::MOD_BRYAR_PISTOL_ALT:
			message = "KILLED_BRYAR";
			break;
		case meansOfDeath_t::MOD_BLASTER:
			message = "KILLED_BLASTER";
			break;
		case meansOfDeath_t::MOD_TURBLAST:
			message = "KILLED_BLASTER";
			break;
		case meansOfDeath_t::MOD_DISRUPTOR:
		case meansOfDeath_t::MOD_DISRUPTOR_SPLASH:
			message = "KILLED_DISRUPTOR";
			break;
		case meansOfDeath_t::MOD_DISRUPTOR_SNIPER:
			message = "KILLED_DISRUPTORSNIPE";
			break;
		case meansOfDeath_t::MOD_BOWCASTER:
			message = "KILLED_BOWCASTER";
			break;
		case meansOfDeath_t::MOD_REPEATER:
			message = "KILLED_REPEATER";
			break;
		case meansOfDeath_t::MOD_REPEATER_ALT:
		case meansOfDeath_t::MOD_REPEATER_ALT_SPLASH:
			message = "KILLED_REPEATERALT";
			break;
		case meansOfDeath_t::MOD_DEMP2:
		case meansOfDeath_t::MOD_DEMP2_ALT:
			message = "KILLED_DEMP2";
			break;
		case meansOfDeath_t::MOD_FLECHETTE:
			message = "KILLED_FLECHETTE";
			break;
		case meansOfDeath_t::MOD_FLECHETTE_ALT_SPLASH:
			message = "KILLED_FLECHETTE_MINE";
			break;
		case meansOfDeath_t::MOD_ROCKET:
		case meansOfDeath_t::MOD_ROCKET_SPLASH:
			message = "KILLED_ROCKET";
			break;
		case meansOfDeath_t::MOD_ROCKET_HOMING:
		case meansOfDeath_t::MOD_ROCKET_HOMING_SPLASH:
			message = "KILLED_ROCKET_HOMING";
			break;
		case meansOfDeath_t::MOD_THERMAL:
		case meansOfDeath_t::MOD_THERMAL_SPLASH:
			message = "KILLED_THERMAL";
			break;
		case meansOfDeath_t::MOD_TRIP_MINE_SPLASH:
			message = "KILLED_TRIPMINE";
			break;
		case meansOfDeath_t::MOD_TIMED_MINE_SPLASH:
			message = "KILLED_TRIPMINE_TIMED";
			break;
		case meansOfDeath_t::MOD_DET_PACK_SPLASH:
			message = "KILLED_DETPACK";
			break;
		case meansOfDeath_t::MOD_VEHICLE:
		case meansOfDeath_t::MOD_CONC:
		case meansOfDeath_t::MOD_CONC_ALT:
			message = "KILLED_GENERIC";
			break;
		case meansOfDeath_t::MOD_FORCE_DARK:
			message = "KILLED_DARKFORCE";
			break;
		case meansOfDeath_t::MOD_SENTRY:
			message = "KILLED_SENTRY";
			break;
		case meansOfDeath_t::MOD_TELEFRAG:
			message = "KILLED_TELEFRAG";
			break;
		case meansOfDeath_t::MOD_CRUSH:
			message = "KILLED_GENERIC";//"KILLED_FORCETOSS";
			break;
		case meansOfDeath_t::MOD_FALLING:
			message = "KILLED_FORCETOSS";
			break;
		case meansOfDeath_t::MOD_TRIGGER_HURT:
			message = "KILLED_GENERIC";//"KILLED_FORCETOSS";
			break;
		default:
			message = "KILLED_GENERIC";
			break;
		}

		if (message) {
			message = (char *)CG_GetStringEdString("MP_INGAME", message);

			trap->Print( "%s %s %s\n",
				targetName, message, attackerName);
			return;
		}
	}

	// we don't know what it was
	trap->Print( "%s %s\n", targetName, (char *)CG_GetStringEdString("MP_INGAME", "DIED_GENERIC") );
}

//==========================================================================

void CG_ToggleBinoculars(centity_t *cent, int forceZoom)
{
	if (cent->currentState.number != cg.snap->ps.clientNum)
	{
		return;
	}

	if (cg.snap->ps.weaponstate != weaponstate_t::WEAPON_READY)
	{ //So we can't fool it and reactivate while switching to the saber or something.
		return;
	}

	/*
	if (cg.snap->ps.weapon == weapon_t::WP_SABER)
	{ //No.
		return;
	}
	*/

	if (forceZoom)
	{
		if (forceZoom == 2)
		{
			cg.snap->ps.zoomMode = 0;
		}
		else if (forceZoom == 1)
		{
			cg.snap->ps.zoomMode = 2;
		}
	}

	if (cg.snap->ps.zoomMode == 0)
	{
		trap->S_StartSound( nullptr, cg.snap->ps.clientNum, soundChannel_t::CHAN_AUTO, cgs.media.zoomStart );
	}
	else if (cg.snap->ps.zoomMode == 2)
	{
		trap->S_StartSound( nullptr, cg.snap->ps.clientNum, soundChannel_t::CHAN_AUTO, cgs.media.zoomEnd );
	}
}

//set the local timing bar
extern int cg_genericTimerBar;
extern int cg_genericTimerDur;
extern vec4_t cg_genericTimerColor;
void CG_LocalTimingBar(int startTime, int duration)
{
    cg_genericTimerBar = startTime + duration;
	cg_genericTimerDur = duration;

	cg_genericTimerColor[0] = 1.0f;
	cg_genericTimerColor[1] = 1.0f;
	cg_genericTimerColor[2] = 0.0f;
	cg_genericTimerColor[3] = 1.0f;
}

/*
===============
CG_UseItem
===============
*/
static void CG_UseItem( centity_t *cent ) {
	clientInfo_t *ci;
	holdable_t	item;
	int			clientNum;
	entityState_t *es;

	es = &cent->currentState;

	item = BG_UsedItem( BG_UnpackEvent( es->event ) );
	if ( item < holdable_t::HI_NONE || item > holdable_t::HI_NUM_HOLDABLE ) {
		item = holdable_t::HI_NONE;
	}

	// print a message if the local player
	if ( es->number == cg.snap->ps.clientNum ) {
		if ( item == holdable_t::HI_NONE ) {
			//CG_CenterPrint( "No item to use", SCREEN_HEIGHT * 0.30, BIGCHAR_WIDTH );
		}
	}

	switch ( item ) {
	default:
	case holdable_t::HI_NONE:
		//trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_BODY, cgs.media.useNothingSound );
		break;

	case holdable_t::HI_BINOCULARS:
		CG_ToggleBinoculars(cent, es->eventParm);
		break;

	case holdable_t::HI_SEEKER:
		trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.deploySeeker );
		break;

	case holdable_t::HI_SHIELD:
	case holdable_t::HI_SENTRY_GUN:
		break;

//	case HI_MEDKIT:
	case holdable_t::HI_MEDPAC:
	case holdable_t::HI_MEDPAC_BIG:
		clientNum = cent->currentState.clientNum;
		if ( clientNum >= 0 && clientNum < MAX_CLIENTS ) {
			ci = &cgs.clientinfo[ clientNum ];
			ci->medkitUsageTime = cg.time;
		}
		//Different sound for big bacta?
		trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.medkitSound );
		break;
	case holdable_t::HI_JETPACK:
		break; //Do something?
	case holdable_t::HI_HEALTHDISP:
		//CG_LocalTimingBar(cg.time, TOSS_DEBOUNCE_TIME);
		break;
	case holdable_t::HI_AMMODISP:
		//CG_LocalTimingBar(cg.time, TOSS_DEBOUNCE_TIME);
		break;
	case holdable_t::HI_EWEB:
		break;
	case holdable_t::HI_CLOAK:
		break; //Do something?
	}

	if (cg.snap && cg.snap->ps.clientNum == cent->currentState.number && item != holdable_t::HI_BINOCULARS &&
		item != holdable_t::HI_JETPACK && item != holdable_t::HI_HEALTHDISP && item != holdable_t::HI_AMMODISP && item != holdable_t::HI_CLOAK && item != holdable_t::HI_EWEB)
	{ //if not using binoculars/jetpack/dispensers/cloak, we just used that item up, so switch
		BG_CycleInven(&cg.snap->ps, 1);
		cg.itemSelect.reset(); //update the client-side selection display
	}
}


/*
================
CG_ItemPickup

A new item was picked up this frame
================
*/
static void CG_ItemPickup( int itemNum ) {
	cg.itemPickup = itemNum;
	cg.itemPickupTime = cg.time;
	cg.itemPickupBlendTime = cg.time;
	// see if it should be the grabbed weapon
	if ( cg.snap && bg_itemlist[itemNum].giType == itemType_t::IT_WEAPON ) {

		// 0 == no switching
		// 1 == automatically switch to best SAFE weapon
		// 2 == automatically switch to best weapon, safe or otherwise
		// 3 == if not saber, automatically switch to best weapon, safe or otherwise

		if (0 == cg_autoSwitch.integer)
		{
			// don't switch
		}
		else if ( cg_autoSwitch.integer == 1)
		{ //only autoselect if not explosive ("safe")
			if (GI_Weapon( bg_itemlist[itemNum] ) != weapon_t::WP_TRIP_MINE &&
				GI_Weapon( bg_itemlist[itemNum] ) != weapon_t::WP_DET_PACK &&
				GI_Weapon( bg_itemlist[itemNum] ) != weapon_t::WP_THERMAL &&
				GI_Weapon( bg_itemlist[itemNum] ) != weapon_t::WP_ROCKET_LAUNCHER &&
				GI_Weapon( bg_itemlist[itemNum] ) > cg.snap->ps.weapon &&
				cg.snap->ps.weapon != weapon_t::WP_SABER)
			{
				if (!cg.snap->ps.emplacedIndex)
				{
					cg.weaponSelectTime = cg.time;
				}
				cg.weaponSelect = GI_Weapon( bg_itemlist[itemNum] );
			}
		}
		else if ( cg_autoSwitch.integer == 2)
		{ //autoselect if better
			if (GI_Weapon( bg_itemlist[itemNum] ) > cg.snap->ps.weapon &&
				cg.snap->ps.weapon != weapon_t::WP_SABER)
			{
				if (!cg.snap->ps.emplacedIndex)
				{
					cg.weaponSelectTime = cg.time;
				}
				cg.weaponSelect = GI_Weapon( bg_itemlist[itemNum] );
			}
		}
		/*
		else if ( cg_autoswitch.integer == 3)
		{ //autoselect if better and not using the saber as a weapon
			if (GI_Weapon( bg_itemlist[itemNum] ) > cg.snap->ps.weapon &&
				cg.snap->ps.weapon != weapon_t::WP_SABER)
			{
				if (!cg.snap->ps.emplacedIndex)
				{
					cg.weaponSelectTime = cg.time;
				}
				cg.weaponSelect = GI_Weapon( bg_itemlist[itemNum] );
			}
		}
		*/
		//No longer required - just not switching ever if using saber
	}

	//rww - print pickup messages
	if (bg_itemlist[itemNum].classname && bg_itemlist[itemNum].classname[0] &&
		(bg_itemlist[itemNum].giType != itemType_t::IT_TEAM || (GI_Powerup( bg_itemlist[itemNum] ) != powerup_t::PW_REDFLAG && GI_Powerup( bg_itemlist[itemNum] ) != powerup_t::PW_BLUEFLAG)) )
	{ //don't print messages for flags, they have their own pickup event broadcasts
		char	text[1024];
		char	upperKey[1024];

		strcpy(upperKey, bg_itemlist[itemNum].classname);

		if ( trap->SE_GetStringTextString( va("SP_INGAME_%s",Q_strupr(upperKey)), text, sizeof( text )))
		{
			Com_Printf("%s %s\n", CG_GetStringEdString("MP_INGAME", "PICKUPLINE"), text);
		}
		else
		{
			Com_Printf("%s %s\n", CG_GetStringEdString("MP_INGAME", "PICKUPLINE"), bg_itemlist[itemNum].classname);
		}
	}
}


/*
================
CG_PainEvent

Also called by playerstate transition
================
*/
void CG_PainEvent( centity_t *cent, int health ) {
	const char	*snd;

	// don't do more than two pain sounds a second
	if ( cg.time - cent->pe.painTime < 500 ) {
		return;
	}

	if ( health < 25 ) {
		snd = "*pain25.wav";
	} else if ( health < 50 ) {
		snd = "*pain50.wav";
	} else if ( health < 75 ) {
		snd = "*pain75.wav";
	} else {
		snd = "*pain100.wav";
	}
	trap->S_StartSound( nullptr, cent->currentState.number, soundChannel_t::CHAN_VOICE,
		CG_CustomSound( cent->currentState.number, snd ) );

	// save pain time for programitic twitch animation
	cent->pe.painTime = cg.time;
	cent->pe.painDirection	^= 1;
}

extern bool BG_GetRootSurfNameWithVariant( void *ghoul2, const char *rootSurfName, char *returnSurfName, int returnSize );
void CG_ReattachLimb(centity_t *source)
{
	clientInfo_t *ci = nullptr;

	if ( source->currentState.number >= MAX_CLIENTS )
	{
		ci = source->npcClient;
	}
	else
	{
		ci = &cgs.clientinfo[source->currentState.number];
	}
	if ( ci )
	{//re-apply the skin
		if ( ci->torsoSkin > 0 )
		{
			trap->G2API_SetSkin(source->ghoul2,0,ci->torsoSkin,ci->torsoSkin);
		}
	}

	/*
	char *limbName;
	char *stubCapName;
	int i = g2ModelParts_t::G2_MODELPART_HEAD;

	//rww NOTE: Assumes G2_MODELPART_HEAD is first and G2_MODELPART_RLEG is last
	while (i <= g2ModelParts_t::G2_MODELPART_RLEG)
	{
		if (source->torsoBolt & (1 << (i-10)))
		{
			switch (i)
			{
			case g2ModelParts_t::G2_MODELPART_HEAD:
				limbName = "head";
				stubCapName = "torso_cap_head";
				break;
			case g2ModelParts_t::G2_MODELPART_WAIST:
				limbName = "torso";
				stubCapName = "hips_cap_torso";
				break;
			case g2ModelParts_t::G2_MODELPART_LARM:
				limbName = "l_arm";
				stubCapName = "torso_cap_l_arm";
				break;
			case g2ModelParts_t::G2_MODELPART_RARM:
				limbName = "r_arm";
				stubCapName = "torso_cap_r_arm";
				break;
			case g2ModelParts_t::G2_MODELPART_RHAND:
				limbName = "r_hand";
				stubCapName = "r_arm_cap_r_hand";
				break;
			case g2ModelParts_t::G2_MODELPART_LLEG:
				limbName = "l_leg";
				stubCapName = "hips_cap_l_leg";
				break;
			case g2ModelParts_t::G2_MODELPART_RLEG:
				limbName = "r_leg";
				stubCapName = "hips_cap_r_leg";
				break;
			default:
				source->torsoBolt = 0;
				source->ghoul2weapon = nullptr;
				return;
			}

			trap->G2API_SetSurfaceOnOff(source->ghoul2, limbName, 0);
			trap->G2API_SetSurfaceOnOff(source->ghoul2, stubCapName, 0x00000100);
		}
		i++;
	}
	*/
	source->torsoBolt = 0;

	source->ghoul2weapon = nullptr;
}

const char *CG_TeamName(team_t team)
{
	if (team==team_t::TEAM_RED)
		return "RED";
	else if (team==team_t::TEAM_BLUE)
		return "BLUE";
	else if (team==team_t::TEAM_SPECTATOR)
		return "SPECTATOR";
	return "FREE";
}

void CG_PrintCTFMessage(clientInfo_t *ci, const char *teamName, ctfMsg_t ctfMessage)
{
	char printMsg[1024];
	const char *refName = nullptr;
	const char *psStringEDString = nullptr;

	switch (ctfMessage)
	{
	case ctfMsg_t::CTFMESSAGE_FRAGGED_FLAG_CARRIER:
		refName = "FRAGGED_FLAG_CARRIER";
		break;
	case ctfMsg_t::CTFMESSAGE_FLAG_RETURNED:
		refName = "FLAG_RETURNED";
		break;
	case ctfMsg_t::CTFMESSAGE_PLAYER_RETURNED_FLAG:
		refName = "PLAYER_RETURNED_FLAG";
		break;
	case ctfMsg_t::CTFMESSAGE_PLAYER_CAPTURED_FLAG:
		refName = "PLAYER_CAPTURED_FLAG";
		break;
	case ctfMsg_t::CTFMESSAGE_PLAYER_GOT_FLAG:
		refName = "PLAYER_GOT_FLAG";
		break;
	default:
		return;
	}

	psStringEDString = CG_GetStringEdString("MP_INGAME", refName);

	if (!psStringEDString || !psStringEDString[0])
	{
		return;
	}

	if (teamName && teamName[0])
	{
		const char *f = strstr(psStringEDString, "%s");

		if (f)
		{
			int strLen = 0;
			int i = 0;

			if (ci)
			{
				Com_sprintf(printMsg, sizeof(printMsg), "%s^7 ", ci->name);
				strLen = strlen(printMsg);
			}

			while (psStringEDString[i] && i < 512)
			{
				if (psStringEDString[i] == '%' &&
					psStringEDString[i+1] == 's')
				{
					printMsg[strLen] = '\0';
					Q_strcat(printMsg, sizeof(printMsg), teamName);
					strLen = strlen(printMsg);

					i++;
				}
				else
				{
					printMsg[strLen] = psStringEDString[i];
					strLen++;
				}

				i++;
			}

			printMsg[strLen] = '\0';

			goto doPrint;
		}
	}

	if (ci)
	{
		Com_sprintf(printMsg, sizeof(printMsg), "%s^7 %s", ci->name, psStringEDString);
	}
	else
	{
		Com_sprintf(printMsg, sizeof(printMsg), "%s", psStringEDString);
	}

doPrint:
	Com_Printf("%s\n", printMsg);
}

void CG_GetCTFMessageEvent(entityState_t *es)
{
	int clIndex = es->trickedentindex;
	int teamIndex = es->trickedentindex2;
	clientInfo_t *ci = nullptr;
	const char *teamName = nullptr;

	if (clIndex < MAX_CLIENTS)
	{
		ci = &cgs.clientinfo[clIndex];
	}

	if (teamIndex < 50)
	{
		teamName = CG_TeamName(static_cast<team_t>( teamIndex ));
	}

	if (!ci)
	{
		return;
	}

	CG_PrintCTFMessage(ci, teamName, static_cast<ctfMsg_t>( es->eventParm ));
}

bool BG_InKnockDownOnly( animNumber_t anim );

void DoFall(centity_t *cent, entityState_t *es, int clientNum)
{
	int delta = es->eventParm;

	if (Any( cent->currentState.eFlags & entityFlags_t::EF_DEAD ))
	{ //corpses crack into the ground ^_^
		if (delta > 25)
		{
			trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.fallSound );
		}
		else
		{
			trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, trap->S_RegisterSound( "sound/movers/objects/objectHit.wav" ) );
		}
	}
	else if (BG_InKnockDownOnly(ES_LegsAnim( *es )))
	{
		if (delta > 14)
		{
			trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.fallSound );
		}
		else
		{
			trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, trap->S_RegisterSound( "sound/movers/objects/objectHit.wav" ) );
		}
	}
	else if (delta > 50)
	{
		trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.fallSound );
		trap->S_StartSound( nullptr, cent->currentState.number, soundChannel_t::CHAN_VOICE,
			CG_CustomSound( cent->currentState.number, "*land1.wav" ) );
		cent->pe.painTime = cg.time;	// don't play a pain sound right after this
	}
	else if (delta > 44)
	{
		trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.fallSound );
		trap->S_StartSound( nullptr, cent->currentState.number, soundChannel_t::CHAN_VOICE,
			CG_CustomSound( cent->currentState.number, "*land1.wav" ) );
		cent->pe.painTime = cg.time;	// don't play a pain sound right after this
	}
	else
	{
		trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.landSound );
	}

	if ( clientNum == cg.predictedPlayerState.clientNum )
	{
		// smooth landing z changes
		cg.landChange = -delta;
		if (cg.landChange > 32)
		{
			cg.landChange = 32;
		}
		if (cg.landChange < -32)
		{
			cg.landChange = -32;
		}
		cg.landTime = cg.time;
	}
}

int CG_InClientBitflags(entityState_t *ent, int client)
{
	int checkIn;
	int sub = 0;

	if (client > 47)
	{
		checkIn = ent->trickedentindex4;
		sub = 48;
	}
	else if (client > 31)
	{
		checkIn = ent->trickedentindex3;
		sub = 32;
	}
	else if (client > 15)
	{
		checkIn = ent->trickedentindex2;
		sub = 16;
	}
	else
	{
		checkIn = ent->trickedentindex;
	}

	if (checkIn & (1 << (client-sub)))
	{
		return 1;
	}

	return 0;
}

void CG_PlayDoorLoopSound( centity_t *cent );
void CG_PlayDoorSound( centity_t *cent, int type );

void CG_TryPlayCustomSound( vec3_t origin, int entityNum, soundChannel_t channel, const char *soundName )
{
	sfxHandle_t cSound = CG_CustomSound(entityNum, soundName);

	if (cSound <= 0)
	{
		return;
	}

	trap->S_StartSound(origin, entityNum, channel, cSound);
}

void CG_G2MarkEvent(entityState_t *es)
{
	//es->origin should be the hit location of the projectile,
	//whereas es->origin2 is the predicted position of the
	//projectile. (based on the trajectory upon impact) -rww
	centity_t *pOwner = &cg_entities[es->otherEntityNum];
	vec3_t startPoint;
	float	size = 0.0f;
	qhandle_t shader = 0;

	if (!pOwner->ghoul2)
	{ //can't do anything then...
		return;
	}

	//es->eventParm being non-0 means to do a special trace check
	//first. This will give us an impact right at the surface to
	//project the mark on. Typically this is used for radius
	//explosions and such, where the source position could be
	//way outside of model space.
	if (es->eventParm)
	{
		trace_t tr;
		int ignore = ENTITYNUM_NONE;

		CG_G2Trace(&tr, es->origin, nullptr, nullptr, es->origin2, ignore, MASK_PLAYERSOLID);

		if (tr.entityNum != es->otherEntityNum)
		{ //try again if we hit an ent but not the one we wanted.
			//CG_TestLine(es->origin, es->origin2, 2000, 0x0000ff, 1);
			if (tr.entityNum < ENTITYNUM_WORLD)
			{
				ignore = tr.entityNum;
				CG_G2Trace(&tr, es->origin, nullptr, nullptr, es->origin2, ignore, MASK_PLAYERSOLID);
				if (tr.entityNum != es->otherEntityNum)
				{ //try extending the trace a bit.. or not
					/*
					vec3_t v;

					VectorSubtract(es->origin2, es->origin, v);
					VectorScale(v, 64.0f, v);
					VectorAdd(es->origin2, v, es->origin2);

					CG_G2Trace(&tr, es->origin, nullptr, nullptr, es->origin2, ignore, MASK_PLAYERSOLID);
					if (tr.entityNum != es->otherEntityNum)
					{
						return;
					}
					*/
					//didn't manage to collide with the desired person. No mark will be placed then.
					return;
				}
			}
		}

		//otherwise we now have a valid starting point.
		VectorCopy(tr.endpos, startPoint);
	}
	else
	{
		VectorCopy(es->origin, startPoint);
	}

	if ( Any(es->eFlags&entityFlags_t::EF_JETPACK_ACTIVE) )
	{// a vehicle weapon, make it a larger size mark
		//OR base this on the size of the thing you hit?
		if ( g_vehWeaponInfo[es->otherEntityNum2].fG2MarkSize )
		{
			size = Q_flrand( 0.6f, 1.4f )*g_vehWeaponInfo[es->otherEntityNum2].fG2MarkSize;
		}
		else
		{
			size = Q_flrand( 32.0f, 72.0f );
		}
		//specify mark shader in vehWeapon file
		if ( g_vehWeaponInfo[es->otherEntityNum2].iG2MarkShaderHandle )
		{//have one we want to use instead of defaults
			shader = g_vehWeaponInfo[es->otherEntityNum2].iG2MarkShaderHandle;
		}
	}
	switch(es->weapon)
	{
	case weapon_t::WP_BRYAR_PISTOL:
	case weapon_t::WP_CONCUSSION:
	case weapon_t::WP_BRYAR_OLD:
	case weapon_t::WP_BLASTER:
	case weapon_t::WP_DISRUPTOR:
	case weapon_t::WP_BOWCASTER:
	case weapon_t::WP_REPEATER:
	case weapon_t::WP_TURRET:
		if ( !size )
		{
			size = 4.0f;
		}
		if ( !shader )
		{
			shader = cgs.media.bdecal_bodyburn1;
		}
		CG_AddGhoul2Mark(shader, size,
			startPoint, es->origin2, es->owner, pOwner->lerpOrigin,
			pOwner->lerpAngles[YAW], pOwner->ghoul2,
			pOwner->modelScale, Q_irand(10000, 20000));
		break;
	case weapon_t::WP_ROCKET_LAUNCHER:
	case weapon_t::WP_THERMAL:
		if ( !size )
		{
			size = 24.0f;
		}
		if ( !shader )
		{
			shader = cgs.media.bdecal_burn1;
		}
		CG_AddGhoul2Mark(shader, size,
			startPoint, es->origin2, es->owner, pOwner->lerpOrigin,
			pOwner->lerpAngles[YAW], pOwner->ghoul2,
			pOwner->modelScale, Q_irand(10000, 20000));
		break;
		/*
	case weapon_t::WP_FLECHETTE:
		CG_AddGhoul2Mark(cgs.media.bdecal_bodyburn1, flrand(0.5f, 1.0f),
			startPoint, es->origin2, es->owner, pOwner->lerpOrigin,
			pOwner->lerpAngles[YAW], pOwner->ghoul2,
			pOwner->modelScale);
		break;
		*/
		//Issues with small scale?
	default:
		break;
	}
}

void CG_CalcVehMuzzle(Vehicle_t *pVeh, centity_t *ent, int muzzleNum)
{
	mdxaBone_t boltMatrix;
	vec3_t	vehAngles;

	assert(pVeh);

	if (pVeh->m_iMuzzleTime[muzzleNum] == cg.time)
	{ //already done for this frame, don't need to do it again
		return;
	}
	//Uh... how about we set this, hunh...?  :)
	pVeh->m_iMuzzleTime[muzzleNum] = cg.time;

	VectorCopy( ent->lerpAngles, vehAngles );
	if ( pVeh->m_pVehicleInfo )
	{
		if (pVeh->m_pVehicleInfo->type == vehicleType_t::VH_ANIMAL
			 ||pVeh->m_pVehicleInfo->type == vehicleType_t::VH_WALKER)
		{
			vehAngles[PITCH] = vehAngles[ROLL] = 0.0f;
		}
		else if (pVeh->m_pVehicleInfo->type == vehicleType_t::VH_SPEEDER)
		{
			vehAngles[PITCH] = 0.0f;
		}
	}
	trap->G2API_GetBoltMatrix_NoRecNoRot(ent->ghoul2, 0, pVeh->m_iMuzzleTag[muzzleNum], &boltMatrix, vehAngles,
		ent->lerpOrigin, cg.time, nullptr, ent->modelScale);
	BG_GiveMeVectorFromMatrix(&boltMatrix, orientations_t::ORIGIN, pVeh->m_vMuzzlePos[muzzleNum]);
	BG_GiveMeVectorFromMatrix(&boltMatrix, orientations_t::NEGATIVE_Y, pVeh->m_vMuzzleDir[muzzleNum]);
}

//corresponds to G_VehMuzzleFireFX -rww
void CG_VehMuzzleFireFX(centity_t *veh, entityState_t *broadcaster)
{
	Vehicle_t *pVeh = veh->m_pVehicle;
	int curMuz = 0, muzFX = 0;

	if (!pVeh || !veh->ghoul2)
	{
		return;
	}

	for ( curMuz = 0; curMuz < MAX_VEHICLE_MUZZLES; curMuz++ )
	{//go through all muzzles and
		if ( pVeh->m_iMuzzleTag[curMuz] != -1//valid muzzle bolt
			&& (broadcaster->trickedentindex&(1<<curMuz)) )//fired
		{//this muzzle fired
			muzFX = 0;
			if ( pVeh->m_pVehicleInfo->weapMuzzle[curMuz] == 0 )
			{//no weaopon for this muzzle?  check turrets
				int i, j;
				for ( i = 0; i < MAX_VEHICLE_TURRETS; i++ )
				{
					for ( j = 0; j < MAX_VEHICLE_TURRETS; j++ )
					{
						if ( pVeh->m_pVehicleInfo->turret[i].iMuzzle[j]-1 == curMuz )
						{//this muzzle belongs to this turret
							muzFX = g_vehWeaponInfo[pVeh->m_pVehicleInfo->turret[i].iWeapon].iMuzzleFX;
							break;
						}
					}
				}
			}
			else
			{
				muzFX = g_vehWeaponInfo[pVeh->m_pVehicleInfo->weapMuzzle[curMuz]].iMuzzleFX;
			}
			if ( muzFX )
			{
				//CG_CalcVehMuzzle(pVeh, veh, curMuz);
				//trap->FX_PlayEffectID(muzFX, pVeh->m_vMuzzlePos[curMuz], pVeh->m_vMuzzleDir[curMuz], -1, -1, false);
				trap->FX_PlayBoltedEffectID(muzFX, veh->currentState.origin, veh->ghoul2, pVeh->m_iMuzzleTag[curMuz], veh->currentState.number, 0, 0, true);
			}
		}
	}
}

const char	*cg_stringEdVoiceChatTable[MAX_CUSTOM_SIEGE_SOUNDS] =
{
	"VC_ATT",//"*att_attack",
	"VC_ATT_PRIMARY",//"*att_primary",
	"VC_ATT_SECONDARY",//"*att_second",
	"VC_DEF_GUNS",//"*def_guns",
	"VC_DEF_POSITION",//"*def_position",
	"VC_DEF_PRIMARY",//"*def_primary",
	"VC_DEF_SECONDARY",//"*def_second",
	"VC_REPLY_COMING",//"*reply_coming",
	"VC_REPLY_GO",//"*reply_go",
	"VC_REPLY_NO",//"*reply_no",
	"VC_REPLY_STAY",//"*reply_stay",
	"VC_REPLY_YES",//"*reply_yes",
	"VC_REQ_ASSIST",//"*req_assist",
	"VC_REQ_DEMO",//"*req_demo",
	"VC_REQ_HVY",//"*req_hvy",
	"VC_REQ_MEDIC",//"*req_medic",
	"VC_REQ_SUPPLY",//"*req_sup",
	"VC_REQ_TECH",//"*req_tech",
	"VC_SPOT_AIR",//"*spot_air",
	"VC_SPOT_DEF",//"*spot_defenses",
	"VC_SPOT_EMPLACED",//"*spot_emplaced",
	"VC_SPOT_SNIPER",//"*spot_sniper",
	"VC_SPOT_TROOP",//"*spot_troops",
	"VC_TAC_COVER",//"*tac_cover",
	"VC_TAC_FALLBACK",//"*tac_fallback",
	"VC_TAC_FOLLOW",//"*tac_follow",
	"VC_TAC_HOLD",//"*tac_hold",
	"VC_TAC_SPLIT",//"*tac_split",
	"VC_TAC_TOGETHER",//"*tac_together",
	nullptr
};

//stupid way of figuring out what string to use for voice chats
const char *CG_GetStringForVoiceSound(const char *s)
{
	int i = 0;
	while (i < MAX_CUSTOM_SIEGE_SOUNDS)
	{
		if (bg_customSiegeSoundNames[i] &&
			!Q_stricmp(bg_customSiegeSoundNames[i], s))
		{ //get the matching reference name
			assert(cg_stringEdVoiceChatTable[i]);
			return CG_GetStringEdString("MENUS", (char *)cg_stringEdVoiceChatTable[i]);
		}
		i++;
	}

	return "voice chat";
}

/*
==============
CG_EntityEvent

An entity has an event value
also called by CG_CheckPlayerstateEvents
==============
*/
#define	DEBUGNAME(x) if(cg_debugEvents.integer){trap->Print(x"\n");}
extern void CG_ChatBox_AddString(char *chatStr); //cg_draw.cpp
void CG_EntityEvent( centity_t *cent, vec3_t position ) {
	entityState_t	*es;
	entity_event_t	event;
	vec3_t			dir;
	const char		*s;
	int				clientNum;
	int				eID = 0;
	int				isnd = 0;
	centity_t		*cl_ent;

	es = &cent->currentState;
	event = BG_UnpackEvent( es->event );

	if ( cg_debugEvents.integer ) {
		trap->Print( "ent:%3i  event:%3i ", es->number, to_underlying( event ) );
	}

	if ( event == entity_event_t::EV_NONE ) {
		DEBUGNAME("ZEROEVENT");
		return;
	}

	clientNum = es->clientNum;
	if ( clientNum < 0 || clientNum >= MAX_CLIENTS ) {
		clientNum = 0;
	}

	if (es->eType == entityType_t::ET_NPC)
	{
		clientNum = es->number;

		if (!cent->npcClient)
		{
			CG_CreateNPCClient(&cent->npcClient); //allocate memory for it

			if (!cent->npcClient)
			{
				assert(0);
				return;
			}

			memset(cent->npcClient, 0, sizeof(clientInfo_t));
			cent->npcClient->ghoul2Model = nullptr;
		}

		assert( cent->npcClient );
	}

	switch ( event ) {
	//
	// movement generated events
	//
	case entity_event_t::EV_CLIENTJOIN:
		DEBUGNAME("EV_CLIENTJOIN");

		//Slight hack to force a local reinit of client entity on join.
		cl_ent = &cg_entities[es->eventParm];

		if (cl_ent)
		{
			//cl_ent->torsoBolt = 0;
			cl_ent->bolt1 = 0;
			cl_ent->bolt2 = 0;
			cl_ent->bolt3 = 0;
			cl_ent->bolt4 = 0;
			cl_ent->bodyHeight = 0;//SABER_LENGTH_MAX;
			//cl_ent->saberExtendTime = 0;
			cl_ent->boltInfo = 0;
			cl_ent->frame_minus1_refreshed = 0;
			cl_ent->frame_minus2_refreshed = 0;
			cl_ent->frame_hold_time = 0;
			cl_ent->frame_hold_refreshed = 0;
			cl_ent->trickAlpha = 0;
			cl_ent->trickAlphaTime = 0;
			cl_ent->ghoul2weapon = nullptr;
			cl_ent->weapon = weapon_t::WP_NONE;
			cl_ent->teamPowerEffectTime = 0;
			cl_ent->teamPowerType = 0;
			cl_ent->numLoopingSounds = 0;
			//cl_ent->localAnimIndex = 0;
		}
		break;

	case entity_event_t::EV_FOOTSTEP:
		DEBUGNAME("EV_FOOTSTEP");
		if (cg_footsteps.integer) {
			footstep_t	soundType;
			switch( static_cast<surfaceMaterial_t>( es->eventParm ) )
			{
			case surfaceMaterial_t::MATERIAL_MUD:
				soundType = footstep_t::FOOTSTEP_MUDWALK;
				break;
			case surfaceMaterial_t::MATERIAL_DIRT:
				soundType = footstep_t::FOOTSTEP_DIRTWALK;
				break;
			case surfaceMaterial_t::MATERIAL_SAND:
				soundType = footstep_t::FOOTSTEP_SANDWALK;
				break;
			case surfaceMaterial_t::MATERIAL_SNOW:
				soundType = footstep_t::FOOTSTEP_SNOWWALK;
				break;
			case surfaceMaterial_t::MATERIAL_SHORTGRASS:
			case surfaceMaterial_t::MATERIAL_LONGGRASS:
				soundType = footstep_t::FOOTSTEP_GRASSWALK;
				break;
			case surfaceMaterial_t::MATERIAL_SOLIDMETAL:
				soundType = footstep_t::FOOTSTEP_METALWALK;
				break;
			case surfaceMaterial_t::MATERIAL_HOLLOWMETAL:
				soundType = footstep_t::FOOTSTEP_PIPEWALK;
				break;
			case surfaceMaterial_t::MATERIAL_GRAVEL:
				soundType = footstep_t::FOOTSTEP_GRAVELWALK;
				break;
			case surfaceMaterial_t::MATERIAL_CARPET:
			case surfaceMaterial_t::MATERIAL_FABRIC:
			case surfaceMaterial_t::MATERIAL_CANVAS:
			case surfaceMaterial_t::MATERIAL_RUBBER:
			case surfaceMaterial_t::MATERIAL_PLASTIC:
				soundType = footstep_t::FOOTSTEP_RUGWALK;
				break;
			case surfaceMaterial_t::MATERIAL_SOLIDWOOD:
			case surfaceMaterial_t::MATERIAL_HOLLOWWOOD:
				soundType = footstep_t::FOOTSTEP_WOODWALK;
				break;

			default:
				soundType = footstep_t::FOOTSTEP_STONEWALK;
				break;
			}

			trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_BODY, cgs.media.footsteps[ soundType ][Q_irand( 0, 3 )] );
		}
		break;
	case entity_event_t::EV_FOOTSTEP_METAL:
		DEBUGNAME("EV_FOOTSTEP_METAL");
		if (cg_footsteps.integer) {
			trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_BODY,
				cgs.media.footsteps[ footstep_t::FOOTSTEP_METALWALK ][Q_irand( 0, 3 )] );
		}
		break;
	case entity_event_t::EV_FOOTSPLASH:
		DEBUGNAME("EV_FOOTSPLASH");
		if (cg_footsteps.integer) {
			trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_BODY,
				cgs.media.footsteps[ footstep_t::FOOTSTEP_SPLASH ][Q_irand( 0, 3 )] );
		}
		break;
	case entity_event_t::EV_FOOTWADE:
		DEBUGNAME("EV_FOOTWADE");
		if (cg_footsteps.integer) {
			trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_BODY,
				cgs.media.footsteps[ footstep_t::FOOTSTEP_SPLASH ][Q_irand( 0, 3 )] );
		}
		break;
	case entity_event_t::EV_SWIM:
		DEBUGNAME("EV_SWIM");
		if (cg_footsteps.integer) {
			trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_BODY,
				cgs.media.footsteps[ footstep_t::FOOTSTEP_SPLASH ][Q_irand( 0, 3 )] );
		}
		break;


	case entity_event_t::EV_FALL:
		DEBUGNAME("EV_FALL");
		if (es->number == cg.snap->ps.clientNum && cg.snap->ps.fallingToDeath)
		{
			break;
		}
		DoFall(cent, es, clientNum);
		break;
	case entity_event_t::EV_STEP_4:
	case entity_event_t::EV_STEP_8:
	case entity_event_t::EV_STEP_12:
	case entity_event_t::EV_STEP_16:		// smooth out step up transitions
		DEBUGNAME("EV_STEP");
	{
		float	oldStep;
		int		delta;
		int		step;

		if ( clientNum != cg.predictedPlayerState.clientNum ) {
			break;
		}
		// if we are interpolating, we don't need to smooth steps
		if ( cg.demoPlayback || Any(cg.snap->ps.pm_flags & pmoveFlags_t::PMF_FOLLOW) ||
			cg_noPredict.integer ) {
			break;
		}
		// check for stepping up before a previous step is completed
		delta = cg.time - cg.stepTime;
		if (delta < STEP_TIME) {
			oldStep = cg.stepChange * (STEP_TIME - delta) / STEP_TIME;
		} else {
			oldStep = 0;
		}

		// add this amount
		step = 4 * (event - entity_event_t::EV_STEP_4 + 1 );
		cg.stepChange = oldStep + step;
		if ( cg.stepChange > MAX_STEP_CHANGE ) {
			cg.stepChange = MAX_STEP_CHANGE;
		}
		cg.stepTime = cg.time;
		break;
	}

	case entity_event_t::EV_JUMP_PAD:
		DEBUGNAME("EV_JUMP_PAD");
		break;

	case entity_event_t::EV_GHOUL2_MARK:
		DEBUGNAME("EV_GHOUL2_MARK");

		if (cg_ghoul2Marks.integer)
		{ //Can we put a burn mark on him?
			CG_G2MarkEvent(es);
		}
		break;

	case entity_event_t::EV_GLOBAL_DUEL:
		DEBUGNAME("EV_GLOBAL_DUEL");
		//used for beginning of power duels
		//if (cg.predictedPlayerState.persistant[persEnum_t::PERS_TEAM] != team_t::TEAM_SPECTATOR)
		if (es->otherEntityNum == cg.predictedPlayerState.clientNum ||
			es->otherEntityNum2 == cg.predictedPlayerState.clientNum ||
			es->groundEntityNum == cg.predictedPlayerState.clientNum)
		{
			CG_CenterPrint( CG_GetStringEdString("MP_SVGAME", "BEGIN_DUEL"), 120, GIANTCHAR_WIDTH*2 );
			trap->S_StartLocalSound( cgs.media.countFightSound, soundChannel_t::CHAN_ANNOUNCER );
		}
		break;

	case entity_event_t::EV_PRIVATE_DUEL:
		DEBUGNAME("EV_PRIVATE_DUEL");

		if (cg.snap->ps.clientNum != es->number)
		{
			break;
		}

		if (es->eventParm)
		{ //starting the duel
			if (es->eventParm == 2)
			{
				CG_CenterPrint( CG_GetStringEdString("MP_SVGAME", "BEGIN_DUEL"), 120, GIANTCHAR_WIDTH*2 );
				trap->S_StartLocalSound( cgs.media.countFightSound, soundChannel_t::CHAN_ANNOUNCER );
			}
			else
			{
				trap->S_StartBackgroundTrack( "music/mp/duel.mp3", "music/mp/duel.mp3", false );
			}
		}
		else
		{ //ending the duel
			CG_StartMusic(true);
		}
		break;

	case entity_event_t::EV_JUMP:
		DEBUGNAME("EV_JUMP");
		if (cg_jumpSounds.integer)
		{
			trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_VOICE, CG_CustomSound( es->number, "*jump1.wav" ) );
		}
		break;
	case entity_event_t::EV_ROLL:
		DEBUGNAME("EV_ROLL");
		if (es->number == cg.snap->ps.clientNum && cg.snap->ps.fallingToDeath)
		{
			break;
		}
		if (es->eventParm)
		{ //fall-roll-in-one event
			DoFall(cent, es, clientNum);
		}

		trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_VOICE, CG_CustomSound( es->number, "*jump1.wav" ) );
		trap->S_StartSound( nullptr, es->number, soundChannel_t::CHAN_BODY, cgs.media.rollSound  );

		//FIXME: need some sort of body impact on ground sound and maybe kick up some dust?
		break;

	case entity_event_t::EV_TAUNT:
		DEBUGNAME("EV_TAUNT");
		{
			int soundIndex = 0;

			if ( cg_noTaunt.integer )
				break;

			if ( cgs.gametype != gametype_t::GT_DUEL
				&& cgs.gametype != gametype_t::GT_POWERDUEL
				&& static_cast<tauntTypes_t>( es->eventParm ) == tauntTypes_t::TAUNT_TAUNT )
			{//normal taunt
				soundIndex = CG_CustomSound( es->number, "*taunt.wav" );
			}
			else
			{
				switch ( static_cast<tauntTypes_t>( es->eventParm ) )
				{
				case tauntTypes_t::TAUNT_TAUNT:
				default:
					if ( Q_irand( 0, 1 ) )
					{
						soundIndex = CG_CustomSound( es->number, va("*anger%d.wav", Q_irand(1,3)) );
					}
					else
					{
						soundIndex = CG_CustomSound( es->number, va("*taunt%d.wav", Q_irand(1,3)) );
						if ( !soundIndex )
						{
							soundIndex = CG_CustomSound( es->number, va("*anger%d.wav", Q_irand(1,3)) );
						}
					}
					break;
				case tauntTypes_t::TAUNT_BOW:
					//soundIndex = CG_CustomSound( es->number, va("*respect%d.wav", Q_irand(1,3)) );
					break;
				case tauntTypes_t::TAUNT_MEDITATE:
					//soundIndex = CG_CustomSound( es->number, va("*meditate%d.wav", Q_irand(1,3)) );
					break;
				case tauntTypes_t::TAUNT_FLOURISH:
					if ( Q_irand( 0, 1 ) )
					{
						soundIndex = CG_CustomSound( es->number, va("*deflect%d.wav", Q_irand(1,3)) );
						if ( !soundIndex )
						{
							soundIndex = CG_CustomSound( es->number, va("*gloat%d.wav", Q_irand(1,3)) );
							if ( !soundIndex )
							{
								soundIndex = CG_CustomSound( es->number, va("*anger%d.wav", Q_irand(1,3)) );
							}
						}
					}
					else
					{
						soundIndex = CG_CustomSound( es->number, va("*gloat%d.wav", Q_irand(1,3)) );
						if ( !soundIndex )
						{
							soundIndex = CG_CustomSound( es->number, va("*deflect%d.wav", Q_irand(1,3)) );
							if ( !soundIndex )
							{
								soundIndex = CG_CustomSound( es->number, va("*anger%d.wav", Q_irand(1,3)) );
							}
						}
					}
					break;
				case tauntTypes_t::TAUNT_GLOAT:
					soundIndex = CG_CustomSound( es->number, va("*victory%d.wav", Q_irand(1,3)) );
					break;
				}
			}
			if ( !soundIndex )
			{
				soundIndex = CG_CustomSound( es->number, "*taunt.wav" );
			}
			if ( soundIndex )
			{
				trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_VOICE, soundIndex );
			}
		}
		break;

		//Begin NPC sounds
	case entity_event_t::EV_ANGER1:	//Say when acquire an enemy when didn't have one before
	case entity_event_t::EV_ANGER2:
	case entity_event_t::EV_ANGER3:
		DEBUGNAME("EV_ANGERx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*anger%i.wav", event - entity_event_t::EV_ANGER1 + 1) );
		break;

	case entity_event_t::EV_VICTORY1:	//Say when killed an enemy
	case entity_event_t::EV_VICTORY2:
	case entity_event_t::EV_VICTORY3:
		DEBUGNAME("EV_VICTORYx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*victory%i.wav", event - entity_event_t::EV_VICTORY1 + 1) );
		break;

	case entity_event_t::EV_CONFUSE1:	//Say when confused
	case entity_event_t::EV_CONFUSE2:
	case entity_event_t::EV_CONFUSE3:
		DEBUGNAME("EV_CONFUSEDx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*confuse%i.wav", event - entity_event_t::EV_CONFUSE1 + 1) );
		break;

	case entity_event_t::EV_PUSHED1:	//Say when pushed
	case entity_event_t::EV_PUSHED2:
	case entity_event_t::EV_PUSHED3:
		DEBUGNAME("EV_PUSHEDx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*pushed%i.wav", event - entity_event_t::EV_PUSHED1 + 1) );
		break;

	case entity_event_t::EV_CHOKE1:	//Say when choking
	case entity_event_t::EV_CHOKE2:
	case entity_event_t::EV_CHOKE3:
		DEBUGNAME("EV_CHOKEx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*choke%i.wav", event - entity_event_t::EV_CHOKE1 + 1) );
		break;

	case entity_event_t::EV_FFWARN:	//Warn ally to stop shooting you
		DEBUGNAME("EV_FFWARN");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, "*ffwarn.wav" );
		break;

	case entity_event_t::EV_FFTURN:	//Turn on ally after being shot by them
		DEBUGNAME("EV_FFTURN");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, "*ffturn.wav" );
		break;

	//extra sounds for ST
	case entity_event_t::EV_CHASE1:
	case entity_event_t::EV_CHASE2:
	case entity_event_t::EV_CHASE3:
		DEBUGNAME("EV_CHASEx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*chase%i.wav", event - entity_event_t::EV_CHASE1 + 1) );
		break;
	case entity_event_t::EV_COVER1:
	case entity_event_t::EV_COVER2:
	case entity_event_t::EV_COVER3:
	case entity_event_t::EV_COVER4:
	case entity_event_t::EV_COVER5:
		DEBUGNAME("EV_COVERx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*cover%i.wav", event - entity_event_t::EV_COVER1 + 1) );
		break;
	case entity_event_t::EV_DETECTED1:
	case entity_event_t::EV_DETECTED2:
	case entity_event_t::EV_DETECTED3:
	case entity_event_t::EV_DETECTED4:
	case entity_event_t::EV_DETECTED5:
		DEBUGNAME("EV_DETECTEDx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*detected%i.wav", event - entity_event_t::EV_DETECTED1 + 1) );
		break;
	case entity_event_t::EV_GIVEUP1:
	case entity_event_t::EV_GIVEUP2:
	case entity_event_t::EV_GIVEUP3:
	case entity_event_t::EV_GIVEUP4:
		DEBUGNAME("EV_GIVEUPx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*giveup%i.wav", event - entity_event_t::EV_GIVEUP1 + 1) );
		break;
	case entity_event_t::EV_LOOK1:
	case entity_event_t::EV_LOOK2:
		DEBUGNAME("EV_LOOKx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*look%i.wav", event - entity_event_t::EV_LOOK1 + 1) );
		break;
	case entity_event_t::EV_LOST1:
		DEBUGNAME("EV_LOST1");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, "*lost1.wav" );
		break;
	case entity_event_t::EV_OUTFLANK1:
	case entity_event_t::EV_OUTFLANK2:
		DEBUGNAME("EV_OUTFLANKx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*outflank%i.wav", event - entity_event_t::EV_OUTFLANK1 + 1) );
		break;
	case entity_event_t::EV_ESCAPING1:
	case entity_event_t::EV_ESCAPING2:
	case entity_event_t::EV_ESCAPING3:
		DEBUGNAME("EV_ESCAPINGx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*escaping%i.wav", event - entity_event_t::EV_ESCAPING1 + 1) );
		break;
	case entity_event_t::EV_SIGHT1:
	case entity_event_t::EV_SIGHT2:
	case entity_event_t::EV_SIGHT3:
		DEBUGNAME("EV_SIGHTx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*sight%i.wav", event - entity_event_t::EV_SIGHT1 + 1) );
		break;
	case entity_event_t::EV_SOUND1:
	case entity_event_t::EV_SOUND2:
	case entity_event_t::EV_SOUND3:
		DEBUGNAME("EV_SOUNDx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*sound%i.wav", event - entity_event_t::EV_SOUND1 + 1) );
		break;
	case entity_event_t::EV_SUSPICIOUS1:
	case entity_event_t::EV_SUSPICIOUS2:
	case entity_event_t::EV_SUSPICIOUS3:
	case entity_event_t::EV_SUSPICIOUS4:
	case entity_event_t::EV_SUSPICIOUS5:
		DEBUGNAME("EV_SUSPICIOUSx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*suspicious%i.wav", event - entity_event_t::EV_SUSPICIOUS1 + 1) );
		break;
	//extra sounds for Jedi
	case entity_event_t::EV_COMBAT1:
	case entity_event_t::EV_COMBAT2:
	case entity_event_t::EV_COMBAT3:
		DEBUGNAME("EV_COMBATx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*combat%i.wav", event - entity_event_t::EV_COMBAT1 + 1) );
		break;
	case entity_event_t::EV_JDETECTED1:
	case entity_event_t::EV_JDETECTED2:
	case entity_event_t::EV_JDETECTED3:
		DEBUGNAME("EV_JDETECTEDx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*jdetected%i.wav", event - entity_event_t::EV_JDETECTED1 + 1) );
		break;
	case entity_event_t::EV_TAUNT1:
	case entity_event_t::EV_TAUNT2:
	case entity_event_t::EV_TAUNT3:
		DEBUGNAME("EV_TAUNTx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*taunt%i.wav", event - entity_event_t::EV_TAUNT1 + 1) );
		break;
	case entity_event_t::EV_JCHASE1:
	case entity_event_t::EV_JCHASE2:
	case entity_event_t::EV_JCHASE3:
		DEBUGNAME("EV_JCHASEx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*jchase%i.wav", event - entity_event_t::EV_JCHASE1 + 1) );
		break;
	case entity_event_t::EV_JLOST1:
	case entity_event_t::EV_JLOST2:
	case entity_event_t::EV_JLOST3:
		DEBUGNAME("EV_JLOSTx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*jlost%i.wav", event - entity_event_t::EV_JLOST1 + 1) );
		break;
	case entity_event_t::EV_DEFLECT1:
	case entity_event_t::EV_DEFLECT2:
	case entity_event_t::EV_DEFLECT3:
		DEBUGNAME("EV_DEFLECTx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*deflect%i.wav", event - entity_event_t::EV_DEFLECT1 + 1) );
		break;
	case entity_event_t::EV_GLOAT1:
	case entity_event_t::EV_GLOAT2:
	case entity_event_t::EV_GLOAT3:
		DEBUGNAME("EV_GLOATx");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, va("*gloat%i.wav", event - entity_event_t::EV_GLOAT1 + 1) );
		break;
	case entity_event_t::EV_PUSHFAIL:
		DEBUGNAME("EV_PUSHFAIL");
		CG_TryPlayCustomSound( nullptr, es->number, soundChannel_t::CHAN_VOICE, "*pushfail.wav" );
		break;
		//End NPC sounds

	case entity_event_t::EV_SIEGESPEC:
		DEBUGNAME("EV_SIEGESPEC");
		if ( es->owner == cg.predictedPlayerState.clientNum )
		{
			cg_siegeDeathTime = es->time;
		}

		break;

	case entity_event_t::EV_WATER_TOUCH:
		DEBUGNAME("EV_WATER_TOUCH");
		trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.watrInSound );
		break;
	case entity_event_t::EV_WATER_LEAVE:
		DEBUGNAME("EV_WATER_LEAVE");
		trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.watrOutSound );
		break;
	case entity_event_t::EV_WATER_UNDER:
		DEBUGNAME("EV_WATER_UNDER");
		trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.watrUnSound );
		break;
	case entity_event_t::EV_WATER_CLEAR:
		DEBUGNAME("EV_WATER_CLEAR");
		trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, CG_CustomSound( es->number, "*gasp.wav" ) );
		break;

	case entity_event_t::EV_ITEM_PICKUP:
		DEBUGNAME("EV_ITEM_PICKUP");
		{
			gitem_t	*item;
			int		index;
			bool	newindex = false;

			index = cg_entities[es->eventParm].currentState.modelindex;		// player predicted

			if (index < 1 && cg_entities[es->eventParm].currentState.isJediMaster)
			{ //a holocron most likely
				const forcePowers_t power = static_cast<forcePowers_t>( cg_entities[es->eventParm].currentState.trickedentindex4 );
				trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO,	cgs.media.holocronPickup );

				if (es->number == cg.snap->ps.clientNum && showPowersName[power])
				{
					const char *strText = CG_GetStringEdString("MP_INGAME", "PICKUPLINE");

					//Com_Printf("%s %s\n", strText, showPowersName[power]);
					CG_CenterPrint( va("%s %s\n", strText, CG_GetStringEdString("SP_INGAME",showPowersName[power])), SCREEN_HEIGHT * 0.30, BIGCHAR_WIDTH );
				}

				//Show the player their force selection bar in case picking the holocron up changed the current selection
				if (power != forcePowers_t::FP_SABER_OFFENSE && power != forcePowers_t::FP_SABER_DEFENSE && power != forcePowers_t::FP_SABERTHROW &&
					power != forcePowers_t::FP_LEVITATION &&
					es->number == cg.snap->ps.clientNum &&
					(power == cg.snap->ps.fd.forcePowerSelected || !(cg.snap->ps.fd.forcePowersActive & Bit( cg.snap->ps.fd.forcePowerSelected ))))
				{
					if (cg.forceSelect != power)
					{
						cg.forceSelect = power;
						newindex = true;
					}
				}

				if (es->number == cg.snap->ps.clientNum && newindex)
				{
					if (cg.forceSelectTime < cg.time)
					{
						cg.forceSelectTime = cg.time;
					}
				}

				break;
			}

			if (cg_entities[es->eventParm].pickupDebounceTime >= cg.time)
			{ //rww - an unfortunately necessary hack to prevent double item pickups
				break;
			}

			//Hopefully even if this entity is somehow removed and replaced with, say, another
			//item, this time will have expired by the time that item needs to be picked up.
			//Of course, it's quite possible this will fail miserably, so if you've got a better
			//solution then please do use it.
			cg_entities[es->eventParm].pickupDebounceTime = cg.time+500;

			if ( index < 1 || index >= bg_numItems ) {
				break;
			}
			item = &bg_itemlist[ index ];

			if ( /*item->giType != itemType_t::IT_POWERUP && */item->giType != itemType_t::IT_TEAM) {
				if (item->pickup_sound && item->pickup_sound[0])
				{
					trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO,	trap->S_RegisterSound( item->pickup_sound ) );
				}
			}

			// show icon and name on status bar
			if ( es->number == cg.snap->ps.clientNum ) {
				CG_ItemPickup( index );
			}
		}
		break;

	case entity_event_t::EV_GLOBAL_ITEM_PICKUP:
		DEBUGNAME("EV_GLOBAL_ITEM_PICKUP");
		{
			gitem_t	*item;
			int		index;

			index = es->eventParm;		// player predicted

			if ( index < 1 || index >= bg_numItems ) {
				break;
			}
			item = &bg_itemlist[ index ];
			// powerup pickups are global
			if( item->pickup_sound && item->pickup_sound[0] ) {
				trap->S_StartSound (nullptr, cg.snap->ps.clientNum, soundChannel_t::CHAN_AUTO, trap->S_RegisterSound( item->pickup_sound) );
			}

			// show icon and name on status bar
			if ( es->number == cg.snap->ps.clientNum ) {
				CG_ItemPickup( index );
			}
		}
		break;

	case entity_event_t::EV_VEH_FIRE:
		DEBUGNAME("EV_VEH_FIRE");
		{
			centity_t *veh = &cg_entities[es->owner];
			CG_VehMuzzleFireFX(veh, es);
		}
		break;

	//
	// weapon events
	//
	case entity_event_t::EV_NOAMMO:
		DEBUGNAME("EV_NOAMMO");
//		trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.noAmmoSound );
		if ( es->number == cg.snap->ps.clientNum )
		{
			if ( CG_InFighter() || CG_InATST() || cg.snap->ps.weapon == weapon_t::WP_NONE )
			{//just letting us know our vehicle is out of ammo
				//FIXME: flash something on HUD or give some message so we know we have no ammo
				centity_t *localCent = &cg_entities[cg.snap->ps.clientNum];
				if ( localCent->m_pVehicle
					&& localCent->m_pVehicle->m_pVehicleInfo
					&& localCent->m_pVehicle->m_pVehicleInfo->weapon[es->eventParm].soundNoAmmo )
				{//play the "no Ammo" sound for this weapon
					trap->S_StartSound (nullptr, cg.snap->ps.clientNum, soundChannel_t::CHAN_AUTO, localCent->m_pVehicle->m_pVehicleInfo->weapon[es->eventParm].soundNoAmmo );
				}
				else
				{//play the default "no ammo" sound
					trap->S_StartSound (nullptr, cg.snap->ps.clientNum, soundChannel_t::CHAN_AUTO, cgs.media.noAmmoSound );
				}
				//flash the HUD so they associate the sound with the visual indicator that they don't have enough ammo
				if ( cg_vehicleAmmoWarningTime < cg.time
					|| cg_vehicleAmmoWarning != es->eventParm )
				{//if there's already one going, don't interrupt it (unless they tried to fire another weapon that's out of ammo)
					cg_vehicleAmmoWarning = es->eventParm;
					cg_vehicleAmmoWarningTime = cg.time+500;
				}
			}
			else if ( cg.snap->ps.weapon == weapon_t::WP_SABER )
			{
				cg.forceHUDTotalFlashTime = cg.time + 1000;
			}
			else
			{
				weapon_t weap = weapon_t::WP_NONE;

				if (es->eventParm && es->eventParm < to_underlying( weapon_t::WP_NUM_WEAPONS ))
				{
					cg.snap->ps.stats[statIndex_t::STAT_WEAPONS] &= ~(1 << es->eventParm);
					weap = cg.snap->ps.weapon;
				}
				else if (es->eventParm)
				{
					weap = static_cast<weapon_t>( es->eventParm - to_underlying( weapon_t::WP_NUM_WEAPONS ) );
				}
				CG_OutOfAmmoChange(weap);
			}
		}
		break;
	case entity_event_t::EV_CHANGE_WEAPON:
		DEBUGNAME("EV_CHANGE_WEAPON");
		{
			const weapon_t weapon = static_cast<weapon_t>( es->eventParm );
			weaponInfo_t *weaponInfo;

			assert(weapon >= weapon_t::WP_NONE && weapon < weapon_t::WP_NUM_WEAPONS);

			weaponInfo = &cg_weapons[weapon];

			assert(weaponInfo);

			if (weaponInfo->selectSound)
			{
				trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, weaponInfo->selectSound );
			}
			else if (weapon != weapon_t::WP_SABER)
			{ //not sure what SP is doing for this but I don't want a select sound for saber (it has the saber-turn-on)
				trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.selectSound );
			}
		}
		break;
	case entity_event_t::EV_FIRE_WEAPON:
		DEBUGNAME("EV_FIRE_WEAPON");
		if (cent->currentState.number >= MAX_CLIENTS && cent->currentState.eType != entityType_t::ET_NPC)
		{ //special case for turret firing
			vec3_t gunpoint, gunangle;
			mdxaBone_t matrix;

			weaponInfo_t *weaponInfo = &cg_weapons[weapon_t::WP_TURRET];

			if ( !weaponInfo->registered )
			{
				CG_RegisterWeapon(weapon_t::WP_TURRET);
			}

			if (cent->ghoul2)
			{
				if (!cent->bolt1)
				{
					cent->bolt1 = trap->G2API_AddBolt(cent->ghoul2, 0, "*flash01");
				}
				if (!cent->bolt2)
				{
					cent->bolt2 = trap->G2API_AddBolt(cent->ghoul2, 0, "*flash02");
				}
				trap->G2API_SetBoneAnim(cent->ghoul2, 0, "Bone02", 1, 4, boneFlags_t::BONE_ANIM_OVERRIDE_FREEZE|boneFlags_t::BONE_ANIM_BLEND,
					1.0f, cg.time, -1, 300);
			}
			else
			{
				break;
			}

			if (cent->currentState.eventParm)
			{
				trap->G2API_GetBoltMatrix(cent->ghoul2, 0, cent->bolt2, &matrix, cent->currentState.angles, cent->currentState.origin, cg.time, cgs.gameModels, cent->modelScale);
			}
			else
			{
				trap->G2API_GetBoltMatrix(cent->ghoul2, 0, cent->bolt1, &matrix, cent->currentState.angles, cent->currentState.origin, cg.time, cgs.gameModels, cent->modelScale);
			}

			gunpoint[0] = matrix.matrix[0][3];
			gunpoint[1] = matrix.matrix[1][3];
			gunpoint[2] = matrix.matrix[2][3];

			gunangle[0] = -matrix.matrix[0][0];
			gunangle[1] = -matrix.matrix[1][0];
			gunangle[2] = -matrix.matrix[2][0];

			trap->FX_PlayEffectID(cgs.effects.mEmplacedMuzzleFlash, gunpoint, gunangle, -1, -1, false);
		}
		else if (cent->currentState.weapon != weapon_t::WP_EMPLACED_GUN || cent->currentState.eType == entityType_t::ET_NPC)
		{
			if (cent->currentState.eType == entityType_t::ET_NPC &&
				cent->currentState.NPC_class == class_t::CLASS_VEHICLE &&
				cent->m_pVehicle)
			{ //vehicles do nothing for clientside weapon fire events.. at least for now.
				break;
			}
			CG_FireWeapon( cent, false );
		}
		break;

	case entity_event_t::EV_ALT_FIRE:
		DEBUGNAME("EV_ALT_FIRE");

		if (cent->currentState.weapon == weapon_t::WP_EMPLACED_GUN)
		{ //don't do anything for emplaced stuff
			break;
		}

		if (cent->currentState.eType == entityType_t::ET_NPC &&
			cent->currentState.NPC_class == class_t::CLASS_VEHICLE &&
			cent->m_pVehicle)
		{ //vehicles do nothing for clientside weapon fire events.. at least for now.
			break;
		}

		CG_FireWeapon( cent, true );

		//if you just exploded your detpacks and you have no ammo left for them, autoswitch
		if ( cg.snap->ps.clientNum == cent->currentState.number &&
			cg.snap->ps.weapon == weapon_t::WP_DET_PACK )
		{
			if (cg.snap->ps.ammo[weaponData[weapon_t::WP_DET_PACK].ammoIndex] == 0)
			{
				CG_OutOfAmmoChange(weapon_t::WP_DET_PACK);
			}
		}

		break;

	case entity_event_t::EV_SABER_ATTACK:
		DEBUGNAME("EV_SABER_ATTACK");
		{
			qhandle_t swingSound = trap->S_RegisterSound(va("sound/weapons/saber/saberhup%i.wav", Q_irand(1, 8)));
			clientInfo_t *client = nullptr;
			if ( cg_entities[es->number].currentState.eType == entityType_t::ET_NPC )
			{
				client = cg_entities[es->number].npcClient;
			}
			else if ( es->number < MAX_CLIENTS )
			{
				client = &cgs.clientinfo[es->number];
			}
			if ( client && client->infoValid && client->saber[0].swingSound[0] )
			{//custom swing sound
				swingSound = client->saber[0].swingSound[Q_irand(0,2)];
			}
            trap->S_StartSound(es->pos.trBase, es->number, soundChannel_t::CHAN_WEAPON, swingSound );
		}
		break;

	case entity_event_t::EV_SABER_HIT:
		DEBUGNAME("EV_SABER_HIT");
		{
			int hitPersonFxID = cgs.effects.mSaberBloodSparks;
			int hitPersonSmallFxID = cgs.effects.mSaberBloodSparksSmall;
			int hitPersonMidFxID = cgs.effects.mSaberBloodSparksMid;
			int hitOtherFxID = cgs.effects.mSaberCut;
			int hitSound = trap->S_RegisterSound(va("sound/weapons/saber/saberhit%i.wav", Q_irand(1, 3)));

			if ( es->otherEntityNum2 >= 0
				&& es->otherEntityNum2 < ENTITYNUM_NONE )
			{//we have a specific person who is causing this effect, see if we should override it with any custom saber effects/sounds
				clientInfo_t *client = nullptr;
				if ( cg_entities[es->otherEntityNum2].currentState.eType == entityType_t::ET_NPC )
				{
					client = cg_entities[es->otherEntityNum2].npcClient;
				}
				else if ( es->otherEntityNum2 < MAX_CLIENTS )
				{
					client = &cgs.clientinfo[es->otherEntityNum2];
				}
				if ( client && client->infoValid )
				{
					int saberNum = ES_WeaponParm( *es );
					int bladeNum = es->legsAnim;
					if ( WP_SaberBladeUseSecondBladeStyle( &client->saber[saberNum], bladeNum ) )
					{//use second blade style values
						if ( client->saber[saberNum].hitPersonEffect2 )
						{//custom hit person effect
							hitPersonFxID = hitPersonSmallFxID = hitPersonMidFxID = client->saber[saberNum].hitPersonEffect2;
						}
						if ( client->saber[saberNum].hitOtherEffect2 )
						{//custom hit other effect
							hitOtherFxID = client->saber[saberNum].hitOtherEffect2;
						}
						if ( client->saber[saberNum].hit2Sound[0] )
						{//custom hit sound
							hitSound = client->saber[saberNum].hit2Sound[Q_irand(0,2)];
						}
					}
					else
					{//use first blade style values
						if ( client->saber[saberNum].hitPersonEffect )
						{//custom hit person effect
							hitPersonFxID = hitPersonSmallFxID = hitPersonMidFxID = client->saber[saberNum].hitPersonEffect;
						}
						if ( client->saber[saberNum].hitOtherEffect )
						{//custom hit other effect
							hitOtherFxID = client->saber[0].hitOtherEffect;
						}
						if ( client->saber[saberNum].hitSound[0] )
						{//custom hit sound
							hitSound = client->saber[saberNum].hitSound[Q_irand(0,2)];
						}
					}
				}
			}

			if (es->eventParm == 16)
			{ //Make lots of sparks, something special happened
				vec3_t fxDir;
				VectorCopy(es->angles, fxDir);
				if (!fxDir[0] && !fxDir[1] && !fxDir[2])
				{
					fxDir[1] = 1;
				}
				trap->S_StartSound(es->origin, es->number, soundChannel_t::CHAN_AUTO, hitSound );
				trap->FX_PlayEffectID( hitPersonFxID, es->origin, fxDir, -1, -1, false );
				trap->FX_PlayEffectID( hitPersonFxID, es->origin, fxDir, -1, -1, false );
				trap->FX_PlayEffectID( hitPersonFxID, es->origin, fxDir, -1, -1, false );
				trap->FX_PlayEffectID( hitPersonFxID, es->origin, fxDir, -1, -1, false );
				trap->FX_PlayEffectID( hitPersonFxID, es->origin, fxDir, -1, -1, false );
				trap->FX_PlayEffectID( hitPersonFxID, es->origin, fxDir, -1, -1, false );
			}
			else if (es->eventParm)
			{ //hit a person
				vec3_t fxDir;
				VectorCopy(es->angles, fxDir);
				if (!fxDir[0] && !fxDir[1] && !fxDir[2])
				{
					fxDir[1] = 1;
				}
				trap->S_StartSound(es->origin, es->number, soundChannel_t::CHAN_AUTO, hitSound );
				if ( es->eventParm == 3 )
				{	// moderate or big hits.
					trap->FX_PlayEffectID( hitPersonSmallFxID, es->origin, fxDir, -1, -1, false );
				}
				else if ( es->eventParm == 2 )
				{	// this is for really big hits.
					trap->FX_PlayEffectID( hitPersonMidFxID, es->origin, fxDir, -1, -1, false );
				}
				else
				{	// this should really just be done in the effect itself, no?
					trap->FX_PlayEffectID( hitPersonFxID, es->origin, fxDir, -1, -1, false );
					trap->FX_PlayEffectID( hitPersonFxID, es->origin, fxDir, -1, -1, false );
					trap->FX_PlayEffectID( hitPersonFxID, es->origin, fxDir, -1, -1, false );
				}
			}
			else
			{ //hit something else
				vec3_t fxDir;
				VectorCopy(es->angles, fxDir);
				if (!fxDir[0] && !fxDir[1] && !fxDir[2])
				{
					fxDir[1] = 1;
				}
				//old jk2mp method
				/*
				trap->S_StartSound(es->origin, es->number, soundChannel_t::CHAN_AUTO, trap->S_RegisterSound("sound/weapons/saber/saberhit.wav"));
				trap->FX_PlayEffectID( trap->FX_RegisterEffect("saber/spark.efx"), es->origin, fxDir, -1, -1, false );
				*/

				trap->FX_PlayEffectID( hitOtherFxID, es->origin, fxDir, -1, -1, false );
			}

			//rww - this means we have the number of the ent being hit and the ent that owns the saber doing
			//the hit. This being the case, we can store these indecies and the current time in order to do
			//some visual tricks on the client between frames to make it look like we're actually continuing
			//to hit between server frames.
			if (es->otherEntityNum != ENTITYNUM_NONE && es->otherEntityNum2 != ENTITYNUM_NONE)
			{
				centity_t *saberOwner;

				saberOwner = &cg_entities[es->otherEntityNum2];

				saberOwner->serverSaberHitIndex = es->otherEntityNum;
				saberOwner->serverSaberHitTime = cg.time;

				if (es->eventParm)
				{
					saberOwner->serverSaberFleshImpact = true;
				}
				else
				{
					saberOwner->serverSaberFleshImpact = false;
				}
			}
		}
		break;

	case entity_event_t::EV_SABER_BLOCK:
		DEBUGNAME("EV_SABER_BLOCK");
		{
			if (es->eventParm)
			{ //saber block
				int			blockFXID = cgs.effects.mSaberBlock;
				qhandle_t	blockSound = trap->S_RegisterSound(va( "sound/weapons/saber/saberblock%d.wav", Q_irand(1, 9) ));
				bool	noFlare = false;

				if ( es->otherEntityNum2 >= 0
					&& es->otherEntityNum2 < ENTITYNUM_NONE )
				{//we have a specific person who is causing this effect, see if we should override it with any custom saber effects/sounds
					clientInfo_t *client = nullptr;
					if ( cg_entities[es->otherEntityNum2].currentState.eType == entityType_t::ET_NPC )
					{
						client = cg_entities[es->otherEntityNum2].npcClient;
					}
					else if ( es->otherEntityNum2 < MAX_CLIENTS )
					{
						client = &cgs.clientinfo[es->otherEntityNum2];
					}
					if ( client && client->infoValid )
					{
						int saberNum = ES_WeaponParm( *es );
						int bladeNum = es->legsAnim;
						if ( WP_SaberBladeUseSecondBladeStyle( &client->saber[saberNum], bladeNum ) )
						{//use second blade style values
							if ( client->saber[saberNum].blockEffect2 )
							{//custom saber block effect
								blockFXID = client->saber[saberNum].blockEffect2;
							}
							if ( client->saber[saberNum].block2Sound[0] )
							{//custom hit sound
								blockSound = client->saber[saberNum].block2Sound[Q_irand(0,2)];
							}
						}
						else
						{
							if ( client->saber[saberNum].blockEffect )
							{//custom saber block effect
								blockFXID = client->saber[saberNum].blockEffect;
							}
							if ( client->saber[saberNum].blockSound[0] )
							{//custom hit sound
								blockSound = client->saber[saberNum].blockSound[Q_irand(0,2)];
							}
						}
						if ( Any( client->saber[saberNum].saberFlags2 & saberFlags2_t::SFL2_NO_CLASH_FLARE ) )
						{
							noFlare = true;
						}
					}
				}

				{
					vec3_t fxDir;

					VectorCopy(es->angles, fxDir);
					if (!fxDir[0] && !fxDir[1] && !fxDir[2])
					{
						fxDir[1] = 1;
					}
					trap->S_StartSound(es->origin, es->number, soundChannel_t::CHAN_AUTO, blockSound );
					trap->FX_PlayEffectID( blockFXID, es->origin, fxDir, -1, -1, false );

					if ( !noFlare )
					{
						cg_saberFlashTime = cg.time-50;
						VectorCopy( es->origin, cg_saberFlashPos );
					}
				}
			}
			else
			{ //projectile block
				vec3_t fxDir;
				VectorCopy(es->angles, fxDir);
				if (!fxDir[0] && !fxDir[1] && !fxDir[2])
				{
					fxDir[1] = 1;
				}
				trap->FX_PlayEffectID(cgs.effects.mBlasterDeflect, es->origin, fxDir, -1, -1, false);
			}
		}
		break;

	case entity_event_t::EV_SABER_CLASHFLARE:
		DEBUGNAME("EV_SABER_CLASHFLARE");
		{
			cg_saberFlashTime = cg.time-50;
			VectorCopy( es->origin, cg_saberFlashPos );
			trap->S_StartSound ( es->origin, -1, soundChannel_t::CHAN_WEAPON, trap->S_RegisterSound( va("sound/weapons/saber/saberhitwall%i", Q_irand(1, 3)) ) );
		}
		break;

	case entity_event_t::EV_SABER_UNHOLSTER:
		DEBUGNAME("EV_SABER_UNHOLSTER");
		{
			clientInfo_t *ci = nullptr;

			if (es->eType == entityType_t::ET_NPC)
			{
				ci = cg_entities[es->number].npcClient;
			}
			else if (es->number < MAX_CLIENTS)
			{
				ci = &cgs.clientinfo[es->number];
			}

			if (ci)
			{
				if (ci->saber[0].soundOn)
				{
					trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, ci->saber[0].soundOn );
				}
				if (ci->saber[1].soundOn)
				{
					trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, ci->saber[1].soundOn );
				}
			}
		}
		break;

	case entity_event_t::EV_BECOME_JEDIMASTER:
		DEBUGNAME("EV_SABER_UNHOLSTER");
		{
			trace_t tr;
			vec3_t playerMins = {-15, -15, DEFAULT_MINS_2+8};
			vec3_t playerMaxs = {15, 15, DEFAULT_MAXS_2};
			vec3_t ang, pos, dpos;

			VectorClear(ang);
			ang[ROLL] = 1;

			VectorCopy(position, dpos);
			dpos[2] -= 4096;

			CG_Trace(&tr, position, playerMins, playerMaxs, dpos, es->number, MASK_SOLID);
			VectorCopy(tr.endpos, pos);

			if (tr.fraction == 1)
			{
				break;
			}
			trap->FX_PlayEffectID(cgs.effects.mJediSpawn, pos, ang, -1, -1, false);

			trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, trap->S_RegisterSound( "sound/weapons/saber/saberon.wav" ) );

			if (cg.snap->ps.clientNum == es->number)
			{
				trap->S_StartLocalSound(cgs.media.happyMusic, soundChannel_t::CHAN_LOCAL);
				CGCam_SetMusicMult(0.3f, 5000);
			}
		}
		break;

	case entity_event_t::EV_DISRUPTOR_MAIN_SHOT:
		DEBUGNAME("EV_DISRUPTOR_MAIN_SHOT");
		if (cent->currentState.eventParm != cg.snap->ps.clientNum ||
			cg.renderingThirdPerson)
		{ //h4q3ry
			CG_GetClientWeaponMuzzleBoltPoint(cent->currentState.eventParm, cent->currentState.origin2);
		}
		else
		{
			if (cg.lastFPFlashPoint[0] ||cg.lastFPFlashPoint[1] || cg.lastFPFlashPoint[2])
			{ //get the position of the muzzle flash for the first person weapon model from the last frame
				VectorCopy(cg.lastFPFlashPoint, cent->currentState.origin2);
			}
		}
		FX_DisruptorMainShot( cent->currentState.origin2, cent->lerpOrigin );
		break;

	case entity_event_t::EV_DISRUPTOR_SNIPER_SHOT:
		DEBUGNAME("EV_DISRUPTOR_SNIPER_SHOT");
		if (cent->currentState.eventParm != cg.snap->ps.clientNum ||
			cg.renderingThirdPerson)
		{ //h4q3ry
			CG_GetClientWeaponMuzzleBoltPoint(cent->currentState.eventParm, cent->currentState.origin2);
		}
		else
		{
			if (cg.lastFPFlashPoint[0] ||cg.lastFPFlashPoint[1] || cg.lastFPFlashPoint[2])
			{ //get the position of the muzzle flash for the first person weapon model from the last frame
				VectorCopy(cg.lastFPFlashPoint, cent->currentState.origin2);
			}
		}
		FX_DisruptorAltShot( cent->currentState.origin2, cent->lerpOrigin, cent->currentState.shouldtarget );
		break;

	case entity_event_t::EV_DISRUPTOR_SNIPER_MISS:
		DEBUGNAME("EV_DISRUPTOR_SNIPER_MISS");
		ByteToDir( es->eventParm, dir );
		if (ES_WeaponParm( *es ))
		{ //primary
			FX_DisruptorHitWall( cent->lerpOrigin, dir );
		}
		else
		{ //secondary
			FX_DisruptorAltMiss( cent->lerpOrigin, dir );
		}
		break;

	case entity_event_t::EV_DISRUPTOR_HIT:
		DEBUGNAME("EV_DISRUPTOR_HIT");
		ByteToDir( es->eventParm, dir );
		if (ES_WeaponParm( *es ))
		{ //client
			FX_DisruptorHitPlayer( cent->lerpOrigin, dir, true );
		}
		else
		{ //non-client
			FX_DisruptorHitWall( cent->lerpOrigin, dir );
		}
		break;

	case entity_event_t::EV_DISRUPTOR_ZOOMSOUND:
		DEBUGNAME("EV_DISRUPTOR_ZOOMSOUND");
		if (es->number == cg.snap->ps.clientNum)
		{
			if (cg.snap->ps.zoomMode)
			{
				trap->S_StartLocalSound(trap->S_RegisterSound("sound/weapons/disruptor/zoomstart.wav"), soundChannel_t::CHAN_AUTO);
			}
			else
			{
				trap->S_StartLocalSound(trap->S_RegisterSound("sound/weapons/disruptor/zoomend.wav"), soundChannel_t::CHAN_AUTO);
			}
		}
		break;
	case entity_event_t::EV_PREDEFSOUND:
		DEBUGNAME("EV_PREDEFSOUND");
		{
			int sID = -1;

			switch (static_cast<pdSounds_t>( es->eventParm ))
			{
			case pdSounds_t::PDSOUND_PROTECTHIT:
				sID = trap->S_RegisterSound("sound/weapons/force/protecthit.mp3");
				break;
			case pdSounds_t::PDSOUND_PROTECT:
				sID = trap->S_RegisterSound("sound/weapons/force/protect.mp3");
				break;
			case pdSounds_t::PDSOUND_ABSORBHIT:
				sID = trap->S_RegisterSound("sound/weapons/force/absorbhit.mp3");
				if (es->trickedentindex >= 0 && es->trickedentindex < MAX_CLIENTS)
				{
					int clnum = es->trickedentindex;

					cg_entities[clnum].teamPowerEffectTime = cg.time + 1000;
					cg_entities[clnum].teamPowerType = 3;
				}
				break;
			case pdSounds_t::PDSOUND_ABSORB:
				sID = trap->S_RegisterSound("sound/weapons/force/absorb.mp3");
				break;
			case pdSounds_t::PDSOUND_FORCEJUMP:
				sID = trap->S_RegisterSound("sound/weapons/force/jump.mp3");
				break;
			case pdSounds_t::PDSOUND_FORCEGRIP:
				sID = trap->S_RegisterSound("sound/weapons/force/grip.mp3");
				break;
			default:
				break;
			}

			if (sID != 1)
			{
				trap->S_StartSound(es->origin, es->number, soundChannel_t::CHAN_AUTO, sID);
			}
		}
		break;

	case entity_event_t::EV_TEAM_POWER:
		DEBUGNAME("EV_TEAM_POWER");
		{
			int clnum = 0;

			while (clnum < MAX_CLIENTS)
			{
				if (CG_InClientBitflags(es, clnum))
				{
					if (es->eventParm == 1)
					{ //eventParm 1 is heal
						trap->S_StartSound (nullptr, clnum, soundChannel_t::CHAN_AUTO, cgs.media.teamHealSound );
						cg_entities[clnum].teamPowerEffectTime = cg.time + 1000;
						cg_entities[clnum].teamPowerType = 1;
					}
					else
					{ //eventParm 2 is force regen
						trap->S_StartSound (nullptr, clnum, soundChannel_t::CHAN_AUTO, cgs.media.teamRegenSound );
						cg_entities[clnum].teamPowerEffectTime = cg.time + 1000;
						cg_entities[clnum].teamPowerType = 0;
					}
				}
				clnum++;
			}
		}
		break;

	case entity_event_t::EV_SCREENSHAKE:
		DEBUGNAME("EV_SCREENSHAKE");
		if (!es->modelindex || cg.predictedPlayerState.clientNum == es->modelindex-1)
		{
			CGCam_Shake(es->angles[0], es->time);
		}
		break;
	case entity_event_t::EV_LOCALTIMER:
		DEBUGNAME("EV_LOCALTIMER");
		if (es->owner == cg.predictedPlayerState.clientNum)
		{
			CG_LocalTimingBar(es->time, es->time2);
		}
		break;
	case entity_event_t::EV_USE_ITEM0:
		DEBUGNAME("EV_USE_ITEM0");
		CG_UseItem( cent );
		break;
	case entity_event_t::EV_USE_ITEM1:
		DEBUGNAME("EV_USE_ITEM1");
		CG_UseItem( cent );
		break;
	case entity_event_t::EV_USE_ITEM2:
		DEBUGNAME("EV_USE_ITEM2");
		CG_UseItem( cent );
		break;
	case entity_event_t::EV_USE_ITEM3:
		DEBUGNAME("EV_USE_ITEM3");
		CG_UseItem( cent );
		break;
	case entity_event_t::EV_USE_ITEM4:
		DEBUGNAME("EV_USE_ITEM4");
		CG_UseItem( cent );
		break;
	case entity_event_t::EV_USE_ITEM5:
		DEBUGNAME("EV_USE_ITEM5");
		CG_UseItem( cent );
		break;
	case entity_event_t::EV_USE_ITEM6:
		DEBUGNAME("EV_USE_ITEM6");
		CG_UseItem( cent );
		break;
	case entity_event_t::EV_USE_ITEM7:
		DEBUGNAME("EV_USE_ITEM7");
		CG_UseItem( cent );
		break;
	case entity_event_t::EV_USE_ITEM8:
		DEBUGNAME("EV_USE_ITEM8");
		CG_UseItem( cent );
		break;
	case entity_event_t::EV_USE_ITEM9:
		DEBUGNAME("EV_USE_ITEM9");
		CG_UseItem( cent );
		break;
	case entity_event_t::EV_USE_ITEM10:
		DEBUGNAME("EV_USE_ITEM10");
		CG_UseItem( cent );
		break;
	case entity_event_t::EV_USE_ITEM11:
		DEBUGNAME("EV_USE_ITEM11");
		CG_UseItem( cent );
		break;
	case entity_event_t::EV_USE_ITEM12:
		DEBUGNAME("EV_USE_ITEM12");
		CG_UseItem( cent );
		break;
	case entity_event_t::EV_USE_ITEM13:
		DEBUGNAME("EV_USE_ITEM13");
		CG_UseItem( cent );
		break;
	case entity_event_t::EV_USE_ITEM14:
		DEBUGNAME("EV_USE_ITEM14");
		CG_UseItem( cent );
		break;

	case entity_event_t::EV_ITEMUSEFAIL:
		DEBUGNAME("EV_ITEMUSEFAIL");
		if (cg.snap->ps.clientNum == es->number)
		{
			char *psStringEDRef = nullptr;

			switch(static_cast<itemUseFail_t>( es->eventParm ))
			{
			case itemUseFail_t::SENTRY_NOROOM:
				psStringEDRef = (char *)CG_GetStringEdString("MP_INGAME", "SENTRY_NOROOM");
				break;
			case itemUseFail_t::SENTRY_ALREADYPLACED:
				psStringEDRef = (char *)CG_GetStringEdString("MP_INGAME", "SENTRY_ALREADYPLACED");
				break;
			case itemUseFail_t::SHIELD_NOROOM:
				psStringEDRef = (char *)CG_GetStringEdString("MP_INGAME", "SHIELD_NOROOM");
				break;
			case itemUseFail_t::SEEKER_ALREADYDEPLOYED:
				psStringEDRef = (char *)CG_GetStringEdString("MP_INGAME", "SEEKER_ALREADYDEPLOYED");
				break;
			default:
				break;
			}

			if (!psStringEDRef)
			{
				break;
			}

			Com_Printf("%s\n", psStringEDRef);
		}
		break;

	//=================================================================

	//
	// other events
	//
	case entity_event_t::EV_PLAYER_TELEPORT_IN:
		DEBUGNAME("EV_PLAYER_TELEPORT_IN");
		{
			trace_t tr;
			vec3_t playerMins = {-15, -15, DEFAULT_MINS_2+8};
			vec3_t playerMaxs = {15, 15, DEFAULT_MAXS_2};
			vec3_t ang, pos, dpos;

			VectorClear(ang);
			ang[ROLL] = 1;

			VectorCopy(position, dpos);
			dpos[2] -= 4096;

			CG_Trace(&tr, position, playerMins, playerMaxs, dpos, es->number, MASK_SOLID);
			VectorCopy(tr.endpos, pos);

			trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.teleInSound );

			if (tr.fraction == 1)
			{
				break;
			}
			trap->FX_PlayEffectID(cgs.effects.mSpawn, pos, ang, -1, -1, false);
		}
		break;

	case entity_event_t::EV_PLAYER_TELEPORT_OUT:
		DEBUGNAME("EV_PLAYER_TELEPORT_OUT");
		{
			trace_t tr;
			vec3_t playerMins = {-15, -15, DEFAULT_MINS_2+8};
			vec3_t playerMaxs = {15, 15, DEFAULT_MAXS_2};
			vec3_t ang, pos, dpos;

			VectorClear(ang);
			ang[ROLL] = 1;

			VectorCopy(position, dpos);
			dpos[2] -= 4096;

			CG_Trace(&tr, position, playerMins, playerMaxs, dpos, es->number, MASK_SOLID);
			VectorCopy(tr.endpos, pos);

			trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.teleOutSound );

			if (tr.fraction == 1)
			{
				break;
			}
			trap->FX_PlayEffectID(cgs.effects.mSpawn, pos, ang, -1, -1, false);
		}
		break;

	case entity_event_t::EV_ITEM_POP:
		DEBUGNAME("EV_ITEM_POP");
		trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.respawnSound );
		break;
	case entity_event_t::EV_ITEM_RESPAWN:
		DEBUGNAME("EV_ITEM_RESPAWN");
		cent->miscTime = cg.time;	// scale up from this
		trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.respawnSound );
		break;

	case entity_event_t::EV_GRENADE_BOUNCE:
		DEBUGNAME("EV_GRENADE_BOUNCE");
		//Do something here?
		break;

	case entity_event_t::EV_SCOREPLUM:
		DEBUGNAME("EV_SCOREPLUM");
		CG_ScorePlum( cent->currentState.otherEntityNum, cent->lerpOrigin, cent->currentState.time );
		break;

	case entity_event_t::EV_CTFMESSAGE:
		DEBUGNAME("EV_CTFMESSAGE");
		CG_GetCTFMessageEvent(es);
		break;

	case entity_event_t::EV_BODYFADE:
		if (es->eType != entityType_t::ET_BODY)
		{
			assert(!"EV_BODYFADE event from a non-corpse");
			break;
		}

		if (cent->ghoul2 && trap->G2_HaveWeGhoul2Models(cent->ghoul2))
		{
			//turn the inside of the face off, to avoid showing the mouth when we start alpha fading the corpse
			trap->G2API_SetSurfaceOnOff( cent->ghoul2, "head_eyes_mouth", g2SurfaceFlags_t::G2SURFACEFLAG_OFF );
		}

		cent->bodyFadeTime = cg.time + 60000;
		break;

	//
	// siege gameplay events
	//
	case entity_event_t::EV_SIEGE_ROUNDOVER:
		DEBUGNAME("EV_SIEGE_ROUNDOVER");
		CG_SiegeRoundOver(&cg_entities[ES_WeaponParm( cent->currentState )], static_cast<team_t>( cent->currentState.eventParm ));
		break;
	case entity_event_t::EV_SIEGE_OBJECTIVECOMPLETE:
		DEBUGNAME("EV_SIEGE_OBJECTIVECOMPLETE");
		CG_SiegeObjectiveCompleted(&cg_entities[ES_WeaponParm( cent->currentState )], static_cast<team_t>( cent->currentState.eventParm ), cent->currentState.trickedentindex);
		break;

	case entity_event_t::EV_DESTROY_GHOUL2_INSTANCE:
		DEBUGNAME("EV_DESTROY_GHOUL2_INSTANCE");
		if (cg_entities[es->eventParm].ghoul2 && trap->G2_HaveWeGhoul2Models(cg_entities[es->eventParm].ghoul2))
		{
			if (es->eventParm < MAX_CLIENTS)
			{ //You try to do very bad thing!
#ifdef _DEBUG
				Com_Printf("WARNING: Tried to kill a client ghoul2 instance with a server event!\n");
#endif
				break;
			}
			trap->G2API_CleanGhoul2Models(&(cg_entities[es->eventParm].ghoul2));
		}
		break;

	case entity_event_t::EV_DESTROY_WEAPON_MODEL:
		DEBUGNAME("EV_DESTROY_WEAPON_MODEL");
		if (cg_entities[es->eventParm].ghoul2 && trap->G2_HaveWeGhoul2Models(cg_entities[es->eventParm].ghoul2) &&
			trap->G2API_HasGhoul2ModelOnIndex(&(cg_entities[es->eventParm].ghoul2), 1))
		{
			trap->G2API_RemoveGhoul2Model(&(cg_entities[es->eventParm].ghoul2), 1);
		}
		break;

	case entity_event_t::EV_GIVE_NEW_RANK:
		DEBUGNAME("EV_GIVE_NEW_RANK");
		if (es->trickedentindex == cg.snap->ps.clientNum)
		{
			trap->Cvar_Set("ui_rankChange", va("%i", es->eventParm));

			trap->Cvar_Set("ui_myteam", va("%i", es->bolt2));

			if (!Any( trap->Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_UI ) && !es->bolt1)
			{
				trap->OpenUIMenu(uiMenuCommand_t::UIMENU_PLAYERCONFIG);
			}
		}
		break;

	case entity_event_t::EV_SET_FREE_SABER:
		DEBUGNAME("EV_SET_FREE_SABER");

		trap->Cvar_Set("ui_freeSaber", va("%i", es->eventParm));
		break;

	case entity_event_t::EV_SET_FORCE_DISABLE:
		DEBUGNAME("EV_SET_FORCE_DISABLE");

		trap->Cvar_Set("ui_forcePowerDisable", va("%i", es->eventParm));
		break;

	//
	// missile impacts
	//
	case entity_event_t::EV_CONC_ALT_IMPACT:
		DEBUGNAME("EV_CONC_ALT_IMPACT");
		{
			float dist;
			float shotDist = VectorNormalize(es->angles);
			vec3_t spot;

			for (dist = 0.0f; dist < shotDist; dist += 64.0f)
			{ //one effect would be.. a whole lot better
				VectorMA( es->origin2, dist, es->angles, spot );
                trap->FX_PlayEffectID(cgs.effects.mConcussionAltRing, spot, es->angles2, -1, -1, false);
			}

			ByteToDir( es->eventParm, dir );
			CG_MissileHitWall(weapon_t::WP_CONCUSSION, es->owner, position, dir, impactSound_t::IMPACTSOUND_DEFAULT, true, 0);

			FX_ConcAltShot(es->origin2, spot);

			//steal the bezier effect from the disruptor
			FX_DisruptorAltMiss(position, dir);
		}
		break;

	case entity_event_t::EV_MISSILE_STICK:
		DEBUGNAME("EV_MISSILE_STICK");
//		trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.missileStick );
		break;

	case entity_event_t::EV_MISSILE_HIT:
		DEBUGNAME("EV_MISSILE_HIT");
		ByteToDir( es->eventParm, dir );
		if ( es->emplacedOwner )
		{//hack: this is an index to a custom effect to use
			trap->FX_PlayEffectID(cgs.gameEffects[es->emplacedOwner], position, dir, -1, -1, false);
		}
		else if ( CG_VehicleWeaponImpact( cent ) )
		{//a vehicle missile that uses an overridden impact effect...
		}
		else if (Any( cent->currentState.eFlags & entityFlags_t::EF_ALT_FIRING ))
		{
			CG_MissileHitPlayer( es->weapon, position, dir, es->otherEntityNum, true);
		}
		else
		{
			CG_MissileHitPlayer( es->weapon, position, dir, es->otherEntityNum, false);
		}

		if (cg_ghoul2Marks.integer &&
			es->trickedentindex)
		{ //flag to place a ghoul2 mark
			CG_G2MarkEvent(es);
		}
		break;

	case entity_event_t::EV_MISSILE_MISS:
		DEBUGNAME("EV_MISSILE_MISS");
		ByteToDir( es->eventParm, dir );
		if ( es->emplacedOwner )
		{//hack: this is an index to a custom effect to use
			trap->FX_PlayEffectID(cgs.gameEffects[es->emplacedOwner], position, dir, -1, -1, false);
		}
		else if ( CG_VehicleWeaponImpact( cent ) )
		{//a vehicle missile that used an overridden impact effect...
		}
		else if (Any( cent->currentState.eFlags & entityFlags_t::EF_ALT_FIRING ))
		{
			CG_MissileHitWall(es->weapon, 0, position, dir, impactSound_t::IMPACTSOUND_DEFAULT, true, es->generic1);
		}
		else
		{
			CG_MissileHitWall(es->weapon, 0, position, dir, impactSound_t::IMPACTSOUND_DEFAULT, false, 0);
		}

		if (cg_ghoul2Marks.integer &&
			es->trickedentindex)
		{ //flag to place a ghoul2 mark
			CG_G2MarkEvent(es);
		}
		break;

	case entity_event_t::EV_MISSILE_MISS_METAL:
		DEBUGNAME("EV_MISSILE_MISS_METAL");
		ByteToDir( es->eventParm, dir );
		if ( es->emplacedOwner )
		{//hack: this is an index to a custom effect to use
			trap->FX_PlayEffectID(cgs.gameEffects[es->emplacedOwner], position, dir, -1, -1, false);
		}
		else if ( CG_VehicleWeaponImpact( cent ) )
		{//a vehicle missile that used an overridden impact effect...
		}
		else if (Any( cent->currentState.eFlags & entityFlags_t::EF_ALT_FIRING ))
		{
			CG_MissileHitWall(es->weapon, 0, position, dir, impactSound_t::IMPACTSOUND_METAL, true, es->generic1);
		}
		else
		{
			CG_MissileHitWall(es->weapon, 0, position, dir, impactSound_t::IMPACTSOUND_METAL, false, 0);
		}
		break;

	case entity_event_t::EV_PLAY_EFFECT:
		DEBUGNAME("EV_PLAY_EFFECT");
		switch( static_cast<effectTypes_t>( es->eventParm ) )
		{ //it isn't a hack, it's ingenuity!
		case effectTypes_t::EFFECT_SMOKE:
			eID = cgs.effects.mEmplacedDeadSmoke;
			break;
		case effectTypes_t::EFFECT_EXPLOSION:
			eID = cgs.effects.mEmplacedExplode;
			break;
		case effectTypes_t::EFFECT_EXPLOSION_PAS:
			eID = cgs.effects.mTurretExplode;
			break;
		case effectTypes_t::EFFECT_SPARK_EXPLOSION:
			eID = cgs.effects.mSparkExplosion;
			break;
		case effectTypes_t::EFFECT_EXPLOSION_TRIPMINE:
			eID = cgs.effects.mTripmineExplosion;
			break;
		case effectTypes_t::EFFECT_EXPLOSION_DETPACK:
			eID = cgs.effects.mDetpackExplosion;
			break;
		case effectTypes_t::EFFECT_EXPLOSION_FLECHETTE:
			eID = cgs.effects.mFlechetteAltBlow;
			break;
		case effectTypes_t::EFFECT_STUNHIT:
			eID = cgs.effects.mStunBatonFleshImpact;
			break;
		case effectTypes_t::EFFECT_EXPLOSION_DEMP2ALT:
			FX_DEMP2_AltDetonate( cent->lerpOrigin, ES_WeaponParm( *es ) );
			eID = cgs.effects.mAltDetonate;
			break;
		case effectTypes_t::EFFECT_EXPLOSION_TURRET:
			eID = cgs.effects.mTurretExplode;
			break;
		case effectTypes_t::EFFECT_SPARKS:
			eID = cgs.effects.mSparksExplodeNoSound;
			break;
		case effectTypes_t::EFFECT_WATER_SPLASH:
			eID = cgs.effects.waterSplash;
			break;
		case effectTypes_t::EFFECT_ACID_SPLASH:
			eID = cgs.effects.acidSplash;
			break;
		case effectTypes_t::EFFECT_LAVA_SPLASH:
			eID = cgs.effects.lavaSplash;
			break;
		case effectTypes_t::EFFECT_LANDING_MUD:
			eID = cgs.effects.landingMud;
			break;
		case effectTypes_t::EFFECT_LANDING_SAND:
			eID = cgs.effects.landingSand;
			break;
		case effectTypes_t::EFFECT_LANDING_DIRT:
			eID = cgs.effects.landingDirt;
			break;
		case effectTypes_t::EFFECT_LANDING_SNOW:
			eID = cgs.effects.landingSnow;
			break;
		case effectTypes_t::EFFECT_LANDING_GRAVEL:
			eID = cgs.effects.landingGravel;
			break;
		default:
			eID = -1;
			break;
		}

		if (eID != -1)
		{
			vec3_t fxDir;

			VectorCopy(es->angles, fxDir);

			if (!fxDir[0] && !fxDir[1] && !fxDir[2])
			{
				fxDir[1] = 1;
			}

			trap->FX_PlayEffectID(eID, es->origin, fxDir, -1, -1, false);
		}
		break;

	case entity_event_t::EV_PLAY_EFFECT_ID:
	case entity_event_t::EV_PLAY_PORTAL_EFFECT_ID:
		DEBUGNAME("EV_PLAY_EFFECT_ID");
		{
			vec3_t fxDir;
			bool portalEffect = false;
			int efxIndex = 0;

			if (event == entity_event_t::EV_PLAY_PORTAL_EFFECT_ID)
			{ //This effect should only be played inside sky portals.
				portalEffect = true;
			}

			AngleVectors(es->angles, fxDir, nullptr, nullptr);

			if (!fxDir[0] && !fxDir[1] && !fxDir[2])
			{
				fxDir[1] = 1;
			}

			if ( cgs.gameEffects[ es->eventParm ] )
			{
				efxIndex = cgs.gameEffects[es->eventParm];
			}
			else
			{
				s = CG_ConfigString( CS_EFFECTS + es->eventParm );
				if (s && s[0])
				{
					efxIndex = trap->FX_RegisterEffect(s);
				}
			}

			if (efxIndex)
			{
				if (portalEffect)
				{
					trap->FX_PlayEffectID(efxIndex, position, fxDir, -1, -1, true );
				}
				else
				{
					trap->FX_PlayEffectID(efxIndex, position, fxDir, -1, -1, false );
				}
			}
		}
		break;

	case entity_event_t::EV_PLAYDOORSOUND:
		CG_PlayDoorSound(cent, es->eventParm);
		break;
	case entity_event_t::EV_PLAYDOORLOOPSOUND:
		CG_PlayDoorLoopSound(cent);
		break;
	case entity_event_t::EV_BMODEL_SOUND:
		DEBUGNAME("EV_BMODEL_SOUND");
		{
			sfxHandle_t sfx;
			const char *soundSet;

			soundSet = CG_ConfigString( CS_AMBIENT_SET + es->soundSetIndex );

			if (!soundSet || !soundSet[0])
			{
				break;
			}

			sfx = trap->AS_GetBModelSound(soundSet, es->eventParm);

			if (sfx == -1)
			{
				break;
			}

			trap->S_StartSound( nullptr, es->number, soundChannel_t::CHAN_AUTO, sfx );
		}
		break;


	case entity_event_t::EV_MUTE_SOUND:
		DEBUGNAME("EV_MUTE_SOUND");
		if (Any( cg_entities[es->trickedentindex2].currentState.eFlags & entityFlags_t::EF_SOUNDTRACKER ))
		{
			cg_entities[es->trickedentindex2].currentState.eFlags &= ~entityFlags_t::EF_SOUNDTRACKER;
		}
		trap->S_MuteSound(es->trickedentindex2, static_cast<soundChannel_t>( es->trickedentindex ));
		CG_S_StopLoopingSound(es->trickedentindex2, -1);
		break;

	case entity_event_t::EV_VOICECMD_SOUND:
		DEBUGNAME("EV_VOICECMD_SOUND");
		if (es->groundEntityNum < MAX_CLIENTS && es->groundEntityNum >= 0)
		{
			int clientNum = es->groundEntityNum;
			sfxHandle_t sfx = cgs.gameSounds[ es->eventParm ];
			clientInfo_t *ci = &cgs.clientinfo[clientNum];
			centity_t *vChatEnt = &cg_entities[clientNum];
			char descr[1024] = {0};

			Q_strncpyz(descr, CG_GetStringForVoiceSound(CG_ConfigString( CS_SOUNDS + es->eventParm )), sizeof( descr ) );

			if (!sfx)
			{
				s = CG_ConfigString( CS_SOUNDS + es->eventParm );
				sfx = CG_CustomSound( clientNum, s );
			}

			if (sfx)
			{
				if (clientNum != cg.predictedPlayerState.clientNum)
				{ //play on the head as well to simulate hearing in radio and in world
					if (ci->team == PS_Team( cg.predictedPlayerState ))
					{ //don't hear it if this person is on the other team, but they can still
						//hear it in the world spot.
						trap->S_StartSound (nullptr, cg.snap->ps.clientNum, soundChannel_t::CHAN_MENU1, sfx);
					}
				}
				if (ci->team == PS_Team( cg.predictedPlayerState ))
				{ //add to the chat box
					//hear it in the world spot.
					char vchatstr[1024] = {0};
					Q_strncpyz(vchatstr, va("<%s^7: %s>\n", ci->name, descr), sizeof( vchatstr ) );
					CG_ChatBox_AddString(vchatstr);
					trap->Print("*%s", vchatstr);
				}

				//and play in world for everyone
				trap->S_StartSound (nullptr, clientNum, soundChannel_t::CHAN_VOICE, sfx);
				vChatEnt->vChatTime = cg.time + 1000;
			}
		}
		break;

	case entity_event_t::EV_GENERAL_SOUND:
		DEBUGNAME("EV_GENERAL_SOUND");
		if (es->saberEntityNum == to_underlying( trackchan_t::TRACK_CHANNEL_2 ) || es->saberEntityNum == to_underlying( trackchan_t::TRACK_CHANNEL_3 ) ||
			es->saberEntityNum == to_underlying( trackchan_t::TRACK_CHANNEL_5 ))
		{ //channels 2 and 3 are for speed and rage, 5 for sight
			if ( cgs.gameSounds[ es->eventParm ] )
			{
				CG_S_AddRealLoopingSound(es->number, es->pos.trBase, vec3_origin, cgs.gameSounds[ es->eventParm ] );
			}
		}
		else
		{
			if ( cgs.gameSounds[ es->eventParm ] ) {
				trap->S_StartSound (nullptr, es->number, static_cast<soundChannel_t>( es->saberEntityNum ), cgs.gameSounds[ es->eventParm ] );
			} else {
				s = CG_ConfigString( CS_SOUNDS + es->eventParm );
				trap->S_StartSound (nullptr, es->number, static_cast<soundChannel_t>( es->saberEntityNum ), CG_CustomSound( es->number, s ) );
			}
		}
		break;

	case entity_event_t::EV_GLOBAL_SOUND:	// play from the player's head so it never diminishes
		DEBUGNAME("EV_GLOBAL_SOUND");
		if ( cgs.gameSounds[ es->eventParm ] ) {
			trap->S_StartSound (nullptr, cg.snap->ps.clientNum, soundChannel_t::CHAN_MENU1, cgs.gameSounds[ es->eventParm ] );
		} else {
			s = CG_ConfigString( CS_SOUNDS + es->eventParm );
			trap->S_StartSound (nullptr, cg.snap->ps.clientNum, soundChannel_t::CHAN_MENU1, CG_CustomSound( es->number, s ) );
		}
		break;

	case entity_event_t::EV_GLOBAL_TEAM_SOUND:	// play from the player's head so it never diminishes
		{
			DEBUGNAME("EV_GLOBAL_TEAM_SOUND");
			switch( static_cast<global_team_sound_t>( es->eventParm ) ) {
				case global_team_sound_t::GTS_RED_CAPTURE: // CTF: red team captured the blue flag, 1FCTF: red team captured the neutral flag
					//CG_AddBufferedSound( cgs.media.redScoredSound );
					break;
				case global_team_sound_t::GTS_BLUE_CAPTURE: // CTF: blue team captured the red flag, 1FCTF: blue team captured the neutral flag
					//CG_AddBufferedSound( cgs.media.blueScoredSound );
					break;
				case global_team_sound_t::GTS_RED_RETURN: // CTF: blue flag returned, 1FCTF: never used
					if (cgs.gametype == gametype_t::GT_CTY)
					{
						CG_AddBufferedSound( cgs.media.blueYsalReturnedSound );
					}
					else
					{
						CG_AddBufferedSound( cgs.media.blueFlagReturnedSound );
					}
					break;
				case global_team_sound_t::GTS_BLUE_RETURN: // CTF red flag returned, 1FCTF: neutral flag returned
					if (cgs.gametype == gametype_t::GT_CTY)
					{
						CG_AddBufferedSound( cgs.media.redYsalReturnedSound );
					}
					else
					{
						CG_AddBufferedSound( cgs.media.redFlagReturnedSound );
					}
					break;

				case global_team_sound_t::GTS_RED_TAKEN: // CTF: red team took blue flag, 1FCTF: blue team took the neutral flag
					// if this player picked up the flag then a sound is played in CG_CheckLocalSounds
					if (cgs.gametype == gametype_t::GT_CTY)
					{
						CG_AddBufferedSound( cgs.media.redTookYsalSound );
					}
					else
					{
					 	CG_AddBufferedSound( cgs.media.redTookFlagSound );
					}
					break;
				case global_team_sound_t::GTS_BLUE_TAKEN: // CTF: blue team took the red flag, 1FCTF red team took the neutral flag
					// if this player picked up the flag then a sound is played in CG_CheckLocalSounds
					if (cgs.gametype == gametype_t::GT_CTY)
					{
						CG_AddBufferedSound( cgs.media.blueTookYsalSound );
					}
					else
					{
						CG_AddBufferedSound( cgs.media.blueTookFlagSound );
					}
					break;
				case global_team_sound_t::GTS_REDTEAM_SCORED:
					CG_AddBufferedSound(cgs.media.redScoredSound);
					break;
				case global_team_sound_t::GTS_BLUETEAM_SCORED:
					CG_AddBufferedSound(cgs.media.blueScoredSound);
					break;
				case global_team_sound_t::GTS_REDTEAM_TOOK_LEAD:
					CG_AddBufferedSound(cgs.media.redLeadsSound);
					break;
				case global_team_sound_t::GTS_BLUETEAM_TOOK_LEAD:
					CG_AddBufferedSound(cgs.media.blueLeadsSound);
					break;
				case global_team_sound_t::GTS_TEAMS_ARE_TIED:
					CG_AddBufferedSound( cgs.media.teamsTiedSound );
					break;
				default:
					break;
			}
			break;
		}

	case entity_event_t::EV_ENTITY_SOUND:
		DEBUGNAME("EV_ENTITY_SOUND");
		//somewhat of a hack - weapon is the caller entity's index, trickedentindex is the proper sound channel
		if ( cgs.gameSounds[ es->eventParm ] ) {
			trap->S_StartSound (nullptr, es->clientNum, static_cast<soundChannel_t>( es->trickedentindex ), cgs.gameSounds[ es->eventParm ] );
		} else {
			s = CG_ConfigString( CS_SOUNDS + es->eventParm );
			trap->S_StartSound (nullptr, es->clientNum, static_cast<soundChannel_t>( es->trickedentindex ), CG_CustomSound( es->clientNum, s ) );
		}
		break;

	case entity_event_t::EV_PLAY_ROFF:
		DEBUGNAME("EV_PLAY_ROFF");
		trap->ROFF_Play(ES_WeaponParm( *es ), es->eventParm, es->trickedentindex);
		break;

	case entity_event_t::EV_GLASS_SHATTER:
		DEBUGNAME("EV_GLASS_SHATTER");
		CG_GlassShatter(es->genericenemyindex, es->origin, es->angles, es->trickedentindex, es->pos.trTime);
		break;

	case entity_event_t::EV_DEBRIS:
		DEBUGNAME("EV_DEBRIS");
		CG_Chunks(es->owner, es->origin, es->angles, es->origin2, es->angles2, es->speed,
			es->eventParm, (material_t)es->trickedentindex, es->modelindex, es->apos.trBase[0]);
		break;

	case entity_event_t::EV_MISC_MODEL_EXP:
		DEBUGNAME("EV_MISC_MODEL_EXP");
		CG_MiscModelExplosion(es->origin2, es->angles2, es->time, (material_t)es->eventParm);
		break;

	case entity_event_t::EV_PAIN:
		// local player sounds are triggered in CG_CheckLocalSounds,
		// so ignore events on the player
		DEBUGNAME("EV_PAIN");

		if ( !cg_oldPainSounds.integer || (cent->currentState.number != cg.snap->ps.clientNum) )
		{
			CG_PainEvent( cent, es->eventParm );
		}
		break;

	case entity_event_t::EV_DEATH1:
	case entity_event_t::EV_DEATH2:
	case entity_event_t::EV_DEATH3:
		DEBUGNAME("EV_DEATHx");
		trap->S_StartSound( nullptr, es->number, soundChannel_t::CHAN_VOICE,
				CG_CustomSound( es->number, va("*death%i.wav", event - entity_event_t::EV_DEATH1 + 1) ) );
		if (es->eventParm && es->number == cg.snap->ps.clientNum)
		{
			trap->S_StartLocalSound(cgs.media.dramaticFailure, soundChannel_t::CHAN_LOCAL);
			CGCam_SetMusicMult(0.3f, 5000);
		}
		break;


	case entity_event_t::EV_OBITUARY:
		DEBUGNAME("EV_OBITUARY");
		CG_Obituary( es );
		break;

	//
	// powerup events
	//
	case entity_event_t::EV_POWERUP_QUAD:
		DEBUGNAME("EV_POWERUP_QUAD");
		if ( es->number == cg.snap->ps.clientNum ) {
			cg.powerupActive = powerup_t::PW_QUAD;
			cg.powerupTime = cg.time;
		}
		//trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_ITEM, cgs.media.quadSound );
		break;
	case entity_event_t::EV_POWERUP_BATTLESUIT:
		DEBUGNAME("EV_POWERUP_BATTLESUIT");
		if ( es->number == cg.snap->ps.clientNum ) {
			cg.powerupActive = powerup_t::PW_BATTLESUIT;
			cg.powerupTime = cg.time;
		}
		//trap->S_StartSound (nullptr, es->number, soundChannel_t::CHAN_ITEM, cgs.media.protectSound );
		break;

	case entity_event_t::EV_FORCE_DRAINED:
		DEBUGNAME("EV_FORCE_DRAINED");
		ByteToDir( es->eventParm, dir );
		//FX_ForceDrained(position, dir);
		trap->S_StartSound (nullptr, es->owner, soundChannel_t::CHAN_AUTO, cgs.media.drainSound );
		cg_entities[es->owner].teamPowerEffectTime = cg.time + 1000;
		cg_entities[es->owner].teamPowerType = 2;
		break;

	case entity_event_t::EV_GIB_PLAYER:
		DEBUGNAME("EV_GIB_PLAYER");
		//trap->S_StartSound( nullptr, es->number, soundChannel_t::CHAN_BODY, cgs.media.gibSound );
		//CG_GibPlayer( cent->lerpOrigin );
		break;

	case entity_event_t::EV_STARTLOOPINGSOUND:
		DEBUGNAME("EV_STARTLOOPINGSOUND");
		if ( cgs.gameSounds[ es->eventParm ] )
		{
			isnd = cgs.gameSounds[es->eventParm];
		}
		else
		{
			s = CG_ConfigString( CS_SOUNDS + es->eventParm );
			isnd = CG_CustomSound(es->number, s);
		}

		CG_S_AddRealLoopingSound( es->number, es->pos.trBase, vec3_origin, isnd );
		es->loopSound = isnd;
		break;

	case entity_event_t::EV_STOPLOOPINGSOUND:
		DEBUGNAME("EV_STOPLOOPINGSOUND");
		CG_S_StopLoopingSound( es->number, -1 );
		es->loopSound = 0;
		break;

	case entity_event_t::EV_WEAPON_CHARGE:
		DEBUGNAME("EV_WEAPON_CHARGE");
		{
			const weapon_t weapon = static_cast<weapon_t>( es->eventParm );
			assert(weapon > weapon_t::WP_NONE && weapon < weapon_t::WP_NUM_WEAPONS);
			if (cg_weapons[weapon].chargeSound)
			{
				trap->S_StartSound(nullptr, es->number, soundChannel_t::CHAN_WEAPON, cg_weapons[weapon].chargeSound);
			}
			else if (weapon == weapon_t::WP_DISRUPTOR)
			{
				trap->S_StartSound(nullptr, es->number, soundChannel_t::CHAN_WEAPON, cgs.media.disruptorZoomLoop);
			}
		}
		break;

	case entity_event_t::EV_WEAPON_CHARGE_ALT:
		DEBUGNAME("EV_WEAPON_CHARGE_ALT");
		{
			const weapon_t weapon = static_cast<weapon_t>( es->eventParm );
			assert(weapon > weapon_t::WP_NONE && weapon < weapon_t::WP_NUM_WEAPONS);
			if (cg_weapons[weapon].altChargeSound)
			{
				trap->S_StartSound(nullptr, es->number, soundChannel_t::CHAN_WEAPON, cg_weapons[weapon].altChargeSound);
			}
		}
		break;

	case entity_event_t::EV_SHIELD_HIT:
		DEBUGNAME("EV_SHIELD_HIT");
		ByteToDir(es->eventParm, dir);
		CG_PlayerShieldHit(es->otherEntityNum, dir, es->time2);
		break;

	case entity_event_t::EV_DEBUG_LINE:
		DEBUGNAME("EV_DEBUG_LINE");
		CG_Beam( cent );
		break;

	case entity_event_t::EV_TESTLINE:
		DEBUGNAME("EV_TESTLINE");
		CG_TestLine(es->origin, es->origin2, es->time2, ES_WeaponParm( *es ), 1);
		break;

	default:
		DEBUGNAME("UNKNOWN");
		trap->Error( errorParm_t::ERR_DROP, "Unknown event: %i", to_underlying( event ) );
		break;
	}

}


/*
==============
CG_CheckEvents

==============
*/
void CG_CheckEvents( centity_t *cent ) {
	// check for event-only entities
	if ( cent->currentState.eType > entityType_t::ET_EVENTS ) {
		if ( cent->previousEvent ) {
			return;	// already fired
		}
		// if this is a player event set the entity number of the client entity number
		if ( Any( cent->currentState.eFlags & entityFlags_t::EF_PLAYER_EVENT ) ) {
			cent->currentState.number = cent->currentState.otherEntityNum;
		}

		cent->previousEvent = 1;

		cent->currentState.event = BG_EventEntityEvent( cent->currentState.eType );
	} else {
		// check for events riding with another entity
		if ( cent->currentState.event == cent->previousEvent ) {
			return;
		}
		cent->previousEvent = cent->currentState.event;
		if ( BG_UnpackEvent( cent->currentState.event ) == entity_event_t::EV_NONE ) {
			return;
		}
	}

	// calculate the position at exactly the frame time
	BG_EvaluateTrajectory( &cent->currentState.pos, cg.snap->serverTime, cent->lerpOrigin );
	CG_SetEntitySoundPosition( cent );

	CG_EntityEvent( cent, cent->lerpOrigin );
}


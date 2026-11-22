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
#include "cg_media.h"
#include "FxScheduler.h"

#include <game/anims.h>

extern void CG_TryPlayCustomSound(vec3_t origin, int entityNum, soundChannel_t channel, const char* soundName, customSoundSet_t customSoundSet);

//==========================================================================

bool CG_IsFemale(const char* infostring)
{
    const char* sex;

    sex = Info_ValueForKey(infostring, "s");
    if (sex[0] == 'f' || sex[0] == 'F')
        return true;
    return false;
}

const char* CG_PlaceString(int rank)
{
    static char str[64];
    const char *s, *t;
    formatText_t sText; // holds the text s points into

    if (rank & RANK_TIED_FLAG) {
        rank &= ~RANK_TIED_FLAG;
        t = "Tied for ";
    }
    else {
        t = "";
    }

    if (rank == 1) {
        s = "\03341st\0337"; // draw in blue
    }
    else if (rank == 2) {
        s = "\03312nd\0337"; // draw in red
    }
    else if (rank == 3) {
        s = "\03333rd\0337"; // draw in yellow
    }
    else if (rank == 11) {
        s = "11th";
    }
    else if (rank == 12) {
        s = "12th";
    }
    else if (rank == 13) {
        s = "13th";
    }
    else if (rank % 10 == 1) {
        s = (sText = Q_Format("{}st", rank)).c_str();
    }
    else if (rank % 10 == 2) {
        s = (sText = Q_Format("{}nd", rank)).c_str();
    }
    else if (rank % 10 == 3) {
        s = (sText = Q_Format("{}rd", rank)).c_str();
    }
    else {
        s = (sText = Q_Format("{}th", rank)).c_str();
    }

    Q_FormatTo(str, "{}{}", t, s);
    return str;
}

/*
================
CG_ItemPickup

A new item was picked up this frame
================
*/
void CG_ItemPickup(itemNum_t itemNum, bool bHadItem)
{
    cg.itemPickup = itemNum;
    cg.itemPickupTime = cg.time;
    cg.itemPickupBlendTime = cg.time;

    if (bg_itemlist[itemNum].classname && bg_itemlist[itemNum].classname[0]) {
        char text[1024], data[1024];
        if (cgi_SP_GetStringTextString("INGAME_PICKUPLINE", text, sizeof(text))) {
            if (cgi_SP_GetStringTextString(Q_Format("INGAME_{}", bg_itemlist[itemNum].classname).c_str(), data, sizeof(data))) {
                Q_Println("{} {}", text, data);
            }
        }
    }

    // see if it should be the grabbed weapon
    if (bg_itemlist[itemNum].giType == itemType_t::IT_WEAPON) {
        const weapon_t nCurWpn = cg.predicted_player_state.weapon;
        const weapon_t nNewWpn = GI_Weapon(bg_itemlist[itemNum]);

        if (nCurWpn == weapon_t::WP_SABER || bHadItem) { // never switch away from the saber!
            return;
        }

        // kef -- check cg_autoswitch...
        //
        // 0 == no switching
        // 1 == automatically switch to best SAFE weapon
        // 2 == automatically switch to best weapon, safe or otherwise
        //
        // NOTE: automatically switching to any weapon you pick up is stupid and annoying and we won't do it.
        //

        if (nNewWpn == weapon_t::WP_SABER) { // always switch to saber
            SetWeaponSelectTime();
            cg.weaponSelect = nNewWpn;
        }
        else if (0 == cg_autoswitch.integer) {
            // don't switch
        }
        else if (1 == cg_autoswitch.integer) {
            // safe switching
            if ((nNewWpn > nCurWpn) && !(nNewWpn == weapon_t::WP_DET_PACK) && !(nNewWpn == weapon_t::WP_TRIP_MINE) && !(nNewWpn == weapon_t::WP_THERMAL) &&
                !(nNewWpn == weapon_t::WP_ROCKET_LAUNCHER)) {
                // switch to new wpn
                //				cg.weaponSelectTime = cg.time;
                SetWeaponSelectTime();
                cg.weaponSelect = nNewWpn;
            }
        }
        else if (2 == cg_autoswitch.integer) {
            // best
            if (nNewWpn > nCurWpn) {
                // switch to new wpn
                //				cg.weaponSelectTime = cg.time;
                SetWeaponSelectTime();
                cg.weaponSelect = nNewWpn;
            }
        }
    }
}

/*
===============
UseItem
===============
*/
extern void CG_ToggleBinoculars(void);
extern void CG_ToggleLAGoggles(void);

void UseItem(inventory_t itemNum)
{
    centity_t* cent;

    cent = &cg_entities[cg.snap->ps.clientNum];

    switch (itemNum) {
    case inventory_t::INV_ELECTROBINOCULARS:
        CG_ToggleBinoculars();
        break;
    case inventory_t::INV_LIGHTAMP_GOGGLES:
        CG_ToggleLAGoggles();
        break;
    case inventory_t::INV_GOODIE_KEY:
        if (cent->gent->client->ps.inventory[inventory_t::INV_GOODIE_KEY]) {
            cent->gent->client->ps.inventory[inventory_t::INV_GOODIE_KEY]--;
        }
        break;
    case inventory_t::INV_SECURITY_KEY:
        if (cent->gent->client->ps.inventory[inventory_t::INV_SECURITY_KEY]) {
            cent->gent->client->ps.inventory[inventory_t::INV_SECURITY_KEY]--;
        }
        break;
    default:
        break;
    }
}

/*
===============
CG_UseForce
===============
*/
static void CG_UseForce(centity_t* cent)
{
    // FIXME: sound or graphic change or something?
    // actual force power action is on game/pm side
}

/*
===============
CG_UseItem
===============
*/
static void CG_UseItem(centity_t* cent)
{
    inventory_t itemNum;
    entityState_t* es;

    es = &cent->currentState;

    itemNum = cg.inventorySelect;
    if (itemNum < inventory_t::INV_ELECTROBINOCULARS || itemNum >= inventory_t::INV_MAX) {
        itemNum = inventory_t::INV_ELECTROBINOCULARS;
    }

    // print a message if the local player
    if (es->number == cg.snap->ps.clientNum) {
        if (itemNum == inventory_t::INV_ELECTROBINOCULARS) {
            //			CG_CenterPrint( "No item to use", SCREEN_HEIGHT * 0.30, BIGCHAR_WIDTH );
        }
        else {
            //			item = BG_FindItemForHoldable( itemNum );
            //			CG_CenterPrint( Q_Format("Use {}", item->pickup_name).c_str(), SCREEN_HEIGHT * 0.30, BIGCHAR_WIDTH );
        }
    }

    UseItem(itemNum);
}

/*
==============
CG_EntityEvent

An entity has an event value
==============
*/
#define DEBUGNAME(x)                                                                                                                                           \
    if (cg_debugEvents.integer) {                                                                                                                              \
        Q_Println(x "");                                                                                                                                       \
    }
void CG_EntityEvent(centity_t* cent, vec3_t position)
{
    entityState_t* es;
    entity_event_t event;
    vec3_t axis[3];
    const char *s, *s2;
    int clientNum;
    // clientInfo_t	*ci;

    es = &cent->currentState;
    event = BG_UnpackEvent(es->event);

    if (cg_debugEvents.integer) {
        Q_Print("ent:{:3}  event:{:3} ", es->number, event);
    }

    if (event == entity_event_t::EV_NONE) {
        DEBUGNAME("ZEROEVENT");
        return;
    }

    if (!cent->gent) //|| !cent->gent->client )
    {
        return;
    }

    // ci = &cent->gent->client->clientInfo;
    clientNum = cent->gent->s.number;

    switch (event) {
    //
    // movement generated events
    //
    case entity_event_t::EV_FOOTSTEP:
        DEBUGNAME("EV_FOOTSTEP");
        if (cg_footsteps.integer) {
            if (cent->gent && cent->gent->s.number == 0 && !cg.renderingThirdPerson) //! cg_thirdPerson.integer )
            {                                                                        // Everyone else has keyframed footsteps in animsounds.cfg
                cgi_S_StartSound(nullptr, es->number, soundChannel_t::CHAN_BODY, cgs.media.footsteps[footstep_t::FOOTSTEP_NORMAL][Q_irand(0, 3)]);
            }
        }
        break;
    case entity_event_t::EV_FOOTSTEP_METAL:
        DEBUGNAME("EV_FOOTSTEP_METAL");
        if (cg_footsteps.integer) {
            if (cent->gent && cent->gent->s.number == 0 && !cg.renderingThirdPerson) //! cg_thirdPerson.integer )
            {                                                                        // Everyone else has keyframed footsteps in animsounds.cfg
                cgi_S_StartSound(nullptr, es->number, soundChannel_t::CHAN_BODY, cgs.media.footsteps[footstep_t::FOOTSTEP_METAL][Q_irand(0, 3)]);
            }
        }
        break;
    case entity_event_t::EV_FOOTSPLASH:
        DEBUGNAME("EV_FOOTSPLASH");
        if (cg_footsteps.integer) {
            cgi_S_StartSound(nullptr, es->number, soundChannel_t::CHAN_BODY, cgs.media.footsteps[footstep_t::FOOTSTEP_SPLASH][Q_irand(0, 3)]);
        }
        break;
    case entity_event_t::EV_FOOTWADE:
        DEBUGNAME("EV_FOOTWADE");
        if (cg_footsteps.integer) {
            cgi_S_StartSound(nullptr, es->number, soundChannel_t::CHAN_BODY, cgs.media.footsteps[footstep_t::FOOTSTEP_WADE][Q_irand(0, 3)]);
        }
        break;
    case entity_event_t::EV_SWIM:
        DEBUGNAME("EV_SWIM");
        if (cg_footsteps.integer) {
            cgi_S_StartSound(nullptr, es->number, soundChannel_t::CHAN_BODY, cgs.media.footsteps[footstep_t::FOOTSTEP_SWIM][Q_irand(0, 3)]);
        }
        break;

    case entity_event_t::EV_FALL_SHORT:
        DEBUGNAME("EV_FALL_SHORT");
        cgi_S_StartSound(nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.landSound);
        if (clientNum == cg.predicted_player_state.clientNum) {
            // smooth landing z changes
            cg.landChange = -8;
            cg.landTime = cg.time;
        }
        // FIXME: maybe kick up some dust?
        break;
    case entity_event_t::EV_FALL_MEDIUM:
        DEBUGNAME("EV_FALL_MEDIUM");
        // use normal pain sound -
        if (g_entities[es->number].health <= 0) { // dead
            cgi_S_StartSound(nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.landSound);
        }
        else if (g_entities[es->number].s.weapon == weapon_t::WP_SABER) { // jedi
            CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_BODY, "*land1.wav", customSoundSet_t::CS_BASIC);
        }
        else { // still alive
            CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_BODY, "*pain100.wav", customSoundSet_t::CS_BASIC);
        }
        if (clientNum == cg.predicted_player_state.clientNum) {
            // smooth landing z changes
            cg.landChange = -16;
            cg.landTime = cg.time;
        }
        // FIXME: maybe kick up some dust?
        break;
    case entity_event_t::EV_FALL_FAR:
        DEBUGNAME("EV_FALL_FAR");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_BODY, "*land1.wav", customSoundSet_t::CS_BASIC);
        cgi_S_StartSound(nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.landSound);
        cent->pe.painTime = cg.time; // don't play a pain sound right after this
        if (clientNum == cg.predicted_player_state.clientNum) {
            // smooth landing z changes
            cg.landChange = -24;
            cg.landTime = cg.time;
        }
        // FIXME: maybe kick up some dust?
        break;

    case entity_event_t::EV_STEP_4:
    case entity_event_t::EV_STEP_8:
    case entity_event_t::EV_STEP_12:
    case entity_event_t::EV_STEP_16: // smooth out step up transitions
        DEBUGNAME("EV_STEP");
        {
            float oldStep;
            int delta;
            int step;

            if (clientNum != cg.predicted_player_state.clientNum) {
                break;
            }
            // if we are interpolating, we don't need to smooth steps
            if (cg_timescale.value >= 1.0f) {
                break;
            }
            // check for stepping up before a previous step is completed
            delta = cg.time - cg.stepTime;
            if (delta < STEP_TIME) {
                oldStep = cg.stepChange * (STEP_TIME - delta) / STEP_TIME;
            }
            else {
                oldStep = 0;
            }

            // add this amount
            step = 4 * (event - entity_event_t::EV_STEP_4 + 1);
            cg.stepChange = oldStep + step;
            if (cg.stepChange > MAX_STEP_CHANGE) {
                cg.stepChange = MAX_STEP_CHANGE;
            }
            cg.stepTime = cg.time;
            break;
        }

    case entity_event_t::EV_JUMP:
        DEBUGNAME("EV_JUMP");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_AUTO, "*jump1.wav", customSoundSet_t::CS_BASIC); // CHAN_VOICE
        break;

    case entity_event_t::EV_ROLL:
        DEBUGNAME("EV_ROLL");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_AUTO, "*jump1.wav", customSoundSet_t::CS_BASIC); // CHAN_VOICE
        cgi_S_StartSound(nullptr, es->number, soundChannel_t::CHAN_BODY, cgs.media.rollSound);                           // CHAN_AUTO
        // FIXME: need some sort of body impact on ground sound and maybe kick up some dust?
        break;

    case entity_event_t::EV_WATER_TOUCH:
        DEBUGNAME("EV_WATER_TOUCH");
        cgi_S_StartSound(nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.watrInSound);
        break;

    case entity_event_t::EV_WATER_LEAVE:
        DEBUGNAME("EV_WATER_LEAVE");
        cgi_S_StartSound(nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.watrOutSound);
        break;

    case entity_event_t::EV_WATER_UNDER:
        DEBUGNAME("EV_WATER_UNDER");
        cgi_S_StartSound(nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.watrUnSound);
        break;

    case entity_event_t::EV_WATER_CLEAR:
        DEBUGNAME("EV_WATER_CLEAR");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_AUTO, "*gasp.wav", customSoundSet_t::CS_BASIC);
        break;

    case entity_event_t::EV_WATER_GURP1:
    case entity_event_t::EV_WATER_GURP2:
        DEBUGNAME("EV_WATER_GURPx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_AUTO, Q_Format("*gurp{}.wav", event - entity_event_t::EV_WATER_GURP1 + 1).c_str(),
                              customSoundSet_t::CS_BASIC);
        break;

    case entity_event_t::EV_WATER_DROWN:
        DEBUGNAME("EV_WATER_DROWN");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_AUTO, "*drown.wav", customSoundSet_t::CS_BASIC);
        break;

    case entity_event_t::EV_ITEM_PICKUP:
        DEBUGNAME("EV_ITEM_PICKUP");
        {
            gitem_t* item;
            int index;
            bool bHadItem = false;

            index = es->eventParm; // player predicted

            if ((char)index < 0) {
                index = -(char)index;
                bHadItem = true;
            }

            if (index >= bg_numItems) {
                break;
            }
            const itemNum_t itemNum = static_cast<itemNum_t>(index); // the event's parameter
            item = &bg_itemlist[itemNum];
            cgi_S_StartSound(nullptr, es->number, soundChannel_t::CHAN_AUTO, cgi_S_RegisterSound(item->pickup_sound));

            // show icon and name on status bar
            if (es->number == cg.snap->ps.clientNum) {
                CG_ItemPickup(itemNum, bHadItem);
            }
        }
        break;

    //
    // weapon events
    //
    case entity_event_t::EV_NOAMMO:
        DEBUGNAME("EV_NOAMMO");
        // cgi_S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.noAmmoSound );
        if (es->number == cg.snap->ps.clientNum) {
            CG_OutOfAmmoChange();
        }
        break;
    case entity_event_t::EV_CHANGE_WEAPON:
        DEBUGNAME("EV_CHANGE_WEAPON");
        if (es->weapon == weapon_t::WP_SABER) {
            /*
            if ( !cent->gent || !cent->gent->client || (cent->currentState.saberInFlight == false && cent->currentState.saberActive == true) )
            {
                cgi_S_StartSound (nullptr, es->number, soundChannel_t::CHAN_AUTO, cgi_S_RegisterSound( "sound/weapons/saber/saberoffquick.wav" ) );
            }
            */
            if (cent->gent && cent->gent->client) {
                // if ( cent->gent->client->ps.saberInFlight )
                { // if it's not in flight or lying around, turn it off!
                    cent->currentState.saberActive = false;
                }
            }
        }

        // FIXME: if it happens that you don't want the saber to play the switch sounds, feel free to modify this bit.
        if (weaponData[cg.weaponSelect].selectSnd[0]) {
            // custom select sound
            cgi_S_StartSound(nullptr, es->number, soundChannel_t::CHAN_AUTO, cgi_S_RegisterSound(weaponData[cg.weaponSelect].selectSnd));
        }
        else {
            // generic sound
            cgi_S_StartSound(nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.media.selectSound);
        }
        break;

    case entity_event_t::EV_FIRE_WEAPON:
        DEBUGNAME("EV_FIRE_WEAPON");
        CG_FireWeapon(cent, false);
        break;

    case entity_event_t::EV_ALT_FIRE:
        DEBUGNAME("EV_ALT_FIRE");
        CG_FireWeapon(cent, true);
        break;

    case entity_event_t::EV_DISRUPTOR_MAIN_SHOT:
        DEBUGNAME("EV_DISRUPTOR_MAIN_SHOT");
        FX_DisruptorMainShot(cent->currentState.origin2, cent->lerpOrigin);
        break;

    case entity_event_t::EV_DISRUPTOR_SNIPER_SHOT:
        DEBUGNAME("EV_DISRUPTOR_SNIPER_SHOT");
        FX_DisruptorAltShot(cent->currentState.origin2, cent->lerpOrigin, cent->gent->alt_fire);
        break;

    case entity_event_t::EV_DISRUPTOR_SNIPER_MISS:
        DEBUGNAME("EV_DISRUPTOR_SNIPER_MISS");
        FX_DisruptorAltMiss(cent->lerpOrigin, cent->gent->pos1);
        break;

    case entity_event_t::EV_DEMP2_ALT_IMPACT:
        FX_DEMP2_AltDetonate(cent->lerpOrigin, es->eventParm);
        break;

        //	case entity_event_t::EV_POWERUP_SEEKER_FIRE:
        //		DEBUGNAME("EV_POWERUP_SEEKER_FIRE");
        //		CG_FireSeeker( cent );
        //		break;

    case entity_event_t::EV_POWERUP_BATTLESUIT:
        DEBUGNAME("EV_POWERUP_BATTLESUIT");
        if (es->number == cg.snap->ps.clientNum) {
            cg.powerupActive = powerup_t::PW_BATTLESUIT;
            cg.powerupTime = cg.time;
        }
        // cgi_S_StartSound (nullptr, es->number, soundChannel_t::CHAN_ITEM, cgs.media.invulnoProtectSound );
        break;

    //=================================================================

    //
    // other events
    //
    case entity_event_t::EV_REPLICATOR:
        DEBUGNAME("EV_REPLICATOR");
        //		FX_Replicator( cent, position );
        break;

    case entity_event_t::EV_BATTERIES_CHARGED:
        cg.batteryChargeTime = cg.time + 3000;
        cgi_S_StartSound((float*)vec3_origin, es->number, soundChannel_t::CHAN_AUTO, cgs.media.batteryChargeSound);
        break;

    case entity_event_t::EV_DISINTEGRATION: {
        DEBUGNAME("EV_DISINTEGRATION");
        bool makeNotSolid = false;
        const powerup_t disintPW = static_cast<powerup_t>(es->eventParm);
        entityFlags_t disintEffect = entityFlags_t::EF_NONE;
        int disintLength = 0;
        qhandle_t disintSound1 = NULL_HANDLE;
        qhandle_t disintSound2 = NULL_HANDLE;

        switch (disintPW) {
        case powerup_t::PW_DISRUPTION:                       // sniper rifle
            disintEffect = entityFlags_t::EF_DISINTEGRATION; // ef_
            disintSound1 = cgs.media.disintegrateSound;      // with scream
            disintSound2 = cgs.media.disintegrate2Sound;     // no scream
            disintLength = 2000;
            makeNotSolid = true;
            break;
            /*			case powerup_t::PW_SHOCKED:// arc welder
                            disintEffect = EF_DISINT_1;//ef_
                            disintSound1 = nullptr;//with scream
                            disintSound2 = nullptr;//no scream
                            disintSound3 = nullptr;//with inhuman scream
                            disintLength = 4000;
                            break;
            */
        default:
            return;
            break;
        }

        if (cent->gent->owner) {
            cent->gent->owner->fx_time = cg.time;
            if (cent->gent->owner->client) {
                if (disintSound1 && disintSound2) { // play an extra sound
                    // listed all the non-humanoids, because there's a lot more humanoids
                    class_t npc_class = cent->gent->owner->client->NPC_class;
                    if (npc_class != class_t::CLASS_ATST && npc_class != class_t::CLASS_GONK && npc_class != class_t::CLASS_INTERROGATOR &&
                        npc_class != class_t::CLASS_MARK1 && npc_class != class_t::CLASS_MARK2 && npc_class != class_t::CLASS_MOUSE &&
                        npc_class != class_t::CLASS_PROBE && npc_class != class_t::CLASS_PROTOCOL && npc_class != class_t::CLASS_R2D2 &&
                        npc_class != class_t::CLASS_R5D2 && npc_class != class_t::CLASS_SEEKER &&
                        npc_class != class_t::CLASS_SENTRY) { // Only the humanoids scream
                        cgi_S_StartSound(nullptr, cent->gent->owner->s.number, soundChannel_t::CHAN_VOICE, disintSound1);
                    }
                    // no more forge or 8472
                    //	else if ( cent->gent->owner->client->playerTeam == TEAM_FORGE ||
                    //			cent->gent->owner->client->playerTeam == TEAM_8472 )
                    //	{
                    //		cgi_S_StartSound ( nullptr, cent->gent->s.number, soundChannel_t::CHAN_VOICE, disintSound3 );
                    //	}
                    else {
                        cgi_S_StartSound(nullptr, cent->gent->s.number, soundChannel_t::CHAN_AUTO, disintSound2);
                    }
                }
                cent->gent->owner->s.powerups |= Bit(disintPW);
                cent->gent->owner->client->ps.powerups[disintPW] = cg.time + disintLength;

                // Things that are being disintegrated should probably not be solid...
                if (makeNotSolid && cent->gent->owner->client->playerTeam != team_t::TEAM_NEUTRAL) {
                    cent->gent->contents = contentsFlags_t::CONTENTS_NONE;
                }
            }
            else {
                cent->gent->owner->s.eFlags = disintEffect; // FIXME: |= ?
                cent->gent->owner->delay = cg.time + disintLength;
            }
        }
    } break;

    // This does not necessarily have to be from a grenade...
    case entity_event_t::EV_GRENADE_BOUNCE:
        DEBUGNAME("EV_GRENADE_BOUNCE");
        CG_BounceEffect(cent, es->weapon, position, cent->gent->pos1);
        break;

        //
        // missile impacts
        //

    case entity_event_t::EV_MISSILE_STICK:
        DEBUGNAME("EV_MISSILE_STICK");
        CG_MissileStick(cent, es->weapon, position);
        break;

    case entity_event_t::EV_MISSILE_HIT:
        DEBUGNAME("EV_MISSILE_HIT");
        CG_MissileHitPlayer(cent, es->weapon, position, cent->gent->pos1, cent->gent->alt_fire);
        break;

    case entity_event_t::EV_MISSILE_MISS:
        DEBUGNAME("EV_MISSILE_MISS");
        CG_MissileHitWall(cent, es->weapon, position, cent->gent->pos1, cent->gent->alt_fire);
        break;

    case entity_event_t::EV_BMODEL_SOUND:
        DEBUGNAME("EV_BMODEL_SOUND");
        cgi_S_StartSound(nullptr, es->number, soundChannel_t::CHAN_AUTO, es->eventParm);
        break;

    case entity_event_t::EV_GENERAL_SOUND:
        DEBUGNAME("EV_GENERAL_SOUND");
        if (cgs.sound_precache[es->eventParm]) {
            cgi_S_StartSound(nullptr, es->number, soundChannel_t::CHAN_AUTO, cgs.sound_precache[es->eventParm]);
        }
        else {
            s = CG_ConfigString(CS_SOUNDS + es->eventParm);
            CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_AUTO, s, customSoundSet_t::CS_BASIC);
        }
        break;

    case entity_event_t::EV_GLOBAL_SOUND: // play from the player's head so it never diminishes
        DEBUGNAME("EV_GLOBAL_SOUND");
        if (cgs.sound_precache[es->eventParm]) {
            cgi_S_StartSound(nullptr, cg.snap->ps.clientNum, soundChannel_t::CHAN_AUTO, cgs.sound_precache[es->eventParm]);
        }
        else {
            s = CG_ConfigString(CS_SOUNDS + es->eventParm);
            CG_TryPlayCustomSound(nullptr, cg.snap->ps.clientNum, soundChannel_t::CHAN_AUTO, s, customSoundSet_t::CS_BASIC);
        }
        break;

    case entity_event_t::EV_DRUGGED:
        DEBUGNAME("EV_DRUGGED");
        if (cent->gent && cent->gent->owner && cent->gent->owner->s.number == 0) {
            // Only allow setting up the wonky vision on the player..do it for 10 seconds...must be synchronized with calcs done in cg_view.  Just search for
            // cg.wonkyTime to find 'em.
            cg.wonkyTime = cg.time + 10000;
        }
        break;

    case entity_event_t::EV_PAIN: {
        const char* snd;
        const int health = es->eventParm;

        if (cent->gent && cent->gent->NPC && Any(cent->gent->NPC->aiFlags & npcAiFlags_t::NPCAI_DIE_ON_IMPACT)) {
            return;
        }
        // FIXME: don't do this if we're falling to our deaths...
        DEBUGNAME("EV_PAIN");
        // don't do more than two pain sounds a second
        if (cg.time - cent->pe.painTime < 500) {
            return;
        }

        if (health < 25) {
            snd = "*pain100.wav";
        }
        else if (health < 50) {
            snd = "*pain75.wav";
        }
        else if (health < 75) {
            snd = "*pain50.wav";
        }
        else {
            snd = "*pain25.wav";
        }
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, snd, customSoundSet_t::CS_BASIC);

        // save pain time for programitic twitch animation
        cent->pe.painTime = cg.time;
        cent->pe.painDirection ^= 1;
    } break;

    case entity_event_t::EV_DEATH1:
    case entity_event_t::EV_DEATH2:
    case entity_event_t::EV_DEATH3:
        DEBUGNAME("EV_DEATHx");
        /*
        if ( cent->gent && cent->gent->NPC && Any(cent->gent->NPC->aiFlags & npcAiFlags_t::NPCAI_DIE_ON_IMPACT) )
        {
            return;
        }
        */
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*death{}.wav", event - entity_event_t::EV_DEATH1 + 1).c_str(),
                              customSoundSet_t::CS_BASIC);
        break;

    // Called by the FxRunner entity...usually for Environmental FX Events
    case entity_event_t::EV_PLAY_EFFECT:
        DEBUGNAME("EV_PLAY_EFFECT");
        s = CG_ConfigString(CS_EFFECTS + es->eventParm);
        // Ghoul2 Insert Start
        if (es->boltInfo != 0) {
            theFxScheduler.PlayEffect(s, cent->lerpOrigin, axis, es->boltInfo, -1);
        }
        else {
            VectorCopy(cent->gent->pos3, axis[0]);
            VectorCopy(cent->gent->pos4, axis[1]);
            CrossProduct(axis[0], axis[1], axis[2]);

            // the entNum the effect may be attached to
            if (es->otherEntityNum) {
                theFxScheduler.PlayEffect(s, cent->lerpOrigin, axis, -1, es->otherEntityNum);
            }
            else {
                theFxScheduler.PlayEffect(s, cent->lerpOrigin, axis, -1, -1);
            }
        }
        // Ghoul2 Insert End
        break;

        // play an effect bolted onto a muzzle
    case entity_event_t::EV_PLAY_MUZZLE_EFFECT:
        DEBUGNAME("EV_PLAY_MUZZLE_EFFECT");
        s = CG_ConfigString(CS_EFFECTS + es->eventParm);

        theFxScheduler.PlayEffect(s, es->otherEntityNum);
        break;

    case entity_event_t::EV_TARGET_BEAM_DRAW:
        DEBUGNAME("EV_TARGET_BEAM_DRAW");
        if (cent->gent) {
            s = CG_ConfigString(CS_EFFECTS + es->eventParm);

            if (s && s[0]) {
                if (cent->gent->delay) {
                    s2 = CG_ConfigString(CS_EFFECTS + cent->gent->delay);
                }
                else {
                    s2 = nullptr;
                }

                CG_DrawTargetBeam(cent->lerpOrigin, cent->gent->s.origin2, cent->gent->pos1, s, s2);
            }
            /*			else
                        {
                            int gack = 0; // this is bad if it get's here
                        }
            */
        }
        break;

    case entity_event_t::EV_ANGER1: // Say when acquire an enemy when didn't have one before
    case entity_event_t::EV_ANGER2:
    case entity_event_t::EV_ANGER3:
        DEBUGNAME("EV_ANGERx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*anger{}.wav", event - entity_event_t::EV_ANGER1 + 1).c_str(),
                              customSoundSet_t::CS_COMBAT);
        break;

    case entity_event_t::EV_VICTORY1: // Say when killed an enemy
    case entity_event_t::EV_VICTORY2:
    case entity_event_t::EV_VICTORY3:
        DEBUGNAME("EV_VICTORYx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*victory{}.wav", event - entity_event_t::EV_VICTORY1 + 1).c_str(),
                              customSoundSet_t::CS_COMBAT);
        break;

    case entity_event_t::EV_CONFUSE1: // Say when confused
    case entity_event_t::EV_CONFUSE2:
    case entity_event_t::EV_CONFUSE3:
        DEBUGNAME("EV_CONFUSEDx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*confuse{}.wav", event - entity_event_t::EV_CONFUSE1 + 1).c_str(),
                              customSoundSet_t::CS_COMBAT);
        break;

    case entity_event_t::EV_PUSHED1: // Say when pushed
    case entity_event_t::EV_PUSHED2:
    case entity_event_t::EV_PUSHED3:
        DEBUGNAME("EV_PUSHEDx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*pushed{}.wav", event - entity_event_t::EV_PUSHED1 + 1).c_str(),
                              customSoundSet_t::CS_COMBAT);
        break;

    case entity_event_t::EV_CHOKE1: // Say when choking
    case entity_event_t::EV_CHOKE2:
    case entity_event_t::EV_CHOKE3:
        DEBUGNAME("EV_CHOKEx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*choke{}.wav", event - entity_event_t::EV_CHOKE1 + 1).c_str(),
                              customSoundSet_t::CS_COMBAT);
        break;

    case entity_event_t::EV_FFWARN: // Warn ally to stop shooting you
        DEBUGNAME("EV_FFWARN");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, "*ffwarn.wav", customSoundSet_t::CS_COMBAT);
        break;

    case entity_event_t::EV_FFTURN: // Turn on ally after being shot by them
        DEBUGNAME("EV_FFTURN");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, "*ffturn.wav", customSoundSet_t::CS_COMBAT);
        break;

    // extra sounds for ST
    case entity_event_t::EV_CHASE1:
    case entity_event_t::EV_CHASE2:
    case entity_event_t::EV_CHASE3:
        DEBUGNAME("EV_CHASEx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*chase{}.wav", event - entity_event_t::EV_CHASE1 + 1).c_str(),
                              customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_COVER1:
    case entity_event_t::EV_COVER2:
    case entity_event_t::EV_COVER3:
    case entity_event_t::EV_COVER4:
    case entity_event_t::EV_COVER5:
        DEBUGNAME("EV_COVERx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*cover{}.wav", event - entity_event_t::EV_COVER1 + 1).c_str(),
                              customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_DETECTED1:
    case entity_event_t::EV_DETECTED2:
    case entity_event_t::EV_DETECTED3:
    case entity_event_t::EV_DETECTED4:
    case entity_event_t::EV_DETECTED5:
        DEBUGNAME("EV_DETECTEDx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*detected{}.wav", event - entity_event_t::EV_DETECTED1 + 1).c_str(),
                              customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_GIVEUP1:
    case entity_event_t::EV_GIVEUP2:
    case entity_event_t::EV_GIVEUP3:
    case entity_event_t::EV_GIVEUP4:
        DEBUGNAME("EV_GIVEUPx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*giveup{}.wav", event - entity_event_t::EV_GIVEUP1 + 1).c_str(),
                              customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_LOOK1:
    case entity_event_t::EV_LOOK2:
        DEBUGNAME("EV_LOOKx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*look{}.wav", event - entity_event_t::EV_LOOK1 + 1).c_str(),
                              customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_LOST1:
        DEBUGNAME("EV_LOST1");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, "*lost1.wav", customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_OUTFLANK1:
    case entity_event_t::EV_OUTFLANK2:
        DEBUGNAME("EV_OUTFLANKx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*outflank{}.wav", event - entity_event_t::EV_OUTFLANK1 + 1).c_str(),
                              customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_ESCAPING1:
    case entity_event_t::EV_ESCAPING2:
    case entity_event_t::EV_ESCAPING3:
        DEBUGNAME("EV_ESCAPINGx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*escaping{}.wav", event - entity_event_t::EV_ESCAPING1 + 1).c_str(),
                              customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_SIGHT1:
    case entity_event_t::EV_SIGHT2:
    case entity_event_t::EV_SIGHT3:
        DEBUGNAME("EV_SIGHTx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*sight{}.wav", event - entity_event_t::EV_SIGHT1 + 1).c_str(),
                              customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_SOUND1:
    case entity_event_t::EV_SOUND2:
    case entity_event_t::EV_SOUND3:
        DEBUGNAME("EV_SOUNDx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*sound{}.wav", event - entity_event_t::EV_SOUND1 + 1).c_str(),
                              customSoundSet_t::CS_EXTRA);
        break;
    case entity_event_t::EV_SUSPICIOUS1:
    case entity_event_t::EV_SUSPICIOUS2:
    case entity_event_t::EV_SUSPICIOUS3:
    case entity_event_t::EV_SUSPICIOUS4:
    case entity_event_t::EV_SUSPICIOUS5:
        DEBUGNAME("EV_SUSPICIOUSx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE,
                              Q_Format("*suspicious{}.wav", event - entity_event_t::EV_SUSPICIOUS1 + 1).c_str(), customSoundSet_t::CS_EXTRA);
        break;
    // extra sounds for Jedi
    case entity_event_t::EV_COMBAT1:
    case entity_event_t::EV_COMBAT2:
    case entity_event_t::EV_COMBAT3:
        DEBUGNAME("EV_COMBATx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*combat{}.wav", event - entity_event_t::EV_COMBAT1 + 1).c_str(),
                              customSoundSet_t::CS_JEDI);
        break;
    case entity_event_t::EV_JDETECTED1:
    case entity_event_t::EV_JDETECTED2:
    case entity_event_t::EV_JDETECTED3:
        DEBUGNAME("EV_JDETECTEDx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*jdetected{}.wav", event - entity_event_t::EV_JDETECTED1 + 1).c_str(),
                              customSoundSet_t::CS_JEDI);
        break;
    case entity_event_t::EV_TAUNT1:
    case entity_event_t::EV_TAUNT2:
    case entity_event_t::EV_TAUNT3:
        DEBUGNAME("EV_TAUNTx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*taunt{}.wav", event - entity_event_t::EV_TAUNT1 + 1).c_str(),
                              customSoundSet_t::CS_JEDI);
        break;
    case entity_event_t::EV_JCHASE1:
    case entity_event_t::EV_JCHASE2:
    case entity_event_t::EV_JCHASE3:
        DEBUGNAME("EV_JCHASEx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*jchase{}.wav", event - entity_event_t::EV_JCHASE1 + 1).c_str(),
                              customSoundSet_t::CS_JEDI);
        break;
    case entity_event_t::EV_JLOST1:
    case entity_event_t::EV_JLOST2:
    case entity_event_t::EV_JLOST3:
        DEBUGNAME("EV_JLOSTx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*jlost{}.wav", event - entity_event_t::EV_JLOST1 + 1).c_str(),
                              customSoundSet_t::CS_JEDI);
        break;
    case entity_event_t::EV_DEFLECT1:
    case entity_event_t::EV_DEFLECT2:
    case entity_event_t::EV_DEFLECT3:
        DEBUGNAME("EV_DEFLECTx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*deflect{}.wav", event - entity_event_t::EV_DEFLECT1 + 1).c_str(),
                              customSoundSet_t::CS_JEDI);
        break;
    case entity_event_t::EV_GLOAT1:
    case entity_event_t::EV_GLOAT2:
    case entity_event_t::EV_GLOAT3:
        DEBUGNAME("EV_GLOATx");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, Q_Format("*gloat{}.wav", event - entity_event_t::EV_GLOAT1 + 1).c_str(),
                              customSoundSet_t::CS_JEDI);
        break;
    case entity_event_t::EV_PUSHFAIL:
        DEBUGNAME("EV_PUSHFAIL");
        CG_TryPlayCustomSound(nullptr, es->number, soundChannel_t::CHAN_VOICE, "*pushfail.wav", customSoundSet_t::CS_JEDI);
        break;

    case entity_event_t::EV_USE_FORCE:
        DEBUGNAME("EV_USE_FORCEITEM");
        CG_UseForce(cent);
        break;

    case entity_event_t::EV_USE_ITEM:
        DEBUGNAME("EV_USE_ITEM");
        CG_UseItem(cent);
        break;

    case entity_event_t::EV_USE_INV_BINOCULARS:
        DEBUGNAME("EV_USE_INV_BINOCULARS");
        UseItem(inventory_t::INV_ELECTROBINOCULARS);
        break;

    case entity_event_t::EV_USE_INV_BACTA:
        DEBUGNAME("EV_USE_INV_BACTA");
        UseItem(inventory_t::INV_BACTA_CANISTER);
        break;

    case entity_event_t::EV_USE_INV_SEEKER:
        DEBUGNAME("EV_USE_INV_SEEKER");
        UseItem(inventory_t::INV_SEEKER);
        break;

    case entity_event_t::EV_USE_INV_LIGHTAMP_GOGGLES:
        DEBUGNAME("EV_USE_INV_LIGHTAMP_GOGGLES");
        UseItem(inventory_t::INV_LIGHTAMP_GOGGLES);
        break;

    case entity_event_t::EV_USE_INV_SENTRY:
        DEBUGNAME("EV_USE_INV_SENTRY");
        UseItem(inventory_t::INV_SENTRY);
        break;

    case entity_event_t::EV_DEBUG_LINE:
        DEBUGNAME("EV_DEBUG_LINE");
        CG_TestLine(position, es->origin2, es->time, (unsigned int)(es->time2), ES_WeaponParm(*es));
        break;

    default:
        DEBUGNAME("UNKNOWN");
        Q_Error(errorParm_t::ERR_DROP, "Unknown event: {}", event);
        break;
    }
}

/*
==============
CG_CheckEvents

==============
*/
void CG_CheckEvents(centity_t* cent)
{
    // check for event-only entities
    if (cent->currentState.eType > entityType_t::ET_EVENTS) {
        if (cent->previousEvent) {
            return; // already fired
        }
        cent->previousEvent = 1;

        cent->currentState.event = BG_EventEntityEvent(cent->currentState.eType);
    }
    else {
        // check for events riding with another entity
        if (cent->currentState.event == cent->previousEvent) {
            return;
        }
        cent->previousEvent = cent->currentState.event;
        if (BG_UnpackEvent(cent->currentState.event) == entity_event_t::EV_NONE) {
            return;
        }
    }

    // calculate the position at exactly the frame time
    EvaluateTrajectory(&cent->currentState.pos, cg.snap->serverTime, cent->lerpOrigin);
    CG_SetEntitySoundPosition(cent);

    CG_EntityEvent(cent, cent->lerpOrigin);
}

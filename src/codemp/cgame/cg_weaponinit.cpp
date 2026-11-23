/*
===========================================================================
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

//
// cg_weaponinit.cpp -- events and effects dealing with weapons
#include "cg_local.h"
#include "fx_local.h"

/*
=================
CG_RegisterWeapon

The server says this item is used on this level
=================
*/
void CG_RegisterWeapon(weapon_t weaponNum)
{
    weaponInfo_t* weaponInfo;
    gitem_t *item, *ammo;
    char path[MAX_QPATH];
    vec3_t mins, maxs;
    int i;

    if (weaponNum <= weapon_t::WP_NONE || weaponNum >= weapon_t::WP_NUM_WEAPONS) {
        return;
    }

    weaponInfo = &cg_weapons[weaponNum];

    if (weaponInfo->registered) {
        return;
    }

    // if ( cgs.wDisable & (1<<weaponNum) )
    //	return;

    memset(weaponInfo, 0, sizeof(*weaponInfo));
    weaponInfo->registered = true;

    for (item = bg_itemlist + 1; item->classname; item++) {
        if (item->giType == itemType_t::IT_WEAPON && GI_Weapon(*item) == weaponNum) {
            weaponInfo->item = item;
            break;
        }
    }
    if (!item->classname) {
        Q_Error(errorParm_t::ERR_DROP, "Couldn't find weapon {}", weaponNum);
    }
    CG_RegisterItemVisuals(item - bg_itemlist);

    // load cmodel before model so filecache works
    weaponInfo->weaponModel = trap->R_RegisterModel(item->world_model[0]);
    // load in-view model also
    weaponInfo->viewModel = trap->R_RegisterModel(item->view_model);

    // calc midpoint for rotation
    trap->R_ModelBounds(weaponInfo->weaponModel, mins, maxs);
    for (i = 0; i < 3; i++) {
        weaponInfo->weaponMidpoint[i] = mins[i] + 0.5 * (maxs[i] - mins[i]);
    }

    weaponInfo->weaponIcon = trap->R_RegisterShader(item->icon);
    weaponInfo->ammoIcon = trap->R_RegisterShader(item->icon);

    for (ammo = bg_itemlist + 1; ammo->classname; ammo++) {
        if (ammo->giType == itemType_t::IT_AMMO && GI_Ammo(*ammo) == weaponData[weaponNum].ammoIndex) {
            break;
        }
    }
    if (ammo->classname && ammo->world_model[0]) {
        weaponInfo->ammoModel = trap->R_RegisterModel(ammo->world_model[0]);
    }

    //	strcpy( path, item->view_model );
    //	COM_StripExtension( path, path );
    //	strcat( path, "_flash.md3" );
    weaponInfo->flashModel = 0; // trap->R_RegisterModel( path );

    if (weaponNum == weapon_t::WP_DISRUPTOR || weaponNum == weapon_t::WP_FLECHETTE || weaponNum == weapon_t::WP_REPEATER ||
        weaponNum == weapon_t::WP_ROCKET_LAUNCHER || weaponNum == weapon_t::WP_CONCUSSION) {
        Q_strncpyz(path, item->view_model, sizeof(path));
        COM_StripExtension(path, path, sizeof(path));
        Q_strcat(path, sizeof(path), "_barrel.md3");
        weaponInfo->barrelModel = trap->R_RegisterModel(path);
    }
    else if (weaponNum == weapon_t::WP_STUN_BATON) { // only weapon with more than 1 barrel..
        trap->R_RegisterModel("models/weapons2/stun_baton/baton_barrel.md3");
        trap->R_RegisterModel("models/weapons2/stun_baton/baton_barrel2.md3");
        trap->R_RegisterModel("models/weapons2/stun_baton/baton_barrel3.md3");
    }
    else {
        weaponInfo->barrelModel = 0;
    }

    if (weaponNum != weapon_t::WP_SABER) {
        Q_strncpyz(path, item->view_model, sizeof(path));
        COM_StripExtension(path, path, sizeof(path));
        Q_strcat(path, sizeof(path), "_hand.md3");
        weaponInfo->handsModel = trap->R_RegisterModel(path);
    }
    else {
        weaponInfo->handsModel = 0;
    }

    //	if ( !weaponInfo->handsModel ) {
    //		weaponInfo->handsModel = trap->R_RegisterModel( "models/weapons2/shotgun/shotgun_hand.md3" );
    //	}

    switch (weaponNum) {
    case weapon_t::WP_STUN_BATON:
    case weapon_t::WP_MELEE:
        /*		MAKERGB( weaponInfo->flashDlightColor, 0.6f, 0.6f, 1.0f );
                weaponInfo->firingSound = trap->sound->Register( "sound/weapons/saber/saberhum.wav" );
        //		weaponInfo->flashSound[0] = trap->sound->Register( "sound/weapons/melee/fstatck.wav" );
        */
        // trap->R_RegisterShader( "gfx/effects/stunPass" );
        trap->FX_RegisterEffect("stunBaton/flesh_impact");

        if (weaponNum == weapon_t::WP_STUN_BATON) {
            trap->sound->Register("sound/weapons/baton/idle.wav");
            weaponInfo->flashSound[0] = trap->sound->Register("sound/weapons/baton/fire.mp3");
            weaponInfo->altFlashSound[0] = trap->sound->Register("sound/weapons/baton/fire.mp3");
        }
        else {
            /*
            int j = 0;

            while (j < 4)
            {
                weaponInfo->flashSound[j] = trap->sound->Register( Q_Format("sound/weapons/melee/swing{}", j+1).c_str() );
                weaponInfo->altFlashSound[j] = weaponInfo->flashSound[j];
                j++;
            }
            */
            // No longer needed, animsound config plays them for us
        }
        break;
    case weapon_t::WP_SABER:
        MAKERGB(weaponInfo->flashDlightColor, 0.6f, 0.6f, 1.0f);
        weaponInfo->firingSound = trap->sound->Register("sound/weapons/saber/saberhum1.wav");
        weaponInfo->missileModel = trap->R_RegisterModel(DEFAULT_SABER_MODEL);
        break;

    case weapon_t::WP_CONCUSSION:
        weaponInfo->selectSound = trap->sound->Register("sound/weapons/concussion/select.wav");

        weaponInfo->flashSound[0] = NULL_SOUND;
        weaponInfo->firingSound = NULL_SOUND;
        weaponInfo->chargeSound = NULL_SOUND;
        weaponInfo->muzzleEffect = trap->FX_RegisterEffect("concussion/muzzle_flash");
        weaponInfo->missileModel = NULL_HANDLE;
        weaponInfo->missileSound = NULL_SOUND;
        weaponInfo->missileDlight = 0;
        // weaponInfo->missileDlightColor= {0,0,0};
        weaponInfo->missileHitSound = NULL_SOUND;
        weaponInfo->missileTrailFunc = FX_ConcussionProjectileThink;

        weaponInfo->altFlashSound[0] = NULL_SOUND;
        weaponInfo->altFiringSound = NULL_SOUND;
        weaponInfo->altChargeSound = trap->sound->Register("sound/weapons/bryar/altcharge.wav");
        weaponInfo->altMuzzleEffect = trap->FX_RegisterEffect("concussion/altmuzzle_flash");
        weaponInfo->altMissileModel = NULL_HANDLE;
        weaponInfo->altMissileSound = NULL_SOUND;
        weaponInfo->altMissileDlight = 0;
        // weaponInfo->altMissileDlightColor= {0,0,0};
        weaponInfo->altMissileHitSound = NULL_SOUND;
        weaponInfo->altMissileTrailFunc = FX_ConcussionProjectileThink;

        cgs.effects.disruptorAltMissEffect = trap->FX_RegisterEffect("disruptor/alt_miss");

        cgs.effects.concussionShotEffect = trap->FX_RegisterEffect("concussion/shot");
        cgs.effects.concussionImpactEffect = trap->FX_RegisterEffect("concussion/explosion");
        trap->R_RegisterShader("gfx/effects/blueLine");
        trap->R_RegisterShader("gfx/misc/whiteline2");
        break;

    case weapon_t::WP_BRYAR_PISTOL:
    case weapon_t::WP_BRYAR_OLD:
        weaponInfo->selectSound = trap->sound->Register("sound/weapons/bryar/select.wav");

        // the shot's sound is its muzzle effect's (effects/bryar/muzzle_flash.efx plays fire): none of its own, or it plays twice
        weaponInfo->firingSound = NULL_SOUND;
        weaponInfo->chargeSound = NULL_SOUND;
        weaponInfo->muzzleEffect = trap->FX_RegisterEffect("bryar/muzzle_flash");
        weaponInfo->missileModel = NULL_HANDLE;
        weaponInfo->missileSound = NULL_SOUND;
        weaponInfo->missileDlight = 0;
        // weaponInfo->missileDlightColor= {0,0,0};
        weaponInfo->missileHitSound = NULL_SOUND;
        weaponInfo->missileTrailFunc = FX_BryarProjectileThink;

        // the shot's sound is its muzzle effect's (effects/bryar/altmuzzle_flash.efx plays alt_fire): none of its own, or it plays twice
        weaponInfo->altFiringSound = NULL_SOUND;
        weaponInfo->altChargeSound = trap->sound->Register("sound/weapons/bryar/altcharge.wav");
        weaponInfo->altMuzzleEffect = trap->FX_RegisterEffect("bryar/altmuzzle_flash");
        weaponInfo->altMissileModel = NULL_HANDLE;
        weaponInfo->altMissileSound = NULL_SOUND;
        weaponInfo->altMissileDlight = 0;
        // weaponInfo->altMissileDlightColor= {0,0,0};
        weaponInfo->altMissileHitSound = NULL_SOUND;
        weaponInfo->altMissileTrailFunc = FX_BryarAltProjectileThink;

        cgs.effects.bryarShotEffect = trap->FX_RegisterEffect("bryar/shot");
        cgs.effects.bryarPowerupShotEffect = trap->FX_RegisterEffect("bryar/crackleShot");
        cgs.effects.bryarWallImpactEffect = trap->FX_RegisterEffect("bryar/wall_impact");
        cgs.effects.bryarWallImpactEffect2 = trap->FX_RegisterEffect("bryar/wall_impact2");
        cgs.effects.bryarWallImpactEffect3 = trap->FX_RegisterEffect("bryar/wall_impact3");
        cgs.effects.bryarFleshImpactEffect = trap->FX_RegisterEffect("bryar/flesh_impact");
        cgs.effects.bryarDroidImpactEffect = trap->FX_RegisterEffect("bryar/droid_impact");

        cgs.media.bryarFrontFlash = trap->R_RegisterShader("gfx/effects/bryarFrontFlash");

        // Note these are temp shared effects
        trap->FX_RegisterEffect("blaster/wall_impact.efx");
        trap->FX_RegisterEffect("blaster/flesh_impact.efx");

        break;

    case weapon_t::WP_BLASTER:
    case weapon_t::WP_EMPLACED_GUN: // rww - just use the same as this for now..
        weaponInfo->selectSound = trap->sound->Register("sound/weapons/blaster/select.wav");

        // the shot's sound is its muzzle effect's (effects/blaster/muzzle_flash.efx plays fire): none of its own, or it plays twice
        weaponInfo->firingSound = NULL_SOUND;
        weaponInfo->chargeSound = NULL_SOUND;
        weaponInfo->muzzleEffect = trap->FX_RegisterEffect("blaster/muzzle_flash");
        weaponInfo->missileModel = NULL_HANDLE;
        weaponInfo->missileSound = NULL_SOUND;
        weaponInfo->missileDlight = 0;
        //		weaponInfo->missileDlightColor	= {0,0,0};
        weaponInfo->missileHitSound = NULL_SOUND;
        weaponInfo->missileTrailFunc = FX_BlasterProjectileThink;

        // the shot's sound is its muzzle effect's (effects/blaster/altmuzzle_flash.efx plays alt_fire): none of its own, or it plays twice
        weaponInfo->altFiringSound = NULL_SOUND;
        weaponInfo->altChargeSound = NULL_SOUND;
        weaponInfo->altMuzzleEffect = trap->FX_RegisterEffect("blaster/altmuzzle_flash");
        weaponInfo->altMissileModel = NULL_HANDLE;
        weaponInfo->altMissileSound = NULL_SOUND;
        weaponInfo->altMissileDlight = 0;
        //		weaponInfo->altMissileDlightColor= {0,0,0};
        weaponInfo->altMissileHitSound = NULL_SOUND;
        weaponInfo->altMissileTrailFunc = FX_BlasterProjectileThink;

        trap->FX_RegisterEffect("blaster/deflect");
        cgs.effects.blasterShotEffect = trap->FX_RegisterEffect("blaster/shot");
        cgs.effects.blasterWallImpactEffect = trap->FX_RegisterEffect("blaster/wall_impact");
        cgs.effects.blasterFleshImpactEffect = trap->FX_RegisterEffect("blaster/flesh_impact");
        cgs.effects.blasterDroidImpactEffect = trap->FX_RegisterEffect("blaster/droid_impact");
        break;

    case weapon_t::WP_DISRUPTOR:
        weaponInfo->selectSound = trap->sound->Register("sound/weapons/disruptor/select.wav");

        // the shot's sound is its muzzle effect's (effects/disruptor/muzzle_flash.efx plays fire): none of its own, or it plays twice
        weaponInfo->firingSound = NULL_SOUND;
        weaponInfo->chargeSound = NULL_SOUND;
        weaponInfo->muzzleEffect = trap->FX_RegisterEffect("disruptor/muzzle_flash");
        weaponInfo->missileModel = NULL_HANDLE;
        weaponInfo->missileSound = NULL_SOUND;
        weaponInfo->missileDlight = 0;
        //		weaponInfo->missileDlightColor	= {0,0,0};
        weaponInfo->missileHitSound = NULL_SOUND;
        weaponInfo->missileTrailFunc = nullptr;

        // the shot's sound is its muzzle effect's (effects/disruptor/altmuzzle_flash.efx plays alt_fire): none of its own, or it plays twice
        weaponInfo->altFiringSound = NULL_SOUND;
        weaponInfo->altChargeSound = trap->sound->Register("sound/weapons/disruptor/altCharge.wav");
        weaponInfo->altMuzzleEffect = trap->FX_RegisterEffect("disruptor/altmuzzle_flash");
        weaponInfo->altMissileModel = NULL_HANDLE;
        weaponInfo->altMissileSound = NULL_SOUND;
        weaponInfo->altMissileDlight = 0;
        //		weaponInfo->altMissileDlightColor= {0,0,0};
        weaponInfo->altMissileHitSound = NULL_SOUND;
        weaponInfo->altMissileTrailFunc = nullptr;

        cgs.effects.disruptorRingsEffect = trap->FX_RegisterEffect("disruptor/rings");
        cgs.effects.disruptorProjectileEffect = trap->FX_RegisterEffect("disruptor/projectile");
        cgs.effects.disruptorWallImpactEffect = trap->FX_RegisterEffect("disruptor/wall_impact");
        cgs.effects.disruptorFleshImpactEffect = trap->FX_RegisterEffect("disruptor/flesh_impact");
        cgs.effects.disruptorAltMissEffect = trap->FX_RegisterEffect("disruptor/alt_miss");
        cgs.effects.disruptorAltHitEffect = trap->FX_RegisterEffect("disruptor/alt_hit");

        trap->R_RegisterShader("gfx/effects/redLine");
        trap->R_RegisterShader("gfx/misc/whiteline2");
        trap->R_RegisterShader("gfx/effects/smokeTrail");

        trap->sound->Register("sound/weapons/disruptor/zoomstart.wav");
        trap->sound->Register("sound/weapons/disruptor/zoomend.wav");

        // Disruptor gun zoom interface
        cgs.media.disruptorMask = trap->R_RegisterShader("gfx/2d/cropCircle2");
        cgs.media.disruptorInsert = trap->R_RegisterShader("gfx/2d/cropCircle");
        cgs.media.disruptorLight = trap->R_RegisterShader("gfx/2d/cropCircleGlow");
        cgs.media.disruptorInsertTick = trap->R_RegisterShader("gfx/2d/insertTick");
        cgs.media.disruptorChargeShader = trap->R_RegisterShaderNoMip("gfx/2d/crop_charge");

        cgs.media.disruptorZoomLoop = trap->sound->Register("sound/weapons/disruptor/zoomloop.wav");
        break;

    case weapon_t::WP_BOWCASTER:
        weaponInfo->selectSound = trap->sound->Register("sound/weapons/bowcaster/select.wav");

        // the shot's sound is its muzzle effect's (effects/bowcaster/muzzle_flash.efx plays fire): none of its own, or it plays twice
        weaponInfo->altFiringSound = NULL_SOUND;
        weaponInfo->altChargeSound = NULL_SOUND;
        weaponInfo->altMuzzleEffect = trap->FX_RegisterEffect("bowcaster/muzzle_flash");
        weaponInfo->altMissileModel = NULL_HANDLE;
        weaponInfo->altMissileSound = NULL_SOUND;
        weaponInfo->altMissileDlight = 0;
        //		weaponInfo->altMissileDlightColor	= {0,0,0};
        weaponInfo->altMissileHitSound = NULL_SOUND;
        weaponInfo->altMissileTrailFunc = FX_BowcasterProjectileThink;

        // the shot's sound is its muzzle effect's (effects/bowcaster/muzzle_flash.efx plays fire): none of its own, or it plays twice
        weaponInfo->firingSound = NULL_SOUND;
        weaponInfo->chargeSound = trap->sound->Register("sound/weapons/bowcaster/altcharge.wav");
        weaponInfo->muzzleEffect = trap->FX_RegisterEffect("bowcaster/muzzle_flash");
        weaponInfo->missileModel = NULL_HANDLE;
        weaponInfo->missileSound = NULL_SOUND;
        weaponInfo->missileDlight = 0;
        //		weaponInfo->missileDlightColor= {0,0,0};
        weaponInfo->missileHitSound = NULL_SOUND;
        weaponInfo->missileTrailFunc = FX_BowcasterAltProjectileThink;

        cgs.effects.bowcasterShotEffect = trap->FX_RegisterEffect("bowcaster/shot");
        cgs.effects.bowcasterImpactEffect = trap->FX_RegisterEffect("bowcaster/explosion");

        trap->FX_RegisterEffect("bowcaster/deflect");

        cgs.media.greenFrontFlash = trap->R_RegisterShader("gfx/effects/greenFrontFlash");
        break;

    case weapon_t::WP_REPEATER:
        weaponInfo->selectSound = trap->sound->Register("sound/weapons/repeater/select.wav");

        // the shot's sound is its muzzle effect's (effects/repeater/muzzle_flash.efx plays fire): none of its own, or it plays twice
        weaponInfo->firingSound = NULL_SOUND;
        weaponInfo->chargeSound = NULL_SOUND;
        weaponInfo->muzzleEffect = trap->FX_RegisterEffect("repeater/muzzle_flash");
        weaponInfo->missileModel = NULL_HANDLE;
        weaponInfo->missileSound = NULL_SOUND;
        weaponInfo->missileDlight = 0;
        //		weaponInfo->missileDlightColor	= {0,0,0};
        weaponInfo->missileHitSound = NULL_SOUND;
        weaponInfo->missileTrailFunc = FX_RepeaterProjectileThink;

        // the shot's sound is its muzzle effect's (effects/repeater/altmuzzle_flash.efx plays alt_fire): none of its own, or it plays twice
        weaponInfo->altFiringSound = NULL_SOUND;
        weaponInfo->altChargeSound = NULL_SOUND;
        weaponInfo->altMuzzleEffect = trap->FX_RegisterEffect("repeater/altmuzzle_flash");
        weaponInfo->altMissileModel = NULL_HANDLE;
        weaponInfo->altMissileSound = NULL_SOUND;
        weaponInfo->altMissileDlight = 0;
        //		weaponInfo->altMissileDlightColor= {0,0,0};
        weaponInfo->altMissileHitSound = NULL_SOUND;
        weaponInfo->altMissileTrailFunc = FX_RepeaterAltProjectileThink;

        cgs.effects.repeaterProjectileEffect = trap->FX_RegisterEffect("repeater/projectile");
        cgs.effects.repeaterAltProjectileEffect = trap->FX_RegisterEffect("repeater/alt_projectile");
        cgs.effects.repeaterWallImpactEffect = trap->FX_RegisterEffect("repeater/wall_impact");
        cgs.effects.repeaterFleshImpactEffect = trap->FX_RegisterEffect("repeater/flesh_impact");
        // cgs.effects.repeaterAltWallImpactEffect	= trap->FX_RegisterEffect( "repeater/alt_wall_impact" );
        cgs.effects.repeaterAltWallImpactEffect = trap->FX_RegisterEffect("repeater/concussion");
        break;

    case weapon_t::WP_DEMP2:
        weaponInfo->selectSound = trap->sound->Register("sound/weapons/demp2/select.wav");

        // the shot's sound is its muzzle effect's (effects/demp2/muzzle_flash.efx plays fire): none of its own, or it plays twice
        weaponInfo->firingSound = NULL_SOUND;
        weaponInfo->chargeSound = NULL_SOUND;
        weaponInfo->muzzleEffect = trap->FX_RegisterEffect("demp2/muzzle_flash");
        weaponInfo->missileModel = NULL_HANDLE;
        weaponInfo->missileSound = NULL_SOUND;
        weaponInfo->missileDlight = 0;
        //		weaponInfo->missileDlightColor	= {0,0,0};
        weaponInfo->missileHitSound = NULL_SOUND;
        weaponInfo->missileTrailFunc = FX_DEMP2_ProjectileThink;

        // the shot's sound is its muzzle effect's (effects/demp2/altmuzzle_flash.efx plays altfire): none of its own, or it plays twice
        weaponInfo->altFiringSound = NULL_SOUND;
        weaponInfo->altChargeSound = trap->sound->Register("sound/weapons/demp2/altCharge.wav");
        weaponInfo->altMuzzleEffect = trap->FX_RegisterEffect("demp2/altmuzzle_flash");
        weaponInfo->altMissileModel = NULL_HANDLE;
        weaponInfo->altMissileSound = NULL_SOUND;
        weaponInfo->altMissileDlight = 0;
        //		weaponInfo->altMissileDlightColor= {0,0,0};
        weaponInfo->altMissileHitSound = NULL_SOUND;
        weaponInfo->altMissileTrailFunc = nullptr;

        cgs.effects.demp2ProjectileEffect = trap->FX_RegisterEffect("demp2/projectile");
        cgs.effects.demp2WallImpactEffect = trap->FX_RegisterEffect("demp2/wall_impact");
        cgs.effects.demp2FleshImpactEffect = trap->FX_RegisterEffect("demp2/flesh_impact");

        cgs.media.demp2Shell = trap->R_RegisterModel("models/items/sphere.md3");
        cgs.media.demp2ShellShader = trap->R_RegisterShader("gfx/effects/demp2shell");

        cgs.media.lightningFlash = trap->R_RegisterShader("gfx/misc/lightningFlash");
        break;

    case weapon_t::WP_FLECHETTE:
        weaponInfo->selectSound = trap->sound->Register("sound/weapons/flechette/select.wav");

        // the shot's sound is its muzzle effect's (effects/flechette/muzzle_flash.efx plays fire): none of its own, or it plays twice
        weaponInfo->firingSound = NULL_SOUND;
        weaponInfo->chargeSound = NULL_SOUND;
        weaponInfo->muzzleEffect = trap->FX_RegisterEffect("flechette/muzzle_flash");
        weaponInfo->missileModel = trap->R_RegisterModel("models/weapons2/golan_arms/projectileMain.md3");
        weaponInfo->missileSound = NULL_SOUND;
        weaponInfo->missileDlight = 0;
        //		weaponInfo->missileDlightColor	= {0,0,0};
        weaponInfo->missileHitSound = NULL_SOUND;
        weaponInfo->missileTrailFunc = FX_FlechetteProjectileThink;

        // the shot's sound is its muzzle effect's (effects/flechette/altmuzzle_flash.efx plays alt_fire): none of its own, or it plays twice
        weaponInfo->altFiringSound = NULL_SOUND;
        weaponInfo->altChargeSound = NULL_SOUND;
        weaponInfo->altMuzzleEffect = trap->FX_RegisterEffect("flechette/altmuzzle_flash");
        weaponInfo->altMissileModel = trap->R_RegisterModel("models/weapons2/golan_arms/projectile.md3");
        weaponInfo->altMissileSound = NULL_SOUND;
        weaponInfo->altMissileDlight = 0;
        //		weaponInfo->altMissileDlightColor= {0,0,0};
        weaponInfo->altMissileHitSound = NULL_SOUND;
        weaponInfo->altMissileTrailFunc = FX_FlechetteAltProjectileThink;

        cgs.effects.flechetteShotEffect = trap->FX_RegisterEffect("flechette/shot");
        cgs.effects.flechetteAltShotEffect = trap->FX_RegisterEffect("flechette/alt_shot");
        cgs.effects.flechetteWallImpactEffect = trap->FX_RegisterEffect("flechette/wall_impact");
        cgs.effects.flechetteFleshImpactEffect = trap->FX_RegisterEffect("flechette/flesh_impact");
        break;

    case weapon_t::WP_ROCKET_LAUNCHER:
        weaponInfo->selectSound = trap->sound->Register("sound/weapons/rocket/select.wav");

        // the shot's sound is its muzzle effect's (effects/rocket/muzzle_flash.efx plays fire): none of its own, or it plays twice
        weaponInfo->firingSound = NULL_SOUND;
        weaponInfo->chargeSound = NULL_SOUND;
        weaponInfo->muzzleEffect = trap->FX_RegisterEffect("rocket/muzzle_flash"); // trap->FX_RegisterEffect( "rocket/muzzle_flash2" );
        // flash2 still looks crappy with the fx bolt stuff. Because the fx bolt stuff doesn't work entirely right.
        weaponInfo->missileModel = trap->R_RegisterModel("models/weapons2/merr_sonn/projectile.md3");
        weaponInfo->missileSound = trap->sound->Register("sound/weapons/rocket/missleloop.wav");
        weaponInfo->missileDlight = 125;
        VectorSet(weaponInfo->missileDlightColor, 1.0, 1.0, 0.5);
        weaponInfo->missileHitSound = NULL_SOUND;
        weaponInfo->missileTrailFunc = FX_RocketProjectileThink;

        // the shot's sound is its muzzle effect's (effects/rocket/altmuzzle_flash.efx plays alt_fire): none of its own, or it plays twice
        weaponInfo->altFiringSound = NULL_SOUND;
        weaponInfo->altChargeSound = NULL_SOUND;
        weaponInfo->altMuzzleEffect = trap->FX_RegisterEffect("rocket/altmuzzle_flash");
        weaponInfo->altMissileModel = trap->R_RegisterModel("models/weapons2/merr_sonn/projectile.md3");
        weaponInfo->altMissileSound = trap->sound->Register("sound/weapons/rocket/missleloop.wav");
        weaponInfo->altMissileDlight = 125;
        VectorSet(weaponInfo->altMissileDlightColor, 1.0, 1.0, 0.5);
        weaponInfo->altMissileHitSound = NULL_SOUND;
        weaponInfo->altMissileTrailFunc = FX_RocketAltProjectileThink;

        cgs.effects.rocketShotEffect = trap->FX_RegisterEffect("rocket/shot");
        cgs.effects.rocketExplosionEffect = trap->FX_RegisterEffect("rocket/explosion");

        trap->R_RegisterShaderNoMip("gfx/2d/wedge");
        trap->R_RegisterShaderNoMip("gfx/2d/lock");

        trap->sound->Register("sound/weapons/rocket/lock.wav");
        trap->sound->Register("sound/weapons/rocket/tick.wav");
        break;

    case weapon_t::WP_THERMAL:
        weaponInfo->selectSound = trap->sound->Register("sound/weapons/thermal/select.wav");

        weaponInfo->flashSound[0] = trap->sound->Register("sound/weapons/thermal/fire.wav");
        weaponInfo->firingSound = NULL_SOUND;
        weaponInfo->chargeSound = trap->sound->Register("sound/weapons/thermal/charge.wav");
        weaponInfo->muzzleEffect = NULL_FX;
        weaponInfo->missileModel = trap->R_RegisterModel("models/weapons2/thermal/thermal_proj.md3");
        weaponInfo->missileSound = NULL_SOUND;
        weaponInfo->missileDlight = 0;
        //		weaponInfo->missileDlightColor	= {0,0,0};
        weaponInfo->missileHitSound = NULL_SOUND;
        weaponInfo->missileTrailFunc = nullptr;

        weaponInfo->altFlashSound[0] = trap->sound->Register("sound/weapons/thermal/fire.wav");
        weaponInfo->altFiringSound = NULL_SOUND;
        weaponInfo->altChargeSound = trap->sound->Register("sound/weapons/thermal/charge.wav");
        weaponInfo->altMuzzleEffect = NULL_FX;
        weaponInfo->altMissileModel = trap->R_RegisterModel("models/weapons2/thermal/thermal_proj.md3");
        weaponInfo->altMissileSound = NULL_SOUND;
        weaponInfo->altMissileDlight = 0;
        //		weaponInfo->altMissileDlightColor= {0,0,0};
        weaponInfo->altMissileHitSound = NULL_SOUND;
        weaponInfo->altMissileTrailFunc = nullptr;

        cgs.effects.thermalExplosionEffect = trap->FX_RegisterEffect("thermal/explosion");
        cgs.effects.thermalShockwaveEffect = trap->FX_RegisterEffect("thermal/shockwave");

        cgs.media.grenadeBounce1 = trap->sound->Register("sound/weapons/thermal/bounce1.wav");
        cgs.media.grenadeBounce2 = trap->sound->Register("sound/weapons/thermal/bounce2.wav");

        trap->sound->Register("sound/weapons/thermal/thermloop.wav");
        trap->sound->Register("sound/weapons/thermal/warning.wav");

        break;

    case weapon_t::WP_TRIP_MINE:
        weaponInfo->selectSound = trap->sound->Register("sound/weapons/detpack/select.wav");

        weaponInfo->flashSound[0] = trap->sound->Register("sound/weapons/laser_trap/fire.wav");
        weaponInfo->firingSound = NULL_SOUND;
        weaponInfo->chargeSound = NULL_SOUND;
        weaponInfo->muzzleEffect = NULL_FX;
        weaponInfo->missileModel = 0; // trap->R_RegisterModel( "models/weapons2/laser_trap/laser_trap_w.md3" );
        weaponInfo->missileSound = NULL_SOUND;
        weaponInfo->missileDlight = 0;
        //		weaponInfo->missileDlightColor	= {0,0,0};
        weaponInfo->missileHitSound = NULL_SOUND;
        weaponInfo->missileTrailFunc = nullptr;

        weaponInfo->altFlashSound[0] = trap->sound->Register("sound/weapons/laser_trap/fire.wav");
        weaponInfo->altFiringSound = NULL_SOUND;
        weaponInfo->altChargeSound = NULL_SOUND;
        weaponInfo->altMuzzleEffect = NULL_FX;
        weaponInfo->altMissileModel = 0; // trap->R_RegisterModel( "models/weapons2/laser_trap/laser_trap_w.md3" );
        weaponInfo->altMissileSound = NULL_SOUND;
        weaponInfo->altMissileDlight = 0;
        //		weaponInfo->altMissileDlightColor= {0,0,0};
        weaponInfo->altMissileHitSound = NULL_SOUND;
        weaponInfo->altMissileTrailFunc = nullptr;

        cgs.effects.tripmineLaserFX = trap->FX_RegisterEffect("tripMine/laserMP.efx");
        cgs.effects.tripmineGlowFX = trap->FX_RegisterEffect("tripMine/glowbit.efx");

        trap->FX_RegisterEffect("tripMine/explosion");
        // NOTENOTE temp stuff
        trap->sound->Register("sound/weapons/laser_trap/stick.wav");
        trap->sound->Register("sound/weapons/laser_trap/warning.wav");
        break;

    case weapon_t::WP_DET_PACK:
        weaponInfo->selectSound = trap->sound->Register("sound/weapons/detpack/select.wav");

        weaponInfo->flashSound[0] = trap->sound->Register("sound/weapons/detpack/fire.wav");
        weaponInfo->firingSound = NULL_SOUND;
        weaponInfo->chargeSound = NULL_SOUND;
        weaponInfo->muzzleEffect = NULL_FX;
        weaponInfo->missileModel = trap->R_RegisterModel("models/weapons2/detpack/det_pack.md3");
        weaponInfo->missileSound = NULL_SOUND;
        weaponInfo->missileDlight = 0;
        //		weaponInfo->missileDlightColor	= {0,0,0};
        weaponInfo->missileHitSound = NULL_SOUND;
        weaponInfo->missileTrailFunc = nullptr;

        weaponInfo->altFlashSound[0] = trap->sound->Register("sound/weapons/detpack/fire.wav");
        weaponInfo->altFiringSound = NULL_SOUND;
        weaponInfo->altChargeSound = NULL_SOUND;
        weaponInfo->altMuzzleEffect = NULL_FX;
        weaponInfo->altMissileModel = trap->R_RegisterModel("models/weapons2/detpack/det_pack.md3");
        weaponInfo->altMissileSound = NULL_SOUND;
        weaponInfo->altMissileDlight = 0;
        //		weaponInfo->altMissileDlightColor= {0,0,0};
        weaponInfo->altMissileHitSound = NULL_SOUND;
        weaponInfo->altMissileTrailFunc = nullptr;

        trap->R_RegisterModel("models/weapons2/detpack/det_pack.md3");
        trap->sound->Register("sound/weapons/detpack/stick.wav");
        trap->sound->Register("sound/weapons/detpack/warning.wav");
        trap->sound->Register("sound/weapons/explosions/explode5.wav");
        break;
    case weapon_t::WP_TURRET:
        weaponInfo->flashSound[0] = NULL_SOUND;
        weaponInfo->firingSound = NULL_SOUND;
        weaponInfo->chargeSound = NULL_SOUND;
        weaponInfo->muzzleEffect = NULL_HANDLE;
        weaponInfo->missileModel = NULL_HANDLE;
        weaponInfo->missileSound = NULL_SOUND;
        weaponInfo->missileDlight = 0;
        weaponInfo->missileHitSound = NULL_SOUND;
        weaponInfo->missileTrailFunc = FX_TurretProjectileThink;

        trap->FX_RegisterEffect("effects/blaster/wall_impact.efx");
        trap->FX_RegisterEffect("effects/blaster/flesh_impact.efx");
        break;

    default:
        MAKERGB(weaponInfo->flashDlightColor, 1, 1, 1);
        weaponInfo->flashSound[0] = trap->sound->Register("sound/weapons/rocket/rocklf1a.wav");
        break;
    }
}

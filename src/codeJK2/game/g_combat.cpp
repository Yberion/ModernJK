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

// g_combat.cpp

#include "g_headers.h"

#include "g_local.h"
#include "b_local.h"
#include "g_functions.h"
#include "anims.h"
#include "objectives.h"
#include <cgame/cg_local.h>
#include "g_icarus.h"
#include "wp_saber.h"
#include "Q3_Interface.h"
#include <code/qcommon/strippublic.h>

extern cvar_t* g_debugDamage;
extern bool stop_icarus;
extern cvar_t* g_dismemberment;
extern cvar_t* g_dismemberProbabilities;
extern cvar_t* g_saberRealisticCombat;
extern cvar_t* g_timescale;
extern cvar_t* d_slowmodeath;
extern gentity_t* player;

gentity_t* g_lastClientDamaged;

extern int killPlayerTimer;

extern void NPC_TempLookTarget(gentity_t* self, int lookEntNum, int minLookTime, int maxLookTime);
extern void G_AddVoiceEvent(gentity_t* self, entity_event_t event, int speakDebounceTime);
extern bool PM_HasAnimation(gentity_t* ent, animNumber_t animation);
extern bool G_TeamEnemy(gentity_t* self);
extern void CG_ChangeWeapon(weapon_t num);
extern void ChangeWeapon(gentity_t* ent, weapon_t newWeapon);

extern void G_SetEnemy(gentity_t* self, gentity_t* enemy);
extern void PM_SetLegsAnimTimer(gentity_t* ent, int* legsAnimTimer, int time);
extern void PM_SetTorsoAnimTimer(gentity_t* ent, int* torsoAnimTimer, int time);
extern animNumber_t PM_PickAnim(gentity_t* self, animNumber_t minAnim, animNumber_t maxAnim);
extern bool PM_InOnGroundAnim(playerState_t* ps);
extern void G_SoundOnEnt(gentity_t* ent, sound::SoundClass soundClass, sound::Slot slot, const char* soundPath);
extern void G_ATSTCheckPain(gentity_t* self, gentity_t* other, vec3_t point, int damage, meansOfDeath_t mod, hitLocation_t hitLoc);
extern bool Jedi_WaitingAmbush(gentity_t* self);
extern bool G_ClearViewEntity(gentity_t* ent);
extern bool PM_CrouchAnim(animNumber_t anim);
extern bool PM_InKnockDown(playerState_t* ps);
extern bool PM_InRoll(playerState_t* ps);
extern bool PM_SpinningAnim(animNumber_t anim);
extern bool PM_RunningAnim(animNumber_t anim);
extern forcePowerLevels_t PM_PowerLevelForSaberAnim(playerState_t* ps);
extern bool PM_SaberInSpecialAttack(animNumber_t anim);
extern bool PM_SpinningSaberAnim(animNumber_t anim);
extern bool PM_FlippingAnim(animNumber_t anim);
extern bool PM_InSpecialJump(animNumber_t anim);
extern bool PM_RollingAnim(animNumber_t anim);
extern bool PM_InAnimForSaberMove(animNumber_t anim, saberMoveName_t saberMove);
extern bool PM_SaberInStart(saberMoveName_t move);
extern bool PM_SaberInReturn(saberMoveName_t move);
extern int PM_AnimLength(int index, animNumber_t anim);

static int G_CheckForLedge(gentity_t* self, vec3_t fallCheckDir, float checkDist);
static std::optional<animNumber_t> G_CheckSpecialDeathAnim(gentity_t* self, vec3_t point, int damage, meansOfDeath_t mod, hitLocation_t hitLoc);
static std::optional<animNumber_t> G_PickDeathAnim(gentity_t* self, vec3_t point, int damage, meansOfDeath_t mod, hitLocation_t hitLoc);
static void G_TrackWeaponUsage(gentity_t* self, gentity_t* inflictor, int add, meansOfDeath_t mod);
static bool G_Dismemberable(gentity_t* self, hitLocation_t hitLoc);
extern gitem_t* FindItemForAmmo(ammo_t ammo);
/*
============
AddScore

Adds score to both the client and his team
============
*/
void AddScore(gentity_t* ent, int score)
{
    if (!ent->client) {
        return;
    }
    // no scoring during pre-match warmup
    ent->client->ps.persistant[persEnum_t::PERS_SCORE] += score;
}

/*
=================
TossClientItems

Toss the weapon and powerups for the killed player
=================
*/
extern gentity_t* WP_DropThermal(gentity_t* ent);
extern bool WP_SaberLose(gentity_t* self, vec3_t throwDir);
gentity_t* TossClientItems(gentity_t* self)
{
    gentity_t* dropped = nullptr;
    gitem_t* item = nullptr;
    weapon_t weapon;

    if (self->client->NPC_class == class_t::CLASS_SEEKER || self->client->NPC_class == class_t::CLASS_REMOTE) {
        // these things are so small that they shouldn't bother throwing anything
        return nullptr;
    }

    // drop the weapon if not a saber or enemy-only weapon
    weapon = self->s.weapon;
    if (weapon == weapon_t::WP_SABER) {
        if (self->weaponModel < 0 || WP_SaberLose(self, nullptr)) {
            self->s.weapon = weapon_t::WP_NONE;
        }
    }
    else if (weapon == weapon_t::WP_BLASTER_PISTOL) { // FIXME: either drop the pistol and make the pickup only give ammo or drop ammo
    }
    else if (weapon > weapon_t::WP_SABER && weapon <= MAX_PLAYER_WEAPONS) //&& self->client->ps.ammo[ weaponData[weapon].ammoIndex ]
    {
        self->s.weapon = weapon_t::WP_NONE;

        if (weapon == weapon_t::WP_THERMAL &&
            self->client->ps.torsoAnim == animNumber_t::BOTH_ATTACK10) { // we were getting ready to throw the thermal, drop it!
            self->client->ps.weaponChargeTime = level.time - FRAMETIME;  // so it just kind of drops it
            dropped = WP_DropThermal(self);
        }
        else { // find the item type for this weapon
            item = FindItemForWeapon(weapon);
        }
        if (item && !dropped) {
            // spawn the item
            dropped = Drop_Item(self, item, 0, true);
            // TEST: dropped items never go away
            dropped->e_ThinkFunc = thinkFunc_t::thinkF_NULL;
            dropped->nextthink = -1;

            if (!self->s.number) { // player's dropped items never go away
                // dropped->e_ThinkFunc = thinkFunc_t::thinkF_NULL;
                // dropped->nextthink = -1;
                dropped->count = 0; // no ammo
            }
            else { // FIXME: base this on the NPC's actual amount of ammo he's used up...
                switch (weapon) {
                case weapon_t::WP_BRYAR_PISTOL:
                    dropped->count = 20;
                    break;
                case weapon_t::WP_BLASTER:
                    dropped->count = 15;
                    break;
                case weapon_t::WP_DISRUPTOR:
                    dropped->count = 20;
                    break;
                case weapon_t::WP_BOWCASTER:
                    dropped->count = 5;
                    break;
                case weapon_t::WP_REPEATER:
                    dropped->count = 20;
                    break;
                case weapon_t::WP_DEMP2:
                    dropped->count = 10;
                    break;
                case weapon_t::WP_FLECHETTE:
                    dropped->count = 30;
                    break;
                case weapon_t::WP_ROCKET_LAUNCHER:
                    dropped->count = 3;
                    break;
                case weapon_t::WP_THERMAL:
                    dropped->count = 4;
                    break;
                case weapon_t::WP_TRIP_MINE:
                    dropped->count = 3;
                    break;
                case weapon_t::WP_DET_PACK:
                    dropped->count = 1;
                    break;
                case weapon_t::WP_STUN_BATON:
                    dropped->count = 20;
                    break;
                default:
                    dropped->count = 0;
                    break;
                }
            }
            // well, dropped weapons are G2 models, so they have to be initialised if they want to draw..give us a radius so we don't get prematurely culled
            if (weapon != weapon_t::WP_THERMAL && weapon != weapon_t::WP_TRIP_MINE && weapon != weapon_t::WP_DET_PACK) {
                gi.G2API_InitGhoul2Model(dropped->ghoul2, item->world_model, G_ModelIndex(item->world_model), NULL_HANDLE, NULL_HANDLE, 0, 0);
                dropped->s.radius = 10;
            }
        }
    }
    //	else if (( self->client->NPC_class == class_t::CLASS_SENTRY ) || ( self->client->NPC_class == class_t::CLASS_PROBE )) // Looks dumb, Steve told us to
    // take it out.
    //	{
    //		item = FindItemForAmmo( ammo_t::AMMO_BLASTER );
    //		Drop_Item( self, item, 0, true );
    //	}
    else if (self->client->NPC_class == class_t::CLASS_MARK1) {

        if (Q_irand(1, 2) > 1) {
            item = FindItemForAmmo(ammo_t::AMMO_METAL_BOLTS);
        }
        else {
            item = FindItemForAmmo(ammo_t::AMMO_BLASTER);
        }
        Drop_Item(self, item, 0, true);
    }
    else if (self->client->NPC_class == class_t::CLASS_MARK2) {

        if (Q_irand(1, 2) > 1) {
            item = FindItemForAmmo(ammo_t::AMMO_METAL_BOLTS);
        }
        else {
            item = FindItemForAmmo(ammo_t::AMMO_POWERCELL);
        }
        Drop_Item(self, item, 0, true);
    }

    return dropped; // NOTE: presumes only drop one thing
}

void G_DropKey(gentity_t* self)
{ // drop whatever security key I was holding
    gitem_t* item = nullptr;
    if (!Q_stricmp("goodie", self->message)) {
        item = FindItemForInventory(inventory_t::INV_GOODIE_KEY);
    }
    else {
        item = FindItemForInventory(inventory_t::INV_SECURITY_KEY);
    }
    gentity_t* dropped = Drop_Item(self, item, 0, true);
    // Don't throw the key
    VectorClear(dropped->s.pos.trDelta);
    dropped->message = G_NewString(self->message);
    self->message = nullptr;
}

void ObjectDie(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, meansOfDeath_t meansOfDeath)
{
    if (self->target)
        G_UseTargets(self, attacker);

    // remove my script_targetname
    G_FreeEntity(self);
}
/*
==================
ExplodeDeath
==================
*/

// FIXME: all hacked up...

// void CG_SurfaceExplosion( vec3_t origin, vec3_t normal, float radius, float shake_speed, bool smoke );
void ExplodeDeath(gentity_t* self)
{
    //	gentity_t	*tent;
    vec3_t forward;

    self->takedamage = false; // stop chain reaction runaway loops

    self->s.loopSound = 0;

    VectorCopy(self->currentOrigin, self->s.pos.trBase);

    //	tent = G_TempEntity( self->s.origin, EV_FX_EXPLOSION );
    AngleVectors(self->s.angles, forward, nullptr, nullptr); // FIXME: letting effect always shoot up?  Might be ok.

    if (self->fxID > 0) {
        G_PlayEffect(self->fxID, self->currentOrigin, forward);
    }
    //	else
    //	{
    //		CG_SurfaceExplosion( self->currentOrigin, forward, 20.0f, 12.0f, ((self->spawnflags&4)==false) );	//FIXME: This needs to be consistent to all
    // exploders! 		G_Sound(self, sound::SoundClass::World, self->sounds);
    //	}

    if (self->splashDamage > 0 && self->splashRadius > 0) {
        gentity_t* attacker = self;
        if (self->owner) {
            attacker = self->owner;
        }
        G_RadiusDamage(self->currentOrigin, attacker, self->splashDamage, self->splashRadius, attacker, meansOfDeath_t::MOD_UNKNOWN);
    }

    ObjectDie(self, self, self, 20, meansOfDeath_t::MOD_UNKNOWN);
}

void ExplodeDeath_Wait(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, meansOfDeath_t meansOfDeath, damageFlags_t dFlags,
                       hitLocation_t hitLoc)
{
    self->e_DieFunc = dieFunc_t::dieF_NULL;
    self->nextthink = level.time + Q_irand(100, 500);
    self->e_ThinkFunc = thinkFunc_t::thinkF_ExplodeDeath;
}

void ExplodeDeath(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, meansOfDeath_t meansOfDeath, damageFlags_t dFlags,
                  hitLocation_t hitLoc)
{
    self->currentOrigin[2] += 16; // me bad for hacking this.  should either do it in the effect file or make a custom explode death??
    ExplodeDeath(self);
}

void GoExplodeDeath(gentity_t* self, gentity_t* other, gentity_t* activator)
{
    G_ActivateBehavior(self, bSet_t::BSET_USE);

    self->targetname = ""; // Make sure this entity cannot be told to explode again (recursive death fix)

    ExplodeDeath(self);
}

bool G_ActivateBehavior(gentity_t* self, bSet_t bset);
void G_CheckVictoryScript(gentity_t* self)
{
    if (!G_ActivateBehavior(self, bSet_t::BSET_VICTORY)) {
        if (self->NPC && self->s.weapon == weapon_t::WP_SABER) { // Jedi taunt from within their AI
            self->NPC->blockedSpeechDebounceTime = 0;            // get them ready to taunt
            return;
        }
        if (self->client && self->client->NPC_class == class_t::CLASS_GALAKMECH) {
            self->wait = 1;
            TIMER_Set(self, "gloatTime", Q_irand(5000, 8000));
            self->NPC->blockedSpeechDebounceTime = 0; // get him ready to taunt
            return;
        }
        // FIXME: any way to not say this *right away*?  Wait for victim's death anim/scream to finish?
        if (self->NPC && self->NPC->group && self->NPC->group->commander && self->NPC->group->commander->NPC &&
            self->NPC->group->commander->NPC->rank > self->NPC->rank && !Q_irand(0, 2)) { // sometimes have the group commander speak instead
            self->NPC->group->commander->NPC->greetingDebounceTime = level.time + Q_irand(2000, 5000);
            // G_AddVoiceEvent( self->NPC->group->commander, Q_irand(entity_event_t::EV_VICTORY1, entity_event_t::EV_VICTORY3), 2000 );
        }
        else if (self->NPC) {
            self->NPC->greetingDebounceTime = level.time + Q_irand(2000, 5000);
            // G_AddVoiceEvent( self, Q_irand(entity_event_t::EV_VICTORY1, entity_event_t::EV_VICTORY3), 2000 );
        }
    }
}

bool OnSameTeam(gentity_t* ent1, gentity_t* ent2)
{
    if (!ent1->client || !ent2->client) {
        if (ent1->noDamageTeam != team_t::TEAM_FREE) {
            if (ent2->client && ent2->client->playerTeam == ent1->noDamageTeam) {
                return true;
            }
            else if (ent2->noDamageTeam == ent1->noDamageTeam) {
                if (ent1->splashDamage && ent2->splashDamage &&
                    Q_stricmp("ambient_etherian_fliers", ent1->classname) != 0) { // Barrels, exploding breakables and mines will blow each other up
                    return false;
                }
                else {
                    return true;
                }
            }
        }
        return false;
    }

    // shouldn't need this anymore, there were problems with certain droids, but now they have been labeled TEAM_ENEMY so this isn't needed
    //	if ((( ent1->client->playerTeam == TEAM_IMPERIAL ) && ( ent1->client->playerTeam == TEAM_BOTS )) ||
    //		(( ent1->client->playerTeam == TEAM_BOTS ) && ( ent1->client->playerTeam == TEAM_IMPERIAL )))
    //	{
    //		return true;
    //	}

    return (ent1->client->playerTeam == ent2->client->playerTeam);
}

/*
-------------------------
G_AlertTeam
-------------------------
*/

void G_AlertTeam(gentity_t* victim, gentity_t* attacker, float radius, float soundDist)
{
    gentity_t* radiusEnts[128];
    vec3_t mins, maxs;
    int numEnts;
    float distSq, sndDistSq = (soundDist * soundDist);
    int i;

    if (attacker == nullptr || attacker->client == nullptr)
        return;

    // Setup the bbox to search in
    for (i = 0; i < 3; i++) {
        mins[i] = victim->currentOrigin[i] - radius;
        maxs[i] = victim->currentOrigin[i] + radius;
    }

    // Get the number of entities in a given space
    numEnts = gi.EntitiesInBox(mins, maxs, radiusEnts, 128);

    // Cull this list
    for (i = 0; i < numEnts; i++) {
        // Validate clients
        if (radiusEnts[i]->client == nullptr)
            continue;

        // only want NPCs
        if (radiusEnts[i]->NPC == nullptr)
            continue;

        // Don't bother if they're ignoring enemies
        if (Any(radiusEnts[i]->svFlags & svFlags_t::SVF_IGNORE_ENEMIES))
            continue;

        // This NPC specifically flagged to ignore alerts
        if (Any(radiusEnts[i]->NPC->scriptFlags & scriptFlags_t::SCF_IGNORE_ALERTS))
            continue;

        // This NPC specifically flagged to ignore alerts
        if (!Any(radiusEnts[i]->NPC->scriptFlags & scriptFlags_t::SCF_LOOK_FOR_ENEMIES))
            continue;

        // this ent does not participate in group AI
        if (Any(radiusEnts[i]->NPC->scriptFlags & scriptFlags_t::SCF_NO_GROUPS))
            continue;

        // Skip the requested avoid radiusEnts[i] if present
        if (radiusEnts[i] == victim)
            continue;

        // Skip the attacker
        if (radiusEnts[i] == attacker)
            continue;

        // Must be on the same team
        if (radiusEnts[i]->client->playerTeam != victim->client->playerTeam)
            continue;

        // Must be alive
        if (radiusEnts[i]->health <= 0)
            continue;

        if (radiusEnts[i]->enemy == nullptr) { // only do this if they're not already mad at someone
            distSq = DistanceSquared(radiusEnts[i]->currentOrigin, victim->currentOrigin);
            if (distSq > 16384 /*128 squared*/ && !gi.inPVS(victim->currentOrigin, radiusEnts[i]->currentOrigin)) { // not even potentially visible/hearable
                continue;
            }
            // NOTE: this allows sound alerts to still go through doors/PVS if the teammate is within 128 of the victim...
            if (soundDist <= 0 || distSq > sndDistSq) { // out of sound range
                if (!InFOV(victim, radiusEnts[i], radiusEnts[i]->NPC->stats.hfov, radiusEnts[i]->NPC->stats.vfov) ||
                    !NPC_ClearLOS(radiusEnts[i], victim->currentOrigin)) { // out of FOV or no LOS
                    continue;
                }
            }

            // FIXME: This can have a nasty cascading effect if setup wrong...
            G_SetEnemy(radiusEnts[i], attacker);
        }
    }
}

/*
-------------------------
G_DeathAlert
-------------------------
*/

constexpr int DEATH_ALERT_RADIUS = 512;
constexpr int DEATH_ALERT_SOUND_RADIUS = 512;

void G_DeathAlert(gentity_t* victim, gentity_t* attacker)
{ // FIXME: with all the other alert stuff, do we really need this?
    G_AlertTeam(victim, attacker, DEATH_ALERT_RADIUS, DEATH_ALERT_SOUND_RADIUS);
}

/*
----------------------------------------
DeathFX

Applies appropriate special effects that occur while the entity is dying
Not to be confused with NPC_RemoveBodyEffects (NPC.cpp), which only applies effect when removing the body
----------------------------------------
*/

void DeathFX(gentity_t* ent)
{
    if (!ent || !ent->client)
        return;
    /*
        switch( ent->client->playerTeam )
        {
        case TEAM_BOTS:
            if (!Q_stricmp( ent->NPC_type, "mouse" ))
            {
                vec3_t		effectPos;
                VectorCopy( ent->currentOrigin, effectPos );
                effectPos[2] -= 20;

                G_PlayEffect( "mouseexplosion1", effectPos );
                G_PlayEffect( "smaller_chunks", effectPos );

            }
            else if (!Q_stricmp( ent->NPC_type, "probe" ))
            {
                vec3_t		effectPos;
                VectorCopy( ent->currentOrigin, effectPos );
                effectPos[2] += 50;

                G_PlayEffect( "probeexplosion1", effectPos );
                G_PlayEffect( "small_chunks", effectPos );
            }
            else
            {
                vec3_t		effectPos;
                VectorCopy( ent->currentOrigin, effectPos );
                effectPos[2] -= 15;
                G_PlayEffect( "droidexplosion1", effectPos );
                G_PlayEffect( "small_chunks", effectPos );
            }

            break;

        default:
            break;
        }
    */
    // team no longer indicates species/race.  NPC_class should be used to identify certain npc types
    vec3_t effectPos, right;
    switch (ent->client->NPC_class) {
    case class_t::CLASS_MOUSE:
        VectorCopy(ent->currentOrigin, effectPos);
        effectPos[2] -= 20;
        G_PlayEffect("env/small_explode", effectPos);
        G_SoundOnEnt(ent, sound::SoundClass::World, sound::Slot::None, "sound/chars/mouse/misc/death1");
        break;

    case class_t::CLASS_PROBE:
        VectorCopy(ent->currentOrigin, effectPos);
        effectPos[2] += 50;
        G_PlayEffect("probeexplosion1", effectPos);
        break;

    case class_t::CLASS_ATST:
        AngleVectors(ent->currentAngles, nullptr, right, nullptr);
        VectorMA(ent->currentOrigin, 20, right, effectPos);
        effectPos[2] += 180;
        G_PlayEffect("droidexplosion1", effectPos);
        VectorMA(effectPos, -40, right, effectPos);
        G_PlayEffect("droidexplosion1", effectPos);
        break;

    case class_t::CLASS_SEEKER:
    case class_t::CLASS_REMOTE:
        G_PlayEffect("env/small_explode", ent->currentOrigin);
        break;

    case class_t::CLASS_GONK:
        VectorCopy(ent->currentOrigin, effectPos);
        effectPos[2] -= 5;
        //		statusTextIndex = Q_irand( IGT_RESISTANCEISFUTILE, IGT_NAMEIS8OF12 );
        G_SoundOnEnt(ent, sound::SoundClass::World, sound::Slot::None, Q_Format("sound/chars/gonk/misc/death{}.wav", Q_irand(1, 3)).c_str());
        G_PlayEffect("env/med_explode", effectPos);
        break;

    // should list all remaining droids here, hope I didn't miss any
    case class_t::CLASS_R2D2:
        VectorCopy(ent->currentOrigin, effectPos);
        effectPos[2] -= 10;
        G_PlayEffect("env/med_explode", effectPos);
        G_SoundOnEnt(ent, sound::SoundClass::World, sound::Slot::None, "sound/chars/mark2/misc/mark2_explo");
        break;

    case class_t::CLASS_PROTOCOL: //??
    case class_t::CLASS_R5D2:
        VectorCopy(ent->currentOrigin, effectPos);
        effectPos[2] -= 10;
        G_PlayEffect("env/med_explode", effectPos);
        G_SoundOnEnt(ent, sound::SoundClass::World, sound::Slot::None, "sound/chars/mark2/misc/mark2_explo");
        break;

    case class_t::CLASS_MARK2:
        VectorCopy(ent->currentOrigin, effectPos);
        effectPos[2] -= 15;
        G_PlayEffect("droidexplosion1", effectPos);
        G_SoundOnEnt(ent, sound::SoundClass::World, sound::Slot::None, "sound/chars/mark2/misc/mark2_explo");
        break;

    case class_t::CLASS_INTERROGATOR:
        VectorCopy(ent->currentOrigin, effectPos);
        effectPos[2] -= 15;
        G_PlayEffect("droidexplosion1", effectPos);
        G_SoundOnEnt(ent, sound::SoundClass::World, sound::Slot::None, "sound/chars/interrogator/misc/int_droid_explo");
        break;

    case class_t::CLASS_MARK1:
        AngleVectors(ent->currentAngles, nullptr, right, nullptr);
        VectorMA(ent->currentOrigin, 10, right, effectPos);
        effectPos[2] -= 15;
        G_PlayEffect("droidexplosion1", effectPos);
        VectorMA(effectPos, -20, right, effectPos);
        G_PlayEffect("droidexplosion1", effectPos);
        VectorMA(effectPos, -20, right, effectPos);
        G_PlayEffect("droidexplosion1", effectPos);
        G_SoundOnEnt(ent, sound::SoundClass::World, sound::Slot::None, "sound/chars/mark1/misc/mark1_explo");
        break;

    case class_t::CLASS_SENTRY:
        G_SoundOnEnt(ent, sound::SoundClass::World, sound::Slot::None, "sound/chars/sentry/misc/sentry_explo");
        VectorCopy(ent->currentOrigin, effectPos);
        G_PlayEffect("env/med_explode", effectPos);
        break;

    default:
        break;
    }
}

void G_SetMissionStatusText(gentity_t* attacker, meansOfDeath_t mod)
{
    if (statusTextIndex >= 0) {
        return;
    }

    if (mod == meansOfDeath_t::MOD_FALLING) { // fell to your death
        statusTextIndex = std::to_underlying(statusText_t::STAT_WATCHYOURSTEP);
    }
    else if (mod == meansOfDeath_t::MOD_CRUSH) { // crushed
        statusTextIndex = std::to_underlying(statusText_t::STAT_JUDGEMENTMUCHDESIRED);
    }
    else if (attacker && Q_stricmp("trigger_hurt", attacker->classname) == 0) { // Killed by something that should have been clearly dangerous
        //		statusTextIndex = Q_irand( IGT_JUDGEMENTDESIRED, IGT_JUDGEMENTMUCHDESIRED );
        statusTextIndex = std::to_underlying(statusText_t::STAT_JUDGEMENTMUCHDESIRED);
    }
    else if (attacker && attacker->s.number != 0 && attacker->client && attacker->client->playerTeam == team_t::TEAM_PLAYER) { // killed by a teammate
        statusTextIndex = std::to_underlying(statusText_t::STAT_INSUBORDINATION);
    }
}

void G_MakeTeamVulnerable(void)
{
    int i, newhealth;
    gentity_t* ent;
    gentity_t* self = &g_entities[0];
    if (!self->client) {
        return;
    }

    //	for ( i = 0; i < globals.num_entities ; i++, ent++)
    for (i = 0; i < globals.num_entities; i++) {
        if (!PInUse(i))
            continue;
        //		if ( !ent->inuse  )
        //		{
        //			continue;
        //		}
        //		if ( !ent )
        //		{
        //			continue;
        //		}
        ent = &g_entities[i];
        if (!ent->client) {
            continue;
        }
        if (ent->client->playerTeam != team_t::TEAM_PLAYER) {
            continue;
        }
        if (!Any(ent->flags & gentityFlags_t::FL_UNDYING)) {
            continue;
        }
        ent->flags &= ~gentityFlags_t::FL_UNDYING;
        newhealth = Q_irand(5, 40);
        if (ent->health > newhealth) {
            ent->health = newhealth;
        }
    }
}

void G_StartMatrixEffect(gentity_t* ent, bool falling = false, int length = 1000)
{                                    // FIXME: only do this if no other enemies around?
    if (g_timescale->value != 1.0) { // already in some slow-mo mode
        return;
    }

    gentity_t* matrix = G_Spawn();
    if (matrix) {
        G_SetOrigin(matrix, ent->currentOrigin);
        gi.linkentity(matrix);
        matrix->s.otherEntityNum = ent->s.number;
        matrix->e_clThinkFunc = clThinkFunc_t::clThinkF_CG_MatrixEffect;
        matrix->s.eType = entityType_t::ET_THINKER;
        matrix->svFlags |= svFlags_t::SVF_BROADCAST; // Broadcast to all clients
        matrix->s.time = level.time;
        matrix->s.eventParm = length;
        matrix->e_ThinkFunc = thinkFunc_t::thinkF_G_FreeEntity;
        matrix->nextthink = level.time + length + 500;
        if (falling) { // no timescale or vert bob
            ES_SetWeaponParm(matrix->s, 1);
        }
    }
}

bool G_JediInRoom(vec3_t from)
{
    gentity_t* ent;
    int i;
    //	for ( i = 1, ent = &g_entities[1]; i < globals.num_entities; i++, ent++ )
    for (i = 1; i < globals.num_entities; i++) {
        if (!PInUse(i))
            continue;
        //		if ( !ent->inuse )
        //		{
        //			continue;
        //		}
        //		if ( !ent )
        //		{
        //			continue;
        //		}
        ent = &g_entities[i];
        if (!ent->NPC) {
            continue;
        }
        if (ent->health <= 0) {
            continue;
        }
        if (Any(ent->s.eFlags & entityFlags_t::EF_NODRAW)) {
            continue;
        }
        if (ent->s.weapon != weapon_t::WP_SABER) {
            continue;
        }
        if (!gi.inPVS(ent->currentOrigin, from)) {
            continue;
        }
        return true;
    }
    return false;
}

bool G_GetHitLocFromSurfName(gentity_t* ent, const char* surfName, hitLocation_t* hitLoc, vec3_t point, vec3_t dir, vec3_t bladeDir, meansOfDeath_t mod)
{
    bool dismember = false;

    *hitLoc = hitLocation_t::HL_NONE;

    if (!surfName || !surfName[0]) {
        return false;
    }

    if (!ent->client) {
        return false;
    }

    if (ent->client &&
        (ent->client->NPC_class == class_t::CLASS_R2D2 || ent->client->NPC_class == class_t::CLASS_R5D2 || ent->client->NPC_class == class_t::CLASS_GONK ||
         ent->client->NPC_class == class_t::CLASS_MOUSE || ent->client->NPC_class == class_t::CLASS_SEEKER ||
         ent->client->NPC_class == class_t::CLASS_INTERROGATOR || ent->client->NPC_class == class_t::CLASS_SENTRY ||
         ent->client->NPC_class == class_t::CLASS_PROBE)) { // we don't care about per-surface hit-locations or dismemberment for these guys
        return false;
    }

    if (ent->client && (ent->client->NPC_class == class_t::CLASS_ATST)) {
        // FIXME: almost impossible to hit these... perhaps we should
        //		check for splashDamage and do radius damage to these parts?
        //		Or, if we ever get bbox G2 traces, that may fix it, too
        if (!Q_stricmp("head_light_blaster_cann", surfName)) {
            *hitLoc = hitLocation_t::HL_ARM_LT;
        }
        else if (!Q_stricmp("head_concussion_charger", surfName)) {
            *hitLoc = hitLocation_t::HL_ARM_RT;
        }
        return (false);
    }
    else if (ent->client && (ent->client->NPC_class == class_t::CLASS_MARK1)) {
        if (!Q_stricmp("l_arm", surfName)) {
            *hitLoc = hitLocation_t::HL_ARM_LT;
        }
        else if (!Q_stricmp("r_arm", surfName)) {
            *hitLoc = hitLocation_t::HL_ARM_RT;
        }
        else if (!Q_stricmp("torso_front", surfName)) {
            *hitLoc = hitLocation_t::HL_CHEST;
        }
        else if (!Q_stricmp("torso_tube1", surfName)) {
            *hitLoc = hitLocation_t::HL_GENERIC1;
        }
        else if (!Q_stricmp("torso_tube2", surfName)) {
            *hitLoc = hitLocation_t::HL_GENERIC2;
        }
        else if (!Q_stricmp("torso_tube3", surfName)) {
            *hitLoc = hitLocation_t::HL_GENERIC3;
        }
        else if (!Q_stricmp("torso_tube4", surfName)) {
            *hitLoc = hitLocation_t::HL_GENERIC4;
        }
        else if (!Q_stricmp("torso_tube5", surfName)) {
            *hitLoc = hitLocation_t::HL_GENERIC5;
        }
        else if (!Q_stricmp("torso_tube6", surfName)) {
            *hitLoc = hitLocation_t::HL_GENERIC6;
        }
        return (false);
    }
    else if (ent->client && (ent->client->NPC_class == class_t::CLASS_MARK2)) {
        if (!Q_stricmp("torso_canister1", surfName)) {
            *hitLoc = hitLocation_t::HL_GENERIC1;
        }
        else if (!Q_stricmp("torso_canister2", surfName)) {
            *hitLoc = hitLocation_t::HL_GENERIC2;
        }
        else if (!Q_stricmp("torso_canister3", surfName)) {
            *hitLoc = hitLocation_t::HL_GENERIC3;
        }
        return (false);
    }
    else if (ent->client && (ent->client->NPC_class == class_t::CLASS_GALAKMECH)) {
        if (!Q_stricmp("torso_antenna", surfName) || !Q_stricmp("torso_antenna_base", surfName)) {
            *hitLoc = hitLocation_t::HL_GENERIC1;
        }
        else if (!Q_stricmp("torso_shield_off", surfName)) {
            *hitLoc = hitLocation_t::HL_GENERIC2;
        }
        else {
            *hitLoc = hitLocation_t::HL_CHEST;
        }
        return (false);
    }

    // FIXME: check the hitLoc and hitDir against the cap tag for the place
    // where the split will be- if the hit dir is roughly perpendicular to
    // the direction of the cap, then the split is allowed, otherwise we
    // hit it at the wrong angle and should not dismember...
    int actualTime = (cg.time ? cg.time : level.time);
    if (!Q_strncmp("hips", surfName, 4)) { // FIXME: test properly for legs
        *hitLoc = hitLocation_t::HL_WAIST;
        if (ent->client != nullptr && ent->ghoul2.size()) {
            mdxaBone_t boltMatrix;
            vec3_t tagOrg, angles;

            VectorSet(angles, 0, ent->currentAngles[YAW], 0);
            if (ent->kneeLBolt >= 0) {
                gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel, ent->kneeLBolt, &boltMatrix, angles, ent->currentOrigin, actualTime, nullptr,
                                       ent->s.modelScale);
                gi.G2API_GiveMeVectorFromMatrix(boltMatrix, Eorientations::ORIGIN, tagOrg);
                if (DistanceSquared(point, tagOrg) < 100) { // actually hit the knee
                    *hitLoc = hitLocation_t::HL_LEG_LT;
                }
            }
            if (*hitLoc == hitLocation_t::HL_WAIST) {
                if (ent->kneeRBolt >= 0) {
                    gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel, ent->kneeRBolt, &boltMatrix, angles, ent->currentOrigin, actualTime, nullptr,
                                           ent->s.modelScale);
                    gi.G2API_GiveMeVectorFromMatrix(boltMatrix, Eorientations::ORIGIN, tagOrg);
                    if (DistanceSquared(point, tagOrg) < 100) { // actually hit the knee
                        *hitLoc = hitLocation_t::HL_LEG_RT;
                    }
                }
            }
        }
    }
    else if (!Q_strncmp("torso", surfName, 5)) {
        if (!ent->client) {
            *hitLoc = hitLocation_t::HL_CHEST;
        }
        else {
            vec3_t t_fwd, t_rt, t_up, dirToImpact;
            float frontSide, rightSide, upSide;
            AngleVectors(ent->client->renderInfo.torsoAngles, t_fwd, t_rt, t_up);
            VectorSubtract(point, ent->client->renderInfo.torsoPoint, dirToImpact);
            frontSide = DotProduct(t_fwd, dirToImpact);
            rightSide = DotProduct(t_rt, dirToImpact);
            upSide = DotProduct(t_up, dirToImpact);
            if (upSide < -10) { // hit at waist
                *hitLoc = hitLocation_t::HL_WAIST;
            }
            else { // hit on upper torso
                if (rightSide > 4) {
                    *hitLoc = hitLocation_t::HL_ARM_RT;
                }
                else if (rightSide < -4) {
                    *hitLoc = hitLocation_t::HL_ARM_LT;
                }
                else if (rightSide > 2) {
                    if (frontSide > 0) {
                        *hitLoc = hitLocation_t::HL_CHEST_RT;
                    }
                    else {
                        *hitLoc = hitLocation_t::HL_BACK_RT;
                    }
                }
                else if (rightSide < -2) {
                    if (frontSide > 0) {
                        *hitLoc = hitLocation_t::HL_CHEST_LT;
                    }
                    else {
                        *hitLoc = hitLocation_t::HL_BACK_LT;
                    }
                }
                else if (upSide > -3 && mod == meansOfDeath_t::MOD_SABER) {
                    *hitLoc = hitLocation_t::HL_HEAD;
                }
                else if (frontSide > 0) {
                    *hitLoc = hitLocation_t::HL_CHEST;
                }
                else {
                    *hitLoc = hitLocation_t::HL_BACK;
                }
            }
        }
    }
    else if (!Q_strncmp("head", surfName, 4)) {
        *hitLoc = hitLocation_t::HL_HEAD;
    }
    else if (!Q_strncmp("r_arm", surfName, 5)) {
        *hitLoc = hitLocation_t::HL_ARM_RT;
        if (ent->client != nullptr && ent->ghoul2.size()) {
            mdxaBone_t boltMatrix;
            vec3_t tagOrg, angles;

            VectorSet(angles, 0, ent->currentAngles[YAW], 0);
            if (ent->handRBolt >= 0) {
                gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel, ent->handRBolt, &boltMatrix, angles, ent->currentOrigin, actualTime, nullptr,
                                       ent->s.modelScale);
                gi.G2API_GiveMeVectorFromMatrix(boltMatrix, Eorientations::ORIGIN, tagOrg);
                if (DistanceSquared(point, tagOrg) < 256) { // actually hit the hand
                    *hitLoc = hitLocation_t::HL_HAND_RT;
                }
            }
        }
    }
    else if (!Q_strncmp("l_arm", surfName, 5)) {
        *hitLoc = hitLocation_t::HL_ARM_LT;
        if (ent->client != nullptr && ent->ghoul2.size()) {
            mdxaBone_t boltMatrix;
            vec3_t tagOrg, angles;

            VectorSet(angles, 0, ent->currentAngles[YAW], 0);
            if (ent->handLBolt >= 0) {
                gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel, ent->handLBolt, &boltMatrix, angles, ent->currentOrigin, actualTime, nullptr,
                                       ent->s.modelScale);
                gi.G2API_GiveMeVectorFromMatrix(boltMatrix, Eorientations::ORIGIN, tagOrg);
                if (DistanceSquared(point, tagOrg) < 256) { // actually hit the hand
                    *hitLoc = hitLocation_t::HL_HAND_LT;
                }
            }
        }
    }
    else if (!Q_strncmp("r_leg", surfName, 5)) {
        *hitLoc = hitLocation_t::HL_LEG_RT;
        if (ent->client != nullptr && ent->ghoul2.size()) {
            mdxaBone_t boltMatrix;
            vec3_t tagOrg, angles;

            VectorSet(angles, 0, ent->currentAngles[YAW], 0);
            if (ent->footRBolt >= 0) {
                gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel, ent->footRBolt, &boltMatrix, angles, ent->currentOrigin, actualTime, nullptr,
                                       ent->s.modelScale);
                gi.G2API_GiveMeVectorFromMatrix(boltMatrix, Eorientations::ORIGIN, tagOrg);
                if (DistanceSquared(point, tagOrg) < 100) { // actually hit the foot
                    *hitLoc = hitLocation_t::HL_FOOT_RT;
                }
            }
        }
    }
    else if (!Q_strncmp("l_leg", surfName, 5)) {
        *hitLoc = hitLocation_t::HL_LEG_LT;
        if (ent->client != nullptr && ent->ghoul2.size()) {
            mdxaBone_t boltMatrix;
            vec3_t tagOrg, angles;

            VectorSet(angles, 0, ent->currentAngles[YAW], 0);
            if (ent->footLBolt >= 0) {
                gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel, ent->footLBolt, &boltMatrix, angles, ent->currentOrigin, actualTime, nullptr,
                                       ent->s.modelScale);
                gi.G2API_GiveMeVectorFromMatrix(boltMatrix, Eorientations::ORIGIN, tagOrg);
                if (DistanceSquared(point, tagOrg) < 100) { // actually hit the foot
                    *hitLoc = hitLocation_t::HL_FOOT_LT;
                }
            }
        }
    }
    else if (!Q_strncmp("r_hand", surfName, 6) || !Q_strncmp("w_", surfName, 2)) { // right hand or weapon
        *hitLoc = hitLocation_t::HL_HAND_RT;
    }
    else if (!Q_strncmp("l_hand", surfName, 6)) {
        *hitLoc = hitLocation_t::HL_HAND_LT;
    }
#ifdef _DEBUG
    else {
        Q_PrintlnError("surface {} does not belong to any hitLocation!!!", surfName);
    }
#endif //_DEBUG

    if (g_saberRealisticCombat->integer) {
        dismember = true;
    }
    else if (g_dismemberment->integer >= 11'381'138 || !ent->client->dismembered) {
        if (ent->client && ent->client->NPC_class == class_t::CLASS_PROTOCOL) {
            dismember = true;
        }
        else if (dir && (dir[0] || dir[1] || dir[2]) && bladeDir &&
                 (bladeDir[0] || bladeDir[1] || bladeDir[2])) { // we care about direction (presumably for dismemberment)
            if (g_dismemberProbabilities->value <= 0.0f ||
                G_Dismemberable(ent, *hitLoc)) { // either we don't care about probabilties or the probability let us continue
                const char* tagName = nullptr;
                float aoa = 0.5f;
                // dir must be roughly perpendicular to the hitLoc's cap bolt
                switch (*hitLoc) {
                case hitLocation_t::HL_LEG_RT:
                    tagName = "*hips_cap_r_leg";
                    break;
                case hitLocation_t::HL_LEG_LT:
                    tagName = "*hips_cap_l_leg";
                    break;
                case hitLocation_t::HL_WAIST:
                    tagName = "*hips_cap_torso";
                    aoa = 0.25f;
                    break;
                case hitLocation_t::HL_CHEST_RT:
                case hitLocation_t::HL_ARM_RT:
                case hitLocation_t::HL_BACK_LT:
                    tagName = "*torso_cap_r_arm";
                    break;
                case hitLocation_t::HL_CHEST_LT:
                case hitLocation_t::HL_ARM_LT:
                case hitLocation_t::HL_BACK_RT:
                    tagName = "*torso_cap_l_arm";
                    break;
                case hitLocation_t::HL_HAND_RT:
                    tagName = "*r_arm_cap_r_hand";
                    break;
                case hitLocation_t::HL_HAND_LT:
                    tagName = "*l_arm_cap_l_hand";
                    break;
                case hitLocation_t::HL_HEAD:
                    tagName = "*torso_cap_head";
                    aoa = 0.25f;
                    break;
                case hitLocation_t::HL_CHEST:
                case hitLocation_t::HL_BACK:
                case hitLocation_t::HL_FOOT_RT:
                case hitLocation_t::HL_FOOT_LT:
                default:
                    // no dismemberment possible with these, so no checks needed
                    break;
                }
                if (tagName) {
                    int tagBolt = gi.G2API_AddBolt(&ent->ghoul2[ent->playerModel], tagName);
                    if (tagBolt != -1) {
                        mdxaBone_t boltMatrix;
                        vec3_t tagOrg, tagDir, angles;
                        VectorSet(angles, 0, ent->currentAngles[YAW], 0);
                        gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel, tagBolt, &boltMatrix, angles, ent->currentOrigin, actualTime, nullptr,
                                               ent->s.modelScale);
                        gi.G2API_GiveMeVectorFromMatrix(boltMatrix, Eorientations::ORIGIN, tagOrg);
                        gi.G2API_GiveMeVectorFromMatrix(boltMatrix, Eorientations::NEGATIVE_Y, tagDir);
                        if (DistanceSquared(point, tagOrg) < 256) { // hit close
                            float dot = DotProduct(dir, tagDir);
                            if (dot < aoa && dot > -aoa) { // hit roughly perpendicular
                                dot = DotProduct(bladeDir, tagDir);
                                if (dot < aoa && dot > -aoa) { // blade was roughly perpendicular
                                    dismember = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return dismember;
}

hitLocation_t G_GetHitLocation(gentity_t* target, vec3_t ppoint)
{
    vec3_t point, point_dir;
    vec3_t forward, right, up;
    vec3_t tangles, tcenter;
    float udot, fdot, rdot;
    int Vertical, Forward, Lateral;
    int HitLoc;

    // get target forward, right and up
    if (target->client) { // ignore player's pitch and roll
        VectorSet(tangles, 0, target->currentAngles[YAW], 0);
    }

    AngleVectors(tangles, forward, right, up);

    // get center of target
    VectorAdd(target->absmin, target->absmax, tcenter);
    VectorScale(tcenter, 0.5, tcenter);

    // get impact point
    if (ppoint && !VectorCompare(ppoint, vec3_origin)) {
        VectorCopy(ppoint, point);
    }
    else {
        return hitLocation_t::HL_NONE;
    }

    /*
    //get impact dir
        if(pdir && !VectorCompare(pdir, vec3_origin))
        {
            VectorCopy(pdir, dir);
        }
        else
        {
            return;
        }

    //put point at controlled distance from center
        VectorSubtract(point, tcenter, tempvec);
        tempvec[2] = 0;
        hdist = VectorLength(tempvec);

        VectorMA(point, hdist - tradius, dir, point);
        //now a point on the surface of a cylinder with a radius of tradius
    */
    VectorSubtract(point, tcenter, point_dir);
    VectorNormalize(point_dir);

    // Get bottom to top (Vertical) position index
    udot = DotProduct(up, point_dir);
    if (udot > .800)
        Vertical = 4;
    else if (udot > .400)
        Vertical = 3;
    else if (udot > -.333)
        Vertical = 2;
    else if (udot > -.666)
        Vertical = 1;
    else
        Vertical = 0;

    // Get back to front (Forward) position index
    fdot = DotProduct(forward, point_dir);
    if (fdot > .666)
        Forward = 4;
    else if (fdot > .333)
        Forward = 3;
    else if (fdot > -.333)
        Forward = 2;
    else if (fdot > -.666)
        Forward = 1;
    else
        Forward = 0;

    // Get left to right (Lateral) position index
    rdot = DotProduct(right, point_dir);
    if (rdot > .666)
        Lateral = 4;
    else if (rdot > .333)
        Lateral = 3;
    else if (rdot > -.333)
        Lateral = 2;
    else if (rdot > -.666)
        Lateral = 1;
    else
        Lateral = 0;

    HitLoc = Vertical * 25 + Forward * 5 + Lateral;

    if (HitLoc <= 10) { // feet
        if (rdot > 0) {
            return hitLocation_t::HL_FOOT_RT;
        }
        else {
            return hitLocation_t::HL_FOOT_LT;
        }
    }
    else if (HitLoc <= 50) { // legs
        if (rdot > 0) {
            return hitLocation_t::HL_LEG_RT;
        }
        else {
            return hitLocation_t::HL_LEG_LT;
        }
    }
    else if (HitLoc == 56 || HitLoc == 60 || HitLoc == 61 || HitLoc == 65 || HitLoc == 66 || HitLoc == 70) { // hands
        if (rdot > 0) {
            return hitLocation_t::HL_HAND_RT;
        }
        else {
            return hitLocation_t::HL_HAND_LT;
        }
    }
    else if (HitLoc == 83 || HitLoc == 87 || HitLoc == 88 || HitLoc == 92 || HitLoc == 93 || HitLoc == 97) { // arms
        if (rdot > 0) {
            return hitLocation_t::HL_ARM_RT;
        }
        else {
            return hitLocation_t::HL_ARM_LT;
        }
    }
    else if ((HitLoc >= 107 && HitLoc <= 109) || (HitLoc >= 112 && HitLoc <= 114) || (HitLoc >= 117 && HitLoc <= 119)) { // head
        return hitLocation_t::HL_HEAD;
    }
    else {
        if (udot < 0.3) {
            return hitLocation_t::HL_WAIST;
        }
        else if (fdot < 0) {
            if (rdot > 0.4) {
                return hitLocation_t::HL_BACK_RT;
            }
            else if (rdot < -0.4) {
                return hitLocation_t::HL_BACK_LT;
            }
            else {
                return hitLocation_t::HL_BACK;
            }
        }
        else {
            if (rdot > 0.3) {
                return hitLocation_t::HL_CHEST_RT;
            }
            else if (rdot < -0.3) {
                return hitLocation_t::HL_CHEST_LT;
            }
            else {
                return hitLocation_t::HL_CHEST;
            }
        }
    }
    // return hitLocation_t::HL_NONE;
}

std::optional<animNumber_t> G_PickPainAnim(gentity_t* self, vec3_t point, int damage, hitLocation_t hitLoc = hitLocation_t::HL_NONE)
{
    if (hitLoc == hitLocation_t::HL_NONE) {
        hitLoc = G_GetHitLocation(self, point);
    }
    switch (hitLoc) {
    case hitLocation_t::HL_FOOT_RT:
        return animNumber_t::BOTH_PAIN12;
        // PAIN12 = right foot
        break;
    case hitLocation_t::HL_FOOT_LT:
        return std::nullopt;
        break;
    case hitLocation_t::HL_LEG_RT:
        if (!Q_irand(0, 1)) {
            return animNumber_t::BOTH_PAIN11;
        }
        else {
            return animNumber_t::BOTH_PAIN13;
        }
        // PAIN11 = twitch right leg
        // PAIN13 = right knee
        break;
    case hitLocation_t::HL_LEG_LT:
        return animNumber_t::BOTH_PAIN14;
        // PAIN14 = twitch left leg
        break;
    case hitLocation_t::HL_BACK_RT:
        return animNumber_t::BOTH_PAIN7;
        // PAIN7 = med left shoulder
        break;
    case hitLocation_t::HL_BACK_LT:
        return Q_irand(animNumber_t::BOTH_PAIN15, animNumber_t::BOTH_PAIN16);
        // PAIN15 = med right shoulder
        // PAIN16 = twitch right shoulder
        break;
    case hitLocation_t::HL_BACK:
        if (!Q_irand(0, 1)) {
            return animNumber_t::BOTH_PAIN1;
        }
        else {
            return animNumber_t::BOTH_PAIN5;
        }
        // PAIN1 = back
        // PAIN5 = same as 1
        break;
    case hitLocation_t::HL_CHEST_RT:
        return animNumber_t::BOTH_PAIN3;
        // PAIN3 = long, right shoulder
        break;
    case hitLocation_t::HL_CHEST_LT:
        return animNumber_t::BOTH_PAIN2;
        // PAIN2 = long, left shoulder
        break;
    case hitLocation_t::HL_WAIST:
    case hitLocation_t::HL_CHEST:
        if (!Q_irand(0, 3)) {
            return animNumber_t::BOTH_PAIN6;
        }
        else if (!Q_irand(0, 2)) {
            return animNumber_t::BOTH_PAIN8;
        }
        else if (!Q_irand(0, 1)) {
            return animNumber_t::BOTH_PAIN17;
        }
        else {
            return animNumber_t::BOTH_PAIN19;
        }
        // PAIN6 = gut
        // PAIN8 = chest
        // PAIN17 = twitch crotch
        // PAIN19 = med crotch
        break;
    case hitLocation_t::HL_ARM_RT:
    case hitLocation_t::HL_HAND_RT:
        return animNumber_t::BOTH_PAIN9;
        // PAIN9 = twitch right arm
        break;
    case hitLocation_t::HL_ARM_LT:
    case hitLocation_t::HL_HAND_LT:
        return animNumber_t::BOTH_PAIN10;
        // PAIN10 = twitch left arm
        break;
    case hitLocation_t::HL_HEAD:
        return animNumber_t::BOTH_PAIN4;
        // PAIN4 = head
        break;
    default:
        return std::nullopt;
        break;
    }
}

extern void G_BounceMissile(gentity_t* ent, trace_t* trace);
void LimbThink(gentity_t* ent)
{ // FIXME: just use object thinking?
    vec3_t origin;
    trace_t tr;

    ent->nextthink = level.time + FRAMETIME;

    if (ent->enemy) { // alert people that I am a piece of one of their friends
        AddSightEvent(ent->enemy, ent->currentOrigin, 384, alertEventLevel_e::AEL_DISCOVERED);
    }

    if (ent->s.pos.trType == trType_t::TR_STATIONARY) { // stopped
        if (level.time > ent->s.apos.trTime + ent->s.apos.trDuration) {
            ent->nextthink = level.time + Q_irand(5000, 15000);
            ent->e_ThinkFunc = thinkFunc_t::thinkF_G_FreeEntity;
            // FIXME: these keep drawing for a frame or so after being freed?!  See them lerp to origin of world...
        }
        else {
            EvaluateTrajectory(&ent->s.apos, level.time, ent->currentAngles);
        }
        return;
    }

    // get current position
    EvaluateTrajectory(&ent->s.pos, level.time, origin);
    // get current angles
    EvaluateTrajectory(&ent->s.apos, level.time, ent->currentAngles);

    // trace a line from the previous position to the current position,
    // ignoring interactions with the missile owner
    gi.trace(&tr, ent->currentOrigin, ent->mins, ent->maxs, origin, ent->owner ? ent->owner->s.number : ENTITYNUM_NONE, ent->clipmask,
             EG2_Collision::G2_NOCOLLIDE, 0);

    VectorCopy(tr.endpos, ent->currentOrigin);
    if (tr.startsolid) {
        tr.fraction = 0;
    }

    gi.linkentity(ent);

    if (tr.fraction != 1) {
        G_BounceMissile(ent, &tr);
        if (ent->s.pos.trType == trType_t::TR_STATIONARY) { // stopped, stop spinning
            // lay flat
            // pitch
            VectorCopy(ent->currentAngles, ent->s.apos.trBase);
            vec3_t flatAngles;
            if (ent->s.angles2[0] == -1) { // any pitch is okay
                flatAngles[0] = ent->currentAngles[0];
            }
            else { // lay flat
                if (ent->owner && ent->owner->client && ent->owner->client->NPC_class == class_t::CLASS_PROTOCOL &&
                    G_LimbAnim(*ent) == animNumber_t::BOTH_DISMEMBER_TORSO1) {
                    if (ent->currentAngles[0] > 0 || ent->currentAngles[0] < -180) {
                        flatAngles[0] = -90;
                    }
                    else {
                        flatAngles[0] = 90;
                    }
                }
                else {
                    if (ent->currentAngles[0] > 90 || ent->currentAngles[0] < -90) {
                        flatAngles[0] = 180;
                    }
                    else {
                        flatAngles[0] = 0;
                    }
                }
            }
            // yaw
            flatAngles[1] = ent->currentAngles[1];
            // roll
            if (ent->s.angles2[2] == -1) { // any roll is okay
                flatAngles[2] = ent->currentAngles[2];
            }
            else {
                if (ent->currentAngles[2] > 90 || ent->currentAngles[2] < -90) {
                    flatAngles[2] = 180;
                }
                else {
                    flatAngles[2] = 0;
                }
            }
            VectorSubtract(flatAngles, ent->s.apos.trBase, ent->s.apos.trDelta);
            for (int i = 0; i < 3; i++) {
                ent->s.apos.trDelta[i] = AngleNormalize180(ent->s.apos.trDelta[i]);
            }
            ent->s.apos.trTime = level.time;
            ent->s.apos.trDuration = 1000;
            ent->s.apos.trType = trType_t::TR_LINEAR_STOP;
            // VectorClear( ent->s.apos.trDelta );
        }
    }
}

EnumArray<float, hitLocation_t, hitLocation_t::HL_MAX> hitLocHealthPercentage = {{
    0.0f,  // hitLocation_t::HL_NONE = 0,
    0.05f, // HL_FOOT_RT,
    0.05f, // HL_FOOT_LT,
    0.20f, // HL_LEG_RT,
    0.20f, // HL_LEG_LT,
    0.30f, // HL_WAIST,
    0.15f, // HL_BACK_RT,
    0.15f, // HL_BACK_LT,
    0.30f, // HL_BACK,
    0.15f, // HL_CHEST_RT,
    0.15f, // HL_CHEST_LT,
    0.30f, // HL_CHEST,
    0.05f, // HL_ARM_RT,
    0.05f, // HL_ARM_LT,
    0.01f, // HL_HAND_RT,
    0.01f, // HL_HAND_LT,
    0.10f, // HL_HEAD
    0.0f,  // HL_GENERIC1,
    0.0f,  // HL_GENERIC2,
    0.0f,  // HL_GENERIC3,
    0.0f,  // HL_GENERIC4,
    0.0f,  // HL_GENERIC5,
    0.0f   // HL_GENERIC6
}};

EnumArray<const char*, hitLocation_t, hitLocation_t::HL_MAX> hitLocName = {{
    "none",                // hitLocation_t::HL_NONE = 0,
    "right foot",          // HL_FOOT_RT,
    "left foot",           // HL_FOOT_LT,
    "right leg",           // HL_LEG_RT,
    "left leg",            // HL_LEG_LT,
    "waist",               // HL_WAIST,
    "back right shoulder", // HL_BACK_RT,
    "back left shoulder",  // HL_BACK_LT,
    "back",                // HL_BACK,
    "front right shouler", // HL_CHEST_RT,
    "front left shoulder", // HL_CHEST_LT,
    "chest",               // HL_CHEST,
    "right arm",           // HL_ARM_RT,
    "left arm",            // HL_ARM_LT,
    "right hand",          // HL_HAND_RT,
    "left hand",           // HL_HAND_LT,
    "head",                // HL_HEAD
    "generic1",            // HL_GENERIC1,
    "generic2",            // HL_GENERIC2,
    "generic3",            // HL_GENERIC3,
    "generic4",            // HL_GENERIC4,
    "generic5",            // HL_GENERIC5,
    "generic6"             // HL_GENERIC6
}};

bool G_LimbLost(gentity_t* ent, hitLocation_t hitLoc)
{
    switch (hitLoc) {
    case hitLocation_t::HL_FOOT_RT:
        if (ent->locationDamage[hitLocation_t::HL_FOOT_RT] >= Q3_INFINITE) {
            return true;
        }
        // NOTE: falls through
    case hitLocation_t::HL_LEG_RT:
        // NOTE: feet fall through
        if (ent->locationDamage[hitLocation_t::HL_LEG_RT] >= Q3_INFINITE) {
            return true;
        }
        return false;

    case hitLocation_t::HL_FOOT_LT:
        if (ent->locationDamage[hitLocation_t::HL_FOOT_LT] >= Q3_INFINITE) {
            return true;
        }
        // NOTE: falls through
    case hitLocation_t::HL_LEG_LT:
        // NOTE: feet fall through
        if (ent->locationDamage[hitLocation_t::HL_LEG_LT] >= Q3_INFINITE) {
            return true;
        }
        return false;

    case hitLocation_t::HL_HAND_LT:
        if (ent->locationDamage[hitLocation_t::HL_HAND_LT] >= Q3_INFINITE) {
            return true;
        }
        // NOTE: falls through
    case hitLocation_t::HL_ARM_LT:
    case hitLocation_t::HL_CHEST_LT:
    case hitLocation_t::HL_BACK_RT:
        // NOTE: hand falls through
        if (ent->locationDamage[hitLocation_t::HL_ARM_LT] >= Q3_INFINITE || ent->locationDamage[hitLocation_t::HL_CHEST_LT] >= Q3_INFINITE ||
            ent->locationDamage[hitLocation_t::HL_BACK_RT] >= Q3_INFINITE || ent->locationDamage[hitLocation_t::HL_WAIST] >= Q3_INFINITE) {
            return true;
        }
        return false;

    case hitLocation_t::HL_HAND_RT:
        if (ent->locationDamage[hitLocation_t::HL_HAND_RT] >= Q3_INFINITE) {
            return true;
        }
        // NOTE: falls through
    case hitLocation_t::HL_ARM_RT:
    case hitLocation_t::HL_CHEST_RT:
    case hitLocation_t::HL_BACK_LT:
        // NOTE: hand falls through
        if (ent->locationDamage[hitLocation_t::HL_ARM_RT] >= Q3_INFINITE || ent->locationDamage[hitLocation_t::HL_CHEST_RT] >= Q3_INFINITE ||
            ent->locationDamage[hitLocation_t::HL_BACK_LT] >= Q3_INFINITE || ent->locationDamage[hitLocation_t::HL_WAIST] >= Q3_INFINITE) {
            return true;
        }
        return false;

    case hitLocation_t::HL_HEAD:
        if (ent->locationDamage[hitLocation_t::HL_HEAD] >= Q3_INFINITE) {
            return true;
        }
        // NOTE: falls through
    case hitLocation_t::HL_WAIST:
        // NOTE: head falls through
        if (ent->locationDamage[hitLocation_t::HL_WAIST] >= Q3_INFINITE) {
            return true;
        }
        return false;
    default:
        return (ent->locationDamage[hitLoc] >= Q3_INFINITE);
    }
}

static bool G_Dismember(gentity_t* ent, vec3_t point, const char* limbBone, const char* rotateBone, const char* limbName, const char* limbCapName,
                        const char* stubCapName, const char* limbTagName, const char* stubTagName, animNumber_t limbAnim, float limbRollBase,
                        float limbPitchBase, int damage, hitLocation_t hitLoc)
{
    int newBolt;
    vec3_t dir, newPoint, limbAngles = {0, ent->client->ps.legsYaw, 0};
    gentity_t* limb;
    trace_t trace;

    // make sure this limb hasn't been lopped off already!
    if (gi.G2API_GetSurfaceRenderStatus(&ent->ghoul2[ent->playerModel], limbName) != g2SurfaceFlags_t::G2SURFACEFLAG_NONE) { // already lost this limb
        return false;
    }

    // NOTE: only reason I have this next part is because G2API_GetSurfaceRenderStatus is *not* working
    if (G_LimbLost(ent, hitLoc)) { // already lost this limb
        return false;
    }

    // FIXME: when timescale is high, can sometimes cut off a surf that includes a surf that was already cut off
    // 0) create a limb ent
    VectorCopy(point, newPoint);
    newPoint[2] += 6;
    limb = G_Spawn();
    G_SetOrigin(limb, newPoint);
    // VectorCopy(ent->currentAngles,limbAngles);
    // G_SetAngles( limb, ent->currentAngles );
    VectorCopy(newPoint, limb->s.pos.trBase);
    // 1) copy the g2 instance of the victim into the limb
    gi.G2API_CopyGhoul2Instance(ent->ghoul2, limb->ghoul2, -1);
    limb->playerModel = 0; // assumption!
    limb->craniumBone = ent->craniumBone;
    limb->cervicalBone = ent->cervicalBone;
    limb->thoracicBone = ent->thoracicBone;
    limb->upperLumbarBone = ent->upperLumbarBone;
    limb->lowerLumbarBone = ent->lowerLumbarBone;
    limb->hipsBone = ent->hipsBone;
    limb->rootBone = ent->rootBone;
    // 2) set the root surf on the limb
    if (limbTagName) { // add smoke to cap tag
        newBolt = gi.G2API_AddBolt(&limb->ghoul2[limb->playerModel], limbTagName);
        if (newBolt != -1) {
            G_PlayEffect("blaster/smoke_bolton", limb->playerModel, newBolt, limb->s.number);
        }
    }
    /*
    if ( limbBone && hitLoc == hitLocation_t::HL_HEAD )
    {//stop the current anim on the limb?
        gi.G2API_StopBoneAnim( &limb->ghoul2[limb->playerModel], "model_root" );
        gi.G2API_StopBoneAnim( &limb->ghoul2[limb->playerModel], "motion" );
        gi.G2API_StopBoneAnim( &limb->ghoul2[limb->playerModel], "upper_lumbar" );
    }
    */
    gi.G2API_StopBoneAnimIndex(&limb->ghoul2[limb->playerModel], limb->hipsBone);

    gi.G2API_SetRootSurface(limb->ghoul2, limb->playerModel, limbName);
    /*
    if ( limbBone && hitLoc != hitLocation_t::HL_WAIST )
    {//play the dismember anim on the limb?
        //FIXME: screws up origin
        auto &animations = level.knownAnimFileSets[ent->client->clientInfo.animFileIndex].animations;
        //play the proper dismember anim on the limb
        gi.G2API_SetBoneAnim(&limb->ghoul2[limb->playerModel], 0, animations[limbAnim].firstFrame - 1,
                            animations[limbAnim].numFrames + animations[limbAnim].firstFrame - 1,
                            boneFlags_t::BONE_ANIM_OVERRIDE_FREEZE, 1, cg.time);
    }
    */
    if (limbBone && hitLoc == hitLocation_t::HL_WAIST && ent->client->NPC_class == class_t::CLASS_PROTOCOL) { // play the dismember anim on the limb?
        gi.G2API_StopBoneAnim(&limb->ghoul2[limb->playerModel], "model_root");
        gi.G2API_StopBoneAnim(&limb->ghoul2[limb->playerModel], "motion");
        gi.G2API_StopBoneAnim(&limb->ghoul2[limb->playerModel], "pelvis");
        gi.G2API_StopBoneAnim(&limb->ghoul2[limb->playerModel], "upper_lumbar");
        // FIXME: screws up origin
        auto& animations = level.knownAnimFileSets[ent->client->clientInfo.animFileIndex].animations;
        // play the proper dismember anim on the limb
        gi.G2API_SetBoneAnim(&limb->ghoul2[limb->playerModel], nullptr, animations[limbAnim].firstFrame,
                             animations[limbAnim].numFrames + animations[limbAnim].firstFrame, boneFlags_t::BONE_ANIM_OVERRIDE_FREEZE, 1, cg.time, -1, -1);
    }
    if (rotateBone) {
        gi.G2API_SetNewOrigin(&limb->ghoul2[0], gi.G2API_AddBolt(&limb->ghoul2[0], rotateBone));

        // now let's try to position the limb at the *exact* right spot
        int newBolt = gi.G2API_AddBolt(&ent->ghoul2[0], rotateBone);
        if (newBolt != -1) {
            int actualTime = (cg.time ? cg.time : level.time);
            mdxaBone_t boltMatrix;
            vec3_t angles;

            VectorSet(angles, 0, ent->currentAngles[YAW], 0);
            gi.G2API_GetBoltMatrix(ent->ghoul2, ent->playerModel, newBolt, &boltMatrix, angles, ent->currentOrigin, actualTime, nullptr, ent->s.modelScale);
            gi.G2API_GiveMeVectorFromMatrix(boltMatrix, Eorientations::ORIGIN, limb->s.origin);
            G_SetOrigin(limb, limb->s.origin);
            VectorCopy(limb->s.origin, limb->s.pos.trBase);
            // angles, too
            /*
            vec3_t	limbF, limbR;
            newBolt = gi.G2API_AddBolt( &ent->ghoul2[0], limbBone );
            if ( newBolt != -1 )
            {
                gi.G2API_GetBoltMatrix( ent->ghoul2, ent->playerModel, newBolt,
                                &boltMatrix, angles, ent->currentOrigin,
                                actualTime, nullptr, ent->s.modelScale );
                gi.G2API_GiveMeVectorFromMatrix( boltMatrix, Eorientations::POSITIVE_X, limbF );
                gi.G2API_GiveMeVectorFromMatrix( boltMatrix, Eorientations::NEGATIVE_Y, limbR );
                vectoangles( limbF, limbAngles );
                vectoangles( limbR, angles );
                limbAngles[YAW] += 180;
                limbAngles[ROLL] = angles[PITCH]*-1.0f;
            }
            */
        }
    }
    if (limbCapName) { // turn on caps
        gi.G2API_SetSurfaceOnOff(&limb->ghoul2[limb->playerModel], limbCapName, g2SurfaceFlags_t::G2SURFACEFLAG_NONE);
    }
    // 3) turn off w/descendants that surf in original model
    // NOTE: we actually change the ent's stuff on the cgame side so that there is no 50ms lag
    //		also, if the limb was going to start in solid, we can delete it and return
    if (stubTagName) { // add smoke to cap surf, spawn effect
        limb->target = G_NewString(stubTagName);
        /*
        newBolt = gi.G2API_AddBolt( &ent->ghoul2[ent->playerModel], stubTagName );
        if ( newBolt != -1 )
        {
            G_PlayEffect( "blaster/smoke_bolton", ent->playerModel, newBolt, ent->s.number);
        }
        */
    }
    if (limbName) {
        limb->target2 = G_NewString(limbName);
        // gi.G2API_SetSurfaceOnOff( &ent->ghoul2[ent->playerModel], limbName, g2SurfaceFlags_t::G2SURFACEFLAG_NODESCENDANTS );//G2SURFACEFLAG_NODESCENDANTS
    }
    if (stubCapName) { // turn on caps
        limb->target3 = G_NewString(stubCapName);
        // gi.G2API_SetSurfaceOnOff( &ent->ghoul2[ent->playerModel], stubCapName, g2SurfaceFlags_t::G2SURFACEFLAG_NONE );
    }
    G_SetLimbAnim(*limb, limbAnim);
    //
    limb->s.radius = 60;
    // 4) toss the limb away
    limb->classname = "limb";
    limb->owner = ent;
    limb->enemy = ent->enemy;
    if (ent->weaponModel >= 0 && !ent->client->ps.saberInFlight) { // the corpse hasn't dropped their weapon
        if (limbAnim == animNumber_t::BOTH_DISMEMBER_RARM ||
            limbAnim == animNumber_t::BOTH_DISMEMBER_TORSO1) //&& ent->s.weapon == weapon_t::WP_SABER && ent->weaponModel != -1 )
        {                                                    // FIXME: is this first check needed with this lower one?
            if (gi.G2API_GetSurfaceRenderStatus(&limb->ghoul2[0], "r_hand") ==
                g2SurfaceFlags_t::G2SURFACEFLAG_NONE) { // only copy the weapon over if the right hand is actually on this limb...
                // copy it to limb
                if (ent->s.weapon != weapon_t::WP_NONE) { // only if they actually still have a weapon
                    limb->s.weapon = ent->s.weapon;
                    limb->weaponModel = ent->weaponModel;
                } // else - weaponModel is not -1 but don't have a weapon?  Oops, somehow G2 model wasn't removed?
                // remove it on owner
                if (ent->weaponModel >= 0) {
                    gi.G2API_RemoveGhoul2Model(ent->ghoul2, ent->weaponModel);
                    ent->weaponModel = -1;
                }
                if (ent->client->ps.saberEntityNum != ENTITYNUM_NONE && ent->client->ps.saberEntityNum > 0) { // remove the owner ent's saber model and entity
                    if (g_entities[ent->client->ps.saberEntityNum].inuse) {
                        G_FreeEntity(&g_entities[ent->client->ps.saberEntityNum]);
                    }
                    ent->client->ps.saberEntityNum = ENTITYNUM_NONE;
                }
            }
            else {
                if (ent->weaponModel >= 0) {
                    gi.G2API_RemoveGhoul2Model(limb->ghoul2, ent->weaponModel);
                    limb->weaponModel = -1;
                }
            }
        }
        else {
            if (ent->weaponModel >= 0) {
                gi.G2API_RemoveGhoul2Model(limb->ghoul2, ent->weaponModel);
                limb->weaponModel = -1;
            }
        }
    }

    limb->e_clThinkFunc = clThinkFunc_t::clThinkF_CG_Limb;
    limb->e_ThinkFunc = thinkFunc_t::thinkF_LimbThink;
    limb->nextthink = level.time + FRAMETIME;
    gi.linkentity(limb);
    // need size, contents, clipmask
    limb->svFlags = svFlags_t::SVF_USE_CURRENT_ORIGIN;
    limb->clipmask = MASK_SOLID;
    limb->contents = contentsFlags_t::CONTENTS_CORPSE;
    VectorSet(limb->mins, -3.0f, -3.0f, -6.0f);
    VectorSet(limb->maxs, 3.0f, 3.0f, 6.0f);

    // make sure it doesn't start in solid
    gi.trace(&trace, limb->s.pos.trBase, limb->mins, limb->maxs, limb->s.pos.trBase, limb->s.number, limb->clipmask, EG2_Collision::G2_NOCOLLIDE, 0);
    if (trace.startsolid) {
        limb->s.pos.trBase[2] -= limb->mins[2];
        gi.trace(&trace, limb->s.pos.trBase, limb->mins, limb->maxs, limb->s.pos.trBase, limb->s.number, limb->clipmask, EG2_Collision::G2_NOCOLLIDE, 0);
        if (trace.startsolid) {
            limb->s.pos.trBase[2] += limb->mins[2];
            gi.trace(&trace, limb->s.pos.trBase, limb->mins, limb->maxs, limb->s.pos.trBase, limb->s.number, limb->clipmask, EG2_Collision::G2_NOCOLLIDE, 0);
            if (trace.startsolid) { // stuck?  don't remove
                G_FreeEntity(limb);
                return false;
            }
        }
    }

    // move it
    VectorCopy(limb->s.pos.trBase, limb->currentOrigin);
    gi.linkentity(limb);

    limb->s.eType = entityType_t::ET_THINKER; // entityType_t::ET_GENERAL;
    limb->physicsBounce = 0.2f;
    limb->s.pos.trType = trType_t::TR_GRAVITY;
    limb->s.pos.trTime = level.time; // move a bit on the very first frame
    VectorSubtract(point, ent->currentOrigin, dir);
    VectorNormalize(dir);
    // no trDuration?
    // spin it
    // new way- try to preserve the exact angle and position of the limb as it was when attached
    VectorSet(limb->s.angles2, limbPitchBase, 0, limbRollBase);
    VectorCopy(limbAngles, limb->s.apos.trBase);
    /*
    //old way- just set an angle...
    limb->s.apos.trBase[0] += limbPitchBase;
    limb->s.apos.trBase[1] = ent->client->ps.viewangles[1];
    limb->s.apos.trBase[2] += limbRollBase;
    */
    limb->s.apos.trTime = level.time;
    limb->s.apos.trType = trType_t::TR_LINEAR;
    VectorClear(limb->s.apos.trDelta);

    if (hitLoc == hitLocation_t::HL_HAND_RT || hitLoc == hitLocation_t::HL_HAND_LT) { // hands fly farther
        VectorMA(ent->client->ps.velocity, 200, dir, limb->s.pos.trDelta);
        // make it bounce some
        limb->s.eFlags |= entityFlags_t::EF_BOUNCE_HALF;
        limb->s.apos.trDelta[0] = Q_irand(-300, 300);
        limb->s.apos.trDelta[1] = Q_irand(-800, 800);
    }
    else if (limbAnim == animNumber_t::BOTH_DISMEMBER_HEAD1 || limbAnim == animNumber_t::BOTH_DISMEMBER_LARM ||
             limbAnim == animNumber_t::BOTH_DISMEMBER_RARM) { // head and arms don't fly as far
        limb->s.eFlags |= entityFlags_t::EF_BOUNCE_SHRAPNEL;
        VectorMA(ent->client->ps.velocity, 150, dir, limb->s.pos.trDelta);
        limb->s.apos.trDelta[0] = Q_irand(-200, 200);
        limb->s.apos.trDelta[1] = Q_irand(-400, 400);
    }
    else // if ( limbAnim == animNumber_t::BOTH_DISMEMBER_TORSO1 || limbAnim == animNumber_t::BOTH_DISMEMBER_LLEG || limbAnim ==
         // animNumber_t::BOTH_DISMEMBER_RLEG )
    {    // everything else just kinda falls off
        limb->s.eFlags |= entityFlags_t::EF_BOUNCE_SHRAPNEL;
        VectorMA(ent->client->ps.velocity, 100, dir, limb->s.pos.trDelta);
        limb->s.apos.trDelta[0] = Q_irand(-100, 100);
        limb->s.apos.trDelta[1] = Q_irand(-200, 200);
    }
    // roll? No, doesn't work...
    // limb->s.apos.trDelta[2] = Q_irand( -300, 300 );//FIXME: this scales it down @ 80% and does weird stuff in timescale != 1.0
    // limb->s.apos.trDelta[2] = limbRoll;

    // preserve scale so giants don't have tiny limbs
    VectorCopy(ent->s.modelScale, limb->s.modelScale);

    // mark ent as dismembered
    ent->locationDamage[hitLoc] = Q3_INFINITE; // mark this limb as gone
    ent->client->dismembered = true;

    return true;
}

static bool G_Dismemberable(gentity_t* self, hitLocation_t hitLoc)
{
    if (self->client->dismembered) { // cannot dismember me right now
        return false;
    }
    if (g_dismemberment->integer < 11'381'138 && !g_saberRealisticCombat->integer) {
        if (g_dismemberProbabilities->value > 0.0f) { // use the ent-specific dismemberProbabilities
            float dismemberProb = 0;
            // check which part of the body it is. Then check the npc's probability
            // of that body part coming off, if it doesn't pass, return out.
            switch (hitLoc) {
            case hitLocation_t::HL_LEG_RT:
            case hitLocation_t::HL_LEG_LT:
                dismemberProb = self->client->dismemberProbLegs;
                break;
            case hitLocation_t::HL_WAIST:
                dismemberProb = self->client->dismemberProbWaist;
                break;
            case hitLocation_t::HL_BACK_RT:
            case hitLocation_t::HL_BACK_LT:
            case hitLocation_t::HL_CHEST_RT:
            case hitLocation_t::HL_CHEST_LT:
            case hitLocation_t::HL_ARM_RT:
            case hitLocation_t::HL_ARM_LT:
                dismemberProb = self->client->dismemberProbArms;
                break;
            case hitLocation_t::HL_HAND_RT:
            case hitLocation_t::HL_HAND_LT:
                dismemberProb = self->client->dismemberProbHands;
                break;
            case hitLocation_t::HL_HEAD:
                dismemberProb = self->client->dismemberProbHead;
                break;
            default:
                return false;
                break;
            }

            // check probability of this happening on this npc
            if (Q_floorf((Q_flrand(1, 100) * g_dismemberProbabilities->value)) >
                dismemberProb * 2.0f) // probabilities seemed really really low, had to crank them up
            {
                return false;
            }
        }
    }
    return true;
}

static bool G_Dismemberable2(gentity_t* self, hitLocation_t hitLoc)
{
    if (self->client->dismembered) { // cannot dismember me right now
        return false;
    }
    if (g_dismemberment->integer < 11'381'138 && !g_saberRealisticCombat->integer) {
        if (g_dismemberProbabilities->value <=
            0.0f) { // add the passed-in damage to the locationDamage array, check to see if it's taken enough damage to actually dismember
            if (self->locationDamage[hitLoc] < (self->client->ps.stats[statIndex_t::STAT_MAX_HEALTH] *
                                                hitLocHealthPercentage[hitLoc])) { // this location has not taken enough damage to dismember
                return false;
            }
        }
    }
    return true;
}

extern bool G_StandardHumanoid(const char* modelName);
bool G_DoDismemberment(gentity_t* self, vec3_t point, meansOfDeath_t mod, int damage, hitLocation_t hitLoc, bool force = false)
{
    extern cvar_t* g_iscensored;
    // dismemberment -- FIXME: should have a check for how long npc has been dead so people can't
    // continue to dismember a dead body long after it's been dead
    // NOTE that you can only cut one thing off unless the dismemberment is >= 11381138
#ifdef GERMAN_CENSORED
    if (0) // germany == censorship
#else
    if (!g_iscensored->integer && (g_dismemberment->integer || g_saberRealisticCombat->integer > 1) && mod == meansOfDeath_t::MOD_SABER) // only lightsaber
#endif
    { // FIXME: don't do strcmps here
        if (G_StandardHumanoid(self->NPC_type) && (force || g_dismemberProbabilities->value > 0.0f ||
                                                   G_Dismemberable2(self, hitLoc))) { // either it's a forced dismemberment or we're using probabilities (which
                                                                                      // are checked before this) or we've done enough damage to this location
            // FIXME: check the hitLoc and hitDir against the cap tag for the place
            // where the split will be- if the hit dir is roughly perpendicular to
            // the direction of the cap, then the split is allowed, otherwise we
            // hit it at the wrong angle and should not dismember...
            const char *limbBone = nullptr, *rotateBone = nullptr, *limbName = nullptr, *limbCapName = nullptr, *stubCapName = nullptr, *limbTagName = nullptr,
                       *stubTagName = nullptr;
            std::optional<animNumber_t> anim;
            float limbRollBase = 0, limbPitchBase = 0;
            bool doDismemberment = false;

            switch (hitLoc) // self->hitLoc
            {
            case hitLocation_t::HL_LEG_RT:
                if (g_dismemberment->integer > 1) {
                    doDismemberment = true;
                    limbBone = "rtibia";
                    rotateBone = "rtalus";
                    limbName = "r_leg";
                    limbCapName = "r_leg_cap_hips_off";
                    stubCapName = "hips_cap_r_leg_off";
                    limbTagName = "*r_leg_cap_hips";
                    stubTagName = "*hips_cap_r_leg";
                    anim = animNumber_t::BOTH_DISMEMBER_RLEG;
                    limbRollBase = 0;
                    limbPitchBase = 0;
                }
                break;
            case hitLocation_t::HL_LEG_LT:
                if (g_dismemberment->integer > 1) {
                    doDismemberment = true;
                    limbBone = "ltibia";
                    rotateBone = "ltalus";
                    limbName = "l_leg";
                    limbCapName = "l_leg_cap_hips_off";
                    stubCapName = "hips_cap_l_leg_off";
                    limbTagName = "*l_leg_cap_hips";
                    stubTagName = "*hips_cap_l_leg";
                    anim = animNumber_t::BOTH_DISMEMBER_LLEG;
                    limbRollBase = 0;
                    limbPitchBase = 0;
                }
                break;
            case hitLocation_t::HL_WAIST:
                if (g_dismemberment->integer > 2 && (!self->s.number || !self->message)) {
                    doDismemberment = true;
                    limbBone = "pelvis";
                    rotateBone = "thoracic";
                    limbName = "torso";
                    limbCapName = "torso_cap_hips_off";
                    stubCapName = "hips_cap_torso_off";
                    limbTagName = "*torso_cap_hips";
                    stubTagName = "*hips_cap_torso";
                    anim = animNumber_t::BOTH_DISMEMBER_TORSO1;
                    limbRollBase = 0;
                    limbPitchBase = 0;
                }
                break;
            case hitLocation_t::HL_CHEST_RT:
            case hitLocation_t::HL_ARM_RT:
            case hitLocation_t::HL_BACK_RT:
                if (g_dismemberment->integer) {
                    doDismemberment = true;
                    limbBone = "rhumerus";
                    rotateBone = "rradius";
                    limbName = "r_arm";
                    limbCapName = "r_arm_cap_torso_off";
                    stubCapName = "torso_cap_r_arm_off";
                    limbTagName = "*r_arm_cap_torso";
                    stubTagName = "*torso_cap_r_arm";
                    anim = animNumber_t::BOTH_DISMEMBER_RARM;
                    limbRollBase = 0;
                    limbPitchBase = 0;
                }
                break;
            case hitLocation_t::HL_CHEST_LT:
            case hitLocation_t::HL_ARM_LT:
            case hitLocation_t::HL_BACK_LT:
                if (g_dismemberment->integer && (!self->s.number || !self->message)) { // either the player or not carrying a key on my arm
                    doDismemberment = true;
                    limbBone = "lhumerus";
                    rotateBone = "lradius";
                    limbName = "l_arm";
                    limbCapName = "l_arm_cap_torso_off";
                    stubCapName = "torso_cap_l_arm_off";
                    limbTagName = "*l_arm_cap_torso";
                    stubTagName = "*torso_cap_l_arm";
                    anim = animNumber_t::BOTH_DISMEMBER_LARM;
                    limbRollBase = 0;
                    limbPitchBase = 0;
                }
                break;
            case hitLocation_t::HL_HAND_RT:
                if (g_dismemberment->integer) {
                    doDismemberment = true;
                    limbBone = "rradiusX";
                    rotateBone = "rhand";
                    limbName = "r_hand";
                    limbCapName = "r_hand_cap_r_arm_off";
                    stubCapName = "r_arm_cap_r_hand_off";
                    limbTagName = "*r_hand_cap_r_arm";
                    stubTagName = "*r_arm_cap_r_hand";
                    anim = animNumber_t::BOTH_DISMEMBER_RARM;
                    limbRollBase = 0;
                    limbPitchBase = 0;
                }
                break;
            case hitLocation_t::HL_HAND_LT:
                if (g_dismemberment->integer) {
                    doDismemberment = true;
                    limbBone = "lradiusX";
                    rotateBone = "lhand";
                    limbName = "l_hand";
                    limbCapName = "l_hand_cap_l_arm_off";
                    stubCapName = "l_arm_cap_l_hand_off";
                    limbTagName = "*l_hand_cap_l_arm";
                    stubTagName = "*l_arm_cap_l_hand";
                    anim = animNumber_t::BOTH_DISMEMBER_RARM;
                    limbRollBase = 0;
                    limbPitchBase = 0;
                }
                break;
            case hitLocation_t::HL_HEAD:
                if (g_dismemberment->integer > 2) {
                    doDismemberment = true;
                    limbBone = "cervical";
                    rotateBone = "cranium";
                    limbName = "head";
                    limbCapName = "head_cap_torso_off";
                    stubCapName = "torso_cap_head_off";
                    limbTagName = "*head_cap_torso";
                    stubTagName = "*torso_cap_head";
                    anim = animNumber_t::BOTH_DISMEMBER_HEAD1;
                    limbRollBase = -1;
                    limbPitchBase = -1;
                }
                break;
            case hitLocation_t::HL_FOOT_RT:
            case hitLocation_t::HL_FOOT_LT:
            case hitLocation_t::HL_CHEST:
            case hitLocation_t::HL_BACK:
            default:
                break;
            }
            if (doDismemberment) {
                return G_Dismember(self, point, limbBone, rotateBone, limbName, limbCapName, stubCapName, limbTagName, stubTagName, *anim, limbRollBase,
                                   limbPitchBase, damage, hitLoc);
            }
        }
    }
    return false;
}

static std::optional<animNumber_t> G_CheckSpecialDeathAnim(gentity_t* self, vec3_t point, int damage, meansOfDeath_t mod, hitLocation_t hitLoc)
{
    std::optional<animNumber_t> deathAnim;

    if (PM_InRoll(&self->client->ps)) {
        deathAnim = animNumber_t::BOTH_DEATH_ROLL; // # Death anim from a roll
    }
    else if (PM_FlippingAnim(self->client->ps.legsAnim)) {
        deathAnim = animNumber_t::BOTH_DEATH_FLIP; // # Death anim from a flip
    }
    else if (PM_SpinningAnim(self->client->ps.legsAnim)) {
        float yawDiff = AngleNormalize180(AngleNormalize180(self->client->renderInfo.torsoAngles[YAW]) - AngleNormalize180(self->client->ps.viewangles[YAW]));
        if (yawDiff > 135 || yawDiff < -135) {
            deathAnim = animNumber_t::BOTH_DEATH_SPIN_180; // # Death anim when facing backwards
        }
        else if (yawDiff < -60) {
            deathAnim = animNumber_t::BOTH_DEATH_SPIN_90_R; // # Death anim when facing 90 degrees right
        }
        else if (yawDiff > 60) {
            deathAnim = animNumber_t::BOTH_DEATH_SPIN_90_L; // # Death anim when facing 90 degrees left
        }
    }
    else if (PM_InKnockDown(&self->client->ps)) { // since these happen a lot, let's handle them case by case
        int animLength = PM_AnimLength(self->client->clientInfo.animFileIndex, self->client->ps.legsAnim);
        switch (self->client->ps.legsAnim) {
        case animNumber_t::BOTH_KNOCKDOWN1:
            if (animLength - self->client->ps.legsAnimTimer > 100) { // on our way down
                if (self->client->ps.legsAnimTimer > 600) {          // still partially up
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_UP;
                }
                else { // down
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_UP;
                }
            }
            break;
        case animNumber_t::BOTH_KNOCKDOWN2:
            if (animLength - self->client->ps.legsAnimTimer > 700) { // on our way down
                if (self->client->ps.legsAnimTimer > 600) {          // still partially up
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_UP;
                }
                else { // down
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_UP;
                }
            }
            break;
        case animNumber_t::BOTH_KNOCKDOWN3:
            if (animLength - self->client->ps.legsAnimTimer > 100) { // on our way down
                if (self->client->ps.legsAnimTimer > 1300) {         // still partially up
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_DN;
                }
                else { // down
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_DN;
                }
            }
            break;
        case animNumber_t::BOTH_KNOCKDOWN4:
            if (animLength - self->client->ps.legsAnimTimer > 300) { // on our way down
                if (self->client->ps.legsAnimTimer > 350) {          // still partially up
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_UP;
                }
                else { // down
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_UP;
                }
            }
            else { // crouch death
                vec3_t fwd;
                AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
                float thrown = DotProduct(fwd, self->client->ps.velocity);
                if (thrown < -150) {
                    deathAnim = animNumber_t::BOTH_DEATHBACKWARD1; // # Death anim when crouched and thrown back
                }
                else {
                    deathAnim = animNumber_t::BOTH_DEATH_CROUCHED; // # Death anim when crouched
                }
            }
            break;
        case animNumber_t::BOTH_KNOCKDOWN5:
            if (self->client->ps.legsAnimTimer < 750) { // flat
                deathAnim = animNumber_t::BOTH_DEATH_LYING_DN;
            }
            break;
        case animNumber_t::BOTH_GETUP1:
            if (self->client->ps.legsAnimTimer < 350) { // standing up
            }
            else if (self->client->ps.legsAnimTimer < 800) { // crouching
                vec3_t fwd;
                AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
                float thrown = DotProduct(fwd, self->client->ps.velocity);
                if (thrown < -150) {
                    deathAnim = animNumber_t::BOTH_DEATHBACKWARD1; // # Death anim when crouched and thrown back
                }
                else {
                    deathAnim = animNumber_t::BOTH_DEATH_CROUCHED; // # Death anim when crouched
                }
            }
            else {                                                       // lying down
                if (animLength - self->client->ps.legsAnimTimer > 450) { // partially up
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_UP;
                }
                else { // down
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_UP;
                }
            }
            break;
        case animNumber_t::BOTH_GETUP2:
            if (self->client->ps.legsAnimTimer < 150) { // standing up
            }
            else if (self->client->ps.legsAnimTimer < 850) { // crouching
                vec3_t fwd;
                AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
                float thrown = DotProduct(fwd, self->client->ps.velocity);
                if (thrown < -150) {
                    deathAnim = animNumber_t::BOTH_DEATHBACKWARD1; // # Death anim when crouched and thrown back
                }
                else {
                    deathAnim = animNumber_t::BOTH_DEATH_CROUCHED; // # Death anim when crouched
                }
            }
            else {                                                       // lying down
                if (animLength - self->client->ps.legsAnimTimer > 500) { // partially up
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_UP;
                }
                else { // down
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_UP;
                }
            }
            break;
        case animNumber_t::BOTH_GETUP3:
            if (self->client->ps.legsAnimTimer < 250) { // standing up
            }
            else if (self->client->ps.legsAnimTimer < 600) { // crouching
                vec3_t fwd;
                AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
                float thrown = DotProduct(fwd, self->client->ps.velocity);
                if (thrown < -150) {
                    deathAnim = animNumber_t::BOTH_DEATHBACKWARD1; // # Death anim when crouched and thrown back
                }
                else {
                    deathAnim = animNumber_t::BOTH_DEATH_CROUCHED; // # Death anim when crouched
                }
            }
            else {                                                       // lying down
                if (animLength - self->client->ps.legsAnimTimer > 150) { // partially up
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_DN;
                }
                else { // down
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_DN;
                }
            }
            break;
        case animNumber_t::BOTH_GETUP4:
            if (self->client->ps.legsAnimTimer < 250) { // standing up
            }
            else if (self->client->ps.legsAnimTimer < 600) { // crouching
                vec3_t fwd;
                AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
                float thrown = DotProduct(fwd, self->client->ps.velocity);
                if (thrown < -150) {
                    deathAnim = animNumber_t::BOTH_DEATHBACKWARD1; // # Death anim when crouched and thrown back
                }
                else {
                    deathAnim = animNumber_t::BOTH_DEATH_CROUCHED; // # Death anim when crouched
                }
            }
            else {                                                       // lying down
                if (animLength - self->client->ps.legsAnimTimer > 850) { // partially up
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_DN;
                }
                else { // down
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_UP;
                }
            }
            break;
        case animNumber_t::BOTH_GETUP5:
            if (self->client->ps.legsAnimTimer > 850) {                   // lying down
                if (animLength - self->client->ps.legsAnimTimer > 1500) { // partially up
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_DN;
                }
                else { // down
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_DN;
                }
            }
            break;
        case animNumber_t::BOTH_GETUP_CROUCH_B1:
            if (self->client->ps.legsAnimTimer < 800) { // crouching
                vec3_t fwd;
                AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
                float thrown = DotProduct(fwd, self->client->ps.velocity);
                if (thrown < -150) {
                    deathAnim = animNumber_t::BOTH_DEATHBACKWARD1; // # Death anim when crouched and thrown back
                }
                else {
                    deathAnim = animNumber_t::BOTH_DEATH_CROUCHED; // # Death anim when crouched
                }
            }
            else {                                                       // lying down
                if (animLength - self->client->ps.legsAnimTimer > 400) { // partially up
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_UP;
                }
                else { // down
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_UP;
                }
            }
            break;
        case animNumber_t::BOTH_GETUP_CROUCH_F1:
            if (self->client->ps.legsAnimTimer < 800) { // crouching
                vec3_t fwd;
                AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
                float thrown = DotProduct(fwd, self->client->ps.velocity);
                if (thrown < -150) {
                    deathAnim = animNumber_t::BOTH_DEATHBACKWARD1; // # Death anim when crouched and thrown back
                }
                else {
                    deathAnim = animNumber_t::BOTH_DEATH_CROUCHED; // # Death anim when crouched
                }
            }
            else {                                                       // lying down
                if (animLength - self->client->ps.legsAnimTimer > 150) { // partially up
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_DN;
                }
                else { // down
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_DN;
                }
            }
            break;
        case animNumber_t::BOTH_FORCE_GETUP_B1:
            if (self->client->ps.legsAnimTimer < 325) { // standing up
            }
            else if (self->client->ps.legsAnimTimer < 725) {   // spinning up
                deathAnim = animNumber_t::BOTH_DEATH_SPIN_180; // # Death anim when facing backwards
            }
            else if (self->client->ps.legsAnimTimer < 900) { // crouching
                vec3_t fwd;
                AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
                float thrown = DotProduct(fwd, self->client->ps.velocity);
                if (thrown < -150) {
                    deathAnim = animNumber_t::BOTH_DEATHBACKWARD1; // # Death anim when crouched and thrown back
                }
                else {
                    deathAnim = animNumber_t::BOTH_DEATH_CROUCHED; // # Death anim when crouched
                }
            }
            else {                                                      // lying down
                if (animLength - self->client->ps.legsAnimTimer > 50) { // partially up
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_UP;
                }
                else { // down
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_UP;
                }
            }
            break;
        case animNumber_t::BOTH_FORCE_GETUP_B2:
            if (self->client->ps.legsAnimTimer < 575) { // standing up
            }
            else if (self->client->ps.legsAnimTimer < 875) {   // spinning up
                deathAnim = animNumber_t::BOTH_DEATH_SPIN_180; // # Death anim when facing backwards
            }
            else if (self->client->ps.legsAnimTimer < 900) { // crouching
                vec3_t fwd;
                AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
                float thrown = DotProduct(fwd, self->client->ps.velocity);
                if (thrown < -150) {
                    deathAnim = animNumber_t::BOTH_DEATHBACKWARD1; // # Death anim when crouched and thrown back
                }
                else {
                    deathAnim = animNumber_t::BOTH_DEATH_CROUCHED; // # Death anim when crouched
                }
            }
            else { // lying down
                // partially up
                deathAnim = animNumber_t::BOTH_DEATH_FALLING_UP;
            }
            break;
        case animNumber_t::BOTH_FORCE_GETUP_B3:
            if (self->client->ps.legsAnimTimer < 150) { // standing up
            }
            else if (self->client->ps.legsAnimTimer < 775) {   // flipping
                deathAnim = animNumber_t::BOTH_DEATHBACKWARD2; // backflip
            }
            else { // lying down
                // partially up
                deathAnim = animNumber_t::BOTH_DEATH_FALLING_UP;
            }
            break;
        case animNumber_t::BOTH_FORCE_GETUP_B4:
            if (self->client->ps.legsAnimTimer < 325) { // standing up
            }
            else {                                                       // lying down
                if (animLength - self->client->ps.legsAnimTimer > 150) { // partially up
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_UP;
                }
                else { // down
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_UP;
                }
            }
            break;
        case animNumber_t::BOTH_FORCE_GETUP_B5:
            if (self->client->ps.legsAnimTimer < 550) { // standing up
            }
            else if (self->client->ps.legsAnimTimer < 1025) {  // kicking up
                deathAnim = animNumber_t::BOTH_DEATHBACKWARD2; // backflip
            }
            else {                                                      // lying down
                if (animLength - self->client->ps.legsAnimTimer > 50) { // partially up
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_UP;
                }
                else { // down
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_UP;
                }
            }
            break;
        case animNumber_t::BOTH_FORCE_GETUP_B6:
            if (self->client->ps.legsAnimTimer < 225) { // standing up
            }
            else if (self->client->ps.legsAnimTimer < 425) { // crouching up
                vec3_t fwd;
                AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
                float thrown = DotProduct(fwd, self->client->ps.velocity);
                if (thrown < -150) {
                    deathAnim = animNumber_t::BOTH_DEATHBACKWARD1; // # Death anim when crouched and thrown back
                }
                else {
                    deathAnim = animNumber_t::BOTH_DEATH_CROUCHED; // # Death anim when crouched
                }
            }
            else if (self->client->ps.legsAnimTimer < 825) {  // flipping up
                deathAnim = animNumber_t::BOTH_DEATHFORWARD3; // backflip
            }
            else {                                                       // lying down
                if (animLength - self->client->ps.legsAnimTimer > 225) { // partially up
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_UP;
                }
                else { // down
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_UP;
                }
            }
            break;
        case animNumber_t::BOTH_FORCE_GETUP_F1:
            if (self->client->ps.legsAnimTimer < 275) { // standing up
            }
            else if (self->client->ps.legsAnimTimer < 750) { // flipping
                deathAnim = animNumber_t::BOTH_DEATH14;
            }
            else {                                                       // lying down
                if (animLength - self->client->ps.legsAnimTimer > 100) { // partially up
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_DN;
                }
                else { // down
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_DN;
                }
            }
            break;
        case animNumber_t::BOTH_FORCE_GETUP_F2:
            if (self->client->ps.legsAnimTimer < 1200) { // standing
            }
            else {                                                       // lying down
                if (animLength - self->client->ps.legsAnimTimer > 225) { // partially up
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_DN;
                }
                else { // down
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_DN;
                }
            }
            break;
        default:
            break;
        }
    }
    else if (PM_InOnGroundAnim(&self->client->ps)) {
        if (AngleNormalize180(self->client->renderInfo.torsoAngles[PITCH]) < 0) {
            deathAnim = animNumber_t::BOTH_DEATH_LYING_UP; // # Death anim when lying on back
        }
        else {
            deathAnim = animNumber_t::BOTH_DEATH_LYING_DN; // # Death anim when lying on front
        }
    }
    else if (PM_CrouchAnim(self->client->ps.legsAnim)) {
        vec3_t fwd;
        AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
        float thrown = DotProduct(fwd, self->client->ps.velocity);
        if (thrown < -200) {
            deathAnim = animNumber_t::BOTH_DEATHBACKWARD1; // # Death anim when crouched and thrown back
            if (self->client->ps.velocity[2] > 0 && self->client->ps.velocity[2] < 100) {
                self->client->ps.velocity[2] = 100;
            }
        }
        else {
            deathAnim = animNumber_t::BOTH_DEATH_CROUCHED; // # Death anim when crouched
        }
    }

    return deathAnim;
}
extern bool PM_FinishedCurrentLegsAnim(gentity_t* self);
static std::optional<animNumber_t> G_PickDeathAnim(gentity_t* self, vec3_t point, int damage, meansOfDeath_t mod, hitLocation_t hitLoc)
{ // FIXME: play dead flop anims on body if in an appropriate _DEAD anim when this func is called
    std::optional<animNumber_t> deathAnim;
    if (hitLoc == hitLocation_t::HL_NONE) {
        hitLoc = G_GetHitLocation(self, point); // self->hitLoc
    }
    // dead flops...if you are already playing a death animation, I guess it can just return directly
    switch (self->client->ps.legsAnim) {
    case animNumber_t::BOTH_DEATH1: // # First Death anim
    case animNumber_t::BOTH_DEAD1:
    case animNumber_t::BOTH_DEATH2: // # Second Death anim
    case animNumber_t::BOTH_DEAD2:
    case animNumber_t::BOTH_DEATH8: // #
    case animNumber_t::BOTH_DEAD8:
    case animNumber_t::BOTH_DEATH13: // #
    case animNumber_t::BOTH_DEAD13:
    case animNumber_t::BOTH_DEATH14: // #
    case animNumber_t::BOTH_DEAD14:
    case animNumber_t::BOTH_DEATH16: // #
    case animNumber_t::BOTH_DEAD16:
    case animNumber_t::BOTH_DEADBACKWARD1: // # First thrown backward death finished pose
    case animNumber_t::BOTH_DEADBACKWARD2: // # Second thrown backward death finished pose
        return std::nullopt;
        break;
        /*
        if ( PM_FinishedCurrentLegsAnim( self ) )
        {//done with the anim
            deathAnim = animNumber_t::BOTH_DEADFLOP2;
        }
        else
        {
            deathAnim = std::nullopt;
        }
        break;
    case animNumber_t::BOTH_DEADFLOP2:
        deathAnim = animNumber_t::BOTH_DEADFLOP2;
        break;
        */
    case animNumber_t::BOTH_DEATH10: // #
    case animNumber_t::BOTH_DEAD10:
    case animNumber_t::BOTH_DEATH15: // #
    case animNumber_t::BOTH_DEAD15:
    case animNumber_t::BOTH_DEADFORWARD1: // # First thrown forward death finished pose
    case animNumber_t::BOTH_DEADFORWARD2: // # Second thrown forward death finished pose
        return std::nullopt;
        break;
        /*
        if ( PM_FinishedCurrentLegsAnim( self ) )
        {//done with the anim
            deathAnim = animNumber_t::BOTH_DEADFLOP1;
        }
        else
        {
            deathAnim = std::nullopt;
        }
        break;
        */
    case animNumber_t::BOTH_DEADFLOP1:
        // deathAnim = animNumber_t::BOTH_DEADFLOP1;
        return std::nullopt;
        break;
    case animNumber_t::BOTH_DEAD3:           // # Third Death finished pose
    case animNumber_t::BOTH_DEAD4:           // # Fourth Death finished pose
    case animNumber_t::BOTH_DEAD5:           // # Fifth Death finished pose
    case animNumber_t::BOTH_DEAD6:           // # Sixth Death finished pose
    case animNumber_t::BOTH_DEAD7:           // # Seventh Death finished pose
    case animNumber_t::BOTH_DEAD9:           // #
    case animNumber_t::BOTH_DEAD11:          // #
    case animNumber_t::BOTH_DEAD12:          // #
    case animNumber_t::BOTH_DEAD17:          // #
    case animNumber_t::BOTH_DEAD18:          // #
    case animNumber_t::BOTH_DEAD19:          // #
    case animNumber_t::BOTH_DEAD20:          // #
    case animNumber_t::BOTH_DEAD21:          // #
    case animNumber_t::BOTH_DEAD22:          // #
    case animNumber_t::BOTH_DEAD23:          // #
    case animNumber_t::BOTH_DEAD24:          // #
    case animNumber_t::BOTH_DEAD25:          // #
    case animNumber_t::BOTH_LYINGDEAD1:      // # Killed lying down death finished pose
    case animNumber_t::BOTH_STUMBLEDEAD1:    // # Stumble forward death finished pose
    case animNumber_t::BOTH_FALLDEAD1LAND:   // # Fall forward and splat death finished pose
    case animNumber_t::BOTH_DEATH3:          // # Third Death anim
    case animNumber_t::BOTH_DEATH4:          // # Fourth Death anim
    case animNumber_t::BOTH_DEATH5:          // # Fifth Death anim
    case animNumber_t::BOTH_DEATH6:          // # Sixth Death anim
    case animNumber_t::BOTH_DEATH7:          // # Seventh Death anim
    case animNumber_t::BOTH_DEATH9:          // #
    case animNumber_t::BOTH_DEATH11:         // #
    case animNumber_t::BOTH_DEATH12:         // #
    case animNumber_t::BOTH_DEATH17:         // #
    case animNumber_t::BOTH_DEATH18:         // #
    case animNumber_t::BOTH_DEATH19:         // #
    case animNumber_t::BOTH_DEATH20:         // #
    case animNumber_t::BOTH_DEATH21:         // #
    case animNumber_t::BOTH_DEATH22:         // #
    case animNumber_t::BOTH_DEATH23:         // #
    case animNumber_t::BOTH_DEATH24:         // #
    case animNumber_t::BOTH_DEATH25:         // #
    case animNumber_t::BOTH_DEATHFORWARD1:   // # First Death in which they get thrown forward
    case animNumber_t::BOTH_DEATHFORWARD2:   // # Second Death in which they get thrown forward
    case animNumber_t::BOTH_DEATHFORWARD3:   // # Second Death in which they get thrown forward
    case animNumber_t::BOTH_DEATHBACKWARD1:  // # First Death in which they get thrown backward
    case animNumber_t::BOTH_DEATHBACKWARD2:  // # Second Death in which they get thrown backward
    case animNumber_t::BOTH_DEATH1IDLE:      // # Idle while close to death
    case animNumber_t::BOTH_LYINGDEATH1:     // # Death to play when killed lying down
    case animNumber_t::BOTH_STUMBLEDEATH1:   // # Stumble forward and fall face first death
    case animNumber_t::BOTH_FALLDEATH1:      // # Fall forward off a high cliff and splat death - start
    case animNumber_t::BOTH_FALLDEATH1INAIR: // # Fall forward off a high cliff and splat death - loop
    case animNumber_t::BOTH_FALLDEATH1LAND:  // # Fall forward off a high cliff and splat death - hit bottom
        return std::nullopt;
        break;
    case animNumber_t::BOTH_DEATH_ROLL:       // # Death anim from a roll
    case animNumber_t::BOTH_DEATH_FLIP:       // # Death anim from a flip
    case animNumber_t::BOTH_DEATH_SPIN_90_R:  // # Death anim when facing 90 degrees right
    case animNumber_t::BOTH_DEATH_SPIN_90_L:  // # Death anim when facing 90 degrees left
    case animNumber_t::BOTH_DEATH_SPIN_180:   // # Death anim when facing backwards
    case animNumber_t::BOTH_DEATH_LYING_UP:   // # Death anim when lying on back
    case animNumber_t::BOTH_DEATH_LYING_DN:   // # Death anim when lying on front
    case animNumber_t::BOTH_DEATH_FALLING_DN: // # Death anim when falling on face
    case animNumber_t::BOTH_DEATH_FALLING_UP: // # Death anim when falling on back
    case animNumber_t::BOTH_DEATH_CROUCHED:   // # Death anim when crouched
    case animNumber_t::BOTH_RIGHTHANDCHOPPEDOFF:
        return std::nullopt;
        break;
    default:
        break;
    }
    // Not currently playing a death animation, so try and get an appropriate one now.
    if (!deathAnim) {
        deathAnim = G_CheckSpecialDeathAnim(self, point, damage, mod, hitLoc);

        if (!deathAnim) { // base on hitLoc
            vec3_t fwd;
            AngleVectors(self->currentAngles, fwd, nullptr, nullptr);
            float thrown = DotProduct(fwd, self->client->ps.velocity);
            // death anims
            switch (hitLoc) {
            case hitLocation_t::HL_FOOT_RT:
                if (!Q_irand(0, 2) && thrown < 250) {
                    deathAnim = animNumber_t::BOTH_DEATH24; // right foot trips up, spin
                }
                else if (!Q_irand(0, 1)) {
                    deathAnim = animNumber_t::BOTH_DEATH4; // back: forward
                }
                else {
                    deathAnim = animNumber_t::BOTH_DEATH5; // same as 4
                }
                break;
            case hitLocation_t::HL_FOOT_LT:
                if (!Q_irand(0, 2) && thrown < 250) {
                    deathAnim = animNumber_t::BOTH_DEATH25; // left foot trips up, spin
                }
                else if (!Q_irand(0, 1)) {
                    deathAnim = animNumber_t::BOTH_DEATH4; // back: forward
                }
                else {
                    deathAnim = animNumber_t::BOTH_DEATH5; // same as 4
                }
                break;
            case hitLocation_t::HL_LEG_RT:
                if (!Q_irand(0, 2) && thrown < 250) {
                    deathAnim = animNumber_t::BOTH_DEATH3; // right leg collapse
                }
                else if (!Q_irand(0, 1)) {
                    deathAnim = animNumber_t::BOTH_DEATH5; // same as 4
                }
                else {
                    deathAnim = animNumber_t::BOTH_DEATH4; // back: forward
                }
                break;
            case hitLocation_t::HL_LEG_LT:
                if (!Q_irand(0, 2) && thrown < 250) {
                    deathAnim = animNumber_t::BOTH_DEATH7; // left leg collapse
                }
                else if (!Q_irand(0, 1)) {
                    deathAnim = animNumber_t::BOTH_DEATH5; // same as 4
                }
                else {
                    deathAnim = animNumber_t::BOTH_DEATH4; // back: forward
                }
                break;
            case hitLocation_t::HL_BACK:
                if (Q_fabsf(thrown) < 50 || (Q_fabsf(thrown) < 200 && !Q_irand(0, 3))) {
                    if (Q_irand(0, 1)) {
                        deathAnim = animNumber_t::BOTH_DEATH17; // head/back: croak
                    }
                    else {
                        deathAnim = animNumber_t::BOTH_DEATH10; // back: bend back, fall forward
                    }
                }
                else {
                    if (!Q_irand(0, 1)) {
                        deathAnim = animNumber_t::BOTH_DEATH4; // back: forward
                    }
                    else {
                        deathAnim = animNumber_t::BOTH_DEATH5; // same as 4
                    }
                }
                break;
            case hitLocation_t::HL_HAND_RT:
            case hitLocation_t::HL_CHEST_RT:
            case hitLocation_t::HL_ARM_RT:
            case hitLocation_t::HL_BACK_LT:
                if ((damage <= self->max_health * 0.25 && Q_irand(0, 1)) || (Q_fabsf(thrown) < 200 && !Q_irand(0, 2)) || !Q_irand(0, 10)) {
                    if (Q_irand(0, 1)) {
                        deathAnim = animNumber_t::BOTH_DEATH9; // chest right: snap, fall forward
                    }
                    else {
                        deathAnim = animNumber_t::BOTH_DEATH20; // chest right: snap, fall forward
                    }
                }
                else if ((damage <= self->max_health * 0.5 && Q_irand(0, 1)) || !Q_irand(0, 10)) {
                    deathAnim = animNumber_t::BOTH_DEATH3; // chest right: back
                }
                else if ((damage <= self->max_health * 0.75 && Q_irand(0, 1)) || !Q_irand(0, 10)) {
                    deathAnim = animNumber_t::BOTH_DEATH6; // chest right: spin
                }
                else {
                    // TEMP HACK: play spinny deaths less often
                    if (Q_irand(0, 1)) {
                        deathAnim = animNumber_t::BOTH_DEATH8; // chest right: spin high
                    }
                    else {
                        switch (Q_irand(0, 3)) {
                        default:
                        case 0:
                            deathAnim = animNumber_t::BOTH_DEATH9; // chest right: snap, fall forward
                            break;
                        case 1:
                            deathAnim = animNumber_t::BOTH_DEATH3; // chest right: back
                            break;
                        case 2:
                            deathAnim = animNumber_t::BOTH_DEATH6; // chest right: spin
                            break;
                        case 3:
                            deathAnim = animNumber_t::BOTH_DEATH20; // chest right: spin
                            break;
                        }
                    }
                }
                break;
            case hitLocation_t::HL_CHEST_LT:
            case hitLocation_t::HL_ARM_LT:
            case hitLocation_t::HL_HAND_LT:
            case hitLocation_t::HL_BACK_RT:
                if ((damage <= self->max_health * 0.25 && Q_irand(0, 1)) || (Q_fabsf(thrown) < 200 && !Q_irand(0, 2)) || !Q_irand(0, 10)) {
                    if (Q_irand(0, 1)) {
                        deathAnim = animNumber_t::BOTH_DEATH11; // chest left: snap, fall forward
                    }
                    else {
                        deathAnim = animNumber_t::BOTH_DEATH21; // chest left: snap, fall forward
                    }
                }
                else if ((damage <= self->max_health * 0.5 && Q_irand(0, 1)) || !Q_irand(0, 10)) {
                    deathAnim = animNumber_t::BOTH_DEATH7; // chest left: back
                }
                else if ((damage <= self->max_health * 0.75 && Q_irand(0, 1)) || !Q_irand(0, 10)) {
                    deathAnim = animNumber_t::BOTH_DEATH12; // chest left: spin
                }
                else {
                    // TEMP HACK: play spinny deaths less often
                    if (Q_irand(0, 1)) {
                        deathAnim = animNumber_t::BOTH_DEATH14; // chest left: spin high
                    }
                    else {
                        switch (Q_irand(0, 3)) {
                        default:
                        case 0:
                            deathAnim = animNumber_t::BOTH_DEATH11; // chest left: snap, fall forward
                            break;
                        case 1:
                            deathAnim = animNumber_t::BOTH_DEATH7; // chest left: back
                            break;
                        case 2:
                            deathAnim = animNumber_t::BOTH_DEATH12; // chest left: spin
                            break;
                        case 3:
                            deathAnim = animNumber_t::BOTH_DEATH21; // chest left: spin
                            break;
                        }
                    }
                }
                break;
            case hitLocation_t::HL_CHEST:
            case hitLocation_t::HL_WAIST:
                if ((damage <= self->max_health * 0.25 && Q_irand(0, 1)) || thrown > -50) {
                    if (!Q_irand(0, 1)) {
                        deathAnim = animNumber_t::BOTH_DEATH18; // gut: fall right
                    }
                    else {
                        deathAnim = animNumber_t::BOTH_DEATH19; // gut: fall left
                    }
                }
                else if ((damage <= self->max_health * 0.5 && !Q_irand(0, 1)) || (Q_fabsf(thrown) < 200 && !Q_irand(0, 3))) {
                    if (Q_irand(0, 2)) {
                        deathAnim = animNumber_t::BOTH_DEATH2; // chest: backward short
                    }
                    else if (Q_irand(0, 1)) {
                        deathAnim = animNumber_t::BOTH_DEATH22; // chest: backward short
                    }
                    else {
                        deathAnim = animNumber_t::BOTH_DEATH23; // chest: backward short
                    }
                }
                else if (thrown < -300 && Q_irand(0, 1)) {
                    if (Q_irand(0, 1)) {
                        deathAnim = animNumber_t::BOTH_DEATHBACKWARD1; // chest: fly back
                    }
                    else {
                        deathAnim = animNumber_t::BOTH_DEATHBACKWARD2; // chest: flip back
                    }
                }
                else if (thrown < -200 && Q_irand(0, 1)) {
                    deathAnim = animNumber_t::BOTH_DEATH15; // chest: roll backward
                }
                else {
                    if (!Q_irand(0, 1)) {
                        deathAnim = animNumber_t::BOTH_DEATH1; // chest: backward med
                    }
                    else {
                        deathAnim = animNumber_t::BOTH_DEATH16; // same as 1
                    }
                }
                break;
            case hitLocation_t::HL_HEAD:
                if (damage <= self->max_health * 0.5 && Q_irand(0, 2)) {
                    deathAnim = animNumber_t::BOTH_DEATH17; // head/back: croak
                }
                else {
                    if (Q_irand(0, 2)) {
                        deathAnim = animNumber_t::BOTH_DEATH13; // head: stumble, fall back
                    }
                    else {
                        deathAnim = animNumber_t::BOTH_DEATH10; // head: stumble, fall back
                    }
                }
                break;
            default:
                break;
            }
        }
    }

    // Validate.....
    if (!deathAnim || !PM_HasAnimation(self, *deathAnim)) {
        // I guess we'll take what we can get.....
        deathAnim = PM_PickAnim(self, animNumber_t::BOTH_DEATH1, animNumber_t::BOTH_DEATH25);
    }

    return deathAnim;
}

int G_CheckLedgeDive(gentity_t* self, float checkDist, vec3_t checkVel, bool tryOpposite, bool tryPerp)
{
    //		Intelligent Ledge-Diving Deaths:
    //		If I'm an NPC, check for nearby ledges and fall off it if possible
    //		How should/would/could this interact with knockback if we already have some?
    //		Ideally - apply knockback if there are no ledges or a ledge in that dir
    //		But if there is a ledge and it's not in the dir of my knockback, fall off the ledge instead
    if (!self || !self->client) {
        return 0;
    }

    vec3_t fallForwardDir, fallRightDir;
    vec3_t angles = {0};
    int cliff_fall = 0;

    if (checkVel && !VectorCompare(checkVel, vec3_origin)) { // already moving in a dir
        angles[1] = vectoyaw(self->client->ps.velocity);
        AngleVectors(angles, fallForwardDir, fallRightDir, nullptr);
    }
    else { // try forward first
        angles[1] = self->client->ps.viewangles[1];
        AngleVectors(angles, fallForwardDir, fallRightDir, nullptr);
    }
    VectorNormalize(fallForwardDir);
    float fallDist = G_CheckForLedge(self, fallForwardDir, checkDist);
    if (fallDist >= 128) {
        VectorClear(self->client->ps.velocity);
        G_Throw(self, fallForwardDir, 85);
        self->client->ps.velocity[2] = 100;
        self->client->ps.groundEntityNum = ENTITYNUM_NONE;
    }
    else if (tryOpposite) {
        VectorScale(fallForwardDir, -1, fallForwardDir);
        fallDist = G_CheckForLedge(self, fallForwardDir, checkDist);
        if (fallDist >= 128) {
            VectorClear(self->client->ps.velocity);
            G_Throw(self, fallForwardDir, 85);
            self->client->ps.velocity[2] = 100;
            self->client->ps.groundEntityNum = ENTITYNUM_NONE;
        }
    }
    if (!cliff_fall && tryPerp) { // try sides
        VectorNormalize(fallRightDir);
        fallDist = G_CheckForLedge(self, fallRightDir, checkDist);
        if (fallDist >= 128) {
            VectorClear(self->client->ps.velocity);
            G_Throw(self, fallRightDir, 85);
            self->client->ps.velocity[2] = 100;
        }
        else {
            VectorScale(fallRightDir, -1, fallRightDir);
            fallDist = G_CheckForLedge(self, fallRightDir, checkDist);
            if (fallDist >= 128) {
                VectorClear(self->client->ps.velocity);
                G_Throw(self, fallRightDir, 85);
                self->client->ps.velocity[2] = 100;
            }
        }
    }
    if (fallDist >= 256) {
        cliff_fall = 2;
    }
    else if (fallDist >= 128) {
        cliff_fall = 1;
    }
    return cliff_fall;
}
/*
==================
player_die
==================
*/
void NPC_SetAnim(gentity_t* ent, setAnimParts_t type, animNumber_t anim, setAnimFlags_t priority);
extern void AI_DeleteSelfFromGroup(gentity_t* self);
extern void AI_GroupMemberKilled(gentity_t* self);
extern bool FlyingCreature(gentity_t* ent);
extern void G_DrivableATSTDie(gentity_t* self);
void player_die(gentity_t* self, gentity_t* inflictor, gentity_t* attacker, int damage, meansOfDeath_t meansOfDeath, damageFlags_t dflags, hitLocation_t hitLoc)
{
    std::optional<animNumber_t> anim;
    contentsFlags_t contents;
    bool deathScript = false;
    bool lastInGroup = false;
    bool specialAnim = false;
    bool holdingSaber = false;
    int cliff_fall = 0;

    // FIXME: somehow people are sometimes not completely dying???
    if (self->client->ps.pm_type == pmtype_t::PM_DEAD &&
        (meansOfDeath != meansOfDeath_t::MOD_SNIPER || Any(self->flags & gentityFlags_t::FL_DISINTEGRATED))) { // do dismemberment/twitching
        if (self->client->NPC_class == class_t::CLASS_MARK1) {
            DeathFX(self);
            self->takedamage = false;
            self->client->ps.eFlags |= entityFlags_t::EF_NODRAW;
            self->contents = contentsFlags_t::CONTENTS_NONE;
            // G_FreeEntity( self ); // Is this safe?  I can't see why we'd mark it nodraw and then just leave it around??
            self->e_ThinkFunc = thinkFunc_t::thinkF_G_FreeEntity;
            self->nextthink = level.time + FRAMETIME;
        }
        else {
            anim = G_PickDeathAnim(self, self->pos1, damage, meansOfDeath, hitLoc);
            if (Any(dflags & damageFlags_t::DAMAGE_DISMEMBER)) {
                G_DoDismemberment(self, self->pos1, meansOfDeath, damage, hitLoc);
            }
            if (anim) {
                NPC_SetAnim(self, setAnimParts_t::SETANIM_BOTH, *anim,
                            setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_RESTART | setAnimFlags_t::SETANIM_FLAG_HOLD);
            }
        }
        return;
    }

#ifndef FINAL_BUILD
    if (d_saberCombat->integer && attacker && attacker->client) {
        Q_Println(S_COLOR_YELLOW "combatant {} died, killer anim = {}", self->targetname, BG_AnimName(attacker->client->ps.torsoAnim));
    }
#endif // FINAL_BUILD

    if (self->NPC) {
        if (self->client && Jedi_WaitingAmbush(self)) { // ambushing trooper
            self->client->noclip = false;
        }
        NPC_FreeCombatPoint(self->NPC->combatPoint);
        if (self->NPC->group) {
            lastInGroup = (self->NPC->group->numGroup < 2);
            AI_GroupMemberKilled(self);
            AI_DeleteSelfFromGroup(self);
        }

        if (self->NPC->tempGoal) {
            G_FreeEntity(self->NPC->tempGoal);
            self->NPC->tempGoal = nullptr;
        }
        if (Any(self->s.eFlags & entityFlags_t::EF_LOCKED_TO_WEAPON)) {
            // dumb, just get the NPC out of the chair
            extern void RunEmplacedWeapon(gentity_t * ent, usercmd_t * *ucmd);

            usercmd_t cmd, *ad_cmd;

            memset(&cmd, 0, sizeof(usercmd_t));

            // gentity_t *old = self->owner;

            if (self->owner) {
                self->owner->s.frame = self->owner->startFrame = self->owner->endFrame = 0;
                self->owner->svFlags &= ~svFlags_t::SVF_ANIMATING;
            }

            cmd.buttons |= buttonFlags_t::BUTTON_USE;
            ad_cmd = &cmd;
            RunEmplacedWeapon(self, &ad_cmd);
            // self->owner = old;
        }
    }
    if (attacker && attacker->NPC && attacker->NPC->group && attacker->NPC->group->enemy == self) {
        attacker->NPC->group->enemy = nullptr;
    }
    if (self->s.weapon == weapon_t::WP_SABER) {
        holdingSaber = true;
    }
    if (self->client->ps.saberEntityNum != ENTITYNUM_NONE && self->client->ps.saberEntityNum > 0) {
        if (self->client->ps.saberInFlight) { // just drop it
            self->client->ps.saberActive = false;
        }
        else {
            if ((hitLoc != hitLocation_t::HL_HAND_RT || self->client->dismembered ||
                 meansOfDeath != meansOfDeath_t::MOD_SABER) // if might get hand cut off, leave saber in hand
                && holdingSaber &&
                (Q_irand(0, 1) || meansOfDeath == meansOfDeath_t::MOD_EXPLOSIVE || meansOfDeath == meansOfDeath_t::MOD_REPEATER_ALT ||
                 meansOfDeath == meansOfDeath_t::MOD_FLECHETTE_ALT || meansOfDeath == meansOfDeath_t::MOD_ROCKET ||
                 meansOfDeath == meansOfDeath_t::MOD_ROCKET_ALT || meansOfDeath == meansOfDeath_t::MOD_THERMAL ||
                 meansOfDeath == meansOfDeath_t::MOD_THERMAL_ALT || meansOfDeath == meansOfDeath_t::MOD_DETPACK ||
                 meansOfDeath == meansOfDeath_t::MOD_LASERTRIP || meansOfDeath == meansOfDeath_t::MOD_LASERTRIP_ALT ||
                 meansOfDeath == meansOfDeath_t::MOD_MELEE || meansOfDeath == meansOfDeath_t::MOD_FORCE_GRIP || meansOfDeath == meansOfDeath_t::MOD_KNOCKOUT ||
                 meansOfDeath == meansOfDeath_t::MOD_CRUSH || meansOfDeath == meansOfDeath_t::MOD_IMPACT || meansOfDeath == meansOfDeath_t::MOD_FALLING ||
                 meansOfDeath == meansOfDeath_t::MOD_EXPLOSIVE_SPLASH)) { // drop it
                TossClientItems(self);
            }
            else { // just free it
                if (g_entities[self->client->ps.saberEntityNum].inuse) {
                    G_FreeEntity(&g_entities[self->client->ps.saberEntityNum]);
                }
                self->client->ps.saberEntityNum = ENTITYNUM_NONE;
            }
        }
    }
    if (self->client->NPC_class == class_t::CLASS_SHADOWTROOPER) { // drop a force crystal
        gitem_t* item;
        item = FindItemForAmmo(ammo_t::AMMO_FORCE);
        Drop_Item(self, item, 0, true);
    }
    // Use any target we had
    if (meansOfDeath != meansOfDeath_t::MOD_KNOCKOUT) {
        G_UseTargets(self, self);
    }

    if (attacker) {
        if (attacker->client && !attacker->s.number) {
            if (self->client) {                                                                                             // killed a client
                if (self->client->playerTeam == team_t::TEAM_ENEMY || (self->NPC && self->NPC->charmedTime > level.time)) { // killed an enemy
                    attacker->client->sess.missionStats.enemiesKilled++;
                }
            }
            if (attacker != self) {
                G_TrackWeaponUsage(attacker, inflictor, 30, meansOfDeath);
            }
        }
        G_CheckVictoryScript(attacker);
        // player killing a jedi with a lightsaber spawns a matrix-effect entity
        if (d_slowmodeath->integer) {
            if (!self->s.number) { // what the hell, always do slow-mo when player dies
                // FIXME: don't do this when crushed to death?
                if (meansOfDeath == meansOfDeath_t::MOD_FALLING && self->client->ps.groundEntityNum == ENTITYNUM_NONE) { // falling to death, have not hit yet
                    G_StartMatrixEffect(self, true, 10000);
                }
                else if (meansOfDeath != meansOfDeath_t::MOD_CRUSH) { // for all deaths except being crushed
                    G_StartMatrixEffect(self);
                }
            }
            else if (d_slowmodeath->integer < 4) { // any jedi killed by player-saber
                if (d_slowmodeath->integer < 3) {  // must be the last jedi in the room
                    if (!G_JediInRoom(attacker->currentOrigin)) {
                        lastInGroup = true;
                    }
                    else {
                        lastInGroup = false;
                    }
                }
                if (!attacker->s.number && holdingSaber && meansOfDeath == meansOfDeath_t::MOD_SABER && attacker->client &&
                    attacker->client->ps.weapon == weapon_t::WP_SABER && !attacker->client->ps.saberInFlight &&
                    (d_slowmodeath->integer > 2 || lastInGroup)) // either slow mo death level 3 (any jedi) or 2 and I was the last jedi in the room
                {                                                // Matrix!
                    G_StartMatrixEffect(self);
                }
            }
            else { // all player-saber kills
                if (!attacker->s.number && meansOfDeath == meansOfDeath_t::MOD_SABER && attacker->client && attacker->client->ps.weapon == weapon_t::WP_SABER &&
                    !attacker->client->ps.saberInFlight &&
                    (d_slowmodeath->integer > 4 || lastInGroup ||
                     holdingSaber)) // either slow mo death level 5 (any enemy) or 4 and I was the last in my group or I'm a saber user
                {                   // Matrix!
                    G_StartMatrixEffect(self);
                }
            }
        }
    }

    self->enemy = attacker;
    self->client->renderInfo.lookTarget = ENTITYNUM_NONE;

    self->client->ps.persistant[persEnum_t::PERS_KILLED]++;
    if (self->client->playerTeam == team_t::TEAM_PLAYER) { // FIXME: just HazTeam members in formation on away missions?
        // or more controlled- via deathscripts?
        //  Don't count player
        if ((g_entities[0].inuse && g_entities[0].client) && (self->s.number != 0)) { // add to the number of teammates lost
            g_entities[0].client->ps.persistant[persEnum_t::PERS_TEAMMATES_KILLED]++;
        }
        else // Player died, fire off scoreboard soon
        {
            cg.missionStatusDeadTime = level.time + 1000; // Too long?? Too short??
            cg.zoomMode = 0;                              // turn off zooming when we die
        }
    }

    if (self->s.number == 0 && attacker) {
        //		G_SetMissionStatusText( attacker, meansOfDeath );
        // TEST: If player killed, unmark all teammates from being undying so they can buy it too
        // NOTE: we want this to happen ONLY on our squad ONLY on missions... in the tutorial or on voyager levels this could be really weird.
        G_MakeTeamVulnerable();
    }

    if (attacker && attacker->client) {
        if (attacker == self || OnSameTeam(self, attacker)) {
            AddScore(attacker, -1);
        }
        else {
            AddScore(attacker, 1);
        }
    }
    else {
        AddScore(self, -1);
    }

    // if client is in a nodrop area, don't drop anything
    contents = gi.pointcontents(self->currentOrigin, -1);
    if (!holdingSaber
        //&& self->s.number != 0
        && !Any(contents & contentsFlags_t::CONTENTS_NODROP) && meansOfDeath != meansOfDeath_t::MOD_SNIPER &&
        (!self->client || self->client->NPC_class != class_t::CLASS_GALAKMECH)) {
        TossClientItems(self);
    }

    if (meansOfDeath == meansOfDeath_t::MOD_SNIPER) { // I was disintegrated
        if (self->message) {                          // I was holding a key
            // drop the key
            G_DropKey(self);
        }
    }

    if (holdingSaber) { // never drop a lightsaber!
        if (self->client->ps.saberActive) {
            self->client->ps.saberActive = false;
            if (self->client->playerTeam == team_t::TEAM_PLAYER) {
                G_SoundOnEnt(self, sound::SoundClass::Saber, sound::Slot::None, "sound/weapons/saber/saberoff.wav");
            }
            else {
                G_SoundOnEnt(self, sound::SoundClass::Saber, sound::Slot::None, "sound/weapons/saber/enemy_saber_off.wav");
            }
        }
    }
    else if (self->s.weapon != weapon_t::WP_BLASTER_PISTOL) { // Sigh...borg shouldn't drop their weapon attachments when they die..
        self->s.weapon = weapon_t::WP_NONE;
        if (self->weaponModel >= 0 && self->ghoul2.size()) {
            gi.G2API_RemoveGhoul2Model(self->ghoul2, self->weaponModel);
            self->weaponModel = -1;
        }
    }

    self->s.powerups &= ~PW_REMOVE_AT_DEATH; // removes everything but electricity and force push

    // FIXME: do this on a callback?  So people can't walk through long death anims?
    // Maybe set on last frame?  Would be cool for big blocking corpses if the never got set?
    // self->contents = contentsFlags_t::CONTENTS_CORPSE;//now done a second after death
    /*
    self->takedamage = false;	// no gibbing
    if ( self->client->playerTeam == TEAM_PARASITE )
    {
        self->contents = contentsFlags_t::CONTENTS_NONE; // FIXME: temp fix
    }
    else
    {
        self->contents = contentsFlags_t::CONTENTS_CORPSE;
        self->maxs[2] = -8;
    }
    */
    if (!self->s.number) { // player
        self->contents = contentsFlags_t::CONTENTS_CORPSE;
        self->maxs[2] = -8;
    }
    self->clipmask &= ~(contentsFlags_t::CONTENTS_MONSTERCLIP | contentsFlags_t::CONTENTS_BOTCLIP); // so dead NPC can fly off ledges

    // FACING==========================================================
    if (attacker && self->s.number == 0) {
        self->client->ps.stats[statIndex_t::STAT_DEAD_YAW] = AngleNormalize180(self->client->ps.viewangles[YAW]);
    }
    self->currentAngles[PITCH] = 0;
    self->currentAngles[ROLL] = 0;
    if (self->NPC) {
        self->NPC->desiredYaw = 0;
        self->NPC->desiredPitch = 0;
        self->NPC->confusionTime = 0;
        self->NPC->charmedTime = 0;
    }
    VectorCopy(self->currentAngles, self->client->ps.viewangles);
    // FACING==========================================================
    if (player && player->client &&
        player->client->ps.viewEntity == self->s.number) { // I was the player's viewentity and I died, kick him back to his normal view
        G_ClearViewEntity(player);
    }
    else if (!self->s.number && self->client->ps.viewEntity > 0 && self->client->ps.viewEntity < ENTITYNUM_NONE) {
        G_ClearViewEntity(self);
    }
    else if (!self->s.number && self->client->ps.viewEntity > 0 && self->client->ps.viewEntity < ENTITYNUM_NONE) {
        G_ClearViewEntity(self);
    }

    self->s.loopSound = 0;

    // remove powerups
    self->client->ps.powerups = {};

    if (self->client->NPC_class == class_t::CLASS_MARK1) {
        Mark1_die(self, inflictor, attacker, damage, meansOfDeath, dflags, hitLoc);
    }
    else if (self->client->NPC_class == class_t::CLASS_INTERROGATOR) {
        Interrogator_die(self, inflictor, attacker, damage, meansOfDeath, dflags, hitLoc);
    }
    else if (self->client->NPC_class == class_t::CLASS_GALAKMECH) { // FIXME: need keyframed explosions?
        NPC_SetAnim(self, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_DEATH1, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
        G_AddEvent(self, Q_irand(entity_event_t::EV_DEATH1, entity_event_t::EV_DEATH3), self->health);
    }
    else if (self->client->NPC_class == class_t::CLASS_ATST) { // FIXME: need keyframed explosions
        if (!self->s.number) {
            G_DrivableATSTDie(self);
        }
        anim = PM_PickAnim(self, animNumber_t::BOTH_DEATH1, animNumber_t::BOTH_DEATH25); // initialize to good data
        if (anim) {
            NPC_SetAnim(self, setAnimParts_t::SETANIM_BOTH, *anim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
        }
    }
    else if (self->s.number && self->message && meansOfDeath != meansOfDeath_t::MOD_SNIPER) { // imp with a key on his arm
        // pick a death anim that leaves key visible
        switch (Q_irand(0, 3)) {
        case 0:
            anim = animNumber_t::BOTH_DEATH4;
            break;
        case 1:
            anim = animNumber_t::BOTH_DEATH21;
            break;
        case 2:
            anim = animNumber_t::BOTH_DEATH17;
            break;
        case 3:
        default:
            anim = animNumber_t::BOTH_DEATH18;
            break;
        }
        // FIXME: verify we have this anim?
        NPC_SetAnim(self, setAnimParts_t::SETANIM_BOTH, *anim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
        if (meansOfDeath == meansOfDeath_t::MOD_KNOCKOUT || meansOfDeath == meansOfDeath_t::MOD_MELEE) {
            G_AddEvent(self, entity_event_t::EV_JUMP, 0);
        }
        else {
            G_AddEvent(self, Q_irand(entity_event_t::EV_DEATH1, entity_event_t::EV_DEATH3), self->health);
        }
    }
    else if (meansOfDeath == meansOfDeath_t::MOD_FALLING ||
             (self->client->ps.legsAnim == animNumber_t::BOTH_FALLDEATH1INAIR && self->client->ps.torsoAnim == animNumber_t::BOTH_FALLDEATH1INAIR) ||
             (self->client->ps.legsAnim == animNumber_t::BOTH_FALLDEATH1 && self->client->ps.torsoAnim == animNumber_t::BOTH_FALLDEATH1)) {
        // FIXME: no good way to predict you're going to fall to your death... need falling bushes/triggers?
        if (self->client->ps.groundEntityNum == ENTITYNUM_NONE                   // in the air
            && self->client->ps.velocity[2] < 0                                  // falling
            && self->client->ps.legsAnim != animNumber_t::BOTH_FALLDEATH1INAIR   // not already in falling loop
            && self->client->ps.torsoAnim != animNumber_t::BOTH_FALLDEATH1INAIR) // not already in falling loop
        {
            NPC_SetAnim(self, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_FALLDEATH1INAIR,
                        setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
            if (!self->NPC) {
                G_SoundOnEnt(self, sound::SoundClass::Voice, sound::Slot::Voice, "*falling1.wav"); // CHAN_VOICE_ATTEN
            }
            else if (!Any(self->NPC->aiFlags & npcAiFlags_t::NPCAI_DIE_ON_IMPACT)) {
                G_SoundOnEnt(self, sound::SoundClass::Voice, sound::Slot::Voice, "*falling1.wav"); // CHAN_VOICE_ATTEN
                // so we don't do this again
                self->NPC->aiFlags |= npcAiFlags_t::NPCAI_DIE_ON_IMPACT;
                // self->client->ps.gravity *= 0.5;//Fall a bit slower
                self->client->ps.friction = 1;
            }
        }
        else {
            animNumber_t deathAnim = animNumber_t::BOTH_FALLDEATH1LAND;
            if (PM_InOnGroundAnim(&self->client->ps)) {
                if (AngleNormalize180(self->client->renderInfo.torsoAngles[PITCH]) < 0) {
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_UP; // # Death anim when lying on back
                }
                else {
                    deathAnim = animNumber_t::BOTH_DEATH_LYING_DN; // # Death anim when lying on front
                }
            }
            else if (PM_InKnockDown(&self->client->ps)) {
                if (AngleNormalize180(self->client->renderInfo.torsoAngles[PITCH]) < 0) {
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_UP; // # Death anim when falling on back
                }
                else {
                    deathAnim = animNumber_t::BOTH_DEATH_FALLING_DN; // # Death anim when falling on face
                }
            }
            NPC_SetAnim(self, setAnimParts_t::SETANIM_BOTH, deathAnim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
            // HMM: check for nodrop?
            G_SoundOnEnt(self, sound::SoundClass::Body, sound::Slot::None, "sound/player/fallsplat.wav");
            if (gi.sound->GetVoiceState(self->s.number) != sound::VoiceState::NotTalking && self->NPC &&
                Any(self->NPC->aiFlags & npcAiFlags_t::NPCAI_DIE_ON_IMPACT)) { // I was talking, so cut it off... with a jump sound?
                G_SoundOnEnt(self, sound::SoundClass::VoiceQuiet, sound::Slot::Voice, "*pain100.wav");
            }
        }
    }
    else { // normal death
        anim = G_CheckSpecialDeathAnim(self, self->pos1, damage, meansOfDeath, hitLoc);
        if (!anim) {
            if (self->s.legsAnim == animNumber_t::BOTH_RESTRAINED1 ||
                self->s.legsAnim == animNumber_t::BOTH_RESTRAINED1POINT) { // super special case, floating in air lying down
                anim = animNumber_t::BOTH_RESTRAINED1;
            }
            else if (PM_InOnGroundAnim(&self->client->ps) && PM_HasAnimation(self, animNumber_t::BOTH_LYINGDEATH1)) { // on ground, need different death anim
                anim = animNumber_t::BOTH_LYINGDEATH1;
            }
            else if (meansOfDeath == meansOfDeath_t::MOD_TRIGGER_HURT && (self->s.powerups & Bit(powerup_t::PW_SHOCKED))) { // electrocuted
                anim = animNumber_t::BOTH_DEATH17;
            }
            else if (meansOfDeath == meansOfDeath_t::MOD_WATER) { // drowned
                anim = animNumber_t::BOTH_DEATH17;
            }
            else if (meansOfDeath != meansOfDeath_t::MOD_SNIPER) {
                cliff_fall = G_CheckLedgeDive(self, 128, self->client->ps.velocity, true, false);
                if (cliff_fall == 2) {
                    if (!FlyingCreature(self) && g_gravity->value > 0) {
                        if (!self->NPC) {
                            G_SoundOnEnt(self, sound::SoundClass::Voice, sound::Slot::Voice, "*falling1.wav"); // CHAN_VOICE_ATTEN
                        }
                        else if (!Any(self->NPC->aiFlags & npcAiFlags_t::NPCAI_DIE_ON_IMPACT)) {
                            G_SoundOnEnt(self, sound::SoundClass::Voice, sound::Slot::Voice, "*falling1.wav"); // CHAN_VOICE_ATTEN
                            self->NPC->aiFlags |= npcAiFlags_t::NPCAI_DIE_ON_IMPACT;
                            self->client->ps.friction = 0;
                        }
                    }
                }
                if (self->client->ps.pm_time > 0 && Any(self->client->ps.pm_flags & pmoveFlags_t::PMF_TIME_KNOCKBACK) && self->client->ps.velocity[2] > 0) {
                    float thrown, dot;
                    vec3_t throwdir, forward;

                    AngleVectors(self->currentAngles, forward, nullptr, nullptr);
                    thrown = VectorNormalize2(self->client->ps.velocity, throwdir);
                    dot = DotProduct(forward, throwdir);
                    if (thrown > 100) {
                        if (dot > 0.3) { // falling forward
                            if (cliff_fall == 2 && PM_HasAnimation(self, animNumber_t::BOTH_FALLDEATH1)) {
                                anim = animNumber_t::BOTH_FALLDEATH1;
                            }
                            else {
                                switch (Q_irand(0, 7)) {
                                case 0:
                                case 1:
                                case 2:
                                    anim = animNumber_t::BOTH_DEATH4;
                                    break;
                                case 3:
                                case 4:
                                case 5:
                                    anim = animNumber_t::BOTH_DEATH5;
                                    break;
                                case 6:
                                    anim = animNumber_t::BOTH_DEATH8;
                                    break;
                                case 7:
                                    anim = animNumber_t::BOTH_DEATH14;
                                    break;
                                }
                                if (PM_HasAnimation(self, *anim)) {
                                    self->client->ps.gravity *= 0.8;
                                    self->client->ps.friction = 0;
                                    if (self->client->ps.velocity[2] > 0 && self->client->ps.velocity[2] < 100) {
                                        self->client->ps.velocity[2] = 100;
                                    }
                                }
                                else {
                                    anim = std::nullopt;
                                }
                            }
                        }
                        else if (dot < -0.3) {
                            if (thrown >= 250 && !Q_irand(0, 3)) {
                                if (Q_irand(0, 1)) {
                                    anim = animNumber_t::BOTH_DEATHBACKWARD1;
                                }
                                else {
                                    anim = animNumber_t::BOTH_DEATHBACKWARD2;
                                }
                            }
                            else {
                                switch (Q_irand(0, 7)) {
                                case 0:
                                case 1:
                                    anim = animNumber_t::BOTH_DEATH1;
                                    break;
                                case 2:
                                case 3:
                                    anim = animNumber_t::BOTH_DEATH2;
                                    break;
                                case 4:
                                case 5:
                                    anim = animNumber_t::BOTH_DEATH22;
                                    break;
                                case 6:
                                case 7:
                                    anim = animNumber_t::BOTH_DEATH23;
                                    break;
                                }
                            }
                            if (PM_HasAnimation(self, *anim)) {
                                self->client->ps.gravity *= 0.8;
                                self->client->ps.friction = 0;
                                if (self->client->ps.velocity[2] > 0 && self->client->ps.velocity[2] < 100) {
                                    self->client->ps.velocity[2] = 100;
                                }
                            }
                            else {
                                anim = std::nullopt;
                            }
                        }
                        else { // falling to one of the sides
                            if (cliff_fall == 2 && PM_HasAnimation(self, animNumber_t::BOTH_FALLDEATH1)) {
                                anim = animNumber_t::BOTH_FALLDEATH1;
                                if (self->client->ps.velocity[2] > 0 && self->client->ps.velocity[2] < 100) {
                                    self->client->ps.velocity[2] = 100;
                                }
                            }
                        }
                    }
                }
            }
        }
        else {
            specialAnim = true;
        }

        bool keepDeathAnim = false;
        if (!anim) {
            if (meansOfDeath == meansOfDeath_t::MOD_ELECTROCUTE ||
                (meansOfDeath == meansOfDeath_t::MOD_CRUSH && Any(self->s.eFlags & entityFlags_t::EF_FORCE_GRIPPED))) { // electrocuted or choked to death
                anim = animNumber_t::BOTH_DEATH17;
            }
            else {
                anim = G_PickDeathAnim(self, self->pos1, damage, meansOfDeath, hitLoc);
                keepDeathAnim = !anim; // already in a death anim
            }
        }
        if (!anim && !keepDeathAnim) {
            anim = PM_PickAnim(self, animNumber_t::BOTH_DEATH1, animNumber_t::BOTH_DEATH25); // initialize to good data
            // TEMP HACK: these spinny deaths should happen less often
            if ((anim == animNumber_t::BOTH_DEATH8 || anim == animNumber_t::BOTH_DEATH14) && Q_irand(0, 1)) {
                anim = PM_PickAnim(self, animNumber_t::BOTH_DEATH1, animNumber_t::BOTH_DEATH25); // initialize to good data
            }
        }

        if (meansOfDeath == meansOfDeath_t::MOD_KNOCKOUT) {
            // FIXME: knock-out sound, and don't remove me
            G_AddEvent(self, entity_event_t::EV_JUMP, 0);
            G_UseTargets2(self, self, self->target2);
            G_AlertTeam(self, attacker, 512, 32);
            if (self->NPC) { // stick around for a while
                self->NPC->timeOfDeath = level.time + 10000;
            }
        }

        else if (meansOfDeath == meansOfDeath_t::MOD_SNIPER) {
            gentity_t* tent;
            vec3_t spot;

            VectorCopy(self->currentOrigin, spot);

            self->flags |= gentityFlags_t::FL_DISINTEGRATED;
            self->svFlags |= svFlags_t::SVF_BROADCAST;
            tent = G_TempEntity(spot, entity_event_t::EV_DISINTEGRATION);
            tent->s.eventParm = std::to_underlying(powerup_t::PW_DISRUPTION);
            tent->svFlags |= svFlags_t::SVF_BROADCAST;
            tent->owner = self;

            G_AlertTeam(self, attacker, 512, 88);

            if (self->playerModel >= 0) {
                // don't let 'em animate
                gi.G2API_PauseBoneAnimIndex(&self->ghoul2[self->playerModel], self->rootBone, cg.time);
                gi.G2API_PauseBoneAnimIndex(&self->ghoul2[self->playerModel], self->motionBone, cg.time);
                gi.G2API_PauseBoneAnimIndex(&self->ghoul2[self->playerModel], self->lowerLumbarBone, cg.time);
                anim = std::nullopt;
            }

            // not solid anymore
            self->contents = contentsFlags_t::CONTENTS_NONE;
            self->maxs[2] = -8;

            if (self->NPC) {
                // need to pad deathtime some to stick around long enough for death effect to play
                self->NPC->timeOfDeath = level.time + 2000;
            }
        }
        else {
            if (hitLoc == hitLocation_t::HL_HEAD && !Any(dflags & damageFlags_t::DAMAGE_RADIUS) && meansOfDeath != meansOfDeath_t::MOD_REPEATER_ALT &&
                meansOfDeath != meansOfDeath_t::MOD_FLECHETTE_ALT && meansOfDeath != meansOfDeath_t::MOD_ROCKET &&
                meansOfDeath != meansOfDeath_t::MOD_ROCKET_ALT && meansOfDeath != meansOfDeath_t::MOD_THERMAL &&
                meansOfDeath != meansOfDeath_t::MOD_THERMAL_ALT && meansOfDeath != meansOfDeath_t::MOD_DETPACK &&
                meansOfDeath != meansOfDeath_t::MOD_LASERTRIP && meansOfDeath != meansOfDeath_t::MOD_LASERTRIP_ALT &&
                meansOfDeath != meansOfDeath_t::MOD_EXPLOSIVE &&
                meansOfDeath != meansOfDeath_t::MOD_EXPLOSIVE_SPLASH) { // no sound when killed by headshot (explosions don't count)
                G_AlertTeam(self, attacker, 512, 0);
                if (gi.sound->GetVoiceState(self->s.number) != sound::VoiceState::NotTalking) { // I was talking, so cut it off... with a jump sound?
                    G_SoundOnEnt(self, sound::SoundClass::Voice, sound::Slot::Voice, "*jump1.wav");
                }
            }
            else {
                if (Any(self->client->ps.eFlags & entityFlags_t::EF_FORCE_GRIPPED)) { // killed while gripped - no loud scream
                    G_AlertTeam(self, attacker, 512, 32);
                }
                else if (cliff_fall != 2) {
                    if (meansOfDeath == meansOfDeath_t::MOD_KNOCKOUT || meansOfDeath == meansOfDeath_t::MOD_MELEE) {
                        G_AddEvent(self, entity_event_t::EV_JUMP, 0);
                    }
                    else {
                        G_AddEvent(self, Q_irand(entity_event_t::EV_DEATH1, entity_event_t::EV_DEATH3), self->health);
                    }
                    G_DeathAlert(self, attacker);
                }
                else { // screaming death is louder
                    G_AlertTeam(self, attacker, 512, 1024);
                }
            }
        }

        if (attacker && attacker->s.number == 0) { // killed by player
            // FIXME: this should really be wherever my body comes to rest...
            AddSightEvent(attacker, self->currentOrigin, 384, alertEventLevel_e::AEL_DISCOVERED, 10);
            // FIXME: danger event so that others will run away from this area since it's obviously dangerous
        }

        if (anim) // empty if it fails or if it's already in a death anim
        {
            NPC_SetAnim(self, setAnimParts_t::SETANIM_BOTH, *anim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
        }
    }

    // do any dismemberment if there's any to do...
    if (Any(dflags & damageFlags_t::DAMAGE_DISMEMBER) && G_DoDismemberment(self, self->pos1, meansOfDeath, damage, hitLoc) &&
        !specialAnim) { // we did dismemberment and our death anim is okay to override
        if (hitLoc == hitLocation_t::HL_HAND_RT && self->locationDamage[hitLoc] >= Q3_INFINITE && cliff_fall != 2 &&
            self->client->ps.groundEntityNum != ENTITYNUM_NONE) { // just lost our right hand and we're on the ground, use the special anim
            NPC_SetAnim(self, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_RIGHTHANDCHOPPEDOFF,
                        setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
        }
    }

    // don't allow player to respawn for a few seconds
    self->client->respawnTime = level.time + 2000; // self->client->ps.legsAnimTimer;

    // lock it to this anim forever
    if (self->client) {
        PM_SetLegsAnimTimer(self, &self->client->ps.legsAnimTimer, -1);
        PM_SetTorsoAnimTimer(self, &self->client->ps.torsoAnimTimer, -1);
    }

    // Flying creatures should drop when killed
    // FIXME: This may screw up certain things that expect to float even while dead <?>
    self->svFlags &= ~svFlags_t::SVF_CUSTOM_GRAVITY;

    self->client->ps.pm_type = pmtype_t::PM_DEAD;
    // need to update STAT_HEALTH here because ClientThink_real for self may happen before STAT_HEALTH is updated from self->health and pmove will stomp death
    // anim with a move anim
    self->client->ps.stats[statIndex_t::STAT_HEALTH] = self->health;

    if (self->NPC) { // If an NPC, make sure we start running our scripts again- this gets set to infinite while we fall to our deaths
        self->NPC->nextBStateThink = level.time;
    }

    if (G_ActivateBehavior(self, bSet_t::BSET_DEATH)) {
        deathScript = true;
    }

    if (self->NPC && Any(self->NPC->scriptFlags & scriptFlags_t::SCF_FFDEATH)) {
        if (G_ActivateBehavior(self, bSet_t::BSET_FFDEATH)) { // FIXME: should running this preclude running the normal deathscript?
            deathScript = true;
        }
        G_UseTargets2(self, self, self->target4);
    }

    if (!deathScript && !Any(self->svFlags & svFlags_t::SVF_KILLED_SELF)) {
        // Should no longer run scripts
        // WARNING!!! DO NOT DO THIS WHILE RUNNING A SCRIPT, ICARUS WILL CRASH!!!
        // FIXME: shouldn't ICARUS handle this internally?
        ICARUS_FreeEnt(self);
    }

    // Free up any timers we may have on us.
    TIMER_Clear(self);

    // Set pending objectives to failed
    OBJ_SetPendingObjectives(self);

    gi.linkentity(self);

    self->bounceCount = -1; // This is a cheap hack for optimizing the pointcontents check in deadthink
    if (self->NPC) {
        self->NPC->timeOfDeath = level.time; // this will change - used for debouncing post-death events
        self->s.time = level.time;           // this will not chage- this is actual time of death
    }

    // Start any necessary death fx for this entity
    DeathFX(self);
}

bool G_CheckForStrongAttackMomentum(gentity_t* self)
{                                                                                           // see if our saber attack has too much momentum to be interrupted
    if (PM_PowerLevelForSaberAnim(&self->client->ps) > forcePowerLevels_t::FORCE_LEVEL_2) { // strong attacks can't be interrupted
        if (PM_InAnimForSaberMove(self->client->ps.torsoAnim,
                                  self->client->ps.saberMove)) { // our saberMove was not already interupted by some other anim (like pain)
            if (PM_SaberInStart(self->client->ps.saberMove)) {
                float animLength = PM_AnimLength(self->client->clientInfo.animFileIndex, self->client->ps.torsoAnim);
                if (animLength - self->client->ps.torsoAnimTimer > 750) { // start anim is already 3/4 of a second into it, can't interrupt it now
                    return true;
                }
            }
            else if (PM_SaberInReturn(self->client->ps.saberMove)) {
                if (self->client->ps.torsoAnimTimer > 750) { // still have a good amount of time left in the return anim, can't interrupt it
                    return true;
                }
            }
            else { // cannot interrupt actual transitions and attacks
                return true;
            }
        }
    }
    return false;
}

void PlayerPain(gentity_t* self, gentity_t* inflictor, gentity_t* other, vec3_t point, int damage, meansOfDeath_t mod, hitLocation_t hitLoc)
{
    if (self->client->NPC_class == class_t::CLASS_ATST) { // different kind of pain checking altogether
        G_ATSTCheckPain(self, other, point, damage, mod, hitLoc);
        const std::optional<g2SurfaceFlags_t> blasterTest = gi.G2API_GetSurfaceRenderStatus(&self->ghoul2[self->playerModel], "head_light_blaster_cann");
        const std::optional<g2SurfaceFlags_t> chargerTest = gi.G2API_GetSurfaceRenderStatus(&self->ghoul2[self->playerModel], "head_concussion_charger");
        if (blasterTest != g2SurfaceFlags_t::G2SURFACEFLAG_NONE && chargerTest != g2SurfaceFlags_t::G2SURFACEFLAG_NONE) { // lost both side guns
            // take away that weapon
            self->client->ps.stats[statIndex_t::STAT_WEAPONS] &= ~Bit(weapon_t::WP_ATST_SIDE);
            // switch to primary guns
            if (self->client->ps.weapon == weapon_t::WP_ATST_SIDE) {
                CG_ChangeWeapon(weapon_t::WP_ATST_MAIN);
            }
        }
    }
    else {
        // play an apropriate pain sound
        if (level.time > self->painDebounceTime && !Any(self->flags & gentityFlags_t::FL_GODMODE)) { // first time hit this frame and not in godmode
            self->client->ps.damageEvent++;
            if (!Q3_TaskIDPending(self, taskID_t::TID_CHAN_VOICE)) {
                if (self->client->damage_blood) { // took damage myself, not just armor
                    G_AddEvent(self, entity_event_t::EV_PAIN, self->health);
                }
            }
        }
        if (damage != -1 && (mod == meansOfDeath_t::MOD_MELEE || damage == 0 /*fake damage*/ ||
                             (Q_irand(0, 10) <= damage && self->client->damage_blood))) { //-1 == don't play pain anim
            if ((((mod == meansOfDeath_t::MOD_SABER || mod == meansOfDeath_t::MOD_MELEE) && self->client->damage_blood) || mod == meansOfDeath_t::MOD_CRUSH) &&
                (self->s.weapon == weapon_t::WP_SABER || self->s.weapon == weapon_t::WP_MELEE)) // FIXME: not only if using saber, but if in third person at
                                                                                                // all?  But then 1st/third person functionality is different...
            { // FIXME: only strong-level saber attacks should make me play pain anim?
                if (!G_CheckForStrongAttackMomentum(self) && !PM_SpinningSaberAnim(self->client->ps.legsAnim) &&
                    !PM_SaberInSpecialAttack(self->client->ps.torsoAnim) &&
                    !PM_InKnockDown(&self->client->ps)) { // strong attacks and spins cannot be interrupted by pain, no pain when in knockdown
                    setAnimParts_t parts = setAnimParts_t::SETANIM_BOTH;
                    if (self->client->ps.groundEntityNum != ENTITYNUM_NONE && !PM_SpinningSaberAnim(self->client->ps.legsAnim) &&
                        !PM_FlippingAnim(self->client->ps.legsAnim) && !PM_InSpecialJump(self->client->ps.legsAnim) &&
                        !PM_RollingAnim(self->client->ps.legsAnim) && !PM_CrouchAnim(self->client->ps.legsAnim) &&
                        !PM_RunningAnim(self->client->ps.legsAnim)) { // if on a surface and not in a spin or flip, play full body pain
                        parts = setAnimParts_t::SETANIM_BOTH;
                    }
                    else { // play pain just in torso
                        parts = setAnimParts_t::SETANIM_TORSO;
                    }
                    if (self->painDebounceTime < level.time) {
                        // temp HACK: these are the only 2 pain anims that look good when holding a saber
                        NPC_SetAnim(self, parts, PM_PickAnim(self, animNumber_t::BOTH_PAIN2, animNumber_t::BOTH_PAIN3),
                                    setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                        self->client->ps.saberMove = saberMoveName_t::LS_READY; // don't finish whatever saber move you may have been in
                        // WTF - insn't working
                        if (self->health < 10 && d_slowmodeath->integer > 5) {
                            G_StartMatrixEffect(self);
                        }
                    }
                    if (parts == setAnimParts_t::SETANIM_BOTH &&
                        (damage > 30 ||
                         (self->painDebounceTime > level.time && damage > 10))) { // took a lot of damage in 1 hit //or took 2 hits in quick succession
                        self->aimDebounceTime = level.time + self->client->ps.torsoAnimTimer;
                        self->client->ps.pm_time = self->client->ps.torsoAnimTimer;
                        self->client->ps.pm_flags |= pmoveFlags_t::PMF_TIME_KNOCKBACK;
                    }
                    self->client->ps.weaponTime = self->client->ps.torsoAnimTimer;
                    self->attackDebounceTime = level.time + self->client->ps.torsoAnimTimer;
                }
                self->painDebounceTime = level.time + self->client->ps.torsoAnimTimer;
            }
        }
    }
    if (self->painDebounceTime <= level.time) {
        self->painDebounceTime = level.time + 700;
    }
}
/*
================
CheckArmor
================
*/
int CheckArmor(gentity_t* ent, int damage, damageFlags_t dflags)
{
    gclient_t* client;
    int save;
    int count;

    if (!damage)
        return 0;

    client = ent->client;

    if (!client)
        return 0;

    if (Any(dflags & damageFlags_t::DAMAGE_NO_ARMOR))
        return 0;

    if (client->NPC_class == class_t::CLASS_GALAKMECH) {      // special case
        if (client->ps.stats[statIndex_t::STAT_ARMOR] <= 0) { // no shields
            client->ps.powerups[powerup_t::PW_GALAK_SHIELD] = 0;
            return 0;
        }
        else { // shields take all the damage
            client->ps.stats[statIndex_t::STAT_ARMOR] -= damage;
            if (client->ps.stats[statIndex_t::STAT_ARMOR] <= 0) {
                client->ps.powerups[powerup_t::PW_GALAK_SHIELD] = 0;
                client->ps.stats[statIndex_t::STAT_ARMOR] = 0;
            }
            return damage;
        }
    }
    else {
        // armor
        count = client->ps.stats[statIndex_t::STAT_ARMOR];

        // No damage to entity until armor is at less than 50% strength
        if (count > (client->ps.stats[statIndex_t::STAT_MAX_HEALTH] / 2)) // MAX_HEALTH is considered max armor. Or so I'm told.
        {
            save = damage;
        }
        else {
            if (!ent->s.number && client->NPC_class == class_t::CLASS_ATST) { // player in ATST... armor takes *all* the damage
                save = damage;
            }
            else {
                save = Q_ceil((float)damage * ARMOR_PROTECTION);
            }
        }

        // Always round up
        if (damage == 1) {
            if (client->ps.stats[statIndex_t::STAT_ARMOR] > 0)
                client->ps.stats[statIndex_t::STAT_ARMOR] -= save;

            return 0;
        }

        if (save >= count)
            save = count;

        if (!save)
            return 0;

        client->ps.stats[statIndex_t::STAT_ARMOR] -= save;

        return save;
    }
}

extern void NPC_SetPainEvent(gentity_t* self);
void G_Knockdown(gentity_t* self, gentity_t* attacker, vec3_t pushDir, float strength, bool breakSaberLock)
{
    if (!self || !self->client || !attacker || !attacker->client) {
        return;
    }

    // break out of a saberLock?
    if (breakSaberLock) {
        self->client->ps.saberLockTime = 0;
        self->client->ps.saberLockEnemy = ENTITYNUM_NONE;
    }

    if (self->health > 0) {
        if (!self->s.number) {
            NPC_SetPainEvent(self);
        }
        else {
            GEntity_PainFunc(self, attacker, attacker, self->currentOrigin, 0, meansOfDeath_t::MOD_MELEE);
        }
        G_CheckLedgeDive(self, 72, pushDir, false, false);

        if (!PM_SpinningSaberAnim(self->client->ps.legsAnim) && !PM_FlippingAnim(self->client->ps.legsAnim) && !PM_RollingAnim(self->client->ps.legsAnim) &&
            !PM_InKnockDown(&self->client->ps)) {
            animNumber_t knockAnim = animNumber_t::BOTH_KNOCKDOWN1;           // default knockdown
            if (!self->s.number && (!g_spskill->integer || strength < 300)) { // player only knocked down if pushed *hard*
                return;
            }
            else if (PM_CrouchAnim(self->client->ps.legsAnim)) { // crouched knockdown
                knockAnim = animNumber_t::BOTH_KNOCKDOWN4;
            }
            else { // plain old knockdown
                vec3_t pLFwd, pLAngles = {0, self->client->ps.viewangles[YAW], 0};
                AngleVectors(pLAngles, pLFwd, nullptr, nullptr);
                if (DotProduct(pLFwd, pushDir) > 0.2f) { // pushing him from behind
                    knockAnim = animNumber_t::BOTH_KNOCKDOWN3;
                }
                else { // pushing him from front
                    knockAnim = animNumber_t::BOTH_KNOCKDOWN1;
                }
            }
            if (knockAnim == animNumber_t::BOTH_KNOCKDOWN1 && strength > 150) { // push *hard*
                knockAnim = animNumber_t::BOTH_KNOCKDOWN2;
            }
            NPC_SetAnim(self, setAnimParts_t::SETANIM_BOTH, knockAnim, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
            if (self->s.number) { // randomize getup times
                int addTime = Q_irand(-300, 1000);
                self->client->ps.legsAnimTimer += addTime;
                self->client->ps.torsoAnimTimer += addTime;
            }
        }
    }
}

void G_CheckKnockdown(gentity_t* targ, gentity_t* attacker, vec3_t newDir, damageFlags_t dflags, meansOfDeath_t mod)
{
    if (!targ || !attacker) {
        return;
    }
    if (!Any(dflags & damageFlags_t::DAMAGE_RADIUS)) { // not inherently explosive damage, check mod
        if (mod != meansOfDeath_t::MOD_REPEATER_ALT && mod != meansOfDeath_t::MOD_FLECHETTE_ALT && mod != meansOfDeath_t::MOD_ROCKET &&
            mod != meansOfDeath_t::MOD_ROCKET_ALT && mod != meansOfDeath_t::MOD_THERMAL && mod != meansOfDeath_t::MOD_THERMAL_ALT &&
            mod != meansOfDeath_t::MOD_DETPACK && mod != meansOfDeath_t::MOD_LASERTRIP && mod != meansOfDeath_t::MOD_LASERTRIP_ALT &&
            mod != meansOfDeath_t::MOD_EXPLOSIVE && mod != meansOfDeath_t::MOD_EXPLOSIVE_SPLASH) {
            return;
        }
    }

    if (!targ->client || targ->client->NPC_class == class_t::CLASS_PROTOCOL || !G_StandardHumanoid(targ->NPC_type)) {
        return;
    }

    if (targ->client->ps.groundEntityNum == ENTITYNUM_NONE) { // already in air
        return;
    }

    if (!targ->s.number) {         // player less likely to be knocked down
        if (!g_spskill->integer) { // never in easy
            return;
        }
        if (!cg.renderingThirdPerson || cg.zoomMode) { // never if not in chase camera view (so more likely with saber out)
            return;
        }
        if (g_spskill->integer == 1) { // 33% chance on medium
            if (Q_irand(0, 2)) {
                return;
            }
        }
        else { // 50% chance on hard
            if (Q_irand(0, 1)) {
                return;
            }
        }
    }

    float strength = VectorLength(targ->client->ps.velocity);
    if (targ->client->ps.velocity[2] > 100 && strength > Q_irand(150, 350)) // 600 ) )
    {                                                                       // explosive concussion possibly do a knockdown?
        G_Knockdown(targ, attacker, newDir, strength, true);
    }
}

void G_ApplyKnockback(gentity_t* targ, vec3_t newDir, float knockback)
{
    vec3_t kvel;
    float mass;

    //--- TEMP TEST
    if (newDir[2] <= 0.0f) {

        newDir[2] += ((0.0f - newDir[2]) * 1.2f);
    }

    knockback *= 2.0f;

    if (knockback > 120) {
        knockback = 120;
    }
    //--- TEMP TEST

    if (targ->physicsBounce > 0) // overide the mass
        mass = targ->physicsBounce;
    else
        mass = 200;

    if (g_gravity->value > 0) {
        VectorScale(newDir, g_knockback->value * (float)knockback / mass * 0.8, kvel);
        kvel[2] = newDir[2] * (g_knockback->value * (float)knockback) / (mass * 1.5) + 20;
    }
    else {
        VectorScale(newDir, g_knockback->value * (float)knockback / mass, kvel);
    }

    if (targ->client) {
        VectorAdd(targ->client->ps.velocity, kvel, targ->client->ps.velocity);
    }
    else if (targ->s.pos.trType != trType_t::TR_STATIONARY && targ->s.pos.trType != trType_t::TR_LINEAR_STOP &&
             targ->s.pos.trType != trType_t::TR_NONLINEAR_STOP) {
        VectorAdd(targ->s.pos.trDelta, kvel, targ->s.pos.trDelta);
        VectorCopy(targ->currentOrigin, targ->s.pos.trBase);
        targ->s.pos.trTime = level.time;
    }

    // set the timer so that the other client can't cancel
    // out the movement immediately
    if (targ->client && !targ->client->ps.pm_time) {
        int t;

        t = knockback * 2;
        if (t < 50) {
            t = 50;
        }
        if (t > 200) {
            t = 200;
        }
        targ->client->ps.pm_time = t;
        targ->client->ps.pm_flags |= pmoveFlags_t::PMF_TIME_KNOCKBACK;
    }
}

static int G_CheckForLedge(gentity_t* self, vec3_t fallCheckDir, float checkDist)
{
    vec3_t start, end;
    trace_t tr;

    VectorMA(self->currentOrigin, checkDist, fallCheckDir, end);
    // Should have clip burshes masked out by now and have bbox resized to death size
    gi.trace(&tr, self->currentOrigin, self->mins, self->maxs, end, self->s.number, self->clipmask, EG2_Collision::G2_NOCOLLIDE, 0);
    if (tr.allsolid || tr.startsolid) {
        return 0;
    }
    VectorCopy(tr.endpos, start);
    VectorCopy(start, end);
    end[2] -= 256;

    gi.trace(&tr, start, self->mins, self->maxs, end, self->s.number, self->clipmask, EG2_Collision::G2_NOCOLLIDE, 0);
    if (tr.allsolid || tr.startsolid) {
        return 0;
    }
    if (tr.fraction >= 1.0) {
        return (start[2] - tr.endpos[2]);
    }
    return 0;
}

static void G_FriendlyFireReaction(gentity_t* self, gentity_t* other, damageFlags_t dflags)
{
    if ((!player->client->ps.viewEntity || other->s.number != player->client->ps.viewEntity)) { // hit by a teammate
        if (other != self->enemy && self != other->enemy) {                                     // we weren't already enemies
            if (self->enemy || other->enemy ||
                (other->s.number &&
                 other->s.number != player->client->ps.viewEntity)) { // if one of us actually has an enemy already, it's okay, just an accident OR wasn't hit
                                                                      // by player or someone controlled by player OR player hit ally and didn't get 25% chance
                                                                      // of getting mad (FIXME:accumulate anger+base on diff?)
                return;
            }
            else if (self->NPC && !other->s.number) // should be assumed, but...
            {                                       // dammit, stop that!
                if (!Any(dflags & damageFlags_t::DAMAGE_RADIUS)) {
                    // if it's radius damage, ignore it
                    if (self->NPC->ffireDebounce < level.time) {
                        // FIXME: way something?  NEED DIALOGUE
                        self->NPC->ffireCount++;
                        // Q_Println("incr: {} < {}", self->NPC->ffireCount, 3+((2-g_spskill->integer)*2));
                        self->NPC->ffireDebounce = level.time + 500;
                    }
                }
            }
        }
    }
}

EnumArray<float, hitLocation_t, hitLocation_t::HL_MAX> damageModifier = {{
    1.0f,  // HL_NONE,
    0.25f, // HL_FOOT_RT,
    0.25f, // HL_FOOT_LT,
    0.75f, // HL_LEG_RT,
    0.75f, // HL_LEG_LT,
    1.0f,  // HL_WAIST,
    1.0f,  // HL_BACK_RT,
    1.0f,  // HL_BACK_LT,
    1.0f,  // HL_BACK,
    1.0f,  // HL_CHEST_RT,
    1.0f,  // HL_CHEST_LT,
    1.0f,  // HL_CHEST,
    0.5f,  // HL_ARM_RT,
    0.5f,  // HL_ARM_LT,
    0.25f, // HL_HAND_RT,
    0.25f, // HL_HAND_LT,
    2.0f,  // HL_HEAD,
    1.0f,  // HL_GENERIC1,
    1.0f,  // HL_GENERIC2,
    1.0f,  // HL_GENERIC3,
    1.0f,  // HL_GENERIC4,
    1.0f,  // HL_GENERIC5,
    1.0f,  // HL_GENERIC6,
}};

void G_TrackWeaponUsage(gentity_t* self, gentity_t* inflictor, int add, meansOfDeath_t mod)
{
    if (!self || !self->client || self->s.number) { // player only
        return;
    }
    weapon_t weapon = weapon_t::WP_NONE;
    // FIXME: need to check the MOD to find out what weapon (if *any*) actually did the killing
    if (inflictor && !inflictor->client && mod != meansOfDeath_t::MOD_SABER && inflictor->lastEnemy &&
        inflictor->lastEnemy != self) {                                         // a missile that was reflected, ie: not owned by me originally
        if (inflictor->owner == self && self->s.weapon == weapon_t::WP_SABER) { // we reflected it
            weapon = weapon_t::WP_SABER;
        }
    }
    if (weapon == weapon_t::WP_NONE) {
        switch (mod) {
        case meansOfDeath_t::MOD_SABER:
            weapon = weapon_t::WP_SABER;
            break;
        case meansOfDeath_t::MOD_BRYAR:
        case meansOfDeath_t::MOD_BRYAR_ALT:
            weapon = weapon_t::WP_BRYAR_PISTOL;
            break;
        case meansOfDeath_t::MOD_BLASTER:
        case meansOfDeath_t::MOD_BLASTER_ALT:
            weapon = weapon_t::WP_BLASTER;
            break;
        case meansOfDeath_t::MOD_DISRUPTOR:
        case meansOfDeath_t::MOD_SNIPER:
            weapon = weapon_t::WP_DISRUPTOR;
            break;
        case meansOfDeath_t::MOD_BOWCASTER:
        case meansOfDeath_t::MOD_BOWCASTER_ALT:
            weapon = weapon_t::WP_BOWCASTER;
            break;
        case meansOfDeath_t::MOD_REPEATER:
        case meansOfDeath_t::MOD_REPEATER_ALT:
            weapon = weapon_t::WP_REPEATER;
            break;
        case meansOfDeath_t::MOD_DEMP2:
        case meansOfDeath_t::MOD_DEMP2_ALT:
            weapon = weapon_t::WP_DEMP2;
            break;
        case meansOfDeath_t::MOD_FLECHETTE:
        case meansOfDeath_t::MOD_FLECHETTE_ALT:
            weapon = weapon_t::WP_FLECHETTE;
            break;
        case meansOfDeath_t::MOD_ROCKET:
        case meansOfDeath_t::MOD_ROCKET_ALT:
            weapon = weapon_t::WP_ROCKET_LAUNCHER;
            break;
        case meansOfDeath_t::MOD_THERMAL:
        case meansOfDeath_t::MOD_THERMAL_ALT:
            weapon = weapon_t::WP_THERMAL;
            break;
        case meansOfDeath_t::MOD_DETPACK:
            weapon = weapon_t::WP_DET_PACK;
            break;
        case meansOfDeath_t::MOD_LASERTRIP:
        case meansOfDeath_t::MOD_LASERTRIP_ALT:
            weapon = weapon_t::WP_TRIP_MINE;
            break;
        case meansOfDeath_t::MOD_MELEE:
            if (self->s.weapon == weapon_t::WP_STUN_BATON) {
                weapon = weapon_t::WP_STUN_BATON;
            }
            else if (self->s.weapon == weapon_t::WP_MELEE) {
                weapon = weapon_t::WP_MELEE;
            }
            break;
        default:
            break;
        }
    }
    if (weapon != weapon_t::WP_NONE) {
        self->client->sess.missionStats.weaponUsed[weapon] += add;
    }
}

bool G_NonLocationSpecificDamage(meansOfDeath_t meansOfDeath)
{
    if (meansOfDeath == meansOfDeath_t::MOD_EXPLOSIVE || meansOfDeath == meansOfDeath_t::MOD_REPEATER_ALT ||
        meansOfDeath == meansOfDeath_t::MOD_FLECHETTE_ALT || meansOfDeath == meansOfDeath_t::MOD_ROCKET || meansOfDeath == meansOfDeath_t::MOD_ROCKET_ALT ||
        meansOfDeath == meansOfDeath_t::MOD_THERMAL || meansOfDeath == meansOfDeath_t::MOD_THERMAL_ALT || meansOfDeath == meansOfDeath_t::MOD_DETPACK ||
        meansOfDeath == meansOfDeath_t::MOD_LASERTRIP || meansOfDeath == meansOfDeath_t::MOD_LASERTRIP_ALT || meansOfDeath == meansOfDeath_t::MOD_MELEE ||
        meansOfDeath == meansOfDeath_t::MOD_FORCE_GRIP || meansOfDeath == meansOfDeath_t::MOD_KNOCKOUT || meansOfDeath == meansOfDeath_t::MOD_CRUSH ||
        meansOfDeath == meansOfDeath_t::MOD_EXPLOSIVE_SPLASH) {
        return true;
    }
    return false;
}
/*
============
T_Damage

targ		entity that is being damaged
inflictor	entity that is causing the damage
attacker	entity that caused the inflictor to damage targ
    example: targ=monster, inflictor=rocket, attacker=player

dir			direction of the attack for knockback
point		point at which the damage is being inflicted, used for headshots
damage		amount of damage being inflicted
knockback	force to be applied against targ as a result of the damage

inflictor, attacker, dir, and point can be nullptr for environmental effects

dflags		these flags are used to control how T_Damage works
    DAMAGE_RADIUS			damage was indirect (from a nearby explosion)
    DAMAGE_NO_ARMOR			armor does not protect from this damage
    DAMAGE_NO_KNOCKBACK		do not affect velocity, just view angles
    DAMAGE_NO_PROTECTION	kills godmode, armor, everything
    DAMAGE_NO_HIT_LOC		Damage not based on hit location
============
*/

void G_Damage(gentity_t* targ, gentity_t* inflictor, gentity_t* attacker, vec3_t dir, vec3_t point, int damage, damageFlags_t dflags, meansOfDeath_t mod,
              hitLocation_t hitLoc)
{
    gclient_t* client;
    int take;
    int asave = 0;
    int knockback;
    vec3_t newDir;
    bool alreadyDead = false;

    if (!targ->takedamage) {
        return;
    }

    if (targ->health <= 0 && !targ->client) { // allow corpses to be disintegrated
        if (mod != meansOfDeath_t::MOD_SNIPER || Any(targ->flags & gentityFlags_t::FL_DISINTEGRATED))
            return;
    }

    // if we are the player and we are locked to an emplaced gun, we have to reroute damage to the gun....sigh.
    if (Any(targ->s.eFlags & entityFlags_t::EF_LOCKED_TO_WEAPON) && targ->s.number == 0 && targ->owner &&
        !Any(targ->owner->flags & gentityFlags_t::FL_GODMODE)) {
        // swapping the gun into our place to absorb our damage
        targ = targ->owner;
    }

    if (Any(targ->flags & gentityFlags_t::FL_SHIELDED) && mod != meansOfDeath_t::MOD_SABER &&
        !targ->client) { // magnetically protected, this thing can only be damaged by lightsabers
        return;
    }

    if (Any(targ->flags & gentityFlags_t::FL_DMG_BY_SABER_ONLY) &&
        mod != meansOfDeath_t::MOD_SABER) { // can only be damaged by lightsabers (but no shield... yeah, it's redundant, but whattayagonnado?)
        return;
    }

    if (Any(targ->flags & gentityFlags_t::FL_DMG_BY_HEAVY_WEAP_ONLY) && !Any(dflags & damageFlags_t::DAMAGE_HEAVY_WEAP_CLASS)) {
        // can only be damaged by an heavy type weapon...but impacting missile was in the heavy weap class...so we just aren't taking damage from this missile
        return;
    }

    if (targ->client && targ->client->NPC_class == class_t::CLASS_ATST) {
        // extra checks can be done here
        if (mod == meansOfDeath_t::MOD_SNIPER || mod == meansOfDeath_t::MOD_DISRUPTOR) {
            // disruptor does not hurt an atst
            return;
        }
    }
    if (mod == meansOfDeath_t::MOD_SABER) { // sabers do less damage to mark1's and atst's
        if (targ->client && (targ->client->NPC_class == class_t::CLASS_ATST || targ->client->NPC_class == class_t::CLASS_MARK1)) {
            // I guess always do 5 points of damage...feel free to tweak as needed
            if (damage > 5) {
                damage = 5;
            }
        }
    }

    if (!inflictor) {
        inflictor = &g_entities[ENTITYNUM_WORLD];
    }
    if (!attacker) {
        attacker = &g_entities[ENTITYNUM_WORLD];
    }

    // no more weakling allies!
    //	if ( attacker->s.number != 0 && damage >= 2 && targ->s.number != 0 && attacker->client && attacker->client->playerTeam == team_t::TEAM_PLAYER )
    //	{//player-helpers do only half damage to enemies
    //		damage = Q_ceilf((float)damage/2.0f);
    //	}

    client = targ->client;

    if (client) {
        if (client->noclip && !targ->s.number) {
            return;
        }
    }

    if (Any(dflags & damageFlags_t::DAMAGE_NO_DAMAGE)) {
        damage = 0;
    }

    if (dir == nullptr) {
        dflags |= damageFlags_t::DAMAGE_NO_KNOCKBACK;
    }
    else {
        VectorNormalize2(dir, newDir);
    }

    if (targ->s.number != 0) { // not the player
        if (Any(targ->flags & gentityFlags_t::FL_GODMODE) ||
            Any(targ->flags & gentityFlags_t::FL_UNDYING)) { // have god or undying on, so ignore no protection flag
            dflags &= ~damageFlags_t::DAMAGE_NO_PROTECTION;
        }
    }

    if (client && PM_InOnGroundAnim(&client->ps)) {
        dflags |= damageFlags_t::DAMAGE_NO_KNOCKBACK;
    }
    if (!attacker->s.number && targ->client && attacker->client &&
        targ->client->playerTeam == attacker->client->playerTeam) { // player doesn't do knockback against allies unless he kills them
        dflags |= damageFlags_t::DAMAGE_DEATH_KNOCKBACK;
    }

    if (client && client->NPC_class == class_t::CLASS_GALAKMECH) { // hit Galak
        if (client->ps.stats[statIndex_t::STAT_ARMOR] > 0) {       // shields are up
            dflags &= ~damageFlags_t::DAMAGE_NO_ARMOR;             // always affect armor
            if (mod == meansOfDeath_t::MOD_ELECTROCUTE || mod == meansOfDeath_t::MOD_DEMP2 ||
                mod == meansOfDeath_t::MOD_DEMP2_ALT) { // shield protects us from this
                damage = 0;
            }
        }
        else {                                                                                                            // shields down
            if (mod == meansOfDeath_t::MOD_MELEE || (mod == meansOfDeath_t::MOD_CRUSH && attacker && attacker->client)) { // Galak takes no impact damage
                return;
            }
            if (Any(dflags & damageFlags_t::DAMAGE_RADIUS) || mod == meansOfDeath_t::MOD_REPEATER_ALT || mod == meansOfDeath_t::MOD_FLECHETTE_ALT ||
                mod == meansOfDeath_t::MOD_ROCKET || mod == meansOfDeath_t::MOD_ROCKET_ALT || mod == meansOfDeath_t::MOD_THERMAL ||
                mod == meansOfDeath_t::MOD_THERMAL_ALT || mod == meansOfDeath_t::MOD_DETPACK || mod == meansOfDeath_t::MOD_LASERTRIP ||
                mod == meansOfDeath_t::MOD_LASERTRIP_ALT || mod == meansOfDeath_t::MOD_EXPLOSIVE_SPLASH || mod == meansOfDeath_t::MOD_ENERGY_SPLASH ||
                mod == meansOfDeath_t::MOD_SABER) { // galak without shields takes quarter damage from explosives and lightsaber
                damage = Q_ceilf((float)damage / 4.0f);
            }
        }
    }

    if (mod == meansOfDeath_t::MOD_DEMP2 || mod == meansOfDeath_t::MOD_DEMP2_ALT) {
        if (client) {
            if (client->NPC_class == class_t::CLASS_PROTOCOL || client->NPC_class == class_t::CLASS_SEEKER || client->NPC_class == class_t::CLASS_R2D2 ||
                client->NPC_class == class_t::CLASS_R5D2 || client->NPC_class == class_t::CLASS_MOUSE || client->NPC_class == class_t::CLASS_GONK) {
                // DEMP2 does more damage to these guys.
                damage *= 2;
            }
            else if (client->NPC_class == class_t::CLASS_PROBE || client->NPC_class == class_t::CLASS_INTERROGATOR ||
                     client->NPC_class == class_t::CLASS_MARK1 || client->NPC_class == class_t::CLASS_MARK2 || client->NPC_class == class_t::CLASS_SENTRY ||
                     client->NPC_class == class_t::CLASS_ATST) {
                // DEMP2 does way more damage to these guys.
                damage *= 5;
            }
        }
        else if (targ->s.weapon == weapon_t::WP_TURRET) {
            damage *= 6; // more damage to turret things
        }
    }
    knockback = damage;

    // Attempt to apply extra knockback
    if (Any(dflags & damageFlags_t::DAMAGE_EXTRA_KNOCKBACK)) {
        knockback *= 2;
    }

    if (knockback > 200) {
        knockback = 200;
    }

    if (mod == meansOfDeath_t::MOD_CRUSH) {
        knockback = 0;
    }
    else if (Any(targ->flags & gentityFlags_t::FL_NO_KNOCKBACK)) {
        knockback = 0;
    }
    else if (targ->client && attacker->client && targ->client->playerTeam == attacker->client->playerTeam) {
        knockback = 0;
    }
    else if (Any(dflags & damageFlags_t::DAMAGE_NO_KNOCKBACK)) {
        knockback = 0;
    }
    // figure momentum add, even if the damage won't be taken
    if (knockback && !Any(dflags & damageFlags_t::DAMAGE_DEATH_KNOCKBACK)) //&& targ->client
    {
        G_ApplyKnockback(targ, newDir, knockback);
        G_CheckKnockdown(targ, attacker, newDir, dflags, mod);
    }

    // check for godmode, completely getting out of the damage
    if (Any(targ->flags & gentityFlags_t::FL_GODMODE) && !Any(dflags & damageFlags_t::DAMAGE_NO_PROTECTION)) {
        if (targ->client && attacker->client && targ->client->playerTeam == attacker->client->playerTeam &&
            (!targ->NPC || !targ->NPC->charmedTime)) { // complain, but don't turn on them
            G_FriendlyFireReaction(targ, attacker, dflags);
        }
        return;
    }

    // Check for team damage
    /*
    if ( targ != attacker && !Any( dflags & damageFlags_t::DAMAGE_IGNORE_TEAM ) && OnSameTeam (targ, attacker)  )
    {//on same team
        if ( !targ->client )
        {//a non-player object should never take damage from an ent on the same team
            return;
        }

        if ( attacker->client && attacker->client->playerTeam == targ->noDamageTeam )
        {//NPC or player shot an object on his own team
            return;
        }

        if ( attacker->s.number != 0 && targ->s.number != 0 &&//player not involved in any way in this exchange
            attacker->client && targ->client &&//two NPCs
            attacker->client->playerTeam == targ->client->playerTeam ) //on the same team
        {//NPCs on same team don't hurt each other
            return;
        }

        if ( targ->s.number == 0 &&//player was hit
            attacker->client && targ->client &&//by an NPC
            attacker->client->playerTeam == team_t::TEAM_PLAYER ) //on the same team
        {
            if ( attacker->enemy != targ )//by accident
            {//do no damage, no armor loss, no reaction, run no scripts
                return;
            }
        }
    }
    */

    // add to the attacker's hit counter
    if (attacker->client && targ != attacker && targ->health > 0) {
        if (OnSameTeam(targ, attacker)) {
            //			attacker->client->ps.persistant[persEnum_t::PERS_HITS] -= damage;
        }
        else {
            //			attacker->client->ps.persistant[persEnum_t::PERS_HITS] += damage;
        }
    }

    take = damage;

    // FIXME: Do not use this method of difficulty changing
    //  Scale the amount of damage given to the player based on the skill setting
    /*
    if ( targ->s.number == 0 && targ != attacker )
    {
        take *= ( g_spskill->integer + 1) * 0.75;
    }

    if ( take < 1 ) {
        take = 1;
    }
    */
    if (client) {
        // don't lose armor if on same team
        //  save some from armor
        asave = CheckArmor(targ, take, dflags);
        if (!asave) { // all out of armor
            targ->client->ps.powerups[powerup_t::PW_BATTLESUIT] = 0;
        }
        else {
            targ->client->ps.powerups[powerup_t::PW_BATTLESUIT] = level.time + ARMOR_EFFECT_TIME;
        }
        take -= asave;
    }
    if (!Any(dflags & damageFlags_t::DAMAGE_NO_HIT_LOC) || !Any(dflags & damageFlags_t::DAMAGE_RADIUS)) {
        if (!G_NonLocationSpecificDamage(mod)) { // certain kinds of damage don't care about hitlocation
            take = Q_ceilf((float)take * damageModifier[hitLoc]);
        }
    }

    if (g_debugDamage->integer) {
        Q_Println("[{}]client:{} health:{} damage:{} armor:{} hitloc:{}", level.time, targ->s.number, targ->health, take, asave, hitLocName[hitLoc]);
    }

    // add to the damage inflicted on a player this frame
    // the total will be turned into screen blends and view angle kicks
    // at the end of the frame
    if (client) {
        client->ps.persistant[persEnum_t::PERS_ATTACKER] = attacker->s.number; // attack can be the world ent
        client->damage_armor += asave;
        client->damage_blood += take;
        client->damage_knockback += knockback;
        if (dir) { // can't check newdir since it's local, newdir is dir normalized
            VectorCopy(newDir, client->damage_from);
            client->damage_fromWorld = false;
        }
        else {
            VectorCopy(targ->currentOrigin, client->damage_from);
            client->damage_fromWorld = true;
        }
    }

    // do the damage
    if (targ->health <= 0) {
        alreadyDead = true;
    }

    if (attacker && attacker->client && !attacker->s.number) {
        if (!alreadyDead) {
            int add;
            if (take > targ->health) {
                add = targ->health;
            }
            else {
                add = take;
            }
            add += asave;
            add = Q_ceilf(add / 10.0f);
            if (attacker != targ) {
                G_TrackWeaponUsage(attacker, inflictor, add, mod);
            }
        }
    }
    if (take || Any(dflags & damageFlags_t::DAMAGE_NO_DAMAGE)) {
        if (!targ->client || !attacker->client) {
            targ->health = targ->health - take;
            if (targ->health < 0) {
                targ->health = 0;
            }
            if (!alreadyDead && ((Any(targ->flags & gentityFlags_t::FL_UNDYING) && !Any(dflags & damageFlags_t::DAMAGE_NO_PROTECTION)) ||
                                 Any(dflags & damageFlags_t::DAMAGE_NO_KILL))) {
                if (targ->health < 1) {
                    G_ActivateBehavior(targ, bSet_t::BSET_DEATH);
                    targ->health = 1;
                }
            }
        }
        else { // two clients
            team_t targTeam = team_t::TEAM_FREE;
            team_t attackerTeam = team_t::TEAM_FREE;

            if (player->client->ps.viewEntity && targ->s.number == player->client->ps.viewEntity) {
                targTeam = player->client->playerTeam;
            }
            else if (targ->client) {
                targTeam = targ->client->playerTeam;
            }
            else {
                targTeam = targ->noDamageTeam;
            }
            //	if ( targTeam == TEAM_DISGUISE ) {
            //		targTeam = team_t::TEAM_PLAYER;
            //	}
            if (player->client->ps.viewEntity && attacker->s.number == player->client->ps.viewEntity) {
                attackerTeam = player->client->playerTeam;
            }
            else if (attacker->client) {
                attackerTeam = attacker->client->playerTeam;
            }
            else {
                attackerTeam = attacker->noDamageTeam;
            }
            //	if ( attackerTeam == TEAM_DISGUISE ) {
            //		attackerTeam = team_t::TEAM_PLAYER;
            //	}

            if (targTeam != attackerTeam) { // on the same team
                targ->health = targ->health - take;

                // MCG - Falling should never kill player- only if a trigger_hurt does so.
                if (mod == meansOfDeath_t::MOD_FALLING && targ->s.number == 0 && targ->health < 1) {
                    targ->health = 1;
                }
                else if (targ->health < 0) {
                    targ->health = 0;
                }

                if (!alreadyDead && ((Any(targ->flags & gentityFlags_t::FL_UNDYING) && !Any(dflags & damageFlags_t::DAMAGE_NO_PROTECTION)) ||
                                     Any(dflags & damageFlags_t::DAMAGE_NO_KILL))) {
                    if (targ->health < 1) {
                        G_ActivateBehavior(targ, bSet_t::BSET_DEATH);
                        targ->health = 1;
                    }
                }
                else if (targ->health < 1 && attacker->client) { // The player or NPC just killed an enemy so increment the kills counter
                    attacker->client->ps.persistant[persEnum_t::PERS_ENEMIES_KILLED]++;
                }
            }
            else if (targTeam == team_t::TEAM_PLAYER) { // on the same team, and target is an ally
                bool takeDamage = true;
                bool yellAtAttacker = true;

                // 1) player doesn't take damage from teammates unless they're angry at him
                if (targ->s.number == 0) {                             // the player
                    if (attacker->enemy != targ && attacker != targ) { // an NPC shot the player by accident
                        takeDamage = false;
                    }
                }
                // 2) NPCs don't take any damage from player during combat
                else { // an NPC
                    if ((Any(dflags & damageFlags_t::DAMAGE_RADIUS)) &&
                        !Any(dflags & damageFlags_t::DAMAGE_IGNORE_TEAM)) { // An NPC got hit by player and this is during combat or it was slash damage
                        // NOTE: though it's not realistic to have teammates not take splash damage,
                        //		even when not in combat, it feels really bad to have them able to
                        //		actually be killed by the player's splash damage
                        takeDamage = false;
                    }

                    if (Any(dflags & damageFlags_t::DAMAGE_RADIUS)) { // you're fighting and it's just radius damage, so don't even mention it
                        yellAtAttacker = false;
                    }
                }

                if (takeDamage) {
                    targ->health = targ->health - take;
                    if (!alreadyDead &&
                        ((Any(targ->flags & gentityFlags_t::FL_UNDYING) && !Any(dflags & damageFlags_t::DAMAGE_NO_PROTECTION) && attacker->s.number != 0) ||
                         Any(dflags & damageFlags_t::DAMAGE_NO_KILL))) { // guy is marked undying and we're not the player or we're in combat
                        if (targ->health < 1) {
                            G_ActivateBehavior(targ, bSet_t::BSET_DEATH);

                            targ->health = 1;
                        }
                    }
                    else if (!alreadyDead && ((Any(targ->flags & gentityFlags_t::FL_UNDYING) && !Any(dflags & damageFlags_t::DAMAGE_NO_PROTECTION) &&
                                               !attacker->s.number && !targ->s.number) ||
                                              Any(dflags & damageFlags_t::DAMAGE_NO_KILL))) { // player is undying and he's attacking himself, don't let him die
                        if (targ->health < 1) {
                            G_ActivateBehavior(targ, bSet_t::BSET_DEATH);

                            targ->health = 1;
                        }
                    }
                    else if (targ->health < 0) {
                        targ->health = 0;
                        if (attacker->s.number == 0 && targ->NPC) {
                            targ->NPC->scriptFlags |= scriptFlags_t::SCF_FFDEATH;
                        }
                    }
                }

                if (yellAtAttacker) {
                    if (!targ->NPC || !targ->NPC->charmedTime) {
                        G_FriendlyFireReaction(targ, attacker, dflags);
                    }
                }
            }
        }

        if (targ->client) {
            targ->client->ps.stats[statIndex_t::STAT_HEALTH] = targ->health;
            g_lastClientDamaged = targ;
        }

        // TEMP HACK FOR PLAYER LOOK AT ENEMY CODE
        // FIXME: move this to a player pain func?
        if (targ->s.number == 0) {
            if (!targ->enemy                                   // player does not have an enemy yet
                || targ->enemy->s.weapon != weapon_t::WP_SABER // or player's enemy is not a jedi
                || attacker->s.weapon == weapon_t::WP_SABER)   // and attacker is a jedi
            // keep enemy jedi over shooters
            {
                if (attacker->enemy == targ || !OnSameTeam(targ, attacker)) { // don't set player's enemy to teammates that hit him by accident
                    targ->enemy = attacker;
                }
                NPC_SetLookTarget(targ, attacker->s.number, level.time + 1000);
            }
        }
        else if (attacker->s.number == 0 && (!targ->NPC || !targ->NPC->timeOfDeath) &&
                 (mod == meansOfDeath_t::MOD_SABER || attacker->s.weapon != weapon_t::WP_SABER || !attacker->enemy ||
                  attacker->enemy->s.weapon != weapon_t::WP_SABER)) // keep enemy jedi over shooters
        {                                      // this looks dumb when they're on the ground and you keep hitting them, so only do this when first kill them
            if (!OnSameTeam(targ, attacker)) { // don't set player's enemy to teammates that he hits by accident
                attacker->enemy = targ;
            }
            NPC_SetLookTarget(attacker, targ->s.number, level.time + 1000);
        }
        // TEMP HACK FOR PLAYER LOOK AT ENEMY CODE

        // add up the damage to the location
        if (targ->client) {
            if (targ->locationDamage[hitLoc] < Q3_INFINITE) {
                targ->locationDamage[hitLoc] += take;
            }
        }
        if (targ->health > 0 && targ->NPC && targ->NPC->surrenderTime > level.time) { // he was surrendering, goes down with one hit
            targ->health = 0;
        }
        if (targ->health <= 0) {
            if (knockback && Any(dflags & damageFlags_t::DAMAGE_DEATH_KNOCKBACK)) //&& targ->client
            {                                                                     // only do knockback on death
                if (mod == meansOfDeath_t::MOD_FLECHETTE) { // special case because this is shotgun-ish damage, we need to multiply the knockback
                    knockback *= 12;                        //*6 for 6 flechette shots
                }
                G_ApplyKnockback(targ, newDir, knockback);
            }

            if (client)
                targ->flags |= gentityFlags_t::FL_NO_KNOCKBACK;

            if (targ->health < -999)
                targ->health = -999;

            // If we are a breaking glass brush, store the damage point so we can do cool things with it.
            if (Any(targ->svFlags & svFlags_t::SVF_GLASS_BRUSH)) {
                VectorCopy(point, targ->pos1);
                VectorCopy(dir, targ->pos2);
            }
            if (targ->client) { // HACK
                if (point) {
                    VectorCopy(point, targ->pos1);
                }
                else {
                    VectorCopy(targ->currentOrigin, targ->pos1);
                }
            }
            if (!alreadyDead && !targ->enemy) { // just killed and didn't have an enemy before
                targ->enemy = attacker;
            }
            GEntity_DieFunc(targ, inflictor, attacker, take, mod, dflags, hitLoc);
        }
        else {
            GEntity_PainFunc(targ, inflictor, attacker, point, take, mod, hitLoc);
            if (targ->s.number == 0) { // player run painscript
                G_ActivateBehavior(targ, bSet_t::BSET_PAIN);
                if (targ->health <= 25) {
                    G_ActivateBehavior(targ, bSet_t::BSET_FLEE);
                }
            }
        }
    }
}

/*
============
CanDamage

Returns true if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/
bool CanDamage(gentity_t* targ, vec3_t origin)
{
    vec3_t dest;
    trace_t tr;
    vec3_t midpoint;

    // use the midpoint of the bounds instead of the origin, because
    // bmodels may have their origin at 0,0,0
    VectorAdd(targ->absmin, targ->absmax, midpoint);
    VectorScale(midpoint, 0.5, midpoint);

    VectorCopy(midpoint, dest);
    gi.trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID, EG2_Collision::G2_NOCOLLIDE, 0);
    if ((tr.fraction == 1.0 && !Any(targ->contents & MASK_SOLID)) ||
        tr.entityNum == targ->s.number) // if we also test the entitynum's we can bust up bbrushes better!
        return true;

    // this should probably check in the plane of projection,
    // rather than in world coordinate, and also include Z
    VectorCopy(midpoint, dest);
    dest[0] += 15.0;
    dest[1] += 15.0;
    gi.trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID, EG2_Collision::G2_NOCOLLIDE, 0);
    if ((tr.fraction == 1.0 && !Any(targ->contents & MASK_SOLID)) || tr.entityNum == targ->s.number)
        return true;

    VectorCopy(midpoint, dest);
    dest[0] += 15.0;
    dest[1] -= 15.0;
    gi.trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID, EG2_Collision::G2_NOCOLLIDE, 0);
    if ((tr.fraction == 1.0 && !Any(targ->contents & MASK_SOLID)) || tr.entityNum == targ->s.number)
        return true;

    VectorCopy(midpoint, dest);
    dest[0] -= 15.0;
    dest[1] += 15.0;
    gi.trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID, EG2_Collision::G2_NOCOLLIDE, 0);
    if ((tr.fraction == 1.0 && !Any(targ->contents & MASK_SOLID)) || tr.entityNum == targ->s.number)
        return true;

    VectorCopy(midpoint, dest);
    dest[0] -= 15.0;
    dest[1] -= 15.0;
    gi.trace(&tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID, EG2_Collision::G2_NOCOLLIDE, 0);
    if ((tr.fraction == 1.0 && !Any(targ->contents & MASK_SOLID)) || tr.entityNum == targ->s.number)
        return true;

    return false;
}

/*
============
G_RadiusDamage
============
*/
void G_RadiusDamage(vec3_t origin, gentity_t* attacker, float damage, float radius, gentity_t* ignore, meansOfDeath_t mod)
{
    float points, dist;
    gentity_t* ent;
    gentity_t* entityList[MAX_GENTITIES];
    int numListedEntities;
    vec3_t mins, maxs;
    vec3_t v;
    vec3_t dir;
    int i, e;

    if (radius < 1) {
        radius = 1;
    }

    for (i = 0; i < 3; i++) {
        mins[i] = origin[i] - radius;
        maxs[i] = origin[i] + radius;
    }

    numListedEntities = gi.EntitiesInBox(mins, maxs, entityList, MAX_GENTITIES);

    for (e = 0; e < numListedEntities; e++) {
        ent = entityList[e];

        if (ent == ignore)
            continue;
        if (!ent->takedamage)
            continue;
        if (!Any(ent->contents))
            continue;

        // find the distance from the edge of the bounding box
        for (i = 0; i < 3; i++) {
            if (origin[i] < ent->absmin[i]) {
                v[i] = ent->absmin[i] - origin[i];
            }
            else if (origin[i] > ent->absmax[i]) {
                v[i] = origin[i] - ent->absmax[i];
            }
            else {
                v[i] = 0;
            }
        }

        dist = VectorLength(v);
        if (dist >= radius) {
            continue;
        }

        points = damage * (1.0 - dist / radius);

        if (CanDamage(ent, origin)) { // FIXME: still do a little damage in in PVS and close?
            if (Any(ent->svFlags & (svFlags_t::SVF_GLASS_BRUSH | svFlags_t::SVF_BBRUSH))) {
                VectorAdd(ent->absmin, ent->absmax, v);
                VectorScale(v, 0.5f, v);
            }
            else {
                VectorCopy(ent->currentOrigin, v);
            }

            VectorSubtract(v, origin, dir);
            // push the center of mass higher than the origin so players
            // get knocked into the air more
            dir[2] += 24;

            if (Any(ent->svFlags & svFlags_t::SVF_GLASS_BRUSH)) {
                if (points > 1.0f) {
                    // we want to cap this at some point, otherwise it just gets crazy
                    if (points > 6.0f) {
                        VectorScale(dir, 6.0f, dir);
                    }
                    else {
                        VectorScale(dir, points, dir);
                    }
                }

                ent->splashRadius = radius; // * ( 1.0 - dist / radius );
            }

            G_Damage(ent, nullptr, attacker, dir, origin, (int)points, damageFlags_t::DAMAGE_RADIUS, mod);
        }
    }
}

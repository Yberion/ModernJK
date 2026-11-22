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

#include <cgame/cg_local.h>
#include "g_functions.h"

#include "b_local.h"
#include "g_nav.h"

constexpr int AMMO_POD_HEALTH = 1;
constexpr g2SurfaceFlags_t TURN_OFF = g2SurfaceFlags_t::G2SURFACEFLAG_NODESCENDANTS;

constexpr int MIN_DISTANCE = 24;
constexpr int MIN_DISTANCE_SQR = MIN_DISTANCE * MIN_DISTANCE;

extern gitem_t* FindItemForAmmo(ammo_t ammo);

// Local state enums
enum class mark2LocalState_t {
    LSTATE_NONE = 0,
    LSTATE_DROPPINGDOWN,
    LSTATE_DOWN,
    LSTATE_RISINGUP,
};

gentity_t* CreateMissile(vec3_t org, vec3_t dir, float vel, int life, gentity_t* owner, bool altFire = false);

void NPC_Mark2_Precache(void)
{
    G_SoundIndex("sound/chars/mark2/misc/mark2_explo"); // blows up on death
    G_SoundIndex("sound/chars/mark2/misc/mark2_pain");
    G_SoundIndex("sound/chars/mark2/misc/mark2_fire");
    G_SoundIndex("sound/chars/mark2/misc/mark2_move_lp");

    G_EffectIndex("explosions/droidexplosion1");
    G_EffectIndex("env/med_explode2");
    G_EffectIndex("blaster/smoke_bolton");
    G_EffectIndex("bryar/muzzle_flash");

    RegisterItem(FindItemForWeapon(weapon_t::WP_BRYAR_PISTOL));
    RegisterItem(FindItemForAmmo(ammo_t::AMMO_METAL_BOLTS));
    RegisterItem(FindItemForAmmo(ammo_t::AMMO_POWERCELL));
    RegisterItem(FindItemForAmmo(ammo_t::AMMO_BLASTER));
}

/*
-------------------------
NPC_Mark2_Part_Explode
-------------------------
*/
void NPC_Mark2_Part_Explode(gentity_t* self, int bolt)
{
    if (bolt >= 0) {
        mdxaBone_t boltMatrix;
        vec3_t org, dir;

        gi.G2API_GetBoltMatrix(self->ghoul2, self->playerModel, bolt, &boltMatrix, self->currentAngles, self->currentOrigin, (cg.time ? cg.time : level.time),
                               nullptr, self->s.modelScale);

        gi.G2API_GiveMeVectorFromMatrix(boltMatrix, Eorientations::ORIGIN, org);
        gi.G2API_GiveMeVectorFromMatrix(boltMatrix, Eorientations::NEGATIVE_Y, dir);

        G_PlayEffect("env/med_explode2", org, dir);
        G_PlayEffect(G_EffectIndex("blaster/smoke_bolton"), self->playerModel, bolt, self->s.number, org);
    }

    self->count++; // Count of pods blown off
}

/*
-------------------------
NPC_Mark2_Pain
- look at what was hit and see if it should be removed from the model.
-------------------------
*/
void NPC_Mark2_Pain(gentity_t* self, gentity_t* inflictor, gentity_t* other, const vec3_t point, int damage, meansOfDeath_t mod, hitLocation_t hitLoc)
{
    int newBolt, i;

    NPC_Pain(self, inflictor, other, point, damage, mod);

    for (i = 0; i < 3; i++) {
        if ((hitLoc == hitLocation_t::HL_GENERIC1 + i) && (self->locationDamage[hitLocation_t::HL_GENERIC1 + i] > AMMO_POD_HEALTH)) // Blow it up?
        {
            if (self->locationDamage[hitLoc] >= AMMO_POD_HEALTH) {
                newBolt = gi.G2API_AddBolt(&self->ghoul2[self->playerModel], Q_Format("torso_canister{}", (i + 1)).c_str());
                if (newBolt != -1) {
                    NPC_Mark2_Part_Explode(self, newBolt);
                }
                gi.G2API_SetSurfaceOnOff(&self->ghoul2[self->playerModel], Q_Format("torso_canister{}", (i + 1)).c_str(), TURN_OFF);
                break;
            }
        }
    }

    G_Sound(self, G_SoundIndex("sound/chars/mark2/misc/mark2_pain"));

    // If any pods were blown off, kill him
    if (self->count > 0) {
        G_Damage(self, nullptr, nullptr, nullptr, nullptr, self->health, damageFlags_t::DAMAGE_NO_PROTECTION, meansOfDeath_t::MOD_UNKNOWN);
    }
}

/*
-------------------------
Mark2_Hunt
-------------------------
*/
void Mark2_Hunt(void)
{
    if (NPCInfo->goalEntity == nullptr) {
        NPCInfo->goalEntity = NPC->enemy;
    }

    // Turn toward him before moving towards him.
    NPC_FaceEnemy(true);

    NPCInfo->combatMove = true;
    NPC_MoveToGoal(true);
}

/*
-------------------------
Mark2_FireBlaster
-------------------------
*/
void Mark2_FireBlaster(bool advance)
{
    vec3_t muzzle1, enemy_org1, delta1, angleToEnemy1;
    static vec3_t forward, vright, up;
    gentity_t* missile;
    mdxaBone_t boltMatrix;

    gi.G2API_GetBoltMatrix(NPC->ghoul2, NPC->playerModel, NPC->genericBolt1, &boltMatrix, NPC->currentAngles, NPC->currentOrigin,
                           (cg.time ? cg.time : level.time), nullptr, NPC->s.modelScale);

    gi.G2API_GiveMeVectorFromMatrix(boltMatrix, Eorientations::ORIGIN, muzzle1);

    if (NPC->health) {
        CalcEntitySpot(NPC->enemy, spot_t::SPOT_HEAD, enemy_org1);
        VectorSubtract(enemy_org1, muzzle1, delta1);
        vectoangles(delta1, angleToEnemy1);
        AngleVectors(angleToEnemy1, forward, vright, up);
    }
    else {
        AngleVectors(NPC->currentAngles, forward, vright, up);
    }

    G_PlayEffect("bryar/muzzle_flash", muzzle1, forward);

    G_Sound(NPC, G_SoundIndex("sound/chars/mark2/misc/mark2_fire"));

    missile = CreateMissile(muzzle1, forward, 1600, 10000, NPC);

    missile->classname = "bryar_proj";
    missile->s.weapon = weapon_t::WP_BRYAR_PISTOL;

    missile->damage = 1;
    missile->dflags = damageFlags_t::DAMAGE_DEATH_KNOCKBACK;
    missile->methodOfDeath = meansOfDeath_t::MOD_ENERGY;
    missile->clipmask = MASK_SHOT | contentsFlags_t::CONTENTS_LIGHTSABER;
}

/*
-------------------------
Mark2_BlasterAttack
-------------------------
*/
void Mark2_BlasterAttack(bool advance)
{
    if (TIMER_Done(NPC, "attackDelay")) // Attack?
    {
        if (NPC_LocalState<mark2LocalState_t>(*NPCInfo) == mark2LocalState_t::LSTATE_NONE) // He's up so shoot less often.
        {
            TIMER_Set(NPC, "attackDelay", Q_irand(500, 2000));
        }
        else {
            TIMER_Set(NPC, "attackDelay", Q_irand(100, 500));
        }
        Mark2_FireBlaster(advance);
        return;
    }
    else if (advance) {
        Mark2_Hunt();
    }
}

/*
-------------------------
Mark2_AttackDecision
-------------------------
*/
void Mark2_AttackDecision(void)
{
    NPC_FaceEnemy(true);

    float distance = (int)DistanceHorizontalSquared(NPC->currentOrigin, NPC->enemy->currentOrigin);
    bool visible = NPC_ClearLOS(NPC->enemy);
    bool advance = (distance > MIN_DISTANCE_SQR);

    // He's been ordered to get up
    if (NPC_LocalState<mark2LocalState_t>(*NPCInfo) == mark2LocalState_t::LSTATE_RISINGUP) {
        NPC->flags &= ~gentityFlags_t::FL_SHIELDED;
        NPC_SetAnim(NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_RUN1START, setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_OVERRIDE);
        if ((NPC->client->ps.legsAnimTimer == 0) && NPC->client->ps.torsoAnim == animNumber_t::BOTH_RUN1START) {
            NPC_SetLocalState(*NPCInfo, mark2LocalState_t::LSTATE_NONE); // He's up again.
        }
        return;
    }

    // If we cannot see our target, move to see it
    if ((!visible) || (!NPC_FaceEnemy(true))) {
        // If he's going down or is down, make him get up
        if ((NPC_LocalState<mark2LocalState_t>(*NPCInfo) == mark2LocalState_t::LSTATE_DOWN) ||
            (NPC_LocalState<mark2LocalState_t>(*NPCInfo) == mark2LocalState_t::LSTATE_DROPPINGDOWN)) {
            if (TIMER_Done(NPC, "downTime")) // Down being down?? (The delay is so he doesn't pop up and down when the player goes in and out of range)
            {
                NPC_SetLocalState(*NPCInfo, mark2LocalState_t::LSTATE_RISINGUP);
                NPC_SetAnim(NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_RUN1STOP,
                            setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_OVERRIDE);
                TIMER_Set(NPC, "runTime", Q_irand(3000, 8000)); // So he runs for a while before testing to see if he should drop down.
            }
        }
        else {
            Mark2_Hunt();
        }
        return;
    }

    // He's down but he could advance if he wants to.
    if ((advance) && (TIMER_Done(NPC, "downTime")) && (NPC_LocalState<mark2LocalState_t>(*NPCInfo) == mark2LocalState_t::LSTATE_DOWN)) {
        NPC_SetLocalState(*NPCInfo, mark2LocalState_t::LSTATE_RISINGUP);
        NPC_SetAnim(NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_RUN1STOP, setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_OVERRIDE);
        TIMER_Set(NPC, "runTime", Q_irand(3000, 8000)); // So he runs for a while before testing to see if he should drop down.
    }

    NPC_FaceEnemy(true);

    // Dropping down to shoot
    if (NPC_LocalState<mark2LocalState_t>(*NPCInfo) == mark2LocalState_t::LSTATE_DROPPINGDOWN) {
        NPC_SetAnim(NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_RUN1STOP, setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_OVERRIDE);
        TIMER_Set(NPC, "downTime", Q_irand(3000, 9000));

        if ((NPC->client->ps.legsAnimTimer == 0) && NPC->client->ps.torsoAnim == animNumber_t::BOTH_RUN1STOP) {
            NPC->flags |= gentityFlags_t::FL_SHIELDED;
            NPC_SetLocalState(*NPCInfo, mark2LocalState_t::LSTATE_DOWN);
        }
    }
    // He's down and shooting
    else if (NPC_LocalState<mark2LocalState_t>(*NPCInfo) == mark2LocalState_t::LSTATE_DOWN) {
        //		NPC->flags |= gentityFlags_t::FL_SHIELDED;//only damagable by lightsabers and missiles

        Mark2_BlasterAttack(false);
    }
    else if (TIMER_Done(NPC, "runTime")) // Lowering down to attack. But only if he's done running at you.
    {
        NPC_SetLocalState(*NPCInfo, mark2LocalState_t::LSTATE_DROPPINGDOWN);
    }
    else if (advance) {
        // We can see enemy so shoot him if timer lets you.
        Mark2_BlasterAttack(advance);
    }
}

/*
-------------------------
Mark2_Patrol
-------------------------
*/
void Mark2_Patrol(void)
{
    if (NPC_CheckPlayerTeamStealth()) {
        //		G_Sound( NPC, G_SoundIndex("sound/chars/mark1/misc/anger.wav"));
        NPC_UpdateAngles(true, true);
        return;
    }

    // If we have somewhere to go, then do that
    if (!NPC->enemy) {
        if (UpdateGoal()) {
            ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
            NPC_MoveToGoal(true);
            NPC_UpdateAngles(true, true);
        }

        // randomly talk
        if (TIMER_Done(NPC, "patrolNoise")) {
            //			G_Sound( NPC, G_SoundIndex(Q_Format("sound/chars/mark1/misc/talk{}.wav", Q_irand(1, 4)).c_str()));

            TIMER_Set(NPC, "patrolNoise", Q_irand(2000, 4000));
        }
    }
}

/*
-------------------------
Mark2_Idle
-------------------------
*/
void Mark2_Idle(void)
{
    NPC_BSIdle();
}

/*
-------------------------
NPC_BSMark2_Default
-------------------------
*/
void NPC_BSMark2_Default(void)
{
    if (NPC->enemy) {
        NPCInfo->goalEntity = NPC->enemy;
        Mark2_AttackDecision();
    }
    else if (Any(NPCInfo->scriptFlags & scriptFlags_t::SCF_LOOK_FOR_ENEMIES)) {
        Mark2_Patrol();
    }
    else {
        Mark2_Idle();
    }
}

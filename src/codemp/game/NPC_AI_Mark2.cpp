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

#include "b_local.h"
#include "g_nav.h"

constexpr int AMMO_POD_HEALTH = 1;
constexpr g2SurfaceFlags_t TURN_OFF = g2SurfaceFlags_t::G2SURFACEFLAG_NODESCENDANTS;

constexpr int MIN_DISTANCE = 24;
constexpr int MIN_DISTANCE_SQR = MIN_DISTANCE * MIN_DISTANCE;

extern gitem_t* BG_FindItemForAmmo(ammo_t ammo);

// Local state enums
enum class mark2LocalState_t {
    LSTATE_NONE = 0,
    LSTATE_DROPPINGDOWN,
    LSTATE_DOWN,
    LSTATE_RISINGUP,
};

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

    RegisterItem(BG_FindItemForWeapon(weapon_t::WP_BRYAR_PISTOL));
    RegisterItem(BG_FindItemForAmmo(ammo_t::AMMO_METAL_BOLTS));
    RegisterItem(BG_FindItemForAmmo(ammo_t::AMMO_POWERCELL));
    RegisterItem(BG_FindItemForAmmo(ammo_t::AMMO_BLASTER));
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

        trap->G2API_GetBoltMatrix(self->ghoul2, 0, bolt, &boltMatrix, self->r.currentAngles, self->r.currentOrigin, level.time, nullptr, self->modelScale);

        BG_GiveMeVectorFromMatrix(&boltMatrix, orientations_t::ORIGIN, org);
        BG_GiveMeVectorFromMatrix(&boltMatrix, orientations_t::NEGATIVE_Y, dir);

        G_PlayEffectID(G_EffectIndex("env/med_explode2"), org, dir);
        G_PlayEffectID(G_EffectIndex("blaster/smoke_bolton"), org, dir);
    }

    // G_PlayEffectID( G_EffectIndex("blaster/smoke_bolton"), self->playerModel, bolt, self->s.number);

    self->count++; // Count of pods blown off
}

/*
-------------------------
NPC_Mark2_Pain
- look at what was hit and see if it should be removed from the model.
-------------------------
*/
void NPC_Mark2_Pain(gentity_t* self, gentity_t* attacker, int damage)
{
    int newBolt, i;
    hitLocation_t hitLoc = static_cast<hitLocation_t>(gPainHitLoc);

    NPC_Pain(self, attacker, damage);

    for (i = 0; i < 3; i++) {
        if ((hitLoc == hitLocation_t::HL_GENERIC1 + i) &&
            (self->locationDamage[std::to_underlying(hitLocation_t::HL_GENERIC1 + i)] > AMMO_POD_HEALTH)) // Blow it up?
        {
            if (self->locationDamage[std::to_underlying(hitLoc)] >= AMMO_POD_HEALTH) {
                newBolt = trap->G2API_AddBolt(self->ghoul2, 0, Q_Format("torso_canister{}", (i + 1)).c_str());
                if (newBolt != -1) {
                    NPC_Mark2_Part_Explode(self, newBolt);
                }
                NPC_SetSurfaceOnOff(self, Q_Format("torso_canister{}", (i + 1)).c_str(), TURN_OFF);
                break;
            }
        }
    }

    G_Sound(self, sound::SoundClass::World, sound::Slot::None, G_SoundIndex("sound/chars/mark2/misc/mark2_pain"));

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
    if (NPCS.NPCInfo->goalEntity == nullptr) {
        NPCS.NPCInfo->goalEntity = NPCS.NPC->enemy;
    }

    // Turn toward him before moving towards him.
    NPC_FaceEnemy(true);

    NPCS.NPCInfo->combatMove = true;
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
    //	static	vec3_t	muzzle;
    gentity_t* missile;
    mdxaBone_t boltMatrix;
    int bolt = trap->G2API_AddBolt(NPCS.NPC->ghoul2, 0, "*flash");

    trap->G2API_GetBoltMatrix(NPCS.NPC->ghoul2, 0, bolt, &boltMatrix, NPCS.NPC->r.currentAngles, NPCS.NPC->r.currentOrigin, level.time, nullptr,
                              NPCS.NPC->modelScale);

    BG_GiveMeVectorFromMatrix(&boltMatrix, orientations_t::ORIGIN, muzzle1);

    if (NPCS.NPC->health) {
        CalcEntitySpot(NPCS.NPC->enemy, spot_t::SPOT_HEAD, enemy_org1);
        VectorSubtract(enemy_org1, muzzle1, delta1);
        vectoangles(delta1, angleToEnemy1);
        AngleVectors(angleToEnemy1, forward, vright, up);
    }
    else {
        AngleVectors(NPCS.NPC->r.currentAngles, forward, vright, up);
    }

    G_PlayEffectID(G_EffectIndex("bryar/muzzle_flash"), muzzle1, forward);

    G_Sound(NPCS.NPC, sound::SoundClass::Weapon, sound::Slot::None, G_SoundIndex("sound/chars/mark2/misc/mark2_fire"));

    missile = CreateMissile(muzzle1, forward, 1600, 10000, NPCS.NPC, false);

    missile->classname = "bryar_proj";
    missile->s.weapon = weapon_t::WP_BRYAR_PISTOL;

    missile->damage = 1;
    missile->dflags = damageFlags_t::DAMAGE_DEATH_KNOCKBACK;
    missile->methodOfDeath = meansOfDeath_t::MOD_BRYAR_PISTOL;
    missile->clipmask = MASK_SHOT | contentsFlags_t::CONTENTS_LIGHTSABER;
}

/*
-------------------------
Mark2_BlasterAttack
-------------------------
*/
void Mark2_BlasterAttack(bool advance)
{
    if (TIMER_Done(NPCS.NPC, "attackDelay")) // Attack?
    {
        if (NPC_LocalState<mark2LocalState_t>(*NPCS.NPCInfo) == mark2LocalState_t::LSTATE_NONE) // He's up so shoot less often.
        {
            TIMER_Set(NPCS.NPC, "attackDelay", Q_irand(500, 2000));
        }
        else {
            TIMER_Set(NPCS.NPC, "attackDelay", Q_irand(100, 500));
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
    float distance;
    bool visible;
    bool advance;

    NPC_FaceEnemy(true);

    distance = (int)DistanceHorizontalSquared(NPCS.NPC->r.currentOrigin, NPCS.NPC->enemy->r.currentOrigin);
    visible = NPC_ClearLOS4(NPCS.NPC->enemy);
    advance = (distance > MIN_DISTANCE_SQR);

    // He's been ordered to get up
    if (NPC_LocalState<mark2LocalState_t>(*NPCS.NPCInfo) == mark2LocalState_t::LSTATE_RISINGUP) {
        NPCS.NPC->flags &= ~gentityFlags_t::FL_SHIELDED;
        NPC_SetAnim(NPCS.NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_RUN1START,
                    setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_OVERRIDE);
        if ((NPCS.NPC->client->ps.legsTimer <= 0) && NPCS.NPC->client->ps.torsoAnim == animNumber_t::BOTH_RUN1START) {
            NPC_SetLocalState(*NPCS.NPCInfo, mark2LocalState_t::LSTATE_NONE); // He's up again.
        }
        return;
    }

    // If we cannot see our target, move to see it
    if ((!visible) || (!NPC_FaceEnemy(true))) {
        // If he's going down or is down, make him get up
        if ((NPC_LocalState<mark2LocalState_t>(*NPCS.NPCInfo) == mark2LocalState_t::LSTATE_DOWN) ||
            (NPC_LocalState<mark2LocalState_t>(*NPCS.NPCInfo) == mark2LocalState_t::LSTATE_DROPPINGDOWN)) {
            if (TIMER_Done(NPCS.NPC, "downTime")) // Down being down?? (The delay is so he doesn't pop up and down when the player goes in and out of range)
            {
                NPC_SetLocalState(*NPCS.NPCInfo, mark2LocalState_t::LSTATE_RISINGUP);
                NPC_SetAnim(NPCS.NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_RUN1STOP,
                            setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_OVERRIDE);
                TIMER_Set(NPCS.NPC, "runTime", Q_irand(3000, 8000)); // So he runs for a while before testing to see if he should drop down.
            }
        }
        else {
            Mark2_Hunt();
        }
        return;
    }

    // He's down but he could advance if he wants to.
    if ((advance) && (TIMER_Done(NPCS.NPC, "downTime")) && (NPC_LocalState<mark2LocalState_t>(*NPCS.NPCInfo) == mark2LocalState_t::LSTATE_DOWN)) {
        NPC_SetLocalState(*NPCS.NPCInfo, mark2LocalState_t::LSTATE_RISINGUP);
        NPC_SetAnim(NPCS.NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_RUN1STOP,
                    setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_OVERRIDE);
        TIMER_Set(NPCS.NPC, "runTime", Q_irand(3000, 8000)); // So he runs for a while before testing to see if he should drop down.
    }

    NPC_FaceEnemy(true);

    // Dropping down to shoot
    if (NPC_LocalState<mark2LocalState_t>(*NPCS.NPCInfo) == mark2LocalState_t::LSTATE_DROPPINGDOWN) {
        NPC_SetAnim(NPCS.NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_RUN1STOP,
                    setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_OVERRIDE);
        TIMER_Set(NPCS.NPC, "downTime", Q_irand(3000, 9000));

        if ((NPCS.NPC->client->ps.legsTimer <= 0) && NPCS.NPC->client->ps.torsoAnim == animNumber_t::BOTH_RUN1STOP) {
            NPCS.NPC->flags |= gentityFlags_t::FL_SHIELDED;
            NPC_SetLocalState(*NPCS.NPCInfo, mark2LocalState_t::LSTATE_DOWN);
        }
    }
    // He's down and shooting
    else if (NPC_LocalState<mark2LocalState_t>(*NPCS.NPCInfo) == mark2LocalState_t::LSTATE_DOWN) {
        NPCS.NPC->flags |= gentityFlags_t::FL_SHIELDED; // only damagable by lightsabers and missiles

        Mark2_BlasterAttack(false);
    }
    else if (TIMER_Done(NPCS.NPC, "runTime")) // Lowering down to attack. But only if he's done running at you.
    {
        NPC_SetLocalState(*NPCS.NPCInfo, mark2LocalState_t::LSTATE_DROPPINGDOWN);
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
    if (!NPCS.NPC->enemy) {
        if (UpdateGoal()) {
            NPCS.ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
            NPC_MoveToGoal(true);
            NPC_UpdateAngles(true, true);
        }

        // randomly talk
        if (TIMER_Done(NPCS.NPC, "patrolNoise")) {
            //			G_Sound( NPC, G_SoundIndex(Q_Format("sound/chars/mark1/misc/talk{}.wav", Q_irand(1, 4)).c_str()));

            TIMER_Set(NPCS.NPC, "patrolNoise", Q_irand(2000, 4000));
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
    if (NPCS.NPC->enemy) {
        NPCS.NPCInfo->goalEntity = NPCS.NPC->enemy;
        Mark2_AttackDecision();
    }
    else if (Any(NPCS.NPCInfo->scriptFlags & scriptFlags_t::SCF_LOOK_FOR_ENEMIES)) {
        Mark2_Patrol();
    }
    else {
        Mark2_Idle();
    }
}

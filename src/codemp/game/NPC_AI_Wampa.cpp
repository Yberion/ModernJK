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

// These define the working combat range for these suckers
constexpr int MIN_DISTANCE = 48;

constexpr int MAX_DISTANCE = 1024;

float enemyDist = 0;

void Wampa_SetBolts(gentity_t* self)
{
    if (self && self->client) {
        renderInfo_t* ri = &self->client->renderInfo;
        ri->headBolt = trap->G2API_AddBolt(self->ghoul2, 0, "*head_eyes");
        // ri->cervicalBolt = trap->G2API_AddBolt(self->ghoul2, 0, "neck_bone" );
        // ri->chestBolt = trap->G2API_AddBolt(self->ghoul2, 0, "upper_spine");
        // ri->gutBolt = trap->G2API_AddBolt(self->ghoul2, 0, "mid_spine");
        ri->torsoBolt = trap->G2API_AddBolt(self->ghoul2, 0, "lower_spine");
        ri->crotchBolt = trap->G2API_AddBolt(self->ghoul2, 0, "rear_bone");
        // ri->elbowLBolt = trap->G2API_AddBolt(self->ghoul2, 0, "*l_arm_elbow");
        // ri->elbowRBolt = trap->G2API_AddBolt(self->ghoul2, 0, "*r_arm_elbow");
        ri->handLBolt = trap->G2API_AddBolt(self->ghoul2, 0, "*l_hand");
        ri->handRBolt = trap->G2API_AddBolt(self->ghoul2, 0, "*r_hand");
        // ri->kneeLBolt = trap->G2API_AddBolt(self->ghoul2, 0, "*hips_l_knee");
        // ri->kneeRBolt = trap->G2API_AddBolt(self->ghoul2, 0, "*hips_r_knee");
        ri->footLBolt = trap->G2API_AddBolt(self->ghoul2, 0, "*l_leg_foot");
        ri->footRBolt = trap->G2API_AddBolt(self->ghoul2, 0, "*r_leg_foot");
    }
}

/*
-------------------------
NPC_Wampa_Precache
-------------------------
*/
void NPC_Wampa_Precache(void)
{
    /*
    int i;
    for ( i = 1; i < 4; i ++ )
    {
        G_SoundIndex( Q_Format("sound/chars/wampa/growl{}.wav", i).c_str() );
    }
    for ( i = 1; i < 3; i ++ )
    {
        G_SoundIndex( Q_Format("sound/chars/wampa/snort{}.wav", i).c_str() );
    }
    */
    G_SoundIndex("sound/chars/rancor/swipehit.wav");
    // G_SoundIndex( "sound/chars/wampa/chomp.wav" );
}

/*
-------------------------
Wampa_Idle
-------------------------
*/
void Wampa_Idle(void)
{
    NPC_SetLocalState(*NPCS.NPCInfo, monsterLocalState_t::LSTATE_CLEAR);

    // If we have somewhere to go, then do that
    if (UpdateGoal()) {
        NPCS.ucmd.buttons &= ~buttonFlags_t::BUTTON_WALKING;
        NPC_MoveToGoal(true);
    }
}

bool Wampa_CheckRoar(gentity_t* self)
{
    if (self->wait < level.time) {
        self->wait = level.time + Q_irand(5000, 20000);
        NPC_SetAnim(self, setAnimParts_t::SETANIM_BOTH, Q_irand(animNumber_t::BOTH_GESTURE1, animNumber_t::BOTH_GESTURE2),
                    (setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD));
        TIMER_Set(self, "rageTime", self->client->ps.legsTimer);
        return true;
    }
    return false;
}
/*
-------------------------
Wampa_Patrol
-------------------------
*/
void Wampa_Patrol(void)
{
    NPC_SetLocalState(*NPCS.NPCInfo, monsterLocalState_t::LSTATE_CLEAR);

    // If we have somewhere to go, then do that
    if (UpdateGoal()) {
        NPCS.ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
        NPC_MoveToGoal(true);
    }
    else {
        if (TIMER_Done(NPCS.NPC, "patrolTime")) {
            TIMER_Set(NPCS.NPC, "patrolTime", Q_flrand(-1.0f, 1.0f) * 5000 + 5000);
        }
    }

    if (NPC_CheckEnemyExt(true) == false) {
        Wampa_Idle();
        return;
    }
    Wampa_CheckRoar(NPCS.NPC);
    TIMER_Set(NPCS.NPC, "lookForNewEnemy", Q_irand(5000, 15000));
}

/*
-------------------------
Wampa_Move
-------------------------
*/
void Wampa_Move(bool visible)
{
    if (NPC_LocalState<monsterLocalState_t>(*NPCS.NPCInfo) != monsterLocalState_t::LSTATE_WAITING) {
        NPCS.NPCInfo->goalEntity = NPCS.NPC->enemy;

        if (NPCS.NPC->enemy) { // pick correct movement speed and anim
            // run by default
            NPCS.ucmd.buttons &= ~buttonFlags_t::BUTTON_WALKING;
            if (!TIMER_Done(NPCS.NPC, "runfar") || !TIMER_Done(NPCS.NPC, "runclose")) { // keep running with this anim & speed for a bit
            }
            else if (!TIMER_Done(NPCS.NPC, "walk")) { // keep walking for a bit
                NPCS.ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
            }
            else if (visible && enemyDist > 384 && NPCS.NPCInfo->stats.runSpeed == 180) { // fast run, all fours
                NPCS.NPCInfo->stats.runSpeed = 300;
                TIMER_Set(NPCS.NPC, "runfar", Q_irand(2000, 4000));
            }
            else if (enemyDist > 256 && NPCS.NPCInfo->stats.runSpeed == 300) { // slow run, upright
                NPCS.NPCInfo->stats.runSpeed = 180;
                TIMER_Set(NPCS.NPC, "runclose", Q_irand(3000, 5000));
            }
            else if (enemyDist < 128) { // walk
                NPCS.NPCInfo->stats.runSpeed = 180;
                NPCS.ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
                TIMER_Set(NPCS.NPC, "walk", Q_irand(4000, 6000));
            }
        }

        if (NPCS.NPCInfo->stats.runSpeed == 300) { // need to use the alternate run - hunched over on all fours
            NPCS.NPC->client->ps.eFlags2 |= entityFlags2_t::EF2_USE_ALT_ANIM;
        }
        NPC_MoveToGoal(true);
        NPCS.NPCInfo->goalRadius = MAX_DISTANCE; // just get us within combat range
    }
}

//---------------------------------------------------------
extern void G_Knockdown(gentity_t* victim);
extern void G_Dismember(gentity_t* ent, gentity_t* enemy, vec3_t point, g2ModelParts_t limbType, float limbRollBase, float limbPitchBase,
                        animNumber_t deathAnim, bool postDeath);
extern int NPC_GetEntsNearBolt(int* radiusEnts, float radius, int boltIndex, vec3_t boltOrg);

void Wampa_Slash(int boltIndex, bool backhand)
{
    int radiusEntNums[128];
    int numEnts;
    const float radius = 88;
    const float radiusSquared = (radius * radius);
    int i;
    vec3_t boltOrg;
    int damage = (backhand) ? Q_irand(10, 15) : Q_irand(20, 30);

    numEnts = NPC_GetEntsNearBolt(radiusEntNums, radius, boltIndex, boltOrg);

    for (i = 0; i < numEnts; i++) {
        gentity_t* radiusEnt = &g_entities[radiusEntNums[i]];
        if (!radiusEnt->inuse) {
            continue;
        }

        if (radiusEnt == NPCS.NPC) { // Skip the wampa ent
            continue;
        }

        if (radiusEnt->client == nullptr) { // must be a client
            continue;
        }

        if (DistanceSquared(radiusEnt->r.currentOrigin, boltOrg) <= radiusSquared) {
            // smack
            G_Damage(radiusEnt, NPCS.NPC, NPCS.NPC, vec3_origin, radiusEnt->r.currentOrigin, damage,
                     ((backhand) ? damageFlags_t::DAMAGE_NO_ARMOR : (damageFlags_t::DAMAGE_NO_ARMOR | damageFlags_t::DAMAGE_NO_KNOCKBACK)),
                     meansOfDeath_t::MOD_MELEE);
            if (backhand) {
                // actually push the enemy
                vec3_t pushDir;
                vec3_t angs;
                VectorCopy(NPCS.NPC->client->ps.viewangles, angs);
                angs[YAW] += Q_flrand(25, 50);
                angs[PITCH] = Q_flrand(-25, -15);
                AngleVectors(angs, pushDir, nullptr, nullptr);
                if (radiusEnt->client->NPC_class != class_t::CLASS_WAMPA && radiusEnt->client->NPC_class != class_t::CLASS_RANCOR &&
                    radiusEnt->client->NPC_class != class_t::CLASS_ATST) {
                    G_Throw(radiusEnt, pushDir, 65);
                    if (BG_KnockDownable(&radiusEnt->client->ps) && radiusEnt->health > 0 && Q_irand(0, 1)) { // do pain on enemy
                        radiusEnt->client->ps.forceHandExtend = forceHandAnims_t::HANDEXTEND_KNOCKDOWN;
                        radiusEnt->client->ps.forceDodgeAnim = 0;
                        radiusEnt->client->ps.forceHandExtendTime = level.time + 1100;
                        radiusEnt->client->ps.quickerGetup = false;
                    }
                }
            }
            else if (radiusEnt->health <= 0 && radiusEnt->client) { // killed them, chance of dismembering
                if (!Q_irand(0, 1)) {                               // bite something off
                    g2ModelParts_t hitLoc = Q_irand(g2ModelParts_t::G2_MODELPART_HEAD, g2ModelParts_t::G2_MODELPART_RLEG);
                    if (hitLoc == g2ModelParts_t::G2_MODELPART_HEAD) {
                        NPC_SetAnim(radiusEnt, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_DEATH17,
                                    setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                    }
                    else if (hitLoc == g2ModelParts_t::G2_MODELPART_WAIST) {
                        NPC_SetAnim(radiusEnt, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_DEATHBACKWARD2,
                                    setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                    }
                    G_Dismember(radiusEnt, NPCS.NPC, radiusEnt->r.currentOrigin, hitLoc, 90, 0, radiusEnt->client->ps.torsoAnim, true);
                }
            }
            else if (!Q_irand(0, 3) && radiusEnt->health > 0) { // one out of every 4 normal hits does a knockdown, too
                vec3_t pushDir;
                vec3_t angs;
                VectorCopy(NPCS.NPC->client->ps.viewangles, angs);
                angs[YAW] += Q_flrand(25, 50);
                angs[PITCH] = Q_flrand(-25, -15);
                AngleVectors(angs, pushDir, nullptr, nullptr);
                G_Knockdown(radiusEnt);
            }
            G_Sound(radiusEnt, sound::SoundClass::Weapon, sound::Slot::Weapon, G_SoundIndex("sound/chars/rancor/swipehit.wav"));
        }
    }
}

//------------------------------
void Wampa_Attack(float distance, bool doCharge)
{
    if (!TIMER_Exists(NPCS.NPC, "attacking")) {
        if (Q_irand(0, 2) && !doCharge) { // double slash
            NPC_SetAnim(NPCS.NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_ATTACK1,
                        setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
            TIMER_Set(NPCS.NPC, "attack_dmg", 750);
        }
        else if (doCharge || (distance > 270 && distance < 430 && !Q_irand(0, 1))) { // leap
            vec3_t fwd, yawAng;
            VectorSet(yawAng, 0, NPCS.NPC->client->ps.viewangles[YAW], 0);
            NPC_SetAnim(NPCS.NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_ATTACK2,
                        setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
            TIMER_Set(NPCS.NPC, "attack_dmg", 500);
            AngleVectors(yawAng, fwd, nullptr, nullptr);
            VectorScale(fwd, distance * 1.5f, NPCS.NPC->client->ps.velocity);
            NPCS.NPC->client->ps.velocity[2] = 150;
            NPCS.NPC->client->ps.groundEntityNum = ENTITYNUM_NONE;
        }
        else { // backhand
            NPC_SetAnim(NPCS.NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_ATTACK3,
                        setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
            TIMER_Set(NPCS.NPC, "attack_dmg", 250);
        }

        TIMER_Set(NPCS.NPC, "attacking", NPCS.NPC->client->ps.legsTimer + Q_flrand(0.0f, 1.0f) * 200);
        // allow us to re-evaluate our running speed/anim
        TIMER_Set(NPCS.NPC, "runfar", -1);
        TIMER_Set(NPCS.NPC, "runclose", -1);
        TIMER_Set(NPCS.NPC, "walk", -1);
    }

    // Need to do delayed damage since the attack animations encapsulate multiple mini-attacks

    if (TIMER_Done2(NPCS.NPC, "attack_dmg", true)) {
        switch (NPCS.NPC->client->ps.legsAnim) {
        case animNumber_t::BOTH_ATTACK1:
            Wampa_Slash(NPCS.NPC->client->renderInfo.handRBolt, false);
            // do second hit
            TIMER_Set(NPCS.NPC, "attack_dmg2", 100);
            break;
        case animNumber_t::BOTH_ATTACK2:
            Wampa_Slash(NPCS.NPC->client->renderInfo.handRBolt, false);
            TIMER_Set(NPCS.NPC, "attack_dmg2", 100);
            break;
        case animNumber_t::BOTH_ATTACK3:
            Wampa_Slash(NPCS.NPC->client->renderInfo.handLBolt, true);
            break;
        default:
            break;
        }
    }
    else if (TIMER_Done2(NPCS.NPC, "attack_dmg2", true)) {
        switch (NPCS.NPC->client->ps.legsAnim) {
        case animNumber_t::BOTH_ATTACK1:
            Wampa_Slash(NPCS.NPC->client->renderInfo.handLBolt, false);
            break;
        case animNumber_t::BOTH_ATTACK2:
            Wampa_Slash(NPCS.NPC->client->renderInfo.handLBolt, false);
            break;
        default:
            break;
        }
    }

    // Just using this to remove the attacking flag at the right time
    TIMER_Done2(NPCS.NPC, "attacking", true);

    if (NPCS.NPC->client->ps.legsAnim == animNumber_t::BOTH_ATTACK1 && distance > (NPCS.NPC->r.maxs[0] + MIN_DISTANCE)) { // okay to keep moving
        NPCS.ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
        Wampa_Move(true);
    }
}

//----------------------------------
void Wampa_Combat(void)
{
    // If we cannot see our target or we have somewhere to go, then do that
    if (!NPC_ClearLOS(NPCS.NPC->r.currentOrigin, NPCS.NPC->enemy->r.currentOrigin)) {
        if (!Q_irand(0, 10)) {
            if (Wampa_CheckRoar(NPCS.NPC)) {
                return;
            }
        }
        NPCS.NPCInfo->combatMove = true;
        NPCS.NPCInfo->goalEntity = NPCS.NPC->enemy;
        NPCS.NPCInfo->goalRadius = MAX_DISTANCE; // just get us within combat range

        Wampa_Move(false);
        return;
    }
    else if (UpdateGoal()) {
        NPCS.NPCInfo->combatMove = true;
        NPCS.NPCInfo->goalEntity = NPCS.NPC->enemy;
        NPCS.NPCInfo->goalRadius = MAX_DISTANCE; // just get us within combat range

        Wampa_Move(true);
        return;
    }
    else {
        float distance = enemyDist = Distance(NPCS.NPC->r.currentOrigin, NPCS.NPC->enemy->r.currentOrigin);
        bool advance = (distance > (NPCS.NPC->r.maxs[0] + MIN_DISTANCE) ? true : false);
        bool doCharge = false;

        // Sometimes I have problems with facing the enemy I'm attacking, so force the issue so I don't look dumb
        // FIXME: always seems to face off to the left or right?!!!!
        NPC_FaceEnemy(true);

        if (advance) { // have to get closer
            vec3_t yawOnlyAngles;
            VectorSet(yawOnlyAngles, 0, NPCS.NPC->r.currentAngles[YAW], 0);
            if (NPCS.NPC->enemy->health > 0                                                                    // enemy still alive
                && Q_fabs(distance - 350) <= 80                                                                // enemy anywhere from 270 to 430 away
                && InFOV3(NPCS.NPC->enemy->r.currentOrigin, NPCS.NPC->r.currentOrigin, yawOnlyAngles, 20, 20)) // enemy generally in front
            {                                                                                                  // 10% chance of doing charge anim
                if (!Q_irand(0, 9)) {                                                                          // go for the charge
                    doCharge = true;
                    advance = false;
                }
            }
        }

        if ((advance || NPC_LocalState<monsterLocalState_t>(*NPCS.NPCInfo) == monsterLocalState_t::LSTATE_WAITING) &&
            TIMER_Done(NPCS.NPC, "attacking")) // waiting monsters can't attack
        {
            if (TIMER_Done2(NPCS.NPC, "takingPain", true)) {
                NPC_SetLocalState(*NPCS.NPCInfo, monsterLocalState_t::LSTATE_CLEAR);
            }
            else {
                Wampa_Move(true);
            }
        }
        else {
            if (!Q_irand(0, 20)) { // FIXME: only do this if we just damaged them or vice-versa?
                if (Wampa_CheckRoar(NPCS.NPC)) {
                    return;
                }
            }
            if (!Q_irand(0, 1)) { // FIXME: base on skill
                Wampa_Attack(distance, doCharge);
            }
        }
    }
}

/*
-------------------------
NPC_Wampa_Pain
-------------------------
*/
void NPC_Wampa_Pain(gentity_t* self, gentity_t* attacker, int damage)
{
    bool hitByWampa = false;
    if (attacker && attacker->client && attacker->client->NPC_class == class_t::CLASS_WAMPA) {
        hitByWampa = true;
    }
    if (attacker && attacker->inuse && attacker != self->enemy && !Any(attacker->flags & gentityFlags_t::FL_NOTARGET)) {
        if ((!attacker->s.number && !Q_irand(0, 3)) || !self->enemy || self->enemy->health == 0 ||
            (self->enemy->client && self->enemy->client->NPC_class == class_t::CLASS_WAMPA) ||
            (!Q_irand(0, 4) &&
             DistanceSquared(attacker->r.currentOrigin, self->r.currentOrigin) <
                 DistanceSquared(
                     self->enemy->r.currentOrigin,
                     self->r.currentOrigin))) { // if my enemy is dead (or attacked by player) and I'm not still holding/eating someone, turn on the attacker
            // FIXME: if can't nav to my enemy, take this guy if I can nav to him
            G_SetEnemy(self, attacker);
            TIMER_Set(self, "lookForNewEnemy", Q_irand(5000, 15000));
            if (hitByWampa) { // stay mad at this Wampa for 2-5 secs before looking for attacker enemies
                TIMER_Set(self, "wampaInfight", Q_irand(2000, 5000));
            }
        }
    }
    if ((hitByWampa || Q_irand(0, 100) < damage) // hit by wampa, hit while holding live victim, or took a lot of damage
        && self->client->ps.legsAnim != animNumber_t::BOTH_GESTURE1 && self->client->ps.legsAnim != animNumber_t::BOTH_GESTURE2 &&
        TIMER_Done(self, "takingPain")) {
        if (!Wampa_CheckRoar(self)) {
            if (self->client->ps.legsAnim != animNumber_t::BOTH_ATTACK1 && self->client->ps.legsAnim != animNumber_t::BOTH_ATTACK2 &&
                self->client->ps.legsAnim != animNumber_t::BOTH_ATTACK3) { // cant interrupt one of the big attack anims
                if (self->health > 100 || hitByWampa) {
                    TIMER_Remove(self, "attacking");

                    VectorCopy(self->NPC->lastPathAngles, self->s.angles);

                    if (!Q_irand(0, 1)) {
                        NPC_SetAnim(self, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_PAIN2,
                                    setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                    }
                    else {
                        NPC_SetAnim(self, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_PAIN1,
                                    setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                    }
                    TIMER_Set(self, "takingPain", self->client->ps.legsTimer + Q_irand(0, 500));
                    // allow us to re-evaluate our running speed/anim
                    TIMER_Set(self, "runfar", -1);
                    TIMER_Set(self, "runclose", -1);
                    TIMER_Set(self, "walk", -1);

                    if (self->NPC) {
                        NPC_SetLocalState(*self->NPC, monsterLocalState_t::LSTATE_WAITING);
                    }
                }
            }
        }
    }
}

/*
-------------------------
NPC_BSWampa_Default
-------------------------
*/
void NPC_BSWampa_Default(void)
{

    NPCS.NPC->client->ps.eFlags2 &= ~entityFlags2_t::EF2_USE_ALT_ANIM;
    // NORMAL ANIMS
    //	stand1 = normal stand
    //	walk1 = normal, non-angry walk
    //	walk2 = injured
    //	run1 = far away run
    //	run2 = close run
    // VICTIM ANIMS
    //	grabswipe = melee1 - sweep out and grab
    //	stand2 attack = attack4 - while holding victim, swipe at him
    //	walk3_drag = walk5 - walk with drag
    //	stand2 = hold victim
    //	stand2to1 = drop victim
    if (!TIMER_Done(NPCS.NPC, "rageTime")) { // do nothing but roar first time we see an enemy
        NPC_FaceEnemy(true);
        return;
    }
    if (NPCS.NPC->enemy) {
        if (!TIMER_Done(NPCS.NPC, "attacking")) { // in middle of attack
            // face enemy
            NPC_FaceEnemy(true);
            // continue attack logic
            enemyDist = Distance(NPCS.NPC->r.currentOrigin, NPCS.NPC->enemy->r.currentOrigin);
            Wampa_Attack(enemyDist, false);
            return;
        }
        else {
            if (TIMER_Done(NPCS.NPC, "angrynoise")) {
                G_Sound(NPCS.NPC, sound::SoundClass::Voice, sound::Slot::Voice,
                        G_SoundIndex(Q_Format("sound/chars/wampa/misc/anger{}.wav", Q_irand(1, 2)).c_str()));

                TIMER_Set(NPCS.NPC, "angrynoise", Q_irand(5000, 10000));
            }
            // else, if he's in our hand, we eat, else if he's on the ground, we keep attacking his dead body for a while
            if (NPCS.NPC->enemy->client && NPCS.NPC->enemy->client->NPC_class == class_t::CLASS_WAMPA) { // got mad at another Wampa, look for a valid enemy
                if (TIMER_Done(NPCS.NPC, "wampaInfight")) {
                    NPC_CheckEnemyExt(true);
                }
            }
            else {
                if (ValidEnemy(NPCS.NPC->enemy) == false) {
                    TIMER_Remove(NPCS.NPC, "lookForNewEnemy"); // make them look again right now
                    if (!NPCS.NPC->enemy->inuse ||
                        level.time - NPCS.NPC->enemy->s.time >
                            Q_irand(10000, 15000)) { // it's been a while since the enemy died, or enemy is completely gone, get bored with him
                        NPCS.NPC->enemy = nullptr;
                        Wampa_Patrol();
                        NPC_UpdateAngles(true, true);
                        // just lost my enemy
                        if ((NPCS.NPC->spawnflags & 2)) { // search around me if I don't have an enemy
                            NPC_BSSearchStart(NPCS.NPC->waypoint, bState_t::BS_SEARCH);
                            NPCS.NPCInfo->tempBehavior = bState_t::BS_DEFAULT;
                        }
                        else if ((NPCS.NPC->spawnflags & 1)) { // wander if I don't have an enemy
                            NPC_BSSearchStart(NPCS.NPC->waypoint, bState_t::BS_WANDER);
                            NPCS.NPCInfo->tempBehavior = bState_t::BS_DEFAULT;
                        }
                        return;
                    }
                }
                if (TIMER_Done(NPCS.NPC, "lookForNewEnemy")) {
                    gentity_t *newEnemy, *sav_enemy = NPCS.NPC->enemy; // FIXME: what about NPC->lastEnemy?
                    NPCS.NPC->enemy = nullptr;
                    newEnemy = NPC_CheckEnemy(NPCS.NPCInfo->confusionTime < level.time, false, false);
                    NPCS.NPC->enemy = sav_enemy;
                    if (newEnemy && newEnemy != sav_enemy) { // picked up a new enemy!
                        NPCS.NPC->lastEnemy = NPCS.NPC->enemy;
                        G_SetEnemy(NPCS.NPC, newEnemy);
                        // hold this one for at least 5-15 seconds
                        TIMER_Set(NPCS.NPC, "lookForNewEnemy", Q_irand(5000, 15000));
                    }
                    else { // look again in 2-5 secs
                        TIMER_Set(NPCS.NPC, "lookForNewEnemy", Q_irand(2000, 5000));
                    }
                }
            }
            Wampa_Combat();
            return;
        }
    }
    else {
        if (TIMER_Done(NPCS.NPC, "idlenoise")) {
            G_Sound(NPCS.NPC, sound::SoundClass::World, sound::Slot::None, G_SoundIndex("sound/chars/wampa/misc/anger3.wav"));

            TIMER_Set(NPCS.NPC, "idlenoise", Q_irand(2000, 4000));
        }
        if ((NPCS.NPC->spawnflags & 2)) {                // search around me if I don't have an enemy
            if (NPCS.NPCInfo->homeWp == WAYPOINT_NONE) { // no homewap, initialize the search behavior
                NPC_BSSearchStart(WAYPOINT_NONE, bState_t::BS_SEARCH);
                NPCS.NPCInfo->tempBehavior = bState_t::BS_DEFAULT;
            }
            NPCS.ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
            NPC_BSSearch(); // this automatically looks for enemies
        }
        else if ((NPCS.NPC->spawnflags & 1)) {           // wander if I don't have an enemy
            if (NPCS.NPCInfo->homeWp == WAYPOINT_NONE) { // no homewap, initialize the wander behavior
                NPC_BSSearchStart(WAYPOINT_NONE, bState_t::BS_WANDER);
                NPCS.NPCInfo->tempBehavior = bState_t::BS_DEFAULT;
            }
            NPCS.ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
            NPC_BSWander();
            if (Any(NPCS.NPCInfo->scriptFlags & scriptFlags_t::SCF_LOOK_FOR_ENEMIES)) {
                if (NPC_CheckEnemyExt(true) == false) {
                    Wampa_Idle();
                }
                else {
                    Wampa_CheckRoar(NPCS.NPC);
                    TIMER_Set(NPCS.NPC, "lookForNewEnemy", Q_irand(5000, 15000));
                }
            }
        }
        else {
            if (Any(NPCS.NPCInfo->scriptFlags & scriptFlags_t::SCF_LOOK_FOR_ENEMIES)) {
                Wampa_Patrol();
            }
            else {
                Wampa_Idle();
            }
        }
    }

    NPC_UpdateAngles(true, true);
}

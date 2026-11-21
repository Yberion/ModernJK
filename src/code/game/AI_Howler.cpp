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
#include <cgame/cg_camera.h>

// These define the working combat range for these suckers
constexpr int MIN_DISTANCE = 54;

constexpr int MAX_DISTANCE = 128;
constexpr int MAX_DISTANCE_SQR = MAX_DISTANCE * MAX_DISTANCE;

enum class howlerLocalState_t {
    LSTATE_CLEAR = 0,
    LSTATE_WAITING,
    LSTATE_FLEE,
    LSTATE_BERZERK,
};

constexpr float HOWLER_RETREAT_DIST = 300.0f;
constexpr int HOWLER_PANIC_HEALTH = 10;

extern void G_UcmdMoveForDir(gentity_t* self, usercmd_t* cmd, vec3_t dir);
extern void G_GetBoltPosition(gentity_t* self, int boltIndex, vec3_t pos, int modelIndex = 0);
extern int PM_AnimLength(int index, animNumber_t anim);
extern bool NAV_DirSafe(gentity_t* self, vec3_t dir, float dist);
extern void G_Knockdown(gentity_t* self, gentity_t* attacker, const vec3_t pushDir, float strength, bool breakSaberLock);
extern float NPC_EntRangeFromBolt(gentity_t* targEnt, int boltIndex);
extern int NPC_GetEntsNearBolt(gentity_t** radiusEnts, float radius, int boltIndex, vec3_t boltOrg);
extern bool PM_InKnockDown(playerState_t* ps);
extern bool PM_HasAnimation(gentity_t* ent, animNumber_t animation);

static void Howler_Attack(float enemyDist, bool howl = false);
/*
-------------------------
NPC_Howler_Precache
-------------------------
*/
void NPC_Howler_Precache(void)
{
    int i;
    // G_SoundIndex( "sound/chars/howler/howl.mp3" );
    G_EffectIndex("howler/sonic");
    G_SoundIndex("sound/chars/howler/howl.mp3");
    for (i = 1; i < 3; i++) {
        G_SoundIndex(va("sound/chars/howler/idle_hiss%d.mp3", i));
    }
    for (i = 1; i < 6; i++) {
        G_SoundIndex(va("sound/chars/howler/howl_talk%d.mp3", i));
        G_SoundIndex(va("sound/chars/howler/howl_yell%d.mp3", i));
    }
}

void Howler_ClearTimers(gentity_t* self)
{
    // clear all my timers
    TIMER_Set(self, "flee", -level.time);
    TIMER_Set(self, "retreating", -level.time);
    TIMER_Set(self, "standing", -level.time);
    TIMER_Set(self, "walking", -level.time);
    TIMER_Set(self, "running", -level.time);
    TIMER_Set(self, "aggressionDecay", -level.time);
    TIMER_Set(self, "speaking", -level.time);
}

static bool NPC_Howler_Move(int randomJumpChance = 0)
{
    if (!TIMER_Done(NPC, "standing")) { // standing around
        return false;
    }
    if (NPC->client->ps.groundEntityNum == ENTITYNUM_NONE) { // in air, don't do anything
        return false;
    }
    if ((!NPC->enemy && TIMER_Done(NPC, "running")) || !TIMER_Done(NPC, "walking")) {
        ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
    }
    if ((!randomJumpChance || Q_irand(0, randomJumpChance)) && NPC_MoveToGoal(true)) {
        if (VectorCompare(NPC->client->ps.moveDir, vec3_origin) || !NPC->client->ps.speed) { // uh.... wtf?  Got there?
            if (NPCInfo->goalEntity) {
                NPC_FaceEntity(NPCInfo->goalEntity, false);
            }
            else {
                NPC_UpdateAngles(false, true);
            }
            return true;
        }
        // TEMP: don't want to strafe
        VectorClear(NPC->client->ps.moveDir);
        ucmd.rightmove = 0.0f;
        //		Com_Printf( "Howler moving %d\n",ucmd.forwardmove );
        // if backing up, go slow...
        if (ucmd.forwardmove < 0.0f) {
            ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
            // if ( NPC->client->ps.speed > NPCInfo->stats.walkSpeed )
            { // don't walk faster than I'm allowed to
                NPC->client->ps.speed = NPCInfo->stats.walkSpeed;
            }
        }
        else {
            if (Any(ucmd.buttons & buttonFlags_t::BUTTON_WALKING)) {
                NPC->client->ps.speed = NPCInfo->stats.walkSpeed;
            }
            else {
                NPC->client->ps.speed = NPCInfo->stats.runSpeed;
            }
        }
        NPCInfo->lockedDesiredYaw = NPCInfo->desiredYaw = NPCInfo->lastPathAngles[YAW];
        NPC_UpdateAngles(false, true);
    }
    else if (NPCInfo->goalEntity) { // couldn't get where we wanted to go, try to jump there
        NPC_FaceEntity(NPCInfo->goalEntity, false);
        NPC_TryJump(NPCInfo->goalEntity, 400.0f, -256.0f);
    }
    return true;
}
/*
-------------------------
Howler_Idle
-------------------------
*/
static void Howler_Idle(void) {}

/*
-------------------------
Howler_Patrol
-------------------------
*/
static void Howler_Patrol(void)
{
    NPC_SetLocalState(*NPCInfo, howlerLocalState_t::LSTATE_CLEAR);

    // If we have somewhere to go, then do that
    if (UpdateGoal()) {
        NPC_Howler_Move(100);
    }

    vec3_t dif;
    VectorSubtract(g_entities[0].currentOrigin, NPC->currentOrigin, dif);

    if (VectorLengthSquared(dif) < 256 * 256) {
        G_SetEnemy(NPC, &g_entities[0]);
    }

    if (NPC_CheckEnemyExt(true) == false) {
        Howler_Idle();
        return;
    }

    Howler_Attack(0.0f, true);
}

/*
-------------------------
Howler_Move
-------------------------
*/
static bool Howler_Move(bool visible)
{
    if (NPC_LocalState<howlerLocalState_t>(*NPCInfo) != howlerLocalState_t::LSTATE_WAITING) {
        NPCInfo->goalEntity = NPC->enemy;
        NPCInfo->goalRadius = MAX_DISTANCE; // just get us within combat range
        return NPC_Howler_Move(30);
    }
    return false;
}

//---------------------------------------------------------
static void Howler_TryDamage(int damage, bool tongue, bool knockdown)
{
    vec3_t start, end, dir;
    trace_t tr;

    if (tongue) {
        G_GetBoltPosition(NPC, NPC->genericBolt1, start);
        G_GetBoltPosition(NPC, NPC->genericBolt2, end);
        VectorSubtract(end, start, dir);
        float dist = VectorNormalize(dir);
        VectorMA(start, dist + 16, dir, end);
    }
    else {
        VectorCopy(NPC->currentOrigin, start);
        AngleVectors(NPC->currentAngles, dir, nullptr, nullptr);
        VectorMA(start, MIN_DISTANCE * 2, dir, end);
    }

#ifndef FINAL_BUILD
    if (d_saberCombat->integer > 1) {
        G_DebugLine(start, end, 1000, 0x000000ff, true);
    }
#endif
    // Should probably trace from the mouth, but, ah well.
    gi.trace(&tr, start, vec3_origin, vec3_origin, end, NPC->s.number, MASK_SHOT, EG2_Collision::G2_NOCOLLIDE, 0);

    if (tr.entityNum < ENTITYNUM_WORLD) { // hit *something*
        gentity_t* victim = &g_entities[tr.entityNum];
        if (!victim->client || victim->client->NPC_class != class_t::CLASS_HOWLER) { // not another howler

            if (knockdown && victim->client) { // only do damage if victim isn't knocked down.  If he isn't, knock him down
                if (PM_InKnockDown(&victim->client->ps)) {
                    return;
                }
            }
            // FIXME: some sort of damage effect (claws and tongue are cutting you... blood?)
            G_Damage(victim, NPC, NPC, dir, tr.endpos, damage, damageFlags_t::DAMAGE_NO_KNOCKBACK, meansOfDeath_t::MOD_MELEE);
            if (knockdown && victim->health > 0) { // victim still alive
                G_Knockdown(victim, NPC, NPC->client->ps.velocity, 500, false);
            }
        }
    }
}

static void Howler_Howl(void)
{
    gentity_t* radiusEnts[128];
    int numEnts;
    const float radius = (NPC->spawnflags & 1) ? 256 : 128;
    const float halfRadSquared = ((radius / 2) * (radius / 2));
    const float radiusSquared = (radius * radius);
    float distSq;
    int i;
    vec3_t boltOrg;

    AddSoundEvent(NPC, NPC->currentOrigin, 512, alertEventLevel_e::AEL_DANGER, false, true);

    numEnts = NPC_GetEntsNearBolt(radiusEnts, radius, NPC->handLBolt, boltOrg);

    for (i = 0; i < numEnts; i++) {
        if (!radiusEnts[i]->inuse) {
            continue;
        }

        if (radiusEnts[i] == NPC) { // Skip the rancor ent
            continue;
        }

        if (radiusEnts[i]->client == nullptr) { // must be a client
            continue;
        }

        if (radiusEnts[i]->client->NPC_class == class_t::CLASS_HOWLER) { // other howlers immune
            continue;
        }

        distSq = DistanceSquared(radiusEnts[i]->currentOrigin, boltOrg);
        if (distSq <= radiusSquared) {
            if (distSq < halfRadSquared) {            // close enough to do damage, too
                if (Q_irand(0, g_spskill->integer)) { // does no damage on easy, does 1 point every other frame on medium, more often on hard
                    G_Damage(radiusEnts[i], NPC, NPC, vec3_origin, NPC->currentOrigin, 1, damageFlags_t::DAMAGE_NO_KNOCKBACK, meansOfDeath_t::MOD_IMPACT);
                }
            }
            if (radiusEnts[i]->health > 0 && radiusEnts[i]->client && radiusEnts[i]->client->NPC_class != class_t::CLASS_RANCOR &&
                radiusEnts[i]->client->NPC_class != class_t::CLASS_ATST && !PM_InKnockDown(&radiusEnts[i]->client->ps)) {
                if (PM_HasAnimation(radiusEnts[i], animNumber_t::BOTH_SONICPAIN_START)) {
                    if (radiusEnts[i]->client->ps.torsoAnim != animNumber_t::BOTH_SONICPAIN_START &&
                        radiusEnts[i]->client->ps.torsoAnim != animNumber_t::BOTH_SONICPAIN_HOLD) {
                        NPC_SetAnim(radiusEnts[i], setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_SONICPAIN_START, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        NPC_SetAnim(radiusEnts[i], setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_SONICPAIN_START,
                                    setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                        radiusEnts[i]->client->ps.torsoAnimTimer += 100;
                        radiusEnts[i]->client->ps.weaponTime = radiusEnts[i]->client->ps.torsoAnimTimer;
                    }
                    else if (radiusEnts[i]->client->ps.torsoAnimTimer <= 100) { // at the end of the sonic pain start or hold anim
                        NPC_SetAnim(radiusEnts[i], setAnimParts_t::SETANIM_LEGS, animNumber_t::BOTH_SONICPAIN_HOLD, setAnimFlags_t::SETANIM_FLAG_NORMAL);
                        NPC_SetAnim(radiusEnts[i], setAnimParts_t::SETANIM_TORSO, animNumber_t::BOTH_SONICPAIN_HOLD,
                                    setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                        radiusEnts[i]->client->ps.torsoAnimTimer += 100;
                        radiusEnts[i]->client->ps.weaponTime = radiusEnts[i]->client->ps.torsoAnimTimer;
                    }
                }
                /*
                else if ( distSq < halfRadSquared
                    && radiusEnts[i]->client->ps.groundEntityNum != ENTITYNUM_NONE
                    && !Q_irand( 0, 10 ) )//FIXME: base on skill
                {//within range
                    G_Knockdown( radiusEnts[i], NPC, vec3_origin, 500, false );
                }
                */
            }
        }
    }

    float playerDist = NPC_EntRangeFromBolt(player, NPC->genericBolt1);
    if (playerDist < 256.0f) {
        CGCam_Shake(1.0f * playerDist / 128.0f, 200);
    }
}

//------------------------------
static void Howler_Attack(float enemyDist, bool howl)
{
    int dmg = (NPC_LocalState<howlerLocalState_t>(*NPCInfo) == howlerLocalState_t::LSTATE_BERZERK) ? 5 : 2;

    if (!TIMER_Exists(NPC, "attacking")) {
        animNumber_t attackAnim = animNumber_t::BOTH_GESTURE1;
        // Going to do an attack
        if (NPC->enemy && NPC->enemy->client && PM_InKnockDown(&NPC->enemy->client->ps) && enemyDist <= MIN_DISTANCE) {
            attackAnim = animNumber_t::BOTH_ATTACK2;
        }
        else if (!Q_irand(0, 4) || howl) { // howl attack
                                           // G_SoundOnEnt( NPC, soundChannel_t::CHAN_VOICE, "sound/chars/howler/howl.mp3" );
        }
        else if (enemyDist > MIN_DISTANCE && Q_irand(0, 1)) { // lunge attack
            // jump foward
            vec3_t fwd, yawAng = {0, NPC->client->ps.viewangles[YAW], 0};
            AngleVectors(yawAng, fwd, nullptr, nullptr);
            VectorScale(fwd, (enemyDist * 3.0f), NPC->client->ps.velocity);
            NPC->client->ps.velocity[2] = 200;
            NPC->client->ps.groundEntityNum = ENTITYNUM_NONE;

            attackAnim = animNumber_t::BOTH_ATTACK1;
        }
        else { // tongue attack
            attackAnim = animNumber_t::BOTH_ATTACK2;
        }

        NPC_SetAnim(NPC, setAnimParts_t::SETANIM_BOTH, attackAnim,
                    setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD | setAnimFlags_t::SETANIM_FLAG_RESTART);
        if (NPC_LocalState<howlerLocalState_t>(*NPCInfo) == howlerLocalState_t::LSTATE_BERZERK) { // attack again right away
            TIMER_Set(NPC, "attacking", NPC->client->ps.legsAnimTimer);
        }
        else {
            TIMER_Set(NPC, "attacking", NPC->client->ps.legsAnimTimer + Q_irand(0, 1500)); // FIXME: base on skill
            TIMER_Set(NPC, "standing", -level.time);
            TIMER_Set(NPC, "walking", -level.time);
            TIMER_Set(NPC, "running", NPC->client->ps.legsAnimTimer + 5000);
        }

        TIMER_Set(NPC, "attack_dmg", 200); // level two damage
    }

    // Need to do delayed damage since the attack animations encapsulate multiple mini-attacks
    switch (NPC->client->ps.legsAnim) {
    case animNumber_t::BOTH_ATTACK1:
    case animNumber_t::BOTH_MELEE1:
        if (NPC->client->ps.legsAnimTimer > 650 // more than 13 frames left
            && PM_AnimLength(NPC->client->clientInfo.animFileIndex, NPC->client->ps.legsAnim) - NPC->client->ps.legsAnimTimer >=
                   800) // at least 16 frames into anim
        {
            Howler_TryDamage(dmg, false, false);
        }
        break;
    case animNumber_t::BOTH_ATTACK2:
    case animNumber_t::BOTH_MELEE2:
        if (NPC->client->ps.legsAnimTimer > 350 // more than 7 frames left
            && PM_AnimLength(NPC->client->clientInfo.animFileIndex, NPC->client->ps.legsAnim) - NPC->client->ps.legsAnimTimer >=
                   550) // at least 11 frames into anim
        {
            Howler_TryDamage(dmg, true, false);
        }
        break;
    case animNumber_t::BOTH_GESTURE1: {
        if (NPC->client->ps.legsAnimTimer > 1800 // more than 36 frames left
            && PM_AnimLength(NPC->client->clientInfo.animFileIndex, NPC->client->ps.legsAnim) - NPC->client->ps.legsAnimTimer >=
                   950) // at least 19 frames into anim
        {
            Howler_Howl();
            if (!NPC->count) {
                G_PlayEffect(G_EffectIndex("howler/sonic"), NPC->playerModel, NPC->genericBolt1, NPC->s.number, NPC->currentOrigin, 4750, true);
                G_SoundOnEnt(NPC, soundChannel_t::CHAN_VOICE, "sound/chars/howler/howl.mp3");
                NPC->count = 1;
            }
        }
    } break;
    default:
        // anims seem to get reset after a load, so just stop attacking and it will restart as needed.
        TIMER_Remove(NPC, "attacking");
        break;
    }

    // Just using this to remove the attacking flag at the right time
    TIMER_Done2(NPC, "attacking", true);
}

//----------------------------------
static void Howler_Combat(void)
{
    bool faced = false;
    float distance;
    bool advance = false;
    if (NPC->client->ps.groundEntityNum == ENTITYNUM_NONE) { // not on the ground
        if (NPC->client->ps.legsAnim == animNumber_t::BOTH_JUMP1 ||
            NPC->client->ps.legsAnim == animNumber_t::BOTH_INAIR1) { // flying through the air with the greatest of ease, etc
            Howler_TryDamage(10, false, false);
        }
    }
    else { // not in air, see if we should attack or advance
        // If we cannot see our target or we have somewhere to go, then do that
        if (!NPC_ClearLOS(NPC->enemy)) //|| UpdateGoal( ))
        {
            NPCInfo->goalEntity = NPC->enemy;
            NPCInfo->goalRadius = MAX_DISTANCE; // just get us within combat range

            if (NPC_LocalState<howlerLocalState_t>(*NPCInfo) == howlerLocalState_t::LSTATE_BERZERK) {
                NPC_Howler_Move(3);
            }
            else {
                NPC_Howler_Move(10);
            }
            NPC_UpdateAngles(false, true);
            return;
        }

        distance = DistanceHorizontal(NPC->currentOrigin, NPC->enemy->currentOrigin);

        if (NPC->enemy && NPC->enemy->client && PM_InKnockDown(&NPC->enemy->client->ps)) { // get really close to knocked down enemies
            advance = (distance > MIN_DISTANCE ? true : false);
        }
        else {
            advance = (distance > MAX_DISTANCE ? true : false); // MIN_DISTANCE
        }

        if ((advance || NPC_LocalState<howlerLocalState_t>(*NPCInfo) == howlerLocalState_t::LSTATE_WAITING) &&
            TIMER_Done(NPC, "attacking")) // waiting monsters can't attack
        {
            if (TIMER_Done2(NPC, "takingPain", true)) {
                NPC_SetLocalState(*NPCInfo, howlerLocalState_t::LSTATE_CLEAR);
            }
            else if (TIMER_Done(NPC, "standing")) {
                faced = Howler_Move(true);
            }
        }
        else {
            Howler_Attack(distance);
        }
    }

    if (!faced) {
        if (                              // TIMER_Done( NPC, "standing" ) //not just standing there
                                          //! advance //not moving
            TIMER_Done(NPC, "attacking")) // not attacking
        {                                 // not standing around
            // Sometimes I have problems with facing the enemy I'm attacking, so force the issue so I don't look dumb
            NPC_FaceEnemy(true);
        }
        else {
            NPC_UpdateAngles(false, true);
        }
    }
}

/*
-------------------------
NPC_Howler_Pain
-------------------------
*/
void NPC_Howler_Pain(gentity_t* self, gentity_t* inflictor, gentity_t* other, const vec3_t point, int damage, meansOfDeath_t mod, hitLocation_t hitLoc)
{
    if (!self || !self->NPC) {
        return;
    }

    if (NPC_LocalState<howlerLocalState_t>(*self->NPC) != howlerLocalState_t::LSTATE_BERZERK) // damage >= 10 )
    {
        self->NPC->stats.aggression += damage;
        NPC_SetLocalState(*self->NPC, howlerLocalState_t::LSTATE_WAITING);

        TIMER_Remove(self, "attacking");

        VectorCopy(self->NPC->lastPathAngles, self->s.angles);

        // if ( self->client->ps.legsAnim == animNumber_t::BOTH_GESTURE1 )
        {
            G_StopEffect(G_EffectIndex("howler/sonic"), self->playerModel, self->genericBolt1, self->s.number);
        }

        NPC_SetAnim(self, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_PAIN1, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
        TIMER_Set(self, "takingPain", self->client->ps.legsAnimTimer); // 2900 );

        if (self->health > HOWLER_PANIC_HEALTH) {            // still have some health left
            if (Q_irand(0, self->max_health) > self->health) // FIXME: or check damage?
            {                                                // back off!
                TIMER_Set(self, "standing", -level.time);
                TIMER_Set(self, "running", -level.time);
                TIMER_Set(self, "walking", -level.time);
                TIMER_Set(self, "retreating", Q_irand(1000, 5000));
            }
            else { // go after him!
                TIMER_Set(self, "standing", -level.time);
                TIMER_Set(self, "running", self->client->ps.legsAnimTimer + Q_irand(3000, 6000));
                TIMER_Set(self, "walking", -level.time);
                TIMER_Set(self, "retreating", -level.time);
            }
        }
        else if (self->NPC) {    // panic!
            if (Q_irand(0, 1)) { // berzerk
                NPC_SetLocalState(*self->NPC, howlerLocalState_t::LSTATE_BERZERK);
            }
            else { // flee
                NPC_SetLocalState(*self->NPC, howlerLocalState_t::LSTATE_FLEE);
                TIMER_Set(self, "flee", Q_irand(10000, 30000));
            }
        }
    }
}

/*
-------------------------
NPC_BSHowler_Default
-------------------------
*/
void NPC_BSHowler_Default(void)
{
    if (NPC->client->ps.legsAnim != animNumber_t::BOTH_GESTURE1) {
        NPC->count = 0;
    }
    // FIXME: if in jump, do damage in front and maybe knock them down?
    if (!TIMER_Done(NPC, "attacking")) {
        if (NPC->enemy) {
            // NPC_FaceEnemy( false );
            Howler_Attack(Distance(NPC->enemy->currentOrigin, NPC->currentOrigin));
        }
        else {
            // NPC_UpdateAngles( false, true );
            Howler_Attack(0.0f);
        }
        NPC_UpdateAngles(false, true);
        return;
    }

    if (NPC->enemy) {
        if (NPCInfo->stats.aggression > 0) {
            if (TIMER_Done(NPC, "aggressionDecay")) {
                NPCInfo->stats.aggression--;
                TIMER_Set(NPC, "aggressionDecay", 500);
            }
        }
        if (!TIMER_Done(NPC, "flee") && NPC_BSFlee()) // this can clear ENEMY
        {                                             // successfully trying to run away
            return;
        }
        if (NPC->enemy == nullptr) {
            NPC_UpdateAngles(false, true);
            return;
        }
        if (NPC_LocalState<howlerLocalState_t>(*NPCInfo) ==
            howlerLocalState_t::LSTATE_FLEE) { // we were fleeing, now done (either timer ran out or we cannot flee anymore
            if (NPC_ClearLOS(NPC->enemy)) {    // if enemy is still around, go berzerk
                NPC_SetLocalState(*NPCInfo, howlerLocalState_t::LSTATE_BERZERK);
            }
            else { // otherwise, lick our wounds?
                NPC_SetLocalState(*NPCInfo, howlerLocalState_t::LSTATE_CLEAR);
                TIMER_Set(NPC, "standing", Q_irand(3000, 10000));
            }
        }
        else if (NPC_LocalState<howlerLocalState_t>(*NPCInfo) == howlerLocalState_t::LSTATE_BERZERK) { // go nuts!
        }
        else if (NPCInfo->stats.aggression >= Q_irand(75, 125)) { // that's it, go nuts!
            NPC_SetLocalState(*NPCInfo, howlerLocalState_t::LSTATE_BERZERK);
        }
        else if (!TIMER_Done(NPC, "retreating")) { // trying to back off
            NPC_FaceEnemy(true);
            if (NPC->client->ps.speed > NPCInfo->stats.walkSpeed) {
                NPC->client->ps.speed = NPCInfo->stats.walkSpeed;
            }
            ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
            if (Distance(NPC->enemy->currentOrigin, NPC->currentOrigin) < HOWLER_RETREAT_DIST) { // enemy is close
                vec3_t moveDir;
                AngleVectors(NPC->currentAngles, moveDir, nullptr, nullptr);
                VectorScale(moveDir, -1, moveDir);
                if (!NAV_DirSafe(NPC, moveDir, 8)) { // enemy is backing me up against a wall or ledge!  Start to get really mad!
                    NPCInfo->stats.aggression += 2;
                }
                else { // back off
                    ucmd.forwardmove = -127;
                }
                // enemy won't leave me alone, get mad...
                NPCInfo->stats.aggression++;
            }
            return;
        }
        else if (TIMER_Done(NPC, "standing")) {                                         // not standing around
            if (!(NPCInfo->last_ucmd.forwardmove) && !(NPCInfo->last_ucmd.rightmove)) { // stood last frame
                if (TIMER_Done(NPC, "walking") && TIMER_Done(NPC, "running")) {         // not walking or running
                    if (Q_irand(0, 2)) {                                                // run for a while
                        TIMER_Set(NPC, "walking", Q_irand(4000, 8000));
                    }
                    else { // walk for a bit
                        TIMER_Set(NPC, "running", Q_irand(2500, 5000));
                    }
                }
            }
            else if (Any(NPCInfo->last_ucmd.buttons & buttonFlags_t::BUTTON_WALKING)) {                                       // walked last frame
                if (TIMER_Done(NPC, "walking")) {                                                                             // just finished walking
                    if (Q_irand(0, 5) || DistanceSquared(NPC->enemy->currentOrigin, NPC->currentOrigin) < MAX_DISTANCE_SQR) { // run for a while
                        TIMER_Set(NPC, "running", Q_irand(4000, 20000));
                    }
                    else { // stand for a bit
                        TIMER_Set(NPC, "standing", Q_irand(2000, 6000));
                    }
                }
            }
            else {                                                                                                            // ran last frame
                if (TIMER_Done(NPC, "running")) {                                                                             // just finished running
                    if (Q_irand(0, 8) || DistanceSquared(NPC->enemy->currentOrigin, NPC->currentOrigin) < MAX_DISTANCE_SQR) { // walk for a while
                        TIMER_Set(NPC, "walking", Q_irand(3000, 10000));
                    }
                    else { // stand for a bit
                        TIMER_Set(NPC, "standing", Q_irand(2000, 6000));
                    }
                }
            }
        }
        if (NPC_ValidEnemy(NPC->enemy) == false) {
            TIMER_Remove(NPC, "lookForNewEnemy"); // make them look again right now
            if (!NPC->enemy->inuse || level.time - NPC->enemy->s.time >
                                          Q_irand(10000, 15000)) { // it's been a while since the enemy died, or enemy is completely gone, get bored with him
                NPC->enemy = nullptr;
                Howler_Patrol();
                NPC_UpdateAngles(true, true);
                return;
            }
        }
        if (TIMER_Done(NPC, "lookForNewEnemy")) {
            gentity_t* sav_enemy = NPC->enemy; // FIXME: what about NPC->lastEnemy?
            NPC->enemy = nullptr;
            gentity_t* newEnemy = NPC_CheckEnemy((NPCInfo->confusionTime < level.time), false, false);
            NPC->enemy = sav_enemy;
            if (newEnemy && newEnemy != sav_enemy) { // picked up a new enemy!
                NPC->lastEnemy = NPC->enemy;
                G_SetEnemy(NPC, newEnemy);
                if (NPC->enemy != NPC->lastEnemy) { // clear this so that we only sniff the player the first time we pick them up
                    NPC->useDebounceTime = 0;
                }
                // hold this one for at least 5-15 seconds
                TIMER_Set(NPC, "lookForNewEnemy", Q_irand(5000, 15000));
            }
            else { // look again in 2-5 secs
                TIMER_Set(NPC, "lookForNewEnemy", Q_irand(2000, 5000));
            }
        }
        Howler_Combat();
        if (TIMER_Done(NPC, "speaking")) {
            if (!TIMER_Done(NPC, "standing") || !TIMER_Done(NPC, "retreating")) {
                G_SoundOnEnt(NPC, soundChannel_t::CHAN_VOICE, va("sound/chars/howler/idle_hiss%d.mp3", Q_irand(1, 2)));
            }
            else if (!TIMER_Done(NPC, "walking") || NPC_LocalState<howlerLocalState_t>(*NPCInfo) == howlerLocalState_t::LSTATE_FLEE) {
                G_SoundOnEnt(NPC, soundChannel_t::CHAN_VOICE, va("sound/chars/howler/howl_talk%d.mp3", Q_irand(1, 5)));
            }
            else {
                G_SoundOnEnt(NPC, soundChannel_t::CHAN_VOICE, va("sound/chars/howler/howl_yell%d.mp3", Q_irand(1, 5)));
            }
            if (NPC_LocalState<howlerLocalState_t>(*NPCInfo) == howlerLocalState_t::LSTATE_BERZERK ||
                NPC_LocalState<howlerLocalState_t>(*NPCInfo) == howlerLocalState_t::LSTATE_FLEE) {
                TIMER_Set(NPC, "speaking", Q_irand(1000, 4000));
            }
            else {
                TIMER_Set(NPC, "speaking", Q_irand(3000, 8000));
            }
        }
        return;
    }
    else {
        if (TIMER_Done(NPC, "speaking")) {
            if (!Q_irand(0, 3)) {
                G_SoundOnEnt(NPC, soundChannel_t::CHAN_VOICE, va("sound/chars/howler/idle_hiss%d.mp3", Q_irand(1, 2)));
            }
            else {
                G_SoundOnEnt(NPC, soundChannel_t::CHAN_VOICE, va("sound/chars/howler/howl_talk%d.mp3", Q_irand(1, 5)));
            }
            TIMER_Set(NPC, "speaking", Q_irand(4000, 12000));
        }
        if (NPCInfo->stats.aggression > 0) {
            if (TIMER_Done(NPC, "aggressionDecay")) {
                NPCInfo->stats.aggression--;
                TIMER_Set(NPC, "aggressionDecay", 200);
            }
        }
        if (TIMER_Done(NPC, "standing")) {                                              // not standing around
            if (!(NPCInfo->last_ucmd.forwardmove) && !(NPCInfo->last_ucmd.rightmove)) { // stood last frame
                if (TIMER_Done(NPC, "walking") && TIMER_Done(NPC, "running")) {         // not walking or running
                    if (NPCInfo->goalEntity) {                                          // have somewhere to go
                        if (Q_irand(0, 2)) {                                            // walk for a while
                            TIMER_Set(NPC, "walking", Q_irand(3000, 10000));
                        }
                        else { // run for a bit
                            TIMER_Set(NPC, "running", Q_irand(2500, 5000));
                        }
                    }
                }
            }
            else if (Any(NPCInfo->last_ucmd.buttons & buttonFlags_t::BUTTON_WALKING)) { // walked last frame
                if (TIMER_Done(NPC, "walking")) {                                       // just finished walking
                    if (Q_irand(0, 3)) {                                                // run for a while
                        TIMER_Set(NPC, "running", Q_irand(3000, 6000));
                    }
                    else { // stand for a bit
                        TIMER_Set(NPC, "standing", Q_irand(2500, 5000));
                    }
                }
            }
            else {                                // ran last frame
                if (TIMER_Done(NPC, "running")) { // just finished running
                    if (Q_irand(0, 2)) {          // walk for a while
                        TIMER_Set(NPC, "walking", Q_irand(6000, 15000));
                    }
                    else { // stand for a bit
                        TIMER_Set(NPC, "standing", Q_irand(4000, 6000));
                    }
                }
            }
        }
        if (Any(NPCInfo->scriptFlags & scriptFlags_t::SCF_LOOK_FOR_ENEMIES)) {
            Howler_Patrol();
        }
        else {
            Howler_Idle();
        }
    }

    NPC_UpdateAngles(false, true);
}

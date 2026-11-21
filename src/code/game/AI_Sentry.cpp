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
#include <cgame/cg_local.h>
#include "g_functions.h"

gentity_t* CreateMissile(vec3_t org, vec3_t dir, float vel, int life, gentity_t* owner, bool altFire = false);
extern gitem_t* FindItemForAmmo(ammo_t ammo);

constexpr int MIN_DISTANCE = 256;
constexpr int MIN_DISTANCE_SQR = MIN_DISTANCE * MIN_DISTANCE;

constexpr int SENTRY_FORWARD_BASE_SPEED = 10;
constexpr int SENTRY_FORWARD_MULTIPLIER = 5;

constexpr float SENTRY_VELOCITY_DECAY = 0.85f;
constexpr int SENTRY_STRAFE_VEL = 256;
constexpr int SENTRY_STRAFE_DIS = 200;
constexpr int SENTRY_UPWARD_PUSH = 32;
constexpr int SENTRY_HOVER_HEIGHT = 24;

// Local state enums
enum class sentryLocalState_t {
    LSTATE_NONE = 0,
    LSTATE_ASLEEP,
    LSTATE_WAKEUP,
    LSTATE_ACTIVE,
    LSTATE_POWERING_UP,
    LSTATE_ATTACKING,
};

/*
-------------------------
NPC_Sentry_Precache
-------------------------
*/
void NPC_Sentry_Precache(void)
{
    G_SoundIndex("sound/chars/sentry/misc/sentry_explo");
    G_SoundIndex("sound/chars/sentry/misc/sentry_pain");
    G_SoundIndex("sound/chars/sentry/misc/sentry_shield_open");
    G_SoundIndex("sound/chars/sentry/misc/sentry_shield_close");
    G_SoundIndex("sound/chars/sentry/misc/sentry_hover_1_lp");
    G_SoundIndex("sound/chars/sentry/misc/sentry_hover_2_lp");

    for (int i = 1; i < 4; i++) {
        G_SoundIndex(va("sound/chars/sentry/misc/talk%d", i));
    }

    G_EffectIndex("bryar/muzzle_flash");
    G_EffectIndex("env/med_explode");

    RegisterItem(FindItemForAmmo(ammo_t::AMMO_BLASTER));
}

/*
================
sentry_use
================
*/
void sentry_use(gentity_t* self, gentity_t* other, gentity_t* activator)
{
    G_ActivateBehavior(self, bSet_t::BSET_USE);

    self->flags &= ~gentityFlags_t::FL_SHIELDED;
    NPC_SetAnim(self, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_POWERUP1, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
    //	NPC_SetLocalState( *self->NPC, sentryLocalState_t::LSTATE_WAKEUP );
    NPC_SetLocalState(*self->NPC, sentryLocalState_t::LSTATE_ACTIVE);
}

/*
-------------------------
NPC_Sentry_Pain
-------------------------
*/
void NPC_Sentry_Pain(gentity_t* self, gentity_t* inflictor, gentity_t* other, const vec3_t point, int damage, meansOfDeath_t mod, hitLocation_t hitLoc)
{
    NPC_Pain(self, inflictor, other, point, damage, mod);

    if (mod == meansOfDeath_t::MOD_DEMP2 || mod == meansOfDeath_t::MOD_DEMP2_ALT) {
        self->NPC->burstCount = 0;
        TIMER_Set(self, "attackDelay", Q_irand(9000, 12000));
        self->flags |= gentityFlags_t::FL_SHIELDED;
        NPC_SetAnim(self, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_FLY_SHIELDED,
                    setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
        G_SoundOnEnt(self, soundChannel_t::CHAN_AUTO, "sound/chars/sentry/misc/sentry_pain");

        NPC_SetLocalState(*self->NPC, sentryLocalState_t::LSTATE_ACTIVE);
    }

    // You got hit, go after the enemy
    //	if (NPC_LocalState<sentryLocalState_t>( *self->NPC ) == sentryLocalState_t::LSTATE_ASLEEP)
    //	{
    //		G_Sound( self, G_SoundIndex("sound/chars/sentry/misc/shieldsopen.wav"));
    //
    //		self->flags &= ~gentityFlags_t::FL_SHIELDED;
    //		NPC_SetAnim( self, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_POWERUP1,
    // setAnimFlags_t::SETANIM_FLAG_OVERRIDE|setAnimFlags_t::SETANIM_FLAG_HOLD ); 		NPC_SetLocalState( *self->NPC, sentryLocalState_t::LSTATE_WAKEUP );
    //	}
}

/*
-------------------------
Sentry_Fire
-------------------------
*/
void Sentry_Fire(void)
{
    vec3_t muzzle;
    static vec3_t forward, vright, up;
    gentity_t* missile;
    mdxaBone_t boltMatrix;
    int bolt;

    NPC->flags &= ~gentityFlags_t::FL_SHIELDED;

    if (NPC_LocalState<sentryLocalState_t>(*NPCInfo) == sentryLocalState_t::LSTATE_POWERING_UP) {
        if (TIMER_Done(NPC, "powerup")) {
            NPC_SetLocalState(*NPCInfo, sentryLocalState_t::LSTATE_ATTACKING);
            NPC_SetAnim(NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_ATTACK1,
                        setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
        }
        else {
            // can't do anything right now
            return;
        }
    }
    else if (NPC_LocalState<sentryLocalState_t>(*NPCInfo) == sentryLocalState_t::LSTATE_ACTIVE) {
        NPC_SetLocalState(*NPCInfo, sentryLocalState_t::LSTATE_POWERING_UP);

        G_SoundOnEnt(NPC, soundChannel_t::CHAN_AUTO, "sound/chars/sentry/misc/sentry_shield_open");
        NPC_SetAnim(NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_POWERUP1, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
        TIMER_Set(NPC, "powerup", 250);
        return;
    }
    else if (NPC_LocalState<sentryLocalState_t>(*NPCInfo) != sentryLocalState_t::LSTATE_ATTACKING) {
        // bad because we are uninitialized
        NPC_SetLocalState(*NPCInfo, sentryLocalState_t::LSTATE_ACTIVE);
        return;
    }

    // Which muzzle to fire from?
    int which = NPCInfo->burstCount % 3;
    switch (which) {
    case 0:
        bolt = NPC->genericBolt1;
        break;
    case 1:
        bolt = NPC->genericBolt2;
        break;
    case 2:
    default:
        bolt = NPC->genericBolt3;
    }

    gi.G2API_GetBoltMatrix(NPC->ghoul2, NPC->playerModel, bolt, &boltMatrix, NPC->currentAngles, NPC->currentOrigin, (cg.time ? cg.time : level.time), nullptr,
                           NPC->s.modelScale);

    gi.G2API_GiveMeVectorFromMatrix(boltMatrix, Eorientations::ORIGIN, muzzle);

    AngleVectors(NPC->currentAngles, forward, vright, up);
    //	G_Sound( NPC, G_SoundIndex("sound/chars/sentry/misc/shoot.wav"));

    G_PlayEffect("bryar/muzzle_flash", muzzle, forward);

    missile = CreateMissile(muzzle, forward, 1600, 10000, NPC);

    missile->classname = "bryar_proj";
    missile->s.weapon = weapon_t::WP_BRYAR_PISTOL;

    missile->dflags = damageFlags_t::DAMAGE_DEATH_KNOCKBACK;
    missile->methodOfDeath = meansOfDeath_t::MOD_ENERGY;
    missile->clipmask = MASK_SHOT | contentsFlags_t::CONTENTS_LIGHTSABER;

    NPCInfo->burstCount++;
    NPC->attackDebounceTime = level.time + 50;
    missile->damage = 5;

    // now scale for difficulty
    if (g_spskill->integer == 0) {
        NPC->attackDebounceTime += 200;
        missile->damage = 1;
    }
    else if (g_spskill->integer == 1) {
        NPC->attackDebounceTime += 100;
        missile->damage = 3;
    }
}

/*
-------------------------
Sentry_MaintainHeight
-------------------------
*/
void Sentry_MaintainHeight(void)
{
    float dif;

    NPC->s.loopSound = G_SoundIndex("sound/chars/sentry/misc/sentry_hover_1_lp");

    // Update our angles regardless
    NPC_UpdateAngles(true, true);

    // If we have an enemy, we should try to hover at about enemy eye level
    if (NPC->enemy) {
        // Find the height difference
        dif = (NPC->enemy->currentOrigin[2] + NPC->enemy->maxs[2]) - NPC->currentOrigin[2];

        // cap to prevent dramatic height shifts
        if (Q_fabsf(dif) > 8) {
            if (Q_fabsf(dif) > SENTRY_HOVER_HEIGHT) {
                dif = (dif < 0 ? -24 : 24);
            }

            NPC->client->ps.velocity[2] = (NPC->client->ps.velocity[2] + dif) / 2;
        }
    }
    else {
        gentity_t* goal = nullptr;

        if (NPCInfo->goalEntity) // Is there a goal?
        {
            goal = NPCInfo->goalEntity;
        }
        else {
            goal = NPCInfo->lastGoalEntity;
        }

        if (goal) {
            dif = goal->currentOrigin[2] - NPC->currentOrigin[2];

            if (Q_fabsf(dif) > SENTRY_HOVER_HEIGHT) {
                ucmd.upmove = (ucmd.upmove < 0 ? -4 : 4);
            }
            else {
                if (NPC->client->ps.velocity[2]) {
                    NPC->client->ps.velocity[2] *= SENTRY_VELOCITY_DECAY;

                    if (Q_fabsf(NPC->client->ps.velocity[2]) < 2) {
                        NPC->client->ps.velocity[2] = 0;
                    }
                }
            }
        }
        // Apply friction to Z
        else if (NPC->client->ps.velocity[2]) {
            NPC->client->ps.velocity[2] *= SENTRY_VELOCITY_DECAY;

            if (Q_fabsf(NPC->client->ps.velocity[2]) < 1) {
                NPC->client->ps.velocity[2] = 0;
            }
        }
    }

    // Apply friction
    if (NPC->client->ps.velocity[0]) {
        NPC->client->ps.velocity[0] *= SENTRY_VELOCITY_DECAY;

        if (Q_fabsf(NPC->client->ps.velocity[0]) < 1) {
            NPC->client->ps.velocity[0] = 0;
        }
    }

    if (NPC->client->ps.velocity[1]) {
        NPC->client->ps.velocity[1] *= SENTRY_VELOCITY_DECAY;

        if (Q_fabsf(NPC->client->ps.velocity[1]) < 1) {
            NPC->client->ps.velocity[1] = 0;
        }
    }

    NPC_FaceEnemy(true);
}

/*
-------------------------
Sentry_Idle
-------------------------
*/
void Sentry_Idle(void)
{
    Sentry_MaintainHeight();

    // Is he waking up?
    if (NPC_LocalState<sentryLocalState_t>(*NPCInfo) == sentryLocalState_t::LSTATE_WAKEUP) {
        if (NPC->client->ps.torsoAnimTimer <= 0) {
            NPCInfo->scriptFlags |= scriptFlags_t::SCF_LOOK_FOR_ENEMIES;
            NPCInfo->burstCount = 0;
        }
    }
    else {
        NPC_SetAnim(NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_SLEEP1, setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
        NPC->flags |= gentityFlags_t::FL_SHIELDED;

        NPC_BSIdle();
    }
}

/*
-------------------------
Sentry_Strafe
-------------------------
*/
void Sentry_Strafe(void)
{
    int dir;
    vec3_t end, right;
    trace_t tr;

    AngleVectors(NPC->client->renderInfo.eyeAngles, nullptr, right, nullptr);

    // Pick a random strafe direction, then check to see if doing a strafe would be
    //	reasonable valid
    dir = (Q_irand(0, 1)) ? -1 : 1;
    VectorMA(NPC->currentOrigin, SENTRY_STRAFE_DIS * dir, right, end);

    gi.trace(&tr, NPC->currentOrigin, nullptr, nullptr, end, NPC->s.number, MASK_SOLID, EG2_Collision::G2_NOCOLLIDE, 0);

    // Close enough
    if (tr.fraction > 0.9f) {
        VectorMA(NPC->client->ps.velocity, SENTRY_STRAFE_VEL * dir, right, NPC->client->ps.velocity);

        // Add a slight upward push
        NPC->client->ps.velocity[2] += SENTRY_UPWARD_PUSH;

        // Set the strafe start time so we can do a controlled roll
        NPC->fx_time = level.time;
        NPCInfo->standTime = level.time + 3000 + Q_flrand(0.0f, 1.0f) * 500;
    }
}

/*
-------------------------
Sentry_Hunt
-------------------------
*/
void Sentry_Hunt(bool visible, bool advance)
{
    float speed;
    vec3_t forward;

    // If we're not supposed to stand still, pursue the player
    if (NPCInfo->standTime < level.time) {
        // Only strafe when we can see the player
        if (visible) {
            Sentry_Strafe();
            return;
        }
    }

    // If we don't want to advance, stop here
    if (!advance && visible)
        return;

    // Only try and navigate if the player is visible
    if (visible == false) {
        // Move towards our goal
        NPCInfo->goalEntity = NPC->enemy;
        NPCInfo->goalRadius = 12;

        NPC_MoveToGoal(true);
        return;
    }
    else {
        VectorSubtract(NPC->enemy->currentOrigin, NPC->currentOrigin, forward);
        /*distance = */ VectorNormalize(forward);
    }

    speed = SENTRY_FORWARD_BASE_SPEED + SENTRY_FORWARD_MULTIPLIER * g_spskill->integer;
    VectorMA(NPC->client->ps.velocity, speed, forward, NPC->client->ps.velocity);
}

/*
-------------------------
Sentry_RangedAttack
-------------------------
*/
void Sentry_RangedAttack(bool visible, bool advance)
{
    if (TIMER_Done(NPC, "attackDelay") && NPC->attackDebounceTime < level.time && visible) // Attack?
    {
        if (NPCInfo->burstCount > 6) {
            if (!NPC->fly_sound_debounce_time) { // delay closing down to give the player an opening
                NPC->fly_sound_debounce_time = level.time + Q_irand(500, 2000);
            }
            else if (NPC->fly_sound_debounce_time < level.time) {
                NPC_SetLocalState(*NPCInfo, sentryLocalState_t::LSTATE_ACTIVE);
                NPC->fly_sound_debounce_time = NPCInfo->burstCount = 0;
                TIMER_Set(NPC, "attackDelay", Q_irand(2000, 3500));
                NPC->flags |= gentityFlags_t::FL_SHIELDED;
                NPC_SetAnim(NPC, setAnimParts_t::SETANIM_BOTH, animNumber_t::BOTH_FLY_SHIELDED,
                            setAnimFlags_t::SETANIM_FLAG_OVERRIDE | setAnimFlags_t::SETANIM_FLAG_HOLD);
                G_SoundOnEnt(NPC, soundChannel_t::CHAN_AUTO, "sound/chars/sentry/misc/sentry_shield_close");
            }
        }
        else {
            Sentry_Fire();
        }
    }

    if (Any(NPCInfo->scriptFlags & scriptFlags_t::SCF_CHASE_ENEMIES)) {
        Sentry_Hunt(visible, advance);
    }
}

/*
-------------------------
Sentry_AttackDecision
-------------------------
*/
void Sentry_AttackDecision(void)
{
    // Always keep a good height off the ground
    Sentry_MaintainHeight();

    NPC->s.loopSound = G_SoundIndex("sound/chars/sentry/misc/sentry_hover_2_lp");

    // randomly talk
    if (TIMER_Done(NPC, "patrolNoise")) {
        if (TIMER_Done(NPC, "angerNoise")) {
            G_SoundOnEnt(NPC, soundChannel_t::CHAN_AUTO, va("sound/chars/sentry/misc/talk%d", Q_irand(1, 3)));

            TIMER_Set(NPC, "patrolNoise", Q_irand(4000, 10000));
        }
    }

    // He's dead.
    if (NPC->enemy->health < 1) {
        NPC->enemy = nullptr;
        Sentry_Idle();
        return;
    }

    // If we don't have an enemy, just idle
    if (NPC_CheckEnemyExt() == false) {
        Sentry_Idle();
        return;
    }

    // Rate our distance to the target and visibilty
    float distance = (int)DistanceHorizontalSquared(NPC->currentOrigin, NPC->enemy->currentOrigin);
    bool visible = NPC_ClearLOS(NPC->enemy);
    bool advance = (distance > MIN_DISTANCE_SQR);

    // If we cannot see our target, move to see it
    if (visible == false) {
        if (Any(NPCInfo->scriptFlags & scriptFlags_t::SCF_CHASE_ENEMIES)) {
            Sentry_Hunt(visible, advance);
            return;
        }
    }

    NPC_FaceEnemy(true);

    Sentry_RangedAttack(visible, advance);
}

bool NPC_CheckPlayerTeamStealth(void);

/*
-------------------------
NPC_Sentry_Patrol
-------------------------
*/
void NPC_Sentry_Patrol(void)
{
    Sentry_MaintainHeight();

    // If we have somewhere to go, then do that
    if (!NPC->enemy) {
        if (NPC_CheckPlayerTeamStealth()) {
            // NPC_AngerSound();
            NPC_UpdateAngles(true, true);
            return;
        }

        if (UpdateGoal()) {
            // start loop sound once we move
            ucmd.buttons |= buttonFlags_t::BUTTON_WALKING;
            NPC_MoveToGoal(true);
        }

        // randomly talk
        if (TIMER_Done(NPC, "patrolNoise")) {
            G_SoundOnEnt(NPC, soundChannel_t::CHAN_AUTO, va("sound/chars/sentry/misc/talk%d", Q_irand(1, 3)));

            TIMER_Set(NPC, "patrolNoise", Q_irand(2000, 4000));
        }
    }

    NPC_UpdateAngles(true, true);
}

/*
-------------------------
NPC_BSSentry_Default
-------------------------
*/
void NPC_BSSentry_Default(void)
{
    if (NPC->targetname) {
        NPC->e_UseFunc = useFunc_t::useF_sentry_use;
    }

    if ((NPC->enemy) && (NPC_LocalState<sentryLocalState_t>(*NPCInfo) != sentryLocalState_t::LSTATE_WAKEUP)) {
        // Don't attack if waking up or if no enemy
        Sentry_AttackDecision();
    }
    else if (Any(NPCInfo->scriptFlags & scriptFlags_t::SCF_LOOK_FOR_ENEMIES)) {
        NPC_Sentry_Patrol();
    }
    else {
        Sentry_Idle();
    }
}

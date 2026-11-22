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

#include "g_local.h"
#include "b_local.h"
#include "g_functions.h"
#include "wp_saber.h"
#include "w_local.h"

//---------------------------------------------------------
void WP_FireStunBaton(gentity_t* ent, bool alt_fire)
{
    gentity_t* tr_ent;
    trace_t tr;
    vec3_t mins, maxs, end, start;

    G_Sound(ent, G_SoundIndex("sound/weapons/baton/fire"));

    VectorCopy(muzzle, start);
    WP_TraceSetStart(ent, start, vec3_origin, vec3_origin);

    VectorMA(start, STUN_BATON_RANGE, forwardVec, end);

    VectorSet(maxs, 5, 5, 5);
    VectorScale(maxs, -1, mins);

    gi.trace(&tr, start, mins, maxs, end, ent->s.number, contentsFlags_t::CONTENTS_SOLID | contentsFlags_t::CONTENTS_BODY | contentsFlags_t::CONTENTS_SHOTCLIP,
             EG2_Collision::G2_NOCOLLIDE, 0);

    if (tr.entityNum >= ENTITYNUM_WORLD || tr.entityNum < 0) {
        return;
    }

    tr_ent = &g_entities[tr.entityNum];

    if (tr_ent && tr_ent->takedamage && tr_ent->client) {
        G_PlayEffect("stunBaton/flesh_impact", tr.endpos, tr.plane.normal);

        // TEMP!
        //		G_Sound( tr_ent, G_SoundIndex( Q_Format("sound/weapons/melee/punch{}", Q_irand(1, 4)).c_str() ) );
        tr_ent->client->ps.powerups[powerup_t::PW_SHOCKED] = level.time + 1500;

        G_Damage(tr_ent, ent, ent, forwardVec, tr.endpos, weaponData[weapon_t::WP_STUN_BATON].damage, damageFlags_t::DAMAGE_NO_KNOCKBACK,
                 meansOfDeath_t::MOD_MELEE);
    }
    else if (Any(tr_ent->svFlags & svFlags_t::SVF_GLASS_BRUSH) ||
             (Any(tr_ent->svFlags & svFlags_t::SVF_BBRUSH) && tr_ent->material == material_t::MAT_GRATE1)) // material grate...we are breaking a grate!
    {
        G_Damage(tr_ent, ent, ent, forwardVec, tr.endpos, 999, damageFlags_t::DAMAGE_NO_KNOCKBACK, meansOfDeath_t::MOD_MELEE); // smash that puppy
    }
}
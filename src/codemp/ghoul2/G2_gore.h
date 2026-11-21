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

#pragma once

#include "ghoul2_shared.h"
#include <qcommon/q_shared.h>

#ifdef _G2_GORE

#include <shared/qcommon/q_renderer.h>
struct GoreTextureCoordinates {
    float* tex[MAX_LODS];

    GoreTextureCoordinates();
    ~GoreTextureCoordinates();
};

int AllocGoreRecord();
GoreTextureCoordinates* FindGoreRecord(int tag);
void DeleteGoreRecord(int tag);

struct SGoreSurface {
    int shader;
    int mGoreTag;
    int mDeleteTime;
    int mFadeTime;
    bool mFadeRGB;

    int mGoreGrowStartTime;
    int mGoreGrowEndTime; // set this to -1 to disable growing
    // curscale = (curtime-mGoreGrowStartTime)*mGoreGrowFactor + mGoreGrowOffset;
    float mGoreGrowFactor;
    float mGoreGrowOffset;
};

class CGoreSet {
public:
    int mMyGoreSetTag;
    unsigned char mRefCount;
    std::multimap<int, SGoreSurface> mGoreRecords; // a map from surface index
    CGoreSet(int tag) : mMyGoreSetTag(tag), mRefCount(0) {}
    ~CGoreSet();
};

CGoreSet* FindGoreSet(int goreSetTag);
CGoreSet* NewGoreSet();
void DeleteGoreSet(int goreSetTag);

#endif // _G2_GORE

// rww - RAGDOLL_BEGIN

/// ragdoll stuff
struct SRagDollEffectorCollision {
    vec3_t effectorPosition;
    const trace_t& tr;
    bool useTracePlane;
    SRagDollEffectorCollision(const vec3_t effectorPos, const trace_t& t) : tr(t), useTracePlane(false) { VectorCopy(effectorPos, effectorPosition); }
};

class CRagDollUpdateParams {
public:
    vec3_t angles;
    vec3_t position;
    vec3_t scale;
    vec3_t velocity;
    // CServerEntity *me;
    int me; // index!
    int settleFrame;

    // at some point I'll want to make VM callbacks in here. For now I am just doing nothing.
    virtual void EffectorCollision(const SRagDollEffectorCollision& data)
    {
        //	assert(0); // you probably meant to override this
    }
    virtual void RagDollBegin()
    {
        //	assert(0); // you probably meant to override this
    }
    virtual void RagDollSettled()
    {
        //	assert(0); // you probably meant to override this
    }

    virtual void Collision()
    {
        //	assert(0); // you probably meant to override this
        // we had a collision, uhh I guess call SetRagDoll RP_DEATH_COLLISION
    }

#ifdef _DEBUG
    virtual void DebugLine(const vec3_t p1, const vec3_t p2, bool bbox) { assert(0); }
#endif
};

class CRagDollParams {
public:
    enum class ERagPhase {
        RP_START_DEATH_ANIM,
        RP_END_DEATH_ANIM,
        RP_DEATH_COLLISION,
        RP_CORPSE_SHOT,
        RP_GET_PELVIS_OFFSET, // this actually does nothing but set the pelvisAnglesOffset, and pelvisPositionOffset
        RP_SET_PELVIS_OFFSET, // this actually does nothing but set the pelvisAnglesOffset, and pelvisPositionOffset
        RP_DISABLE_EFFECTORS  // this removes effectors given by the effectorsToTurnOff member
    };
    vec3_t angles;
    vec3_t position;
    vec3_t scale;
    vec3_t pelvisAnglesOffset;   // always set on return, an argument for RP_SET_PELVIS_OFFSET
    vec3_t pelvisPositionOffset; // always set on return, an argument for RP_SET_PELVIS_OFFSET

    float fImpactStrength; // should be applicable when RagPhase is RP_DEATH_COLLISION
    float fShotStrength;   // should be applicable for setting velocity of corpse on shot (probably only on RP_CORPSE_SHOT)
    // CServerEntity *me;
    int me;

    // rww - we have convenient animation/frame access in the game, so just send this info over from there.
    int startFrame;
    int endFrame;

    int collisionType; // 1 = from a fall, 0 from effectors, this will be going away soon, hence no enum

    bool CallRagDollBegin; // a return value, means that we are now begininng ragdoll and the NPC stuff needs to happen

    ERagPhase RagPhase;

    // effector control, used for RP_DISABLE_EFFECTORS call

    ragEffectorFlags_t effectorsToTurnOff; // set this to an | of the above flags for a RP_DISABLE_EFFECTORS
};
// rww - RAGDOLL_END

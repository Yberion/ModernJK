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
#pragma once

#include "q_platform.h"
#include "q_libm.h"
#include "q_rand.h"
#include "q_enum.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef float vec_t;
typedef float vec2_t[2], vec3_t[3], vec4_t[4], vec5_t[5];
typedef int ivec2_t[2], ivec3_t[3], ivec4_t[4], ivec5_t[5];
typedef vec3_t vec3pair_t[2], matrix3_t[3];

typedef int fixed4_t, fixed8_t, fixed16_t;

inline constexpr int Q3_INFINITE = 16'777'216; // the engine's, ICARUS's and the games' "infinite" time or distance

inline constexpr int MAX_QINT = 0x7fffffff;

///////////////////////////////////////////////////////////////////////////
//
//      DIRECTION ENCODING
//
///////////////////////////////////////////////////////////////////////////
int DirToByte(vec3_t dir);
void ByteToDir(int b, vec3_t dir);
void NormalToLatLong(const vec3_t normal, byte bytes[2]);

///////////////////////////////////////////////////////////////////////////
//
//      RANDOM NUMBER GENERATION
//
///////////////////////////////////////////////////////////////////////////
// q_rand.h

///////////////////////////////////////////////////////////////////////////
//
//      MATH UTILITIES
//
///////////////////////////////////////////////////////////////////////////
#define minimum(x, y) ((x) < (y) ? (x) : (y))
#define maximum(x, y) ((x) > (y) ? (x) : (y))

inline constexpr float PI_DIV_180 = 0.017453292519943295769236907684886f;
inline constexpr float INV_PI_DIV_180 = 57.295779513082320876798154814105f;

#define DEG2RAD(a) (((a) * PI_DIV_180))
#define RAD2DEG(a) (((a) * INV_PI_DIV_180))

#define SQRTFAST(x) ((x) * Q_rsqrtf(x))

#define Q_min(x, y) ((x) < (y) ? (x) : (y))
#define Q_max(x, y) ((x) > (y) ? (x) : (y))

#if defined(_MSC_VER)
static __inline long Q_ftol(float f)
{
    return (long)f;
}
#else
static inline long Q_ftol(float f)
{
    return (long)f;
}
#endif

signed char ClampChar(int i);
signed short ClampShort(int i);
int Com_Clampi(int min, int max, int value);
float Com_Clamp(float min, float max, float value);
int Com_AbsClampi(int min, int max, int value);
float Com_AbsClamp(float min, float max, float value);

int Q_ilog2(int val);

float LerpAngle(float from, float to, float frac);
float AngleSubtract(float a1, float a2);
void AnglesSubtract(vec3_t v1, vec3_t v2, vec3_t v3);
float AngleMod(float a);
float AngleNormalize360(float angle);
float AngleNormalize180(float angle);
float AngleDelta(float angle1, float angle2);

///////////////////////////////////////////////////////////////////////////
//
//      GEOMETRIC UTILITIES
//
///////////////////////////////////////////////////////////////////////////
// angle indexes
inline constexpr int PITCH = 0; // up / down
inline constexpr int YAW = 1;   // left / right
inline constexpr int ROLL = 2;  // fall over

bool PlaneFromPoints(vec4_t plane, const vec3_t a, const vec3_t b, const vec3_t c);
void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees);
void RotateAroundDirection(matrix3_t axis, float yaw);
void vectoangles(const vec3_t value1, vec3_t angles);
vec_t GetYawForDirection(const vec3_t p1, const vec3_t p2);
void GetAnglesForDirection(const vec3_t p1, const vec3_t p2, vec3_t out);
void ProjectPointOnPlane(vec3_t dst, const vec3_t p, const vec3_t normal);
bool G_FindClosestPointOnLineSegment(const vec3_t start, const vec3_t end, const vec3_t from, vec3_t result);
float G_PointDistFromLineSegment(const vec3_t start, const vec3_t end, const vec3_t from);
void MatrixMultiply(float in1[3][3], float in2[3][3], float out[3][3]);

///////////////////////////////////////////////////////////////////////////
//
//      BOUNDING BOX
//
///////////////////////////////////////////////////////////////////////////
float RadiusFromBounds(const vec3_t mins, const vec3_t maxs);
void ClearBounds(vec3_t mins, vec3_t maxs);
void AddPointToBounds(const vec3_t v, vec3_t mins, vec3_t maxs);

///////////////////////////////////////////////////////////////////////////
//
//      PLANE
//
///////////////////////////////////////////////////////////////////////////
// plane types are used to speed some tests
// 0-2 are axial planes
enum class planeType_t : uint8_t { // a byte of cplane_t
    PLANE_X,
    PLANE_Y,
    PLANE_Z,
    PLANE_NON_AXIAL,
};

// the axis of an axial plane, PLANE_X to PLANE_Z: the index of its normal's one component
[[nodiscard]] constexpr int PlaneAxis(planeType_t type)
{
    assert(type < planeType_t::PLANE_NON_AXIAL);
    return std::to_underlying(type);
}

// plane_t structure
typedef struct cplane_s {
    vec3_t normal;
    float dist;
    planeType_t type; // for fast side tests: 0,1,2 = axial, 3 = nonaxial
    byte signbits;    // signx + (signy<<1) + (signz<<2), used as lookup during collision
    byte pad[2];
} cplane_t;
static_assert(sizeof(cplane_t) == 20);

void SetPlaneSignbits(cplane_t* out);
int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, cplane_t* p);

///////////////////////////////////////////////////////////////////////////
//
//      AXIS
//
///////////////////////////////////////////////////////////////////////////
extern matrix3_t axisDefault;

void AxisClear(matrix3_t axis);
void AxisCopy(matrix3_t in, matrix3_t out);
void AnglesToAxis(const vec3_t angles, matrix3_t axis);

///////////////////////////////////////////////////////////////////////////
//
//      VEC2
//
///////////////////////////////////////////////////////////////////////////
extern vec2_t vec3_zero;

#define VectorScale2M(v, factor, dst) (dst)[0] = (v[0]) * (factor), (dst)[1] = (v[1]) * (factor)
#define VectorCopy2M(src, dst) (dst)[0] = (src[0]), (dst)[1] = (src[1])
#define VectorClear2M(dst) memset((dst), 0, sizeof((dst)[0]) * 2)

void VectorAdd2(const vec2_t vec1, const vec2_t vec2, vec2_t vecOut);
void VectorSubtract2(const vec2_t vec1, const vec2_t vec2, vec2_t vec2_t);
void VectorScale2(const vec2_t vecIn, float scale, vec2_t vecOut);
void VectorMA2(const vec2_t vec1, float scale, const vec2_t vec2, vec2_t vecOut);
void VectorSet2(vec2_t vec, float x, float y);
void VectorClear2(vec2_t vec);
void VectorCopy2(const vec2_t vecIn, vec2_t vecOut);

///////////////////////////////////////////////////////////////////////////
//
//      VEC3
//
///////////////////////////////////////////////////////////////////////////
extern vec3_t vec3_origin;

// The small helpers are inline (they were functions of q_math.cpp, called through the linker from every loop of the
// collision and model code: DotProduct alone was 11.5 % of a server tick's samples). The
// bodies and their semantics are unchanged, an output that aliases an input included.

#define VectorScaleM(v, factor, dst) (dst)[0] = (v[0]) * (factor), (dst)[1] = (v[1]) * (factor), (dst)[2] = (v[2]) * (factor)
#define VectorCopyM(src, dst) (dst)[0] = (src[0]), (dst)[1] = (src[1]), (dst)[2] = (src[2])
#define VectorClearM(dst) memset((dst), 0, sizeof((dst)[0]) * 3)

static QINLINE void VectorAdd(const vec3_t vec1, const vec3_t vec2, vec3_t vecOut)
{
    vecOut[0] = vec1[0] + vec2[0];
    vecOut[1] = vec1[1] + vec2[1];
    vecOut[2] = vec1[2] + vec2[2];
}
static QINLINE void VectorSubtract(const vec3_t vec1, const vec3_t vec2, vec3_t vecOut)
{
    vecOut[0] = vec1[0] - vec2[0];
    vecOut[1] = vec1[1] - vec2[1];
    vecOut[2] = vec1[2] - vec2[2];
}
static QINLINE void VectorScale(const vec3_t vecIn, float scale, vec3_t vecOut)
{
    vecOut[0] = vecIn[0] * scale;
    vecOut[1] = vecIn[1] * scale;
    vecOut[2] = vecIn[2] * scale;
}
static QINLINE void VectorMA(const vec3_t vec1, float scale, const vec3_t vec2, vec3_t vecOut)
{
    vecOut[0] = vec1[0] + scale * vec2[0];
    vecOut[1] = vec1[1] + scale * vec2[1];
    vecOut[2] = vec1[2] + scale * vec2[2];
}
static QINLINE void VectorSet(vec3_t vec, float x, float y, float z)
{
    vec[0] = x;
    vec[1] = y;
    vec[2] = z;
}
static QINLINE void VectorClear(vec3_t vec)
{
    vec[0] = vec[1] = vec[2] = 0.0f;
}
static QINLINE void VectorCopy(const vec3_t vecIn, vec3_t vecOut)
{
    vecOut[0] = vecIn[0];
    vecOut[1] = vecIn[1];
    vecOut[2] = vecIn[2];
}
// lengths in double: the squares of floats are exact there, and the result is rounded to float once
static QINLINE float VectorLength(const vec3_t vec)
{
    return (float)Q_sqrt((double)vec[0] * vec[0] + (double)vec[1] * vec[1] + (double)vec[2] * vec[2]);
}
static QINLINE float VectorLengthSquared(const vec3_t vec)
{
    return (float)((double)vec[0] * vec[0] + (double)vec[1] * vec[1] + (double)vec[2] * vec[2]);
}
void VectorNormalizeFast(vec3_t vec);
float VectorNormalize(vec3_t vec);
float VectorNormalize2(const vec3_t vec, vec3_t vecOut);
void VectorAdvance(const vec3_t veca, const float scale, const vec3_t vecb, vec3_t vecc);
static QINLINE void VectorInc(vec3_t vec)
{
    vec[0] += 1.0f;
    vec[1] += 1.0f;
    vec[2] += 1.0f;
}
static QINLINE void VectorDec(vec3_t vec)
{
    vec[0] -= 1.0f;
    vec[1] -= 1.0f;
    vec[2] -= 1.0f;
}
static QINLINE void VectorInverse(vec3_t vec)
{
    vec[0] = -vec[0];
    vec[1] = -vec[1];
    vec[2] = -vec[2];
}
static QINLINE void CrossProduct(const vec3_t vec1, const vec3_t vec2, vec3_t vecOut)
{
    vecOut[0] = vec1[1] * vec2[2] - vec1[2] * vec2[1];
    vecOut[1] = vec1[2] * vec2[0] - vec1[0] * vec2[2];
    vecOut[2] = vec1[0] * vec2[1] - vec1[1] * vec2[0];
}
static QINLINE float DotProduct(const vec3_t vec1, const vec3_t vec2)
{
    return vec1[0] * vec2[0] + vec1[1] * vec2[1] + vec1[2] * vec2[2];
}
static QINLINE bool VectorCompare(const vec3_t vec1, const vec3_t vec2)
{
    return (vec1[0] == vec2[0] && vec1[1] == vec2[1] && vec1[2] == vec2[2]);
}
bool VectorCompare2(const vec3_t v1, const vec3_t v2);

void SnapVector(float* v);
static QINLINE float Distance(const vec3_t p1, const vec3_t p2)
{
    vec3_t v;

    VectorSubtract(p2, p1, v);
    return VectorLength(v);
}
static QINLINE float DistanceSquared(const vec3_t p1, const vec3_t p2)
{
    vec3_t v;

    VectorSubtract(p2, p1, v);
    return VectorLengthSquared(v);
}
float DistanceHorizontal(const vec3_t p1, const vec3_t p2);
float DistanceHorizontalSquared(const vec3_t p1, const vec3_t p2);
void MakeNormalVectors(const vec3_t forward, vec3_t right, vec3_t up);
void VectorRotate(const vec3_t in, matrix3_t matrix, vec3_t out);
void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up);
void PerpendicularVector(vec3_t dst, const vec3_t src);
float DotProductNormalize(const vec3_t inVec1, const vec3_t inVec2);

#define VectorScaleVector(a, b, c) (((c)[0] = (a)[0] * (b)[0]), ((c)[1] = (a)[1] * (b)[1]), ((c)[2] = (a)[2] * (b)[2]))
#define VectorInverseScaleVector(a, b, c) ((c)[0] = (a)[0] / (b)[0], (c)[1] = (a)[1] / (b)[1], (c)[2] = (a)[2] / (b)[2])
#define VectorScaleVectorAdd(c, a, b, o) ((o)[0] = (c)[0] + ((a)[0] * (b)[0]), (o)[1] = (c)[1] + ((a)[1] * (b)[1]), (o)[2] = (c)[2] + ((a)[2] * (b)[2]))
#define VectorAverage(a, b, c) (((c)[0] = ((a)[0] + (b)[0]) * 0.5f), ((c)[1] = ((a)[1] + (b)[1]) * 0.5f), ((c)[2] = ((a)[2] + (b)[2]) * 0.5f))
#define VectorNegate(a, b) ((b)[0] = -(a)[0], (b)[1] = -(a)[1], (b)[2] = -(a)[2])

///////////////////////////////////////////////////////////////////////////
//
//      VEC4
//
///////////////////////////////////////////////////////////////////////////
void VectorScale4(const vec4_t vecIn, float scale, vec4_t vecOut);
void VectorCopy4(const vec4_t vecIn, vec4_t vecOut);
void VectorSet4(vec4_t vec, float x, float y, float z, float w);
void VectorClear4(vec4_t vec);

///////////////////////////////////////////////////////////////////////////
//
//      VEC5
//
///////////////////////////////////////////////////////////////////////////
void VectorSet5(vec5_t vec, float x, float y, float z, float w, float u);

#if defined(__cplusplus)
} // extern "C"
#endif

// with C++ linkage: a function of C linkage that returns a class enum is a warning for MSVC (C4190)
planeType_t PlaneTypeForNormal(const vec3_t normal);

enum class planeSide_t {
    SIDE_FRONT,
    SIDE_BACK,
    SIDE_ON,
    SIDE_CROSS,
};

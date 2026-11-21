#pragma once

#include "qcommon/q_fileformats.h"

#ifdef __cplusplus
extern "C"
{
#endif

inline constexpr float MC_SCALE_X = 1.0f/64;
inline constexpr float MC_SCALE_Y = 1.0f/64;
inline constexpr float MC_SCALE_Z = 1.0f/64;


// currently 24 (.875)

void MC_Compress(const float mat[3][4],unsigned char * comp);
void MC_UnCompress(float mat[3][4],const unsigned char * comp);
void MC_UnCompressQuat(float mat[3][4],const unsigned char * comp);


#ifdef __cplusplus
}
#endif

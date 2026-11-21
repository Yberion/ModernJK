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

// Filename:	mp3struct.h
//
// this file is my struct to gather all loose MP3 global vars into one struct so we can do multiple-stream decompression
//

#ifndef MP3STRUCT_H
#define MP3STRUCT_H

#include "small_header.h"          // for SAMPLE and IN_OUT
#include <shared/qcommon/q_libm.h> // Q_cos, Q_atan, Q_pow: the same floats on every platform

typedef void (*SBT_FUNCTION)(float* sample, short* pcm, int n);
typedef void (*XFORM_FUNCTION)(void* pcm, int igr);
typedef IN_OUT (*DECODE_FUNCTION)(unsigned char* bs, unsigned char* pcm);
typedef unsigned char byte;

typedef struct {
    union {
        struct {
            SBT_FUNCTION sbt;

            float cs_factor[3][64]; // 768 bytes

            int nbat[4];
            int bat[4][16];
            int max_sb;
            int stereo_sb;
            int bit_skip;

            float* cs_factorL1;
            int nbatL1;

        }; // L1_2;

        struct {
            SBT_FUNCTION sbt_L3;
            XFORM_FUNCTION Xform;
            DECODE_FUNCTION decode_function;

            SAMPLE sample[2][2][576]; // if this isn't kept per stream then the decode breaks up

// the 4k version of these 2 seems to work for everything, but I'm reverting to original 8k for safety jic.
//
#define NBUF (8 * 1024)
#define BUF_TRIGGER (NBUF - 1500)
            //		#define NBUF (4096)	// 2048 works for all except 133+ kbps VBR files, 4096 copes with these
            //		#define BUF_TRIGGER ((NBUF/4)*3)

            unsigned char buf[NBUF];
            int buf_ptr0;
            int buf_ptr1;
            int main_pos_bit;

            int band_limit_nsb;
            int nBand[2][22];       /* [long/short][cb] */
            int sfBandIndex[2][22]; /* [long/short][cb] */
            int half_outbytes;
            int crcbytes;
            int nchan;
            int ms_mode;
            int is_mode;
            unsigned int zero_level_pcm;
            int mpeg25_flag;
            int band_limit;
            int band_limit21;
            int band_limit12;
            int gain_adjust;
            int ncbl_mixed;
        }; // L3;
    };
    // from csbt.c...
    //
    // if this isn't kept per stream then the decode breaks up
    signed int vb_ptr;  //
    signed int vb2_ptr; //
    float vbuf[512];    //
    float vbuf2[512];   // this can be lost if we stick to mono samples

    // L3 only...
    //
    int sr_index; // L3 only (99%)
    int id;

    // any type...
    //
    int outvalues;
    int outbytes;
    int framebytes;
    int pad;
    int nsb_limit;

    // stuff added now that the game uses streaming MP3s...
    //
    byte* pbSourceData; // a useful dup ptr only, this whole struct will be owned by an sfx_t struct that has the actual data ptr field
    int iSourceBytesRemaining;
    int iSourceReadIndex;
    int iSourceFrameBytes;
#ifdef _DEBUG
    int iSourceFrameCounter; // not really important
#endif
    int iBytesDecodedTotal;
    int iBytesDecodedThisPacket; // not sure how useful this will be, it's only per-frame, so will always be full frame size (eg 2304 or below for mono) except
                                 // possibly for the last frame?

    int iRewind_FinalReductionCode;
    int iRewind_FinalConvertCode;
    int iRewind_SourceBytesRemaining;
    int iRewind_SourceReadIndex;
    byte bDecodeBuffer[2304 * 2]; // *2 to allow for stereo now
    int iCopyOffset;              // used for painting to DMA-feeder, since 2304 won't match the size it wants

    // some new stuff added for dynamic music, to allow "how many seconds left to play" queries...
    //
    // ( m_lengthInSeconds = ((iUnpackedDataLength / iRate) / iChannels) / iWidth; )
    //
    // Note that these fields are only valid/initialised if MP3Stream_InitPlayingTimeFields() was called.
    //	If not, this->iTimeQuery_UnpackedLength will be zero.
    //
    int iTimeQuery_UnpackedLength;
    int iTimeQuery_SampleRate;
    int iTimeQuery_Channels;
    int iTimeQuery_Width;

} MP3STREAM, *LP_MP3STREAM;

#define MP3STUFF_KNOWN

extern LP_MP3STREAM pMP3Stream;
extern int bFastEstimateOnly;

#endif // #ifndef MP3STRUCT_H

////////////////// eof /////////////////////

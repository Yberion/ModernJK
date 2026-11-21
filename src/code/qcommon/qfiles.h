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

#ifndef __QFILES_H__
#define __QFILES_H__

//
// qfiles.h: quake file formats
// This file must be identical in the quake and utils directories
//

// surface geometry should not exceed these limits
#include "qcommon/q_fileformats.h"

// the maximum size of game reletive pathnames

/*
========================================================================

QVM files

========================================================================
*/

typedef struct {
    int vmMagic;

    int instructionCount;

    int codeOffset;
    int codeLength;

    int dataOffset;
    int dataLength;
    int litLength; // ( dataLength - litLength ) should be byteswapped on load
    int bssLength; // zero filled memory appended to datalength
} vmHeader_t;

/*
========================================================================

PCX files are used for 8 bit images

========================================================================
*/

typedef struct {
    char manufacturer;
    char version;
    char encoding;
    char bits_per_pixel;
    unsigned short xmin, ymin, xmax, ymax;
    unsigned short hres, vres;
    unsigned char palette[48];
    char reserved;
    char color_planes;
    unsigned short bytes_per_line;
    unsigned short palette_type;
    char filler[58];
    unsigned char data; // unbounded
} pcx_t;

/*
========================================================================

.MD3 triangle model file format

========================================================================
*/

inline constexpr int MD3_IDENT = ('3' << 24) + ('P' << 16) + ('D' << 8) + 'I';

// limits

// vertex scales

typedef struct md3Frame_s {
    vec3_t bounds[2];
    vec3_t localOrigin;
    float radius;
    char name[16];
} md3Frame_t;

typedef struct md3Tag_s {
    char name[MAX_QPATH]; // tag name
    vec3_t origin;
    vec3_t axis[3];
} md3Tag_t;

/*
** md3Surface_t
**
** CHUNK			SIZE
** header			sizeof( md3Surface_t )
** shaders			sizeof( md3Shader_t ) * numShaders
** triangles[0]		sizeof( md3Triangle_t ) * numTriangles
** st				sizeof( md3St_t ) * numVerts
** XyzNormals		sizeof( md3XyzNormal_t ) * numVerts * numFrames
*/
typedef struct {
    int ident; //

    char name[MAX_QPATH]; // polyset name

    int flags;
    int numFrames; // all surfaces in a model should have the same

    int numShaders; // all surfaces in a model should have the same
    int numVerts;

    int numTriangles;
    int ofsTriangles;

    int ofsShaders;    // offset from start of md3Surface_t
    int ofsSt;         // texture coords are common for all frames
    int ofsXyzNormals; // numVerts * numFrames

    int ofsEnd; // next surface follows
} md3Surface_t;

typedef struct {
    char name[MAX_QPATH];
    int shaderIndex; // for in-game use
} md3Shader_t;

typedef struct {
    int indexes[3];
} md3Triangle_t;

typedef struct {
    float st[2];
} md3St_t;

typedef struct {
    short xyz[3];
    short normal;
} md3XyzNormal_t;

typedef struct {
    int ident;
    int version;

    char name[MAX_QPATH]; // model name

    int flags;

    int numFrames;
    int numTags;
    int numSurfaces;

    int numSkins;

    int ofsFrames;   // offset for first frame
    int ofsTags;     // numFrames * numTags
    int ofsSurfaces; // first surface, others follow

    int ofsEnd; // end of file
} md3Header_t;

/*
==============================================================================

  .BSP file format

==============================================================================
*/

// little-endian "IBSP"

// there shouldn't be any problem with increasing these values at the
// expense of more memory allocation in the utilities

// key / value pair sizes in the entities lump

// the editor uses these predefined yaw angles to orient entities up or down

//=============================================================================

typedef struct {
    int fileofs, filelen;
} lump_t;

typedef struct {
    int ident;
    int version;

    EnumArray<lump_t, bspLump_t, bspLump_t::HEADER_LUMPS> lumps;
} dheader_t;
static_assert(sizeof(dheader_t) == 152); // the file's header: its identifier, its version and 18 lumps

typedef struct {
    float mins[3], maxs[3];
    int firstSurface, numSurfaces;
    int firstBrush, numBrushes;
} dmodel_t;

typedef struct dshader_s {
    char shader[MAX_QPATH];
    int surfaceFlags;
    int contentFlags;
} dshader_t;

// planes x^1 is allways the opposite of plane x

typedef struct {
    float normal[3];
    float dist;
} dplane_t;

typedef struct {
    int planeNum;
    int children[2]; // negative numbers are -(leafs+1), not nodes
    int mins[3];     // for frustom culling
    int maxs[3];
} dnode_t;

typedef struct {
    int cluster; // -1 = opaque cluster (do I still store these?)
    int area;

    int mins[3]; // for frustum culling
    int maxs[3];

    int firstLeafSurface;
    int numLeafSurfaces;

    int firstLeafBrush;
    int numLeafBrushes;
} dleaf_t;

typedef struct {
    int planeNum; // positive plane side faces out of the leaf
    int shaderNum;
    int drawSurfNum;
} dbrushside_t;

typedef struct {
    int firstSide;
    int numSides;
    int shaderNum; // the shader that determines the contents flags
} dbrush_t;

typedef struct {
    char shader[MAX_QPATH];
    int brushNum;
    int visibleSide; // the brush side that ray tests need to clip against (-1 == none)
} dfog_t;

// Light Style Constants
inline constexpr int LS_NORMAL = 0x00;
inline constexpr int LS_UNUSED = 0xfe;
#define LS_NONE 0xff

typedef struct {
    vec3_t xyz;
    float st[2];
    float lightmap[MAXLIGHTMAPS][2];
    vec3_t normal;
    byte color[MAXLIGHTMAPS][4];
} mapVert_t;

typedef struct {
    vec3_t xyz;
    float st[2];
    float lightmap[MAXLIGHTMAPS][2];
    vec3_t normal;
    byte color[MAXLIGHTMAPS][4];
} drawVert_t;

typedef struct {
    byte ambientLight[MAXLIGHTMAPS][3];
    byte directLight[MAXLIGHTMAPS][3];
    byte styles[MAXLIGHTMAPS];
    byte latLong[2];
} dgrid_t;

enum class mapSurfaceType_t { MST_BAD, MST_PLANAR, MST_PATCH, MST_TRIANGLE_SOUP, MST_FLARE };

typedef struct {
    int shaderNum;
    int fogNum;
    int surfaceType;

    int firstVert;
    int numVerts;

    int firstIndex;
    int numIndexes;

    byte lightmapStyles[MAXLIGHTMAPS], vertexStyles[MAXLIGHTMAPS];
    int lightmapNum[MAXLIGHTMAPS];
    int lightmapX[MAXLIGHTMAPS], lightmapY[MAXLIGHTMAPS];
    int lightmapWidth, lightmapHeight;

    vec3_t lightmapOrigin;
    vec3_t lightmapVecs[3]; // for patches, [0] and [1] are lodbounds

    int patchWidth;
    int patchHeight;
} dsurface_t;

enum class hunkAllocType_t // # hunkAllocType_e
{
    HA_MISC,
    HA_MAP,
    HA_SHADERS,
    HA_LIGHTING,
    HA_FOG,
    HA_PATCHES,
    HA_VIS,
    HA_SUBMODELS,
    HA_MODELS,
    MAX_HA_TYPES
};

/////////////////////////////////////////////////////////////
//
// Defines and structures required for fonts

// Must match define in stmparse.h
inline constexpr unsigned int STYLE_DROPSHADOW = 0x80000000;
inline constexpr int STYLE_BLINK = 0x40000000;
inline constexpr int SET_MASK = 0x00ffffff;

typedef struct {
    short width;        // number of pixels wide
    short height;       // number of scan lines
    short horizAdvance; // number of pixels to advance to the next char
    short horizOffset;  // x offset into space to render glyph
    int baseline;       // y offset
    float s;            // x start tex coord
    float t;            // y start tex coord
    float s2;           // x end tex coord
    float t2;           // y end tex coord
} glyphInfo_t;

// this file corresponds 1:1 with the "*.fontdat" files, so don't change it unless you're going to
//	recompile the fontgen util and regenerate all the fonts!
//
typedef struct dfontdat_s {
    glyphInfo_t mGlyphs[GLYPH_COUNT];

    short mPointSize;
    short mHeight; // max height of font
    short mAscender;
    short mDescender;

    short mKoreanHack; // unused field, written out by John's fontgen program but we have to leave it there for disk structs <sigh>
} dfontdat_t;

/////////////////// fonts end ////////////////////////////////////

#endif

/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

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

// tr_shader.c -- this file deals with the parsing and definition of shaders

#include "tr_local.h"

static char *s_shaderText;

// the shader is parsed into these global variables, then copied into
// dynamically allocated memory if it is valid.
static	shaderStage_t	stages[MAX_SHADER_STAGES];
static	shader_t		shader;
static	texModInfo_t	texMods[MAX_SHADER_STAGES][TR_MAX_TEXMODS];

constexpr int FILE_HASH_SIZE = 1024;
static	shader_t*		hashTable[FILE_HASH_SIZE];

constexpr int MAX_SHADERTEXT_HASH = 2048;
static char **shaderTextHashTable[MAX_SHADERTEXT_HASH] = { nullptr };

void KillTheShaderHashTable(void)
{
	memset(shaderTextHashTable, 0, sizeof(shaderTextHashTable));
}

bool ShaderHashTableExists(void)
{
	if (shaderTextHashTable[0])
	{
		return true;
	}
	return false;
}

const int lightmapsNone[MAXLIGHTMAPS] =
{
	LIGHTMAP_NONE,
	LIGHTMAP_NONE,
	LIGHTMAP_NONE,
	LIGHTMAP_NONE
};

const int lightmaps2d[MAXLIGHTMAPS] =
{
	LIGHTMAP_2D,
	LIGHTMAP_2D,
	LIGHTMAP_2D,
	LIGHTMAP_2D
};

const int lightmapsVertex[MAXLIGHTMAPS] =
{
	LIGHTMAP_BY_VERTEX,
	LIGHTMAP_BY_VERTEX,
	LIGHTMAP_BY_VERTEX,
	LIGHTMAP_BY_VERTEX
};

const int lightmapsFullBright[MAXLIGHTMAPS] =
{
	LIGHTMAP_WHITEIMAGE,
	LIGHTMAP_WHITEIMAGE,
	LIGHTMAP_WHITEIMAGE,
	LIGHTMAP_WHITEIMAGE
};

const byte stylesDefault[MAXLIGHTMAPS] =
{
	LS_NORMAL,
	LS_LSNONE,
	LS_LSNONE,
	LS_LSNONE
};

static void ClearGlobalShader(void)
{
	int	i;

	memset( &shader, 0, sizeof( shader ) );
	memset( &stages, 0, sizeof( stages ) );
	for ( i = 0 ; i < MAX_SHADER_STAGES ; i++ ) {
		stages[i].bundle[0].texMods = texMods[i];
		stages[i].mGLFogColorOverride = EGLFogOverride::GLFOGOVERRIDE_NONE;
	}

	shader.contentFlags = contentsFlags_t::CONTENTS_SOLID | contentsFlags_t::CONTENTS_OPAQUE;
}

/*
================
return a hash value for the filename
================
*/
static long generateHashValue( const char *fname, const int size ) {
	int		i;
	long	hash;
	char	letter;

	hash = 0;
	i = 0;
	while (fname[i] != '\0') {
		letter = tolower((unsigned char)fname[i]);
		if (letter =='.') break;				// don't include extension
		if (letter =='\\') letter = '/';		// damn path names
		if (letter == PATH_SEP) letter = '/';		// damn path names
		hash+=(long)(letter)*(i+119);
		i++;
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));
	hash &= (size-1);
	return hash;
}

void R_RemapShader(const char *shaderName, const char *newShaderName, const char *timeOffset) {
	char		strippedName[MAX_QPATH];
	int			hash;
	shader_t	*sh, *sh2;
	qhandle_t	h;

	sh = R_FindShaderByName( shaderName );
	if (sh == nullptr || sh == tr.defaultShader) {
		h = RE_RegisterShaderLightMap(shaderName, lightmapsNone, stylesDefault);
		sh = R_GetShaderByHandle(h);
	}
	if (sh == nullptr || sh == tr.defaultShader) {
		Com_Printf (S_COLOR_YELLOW  "WARNING: R_RemapShader: shader %s not found\n", shaderName );
		return;
	}

	sh2 = R_FindShaderByName( newShaderName );
	if (sh2 == nullptr || sh2 == tr.defaultShader) {
		h = RE_RegisterShaderLightMap(newShaderName, lightmapsNone, stylesDefault);
		sh2 = R_GetShaderByHandle(h);
	}

	if (sh2 == nullptr || sh2 == tr.defaultShader) {
		Com_Printf (S_COLOR_YELLOW  "WARNING: R_RemapShader: new shader %s not found\n", newShaderName );
		return;
	}

	// remap all the shaders with the given name
	// even tho they might have different lightmaps
	COM_StripExtension( shaderName, strippedName, sizeof( strippedName ) );
	hash = generateHashValue(strippedName, FILE_HASH_SIZE);
	for (sh = hashTable[hash]; sh; sh = sh->next) {
		if (Q_stricmp(sh->name, strippedName) == 0) {
			if (sh != sh2) {
				sh->remappedShader = sh2;
			} else {
				sh->remappedShader = nullptr;
			}
		}
	}
	if (timeOffset) {
		sh2->timeOffset = Q_atof(timeOffset);
	}
}

/*
===============
ParseVector
===============
*/
bool ParseVector( const char **text, int count, float *v ) {
	char	*token;
	int		i;

	// FIXME: spaces are currently required after parens, should change parseext...
	token = COM_ParseExt( text, false );
	if ( strcmp( token, "(" ) ) {
		Com_Printf (S_COLOR_YELLOW  "WARNING: missing parenthesis in shader '%s'\n", shader.name );
		return false;
	}

	for ( i = 0 ; i < count ; i++ ) {
		token = COM_ParseExt( text, false );
		if ( !token[0] ) {
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing vector element in shader '%s'\n", shader.name );
			return false;
		}
		v[i] = Q_atof( token );
	}

	token = COM_ParseExt( text, false );
	if ( strcmp( token, ")" ) ) {
		Com_Printf (S_COLOR_YELLOW  "WARNING: missing parenthesis in shader '%s'\n", shader.name );
		return false;
	}

	return true;
}


/*
===============
NameToAFunc
===============
*/
static glStateBits_t NameToAFunc( const char *funcname )
{
	if ( !Q_stricmp( funcname, "GT0" ) )
	{
		return glStateBits_t::GLS_ATEST_GT_0;
	}
	else if ( !Q_stricmp( funcname, "LT128" ) )
	{
		return glStateBits_t::GLS_ATEST_LT_80;
	}
	else if ( !Q_stricmp( funcname, "GE128" ) )
	{
		return glStateBits_t::GLS_ATEST_GE_80;
	}
	else if ( !Q_stricmp( funcname, "GE192" ) )
	{
		return glStateBits_t::GLS_ATEST_GE_C0;
	}

	Com_Printf (S_COLOR_YELLOW  "WARNING: invalid alphaFunc name '%s' in shader '%s'\n", funcname, shader.name );
	return glStateBits_t::GLS_NONE;
}


/*
===============
NameToSrcBlendMode
===============
*/
static glStateBits_t NameToSrcBlendMode( const char *name )
{
	if ( !Q_stricmp( name, "GL_ONE" ) )
	{
		return glStateBits_t::GLS_SRCBLEND_ONE;
	}
	else if ( !Q_stricmp( name, "GL_ZERO" ) )
	{
		return glStateBits_t::GLS_SRCBLEND_ZERO;
	}
	else if ( !Q_stricmp( name, "GL_DST_COLOR" ) )
	{
		return glStateBits_t::GLS_SRCBLEND_DST_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_COLOR" ) )
	{
		return glStateBits_t::GLS_SRCBLEND_ONE_MINUS_DST_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
	{
		return glStateBits_t::GLS_SRCBLEND_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
	{
		return glStateBits_t::GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
	{
		return glStateBits_t::GLS_SRCBLEND_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
	{
		return glStateBits_t::GLS_SRCBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA_SATURATE" ) )
	{
		return glStateBits_t::GLS_SRCBLEND_ALPHA_SATURATE;
	}

	Com_Printf (S_COLOR_YELLOW  "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader.name );
	return glStateBits_t::GLS_SRCBLEND_ONE;
}

/*
===============
NameToDstBlendMode
===============
*/
static glStateBits_t NameToDstBlendMode( const char *name )
{
	if ( !Q_stricmp( name, "GL_ONE" ) )
	{
		return glStateBits_t::GLS_DSTBLEND_ONE;
	}
	else if ( !Q_stricmp( name, "GL_ZERO" ) )
	{
		return glStateBits_t::GLS_DSTBLEND_ZERO;
	}
	else if ( !Q_stricmp( name, "GL_SRC_ALPHA" ) )
	{
		return glStateBits_t::GLS_DSTBLEND_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )
	{
		return glStateBits_t::GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_DST_ALPHA" ) )
	{
		return glStateBits_t::GLS_DSTBLEND_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )
	{
		return glStateBits_t::GLS_DSTBLEND_ONE_MINUS_DST_ALPHA;
	}
	else if ( !Q_stricmp( name, "GL_SRC_COLOR" ) )
	{
		return glStateBits_t::GLS_DSTBLEND_SRC_COLOR;
	}
	else if ( !Q_stricmp( name, "GL_ONE_MINUS_SRC_COLOR" ) )
	{
		return glStateBits_t::GLS_DSTBLEND_ONE_MINUS_SRC_COLOR;
	}

	Com_Printf (S_COLOR_YELLOW  "WARNING: unknown blend mode '%s' in shader '%s', substituting GL_ONE\n", name, shader.name );
	return glStateBits_t::GLS_DSTBLEND_ONE;
}

/*
===============
NameToGenFunc
===============
*/
static genFunc_t NameToGenFunc( const char *funcname )
{
	if ( !Q_stricmp( funcname, "sin" ) )
	{
		return genFunc_t::GF_SIN;
	}
	else if ( !Q_stricmp( funcname, "square" ) )
	{
		return genFunc_t::GF_SQUARE;
	}
	else if ( !Q_stricmp( funcname, "triangle" ) )
	{
		return genFunc_t::GF_TRIANGLE;
	}
	else if ( !Q_stricmp( funcname, "sawtooth" ) )
	{
		return genFunc_t::GF_SAWTOOTH;
	}
	else if ( !Q_stricmp( funcname, "inversesawtooth" ) )
	{
		return genFunc_t::GF_INVERSE_SAWTOOTH;
	}
	else if ( !Q_stricmp( funcname, "noise" ) )
	{
		return genFunc_t::GF_NOISE;
	}
	else if ( !Q_stricmp( funcname, "random" ) )
	{
		return genFunc_t::GF_RAND;
	}

	Com_Printf (S_COLOR_YELLOW  "WARNING: invalid genfunc name '%s' in shader '%s'\n", funcname, shader.name );
	return genFunc_t::GF_SIN;
}


/*
===================
ParseWaveForm
===================
*/
static void ParseWaveForm( const char **text, waveForm_t *wave )
{
	char *token;

	token = COM_ParseExt( text, false );
	if ( token[0] == 0 )
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->func = NameToGenFunc( token );

	// BASE, AMP, PHASE, FREQ
	token = COM_ParseExt( text, false );
	if ( token[0] == 0 )
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->base = Q_atof( token );

	token = COM_ParseExt( text, false );
	if ( token[0] == 0 )
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->amplitude = Q_atof( token );

	token = COM_ParseExt( text, false );
	if ( token[0] == 0 )
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->phase = Q_atof( token );

	token = COM_ParseExt( text, false );
	if ( token[0] == 0 )
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: missing waveform parm in shader '%s'\n", shader.name );
		return;
	}
	wave->frequency = Q_atof( token );
}


/*
===================
ParseTexMod
===================
*/
static void ParseTexMod( const char *_text, shaderStage_t *stage )
{
	const char *token;
	const char **text = &_text;
	texModInfo_t *tmi;

	if ( stage->bundle[0].numTexMods == TR_MAX_TEXMODS ) {
		Com_Error( errorParm_t::ERR_DROP, "ERROR: too many tcMod stages in shader '%s'\n", shader.name );
		return;
	}

	tmi = &stage->bundle[0].texMods[stage->bundle[0].numTexMods];
	stage->bundle[0].numTexMods++;

	token = COM_ParseExt( text, false );

	//
	// turb
	//
	if ( !Q_stricmp( token, "turb" ) )
	{
		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing tcMod turb parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.base = Q_atof( token );
		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.amplitude = Q_atof( token );
		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.phase = Q_atof( token );
		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing tcMod turb in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.frequency = Q_atof( token );

		tmi->type = texMod_t::TMOD_TURBULENT;
	}
	//
	// scale
	//
	else if ( !Q_stricmp( token, "scale" ) )
	{
		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing scale parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[0] = Q_atof( token );	//scale unioned

		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing scale parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[1] = Q_atof( token );	//scale unioned
		tmi->type = texMod_t::TMOD_SCALE;
	}
	//
	// scroll
	//
	else if ( !Q_stricmp( token, "scroll" ) )
	{
		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing scale scroll parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[0] = Q_atof( token );	//scroll unioned
		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing scale scroll parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[1] = Q_atof( token );	//scroll unioned
		tmi->type = texMod_t::TMOD_SCROLL;
	}
	//
	// stretch
	//
	else if ( !Q_stricmp( token, "stretch" ) )
	{
		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.func = NameToGenFunc( token );

		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.base = Q_atof( token );

		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.amplitude = Q_atof( token );

		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.phase = Q_atof( token );

		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing stretch parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->wave.frequency = Q_atof( token );

		tmi->type = texMod_t::TMOD_STRETCH;
	}
	//
	// transform
	//
	else if ( !Q_stricmp( token, "transform" ) )
	{
		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[0][0] = Q_atof( token );

		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[0][1] = Q_atof( token );

		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[1][0] = Q_atof( token );

		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->matrix[1][1] = Q_atof( token );

		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[0] = Q_atof( token );

		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing transform parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[1] = Q_atof( token );

		tmi->type = texMod_t::TMOD_TRANSFORM;
	}
	//
	// rotate
	//
	else if ( !Q_stricmp( token, "rotate" ) )
	{
		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing tcMod rotate parms in shader '%s'\n", shader.name );
			return;
		}
		tmi->translate[0]= Q_atof( token );	//rotateSpeed unioned
		tmi->type = texMod_t::TMOD_ROTATE;
	}
	//
	// entityTranslate
	//
	else if ( !Q_stricmp( token, "entityTranslate" ) )
	{
		tmi->type = texMod_t::TMOD_ENTITY_TRANSLATE;
	}
	else
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: unknown tcMod '%s' in shader '%s'\n", token, shader.name );
	}
}



/*
/////===== Part of the VERTIGON system =====/////
===================
ParseSurfaceSprites
===================
*/
// surfaceSprites <type> <width> <height> <density> <fadedist>
//
// NOTE:  This parsing function used to be 12 pages long and very complex.  The new version of surfacesprites
// utilizes optional parameters parsed in ParseSurfaceSpriteOptional.
static void ParseSurfaceSprites( const char *_text, shaderStage_t *stage )
{
	const char *token;
	const char **text = &_text;
	float width, height, density, fadedist;
	int sstype=SURFSPRITE_NONE;

	//
	// spritetype
	//
	token = COM_ParseExt( text, false );

	if (token[0]==0)
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: missing surfaceSprites params in shader '%s'\n", shader.name );
		return;
	}

	if (!Q_stricmp(token, "vertical"))
	{
		sstype = SURFSPRITE_VERTICAL;
	}
	else if (!Q_stricmp(token, "oriented"))
	{
		sstype = SURFSPRITE_ORIENTED;
	}
	else if (!Q_stricmp(token, "effect"))
	{
		sstype = SURFSPRITE_EFFECT;
	}
	else if (!Q_stricmp(token, "flattened"))
	{
		sstype = SURFSPRITE_FLATTENED;
	}
	else
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: invalid type in shader '%s'\n", shader.name );
		return;
	}

	//
	// width
	//
	token = COM_ParseExt( text, false );
	if (token[0]==0)
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: missing surfaceSprites params in shader '%s'\n", shader.name );
		return;
	}
	width=Q_atof(token);
	if (width <= 0)
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: invalid width in shader '%s'\n", shader.name );
		return;
	}

	//
	// height
	//
	token = COM_ParseExt( text, false );
	if (token[0]==0)
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: missing surfaceSprites params in shader '%s'\n", shader.name );
		return;
	}
	height=Q_atof(token);
	if (height <= 0)
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: invalid height in shader '%s'\n", shader.name );
		return;
	}

	//
	// density
	//
	token = COM_ParseExt( text, false );
	if (token[0]==0)
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: missing surfaceSprites params in shader '%s'\n", shader.name );
		return;
	}
	density=Q_atof(token);
	if (density <= 0)
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: invalid density in shader '%s'\n", shader.name );
		return;
	}

	//
	// fadedist
	//
	token = COM_ParseExt( text, false );
	if (token[0]==0)
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: missing surfaceSprites params in shader '%s'\n", shader.name );
		return;
	}
	fadedist=Q_atof(token);
	if (fadedist < 32)
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: invalid fadedist (%f < 32) in shader '%s'\n", fadedist, shader.name );
		return;
	}

	if (!stage->ss)
	{
		stage->ss = (surfaceSprite_t *)Hunk_Alloc( sizeof( surfaceSprite_t ), ha_pref::h_low );
	}

	// These are all set by the command lines.
	stage->ss->surfaceSpriteType = sstype;
	stage->ss->width = width;
	stage->ss->height = height;
	stage->ss->density = density;
	stage->ss->fadeDist = fadedist;

	// These are defaults that can be overwritten.
	stage->ss->fadeMax = fadedist*1.33;
	stage->ss->fadeScale = 0.0;
	stage->ss->wind = 0.0;
	stage->ss->windIdle = 0.0;
	stage->ss->variance[0] = 0.0;
	stage->ss->variance[1] = 0.0;
	stage->ss->facing = SURFSPRITE_FACING_NORMAL;

	// A vertical parameter that needs a default regardless
	stage->ss->vertSkew = 0.0f;

	// These are effect parameters that need defaults nonetheless.
	stage->ss->fxDuration = 1000;		// 1 second
	stage->ss->fxGrow[0] = 0.0;
	stage->ss->fxGrow[1] = 0.0;
	stage->ss->fxAlphaStart = 1.0;
	stage->ss->fxAlphaEnd = 0.0;
}




/*
/////===== Part of the VERTIGON system =====/////
===========================
ParseSurfaceSpritesOptional
===========================
*/
//
// ssFademax <fademax>
// ssFadescale <fadescale>
// ssVariance <varwidth> <varheight>
// ssHangdown
// ssAnyangle
// ssFaceup
// ssWind <wind>
// ssWindIdle <windidle>
// ssVertSkew <skew>
// ssFXDuration <duration>
// ssFXGrow <growwidth> <growheight>
// ssFXAlphaRange <alphastart> <startend>
// ssFXWeather
//
// Optional parameters that will override the defaults set in the surfacesprites command above.
//
static void ParseSurfaceSpritesOptional( const char *param, const char *_text, shaderStage_t *stage )
{
	const char *token;
	const char **text = &_text;
	float	value;

	if (!stage->ss)
	{
		stage->ss = (surfaceSprite_t *)Hunk_Alloc( sizeof( surfaceSprite_t ), ha_pref::h_low );
	}
	//
	// fademax
	//
	if (!Q_stricmp(param, "ssFademax"))
	{
		token = COM_ParseExt( text, false);
		if (token[0]==0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing surfacesprite fademax in shader '%s'\n", shader.name );
			return;
		}
		value = Q_atof(token);
		if (value <= stage->ss->fadeDist)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: invalid surfacesprite fademax (%.2f <= fadeDist(%.2f)) in shader '%s'\n", value, stage->ss->fadeDist, shader.name );
			return;
		}
		stage->ss->fadeMax=value;
		return;
	}

	//
	// fadescale
	//
	if (!Q_stricmp(param, "ssFadescale"))
	{
		token = COM_ParseExt( text, false);
		if (token[0]==0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing surfacesprite fadescale in shader '%s'\n", shader.name );
			return;
		}
		value = Q_atof(token);
		stage->ss->fadeScale=value;
		return;
	}

	//
	// variance
	//
	if (!Q_stricmp(param, "ssVariance"))
	{
		token = COM_ParseExt( text, false);
		if (token[0]==0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing surfacesprite variance width in shader '%s'\n", shader.name );
			return;
		}
		value = Q_atof(token);
		if (value < 0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: invalid surfacesprite variance width in shader '%s'\n", shader.name );
			return;
		}
		stage->ss->variance[0]=value;

		token = COM_ParseExt( text, false);
		if (token[0]==0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing surfacesprite variance height in shader '%s'\n", shader.name );
			return;
		}
		value = Q_atof(token);
		if (value < 0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: invalid surfacesprite variance height in shader '%s'\n", shader.name );
			return;
		}
		stage->ss->variance[1]=value;
		return;
	}

	//
	// hangdown
	//
	if (!Q_stricmp(param, "ssHangdown"))
	{
		if (stage->ss->facing != SURFSPRITE_FACING_NORMAL)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: Hangdown facing overrides previous facing in shader '%s'\n", shader.name );
			return;
		}
		stage->ss->facing=SURFSPRITE_FACING_DOWN;
		return;
	}

	//
	// anyangle
	//
	if (!Q_stricmp(param, "ssAnyangle"))
	{
		if (stage->ss->facing != SURFSPRITE_FACING_NORMAL)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: Anyangle facing overrides previous facing in shader '%s'\n", shader.name );
			return;
		}
		stage->ss->facing=SURFSPRITE_FACING_ANY;
		return;
	}

	//
	// faceup
	//
	if (!Q_stricmp(param, "ssFaceup"))
	{
		if (stage->ss->facing != SURFSPRITE_FACING_NORMAL)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: Faceup facing overrides previous facing in shader '%s'\n", shader.name );
			return;
		}
		stage->ss->facing=SURFSPRITE_FACING_UP;
		return;
	}

	//
	// wind
	//
	if (!Q_stricmp(param, "ssWind"))
	{
		token = COM_ParseExt( text, false);
		if (token[0]==0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing surfacesprite wind in shader '%s'\n", shader.name );
			return;
		}
		value = Q_atof(token);
		if (value < 0.0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: invalid surfacesprite wind in shader '%s'\n", shader.name );
			return;
		}
		stage->ss->wind=value;
		if (stage->ss->windIdle <= 0)
		{	// Also override the windidle, it usually is the same as wind
			stage->ss->windIdle = value;
		}
		return;
	}

	//
	// windidle
	//
	if (!Q_stricmp(param, "ssWindidle"))
	{
		token = COM_ParseExt( text, false);
		if (token[0]==0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing surfacesprite windidle in shader '%s'\n", shader.name );
			return;
		}
		value = Q_atof(token);
		if (value < 0.0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: invalid surfacesprite windidle in shader '%s'\n", shader.name );
			return;
		}
		stage->ss->windIdle=value;
		return;
	}

	//
	// vertskew
	//
	if (!Q_stricmp(param, "ssVertskew"))
	{
		token = COM_ParseExt( text, false);
		if (token[0]==0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing surfacesprite vertskew in shader '%s'\n", shader.name );
			return;
		}
		value = Q_atof(token);
		if (value < 0.0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: invalid surfacesprite vertskew in shader '%s'\n", shader.name );
			return;
		}
		stage->ss->vertSkew=value;
		return;
	}

	//
	// fxduration
	//
	if (!Q_stricmp(param, "ssFXDuration"))
	{
		token = COM_ParseExt( text, false);
		if (token[0]==0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing surfacesprite duration in shader '%s'\n", shader.name );
			return;
		}
		value = Q_atof(token);
		if (value <= 0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: invalid surfacesprite duration in shader '%s'\n", shader.name );
			return;
		}
		stage->ss->fxDuration=value;
		return;
	}

	//
	// fxgrow
	//
	if (!Q_stricmp(param, "ssFXGrow"))
	{
		token = COM_ParseExt( text, false);
		if (token[0]==0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing surfacesprite grow width in shader '%s'\n", shader.name );
			return;
		}
		value = Q_atof(token);
		if (value < 0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: invalid surfacesprite grow width in shader '%s'\n", shader.name );
			return;
		}
		stage->ss->fxGrow[0]=value;

		token = COM_ParseExt( text, false);
		if (token[0]==0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing surfacesprite grow height in shader '%s'\n", shader.name );
			return;
		}
		value = Q_atof(token);
		if (value < 0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: invalid surfacesprite grow height in shader '%s'\n", shader.name );
			return;
		}
		stage->ss->fxGrow[1]=value;
		return;
	}

	//
	// fxalpharange
	//
	if (!Q_stricmp(param, "ssFXAlphaRange"))
	{
		token = COM_ParseExt( text, false);
		if (token[0]==0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing surfacesprite fxalpha start in shader '%s'\n", shader.name );
			return;
		}
		value = Q_atof(token);
		if (value < 0 || value > 1.0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: invalid surfacesprite fxalpha start in shader '%s'\n", shader.name );
			return;
		}
		stage->ss->fxAlphaStart=value;

		token = COM_ParseExt( text, false);
		if (token[0]==0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing surfacesprite fxalpha end in shader '%s'\n", shader.name );
			return;
		}
		value = Q_atof(token);
		if (value < 0 || value > 1.0)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: invalid surfacesprite fxalpha end in shader '%s'\n", shader.name );
			return;
		}
		stage->ss->fxAlphaEnd=value;
		return;
	}

	//
	// fxweather
	//
	if (!Q_stricmp(param, "ssFXWeather"))
	{
		if (stage->ss->surfaceSpriteType != SURFSPRITE_EFFECT)
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: weather applied to non-effect surfacesprite in shader '%s'\n", shader.name );
			return;
		}
		stage->ss->surfaceSpriteType = SURFSPRITE_WEATHERFX;
		return;
	}

	//
	// invalid ss command.
	//
	Com_Printf (S_COLOR_YELLOW  "WARNING: invalid optional surfacesprite param '%s' in shader '%s'\n", param, shader.name );
	return;
}


/*
===================
ParseStage
===================
*/
static bool ParseStage( shaderStage_t *stage, const char **text )
{
	char *token;
	glStateBits_t depthMaskBits = glStateBits_t::GLS_DEPTHMASK_TRUE, blendSrcBits = glStateBits_t::GLS_NONE, blendDstBits = glStateBits_t::GLS_NONE, atestBits = glStateBits_t::GLS_NONE, depthFuncBits = glStateBits_t::GLS_NONE;
	bool depthMaskExplicit = false;

	stage->active = true;

	while ( 1 )
	{
		token = COM_ParseExt( text, true );
		if ( !token[0] )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: no matching '}' found\n" );
			return false;
		}

		if ( token[0] == '}' )
		{
			break;
		}
		//
		// map <name>
		//
		else if ( !Q_stricmp( token, "map" ) )
		{
			token = COM_ParseExt( text, false );
			if ( !token[0] )
			{
				Com_Printf (S_COLOR_YELLOW  "WARNING: missing parameter for 'map' keyword in shader '%s'\n", shader.name );
				return false;
			}

			if ( !Q_stricmp( token, "$whiteimage" ) )
			{
				stage->bundle[0].image = tr.whiteImage;
				continue;
			}
			else if ( !Q_stricmp( token, "$lightmap" ) )
			{
				stage->bundle[0].isLightmap = true;
				if ( shader.lightmapIndex[0] < 0 || shader.lightmapIndex[0] >= tr.numLightmaps )
				{
#ifndef FINAL_BUILD
					Com_Printf(S_COLOR_RED"Lightmap requested but none available for shader %s\n", shader.name);
#endif
					stage->bundle[0].image = tr.whiteImage;
				}
				else
				{
					stage->bundle[0].image = tr.lightmaps[shader.lightmapIndex[0]];
				}
				continue;
			}
			else
			{
				stage->bundle[0].image = nullptr;
				return false;
			}
		}
		//
		// clampmap <name>
		//
		else if ( !Q_stricmp( token, "clampmap" ) )
		{
			token = COM_ParseExt( text, false );
			if ( !token[0] )
			{
				Com_Printf (S_COLOR_YELLOW  "WARNING: missing parameter for 'clampmap' keyword in shader '%s'\n", shader.name );
				return false;
			}
			stage->bundle[0].image = nullptr;
			return false;
		}
		//
		// animMap <frequency> <image1> .... <imageN>
		//
		else if ( !Q_stricmp( token, "animMap" ) || !Q_stricmp( token, "clampanimMap" ) || !Q_stricmp( token, "oneshotanimMap" ))
		{
			constexpr int MAX_IMAGE_ANIMATIONS = 32;
			image_t *images[MAX_IMAGE_ANIMATIONS];
			bool bClamp = !Q_stricmp( token, "clampanimMap" );
			bool oneShot = !Q_stricmp( token, "oneshotanimMap" );

			token = COM_ParseExt( text, false );
			if ( !token[0] )
			{
				Com_Printf (S_COLOR_YELLOW  "WARNING: missing parameter for '%s' keyword in shader '%s'\n", (bClamp ? "animMap":"clampanimMap"), shader.name );
				return false;
			}
			stage->bundle[0].imageAnimationSpeed = Q_atof( token );
			stage->bundle[0].oneShotAnimMap = oneShot;

			// parse up to MAX_IMAGE_ANIMATIONS animations
			while ( 1 ) {
				int		num;

				token = COM_ParseExt( text, false );
				if ( !token[0] ) {
					break;
				}
				num = stage->bundle[0].numImageAnimations;
				if ( num < MAX_IMAGE_ANIMATIONS ) {
					stage->bundle[0].image = nullptr;
					return false;
				}
			}
			// Copy image ptrs into an array of ptrs
			stage->bundle[0].image = (image_t*) Hunk_Alloc( stage->bundle[0].numImageAnimations * sizeof( image_t* ), ha_pref::h_low );
			memcpy( stage->bundle[0].image,	images,			stage->bundle[0].numImageAnimations * sizeof( image_t* ) );
		}
		else if ( !Q_stricmp( token, "videoMap" ) )
		{
			token = COM_ParseExt( text, false );
			if ( !token[0] )
			{
				Com_Printf (S_COLOR_YELLOW  "WARNING: missing parameter for 'videoMap' keyword in shader '%s'\n", shader.name );
				return false;
			}
			stage->bundle[0].videoMapHandle = -1;
			if (stage->bundle[0].videoMapHandle != -1) {
				stage->bundle[0].isVideoMap = true;
				assert (stage->bundle[0].videoMapHandle<NUM_SCRATCH_IMAGES);
				stage->bundle[0].image = tr.scratchImage[stage->bundle[0].videoMapHandle];
			}
		}

		//
		// alphafunc <func>
		//
		else if ( !Q_stricmp( token, "alphaFunc" ) )
		{
			token = COM_ParseExt( text, false );
			if ( !token[0] )
			{
				Com_Printf (S_COLOR_YELLOW  "WARNING: missing parameter for 'alphaFunc' keyword in shader '%s'\n", shader.name );
				return false;
			}

			atestBits = NameToAFunc( token );
		}
		//
		// depthFunc <func>
		//
		else if ( !Q_stricmp( token, "depthfunc" ) )
		{
			token = COM_ParseExt( text, false );

			if ( !token[0] )
			{
				Com_Printf (S_COLOR_YELLOW  "WARNING: missing parameter for 'depthfunc' keyword in shader '%s'\n", shader.name );
				return false;
			}

			if ( !Q_stricmp( token, "lequal" ) )
			{
				depthFuncBits = glStateBits_t::GLS_NONE;
			}
			else if ( !Q_stricmp( token, "equal" ) )
			{
				depthFuncBits = glStateBits_t::GLS_DEPTHFUNC_EQUAL;
			}
			else if ( !Q_stricmp( token, "disable" ) )
			{
				depthFuncBits = glStateBits_t::GLS_DEPTHTEST_DISABLE;
			}
			else
			{
				Com_Printf (S_COLOR_YELLOW  "WARNING: unknown depthfunc '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		//
		// detail
		//
		else if ( !Q_stricmp( token, "detail" ) )
		{
			stage->isDetail = true;
		}
		//
		// blendfunc <srcFactor> <dstFactor>
		// or blendfunc <add|filter|blend>
		//
		else if ( !Q_stricmp( token, "blendfunc" ) )
		{
			token = COM_ParseExt( text, false );
			if ( token[0] == 0 )
			{
				Com_Printf (S_COLOR_YELLOW  "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name );
				continue;
			}
			// check for "simple" blends first
			if ( !Q_stricmp( token, "add" ) ) {
				blendSrcBits = glStateBits_t::GLS_SRCBLEND_ONE;
				blendDstBits = glStateBits_t::GLS_DSTBLEND_ONE;
			} else if ( !Q_stricmp( token, "filter" ) ) {
				blendSrcBits = glStateBits_t::GLS_SRCBLEND_DST_COLOR;
				blendDstBits = glStateBits_t::GLS_DSTBLEND_ZERO;
			} else if ( !Q_stricmp( token, "blend" ) ) {
				blendSrcBits = glStateBits_t::GLS_SRCBLEND_SRC_ALPHA;
				blendDstBits = glStateBits_t::GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			} else {
				// complex double blends
				blendSrcBits = NameToSrcBlendMode( token );

				token = COM_ParseExt( text, false );
				if ( token[0] == 0 )
				{
					Com_Printf (S_COLOR_YELLOW  "WARNING: missing parm for blendFunc in shader '%s'\n", shader.name );
					continue;
				}
				blendDstBits = NameToDstBlendMode( token );
			}

			// clear depth mask for blended surfaces
			if ( !depthMaskExplicit )
			{
				depthMaskBits = glStateBits_t::GLS_NONE;
			}
		}
		//
		// rgbGen
		//
		else if ( !Q_stricmp( token, "rgbGen" ) )
		{
			token = COM_ParseExt( text, false );
			if ( token[0] == 0 )
			{
				Com_Printf (S_COLOR_YELLOW  "WARNING: missing parameters for rgbGen in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "wave" ) )
			{
				ParseWaveForm( text, &stage->rgbWave );
				stage->rgbGen = colorGen_t::CGEN_WAVEFORM;
			}
			else if ( !Q_stricmp( token, "const" ) )
			{
				vec3_t	color;

				ParseVector( text, 3, color );
				stage->constantColor[0] = 255 * color[0];
				stage->constantColor[1] = 255 * color[1];
				stage->constantColor[2] = 255 * color[2];

				stage->rgbGen = colorGen_t::CGEN_CONST;
			}
			else if ( !Q_stricmp( token, "identity" ) )
			{
				stage->rgbGen = colorGen_t::CGEN_IDENTITY;
			}
			else if ( !Q_stricmp( token, "identityLighting" ) )
			{
				stage->rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
			}
			else if ( !Q_stricmp( token, "entity" ) )
			{
				stage->rgbGen = colorGen_t::CGEN_ENTITY;
			}
			else if ( !Q_stricmp( token, "oneMinusEntity" ) )
			{
				stage->rgbGen = colorGen_t::CGEN_ONE_MINUS_ENTITY;
			}
			else if ( !Q_stricmp( token, "vertex" ) )
			{
				stage->rgbGen = colorGen_t::CGEN_VERTEX;
				if ( stage->alphaGen == alphaGen_t::AGEN_IDENTITY ) {
					stage->alphaGen = alphaGen_t::AGEN_VERTEX;
				}
			}
			else if ( !Q_stricmp( token, "exactVertex" ) )
			{
				stage->rgbGen = colorGen_t::CGEN_EXACT_VERTEX;
			}
			else if ( !Q_stricmp( token, "lightingDiffuse" ) )
			{
				stage->rgbGen = colorGen_t::CGEN_LIGHTING_DIFFUSE;

			}
			else if ( !Q_stricmp( token, "lightingDiffuseEntity" ) )
			{
				if (shader.lightmapIndex[0] != LIGHTMAP_NONE)
				{
					Com_Printf( S_COLOR_RED "ERROR: rgbGen lightingDiffuseEntity used on a misc_model! in shader '%s'\n", shader.name );
				}
				stage->rgbGen = colorGen_t::CGEN_LIGHTING_DIFFUSE_ENTITY;

			}
			else if ( !Q_stricmp( token, "oneMinusVertex" ) )
			{
				stage->rgbGen = colorGen_t::CGEN_ONE_MINUS_VERTEX;
			}
			else
			{
				Com_Printf (S_COLOR_YELLOW  "WARNING: unknown rgbGen parameter '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		//
		// alphaGen
		//
		else if ( !Q_stricmp( token, "alphaGen" ) )
		{
			token = COM_ParseExt( text, false );
			if ( token[0] == 0 )
			{
				Com_Printf (S_COLOR_YELLOW  "WARNING: missing parameters for alphaGen in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "wave" ) )
			{
				ParseWaveForm( text, &stage->alphaWave );
				stage->alphaGen = alphaGen_t::AGEN_WAVEFORM;
			}
			else if ( !Q_stricmp( token, "const" ) )
			{
				token = COM_ParseExt( text, false );
				stage->constantColor[3] = 255 * Q_atof( token );
				stage->alphaGen = alphaGen_t::AGEN_CONST;
			}
			else if ( !Q_stricmp( token, "identity" ) )
			{
				stage->alphaGen = alphaGen_t::AGEN_IDENTITY;
			}
			else if ( !Q_stricmp( token, "entity" ) )
			{
				stage->alphaGen = alphaGen_t::AGEN_ENTITY;
			}
			else if ( !Q_stricmp( token, "oneMinusEntity" ) )
			{
				stage->alphaGen = alphaGen_t::AGEN_ONE_MINUS_ENTITY;
			}
			else if ( !Q_stricmp( token, "vertex" ) )
			{
				stage->alphaGen = alphaGen_t::AGEN_VERTEX;
			}
			else if ( !Q_stricmp( token, "lightingSpecular" ) )
			{
				stage->alphaGen = alphaGen_t::AGEN_LIGHTING_SPECULAR;
			}
			else if ( !Q_stricmp( token, "oneMinusVertex" ) )
			{
				stage->alphaGen = alphaGen_t::AGEN_ONE_MINUS_VERTEX;
			}
			else if ( !Q_stricmp( token, "dot" ) )
			{
				stage->alphaGen = alphaGen_t::AGEN_DOT;
			}
			else if ( !Q_stricmp( token, "oneMinusDot" ) )
			{
				stage->alphaGen = alphaGen_t::AGEN_ONE_MINUS_DOT;
			}
			else if ( !Q_stricmp( token, "portal" ) )
			{
				stage->alphaGen = alphaGen_t::AGEN_PORTAL;
				token = COM_ParseExt( text, false );
				if ( token[0] == 0 )
				{
					shader.portalRange = 256;
					Com_Printf (S_COLOR_YELLOW  "WARNING: missing range parameter for alphaGen portal in shader '%s', defaulting to 256\n", shader.name );
				}
				else
				{
					shader.portalRange = Q_atof( token );
				}
			}
			else
			{
				Com_Printf (S_COLOR_YELLOW  "WARNING: unknown alphaGen parameter '%s' in shader '%s'\n", token, shader.name );
				continue;
			}
		}
		//
		// tcGen <function>
		//
		else if ( !Q_stricmp(token, "texgen") || !Q_stricmp( token, "tcGen" ) )
		{
			token = COM_ParseExt( text, false );
			if ( token[0] == 0 )
			{
				Com_Printf (S_COLOR_YELLOW  "WARNING: missing texgen parm in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "environment" ) )
			{
				stage->bundle[0].tcGen = texCoordGen_t::TCGEN_ENVIRONMENT_MAPPED;
			}
			else if ( !Q_stricmp( token, "lightmap" ) )
			{
				stage->bundle[0].tcGen = texCoordGen_t::TCGEN_LIGHTMAP;
			}
			else if ( !Q_stricmp( token, "texture" ) || !Q_stricmp( token, "base" ) )
			{
				stage->bundle[0].tcGen = texCoordGen_t::TCGEN_TEXTURE;
			}
			else if ( !Q_stricmp( token, "vector" ) )
			{
				stage->bundle[0].tcGenVectors = ( vec3_t *) Hunk_Alloc( 2 * sizeof( vec3_t ), ha_pref::h_low );
				ParseVector( text, 3, stage->bundle[0].tcGenVectors[0] );
				ParseVector( text, 3, stage->bundle[0].tcGenVectors[1] );

				stage->bundle[0].tcGen = texCoordGen_t::TCGEN_VECTOR;
			}
			else
			{
				Com_Printf (S_COLOR_YELLOW  "WARNING: unknown texgen parm in shader '%s'\n", shader.name );
			}
		}
		//
		// tcMod <type> <...>
		//
		else if ( !Q_stricmp( token, "tcMod" ) )
		{
			char buffer[1024] = "";

			while ( 1 )
			{
				token = COM_ParseExt( text, false );
				if ( token[0] == 0 )
					break;
				Q_strcat( buffer, sizeof( buffer ), token );
				Q_strcat( buffer, sizeof( buffer ), " " );
			}

			ParseTexMod( buffer, stage );

			continue;
		}
		//
		// depthmask
		//
		else if ( !Q_stricmp( token, "depthwrite" ) )
		{
			depthMaskBits = glStateBits_t::GLS_DEPTHMASK_TRUE;
			depthMaskExplicit = true;

			continue;
		}
		// If this stage has glow...	GLOWXXX
		else if ( Q_stricmp( token, "glow" ) == 0 )
		{
			stage->glow = true;

			continue;
		}
		//
		// surfaceSprites <type> ...
		//
		else if ( !Q_stricmp( token, "surfaceSprites" ) )
		{
			char buffer[1024] = "";

			while ( 1 )
			{
				token = COM_ParseExt( text, false );
				if ( token[0] == 0 )
					break;
				Q_strcat( buffer, sizeof( buffer ), token );
				Q_strcat( buffer, sizeof( buffer ), " " );
			}

			ParseSurfaceSprites( buffer, stage );

			continue;
		}
		//
		// ssFademax <fademax>
		// ssFadescale <fadescale>
		// ssVariance <varwidth> <varheight>
		// ssHangdown
		// ssAnyangle
		// ssFaceup
		// ssWind <wind>
		// ssWindIdle <windidle>
		// ssDuration <duration>
		// ssGrow <growwidth> <growheight>
		// ssWeather
		//
		else if (!Q_stricmpn(token, "ss", 2))	// <--- NOTE ONLY COMPARING FIRST TWO LETTERS
		{
			char buffer[1024] = "";
			char param[128];
			strcpy(param,token);

			while ( 1 )
			{
				token = COM_ParseExt( text, false );
				if ( token[0] == 0 )
					break;
				Q_strcat( buffer, sizeof( buffer ), token );
				Q_strcat( buffer, sizeof( buffer ), " " );
			}

			ParseSurfaceSpritesOptional( param, buffer, stage );

			continue;
		}
		else
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: unknown parameter '%s' in shader '%s'\n", token, shader.name );
			return false;
		}
	}

	//
	// if cgen isn't explicitly specified, use either identity or identitylighting
	//
	if ( stage->rgbGen == colorGen_t::CGEN_BAD ) {
		if ( //blendSrcBits == 0 ||
			blendSrcBits == glStateBits_t::GLS_SRCBLEND_ONE ||
			blendSrcBits == glStateBits_t::GLS_SRCBLEND_SRC_ALPHA ) {
			stage->rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
		} else {
			stage->rgbGen = colorGen_t::CGEN_IDENTITY;
		}
	}


	//
	// implicitly assume that a GL_ONE GL_ZERO blend mask disables blending
	//
	if ( ( blendSrcBits == glStateBits_t::GLS_SRCBLEND_ONE ) &&
		 ( blendDstBits == glStateBits_t::GLS_DSTBLEND_ZERO ) )
	{
		blendDstBits = blendSrcBits = glStateBits_t::GLS_NONE;
		depthMaskBits = glStateBits_t::GLS_DEPTHMASK_TRUE;
	}

	// decide which agens we can skip
	if ( stage->alphaGen == alphaGen_t::AGEN_IDENTITY ) {
		if ( stage->rgbGen == colorGen_t::CGEN_IDENTITY
			|| stage->rgbGen == colorGen_t::CGEN_LIGHTING_DIFFUSE ) {
			stage->alphaGen = alphaGen_t::AGEN_SKIP;
		}
	}

	//
	// compute state bits
	//
	stage->stateBits = depthMaskBits |
		               blendSrcBits | blendDstBits |
					   atestBits |
					   depthFuncBits;

	return true;
}

/*
===============
ParseDeform

deformVertexes wave <spread> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes normal <frequency> <amplitude>
deformVertexes move <vector> <waveform> <base> <amplitude> <phase> <frequency>
deformVertexes bulge <bulgeWidth> <bulgeHeight> <bulgeSpeed>
deformVertexes projectionShadow
deformVertexes autoSprite
deformVertexes autoSprite2
deformVertexes text[0-7]
===============
*/
static void ParseDeform( const char **text ) {
	char	*token;
	deformStage_t	*ds;

	token = COM_ParseExt( text, false );
	if ( token[0] == 0 )
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: missing deform parm in shader '%s'\n", shader.name );
		return;
	}

	if ( shader.numDeforms == MAX_SHADER_DEFORMS ) {
		Com_Printf (S_COLOR_YELLOW  "WARNING: MAX_SHADER_DEFORMS in '%s'\n", shader.name );
		return;
	}

	shader.deforms[ shader.numDeforms ] = (deformStage_t *)Hunk_Alloc( sizeof( deformStage_t ), ha_pref::h_low );

	ds = shader.deforms[ shader.numDeforms ];
	shader.numDeforms++;

	if ( !Q_stricmp( token, "projectionShadow" ) ) {
		ds->deformation = deform_t::DEFORM_PROJECTION_SHADOW;
		return;
	}

	if ( !Q_stricmp( token, "autosprite" ) ) {
		ds->deformation = deform_t::DEFORM_AUTOSPRITE;
		return;
	}

	if ( !Q_stricmp( token, "autosprite2" ) ) {
		ds->deformation = deform_t::DEFORM_AUTOSPRITE2;
		return;
	}

	if ( !Q_stricmpn( token, "text", 4 ) ) {
		int		n;

		n = token[4] - '0';
		if ( n < 0 || n > 7 ) {
			n = 0;
		}
		ds->deformation = deform_t::DEFORM_TEXT0 + n;
		return;
	}

	if ( !Q_stricmp( token, "bulge" ) )	{
		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeWidth = Q_atof( token );

		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeHeight = Q_atof( token );

		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing deformVertexes bulge parm in shader '%s'\n", shader.name );
			return;
		}
		ds->bulgeSpeed = Q_atof( token );

		ds->deformation = deform_t::DEFORM_BULGE;
		return;
	}

	if ( !Q_stricmp( token, "wave" ) )
	{
		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}

		if ( Q_atof( token ) != 0 )
		{
			ds->deformationSpread = 1.0f / Q_atof( token );
		}
		else
		{
			ds->deformationSpread = 100.0f;
			Com_Printf (S_COLOR_YELLOW  "WARNING: illegal div value of 0 in deformVertexes command for shader '%s'\n", shader.name );
		}

		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = deform_t::DEFORM_WAVE;
		return;
	}

	if ( !Q_stricmp( token, "normal" ) )
	{
		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}
		ds->deformationWave.amplitude = Q_atof( token );

		token = COM_ParseExt( text, false );
		if ( token[0] == 0 )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
			return;
		}
		ds->deformationWave.frequency = Q_atof( token );

		ds->deformation = deform_t::DEFORM_NORMALS;
		return;
	}

	if ( !Q_stricmp( token, "move" ) ) {
		int		i;

		for ( i = 0 ; i < 3 ; i++ ) {
			token = COM_ParseExt( text, false );
			if ( token[0] == 0 ) {
				Com_Printf (S_COLOR_YELLOW  "WARNING: missing deformVertexes parm in shader '%s'\n", shader.name );
				return;
			}
			ds->moveVector[i] = Q_atof( token );
		}

		ParseWaveForm( text, &ds->deformationWave );
		ds->deformation = deform_t::DEFORM_MOVE;
		return;
	}

	Com_Printf (S_COLOR_YELLOW  "WARNING: unknown deformVertexes subtype '%s' found in shader '%s'\n", token, shader.name );
}


/*
===============
ParseSkyParms

skyParms <outerbox> <cloudheight> <innerbox>
===============
*/
static void ParseSkyParms( const char **text ) {
	char		*token;
	const char	*suf[6] = {"rt", "lf", "bk", "ft", "up", "dn"};
	char		pathname[MAX_QPATH];
	int			i;

	shader.sky = (skyParms_t *)Hunk_Alloc( sizeof( skyParms_t ), ha_pref::h_low );

	// outerbox
	token = COM_ParseExt( text, false );
	if ( token[0] == 0 ) {
		Com_Printf (S_COLOR_YELLOW  "WARNING: 'skyParms' missing parameter in shader '%s'\n", shader.name );
		return;
	}
	if ( strcmp( token, "-" ) ) {
		for (i=0 ; i<6 ; i++) {
			Com_sprintf( pathname, sizeof(pathname), "%s_%s", token, suf[i] );
			shader.sky->outerbox[i] = nullptr;
		}
	}

	// cloudheight
	token = COM_ParseExt( text, false );
	if ( token[0] == 0 ) {
		Com_Printf (S_COLOR_YELLOW  "WARNING: 'skyParms' missing cloudheight in shader '%s'\n", shader.name );
		return;
	}
	shader.sky->cloudHeight = Q_atof( token );
	if ( !shader.sky->cloudHeight ) {
		shader.sky->cloudHeight = 512;
	}

	// innerbox
	token = COM_ParseExt( text, false );
	if ( strcmp( token, "-" ) ) {
		Com_Printf (S_COLOR_YELLOW  "WARNING: in shader '%s' 'skyParms', innerbox is not supported!", shader.name);
	}
}


/*
=================
ParseSort
=================
*/
static void ParseSort( const char **text ) {
	char	*token;

	token = COM_ParseExt( text, false );
	if ( token[0] == 0 ) {
		Com_Printf (S_COLOR_YELLOW  "WARNING: missing sort parameter in shader '%s'\n", shader.name );
		return;
	}

	if ( !Q_stricmp( token, "portal" ) ) {
		shader.sort = to_underlying( shaderSort_t::SS_PORTAL );
	} else if ( !Q_stricmp( token, "sky" ) ) {
		shader.sort = to_underlying( shaderSort_t::SS_ENVIRONMENT );
	} else if ( !Q_stricmp( token, "opaque" ) ) {
		shader.sort = to_underlying( shaderSort_t::SS_OPAQUE );
	} else if ( !Q_stricmp( token, "decal" ) ) {
		shader.sort = to_underlying( shaderSort_t::SS_DECAL );
	} else if ( !Q_stricmp( token, "seeThrough" ) ) {
		shader.sort = to_underlying( shaderSort_t::SS_SEE_THROUGH );
	} else if ( !Q_stricmp( token, "banner" ) ) {
		shader.sort = to_underlying( shaderSort_t::SS_BANNER );
	} else if ( !Q_stricmp( token, "additive" ) ) {
		shader.sort = to_underlying( shaderSort_t::SS_BLEND1 );
	} else if ( !Q_stricmp( token, "nearest" ) ) {
		shader.sort = to_underlying( shaderSort_t::SS_NEAREST );
	} else if ( !Q_stricmp( token, "underwater" ) ) {
		shader.sort = to_underlying( shaderSort_t::SS_UNDERWATER );
	} else if ( !Q_stricmp( token, "inside" ) ) {
		shader.sort = to_underlying( shaderSort_t::SS_INSIDE );
	} else if ( !Q_stricmp( token, "mid_inside" ) ) {
		shader.sort = to_underlying( shaderSort_t::SS_MID_INSIDE );
	} else if ( !Q_stricmp( token, "middle" ) ) {
		shader.sort = to_underlying( shaderSort_t::SS_MIDDLE );
	} else if ( !Q_stricmp( token, "mid_outside" ) ) {
		shader.sort = to_underlying( shaderSort_t::SS_MID_OUTSIDE );
	} else if ( !Q_stricmp( token, "outside" ) ) {
		shader.sort = to_underlying( shaderSort_t::SS_OUTSIDE );
	}
	else {
		shader.sort = Q_atof( token );
	}
}

/*
=================
ParseMaterial
=================
*/
EnumArray<const char *, surfaceMaterial_t, surfaceMaterial_t::MATERIAL_LAST> materialNames =
{
	MATERIALS
};

void ParseMaterial( const char **text )
{
	char	*token;
	surfaceMaterial_t	i;

	token = COM_ParseExt( text, false );
	if ( token[0] == 0 )
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: missing material in shader '%s'\n", shader.name );
		return;
	}
	for(i = surfaceMaterial_t::MATERIAL_NONE; i < surfaceMaterial_t::MATERIAL_LAST; i++)
	{
		if ( !Q_stricmp( token, materialNames[i] ) )
		{
			shader.surfaceFlags |= SURF_MaterialBits( i );
			break;
		}
	}
}


// this table is also present in q3map

typedef struct infoParm_s {
	const char	*name;
	contentsFlags_t	clearSolid;
	surfaceFlags_t	surfaceFlags;
	contentsFlags_t	contents;
} infoParm_t;

infoParm_t	infoParms[] = {
	// Game content Flags
	{ "nonsolid",		~contentsFlags_t::CONTENTS_SOLID,					surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_NONE },		// special hack to clear solid flag
	{ "nonopaque",		~contentsFlags_t::CONTENTS_OPAQUE,					surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_NONE },		// special hack to clear opaque flag
	{ "lava",			~contentsFlags_t::CONTENTS_SOLID,					surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_LAVA },		// very damaging
	{ "slime",			~contentsFlags_t::CONTENTS_SOLID,					surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_SLIME },		// mildly damaging
	{ "water",			~contentsFlags_t::CONTENTS_SOLID,					surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_WATER },		//
	{ "fog",			~contentsFlags_t::CONTENTS_SOLID,					surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_FOG},			// carves surfaces entering
	{ "shotclip",		~contentsFlags_t::CONTENTS_SOLID,					surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_SHOTCLIP },	// block shots, but not people
	{ "playerclip",		~(contentsFlags_t::CONTENTS_SOLID|contentsFlags_t::CONTENTS_OPAQUE),	surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_PLAYERCLIP },	// block only the player
	{ "monsterclip",	~(contentsFlags_t::CONTENTS_SOLID|contentsFlags_t::CONTENTS_OPAQUE),	surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_MONSTERCLIP },	//
	{ "botclip",		~(contentsFlags_t::CONTENTS_SOLID|contentsFlags_t::CONTENTS_OPAQUE),	surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_BOTCLIP },		// for bots
	{ "trigger",		~(contentsFlags_t::CONTENTS_SOLID|contentsFlags_t::CONTENTS_OPAQUE),	surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_TRIGGER },		//
	{ "nodrop",			~(contentsFlags_t::CONTENTS_SOLID|contentsFlags_t::CONTENTS_OPAQUE),	surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_NODROP },		// don't drop items or leave bodies (death fog, lava, etc)
	{ "terrain",		~(contentsFlags_t::CONTENTS_SOLID|contentsFlags_t::CONTENTS_OPAQUE),	surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_TERRAIN },		// use special terrain collsion
	{ "ladder",			~(contentsFlags_t::CONTENTS_SOLID|contentsFlags_t::CONTENTS_OPAQUE),	surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_LADDER },		// climb up in it like water
	{ "abseil",			~(contentsFlags_t::CONTENTS_SOLID|contentsFlags_t::CONTENTS_OPAQUE),	surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_ABSEIL },		// can abseil down this brush
	{ "outside",		~(contentsFlags_t::CONTENTS_SOLID|contentsFlags_t::CONTENTS_OPAQUE),	surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_OUTSIDE },		// volume is considered to be in the outside (i.e. not indoors)
	{ "inside",			~(contentsFlags_t::CONTENTS_SOLID|contentsFlags_t::CONTENTS_OPAQUE),	surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_INSIDE },		// volume is considered to be inside (i.e. indoors)

	{ "detail",			contentsFlags_t::CONTENTS_ALL,						surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_DETAIL },		// don't include in structural bsp
	{ "trans",			contentsFlags_t::CONTENTS_ALL,						surfaceFlags_t::SURF_NONE,			contentsFlags_t::CONTENTS_TRANSLUCENT },	// surface has an alpha component

	/* Game surface flags */
	{ "sky",			contentsFlags_t::CONTENTS_ALL,						surfaceFlags_t::SURF_SKY,			contentsFlags_t::CONTENTS_NONE },		// emit light from an environment map
	{ "slick",			contentsFlags_t::CONTENTS_ALL,						surfaceFlags_t::SURF_SLICK,			contentsFlags_t::CONTENTS_NONE },		//

	{ "nodamage",		contentsFlags_t::CONTENTS_ALL,						surfaceFlags_t::SURF_NODAMAGE,		contentsFlags_t::CONTENTS_NONE },		//
	{ "noimpact",		contentsFlags_t::CONTENTS_ALL,						surfaceFlags_t::SURF_NOIMPACT,		contentsFlags_t::CONTENTS_NONE },		// don't make impact explosions or marks
	{ "nomarks",		contentsFlags_t::CONTENTS_ALL,						surfaceFlags_t::SURF_NOMARKS,		contentsFlags_t::CONTENTS_NONE },		// don't make impact marks, but still explode
	{ "nodraw",			contentsFlags_t::CONTENTS_ALL,						surfaceFlags_t::SURF_NODRAW,		contentsFlags_t::CONTENTS_NONE },		// don't generate a drawsurface (or a lightmap)
	{ "nosteps",		contentsFlags_t::CONTENTS_ALL,						surfaceFlags_t::SURF_NOSTEPS,		contentsFlags_t::CONTENTS_NONE },		//
	{ "nodlight",		contentsFlags_t::CONTENTS_ALL,						surfaceFlags_t::SURF_NODLIGHT,		contentsFlags_t::CONTENTS_NONE },		// don't ever add dynamic lights
	{ "metalsteps",		contentsFlags_t::CONTENTS_ALL,						surfaceFlags_t::SURF_METALSTEPS,	contentsFlags_t::CONTENTS_NONE },		//
	{ "nomiscents",		contentsFlags_t::CONTENTS_ALL,						surfaceFlags_t::SURF_NOMISCENTS,	contentsFlags_t::CONTENTS_NONE },		// No misc ents on this surface
	{ "forcefield",		contentsFlags_t::CONTENTS_ALL,						surfaceFlags_t::SURF_FORCEFIELD,	contentsFlags_t::CONTENTS_NONE },		//
	{ "forcesight",		contentsFlags_t::CONTENTS_ALL,						surfaceFlags_t::SURF_FORCESIGHT,	contentsFlags_t::CONTENTS_NONE },		// only visible with force sight
};


/*
===============
ParseSurfaceParm

surfaceparm <name>
===============
*/
static void ParseSurfaceParm( const char **text ) {
	char	*token;
	int		numInfoParms = sizeof(infoParms) / sizeof(infoParms[0]);
	int		i;

	token = COM_ParseExt( text, false );
	for ( i = 0 ; i < numInfoParms ; i++ ) {
		if ( !Q_stricmp( token, infoParms[i].name ) ) {
			shader.surfaceFlags |= infoParms[i].surfaceFlags;
			shader.contentFlags |= infoParms[i].contents;
			shader.contentFlags &= infoParms[i].clearSolid;
			break;
		}
	}
}

/*
=================
ParseShader

The current text pointer is at the explicit text definition of the
shader.  Parse it into the global shader variable.  Later functions
will optimize it.
=================
*/
static bool ParseShader( const char **text )
{
	char *token;
	int s;

	s = 0;

	token = COM_ParseExt( text, true );
	if ( token[0] != '{' )
	{
		Com_Printf (S_COLOR_YELLOW  "WARNING: expecting '{', found '%s' instead in shader '%s'\n", token, shader.name );
		return false;
	}

	while ( 1 )
	{
		token = COM_ParseExt( text, true );
		if ( !token[0] )
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: no concluding '}' in shader %s\n", shader.name );
			return false;
		}

		// end of shader definition
		if ( token[0] == '}' )
		{
			break;
		}
		// stage definition
		else if ( token[0] == '{' )
		{
			if ( s >= MAX_SHADER_STAGES ) {
				ri.Printf( printParm_t::PRINT_WARNING, "WARNING: too many stages in shader %s (max is %i)\n", shader.name, MAX_SHADER_STAGES );
				return false;
			}

			if ( !ParseStage( &stages[s], text ) )
			{
				return false;
			}
			stages[s].active = true;
			if ( stages[s].glow )
			{
				shader.hasGlow = true;
			}
			s++;
			continue;
		}
		// skip stuff that only the QuakeEdRadient needs
		else if ( !Q_stricmpn( token, "qer", 3 ) ) {
			SkipRestOfLine( text );
			continue;
		}
		// material deprecated as of 11 Jan 01
		// material undeprecated as of 7 May 01 - q3map_material deprecated
		else if ( !Q_stricmp( token, "material" ) || !Q_stricmp( token, "q3map_material" ) )
		{
			ParseMaterial( text );
		}
		// sun parms
		else if ( !Q_stricmp( token, "sun" ) || !Q_stricmp( token, "q3map_sun" ) || !Q_stricmp( token, "q3map_sunExt" ) )
		{
			token = COM_ParseExt( text, false );
			tr.sunLight[0] = Q_atof( token );
			token = COM_ParseExt( text, false );
			tr.sunLight[1] = Q_atof( token );
			token = COM_ParseExt( text, false );
			tr.sunLight[2] = Q_atof( token );

			VectorNormalize( tr.sunLight );

			token = COM_ParseExt( text, false );
			float a = Q_atof( token );
			VectorScale( tr.sunLight, a, tr.sunLight);

			token = COM_ParseExt( text, false );
			a = Q_atof( token );
			a = a / 180 * Q_PI;

			token = COM_ParseExt( text, false );
			float b = Q_atof( token );
			b = b / 180 * Q_PI;

			tr.sunDirection[0] = Q_cos( a ) * Q_cos( b );
			tr.sunDirection[1] = Q_sin( a ) * Q_cos( b );
			tr.sunDirection[2] = Q_sin( b );

			SkipRestOfLine( text );
			continue;
		}
		// q3map_surfacelight deprecated as of 16 Jul 01
		else if ( !Q_stricmp( token, "surfacelight" ) || !Q_stricmp( token, "q3map_surfacelight" ) )
		{
			token = COM_ParseExt( text, false );
			tr.sunSurfaceLight = atoi( token );
		}
		else if ( !Q_stricmp( token, "lightColor" ) )
		{
			/*
			if ( !ParseVector( text, 3, tr.sunAmbient ) )
			{
				return false;
			}
			*/
			//SP skips this so I'm skipping it here too.
			SkipRestOfLine( text );
			continue;
		}
		else if ( !Q_stricmp( token, "deformvertexes" ) || !Q_stricmp( token, "deform" )) {
			ParseDeform( text );
			continue;
		}
		else if ( !Q_stricmp( token, "tesssize" ) ) {
			SkipRestOfLine( text );
			continue;
		}
		else if ( !Q_stricmp( token, "clampTime" ) ) {
			token = COM_ParseExt( text, false );
			if (token[0]) {
				shader.clampTime = Q_atof(token);
			}
		}
		// skip stuff that only the q3map needs
		else if ( !Q_stricmpn( token, "q3map", 5 ) ) {
			SkipRestOfLine( text );
			continue;
		}
		// skip stuff that only q3map or the server needs
		else if ( !Q_stricmp( token, "surfaceParm" ) ) {
			ParseSurfaceParm( text );
			continue;
		}
		// no mip maps
		else if ( !Q_stricmp( token, "nomipmaps" ) )
		{
			shader.noMipMaps = true;
			shader.noPicMip = true;
			continue;
		}
		// no picmip adjustment
		else if ( !Q_stricmp( token, "nopicmip" ) )
		{
			shader.noPicMip = true;
			continue;
		}
		else if ( !Q_stricmp( token, "noglfog" ) )
		{
			shader.fogPass = fogPass_t::FP_NONE;
			continue;
		}
		// polygonOffset
		else if ( !Q_stricmp( token, "polygonOffset" ) )
		{
			shader.polygonOffset = true;
			continue;
		}
		else if ( !Q_stricmp( token, "noTC" ) )
		{
			shader.noTC = true;
			continue;
		}
		// entityMergable, allowing sprite surfaces from multiple entities
		// to be merged into one batch.  This is a savings for smoke
		// puffs and blood, but can't be used for anything where the
		// shader calcs (not the surface function) reference the entity color or scroll
		else if ( !Q_stricmp( token, "entityMergable" ) )
		{
			shader.entityMergable = true;
			continue;
		}
		// fogParms
		else if ( !Q_stricmp( token, "fogParms" ) )
		{
			shader.fogParms = (fogParms_t *)Hunk_Alloc( sizeof( fogParms_t ), ha_pref::h_low );
			if ( !ParseVector( text, 3, shader.fogParms->color ) ) {
				return false;
			}

			token = COM_ParseExt( text, false );
			if ( !token[0] )
			{
				Com_Printf (S_COLOR_YELLOW  "WARNING: missing parm for 'fogParms' keyword in shader '%s'\n", shader.name );
				continue;
			}
			shader.fogParms->depthForOpaque = Q_atof( token );

			// skip any old gradient directions
			SkipRestOfLine( text );
			continue;
		}
		// portal
		else if ( !Q_stricmp(token, "portal") )
		{
			shader.sort = to_underlying( shaderSort_t::SS_PORTAL );
			continue;
		}
		// skyparms <cloudheight> <outerbox> <innerbox>
		else if ( !Q_stricmp( token, "skyparms" ) )
		{
			ParseSkyParms( text );
			continue;
		}
		// light <value> determines flaring in q3map, not needed here
		else if ( !Q_stricmp(token, "light") )
		{
			token = COM_ParseExt( text, false );
			continue;
		}
		// cull <face>
		else if ( !Q_stricmp( token, "cull") )
		{
			token = COM_ParseExt( text, false );
			if ( token[0] == 0 )
			{
				Com_Printf (S_COLOR_YELLOW  "WARNING: missing cull parms in shader '%s'\n", shader.name );
				continue;
			}

			if ( !Q_stricmp( token, "none" ) || !Q_stricmp( token, "twosided" ) || !Q_stricmp( token, "disable" ) )
			{
				shader.cullType = cullType_t::CT_TWO_SIDED;
			}
			else if ( !Q_stricmp( token, "back" ) || !Q_stricmp( token, "backside" ) || !Q_stricmp( token, "backsided" ) )
			{
				shader.cullType = cullType_t::CT_BACK_SIDED;
			}
			else
			{
				Com_Printf (S_COLOR_YELLOW  "WARNING: invalid cull parm '%s' in shader '%s'\n", token, shader.name );
			}
			continue;
		}
		// sort
		else if ( !Q_stricmp( token, "sort" ) )
		{
			ParseSort( text );
			continue;
		}
		else
		{
			Com_Printf (S_COLOR_YELLOW  "WARNING: unknown general shader parameter '%s' in '%s'\n", token, shader.name );
			return false;
		}
	}

	//
	// ignore shaders that don't have any stages, unless it is a sky or fog
	//
	if ( s == 0 && !shader.sky && !Any( shader.contentFlags & contentsFlags_t::CONTENTS_FOG ) ) {
		return false;
	}

	shader.explicitlyDefined = true;

	return true;
}

/*
========================================================================================

SHADER OPTIMIZATION AND FOGGING

========================================================================================
*/

typedef struct collapse_s {
	int		blendA;
	int		blendB;

	int		multitextureEnv;
	int		multitextureBlend;
} collapse_t;

/*
================
CollapseMultitexture

Attempt to combine two stages into a single multitexture stage
FIXME: I think modulated add + modulated add collapses incorrectly
=================
*/
static bool CollapseMultitexture( void ) {
	return false;
}

/*
=============

FixRenderCommandList
https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=493
Arnout: this is a nasty issue. Shaders can be registered after drawsurfaces are generated
but before the frame is rendered. This will, for the duration of one frame, cause drawsurfaces
to be rendered with bad shaders. To fix this, need to go through all render commands and fix
sortedIndex.
==============
*/
extern bool gServerSkinHack;
static void FixRenderCommandList( int newShader ) {
}

/*
==============
SortNewShader

Positions the most recently created shader in the tr.sortedShaders[]
array so that the shader->sort key is sorted reletive to the other
shaders.

Sets shader->sortedIndex
==============
*/
static void SortNewShader( void ) {
	int		i;
	float	sort;
	shader_t	*newShader;

	newShader = tr.shaders[ tr.numShaders - 1 ];
	sort = newShader->sort;

	for ( i = tr.numShaders - 2 ; i >= 0 ; i-- ) {
		if ( tr.sortedShaders[ i ]->sort <= sort ) {
			break;
		}
		tr.sortedShaders[i+1] = tr.sortedShaders[i];
		tr.sortedShaders[i+1]->sortedIndex++;
	}

	// Arnout: fix rendercommandlist
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=493
	FixRenderCommandList( i+1 );

	newShader->sortedIndex = i+1;
	tr.sortedShaders[i+1] = newShader;
}


/*
====================
GeneratePermanentShader
====================
*/
static shader_t *GeneratePermanentShader( void ) {
	shader_t	*newShader;
	int			i, b;
	int			size;

	if ( tr.numShaders == MAX_SHADERS ) {
		//Com_Printf (S_COLOR_YELLOW  "WARNING: GeneratePermanentShader - MAX_SHADERS hit\n");
		Com_Printf( "WARNING: GeneratePermanentShader - MAX_SHADERS hit\n");
		return tr.defaultShader;
	}

	newShader = (struct shader_s *)ri.Hunk_Alloc( sizeof( shader_t ), ha_pref::h_low );

	*newShader = shader;

	if ( shader.sort <= /*shaderSort_t::SS_OPAQUE*/(float)shaderSort_t::SS_SEE_THROUGH ) {
		newShader->fogPass = fogPass_t::FP_EQUAL;
	} else if ( Any( shader.contentFlags & contentsFlags_t::CONTENTS_FOG ) ) {
		newShader->fogPass = fogPass_t::FP_LE;
	}

	tr.shaders[ tr.numShaders ] = newShader;
	newShader->index = tr.numShaders;

	tr.sortedShaders[ tr.numShaders ] = newShader;
	newShader->sortedIndex = tr.numShaders;

	tr.numShaders++;

	size = newShader->numUnfoggedPasses ? newShader->numUnfoggedPasses * sizeof( stages[0] ) : sizeof( stages[0] );
	newShader->stages = (shaderStage_t *) Hunk_Alloc( size, ha_pref::h_low );

	for ( i = 0 ; i < newShader->numUnfoggedPasses ; i++ ) {
		if ( !stages[i].active ) {
			break;
		}
		newShader->stages[i] = stages[i];

		for ( b = 0 ; b < NUM_TEXTURE_BUNDLES ; b++ ) {
			if (newShader->stages[i].bundle[b].numTexMods)
			{
				size = newShader->stages[i].bundle[b].numTexMods * sizeof( texModInfo_t );
				newShader->stages[i].bundle[b].texMods = (texModInfo_t *)Hunk_Alloc( size, ha_pref::h_low );
				memcpy( newShader->stages[i].bundle[b].texMods, stages[i].bundle[b].texMods, size );
			}
			else
			{
				newShader->stages[i].bundle[b].texMods = nullptr;	//clear the globabl ptr jic
			}
		}
	}

	SortNewShader();

	const int hash = generateHashValue(newShader->name, FILE_HASH_SIZE);
	newShader->next = hashTable[hash];
	hashTable[hash] = newShader;

	return newShader;
}

/*
=================
VertexLightingCollapse

If vertex lighting is enabled, only render a single
pass, trying to guess which is the correct one to best approximate
what it is supposed to look like.

  OUTPUT:  Number of stages after the collapse (in the case of surfacesprites this isn't one).
=================
*/
//rww - no longer used, at least for now. destroys alpha shaders completely.
#if 0
static int VertexLightingCollapse( void ) {
	int		stage, nextopenstage;
	shaderStage_t	*bestStage;
	int		bestImageRank;
	int		rank;
	int		finalstagenum=1;

	// if we aren't opaque, just use the first pass
	if ( shader.sort == to_underlying( shaderSort_t::SS_OPAQUE ) ) {

		// pick the best texture for the single pass
		bestStage = &stages[0];
		bestImageRank = -999999;

		for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ ) {
			shaderStage_t *pStage = &stages[stage];

			if ( !pStage->active ) {
				break;
			}
			rank = 0;

			if ( pStage->bundle[0].isLightmap ) {
				rank -= 100;
			}
			if ( pStage->bundle[0].tcGen != texCoordGen_t::TCGEN_TEXTURE ) {
				rank -= 5;
			}
			if ( pStage->bundle[0].numTexMods ) {
				rank -= 5;
			}
			if ( pStage->rgbGen != colorGen_t::CGEN_IDENTITY && pStage->rgbGen != colorGen_t::CGEN_IDENTITY_LIGHTING ) {
				rank -= 3;
			}

			// SurfaceSprites are most certainly NOT desireable as the collapsed surface texture.
			if ( pStage->ss && pstage->ss->surfaceSpriteType)
			{
				rank -= 1000;
			}

			if ( rank > bestImageRank  ) {
				bestImageRank = rank;
				bestStage = pStage;
			}
		}

		stages[0].bundle[0] = bestStage->bundle[0];
		stages[0].stateBits &= ~( glStateBits_t::GLS_DSTBLEND_BITS | glStateBits_t::GLS_SRCBLEND_BITS );
		stages[0].stateBits |= glStateBits_t::GLS_DEPTHMASK_TRUE;
		if ( shader.lightmapIndex[0] == LIGHTMAP_NONE ) {
			stages[0].rgbGen = colorGen_t::CGEN_LIGHTING_DIFFUSE;
		} else {
			stages[0].rgbGen = colorGen_t::CGEN_EXACT_VERTEX;
		}
		stages[0].alphaGen = alphaGen_t::AGEN_SKIP;
	} else {
		// don't use a lightmap (tesla coils)
		if ( stages[0].bundle[0].isLightmap ) {
			stages[0] = stages[1];
		}

		// if we were in a cross-fade cgen, hack it to normal
		if ( stages[0].rgbGen == colorGen_t::CGEN_ONE_MINUS_ENTITY || stages[1].rgbGen == colorGen_t::CGEN_ONE_MINUS_ENTITY ) {
			stages[0].rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
		}
		if ( ( stages[0].rgbGen == colorGen_t::CGEN_WAVEFORM && stages[0].rgbWave.func == genFunc_t::GF_SAWTOOTH )
			&& ( stages[1].rgbGen == colorGen_t::CGEN_WAVEFORM && stages[1].rgbWave.func == genFunc_t::GF_INVERSE_SAWTOOTH ) ) {
			stages[0].rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
		}
		if ( ( stages[0].rgbGen == colorGen_t::CGEN_WAVEFORM && stages[0].rgbWave.func == genFunc_t::GF_INVERSE_SAWTOOTH )
			&& ( stages[1].rgbGen == colorGen_t::CGEN_WAVEFORM && stages[1].rgbWave.func == genFunc_t::GF_SAWTOOTH ) ) {
			stages[0].rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
		}
	}

	for ( stage=1, nextopenstage=1; stage < MAX_SHADER_STAGES; stage++ ) {
		shaderStage_t *pStage = &stages[stage];

		if ( !pStage->active ) {
			break;
		}

		if ( pStage->ss && pstage->ss->surfaceSpriteType)
		{
			// Copy this stage to the next open stage list (that is, we don't want any inactive stages before this one)
			if (nextopenstage != stage)
			{
				stages[nextopenstage] = *pStage;
				stages[nextopenstage].bundle[0] = pStage->bundle[0];
			}
			nextopenstage++;
			finalstagenum++;
			continue;
		}

		memset( pStage, 0, sizeof( *pStage ) );
	}

	return finalstagenum;
}
#endif

/*
=========================
FinishShader

Returns a freshly allocated shader with all the needed info
from the current global working shader
=========================
*/
static shader_t *FinishShader( void ) {
	int				stage, lmStage, stageIndex; //rwwRMG - stageIndex for AGEN_BLEND
	bool		hasLightmapStage;
	bool		vertexLightmap;

	hasLightmapStage = false;
	vertexLightmap = false;

	//
	// set sky stuff appropriate
	//
	if ( shader.sky ) {
		shader.sort = to_underlying( shaderSort_t::SS_ENVIRONMENT );
	}

	//
	// set polygon offset
	//
	if ( shader.polygonOffset && !shader.sort ) {
		shader.sort = to_underlying( shaderSort_t::SS_DECAL );
	}

	for(lmStage = 0; lmStage < MAX_SHADER_STAGES; lmStage++)
	{
		shaderStage_t *pStage = &stages[lmStage];
		if (pStage->active && pStage->bundle[0].isLightmap)
		{
			break;
		}
	}

	if (lmStage < MAX_SHADER_STAGES)
	{
		if (shader.lightmapIndex[0] == LIGHTMAP_BY_VERTEX)
		{
			if (lmStage == 0)	//< MAX_SHADER_STAGES-1)
			{//copy the rest down over the lightmap slot
				memmove(&stages[lmStage], &stages[lmStage+1], sizeof(shaderStage_t) * (MAX_SHADER_STAGES-lmStage-1));
				memset(&stages[MAX_SHADER_STAGES-1], 0, sizeof(shaderStage_t));
				//change blending on the moved down stage
				stages[lmStage].stateBits = glStateBits_t::GLS_DEFAULT;
			}
			//change anything that was moved down (or the *white if LM is first) to use vertex color
			stages[lmStage].rgbGen = colorGen_t::CGEN_EXACT_VERTEX;
			stages[lmStage].alphaGen = alphaGen_t::AGEN_SKIP;
			lmStage = MAX_SHADER_STAGES;	//skip the style checking below
		}
	}

	if (lmStage < MAX_SHADER_STAGES)// && !r_fullbright->value)
	{
		int	numStyles;
		int	i;

		for(numStyles=0;numStyles<MAXLIGHTMAPS;numStyles++)
		{
			if (shader.styles[numStyles] >= LS_UNUSED)
			{
				break;
			}
		}
		numStyles--;
		if (numStyles > 0)
		{
			for(i=MAX_SHADER_STAGES-1;i>lmStage+numStyles;i--)
			{
				stages[i] = stages[i-numStyles];
			}

			for(i=0;i<numStyles;i++)
			{
				stages[lmStage+i+1] = stages[lmStage];
				if (shader.lightmapIndex[i+1] == LIGHTMAP_BY_VERTEX)
				{
					stages[lmStage+i+1].bundle[0].image = tr.whiteImage;
				}
				else if (shader.lightmapIndex[i+1] < 0)
				{
					Com_Error( errorParm_t::ERR_DROP, "FinishShader: light style with no light map or vertex color for shader %s", shader.name);
				}
				else
				{
					stages[lmStage+i+1].bundle[0].image = tr.lightmaps[shader.lightmapIndex[i+1]];
					stages[lmStage+i+1].bundle[0].tcGen = texCoordGen_t::TCGEN_LIGHTMAP+i+1;
				}
				stages[lmStage+i+1].rgbGen = colorGen_t::CGEN_LIGHTMAPSTYLE;
				stages[lmStage+i+1].stateBits &= ~(glStateBits_t::GLS_SRCBLEND_BITS | glStateBits_t::GLS_DSTBLEND_BITS);
				stages[lmStage+i+1].stateBits |= glStateBits_t::GLS_SRCBLEND_ONE | glStateBits_t::GLS_DSTBLEND_ONE;
			}
		}

		for(i=0;i<=numStyles;i++)
		{
			stages[lmStage+i].lightmapStyle = shader.styles[i];
		}
	}

	//
	// set appropriate stage information
	//
	stageIndex = 0; //rwwRMG - needed for AGEN_BLEND
	for ( stage = 0; stage < MAX_SHADER_STAGES; ) {
		shaderStage_t *pStage = &stages[stage];

		if ( !pStage->active ) {
			break;
		}

    // check for a missing texture
		if ( !pStage->bundle[0].image ) {
			Com_Printf (S_COLOR_YELLOW  "Shader %s has a stage with no image\n", shader.name );
			pStage->active = false;
			stage++;
			continue;
		}

		//
		// ditch this stage if it's detail and detail textures are disabled
		//
		if ( pStage->isDetail && !r_detailTextures->integer ) {
			int index;

			for ( index = stage + 1; index<MAX_SHADER_STAGES; index++ ) {
				if ( !stages[index].active )
					break;
			}

			if ( index < MAX_SHADER_STAGES )
				memmove( pStage, pStage + 1, sizeof( *pStage ) * (index - stage) );
			else {
				if ( stage + 1 < MAX_SHADER_STAGES )
					memmove( pStage, pStage + 1, sizeof( *pStage ) * (index - stage - 1) );

				Com_Memset( &stages[index - 1], 0, sizeof( *stages ) );
			}

			continue;
		}

		pStage->index = stageIndex; //rwwRMG - needed for AGEN_BLEND

		//
		// default texture coordinate generation
		//
		if ( pStage->bundle[0].isLightmap ) {
			if ( pStage->bundle[0].tcGen == texCoordGen_t::TCGEN_BAD ) {
				pStage->bundle[0].tcGen = texCoordGen_t::TCGEN_LIGHTMAP;
			}
			hasLightmapStage = true;
		} else {
			if ( pStage->bundle[0].tcGen == texCoordGen_t::TCGEN_BAD ) {
				pStage->bundle[0].tcGen = texCoordGen_t::TCGEN_TEXTURE;
			}
		}


    // not a true lightmap but we want to leave existing
    // behaviour in place and not print out a warning
    //if (pStage->rgbGen == colorGen_t::CGEN_VERTEX) {
    //  vertexLightmap = true;
    //}



		//
		// determine sort order and fog color adjustment
		//
		if ( Any( pStage->stateBits & ( glStateBits_t::GLS_SRCBLEND_BITS | glStateBits_t::GLS_DSTBLEND_BITS ) ) &&
			 Any( stages[0].stateBits & ( glStateBits_t::GLS_SRCBLEND_BITS | glStateBits_t::GLS_DSTBLEND_BITS ) ) ) {
			glStateBits_t blendSrcBits = pStage->stateBits & glStateBits_t::GLS_SRCBLEND_BITS;
			glStateBits_t blendDstBits = pStage->stateBits & glStateBits_t::GLS_DSTBLEND_BITS;

			// fog color adjustment only works for blend modes that have a contribution
			// that aproaches 0 as the modulate values aproach 0 --
			// GL_ONE, GL_ONE
			// GL_ZERO, GL_ONE_MINUS_SRC_COLOR
			// GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA

			// modulate, additive
			if ( ( ( blendSrcBits == glStateBits_t::GLS_SRCBLEND_ONE ) && ( blendDstBits == glStateBits_t::GLS_DSTBLEND_ONE ) ) ||
				( ( blendSrcBits == glStateBits_t::GLS_SRCBLEND_ZERO ) && ( blendDstBits == glStateBits_t::GLS_DSTBLEND_ONE_MINUS_SRC_COLOR ) ) ) {
				pStage->adjustColorsForFog = acff_t::ACFF_MODULATE_RGB;
			}
			// strict blend
			else if ( ( blendSrcBits == glStateBits_t::GLS_SRCBLEND_SRC_ALPHA ) && ( blendDstBits == glStateBits_t::GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) )
			{
				pStage->adjustColorsForFog = acff_t::ACFF_MODULATE_ALPHA;
			}
			// premultiplied alpha
			else if ( ( blendSrcBits == glStateBits_t::GLS_SRCBLEND_ONE ) && ( blendDstBits == glStateBits_t::GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) )
			{
				pStage->adjustColorsForFog = acff_t::ACFF_MODULATE_RGBA;
			} else {
				// we can't adjust this one correctly, so it won't be exactly correct in fog
			}

			// don't screw with sort order if this is a portal or environment
			if ( !shader.sort ) {
				// see through item, like a grill or grate
				if ( Any( pStage->stateBits & glStateBits_t::GLS_DEPTHMASK_TRUE ) )
				{
					shader.sort = to_underlying( shaderSort_t::SS_SEE_THROUGH );
				}
				else
				{
					/*
					if (( blendSrcBits == glStateBits_t::GLS_SRCBLEND_ONE ) && ( blendDstBits == glStateBits_t::GLS_DSTBLEND_ONE ))
					{
						// GL_ONE GL_ONE needs to come a bit later
						shader.sort = to_underlying( shaderSort_t::SS_BLEND2 );
					}
					else if (( blendSrcBits == glStateBits_t::GLS_SRCBLEND_SRC_ALPHA ) && ( blendDstBits == glStateBits_t::GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ))
					{ //rww - Pushed SS_BLEND1 up to SS_BLEND2, inserting this so that saber glow will render above water and things.
					  //Unfortunately it still affects other shaders with the same blend settings, but it seems more or less alright.
						shader.sort = to_underlying( shaderSort_t::SS_BLEND1 );
					}
					else
					{
						shader.sort = to_underlying( shaderSort_t::SS_BLEND0 );
					}
					*/
					if (( blendSrcBits == glStateBits_t::GLS_SRCBLEND_ONE ) && ( blendDstBits == glStateBits_t::GLS_DSTBLEND_ONE ))
					{
						// GL_ONE GL_ONE needs to come a bit later
						shader.sort = to_underlying( shaderSort_t::SS_BLEND1 );
					}
					else
					{
						shader.sort = to_underlying( shaderSort_t::SS_BLEND0 );
					}
				}
			}
		}

		//rww - begin hw fog
		if ((pStage->stateBits & (glStateBits_t::GLS_SRCBLEND_BITS|glStateBits_t::GLS_DSTBLEND_BITS)) == (glStateBits_t::GLS_SRCBLEND_ONE|glStateBits_t::GLS_DSTBLEND_ONE))
		{
			pStage->mGLFogColorOverride = EGLFogOverride::GLFOGOVERRIDE_BLACK;
		}
		else if ((pStage->stateBits & (glStateBits_t::GLS_SRCBLEND_BITS|glStateBits_t::GLS_DSTBLEND_BITS)) == (glStateBits_t::GLS_SRCBLEND_SRC_ALPHA|glStateBits_t::GLS_DSTBLEND_ONE) &&
			pStage->alphaGen == alphaGen_t::AGEN_LIGHTING_SPECULAR && stage)
		{
			pStage->mGLFogColorOverride = EGLFogOverride::GLFOGOVERRIDE_BLACK;
		}
		else if ((pStage->stateBits & (glStateBits_t::GLS_SRCBLEND_BITS|glStateBits_t::GLS_DSTBLEND_BITS)) == (glStateBits_t::GLS_SRCBLEND_ZERO|glStateBits_t::GLS_DSTBLEND_ZERO))
		{
			pStage->mGLFogColorOverride = EGLFogOverride::GLFOGOVERRIDE_WHITE;
		}
		else if ((pStage->stateBits & (glStateBits_t::GLS_SRCBLEND_BITS|glStateBits_t::GLS_DSTBLEND_BITS)) == (glStateBits_t::GLS_SRCBLEND_ONE|glStateBits_t::GLS_DSTBLEND_ZERO))
		{
			pStage->mGLFogColorOverride = EGLFogOverride::GLFOGOVERRIDE_WHITE;
		}
		else if (!Any(pStage->stateBits & (glStateBits_t::GLS_SRCBLEND_BITS|glStateBits_t::GLS_DSTBLEND_BITS)) && stage)
		{	//
			pStage->mGLFogColorOverride = EGLFogOverride::GLFOGOVERRIDE_WHITE;
		}
		else if (!Any(pStage->stateBits & (glStateBits_t::GLS_SRCBLEND_BITS|glStateBits_t::GLS_DSTBLEND_BITS)) && pStage->bundle[0].isLightmap && stage < MAX_SHADER_STAGES-1 &&
			stages[stage+1].bundle[0].isLightmap)
		{	// multiple light map blending
			pStage->mGLFogColorOverride = EGLFogOverride::GLFOGOVERRIDE_WHITE;
		}
		else if ((pStage->stateBits & (glStateBits_t::GLS_SRCBLEND_BITS|glStateBits_t::GLS_DSTBLEND_BITS)) == (glStateBits_t::GLS_SRCBLEND_DST_COLOR|glStateBits_t::GLS_DSTBLEND_ZERO) && pStage->bundle[0].isLightmap)
		{ //I don't know, it works. -rww
			pStage->mGLFogColorOverride = EGLFogOverride::GLFOGOVERRIDE_WHITE;
		}
		else if ((pStage->stateBits & (glStateBits_t::GLS_SRCBLEND_BITS|glStateBits_t::GLS_DSTBLEND_BITS)) == (glStateBits_t::GLS_SRCBLEND_DST_COLOR|glStateBits_t::GLS_DSTBLEND_ZERO))
		{ //I don't know, it works. -rww
			pStage->mGLFogColorOverride = EGLFogOverride::GLFOGOVERRIDE_BLACK;
		}
		else if ((pStage->stateBits & (glStateBits_t::GLS_SRCBLEND_BITS|glStateBits_t::GLS_DSTBLEND_BITS)) == (glStateBits_t::GLS_SRCBLEND_ONE|glStateBits_t::GLS_DSTBLEND_ONE_MINUS_SRC_COLOR))
		{ //I don't know, it works. -rww
			pStage->mGLFogColorOverride = EGLFogOverride::GLFOGOVERRIDE_BLACK;
		}
		else
		{
			pStage->mGLFogColorOverride = EGLFogOverride::GLFOGOVERRIDE_NONE;
		}
		//rww - end hw fog

		stageIndex++; //rwwRMG - needed for AGEN_BLEND
		stage++;
	}

	// there are times when you will need to manually apply a sort to
	// opaque alpha tested shaders that have later blend passes
	if ( !shader.sort ) {
		shader.sort = to_underlying( shaderSort_t::SS_OPAQUE );
	}

	//
	// if we are in r_vertexLight mode, never use a lightmap texture
	//
	if ( stage > 1 && (r_vertexLight->integer && !r_uiFullScreen->integer) ) {
		//stage = VertexLightingCollapse();
		//rww - since this does bad things, I am commenting it out for now. If you want to attempt a fix, feel free.
		hasLightmapStage = false;
	}

	//
	// look for multitexture potential
	//
	if ( stage > 1 && CollapseMultitexture() ) {
		stage--;
	}

	if ( shader.lightmapIndex[0] >= 0 && !hasLightmapStage )
	{
		if (vertexLightmap)
		{
//			ri.DPrintf( "WARNING: shader '%s' has VERTEX forced lightmap!\n", shader.name );
		}
		else
		{
			Com_DPrintf ( "WARNING: shader '%s' has lightmap but no lightmap stage!\n", shader.name );
			memcpy(shader.lightmapIndex, lightmapsNone, sizeof(shader.lightmapIndex));
			memcpy(shader.styles, stylesDefault, sizeof(shader.styles));
		}
	}


	//
	// compute number of passes
	//
	shader.numUnfoggedPasses = stage;

	// fogonly shaders don't have any normal passes
	if ( stage == 0 && !shader.sky ) {
		shader.sort = to_underlying( shaderSort_t::SS_FOG );
	}

	for ( stage = 1; stage < shader.numUnfoggedPasses; stage++ )
	{
		// Make sure stage is non detail and active
		if(stages[stage].isDetail || !stages[stage].active)
		{
			break;
		}
		// MT lightmaps are always in bundle 1
		if(stages[stage].bundle[0].isLightmap)
		{
			continue;
		}
	}

	return GeneratePermanentShader();
}

//========================================================================================

/*
====================
FindShaderInShaderText

Scans the combined text description of all the shader files for
the given shader name.

return nullptr if not found

If found, it will return a valid shader
=====================
*/
static const char *FindShaderInShaderText( const char *shadername ) {
	char *token;
	const char *p;

	int i, hash;

	hash = generateHashValue(shadername, MAX_SHADERTEXT_HASH);

	if ( shaderTextHashTable[hash] ) {
		for (i = 0; shaderTextHashTable[hash][i]; i++) {
			p = shaderTextHashTable[hash][i];
			token = COM_ParseExt(&p, true);
			if ( !Q_stricmp( token, shadername ) )
				return p;
		}
	}

	p = s_shaderText;

	if ( !p ) {
		return nullptr;
	}

	// look for label
	while ( 1 ) {
		token = COM_ParseExt( &p, true );
		if ( token[0] == 0 ) {
			break;
		}

		if ( !Q_stricmp( token, shadername ) ) {
			return p;
		}
		else {
			// skip the definition
			SkipBracedSection( &p, 0 );
		}
	}

	return nullptr;
}


/*
==================
R_FindShaderByName

Will always return a valid shader, but it might be the
default shader if the real one can't be found.
==================
*/
shader_t *R_FindShaderByName( const char *name ) {
	char		strippedName[MAX_QPATH];
	int			hash;
	shader_t	*sh;

	if ( (name==nullptr) || (name[0] == 0) ) {
		return tr.defaultShader;
	}

	COM_StripExtension( name, strippedName, sizeof( strippedName ) );

	hash = generateHashValue(strippedName, FILE_HASH_SIZE);

	//
	// see if the shader is already loaded
	//
	for (sh=hashTable[hash]; sh; sh=sh->next) {
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if (Q_stricmp(sh->name, strippedName) == 0) {
			// match found
			return sh;
		}
	}

	return tr.defaultShader;
}


inline bool IsShader(shader_t *sh, const char *name, const int *lightmapIndex, const byte *styles)
{
	int	i;

	if (Q_stricmp(sh->name, name))
	{
		return false;
	}

	if (!sh->defaultShader)
	{
		for(i=0;i<MAXLIGHTMAPS;i++)
		{
			if (sh->lightmapIndex[i] != lightmapIndex[i])
			{
				return false;
			}
			if (sh->styles[i] != styles[i])
			{
				return false;
			}
		}
	}

	return true;
}

/*
===============
R_FindShader

Will always return a valid shader, but it might be the
default shader if the real one can't be found.

In the interest of not requiring an explicit shader text entry to
be defined for every single image used in the game, three default
shader behaviors can be auto-created for any image:

If lightmapIndex == LIGHTMAP_NONE, then the image will have
dynamic diffuse lighting applied to it, as appropriate for most
entity skin surfaces.

If lightmapIndex == LIGHTMAP_2D, then the image will be used
for 2D rendering unless an explicit shader is found

If lightmapIndex == LIGHTMAP_BY_VERTEX, then the image will use
the vertex rgba modulate values, as appropriate for misc_model
pre-lit surfaces.

Other lightmapIndex values will have a lightmap stage created
and src*dest blending applied with the texture, as appropriate for
most world construction surfaces.

===============
*/
shader_t *R_FindShader( const char *name, const int *lightmapIndex, const byte *styles, bool mipRawImage )
{
	char		strippedName[MAX_QPATH];
	char		fileName[MAX_QPATH];
	int			hash;
	const char	*shaderText;
	shader_t	*sh;

	if ( name[0] == 0 ) {
		return tr.defaultShader;
	}

	// use (fullbright) vertex lighting if the bsp file doesn't have
	// lightmaps
	if ( lightmapIndex[0] >= 0 && lightmapIndex[0] >= tr.numLightmaps )
	{
		lightmapIndex = lightmapsVertex;
	}
	else if ( lightmapIndex[0] < LIGHTMAP_2D )
	{
		// negative lightmap indexes cause stray pointers (think tr.lightmaps[lightmapIndex])
		ri.Printf( printParm_t::PRINT_WARNING, "WARNING: shader '%s' has invalid lightmap index of %d\n", name, lightmapIndex[0] );
		lightmapIndex = lightmapsVertex;
	}

	COM_StripExtension( name, strippedName, sizeof( strippedName ) );

	hash = generateHashValue(strippedName, FILE_HASH_SIZE);

	//
	// see if the shader is already loaded
	//
	for (sh = hashTable[hash]; sh; sh = sh->next) {
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if (IsShader(sh, strippedName, lightmapIndex, styles))
		{
			return sh;
		}
	}

	// clear the global shader
	ClearGlobalShader();
	Q_strncpyz(shader.name, strippedName, sizeof(shader.name));
	Com_Memcpy(shader.lightmapIndex, lightmapIndex, sizeof(shader.lightmapIndex));
	Com_Memcpy(shader.styles, styles, sizeof(shader.styles));

	//
	// attempt to define shader from an explicit parameter file
	//
	shaderText = FindShaderInShaderText( strippedName );
	if ( shaderText ) {
		if ( !ParseShader( &shaderText ) ) {
			// had errors, so use default shader
			shader.defaultShader = true;
		}
		sh = FinishShader();
		return sh;
	}


	//
	// if not defined in the in-memory shader descriptions,
	// look for a single TGA, BMP, or PCX
	//
	COM_StripExtension(name,fileName, sizeof( fileName ));
	shader.defaultShader = true;
	return FinishShader();
}

shader_t *R_FindServerShader( const char *name, const int *lightmapIndex, const byte *styles, bool mipRawImage )
{
	char		strippedName[MAX_QPATH];
	int			hash;
	shader_t	*sh;

	if ( name[0] == 0 ) {
		return tr.defaultShader;
	}

	COM_StripExtension( name, strippedName, sizeof( strippedName ) );

	hash = generateHashValue(strippedName, FILE_HASH_SIZE);

	//
	// see if the shader is already loaded
	//
	for (sh = hashTable[hash]; sh; sh = sh->next) {
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if (IsShader(sh, strippedName, lightmapIndex, styles))
		{
			return sh;
		}
	}

	// clear the global shader
	ClearGlobalShader();
	Q_strncpyz(shader.name, strippedName, sizeof(shader.name));
	memcpy(shader.lightmapIndex, lightmapIndex, sizeof(shader.lightmapIndex));
	memcpy(shader.styles, styles, sizeof(shader.styles));

	shader.defaultShader = true;
	return FinishShader();
}

qhandle_t RE_RegisterShaderFromImage(const char *name, int *lightmapIndex, byte *styles, image_t *image, bool mipRawImage) {
	int			i, hash;
	shader_t	*sh;

	hash = generateHashValue(name, FILE_HASH_SIZE);

	// probably not necessary since this function
	// only gets called from tr_font.c with lightmapIndex == LIGHTMAP_2D
	// but better safe than sorry.
	// Doesn't actually ever get called in JA at all
	if ( lightmapIndex[0] >= tr.numLightmaps ) {
		lightmapIndex = (int *)lightmapsFullBright;
	}

	//
	// see if the shader is already loaded
	//
	for (sh=hashTable[hash]; sh; sh=sh->next) {
		// NOTE: if there was no shader or image available with the name strippedName
		// then a default shader is created with lightmapIndex == LIGHTMAP_NONE, so we
		// have to check all default shaders otherwise for every call to R_FindShader
		// with that same strippedName a new default shader is created.
		if (IsShader(sh, name, lightmapIndex, styles))
		{
			return sh->index;
		}
	}

	// clear the global shader
	memset( &shader, 0, sizeof( shader ) );
	memset( &stages, 0, sizeof( stages ) );
	Q_strncpyz(shader.name, name, sizeof(shader.name));
	memcpy(shader.lightmapIndex, lightmapIndex, sizeof(shader.lightmapIndex));
	memcpy(shader.styles, styles, sizeof(shader.styles));

	for ( i = 0 ; i < MAX_SHADER_STAGES ; i++ ) {
		stages[i].bundle[0].texMods = texMods[i];
	}

	//
	// create the default shading commands
	//
	if ( shader.lightmapIndex[0] == LIGHTMAP_NONE ) {
		// dynamic colors at vertexes
		stages[0].bundle[0].image = image;
		stages[0].active = true;
		stages[0].rgbGen = colorGen_t::CGEN_LIGHTING_DIFFUSE;
		stages[0].stateBits = glStateBits_t::GLS_DEFAULT;
	} else if ( shader.lightmapIndex[0] == LIGHTMAP_BY_VERTEX ) {
		// explicit colors at vertexes
		stages[0].bundle[0].image = image;
		stages[0].active = true;
		stages[0].rgbGen = colorGen_t::CGEN_EXACT_VERTEX;
		stages[0].alphaGen = alphaGen_t::AGEN_SKIP;
		stages[0].stateBits = glStateBits_t::GLS_DEFAULT;
	} else if ( shader.lightmapIndex[0] == LIGHTMAP_2D ) {
		// GUI elements
		stages[0].bundle[0].image = image;
		stages[0].active = true;
		stages[0].rgbGen = colorGen_t::CGEN_VERTEX;
		stages[0].alphaGen = alphaGen_t::AGEN_VERTEX;
		stages[0].stateBits = glStateBits_t::GLS_DEPTHTEST_DISABLE |
			  glStateBits_t::GLS_SRCBLEND_SRC_ALPHA |
			  glStateBits_t::GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	} else if ( shader.lightmapIndex[0] == LIGHTMAP_WHITEIMAGE ) {
		// fullbright level
		stages[0].bundle[0].image = tr.whiteImage;
		stages[0].active = true;
		stages[0].rgbGen = colorGen_t::CGEN_IDENTITY_LIGHTING;
		stages[0].stateBits = glStateBits_t::GLS_DEFAULT;

		stages[1].bundle[0].image = image;
		stages[1].active = true;
		stages[1].rgbGen = colorGen_t::CGEN_IDENTITY;
		stages[1].stateBits |= glStateBits_t::GLS_SRCBLEND_DST_COLOR | glStateBits_t::GLS_DSTBLEND_ZERO;
	} else {
		// two pass lightmap
		stages[0].bundle[0].image = tr.lightmaps[shader.lightmapIndex[0]];
		stages[0].bundle[0].isLightmap = true;
		stages[0].active = true;
		stages[0].rgbGen = colorGen_t::CGEN_IDENTITY;	// lightmaps are scaled on creation
													// for identitylight
		stages[0].stateBits = glStateBits_t::GLS_DEFAULT;

		stages[1].bundle[0].image = image;
		stages[1].active = true;
		stages[1].rgbGen = colorGen_t::CGEN_IDENTITY;
		stages[1].stateBits |= glStateBits_t::GLS_SRCBLEND_DST_COLOR | glStateBits_t::GLS_DSTBLEND_ZERO;
	}

	sh = FinishShader();
  return sh->index;
}


/*
====================
RE_RegisterShader

This is the exported shader entry point for the rest of the system
It will always return an index that will be valid.

This should really only be used for explicit shaders, because there is no
way to ask for different implicit lighting modes (vertex, lightmap, etc)
====================
*/
qhandle_t RE_RegisterShaderLightMap( const char *name, const int *lightmapIndex, const byte *styles )
{
	shader_t	*sh;

	if ( strlen( name ) >= MAX_QPATH ) {
		Com_Printf( "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name, lightmapIndex, styles, true );

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if ( sh->defaultShader ) {
		return 0;
	}

	return sh->index;
}


/*
====================
RE_RegisterShader

This is the exported shader entry point for the rest of the system
It will always return an index that will be valid.

This should really only be used for explicit shaders, because there is no
way to ask for different implicit lighting modes (vertex, lightmap, etc)
====================
*/
qhandle_t RE_RegisterShader( const char *name ) {
	shader_t	*sh;

	if ( strlen( name ) >= MAX_QPATH ) {
		Com_Printf( "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name, lightmaps2d, stylesDefault, true );

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if ( sh->defaultShader ) {
		return 0;
	}

	return sh->index;
}


/*
====================
RE_RegisterShaderNoMip

For menu graphics that should never be picmiped
====================
*/
qhandle_t RE_RegisterShaderNoMip( const char *name ) {
	shader_t	*sh;

	if ( strlen( name ) >= MAX_QPATH ) {
		Com_Printf( "Shader name exceeds MAX_QPATH\n" );
		return 0;
	}

	sh = R_FindShader( name, lightmaps2d, stylesDefault, false );

	// we want to return 0 if the shader failed to
	// load for some reason, but R_FindShader should
	// still keep a name allocated for it, so if
	// something calls RE_RegisterShader again with
	// the same name, we don't try looking for it again
	if ( sh->defaultShader ) {
		return 0;
	}

	return sh->index;
}


//added for ui -rww
const char *RE_ShaderNameFromIndex(int index)
{
	assert(index >= 0 && index < tr.numShaders && tr.shaders[index]);
	return tr.shaders[index]->name;
}


/*
====================
R_GetShaderByHandle

When a handle is passed in by another module, this range checks
it and returns a valid (possibly default) shader_t to be used internally.
====================
*/
shader_t *R_GetShaderByHandle( qhandle_t hShader ) {
	if ( hShader < 0 ) {
		Com_Printf (S_COLOR_YELLOW  "R_GetShaderByHandle: out of range hShader '%d'\n", hShader );
		return tr.defaultShader;
	}
	if ( hShader >= tr.numShaders ) {
		Com_Printf (S_COLOR_YELLOW  "R_GetShaderByHandle: out of range hShader '%d'\n", hShader );
		return tr.defaultShader;
	}
	return tr.shaders[hShader];
}

/*
==================
R_InitShaders
==================
*/
void R_InitShaders(bool server)
{
	//Com_Printf ("Initializing Shaders\n" );

	memset(hashTable, 0, sizeof(hashTable));
}

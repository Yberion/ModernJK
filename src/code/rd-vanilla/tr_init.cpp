/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2005 - 2015, ioquake3 contributors
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

// tr_init.c -- functions that are not called every frame

#include <server/exe_headers.h>

#include "tr_local.h"
#include <rd-common/tr_common.h>
#include "tr_stl.h"
#include <rd-common/tr_font.h>
#include "tr_WorldEffects.h"
#include <shared/rd-common/output/engine/engine.h>

glconfig_t glConfig;
glstate_t glState;

static void GfxInfo_f(void);

cvar_t* r_verbose;
cvar_t* r_ignore;

cvar_t* r_detailTextures;

cvar_t* r_znear;

cvar_t* r_skipBackEnd;

cvar_t* r_measureOverdraw;

cvar_t* r_fastsky;
cvar_t* r_drawSun;
cvar_t* r_dynamiclight;
// rjr - removed for hacking cvar_t	*r_dlightBacks;

cvar_t* r_lodbias;
cvar_t* r_lodscale;

cvar_t* r_norefresh;
cvar_t* r_drawentities;
cvar_t* r_drawworld;
cvar_t* r_drawfog;
cvar_t* r_speeds;
cvar_t* r_fullbright;
cvar_t* r_novis;
cvar_t* r_nocull;
cvar_t* r_facePlaneCull;
cvar_t* r_showcluster;
cvar_t* r_nocurves;

cvar_t* r_dlightStyle;
cvar_t* r_surfaceSprites;
cvar_t* r_surfaceWeather;

cvar_t* r_windSpeed;
cvar_t* r_windAngle;
cvar_t* r_windGust;
cvar_t* r_windDampFactor;
cvar_t* r_windPointForce;
cvar_t* r_windPointX;
cvar_t* r_windPointY;

cvar_t* r_allowExtensions;

cvar_t* r_ext_compressed_textures;
cvar_t* r_ext_compressed_lightmaps;
cvar_t* r_ext_preferred_tc_method;
cvar_t* r_ext_gamma_control;
cvar_t* r_ext_multitexture;
cvar_t* r_ext_compiled_vertex_array;
cvar_t* r_ext_texture_env_add;
cvar_t* r_ext_texture_filter_anisotropic;
cvar_t* r_ext_multisample;
cvar_t* r_outputPrecision;
cvar_t* r_outputDither;

cvar_t* r_DynamicGlow;
cvar_t* r_DynamicGlowPasses;
cvar_t* r_DynamicGlowDelta;
cvar_t* r_DynamicGlowIntensity;
cvar_t* r_DynamicGlowSoft;
cvar_t* r_DynamicGlowWidth;
cvar_t* r_DynamicGlowHeight;

cvar_t* r_ignoreGLErrors;
cvar_t* r_logFile;

cvar_t* r_primitives;
cvar_t* r_texturebits;
cvar_t* r_texturebitslm;

cvar_t* r_lightmap;
cvar_t* r_vertexLight;
cvar_t* r_shadows;
cvar_t* r_shadowRange;
cvar_t* r_flares;
cvar_t* r_nobind;
cvar_t* r_singleShader;
cvar_t* r_colorMipLevels;
cvar_t* r_picmip;
cvar_t* r_showtris;
cvar_t* r_showtriscolor;
cvar_t* r_showsky;
cvar_t* r_shownormals;
cvar_t* r_finish;
cvar_t* r_clear;
cvar_t* r_textureMode;
cvar_t* r_offsetFactor;
cvar_t* r_offsetUnits;
cvar_t* r_gamma;
cvar_t* r_intensity;
cvar_t* r_lockpvs;
cvar_t* r_noportals;
cvar_t* r_portalOnly;

cvar_t* r_subdivisions;
cvar_t* r_lodCurveError;

cvar_t* r_mapOverBrightBits;

cvar_t* r_debugSurface;
cvar_t* r_simpleMipMaps;

cvar_t* r_showImages;

cvar_t* r_ambientScale;
cvar_t* r_directedScale;
cvar_t* r_debugLight;
cvar_t* r_debugSort;
cvar_t* r_debugStyle;

cvar_t* r_modelpoolmegs;

cvar_t* r_noGhoul2;
cvar_t* r_Ghoul2AnimSmooth;
cvar_t* r_Ghoul2UnSqash;
cvar_t* r_Ghoul2TimeBase = nullptr;
cvar_t* r_Ghoul2NoLerp;
cvar_t* r_Ghoul2NoBlend;
cvar_t* r_Ghoul2BlendMultiplier = nullptr;
cvar_t* r_Ghoul2UnSqashAfterSmooth;

cvar_t* broadsword;
cvar_t* broadsword_kickbones;
cvar_t* broadsword_kickorigin;
cvar_t* broadsword_playflop;
cvar_t* broadsword_dontstopanim;
cvar_t* broadsword_waitforshot;
cvar_t* broadsword_smallbbox;
cvar_t* broadsword_extra1;
cvar_t* broadsword_extra2;

cvar_t* broadsword_effcorr;
cvar_t* broadsword_ragtobase;
cvar_t* broadsword_dircap;

// More bullshit needed for the proper modular renderer --eez
cvar_t* sv_mapname;
cvar_t* sv_mapChecksum;
cvar_t* se_language; // JKA
#ifdef JK2_MODE
cvar_t* sp_language; // JK2
#endif
cvar_t* com_buildScript;

cvar_t* r_environmentMapping;
cvar_t* r_screenshotJpegQuality;

#if !defined(__APPLE__)
PFNGLSTENCILOPSEPARATEPROC qglStencilOpSeparate;
#endif

PFNGLACTIVETEXTUREARBPROC qglActiveTextureARB;
PFNGLCLIENTACTIVETEXTUREARBPROC qglClientActiveTextureARB;
PFNGLMULTITEXCOORD2FARBPROC qglMultiTexCoord2fARB;

PFNGLCOMBINERPARAMETERFVNVPROC qglCombinerParameterfvNV;
PFNGLCOMBINERPARAMETERIVNVPROC qglCombinerParameterivNV;
PFNGLCOMBINERPARAMETERFNVPROC qglCombinerParameterfNV;
PFNGLCOMBINERPARAMETERINVPROC qglCombinerParameteriNV;
PFNGLCOMBINERINPUTNVPROC qglCombinerInputNV;
PFNGLCOMBINEROUTPUTNVPROC qglCombinerOutputNV;

PFNGLFINALCOMBINERINPUTNVPROC qglFinalCombinerInputNV;
PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC qglGetCombinerInputParameterfvNV;
PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC qglGetCombinerInputParameterivNV;
PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC qglGetCombinerOutputParameterfvNV;
PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC qglGetCombinerOutputParameterivNV;
PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC qglGetFinalCombinerInputParameterfvNV;
PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC qglGetFinalCombinerInputParameterivNV;

PFNGLPROGRAMSTRINGARBPROC qglProgramStringARB;
PFNGLBINDPROGRAMARBPROC qglBindProgramARB;
PFNGLDELETEPROGRAMSARBPROC qglDeleteProgramsARB;
PFNGLGENPROGRAMSARBPROC qglGenProgramsARB;
PFNGLPROGRAMENVPARAMETER4DARBPROC qglProgramEnvParameter4dARB;
PFNGLPROGRAMENVPARAMETER4DVARBPROC qglProgramEnvParameter4dvARB;
PFNGLPROGRAMENVPARAMETER4FARBPROC qglProgramEnvParameter4fARB;
PFNGLPROGRAMENVPARAMETER4FVARBPROC qglProgramEnvParameter4fvARB;
PFNGLPROGRAMLOCALPARAMETER4DARBPROC qglProgramLocalParameter4dARB;
PFNGLPROGRAMLOCALPARAMETER4DVARBPROC qglProgramLocalParameter4dvARB;
PFNGLPROGRAMLOCALPARAMETER4FARBPROC qglProgramLocalParameter4fARB;
PFNGLPROGRAMLOCALPARAMETER4FVARBPROC qglProgramLocalParameter4fvARB;
PFNGLGETPROGRAMENVPARAMETERDVARBPROC qglGetProgramEnvParameterdvARB;
PFNGLGETPROGRAMENVPARAMETERFVARBPROC qglGetProgramEnvParameterfvARB;
PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC qglGetProgramLocalParameterdvARB;
PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC qglGetProgramLocalParameterfvARB;
PFNGLGETPROGRAMIVARBPROC qglGetProgramivARB;
PFNGLGETPROGRAMSTRINGARBPROC qglGetProgramStringARB;
PFNGLISPROGRAMARBPROC qglIsProgramARB;

PFNGLLOCKARRAYSEXTPROC qglLockArraysEXT;
PFNGLUNLOCKARRAYSEXTPROC qglUnlockArraysEXT;

bool g_bTextureRectangleHack = false;

void RE_SetLightStyle(int style, int color);

void R_Splash()
{
    image_t* pImage = R_FindImageFile("menu/splash", false, false, false, GL_CLAMP);

    if (!pImage) {
        // Can't find the splash image so just clear to black
        qglClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        qglClear(GL_COLOR_BUFFER_BIT);
    }
    else {
        extern void RB_SetGL2D(void);
        RB_SetGL2D();

        GL_Bind(pImage);
        GL_State(glStateBits_t::GLS_SRCBLEND_ONE | glStateBits_t::GLS_DSTBLEND_ZERO);

        const int width = 640;
        const int height = 480;
        const float x1 = 320 - width / 2;
        const float x2 = 320 + width / 2;
        const float y1 = 240 - height / 2;
        const float y2 = 240 + height / 2;

        qglBegin(GL_TRIANGLE_STRIP);
        qglTexCoord2f(0, 0);
        qglVertex2f(x1, y1);
        qglTexCoord2f(1, 0);
        qglVertex2f(x2, y1);
        qglTexCoord2f(0, 1);
        qglVertex2f(x1, y2);
        qglTexCoord2f(1, 1);
        qglVertex2f(x2, y2);
        qglEnd();
    }

    Output_Present();
    ri.Platform_Present();
    Output_DrawScene();
}

/*
** GLW_CheckForExtension

  Cannot use strstr directly to differentiate between (for eg) reg_combiners and reg_combiners2
*/

static void GLW_InitTextureCompression(void)
{
    bool newer_tc, old_tc;
    // Check for available tc methods.
    newer_tc = ri.GL_ExtensionSupported("GL_ARB_texture_compression") && ri.GL_ExtensionSupported("GL_EXT_texture_compression_s3tc");
    old_tc = ri.GL_ExtensionSupported("GL_S3_s3tc");

    if (old_tc) {
        Q_Println("...GL_S3_s3tc available");
    }

    if (newer_tc) {
        Q_Println("...GL_EXT_texture_compression_s3tc available");
    }

    if (!r_ext_compressed_textures->value) {
        // Compressed textures are off
        glConfig.textureCompression = textureCompression_t::TC_NONE;
        Q_Println("...ignoring texture compression");
    }
    else if (!old_tc && !newer_tc) {
        // Requesting texture compression, but no method found
        glConfig.textureCompression = textureCompression_t::TC_NONE;
        Q_Println("...no supported texture compression method found");
        Q_Println(".....ignoring texture compression");
    }
    else {
        // some form of supported texture compression is avaiable, so see if the user has a preference
        if (static_cast<textureCompression_t>(r_ext_preferred_tc_method->integer) == textureCompression_t::TC_NONE) {
            // No preference, so pick the best
            if (newer_tc) {
                Q_Println("...no tc preference specified");
                Q_Println(".....using GL_EXT_texture_compression_s3tc");
                glConfig.textureCompression = textureCompression_t::TC_S3TC_DXT;
            }
            else {
                Q_Println("...no tc preference specified");
                Q_Println(".....using GL_S3_s3tc");
                glConfig.textureCompression = textureCompression_t::TC_S3TC;
            }
        }
        else {
            // User has specified a preference, now see if this request can be honored
            if (old_tc && newer_tc) {
                // both are avaiable, so we can use the desired tc method
                if (static_cast<textureCompression_t>(r_ext_preferred_tc_method->integer) == textureCompression_t::TC_S3TC) {
                    Q_Println("...using preferred tc method, GL_S3_s3tc");
                    glConfig.textureCompression = textureCompression_t::TC_S3TC;
                }
                else {
                    Q_Println("...using preferred tc method, GL_EXT_texture_compression_s3tc");
                    glConfig.textureCompression = textureCompression_t::TC_S3TC_DXT;
                }
            }
            else {
                // Both methods are not available, so this gets trickier
                if (static_cast<textureCompression_t>(r_ext_preferred_tc_method->integer) == textureCompression_t::TC_S3TC) {
                    // Preferring to user older compression
                    if (old_tc) {
                        Q_Println("...using GL_S3_s3tc");
                        glConfig.textureCompression = textureCompression_t::TC_S3TC;
                    }
                    else {
                        // Drat, preference can't be honored
                        Q_Println("...preferred tc method, GL_S3_s3tc not available");
                        Q_Println(".....falling back to GL_EXT_texture_compression_s3tc");
                        glConfig.textureCompression = textureCompression_t::TC_S3TC_DXT;
                    }
                }
                else {
                    // Preferring to user newer compression
                    if (newer_tc) {
                        Q_Println("...using GL_EXT_texture_compression_s3tc");
                        glConfig.textureCompression = textureCompression_t::TC_S3TC_DXT;
                    }
                    else {
                        // Drat, preference can't be honored
                        Q_Println("...preferred tc method, GL_EXT_texture_compression_s3tc not available");
                        Q_Println(".....falling back to GL_S3_s3tc");
                        glConfig.textureCompression = textureCompression_t::TC_S3TC;
                    }
                }
            }
        }
    }
}

/*
===============
GLimp_InitExtensions
===============
*/
extern bool g_bDynamicGlowSupported;
static void GLimp_InitExtensions(void)
{
    if (!r_allowExtensions->integer) {
        Q_Println("*** IGNORING OPENGL EXTENSIONS ***");
        g_bDynamicGlowSupported = false;
        ri.Cvar_Set("r_DynamicGlow", "0");
        return;
    }

    Q_Println("Initializing OpenGL extensions");

    // Select our tc scheme
    GLW_InitTextureCompression();

    // GL_EXT_texture_env_add
    glConfig.textureEnvAddAvailable = false;
    if (ri.GL_ExtensionSupported("GL_EXT_texture_env_add")) {
        if (r_ext_texture_env_add->integer) {
            glConfig.textureEnvAddAvailable = true;
            Q_Println("...using GL_EXT_texture_env_add");
        }
        else {
            glConfig.textureEnvAddAvailable = false;
            Q_Println("...ignoring GL_EXT_texture_env_add");
        }
    }
    else {
        Q_Println("...GL_EXT_texture_env_add not found");
    }

    // GL_EXT_texture_filter_anisotropic
    glConfig.maxTextureFilterAnisotropy = 0;
    if (ri.GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic")) {
        qglGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &glConfig.maxTextureFilterAnisotropy);
        Q_Println("...GL_EXT_texture_filter_anisotropic available");

        if (r_ext_texture_filter_anisotropic->integer > 1) {
            Q_Println("...using GL_EXT_texture_filter_anisotropic");
        }
        else {
            Q_Println("...ignoring GL_EXT_texture_filter_anisotropic");
        }
        ri.Cvar_SetValue("r_ext_texture_filter_anisotropic_avail", glConfig.maxTextureFilterAnisotropy);
        if (r_ext_texture_filter_anisotropic->value > glConfig.maxTextureFilterAnisotropy) {
            ri.Cvar_SetValue("r_ext_texture_filter_anisotropic_avail", glConfig.maxTextureFilterAnisotropy);
        }
    }
    else {
        Q_Println("...GL_EXT_texture_filter_anisotropic not found");
        ri.Cvar_Set("r_ext_texture_filter_anisotropic_avail", "0");
    }

    // GL_EXT_clamp_to_edge
    glConfig.clampToEdgeAvailable = true;
    Q_Println("...using GL_EXT_texture_edge_clamp");

    // GL_ARB_multitexture
    qglMultiTexCoord2fARB = nullptr;
    qglActiveTextureARB = nullptr;
    qglClientActiveTextureARB = nullptr;
    if (ri.GL_ExtensionSupported("GL_ARB_multitexture")) {
        if (r_ext_multitexture->integer) {
            qglMultiTexCoord2fARB = (PFNGLMULTITEXCOORD2FARBPROC)ri.GL_GetProcAddress("glMultiTexCoord2fARB");
            qglActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)ri.GL_GetProcAddress("glActiveTextureARB");
            qglClientActiveTextureARB = (PFNGLCLIENTACTIVETEXTUREARBPROC)ri.GL_GetProcAddress("glClientActiveTextureARB");

            if (qglActiveTextureARB) {
                qglGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &glConfig.maxActiveTextures);

                if (glConfig.maxActiveTextures > 1) {
                    Q_Println("...using GL_ARB_multitexture");
                }
                else {
                    qglMultiTexCoord2fARB = nullptr;
                    qglActiveTextureARB = nullptr;
                    qglClientActiveTextureARB = nullptr;
                    Q_Println("...not using GL_ARB_multitexture, < 2 texture units");
                }
            }
        }
        else {
            Q_Println("...ignoring GL_ARB_multitexture");
        }
    }
    else {
        Q_Println("...GL_ARB_multitexture not found");
    }

    // GL_EXT_compiled_vertex_array
    qglLockArraysEXT = nullptr;
    qglUnlockArraysEXT = nullptr;
    if (ri.GL_ExtensionSupported("GL_EXT_compiled_vertex_array")) {
        if (r_ext_compiled_vertex_array->integer) {
            Q_Println("...using GL_EXT_compiled_vertex_array");
            qglLockArraysEXT = (PFNGLLOCKARRAYSEXTPROC)ri.GL_GetProcAddress("glLockArraysEXT");
            qglUnlockArraysEXT = (PFNGLUNLOCKARRAYSEXTPROC)ri.GL_GetProcAddress("glUnlockArraysEXT");
            if (!qglLockArraysEXT || !qglUnlockArraysEXT) {
                Q_Error(errorParm_t::ERR_FATAL, "bad getprocaddress");
            }
        }
        else {
            Q_Println("...ignoring GL_EXT_compiled_vertex_array");
        }
    }
    else {
        Q_Println("...GL_EXT_compiled_vertex_array not found");
    }

    bool bNVRegisterCombiners = false;
    // Register Combiners.
    if (ri.GL_ExtensionSupported("GL_NV_register_combiners")) {
        // NOTE: This extension requires multitexture support (over 2 units).
        if (glConfig.maxActiveTextures >= 2) {
            bNVRegisterCombiners = true;
            // Register Combiners function pointer address load.	- AReis
            // NOTE: VV guys will _definetly_ not be able to use regcoms. Pixel Shaders are just as good though :-)
            // NOTE: Also, this is an nVidia specific extension (of course), so fragment shaders would serve the same purpose
            // if we needed some kind of fragment/pixel manipulation support.
            qglCombinerParameterfvNV = (PFNGLCOMBINERPARAMETERFVNVPROC)ri.GL_GetProcAddress("glCombinerParameterfvNV");
            qglCombinerParameterivNV = (PFNGLCOMBINERPARAMETERIVNVPROC)ri.GL_GetProcAddress("glCombinerParameterivNV");
            qglCombinerParameterfNV = (PFNGLCOMBINERPARAMETERFNVPROC)ri.GL_GetProcAddress("glCombinerParameterfNV");
            qglCombinerParameteriNV = (PFNGLCOMBINERPARAMETERINVPROC)ri.GL_GetProcAddress("glCombinerParameteriNV");
            qglCombinerInputNV = (PFNGLCOMBINERINPUTNVPROC)ri.GL_GetProcAddress("glCombinerInputNV");
            qglCombinerOutputNV = (PFNGLCOMBINEROUTPUTNVPROC)ri.GL_GetProcAddress("glCombinerOutputNV");
            qglFinalCombinerInputNV = (PFNGLFINALCOMBINERINPUTNVPROC)ri.GL_GetProcAddress("glFinalCombinerInputNV");
            qglGetCombinerInputParameterfvNV = (PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC)ri.GL_GetProcAddress("glGetCombinerInputParameterfvNV");
            qglGetCombinerInputParameterivNV = (PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC)ri.GL_GetProcAddress("glGetCombinerInputParameterivNV");
            qglGetCombinerOutputParameterfvNV = (PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC)ri.GL_GetProcAddress("glGetCombinerOutputParameterfvNV");
            qglGetCombinerOutputParameterivNV = (PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC)ri.GL_GetProcAddress("glGetCombinerOutputParameterivNV");
            qglGetFinalCombinerInputParameterfvNV = (PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC)ri.GL_GetProcAddress("glGetFinalCombinerInputParameterfvNV");
            qglGetFinalCombinerInputParameterivNV = (PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC)ri.GL_GetProcAddress("glGetFinalCombinerInputParameterivNV");

            // Validate the functions we need.
            if (!qglCombinerParameterfvNV || !qglCombinerParameterivNV || !qglCombinerParameterfNV || !qglCombinerParameteriNV || !qglCombinerInputNV ||
                !qglCombinerOutputNV || !qglFinalCombinerInputNV || !qglGetCombinerInputParameterfvNV || !qglGetCombinerInputParameterivNV ||
                !qglGetCombinerOutputParameterfvNV || !qglGetCombinerOutputParameterivNV || !qglGetFinalCombinerInputParameterfvNV ||
                !qglGetFinalCombinerInputParameterivNV) {
                bNVRegisterCombiners = false;
                qglCombinerParameterfvNV = nullptr;
                qglCombinerParameteriNV = nullptr;
                Q_Println("...GL_NV_register_combiners failed");
            }
        }
        else {
            bNVRegisterCombiners = false;
            Q_Println("...ignoring GL_NV_register_combiners");
        }
    }
    else {
        bNVRegisterCombiners = false;
        Q_Println("...GL_NV_register_combiners not found");
    }

    // NOTE: Vertex and Fragment Programs are very dependant on each other - this is actually a
    // good thing! So, just check to see which we support (one or the other) and load the shared
    // function pointers. ARB rocks!

    // Vertex Programs.
    bool bARBVertexProgram = false;
    if (ri.GL_ExtensionSupported("GL_ARB_vertex_program")) {
        bARBVertexProgram = true;
    }
    else {
        bARBVertexProgram = false;
        Q_Println("...GL_ARB_vertex_program not found");
    }

    // Fragment Programs.
    bool bARBFragmentProgram = false;
    if (ri.GL_ExtensionSupported("GL_ARB_fragment_program")) {
        bARBFragmentProgram = true;
    }
    else {
        bARBFragmentProgram = false;
        Q_Println("...GL_ARB_fragment_program not found");
    }

    // If we support one or the other, load the shared function pointers.
    if (bARBVertexProgram || bARBFragmentProgram) {
        qglProgramStringARB = (PFNGLPROGRAMSTRINGARBPROC)ri.GL_GetProcAddress("glProgramStringARB");
        qglBindProgramARB = (PFNGLBINDPROGRAMARBPROC)ri.GL_GetProcAddress("glBindProgramARB");
        qglDeleteProgramsARB = (PFNGLDELETEPROGRAMSARBPROC)ri.GL_GetProcAddress("glDeleteProgramsARB");
        qglGenProgramsARB = (PFNGLGENPROGRAMSARBPROC)ri.GL_GetProcAddress("glGenProgramsARB");
        qglProgramEnvParameter4dARB = (PFNGLPROGRAMENVPARAMETER4DARBPROC)ri.GL_GetProcAddress("glProgramEnvParameter4dARB");
        qglProgramEnvParameter4dvARB = (PFNGLPROGRAMENVPARAMETER4DVARBPROC)ri.GL_GetProcAddress("glProgramEnvParameter4dvARB");
        qglProgramEnvParameter4fARB = (PFNGLPROGRAMENVPARAMETER4FARBPROC)ri.GL_GetProcAddress("glProgramEnvParameter4fARB");
        qglProgramEnvParameter4fvARB = (PFNGLPROGRAMENVPARAMETER4FVARBPROC)ri.GL_GetProcAddress("glProgramEnvParameter4fvARB");
        qglProgramLocalParameter4dARB = (PFNGLPROGRAMLOCALPARAMETER4DARBPROC)ri.GL_GetProcAddress("glProgramLocalParameter4dARB");
        qglProgramLocalParameter4dvARB = (PFNGLPROGRAMLOCALPARAMETER4DVARBPROC)ri.GL_GetProcAddress("glProgramLocalParameter4dvARB");
        qglProgramLocalParameter4fARB = (PFNGLPROGRAMLOCALPARAMETER4FARBPROC)ri.GL_GetProcAddress("glProgramLocalParameter4fARB");
        qglProgramLocalParameter4fvARB = (PFNGLPROGRAMLOCALPARAMETER4FVARBPROC)ri.GL_GetProcAddress("glProgramLocalParameter4fvARB");
        qglGetProgramEnvParameterdvARB = (PFNGLGETPROGRAMENVPARAMETERDVARBPROC)ri.GL_GetProcAddress("glGetProgramEnvParameterdvARB");
        qglGetProgramEnvParameterfvARB = (PFNGLGETPROGRAMENVPARAMETERFVARBPROC)ri.GL_GetProcAddress("glGetProgramEnvParameterfvARB");
        qglGetProgramLocalParameterdvARB = (PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC)ri.GL_GetProcAddress("glGetProgramLocalParameterdvARB");
        qglGetProgramLocalParameterfvARB = (PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC)ri.GL_GetProcAddress("glGetProgramLocalParameterfvARB");
        qglGetProgramivARB = (PFNGLGETPROGRAMIVARBPROC)ri.GL_GetProcAddress("glGetProgramivARB");
        qglGetProgramStringARB = (PFNGLGETPROGRAMSTRINGARBPROC)ri.GL_GetProcAddress("glGetProgramStringARB");
        qglIsProgramARB = (PFNGLISPROGRAMARBPROC)ri.GL_GetProcAddress("glIsProgramARB");

        // Validate the functions we need.
        if (!qglProgramStringARB || !qglBindProgramARB || !qglDeleteProgramsARB || !qglGenProgramsARB || !qglProgramEnvParameter4dARB ||
            !qglProgramEnvParameter4dvARB || !qglProgramEnvParameter4fARB || !qglProgramEnvParameter4fvARB || !qglProgramLocalParameter4dARB ||
            !qglProgramLocalParameter4dvARB || !qglProgramLocalParameter4fARB || !qglProgramLocalParameter4fvARB || !qglGetProgramEnvParameterdvARB ||
            !qglGetProgramEnvParameterfvARB || !qglGetProgramLocalParameterdvARB || !qglGetProgramLocalParameterfvARB || !qglGetProgramivARB ||
            !qglGetProgramStringARB || !qglIsProgramARB) {
            bARBVertexProgram = false;
            bARBFragmentProgram = false;
            qglGenProgramsARB = nullptr; // clear ptrs that get checked
            qglProgramEnvParameter4fARB = nullptr;
            Q_Println("...ignoring GL_ARB_vertex_program");
            Q_Println("...ignoring GL_ARB_fragment_program");
        }
    }

    // Figure out which texture rectangle extension to use.
    bool bTexRectSupported = false;
    if (Q_stricmpn(glConfig.vendor_string, "ATI Technologies", 16) == 0 && Q_stricmpn(glConfig.version_string, "1.3.3", 5) == 0 &&
        glConfig.version_string[5] < '9') // 1.3.34 and 1.3.37 and 1.3.38 are broken for sure, 1.3.39 is not
    {
        g_bTextureRectangleHack = true;
    }

    if (ri.GL_ExtensionSupported("GL_NV_texture_rectangle") || ri.GL_ExtensionSupported("GL_EXT_texture_rectangle")) {
        bTexRectSupported = true;
    }

// Find out how many general combiners they have.
#define GL_MAX_GENERAL_COMBINERS_NV 0x854D
    GLint iNumGeneralCombiners = 0;
    if (bNVRegisterCombiners)
        qglGetIntegerv(GL_MAX_GENERAL_COMBINERS_NV, &iNumGeneralCombiners);

    // Only allow dynamic glows/flares if they have the hardware
    if (bTexRectSupported && bARBVertexProgram && qglActiveTextureARB && glConfig.maxActiveTextures >= 4 &&
        ((bNVRegisterCombiners && iNumGeneralCombiners >= 2) || bARBFragmentProgram)) {
        g_bDynamicGlowSupported = true;
        // this would overwrite any achived setting gwg
        // ri.Cvar_Set( "r_DynamicGlow", "1" );
    }
    else {
        g_bDynamicGlowSupported = false;
        ri.Cvar_Set("r_DynamicGlow", "0");
    }

#if !defined(__APPLE__)
    qglStencilOpSeparate = (PFNGLSTENCILOPSEPARATEPROC)ri.GL_GetProcAddress("glStencilOpSeparate");
    if (qglStencilOpSeparate) {
        glConfig.doStencilShadowsInOneDrawcall = true;
    }
#else
    glConfig.doStencilShadowsInOneDrawcall = true;
#endif
}

/*
** InitOpenGL
**
** This function is responsible for initializing a valid OpenGL subsystem.  This
** is done by calling GLimp_Init (which gives us a working OGL subsystem) then
** setting variables, checking GL constants, and reporting the gfx system config
** to the user.
*/
static void InitOpenGL(void)
{
    //
    // the client's window is open: the renderer asks what it is like and draws in it
    //

    if (glConfig.vidWidth == 0) {
        memset(&glConfig, 0, sizeof(glConfig));

        ri.Platform_Display(&glConfig);

        // get our config strings
        glConfig.vendor_string = (const char*)qglGetString(GL_VENDOR);
        glConfig.renderer_string = (const char*)qglGetString(GL_RENDERER);
        glConfig.version_string = (const char*)qglGetString(GL_VERSION);
        glConfig.extensions_string = (const char*)qglGetString(GL_EXTENSIONS);

        // OpenGL driver constants
        qglGetIntegerv(GL_MAX_TEXTURE_SIZE, &glConfig.maxTextureSize);

        // stubbed or broken drivers may have reported 0...
        glConfig.maxTextureSize = Q_max(0, glConfig.maxTextureSize);

        // initialize extensions
        GLimp_InitExtensions();

        // the picture is drawn into the output stage's scene, then into the window through its pass
        Output_Init();

        // set default state
        GL_SetDefaultState();
        R_Splash(); // get something on screen asap
    }
    else {
        // set default state
        GL_SetDefaultState();
    }
}

/*
==================
GL_CheckErrors
==================
*/
void GL_CheckErrors(void)
{
    int err;
    char s[64];

    err = qglGetError();
    if (err == GL_NO_ERROR) {
        return;
    }
    if (r_ignoreGLErrors->integer) {
        return;
    }
    switch (err) {
    case GL_INVALID_ENUM:
        strcpy(s, "GL_INVALID_ENUM");
        break;
    case GL_INVALID_VALUE:
        strcpy(s, "GL_INVALID_VALUE");
        break;
    case GL_INVALID_OPERATION:
        strcpy(s, "GL_INVALID_OPERATION");
        break;
    case GL_STACK_OVERFLOW:
        strcpy(s, "GL_STACK_OVERFLOW");
        break;
    case GL_STACK_UNDERFLOW:
        strcpy(s, "GL_STACK_UNDERFLOW");
        break;
    case GL_OUT_OF_MEMORY:
        strcpy(s, "GL_OUT_OF_MEMORY");
        break;
    default:
        Q_FormatTo(s, "{}", err);
        break;
    }

    Q_Error(errorParm_t::ERR_FATAL, "GL_CheckErrors: {}", s);
}

/*
==============================================================================

                        SCREEN SHOTS

==============================================================================
*/

/*
==================

RB_ReadPixels

Reads an image but takes care of alignment issues for reading RGB images.

Reads a minimum offset for where the RGB data starts in the image from
integer stored at pointer offset. When the function has returned the actual
offset was written back to address offset. This address will always have an
alignment of packAlign to ensure efficient copying.

Stores the length of padding after a line of pixels to address padlen

Return value must be freed with Hunk_FreeTempMemory()
==================
*/

byte* RB_ReadPixels(int x, int y, int width, int height, size_t* offset, int* padlen)
{
    byte *buffer, *bufstart;
    int padwidth, linelen;
    GLint packAlign;

    qglGetIntegerv(GL_PACK_ALIGNMENT, &packAlign);

    linelen = width * 3;
    padwidth = PAD(linelen, packAlign);

    // Allocate a few more bytes so that we can choose an alignment we like
    buffer = (byte*)R_Malloc(padwidth * height + *offset + packAlign - 1, memtag::TAG_TEMP_WORKSPACE, false);

    bufstart = (byte*)PADP((intptr_t)buffer + *offset, packAlign);
    // the picture as the screen shows it: through the output stage's pass, read in the window's back buffer
    Output_Present();
    qglReadPixels(x, y, width, height, GL_RGB, GL_UNSIGNED_BYTE, bufstart);
    Output_DrawScene();

    *offset = bufstart - buffer;
    *padlen = padwidth - linelen;

    return buffer;
}

/*
==================
R_TakeScreenshot
==================
*/
void R_TakeScreenshot(int x, int y, int width, int height, char* fileName)
{
    byte *allbuf, *buffer;
    byte *srcptr, *destptr;
    byte *endline, *endmem;
    byte temp;

    int linelen, padlen;
    size_t offset = 18, memcount;

    allbuf = RB_ReadPixels(x, y, width, height, &offset, &padlen);
    buffer = allbuf + offset - 18;

    Com_Memset(buffer, 0, 18);
    buffer[2] = 2; // uncompressed type
    buffer[12] = width & 255;
    buffer[13] = width >> 8;
    buffer[14] = height & 255;
    buffer[15] = height >> 8;
    buffer[16] = 24; // pixel size

    // swap rgb to bgr and remove padding from line endings
    linelen = width * 3;

    srcptr = destptr = allbuf + offset;
    endmem = srcptr + (linelen + padlen) * height;

    while (srcptr < endmem) {
        endline = srcptr + linelen;

        while (srcptr < endline) {
            temp = srcptr[0];
            *destptr++ = srcptr[2];
            *destptr++ = srcptr[1];
            *destptr++ = temp;

            srcptr += 3;
        }

        // Skip the pad
        srcptr += padlen;
    }

    memcount = linelen * height;

    ri.FS_WriteFile(fileName, buffer, memcount + 18);

    R_Free(allbuf);
}

/*
==================
R_TakeScreenshotPNG
==================
*/
void R_TakeScreenshotPNG(int x, int y, int width, int height, char* fileName)
{
    byte* buffer = nullptr;
    size_t offset = 0;
    int padlen = 0;

    buffer = RB_ReadPixels(x, y, width, height, &offset, &padlen);
    RE_SavePNG(fileName, buffer, width, height, 3);
    R_Free(buffer);
}

/*
==================
R_TakeScreenshotJPEG
==================
*/
void R_TakeScreenshotJPEG(int x, int y, int width, int height, char* fileName)
{
    byte* buffer;
    size_t offset = 0;
    int padlen;

    buffer = RB_ReadPixels(x, y, width, height, &offset, &padlen);
    RE_SaveJPG(fileName, r_screenshotJpegQuality->integer, width, height, buffer + offset, padlen);
    R_Free(buffer);
}

/*
==================
R_ScreenshotFilename
==================
*/
void R_ScreenshotFilename(char* buf, int bufSize, const char* ext)
{
    time_t rawtime;
    char timeStr[32] = {0}; // should really only reach ~19 chars

    time(&rawtime);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d_%H-%M-%S", localtime(&rawtime)); // or gmtime

    Q_FormatTo(std::span(buf, bufSize), "screenshots/shot{}{}", timeStr, ext);
}

/*
====================
R_LevelShot

levelshots are specialized 256*256 thumbnails for
the menu system, sampled down from full screen distorted images
====================
*/
constexpr int LEVELSHOTSIZE = 256;
static void R_LevelShot(void)
{
    char checkname[MAX_OSPATH];
    byte* buffer;
    byte *source, *allsource;
    byte *src, *dst;
    size_t offset = 0;
    int padlen;
    int x, y;
    int r, g, b;
    float xScale, yScale;
    int xx, yy;

    Q_FormatTo(checkname, "levelshots/{}.tga", tr.world->baseName);

    allsource = RB_ReadPixels(0, 0, glConfig.vidWidth, glConfig.vidHeight, &offset, &padlen);
    source = allsource + offset;

    buffer = (byte*)R_Malloc(LEVELSHOTSIZE * LEVELSHOTSIZE * 3 + 18, memtag::TAG_TEMP_WORKSPACE, false);
    Com_Memset(buffer, 0, 18);
    buffer[2] = 2; // uncompressed type
    buffer[12] = LEVELSHOTSIZE & 255;
    buffer[13] = LEVELSHOTSIZE >> 8;
    buffer[14] = LEVELSHOTSIZE & 255;
    buffer[15] = LEVELSHOTSIZE >> 8;
    buffer[16] = 24; // pixel size

    // resample from source
    xScale = glConfig.vidWidth / (4.0 * LEVELSHOTSIZE);
    yScale = glConfig.vidHeight / (3.0 * LEVELSHOTSIZE);
    for (y = 0; y < LEVELSHOTSIZE; y++) {
        for (x = 0; x < LEVELSHOTSIZE; x++) {
            r = g = b = 0;
            for (yy = 0; yy < 3; yy++) {
                for (xx = 0; xx < 4; xx++) {
                    src = source + 3 * (glConfig.vidWidth * (int)((y * 3 + yy) * yScale) + (int)((x * 4 + xx) * xScale));
                    r += src[0];
                    g += src[1];
                    b += src[2];
                }
            }
            dst = buffer + 18 + 3 * (y * LEVELSHOTSIZE + x);
            dst[0] = b / 12;
            dst[1] = g / 12;
            dst[2] = r / 12;
        }
    }

    ri.FS_WriteFile(checkname, buffer, LEVELSHOTSIZE * LEVELSHOTSIZE * 3 + 18);

    R_Free(buffer);
    R_Free(allsource);

    Q_Println("Wrote {}", checkname);
}

/*
==================
R_ScreenShotTGA_f

screenshot
screenshot [silent]
screenshot [levelshot]
screenshot [filename]

Doesn't print the pacifier message if there is a second arg
==================
*/
void R_ScreenShotTGA_f(void)
{
    char checkname[MAX_OSPATH] = {0};
    bool silent = false;

    if (!strcmp(ri.Cmd_Argv(1), "levelshot")) {
        R_LevelShot();
        return;
    }

    if (!strcmp(ri.Cmd_Argv(1), "silent"))
        silent = true;

    if (ri.Cmd_Argc() == 2 && !silent) {
        // explicit filename
        Q_FormatTo(checkname, "screenshots/{}.tga", ri.Cmd_Argv(1));
    }
    else {
        // timestamp the file
        R_ScreenshotFilename(checkname, sizeof(checkname), ".tga");

        if (ri.FS_FileExists(checkname)) {
            Q_Println("ScreenShot: Couldn't create a file");
            return;
        }
    }

    R_TakeScreenshot(0, 0, glConfig.vidWidth, glConfig.vidHeight, checkname);

    if (!silent)
        Q_Println("Wrote {}", checkname);
}

/*
==================
R_ScreenShotPNG_f

screenshot
screenshot [silent]
screenshot [levelshot]
screenshot [filename]

Doesn't print the pacifier message if there is a second arg
==================
*/
void R_ScreenShotPNG_f(void)
{
    char checkname[MAX_OSPATH] = {0};
    bool silent = false;

    if (!strcmp(ri.Cmd_Argv(1), "levelshot")) {
        R_LevelShot();
        return;
    }

    if (!strcmp(ri.Cmd_Argv(1), "silent"))
        silent = true;

    if (ri.Cmd_Argc() == 2 && !silent) {
        // explicit filename
        Q_FormatTo(checkname, "screenshots/{}.png", ri.Cmd_Argv(1));
    }
    else {
        // timestamp the file
        R_ScreenshotFilename(checkname, sizeof(checkname), ".png");

        if (ri.FS_FileExists(checkname)) {
            Q_Println("ScreenShot: Couldn't create a file");
            return;
        }
    }

    R_TakeScreenshotPNG(0, 0, glConfig.vidWidth, glConfig.vidHeight, checkname);

    if (!silent)
        Q_Println("Wrote {}", checkname);
}

void R_ScreenShot_f(void)
{
    char checkname[MAX_OSPATH] = {0};
    bool silent = false;

    if (!strcmp(ri.Cmd_Argv(1), "levelshot")) {
        R_LevelShot();
        return;
    }
    if (!strcmp(ri.Cmd_Argv(1), "silent"))
        silent = true;

    if (ri.Cmd_Argc() == 2 && !silent) {
        // explicit filename
        Q_FormatTo(checkname, "screenshots/{}.jpg", ri.Cmd_Argv(1));
    }
    else {
        // timestamp the file
        R_ScreenshotFilename(checkname, sizeof(checkname), ".jpg");

        if (ri.FS_FileExists(checkname)) {
            Q_Println("ScreenShot: Couldn't create a file");
            return;
        }
    }

    R_TakeScreenshotJPEG(0, 0, glConfig.vidWidth, glConfig.vidHeight, checkname);

    if (!silent)
        Q_Println("Wrote {}", checkname);
}

//============================================================================

/*
** GL_SetDefaultState
*/
void GL_SetDefaultState(void)
{
    qglClearDepth(1.0f);

    qglCullFace(GL_FRONT);

    qglColor4f(1, 1, 1, 1);

    // initialize downstream texture unit if we're running
    // in a multitexture environment
    if (qglActiveTextureARB) {
        GL_SelectTexture(1);
        GL_TextureMode(r_textureMode->string);
        GL_TexEnv(GL_MODULATE);
        qglDisable(GL_TEXTURE_2D);
        GL_SelectTexture(0);
    }

    qglEnable(GL_TEXTURE_2D);
    GL_TextureMode(r_textureMode->string);
    GL_TexEnv(GL_MODULATE);

    qglShadeModel(GL_SMOOTH);
    qglDepthFunc(GL_LEQUAL);

    // the vertex array is always enabled, but the color and texture
    // arrays are enabled and disabled around the compiled vertex array call
    qglEnableClientState(GL_VERTEX_ARRAY);

    //
    // make sure our GL state vector is set correctly
    //
    glState.glStateBits = glStateBits_t::GLS_DEPTHTEST_DISABLE | glStateBits_t::GLS_DEPTHMASK_TRUE;

    qglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    qglDepthMask(GL_TRUE);
    qglDisable(GL_DEPTH_TEST);
    qglEnable(GL_SCISSOR_TEST);
    qglDisable(GL_CULL_FACE);
    qglDisable(GL_BLEND);
    qglDisable(GL_ALPHA_TEST);
    qglBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

/*
================
R_PrintLongString

Workaround for the print buffer's limit (MAXPRINTMSG characters in the engine).
================
*/
void R_PrintLongString(const char* string)
{
    char buffer[1024];
    const char* p = string;
    int remainingLength = strlen(string);

    while (remainingLength > 0) {
        // Take as much characters as possible from the string without splitting words between buffers
        // This avoids the client console splitting a word up when one half fits on the current line,
        // but the second half would have to be written on a new line
        int charsToTake = sizeof(buffer) - 1;
        if (remainingLength > charsToTake) {
            while (p[charsToTake - 1] > ' ' && p[charsToTake] > ' ') {
                charsToTake--;
                if (charsToTake == 0) {
                    charsToTake = sizeof(buffer) - 1;
                    break;
                }
            }
        }
        else if (remainingLength < charsToTake) {
            charsToTake = remainingLength;
        }

        Q_strncpyz(buffer, p, charsToTake + 1);
        Q_Print("{}", buffer);
        remainingLength -= charsToTake;
        p += charsToTake;
    }
}

/*
================
GfxInfo_f
================
*/
extern bool g_bTextureRectangleHack;

void GfxInfo_f(void)
{
    const char* enablestrings[] = {"disabled", "enabled"};
    const char* fsstrings[] = {"windowed", "fullscreen"};
    const EnumArray<const char*, textureCompression_t, std::to_underlying(textureCompression_t::TC_S3TC_DXT) + 1> tc_table = {
        "None",
        "GL_S3_s3tc",
        "GL_EXT_texture_compression_s3tc",
    };

    Q_Println("\nGL_VENDOR: {}", glConfig.vendor_string);
    Q_Println("GL_RENDERER: {}", glConfig.renderer_string);
    Q_Println("GL_VERSION: {}", glConfig.version_string);
    R_PrintLongString(glConfig.extensions_string);
    Q_Println("");
    Q_Println("GL_MAX_TEXTURE_SIZE: {}", glConfig.maxTextureSize);
    Q_Println("GL_MAX_ACTIVE_TEXTURES_ARB: {}", glConfig.maxActiveTextures);
    Q_Println("\nPIXELFORMAT: color({}-bits) Z({}-bit) stencil({}-bits)", glConfig.colorBits, glConfig.depthBits, glConfig.stencilBits);
    Q_Print("MODE: {} x {} {} hz:", glConfig.vidWidth, glConfig.vidHeight, fsstrings[glConfig.isFullscreen]);
    if (glConfig.displayFrequency) {
        Q_Println("{}", glConfig.displayFrequency);
    }
    else {
        Q_Println("N/A");
    }
    Output_PrintInfo();

    // rendering primitives
    {
        int primitives;

        // default is to use triangles if compiled vertex arrays are present
        Q_Print("rendering primitives: ");
        primitives = r_primitives->integer;
        if (primitives == 0) {
            if (qglLockArraysEXT) {
                primitives = 2;
            }
            else {
                primitives = 1;
            }
        }
        if (primitives == -1) {
            Q_Println("none");
        }
        else if (primitives == 2) {
            Q_Println("single glDrawElements");
        }
        else if (primitives == 1) {
            Q_Println("multiple glArrayElement");
        }
        else if (primitives == 3) {
            Q_Println("multiple glColor4ubv + glTexCoord2fv + glVertex3fv");
        }
    }

    Q_Println("texturemode: {}", r_textureMode->string);
    Q_Println("picmip: {}", r_picmip->integer);
    Q_Println("texture bits: {}", r_texturebits->integer);
    if (r_texturebitslm->integer > 0)
        Q_Println("lightmap texture bits: {}", r_texturebitslm->integer);
    Q_Println("multitexture: {}", enablestrings[qglActiveTextureARB != nullptr]);
    Q_Println("compiled vertex arrays: {}", enablestrings[qglLockArraysEXT != nullptr]);
    Q_Println("texenv add: {}", enablestrings[glConfig.textureEnvAddAvailable != 0]);
    Q_Println("compressed textures: {}", enablestrings[glConfig.textureCompression != textureCompression_t::TC_NONE]);
    Q_Println("compressed lightmaps: {}",
              enablestrings[(r_ext_compressed_lightmaps->integer != 0 && glConfig.textureCompression != textureCompression_t::TC_NONE)]);
    Q_Println("texture compression method: {}", tc_table[glConfig.textureCompression]);
    Q_Print("anisotropic filtering: {}  ", enablestrings[(r_ext_texture_filter_anisotropic->integer != 0) && glConfig.maxTextureFilterAnisotropy]);
    if (r_ext_texture_filter_anisotropic->integer != 0 && glConfig.maxTextureFilterAnisotropy) {
        if (Q_isintegral(r_ext_texture_filter_anisotropic->value))
            Q_Print("({} of ", (int)r_ext_texture_filter_anisotropic->value);
        else
            Q_Print("({:f} of ", r_ext_texture_filter_anisotropic->value);

        if (Q_isintegral(glConfig.maxTextureFilterAnisotropy))
            Q_Println("{})", (int)glConfig.maxTextureFilterAnisotropy);
        else
            Q_Println("{:f})", glConfig.maxTextureFilterAnisotropy);
    }
    Q_Println("Dynamic Glow: {}", enablestrings[r_DynamicGlow->integer ? 1 : 0]);
    if (g_bTextureRectangleHack)
        Q_Println("Dynamic Glow ATI BAD DRIVER HACK {}", enablestrings[g_bTextureRectangleHack]);

    if (r_finish->integer) {
        Q_Println("Forcing glFinish");
    }

    int displayRefresh = ri.Cvar_VariableIntegerValue("r_displayRefresh");
    if (displayRefresh) {
        Q_Println("Display refresh set to {}", displayRefresh);
    }
    if (tr.world) {
        Q_Println("Light Grid size set to ({:.2f} {:.2f} {:.2f})", tr.world->lightGridSize[0], tr.world->lightGridSize[1], tr.world->lightGridSize[2]);
    }
}

void R_AtiHackToggle_f(void)
{
    g_bTextureRectangleHack = !g_bTextureRectangleHack;
}

/************************************************************************************************
 * R_FogDistance_f                                                                              *
 *    Console command to change the global fog opacity distance.  If you specify nothing on the *
 *    command line, it will display the current fog opacity distance.  Specifying a float       *
 *    representing the world units away the fog should be completely opaque will change the     *
 *    value.                                                                                    *
 *                                                                                              *
 * Input                                                                                        *
 *    none                                                                                      *
 *                                                                                              *
 * Output / Return                                                                              *
 *    none                                                                                      *
 *                                                                                              *
 ************************************************************************************************/
void R_FogDistance_f(void)
{
    float distance;

    if (!tr.world) {
        Q_Println("R_FogDistance_f: World is not initialized");
        return;
    }

    if (tr.world->globalFog == -1) {
        Q_Println("R_FogDistance_f: World does not have a global fog");
        return;
    }

    if (ri.Cmd_Argc() <= 1) {
        //		should not ever be 0.0
        //		if (tr.world->fogs[tr.world->globalFog].tcScale == 0.0)
        //		{
        //			distance = 0.0;
        //		}
        //		else
        {
            distance = 1.0 / (8.0 * tr.world->fogs[tr.world->globalFog].tcScale);
        }

        Q_Println("R_FogDistance_f: Current Distance: {:.0f}", distance);
        return;
    }

    if (ri.Cmd_Argc() != 2) {
        Q_Println("R_FogDistance_f: Invalid number of arguments to set distance");
        return;
    }

    distance = Q_atof(ri.Cmd_Argv(1));
    if (distance < 1.0) {
        distance = 1.0;
    }
    tr.world->fogs[tr.world->globalFog].parms.depthForOpaque = distance;
    tr.world->fogs[tr.world->globalFog].tcScale = 1.0 / (distance * 8);
}

/************************************************************************************************
 * R_FogColor_f                                                                                 *
 *    Console command to change the global fog color.  Specifying nothing on the command will   *
 *    display the current global fog color.  Specifying a float R G B values between 0.0 and    *
 *    1.0 will change the fog color.                                                            *
 *                                                                                              *
 * Input                                                                                        *
 *    none                                                                                      *
 *                                                                                              *
 * Output / Return                                                                              *
 *    none                                                                                      *
 *                                                                                              *
 ************************************************************************************************/
void R_FogColor_f(void)
{
    if (!tr.world) {
        Q_Println("R_FogColor_f: World is not initialized");
        return;
    }

    if (tr.world->globalFog == -1) {
        Q_Println("R_FogColor_f: World does not have a global fog");
        return;
    }

    if (ri.Cmd_Argc() <= 1) {
        unsigned i = tr.world->fogs[tr.world->globalFog].colorInt;

        Q_Println("R_FogColor_f: Current Color: {:0f} {:0f} {:0f}", ((byte*)&i)[0] / 255.0, ((byte*)&i)[1] / 255.0, ((byte*)&i)[2] / 255.0);
        return;
    }

    if (ri.Cmd_Argc() != 4) {
        Q_Println("R_FogColor_f: Invalid number of arguments to set color");
        return;
    }

    tr.world->fogs[tr.world->globalFog].parms.color[0] = Q_atof(ri.Cmd_Argv(1));
    tr.world->fogs[tr.world->globalFog].parms.color[1] = Q_atof(ri.Cmd_Argv(2));
    tr.world->fogs[tr.world->globalFog].parms.color[2] = Q_atof(ri.Cmd_Argv(3));
    tr.world->fogs[tr.world->globalFog].colorInt =
        ColorBytes4(Q_atof(ri.Cmd_Argv(1)) * tr.identityLight, Q_atof(ri.Cmd_Argv(2)) * tr.identityLight, Q_atof(ri.Cmd_Argv(3)) * tr.identityLight, 1.0);
}

typedef struct consoleCommand_s {
    const char* cmd;
    xcommand_t func;
} consoleCommand_t;

void R_ReloadFonts_f(void);

static consoleCommand_t commands[] = {
    {"imagelist", R_ImageList_f},
    {"shaderlist", R_ShaderList_f},
    {"skinlist", R_SkinList_f},
    {"fontlist", R_FontList_f},
    {"screenshot", R_ScreenShot_f},
    {"screenshot_png", R_ScreenShotPNG_f},
    {"screenshot_tga", R_ScreenShotTGA_f},
    {"gfxinfo", GfxInfo_f},
    {"r_atihack", R_AtiHackToggle_f},
    {"r_we", R_WorldEffect_f},
    {"imagecacheinfo", RE_RegisterImages_Info_f},
    {"modellist", R_Modellist_f},
    {"modelcacheinfo", RE_RegisterModels_Info_f},
    {"r_fogDistance", R_FogDistance_f},
    {"r_fogColor", R_FogColor_f},
    {"r_reloadfonts", R_ReloadFonts_f},
};

static const size_t numCommands = ARRAY_LEN(commands);

#ifdef _DEBUG
constexpr int MIN_PRIMITIVES = -1;
#else
constexpr int MIN_PRIMITIVES = 0;
#endif
constexpr int MAX_PRIMITIVES = 3;

/*
===============
R_Register
===============
*/
void R_Register(void)
{
    //
    // latched and archived variables
    //

    r_allowExtensions = ri.Cvar_Get("r_allowExtensions", "1", cvarFlags_t::CVAR_ARCHIVE_ND | cvarFlags_t::CVAR_LATCH);
    r_ext_compressed_textures = ri.Cvar_Get("r_ext_compress_textures", "1", cvarFlags_t::CVAR_ARCHIVE_ND | cvarFlags_t::CVAR_LATCH);
    r_ext_compressed_lightmaps = ri.Cvar_Get("r_ext_compress_lightmaps", "0", cvarFlags_t::CVAR_ARCHIVE_ND | cvarFlags_t::CVAR_LATCH);
    r_ext_preferred_tc_method = ri.Cvar_Get("r_ext_preferred_tc_method", "0", cvarFlags_t::CVAR_ARCHIVE_ND | cvarFlags_t::CVAR_LATCH);
    r_ext_gamma_control = ri.Cvar_Get("r_ext_gamma_control", "1", cvarFlags_t::CVAR_ARCHIVE_ND | cvarFlags_t::CVAR_LATCH);
    r_ext_multitexture = ri.Cvar_Get("r_ext_multitexture", "1", cvarFlags_t::CVAR_ARCHIVE_ND | cvarFlags_t::CVAR_LATCH);
    r_ext_compiled_vertex_array = ri.Cvar_Get("r_ext_compiled_vertex_array", "1", cvarFlags_t::CVAR_ARCHIVE_ND | cvarFlags_t::CVAR_LATCH);
    r_ext_texture_env_add = ri.Cvar_Get("r_ext_texture_env_add", "1", cvarFlags_t::CVAR_ARCHIVE_ND | cvarFlags_t::CVAR_LATCH);
    r_ext_texture_filter_anisotropic = ri.Cvar_Get("r_ext_texture_filter_anisotropic", "16", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_ext_multisample = ri.Cvar_Get("r_ext_multisample", "0", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_outputPrecision = ri.Cvar_Get("r_outputPrecision", "0", cvarFlags_t::CVAR_NONE);
    r_outputDither = ri.Cvar_Get("r_outputDither", "0", cvarFlags_t::CVAR_NONE);

    r_DynamicGlow = ri.Cvar_Get("r_DynamicGlow", "0", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_DynamicGlowPasses = ri.Cvar_Get("r_DynamicGlowPasses", "5", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_DynamicGlowDelta = ri.Cvar_Get("r_DynamicGlowDelta", "0.8f", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_DynamicGlowIntensity = ri.Cvar_Get("r_DynamicGlowIntensity", "1.13f", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_DynamicGlowSoft = ri.Cvar_Get("r_DynamicGlowSoft", "1", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_DynamicGlowWidth = ri.Cvar_Get("r_DynamicGlowWidth", "320", cvarFlags_t::CVAR_ARCHIVE_ND | cvarFlags_t::CVAR_LATCH);
    r_DynamicGlowHeight = ri.Cvar_Get("r_DynamicGlowHeight", "240", cvarFlags_t::CVAR_ARCHIVE_ND | cvarFlags_t::CVAR_LATCH);

    r_picmip = ri.Cvar_Get("r_picmip", "0", cvarFlags_t::CVAR_ARCHIVE | cvarFlags_t::CVAR_LATCH);
    ri.Cvar_CheckRange(r_picmip, 0, 16, true);
    r_colorMipLevels = ri.Cvar_Get("r_colorMipLevels", "0", cvarFlags_t::CVAR_LATCH);
    r_detailTextures = ri.Cvar_Get("r_detailtextures", "1", cvarFlags_t::CVAR_ARCHIVE_ND | cvarFlags_t::CVAR_LATCH);
    r_texturebits = ri.Cvar_Get("r_texturebits", "0", cvarFlags_t::CVAR_ARCHIVE_ND | cvarFlags_t::CVAR_LATCH);
    r_texturebitslm = ri.Cvar_Get("r_texturebitslm", "0", cvarFlags_t::CVAR_ARCHIVE_ND | cvarFlags_t::CVAR_LATCH);
    r_mapOverBrightBits = ri.Cvar_Get("r_mapOverBrightBits", "0", cvarFlags_t::CVAR_ARCHIVE_ND | cvarFlags_t::CVAR_LATCH);
    r_simpleMipMaps = ri.Cvar_Get("r_simpleMipMaps", "1", cvarFlags_t::CVAR_ARCHIVE_ND | cvarFlags_t::CVAR_LATCH);
    r_vertexLight = ri.Cvar_Get("r_vertexLight", "0", cvarFlags_t::CVAR_ARCHIVE | cvarFlags_t::CVAR_LATCH);
    r_subdivisions = ri.Cvar_Get("r_subdivisions", "4", cvarFlags_t::CVAR_ARCHIVE_ND | cvarFlags_t::CVAR_LATCH);
    ri.Cvar_CheckRange(r_subdivisions, 0, 80, false);
    r_intensity = ri.Cvar_Get("r_intensity", "1", cvarFlags_t::CVAR_LATCH | cvarFlags_t::CVAR_ARCHIVE_ND);

    //
    // temporary latched variables that can only change over a restart
    //
    r_fullbright = ri.Cvar_Get("r_fullbright", "0", cvarFlags_t::CVAR_LATCH);
    r_singleShader = ri.Cvar_Get("r_singleShader", "0", cvarFlags_t::CVAR_CHEAT | cvarFlags_t::CVAR_LATCH);

    //
    // archived variables that can change at any time
    //
    r_lodCurveError = ri.Cvar_Get("r_lodCurveError", "250", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_lodbias = ri.Cvar_Get("r_lodbias", "0", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_flares = ri.Cvar_Get("r_flares", "1", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_lodscale = ri.Cvar_Get("r_lodscale", "10", cvarFlags_t::CVAR_ARCHIVE_ND);

    r_znear = ri.Cvar_Get("r_znear", "4", cvarFlags_t::CVAR_ARCHIVE_ND); // if set any lower, you lose a lot of precision in the distance
    ri.Cvar_CheckRange(r_znear, 0.001f, 10, false);                      // was true in JA, is false properly in ioq3
    r_ignoreGLErrors = ri.Cvar_Get("r_ignoreGLErrors", "1", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_fastsky = ri.Cvar_Get("r_fastsky", "0", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_drawSun = ri.Cvar_Get("r_drawSun", "0", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_dynamiclight = ri.Cvar_Get("r_dynamiclight", "1", cvarFlags_t::CVAR_ARCHIVE);
    // rjr - removed for hacking
    //	r_dlightBacks = ri.Cvar_Get( "r_dlightBacks", "0", cvarFlags_t::CVAR_ARCHIVE );
    r_finish = ri.Cvar_Get("r_finish", "0", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_textureMode = ri.Cvar_Get("r_textureMode", "GL_LINEAR_MIPMAP_LINEAR", cvarFlags_t::CVAR_ARCHIVE);
    r_gamma = ri.Cvar_Get("r_gamma", "1", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_facePlaneCull = ri.Cvar_Get("r_facePlaneCull", "1", cvarFlags_t::CVAR_ARCHIVE_ND);

    r_dlightStyle = ri.Cvar_Get("r_dlightStyle", "1", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_surfaceSprites = ri.Cvar_Get("r_surfaceSprites", "1", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_surfaceWeather = ri.Cvar_Get("r_surfaceWeather", "0", cvarFlags_t::CVAR_TEMP);

    r_windSpeed = ri.Cvar_Get("r_windSpeed", "0", cvarFlags_t::CVAR_NONE);
    r_windAngle = ri.Cvar_Get("r_windAngle", "0", cvarFlags_t::CVAR_NONE);
    r_windGust = ri.Cvar_Get("r_windGust", "0", cvarFlags_t::CVAR_NONE);
    r_windDampFactor = ri.Cvar_Get("r_windDampFactor", "0.1", cvarFlags_t::CVAR_NONE);
    r_windPointForce = ri.Cvar_Get("r_windPointForce", "0", cvarFlags_t::CVAR_NONE);
    r_windPointX = ri.Cvar_Get("r_windPointX", "0", cvarFlags_t::CVAR_NONE);
    r_windPointY = ri.Cvar_Get("r_windPointY", "0", cvarFlags_t::CVAR_NONE);

    r_primitives = ri.Cvar_Get("r_primitives", "0", cvarFlags_t::CVAR_ARCHIVE_ND);
    ri.Cvar_CheckRange(r_primitives, MIN_PRIMITIVES, MAX_PRIMITIVES, true);

    r_ambientScale = ri.Cvar_Get("r_ambientScale", "0.5", cvarFlags_t::CVAR_CHEAT);
    r_directedScale = ri.Cvar_Get("r_directedScale", "1", cvarFlags_t::CVAR_CHEAT);

    //
    // temporary variables that can change at any time
    //
    r_showImages = ri.Cvar_Get("r_showImages", "0", cvarFlags_t::CVAR_CHEAT);

    r_debugLight = ri.Cvar_Get("r_debuglight", "0", cvarFlags_t::CVAR_TEMP);
    r_debugStyle = ri.Cvar_Get("r_debugStyle", "-1", cvarFlags_t::CVAR_CHEAT);
    r_debugSort = ri.Cvar_Get("r_debugSort", "0", cvarFlags_t::CVAR_CHEAT);

    r_nocurves = ri.Cvar_Get("r_nocurves", "0", cvarFlags_t::CVAR_CHEAT);
    r_drawworld = ri.Cvar_Get("r_drawworld", "1", cvarFlags_t::CVAR_CHEAT);
#ifdef JK2_MODE
    r_drawfog = ri.Cvar_Get("r_drawfog", "1", cvarFlags_t::CVAR_CHEAT);
#else
    r_drawfog = ri.Cvar_Get("r_drawfog", "2", cvarFlags_t::CVAR_CHEAT);
#endif
    r_lightmap = ri.Cvar_Get("r_lightmap", "0", cvarFlags_t::CVAR_CHEAT);
    r_portalOnly = ri.Cvar_Get("r_portalOnly", "0", cvarFlags_t::CVAR_CHEAT);

    r_skipBackEnd = ri.Cvar_Get("r_skipBackEnd", "0", cvarFlags_t::CVAR_CHEAT);

    r_measureOverdraw = ri.Cvar_Get("r_measureOverdraw", "0", cvarFlags_t::CVAR_CHEAT);
    r_norefresh = ri.Cvar_Get("r_norefresh", "0", cvarFlags_t::CVAR_CHEAT);
    r_drawentities = ri.Cvar_Get("r_drawentities", "1", cvarFlags_t::CVAR_CHEAT);
    r_ignore = ri.Cvar_Get("r_ignore", "1", cvarFlags_t::CVAR_TEMP);
    r_nocull = ri.Cvar_Get("r_nocull", "0", cvarFlags_t::CVAR_CHEAT);
    r_novis = ri.Cvar_Get("r_novis", "0", cvarFlags_t::CVAR_CHEAT);
    r_showcluster = ri.Cvar_Get("r_showcluster", "0", cvarFlags_t::CVAR_CHEAT);
    r_speeds = ri.Cvar_Get("r_speeds", "0", cvarFlags_t::CVAR_CHEAT);
    r_verbose = ri.Cvar_Get("r_verbose", "0", cvarFlags_t::CVAR_CHEAT);
    r_logFile = ri.Cvar_Get("r_logFile", "0", cvarFlags_t::CVAR_CHEAT);
    r_debugSurface = ri.Cvar_Get("r_debugSurface", "0", cvarFlags_t::CVAR_CHEAT);
    r_nobind = ri.Cvar_Get("r_nobind", "0", cvarFlags_t::CVAR_CHEAT);
    r_showtris = ri.Cvar_Get("r_showtris", "0", cvarFlags_t::CVAR_CHEAT);
    r_showtriscolor = ri.Cvar_Get("r_showtriscolor", "0", cvarFlags_t::CVAR_ARCHIVE_ND);
    r_showsky = ri.Cvar_Get("r_showsky", "0", cvarFlags_t::CVAR_CHEAT);
    r_shownormals = ri.Cvar_Get("r_shownormals", "0", cvarFlags_t::CVAR_CHEAT);
    r_clear = ri.Cvar_Get("r_clear", "0", cvarFlags_t::CVAR_CHEAT);
    r_offsetFactor = ri.Cvar_Get("r_offsetfactor", "-1", cvarFlags_t::CVAR_CHEAT);
    r_offsetUnits = ri.Cvar_Get("r_offsetunits", "-2", cvarFlags_t::CVAR_CHEAT);
    r_lockpvs = ri.Cvar_Get("r_lockpvs", "0", cvarFlags_t::CVAR_CHEAT);
    r_noportals = ri.Cvar_Get("r_noportals", "0", cvarFlags_t::CVAR_CHEAT);
    r_shadows = ri.Cvar_Get("cg_shadows", "1", cvarFlags_t::CVAR_NONE);
    r_shadowRange = ri.Cvar_Get("r_shadowRange", "1000", cvarFlags_t::CVAR_ARCHIVE_ND);

    /*
    Ghoul2 Insert Start
    */
    r_noGhoul2 = ri.Cvar_Get("r_noghoul2", "0", cvarFlags_t::CVAR_CHEAT);
    r_Ghoul2AnimSmooth = ri.Cvar_Get("r_ghoul2animsmooth", "0.25", cvarFlags_t::CVAR_NONE);
    r_Ghoul2UnSqash = ri.Cvar_Get("r_ghoul2unsquash", "1", cvarFlags_t::CVAR_NONE);
    r_Ghoul2TimeBase = ri.Cvar_Get("r_ghoul2timebase", "2", cvarFlags_t::CVAR_NONE);
    r_Ghoul2NoLerp = ri.Cvar_Get("r_ghoul2nolerp", "0", cvarFlags_t::CVAR_NONE);
    r_Ghoul2NoBlend = ri.Cvar_Get("r_ghoul2noblend", "0", cvarFlags_t::CVAR_NONE);
    r_Ghoul2BlendMultiplier = ri.Cvar_Get("r_ghoul2blendmultiplier", "1", cvarFlags_t::CVAR_NONE);
    r_Ghoul2UnSqashAfterSmooth = ri.Cvar_Get("r_ghoul2unsquashaftersmooth", "1", cvarFlags_t::CVAR_NONE);

    broadsword = ri.Cvar_Get("broadsword", "1", cvarFlags_t::CVAR_NONE);
    broadsword_kickbones = ri.Cvar_Get("broadsword_kickbones", "1", cvarFlags_t::CVAR_NONE);
    broadsword_kickorigin = ri.Cvar_Get("broadsword_kickorigin", "1", cvarFlags_t::CVAR_NONE);
    broadsword_dontstopanim = ri.Cvar_Get("broadsword_dontstopanim", "0", cvarFlags_t::CVAR_NONE);
    broadsword_waitforshot = ri.Cvar_Get("broadsword_waitforshot", "0", cvarFlags_t::CVAR_NONE);
    broadsword_playflop = ri.Cvar_Get("broadsword_playflop", "1", cvarFlags_t::CVAR_NONE);
    broadsword_smallbbox = ri.Cvar_Get("broadsword_smallbbox", "0", cvarFlags_t::CVAR_NONE);
    broadsword_extra1 = ri.Cvar_Get("broadsword_extra1", "0", cvarFlags_t::CVAR_NONE);
    broadsword_extra2 = ri.Cvar_Get("broadsword_extra2", "0", cvarFlags_t::CVAR_NONE);
    broadsword_effcorr = ri.Cvar_Get("broadsword_effcorr", "1", cvarFlags_t::CVAR_NONE);
    broadsword_ragtobase = ri.Cvar_Get("broadsword_ragtobase", "2", cvarFlags_t::CVAR_NONE);
    broadsword_dircap = ri.Cvar_Get("broadsword_dircap", "64", cvarFlags_t::CVAR_NONE);

    /*
    Ghoul2 Insert End
    */

    sv_mapname = ri.Cvar_Get("mapname", "nomap", cvarFlags_t::CVAR_SERVERINFO | cvarFlags_t::CVAR_ROM);
    sv_mapChecksum = ri.Cvar_Get("sv_mapChecksum", "", cvarFlags_t::CVAR_ROM);
    se_language = ri.Cvar_Get("se_language", "english", cvarFlags_t::CVAR_ARCHIVE | cvarFlags_t::CVAR_NORESTART);
#ifdef JK2_MODE
    sp_language =
        ri.Cvar_Get("sp_language", Q_Format("{}", spLanguage_t::SP_LANGUAGE_ENGLISH).c_str(), cvarFlags_t::CVAR_ARCHIVE | cvarFlags_t::CVAR_NORESTART);
#endif
    com_buildScript = ri.Cvar_Get("com_buildScript", "0", cvarFlags_t::CVAR_NONE);

    r_modelpoolmegs = ri.Cvar_Get("r_modelpoolmegs", "20", cvarFlags_t::CVAR_ARCHIVE);
    if (ri.LowPhysicalMemory()) {
        ri.Cvar_Set("r_modelpoolmegs", "0");
    }

    r_environmentMapping = ri.Cvar_Get("r_environmentMapping", "1", cvarFlags_t::CVAR_ARCHIVE_ND);

    r_screenshotJpegQuality = ri.Cvar_Get("r_screenshotJpegQuality", "95", cvarFlags_t::CVAR_ARCHIVE_ND);

    ri.Cvar_CheckRange(r_screenshotJpegQuality, 10, 100, true);

    for (size_t i = 0; i < numCommands; i++)
        ri.Cmd_AddCommand(commands[i].cmd, commands[i].func);
}

// need to do this hackery so ghoul2 doesn't crash the game because of ITS hackery...
//
void R_ClearStuffToStopGhoul2CrashingThings(void)
{
    memset(&tr, 0, sizeof(tr));
}

/*
===============
R_Init
===============
*/
extern void R_InitWorldEffects();
void R_Init(void)
{
    int err;
    int i;

    // Q_Println("----- R_Init -----");

    ShaderEntryPtrs_Clear();

    // clear all our internal state
    memset(&tr, 0, sizeof(tr));
    memset(&backEnd, 0, sizeof(backEnd));
    memset(&tess, 0, sizeof(tess));

#ifndef FINAL_BUILD
    if ((intptr_t)tess.xyz & 15) {
        Q_PrintlnWarning("tess.xyz not 16 byte aligned");
    }
#endif

    //
    // init function tables
    //
    for (i = 0; i < FUNCTABLE_SIZE; i++) {
        tr.sinTable[i] = Q_sinf(DEG2RAD(i * 360.0f / ((float)(FUNCTABLE_SIZE - 1))));
        tr.squareTable[i] = (i < FUNCTABLE_SIZE / 2) ? 1.0f : -1.0f;
        tr.sawToothTable[i] = (float)i / FUNCTABLE_SIZE;
        tr.inverseSawToothTable[i] = 1.0f - tr.sawToothTable[i];

        if (i < FUNCTABLE_SIZE / 2) {
            if (i < FUNCTABLE_SIZE / 4) {
                tr.triangleTable[i] = (float)i / (FUNCTABLE_SIZE / 4);
            }
            else {
                tr.triangleTable[i] = 1.0f - tr.triangleTable[i - FUNCTABLE_SIZE / 4];
            }
        }
        else {
            tr.triangleTable[i] = -tr.triangleTable[i - FUNCTABLE_SIZE / 2];
        }
    }
    R_InitFogTable();

    R_ImageLoader_Init();
    R_NoiseInit();
    R_Register();

    backEndData = (backEndData_t*)R_Hunk_Alloc(sizeof(backEndData_t), true);
    R_InitNextFrame();

    const color4ub_t color = {0xff, 0xff, 0xff, 0xff};
    for (i = 0; i < MAX_LIGHT_STYLES; i++) {
        byteAlias_t* ba = (byteAlias_t*)&color;
        RE_SetLightStyle(i, ba->i);
    }
    InitOpenGL();

    R_InitImages();
    R_InitShaders();
    R_InitSkins();
    R_ModelInit();
    R_InitWorldEffects();
    R_InitFonts();

    err = qglGetError();
    if (err != GL_NO_ERROR)
        Q_Println("glGetError() = 0x{:x}", err);

    RestoreGhoul2InfoArray();
    // print info
    GfxInfo_f();

    // Q_Println("----- finished R_Init -----");
}

/*
===============
RE_Shutdown
===============
*/
extern void R_ShutdownWorldEffects(void);
void RE_Shutdown(bool destroyWindow, bool restarting)
{
    for (size_t i = 0; i < numCommands; i++)
        ri.Cmd_RemoveCommand(commands[i].cmd);

    if (r_DynamicGlow && r_DynamicGlow->integer) {
        // Release the Glow Vertex Shader.
        if (tr.glowVShader) {
            qglDeleteProgramsARB(1, &tr.glowVShader);
        }

        // Release Pixel Shader.
        if (tr.glowPShader) {
            if (qglCombinerParameteriNV) {
                // Release the Glow Regcom call list.
                qglDeleteLists(tr.glowPShader, 1);
            }
            else if (qglGenProgramsARB) {
                // Release the Glow Fragment Shader.
                qglDeleteProgramsARB(1, &tr.glowPShader);
            }
        }

        // Release the scene glow texture.
        qglDeleteTextures(1, &tr.screenGlow);

        // Release the scene texture.
        qglDeleteTextures(1, &tr.sceneImage);

        // Release the blur texture.
        qglDeleteTextures(1, &tr.blurImage);
    }

    R_ShutdownWorldEffects();
    R_ShutdownFonts();
    if (tr.registered) {
        R_IssuePendingRenderCommands();
        if (destroyWindow) {
            R_DeleteTextures(); // only do this for vid_restart now, not during things like map load

            if (restarting) {
                SaveGhoul2InfoArray();
            }
        }
    }

    // a restart of the video asks the client again what the display is like (InitOpenGL asks when vidWidth is 0): the
    // library may stay loaded until the next start, keeping these, since glibc never unloads a library that holds GCC's
    // unique symbols (a debug build's)
    if (destroyWindow) {
        Output_Shutdown();
        memset(&glConfig, 0, sizeof(glConfig));
    }

    tr.registered = false;
}

/*
=============
RE_EndRegistration

Touch all images to make sure they are resident
=============
*/
void RE_EndRegistration(void)
{
    R_IssuePendingRenderCommands();
}

void RE_GetLightStyle(int style, color4ub_t color)
{
    if (style >= MAX_LIGHT_STYLES) {
        Q_Error(errorParm_t::ERR_FATAL, "RE_GetLightStyle: {} is out of range", (int)style);
        return;
    }

    byteAlias_t *baDest = (byteAlias_t*)&color, *baSource = (byteAlias_t*)&styleColors[style];
    baDest->i = baSource->i;
}

void RE_SetLightStyle(int style, int color)
{
    if (style >= MAX_LIGHT_STYLES) {
        Q_Error(errorParm_t::ERR_FATAL, "RE_SetLightStyle: {} is out of range", (int)style);
        return;
    }

    byteAlias_t* ba = (byteAlias_t*)&styleColors[style];
    if (ba->i != color) {
        ba->i = color;
        styleUpdated[style] = true;
    }
}

/*
=====================
tr_distortionX

DLL glue
=====================
*/

extern float tr_distortionAlpha;
extern float tr_distortionStretch;
extern bool tr_distortionPrePost;
extern bool tr_distortionNegate;

float* get_tr_distortionAlpha(void)
{
    return &tr_distortionAlpha;
}

float* get_tr_distortionStretch(void)
{
    return &tr_distortionStretch;
}

bool* get_tr_distortionPrePost(void)
{
    return &tr_distortionPrePost;
}

bool* get_tr_distortionNegate(void)
{
    return &tr_distortionNegate;
}

float g_oldRangedFog = 0.0f;
void RE_SetRangedFog(float dist)
{
    if (tr.rangedFog <= 0.0f) {
        g_oldRangedFog = tr.rangedFog;
    }
    tr.rangedFog = dist;
    if (tr.rangedFog == 0.0f && g_oldRangedFog) { // restore to previous state if applicable
        tr.rangedFog = g_oldRangedFog;
    }
}

// bool inServer = false;
void RE_SVModelInit(void)
{
    tr.numModels = 0;
    tr.numShaders = 0;
    tr.numSkins = 0;
    R_InitImages();
    // inServer = true;
    R_InitShaders();
    // inServer = false;
    R_ModelInit();
}

/*
@@@@@@@@@@@@@@@@@@@@@
GetRefAPI

@@@@@@@@@@@@@@@@@@@@@
*/
extern void R_LoadImage(const char* shortname, byte** pic, int* width, int* height);
extern void R_WorldEffectCommand(const char* command);
extern bool R_inPVS(vec3_t p1, vec3_t p2);
extern void RE_GetModelBounds(refEntity_t* refEnt, vec3_t bounds1, vec3_t bounds2);
extern void G2API_AnimateG2Models(CGhoul2Info_v& ghoul2, int AcurrentTime, CRagDollUpdateParams* params);
extern bool G2API_GetRagBonePos(CGhoul2Info_v& ghoul2, const char* boneName, vec3_t pos, vec3_t entAngles, vec3_t entPos, vec3_t entScale);
extern bool G2API_RagEffectorKick(CGhoul2Info_v& ghoul2, const char* boneName, vec3_t velocity);
extern bool G2API_RagForceSolve(CGhoul2Info_v& ghoul2, bool force);
extern bool G2API_SetBoneIKState(CGhoul2Info_v& ghoul2, int time, const char* boneName, sharedEIKMoveState ikState, sharedSetBoneIKStateParams_t* params);
extern bool G2API_IKMove(CGhoul2Info_v& ghoul2, int time, sharedIKMoveParams_t* params);
extern bool G2API_RagEffectorGoal(CGhoul2Info_v& ghoul2, const char* boneName, vec3_t pos);
extern bool G2API_RagPCJGradientSpeed(CGhoul2Info_v& ghoul2, const char* boneName, const float speed);
extern bool G2API_RagPCJConstraint(CGhoul2Info_v& ghoul2, const char* boneName, vec3_t min, vec3_t max);
extern void G2API_SetRagDoll(CGhoul2Info_v& ghoul2, CRagDollParams* parms);
#ifdef G2_PERFORMANCE_ANALYSIS
extern void G2Time_ResetTimers(void);
extern void G2Time_ReportTimers(void);
#endif
extern IGhoul2InfoArray& TheGhoul2InfoArray();

#ifdef JK2_MODE
unsigned int AnyLanguage_ReadCharFromString_JK2(char** text, bool* pbIsTrailingPunctuation)
{
    return AnyLanguage_ReadCharFromString(text, pbIsTrailingPunctuation);
}
#endif

extern "C" Q_EXPORT refexport_t* QDECL GetRefAPI(int apiVersion, refimport_t* refimp)
{
    static refexport_t re;

    ri = *refimp;

    memset(&re, 0, sizeof(re));

    if (apiVersion != REF_API_VERSION) {
        Q_Println("Mismatched REF_API_VERSION: expected {}, got {}", REF_API_VERSION, apiVersion);
        return nullptr;
    }

    // the RE_ functions are Renderer Entry points

#define REX(x) re.x = RE_##x

    REX(Shutdown);

    REX(BeginRegistration);
    REX(RegisterModel);
    REX(RegisterSkin);
    REX(GetAnimationCFG);
    REX(RegisterShader);
    REX(RegisterShaderNoMip);
    re.LoadWorld = RE_LoadWorldMap;
    re.R_LoadImage = R_LoadImage;

    REX(RegisterMedia_LevelLoadBegin);
    REX(RegisterMedia_LevelLoadEnd);
    REX(RegisterMedia_GetLevel);
    REX(RegisterImages_LevelLoadEnd);
    REX(RegisterModels_LevelLoadEnd);

    REX(SetWorldVisData);

    REX(EndRegistration);

    REX(ClearScene);
    REX(AddRefEntityToScene);
    REX(GetLighting);
    REX(AddPolyToScene);
    REX(AddLightToScene);
    REX(RenderScene);
    REX(GetLighting);

    REX(SetColor);
    re.DrawStretchPic = RE_StretchPic;
    re.DrawRotatePic = RE_RotatePic;
    re.DrawRotatePic2 = RE_RotatePic2;
    REX(LAGoggles);
    REX(Scissor);

    re.DrawStretchRaw = RE_StretchRaw;
    REX(UploadCinematic);

    REX(BeginFrame);
    REX(EndFrame);

    REX(ProcessDissolve);
    REX(InitDissolve);

    REX(GetScreenShot);
#ifdef JK2_MODE
    REX(SaveJPGToBuffer);
    re.LoadJPGFromBuffer = LoadJPGFromBuffer;
#endif
    REX(TempRawImage_ReadFromFile);
    REX(TempRawImage_CleanUp);

    re.MarkFragments = R_MarkFragments;
    re.LerpTag = R_LerpTag;
    re.ModelBounds = R_ModelBounds;
    REX(GetLightStyle);
    REX(SetLightStyle);
    REX(GetBModelVerts);
    re.WorldEffectCommand = R_WorldEffectCommand;
    REX(GetModelBounds);

    REX(SVModelInit);

    REX(RegisterFont);
    REX(Font_HeightPixels);
    REX(Font_StrLenPixels);
    REX(Font_DrawString);
    REX(Font_StrLenChars);
    re.Language_IsAsian = Language_IsAsian;
    re.Language_UsesSpaces = Language_UsesSpaces;
    re.AnyLanguage_ReadCharFromString = AnyLanguage_ReadCharFromString;
#ifdef JK2_MODE
    re.AnyLanguage_ReadCharFromString2 = AnyLanguage_ReadCharFromString_JK2;
#endif

    re.R_InitWorldEffects = R_InitWorldEffects;
    re.R_ClearStuffToStopGhoul2CrashingThings = R_ClearStuffToStopGhoul2CrashingThings;
    re.R_inPVS = R_inPVS;

    re.tr_distortionAlpha = get_tr_distortionAlpha;
    re.tr_distortionStretch = get_tr_distortionStretch;
    re.tr_distortionPrePost = get_tr_distortionPrePost;
    re.tr_distortionNegate = get_tr_distortionNegate;

    re.GetWindVector = R_GetWindVector;
    re.GetWindGusting = R_GetWindGusting;
    re.IsOutside = R_IsOutside;
    re.IsOutsideCausingPain = R_IsOutsideCausingPain;
    re.GetChanceOfSaberFizz = R_GetChanceOfSaberFizz;
    re.IsShaking = R_IsShaking;
    re.AddWeatherZone = R_AddWeatherZone;
    re.SetTempGlobalFogColor = R_SetTempGlobalFogColor;

    REX(SetRangedFog);

    re.TheGhoul2InfoArray = TheGhoul2InfoArray;

#define G2EX(x) re.G2API_##x = G2API_##x

    G2EX(AddBolt);
    G2EX(AddBoltSurfNum);
    G2EX(AddSurface);
    G2EX(AnimateG2Models);
    G2EX(AttachEnt);
    G2EX(AttachG2Model);
    G2EX(CollisionDetect);
    G2EX(CleanGhoul2Models);
    G2EX(CopyGhoul2Instance);
    G2EX(DetachEnt);
    G2EX(DetachG2Model);
    G2EX(GetAnimFileName);
    G2EX(GetAnimFileNameIndex);
    G2EX(GetAnimFileInternalNameIndex);
    G2EX(GetAnimIndex);
    G2EX(GetAnimRange);
    G2EX(GetAnimRangeIndex);
    G2EX(GetBoneAnim);
    G2EX(GetBoneAnimIndex);
    G2EX(GetBoneIndex);
    G2EX(GetBoltMatrix);
    G2EX(GetGhoul2ModelFlags);
    G2EX(GetGLAName);
    G2EX(GetParentSurface);
    G2EX(GetRagBonePos);
    G2EX(GetSurfaceIndex);
    G2EX(GetSurfaceName);
    G2EX(GetSurfaceRenderStatus);
    G2EX(GetTime);
    G2EX(GiveMeVectorFromMatrix);
    G2EX(HaveWeGhoul2Models);
    G2EX(IKMove);
    G2EX(InitGhoul2Model);
    G2EX(IsPaused);
    G2EX(ListBones);
    G2EX(ListSurfaces);
    G2EX(LoadGhoul2Models);
    G2EX(LoadSaveCodeDestructGhoul2Info);
    G2EX(PauseBoneAnim);
    G2EX(PauseBoneAnimIndex);
    G2EX(PrecacheGhoul2Model);
    G2EX(RagEffectorGoal);
    G2EX(RagEffectorKick);
    G2EX(RagForceSolve);
    G2EX(RagPCJConstraint);
    G2EX(RagPCJGradientSpeed);
    G2EX(RemoveBolt);
    G2EX(RemoveBone);
    G2EX(RemoveGhoul2Model);
    G2EX(RemoveSurface);
    G2EX(SaveGhoul2Models);
    G2EX(SetAnimIndex);
    G2EX(SetBoneAnim);
    G2EX(SetBoneAnimIndex);
    G2EX(SetBoneAngles);
    G2EX(SetBoneAnglesIndex);
    G2EX(SetBoneAnglesMatrix);
    G2EX(SetBoneIKState);
    G2EX(SetGhoul2ModelFlags);
    G2EX(SetGhoul2ModelIndexes);
    G2EX(SetLodBias);
    // G2EX(SetModelIndexes);
    G2EX(SetNewOrigin);
    G2EX(SetRagDoll);
    G2EX(SetRootSurface);
    G2EX(SetShader);
    G2EX(SetSkin);
    G2EX(SetSurfaceOnOff);
    G2EX(SetTime);
    G2EX(StopBoneAnim);
    G2EX(StopBoneAnimIndex);
    G2EX(StopBoneAngles);
    G2EX(StopBoneAnglesIndex);
#ifdef _G2_GORE
    G2EX(AddSkinGore);
    G2EX(ClearSkinGore);
#endif

#ifdef G2_PERFORMANCE_ANALYSIS
    re.G2Time_ReportTimers = G2Time_ReportTimers;
    re.G2Time_ResetTimers = G2Time_ResetTimers;
#endif

    // Swap_Init();

    return &re;
}

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

#include <server/exe_headers.h>

#include "ui_local.h"

float trap_Cvar_VariableValue(const char* var_name)
{
    return Cvar_VariableValue(var_name);
}

void trap_R_ClearScene(void)
{
    ui.R_ClearScene();
}

void trap_R_AddRefEntityToScene(const refEntity_t* re)
{
    ui.R_AddRefEntityToScene(re);
}

void trap_R_RenderScene(const refdef_t* fd)
{
    //	syscall( uiImport_t::UI_R_RENDERSCENE, fd );
    ui.R_RenderScene(fd);
}

void trap_R_SetColor(const float* rgba)
{
    //	syscall( uiImport_t::UI_R_SETCOLOR, rgba );
    //	re.SetColor( rgba );
    ui.R_SetColor(rgba);
}

void trap_R_DrawStretchPic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader)
{
    //	syscall( uiImport_t::UI_R_DRAWSTRETCHPIC, PASSFLOAT(x), PASSFLOAT(y), PASSFLOAT(w), PASSFLOAT(h), PASSFLOAT(s1), PASSFLOAT(t1), PASSFLOAT(s2),
    // PASSFLOAT(t2), hShader ); 	re.DrawStretchPic( x, y, w, h, s1, t1, s2, t2, hShader  );

    ui.R_DrawStretchPic(x, y, w, h, s1, t1, s2, t2, hShader);
}

void trap_R_ModelBounds(clipHandle_t model, vec3_t mins, vec3_t maxs)
{
    //	syscall( uiImport_t::UI_R_MODELBOUNDS, model, mins, maxs );
    ui.R_ModelBounds(model, mins, maxs);
}

void trap_S_StartLocalSound(sfxHandle_t sfx, soundChannel_t channelNum)
{
    //	syscall( uiImport_t::UI_S_STARTLOCALSOUND, sfx, channelNum );
    S_StartLocalSound(sfx, channelNum);
}

void trap_S_StopSounds(void)
{
    S_StopSounds();
}

sfxHandle_t trap_S_RegisterSound(const char* sample, bool compressed)
{
    return S_RegisterSound(sample);
}

void trap_Key_SetBinding(fakeAscii_t keynum, const char* binding)
{
    Key_SetBinding(keynum, binding);
}

bool trap_Key_GetOverstrikeMode(void)
{
    return Key_GetOverstrikeMode();
}

void trap_Key_SetOverstrikeMode(bool state)
{
    Key_SetOverstrikeMode(state);
}

void trap_Key_ClearStates(void)
{
    Key_ClearStates();
}

keyCatchFlags_t Key_GetCatcher(void);

keyCatchFlags_t trap_Key_GetCatcher(void)
{
    return Key_GetCatcher();
}

void Key_SetCatcher(keyCatchFlags_t catcher);

void trap_Key_SetCatcher(keyCatchFlags_t catcher)
{
    Key_SetCatcher(catcher);
}
/*
void trap_GetClipboardData( char *buf, int bufsize ) {
    syscall( uiImport_t::UI_GETCLIPBOARDDATA, buf, bufsize );
}

void trap_GetClientState( uiClientState_t *state ) {
    syscall( uiImport_t::UI_GETCLIENTSTATE, state );
}
*/

void CL_GetGlconfig(glconfig_t* glconfig);

void trap_GetGlconfig(glconfig_t* glconfig)
{
    //	syscall( uiImport_t::UI_GETGLCONFIG, glconfig );
    CL_GetGlconfig(glconfig);
}

// this returns a handle.  arg0 is the name in the format "idlogo.roq", set arg1 to nullptr, alteredstates to false (do not alter gamestate)
int trap_CIN_PlayCinematic(const char* arg0, int xpos, int ypos, int width, int height, cinematicFlags_t bits, const char* psAudioFile)
{
    //  return syscall(uiImport_t::UI_CIN_PLAYCINEMATIC, arg0, xpos, ypos, width, height, to_underlying( bits ), psAudioFile);
    return CIN_PlayCinematic(arg0, xpos, ypos, width, height, bits, psAudioFile);
}

// stops playing the cinematic and ends it.  should always return FMV_EOF
// cinematics must be stopped in reverse order of when they are started
int trap_CIN_StopCinematic(int handle)
{
    //  return syscall(uiImport_t::UI_CIN_STOPCINEMATIC, handle);
    return to_underlying(CIN_StopCinematic(handle));
}

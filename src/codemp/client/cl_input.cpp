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

// cl_input.cpp -- the multiplayer's part of the input (src/shared/client/engine/input.h): one command per tick, made
// as the command clock enters it (between two frames when it does: CL_InputTick) and sent in its packet; the buttons'
// bits the game reads, the generic commands, the force powers, the automap that takes the movement keys and the mouse
// while it shows, the fighters' pitch.

#include "client.h"
#include "cl_cgameapi.h"
#include "cl_uiapi.h"
#include <shared/platform/engine/engine.h>
#include <cstdlib>
#ifndef _WIN32
#include <cmath>
#endif
// The input's time, on the clock of com_frameTime: the frame's time when a frame reads the input, the present time when
// a tick's command is made between two frames (CL_InputTick); and the same moment on the client's clock, in nanoseconds
static int s_inputTime;
static int64_t s_inputNsec;

bool cl_bUseFighterPitch = false;
bool cl_crazyShipControls = false;

void IN_UseGivenForce(void)
{
    const char* c = Cmd_Argv(1);
    int forceNum = -1;
    genCmds_t genCmdNum = genCmds_t::GENCMD_NONE;

    if (c) {
        forceNum = atoi(c);
    }
    else {
        return;
    }

    switch (static_cast<forcePowers_t>(forceNum)) {
    case forcePowers_t::FP_DRAIN:
        IN_Button11Down();
        IN_Button11Up();
        break;
    case forcePowers_t::FP_PUSH:
        genCmdNum = genCmds_t::GENCMD_FORCE_THROW;
        break;
    case forcePowers_t::FP_SPEED:
        genCmdNum = genCmds_t::GENCMD_FORCE_SPEED;
        break;
    case forcePowers_t::FP_PULL:
        genCmdNum = genCmds_t::GENCMD_FORCE_PULL;
        break;
    case forcePowers_t::FP_TELEPATHY:
        genCmdNum = genCmds_t::GENCMD_FORCE_DISTRACT;
        break;
    case forcePowers_t::FP_GRIP:
        IN_Button6Down();
        IN_Button6Up();
        break;
    case forcePowers_t::FP_LIGHTNING:
        IN_Button10Down();
        IN_Button10Up();
        break;
    case forcePowers_t::FP_RAGE:
        genCmdNum = genCmds_t::GENCMD_FORCE_RAGE;
        break;
    case forcePowers_t::FP_PROTECT:
        genCmdNum = genCmds_t::GENCMD_FORCE_PROTECT;
        break;
    case forcePowers_t::FP_ABSORB:
        genCmdNum = genCmds_t::GENCMD_FORCE_ABSORB;
        break;
    case forcePowers_t::FP_SEE:
        genCmdNum = genCmds_t::GENCMD_FORCE_SEEING;
        break;
    case forcePowers_t::FP_HEAL:
        genCmdNum = genCmds_t::GENCMD_FORCE_HEAL;
        break;
    case forcePowers_t::FP_TEAM_HEAL:
        genCmdNum = genCmds_t::GENCMD_FORCE_HEALOTHER;
        break;
    case forcePowers_t::FP_TEAM_FORCE:
        genCmdNum = genCmds_t::GENCMD_FORCE_FORCEPOWEROTHER;
        break;
    default:
        assert(0);
        break;
    }

    if (genCmdNum != genCmds_t::GENCMD_NONE) {
        cl.gcmdSendValue = true;
        cl.gcmdValue = std::to_underlying(genCmdNum);
    }
}

void IN_GenCMD1(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_SABERSWITCH);
}

void IN_GenCMD2(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_ENGAGE_DUEL);
}

void IN_GenCMD3(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_FORCE_HEAL);
}

void IN_GenCMD4(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_FORCE_SPEED);
}

void IN_GenCMD5(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_FORCE_PULL);
}

void IN_GenCMD6(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_FORCE_DISTRACT);
}

void IN_GenCMD7(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_FORCE_RAGE);
}

void IN_GenCMD8(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_FORCE_PROTECT);
}

void IN_GenCMD9(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_FORCE_ABSORB);
}

void IN_GenCMD10(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_FORCE_HEALOTHER);
}

void IN_GenCMD11(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_FORCE_FORCEPOWEROTHER);
}

void IN_GenCMD12(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_FORCE_SEEING);
}

void IN_GenCMD13(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_USE_SEEKER);
}

void IN_GenCMD14(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_USE_FIELD);
}

void IN_GenCMD15(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_USE_BACTA);
}

void IN_GenCMD16(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_USE_ELECTROBINOCULARS);
}

void IN_GenCMD17(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_ZOOM);
}

void IN_GenCMD18(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_USE_SENTRY);
}

void IN_GenCMD19(void)
{
    if (Cvar_VariableIntegerValue("d_saberStanceDebug")) {
        Q_Println("SABERSTANCEDEBUG: Gencmd on client set successfully.");
    }
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_SABERATTACKCYCLE);
}

void IN_GenCMD20(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_FORCE_THROW);
}

void IN_GenCMD21(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_USE_JETPACK);
}

void IN_GenCMD22(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_USE_BACTABIG);
}

void IN_GenCMD23(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_USE_HEALTHDISP);
}

void IN_GenCMD24(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_USE_AMMODISP);
}

void IN_GenCMD25(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_USE_EWEB);
}

void IN_GenCMD26(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_USE_CLOAK);
}

void IN_GenCMD27(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_TAUNT);
}

void IN_GenCMD28(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_BOW);
}

void IN_GenCMD29(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_MEDITATE);
}

void IN_GenCMD30(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_FLOURISH);
}

void IN_GenCMD31(void)
{
    cl.gcmdSendValue = true;
    cl.gcmdValue = std::to_underlying(genCmds_t::GENCMD_GLOAT);
}

// toggle automap view mode
static bool g_clAutoMapMode = false;
void IN_AutoMapButton(void)
{
    g_clAutoMapMode = !g_clAutoMapMode;
}

// toggle between automap, radar, nothing
extern cvar_t* r_autoMap;
void IN_AutoMapToggle(void)
{
    Cvar_User_SetValue("cg_drawRadar", !Cvar_VariableValue("cg_drawRadar"));
    /*
    if (r_autoMap && r_autoMap->integer)
    { //automap off, radar on
        Cvar_Set("r_autoMap", "0");
        Cvar_Set("cg_drawRadar", "1");
    }
    else if (Cvar_VariableIntegerValue("cg_drawRadar"))
    { //radar off, automap should be off too
        Cvar_Set("cg_drawRadar", "0");
    }
    else
    { //turn automap on
        Cvar_Set("r_autoMap", "1");
    }
    */
}

void IN_VoiceChatButton(void)
{
    if (!cls.uiStarted) { // ui not loaded so this command is useless
        return;
    }
    UIVM_SetActiveMenu(uiMenuCommand_t::UIMENU_VOICECHAT);
}

enum class autoMapKey_t {
    AUTOMAP_KEY_FORWARD = 1,
    AUTOMAP_KEY_BACK,
    AUTOMAP_KEY_YAWLEFT,
    AUTOMAP_KEY_YAWRIGHT,
    AUTOMAP_KEY_PITCHUP,
    AUTOMAP_KEY_PITCHDOWN,
    AUTOMAP_KEY_DEFAULTVIEW,
};
static autoMapInput_t g_clAutoMapInput;
// intercept certain keys during automap mode
static void CL_AutoMapKey(autoMapKey_t autoMapKey, bool up)
{
    autoMapInput_t* data = (autoMapInput_t*)cl.mSharedMemory;

    switch (autoMapKey) {
    case autoMapKey_t::AUTOMAP_KEY_FORWARD:
        if (up) {
            g_clAutoMapInput.up = 0.0f;
        }
        else {
            g_clAutoMapInput.up = 16.0f;
        }
        break;
    case autoMapKey_t::AUTOMAP_KEY_BACK:
        if (up) {
            g_clAutoMapInput.down = 0.0f;
        }
        else {
            g_clAutoMapInput.down = 16.0f;
        }
        break;
    case autoMapKey_t::AUTOMAP_KEY_YAWLEFT:
        if (up) {
            g_clAutoMapInput.yaw = 0.0f;
        }
        else {
            g_clAutoMapInput.yaw = -4.0f;
        }
        break;
    case autoMapKey_t::AUTOMAP_KEY_YAWRIGHT:
        if (up) {
            g_clAutoMapInput.yaw = 0.0f;
        }
        else {
            g_clAutoMapInput.yaw = 4.0f;
        }
        break;
    case autoMapKey_t::AUTOMAP_KEY_PITCHUP:
        if (up) {
            g_clAutoMapInput.pitch = 0.0f;
        }
        else {
            g_clAutoMapInput.pitch = -4.0f;
        }
        break;
    case autoMapKey_t::AUTOMAP_KEY_PITCHDOWN:
        if (up) {
            g_clAutoMapInput.pitch = 0.0f;
        }
        else {
            g_clAutoMapInput.pitch = 4.0f;
        }
        break;
    case autoMapKey_t::AUTOMAP_KEY_DEFAULTVIEW:
        memset(&g_clAutoMapInput, 0, sizeof(autoMapInput_t));
        g_clAutoMapInput.goToDefaults = true;
        break;
    default:
        break;
    }

    memcpy(data, &g_clAutoMapInput, sizeof(autoMapInput_t));

    if (cls.cgameStarted) {
        CGVM_AutomapInput();
    }

    g_clAutoMapInput.goToDefaults = false;
}

/*
=================
CL_MapTakesButton

The automap takes the movement keys while it shows: forward and back move it, the strafe keys turn it, jump and crouch
tilt it, use puts it back to its first view
=================
*/
bool CL_MapTakesButton(const kbutton_t* button, bool down)
{
    if (!g_clAutoMapMode) {
        return false;
    }

    if (button == &in_forward) {
        CL_AutoMapKey(autoMapKey_t::AUTOMAP_KEY_FORWARD, !down);
    }
    else if (button == &in_back) {
        CL_AutoMapKey(autoMapKey_t::AUTOMAP_KEY_BACK, !down);
    }
    else if (button == &in_moveleft) {
        CL_AutoMapKey(autoMapKey_t::AUTOMAP_KEY_YAWLEFT, !down);
    }
    else if (button == &in_moveright) {
        CL_AutoMapKey(autoMapKey_t::AUTOMAP_KEY_YAWRIGHT, !down);
    }
    else if (button == &in_up) {
        CL_AutoMapKey(autoMapKey_t::AUTOMAP_KEY_PITCHUP, !down);
    }
    else if (button == &in_down) {
        CL_AutoMapKey(autoMapKey_t::AUTOMAP_KEY_PITCHDOWN, !down);
    }
    else if (button == &in_buttons[5] && down) {
        CL_AutoMapKey(autoMapKey_t::AUTOMAP_KEY_DEFAULTVIEW, false);
    }
    else {
        return false;
    }

    return true;
}

/*
=================
CL_MapTakesMouse

The automap takes the mouse's motion while it shows
=================
*/
bool CL_MapTakesMouse(float dx, float dy)
{
    if (!g_clAutoMapMode || !cls.cgameStarted) {
        return false;
    }

    autoMapInput_t* data = (autoMapInput_t*)cl.mSharedMemory;

    g_clAutoMapInput.yaw = dx;
    g_clAutoMapInput.pitch = dy;
    memcpy(data, &g_clAutoMapInput, sizeof(autoMapInput_t));
    CGVM_AutomapInput();

    g_clAutoMapInput.yaw = 0.0f;
    g_clAutoMapInput.pitch = 0.0f;
    return true;
}

// the fighters' own pitch factor while the cgame asks for it
float CL_MousePitch(void)
{
    return cl_bUseFighterPitch ? m_pitchVeh->value : m_pitch->value;
}

void IN_CenterView(void)
{
    cl.viewangles[PITCH] = -SHORT2ANGLE(cl.snap.ps.delta_angles[PITCH]);
}

/*
=================
CL_PointerEvent

The system's pointer moved over the window while a menu takes it: the client's own menu, the UI's, the cgame's
=================
*/
void CL_PointerEvent(float x, float y)
{
    if (cls.cursorActive) {
        cls.cursorX = Com_Clampi(0, SCREEN_WIDTH, static_cast<int>(x * SCREEN_WIDTH / cls.glconfig.vidWidth));
        cls.cursorY = Com_Clampi(0, SCREEN_HEIGHT, static_cast<int>(y * SCREEN_HEIGHT / cls.glconfig.vidHeight));
    }
    else if (Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_UI)) {
        UIVM_MouseEvent(static_cast<int>(x), static_cast<int>(y));
    }
    else if (Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_CGAME)) {
        CGVM_MouseEvent(static_cast<int>(x), static_cast<int>(y));
    }
}

/*
=================
CL_MouseTurnsView

In game, while nothing takes the pointer: a menu of the UI or of the cgame, the console, the client's own menu
=================
*/
bool CL_MouseTurnsView(void)
{
    if (cls.state != connstate_t::CA_ACTIVE || cls.cursorActive) {
        return false;
    }

    return !Any(Key_GetCatcher() & (keyCatchFlags_t::KEYCATCH_CONSOLE | keyCatchFlags_t::KEYCATCH_UI | keyCatchFlags_t::KEYCATCH_CGAME));
}

bool CL_NoUseableForce(void)
{
    if (!cls.cgameStarted) { // ahh, no cgame loaded
        return false;
    }

    return CGVM_NoUseableForce();
}

/*
==============
CL_CmdButtons
==============
*/
void CL_CmdButtons(usercmd_t* cmd)
{
    int i;

    //
    // figure button bits
    // send a button bit even if the key was pressed and released in
    // less than a frame
    //
    for (i = 0; i < MAX_KBUTTONS; i++) {
        if (in_buttons[i].active || in_buttons[i].wasPressed) {
            cmd->buttons |= static_cast<buttonFlags_t>(1 << i);
        }
        in_buttons[i].wasPressed = false;
    }

    if (Any(cmd->buttons & buttonFlags_t::BUTTON_FORCEPOWER)) {                     // check for transferring a use force to a use inventory...
        if (Any(cmd->buttons & buttonFlags_t::BUTTON_USE) || CL_NoUseableForce()) { // it's pushed, remap it!
            cmd->buttons &= ~buttonFlags_t::BUTTON_FORCEPOWER;
            cmd->buttons |= buttonFlags_t::BUTTON_USE_HOLDABLE;
        }
    }

    if (Any(Key_GetCatcher())) {
        cmd->buttons |= buttonFlags_t::BUTTON_TALK;
    }

    // allow the game to know if any key at all is
    // currently pressed, even if it isn't bound to anything
    if (kg.anykeydown && !Any(Key_GetCatcher())) {
        cmd->buttons |= buttonFlags_t::BUTTON_ANY;
    }
}

/*
==============
CL_FinishMove
==============
*/
vec3_t cl_sendAngles = {0};
vec3_t cl_lastViewAngles = {0};
void CL_FinishMove(usercmd_t* cmd)
{
    int i;

    // copy the state that the cgame is currently sending
    cmd->weapon = std::to_underlying(cl.cgameUserCmdValue);
    cmd->forcesel = std::to_underlying(cl.cgameForceSelection);
    cmd->invensel = cl.cgameInvenSelection;

    if (cl.gcmdSendValue) {
        cmd->generic_cmd = cl.gcmdValue;
        // cl.gcmdSendValue = false;
        cl.gcmdSentValue = true;
    }
    else {
        cmd->generic_cmd = 0;
    }

    // send the current server time so the amount of movement
    // can be determined without allowing cheating
    cmd->serverTime = cl.serverTime;

    if (cl.cgameViewAngleForceTime > cl.serverTime) {
        cl.cgameViewAngleForce[YAW] -= SHORT2ANGLE(cl.snap.ps.delta_angles[YAW]);

        cl.viewangles[YAW] = cl.cgameViewAngleForce[YAW];
        cl.cgameViewAngleForceTime = 0;
    }

    if (cl_crazyShipControls) {
        float pitchSubtract, pitchDelta, yawDelta;

        yawDelta = AngleSubtract(cl.viewangles[YAW], cl_lastViewAngles[YAW]);
        // yawDelta *= (4.0f*pVeh->m_fTimeModifier);
        cl_sendAngles[ROLL] -= yawDelta;

        float nRoll = Q_fabs(cl_sendAngles[ROLL]);

        pitchDelta = AngleSubtract(cl.viewangles[PITCH], cl_lastViewAngles[PITCH]);
        // pitchDelta *= (2.0f*pVeh->m_fTimeModifier);
        pitchSubtract = pitchDelta * (nRoll / 90.0f);
        cl_sendAngles[PITCH] += pitchDelta - pitchSubtract;

        // yaw-roll calc should be different
        if (nRoll > 90.0f) {
            nRoll -= 180.0f;
        }
        if (nRoll < 0.0f) {
            nRoll = -nRoll;
        }
        pitchSubtract = pitchDelta * (nRoll / 90.0f);
        if (cl_sendAngles[ROLL] > 0.0f) {
            cl_sendAngles[YAW] += pitchSubtract;
        }
        else {
            cl_sendAngles[YAW] -= pitchSubtract;
        }

        cl_sendAngles[PITCH] = AngleNormalize180(cl_sendAngles[PITCH]);
        cl_sendAngles[YAW] = AngleNormalize360(cl_sendAngles[YAW]);
        cl_sendAngles[ROLL] = AngleNormalize180(cl_sendAngles[ROLL]);

        for (i = 0; i < 3; i++) {
            cmd->angles[i] = ANGLE2SHORT(cl_sendAngles[i]);
        }
    }
    else {
        for (i = 0; i < 3; i++) {
            cmd->angles[i] = ANGLE2SHORT(cl.viewangles[i]);
        }
        // in case we switch to the cl_crazyShipControls
        VectorCopy(cl.viewangles, cl_sendAngles);
    }
    // always needed in for the cl_crazyShipControls
    VectorCopy(cl.viewangles, cl_lastViewAngles);
}

/*
=================
CL_CreateCmd
=================
*/
usercmd_t CL_CreateCmd(void)
{
    usercmd_t cmd;

    Com_Memset(&cmd, 0, sizeof(cmd));

    CL_CmdButtons(&cmd);

    // get basic movement from keyboard
    CL_KeyMove(&cmd);

    // get basic movement from mouse, gathered over the frames since the last command
    CL_MouseMoveToCommand(&cmd);

    // get basic movement from joystick
    CL_JoystickMove(&cmd);

    // store out the final values
    CL_FinishMove(&cmd);

    return cmd;
}

/*
=================
CL_CreateNewCommands

Turns the view, then creates a usercmd_t for each tick of cl.serverTime this frame
entered: every command moves TICK_MSEC, whatever the frame rate. A
command is made as the clock enters its tick, with the input gathered over the tick
before, and carries the time the tick ends, so that
cgame predicts the tick in progress and draws the local player between two ticks
=================
*/
void CL_CreateNewCommands(void)
{
    // no need to create usercmds until we have a gamestate
    if (cls.state < connstate_t::CA_PRIMED)
        return;

    CL_InputRead(s_inputTime, s_inputNsec);
    CL_UpdateViewAngles();

    // until the first snapshot the client's clock does not run: a command every frame, as before, so that the packets
    // whose commands make the server put the client in the world keep coming
    if (cls.state != connstate_t::CA_ACTIVE) {
        CL_CommandStart(false);
        cl.cmdNumber++;
        cl.cmds[cl.cmdNumber & CMD_MASK] = CL_CreateCmd();
        return;
    }

    // the end of the tick in progress on the command clock (ahead of the server's clock, so that
    // the command reaches the server just before that tick runs); after the first command, a jump of the clock (by more
    // than 200 ms either way: a map_restart, a hitch) gives one command at the new time, as one frame's command did
    const int tick = cl.cmdTime - cl.cmdTime % TICK_MSEC + TICK_MSEC;
    int last = cl.cmds[cl.cmdNumber & CMD_MASK].serverTime;
    if (cl.cmdNumber == 0 || abs(tick - last) > 200) {
        last = tick - TICK_MSEC;
    }
    if (last >= tick) {
        return; // the gathered input waits for the next tick
    }

    CL_CommandStart(true);
    usercmd_t cmd = CL_CreateCmd();
    for (int t = last + TICK_MSEC; t <= tick; t += TICK_MSEC) {
        cmd.serverTime = t;
        cl.cmdNumber++;
        cl.cmds[cl.cmdNumber & CMD_MASK] = cmd;
        cl.cmdDeltaAt[cl.cmdNumber & CMD_MASK] = cl.cmdTimeDelta;
        cmd.generic_cmd = 0; // a one-off command (a style change, a force power): in the first tick only
    }
}

/*
=================
CL_PendingCmd

The view angles and the command clock of now, for cgame's drawing of the local player
(the commands are made per tick, the view turns every frame): the last
command with the view angles of the frame and the command clock as its time, never
sent. false before the first command.
=================
*/
bool CL_PendingCmd(usercmd_t* cmd)
{
    if (cl.cmdNumber == 0) {
        return false;
    }
    *cmd = cl.cmds[cl.cmdNumber & CMD_MASK];
    cmd->serverTime = cl.cmdTime;
    for (int i = 0; i < 3; i++) {
        cmd->angles[i] = ANGLE2SHORT(cl.viewangles[i]);
    }
    return true;
}

/*
=================
CL_ReadyToSendPacket

Returns true when this frame made a new command (a packet per tick), or,
before the client is in the game (awaiting the gamestate or its first snapshot), once
per interval of MAX_PACKET_RATE
=================
*/
static bool CL_ReadyToSendPacket(bool newCommand)
{
    const int interval = 1000 / MAX_PACKET_RATE;

    // don't send anything if playing back a demo
    if (clc.demoplaying || cls.state == connstate_t::CA_CINEMATIC) {
        return false;
    }

    // If we are downloading, we send no less than 50ms between packets
    if (*clc.downloadTempName && cls.realtime - clc.lastPacketSentTime < 50) {
        return false;
    }

    // While awaiting the gamestate the client sends at the normal rate (OpenJK sent one packet a second): the
    // server resends a gamestate as soon as the client acknowledges a later message (SV_ExecuteClientMessage), so a
    // lost gamestate fragment now costs about one round trip instead of a second or more.

    // in the game, a packet carries the new commands (and repeats those of the last CL_PACKET_DUP packets)
    if (cls.state == connstate_t::CA_ACTIVE) {
        return newCommand;
    }

    // send every frame for loopbacks
    if (clc.netchan.remoteAddress.type == netadrtype_t::NA_LOOPBACK) {
        return true;
    }

    // a packet every frame, up to MAX_PACKET_RATE per second (the server decodes no more). The unused part
    // of the interval carries over to the next frame (at most one interval), so that the client sends min(frame rate,
    // MAX_PACKET_RATE) packets per second on average: a minimum gap checked once per frame let a packet leave only every
    // second frame when the frames were slightly shorter than the gap (62.5 packets per second at 125 fps with a cap of
    // 100, 72 at 144 fps with a cap of 125).
    if (cls.realtime < clc.nextPacketTime) {
        // the accumulated commands will go out in the next packet
        return false;
    }
    clc.nextPacketTime = Q_max(clc.nextPacketTime, cls.realtime - interval) + interval;

    return true;
}

/*
===================
CL_WritePacket

Create and send the command packet to the server
Including both the reliable commands and the usercmds

After the netchan header (sequence number, qport), a client packet holds the serverId, the last server message and
server command received, the client commands not yet acknowledged and the usercmds of the last few packets, all coded
by netcodec (CL_NetcodecWritePacket).
===================
*/
void CL_WritePacket(void)
{
    msg_t buf;
    byte data[MAX_MSGLEN];
    int packetNum;
    int oldPacketNum;
    int count;

    // don't send anything if playing back a demo
    if (clc.demoplaying || cls.state == connstate_t::CA_CINEMATIC) {
        return;
    }

    MSG_Init(&buf, data, sizeof(data));

    // we want to send all the usercmds that were generated in the last
    // few packet, so even if a couple packets are dropped in a row,
    // all the cmds will make it to the server
    oldPacketNum = (clc.netchan.outgoingSequence - 1 - CL_PACKET_DUP) & OUT_PACKET_MASK;
    count = cl.cmdNumber - cl.outPackets[oldPacketNum].p_cmdNumber;
    if (count > MAX_PACKET_USERCMDS) {
        count = MAX_PACKET_USERCMDS;
        Q_Println("MAX_PACKET_USERCMDS");
    }
    if (count >= 1 && cl_showSend->integer) {
        Q_Print("({})", count);
    }

    // the usercmds are a delta (clc_move) unless the last message's snapshot is missing (clc_moveNoDelta: the server
    // sends a full snapshot)
    const bool delta = (!cl_nodelta->integer && cl.snap.valid && clc.serverMessageSequence == cl.snap.messageNum) ? true : false;
    if (!CL_NetcodecWritePacket(&buf, count, delta)) {
        return;
    }

    if (count >= 1 && cl.gcmdSentValue) { // hmm, just clear here, I guess.. hoping it will resolve issues with gencmd values sometimes not going through.
        cl.gcmdSendValue = false;
        cl.gcmdSentValue = false;
        cl.gcmdValue = 0;
    }

    //
    // deliver the message
    //
    packetNum = clc.netchan.outgoingSequence & OUT_PACKET_MASK;
    cl.outPackets[packetNum].p_realtime = Sys_Milliseconds();
    cl.outPackets[packetNum].p_serverTime = count >= 1 ? cl.cmds[cl.cmdNumber & CMD_MASK].serverTime : 0;
    cl.outPackets[packetNum].p_cmdNumber = cl.cmdNumber;
    clc.lastPacketSentTime = cls.realtime;

    if (cl_showSend->integer) {
        Q_Print("{} ", buf.cursize);
    }

    CL_Netchan_Transmit(&clc.netchan, &buf);

    // clients never really should have messages large enough
    // to fragment, but in case they do, fire them all off
    // at once
    while (clc.netchan.unsentFragments) {
        CL_Netchan_TransmitNextFragment(&clc.netchan);
    }
}

/*
=================
CL_SendCmd

Called every frame to builds and sends a command packet to the server.
=================
*/
// makes the commands that are due at the input's time and sends their packet
static void CL_MakeAndSend(void)
{
    // we create commands even if a demo is playing,
    const int cmdNumber = cl.cmdNumber;
    CL_CreateNewCommands();

    // don't send a packet if the last packet was sent too recently
    if (!CL_ReadyToSendPacket(cl.cmdNumber != cmdNumber ? true : false)) {
        if (cl_showSend->integer) {
            Q_Print(". ");
        }
        return;
    }

    CL_WritePacket();
}

void CL_SendCmd(void)
{
    // don't send any message if not connected
    if (cls.state < connstate_t::CA_CONNECTED) {
        return;
    }

    // don't send commands if paused
    if (com_sv_running->integer && sv_paused->integer && cl_paused->integer) {
        return;
    }

    s_inputTime = com_frameTime;
    s_inputNsec = Platform_InputNanoseconds();
    CL_MakeAndSend();
}

/*
=================
CL_NextCommandMsec

How long until the command clock enters its next tick, in ms of real time (0: it has, the tick's command is due); -1
when no command is to be made between two frames: not in a game, a demo, a pause, or a jump of the clock (left to the
frame, as a map_restart or a hitch is). The command clock is read at the present time: cls.realtime moves once per
frame, with com_frameTime.
=================
*/
int CL_NextCommandMsec(void)
{
    if (cls.state != connstate_t::CA_ACTIVE || clc.demoplaying || cl.cmdNumber == 0) {
        return -1;
    }
    if (com_sv_running->integer && sv_paused->integer && cl_paused->integer) {
        return -1;
    }
    const int now = Q_max(cls.realtime + (Sys_Milliseconds() - com_frameTime) + cl.cmdTimeDelta, cl.oldCmdTime);
    const int last = cl.cmds[cl.cmdNumber & CMD_MASK].serverTime; // the end of the tick the last command is for

    if (abs(now - last) > 200) {
        return -1;
    }
    return Q_max(last - now, 0);
}

/*
=================
CL_InputTick

Between two frames, when the command clock enters a tick (Com_Frame): reads the input (the system's events, the key
binds), turns the view, makes the tick's command and sends its packet, as a frame does, without drawing. The client's
clock is set to the present for it (the packet's send time, the command clock) and put back: the frame that follows
moves it by its whole length. Returns false when no command was made (the events changed the client's state).
=================
*/
bool CL_InputTick(void)
{
    const int cmdNumber = cl.cmdNumber;
    const int realtime = cls.realtime;

    s_inputTime = Com_EventLoop(); // Com_Frame has just read the input (Platform_Frame)
    s_inputNsec = Platform_InputNanoseconds();
    // the key binds go through the command buffer; not while a wait command counts frames (this is not one)
    if (!Cbuf_Waiting()) {
        Cbuf_Execute();
    }
    if (cls.state < connstate_t::CA_CONNECTED || CL_NextCommandMsec() != 0) {
        return false;
    }

    cls.realtime += s_inputTime - com_frameTime;
    cl.cmdTime = Q_max(cls.realtime + cl.cmdTimeDelta, cl.oldCmdTime);
    cl.oldCmdTime = cl.cmdTime;
    CL_MakeAndSend();
    cls.realtime = realtime;

    return cl.cmdNumber != cmdNumber ? true : false;
}

// the multiplayer's own commands (the three games' common ones: CL_InitCommonInput)
static const cmdList_t inputCmds[] = {{"+attack", "Primary Attack", IN_Button0Down, nullptr},
                                      {"-attack", nullptr, IN_Button0Up, nullptr},
                                      {"+use", "Use item", IN_Button5Down, nullptr},
                                      {"-use", nullptr, IN_Button5Up, nullptr},
                                      {"+force_grip", "Hold to use grip force power", IN_Button6Down, nullptr},
                                      {"-force_grip", nullptr, IN_Button6Up, nullptr},
                                      {"+altattack", "Alternate Attack", IN_Button7Down, nullptr},
                                      {"-altattack", nullptr, IN_Button7Up, nullptr},
                                      {"+useforce", "Use selected force power", IN_Button9Down, nullptr},
                                      {"-useforce", nullptr, IN_Button9Up, nullptr},
                                      {"+force_lightning", "Hold to use lightning force power", IN_Button10Down, nullptr},
                                      {"-force_lightning", nullptr, IN_Button10Up, nullptr},
                                      {"+force_drain", "Hold to use drain force power", IN_Button11Down, nullptr},
                                      {"-force_drain", nullptr, IN_Button11Up, nullptr},
                                      {"sv_saberswitch", "Holster/activate lightsaber", IN_GenCMD1, nullptr},
                                      {"engage_duel", "Engage private duel", IN_GenCMD2, nullptr},
                                      {"force_heal", "Use heal force power", IN_GenCMD3, nullptr},
                                      {"force_speed", "Activate speed force power", IN_GenCMD4, nullptr},
                                      {"force_pull", "Use pull force power", IN_GenCMD5, nullptr},
                                      {"force_distract", "Activate mind trick force power", IN_GenCMD6, nullptr},
                                      {"force_rage", "Activate rage force power", IN_GenCMD7, nullptr},
                                      {"force_protect", "Activate protect force power", IN_GenCMD8, nullptr},
                                      {"force_absorb", "Activate absorb force power", IN_GenCMD9, nullptr},
                                      {"force_healother", "Use team heal force power", IN_GenCMD10, nullptr},
                                      {"force_forcepowerother", "Use team energize force power", IN_GenCMD11, nullptr},
                                      {"force_seeing", "Activate seeing force power", IN_GenCMD12, nullptr},
                                      {"use_seeker", "Use seeker drone item", IN_GenCMD13, nullptr},
                                      {"use_field", "Use forcefield item", IN_GenCMD14, nullptr},
                                      {"use_bacta", "Use bacta item", IN_GenCMD15, nullptr},
                                      {"use_electrobinoculars", "Use electro binoculars item", IN_GenCMD16, nullptr},
                                      {"zoom", "Use binoculars item", IN_GenCMD17, nullptr},
                                      {"use_sentry", "Use sentry gun item", IN_GenCMD18, nullptr},
                                      {"saberAttackCycle", "Switch lightsaber attack styles", IN_GenCMD19, nullptr},
                                      {"force_throw", "Use push force power", IN_GenCMD20, nullptr},
                                      {"use_jetpack", "Use jetpack item", IN_GenCMD21, nullptr},
                                      {"use_bactabig", "Use big bacta item", IN_GenCMD22, nullptr},
                                      {"use_healthdisp", "Use health dispenser item", IN_GenCMD23, nullptr},
                                      {"use_ammodisp", "Use ammo dispenser item", IN_GenCMD24, nullptr},
                                      {"use_eweb", "Use e-web item", IN_GenCMD25, nullptr},
                                      {"use_cloak", "Use cloaking item", IN_GenCMD26, nullptr},
                                      {"taunt", "Taunt", IN_GenCMD27, nullptr},
                                      {"bow", "Bow", IN_GenCMD28, nullptr},
                                      {"meditate", "Meditate", IN_GenCMD29, nullptr},
                                      {"flourish", "Flourish", IN_GenCMD30, nullptr},
                                      {"gloat", "Gloat", IN_GenCMD31, nullptr},
                                      {"useGivenForce", "Use specified force power", IN_UseGivenForce, nullptr},
                                      {"automap_button", "Show/hide automap", IN_AutoMapButton, nullptr},
                                      {"automap_toggle", "Show/hide radar", IN_AutoMapToggle, nullptr},
                                      {"voicechat", "Open voice chat menu", IN_VoiceChatButton, nullptr},
                                      {nullptr, nullptr, nullptr, nullptr}};

/*
============
CL_InitInput
============
*/
void CL_InitInput(void)
{
    CL_InitCommonInput();
    Cmd_AddCommandList(inputCmds);

    cl_nodelta = Cvar_Get("cl_nodelta", "0", cvarFlags_t::CVAR_NONE);
}

/*
============
CL_ShutdownInput
============
*/
void CL_ShutdownInput(void)
{
    CL_ShutdownCommonInput();
    Cmd_RemoveCommandList(inputCmds);
}

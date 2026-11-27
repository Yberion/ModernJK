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

// cl_input.cpp -- the single player's part of the input (src/shared/client/engine/input.h): one command per frame,
// sent to the game in this process; the buttons' bits the game reads, the force powers; no map view takes the keys.

#include <server/exe_headers.h>

#include "client.h"
#include "client_ui.h"
#include <shared/platform/engine/engine.h>

static void IN_UseGivenForce(void)
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
#ifndef JK2_MODE
    case forcePowers_t::FP_DRAIN:
        genCmdNum = genCmds_t::GENCMD_FORCE_DRAIN;
        break;
#endif // !JK2_MODE

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
        genCmdNum = genCmds_t::GENCMD_FORCE_GRIP;
        break;
    case forcePowers_t::FP_LIGHTNING:
        genCmdNum = genCmds_t::GENCMD_FORCE_LIGHTNING;
        break;

#ifndef JK2_MODE
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
#endif // !JK2_MODE

    case forcePowers_t::FP_HEAL:
        genCmdNum = genCmds_t::GENCMD_FORCE_HEAL;
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

void IN_CenterView(void)
{
    cl.viewangles[PITCH] = -SHORT2ANGLE(cl.frame.ps.delta_angles[PITCH]);
}

// no map view takes the movement keys
bool CL_MapTakesButton(const kbutton_t* button, bool down)
{
    return false;
}

// no map view takes the mouse
bool CL_MapTakesMouse(float dx, float dy)
{
    return false;
}

float CL_MousePitch(void)
{
    return m_pitch->value;
}

/*
=================
CL_PointerEvent

The system's pointer moved over the window while the menus take it
=================
*/
void CL_PointerEvent(float x, float y)
{
    if (Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_UI)) {
        _UI_MouseEvent(static_cast<int>(x), static_cast<int>(y));
    }
}

/*
=================
CL_MouseTurnsView

In game while neither the menus nor the console take the pointer, and while a video plays: no pointer is shown over a
video, whether the menus it was started from still take the keys or not
=================
*/
bool CL_MouseTurnsView(void)
{
    if (Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_CONSOLE)) {
        return false;
    }

    if (cls.state == connstate_t::CA_CINEMATIC || CL_IsRunningInGameCinematic()) {
        return true;
    }

    return cls.state == connstate_t::CA_ACTIVE && !Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_UI);
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

    if (Any(Key_GetCatcher())) {
        // cmd->buttons |= BUTTON_TALK;
    }

    // allow the game to know if any key at all is
    // currently pressed, even if it isn't bound to anything
    /*
    if ( kg.anykeydown && !Any( Key_GetCatcher( ) ) ) {
        cmd->buttons |= BUTTON_ANY;
    }
    */
}

/*
==============
CL_FinishMove
==============
*/
void CL_FinishMove(usercmd_t* cmd)
{
    int i;

    // copy the state that the cgame is currently sending
    cmd->weapon = cl.cgameUserCmdValue;

    if (cl.gcmdSendValue) {
        cmd->generic_cmd = cl.gcmdValue;
        cl.gcmdSendValue = false;
    }
    else {
        cmd->generic_cmd = 0;
    }

    // send the current server time so the amount of movement
    // can be determined without allowing cheating
    cmd->serverTime = cl.serverTime;

    for (i = 0; i < 3; i++) {
        cmd->angles[i] = ANGLE2SHORT(cl.viewangles[i]);
    }
}

/*
=================
CL_CreateCmd
=================
*/
vec3_t cl_overriddenAngles = {0, 0, 0};
bool cl_overrideAngles = false;
usercmd_t CL_CreateCmd(void)
{
    usercmd_t cmd;

    memset(&cmd, 0, sizeof(cmd));

    CL_CmdButtons(&cmd);

    // get basic movement from keyboard
    CL_KeyMove(&cmd);

    // get basic movement from mouse
    CL_MouseMoveToCommand(&cmd);

    // get basic movement from joystick
    CL_JoystickMove(&cmd);

    if (cl_overrideAngles) {
        VectorCopy(cl_overriddenAngles, cl.viewangles);
        cl_overrideAngles = false;
    }
    // store out the final values
    CL_FinishMove(&cmd);

    return cmd;
}

/*
=================
CL_CreateNewCommands

Turns the view, then creates a new usercmd_t structure for this frame
=================
*/
void CL_CreateNewCommands(void)
{
    int cmdNum;

    // no need to create usercmds until we have a gamestate
    //	if ( cls.state < connstate_t::CA_PRIMED )
    //		return;

    // Com_Frame has just read the input (Platform_Frame)
    CL_InputRead(com_frameTime, Platform_InputNanoseconds());
    CL_UpdateViewAngles();

    // generate a command for this frame
    CL_CommandStart(true);
    cl.cmdNumber++;
    cmdNum = cl.cmdNumber & CMD_MASK;
    cl.cmds[cmdNum] = CL_CreateCmd();
}

/*
=================
CL_ReadyToSendPacket

Returns false if we are over the maxpackets limit
and should choke back the bandwidth a bit by not sending
a packet this frame.  All the commands will still get
delivered in the next packet, but saving a header and
getting more delta compression will reduce total bandwidth.
=================
*/
bool CL_ReadyToSendPacket(void)
{
    // don't send anything if playing back a demo
    //	if ( cls.state == connstate_t::CA_CINEMATIC )
    if (cls.state == connstate_t::CA_CINEMATIC || CL_IsRunningInGameCinematic()) {
        return false;
    }

    // if we don't have a valid gamestate yet, only send
    // one packet a second
    if (cls.state != connstate_t::CA_ACTIVE && cls.state != connstate_t::CA_PRIMED && cls.realtime - clc.lastPacketSentTime < 1000) {
        return false;
    }

    // send every frame for loopbacks
    return true;
}

/*
===================
CL_WritePacket

Create and send the command packet to the server
Including both the reliable commands and the usercmds

During normal gameplay, a client packet will contain something like:

4	sequence number
2	qport
4	serverid
4	acknowledged sequence number
4	clc.serverCommandSequence
<optional reliable commands>
1	clc_move or clc_moveNoDelta
1	command count
<count * usercmds>

===================
*/
void CL_WritePacket(void)
{
    msg_t buf;
    byte data[MAX_MSGLEN];
    int i, j;
    usercmd_t *cmd, *oldcmd;
    usercmd_t nullcmd;
    int packetNum;
    int oldPacketNum;
    int count;

    // don't send anything if playing back a demo
    //	if ( cls.state == connstate_t::CA_CINEMATIC )
    if (cls.state == connstate_t::CA_CINEMATIC || CL_IsRunningInGameCinematic()) {
        return;
    }

    MSG_Init(&buf, data, sizeof(data));

    // write any unacknowledged clientCommands
    for (i = clc.reliableAcknowledge + 1; i <= clc.reliableSequence; i++) {
        MSG_WriteByte(&buf, std::to_underlying(clc_ops_e::clc_clientCommand));
        MSG_WriteLong(&buf, i);
        MSG_WriteString(&buf, clc.reliableCommands[i & (MAX_RELIABLE_COMMANDS - 1)]);
    }

    // we want to send all the usercmds that were generated in the last
    // few packet, so even if a couple packets are dropped in a row,
    // all the cmds will make it to the server
    if (cl_packetdup->integer < 0) {
        Cvar_Set("cl_packetdup", "0");
    }
    else if (cl_packetdup->integer > 5) {
        Cvar_Set("cl_packetdup", "5");
    }
    oldPacketNum = (clc.netchan.outgoingSequence - 1 - cl_packetdup->integer) & PACKET_MASK;
    count = cl.cmdNumber - cl.packetCmdNumber[oldPacketNum];
    if (count > MAX_PACKET_USERCMDS) {
        count = MAX_PACKET_USERCMDS;
        Q_Println("MAX_PACKET_USERCMDS");
    }
    if (count >= 1) {
        // begin a client move command
        MSG_WriteByte(&buf, std::to_underlying(clc_ops_e::clc_move));

        // write the last reliable message we received
        MSG_WriteLong(&buf, clc.serverCommandSequence);

        // write the current serverId so the server
        // can tell if this is from the current gameState
        MSG_WriteLong(&buf, cl.serverId);

        // write the current time
        MSG_WriteLong(&buf, cls.realtime);

        // let the server know what the last messagenum we
        // got was, so the next message can be delta compressed
        // FIXME: this could just be a bit flag, with the message implicit
        // from the unreliable ack of the netchan
        if (cl_nodelta->integer || !cl.frame.valid) {
            MSG_WriteLong(&buf, -1); // no compression
        }
        else {
            MSG_WriteLong(&buf, cl.frame.messageNum);
        }

        // write the cmdNumber so the server can determine which ones it
        // has already received
        MSG_WriteLong(&buf, cl.cmdNumber);

        // write the command count
        MSG_WriteByte(&buf, count);

        // write all the commands, including the predicted command
        memset(&nullcmd, 0, sizeof(nullcmd));
        oldcmd = &nullcmd;
        for (i = 0; i < count; i++) {
            j = (cl.cmdNumber - count + i + 1) & CMD_MASK;
            cmd = &cl.cmds[j];
            MSG_WriteDeltaUsercmd(&buf, oldcmd, cmd);
            oldcmd = cmd;
        }
    }

    //
    // deliver the message
    //
    packetNum = clc.netchan.outgoingSequence & PACKET_MASK;
    cl.packetTime[packetNum] = cls.realtime;
    cl.packetCmdNumber[packetNum] = cl.cmdNumber;
    clc.lastPacketSentTime = cls.realtime;
    Netchan_Transmit(&clc.netchan, buf.cursize, buf.data);
}

/*
=================
CL_SendCmd

Called every frame to builds and sends a command packet to the server.
=================
*/
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

    // we create commands even if a demo is playing,
    CL_CreateNewCommands();

    // don't send a packet if the last packet was sent too recently
    if (!CL_ReadyToSendPacket()) {
        return;
    }

    CL_WritePacket();
}

/*
============
CL_InitInput
============
*/
void CL_InitInput(void)
{
    CL_InitCommonInput();

    // the single player's own commands (the three games' common ones: CL_InitCommonInput)
    Cmd_AddCommand("useGivenForce", IN_UseGivenForce);
    // buttons
    Cmd_AddCommand("+attack", IN_Button0Down); // attack
    Cmd_AddCommand("-attack", IN_Button0Up);
    Cmd_AddCommand("+force_lightning", IN_Button1Down); // force lightning
    Cmd_AddCommand("-force_lightning", IN_Button1Up);
    Cmd_AddCommand("+useforce", IN_Button2Down); // use current force power
    Cmd_AddCommand("-useforce", IN_Button2Up);
#ifdef JK2_MODE
    Cmd_AddCommand("+block", IN_Button3Down); // manual blocking
    Cmd_AddCommand("-block", IN_Button3Up);
#else
    Cmd_AddCommand("+force_drain", IN_Button3Down); // force drain
    Cmd_AddCommand("-force_drain", IN_Button3Up);
#endif
    Cmd_AddCommand("+walk", IN_Button4Down); // walking
    Cmd_AddCommand("-walk", IN_Button4Up);
    Cmd_AddCommand("+use", IN_Button5Down); // use object
    Cmd_AddCommand("-use", IN_Button5Up);
    Cmd_AddCommand("+force_grip", IN_Button6Down); // force jump
    Cmd_AddCommand("-force_grip", IN_Button6Up);
    Cmd_AddCommand("+altattack", IN_Button7Down); // altattack
    Cmd_AddCommand("-altattack", IN_Button7Up);
#ifndef JK2_MODE
    Cmd_AddCommand("+forcefocus", IN_Button8Down); // special saber attacks
    Cmd_AddCommand("-forcefocus", IN_Button8Up);
    Cmd_AddCommand("+block", IN_Button8Down); // manual blocking
    Cmd_AddCommand("-block", IN_Button8Up);
#endif
    // end buttons

    cl_nodelta = Cvar_Get("cl_nodelta", "0", cvarFlags_t::CVAR_NONE);
}

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

// cl_main.c  -- client main loop

#include "client.h"

#include <limits.h>
#include <ghoul2/G2.h>
#include <qcommon/cm_public.h>
#include <qcommon/MiniHeap.h>
#include <qcommon/stringed_ingame.h>
#include <qcommon/game_version.h>
#include "cl_cgameapi.h"
#include "cl_uiapi.h"
#include "cl_lan.h"
#include <shared/sys/sys_loadlib.h>
#include <shared/platform/engine/engine.h>

cvar_t* cl_renderer;

cvar_t* cl_nodelta;
cvar_t* cl_debugMove;

cvar_t* cl_noprint;
cvar_t* cl_motd;
cvar_t* cl_motdServer[MAX_MASTER_SERVERS];

cvar_t* rcon_client_password;
cvar_t* rconAddress;

cvar_t* cl_debugInterpForce;
cvar_t* cl_debugInterpDelay;
cvar_t* cl_showTimeDelta;
cvar_t* cl_freezeDemo;

cvar_t* cl_shownet;
cvar_t* cl_showSend;
cvar_t* cl_timedemo;

cvar_t* cl_freelook;
cvar_t* cl_sensitivity;

cvar_t* cl_mouseAccel;
cvar_t* cl_mouseAccelOffset;
cvar_t* cl_mouseAccelStyle;
cvar_t* cl_showMouseRate;

cvar_t* m_pitchVeh;
cvar_t* m_pitch;
cvar_t* m_yaw;
cvar_t* m_forward;
cvar_t* m_side;
cvar_t* m_filter;

cvar_t* cl_activeAction;

cvar_t* cl_motdString;

cvar_t* cl_allowDownload;
cvar_t* cl_allowAltEnter;
cvar_t* cl_conXOffset;
cvar_t* cl_inGameVideo;

cvar_t* cl_serverStatusResendTime;
cvar_t* cl_framerate;

// cvar to enable sending a "ja_guid" player identifier in userinfo to servers
// ja_guid is a persistent "cookie" that allows servers to track players across game sessions
cvar_t* cl_enableGuid;
cvar_t* cl_guidServerUniq;

cvar_t* cl_autolodscale;

cvar_t* cl_drawRecording;

cvar_t* cl_downloadName;
cvar_t* cl_downloadPrompt;
cvar_t* cl_downloadOverlay;

cvar_t* cl_filterGames;

vec3_t cl_windVec;

clientActive_t cl;
clientConnection_t clc;
clientStatic_t cls;

netadr_t rcon_address;

cvar_t* cl_reconnectArgs;

// Structure containing functions exported from refresh DLL
refexport_t* re = nullptr;
static void* rendererLib = nullptr;

ping_t cl_pinglist[MAX_PINGREQUESTS];

typedef struct serverStatus_s {
    char string[BIG_INFO_STRING];
    netadr_t address;
    int time, startTime;
    bool pending;
    bool print;
    bool retrieved;
} serverStatus_t;

serverStatus_t cl_serverStatusList[MAX_SERVERSTATUSREQUESTS];
int serverStatusCount;

IHeapAllocator* G2VertSpaceClient = nullptr;

extern void SV_BotFrame(int time);
void CL_CheckForResend(void);
void CL_ShowIP_f(void);
void CL_ServerStatus_f(void);
void CL_ServerStatusResponse(const netadr_t* from, msg_t* msg);
static void CL_ShutdownRef(bool restarting);

/*
=======================================================================

CLIENT RELIABLE COMMAND COMMUNICATION

=======================================================================
*/

/*
======================
CL_AddReliableCommand

The given command will be transmitted to the server, and is gauranteed to
not have future usercmd_t executed before it is executed
======================
*/
void CL_AddReliableCommand(const char* cmd, bool isDisconnectCmd)
{
    int unacknowledged = clc.reliableSequence - clc.reliableAcknowledge;

    // if we would be losing an old command that hasn't been acknowledged,
    // we must drop the connection
    // also leave one slot open for the disconnect command in this case.

    if ((isDisconnectCmd && unacknowledged > MAX_RELIABLE_COMMANDS) || (!isDisconnectCmd && unacknowledged >= MAX_RELIABLE_COMMANDS)) {
        if (com_errorEntered)
            return;
        else
            Q_Error(errorParm_t::ERR_DROP, "Client command overflow");
    }

    Q_strncpyz(clc.reliableCommands[++clc.reliableSequence & (MAX_RELIABLE_COMMANDS - 1)], cmd, sizeof(*clc.reliableCommands));
}

/*
=======================================================================

CLIENT SIDE DEMO RECORDING

=======================================================================
*/

/*
====================
CL_StopRecording_f

stop recording a demo
====================
*/
void CL_StopRecord_f(void)
{
    int len;

    if (!clc.demorecording) {
        Q_Println("Not recording a demo.");
        return;
    }

    // finish up
    CL_NetcodecDemoStop();
    len = -1;
    FS_Write(&len, 4, clc.demofile);
    FS_Write(&len, 4, clc.demofile);
    FS_FCloseFile(clc.demofile);
    clc.demofile = 0;
    clc.demorecording = false;
    clc.spDemoRecording = false;
    Q_Println("Stopped demo.");
}

/*
==================
CL_DemoFilename
==================
*/
void CL_DemoFilename(char* buf, int bufSize)
{
    time_t rawtime;
    char timeStr[32] = {0}; // should really only reach ~19 chars

    time(&rawtime);
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d_%H-%M-%S", localtime(&rawtime)); // or gmtime

    Q_FormatTo(std::span(buf, bufSize), "demo{}", timeStr);
}

/*
====================
CL_Record_f

record <demoname>

Begins recording a demo from the current position
====================
*/
static char demoName[MAX_QPATH]; // compiler bug workaround
void CL_Record_f(void)
{
    char name[MAX_OSPATH];
    const char* s;

    if (Cmd_Argc() > 2) {
        Q_Println("record <demoname>");
        return;
    }

    if (clc.demorecording) {
        if (!clc.spDemoRecording) {
            Q_Println("Already recording.");
        }
        return;
    }

    if (cls.state != connstate_t::CA_ACTIVE) {
        Q_Println("You must be in a level to record.");
        return;
    }

    if (Cmd_Argc() == 2) {
        s = Cmd_Argv(1);
        Q_strncpyz(demoName, s, sizeof(demoName));
        Q_FormatTo(name, "demos/{}.dm_{}", demoName, PROTOCOL_VERSION);
    }
    else {
        // timestamp the file
        CL_DemoFilename(demoName, sizeof(demoName));

        Q_FormatTo(name, "demos/{}.dm_{}", demoName, PROTOCOL_VERSION);

        if (FS_FileExists(name)) {
            Q_Println("Record: Couldn't create a file");
            return;
        }
    }

    // open the demo file

    Q_Println("recording to {}.", name);
    clc.demofile = FS_FOpenFileWrite(name);
    if (!clc.demofile) {
        Q_PrintlnError("couldn't open.");
        return;
    }
    clc.demorecording = true;
    if (Cvar_VariableValue("ui_recordSPDemo")) {
        clc.spDemoRecording = true;
    }
    else {
        clc.spDemoRecording = false;
    }

    Q_strncpyz(clc.demoName, demoName, sizeof(clc.demoName));

    // write out the gamestate message; every message decoded from now on is coded again into the demo against the
    // demo's own last snapshot (cl_netcodec.cpp, qcommon/demo_record.cpp), so no full snapshot needs to be asked of
    // the server
    if (!CL_NetcodecRecordGameState()) {
        Q_PrintlnError("the gamestate could not be coded.");
        FS_FCloseFile(clc.demofile);
        clc.demofile = 0;
        clc.demorecording = false;
        clc.spDemoRecording = false;
    }
}

/*
=======================================================================

CLIENT SIDE DEMO PLAYBACK

=======================================================================
*/

/*
=================
CL_DemoCompleted
=================
*/
void CL_DemoCompleted(void)
{
    if (cl_timedemo && cl_timedemo->integer) {
        int time;

        time = Sys_Milliseconds() - clc.timeDemoStart;
        if (time > 0) {
            Q_Println("{} frames, {:3.1f} seconds: {:3.1f} fps", clc.timeDemoFrames, time / 1000.0, clc.timeDemoFrames * 1000.0 / time);
        }
    }

    /*	CL_Disconnect( true );
        CL_NextDemo();
        */

    // rww - The above code seems to just stick you in a no-menu state and you can't do anything there.
    // I'm not sure why it ever worked in TA, but whatever. This code will bring us back to the main menu
    // after a demo is finished playing instead.
    CL_Disconnect_f();
    SND_StopAll();
    UIVM_SetActiveMenu(uiMenuCommand_t::UIMENU_MAIN);

    CL_NextDemo();
}

/*
=================
CL_ReadDemoMessage
=================
*/
void CL_ReadDemoMessage(void)
{
    int r;
    msg_t buf;
    byte bufData[MAX_MSGLEN];
    int s;

    if (!clc.demofile) {
        CL_DemoCompleted();
        return;
    }

    // get the sequence number
    r = FS_Read(&s, 4, clc.demofile);
    if (r != 4) {
        CL_DemoCompleted();
        return;
    }
    clc.serverMessageSequence = LittleLong(s);

    // init the message
    MSG_Init(&buf, bufData, sizeof(bufData));

    // get the length
    r = FS_Read(&buf.cursize, 4, clc.demofile);
    if (r != 4) {
        CL_DemoCompleted();
        return;
    }
    buf.cursize = LittleLong(buf.cursize);
    if (buf.cursize == -1) {
        CL_DemoCompleted();
        return;
    }
    if (buf.cursize > buf.maxsize) {
        Q_Error(errorParm_t::ERR_DROP, "CL_ReadDemoMessage: demoMsglen > MAX_MSGLEN");
    }
    r = FS_Read(buf.data, buf.cursize, clc.demofile);
    if (r != buf.cursize) {
        Q_Println("Demo file was truncated.");
        CL_DemoCompleted();
        return;
    }

    clc.lastPacketTime = cls.realtime;
    buf.readcount = 0;
    CL_ParseServerMessage(&buf);
}

/*
====================
CL_CompleteDemoName
====================
*/
static void CL_CompleteDemoName(char* args, int argNum)
{
    if (argNum == 2) {
        char demoExt[16];

        Q_FormatTo(demoExt, ".dm_{}", PROTOCOL_VERSION);
        Field_CompleteFilename("demos", demoExt, true, true);
    }
}

/*
====================
CL_PlayDemo_f

demo <demoname>

====================
*/
void CL_PlayDemo_f(void)
{
    char name[MAX_OSPATH], extension[32];
    const char* arg;

    if (Cmd_Argc() != 2) {
        Q_Println("demo <demoname>");
        return;
    }

    // make sure a local server is killed
    // 2 means don't force disconnect of local client
    Cvar_Set("sv_killserver", "2");

    // open the demo file
    arg = Cmd_Argv(1);

    CL_Disconnect(true);

    Q_FormatTo(extension, ".dm_{}", PROTOCOL_VERSION);
    if (!Q_stricmp(arg + strlen(arg) - strlen(extension), extension)) {
        Q_FormatTo(name, "demos/{}", arg);
    }
    else {
        Q_FormatTo(name, "demos/{}.dm_{}", arg, PROTOCOL_VERSION);
    }

    FS_FOpenFileRead(name, &clc.demofile, true);
    if (!clc.demofile) {
        if (!Q_stricmp(arg, "(null)")) {
            Q_Error(errorParm_t::ERR_DROP, "{}", SE_GetString("CON_TEXT_NO_DEMO_SELECTED"));
        }
        else {
            Q_Error(errorParm_t::ERR_DROP, "couldn't open {}", name);
        }
        return;
    }
    Q_strncpyz(clc.demoName, Cmd_Argv(1), sizeof(clc.demoName));

    Con_Close();

    cls.state = connstate_t::CA_CONNECTED;
    clc.demoplaying = true;
    Q_strncpyz(cls.servername, Cmd_Argv(1), sizeof(cls.servername));

    // read demo messages until connected
    while (cls.state >= connstate_t::CA_CONNECTED && cls.state < connstate_t::CA_PRIMED) {
        CL_ReadDemoMessage();
    }
    // don't get the first snapshot this frame, to prevent the long
    // time from the gamestate load from messing causing a time skip
    clc.firstDemoFrameSkipped = false;
}

/*
====================
CL_StartDemoLoop

Closing the main menu will restart the demo loop
====================
*/
void CL_StartDemoLoop(void)
{
    // start the demo loop again
    Cbuf_AddText("d1\n");
    Key_SetCatcher(keyCatchFlags_t::KEYCATCH_NONE);
}

/*
==================
CL_NextDemo

Called when a demo or cinematic finishes
If the "nextdemo" cvar is set, that command will be issued
==================
*/
void CL_NextDemo(void)
{
    char v[MAX_STRING_CHARS];

    Q_strncpyz(v, Cvar_VariableString("nextdemo"), sizeof(v));
    v[MAX_STRING_CHARS - 1] = 0;
    Q_DPrintln("CL_NextDemo: {}", v);
    if (!v[0]) {
        return;
    }

    Cvar_Set("nextdemo", "");
    Cbuf_AddText(v);
    Cbuf_AddText("\n");
    Cbuf_Execute();
}

//======================================================================

/*
=====================
CL_ShutdownAll
=====================
*/
void CL_ShutdownAll(bool shutdownRef)
{
    if (clc.demorecording)
        CL_StopRecord_f();

#if 0 // rwwFIXMEFIXME: Disable this before release!!!!!! I am just trying to find a crash bug.
	//so it doesn't barf on shutdown saying refentities belong to each other
	tr.refdef.num_entities = 0;
#endif

    // clear sounds
    SND_StopAll();
    // shutdown CGame
    CL_ShutdownCGame();
    // shutdown UI
    CL_ShutdownUI();

    // shutdown the renderer
    if (shutdownRef)
        CL_ShutdownRef(false);
    if (re && re->Shutdown) {
        re->Shutdown(false, false); // don't destroy window or context
    }

    cls.uiStarted = false;
    cls.cgameStarted = false;
    cls.rendererStarted = false;
    cls.soundRegistered = false;
}

/*
=================
CL_FlushMemory

Called by CL_MapLoading, CL_Connect_f, CL_PlayDemo_f, and CL_ParseGamestate the only
ways a client gets into a game
Also called by Q_Error
=================
*/
void CL_FlushMemory(void)
{

    // shutdown all the client stuff
    CL_ShutdownAll(false);

    // if not running a server clear the whole hunk
    if (!com_sv_running->integer) {
        // clear collision map data
        CM_ClearMap();
        // clear the whole hunk
        Hunk_Clear();
    }
    else {
        // clear all the client data on the hunk
        Hunk_ClearToMark();
    }

    CL_StartHunkUsers();
}

/*
=====================
CL_MapLoading

A local server is starting to load a map, so update the
screen to let the user know about it, then dump all client
memory on the hunk from cgame, ui, and renderer
=====================
*/
void CL_MapLoading(void)
{
    if (!com_cl_running->integer) {
        return;
    }

    // Set this to localhost.
    Cvar_Set("cl_currentServerAddress", "Localhost");
    Cvar_Set("cl_currentServerIP", "loopback");

    Con_Close();
    Key_SetCatcher(keyCatchFlags_t::KEYCATCH_NONE);

    // if we are already connected to the local host, stay connected
    if (cls.state >= connstate_t::CA_CONNECTED && !Q_stricmp(cls.servername, "localhost")) {
        cls.state = connstate_t::CA_CONNECTED; // so the connect screen is drawn
        Com_Memset(cls.updateInfoString, 0, sizeof(cls.updateInfoString));
        Com_Memset(clc.serverMessage, 0, sizeof(clc.serverMessage));
        Com_Memset(&cl.gameState, 0, sizeof(cl.gameState));
        clc.lastPacketSentTime = -9999;
        SCR_UpdateScreen();
    }
    else {
        // clear nextmap so the cinematic shutdown doesn't execute it
        Cvar_Set("nextmap", "");
        CL_Disconnect(true);
        Q_strncpyz(cls.servername, "localhost", sizeof(cls.servername));
        cls.state = connstate_t::CA_CHALLENGING; // so the connect screen is drawn
        Key_SetCatcher(keyCatchFlags_t::KEYCATCH_NONE);
        SCR_UpdateScreen();
        clc.connectTime = -RETRANSMIT_TIMEOUT;
        NET_StringToAdr(cls.servername, &clc.serverAddress);
        // we don't need a challenge on the localhost

        CL_CheckForResend();
    }
}

/*
=====================
CL_ClearState

Called before parsing a gamestate
=====================
*/
void CL_ClearState(void)
{

    //	SND_StopAll();
    Com_Memset(&cl, 0, sizeof(cl));
    CL_NetcodecReset(); // its frames point into cl.snapshots and cl.parseEntities
}

/*
====================
CL_UpdateGUID

update cl_guid using QKEY_FILE and optional prefix
====================
*/
static void CL_UpdateGUID(const char* prefix, int prefix_len)
{
    if (cl_enableGuid->integer) {
        fileHandle_t f;
        int len;

        len = FS_SV_FOpenFileRead(QKEY_FILE, &f);
        FS_FCloseFile(f);

        // initialize the cvar here in case it's unset or was user-created
        // while tracking was disabled (removes CVAR_USER_CREATED)
        Cvar_Get("ja_guid", "", cvarFlags_t::CVAR_USERINFO | cvarFlags_t::CVAR_ROM, "Client GUID");

        if (len != QKEY_SIZE) {
            Cvar_Set("ja_guid", "");
        }
        else {
            Cvar_Set("ja_guid", Com_MD5File(QKEY_FILE, QKEY_SIZE, prefix, prefix_len));
        }
    }
    else {
        // Remove the cvar entirely if tracking is disabled
        const std::optional<cvarFlags_t> flags = Cvar_Flags("ja_guid");
        // keep the cvar if it's user-created, but destroy it otherwise
        if (flags && !Any(*flags & cvarFlags_t::CVAR_USER_CREATED)) {
            cvar_t* ja_guid = Cvar_Get("ja_guid", "", cvarFlags_t::CVAR_NONE, "Client GUID");
            Cvar_Unset(ja_guid);
        }
    }
}

/*
=====================
CL_Disconnect

Called when a connection, demo, or cinematic is being terminated.
Goes from a connected state to either a menu state or a console state
Sends a disconnect message to the server
This is also called on Q_Error and Com_Quit, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect(bool showMainMenu)
{
    if (!com_cl_running || !com_cl_running->integer) {
        return;
    }

    // shutting down the client so enter full screen ui mode
    Cvar_Set("r_uiFullScreen", "1");

    if (clc.demorecording) {
        CL_StopRecord_f();
    }

    if (clc.download) {
        FS_FCloseFile(clc.download);
        clc.download = 0;
    }
    *clc.downloadTempName = *clc.downloadName = 0;
    Cvar_Set("cl_downloadName", "");

    if (clc.demofile) {
        FS_FCloseFile(clc.demofile);
        clc.demofile = 0;
    }

    if (cls.uiStarted && showMainMenu) {
        UIVM_SetActiveMenu(uiMenuCommand_t::UIMENU_NONE);
    }

    SCR_StopCinematic();

    // the level's sounds stopped, since its loops would play on while connecting to another server, and its reverb zones
    // left, so that no reverb is computed in the menus
    SND_StopSounds();
    SND_LoadZones("");
    Cvar_Set("s_musicMult", "1"); // the music the cgame was lowering under a tune back to its level

    // send a disconnect message to the server
    // send it a few times in case one is dropped
    if (cls.state >= connstate_t::CA_CONNECTED) {
        CL_AddReliableCommand("disconnect", true);
        CL_WritePacket();
        CL_WritePacket();
        CL_WritePacket();
    }

    // Remove pure paks
    FS_PureServerSetLoadedPaks("", "");
    FS_PureServerSetReferencedPaks("", "");

    CL_ClearState();

    // wipe the client connection
    Com_Memset(&clc, 0, sizeof(clc));

    cls.state = connstate_t::CA_DISCONNECTED;

    // allow cheats locally
    Cvar_Set("sv_cheats", "1");

    // not connected to a pure server anymore
    cl_connectedToPureServer = false;

    CL_UpdateGUID(nullptr, 0);
}

/*
===================
CL_ForwardCommandToServer

adds the current command line as a clientCommand
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void CL_ForwardCommandToServer(const char* string)
{
    const char* cmd;

    cmd = Cmd_Argv(0);

    // ignore key up commands
    if (cmd[0] == '-') {
        return;
    }

    if (clc.demoplaying || cls.state < connstate_t::CA_CONNECTED || cmd[0] == '+') {
        Q_Println("Unknown command \"{}" S_COLOR_WHITE "\"", cmd);
        return;
    }

    if (Cmd_Argc() > 1) {
        CL_AddReliableCommand(string, false);
    }
    else {
        CL_AddReliableCommand(cmd, false);
    }
}

/*
===================
CL_RequestMotd

===================
*/
void CL_RequestMotd(void)
{
    netadr_t to;
    int i;
    char command[MAX_STRING_CHARS], info[MAX_INFO_STRING];
    const char* motdaddress;

    if (!cl_motd->integer) {
        return;
    }

    if (cl_motd->integer < 1 || cl_motd->integer > MAX_MASTER_SERVERS) {
        Q_Println("CL_RequestMotd: Invalid motd server num. Valid values are 1-{} or 0 to disable", MAX_MASTER_SERVERS);
        return;
    }

    Q_FormatTo(command, "cl_motdServer{}", cl_motd->integer);
    motdaddress = Cvar_VariableString(command);

    if (!*motdaddress) {
        Q_Println("CL_RequestMotd: Error: No motd server address given.");
        return;
    }

    i = NET_StringToAdr(motdaddress, &to);

    if (!i) {
        Q_Println("CL_RequestMotd: Error: could not resolve address of motd server {}", motdaddress);
        return;
    }
    to.type = netadrtype_t::NA_IP;
    to.port = BigShort(PORT_UPDATE);

    Q_Println("Requesting motd from update {} ({})...", motdaddress, NET_AdrToString(&to));

    cls.updateServer = to;

    info[0] = 0;
    // NOTE TTimo xoring against Com_Milliseconds, otherwise we may not have a true randomization
    // the challenge from the system's random bytes, which an attacker cannot guess (rand() and the clock could be)
    {
        int challenge;
        Com_RandomBytes((byte*)&challenge, sizeof(challenge));
        Q_FormatTo(cls.updateChallenge, "{}", challenge);
    }

    Info_SetValueForKey(info, "challenge", cls.updateChallenge);
    Info_SetValueForKey(info, "renderer", cls.glconfig.renderer_string);
    Info_SetValueForKey(info, "rvendor", cls.glconfig.vendor_string);
    Info_SetValueForKey(info, "version", JK_VERSION_OLD " " PLATFORM_STRING " " SOURCE_DATE);

    // If raven starts filtering for this, add this code back in
#if 0
	Info_SetValueForKey( info, "cputype", "Intel Pentium IV");
	Info_SetValueForKey( info, "mhz", "3000" );
	Info_SetValueForKey( info, "memory", "4096" );
#endif
    Info_SetValueForKey(info, "joystick", Cvar_VariableString("in_joystick"));
    Info_SetValueForKey(info, "colorbits", Q_Format("{}", cls.glconfig.colorBits).c_str());

    NET_OutOfBandPrint(netsrc_t::NS_CLIENT, &cls.updateServer, "getmotd \"{}\"\n", info);
}

/*
======================================================================

CONSOLE COMMANDS

======================================================================
*/

/*
==================
CL_ForwardToServer_f
==================
*/
void CL_ForwardToServer_f(void)
{
    if (cls.state != connstate_t::CA_ACTIVE || clc.demoplaying) {
        Q_Println("Not connected to a server.");
        return;
    }

    // don't forward the first argument
    if (Cmd_Argc() > 1) {
        CL_AddReliableCommand(Cmd_Args(), false);
    }
}

/*
==================
CL_Disconnect_f
==================
*/
void CL_Disconnect_f(void)
{
    SCR_StopCinematic();
    Cvar_Set("ui_singlePlayerActive", "0");
    if (cls.state != connstate_t::CA_DISCONNECTED && cls.state != connstate_t::CA_CINEMATIC) {
        Q_Error(errorParm_t::ERR_DISCONNECT, "Disconnected from server");
    }
}

/*
================
CL_Reconnect_f

================
*/
void CL_Reconnect_f(void)
{
    if (!strlen(cl_reconnectArgs->string)) {
        return;
    }
    Cvar_Set("ui_singlePlayerActive", "0");
    Cbuf_AddText(Q_Format("connect {}\n", cl_reconnectArgs->string).c_str());
}

/*
================
CL_Connect_f

================
*/
void CL_Connect_f(void)
{
    const char* server;
    const char* serverString;

    if (Cmd_Argc() != 2) {
        Q_Println("usage: connect [server]");
        return;
    }

    // save arguments for reconnect
    Cvar_Set("cl_reconnectArgs", Cmd_Args());

    Cvar_Set("ui_singlePlayerActive", "0");

    // fire a message off to the motd server
    CL_RequestMotd();

    // clear any previous "server full" type messages
    clc.serverMessage[0] = 0;

    server = Cmd_Argv(1);

    if (com_sv_running->integer && !strcmp(server, "localhost")) {
        // if running a local server, kill it
        SV_Shutdown("Server quit\n");
    }

    // make sure a local server is killed
    Cvar_Set("sv_killserver", "1");
    SV_Frame(0);

    CL_Disconnect(true);
    Con_Close();

    Q_strncpyz(cls.servername, server, sizeof(cls.servername));

    if (!NET_StringToAdr(cls.servername, &clc.serverAddress)) {
        Q_Println("Bad server address");
        cls.state = connstate_t::CA_DISCONNECTED;
        return;
    }
    if (clc.serverAddress.port == 0) {
        clc.serverAddress.port = BigShort(PORT_SERVER);
    }

    serverString = NET_AdrToString(&clc.serverAddress);

    Q_Println("{} resolved to {}", cls.servername, serverString);

    if (cl_guidServerUniq->integer)
        CL_UpdateGUID(serverString, strlen(serverString));
    else
        CL_UpdateGUID(nullptr, 0);

    // if we aren't playing on a lan, we need to authenticate
    if (NET_IsLocalAddress(&clc.serverAddress)) {
        cls.state = connstate_t::CA_CHALLENGING;
    }
    else {
        cls.state = connstate_t::CA_CONNECTING;

        // Set a client challenge number that ideally is mirrored back by the server: the system's random bytes, which
        // an attacker cannot guess
        Com_RandomBytes((byte*)&clc.challenge, sizeof(clc.challenge));
    }

    Key_SetCatcher(keyCatchFlags_t::KEYCATCH_NONE);
    clc.connectTime = -99999; // CL_CheckForResend() will fire immediately
    clc.connectPacketCount = 0;

    // server connection string
    Cvar_Set("cl_currentServerAddress", server);
    Cvar_Set("cl_currentServerIP", serverString);
}

constexpr int MAX_RCON_MESSAGE = 1024;

/*
==================
CL_CompleteRcon
==================
*/
static void CL_CompleteRcon(char* args, int argNum)
{
    if (argNum == 2) {
        // Skip "rcon "
        char* p = Com_SkipTokens(args, 1, " ");

        if (p > args)
            Field_CompleteCommand(p, true, true);
    }
}

/*
=====================
CL_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
void CL_Rcon_f(void)
{
    char message[MAX_RCON_MESSAGE];

    if (!rcon_client_password->string[0]) {
        Q_Println("You must set 'rconpassword' before issuing an rcon command.");
        return;
    }

    message[0] = -1;
    message[1] = -1;
    message[2] = -1;
    message[3] = -1;
    message[4] = 0;

    Q_strcat(message, MAX_RCON_MESSAGE, "rcon ");

    Q_strcat(message, MAX_RCON_MESSAGE, rcon_client_password->string);
    Q_strcat(message, MAX_RCON_MESSAGE, " ");

    // https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=543
    Q_strcat(message, MAX_RCON_MESSAGE, Cmd_Cmd() + 5);

    if (cls.state >= connstate_t::CA_CONNECTED) {
        rcon_address = clc.netchan.remoteAddress;
    }
    else {
        if (!strlen(rconAddress->string)) {
            Q_Println("You must either be connected,\nor set the 'rconAddress' cvar\nto issue rcon commands");

            return;
        }
        NET_StringToAdr(rconAddress->string, &rcon_address);
        if (rcon_address.port == 0) {
            rcon_address.port = BigShort(PORT_SERVER);
        }
    }

    NET_SendPacket(netsrc_t::NS_CLIENT, strlen(message) + 1, message, &rcon_address);
}

/*
=================
CL_SendPureChecksums
=================
*/
void CL_SendPureChecksums(void)
{
    char cMsg[MAX_INFO_VALUE];

    // if we are pure we need to send back a command with our referenced pk3 checksums
    Q_FormatTo(cMsg, "cp {}", FS_ReferencedPakPureChecksums());

    CL_AddReliableCommand(cMsg, false);
}

/*
=================
CL_ResetPureClientAtServer
=================
*/
void CL_ResetPureClientAtServer(void)
{
    CL_AddReliableCommand("vdr", false);
}

/*
=================
CL_Vid_Restart_f

Restart the video subsystem

we also have to reload the UI and CGame because the renderer
doesn't know what graphics to reload
=================
*/
extern bool g_nOverrideChecked;
void CL_Vid_Restart_f(void)
{
    if (clc.demorecording)
        CL_StopRecord_f();

    // rww - sort of nasty, but when a user selects a mod
    // from the menu all it does is a vid_restart, so we
    // have to check for new net overrides for the mod then.
    g_nOverrideChecked = false;

    // don't let them loop during the restart
    SND_StopAll();
    // shutdown the UI
    CL_ShutdownUI();
    // shutdown the CGame
    CL_ShutdownCGame();
    // shutdown the renderer and clear the renderer interface
    CL_ShutdownRef(true);
    // client is no longer pure untill new checksums are sent
    CL_ResetPureClientAtServer();
    // clear pak references
    FS_ClearPakReferences(fsRefFlags_t::FS_UI_REF | fsRefFlags_t::FS_CGAME_REF);
    // reinitialize the filesystem if the game directory or checksum has changed
    FS_ConditionalRestart(clc.checksumFeed);

    cls.rendererStarted = false;
    cls.uiStarted = false;
    cls.cgameStarted = false;
    cls.soundRegistered = false;

    // unpause so the cgame definately gets a snapshot and renders a frame
    Cvar_Set("cl_paused", "0");

    // if not running a server clear the whole hunk
    if (!com_sv_running->integer) {
        CM_ClearMap();
        // clear the whole hunk
        Hunk_Clear();
    }
    else {
        // clear all the client data on the hunk
        Hunk_ClearToMark();
    }

    // initialize the renderer interface
    CL_InitRef();

    // startup all the client stuff
    CL_StartHunkUsers();

    // start the cgame if connected
    if (cls.state > connstate_t::CA_CONNECTED && cls.state != connstate_t::CA_CINEMATIC) {
        cls.cgameStarted = true;
        CL_InitCGame();
        // send pure checksums
        CL_SendPureChecksums();
    }
}

/*
=================
CL_Snd_Restart_f

Restart the sound subsystem: the sounds registered keep their handles, so the cgame and the UI go on
=================
*/
void CL_Snd_Restart_f(void)
{
    SND_Restart();
}

/*
==================
CL_PK3List_f
==================
*/
void CL_OpenedPK3List_f(void)
{
    Q_Println("Opened PK3 Names: {}", FS_LoadedPakNames());
}

/*
==================
CL_PureList_f
==================
*/
void CL_ReferencedPK3List_f(void)
{
    Q_Println("Referenced PK3 Names: {}", FS_ReferencedPakNames());
}

/*
==================
CL_Configstrings_f
==================
*/
void CL_Configstrings_f(void)
{
    int i;
    int ofs;

    if (cls.state != connstate_t::CA_ACTIVE) {
        Q_Println("Not connected to a server.");
        return;
    }

    for (i = 0; i < MAX_CONFIGSTRINGS; i++) {
        ofs = cl.gameState.stringOffsets[i];
        if (!ofs) {
            continue;
        }
        Q_Println("{:4}: {}", i, cl.gameState.stringData + ofs);
    }
}

/*
==============
CL_Clientinfo_f
==============
*/
void CL_Clientinfo_f(void)
{
    Q_Println("--------- Client Information ---------");
    Q_Println("state: {}", cls.state);
    Q_Println("Server: {}", cls.servername);
    Q_Println("User info settings:");
    Info_Print(Cvar_InfoString(cvarFlags_t::CVAR_USERINFO));
    Q_Println("--------------------------------------");
}

//====================================================================

/*
=================
CL_DownloadsComplete

Called when all downloading has been completed
=================
*/
void CL_DownloadsComplete(void)
{
    clc.downloadMenuActive = false;

    // if we downloaded files we need to restart the file system
    if (clc.downloadRestart) {
        clc.downloadRestart = false;

        FS_Restart(clc.checksumFeed); // We possibly downloaded a pak, restart the file system to load it

        // inform the server so we get new gamestate info
        CL_AddReliableCommand("donedl", false);
        clc.downloadFinished = true;

        // by sending the donedl command we request a new gamestate
        // so we don't want to load stuff yet
        return;
    }

    // let the client game init and load data
    cls.state = connstate_t::CA_LOADING;

    // Pump the loop, this may change gamestate!
    Com_EventLoop();

    // if the gamestate was changed by calling Com_EventLoop
    // then we loaded everything already and we don't want to do it again.
    if (cls.state != connstate_t::CA_LOADING) {
        return;
    }

    // starting to load a map so we get out of full screen ui mode
    Cvar_Set("r_uiFullScreen", "0");

    // flush client memory and start loading stuff
    // this will also (re)load the UI
    // if this is a local client then only the client part of the hunk
    // will be cleared, note that this is done after the hunk mark has been set
    CL_FlushMemory();

    // initialize the CGame
    cls.cgameStarted = true;
    CL_InitCGame();

    // set pure checksums
    CL_SendPureChecksums();

    CL_WritePacket();
    CL_WritePacket();
    CL_WritePacket();
}

/*
=================
CL_BeginDownload

Requests a file to download from the server.  Stores it in the current
game directory.
=================
*/

void CL_BeginDownloadConfirm(void)
{
    clc.downloadWaitingOnUser = false;

    if (!cl_downloadOverlay->integer) {
        clc.downloadMenuActive = false;
    }

    Q_DPrintln("***** CL_BeginDownload *****\nLocalname: {}\nRemotename: {}\n****************************", clc.downloadName, cl_downloadName->string);

    clc.downloadBlock = 0; // Starting new file
    clc.downloadCount = 0;
    clc.downloadTime = cls.realtime;

    // Set current time to make sure the module knows the real start time after the delay
    Cvar_SetValue("cl_downloadTime", (float)cls.realtime);

    CL_AddReliableCommand(Q_Format("download {}", cl_downloadName->string).c_str(), false);
}

void CL_BeginDownload(const char* localName, const char* remoteName)
{
    Q_strncpyz(clc.downloadName, localName, sizeof(clc.downloadName));
    Q_FormatTo(clc.downloadTempName, "{}.tmp", localName);

    // Set so UI gets access to it
    Cvar_Set("cl_downloadName", remoteName);
    Cvar_Set("cl_downloadSize", "0");
    Cvar_Set("cl_downloadCount", "0");
    Cvar_SetValue("cl_downloadTime", (float)cls.realtime);

    // Prompt the user (unless they disabled it)
    if (cl_downloadPrompt->integer) {
        clc.downloadMenuActive = true;
        clc.downloadWaitingOnUser = true;
    }
    else {
        CL_BeginDownloadConfirm();
    }
}

/*
=================
CL_NextDownload

A download completed or failed
=================
*/
void CL_NextDownload(void)
{
    char* s;
    char *remoteName, *localName;

    clc.downloadWaitingOnUser = false;

    // A download has finished, check whether this matches a referenced checksum
    if (*clc.downloadName && clc.downloadSize) {
        char* zippath = FS_BuildOSPath(Cvar_VariableString("fs_homepath"), clc.downloadName, "");
        zippath[strlen(zippath) - 1] = '\0';

        if (!FS_CompareZipChecksum(zippath))
            Q_Error(errorParm_t::ERR_DROP, "Incorrect checksum for file: {}", clc.downloadName);
    }

    *clc.downloadTempName = *clc.downloadName = 0;
    Cvar_Set("cl_downloadName", "");

    // We are looking to start a download here
    if (*clc.downloadList) {
        s = clc.downloadList;

        // format is:
        //  @remotename@localname@remotename@localname, etc.

        if (*s == '@')
            s++;
        remoteName = s;

        if ((s = strchr(s, '@')) == nullptr) {
            CL_DownloadsComplete();
            return;
        }

        *s++ = 0;
        localName = s;
        if ((s = strchr(s, '@')) != nullptr)
            *s++ = 0;
        else
            s = localName + strlen(localName); // point at the nul byte

        if (!cl_allowDownload->integer) {
            Q_Error(errorParm_t::ERR_DROP, "UDP Downloads are disabled on your client. (cl_allowDownload is {})", cl_allowDownload->integer);
            return;
        }
        else {
            CL_BeginDownload(localName, remoteName);
        }

        clc.downloadRestart = true;

        // move over the rest
        memmove(clc.downloadList, s, strlen(s) + 1);

        return;
    }

    CL_DownloadsComplete();
}

/*
=================
CL_InitDownloads

After receiving a valid game state, we valid the cgame and local zip files here
and determine if we need to download them
=================
*/
void CL_InitDownloads(void)
{
    char missingfiles[1024];

    if (clc.downloadFinished) {
        // If we just finished a download with a "donedl" we are getting another gamestate and we would be asked to
        // download skipped files again. To avoid this we just skip this one...
        clc.downloadFinished = false;
        CL_DownloadsComplete();
        return;
    }

    if (cl_downloadOverlay->integer) {
        clc.downloadMenuActive = true;
    }

    if (!cl_allowDownload->integer) {
        // autodownload is disabled on the client
        // but it's possible that some referenced files on the server are missing
        if (FS_ComparePaks(missingfiles, sizeof(missingfiles), false)) {
            // NOTE TTimo I would rather have that printed as a modal message box
            //   but at this point while joining the game we don't know wether we will successfully join or not
            Q_Println("\nWARNING: You are missing some files referenced by the server:\n{}You might not be able to join the game\nGo to the setting menu to "
                      "turn on autodownload, or get the file elsewhere\n",
                      missingfiles);
        }
    }
    else if (FS_ComparePaks(clc.downloadList, sizeof(clc.downloadList), true)) {
        const char* serverInfo = cl.gameState.stringData + cl.gameState.stringOffsets[CS_SERVERINFO];
        const char* serverAllowDownloads = Info_ValueForKey(serverInfo, "sv_allowDownload");

        Q_Println("Need paks: {}", clc.downloadList);

        if (serverAllowDownloads[0] && !atoi(serverAllowDownloads)) {
            // The server has an "sv_allowDownload" value set, but it's 0
            Q_Println("Skipping downloads, because the server does not allow downloads");
        }
        else if (*clc.downloadList) {
            // if autodownloading is not enabled on the server
            cls.state = connstate_t::CA_CONNECTED;

            *clc.downloadTempName = *clc.downloadName = 0;
            Cvar_Set("cl_downloadName", "");

            CL_NextDownload();
            return;
        }
    }
    CL_DownloadsComplete();
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CL_CheckForResend(void)
{
    int port;
    char info[MAX_INFO_STRING];
    char data[MAX_INFO_STRING + 10];

    // don't send anything if playing back a demo
    if (clc.demoplaying) {
        return;
    }

    // resend if we haven't gotten a reply yet
    if (cls.state != connstate_t::CA_CONNECTING && cls.state != connstate_t::CA_CHALLENGING) {
        return;
    }

    if (cls.realtime - clc.connectTime < RETRANSMIT_TIMEOUT) {
        return;
    }

    clc.connectTime = cls.realtime; // for retransmit requests
    clc.connectPacketCount++;

    switch (cls.state) {
    case connstate_t::CA_CONNECTING:
        // requesting a challenge

        // The challenge request shall be followed by a client challenge so no malicious server can hijack this connection.
        Q_FormatTo(data, "getchallenge {}", clc.challenge);

        NET_OutOfBandPrint(netsrc_t::NS_CLIENT, &clc.serverAddress, "{}", data);
        break;

    case connstate_t::CA_CHALLENGING:
        // sending back the challenge
        port = (int)Cvar_VariableValue("net_qport");

        Q_strncpyz(info, Cvar_InfoString(cvarFlags_t::CVAR_USERINFO), sizeof(info));
        Info_SetValueForKey(info, "protocol", Q_Format("{}", PROTOCOL_VERSION).c_str());
        Info_SetValueForKey(info, "qport", Q_Format("{}", port).c_str());
        Info_SetValueForKey(info, "challenge", Q_Format("{}", clc.challenge).c_str());

        Q_FormatTo(data, "connect \"{}\"", info);
        NET_OutOfBandData(netsrc_t::NS_CLIENT, &clc.serverAddress, (byte*)data, strlen(data));

        // the most current userinfo has been sent, so watch for any
        // newer changes to userinfo variables
        cvar_modifiedFlags &= ~cvarFlags_t::CVAR_USERINFO;
        break;

    default:
        Q_Error(errorParm_t::ERR_FATAL, "CL_CheckForResend: bad cls.state");
    }
}

/*
===================
CL_DisconnectPacket

Sometimes the server can drop the client and the netchan based
disconnect can be lost.  If the client continues to send packets
to the server, the server will send out of band disconnect packets
to the client so it doesn't have to wait for the full timeout period.
===================
*/
void CL_DisconnectPacket(const netadr_t* from)
{
    if (cls.state < connstate_t::CA_AUTHORIZING) {
        return;
    }

    // if not from our server, ignore it
    if (!NET_CompareAdr(from, &clc.netchan.remoteAddress)) {
        return;
    }

    // if we have received packets within three seconds, ignore it
    // (it might be a malicious spoof)
    if (cls.realtime - clc.lastPacketTime < 3000) {
        return;
    }

    // drop the connection (FIXME: connection dropped dialog)
    Q_Println("Server disconnected for unknown reason");

    CL_Disconnect(true);
}

/*
===================
CL_MotdPacket

===================
*/
void CL_MotdPacket(const netadr_t* from)
{
    const char* challenge;
    const char* info;

    // if not from our server, ignore it
    if (!NET_CompareAdr(from, &cls.updateServer)) {
        return;
    }

    info = Cmd_Argv(1);

    // check challenge
    challenge = Info_ValueForKey(info, "challenge");
    if (strcmp(challenge, cls.updateChallenge)) {
        return;
    }

    challenge = Info_ValueForKey(info, "motd");

    Q_strncpyz(cls.updateInfoString, info, sizeof(cls.updateInfoString));
    Cvar_Set("cl_motdString", challenge);
}

/*
===================
CL_InitServerInfo
===================
*/
void CL_InitServerInfo(serverInfo_t* server, const netadr_t* address)
{
    server->adr = *address;
    server->clients = 0;
    server->hostName[0] = '\0';
    server->mapName[0] = '\0';
    server->maxClients = 0;
    server->maxPing = 0;
    server->minPing = 0;
    server->netType = 0;
    server->needPassword = false;
    server->trueJedi = 0;
    server->weaponDisable = 0;
    server->forceDisable = 0;
    server->ping = 0;
    server->state = lanServerState_t::LANSERVER_NOT_ASKED;
    server->game[0] = '\0';
    server->gameType = 0;
    server->humans = server->bots = 0;
}

constexpr int MAX_SERVERSPERPACKET = 256;

/*
===================
CL_ServersResponsePacket
===================
*/
void CL_ServersResponsePacket(const netadr_t* from, msg_t* msg)
{
    int i, j, count, total;
    netadr_t addresses[MAX_SERVERSPERPACKET];
    int numservers;
    byte* buffptr;
    byte* buffend;

    Q_Println("CL_ServersResponsePacket from {}", NET_AdrToString(from));

    if (cls.numglobalservers == -1) {
        // state to detect lack of servers or lack of response
        cls.numglobalservers = 0;
        cls.numGlobalServerAddresses = 0;
    }

    // parse through server response string
    numservers = 0;
    buffptr = msg->data;
    buffend = buffptr + msg->cursize;

    // advance to initial token
    do {
        if (*buffptr == '\\')
            break;

        buffptr++;
    } while (buffptr < buffend);

    while (buffptr + 1 < buffend) {
        // IPv4 address
        if (*buffptr == '\\') {
            buffptr++;

            if (buffend - buffptr < (int)(sizeof(addresses[numservers].ip) + sizeof(addresses[numservers].port) + 1))
                break;

            for (size_t i = 0; i < sizeof(addresses[numservers].ip); i++)
                addresses[numservers].ip[i] = *buffptr++;

            addresses[numservers].type = netadrtype_t::NA_IP;
        }
        else
            // syntax error!
            break;

        // parse out port
        addresses[numservers].port = (*buffptr++) << 8;
        addresses[numservers].port += *buffptr++;
        addresses[numservers].port = BigShort(addresses[numservers].port);

        // syntax check
        if (*buffptr != '\\')
            break;

        numservers++;
        if (numservers >= MAX_SERVERSPERPACKET)
            break;
    }

    count = cls.numglobalservers;

    for (i = 0; i < numservers && count < MAX_GLOBAL_SERVERS; i++) {
        // build net address
        serverInfo_t* server = &cls.globalServers[count];

        // Tequila: It's possible to have sent many master server requests. Then
        // we may receive many times the same addresses from the master server.
        // We just avoid to add a server if it is still in the global servers list.
        for (j = 0; j < count; j++) {
            if (NET_CompareAdr(&cls.globalServers[j].adr, &addresses[i]))
                break;
        }

        if (j < count)
            continue;

        CL_InitServerInfo(server, &addresses[i]);
        // advance to next slot
        count++;
    }

    // if getting the global list
    if (count >= MAX_GLOBAL_SERVERS && cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS) {
        // if we couldn't store the servers in the main list anymore
        for (; i < numservers && cls.numGlobalServerAddresses < MAX_GLOBAL_SERVERS; i++) {
            // just store the addresses in an additional list
            cls.globalServerAddresses[cls.numGlobalServerAddresses++] = addresses[i];
        }
    }

    cls.numglobalservers = count;
    total = count + cls.numGlobalServerAddresses;

    Q_Println("{} servers parsed (total {})", numservers, total);
}

#ifndef MAX_STRINGED_SV_STRING
#define MAX_STRINGED_SV_STRING 1024
#endif
static void CL_CheckSVStringEdRef(char* buf, const char* str)
{ // I don't really like doing this. But it utilizes the system that was already in place.
    int i = 0;
    int b = 0;
    int strLen = 0;
    bool gotStrip = false;

    if (!str || !str[0]) {
        if (str) {
            strcpy(buf, str);
        }
        return;
    }

    strcpy(buf, str);

    strLen = strlen(str);

    if (strLen >= MAX_STRINGED_SV_STRING) {
        return;
    }

    while (i < strLen && str[i]) {
        gotStrip = false;

        if (str[i] == '@' && (i + 1) < strLen) {
            if (str[i + 1] == '@' && (i + 2) < strLen) {
                if (str[i + 2] == '@' && (i + 3) < strLen) { //@@@ should mean to insert a stringed reference here, so insert it into buf at the current place
                    char stripRef[MAX_STRINGED_SV_STRING];
                    int r = 0;

                    while (i < strLen && str[i] == '@') {
                        i++;
                    }

                    while (i < strLen && str[i] && str[i] != ' ' && str[i] != ':' && str[i] != '.' && str[i] != '\n') {
                        stripRef[r] = str[i];
                        r++;
                        i++;
                    }
                    stripRef[r] = 0;

                    buf[b] = 0;
                    Q_strcat(buf, MAX_STRINGED_SV_STRING, SE_GetString(Q_Format("MP_SVGAME_{}", stripRef).c_str()));
                    b = strlen(buf);
                }
            }
        }

        if (!gotStrip) {
            buf[b] = str[i];
            b++;
        }
        i++;
    }

    buf[b] = 0;
}

/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
void CL_ConnectionlessPacket(const netadr_t* from, msg_t* msg)
{
    char* s;
    const char* c;
    int challenge = 0;

    MSG_BeginReadingOOB(msg);
    MSG_ReadLong(msg); // skip the -1

    s = MSG_ReadStringLine(msg);

    Cmd_TokenizeString(s);

    c = Cmd_Argv(0);

    if (com_developer->integer) {
        Q_Println("CL packet {}: {}", NET_AdrToString(from), c);
    }

    // challenge from the server we are connecting to
    if (!Q_stricmp(c, "challengeResponse")) {
        if (cls.state != connstate_t::CA_CONNECTING) {
            Q_Println("Unwanted challenge response received.  Ignored.");
            return;
        }

        c = Cmd_Argv(2);
        if (*c)
            challenge = atoi(c);

        if (!NET_CompareAdr(from, &clc.serverAddress)) {
            // This challenge response is not coming from the expected address.
            // Check whether we have a matching client challenge to prevent
            // connection hi-jacking.

            if (!*c || challenge != clc.challenge) {
                Q_DPrintln("Challenge response received from unexpected source. Ignored.");
                return;
            }
        }

        // start sending challenge response instead of challenge request packets
        clc.challenge = atoi(Cmd_Argv(1));
        cls.state = connstate_t::CA_CHALLENGING;
        clc.connectPacketCount = 0;
        clc.connectTime = -99999;

        // take this address as the new server address.  This allows
        // a server proxy to hand off connections to multiple servers
        clc.serverAddress = *from;
        Q_DPrintln("challengeResponse: {}", clc.challenge);
        return;
    }

    // server connection
    if (!Q_stricmp(c, "connectResponse")) {
        if (cls.state >= connstate_t::CA_CONNECTED) {
            Q_Println("Dup connect received. Ignored.");
            return;
        }
        if (cls.state != connstate_t::CA_CHALLENGING) {
            Q_Println("connectResponse packet while not connecting. Ignored.");
            return;
        }
        if (!NET_CompareAdr(from, &clc.serverAddress)) {
            Q_Println("connectResponse from wrong address. Ignored.");
            return;
        }
        Netchan_Setup(netsrc_t::NS_CLIENT, &clc.netchan, from, Cvar_VariableValue("net_qport"));
        cls.state = connstate_t::CA_CONNECTED;
        clc.lastPacketSentTime = -9999; // send first packet immediately
        return;
    }

    // server responding to an info broadcast
    if (!Q_stricmp(c, "infoResponse")) {
        CL_ServerInfoPacket(from, msg);
        return;
    }

    // server responding to a get playerlist
    if (!Q_stricmp(c, "statusResponse")) {
        CL_ServerStatusResponse(from, msg);
        return;
    }

    // a disconnect message from the server, which will happen if the server
    // dropped the connection but it is still getting packets from us
    if (!Q_stricmp(c, "disconnect")) {
        CL_DisconnectPacket(from);
        return;
    }

    // echo request from server
    if (!Q_stricmp(c, "echo")) {
        NET_OutOfBandPrint(netsrc_t::NS_CLIENT, from, "{}", Cmd_Argv(1));
        return;
    }

    // cd check
    if (!Q_stricmp(c, "keyAuthorize")) {
        // we don't use these now, so dump them on the floor
        return;
    }

    // global MOTD from id
    if (!Q_stricmp(c, "motd")) {
        CL_MotdPacket(from);
        return;
    }

    // echo request from server
    if (!Q_stricmp(c, "print")) {
        // NOTE: we may have to add exceptions for auth and update servers
        if (NET_CompareAdr(from, &clc.serverAddress) || NET_CompareAdr(from, &rcon_address)) {
            char sTemp[MAX_STRINGED_SV_STRING];

            s = MSG_ReadString(msg);
            CL_CheckSVStringEdRef(sTemp, s);
            Q_strncpyz(clc.serverMessage, sTemp, sizeof(clc.serverMessage));
            Q_Print("{}", sTemp);
        }
        return;
    }

    // list of servers sent back by a master server (classic)
    if (!Q_strncmp(c, "getserversResponse", 18)) {
        CL_ServersResponsePacket(from, msg);
        return;
    }

    Q_DPrintln("Unknown connectionless packet command.");
}

/*
=================
CL_PacketEvent

A packet has arrived from the main event loop
=================
*/
void CL_PacketEvent(const netadr_t* from, msg_t* msg)
{
    clc.lastPacketTime = cls.realtime;

    if (msg->cursize >= 4 && *(int*)msg->data == -1) {
        CL_ConnectionlessPacket(from, msg);
        return;
    }

    if (cls.state < connstate_t::CA_CONNECTED) {
        return; // can't be a valid sequenced packet
    }

    if (msg->cursize < 4) {
        Q_Println("{}: Runt packet", NET_AdrToString(from));
        return;
    }

    //
    // packet from server
    //
    if (!NET_CompareAdr(from, &clc.netchan.remoteAddress)) {
        if (com_developer->integer) {
            Q_Println("{}:sequenced packet without connection", NET_AdrToString(from));
        }
        // FIXME: send a client disconnect?
        return;
    }

    if (!CL_Netchan_Process(&clc.netchan, msg)) {
        return; // out of order, duplicated, etc
    }

    // track the last message received so it can be returned in
    // client messages, allowing the server to detect a dropped
    // gamestate
    clc.serverMessageSequence = LittleLong(*(int*)msg->data);

    clc.lastPacketTime = cls.realtime;
    CL_ParseServerMessage(msg); // a demo being recorded gets the decoded message coded again (cl_netcodec.cpp)
}

/*
==================
CL_CheckTimeout

==================
*/
void CL_CheckTimeout(void)
{
    //
    // check timeout
    //
    if ((!CL_CheckPaused() || !sv_paused->integer) && cls.state >= connstate_t::CA_CONNECTED && cls.state != connstate_t::CA_CINEMATIC &&
        cls.realtime - clc.lastPacketTime > 1000 * (cls.state == connstate_t::CA_ACTIVE ? TIMEOUT_ACTIVE : TIMEOUT_CONNECTING)) {
        if (++cl.timeoutcount > 5) { // timeoutcount saves debugger
            const char* psTimedOut = SE_GetString("MP_SVGAME_SERVER_CONNECTION_TIMED_OUT");
            Q_Println("\n{}", psTimedOut);
            Q_Error(errorParm_t::ERR_DROP, "{}", psTimedOut);
            // CL_Disconnect( true );
            return;
        }
    }
    else {
        cl.timeoutcount = 0;
    }
}

/*
==================
CL_CheckPaused
Check whether client has been paused.
==================
*/
bool CL_CheckPaused(void)
{
    // if cl_paused->modified is set, the cvar has only been changed in
    // this frame. Keep paused in this frame to ensure the server doesn't
    // lag behind.
    if (cl_paused->integer || cl_paused->modified)
        return true;

    return false;
}

//============================================================================

/*
==================
CL_CheckUserinfo

==================
*/
void CL_CheckUserinfo(void)
{
    // don't add reliable commands when not yet connected
    if (cls.state < connstate_t::CA_CONNECTED) {
        return;
    }
    // don't overflow the reliable command buffer when paused
    if (CL_CheckPaused()) {
        return;
    }
    // send a reliable userinfo update if needed
    if (Any(cvar_modifiedFlags & cvarFlags_t::CVAR_USERINFO)) {
        cvar_modifiedFlags &= ~cvarFlags_t::CVAR_USERINFO;
        CL_AddReliableCommand(Q_Format("userinfo \"{}\"", Cvar_InfoString(cvarFlags_t::CVAR_USERINFO)).c_str(), false);
    }
}

// the client's sound frame, run before the screen's swap (SCR_UpdateScreen)
static void CL_UpdateSound(void)
{
    SND_Frame(cls.realtime);
}

/*
==================
CL_Frame

==================
*/
static unsigned int frameCount;
static float avgFrametime = 0.0;
extern void SE_CheckForLanguageUpdates(void);
void CL_Frame(int msec)
{
    if (!com_cl_running->integer) {
        return;
    }

    SE_CheckForLanguageUpdates(); // will take zero time to execute unless language changes, then will reload strings.
                                  //	of course this still doesn't work for menus...

    if (cls.state == connstate_t::CA_DISCONNECTED && !Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_UI) && !com_sv_running->integer && cls.uiStarted) {
        // if disconnected, bring up the menu
        SND_StopAll();
        UIVM_SetActiveMenu(uiMenuCommand_t::UIMENU_MAIN);
    }

    // save the msec before checking pause
    cls.realFrametime = msec;

    // decide the simulation time
    cls.frametime = msec;
    if (cl_framerate->integer) {
        avgFrametime += msec;
        char mess[256];
        if (!(frameCount & 0x1f)) {
            Q_FormatTo(mess, "Frame rate={:f}\n\n", 1000.0f * (1.0 / (avgFrametime / 32.0f)));
            //		Q_DPrintln("{}", mess);
            Q_Print("{}", mess);
            avgFrametime = 0.0f;
        }
        frameCount++;
    }

    cls.realtime += cls.frametime;

    if (cl_timegraph->integer) {
        SCR_DebugGraph(cls.realFrametime * 0.25, 0);
    }

    // see if we need to update any userinfo
    CL_CheckUserinfo();

    // if we haven't gotten a packet in a long time,
    // drop the connection
    CL_CheckTimeout();

    // resend a connection request if necessary
    CL_CheckForResend();

    // decide on the serverTime to render
    CL_SetCGameTime();

    // send intentions now, once the time to render is decided: the command of the tick in progress then exists when cgame
    // predicts the local player and draws it between the last two ticks
    CL_SendCmd();

    // update the screen, and the audio before the screen's swap
    SCR_UpdateScreen(CL_UpdateSound);

    // advance local effects for next frame
    SCR_RunCinematic();

    Con_RunConsole();

    // reset the heap for Ghoul2 vert transform space gameside
    if (G2VertSpaceServer) {
        G2VertSpaceServer->ResetHeap();
    }

    cls.framecount++;
}

//============================================================================

/*
============
CL_ShutdownRef
============
*/
static void CL_ShutdownRef(bool restarting)
{
    if (re) {
        if (re->Shutdown) {
            re->Shutdown(true, restarting);
        }
    }

    re = nullptr;

    if (rendererLib != nullptr) {
        Sys_UnloadDll(rendererLib);
        rendererLib = nullptr;
    }
}

/*
============
CL_SetPointerImage

The menus' pointer is the system's own, with the game's image at the size the menus drew it: 40 of their 640 by 480,
whatever the window's size
============
*/
static void CL_SetPointerImage(void)
{
    constexpr int POINTER_SIZE = 40;
    byte* pic = nullptr;
    int width = 0;
    int height = 0;

    // the image of the shader "cursor", which the menus name as their pointer
    re->ext.LoadImagePixels("gfx/menus/cursor1", &pic, &width, &height);
    if (!pic) {
        Q_PrintlnWarning("the menus' pointer image gfx/menus/cursor1 was not found");
        return;
    }

    if (!Platform_SetPointerImage({pic, static_cast<size_t>(width) * height * 4}, width, height, POINTER_SIZE * cls.glconfig.vidWidth / SCREEN_WIDTH,
                                  POINTER_SIZE * cls.glconfig.vidHeight / SCREEN_HEIGHT)) {
        Q_PrintlnWarning("the menus' pointer image could not be given to the system");
    }
    Z_Free(pic);
}

/*
============
CL_DisplayChanged
============
*/
void CL_DisplayChanged(void)
{
    if (!cls.rendererStarted) {
        return;
    }

    const int width = cls.glconfig.vidWidth;
    const int height = cls.glconfig.vidHeight;
    re->DisplayChanged(&cls.glconfig);

    // the pointer's image is shown at a size in the window's pixels
    if (cls.glconfig.vidWidth != width || cls.glconfig.vidHeight != height) {
        CL_SetPointerImage();
    }

    // also at the same size in another state (a window as large as its screen made borderless fullscreen): the video
    // menu's display rows show the state
    if (cls.uiStarted) {
        UIVM_DisplayChanged();
    }

    if (cls.cgameStarted) {
        CGVM_DisplayChanged();
    }
}

/*
============
CL_InitRenderer
============
*/
void CL_InitRenderer(void)
{
    // this sets up the renderer and calls R_Init
    re->BeginRegistration(&cls.glconfig);

    // load character sets
    cls.charSetShader = re->RegisterShaderNoMip("gfx/2d/charsgrid_med");
    cls.consoleFont = re->RegisterFont("ocr_a");

    cls.whiteShader = re->RegisterShader("white");
    cls.consoleShader = re->RegisterShader("console");
    g_console_field_width = cls.glconfig.vidWidth / SMALLCHAR_WIDTH - 2;
    g_consoleField.widthInChars = g_console_field_width;

    cls.menuFont = re->RegisterFont("ocr_a");
    CL_SetPointerImage();
}

/*
============================
CL_StartHunkUsers

After the server has cleared the hunk, these will need to be restarted
This is the only place that any of these functions are called from
============================
*/
void CL_StartHunkUsers(void)
{
    if (!com_cl_running) {
        return;
    }

    if (!com_cl_running->integer) {
        return;
    }

    if (!cls.rendererStarted) {
        cls.rendererStarted = true;
        CL_InitRenderer();
    }

    if (!cls.soundStarted) {
        cls.soundStarted = true;
        SND_Init();
    }

    if (!cls.soundRegistered) {
        cls.soundRegistered = true;
        SND_BeginRegistration();
    }

    if (!cls.uiStarted) {
        cls.uiStarted = true;
        CL_InitUI();
    }
}

/*
============
CL_InitRef
============
*/
bool Com_TheHunkMarkHasBeenMade(void);

// qcommon/cm_load.cpp
extern void* gpvCachedMapDiskImage;
extern bool gbUsingCachedMapDataRightNow;

static char* GetSharedMemory(void)
{
    return cl.mSharedMemory;
}
static vm_t* GetCurrentVM(void)
{
    return currentVM;
}
static bool CGVMLoaded(void)
{
    return cls.cgameStarted;
}
static void* CM_GetCachedMapDiskImage(void)
{
    return gpvCachedMapDiskImage;
}
static void CM_SetCachedMapDiskImage(void* ptr)
{
    gpvCachedMapDiskImage = ptr;
}
static void CM_SetUsingCache(bool usingCache)
{
    gbUsingCachedMapDataRightNow = usingCache;
}

constexpr int G2_VERT_SPACE_SERVER_SIZE = 256;
IHeapAllocator* G2VertSpaceServer = nullptr;
CMiniHeap IHeapAllocator_singleton(G2_VERT_SPACE_SERVER_SIZE * 1024);

static IHeapAllocator* GetG2VertSpaceServer(void)
{
    return G2VertSpaceServer;
}

#define DEFAULT_RENDER_LIBRARY "rd-vanilla"

void CL_InitRef(void)
{
    static refimport_t ri;
    refexport_t* ret;
    GetRefAPI_t GetRefAPI;
    char dllName[MAX_OSPATH];

    // the client's window, opened once and kept: the renderer draws in it and a restart of the renderer leaves it
    Platform_OpenWindow();

    Q_Println("----- Initializing Renderer ----");

    cl_renderer = Cvar_Get("cl_renderer", DEFAULT_RENDER_LIBRARY, cvarFlags_t::CVAR_ARCHIVE | cvarFlags_t::CVAR_LATCH, "Which renderer library to use");

    Q_FormatTo(dllName, "{}_" ARCH_STRING DLL_EXT, cl_renderer->string);

    if (!(rendererLib = Sys_LoadDll(dllName, false)) && strcmp(cl_renderer->string, cl_renderer->resetString)) {
        Q_Println("failed: trying to load fallback renderer");
        Cvar_ForceReset("cl_renderer");

        Q_FormatTo(dllName, DEFAULT_RENDER_LIBRARY "_" ARCH_STRING DLL_EXT);
        rendererLib = Sys_LoadDll(dllName, false);
    }

    if (!rendererLib) {
        Q_Error(errorParm_t::ERR_FATAL, "Failed to load renderer");
    }

    memset(&ri, 0, sizeof(ri));

    GetRefAPI = (GetRefAPI_t)Sys_LoadFunction(rendererLib, "GetRefAPI");
    if (!GetRefAPI)
        Q_Error(errorParm_t::ERR_FATAL, "Can't load symbol GetRefAPI: '{}'", Sys_LibraryError());

    // set up the import table
    ri.Print = Q_VPrint;
    ri.Error = Q_VError;
    ri.Milliseconds = Sys_Milliseconds2; // FIXME: unix+mac need this
    ri.Hunk_AllocateTempMemory = Hunk_AllocateTempMemory;
    ri.Hunk_FreeTempMemory = Hunk_FreeTempMemory;
    ri.Hunk_Alloc = Hunk_Alloc;
    ri.Hunk_MemoryRemaining = Hunk_MemoryRemaining;
    ri.Z_Malloc = Z_Malloc;
    ri.Z_Free = Z_Free;
    ri.Z_MemSize = Z_MemSize;
    ri.Z_MorphMallocTag = Z_MorphMallocTag;
    ri.Cmd_ExecuteString = Cmd_ExecuteString;
    ri.Cmd_Argc = Cmd_Argc;
    ri.Cmd_Argv = Cmd_Argv;
    ri.Cmd_ArgsBuffer = Cmd_ArgsBuffer;
    ri.Cmd_AddCommand = Cmd_AddCommand;
    ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
    ri.Cvar_Set = Cvar_Set;
    ri.Cvar_Get = Cvar_Get;
    ri.Cvar_SetValue = Cvar_SetValue;
    ri.Cvar_CheckRange = Cvar_CheckRange;
    ri.Cvar_VariableStringBuffer = Cvar_VariableStringBuffer;
    ri.Cvar_VariableString = Cvar_VariableString;
    ri.Cvar_VariableValue = Cvar_VariableValue;
    ri.Cvar_VariableIntegerValue = Cvar_VariableIntegerValue;
    ri.Sys_LowPhysicalMemory = Sys_LowPhysicalMemory;
    ri.SE_GetString = SE_GetString;
    ri.FS_FreeFile = FS_FreeFile;
    ri.FS_FreeFileList = FS_FreeFileList;
    ri.FS_Read = FS_Read;
    ri.FS_ReadFile = FS_ReadFile;
    ri.FS_FCloseFile = FS_FCloseFile;
    ri.FS_FOpenFileRead = FS_FOpenFileRead;
    ri.FS_FOpenFileWrite = FS_FOpenFileWrite;
    ri.FS_FOpenFileByMode = FS_FOpenFileByMode;
    ri.FS_FileExists = FS_FileExists;
    ri.FS_FileIsInPAK = FS_FileIsInPAK;
    ri.FS_ListFiles = FS_ListFiles;
    ri.FS_Write = FS_Write;
    ri.FS_WriteFile = FS_WriteFile;
    ri.CM_BoxTrace = CM_BoxTrace;
    ri.CM_DrawDebugSurface = CM_DrawDebugSurface;
    ri.CM_CullWorldBox = CM_CullWorldBox;
    ri.CM_ClusterPVS = CM_ClusterPVS;
    ri.CM_LeafArea = CM_LeafArea;
    ri.CM_LeafCluster = CM_LeafCluster;
    ri.CM_PointLeafnum = CM_PointLeafnum;
    ri.CM_PointContents = CM_PointContents;
    ri.Com_TheHunkMarkHasBeenMade = Com_TheHunkMarkHasBeenMade;
    ri.CIN_RunCinematic = CIN_RunCinematic;
    ri.CIN_PlayCinematic = CIN_PlayCinematic;
    ri.CIN_UploadCinematic = CIN_UploadCinematic;

    // g2 data access
    ri.GetSharedMemory = GetSharedMemory;

    // (c)g vm callbacks
    ri.GetCurrentVM = GetCurrentVM;
    ri.CGVMLoaded = CGVMLoaded;
    ri.CGVM_RagCallback = CGVM_RagCallback;

    ri.Platform_Display = Platform_Display;
    ri.Platform_Present = Platform_Present;
    ri.GL_GetProcAddress = Platform_GlProcAddress;
    ri.GL_ExtensionSupported = Platform_GlExtensionSupported;

    ri.CM_GetCachedMapDiskImage = CM_GetCachedMapDiskImage;
    ri.CM_SetCachedMapDiskImage = CM_SetCachedMapDiskImage;
    ri.CM_SetUsingCache = CM_SetUsingCache;

    // FIXME: Might have to do something about this...
    ri.GetG2VertSpaceServer = GetG2VertSpaceServer;
    G2VertSpaceServer = &IHeapAllocator_singleton;

    ri.PD_Store = PD_Store;
    ri.PD_Load = PD_Load;

    ret = GetRefAPI(REF_API_VERSION, &ri);

    //	Q_Println("-------------------------------");

    if (!ret) {
        Q_Error(errorParm_t::ERR_FATAL, "Couldn't initialize refresh");
    }

    re = ret;

    // unpause so the cgame definately gets a snapshot and renders a frame
    Cvar_Set("cl_paused", "0");
}

//===========================================================================================

[[maybe_unused]] constexpr int MODEL_CHANGE_DELAY = 5000;
int gCLModelDelay = 0;

void CL_SetModel_f(void)
{
    const char* arg;
    char name[256];

    arg = Cmd_Argv(1);
    if (arg[0]) {
        /*
        //If you wanted to be foolproof you would put this on the server I guess. But that
        //tends to put things out of sync regarding cvar status. And I sort of doubt someone
        //is going to write a client and figure out the protocol so that they can annoy people
        //by changing models real fast.
        int curTime = Com_Milliseconds();
        if (gCLModelDelay > curTime)
        {
            Q_Println("You can only change your model every {} seconds.", (MODEL_CHANGE_DELAY/1000));
            return;
        }

        gCLModelDelay = curTime + MODEL_CHANGE_DELAY;
        */
        // rwwFIXMEFIXME: This is currently broken and doesn't seem to work for connecting clients
        Cvar_Set("model", arg);
    }
    else {
        Cvar_VariableStringBuffer("model", name, sizeof(name));
        Q_Println("model is set to {}", name);
    }
}

void CL_SetForcePowers_f(void)
{
    return;
}

static void CL_AddFavorite_f(void)
{
    const bool connected = (cls.state == connstate_t::CA_ACTIVE) && !clc.demoplaying;
    const int argc = Cmd_Argc();
    if (!connected && argc != 2) {
        Q_Println("syntax: addFavorite <ip or hostname>");
        return;
    }

    const char* server = (argc == 2) ? Cmd_Argv(1) : NET_AdrToString(&clc.serverAddress);
    const int status = LAN_AddFavAddr(server);
    switch (status) {
    case -1:
        Q_Println("error adding favorite server: too many favorite servers");
        break;
    case 0:
        Q_Println("error adding favorite server: server already exists");
        break;
    case 1:
        Q_Println("successfully added favorite server \"{}\"", server);
        break;
    default:
        Q_Println("unknown error ({}) adding favorite server", status);
        break;
    }
}

constexpr int G2_VERT_SPACE_CLIENT_SIZE = 256;

/*
===============
CL_GenerateQKey

test to see if a valid QKEY_FILE exists.  If one does not, try to generate
it by filling it with 2048 bytes of random data.
===============
*/

static void CL_GenerateQKey(void)
{
    if (cl_enableGuid->integer) {
        int len = 0;
        unsigned char buff[QKEY_SIZE];
        fileHandle_t f;

        len = FS_SV_FOpenFileRead(QKEY_FILE, &f);
        FS_FCloseFile(f);
        if (len == QKEY_SIZE) {
            Q_Println("QKEY found.");
            return;
        }
        else {
            if (len > 0) {
                Q_Println("QKEY file size != {}, regenerating", QKEY_SIZE);
            }

            Q_Println("QKEY building random string");
            Com_RandomBytes(buff, sizeof(buff));

            f = FS_SV_FOpenFileWrite(QKEY_FILE);
            if (!f) {
                Q_Println("QKEY could not open {} for write", QKEY_FILE);
                return;
            }
            FS_Write(buff, sizeof(buff), f);
            FS_FCloseFile(f);
            Q_Println("QKEY generated");
        }
    }
}

/*
====================
CL_Init
====================
*/
void CL_Init(void)
{
    //	Q_Println("----- Client Initialization -----");

    Con_Init();

    CL_ClearState();

    cls.state = connstate_t::CA_DISCONNECTED; // no longer CA_UNINITIALIZED

    cls.realtime = 0;

    CL_InitInput();

    //
    // register our variables
    //
    cl_noprint = Cvar_Get("cl_noprint", "0", cvarFlags_t::CVAR_NONE);
    cl_motd = Cvar_Get("cl_motd", "1", cvarFlags_t::CVAR_ARCHIVE_ND, "Display welcome message from master server on the bottom of connection screen");
    cl_motdServer[0] = Cvar_Get("cl_motdServer1", UPDATE_SERVER_NAME, cvarFlags_t::CVAR_NONE);
    cl_motdServer[1] = Cvar_Get("cl_motdServer2", JKHUB_UPDATE_SERVER_NAME, cvarFlags_t::CVAR_NONE);
    for (int index = 2; index < MAX_MASTER_SERVERS; index++)
        cl_motdServer[index] = Cvar_Get(Q_Format("cl_motdServer{}", index + 1).c_str(), "", cvarFlags_t::CVAR_ARCHIVE_ND);

    cl_debugInterpForce = Cvar_Get("cl_debugInterpForce", "0", cvarFlags_t::CVAR_CHEAT,
                                   "Debug: 1 shows the world cl_debugInterpDelay ms behind the newest snapshot instead of the computed delay");
    cl_debugInterpDelay = Cvar_Get("cl_debugInterpDelay", "50", cvarFlags_t::CVAR_CHEAT,
                                   "Debug: the interpolation delay forced by cl_debugInterpForce, in ms behind the newest snapshot when it arrives");
    cl_shownet = Cvar_Get("cl_shownet", "0", cvarFlags_t::CVAR_TEMP);
    cl_showSend = Cvar_Get("cl_showSend", "0", cvarFlags_t::CVAR_TEMP);
    cl_showTimeDelta = Cvar_Get("cl_showTimeDelta", "0", cvarFlags_t::CVAR_TEMP);
    cl_freezeDemo = Cvar_Get("cl_freezeDemo", "0", cvarFlags_t::CVAR_TEMP);
    rcon_client_password = Cvar_Get("rconPassword", "", cvarFlags_t::CVAR_TEMP, "Password for remote console access");
    cl_activeAction = Cvar_Get("activeAction", "", cvarFlags_t::CVAR_TEMP);

    cl_timedemo = Cvar_Get("timedemo", "0", cvarFlags_t::CVAR_NONE);

    rconAddress = Cvar_Get("rconAddress", "", cvarFlags_t::CVAR_NONE, "Alternate server address to remotely access via rcon protocol");

    cl_yawspeed = Cvar_Get("cl_yawspeed", "140", cvarFlags_t::CVAR_ARCHIVE_ND);
    cl_pitchspeed = Cvar_Get("cl_pitchspeed", "140", cvarFlags_t::CVAR_ARCHIVE_ND);
    cl_anglespeedkey = Cvar_Get("cl_anglespeedkey", "1.5", cvarFlags_t::CVAR_ARCHIVE_ND);

    cl_run = Cvar_Get("cl_run", "1", cvarFlags_t::CVAR_ARCHIVE_ND, "Always run");
    cl_sensitivity = Cvar_Get("sensitivity", "5", cvarFlags_t::CVAR_ARCHIVE, "Mouse sensitivity value");
    cl_mouseAccel = Cvar_Get("cl_mouseAccel", "0", cvarFlags_t::CVAR_ARCHIVE_ND, "Mouse acceleration value");
    cl_freelook = Cvar_Get("cl_freelook", "1", cvarFlags_t::CVAR_ARCHIVE_ND, "Mouse look");

    // 0: legacy mouse acceleration
    // 1: new implementation
    cl_mouseAccelStyle = Cvar_Get("cl_mouseAccelStyle", "0", cvarFlags_t::CVAR_ARCHIVE_ND, "Mouse accelration style (0:legacy, 1:QuakeLive)");
    // offset for the power function (for style 1, ignored otherwise)
    // this should be set to the max rate value
    cl_mouseAccelOffset = Cvar_Get("cl_mouseAccelOffset", "5", cvarFlags_t::CVAR_ARCHIVE_ND, "Mouse acceleration offset for style 1");

    cl_showMouseRate = Cvar_Get("cl_showmouserate", "0", cvarFlags_t::CVAR_NONE);
    cl_framerate = Cvar_Get("cl_framerate", "0", cvarFlags_t::CVAR_TEMP);
    cl_allowDownload = Cvar_Get("cl_allowDownload", "1", cvarFlags_t::CVAR_ARCHIVE_ND, "Allow downloading custom paks from server");
    cl_allowAltEnter = Cvar_Get("cl_allowAltEnter", "1", cvarFlags_t::CVAR_ARCHIVE_ND, "Enables use of ALT+ENTER keyboard combo to toggle fullscreen");

    cl_autolodscale = Cvar_Get("cl_autolodscale", "1", cvarFlags_t::CVAR_ARCHIVE_ND);

    cl_conXOffset = Cvar_Get("cl_conXOffset", "0", cvarFlags_t::CVAR_NONE);
    cl_inGameVideo = Cvar_Get("r_inGameVideo", "1", cvarFlags_t::CVAR_ARCHIVE_ND);

    cl_serverStatusResendTime = Cvar_Get("cl_serverStatusResendTime", "750", cvarFlags_t::CVAR_NONE);

    // init autoswitch so the ui will have it correctly even
    // if the cgame hasn't been started
    Cvar_Get("cg_autoswitch", "1", cvarFlags_t::CVAR_ARCHIVE);

    m_pitchVeh = Cvar_Get("m_pitchVeh", "0.022", cvarFlags_t::CVAR_ARCHIVE_ND);
    m_pitch = Cvar_Get("m_pitch", "0.022", cvarFlags_t::CVAR_ARCHIVE_ND);
    m_yaw = Cvar_Get("m_yaw", "0.022", cvarFlags_t::CVAR_ARCHIVE_ND);
    m_forward = Cvar_Get("m_forward", "0.25", cvarFlags_t::CVAR_ARCHIVE_ND);
    m_side = Cvar_Get("m_side", "0.25", cvarFlags_t::CVAR_ARCHIVE_ND);
#ifdef MACOS_X
    // Input is jittery on OS X w/o this
    m_filter = Cvar_Get("m_filter", "1", cvarFlags_t::CVAR_ARCHIVE_ND);
#else
    m_filter = Cvar_Get("m_filter", "0", cvarFlags_t::CVAR_ARCHIVE_ND);
#endif

    cl_motdString = Cvar_Get("cl_motdString", "", cvarFlags_t::CVAR_ROM);

    Cvar_Get("cl_maxPing", "800", cvarFlags_t::CVAR_ARCHIVE_ND, "Max. ping for servers when searching the serverlist");

    cl_drawRecording = Cvar_Get("cl_drawRecording", "1", cvarFlags_t::CVAR_ARCHIVE);

    // enable the ja_guid player identifier in userinfo by default in OpenJK
    cl_enableGuid = Cvar_Get("cl_enableGuid", "1", cvarFlags_t::CVAR_ARCHIVE_ND, "Enable GUID userinfo identifier");
    cl_guidServerUniq = Cvar_Get("cl_guidServerUniq", "1", cvarFlags_t::CVAR_ARCHIVE_ND, "Use a unique guid value per server");

    // ~ and `, as keys and characters

    cl_filterGames = Cvar_Get("cl_filterGames", "MBII MBIIOpenBeta", cvarFlags_t::CVAR_ARCHIVE_ND, "List of fs_game to filter (space separated)");

    cl_downloadName = Cvar_Get("cl_downloadName", "", cvarFlags_t::CVAR_INTERNAL);
    cl_downloadPrompt = Cvar_Get("cl_downloadPrompt", "1", cvarFlags_t::CVAR_ARCHIVE, "Confirm pk3 downloads from the server");
    cl_downloadOverlay = Cvar_Get("cl_downloadOverlay", "1", cvarFlags_t::CVAR_ARCHIVE, "Draw download info overlay");

    cl_reconnectArgs = Cvar_Get("cl_reconnectArgs", "", cvarFlags_t::CVAR_ARCHIVE, "Arguments provided when last connecting to a server");

    // userinfo
    Cvar_Get("name", "Padawan", cvarFlags_t::CVAR_USERINFO | cvarFlags_t::CVAR_ARCHIVE_ND, "Player name");
    Cvar_Get("model", DEFAULT_MODEL "/default", cvarFlags_t::CVAR_USERINFO | cvarFlags_t::CVAR_ARCHIVE, "Player model");
    Cvar_Get("forcepowers", "7-1-032330000000001333", cvarFlags_t::CVAR_USERINFO | cvarFlags_t::CVAR_ARCHIVE, "Player forcepowers");
    //	Cvar_Get ("g_redTeam", DEFAULT_REDTEAM_NAME, cvarFlags_t::CVAR_SERVERINFO | cvarFlags_t::CVAR_ARCHIVE);
    //	Cvar_Get ("g_blueTeam", DEFAULT_BLUETEAM_NAME, cvarFlags_t::CVAR_SERVERINFO | cvarFlags_t::CVAR_ARCHIVE);
    Cvar_Get("color1", "4", cvarFlags_t::CVAR_USERINFO | cvarFlags_t::CVAR_ARCHIVE, "Player saber1 color");
    Cvar_Get("color2", "4", cvarFlags_t::CVAR_USERINFO | cvarFlags_t::CVAR_ARCHIVE, "Player saber2 color");
    Cvar_Get("handicap", "100", cvarFlags_t::CVAR_USERINFO | cvarFlags_t::CVAR_ARCHIVE, "Player handicap");
    Cvar_Get("sex", "male", cvarFlags_t::CVAR_USERINFO | cvarFlags_t::CVAR_ARCHIVE, "Player sex");
    Cvar_Get("password", "", cvarFlags_t::CVAR_USERINFO, "Password to join server");
    Cvar_Get("cg_predictItems", "1", cvarFlags_t::CVAR_USERINFO | cvarFlags_t::CVAR_ARCHIVE);

    // default sabers
    Cvar_Get("saber1", DEFAULT_SABER, cvarFlags_t::CVAR_USERINFO | cvarFlags_t::CVAR_ARCHIVE, "Player default right hand saber");
    Cvar_Get("saber2", "none", cvarFlags_t::CVAR_USERINFO | cvarFlags_t::CVAR_ARCHIVE, "Player left hand saber");

    // skin color
    Cvar_Get("char_color_red", "255", cvarFlags_t::CVAR_USERINFO | cvarFlags_t::CVAR_ARCHIVE, "Player tint (Red)");
    Cvar_Get("char_color_green", "255", cvarFlags_t::CVAR_USERINFO | cvarFlags_t::CVAR_ARCHIVE, "Player tint (Green)");
    Cvar_Get("char_color_blue", "255", cvarFlags_t::CVAR_USERINFO | cvarFlags_t::CVAR_ARCHIVE, "Player tint (Blue)");

    // cgame might not be initialized before menu is used
    Cvar_Get("cg_viewsize", "100", cvarFlags_t::CVAR_ARCHIVE_ND);

    //
    // register our commands
    //
    Cmd_AddCommand("cmd", CL_ForwardToServer_f, "Forward command to server");
    Cmd_AddCommand("globalservers", CL_GlobalServers_f, "Query the masterserver for serverlist");
    Cmd_AddCommand("addFavorite", CL_AddFavorite_f, "Add server to favorites");
    Cmd_AddCommand("record", CL_Record_f, "Record a demo");
    Cmd_AddCommand("demo", CL_PlayDemo_f, "Playback a demo");
    Cmd_SetCommandCompletionFunc("demo", CL_CompleteDemoName);
    Cmd_AddCommand("stoprecord", CL_StopRecord_f, "Stop recording a demo");
    Cmd_AddCommand("netstats", CL_NetcodecStats_f, "Message codec statistics since the last call");
    Cmd_AddCommand("configstrings", CL_Configstrings_f, "Prints the configstrings list");
    Cmd_AddCommand("clientinfo", CL_Clientinfo_f, "Prints the userinfo variables");
    Cmd_AddCommand("snd_restart", CL_Snd_Restart_f, "Restart sound");
    Cmd_AddCommand("vid_restart", CL_Vid_Restart_f, "Restart the renderer - or change the resolution");
    Cmd_AddCommand("disconnect", CL_Disconnect_f, "Disconnect from current server");
    Cmd_AddCommand("cinematic", CL_PlayCinematic_f, "Play a cinematic video");
    Cmd_AddCommand("connect", CL_Connect_f, "Connect to a server");
    Cmd_AddCommand("reconnect", CL_Reconnect_f, "Reconnect to current server");
    Cmd_AddCommand("localservers", CL_LocalServers_f, "Query LAN for local servers");
    Cmd_AddCommand("rcon", CL_Rcon_f, "Execute commands remotely to a server");
    Cmd_SetCommandCompletionFunc("rcon", CL_CompleteRcon);
    Cmd_AddCommand("ping", CL_Ping_f, "Ping a server for info response");
    Cmd_AddCommand("serverstatus", CL_ServerStatus_f, "Retrieve current or specified server's status");
    Cmd_AddCommand("showip", CL_ShowIP_f, "Shows local IP");
    Cmd_AddCommand("fs_openedList", CL_OpenedPK3List_f, "Lists open pak files");
    Cmd_AddCommand("fs_referencedList", CL_ReferencedPK3List_f, "Lists referenced pak files");
    Cmd_AddCommand("model", CL_SetModel_f, "Set the player model");
    Cmd_AddCommand("forcepowers", CL_SetForcePowers_f);

    CL_InitRef();

    SCR_Init();

    Cbuf_Execute();

    Cvar_Set("cl_running", "1");

    G2VertSpaceClient = new CMiniHeap(G2_VERT_SPACE_CLIENT_SIZE * 1024);

    CL_GenerateQKey();
    CL_UpdateGUID(nullptr, 0);

    //	Q_Println("----- Client Initialization Complete -----");
}

/*
===============
CL_Shutdown

===============
*/
void CL_Shutdown(void)
{
    static bool recursive = false;

    // Q_Println("----- CL_Shutdown -----");

    if (recursive) {
        Q_PrintlnWarning("Recursive CL_Shutdown called!");
        return;
    }
    recursive = true;

    if (G2VertSpaceClient) {
        delete G2VertSpaceClient;
        G2VertSpaceClient = nullptr;
    }

    CL_Disconnect(true);

    // RJ: added the shutdown all to close down the cgame (to free up some memory, such as in the fx system)
    CL_ShutdownAll(true);

    SND_Shutdown();
    // CL_ShutdownUI();

    Cmd_RemoveCommand("cmd");
    Cmd_RemoveCommand("configstrings");
    Cmd_RemoveCommand("clientinfo");
    Cmd_RemoveCommand("snd_restart");
    Cmd_RemoveCommand("vid_restart");
    Cmd_RemoveCommand("disconnect");
    Cmd_RemoveCommand("record");
    Cmd_RemoveCommand("demo");
    Cmd_RemoveCommand("cinematic");
    Cmd_RemoveCommand("stoprecord");
    Cmd_RemoveCommand("netstats");
    Cmd_RemoveCommand("connect");
    Cmd_RemoveCommand("reconnect");
    Cmd_RemoveCommand("localservers");
    Cmd_RemoveCommand("globalservers");
    Cmd_RemoveCommand("addFavorite");
    Cmd_RemoveCommand("rcon");
    Cmd_RemoveCommand("ping");
    Cmd_RemoveCommand("serverstatus");
    Cmd_RemoveCommand("showip");
    Cmd_RemoveCommand("fs_openedList");
    Cmd_RemoveCommand("fs_referencedList");
    Cmd_RemoveCommand("model");
    Cmd_RemoveCommand("forcepowers");

    CL_ShutdownInput();
    Con_Shutdown();

    Cvar_Set("cl_running", "0");

    recursive = false;

    Com_Memset(&cls, 0, sizeof(cls));
    Key_SetCatcher(keyCatchFlags_t::KEYCATCH_NONE);

    // Q_Println("-----------------------");
}

bool CL_ConnectedToRemoteServer(void)
{
    return (com_sv_running && !com_sv_running->integer && cls.state >= connstate_t::CA_CONNECTED && !clc.demoplaying);
}

static void CL_SetServerInfo(serverInfo_t* server, const char* info, lanServerState_t state, int ping)
{
    if (server) {
        if (info) {
            server->clients = atoi(Info_ValueForKey(info, "clients"));
            Q_strncpyz(server->hostName, Info_ValueForKey(info, "hostname"), MAX_NAME_LENGTH);
            Q_strncpyz(server->mapName, Info_ValueForKey(info, "mapname"), MAX_NAME_LENGTH);
            server->maxClients = atoi(Info_ValueForKey(info, "sv_maxclients"));
            Q_strncpyz(server->game, Info_ValueForKey(info, "game"), MAX_NAME_LENGTH);
            server->gameType = atoi(Info_ValueForKey(info, "gametype"));
            server->netType = atoi(Info_ValueForKey(info, "nettype"));
            server->minPing = atoi(Info_ValueForKey(info, "minping"));
            server->maxPing = atoi(Info_ValueForKey(info, "maxping"));
            //			server->allowAnonymous = atoi(Info_ValueForKey(info, "sv_allowAnonymous"));
            server->needPassword = atoi(Info_ValueForKey(info, "needpass"));
            server->trueJedi = atoi(Info_ValueForKey(info, "truejedi"));
            server->weaponDisable = atoi(Info_ValueForKey(info, "wdisable"));
            server->forceDisable = atoi(Info_ValueForKey(info, "fdisable"));
            server->humans = atoi(Info_ValueForKey(info, "g_humanplayers"));
            server->bots = atoi(Info_ValueForKey(info, "bots"));
            //			server->pure = atoi(Info_ValueForKey(info, "pure" ));
        }
        server->state = state;
        server->ping = ping;
    }
}

static void CL_SetServerInfoByAddress(const netadr_t* from, const char* info, lanServerState_t state, int ping)
{
    int i;

    for (i = 0; i < MAX_OTHER_SERVERS; i++) {
        if (NET_CompareAdr(from, &cls.localServers[i].adr)) {
            CL_SetServerInfo(&cls.localServers[i], info, state, ping);
        }
    }

    for (i = 0; i < MAX_GLOBAL_SERVERS; i++) {
        if (NET_CompareAdr(from, &cls.globalServers[i].adr)) {
            CL_SetServerInfo(&cls.globalServers[i], info, state, ping);
        }
    }

    for (i = 0; i < MAX_OTHER_SERVERS; i++) {
        if (NET_CompareAdr(from, &cls.favoriteServers[i].adr)) {
            CL_SetServerInfo(&cls.favoriteServers[i], info, state, ping);
        }
    }
}

/*
===================
CL_ServerInfoPacket
===================
*/
void CL_ServerInfoPacket(const netadr_t* from, msg_t* msg)
{
    int i, type;
    char info[MAX_INFO_STRING];
    char* infoString;
    int prot;

    infoString = MSG_ReadString(msg);

    // if this isn't the correct protocol version, ignore it
    prot = atoi(Info_ValueForKey(infoString, "protocol"));
    if (prot != PROTOCOL_VERSION) {
        Q_DPrintln("Different protocol info packet: {}", infoString);
        return;
    }

    if (cl_filterGames && cl_filterGames->string && cl_filterGames->string[0]) {
        const char* gameFolder = Info_ValueForKey(infoString, "game");

        // If no game folder was specified the server is using base. Use the BASEGAME string so we can filter for it.
        if (!gameFolder[0])
            gameFolder = BASEGAME;

        // NOTE: As the command tokenization doesn't support nested quotes we can't filter fs_game with spaces using
        //       this approach, but fs_game with spaces cause other issues as well, like downloads not working and at
        //       the time of writing this no public servers actually use an fs_game with spaces...
        Cmd_TokenizeString(cl_filterGames->string);
        for (i = 0; i < Cmd_Argc(); i++) {
            if (!Q_stricmp(Cmd_Argv(i), gameFolder) && Q_stricmp(Cmd_Argv(i), FS_GetCurrentGameDir(false))) {
                return;
            }
        }
    }

    // iterate servers waiting for ping response
    for (i = 0; i < MAX_PINGREQUESTS; i++) {
        if (cl_pinglist[i].adr.port && !cl_pinglist[i].answered && NET_CompareAdr(from, &cl_pinglist[i].adr)) {
            // calc ping time
            cl_pinglist[i].time = Sys_Milliseconds() - cl_pinglist[i].start;
            cl_pinglist[i].answered = true;
            if (com_developer->integer) {
                Q_Println("ping time {}ms from {}", cl_pinglist[i].time, NET_AdrToString(from));
            }

            // save of info
            Q_strncpyz(cl_pinglist[i].info, infoString, sizeof(cl_pinglist[i].info));

            // tack on the net type
            // NOTE: make sure these types are in sync with the netnames strings in the UI
            switch (from->type) {
            case netadrtype_t::NA_BROADCAST:
            case netadrtype_t::NA_IP:
                type = 1;
                break;

            default:
                type = 0;
                break;
            }
            Info_SetValueForKey(cl_pinglist[i].info, "nettype", Q_Format("{}", type).c_str());
            CL_SetServerInfoByAddress(from, infoString, lanServerState_t::LANSERVER_ANSWERED, cl_pinglist[i].time);

            return;
        }
    }

    // if not just sent a local broadcast or pinging local servers
    if (cls.pingUpdateSource != lanSource_t::AS_LOCAL) {
        return;
    }

    for (i = 0; i < MAX_OTHER_SERVERS; i++) {
        // empty slot
        if (cls.localServers[i].adr.port == 0) {
            break;
        }

        // avoid duplicate
        if (NET_CompareAdr(from, &cls.localServers[i].adr)) {
            return;
        }
    }

    if (i == MAX_OTHER_SERVERS) {
        Q_DPrintln("MAX_OTHER_SERVERS hit, dropping infoResponse");
        return;
    }

    // add this to the list
    cls.numlocalservers = i + 1;
    CL_InitServerInfo(&cls.localServers[i], from);

    Q_strncpyz(info, MSG_ReadString(msg), MAX_INFO_STRING);
    if (strlen(info)) {
        if (info[strlen(info) - 1] != '\n') {
            Q_strcat(info, sizeof(info), "\n");
        }
        Q_Print("{}: {}", NET_AdrToString(from), info);
    }
}

/*
===================
CL_GetServerStatus
===================
*/
serverStatus_t* CL_GetServerStatus(const netadr_t* from)
{
    int i, oldest, oldestTime;

    for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
        if (NET_CompareAdr(from, &cl_serverStatusList[i].address)) {
            return &cl_serverStatusList[i];
        }
    }
    for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
        if (cl_serverStatusList[i].retrieved) {
            return &cl_serverStatusList[i];
        }
    }
    oldest = -1;
    oldestTime = 0;
    for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
        if (oldest == -1 || cl_serverStatusList[i].startTime < oldestTime) {
            oldest = i;
            oldestTime = cl_serverStatusList[i].startTime;
        }
    }
    if (oldest != -1) {
        return &cl_serverStatusList[oldest];
    }
    serverStatusCount++;
    return &cl_serverStatusList[serverStatusCount & (MAX_SERVERSTATUSREQUESTS - 1)];
}

/*
===================
CL_ServerStatus
===================
*/
int CL_ServerStatus(const char* serverAddress, char* serverStatusString, int maxLen)
{
    int i;
    netadr_t to;
    serverStatus_t* serverStatus;

    // if no server address then reset all server status requests
    if (!serverAddress) {
        for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
            cl_serverStatusList[i].address.port = 0;
            cl_serverStatusList[i].retrieved = true;
        }
        return false;
    }
    // get the address
    if (!NET_StringToAdr(serverAddress, &to)) {
        return false;
    }
    serverStatus = CL_GetServerStatus(&to);
    // if no server status string then reset the server status request for this address
    if (!serverStatusString) {
        serverStatus->retrieved = true;
        return false;
    }

    // if this server status request has the same address
    if (NET_CompareAdr(&to, &serverStatus->address)) {
        // if we received a response for this server status request
        if (!serverStatus->pending) {
            Q_strncpyz(serverStatusString, serverStatus->string, maxLen);
            serverStatus->retrieved = true;
            serverStatus->startTime = 0;
            return true;
        }
        // resend the request regularly
        else if (serverStatus->startTime < Com_Milliseconds() - cl_serverStatusResendTime->integer) {
            serverStatus->print = false;
            serverStatus->pending = true;
            serverStatus->retrieved = false;
            serverStatus->time = 0;
            serverStatus->startTime = Com_Milliseconds();
            NET_OutOfBandPrint(netsrc_t::NS_CLIENT, &to, "getstatus");
            return false;
        }
    }
    // if retrieved
    else if (serverStatus->retrieved) {
        serverStatus->address = to;
        serverStatus->print = false;
        serverStatus->pending = true;
        serverStatus->retrieved = false;
        serverStatus->startTime = Com_Milliseconds();
        serverStatus->time = 0;
        NET_OutOfBandPrint(netsrc_t::NS_CLIENT, &to, "getstatus");
        return false;
    }
    return false;
}

/*
===================
CL_ServerStatusResponse
===================
*/
void CL_ServerStatusResponse(const netadr_t* from, msg_t* msg)
{
    const char* s;
    char info[MAX_INFO_STRING];
    int i, l, score, ping;
    int len;
    serverStatus_t* serverStatus;

    serverStatus = nullptr;
    for (i = 0; i < MAX_SERVERSTATUSREQUESTS; i++) {
        if (NET_CompareAdr(from, &cl_serverStatusList[i].address)) {
            serverStatus = &cl_serverStatusList[i];
            break;
        }
    }
    // if we didn't request this server status
    if (!serverStatus) {
        return;
    }

    s = MSG_ReadStringLine(msg);

    len = 0;
    Q_FormatTo(std::span(&serverStatus->string[len], sizeof(serverStatus->string) - len), "{}", s);

    if (serverStatus->print) {
        Q_Println("Server ({})", NET_AdrToString(&serverStatus->address));
        Q_Println("Server settings:");
        // print cvars
        while (*s) {
            for (i = 0; i < 2 && *s; i++) {
                if (*s == '\\')
                    s++;
                l = 0;
                while (*s) {
                    info[l++] = *s;
                    if (l >= MAX_INFO_STRING - 1)
                        break;
                    s++;
                    if (*s == '\\') {
                        break;
                    }
                }
                info[l] = '\0';
                if (i) {
                    Q_Println("{}", info);
                }
                else {
                    Q_Print("{:24}", info);
                }
            }
        }
    }

    len = strlen(serverStatus->string);
    Q_FormatTo(std::span(&serverStatus->string[len], sizeof(serverStatus->string) - len), "\\");

    if (serverStatus->print) {
        Q_Println("\nPlayers:");
        Q_Println("num: score: ping: name:");
    }
    for (i = 0, s = MSG_ReadStringLine(msg); *s; s = MSG_ReadStringLine(msg), i++) {

        len = strlen(serverStatus->string);
        Q_FormatTo(std::span(&serverStatus->string[len], sizeof(serverStatus->string) - len), "\\{}", s);

        if (serverStatus->print) {
            score = ping = 0;
            sscanf(s, "%d %d", &score, &ping);
            s = strchr(s, ' ');
            if (s)
                s = strchr(s + 1, ' ');
            if (s)
                s++;
            else
                s = "unknown";
            Q_Println("{:<2}   {:<3}    {:<3}   {}", i, score, ping, s);
        }
    }
    len = strlen(serverStatus->string);
    Q_FormatTo(std::span(&serverStatus->string[len], sizeof(serverStatus->string) - len), "\\");

    serverStatus->time = Com_Milliseconds();
    serverStatus->address = *from;
    serverStatus->pending = false;
    if (serverStatus->print) {
        serverStatus->retrieved = true;
    }
}

/*
==================
CL_LocalServers_f
==================
*/
void CL_LocalServers_f(void)
{
    const char* message;
    int i, j;
    netadr_t to;

    Q_Println("Scanning for servers on the local network...");

    // reset the list, waiting for response
    cls.numlocalservers = 0;
    cls.pingUpdateSource = lanSource_t::AS_LOCAL;

    for (i = 0; i < MAX_OTHER_SERVERS; i++) {
        bool b = cls.localServers[i].visible;
        Com_Memset(&cls.localServers[i], 0, sizeof(cls.localServers[i]));
        cls.localServers[i].visible = b;
    }
    Com_Memset(&to, 0, sizeof(to));

    // The 'xxx' in the message is a challenge that will be echoed back
    // by the server.  We don't care about that here, but master servers
    // can use that to prevent spoofed server responses from invalid ip
    message = "\377\377\377\377getinfo xxx";

    // send each message twice in case one is dropped
    for (i = 0; i < 2; i++) {
        // send a broadcast packet on each server port
        // we support multiple server ports so a single machine
        // can nicely run multiple servers
        for (j = 0; j < NUM_SERVER_PORTS; j++) {
            to.port = BigShort((short)(PORT_SERVER + j));

            to.type = netadrtype_t::NA_BROADCAST;
            NET_SendPacket(netsrc_t::NS_CLIENT, strlen(message), message, &to);
        }
    }
}

/*
==================
CL_GlobalServers_f

Originally master 0 was Internet and master 1 was MPlayer.
ioquake3 2008; added support for requesting five separate master servers using 0-4.
ioquake3 2017; made master 0 fetch all master servers and 1-5 request a single master server.
OpenJK 2013; added support for requesting five separate master servers using 0-4.
OpenJK July 2017; made master 0 fetch all master servers and 1-5 request a single master server.

==================
*/
void CL_GlobalServers_f(void)
{
    netadr_t to;
    int count, i, masterNum;
    char command[1024];
    const char* masteraddress;

    if ((count = Cmd_Argc()) < 3 || (masterNum = atoi(Cmd_Argv(1))) < 0 || masterNum > MAX_MASTER_SERVERS) {
        Q_Println("usage: globalservers <master# 0-{}> <protocol> [keywords]", MAX_MASTER_SERVERS);
        return;
    }

    // request from all master servers
    if (masterNum == 0) {
        int numAddress = 0;

        for (i = 1; i <= MAX_MASTER_SERVERS; i++) {
            Q_FormatTo(command, "sv_master{}", i);
            masteraddress = Cvar_VariableString(command);

            if (!*masteraddress)
                continue;

            numAddress++;

            Q_FormatTo(command, "globalservers {} {} {}\n", i, Cmd_Argv(2), Cmd_ArgsFrom(3));
            Cbuf_AddText(command);
        }

        if (!numAddress) {
            Q_Println("CL_GlobalServers_f: Error: No master server addresses.");
        }
        return;
    }

    Q_FormatTo(command, "sv_master{}", masterNum);
    masteraddress = Cvar_VariableString(command);

    if (!*masteraddress) {
        Q_Println("CL_GlobalServers_f: Error: No master server address given for {}.", command);
        return;
    }

    // reset the list, waiting for response
    // -1 is used to distinguish a "no response"

    i = NET_StringToAdr(masteraddress, &to);

    if (!i) {
        Q_Println("CL_GlobalServers_f: Error: could not resolve address of master {}", masteraddress);
        return;
    }
    to.type = netadrtype_t::NA_IP;
    to.port = BigShort(PORT_MASTER);

    Q_Println("Requesting servers from the master {} ({})...", masteraddress, NET_AdrToString(&to));

    cls.numglobalservers = -1;
    cls.pingUpdateSource = lanSource_t::AS_GLOBAL;

    Q_FormatTo(command, "getservers {}", Cmd_Argv(2));

    // tack on keywords
    for (i = 3; i < count; i++) {
        Q_strcat(command, sizeof(command), " ");
        Q_strcat(command, sizeof(command), Cmd_Argv(i));
    }

    NET_OutOfBandPrint(netsrc_t::NS_SERVER, &to, "{}", command);
}

/*
==================
CL_GetPing
==================
*/
lanServerState_t CL_GetPing(int n, char* buf, int buflen, int* pingtime)
{
    const char* str;
    int time;
    int maxPing;
    lanServerState_t state;

    if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port) {
        // empty or invalid slot
        buf[0] = '\0';
        *pingtime = 0;
        return lanServerState_t::LANSERVER_NOT_ASKED;
    }

    str = NET_AdrToString(&cl_pinglist[n].adr);
    Q_strncpyz(buf, str, buflen);

    time = cl_pinglist[n].time;
    state = lanServerState_t::LANSERVER_ANSWERED;
    if (!cl_pinglist[n].answered) {
        // check for timeout
        time = Sys_Milliseconds() - cl_pinglist[n].start;
        maxPing = Cvar_VariableIntegerValue("cl_maxPing");
        if (maxPing < 100) {
            maxPing = 100;
        }
        if (time < maxPing) {
            // not timed out yet
            time = 0;
            state = lanServerState_t::LANSERVER_WAITING;
        }
        else {
            state = lanServerState_t::LANSERVER_NO_ANSWER;
        }
    }

    CL_SetServerInfoByAddress(&cl_pinglist[n].adr, cl_pinglist[n].info, state, cl_pinglist[n].time);

    *pingtime = time;
    return state;
}

/*
==================
CL_GetPingInfo
==================
*/
void CL_GetPingInfo(int n, char* buf, int buflen)
{
    if (n < 0 || n >= MAX_PINGREQUESTS || !cl_pinglist[n].adr.port) {
        // empty or invalid slot
        if (buflen)
            buf[0] = '\0';
        return;
    }

    Q_strncpyz(buf, cl_pinglist[n].info, buflen);
}

/*
==================
CL_ClearPing
==================
*/
void CL_ClearPing(int n)
{
    if (n < 0 || n >= MAX_PINGREQUESTS)
        return;

    cl_pinglist[n].adr.port = 0;
}

/*
==================
CL_GetPingQueueCount
==================
*/
int CL_GetPingQueueCount(void)
{
    int i;
    int count;
    ping_t* pingptr;

    count = 0;
    pingptr = cl_pinglist;

    for (i = 0; i < MAX_PINGREQUESTS; i++, pingptr++) {
        if (pingptr->adr.port) {
            count++;
        }
    }

    return (count);
}

/*
==================
CL_GetFreePing
==================
*/
ping_t* CL_GetFreePing(void)
{
    ping_t* pingptr;
    ping_t* best;
    int oldest;
    int i;
    int time;

    pingptr = cl_pinglist;
    for (i = 0; i < MAX_PINGREQUESTS; i++, pingptr++) {
        // find free ping slot
        if (pingptr->adr.port) {
            if (!pingptr->answered) {
                if (Sys_Milliseconds() - pingptr->start < 500) {
                    // still waiting for response
                    continue;
                }
            }
            else if (pingptr->time < 500) {
                // results have not been queried
                continue;
            }
        }

        // clear it
        pingptr->adr.port = 0;
        return (pingptr);
    }

    // use oldest entry
    pingptr = cl_pinglist;
    best = cl_pinglist;
    oldest = INT_MIN;
    for (i = 0; i < MAX_PINGREQUESTS; i++, pingptr++) {
        // scan for oldest
        time = Sys_Milliseconds() - pingptr->start;
        if (time > oldest) {
            oldest = time;
            best = pingptr;
        }
    }

    return (best);
}

/*
==================
CL_Ping_f
==================
*/
void CL_Ping_f(void)
{
    netadr_t to;
    ping_t* pingptr;
    const char* server;

    if (Cmd_Argc() != 2) {
        Q_Println("usage: ping [server]");
        return;
    }

    Com_Memset(&to, 0, sizeof(netadr_t));

    server = Cmd_Argv(1);

    if (!NET_StringToAdr(server, &to)) {
        return;
    }

    pingptr = CL_GetFreePing();

    memcpy(&pingptr->adr, &to, sizeof(netadr_t));
    pingptr->start = Sys_Milliseconds();
    pingptr->time = 0;
    pingptr->answered = false;

    CL_SetServerInfoByAddress(&pingptr->adr, nullptr, lanServerState_t::LANSERVER_WAITING, 0);

    NET_OutOfBandPrint(netsrc_t::NS_CLIENT, &to, "getinfo xxx");
}

/*
==================
CL_UpdateVisiblePings_f
==================
*/
bool CL_UpdateVisiblePings_f(lanSource_t source)
{
    int slots, i;
    char buff[MAX_STRING_CHARS];
    int pingTime;
    int max;
    bool status = false;

    if (source < lanSource_t::AS_LOCAL || source > lanSource_t::AS_FAVORITES) {
        return false;
    }

    cls.pingUpdateSource = source;

    slots = CL_GetPingQueueCount();
    if (slots < MAX_PINGREQUESTS) {
        serverInfo_t* server = nullptr;

        switch (source) {
        case lanSource_t::AS_LOCAL:
            server = &cls.localServers[0];
            max = cls.numlocalservers;
            break;
        case lanSource_t::AS_GLOBAL:
            server = &cls.globalServers[0];
            max = cls.numglobalservers;
            break;
        case lanSource_t::AS_FAVORITES:
            server = &cls.favoriteServers[0];
            max = cls.numfavoriteservers;
            break;
        default:
            return false;
        }
        for (i = 0; i < max; i++) {
            if (server[i].visible) {
                if (server[i].state == lanServerState_t::LANSERVER_NOT_ASKED) {
                    int j;

                    if (slots >= MAX_PINGREQUESTS) {
                        break;
                    }
                    for (j = 0; j < MAX_PINGREQUESTS; j++) {
                        if (!cl_pinglist[j].adr.port) {
                            continue;
                        }
                        if (NET_CompareAdr(&cl_pinglist[j].adr, &server[i].adr)) {
                            // already on the list
                            break;
                        }
                    }
                    if (j >= MAX_PINGREQUESTS) {
                        status = true;
                        for (j = 0; j < MAX_PINGREQUESTS; j++) {
                            if (!cl_pinglist[j].adr.port) {
                                break;
                            }
                        }
                        memcpy(&cl_pinglist[j].adr, &server[i].adr, sizeof(netadr_t));
                        cl_pinglist[j].start = Sys_Milliseconds();
                        cl_pinglist[j].time = 0;
                        cl_pinglist[j].answered = false;
                        NET_OutOfBandPrint(netsrc_t::NS_CLIENT, &cl_pinglist[j].adr, "getinfo xxx");
                        slots++;
                    }
                }
                // if the server has a ping higher than cl_maxPing or
                // the ping packet got lost
                else if (server[i].state == lanServerState_t::LANSERVER_NO_ANSWER) {
                    // if we are updating global servers
                    if (source == lanSource_t::AS_GLOBAL) {
                        //
                        if (cls.numGlobalServerAddresses > 0) {
                            // overwrite this server with one from the additional global servers
                            cls.numGlobalServerAddresses--;
                            CL_InitServerInfo(&server[i], &cls.globalServerAddresses[cls.numGlobalServerAddresses]);
                            // NOTE: the server[i].visible flag stays untouched
                        }
                    }
                }
            }
        }
    }

    if (slots) {
        status = true;
    }
    for (i = 0; i < MAX_PINGREQUESTS; i++) {
        if (!cl_pinglist[i].adr.port) {
            continue;
        }
        if (CL_GetPing(i, buff, MAX_STRING_CHARS, &pingTime) != lanServerState_t::LANSERVER_WAITING) {
            CL_ClearPing(i);
            status = true;
        }
    }

    return status;
}

/*
==================
CL_ServerStatus_f
==================
*/
void CL_ServerStatus_f(void)
{
    netadr_t to, *toptr = nullptr;
    const char* server;
    serverStatus_t* serverStatus;

    if (Cmd_Argc() != 2) {
        if (cls.state != connstate_t::CA_ACTIVE || clc.demoplaying) {
            Q_Println("Not connected to a server.");
            Q_Println("Usage: serverstatus [server]");
            return;
        }

        toptr = &clc.serverAddress;
    }

    if (!toptr) {
        Com_Memset(&to, 0, sizeof(netadr_t));

        server = Cmd_Argv(1);

        toptr = &to;
        if (!NET_StringToAdr(server, toptr))
            return;
    }

    NET_OutOfBandPrint(netsrc_t::NS_CLIENT, toptr, "getstatus");

    serverStatus = CL_GetServerStatus(toptr);
    serverStatus->address = *toptr;
    serverStatus->print = true;
    serverStatus->pending = true;
}

/*
==================
CL_ShowIP_f
==================
*/
void CL_ShowIP_f(void)
{
    Sys_ShowIP();
}

/*
==================
  Internal Menu
==================
*/

void CL_DrawMenuRect(float x, float y, float width, float height, float borderSize, vec4_t elementBackgroundColor, vec4_t elementBorderColor)
{
    // Draw the background
    if (elementBackgroundColor) {
        re->SetColor(elementBackgroundColor);
        re->DrawStretchPic(x, y, width, height, 0, 0, 0, 0, cls.whiteShader);
    }

    // Draw the borders
    if (elementBorderColor) {
        re->SetColor(elementBorderColor);
        re->DrawStretchPic(x, y, width, borderSize, 0, 0, 0, 0, cls.whiteShader);
        re->DrawStretchPic(x, y + height - borderSize, width, borderSize, 0, 0, 0, 0, cls.whiteShader);
        re->DrawStretchPic(x, y, borderSize, height, 0, 0, 0, 0, cls.whiteShader);
        re->DrawStretchPic(x + width - borderSize, y, borderSize, height, 0, 0, 0, 0, cls.whiteShader);
    }

    // Prevent color from leaking
    re->SetColor(nullptr);
}

void CL_DrawCenterStringAt(int x, int y, const char* str, int font, float scale)
{
    int lenX = re->Font_StrLenPixels(str, font, scale);
    int lenY = re->Font_HeightPixels(font, scale);
    re->Font_DrawString(x - (lenX / 2), y - (lenY / 2), str, colorWhite, font, -1, scale);
}

typedef struct menuButton_s {
    int x;
    int y;
    float width;
    float height;
    const char* text;
    int font;
    float scale;
    void (*action)(void);
} menuButton_t;
static menuButton_t menuButtons[16]; // No need for dynamic lists. We currently got exactly one menu with max. 3 buttons
static int menuButtonsActive = 0;

static menuButton_t* CL_RegisterMenuButtonArea(int x, int y, float width, float height, const char* text, int font, float scale, void (*action)(void))
{
    // Too many buttons? Silently discard it to avoid console spam
    if (menuButtonsActive >= (int)ARRAY_LEN(menuButtons))
        return nullptr;

    // Set the values
    menuButtons[menuButtonsActive].x = x;
    menuButtons[menuButtonsActive].y = y;
    menuButtons[menuButtonsActive].width = width;
    menuButtons[menuButtonsActive].height = height;
    menuButtons[menuButtonsActive].text = text;
    menuButtons[menuButtonsActive].font = font;
    menuButtons[menuButtonsActive].scale = scale;
    menuButtons[menuButtonsActive].action = action;

    // Increment counter
    return &menuButtons[menuButtonsActive++];
}

static bool CL_IsCursorOnMenuButton(menuButton_t* button)
{
    if (cls.cursorX >= button->x && cls.cursorX <= button->x + button->width && cls.cursorY >= button->y && cls.cursorY <= button->y + button->height)
        return true;
    return false;
}

static void CL_DrawMenuButton(menuButton_t* button, vec4_t buttonBackgroundColor, vec4_t buttonBorderColor)
{
    if (!button)
        return;
    CL_DrawMenuRect(button->x, button->y, button->width, button->height, 3, buttonBackgroundColor, buttonBorderColor);
    CL_DrawCenterStringAt(button->x + (button->width / 2), button->y + 5, button->text, button->font, button->scale);
}

static formatText_t CL_ByteCountToHumanString(int byteCount)
{
    constexpr int GB_BYTES = 1024 * 1024 * 1024;
    constexpr int MB_BYTES = 1024 * 1024;
    constexpr int KB_BYTES = 1024;

    if (byteCount > GB_BYTES)
        return Q_Format("{:.2f} GB", (float)byteCount / GB_BYTES);
    else if (byteCount > MB_BYTES)
        return Q_Format("{:.2f} MB", (float)byteCount / MB_BYTES);
    else if (byteCount > KB_BYTES)
        return Q_Format("{:.1f} KB", (float)byteCount / KB_BYTES);
    else
        return Q_Format("{} B", byteCount);
}

static const char* CL_DurationSecToString(int duration)
{ // SkyMod: Duration seconds to string
    constexpr int TIME_YEAR = 60 * 60 * 24 * 365;
    constexpr int TIME_WEEK = 60 * 60 * 24 * 7;
    constexpr int TIME_DAY = 60 * 60 * 24;
    constexpr int TIME_HOUR = 60 * 60;
    constexpr int TIME_MINUTE = 60;

    static int call;
    static char bufs[2][128];
    char* durationStr = bufs[call & 1];
    int years = 0, weeks = 0, days = 0, hours = 0, minutes = 0, seconds = 0;
    call++;

    while (duration) {
        if (duration >= TIME_YEAR) {
            duration -= TIME_YEAR;
            years++;
        }
        else if (duration >= TIME_WEEK) {
            duration -= TIME_WEEK;
            weeks++;
        }
        else if (duration >= TIME_DAY) {
            duration -= TIME_DAY;
            days++;
        }
        else if (duration >= TIME_HOUR) {
            duration -= TIME_HOUR;
            hours++;
        }
        else if (duration >= TIME_MINUTE) {
            duration -= TIME_MINUTE;
            minutes++;
        }
        else {
            seconds = duration;
            duration = 0;
        }
    }

    *durationStr = 0;
    if (years)
        Q_strcat(durationStr, sizeof(bufs[0]), Q_Format("{}y ", years).c_str());
    if (weeks)
        Q_strcat(durationStr, sizeof(bufs[0]), Q_Format("{}w ", weeks).c_str());
    if (days)
        Q_strcat(durationStr, sizeof(bufs[0]), Q_Format("{}d ", days).c_str());
    if (hours)
        Q_strcat(durationStr, sizeof(bufs[0]), Q_Format("{}h ", hours).c_str());
    if (minutes)
        Q_strcat(durationStr, sizeof(bufs[0]), Q_Format("{}m ", minutes).c_str());
    if (seconds)
        Q_strcat(durationStr, sizeof(bufs[0]), Q_Format("{}s ", seconds).c_str());

    if (*durationStr) { // Strip tailing space
        char* ptr = durationStr;
        while (*ptr)
            ptr++;
        if (*(ptr - 1) == ' ')
            *(ptr - 1) = 0;
    }

    return durationStr;
}

void CL_DrawDownloadRequest(void)
{
    // Menu values
    static vec4_t backgroundColor = {0.1f, 0.2f, 0.45f, 0.9f};
    static vec4_t borderColor = {0.05f, 0.1f, 0.35f, 1.0f};
    static float width = SCREEN_WIDTH / 6;
    static float height = SCREEN_HEIGHT / 4;

    static float centerX = SCREEN_WIDTH / 2;
    static float centerY = SCREEN_HEIGHT / 2;

    static float scale = 1.0f;

    // Button values
    static vec4_t buttonBackgroundColor = {0.1f, 0.1f, 0.4f, 0.9f};
    static vec4_t buttonBorderColor = {0.05f, 0.05f, 0.2f, 1.0f};
    menuButton_t* button;

    // Draw frame
    CL_DrawMenuRect(width, height, width * 4, height * 2, 6, backgroundColor, borderColor);

    // Header
    CL_DrawCenterStringAt(centerX, height + 10, "^1[ ^7File Download ^1]", cls.menuFont, scale);

    // File name
    CL_DrawCenterStringAt(centerX, centerY - (height / 2), cl_downloadName->string, cls.menuFont, scale);

    if (clc.downloadWaitingOnUser) {
        // Tell user that we're waiting on their decision
        CL_DrawCenterStringAt(centerX, centerY - 10, "Do you want to download this file?", cls.menuFont, scale);
        CL_DrawCenterStringAt(centerX, centerY + 10, "Please select an option", cls.menuFont, scale);
    }
    else {
        // Draw Progress Bar
        static vec4_t barBackgroundColor = {0.1f, 0.8f, 0.4f, 0.9f};
        static vec4_t barBorderColor = {0.05f, 0.6f, 0.2f, 1.0f};
        float dlFrac = clc.downloadSize ? (float)clc.downloadCount / clc.downloadSize : 0.0f;

        // Download speed
        int dlTime = (float)(cls.realtime - clc.downloadTime) / 1000.0f;
        static int dlRate;
        static int dlLastTime;
        static int dlLastCount;

        // Bar with percentage
        CL_DrawCenterStringAt(centerX, centerY - 10, "Progress:", cls.menuFont, scale);
        CL_DrawMenuRect(centerX - width * 2 + 10, centerY + 5, (width * 4 - 20 - 3) * dlFrac, 20, 3, barBackgroundColor, nullptr);
        CL_DrawMenuRect(centerX - width * 2 + 10, centerY + 5, width * 4 - 20 - 3, 20, 3, nullptr, barBorderColor);
        CL_DrawCenterStringAt(centerX, centerY + 10, Q_Format("{:.2f}%", dlFrac * 100).c_str(), cls.menuFont, scale);

        // Draw size info
        re->Font_DrawString(width + 10, centerY + 30, Q_Format("File Size: {}", CL_ByteCountToHumanString(clc.downloadSize)).c_str(), colorWhite, cls.menuFont,
                            -1, scale);
        re->Font_DrawString(width + 10, centerY + 50, Q_Format("Downloaded: {}", CL_ByteCountToHumanString(clc.downloadCount)).c_str(), colorWhite,
                            cls.menuFont, -1, scale);

        // Download Speed
        if (dlTime >= 1) {
            if (dlTime != dlLastTime) {
                // Second passed, update measured values
                dlRate = clc.downloadCount - dlLastCount;
                dlLastTime = dlTime;
                dlLastCount = clc.downloadCount;
            }

            // Draw info texts
            re->Font_DrawString(width + 10, centerY + 70, Q_Format("Transfer Rate: {}/sec", CL_ByteCountToHumanString(dlRate)).c_str(), colorWhite,
                                cls.menuFont, -1, scale);
            re->Font_DrawString(
                width + 10, centerY + 90,
                Q_Format("Estimated Time Left: {}", dlRate ? CL_DurationSecToString(clc.downloadSize / dlRate - dlLastCount / dlRate) : "unknown").c_str(),
                colorWhite, cls.menuFont, -1, scale);
        }
        else {
            // Set defaults - if we somehow don't get here and start with dlTime >= 1 we get incorrect values for one second, but that's okay
            dlLastCount = clc.downloadCount;
            dlLastTime = 0;
            dlRate = 0;

            re->Font_DrawString(width + 10, centerY + 70, "Transfer Rate: estimating", colorWhite, cls.menuFont, -1, scale);
            re->Font_DrawString(width + 10, centerY + 90, "Estimated Time Left: estimating", colorWhite, cls.menuFont, -1, scale);
        }
    }

    // Draw Buttons
    if (clc.downloadWaitingOnUser) {
        // Only show yes/no if we're waiting on a user decision
        button = CL_RegisterMenuButtonArea(width + 10, (height * 3) - 30, 40, 20, "Yes", cls.menuFont, scale, CL_BeginDownloadConfirm);
        CL_DrawMenuButton(button, buttonBackgroundColor, buttonBorderColor);

        button = CL_RegisterMenuButtonArea(width + 60, (height * 3) - 30, 40, 20, "No", cls.menuFont, scale, CL_NextDownload);
        CL_DrawMenuButton(button, buttonBackgroundColor, buttonBorderColor);
    }

    // The user can always abort if they change their mind
    button = CL_RegisterMenuButtonArea(SCREEN_WIDTH - width - 13 - 80, (height * 3) - 30, 80, 20, "Abort", cls.menuFont, scale, CL_Disconnect_f);
    CL_DrawMenuButton(button, buttonBackgroundColor, buttonBorderColor);
}

void CL_DrawEngineMenus(void)
{
    if (clc.downloadMenuActive) {
        // Accept cursor movement
        cls.cursorActive = true;
    }
    else {
        // Disable movement
        cls.cursorActive = false;
    }

    // Reset all menu buttons, the menu set them if required
    menuButtonsActive = 0;

    // Draw menus
    if (clc.downloadMenuActive) {
        CL_DrawDownloadRequest();
    }

    // Draw cursor
    if (cls.cursorActive) {
        int i;

        // Re-draw the buttons the user is hovering over, but use a different color
        static vec4_t buttonHoverBackground = {0.75f, 0.5f, 0.0f, 1.0f};
        static vec4_t buttonHoverFrame = {0.50f, 0.25f, 0.0f, 1.0f};
        for (i = 0; i < menuButtonsActive; i++) {
            if (CL_IsCursorOnMenuButton(&menuButtons[i])) {
                CL_DrawMenuButton(&menuButtons[i], buttonHoverBackground, buttonHoverFrame);
            }
        }
    }
}

void CL_CursorButton(fakeAscii_t key)
{
    int i;
    if (key == fakeAscii_t::A_MOUSE1) {
        for (i = 0; i < menuButtonsActive; i++) {
            if (CL_IsCursorOnMenuButton(&menuButtons[i])) {
                menuButtons[i].action();
            }
        }
    }
}

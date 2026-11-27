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

// input.h -- the clients' input, the same in the three games: cl_input.cpp beside it, compiled by each client against
// its own headers (each client's client.h includes this one), and a part each tree writes in its own client/cl_input.cpp
// (its commands and their packets, the buttons' bits its game reads, its map view) and client.h (the values of its game:
// CL_OVERRIDE_MOUSE_SENSITIVITY, CL_WALK_MOVESPEED). The view turns at every frame; the movement is measured over the
// span of each command, a frame in the single player, a tick in the multiplayer.
#pragma once

#include <shared/client/button.h>

#include <cstdint>

// the buttons a command carries as bits, +button0 to +button15
inline constexpr int MAX_KBUTTONS = 16;

//
// cl_input.cpp (src/shared/client/engine)
//
extern kbutton_t in_left, in_right, in_forward, in_back;
extern kbutton_t in_lookup, in_lookdown, in_moveleft, in_moveright;
extern kbutton_t in_strafe, in_speed;
extern kbutton_t in_up, in_down;
extern kbutton_t in_buttons[MAX_KBUTTONS];
extern bool in_mlooking;

// the cgame's override of the view's turn while it controls it (a vehicle, a creature ridden): the factors of the yaw
// and the pitch, and of the mouse's sensitivity (the multiplayer's)
extern float cl_mPitchOverride;
extern float cl_mYawOverride;
extern float cl_mSensitivityOverride;

void IN_KeyDown(kbutton_t* b);
void IN_KeyUp(kbutton_t* b);
void IN_Button0Down(void);
void IN_Button0Up(void);
void IN_Button1Down(void);
void IN_Button1Up(void);
void IN_Button2Down(void);
void IN_Button2Up(void);
void IN_Button3Down(void);
void IN_Button3Up(void);
void IN_Button4Down(void);
void IN_Button4Up(void);
void IN_Button5Down(void);
void IN_Button5Up(void);
void IN_Button6Down(void);
void IN_Button6Up(void);
void IN_Button7Down(void);
void IN_Button7Up(void);
void IN_Button8Down(void);
void IN_Button8Up(void);
void IN_Button9Down(void);
void IN_Button9Up(void);
void IN_Button10Down(void);
void IN_Button10Up(void);
void IN_Button11Down(void);
void IN_Button11Up(void);
void IN_Button12Down(void);
void IN_Button12Up(void);
void IN_Button13Down(void);
void IN_Button13Up(void);
void IN_Button14Down(void);
void IN_Button14Up(void);
void IN_Button15Down(void);
void IN_Button15Up(void);

// the share of a span (the frame for the view, the command's span for the movement) a button was held
float CL_KeyState(kbutton_t* key, int64_t span);

// the input read at a time of the engine's clock (com_frameTime's, in ms) and of the client's (in ns): the frame's span
// since the input was last read
void CL_InputRead(int inputTime, int64_t inputNsec);
// a command starts: its span is the time since the last command, or the frame's (before the multiplayer's ticks run)
void CL_CommandStart(bool sinceLastCommand);
// the view turned at every frame: the keys, the mouse, the joystick
void CL_UpdateViewAngles(void);
// a command's movement and walking flag from the keys, over the command's span
void CL_KeyMove(usercmd_t* cmd);
// a command's movement from the mouse (while strafing, or without mouse look), gathered since the last command
void CL_MouseMoveToCommand(usercmd_t* cmd);
// a command's movement from the joystick
void CL_JoystickMove(usercmd_t* cmd);

// the commands of the buttons the three games share (+forward, +button0...) and the input's settings registered, and
// the commands removed
void CL_InitCommonInput(void);
void CL_ShutdownCommonInput(void);

//
// each tree's client/cl_input.cpp
//
// the view's pitch centred on the player's
void IN_CenterView(void);
// a press or a release of a movement button that the map view takes instead (the multiplayer's automap): true when it
// takes it
bool CL_MapTakesButton(const kbutton_t* button, bool down);
// the mouse's motion that the map view takes instead: true when it takes it
bool CL_MapTakesMouse(float dx, float dy);
// the mouse's pitch factor (m_pitch, the multiplayer's m_pitchVeh in a fighter)
float CL_MousePitch(void);

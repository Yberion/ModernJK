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

// cl_input.cpp -- the clients' input, the same in the three games (input.h), compiled by each client against its own
// headers: the buttons of the game's commands and the shares of a span they were held, the view turned at every frame
// by the keys, the mouse and the joystick, and the movement of a command. Each tree's client/cl_input.cpp makes the
// commands and sends them. A button's arithmetic is src/shared/client's (button.h).
#include <client/client.h>

#include <cstdlib>
#include <optional>

unsigned frame_msec; // the frame's length: the view turns every frame
int old_com_frameTime;
// The keys' own times are in nanoseconds on the client's clock, as the platform layer dates them: the time the input was
// last read at on that clock, the time between the two last reads (the view's keys) and the time since the last command
// (the movement's keys). The share of these times a key was held is exact, whatever the frame rate.
static int64_t s_inputNsec;
static int64_t s_lastInputNsec;
static int64_t s_frameNsec = 1'000'000;
static int64_t s_cmdNsec = 1'000'000;
static int64_t s_lastCmdNsec;
// the shortest and the longest of these times: over 1,000 frames per second each frame counts as a millisecond, and
// after a hitch the extra time is cut, so that it gives no unexpected move
constexpr int64_t MIN_INPUT_NSEC = 1'000'000;
constexpr int64_t MAX_INPUT_NSEC = 200'000'000;
// the movement the mouse gives while strafing or without mouse look, gathered over the frames until the next command
static float s_mouseSide, s_mouseForward;

float cl_mPitchOverride = 0.0f;
float cl_mYawOverride = 0.0f;
float cl_mSensitivityOverride = 0.0f;

/*
===============================================================================

KEY BUTTONS

Continuous button event tracking is complicated by the fact that two different
input sources (say, mouse button 1 and the control key) can both press the
same button, but the button should only be released when both of the
pressing key have been released.

When a key event issues a button command (+forward, +attack, etc), it appends
its key number as argv(1) so it can be matched up with the release.

argv(2) will be set to the time the event happened, which allows exact
control even at low framerates when the down and up events may both get qued
at the same time.

===============================================================================
*/

kbutton_t in_left, in_right, in_forward, in_back;
kbutton_t in_lookup, in_lookdown, in_moveleft, in_moveright;
kbutton_t in_strafe, in_speed;
kbutton_t in_up, in_down;

kbutton_t in_buttons[MAX_KBUTTONS];

bool in_mlooking;

void IN_MLookDown(void)
{
    in_mlooking = true;
}

void IN_MLookUp(void)
{
    in_mlooking = false;
    if (!cl_freelook->integer) {
        IN_CenterView();
    }
}

void IN_KeyDown(kbutton_t* b)
{
    const char* c = Cmd_Argv(1);
    const int k = c[0] ? atoi(c) : -1; // -1: typed manually at the console for continuous down

    if (!client_input::PressButton(*b, k, strtoll(Cmd_Argv(2), nullptr, 10))) {
        Q_Println("Three keys down for a button!");
    }
}

void IN_KeyUp(kbutton_t* b)
{
    const char* c = Cmd_Argv(1);

    // without a key: typed manually at the console, assume for unsticking, so clear all
    client_input::ReleaseButton(*b, c[0] ? std::optional<int>(atoi(c)) : std::nullopt, strtoll(Cmd_Argv(2), nullptr, 10), s_frameNsec);
}

/*
===============
CL_KeyState

Returns the fraction of the span (the frame for the view angles, the time since the last
command for the movement) that the key was down
===============
*/
float CL_KeyState(kbutton_t* key, int64_t span)
{
    return client_input::HeldShare(*key, s_inputNsec, span);
}

// a button's command: the press or the release given to the map view when it takes it
static void IN_Press(kbutton_t* b)
{
    if (!CL_MapTakesButton(b, true)) {
        IN_KeyDown(b);
    }
}
static void IN_Release(kbutton_t* b)
{
    if (!CL_MapTakesButton(b, false)) {
        IN_KeyUp(b);
    }
}

void IN_UpDown(void)
{
    IN_Press(&in_up);
}
void IN_UpUp(void)
{
    IN_Release(&in_up);
}
void IN_DownDown(void)
{
    IN_Press(&in_down);
}
void IN_DownUp(void)
{
    IN_Release(&in_down);
}
void IN_LeftDown(void)
{
    IN_Press(&in_left);
}
void IN_LeftUp(void)
{
    IN_Release(&in_left);
}
void IN_RightDown(void)
{
    IN_Press(&in_right);
}
void IN_RightUp(void)
{
    IN_Release(&in_right);
}
void IN_ForwardDown(void)
{
    IN_Press(&in_forward);
}
void IN_ForwardUp(void)
{
    IN_Release(&in_forward);
}
void IN_BackDown(void)
{
    IN_Press(&in_back);
}
void IN_BackUp(void)
{
    IN_Release(&in_back);
}
void IN_LookupDown(void)
{
    IN_Press(&in_lookup);
}
void IN_LookupUp(void)
{
    IN_Release(&in_lookup);
}
void IN_LookdownDown(void)
{
    IN_Press(&in_lookdown);
}
void IN_LookdownUp(void)
{
    IN_Release(&in_lookdown);
}
void IN_MoveleftDown(void)
{
    IN_Press(&in_moveleft);
}
void IN_MoveleftUp(void)
{
    IN_Release(&in_moveleft);
}
void IN_MoverightDown(void)
{
    IN_Press(&in_moveright);
}
void IN_MoverightUp(void)
{
    IN_Release(&in_moveright);
}

void IN_SpeedDown(void)
{
    IN_Press(&in_speed);
}
void IN_SpeedUp(void)
{
    IN_Release(&in_speed);
}
void IN_StrafeDown(void)
{
    IN_Press(&in_strafe);
}
void IN_StrafeUp(void)
{
    IN_Release(&in_strafe);
}

void IN_Button0Down(void)
{
    IN_Press(&in_buttons[0]);
}
void IN_Button0Up(void)
{
    IN_Release(&in_buttons[0]);
}
void IN_Button1Down(void)
{
    IN_Press(&in_buttons[1]);
}
void IN_Button1Up(void)
{
    IN_Release(&in_buttons[1]);
}
void IN_Button2Down(void)
{
    IN_Press(&in_buttons[2]);
}
void IN_Button2Up(void)
{
    IN_Release(&in_buttons[2]);
}
void IN_Button3Down(void)
{
    IN_Press(&in_buttons[3]);
}
void IN_Button3Up(void)
{
    IN_Release(&in_buttons[3]);
}
void IN_Button4Down(void)
{
    IN_Press(&in_buttons[4]);
}
void IN_Button4Up(void)
{
    IN_Release(&in_buttons[4]);
}
void IN_Button5Down(void) // use key
{
    IN_Press(&in_buttons[5]);
}
void IN_Button5Up(void)
{
    IN_Release(&in_buttons[5]);
}
void IN_Button6Down(void)
{
    IN_Press(&in_buttons[6]);
}
void IN_Button6Up(void)
{
    IN_Release(&in_buttons[6]);
}
void IN_Button7Down(void)
{
    IN_Press(&in_buttons[7]);
}
void IN_Button7Up(void)
{
    IN_Release(&in_buttons[7]);
}
void IN_Button8Down(void)
{
    IN_Press(&in_buttons[8]);
}
void IN_Button8Up(void)
{
    IN_Release(&in_buttons[8]);
}
void IN_Button9Down(void)
{
    IN_Press(&in_buttons[9]);
}
void IN_Button9Up(void)
{
    IN_Release(&in_buttons[9]);
}
void IN_Button10Down(void)
{
    IN_Press(&in_buttons[10]);
}
void IN_Button10Up(void)
{
    IN_Release(&in_buttons[10]);
}
void IN_Button11Down(void)
{
    IN_Press(&in_buttons[11]);
}
void IN_Button11Up(void)
{
    IN_Release(&in_buttons[11]);
}
void IN_Button12Down(void)
{
    IN_Press(&in_buttons[12]);
}
void IN_Button12Up(void)
{
    IN_Release(&in_buttons[12]);
}
void IN_Button13Down(void)
{
    IN_Press(&in_buttons[13]);
}
void IN_Button13Up(void)
{
    IN_Release(&in_buttons[13]);
}
void IN_Button14Down(void)
{
    IN_Press(&in_buttons[14]);
}
void IN_Button14Up(void)
{
    IN_Release(&in_buttons[14]);
}
void IN_Button15Down(void)
{
    IN_Press(&in_buttons[15]);
}
void IN_Button15Up(void)
{
    IN_Release(&in_buttons[15]);
}

//==========================================================================

cvar_t* cl_upspeed;
cvar_t* cl_forwardspeed;
cvar_t* cl_sidespeed;

cvar_t* cl_yawspeed;
cvar_t* cl_pitchspeed;

cvar_t* cl_run;

cvar_t* cl_anglespeedkey;

/*
================
CL_AdjustAngles

Moves the local angle positions
================
*/
static void CL_AdjustAngles(void)
{
    float speed;

    if (in_speed.active) {
        speed = 0.001 * cls.frametime * cl_anglespeedkey->value;
    }
    else {
        speed = 0.001 * cls.frametime;
    }

    if (!in_strafe.active) {
        if (cl_mYawOverride) {
            if (cl_mSensitivityOverride) {
                cl.viewangles[YAW] -= cl_mYawOverride * cl_mSensitivityOverride * speed * cl_yawspeed->value * CL_KeyState(&in_right, s_frameNsec);
                cl.viewangles[YAW] += cl_mYawOverride * cl_mSensitivityOverride * speed * cl_yawspeed->value * CL_KeyState(&in_left, s_frameNsec);
            }
            else {
                cl.viewangles[YAW] -= cl_mYawOverride * CL_OVERRIDE_MOUSE_SENSITIVITY * speed * cl_yawspeed->value * CL_KeyState(&in_right, s_frameNsec);
                cl.viewangles[YAW] += cl_mYawOverride * CL_OVERRIDE_MOUSE_SENSITIVITY * speed * cl_yawspeed->value * CL_KeyState(&in_left, s_frameNsec);
            }
        }
        else {
            cl.viewangles[YAW] -= speed * cl_yawspeed->value * CL_KeyState(&in_right, s_frameNsec);
            cl.viewangles[YAW] += speed * cl_yawspeed->value * CL_KeyState(&in_left, s_frameNsec);
        }
    }

    if (cl_mPitchOverride) {
        if (cl_mSensitivityOverride) {
            cl.viewangles[PITCH] -= cl_mPitchOverride * cl_mSensitivityOverride * speed * cl_pitchspeed->value * CL_KeyState(&in_lookup, s_frameNsec);
            cl.viewangles[PITCH] += cl_mPitchOverride * cl_mSensitivityOverride * speed * cl_pitchspeed->value * CL_KeyState(&in_lookdown, s_frameNsec);
        }
        else {
            cl.viewangles[PITCH] -= cl_mPitchOverride * CL_OVERRIDE_MOUSE_SENSITIVITY * speed * cl_pitchspeed->value * CL_KeyState(&in_lookup, s_frameNsec);
            cl.viewangles[PITCH] += cl_mPitchOverride * CL_OVERRIDE_MOUSE_SENSITIVITY * speed * cl_pitchspeed->value * CL_KeyState(&in_lookdown, s_frameNsec);
        }
    }
    else {
        cl.viewangles[PITCH] -= speed * cl_pitchspeed->value * CL_KeyState(&in_lookup, s_frameNsec);
        cl.viewangles[PITCH] += speed * cl_pitchspeed->value * CL_KeyState(&in_lookdown, s_frameNsec);
    }
}

/*
================
CL_KeyMove

Sets the usercmd_t based on key states
================
*/
void CL_KeyMove(usercmd_t* cmd)
{
    int movespeed;
    int forward, side, up;

    //
    // adjust for speed key / running
    // the walking flag is to keep animations consistant
    // even during acceleration and develeration
    //
    if (in_speed.active != (cl_run->integer != 0)) {
        movespeed = 127;
        cmd->buttons &= ~buttonFlags_t::BUTTON_WALKING;
    }
    else {
        cmd->buttons |= buttonFlags_t::BUTTON_WALKING;
        movespeed = CL_WALK_MOVESPEED;
    }

    forward = 0;
    side = 0;
    up = 0;
    if (in_strafe.active) {
        side += movespeed * CL_KeyState(&in_right, s_cmdNsec);
        side -= movespeed * CL_KeyState(&in_left, s_cmdNsec);
    }

    side += movespeed * CL_KeyState(&in_moveright, s_cmdNsec);
    side -= movespeed * CL_KeyState(&in_moveleft, s_cmdNsec);

    up += movespeed * CL_KeyState(&in_up, s_cmdNsec);
    up -= movespeed * CL_KeyState(&in_down, s_cmdNsec);

    forward += movespeed * CL_KeyState(&in_forward, s_cmdNsec);
    forward -= movespeed * CL_KeyState(&in_back, s_cmdNsec);

    cmd->forwardmove = ClampChar(forward);
    cmd->rightmove = ClampChar(side);
    cmd->upmove = ClampChar(up);
}

/*
=================
CL_MouseEvent
=================
*/
void CL_MouseEvent(float dx, float dy)
{
    if (CL_MapTakesMouse(dx, dy)) {
        return;
    }

    cl.mouseDx[cl.mouseIndex] += dx;
    cl.mouseDy[cl.mouseIndex] += dy;
}

/*
=================
CL_JoystickEvent

Joystick values stay set until changed
=================
*/
void CL_JoystickEvent(int axis, int value, int time)
{
    if (axis < 0 || axis >= std::to_underlying(joystickAxis_t::MAX_JOYSTICK_AXIS)) {
        Q_Error(errorParm_t::ERR_DROP, "CL_JoystickEvent: bad axis {}", axis);
    }
    cl.joystickAxis[static_cast<joystickAxis_t>(axis)] = value;
}

/*
=================
CL_JoystickAngles

The view angles the joystick gives, every frame
=================
*/
// the joystick's setting, kept with its menu's row: no device is read
static cvar_t* in_joystick;
static void CL_JoystickAngles(void)
{
    float anglespeed;

    if (!in_joystick->integer) {
        return;
    }

    if (in_speed.active) {
        anglespeed = 0.001 * cls.frametime * cl_anglespeedkey->value;
    }
    else {
        anglespeed = 0.001 * cls.frametime;
    }

    if (!in_strafe.active) {
        if (cl_mYawOverride) {
            if (cl_mSensitivityOverride) {
                cl.viewangles[YAW] += cl_mYawOverride * cl_mSensitivityOverride * cl.joystickAxis[joystickAxis_t::AXIS_SIDE] / 2.0f;
            }
            else {
                cl.viewangles[YAW] += cl_mYawOverride * CL_OVERRIDE_MOUSE_SENSITIVITY * cl.joystickAxis[joystickAxis_t::AXIS_SIDE] / 2.0f;
            }
        }
        else {
            cl.viewangles[YAW] += anglespeed * (cl_yawspeed->value / 100.0f) * cl.joystickAxis[joystickAxis_t::AXIS_SIDE];
        }
    }

    if (in_mlooking || cl_freelook->integer) {
        if (cl_mPitchOverride) {
            if (cl_mSensitivityOverride) {
                cl.viewangles[PITCH] += cl_mPitchOverride * cl_mSensitivityOverride * cl.joystickAxis[joystickAxis_t::AXIS_FORWARD] / 2.0f;
            }
            else {
                cl.viewangles[PITCH] += cl_mPitchOverride * CL_OVERRIDE_MOUSE_SENSITIVITY * cl.joystickAxis[joystickAxis_t::AXIS_FORWARD] / 2.0f;
            }
        }
        else {
            cl.viewangles[PITCH] += anglespeed * (cl_pitchspeed->value / 100.0f) * cl.joystickAxis[joystickAxis_t::AXIS_FORWARD];
        }
    }
}

/*
=================
CL_JoystickMove

The movement the joystick gives, per command
=================
*/
void CL_JoystickMove(usercmd_t* cmd)
{
    if (!in_joystick->integer) {
        return;
    }

    if (in_speed.active == (cl_run->integer != 0)) {
        cmd->buttons |= buttonFlags_t::BUTTON_WALKING;
    }

    if (in_strafe.active) {
        cmd->rightmove = ClampChar(cmd->rightmove + cl.joystickAxis[joystickAxis_t::AXIS_SIDE]);
    }

    if (!in_mlooking && !cl_freelook->integer) {
        cmd->forwardmove = ClampChar(cmd->forwardmove + cl.joystickAxis[joystickAxis_t::AXIS_FORWARD]);
    }

    cmd->upmove = ClampChar(cmd->upmove + cl.joystickAxis[joystickAxis_t::AXIS_UP]);
}

/*
=================
CL_MouseMove

Every frame: the view angles now, and the movement of strafing or of no mouse look gathered for the next command
=================
*/
static void CL_MouseMove(void)
{
    float mx, my;
    const float speed = static_cast<float>(frame_msec);

    // allow mouse smoothing
    if (m_filter->integer) {
        mx = (cl.mouseDx[0] + cl.mouseDx[1]) * 0.5;
        my = (cl.mouseDy[0] + cl.mouseDy[1]) * 0.5;
    }
    else {
        mx = cl.mouseDx[cl.mouseIndex];
        my = cl.mouseDy[cl.mouseIndex];
    }

    cl.mouseIndex ^= 1;
    cl.mouseDx[cl.mouseIndex] = 0;
    cl.mouseDy[cl.mouseIndex] = 0;

    // no motion, nothing to add; the rate below would be 0 times the inverse root of 0, which is not a number, and
    // the view angles would never be numbers again
    if (mx == 0.0f && my == 0.0f)
        return;

    if (cl_mouseAccel->value != 0.0f) {
        if (cl_mouseAccelStyle->integer == 0) {
            float accelSensitivity;
            float rate;

            rate = SQRTFAST(mx * mx + my * my) / speed;

            if (cl_mYawOverride || cl_mPitchOverride) { // FIXME: different people have different speed mouses,
                if (cl_mSensitivityOverride) {
                    // this will fuck things up for them, need to clamp
                    // max input?
                    accelSensitivity = cl_mSensitivityOverride;
                }
                else {
                    accelSensitivity = cl_sensitivity->value + rate * cl_mouseAccel->value;
                }
            }
            else {
                accelSensitivity = cl_sensitivity->value + rate * cl_mouseAccel->value;
            }
            mx *= accelSensitivity;
            my *= accelSensitivity;

            if (cl_showMouseRate->integer)
                Q_Println("rate: {:f}, accelSensitivity: {:f}", rate, accelSensitivity);
        }
        else {
            float rate[2];
            float power[2];

            // sensitivity remains pretty much unchanged at low speeds
            // cl_mouseAccel is a power value to how the acceleration is shaped
            // cl_mouseAccelOffset is the rate for which the acceleration will have doubled the non accelerated amplification
            // NOTE: decouple the config cvars for independent acceleration setup along X and Y?

            rate[0] = Q_fabs(mx) / speed;
            rate[1] = Q_fabs(my) / speed;
            power[0] = Q_powf(rate[0] / cl_mouseAccelOffset->value, cl_mouseAccel->value);
            power[1] = Q_powf(rate[1] / cl_mouseAccelOffset->value, cl_mouseAccel->value);

            if (cl_mYawOverride || cl_mPitchOverride) { // FIXME: different people have different speed mouses,
                if (cl_mSensitivityOverride) {
                    // this will fuck things up for them, need to clamp
                    // max input?
                    mx = cl_mSensitivityOverride * (mx + ((mx < 0) ? -power[0] : power[0]) * cl_mouseAccelOffset->value);
                    my = cl_mSensitivityOverride * (my + ((my < 0) ? -power[1] : power[1]) * cl_mouseAccelOffset->value);
                }
                else {
                    mx = cl_sensitivity->value * (mx + ((mx < 0) ? -power[0] : power[0]) * cl_mouseAccelOffset->value);
                    my = cl_sensitivity->value * (my + ((my < 0) ? -power[1] : power[1]) * cl_mouseAccelOffset->value);
                }
            }
            else {
                mx = cl_sensitivity->value * (mx + ((mx < 0) ? -power[0] : power[0]) * cl_mouseAccelOffset->value);
                my = cl_sensitivity->value * (my + ((my < 0) ? -power[1] : power[1]) * cl_mouseAccelOffset->value);
            }

            if (cl_showMouseRate->integer)
                Q_Println("ratex: {:f}, ratey: {:f}, powx: {:f}, powy: {:f}", rate[0], rate[1], power[0], power[1]);
        }
    }
    else {
        if (cl_mYawOverride || cl_mPitchOverride) { // FIXME: different people have different speed mouses,
            if (cl_mSensitivityOverride) {
                // this will fuck things up for them, need to clamp
                // max input?
                mx *= cl_mSensitivityOverride;
                my *= cl_mSensitivityOverride;
            }
            else {
                mx *= cl_sensitivity->value;
                my *= cl_sensitivity->value;
            }
        }
        else {
            mx *= cl_sensitivity->value;
            my *= cl_sensitivity->value;
        }
    }

    // ingame FOV
    mx *= cl.cgameSensitivity;
    my *= cl.cgameSensitivity;

    // add mouse X/Y movement to cmd
    if (in_strafe.active)
        s_mouseSide += m_side->value * mx;
    else {
        if (cl_mYawOverride)
            cl.viewangles[YAW] -= cl_mYawOverride * mx;
        else
            cl.viewangles[YAW] -= m_yaw->value * mx;
    }

    if ((in_mlooking || cl_freelook->integer) && !in_strafe.active) {
        // VVFIXME - This is supposed to be a CVAR
        const float cl_pitchSensitivity = 1.0f;
        const float pitch = CL_MousePitch();
        if (cl_mPitchOverride) {
            if (pitch > 0)
                cl.viewangles[PITCH] += cl_mPitchOverride * my * cl_pitchSensitivity;
            else
                cl.viewangles[PITCH] -= cl_mPitchOverride * my * cl_pitchSensitivity;
        }
        else
            cl.viewangles[PITCH] += pitch * my * cl_pitchSensitivity;
    }
    else
        s_mouseForward -= m_forward->value * my;
}

/*
=================
CL_InputRead

The input read at a time of the engine's clock and of the client's: the frame's span
=================
*/
void CL_InputRead(int inputTime, int64_t inputNsec)
{
    frame_msec = inputTime - old_com_frameTime; // since the input was last read (a frame, or a tick between two)

    // if running over 1000fps, act as if each frame is 1ms
    // prevents divisions by zero
    if (frame_msec < 1)
        frame_msec = 1;

    // if running less than 5fps, truncate the extra time to prevent
    // unexpected moves after a hitch
    if (frame_msec > 200)
        frame_msec = 200;

    old_com_frameTime = inputTime;
    s_inputNsec = inputNsec;
    s_frameNsec = Q_min(Q_max(s_inputNsec - s_lastInputNsec, MIN_INPUT_NSEC), MAX_INPUT_NSEC);
    s_lastInputNsec = s_inputNsec;
}

/*
=================
CL_CommandStart

A command starts at the input's time: its span is the time since the last command, or the frame's
=================
*/
void CL_CommandStart(bool sinceLastCommand)
{
    s_cmdNsec = sinceLastCommand ? Q_min(Q_max(s_inputNsec - s_lastCmdNsec, MIN_INPUT_NSEC), MAX_INPUT_NSEC) : s_frameNsec;
    s_lastCmdNsec = s_inputNsec;
}

/*
=================
CL_UpdateViewAngles

The view angles, every frame (the multiplayer's commands are made per tick, the view turns at the frame rate)
=================
*/
void CL_UpdateViewAngles(void)
{
    vec3_t oldAngles;

    VectorCopy(cl.viewangles, oldAngles);

    // keyboard angle adjustment
    CL_AdjustAngles();

    // the mouse
    CL_MouseMove();

    // the joystick
    CL_JoystickAngles();

    // check to make sure the angles haven't wrapped
    if (cl.viewangles[PITCH] - oldAngles[PITCH] > 90) {
        cl.viewangles[PITCH] = oldAngles[PITCH] + 90;
    }
    else if (oldAngles[PITCH] - cl.viewangles[PITCH] > 90) {
        cl.viewangles[PITCH] = oldAngles[PITCH] - 90;
    }

    // draw debug graphs of turning for mouse testing
    if (cl_debugMove->integer) {
        if (cl_debugMove->integer == 1) {
            SCR_DebugGraph(Q_fabsf(cl.viewangles[YAW] - oldAngles[YAW]), 0);
        }
        if (cl_debugMove->integer == 2) {
            SCR_DebugGraph(Q_fabsf(cl.viewangles[PITCH] - oldAngles[PITCH]), 0);
        }
    }
}

/*
=================
CL_MouseMoveToCommand

The mouse's movement gathered over the frames since the last command, into the command
=================
*/
void CL_MouseMoveToCommand(usercmd_t* cmd)
{
    cmd->rightmove = ClampChar(cmd->rightmove + s_mouseSide);
    cmd->forwardmove = ClampChar(cmd->forwardmove + s_mouseForward);
    s_mouseSide = s_mouseForward = 0.0f;
}

// the commands of the buttons the three games share, with their descriptions (the single player keeps none)
struct commonInputCommand_t {
    const char* name;
    const char* description;
    xcommand_t function;
};
static const commonInputCommand_t commonInputCommands[] = {
    {"centerview", "Centers view on screen", IN_CenterView},
    {"+moveup", "Jump", IN_UpDown},
    {"-moveup", nullptr, IN_UpUp},
    {"+movedown", "Crouch", IN_DownDown},
    {"-movedown", nullptr, IN_DownUp},
    {"+left", "Rotate camera left", IN_LeftDown},
    {"-left", nullptr, IN_LeftUp},
    {"+right", "Rotate camera right", IN_RightDown},
    {"-right", nullptr, IN_RightUp},
    {"+forward", "Move forward", IN_ForwardDown},
    {"-forward", nullptr, IN_ForwardUp},
    {"+back", "Move backward", IN_BackDown},
    {"-back", nullptr, IN_BackUp},
    {"+lookup", "Tilt camera up", IN_LookupDown},
    {"-lookup", nullptr, IN_LookupUp},
    {"+lookdown", "Tilt camera down", IN_LookdownDown},
    {"-lookdown", nullptr, IN_LookdownUp},
    {"+strafe", "Hold to strafe", IN_StrafeDown},
    {"-strafe", nullptr, IN_StrafeUp},
    {"+moveleft", "Strafe left", IN_MoveleftDown},
    {"-moveleft", nullptr, IN_MoveleftUp},
    {"+moveright", "Strafe right", IN_MoverightDown},
    {"-moveright", nullptr, IN_MoverightUp},
    {"+speed", "Walk or run", IN_SpeedDown},
    {"-speed", nullptr, IN_SpeedUp},
    {"+button0", "Button 0", IN_Button0Down},
    {"-button0", nullptr, IN_Button0Up},
    {"+button1", "Button 1", IN_Button1Down},
    {"-button1", nullptr, IN_Button1Up},
    {"+button2", "Button 2", IN_Button2Down},
    {"-button2", nullptr, IN_Button2Up},
    {"+button3", "Button 3", IN_Button3Down},
    {"-button3", nullptr, IN_Button3Up},
    {"+button4", "Button 4", IN_Button4Down},
    {"-button4", nullptr, IN_Button4Up},
    {"+button5", "Button 5", IN_Button5Down},
    {"-button5", nullptr, IN_Button5Up},
    {"+button6", "Button 6", IN_Button6Down},
    {"-button6", nullptr, IN_Button6Up},
    {"+button7", "Button 7", IN_Button7Down},
    {"-button7", nullptr, IN_Button7Up},
    {"+button8", "Button 8", IN_Button8Down},
    {"-button8", nullptr, IN_Button8Up},
    {"+button9", "Button 9", IN_Button9Down},
    {"-button9", nullptr, IN_Button9Up},
    {"+button10", "Button 10", IN_Button10Down},
    {"-button10", nullptr, IN_Button10Up},
    {"+button11", "Button 11", IN_Button11Down},
    {"-button11", nullptr, IN_Button11Up},
    {"+button12", "Button 12", IN_Button12Down},
    {"-button12", nullptr, IN_Button12Up},
    {"+button13", "Button 13", IN_Button13Down},
    {"-button13", nullptr, IN_Button13Up},
    {"+button14", "Button 14", IN_Button14Down},
    {"-button14", nullptr, IN_Button14Up},
    {"+button15", "Button 15", IN_Button15Down},
    {"-button15", nullptr, IN_Button15Up},
    {"+mlook", "Hold to use mouse look", IN_MLookDown},
    {"-mlook", nullptr, IN_MLookUp},
};

/*
============
CL_InitCommonInput
============
*/
void CL_InitCommonInput(void)
{
    for (const commonInputCommand_t& command : commonInputCommands)
        Cmd_AddCommand(command.name, command.function, command.description);

    cl_debugMove = Cvar_Get("cl_debugMove", "0", cvarFlags_t::CVAR_NONE);
    in_joystick = Cvar_Get("in_joystick", "0", cvarFlags_t::CVAR_ARCHIVE_ND | cvarFlags_t::CVAR_LATCH);
}

/*
============
CL_ShutdownCommonInput
============
*/
void CL_ShutdownCommonInput(void)
{
    for (const commonInputCommand_t& command : commonInputCommands)
        Cmd_RemoveCommand(command.name);
}

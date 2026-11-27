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

// cl_keys.cpp -- the clients' keys, the same in the three games (keys.h), compiled by each client against its own
// headers: the keys' states and binds, the console's line and its history, the edit fields drawn and typed in, the
// bind commands, and a key or a character given to the console or to what each tree's client/cl_keys.cpp names. The
// fields' editing and the keys' names are src/shared/client's (field.h, keynames.h).
#include <client/client.h>
#include <shared/platform/engine/engine.h>

#include <array>
/*

key up events are sent even if in console mode

*/

// console
field_t g_consoleField;
int nextHistoryLine; // the last line in the history buffer, not masked
int historyLine;     // the line being displayed from history buffer will be <= nextHistoryLine
field_t historyEditLines[COMMAND_HISTORY];

keyGlobals_t kg;

// the keys' table, whose free codes take the names of the keys it has none for (Key_DynamicKey)
keyNames_t keynames = client_input::DefaultKeyNames();
static client_input::DynamicKeys dynamicKeys;

// either side's modifier: the right ones are keys of their own (RSHIFT, RCTRL, RALT), bindable apart
static bool Key_ShiftDown(void)
{
    return (kg.keys[fakeAscii_t::A_SHIFT].down || kg.keys[fakeAscii_t::A_SHIFT2].down);
}
static bool Key_CtrlDown(void)
{
    return (kg.keys[fakeAscii_t::A_CTRL].down || kg.keys[fakeAscii_t::A_CTRL2].down);
}
static bool Key_AltDown(void)
{
    return (kg.keys[fakeAscii_t::A_ALT].down || kg.keys[fakeAscii_t::A_ALT2].down);
}

// the key whose state and binding a key shares (a lower-case letter's upper-case one)
static fakeAscii_t Key_Upper(fakeAscii_t keynum)
{
    return client_input::SharedKey(keynames, keynum);
}

/*
=============================================================================

EDIT FIELDS

=============================================================================
*/

/*
===================
Field_Draw

Handles horizontal scrolling and cursor blinking
x, y, amd width are in pixels
===================
*/
extern console_t con;
void Field_VariableSizeDraw(field_t* edit, int x, int y, int size, bool showCursor, bool noColorEscape)
{
    const client_input::FieldView view = client_input::VisibleField(*edit);
    char str[MAX_EDIT_LINE];
    int cursorChar;

    // extract the characters shown
    Com_Memcpy(str, edit->buffer + view.start, view.length);
    str[view.length] = 0;

    // draw it
    if (size == con.charWidth) {
        float color[4];

        color[0] = color[1] = color[2] = color[3] = 1.0;
        SCR_DrawSmallStringExt(x, y, str, color, false, noColorEscape);
    }
    else {
        // draw big string with drop shadow
        SCR_DrawBigString(x, y, str, 1.0, noColorEscape);
    }

    // draw the cursor
    if (showCursor) {
        if ((int)(cls.realtime >> 8) & 1) {
            return; // off blink
        }

        if (kg.key_overstrikeMode) {
            cursorChar = 11;
        }
        else {
            cursorChar = 10;
        }

        if (size == con.charWidth) {
            SCR_DrawSmallChar(x + view.cursorColumn * size, y, cursorChar);
        }
        else {
            str[0] = cursorChar;
            str[1] = 0;
            SCR_DrawBigString(x + view.cursorColumn * size, y, str, 1.0, false);
        }
    }
}

void Field_Draw(field_t* edit, int x, int y, bool showCursor, bool noColorEscape)
{
    Field_VariableSizeDraw(edit, x, y, con.charWidth, showCursor, noColorEscape);
}

void Field_BigDraw(field_t* edit, int x, int y, bool showCursor, bool noColorEscape)
{
    Field_VariableSizeDraw(edit, x, y, BIGCHAR_WIDTH, showCursor, noColorEscape);
}

/*
================
Field_Paste
================
*/
void Field_Paste(field_t* edit)
{
    char *cbd, *c;

    c = cbd = Sys_GetClipboardData();

    if (!cbd) {
        return;
    }

    // send as if typed, so insert / overstrike works properly; but only the printable characters: a control
    // character of the clipboard would act as its key (Ctrl-C emptying the line, Ctrl-V pasting again without end)
    while (*c) {
        const int ch = ConvertUTF32ToExpectedCharset(ConvertUTF8ToUTF32(c, &c));

        if (ch >= ' ') {
            Field_CharEvent(edit, ch);
        }
    }

    Z_Free(cbd);
}

/*
=================
Field_KeyDownEvent

Performs the basic line editing functions for the console,
in-game talk, and menu fields

Key events are used for non-printable characters, others are gotten from char events.
=================
*/
void Field_KeyDownEvent(field_t* edit, fakeAscii_t key)
{
    switch (client_input::PressKey(*edit, key, Key_ShiftDown())) {
    case client_input::FieldRequest::Paste:
        Field_Paste(edit);
        break;
    case client_input::FieldRequest::SwitchOverstrike:
        kg.key_overstrikeMode = !kg.key_overstrikeMode;
        break;
    case client_input::FieldRequest::None:
        break;
    }
}

/*
==================
Field_CharEvent
==================
*/
void Field_CharEvent(field_t* edit, int ch)
{
    if (client_input::TypeCharacter(*edit, ch, kg.key_overstrikeMode) == client_input::FieldRequest::Paste) {
        Field_Paste(edit);
    }
}

/*
====================
Console_Key

Handles history and console scrollback
====================
*/
void Console_Key(fakeAscii_t key)
{
    // ctrl-L clears screen
    if (keynames[key].lower == 'l' && Key_CtrlDown()) {
        Cbuf_AddText("clear\n");
        return;
    }

    // enter finishes the line
    if (key == fakeAscii_t::A_ENTER || key == fakeAscii_t::A_KP_ENTER) {
        // legacy hack: strip any prepended slashes. they're not necessary anymore
        if (g_consoleField.buffer[0] && (g_consoleField.buffer[0] == '\\' || g_consoleField.buffer[0] == '/')) {
            char temp[MAX_EDIT_LINE - 1];

            Q_strncpyz(temp, g_consoleField.buffer + 1, sizeof(temp));
            Q_FormatTo(g_consoleField.buffer, "{}", temp);
            g_consoleField.cursor--;
        }
        //	else
        //		Field_AutoComplete( &g_consoleField );

        // print executed command
        Q_Println("{}{}", CONSOLE_PROMPT_CHAR, g_consoleField.buffer);

        Cbuf_AddText(g_consoleField.buffer);
        Cbuf_AddText("\n");

        if (!g_consoleField.buffer[0]) {
            return; // empty lines just scroll the console without adding to history
        }

        // copy line to history buffer
        historyEditLines[nextHistoryLine % COMMAND_HISTORY] = g_consoleField;
        nextHistoryLine++;
        historyLine = nextHistoryLine;

        Field_Clear(&g_consoleField);

        g_consoleField.widthInChars = g_console_field_width;

        //	CL_SaveConsoleHistory();

        if (cls.state == connstate_t::CA_DISCONNECTED)
            SCR_UpdateScreen(); // force an update, because the command may take some time

        return;
    }

    // tab completion
    if (key == fakeAscii_t::A_TAB) {
        Field_AutoComplete(&g_consoleField);
        return;
    }

    // history scrolling
    if (key == fakeAscii_t::A_CURSOR_UP || key == fakeAscii_t::A_KP_8 || (Key_ShiftDown() && key == fakeAscii_t::A_MWHEELUP) ||
        (Key_CtrlDown() && keynames[key].lower == 'p')) { // scroll up: arrow-up, numpad-up, shift + mwheelup, ctrl + p
        if (nextHistoryLine - historyLine < COMMAND_HISTORY && historyLine > 0)
            historyLine--;
        g_consoleField = historyEditLines[historyLine % COMMAND_HISTORY];

        return;
    }

    if (key == fakeAscii_t::A_CURSOR_DOWN || key == fakeAscii_t::A_KP_2 || (Key_ShiftDown() && key == fakeAscii_t::A_MWHEELDOWN) ||
        (Key_CtrlDown() && keynames[key].lower == 'n')) { // scroll down: arrow-down, numpad-down, shift + mwheeldown, ctrl + n
        historyLine++;
        if (historyLine >= nextHistoryLine) {
            historyLine = nextHistoryLine;
            Field_Clear(&g_consoleField);
            g_consoleField.widthInChars = g_console_field_width;
            return;
        }
        g_consoleField = historyEditLines[historyLine % COMMAND_HISTORY];
        return;
    }

    // console scrolling (ctrl to scroll fast)
    if (key == fakeAscii_t::A_PAGE_UP || key == fakeAscii_t::A_MWHEELUP) {
        int count = Key_CtrlDown() ? 5 : 1;
        for (int i = 0; i < count; i++)
            Con_PageUp();
        return;
    }

    if (key == fakeAscii_t::A_PAGE_DOWN || key == fakeAscii_t::A_MWHEELDOWN) {
        int count = Key_CtrlDown() ? 5 : 1;
        for (int i = 0; i < count; i++)
            Con_PageDown();
        return;
    }

    // ctrl-home = top of console
    if (key == fakeAscii_t::A_HOME && Key_CtrlDown()) {
        Con_Top();
        return;
    }

    // ctrl-end = bottom of console
    if (key == fakeAscii_t::A_END && Key_CtrlDown()) {
        Con_Bottom();
        return;
    }

    // pass to the normal editline routine
    Field_KeyDownEvent(&g_consoleField, key);
}

//============================================================================

bool Key_GetOverstrikeMode(void)
{
    return kg.key_overstrikeMode;
}

void Key_SetOverstrikeMode(bool state)
{
    kg.key_overstrikeMode = state;
}

/*
===================
Key_IsDown
===================
*/
bool Key_IsDown(fakeAscii_t keynum)
{
    if (keynum < fakeAscii_t::A_NULL || keynum >= fakeAscii_t::MAX_KEYS)
        return false;

    return kg.keys[Key_Upper(keynum)].down;
}

/*
===================
Key_DynamicKey

The keys the table has no code for (a function key beyond F12, a key of a layout whose character is not in the
game's character set) share the sixteen codes it leaves free: a key takes one when it is first pressed or named in
a bind, with its name, which a bind is written and read back by. The code already given to that name; else one
never given; else the one of such a key that has no bind and is not held, which loses it. None when all sixteen are
bound.
===================
*/
std::optional<fakeAscii_t> Key_DynamicKey(const char* name)
{
    std::array<bool, client_input::kDynamicKeyCount> mayTake{};

    for (int i = 0; i < client_input::kDynamicKeyCount; i++) {
        const qkey_t& key = kg.keys[fakeAscii_t::A_UNDEFINED_7 + i];
        mayTake[i] = !key.down && !(key.binding && key.binding[0]);
    }

    const std::optional<fakeAscii_t> key = dynamicKeys.CodeFor(keynames, name, mayTake);
    if (!key) {
        Q_PrintlnWarning("no key code left for {}: {} keys of its kind are bound, unbind one", name, client_input::kDynamicKeyCount);
    }
    return key;
}

/*
===================
Key_StringToKeynum

Returns a key number to be used to index keys[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.

0x11 will be interpreted as raw hex, which will allow new controlers
to be configured even if they don't have defined names.
===================
*/
std::optional<fakeAscii_t> Key_StringToKeynum(const char* str)
{
    if (!VALIDSTRING(str))
        return std::nullopt;

    if (const std::optional<fakeAscii_t> key = client_input::KeyOfName(keynames, str))
        return key;

    // a key the table has no code for, written by its name
    if (Platform_IsKeyName(str))
        return Key_DynamicKey(str);

    return std::nullopt;
}

// Returns the ascii code of the keynum
const char* Key_KeynumToAscii(fakeAscii_t keynum)
{
    static client_input::KeyText text;

    text = client_input::CharacterOfKey(keynames, keynum);
    return text.c_str();
}

/*
===================
Key_KeynumToString

Returns a string (either a single ascii char, a K_* name, or a 0x11 hex string) for the
given keynum.
===================
*/
// Returns a console/config file friendly name for the key
const char* Key_KeynumToString(fakeAscii_t keynum)
{
    static client_input::KeyText text;

    text = client_input::NameOfKey(keynames, keynum);
    return text.c_str();
}

/*
===================
Key_SetBinding
===================
*/
void Key_SetBinding(fakeAscii_t keynum, const char* binding)
{
    if (keynum < fakeAscii_t::A_NULL || keynum >= fakeAscii_t::MAX_KEYS)
        return;

    // free old bindings
    if (kg.keys[Key_Upper(keynum)].binding) {
        Z_Free(kg.keys[Key_Upper(keynum)].binding);
        kg.keys[Key_Upper(keynum)].binding = nullptr;
    }

    // allocate memory for new binding
    if (binding)
        kg.keys[Key_Upper(keynum)].binding = CopyString(binding);

    // consider this like modifying an archived cvar, so the
    // file write will be triggered at the next oportunity
    cvar_modifiedFlags |= cvarFlags_t::CVAR_ARCHIVE;
}

/*
===================
Key_GetBinding
===================
*/
const char* Key_GetBinding(fakeAscii_t keynum)
{
    if (keynum < fakeAscii_t::A_NULL || keynum >= fakeAscii_t::MAX_KEYS)
        return "";

    return kg.keys[keynum].binding;
}

/*
===================
Key_GetKey
===================
*/
std::optional<fakeAscii_t> Key_GetKey(const char* binding)
{
    if (binding) {
        for (fakeAscii_t i = fakeAscii_t::A_NULL; i < fakeAscii_t::MAX_KEYS; i++) {
            if (kg.keys[i].binding && !Q_stricmp(binding, kg.keys[i].binding))
                return i;
        }
    }

    return std::nullopt;
}

/*
===================
Key_Unbind_f
===================
*/
void Key_Unbind_f(void)
{
    if (Cmd_Argc() != 2) {
        Q_Println("unbind <key> : remove commands from a key");
        return;
    }

    const std::optional<fakeAscii_t> b = Key_StringToKeynum(Cmd_Argv(1));
    if (!b) {
        Q_Println("\"{}\" isn't a valid key", Cmd_Argv(1));
        return;
    }

    Key_SetBinding(*b, "");
}

/*
===================
Key_Unbindall_f
===================
*/
void Key_Unbindall_f(void)
{
    for (fakeAscii_t i = fakeAscii_t::A_NULL; i < fakeAscii_t::MAX_KEYS; i++) {
        if (kg.keys[i].binding)
            Key_SetBinding(i, "");
    }
}

/*
===================
Key_Bind_f
===================
*/
void Key_Bind_f(void)
{
    int c = Cmd_Argc();

    if (c < 2) {
        Q_Println("bind <key> [command] : attach a command to a key");
        return;
    }

    const std::optional<fakeAscii_t> b = Key_StringToKeynum(Cmd_Argv(1));
    if (!b) {
        Q_Println("\"{}\" isn't a valid key", Cmd_Argv(1));
        return;
    }

    if (c == 2) {
        if (kg.keys[*b].binding && kg.keys[*b].binding[0])
            Q_Println(S_COLOR_GREY "Bind " S_COLOR_WHITE "{} = " S_COLOR_GREY "\"" S_COLOR_WHITE "{}" S_COLOR_GREY "\"" S_COLOR_WHITE, Key_KeynumToString(*b),
                      kg.keys[*b].binding);
        else
            Q_Println("\"{}\" is not bound", Key_KeynumToString(*b));
        return;
    }

    Key_SetBinding(*b, Cmd_ArgsFrom(2));
}

/*
============
Key_WriteBindings

Writes lines containing "bind key value"
============
*/
void Key_WriteBindings(fileHandle_t f)
{
    FS_Print(f, "unbindall\n");
    for (fakeAscii_t i = fakeAscii_t::A_NULL; i < fakeAscii_t::MAX_KEYS; i++) {
        if (kg.keys[i].binding && kg.keys[i].binding[0]) {
            const char* name = Key_KeynumToString(i);

            // handle the escape character nicely
            if (!strcmp(name, "\\"))
                FS_Print(f, "bind \"\\\" \"{}\"\n", kg.keys[i].binding);
            else
                FS_Print(f, "bind \"{}\" \"{}\"\n", name, kg.keys[i].binding);
        }
    }
}

/*
============
Key_Bindlist_f

============
*/
void Key_Bindlist_f(void)
{
    for (fakeAscii_t i = fakeAscii_t::A_NULL; i < fakeAscii_t::MAX_KEYS; i++) {
        if (kg.keys[i].binding && kg.keys[i].binding[0])
            Q_Println(S_COLOR_GREY "Key " S_COLOR_WHITE "{} ({}) = " S_COLOR_GREY "\"" S_COLOR_WHITE "{}" S_COLOR_GREY "\"" S_COLOR_WHITE, Key_KeynumToAscii(i),
                      Key_KeynumToString(i), kg.keys[i].binding);
    }
}

/*
============
Key_KeynameCompletion
============
*/
void Key_KeynameCompletion(callbackFunc_t callback)
{
    for (const keyname_t& keyname : keynames) {
        if (keyname.name)
            callback(keyname.name);
    }
}

/*
====================
Key_CompleteUnbind
====================
*/
static void Key_CompleteUnbind(char* args, int argNum)
{
    if (argNum == 2) {
        // Skip "unbind "
        char* p = Com_SkipTokens(args, 1, " ");
        if (p > args)
            Field_CompleteKeyname();
    }
}

/*
====================
Key_CompleteBind
====================
*/
static void Key_CompleteBind(char* args, int argNum)
{
    char* p;

    if (argNum == 2) {
        // Skip "bind "
        p = Com_SkipTokens(args, 1, " ");

        if (p > args)
            Field_CompleteKeyname();
    }
    else if (argNum >= 3) {
        // Skip "bind <key> "
        p = Com_SkipTokens(args, 2, " ");

        if (p > args)
            Field_CompleteCommand(p, true, true);
    }
}

/*
===================
CL_InitKeyCommands
===================
*/
void CL_InitKeyCommands(void)
{
    // register our functions
    Cmd_AddCommand("bind", Key_Bind_f, "Bind a key to a console command");
    Cmd_SetCommandCompletionFunc("bind", Key_CompleteBind);
    Cmd_AddCommand("unbind", Key_Unbind_f, "Unbind a key");
    Cmd_SetCommandCompletionFunc("unbind", Key_CompleteUnbind);
    Cmd_AddCommand("unbindall", Key_Unbindall_f, "Delete all key bindings");
    Cmd_AddCommand("bindlist", Key_Bindlist_f, "Show all bindings in the console");
}

/*
===================
CL_BindUICommand

Returns true if bind command should be executed while user interface is shown
===================
*/
static bool CL_BindUICommand(const char* cmd)
{
    if (Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_CONSOLE))
        return false;

    if (!Q_stricmp(cmd, "toggleconsole"))
        return true;
    if (!Q_stricmp(cmd, "togglemenu"))
        return true;

    return false;
}

/*
===================
CL_ParseBinding

Execute the commands in the bind string
===================
*/
void CL_ParseBinding(fakeAscii_t key, bool down, int64_t time)
{
    char buf[MAX_STRING_CHARS], *p = buf, *end;
    bool allCommands, allowUpCmds;

    if (cls.state == connstate_t::CA_DISCONNECTED && !Any(Key_GetCatcher()))
        return;
    if (!kg.keys[Key_Upper(key)].binding || !kg.keys[Key_Upper(key)].binding[0])
        return;
    Q_strncpyz(buf, kg.keys[Key_Upper(key)].binding, sizeof(buf));

    // run all bind commands if console, ui, etc aren't reading keys
    allCommands = !Any(Key_GetCatcher());

    // allow button up commands if in game even if key catcher is set
    allowUpCmds = (cls.state != connstate_t::CA_DISCONNECTED);

    while (1) {
        while (isspace(*p))
            p++;
        end = strchr(p, ';');
        if (end)
            *end = '\0';
        if (*p == '+') {
            // button commands add keynum and time as parameters
            // so that multiple sources can be discriminated and
            // subframe corrected
            if (allCommands || (allowUpCmds && !down)) {
                char cmd[1024];
                Q_FormatTo(cmd, "{}{} {} {}\n", (down) ? '+' : '-', p + 1, key, time);
                Cbuf_AddText(cmd);
            }
        }
        else if (down) {
            // normal commands only execute on key press
            if (allCommands || CL_BindUICommand(p)) {
                Cbuf_AddText(p);
                Cbuf_AddText("\n");
            }
        }
        if (!end)
            break;
        p = end + 1;
    }
}

/*
===================
CL_KeyDownEvent

Called by CL_KeyEvent to handle a keypress
===================
*/
void CL_KeyDownEvent(fakeAscii_t key, int64_t time)
{
    kg.keys[Key_Upper(key)].down = true;
    kg.keys[Key_Upper(key)].repeats++;
    if (kg.keys[Key_Upper(key)].repeats == 1) {
        kg.keyDownCount++;
        kg.anykeydown = true;
    }

    if (cl_allowAltEnter->integer && Key_AltDown() && key == fakeAscii_t::A_ENTER) {
        // between a window and borderless fullscreen, applied once the frame's events are read: the window stays the
        // same one, the renderer and the modules take its new size without starting again
        Cvar_SetValue("r_displayState", Cvar_VariableIntegerValue("r_displayState") == 0 ? 1 : 0);
        return;
    }

    // console key is hardcoded, so the user can never unbind it
    if (key == fakeAscii_t::A_CONSOLE || (Key_ShiftDown() && key == fakeAscii_t::A_ESCAPE)) {
        Con_ToggleConsole_f();
        Key_ClearStates();
        return;
    }

    // a video playing: each tree's rule
    if (!Any(Key_GetCatcher()) && CL_KeyStopsVideo())
        return;

    // escape is always handled special
    if (key == fakeAscii_t::A_ESCAPE) {
        if (!Key_ShiftDown() && Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_CONSOLE)) {
            Con_ToggleConsole_f();
            Key_ClearStates();
            return;
        }

        if (CL_EscapeClosesCatcher())
            return;

        if (!Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_UI)) {
            CL_EscapeOpensMenu();
            return;
        }

        CL_ModuleKeyEvent(key, true);
        return;
    }

    // send the bound action
    if (!CL_ClientMenuTakesKeys())
        CL_ParseBinding(key, true, time);

    // distribute the key down event to the appropriate handler: the console, what each tree names, the console while
    // disconnected
    if (Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_CONSOLE))
        Console_Key(key);
    else if (!CL_ModuleKeyEvent(key, true) && cls.state == connstate_t::CA_DISCONNECTED)
        Console_Key(key);
}

/*
===================
CL_KeyUpEvent

Called by CL_KeyEvent to handle a keyrelease
===================
*/
void CL_KeyUpEvent(fakeAscii_t key, int64_t time)
{
    kg.keys[Key_Upper(key)].repeats = 0;
    kg.keys[Key_Upper(key)].down = false;
    kg.keyDownCount--;

    if (kg.keyDownCount <= 0) {
        kg.anykeydown = false;
        kg.keyDownCount = 0;
    }

    // don't process key-up events for the console key
    if (key == fakeAscii_t::A_CONSOLE || (key == fakeAscii_t::A_ESCAPE && Key_ShiftDown()))
        return;

    //
    // key up events only perform actions if the game key binding is
    // a button command (leading + sign).  These will be processed even in
    // console mode and menu mode, to keep the character from continuing
    // an action started before a mode switch.
    //
    CL_ParseBinding(key, false, time);

    CL_ModuleKeyEvent(key, false);
}

/*
===================
CL_KeyEvent

Called by the system for both key up and key down events
===================
*/
void CL_KeyEvent(fakeAscii_t key, bool down, int64_t time)
{
    if (down)
        CL_KeyDownEvent(key, time);
    else
        CL_KeyUpEvent(key, time);
}

/*
===================
CL_CharEvent

Normal keyboard characters, already shifted / capslocked / etc
===================
*/
void CL_CharEvent(int key)
{
    // delete is not a printable character and is otherwise handled by Field_KeyDownEvent
    if (key == 127)
        return;

    // distribute the key down event to the appropriate handler: the console, what each tree names, the console while
    // disconnected
    if (Any(Key_GetCatcher() & keyCatchFlags_t::KEYCATCH_CONSOLE))
        Field_CharEvent(&g_consoleField, key);
    else if (!CL_ModuleCharEvent(key) && cls.state == connstate_t::CA_DISCONNECTED)
        Field_CharEvent(&g_consoleField, key);
}

/*
===================
Key_ClearStates
===================
*/
void Key_ClearStates(void)
{
    kg.anykeydown = false;
    kg.keyDownCount = 0;

    for (fakeAscii_t i = fakeAscii_t::A_NULL; i < fakeAscii_t::MAX_KEYS; i++) {
        if (kg.keys[i].down)
            CL_KeyEvent(i, false, 0);
        kg.keys[i].down = false;
        kg.keys[i].repeats = 0;
    }
}

static keyCatchFlags_t keyCatchers = keyCatchFlags_t::KEYCATCH_NONE;

/*
====================
Key_GetCatcher
====================
*/
keyCatchFlags_t Key_GetCatcher(void)
{
    return keyCatchers;
}

/*
====================
Key_SetCatcher
====================
*/
void Key_SetCatcher(keyCatchFlags_t catcher)
{
    // If the catcher state is changing, clear all key states
    if (catcher != keyCatchers)
        Key_ClearStates();

    keyCatchers = catcher;
}
